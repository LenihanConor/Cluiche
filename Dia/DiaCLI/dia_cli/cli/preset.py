"""dia preset — list and run saved command presets (console-presets.md).

Additive (SD-CONSOLE-008): one more file under dia_cli/cli/, auto-discovered
the same way as every other command. ``run`` is a plain, inherited-stdio
subprocess passthrough (SD-CONSOLE-009) -- it never calls a DiaCLI Click
command's callback in-process, and it reuses
``ExecutionService.build_argv`` for argv construction rather than
reimplementing serialization here.
"""
from __future__ import annotations

import subprocess
import uuid
from pathlib import Path

import click

from dia_cli.utils.repo_root import find_repo_root
from dia_console.execution import ExecutionService
from dia_console.presets import build_execute_request, load_presets
from dia_console.registry import CommandRegistry


def _real_registry() -> CommandRegistry:
    from dia_cli.cli_main import cli as dia_cli_app

    return CommandRegistry.from_click_app(dia_cli_app)


def list_presets_text(repo_root: Path) -> str:
    """The exact text ``dia preset list`` prints: one ``id\\tname\\tcommand_id\\tsource`` line per preset."""
    lines = [
        f"{preset.id}\t{preset.name}\t{preset.command_id}\t{preset.source}"
        for preset in load_presets(repo_root)
    ]
    return "\n".join(lines)


def run_preset(
    repo_root: Path,
    preset_id: str,
    *,
    registry: CommandRegistry | None = None,
    execution_service: ExecutionService | None = None,
) -> int:
    """Resolve ``preset_id`` -> descriptor -> argv, then run it as a real subprocess.

    ``registry``/``execution_service`` are injectable so tests never need to
    reflect the real DiaCLI tree or spawn a real ``dia`` process -- production
    call sites (the Click commands below) always use the real defaults.
    Raises :class:`click.ClickException` if ``preset_id`` or its
    ``command_id`` isn't found.
    """
    presets_by_id = {preset.id: preset for preset in load_presets(repo_root)}
    preset = presets_by_id.get(preset_id)
    if preset is None:
        raise click.ClickException(f"Unknown preset id: {preset_id!r}")

    registry = registry if registry is not None else _real_registry()
    try:
        descriptor = registry.require(preset.command_id)
    except KeyError as exc:
        raise click.ClickException(str(exc)) from exc

    service = execution_service if execution_service is not None else ExecutionService(
        registry, repo_root=repo_root)
    request = build_execute_request(descriptor, preset.values)
    argv = service.build_argv(request, execution_id=uuid.uuid4().hex)

    result = subprocess.run(argv, cwd=str(repo_root))
    return result.returncode


@click.group()
def cli() -> None:
    """List and run saved command presets (dia_console/presets.py)."""


@cli.command("list")
def list_command() -> None:
    """Print every merged preset's id, name, command_id, and source."""
    repo_root = find_repo_root(__file__)
    text = list_presets_text(repo_root)
    if text:
        click.echo(text)


@cli.command("run")
@click.argument("preset_id")
@click.pass_context
def run_command(ctx: click.Context, preset_id: str) -> None:
    """Resolve PRESET_ID and run it as a plain inherited-stdio subprocess."""
    repo_root = find_repo_root(__file__)
    returncode = run_preset(repo_root, preset_id)
    ctx.exit(returncode)
