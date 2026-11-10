"""dia schema - value-schema tooling (DiaValueSchema)."""
from __future__ import annotations

from typing import Optional

import click


@click.group()
def cli() -> None:
    """Value-schema tooling."""


@cli.command("export")
@click.option("--target", required=True, metavar="TARGET", help="pipeline.toml target (e.g. cow, cluichetest).")
@click.option("--stage", default=None, metavar="NAME",
              help="Describe the schema as seen from this stage (writes valueschema.<NAME>.json).")
@click.option("--out", default=None, type=click.Path(dir_okay=False), metavar="PATH",
              help="Output file (default: Cluiche/out/<App>/valueschema.json).")
@click.option("--sidecars-only", is_flag=True, default=False,
              help="Build the export from sidecars and ramps alone: no launch, no code-side data.")
@click.option("--config", default="Debug", metavar="CONFIG", help="Build configuration to launch (default: Debug).")
@click.option("--timeout", default=180.0, type=float, show_default=True, metavar="SECONDS",
              help="Give up (and terminate the launched app) when describe mode has not finished after SECONDS.")
def export(target: str, stage: Optional[str], out: Optional[str], sidecars_only: bool, config: str,
           timeout: float) -> None:
    """Write the dia.value_schema_export file for TARGET."""
    from dia_cli.commands.schema.export_cmd import export_command
    export_command(target, stage, out, sidecars_only, config, timeout)
