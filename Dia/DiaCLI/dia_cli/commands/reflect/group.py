"""'dia reflect' Click group."""
import click

from .reflect_cmd import reflect


@click.group("reflect")
def reflect_group():
    """Generate and manage registered-types schema from game binaries."""


reflect_group.add_command(reflect)
