from __future__ import annotations

import click
from dia_cli.utils.repo_root import find_repo_root
from ._common_handler import run_asset_phases


def handle_validate(ctx: click.Context, target: str, config: str, platform: str) -> int:
    repo_root = find_repo_root(__file__)
    return run_asset_phases(
        target=target,
        config=config,
        platform=platform,
        force=False,
        phases=["validate"],
        ctx=ctx,
        repo_root=repo_root,
    )
