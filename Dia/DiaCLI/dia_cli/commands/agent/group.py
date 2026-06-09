"""'dia agent' Click group — autonomous agents."""
import click

from .implement_cmd import implement


@click.group("agent")
def agent_group():
    """Autonomous agents for the Dia engine."""


agent_group.add_command(implement)
