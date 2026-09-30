"""Target/config context for DiaConsole (console-project-context.md).

This is target/config/platform context ONLY, within the one real project in
this repo -- there is no sibling project (``CoW`` doesn't exist), and no
discovery mechanism for finding other project roots exists in DiaCLI. Nothing
here ever re-parses ``pipeline.toml``; it always goes through the existing
``pipeline_config.load_pipeline_config`` loader.
"""
from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path

from dia_cli.commands.pipeline import pipeline_config


@dataclass(frozen=True)
class TargetInfo:
    """A user-facing pipeline target -- everything DiaConsole's target
    selector needs, and nothing more (no stages/deploy/build_deps)."""

    name: str
    app_name: str


def list_targets(repo_root: Path) -> list[TargetInfo]:
    """Non-hidden targets from ``pipeline.toml``, via the existing loader.

    Excludes any target with ``hidden = true`` (``diasfml``,
    ``diauiultralight`` in the real ``pipeline.toml`` -- internal
    build-dependency targets no other DiaCLI tooling exposes as user-facing
    choices).
    """
    cfg = pipeline_config.load_pipeline_config(repo_root)
    return [
        TargetInfo(name=name, app_name=target.app_name)
        for name, target in cfg.targets.items()
        if not target.hidden
    ]
