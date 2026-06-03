"""dia reflect — generate registeredtypes.diaschema from a game binary."""
from __future__ import annotations

import sys

import click

from dia_cli.utils.repo_root import find_repo_root
from .reflect_handler import handle_reflect


@click.command("reflect")
@click.option(
    "--target",
    required=True,
    metavar="TARGET",
    help="Pipeline target name (e.g. cluichetest).",
)
@click.option(
    "--config",
    default="Debug",
    show_default=True,
    type=click.Choice(["Debug", "Release"]),
    help="Build configuration.",
)
@click.option(
    "--breaking",
    "is_breaking",
    is_flag=True,
    default=False,
    help="Signal a breaking change — increments major version and resets minor.",
)
def reflect(target: str, config: str, is_breaking: bool) -> None:
    """Generate registeredtypes.diaschema by running <TARGET> with --dump-schema.

    Reads the binary listed in pipeline.toml for TARGET, runs it with
    --dump-schema, parses the JSON output, applies versioning, and writes
    the schema file alongside the .diagame asset directory.
    """
    repo_root = find_repo_root(__file__)
    exit_code = handle_reflect(
        repo_root=repo_root,
        target_name=target,
        config=config,
        is_breaking=is_breaking,
    )
    sys.exit(exit_code)
