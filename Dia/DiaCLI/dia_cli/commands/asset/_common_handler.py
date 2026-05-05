"""Shared logic for all three asset CLI commands."""
from __future__ import annotations

import json
import shutil
import time
from pathlib import Path

import click

from .config_loader import AssetConfigError, load_asset_target_config
from .context import BuildContext
from .handlers import register_built_in_handlers
from .registry import AssetHandlerRegistry
from .runner import BuildRunner


def run_asset_phases(
    *,
    target: str,
    config: str,
    platform: str,
    force: bool,
    phases: list[str],
    ctx: click.Context,
    repo_root: Path,
) -> int:
    """Shared entry point for build/validate/deploy handlers.

    Returns exit code: 0 success, 1 asset failures, 2 config/IO error.
    """
    # --- resolve output context ---
    output_ctx = _get_output(ctx, repo_root)

    # --- load pipeline.toml target config ---
    try:
        target_cfg = load_asset_target_config(repo_root, target)
    except AssetConfigError as exc:
        click.echo(f"Error: {exc}", err=True)
        return 2

    # --- load catalogue manifest ---
    catalogue_path = (repo_root / target_cfg.catalogue_manifest).resolve()
    if not catalogue_path.exists():
        click.echo(
            f"Error: catalogue manifest not found: {catalogue_path}", err=True
        )
        return 2

    try:
        with open(catalogue_path, encoding="utf-8") as f:
            catalogue = json.load(f)
    except (OSError, json.JSONDecodeError) as exc:
        click.echo(f"Error: could not read catalogue manifest: {exc}", err=True)
        return 2

    # --- build context ---
    deploy_root = (
        repo_root / "bin" / target_cfg.app_name / config / platform / "assets"
    )
    if force and deploy_root.exists():
        shutil.rmtree(deploy_root)

    # source_root is the parent of the catalogue manifest — asset source_path
    # values are relative to this directory
    source_root = catalogue_path.parent

    context = BuildContext(
        catalogue=catalogue,
        config=config,
        platform=platform,
        app_name=target_cfg.app_name,
        deploy_root=deploy_root,
        asset_stages=target_cfg.asset_stages,
        output=output_ctx,
        source_root=source_root,
    )

    # --- register handlers and run ---
    registry = AssetHandlerRegistry()
    register_built_in_handlers(registry)

    t0 = time.time()
    runner = BuildRunner(registry, context)
    run_result = runner.run_with_result(phases=phases)
    elapsed = time.time() - t0

    # human-readable summary (AC 14)
    total_processed = run_result.pass_count + run_result.fail_count
    if run_result.exit_code == 0:
        click.echo(
            f"Asset {'+'.join(phases)} complete: "
            f"{run_result.pass_count} passed, {total_processed} processed  ({elapsed:.1f}s)"
        )
    else:
        click.echo(
            f"Asset {'+'.join(phases)} FAILED: "
            f"{run_result.pass_count} passed, {run_result.fail_count} failed, "
            f"{total_processed} processed  ({elapsed:.1f}s)"
        )

    return run_result.exit_code


def _get_output(ctx: click.Context, repo_root: Path):
    if ctx.obj is not None and hasattr(ctx.obj, "output") and ctx.obj.output is not None:
        return ctx.obj.output
    from dia_cli.utils.dia_output import OutputContext
    log_dir = repo_root / "Cluiche" / "out" / "DiaCLI" / "logs"
    return OutputContext(log_dir=log_dir)
