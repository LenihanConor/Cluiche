"""build-assets stage: delegates to DiaAssetPipeline."""
from pathlib import Path

from loguru import logger

from ..pipeline_config import PipelineConfig


def run(config: PipelineConfig, target: str, build_config: str, force: bool, repo_root: Path, output=None, system: str = "pipeline") -> int:
    from dia_cli.commands.asset._common_handler import run_asset_phases
    import click

    # Build a minimal Click context so _common_handler can resolve output
    ctx = click.Context(click.Command("build-assets"))
    if output is not None:
        class _Obj:
            pass
        obj = _Obj()
        obj.output = output
        ctx.obj = obj

    return run_asset_phases(
        target=target,
        config=build_config,
        platform="x64",
        force=force,
        phases=["validate", "transform", "deploy"],
        ctx=ctx,
        repo_root=repo_root,
    )
