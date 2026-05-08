"""dia fix <target> — run an aider test-fix loop against a dia run target."""
import click

from dia_cli.commands.fix.fix_handler import run_fix

_DEFAULT_MODEL = "ollama/qwen2.5-coder:14b"
_DEFAULT_MAX_ITERATIONS = 10


@click.command()
@click.argument("target")
@click.option("--filter", "filter_pattern", default=None, metavar="PATTERN",
              help="Passed to 'dia run <target> --filter=PATTERN'.")
@click.option("--model", default=_DEFAULT_MODEL, show_default=True,
              help="Aider-compatible model string (local or cloud).")
@click.option("--config", default="Debug", show_default=True,
              help="Build configuration passed to 'dia run'.")
@click.option("--max-iterations", "max_iterations", default=_DEFAULT_MAX_ITERATIONS,
              show_default=True, type=int,
              help="Maximum aider fix iterations before stopping.")
@click.option("--dry-run", "dry_run", is_flag=True, default=False,
              help="Print the aider command without executing it.")
@click.pass_context
def cli(ctx, target, filter_pattern, model, config, max_iterations, dry_run):
    """Run an aider test-fix loop against any 'dia run' target.

    TARGET is any valid 'dia run' target (e.g. googletest, cluichetest).

    Uses a local Ollama model by default. Pass --model to use a cloud model.
    """
    exit_code = run_fix(
        target=target,
        filter_pattern=filter_pattern,
        model=model,
        config=config,
        max_iterations=max_iterations,
        dry_run=dry_run,
    )
    ctx.exit(exit_code)
