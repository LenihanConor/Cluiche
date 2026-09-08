"""'dia reflect' Click group."""
import click

from .reflect_cmd import reflect
from .export_types_cmd import export_types


@click.group("reflect")
def reflect_group():
    """Generate and manage registered-types schema from game binaries."""


reflect_group.add_command(reflect)
reflect_group.add_command(export_types, "export-types")
