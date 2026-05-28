"""'dia scaffold' Click group — parents stage and future scaffold subcommands."""
import click

from .stage_cmd import stage


@click.group("scaffold")
def scaffold_group():
    """Generate boilerplate for CluicheTest stages and other engine artifacts."""


scaffold_group.add_command(stage)
