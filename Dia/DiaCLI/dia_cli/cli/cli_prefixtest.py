"""Test cli_ prefix stripping."""
import click


@click.command()
def cli():
    """Prefix test - should appear as 'prefixtest' not 'cli-prefixtest'"""
    click.echo("cli_ prefix was stripped correctly!")
