import click
from commands.command import command_cmd
from mdk_cli.utils.mdk_config import Config
from mdk_cli.utils.mdk_paths import mdkPaths

## @defgroup command_grp command
#  Everything related to `mdk command` command
#  @{

## @package command Command declarations for `command` top-level command

@click.group()
def cli():
    """! Commands for managing commands.
    Not callable without arguments."""
    pass


@cli.command()
@click.argument("command_name")
@click.argument("dest_folder")
@click.pass_context
def create(ctx, command_name, dest_folder):
    """! Creates a new top-level command called COMMAND_NAME in DEST_FOLDER.

    Creates module containing Click command declaration as <DEST_FOLDER>/cli/<COMMAND_NAME>.py.

    Creates module for command implementation as <DEST_FOLDER>/commands/<COMMAND_NAME>_cmd.py
    \f
    Invoked with `mdk command create COMMAND_NAME DEST_FOLDER`

    @param [in] ctx the Click context
    @param [in] command_name the name of the new top-level command
    @param [in] dest_folder the destination folder, relative to the project root
    """
    config = Config.from_context(ctx)
    dest_folder = config.root_path().joinpath(dest_folder).resolve()
    command_cmd.create_command(mdkPaths(config), command_name, dest_folder)

## @}
