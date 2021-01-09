"""(c) 2020 Electronic Arts Inc."""
from typing import Optional

import click


## @defgroup utils_grp utils
# A collection of useful utilities for use by all commands
#  @{

## @package mdk_decorators mdk python decortors.

def sub_command(*commands: click.Group, name: Optional[str] = None, cls: Optional[str] = None, **attrs):
    """! Decorate a click.command function to register with the pass in groups.

    @param [in] commands the click.Groups that the command should be attached to
    @param [in] name name that should be registered for the command.
    @param [in] cls the command class to instantiate. Defaults to click.Command.

    ```
    @click.group(invoke_without_command=True)
    @click.pass_context
    @pass_mdk_config
    def cli(mdk_context: Config, ctx: click.Context):
        if ctx.invoked_subcommand is None:
            {{command_name}}_cmd.execute(mdk_context)
        else:
            pass

    @sub_command(cli)
    def sub_cmd(...):
        pass
    ```
    """
    def decorator(f):
        sub_cmd = click.command(name, cls, **attrs)(f)
        for cmd in commands:
            cmd.add_command(sub_cmd)
        return sub_cmd

    return decorator


## @}
