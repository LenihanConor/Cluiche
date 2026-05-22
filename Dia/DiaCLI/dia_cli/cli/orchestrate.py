"""dia orchestrate — run E2E automation scenarios against a Dia application."""
import sys
from pathlib import Path

import click

from dia_cli.utils.repo_root import find_repo_root


def _get_e2e_root(repo_root: Path) -> Path:
    return repo_root / "Cluiche" / "Tests" / "E2E"


@click.command()
@click.option("--suite", default="cluichetest/default", show_default=True,
              help="Plan path: <app>/<name> (without .json)")
@click.option("--filter", "filter_expr", default=None, metavar="EXPR",
              help="pytest -k filter expression")
@click.option("--scenario", "scenario_glob", default=None, metavar="GLOB",
              help="fnmatch glob to filter scenario paths (e.g. '*boot*')")
@click.option("--list", "list_only", is_flag=True, default=False,
              help="Print matched scenario paths and exit (no launch/pytest)")
@click.option("--port", type=int, default=None,
              help="Override WebSocket port from plan")
@click.option("--no-launch", "no_launch", is_flag=True, default=False,
              help="Skip app launch; connect to an already-running app")
@click.pass_context
def cli(ctx, suite, filter_expr, scenario_glob, list_only, port, no_launch):
    """Run E2E automation scenarios against a Dia application.

    Loads plan from Cluiche/Tests/E2E/plans/<suite>.json, launches the app,
    and runs the listed scenarios through pytest.

    \b
    Examples:
        dia orchestrate
        dia orchestrate --suite=cluichetest/default
        dia orchestrate --filter=test_boot
        dia orchestrate --scenario="*boot*"
        dia orchestrate --list --scenario="*rigidbody*"
        dia orchestrate --no-launch --port=9876
    """
    repo_root = find_repo_root(__file__)
    e2e_root = _get_e2e_root(repo_root)

    # Ensure E2E root is importable
    if str(e2e_root) not in sys.path:
        sys.path.insert(0, str(e2e_root))

    try:
        from cli import load_plan, run_orchestration
    except ImportError as e:
        click.echo(f"ERROR: could not import E2E cli: {e}", err=True)
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
        scenario_glob=scenario_glob,
        list_only=list_only,
    )
    ctx.exit(exit_code)
