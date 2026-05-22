"""dia orchestrate — run E2E automation scenarios against a Dia application."""
import sys
from pathlib import Path

import click

from dia_cli.utils.repo_root import find_repo_root

# Tools/orchestrator is 4 levels up from this file:
# Dia/DiaCLI/dia_cli/cli/orchestrate.py  →  <repo>/Tools/orchestrator
_REPO_ROOT = None  # resolved lazily


def _get_orchestrator_root(repo_root: Path) -> Path:
    return repo_root / "Tools" / "orchestrator"


@click.command()
@click.option("--suite", default="cluichetest/default", show_default=True,
              help="Plan path: <app>/<name> (without .json)")
@click.option("--filter", "filter_expr", default=None, metavar="EXPR",
              help="pytest -k filter expression")
@click.option("--port", type=int, default=None,
              help="Override WebSocket port from plan")
@click.option("--no-launch", "no_launch", is_flag=True, default=False,
              help="Skip app launch; connect to an already-running app")
@click.pass_context
def cli(ctx, suite, filter_expr, port, no_launch):
    """Run E2E automation scenarios against a Dia application.

    Loads plan from Tools/orchestrator/plans/<suite>.json, launches the app
    via 'dia launch', and runs the listed scenarios through pytest.

    \b
    Examples:
        dia orchestrate
        dia orchestrate --suite=cluichetest/default
        dia orchestrate --filter=test_boot
        dia orchestrate --no-launch --port=9876
    """
    repo_root = find_repo_root(__file__)
    orch_root = _get_orchestrator_root(repo_root)

    # Ensure orchestrator is importable
    tools_root = str(repo_root / "Tools")
    if tools_root not in sys.path:
        sys.path.insert(0, tools_root)

    try:
        from orchestrator.cli import load_plan, run_orchestration
    except ImportError as e:
        click.echo(f"ERROR: could not import orchestrator: {e}", err=True)
        ctx.exit(1)
        return

    try:
        plan = load_plan(suite)
    except FileNotFoundError as e:
        click.echo(f"ERROR: {e}", err=True)
        ctx.exit(1)
        return

    if port is not None:
        plan["port"] = port

    exit_code = run_orchestration(
        plan=plan,
        filter_expr=filter_expr,
        no_launch=no_launch,
        repo_root=repo_root,
    )
    ctx.exit(exit_code)
