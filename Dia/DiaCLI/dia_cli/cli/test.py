"""Test command to verify plugin discovery."""
import click


@click.command()
def cli():
    """Test command - verifies plugin discovery is working"""
    click.echo("Plugin discovery is working! This command was auto-discovered.")
