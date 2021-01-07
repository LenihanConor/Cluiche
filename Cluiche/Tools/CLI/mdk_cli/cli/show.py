import click
from loguru import logger
import pprint
import sys
from mdk_cli.utils.mdk_config import Config


## @defgroup show_grp show
#  Everything related to `mdk show` command
#  @{

## @package show Command declarations for `show` top-level command

@click.group()
def cli():
    """! Commands for showing information.
    Not callable without arguments."""
    pass


@cli.command()
@click.pass_context
def config(ctx):
    """! Prints the current configuration

    \f
    Invoked with `mdk show config`

    @param [in] ctx the Click context
    """
    config = Config.from_context(ctx)
    logger.info(f'\nRoot Path: {config.root_path()}')
    logger.info(f'\n{pprint.pformat(config.value, indent=3, width=100)}')


@cli.command()
@click.pass_context
def modules(ctx):
    """! Show info about Python modules.

    \f
    Invoked with mdk show modules

    @param [in] ctx the Click context
    """
    logger.info(f'\nModule Search Path (sys.path):\n{pprint.pformat(sys.path)}')
    # FIXME sys.modules is too much information. Need a way to display
    # only modules defined by mdk-cli and game teams.
    # logger.info(f'\nLoaded Modules (sys.modules):\n{pprint.pformat(sys.modules)}')
    pass

## @}
