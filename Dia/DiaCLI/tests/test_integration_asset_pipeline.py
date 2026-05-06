"""End-to-end integration test for the asset pipeline.

Exercises the full flow: catalogue on disk → validate → transform → deploy → manifest,
using real files and the real BuildRunner + built-in handlers + OutputContext NDJSON logging.
"""
from __future__ import annotations

import json
from pathlib import Path

import pytest

from dia_cli.commands.asset.context import BuildContext
from dia_cli.commands.asset.handlers import register_built_in_handlers
from dia_cli.commands.asset.registry import AssetHandlerRegistry
from dia_cli.commands.asset.runner import BuildRunner
from dia_cli.utils.dia_output import OutputContext


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _write_catalogue(path: Path, assets: list[dict]) -> Path:
    cat_path = path / "assets.catalogue.json"
    cat_path.write_text(json.dumps({"assets": assets}), encoding="utf-8")
    return cat_path


def _make_stage_record(stage_id: str, members: list[str], source_path: str) -> dict:
    return {
        "id": stage_id,
        "type": "stage",
        "source_path": source_path,
        "content_hash": 0,
        "status": "active",
        "scope": "global",
        "stage_name": "",
        "tags": [],
        "references": [{"type": "contains", "target": m} for m in members],
    }


def _make_asset_record(
    asset_id: str,
    source_path: str,
    scope: str = "global",
    stage_name: str = "",
    tags: list[str] | None = None,
) -> dict:
    return {
        "id": asset_id,
        "type": asset_id.split(".")[0],
        "source_path": source_path,
        "content_hash": 0,
        "status": "active",
        "scope": scope,
        "stage_name": stage_name,
        "tags": tags or [],
        "references": [],
    }


def _read_ndjson(path: Path) -> list[dict]:
    lines = path.read_text(encoding="utf-8").strip().splitlines()
    return [json.loads(line) for line in lines]


# ---------------------------------------------------------------------------
# Full pipeline round-trip
# ---------------------------------------------------------------------------

def test_full_pipeline_round_trip(tmp_path):
    """Validate → transform → deploy a mixed catalogue and verify outputs."""
    source_root = tmp_path / "Assets" / "TestApp"
    source_root.mkdir(parents=True)

    # Create source files
    (source_root / "hero.texture.png").write_bytes(b"\x89PNG_hero_image_data")
    (source_root / "config.json").write_bytes(b'{"key": "value"}')
    (source_root / "menu.stage.json").write_text(
        json.dumps({"name": "Main Menu", "assets": []}), encoding="utf-8"
    )
    art_pack = source_root / "art_pack"
    art_pack.mkdir()
    (art_pack / "sprite1.png").write_bytes(b"sprite1")
    (art_pack / "sprite2.png").write_bytes(b"sprite2")

    # Build catalogue
    assets = [
        _make_asset_record("texture.hero", "hero.texture.png", tags=["characters"]),
        _make_asset_record(
            "config.hud", "config.json",
            scope="stage", stage_name="menu", tags=["Presentation/UI"],
        ),
        _make_asset_record("folder.art_pack", str(art_pack), tags=["characters"]),
        _make_stage_record("stage.menu", ["texture.hero", "config.hud"], "menu.stage.json"),
    ]
    catalogue_path = _write_catalogue(source_root, assets)
    catalogue = json.loads(catalogue_path.read_text(encoding="utf-8"))

    # Set up output context with NDJSON logging
    log_dir = tmp_path / "logs"
    output = OutputContext(log_dir=log_dir, quiet=True)

    deploy_root = tmp_path / "bin" / "TestApp" / "Debug" / "x64" / "assets"

    context = BuildContext(
        catalogue=catalogue,
        config="Debug",
        platform="x64",
        app_name="TestApp",
        deploy_root=deploy_root,
        asset_stages=["stage.menu"],
        output=output,
        source_root=source_root,
    )

    registry = AssetHandlerRegistry()
    register_built_in_handlers(registry)

    runner = BuildRunner(registry, context)
    result = runner.run_with_result()

    # --- Assertions ---

    # Exit code
    assert result.exit_code == 0, "Pipeline should succeed with valid source files"
    assert result.pass_count == 4  # texture, config, folder, stage
    assert result.fail_count == 0

    # Deployed files exist at correct paths
    hero_path = deploy_root / "global" / "characters" / "hero.texture.png"
    assert hero_path.exists(), f"Expected deployed texture at {hero_path}"
    assert hero_path.read_bytes() == b"\x89PNG_hero_image_data"

    config_path = deploy_root / "stages" / "menu" / "Presentation/UI" / "config.json"
    assert config_path.exists(), f"Expected deployed config at {config_path}"

    # Folder assets are deployed as directory trees
    folder_path = deploy_root / "global" / "characters" / "art_pack"
    assert folder_path.is_dir(), f"Expected deployed folder at {folder_path}"
    assert (folder_path / "sprite1.png").exists()
    assert (folder_path / "sprite2.png").exists()

    # Runtime manifest generated
    manifest_path = deploy_root / "assets.runtime.json"
    assert manifest_path.exists(), "Runtime manifest should be generated"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))

    assert manifest["version"] == 1
    assert manifest["app_name"] == "TestApp"
    assert len(manifest["assets"]) == 4  # texture, config, folder, stage
    asset_ids = {a["id"] for a in manifest["assets"]}
    assert "texture.hero" in asset_ids
    assert "config.hud" in asset_ids
    assert "folder.art_pack" in asset_ids
    assert "stage.menu" in asset_ids

    # Folder asset has trailing slash in deploy_path
    folder_entry = next(a for a in manifest["assets"] if a["id"] == "folder.art_pack")
    assert folder_entry["deploy_path"].endswith("/")

    # Stages array references correct members
    assert len(manifest["stages"]) == 1
    stage = manifest["stages"][0]
    assert stage["id"] == "stage.menu"
    assert "texture.hero" in stage["assets"]
    assert "config.hud" in stage["assets"]

    # NDJSON log file created
    ndjson_path = log_dir / "asset-pipeline" / "last-run.ndjson"
    assert ndjson_path.exists(), "NDJSON log should be written"
    events = _read_ndjson(ndjson_path)
    assert len(events) > 0
    event_types = [e["event"] for e in events]
    assert "OnRunStarted" in event_types
    assert "OnAssetValidated" in event_types
    assert "OnAssetTransformed" in event_types
    assert "OnAssetDeployed" in event_types
    assert "OnBuildCompleted" in event_types
    assert "OnRunCompleted" in event_types


def test_pipeline_with_validation_failure(tmp_path):
    """A missing source file should cause validate failure but not crash."""
    source_root = tmp_path / "Assets" / "TestApp"
    source_root.mkdir(parents=True)

    # texture.hero references a file that does NOT exist
    assets = [
        _make_asset_record("texture.hero", "missing.png", tags=["characters"]),
    ]
    catalogue_path = _write_catalogue(source_root, assets)
    catalogue = json.loads(catalogue_path.read_text(encoding="utf-8"))

    log_dir = tmp_path / "logs"
    output = OutputContext(log_dir=log_dir, quiet=True)
    deploy_root = tmp_path / "deploy" / "assets"

    context = BuildContext(
        catalogue=catalogue,
        config="Debug",
        platform="x64",
        app_name="TestApp",
        deploy_root=deploy_root,
        asset_stages=[],
        output=output,
        source_root=source_root,
    )

    registry = AssetHandlerRegistry()
    register_built_in_handlers(registry)

    runner = BuildRunner(registry, context)
    result = runner.run_with_result()

    assert result.exit_code == 1
    assert result.fail_count == 1
    assert result.pass_count == 0

    # No manifest generated (nothing deployed)
    assert not (deploy_root / "assets.runtime.json").exists()

    # NDJSON log shows failure event
    ndjson_path = log_dir / "asset-pipeline" / "last-run.ndjson"
    events = _read_ndjson(ndjson_path)
    event_types = [e["event"] for e in events]
    assert "OnAssetFailed" in event_types
    assert "OnRunFailed" in event_types


def test_pipeline_stage_filtering_excludes_unneeded(tmp_path):
    """Stage-scoped assets not in asset_stages list are excluded from processing."""
    source_root = tmp_path / "Assets" / "TestApp"
    source_root.mkdir(parents=True)

    (source_root / "bg.texture.png").write_bytes(b"bg")
    (source_root / "enemy.texture.png").write_bytes(b"enemy")
    (source_root / "menu.stage.json").write_text(
        json.dumps({"name": "Menu"}), encoding="utf-8"
    )
    (source_root / "level1.stage.json").write_text(
        json.dumps({"name": "Level 1"}), encoding="utf-8"
    )

    assets = [
        _make_asset_record("texture.bg", "bg.texture.png", scope="stage", stage_name="menu", tags=["characters"]),
        _make_asset_record("texture.enemy", "enemy.texture.png", scope="stage", stage_name="level1", tags=["characters"]),
        _make_stage_record("stage.menu", ["texture.bg"], "menu.stage.json"),
        _make_stage_record("stage.level1", ["texture.enemy"], "level1.stage.json"),
    ]
    catalogue_path = _write_catalogue(source_root, assets)
    catalogue = json.loads(catalogue_path.read_text(encoding="utf-8"))

    log_dir = tmp_path / "logs"
    output = OutputContext(log_dir=log_dir, quiet=True)
    deploy_root = tmp_path / "deploy" / "assets"

    context = BuildContext(
        catalogue=catalogue,
        config="Debug",
        platform="x64",
        app_name="TestApp",
        deploy_root=deploy_root,
        asset_stages=["stage.menu"],  # only menu stage
        output=output,
        source_root=source_root,
    )

    registry = AssetHandlerRegistry()
    register_built_in_handlers(registry)

    runner = BuildRunner(registry, context)
    result = runner.run_with_result()

    assert result.exit_code == 0
    # stage.menu + texture.bg processed, stage.level1 processed (global scope), texture.enemy excluded
    assert result.pass_count == 3

    # Only texture.bg deployed (texture.enemy filtered out)
    bg_deployed = deploy_root / "stages" / "menu" / "characters" / "bg.texture.png"
    assert bg_deployed.exists()
    enemy_path = deploy_root / "stages" / "level1" / "characters" / "enemy.texture.png"
    assert not enemy_path.exists()


def test_pipeline_empty_catalogue(tmp_path):
    """Empty catalogue should succeed with 0 assets processed and no manifest."""
    source_root = tmp_path / "Assets" / "TestApp"
    source_root.mkdir(parents=True)

    catalogue_path = _write_catalogue(source_root, [])
    catalogue = json.loads(catalogue_path.read_text(encoding="utf-8"))

    log_dir = tmp_path / "logs"
    output = OutputContext(log_dir=log_dir, quiet=True)
    deploy_root = tmp_path / "deploy" / "assets"

    context = BuildContext(
        catalogue=catalogue,
        config="Debug",
        platform="x64",
        app_name="TestApp",
        deploy_root=deploy_root,
        asset_stages=[],
        output=output,
        source_root=source_root,
    )

    registry = AssetHandlerRegistry()
    register_built_in_handlers(registry)

    runner = BuildRunner(registry, context)
    result = runner.run_with_result()

    assert result.exit_code == 0
    assert result.pass_count == 0
    assert result.fail_count == 0
    assert not (deploy_root / "assets.runtime.json").exists()
