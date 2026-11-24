"""Characterization test: the asset pipeline never removes or rewrites catalogue
records, so `author: game` records survive even when their source is missing.

A future prune command must skip records with ``author == "game"``.
"""
from __future__ import annotations

import json
from pathlib import Path

from dia_cli.commands.asset.context import BuildContext
from dia_cli.commands.asset.handlers import register_built_in_handlers
from dia_cli.commands.asset.registry import AssetHandlerRegistry
from dia_cli.commands.asset.runner import BuildRunner
from dia_cli.utils.dia_output import OutputContext


def _record(asset_id: str, source_path: str, tags: list[str], **extra) -> dict:
    rec = {
        "id": asset_id,
        "type": asset_id.split(".")[0],
        "source_path": source_path,
        "content_hash": 0,
        "status": "active",
        "scope": "global",
        "stage_name": "",
        "tags": tags,
        "references": [],
    }
    rec.update(extra)
    return rec


def test_author_game_survives_missing_source(tmp_path: Path) -> None:
    source_root = tmp_path / "Assets" / "TestApp"
    (source_root / "Global" / "world").mkdir(parents=True)
    (source_root / "hero.png").write_bytes(b"hero")

    normal = _record("texture.hero", "hero.png", ["characters"])
    game = _record(
        "world.region_001", "Global/world/region_001.json", ["world"], author="game"
    )  # source file deliberately never created
    cat_path = source_root / "assets.catalogue.json"
    cat_path.write_text(json.dumps({"assets": [normal, game]}), encoding="utf-8")
    before = json.loads(cat_path.read_text(encoding="utf-8"))

    context = BuildContext(
        catalogue=json.loads(cat_path.read_text(encoding="utf-8")),
        config="Debug",
        platform="x64",
        app_name="TestApp",
        deploy_root=tmp_path / "bin" / "assets",
        asset_stages=[],
        output=OutputContext(log_dir=tmp_path / "logs", quiet=True),
        source_root=source_root,
    )
    registry = AssetHandlerRegistry()
    register_built_in_handlers(registry)

    # Full build + deploy; the missing source may fail that asset, but must not drop it.
    BuildRunner(registry, context).run_with_result()

    after = json.loads(cat_path.read_text(encoding="utf-8"))
    assert after == before
    survivors = [a for a in after["assets"] if a.get("author") == "game"]
    assert survivors == [game]
    # In-memory catalogue also untouched
    assert game in context.catalogue["assets"]
