"""editor-ui-test command for Dia CLI."""
import click


@click.command()
@click.option('--watch', is_flag=True, default=False, help='Run in watch mode (re-run on file changes).')
@click.option('--coverage', is_flag=True, default=False, help='Generate a coverage report.')
@click.pass_context
def cli(ctx, watch, coverage):
    """Run the CluicheEditor UI test suite (Vitest)."""
    from dia_cli.commands.editor_ui_test_cmd import execute
    exit_code = execute(watch=watch, coverage=coverage)
    ctx.exit(exit_code)
