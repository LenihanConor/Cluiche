"""dia capture — manage render captures (bless, list)."""
from __future__ import annotations

import shutil
from pathlib import Path

import click

from dia_cli.utils.repo_root import find_repo_root


@click.group()
def cli():
    """Manage render captures."""
    pass


@cli.command("bless")
@click.argument("tag")
@click.pass_context
def bless(ctx, tag: str) -> None:
    """Promote a run capture to the checked-in reference.

    Copies out/CluicheTest/captures/run/<tag>.png to
    Cluiche/Assets/CluicheTest/captures/reference/<tag>.png.
    """
    repo_root = find_repo_root(__file__)
    src = repo_root / "Cluiche" / "out" / "CluicheTest" / "captures" / "run" / f"{tag}.png"
    dst_dir = repo_root / "Cluiche" / "Assets" / "CluicheTest" / "captures" / "reference"
    dst = dst_dir / f"{tag}.png"

    if not src.exists():
        click.echo(f"[dia capture bless] ERROR: run capture not found: {src}")
        ctx.exit(1)
        return

    dst_dir.mkdir(parents=True, exist_ok=True)
    shutil.copy2(src, dst)
    click.echo(f"[dia capture bless] Blessed: {src.name} -> {dst}")


@cli.command("list")
@click.pass_context
def list_captures(ctx) -> None:
    """List all run captures and whether they have a matching blessed reference."""
    repo_root = find_repo_root(__file__)
    run_dir = repo_root / "Cluiche" / "out" / "CluicheTest" / "captures" / "run"
    ref_dir = repo_root / "Cluiche" / "Assets" / "CluicheTest" / "captures" / "reference"

    if not run_dir.exists():
        click.echo("[dia capture list] No run captures found (directory does not exist).")
        return

    pngs = sorted(run_dir.glob("*.png"))
    if not pngs:
        click.echo("[dia capture list] No run captures found.")
        return

    click.echo(f"{'Tag':<40} {'Blessed'}")
    click.echo("-" * 50)
    for png in pngs:
        tag = png.stem
        blessed = "OK" if (ref_dir / png.name).exists() else "not blessed"
        click.echo(f"{tag:<40} {blessed}")
