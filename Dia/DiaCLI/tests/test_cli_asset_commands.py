"""Unit tests for the dia asset CLI commands and supporting config loader."""
from __future__ import annotations

import json
import textwrap
from pathlib import Path
from unittest.mock import MagicMock, patch

import pytest
from click.testing import CliRunner

from dia_cli.cli_main import cli
from dia_cli.commands.asset.config_loader import (
    AssetConfigError,
    AssetTargetConfig,
    load_asset_target_config,
)


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

_MINIMAL_TOML = textwrap.dedent("""\
    [global]
    default_config = "Debug"

    [targets.cluichetest]
    project = "Cluiche/CluicheTest/CluicheTest.vcxproj"
    app_name = "CluicheTest"
    catalogue_manifest = "Assets/CluicheTest/assets.catalogue.json"
    asset_stages = ["stage.main_menu"]

    [targets.noassets]
    project = "Cluiche/NoAssets/NoAssets.vcxproj"
    app_name = "NoAssets"
    catalogue_manifest = "Assets/NoAssets/assets.catalogue.json"
    asset_stages = []
""")

_EMPTY_CATALOGUE = json.dumps({"assets": []})


def _write_toml(tmp_path: Path, content: str = _MINIMAL_TOML) -> None:
    (tmp_path / "pipeline.toml").write_text(content)


def _write_catalogue(tmp_path: Path, target: str = "CluicheTest", content: str = _EMPTY_CATALOGUE) -> Path:
    cat_dir = tmp_path / "Assets" / target
    cat_dir.mkdir(parents=True, exist_ok=True)
    cat_path = cat_dir / "assets.catalogue.json"
    cat_path.write_text(content)
    return cat_path


# ---------------------------------------------------------------------------
# CLI discovery
# ---------------------------------------------------------------------------

def test_asset_command_discovered():
    runner = CliRunner()
    result = runner.invoke(cli, ["--help"])
    assert "asset" in result.output


def test_asset_help():
    runner = CliRunner()
    result = runner.invoke(cli, ["asset", "--help"])
    assert result.exit_code == 0
    assert "build" in result.output
    assert "validate" in result.output
    assert "deploy" in result.output


def test_asset_build_help():
    runner = CliRunner()
    result = runner.invoke(cli, ["asset", "build", "--help"])
    assert result.exit_code == 0
    assert "--target" in result.output
    assert "--config" in result.output
    assert "--force" in result.output


def test_asset_validate_help():
    runner = CliRunner()
    result = runner.invoke(cli, ["asset", "validate", "--help"])
    assert result.exit_code == 0
    assert "--target" in result.output


def test_asset_deploy_help():
    runner = CliRunner()
    result = runner.invoke(cli, ["asset", "deploy", "--help"])
    assert result.exit_code == 0
    assert "--target" in result.output


# ---------------------------------------------------------------------------
# config_loader: load_asset_target_config
# ---------------------------------------------------------------------------

def test_load_config_returns_target(tmp_path):
    _write_toml(tmp_path)
    cfg = load_asset_target_config(tmp_path, "cluichetest")
    assert isinstance(cfg, AssetTargetConfig)
    assert cfg.app_name == "CluicheTest"
    assert cfg.catalogue_manifest == "Assets/CluicheTest/assets.catalogue.json"
    assert cfg.asset_stages == ["stage.main_menu"]


def test_load_config_missing_toml_raises(tmp_path):
    with pytest.raises(AssetConfigError, match="pipeline.toml not found"):
        load_asset_target_config(tmp_path, "cluichetest")


def test_load_config_unknown_target_raises(tmp_path):
    _write_toml(tmp_path)
    with pytest.raises(AssetConfigError, match="not found in pipeline.toml"):
        load_asset_target_config(tmp_path, "nonexistent")


def test_load_config_missing_catalogue_manifest_raises(tmp_path):
    toml_no_manifest = textwrap.dedent("""\
        [targets.bare]
        project = "x.vcxproj"
        app_name = "Bare"
    """)
    _write_toml(tmp_path, toml_no_manifest)
    with pytest.raises(AssetConfigError, match="catalogue_manifest"):
        load_asset_target_config(tmp_path, "bare")


def test_load_config_empty_asset_stages_ok(tmp_path):
    _write_toml(tmp_path)
    cfg = load_asset_target_config(tmp_path, "noassets")
    assert cfg.asset_stages == []


def test_load_config_app_name_defaults_to_target(tmp_path):
    toml = textwrap.dedent("""\
        [targets.mytarget]
        project = "x.vcxproj"
        catalogue_manifest = "Assets/mytarget/assets.catalogue.json"
    """)
    _write_toml(tmp_path, toml)
    cfg = load_asset_target_config(tmp_path, "mytarget")
    assert cfg.app_name == "mytarget"


# ---------------------------------------------------------------------------
# Exit codes via _common_handler (mocked runner)
# ---------------------------------------------------------------------------

def _run_via_handler(tmp_path, phases, runner_return=0, force=False):
    """Helper: invoke run_asset_phases with a mocked BuildRunner."""
    _write_toml(tmp_path)
    _write_catalogue(tmp_path)
    import click
    from dia_cli.commands.asset._common_handler import run_asset_phases

    ctx = click.Context(click.Command("test"))

    with patch("dia_cli.commands.asset._common_handler.BuildRunner") as MockRunner:
        instance = MockRunner.return_value
        instance.run.return_value = runner_return
        code = run_asset_phases(
            target="cluichetest",
            config="Debug",
            platform="x64",
            force=force,
            phases=phases,
            ctx=ctx,
            repo_root=tmp_path,
        )
    return code


def test_exit_code_0_on_success(tmp_path):
    assert _run_via_handler(tmp_path, ["validate", "transform", "deploy"], runner_return=0) == 0


def test_exit_code_1_on_asset_failure(tmp_path):
    assert _run_via_handler(tmp_path, ["validate", "transform", "deploy"], runner_return=1) == 1


def test_exit_code_2_when_toml_missing(tmp_path):
    import click
    from dia_cli.commands.asset._common_handler import run_asset_phases
    ctx = click.Context(click.Command("test"))
    # no pipeline.toml written
    code = run_asset_phases(
        target="cluichetest", config="Debug", platform="x64",
        force=False, phases=["validate"], ctx=ctx, repo_root=tmp_path,
    )
    assert code == 2


def test_exit_code_2_when_catalogue_missing(tmp_path):
    _write_toml(tmp_path)
    # catalogue file NOT written
    import click
    from dia_cli.commands.asset._common_handler import run_asset_phases
    ctx = click.Context(click.Command("test"))
    code = run_asset_phases(
        target="cluichetest", config="Debug", platform="x64",
        force=False, phases=["validate"], ctx=ctx, repo_root=tmp_path,
    )
    assert code == 2


# ---------------------------------------------------------------------------
# Phase selection
# ---------------------------------------------------------------------------

def test_validate_only_passes_validate_phase(tmp_path):
    _write_toml(tmp_path)
    _write_catalogue(tmp_path)
    import click
    from dia_cli.commands.asset._common_handler import run_asset_phases

    ctx = click.Context(click.Command("test"))
    captured_phases = []

    with patch("dia_cli.commands.asset._common_handler.BuildRunner") as MockRunner:
        def fake_run(phases=None):
            captured_phases.extend(phases or [])
            return 0
        MockRunner.return_value.run.side_effect = fake_run
        run_asset_phases(
            target="cluichetest", config="Debug", platform="x64",
            force=False, phases=["validate"], ctx=ctx, repo_root=tmp_path,
        )

    assert captured_phases == ["validate"]


def test_deploy_only_passes_deploy_phase(tmp_path):
    _write_toml(tmp_path)
    _write_catalogue(tmp_path)
    import click
    from dia_cli.commands.asset._common_handler import run_asset_phases

    ctx = click.Context(click.Command("test"))
    captured_phases = []

    with patch("dia_cli.commands.asset._common_handler.BuildRunner") as MockRunner:
        def fake_run(phases=None):
            captured_phases.extend(phases or [])
            return 0
        MockRunner.return_value.run.side_effect = fake_run
        run_asset_phases(
            target="cluichetest", config="Debug", platform="x64",
            force=False, phases=["deploy"], ctx=ctx, repo_root=tmp_path,
        )

    assert captured_phases == ["deploy"]


# ---------------------------------------------------------------------------
# --force cleans deploy directory
# ---------------------------------------------------------------------------

def test_force_removes_deploy_directory(tmp_path):
    _write_toml(tmp_path)
    _write_catalogue(tmp_path)

    # Pre-create a deploy directory with a sentinel file
    deploy_root = tmp_path / "bin" / "CluicheTest" / "Debug" / "x64" / "assets"
    deploy_root.mkdir(parents=True)
    sentinel = deploy_root / "old_file.dat"
    sentinel.write_bytes(b"stale")

    import click
    from dia_cli.commands.asset._common_handler import run_asset_phases

    ctx = click.Context(click.Command("test"))
    with patch("dia_cli.commands.asset._common_handler.BuildRunner") as MockRunner:
        MockRunner.return_value.run.return_value = 0
        run_asset_phases(
            target="cluichetest", config="Debug", platform="x64",
            force=True, phases=["deploy"], ctx=ctx, repo_root=tmp_path,
        )

    assert not sentinel.exists()


def test_no_force_preserves_deploy_directory(tmp_path):
    _write_toml(tmp_path)
    _write_catalogue(tmp_path)

    deploy_root = tmp_path / "bin" / "CluicheTest" / "Debug" / "x64" / "assets"
    deploy_root.mkdir(parents=True)
    sentinel = deploy_root / "keep_me.dat"
    sentinel.write_bytes(b"keep")

    import click
    from dia_cli.commands.asset._common_handler import run_asset_phases

    ctx = click.Context(click.Command("test"))
    with patch("dia_cli.commands.asset._common_handler.BuildRunner") as MockRunner:
        MockRunner.return_value.run.return_value = 0
        run_asset_phases(
            target="cluichetest", config="Debug", platform="x64",
            force=False, phases=["deploy"], ctx=ctx, repo_root=tmp_path,
        )

    assert sentinel.exists()
