"""Unit tests for deploy layout engine — path resolution and copy utilities."""
from __future__ import annotations

import shutil
from pathlib import Path
from unittest.mock import MagicMock

import pytest

from dia_cli.commands.asset.context import BuildContext
from dia_cli.commands.asset.layout import (
    CATEGORY_TAGS,
    copy_asset,
    copy_asset_directory,
    create_deploy_directories,
    resolve_category,
    resolve_deploy_path,
)


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
    asset_id: str = "texture.bg",
    scope: str = "global",
    stage_name: str = "",
    tags: list[str] | None = None,
    source_path: str = "Raw/bg.png",
    asset_type: str | None = None,
) -> dict:
    if asset_type is None:
        asset_type = asset_id.split(".")[0]
    return {
        "id": asset_id,
        "type": asset_type,
        "source_path": source_path,
        "scope": scope,
        "stage_name": stage_name,
        "tags": tags or [],
    }


# ---------------------------------------------------------------------------
# resolve_category
# ---------------------------------------------------------------------------

def test_resolve_category_first_match_wins():
    # "characters" comes before "gameplay" in CATEGORY_TAGS
    assert resolve_category(["characters", "gameplay"]) == "characters"


def test_resolve_category_each_known_tag():
    for tag in CATEGORY_TAGS:
        assert resolve_category([tag]) == tag


def test_resolve_category_no_match_returns_misc():
    assert resolve_category(["unknown", "random"]) == "misc"


def test_resolve_category_empty_tags_returns_misc():
    assert resolve_category([]) == "misc"


def test_resolve_category_unrecognised_tag_before_recognised():
    assert resolve_category(["unknown", "gameplay"]) == "gameplay"


# ---------------------------------------------------------------------------
# resolve_deploy_path — global scope
# ---------------------------------------------------------------------------

def test_resolve_global_characters(tmp_path):
    ctx = _make_context(tmp_path)
    record = _make_record(tags=["characters"], source_path="Raw/hero.png")
    path = resolve_deploy_path(record, ctx)
    assert path == ctx.deploy_root / "global" / "characters" / "hero.png"


def test_resolve_global_presentation_ui(tmp_path):
    ctx = _make_context(tmp_path)
    record = _make_record(tags=["Presentation/UI"], source_path="Raw/hud.png")
    path = resolve_deploy_path(record, ctx)
    assert path == ctx.deploy_root / "global" / "Presentation/UI" / "hud.png"


def test_resolve_global_untagged_goes_to_misc(tmp_path):
    ctx = _make_context(tmp_path)
    record = _make_record(tags=[], source_path="Raw/thing.bin")
    path = resolve_deploy_path(record, ctx)
    assert path == ctx.deploy_root / "global" / "misc" / "thing.bin"


def test_resolve_global_unknown_tag_goes_to_misc(tmp_path):
    ctx = _make_context(tmp_path)
    record = _make_record(tags=["custom_tag"], source_path="Raw/thing.bin")
    path = resolve_deploy_path(record, ctx)
    assert path == ctx.deploy_root / "global" / "misc" / "thing.bin"


# ---------------------------------------------------------------------------
# resolve_deploy_path — stage scope
# ---------------------------------------------------------------------------

def test_resolve_stage_uses_stage_name(tmp_path):
    ctx = _make_context(tmp_path)
    record = _make_record(
        scope="stage", stage_name="main_menu",
        tags=["Presentation"], source_path="Raw/bg.png",
    )
    path = resolve_deploy_path(record, ctx)
    assert path == ctx.deploy_root / "stages" / "main_menu" / "Presentation" / "bg.png"


def test_resolve_stage_untagged_goes_to_misc(tmp_path):
    ctx = _make_context(tmp_path)
    record = _make_record(scope="stage", stage_name="level1", tags=[], source_path="Raw/x.bin")
    path = resolve_deploy_path(record, ctx)
    assert path == ctx.deploy_root / "stages" / "level1" / "misc" / "x.bin"


# ---------------------------------------------------------------------------
# resolve_deploy_path — folder type trailing slash
# ---------------------------------------------------------------------------

def test_resolve_folder_path_points_into_category_dir(tmp_path):
    """Folder assets resolve to <category>/<filename> — trailing slash is added
    by manifest_generator.add_asset(is_folder=True), not by resolve_deploy_path."""
    ctx = _make_context(tmp_path)
    record = _make_record(
        asset_id="folder.art_pack",
        asset_type="folder",
        tags=["characters"],
        source_path="Raw/art_pack.folder",
    )
    path = resolve_deploy_path(record, ctx)
    assert path == ctx.deploy_root / "global" / "characters" / "art_pack.folder"


# ---------------------------------------------------------------------------
# create_deploy_directories
# ---------------------------------------------------------------------------

def test_create_deploy_directories_creates_global_tree(tmp_path):
    deploy_root = tmp_path / "assets"
    create_deploy_directories(deploy_root)
    for tag in CATEGORY_TAGS:
        assert (deploy_root / "global" / tag).is_dir()


def test_create_deploy_directories_idempotent(tmp_path):
    deploy_root = tmp_path / "assets"
    create_deploy_directories(deploy_root)
    create_deploy_directories(deploy_root)  # must not raise


# ---------------------------------------------------------------------------
# copy_asset
# ---------------------------------------------------------------------------

def test_copy_asset_copies_file(tmp_path):
    src = tmp_path / "src.txt"
    src.write_text("hello")
    dest = tmp_path / "out" / "sub" / "src.txt"
    copy_asset(src, dest)
    assert dest.exists()
    assert dest.read_text() == "hello"


def test_copy_asset_overwrites_existing(tmp_path):
    src = tmp_path / "src.txt"
    src.write_text("new")
    dest = tmp_path / "dest.txt"
    dest.write_text("old")
    copy_asset(src, dest)
    assert dest.read_text() == "new"


def test_copy_asset_creates_parent_dirs(tmp_path):
    src = tmp_path / "src.bin"
    src.write_bytes(b"\x00\x01")
    dest = tmp_path / "a" / "b" / "c" / "dest.bin"
    copy_asset(src, dest)
    assert dest.exists()


# ---------------------------------------------------------------------------
# copy_asset_directory
# ---------------------------------------------------------------------------

def test_copy_asset_directory_copies_tree(tmp_path):
    src = tmp_path / "art_pack"
    src.mkdir()
    (src / "hero.png").write_bytes(b"img")
    (src / "sub").mkdir()
    (src / "sub" / "icon.png").write_bytes(b"icon")

    dest = tmp_path / "out" / "art_pack"
    copy_asset_directory(src, dest)

    assert (dest / "hero.png").exists()
    assert (dest / "sub" / "icon.png").exists()


def test_copy_asset_directory_merges_existing(tmp_path):
    src = tmp_path / "pack"
    src.mkdir()
    (src / "new.png").write_bytes(b"new")

    dest = tmp_path / "out" / "pack"
    dest.mkdir(parents=True)
    (dest / "existing.png").write_bytes(b"keep")

    copy_asset_directory(src, dest)

    assert (dest / "new.png").exists()
    assert (dest / "existing.png").exists()
