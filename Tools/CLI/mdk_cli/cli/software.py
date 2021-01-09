import click
from loguru import logger
from mdk_cli.commands.software import protoc_cmd
from mdk_cli.utils.mdk_config import Config


## @defgroup software_grp software
#  Everything related to `mdk software` command
#  @{

## @package software Command declarations for `software` top-level command

@click.group()
def cli():
    """! Software installation commands.
    \f
    Not callable without arguments."""
    pass


@cli.command()
@click.pass_context
def all(ctx):
    """! Installs all software dependencies by invoking sub-commands.

    Currently installs:

    1. the `protoc compiler`
    \f
    Invoked with `mdk software all`

    @param [in] ctx the Click context
    """
    logger.info('Installing everything')
    ctx.invoke(protoc)


@cli.command()
@click.pass_context
def protoc(ctx):
    """! Installs Protobuf compiler, `protoc`.
    Installs into configured `paths.external` folder.
    \f
    Invoked with `mdk software protoc`

    @param [in] ctx the Click context
    """
    protoc_cmd.execute(Config.from_context(ctx))

## @}
