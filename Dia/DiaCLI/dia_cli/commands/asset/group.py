"""'dia asset' Click group — parents build, validate, deploy subcommands."""
import click

from .build_cmd import build
from .deploy_cmd import deploy
from .validate_cmd import validate


@click.group("asset")
def asset_group():
    """Manage and process game assets."""


asset_group.add_command(build)
asset_group.add_command(validate)
asset_group.add_command(deploy)
