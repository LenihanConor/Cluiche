"""'dia scaffold' Click group — parents stage and future scaffold subcommands."""
import click

from .stage_cmd import stage
from .module_cmd import module
from .plugin_cmd import plugin
from .sln_cmd import sln_add


@click.group("scaffold")
def scaffold_group():
    """Generate boilerplate for CluicheTest stages and other engine artifacts."""


scaffold_group.add_command(stage)
scaffold_group.add_command(module)
scaffold_group.add_command(plugin)
scaffold_group.add_command(sln_add)
