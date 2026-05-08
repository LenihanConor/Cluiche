"""mycommand command for Dia CLI."""
import click


@click.command()
def cli():
    """mycommand command - add your description here"""
    click.echo("mycommand command executed!")
    # Add your implementation here