"""Tests for world-tag deploy layout (subfolder preservation)."""
from __future__ import annotations

from pathlib import Path
from unittest.mock import MagicMock

from dia_cli.commands.asset.context import BuildContext
from dia_cli.commands.asset.layout import compute_world_subfolder, resolve_deploy_path


def _ctx(tmp_path: Path) -> BuildContext:
    return BuildContext(
        catalogue={}, config="Debug", platform="x64", app_name="TestApp",
        deploy_root=tmp_path / "assets", asset_stages=[], output=MagicMock(),
        source_root=tmp_path,
    )


def _rec(tags, source_path, scope="global", stage_name="") -> dict:
    return {"id": "ecology.lush", "type": "ecology", "source_path": source_path,
            "scope": scope, "stage_name": stage_name, "tags": tags}


def test_world_tag_preserves_subfolders(tmp_path):
    ctx = _ctx(tmp_path)
    src = "Global/Ecology/Packs/lush.ecology.json"
    assert compute_world_subfolder(src) == "Ecology/Packs" or \
        compute_world_subfolder(src).replace("\\", "/") == "Ecology/Packs"
    p = resolve_deploy_path(_rec(["world"], src), ctx)
    assert p == ctx.deploy_root / "global" / "world" / "Ecology" / "Packs" / "lush.ecology.json"
    # AC2: world ignores category tags
    p2 = resolve_deploy_path(_rec(["world", "misc"], src), ctx)
    assert p2 == p
    ctx.output.warn.assert_not_called()


def test_world_tag_no_subfolder(tmp_path):
    ctx = _ctx(tmp_path)
    assert compute_world_subfolder("Raw/lush.json") == ""
    assert compute_world_subfolder("Global/lush.json") == ""
    p = resolve_deploy_path(_rec(["world"], "Raw/lush.json"), ctx)
    assert p == ctx.deploy_root / "global" / "world" / "lush.json"
    ctx.output.warn.assert_called()
    msg = ctx.output.warn.call_args[0][1]
    assert "ecology.lush" in msg and "Global/" in msg


def test_non_world_tag_unchanged(tmp_path):
    ctx = _ctx(tmp_path)
    p = resolve_deploy_path(_rec(["misc"], "Global/Ecology/Packs/a.json"), ctx)
    assert p == ctx.deploy_root / "global" / "misc" / "a.json"


def test_stage_scope_ignores_world_tag(tmp_path):
    ctx = _ctx(tmp_path)
    p = resolve_deploy_path(
        _rec(["world", "misc"], "Global/Ecology/a.json", scope="stage", stage_name="S1"), ctx)
    assert p == ctx.deploy_root / "stages" / "S1" / "misc" / "a.json"
