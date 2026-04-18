"""DiaAPI bridge commands - invoke C++ DiaAPI from Python CLI."""
import click
from dia_cli.utils.diaapi_bridge import get_bridge
from loguru import logger


@click.group(invoke_without_command=True)
@click.pass_context
def cli(ctx):
    """Invoke C++ DiaAPI commands from Python

    Provides access to commands registered in the C++ DiaAPI system.
    Requires DiaPython bindings to be available.
    """
    if ctx.invoked_subcommand is None:
        bridge = get_bridge()
        if not bridge.is_available():
            click.echo("DiaAPI bridge not available.", err=True)
            click.echo("Make sure DiaPython bindings are built and accessible.", err=True)
            ctx.exit(1)
        else:
            click.echo("DiaAPI bridge is available!")
            click.echo("Use 'dia api list' to see available commands.")
            click.echo("Use 'dia api <command>' to execute a DiaAPI command.")


@cli.command()
def list():
    """List all registered DiaAPI commands"""
    bridge = get_bridge()

    if not bridge.is_available():
        click.echo("Error: DiaAPI not available", err=True)
        return 1

    commands = bridge.list_commands()

    if not commands:
        click.echo("No DiaAPI commands registered.")
        return 0

    click.echo(f"Available DiaAPI commands ({len(commands)}):")
    for cmd in sorted(commands):
        click.echo(f"  - {cmd}")

    return 0


@cli.command(name='exec', context_settings=dict(ignore_unknown_options=True))
@click.argument('command_name')
@click.argument('args', nargs=-1, type=click.UNPROCESSED)
def execute(command_name, args):
    """Execute a DiaAPI command with arguments

    Example: dia api exec validate-assets --path Assets/
    """
    bridge = get_bridge()

    if not bridge.is_available():
        click.echo("Error: DiaAPI not available", err=True)
        return 1

    logger.debug(f"Executing DiaAPI command: {command_name} with args: {args}")

    exit_code = bridge.execute(command_name, list(args))

    if exit_code != 0:
        logger.error(f"DiaAPI command '{command_name}' failed with exit code {exit_code}")

    return exit_code


@cli.command()
@click.argument('command_name')
def help(command_name):
    """Show help for a specific DiaAPI command"""
    bridge = get_bridge()

    if not bridge.is_available():
        click.echo("Error: DiaAPI not available", err=True)
        return 1

    help_text = bridge.get_command_help(command_name)

    if help_text:
        click.echo(help_text)
    else:
        click.echo(f"No help available for command: {command_name}")

    return 0
