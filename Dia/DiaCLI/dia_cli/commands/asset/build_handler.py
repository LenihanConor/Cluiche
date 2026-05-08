from __future__ import annotations

import click

from dia_cli.utils.repo_root import find_repo_root
from ._common_handler import run_asset_phases


def handle_build(ctx: click.Context, target: str, config: str, platform: str, force: bool) -> int:
    repo_root = find_repo_root(__file__)
    return run_asset_phases(
        target=target,
        config=config,
        platform=platform,
        force=force,
        phases=["validate", "transform", "deploy"],
        ctx=ctx,
        repo_root=repo_root,
    )
