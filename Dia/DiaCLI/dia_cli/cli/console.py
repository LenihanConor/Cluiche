"""dia console — launch DiaConsole's native window shell.

Additive (SD-CONSOLE-008): this is one more file under dia_cli/cli/,
auto-discovered the same way as every other command. Omitting it from a
build leaves the rest of DiaCLI working unchanged.
"""
from __future__ import annotations

import click


@click.command()
def cli() -> None:
    """Launch DiaConsole -- a chromeless native window for browsing and
    running DiaCLI commands with a live log view."""
    from dia_console.shell import launch

    launch()
