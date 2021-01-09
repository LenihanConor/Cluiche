import click
from commands import console_cmd


## @defgroup console_grp console
#  Everything related to `blue console` command.
#  @{

## @package console Command declarations for `console` top-level command

@click.group(invoke_without_command=True)
@click.pass_context
def cli(ctx):
    """! Starts the Blue Console
    \f
    invoked with `blue console`
    @param [in] ctx the Click context
    """
    if ctx.invoked_subcommand is None:
        blue_context = ctx.obj
        console_cmd.execute(blue_context)
    else:
        pass

## @}
