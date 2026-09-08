"""'dia codegen' Click group."""
import click

from .messages_cmd import messages


@click.group("codegen")
def codegen_group():
    """Generate C++ source from declarative Dia IDL files (e.g. .diagamemessages)."""


codegen_group.add_command(messages)
