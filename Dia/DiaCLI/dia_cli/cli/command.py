import click
from dia_cli.commands.command import command_cmd
from dia_cli.utils.dia_cli_config import Config
from dia_cli.utils.dia_cli_paths import DiaCLIPaths


@click.group()
def cli():
    """Commands for managing commands."""
    pass


@cli.command()
@click.argument("command_name")
@click.argument("dest_folder")
@click.pass_context
def create(ctx, command_name, dest_folder):
    """Create a new top-level command called COMMAND_NAME in DEST_FOLDER.

    Creates cli/<COMMAND_NAME>.py and commands/<COMMAND_NAME>_cmd.py
    inside DEST_FOLDER.
    """
    config = Config.from_context(ctx)
    dest_folder = config.root_path().joinpath(dest_folder).resolve()
    command_cmd.create_command(DiaCLIPaths(config), command_name, dest_folder)
