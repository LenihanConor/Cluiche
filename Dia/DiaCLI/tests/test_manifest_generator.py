"""Unit tests for RuntimeManifestGenerator."""
from __future__ import annotations

import json
from pathlib import Path
from unittest.mock import MagicMock

import pytest

from dia_cli.commands.asset.context import BuildContext
from dia_cli.commands.asset.manifest_generator import RuntimeManifestGenerator


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _make_context(tmp_path: Path, catalogue: dict | None = None) -> BuildContext:
    return BuildContext(
        catalogue=catalogue or {"assets": []},
        config="Debug",
        platform="x64",
        app_name="CluicheTest",
        deploy_root=tmp_path / "assets",
        asset_stages=[],
        output=MagicMock(),
        source_root=tmp_path,
    )


def _make_stage_record(stage_id: str, members: list[str]) -> dict:
    return {
        "id": stage_id,
        "type": "stage",
        "source_path": "",
        "scope": "global",
        "stage_name": "",
        "tags": [],
        "references": [{"type": "contains", "target": m} for m in members],
    }


def _read_manifest(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


# ---------------------------------------------------------------------------
# add_asset / generate — assets array
# ---------------------------------------------------------------------------

def test_generate_writes_file(tmp_path):
    ctx = _make_context(tmp_path)
    gen = RuntimeManifestGenerator(ctx)
    out = gen.generate()
    assert out.exists()
    assert out.name == "assets.runtime.json"


def test_generate_includes_version_and_app_name(tmp_path):
    ctx = _make_context(tmp_path)
    gen = RuntimeManifestGenerator(ctx)
    out = gen.generate()
    data = _read_manifest(out)
    assert data["version"] == 1
    assert data["app_name"] == "CluicheTest"


def test_add_asset_appears_in_manifest(tmp_path):
    ctx = _make_context(tmp_path)
    deploy_root = ctx.deploy_root
    deploy_root.mkdir(parents=True, exist_ok=True)

    gen = RuntimeManifestGenerator(ctx)
    gen.add_asset("texture.hero", "global", deploy_root / "global" / "characters" / "hero.png")
    out = gen.generate()
    data = _read_manifest(out)

    assert len(data["assets"]) == 1
    entry = data["assets"][0]
    assert entry["id"] == "texture.hero"
    assert entry["scope"] == "global"
    assert entry["deploy_path"] == "global/characters/hero.png"


def test_deploy_path_is_relative_to_deploy_root(tmp_path):
    ctx = _make_context(tmp_path)
    deploy_root = ctx.deploy_root
    deploy_root.mkdir(parents=True, exist_ok=True)

    gen = RuntimeManifestGenerator(ctx)
    gen.add_asset("config.hud", "stage", deploy_root / "stages" / "menu" / "Presentation/UI" / "hud.json")
    out = gen.generate()
    data = _read_manifest(out)
    assert data["assets"][0]["deploy_path"] == "stages/menu/Presentation/UI/hud.json"


def test_multiple_assets_all_appear(tmp_path):
    ctx = _make_context(tmp_path)
    deploy_root = ctx.deploy_root
    deploy_root.mkdir(parents=True, exist_ok=True)

    gen = RuntimeManifestGenerator(ctx)
    gen.add_asset("texture.a", "global", deploy_root / "global" / "misc" / "a.png")
    gen.add_asset("texture.b", "global", deploy_root / "global" / "misc" / "b.png")
    out = gen.generate()
    data = _read_manifest(out)
    ids = [e["id"] for e in data["assets"]]
    assert "texture.a" in ids
    assert "texture.b" in ids


def test_manifest_has_no_source_paths_or_hashes(tmp_path):
    ctx = _make_context(tmp_path)
    deploy_root = ctx.deploy_root
    deploy_root.mkdir(parents=True, exist_ok=True)

    gen = RuntimeManifestGenerator(ctx)
    gen.add_asset("texture.hero", "global", deploy_root / "global" / "misc" / "hero.png")
    out = gen.generate()
    data = _read_manifest(out)
    entry = data["assets"][0]
    assert "source_path" not in entry
    assert "content_hash" not in entry
    assert "tags" not in entry
    assert "type" not in entry


# ---------------------------------------------------------------------------
# build_stages — stages array
# ---------------------------------------------------------------------------

def test_build_stages_creates_stage_entry(tmp_path):
    catalogue = {
        "assets": [
            _make_stage_record("stage.menu", ["texture.bg", "audio.music"]),
        ]
    }
    ctx = _make_context(tmp_path, catalogue=catalogue)
    deploy_root = ctx.deploy_root
    deploy_root.mkdir(parents=True, exist_ok=True)

    gen = RuntimeManifestGenerator(ctx)
    gen.add_asset("texture.bg", "global", deploy_root / "global" / "misc" / "bg.png")
    gen.add_asset("audio.music", "stage", deploy_root / "stages" / "menu" / "misc" / "music.ogg")
    gen.build_stages()
    out = gen.generate()
    data = _read_manifest(out)

    assert len(data["stages"]) == 1
    stage = data["stages"][0]
    assert stage["id"] == "stage.menu"
    assert set(stage["assets"]) == {"texture.bg", "audio.music"}


def test_build_stages_only_includes_deployed_assets(tmp_path):
    """Assets referenced by a stage but not deployed must not appear in stage assets."""
    catalogue = {
        "assets": [
            _make_stage_record("stage.menu", ["texture.bg", "texture.missing"]),
        ]
    }
    ctx = _make_context(tmp_path, catalogue=catalogue)
    deploy_root = ctx.deploy_root
    deploy_root.mkdir(parents=True, exist_ok=True)

    gen = RuntimeManifestGenerator(ctx)
    gen.add_asset("texture.bg", "global", deploy_root / "global" / "misc" / "bg.png")
    # texture.missing intentionally not added
    gen.build_stages()
    out = gen.generate()
    data = _read_manifest(out)

    stage = data["stages"][0]
    assert "texture.bg" in stage["assets"]
    assert "texture.missing" not in stage["assets"]


def test_global_asset_appears_in_referencing_stage(tmp_path):
    """A global asset referenced by a stage must appear in that stage's assets array."""
    catalogue = {
        "assets": [
            _make_stage_record("stage.level1", ["texture.hero"]),
        ]
    }
    ctx = _make_context(tmp_path, catalogue=catalogue)
    deploy_root = ctx.deploy_root
    deploy_root.mkdir(parents=True, exist_ok=True)

    gen = RuntimeManifestGenerator(ctx)
    gen.add_asset("texture.hero", "global", deploy_root / "global" / "characters" / "hero.png")
    gen.build_stages()
    out = gen.generate()
    data = _read_manifest(out)

    stage = data["stages"][0]
    assert "texture.hero" in stage["assets"]


def test_empty_catalogue_produces_empty_stages(tmp_path):
    ctx = _make_context(tmp_path, catalogue={"assets": []})
    deploy_root = ctx.deploy_root
    deploy_root.mkdir(parents=True, exist_ok=True)

    gen = RuntimeManifestGenerator(ctx)
    gen.build_stages()
    out = gen.generate()
    data = _read_manifest(out)
    assert data["stages"] == []


def test_generate_creates_parent_dirs(tmp_path):
    """deploy_root does not need to exist before generate() is called."""
    ctx = _make_context(tmp_path)
    # deploy_root not created — generate must create it
    gen = RuntimeManifestGenerator(ctx)
    out = gen.generate()
    assert out.exists()


def test_folder_asset_deploy_path_has_trailing_slash(tmp_path):
    ctx = _make_context(tmp_path)
    deploy_root = ctx.deploy_root
    deploy_root.mkdir(parents=True, exist_ok=True)

    gen = RuntimeManifestGenerator(ctx)
    gen.add_asset(
        "folder.art_pack", "global",
        deploy_root / "global" / "characters" / "art_pack.folder",
        is_folder=True,
    )
    out = gen.generate()
    data = _read_manifest(out)
    entry = data["assets"][0]
    assert entry["deploy_path"].endswith("/")
