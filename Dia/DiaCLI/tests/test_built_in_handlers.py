"""Unit tests for built-in asset type handlers."""
from __future__ import annotations

import json
from pathlib import Path
from unittest.mock import MagicMock

import pytest

from dia_cli.commands.asset.context import BuildContext
from dia_cli.commands.asset.handler import AssetError
from dia_cli.commands.asset.handlers import (
    AudioHandler,
    ConfigHandler,
    DefaultAssetHandler,
    EntityHandler,
    FolderHandler,
    SpriteHandler,
    StageHandler,
    TextureHandler,
    UIHandler,
    register_built_in_handlers,
)
from dia_cli.commands.asset.registry import AssetHandlerRegistry


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _make_context(tmp_path: Path) -> BuildContext:
    return BuildContext(
        catalogue={},
        config="Debug",
        platform="x64",
        app_name="TestApp",
        deploy_root=tmp_path / "assets",
        asset_stages=[],
        output=MagicMock(),
        source_root=tmp_path,
    )


def _make_record(
    asset_id: str,
    source_path: str = "",
    scope: str = "global",
    stage_name: str = "",
    tags: list[str] | None = None,
) -> dict:
    return {
        "id": asset_id,
        "type": asset_id.split(".")[0],
        "source_path": source_path,
        "scope": scope,
        "stage_name": stage_name,
        "tags": tags or [],
    }


# ---------------------------------------------------------------------------
# register_built_in_handlers
# ---------------------------------------------------------------------------

def test_all_8_types_registered():
    reg = AssetHandlerRegistry()
    register_built_in_handlers(reg)
    for type_id in ("texture", "sprite", "audio", "config", "entity", "stage", "ui", "folder"):
        assert reg.get(type_id) is not None, f"Missing handler for '{type_id}'"


def test_type_ids_match_handler_classes():
    assert TextureHandler.type_id == "texture"
    assert SpriteHandler.type_id == "sprite"
    assert AudioHandler.type_id == "audio"
    assert ConfigHandler.type_id == "config"
    assert EntityHandler.type_id == "entity"
    assert StageHandler.type_id == "stage"
    assert UIHandler.type_id == "ui"
    assert FolderHandler.type_id == "folder"


def test_register_twice_raises():
    reg = AssetHandlerRegistry()
    register_built_in_handlers(reg)
    with pytest.raises(ValueError, match="Duplicate handler"):
        register_built_in_handlers(reg)


# ---------------------------------------------------------------------------
# DefaultAssetHandler — validate
# ---------------------------------------------------------------------------

def test_default_validate_passes_when_file_exists(tmp_path):
    src = tmp_path / "hero.texture.png"
    src.write_bytes(b"img")
    ctx = _make_context(tmp_path)
    record = _make_record("texture.hero", source_path=str(src))
    errors = DefaultAssetHandler().validate(record, ctx)
    assert errors == []


def test_default_validate_error_when_file_missing(tmp_path):
    ctx = _make_context(tmp_path)
    record = _make_record("texture.hero", source_path=str(tmp_path / "missing.png"))
    errors = DefaultAssetHandler().validate(record, ctx)
    assert len(errors) == 1
    assert errors[0].phase == "validate"
    assert "not found" in errors[0].message


def test_default_validate_pattern_mismatch_is_warning_not_error(tmp_path):
    src = tmp_path / "hero.png"  # missing ".texture" segment
    src.write_bytes(b"img")
    ctx = _make_context(tmp_path)
    record = _make_record("texture.hero", source_path=str(src))
    handler = TextureHandler()
    errors = handler.validate(record, ctx)
    assert errors == []  # no error
    ctx.output.warn.assert_called_once()  # but a warning was emitted


def test_default_validate_no_pattern_no_warning(tmp_path):
    src = tmp_path / "anything.bin"
    src.write_bytes(b"x")
    ctx = _make_context(tmp_path)
    record = _make_record("config.x", source_path=str(src))
    handler = DefaultAssetHandler()  # no file_pattern
    errors = handler.validate(record, ctx)
    assert errors == []
    ctx.output.warn.assert_not_called()


# ---------------------------------------------------------------------------
# DefaultAssetHandler — transform
# ---------------------------------------------------------------------------

def test_default_transform_returns_source_path(tmp_path):
    ctx = _make_context(tmp_path)
    record = _make_record("texture.hero", source_path="Raw/hero.texture.png")
    result = DefaultAssetHandler().transform(record, ctx)
    assert result.success is True
    assert Path(result.output_path) == ctx.source_root / "Raw" / "hero.texture.png"


# ---------------------------------------------------------------------------
# DefaultAssetHandler — deploy
# ---------------------------------------------------------------------------

def test_default_deploy_copies_file(tmp_path):
    src = tmp_path / "hero.texture.png"
    src.write_bytes(b"img")
    ctx = _make_context(tmp_path)
    ctx.deploy_root.mkdir(parents=True, exist_ok=True)
    record = _make_record("texture.hero", source_path=str(src), tags=["characters"])
    result = TextureHandler().deploy(record, ctx)
    assert result.success is True
    dest = Path(result.deploy_path)
    assert dest.exists()
    assert dest.read_bytes() == b"img"


def test_default_deploy_failure_returns_error(tmp_path):
    ctx = _make_context(tmp_path)
    record = _make_record("texture.hero", source_path=str(tmp_path / "nonexistent.png"))
    result = DefaultAssetHandler().deploy(record, ctx)
    assert result.success is False
    assert len(result.errors) == 1
    assert result.errors[0].phase == "deploy"


# ---------------------------------------------------------------------------
# FolderHandler — validate
# ---------------------------------------------------------------------------

def test_folder_validate_passes_for_directory(tmp_path):
    src_dir = tmp_path / "art_pack"
    src_dir.mkdir()
    ctx = _make_context(tmp_path)
    record = _make_record("folder.art_pack", source_path=str(src_dir))
    errors = FolderHandler().validate(record, ctx)
    assert errors == []


def test_folder_validate_error_for_file(tmp_path):
    src_file = tmp_path / "art_pack.folder"
    src_file.write_bytes(b"not a dir")
    ctx = _make_context(tmp_path)
    record = _make_record("folder.art_pack", source_path=str(src_file))
    errors = FolderHandler().validate(record, ctx)
    assert len(errors) == 1
    assert "not a directory" in errors[0].message


def test_folder_validate_error_for_missing_path(tmp_path):
    ctx = _make_context(tmp_path)
    record = _make_record("folder.art_pack", source_path=str(tmp_path / "nonexistent"))
    errors = FolderHandler().validate(record, ctx)
    assert len(errors) == 1


# ---------------------------------------------------------------------------
# FolderHandler — transform
# ---------------------------------------------------------------------------

def test_folder_transform_is_noop(tmp_path):
    ctx = _make_context(tmp_path)
    record = _make_record("folder.art_pack", source_path="Raw/art_pack")
    result = FolderHandler().transform(record, ctx)
    assert result.success is True
    assert Path(result.output_path) == ctx.source_root / "Raw" / "art_pack"


# ---------------------------------------------------------------------------
# FolderHandler — deploy (recursive copy)
# ---------------------------------------------------------------------------

def test_folder_deploy_copies_tree(tmp_path):
    src = tmp_path / "art_pack"
    src.mkdir()
    (src / "hero.png").write_bytes(b"hero")
    (src / "sub").mkdir()
    (src / "sub" / "icon.png").write_bytes(b"icon")

    ctx = _make_context(tmp_path)
    ctx.deploy_root.mkdir(parents=True, exist_ok=True)
    record = {
        "id": "folder.art_pack",
        "type": "folder",
        "source_path": str(src),
        "scope": "global",
        "stage_name": "",
        "tags": ["characters"],
    }
    result = FolderHandler().deploy(record, ctx)
    assert result.success is True
    dest = Path(result.deploy_path)
    assert (dest / "hero.png").exists()
    assert (dest / "sub" / "icon.png").exists()


# ---------------------------------------------------------------------------
# StageHandler — validate
# ---------------------------------------------------------------------------

def test_stage_validate_passes_with_name(tmp_path):
    src = tmp_path / "main_menu.stage.json"
    src.write_text(json.dumps({"name": "Main Menu", "assets": []}), encoding="utf-8")
    ctx = _make_context(tmp_path)
    record = _make_record("stage.main_menu", source_path=str(src))
    errors = StageHandler().validate(record, ctx)
    assert errors == []


def test_stage_validate_error_when_name_missing(tmp_path):
    src = tmp_path / "main_menu.stage.json"
    src.write_text(json.dumps({"assets": []}), encoding="utf-8")
    ctx = _make_context(tmp_path)
    record = _make_record("stage.main_menu", source_path=str(src))
    errors = StageHandler().validate(record, ctx)
    assert any("name" in e.message for e in errors)


def test_stage_validate_error_when_name_empty(tmp_path):
    src = tmp_path / "main_menu.stage.json"
    src.write_text(json.dumps({"name": ""}), encoding="utf-8")
    ctx = _make_context(tmp_path)
    record = _make_record("stage.main_menu", source_path=str(src))
    errors = StageHandler().validate(record, ctx)
    assert any("name" in e.message for e in errors)


def test_stage_validate_does_not_check_member_assets(tmp_path):
    """StageHandler must NOT validate member assets (SD-CAT-012)."""
    src = tmp_path / "level.stage.json"
    # member assets listed are intentionally missing from disk — must not cause error
    src.write_text(json.dumps({"name": "Level 1", "members": ["texture.missing"]}), encoding="utf-8")
    ctx = _make_context(tmp_path)
    record = _make_record("stage.level", source_path=str(src))
    errors = StageHandler().validate(record, ctx)
    assert errors == []


def test_stage_validate_also_checks_file_exists(tmp_path):
    ctx = _make_context(tmp_path)
    record = _make_record("stage.main_menu", source_path=str(tmp_path / "missing.stage.json"))
    errors = StageHandler().validate(record, ctx)
    assert any("not found" in e.message for e in errors)


# ---------------------------------------------------------------------------
# Handler subclassing (AI Q5)
# ---------------------------------------------------------------------------

def test_handler_can_be_subclassed_to_override_transform(tmp_path):
    class CompressingTextureHandler(TextureHandler):
        def transform(self, record, context):
            from dia_cli.commands.asset.handler import TransformResult
            return TransformResult(success=True, output_path="compressed/" + record["id"])

    h = CompressingTextureHandler()
    assert h.type_id == "texture"
    ctx = _make_context(tmp_path)
    result = h.transform({"id": "texture.hero"}, ctx)
    assert result.output_path == "compressed/texture.hero"
