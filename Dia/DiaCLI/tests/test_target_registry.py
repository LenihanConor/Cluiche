"""Data-driven target registry: launch/deploy target lists derive from pipeline.toml."""
from __future__ import annotations

from pathlib import Path

import click
import pytest
from click.testing import CliRunner

from dia_cli.commands.pipeline.pipeline_config import (
    load_pipeline_config,
    launchable_targets,
    resolve_target_exe,
)

REPO_ROOT = Path(__file__).resolve().parents[3]

_TOML = """
[global]
default_target = "alpha"

[targets.alpha]
project = "Cluiche/Alpha/Alpha.vcxproj"
stages = ["compile-code", "deploy"]
app_name = "Alpha"

[targets.beta]
project = "Cluiche/Beta/Beta.vcxproj"
stages = ["compile-code"]
exe = "Cluiche/bin/Custom/{config}/x64/Beta.exe"

[targets.libonly]
project = "Dia/Lib/Lib.vcxproj"
stages = ["compile-code"]
"""


def _write_repo(tmp_path: Path, toml_text: str = _TOML) -> Path:
    (tmp_path / "pipeline.toml").write_text(toml_text, encoding="utf-8")
    return tmp_path


# --- AC1: existing targets resolve exactly as the old hard-coded maps did ---

@pytest.mark.parametrize("target,expected", [
    ("googletest", "Cluiche/bin/GoogleTests/{config}/x64/GoogleTests.exe"),
    ("cluichetest", "Cluiche/bin/CluicheTest/{config}/x64/CluicheTest.exe"),
    ("cluicheeditor", "Cluiche/bin/CluicheEditor/{config}/x64/CluicheEditor.exe"),
])
def test_existing_targets_resolve_unchanged(target, expected):
    cfg = load_pipeline_config(REPO_ROOT)
    assert resolve_target_exe(cfg.targets[target]) == expected


def test_existing_launchable_set_unchanged():
    cfg = load_pipeline_config(REPO_ROOT)
    launchable = set(launchable_targets(cfg))
    assert {"googletest", "cluichetest", "cluicheeditor"} <= launchable
    assert "diauiultralight" not in launchable  # library, no executable


def test_existing_deploy_targets_unchanged():
    from dia_cli.commands.pipeline.deploy_runner import deploy_group
    names = set(deploy_group.list_commands(click.Context(deploy_group)))
    assert {"cluichetest", "googletest", "cluicheeditor", "diauiultralight"} <= names


# --- AC2: a new [targets.x] block is recognised with no Python edit ---

def test_app_name_target_derives_default_exe(tmp_path):
    cfg = load_pipeline_config(_write_repo(tmp_path))
    assert resolve_target_exe(cfg.targets["alpha"]) == "Cluiche/bin/Alpha/{config}/x64/Alpha.exe"


def test_explicit_exe_wins(tmp_path):
    cfg = load_pipeline_config(_write_repo(tmp_path))
    assert resolve_target_exe(cfg.targets["beta"]) == "Cluiche/bin/Custom/{config}/x64/Beta.exe"


def test_target_without_exe_or_app_name_is_not_launchable(tmp_path):
    cfg = load_pipeline_config(_write_repo(tmp_path))
    assert sorted(launchable_targets(cfg)) == ["alpha", "beta"]
    assert resolve_target_exe(cfg.targets["libonly"]) is None


def test_launch_recognises_new_target(tmp_path, monkeypatch):
    from dia_cli.cli import launch

    repo = _write_repo(tmp_path)
    exe = repo / "Cluiche/bin/Alpha/Debug/x64/Alpha.exe"
    exe.parent.mkdir(parents=True)
    exe.write_text("")
    monkeypatch.setattr(launch, "find_repo_root", lambda _anchor: repo)

    calls = []

    class _Result:
        returncode = 0

    monkeypatch.setattr(launch.subprocess, "run",
                        lambda cmd, cwd=None: calls.append((cmd, cwd)) or _Result())
    assert launch.launch_target("alpha", "Debug") == 0
    assert calls[0][0] == [str(exe)]
    assert Path(calls[0][1]) == exe.parent


def test_deploy_group_lists_new_target(tmp_path, monkeypatch):
    from dia_cli.commands.pipeline import deploy_runner

    repo = _write_repo(tmp_path)
    monkeypatch.setattr(deploy_runner, "find_repo_root", lambda _anchor: repo)
    ctx = click.Context(deploy_runner.deploy_group)
    assert set(deploy_runner.deploy_group.list_commands(ctx)) == {"alpha", "beta", "libonly"}
    assert deploy_runner.deploy_group.get_command(ctx, "alpha") is not None
    assert deploy_runner.deploy_group.get_command(ctx, "nope") is None


# --- AC3: unknown-target errors list toml targets ---

def test_launch_unknown_target_lists_toml_targets(tmp_path, monkeypatch, capsys):
    from dia_cli.cli import launch

    monkeypatch.setattr(launch, "find_repo_root", lambda _anchor: _write_repo(tmp_path))
    assert launch.launch_target("zzz", "Debug") == 2
    err = capsys.readouterr().err
    assert "unknown target 'zzz'" in err
    assert "alpha" in err and "beta" in err
    assert "libonly" not in err  # not launchable
    assert "googletest" not in err  # list comes from the toml, not a hard-coded map


# --- skeleton B1: the cow target is toml-only ---

def test_cow_target_resolves_and_deploys():
    from dia_cli.commands.pipeline.deploy_runner import deploy_group

    cfg = load_pipeline_config(REPO_ROOT)
    cow = cfg.targets["cow"]
    assert resolve_target_exe(cow) == "Cluiche/bin/CoW/{config}/x64/CoW.exe"
    assert "cow" in launchable_targets(cfg)
    assert cow.project == "Cluiche/CoW/CoW.vcxproj"
    assert cow.stages == ["compile-code", "build-assets", "deploy"]
    assert cow.build_deps.protobuf and cow.build_deps.bgfx_shaders
    import toml
    raw = toml.load(str(REPO_ROOT / "pipeline.toml"))["targets"]["cow"]
    assert raw["catalogue_manifest"] == "Cluiche/Assets/CoW/assets.catalogue.json"
    assert any("Stages/**" in f.src for f in cow.deploy.files)
    assert "cow" in deploy_group.list_commands(click.Context(deploy_group))
