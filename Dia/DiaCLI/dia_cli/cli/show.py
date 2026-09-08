import click
from loguru import logger
import pprint
import sys
from dia_cli.utils.dia_cli_config import Config


@click.group()
def cli():
    """Commands for showing information."""
    pass


@cli.command()
@click.pass_context
def config(ctx):
    """Print the current dia_cli_prime_config.json configuration."""
    config = Config.from_context(ctx)
    logger.info(f'\nRoot Path: {config.root_path()}')
    logger.info(f'\n{pprint.pformat(config.value, indent=3, width=100)}')


@cli.command()
@click.pass_context
def modules(ctx):
    """Show Python module search paths (sys.path)."""
    logger.info(f'\nModule Search Path (sys.path):\n{pprint.pformat(sys.path)}')
