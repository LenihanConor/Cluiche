import click
from loguru import logger
from mdk_cli.commands import setup_cmd
from mdk_cli.commands.setup import executable_cmd
from mdk_cli.commands.setup import setpath_cmd
from mdk_cli.utils.mdk_config import Config
from mdk_cli.utils.mdk_paths import mdkPaths


## @defgroup setup_grp setup
#  Everything related to `mdk setup` command.
#  @{

## @package setup Command declarations for `setup` top-level command

@click.group(invoke_without_command=True)
@click.pass_context
def cli(ctx):
    """! Sets up a mdk-based project locally.

    Should be run from the folder containing mdk_prime_config.json,
    which for standard projects is the project's root folder.

    If mdk_prime_config.json does not yet exist, this command will
    create it, and will set the project's root folder to be the cwd.
    \f

    @param [in] ctx the Click context
    """
    if ctx.invoked_subcommand is None:
        setup_cmd.execute(ctx.obj)
    else:
        pass


@cli.command()
@click.argument("name")
@click.pass_context
def executable(ctx, name):
    """! Sets a custom alias, NAME, for the 'mdk' command.
    \f
    Invoked with `mdk setup executable NAME`

    @param[in] ctx the Click context
    @param[in] name the name of the custom executable alias. Command argument is NAME.
    """
    mdk_paths = mdkPaths(Config.from_context(ctx))
    executable_cmd.customize_name(mdk_paths, name)


@cli.group(invoke_without_command=True)
@click.pass_context
def paths(ctx):
    """! View custom paths for the cli.
    \f
    Invoked with `mdk setup paths`

    @param [in] ctx the Click context
"""
    if ctx.invoked_subcommand is None:
        setpath_cmd.show_paths(Config.from_context(ctx))


@paths.command()
@click.argument('path_key')
@click.argument('path')
@click.pass_context
def set(ctx, path_key, path):
    """! Add a custom path to the prime configuration file.

    To change/add a path use:
    `mdk setup paths set PATH_KEY PATH`

    For instance:
    `mdk setup paths set tool1_output path/to/tool1/output`
    \f
    @param [in] ctx the Click context
    @param [in] path_key the path's key in the `paths` map in mdk_prime_config.json
    @param [in] path the path value. This is always interpreted as relative to the project root.
    """
    logger.info(f"Setting path '{path_key}' to '{path}'")
    setpath_cmd.set_path(Config.from_context(ctx), path_key, path)

## @}
