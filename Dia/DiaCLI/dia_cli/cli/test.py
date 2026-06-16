"""dia test command group."""
import sys
import click

from dia_cli.utils.repo_root import find_repo_root


@click.group()
def cli():
    """Run Dia test suites."""
    pass


@cli.command("editor-ui")
@click.option("--filter", "filter_pattern", default=None, metavar="PATTERN",
              help="Run only tests whose name matches PATTERN (passed to vitest -t).")
@click.option("--watch", is_flag=True, default=False,
              help="Run in watch mode (re-runs on file change).")
@click.option("--coverage", is_flag=True, default=False,
              help="Generate a coverage report.")
@click.option("--docker", is_flag=True, default=False,
              help="Re-invoke inside Docker container.")
@click.pass_context
def editor_ui(ctx, filter_pattern, watch, coverage, docker):
    """Run the DiaApplicationFlowEditor (CEF) UI Vitest suite."""
    from dia_cli.commands.test.ui_runner import run
    exit_code = run(
        repo_root=None,
        ui_subpath="Dia/DiaApplicationFlowEditor/UI",
        docker_subcmd="editor-ui",
        filter_pattern=filter_pattern,
        watch=watch,
        coverage=coverage,
        docker=docker,
    )
    ctx.exit(exit_code)


@cli.command("game-ui")
@click.option("--filter", "filter_pattern", default=None, metavar="PATTERN",
              help="Run only tests whose name matches PATTERN (passed to vitest -t).")
@click.option("--watch", is_flag=True, default=False,
              help="Run in watch mode (re-runs on file change).")
@click.option("--docker", is_flag=True, default=False,
              help="Re-invoke inside Docker container.")
@click.pass_context
def game_ui(ctx, filter_pattern, watch, docker):
    """Run the CluicheTest game UI Vitest suite."""
    from dia_cli.commands.test.ui_runner import run
    exit_code = run(
        repo_root=None,
        ui_subpath="Cluiche/CluicheTest/UI",
        docker_subcmd="game-ui",
        filter_pattern=filter_pattern,
        watch=watch,
        docker=docker,
    )
    ctx.exit(exit_code)


@cli.command()
@click.option("--filter", "filter_pattern", default=None, metavar="PATTERN",
              help="Run only tests matching PATTERN (passed to --gtest_filter).")
@click.option("--config", default="Debug", metavar="CONFIG",
              help="Build configuration to test: Debug or Release (default: Debug).")
@click.option("--verbose", is_flag=True, default=False,
              help="Pass --gtest_verbose to the binary.")
@click.option("--docker", is_flag=True, default=False,
              help="Re-invoke inside Docker container.")
@click.option("--all", "run_all", is_flag=True, default=False,
              help="Include SLOW_* suites (default excludes them).")
@click.option("--shards", default=0, metavar="N", type=int,
              help="Run in N parallel shards (0=disabled, omit for cpu_count-1).")
@click.pass_context
def googletest(ctx, filter_pattern, config, verbose, docker, run_all, shards):
    """Run the GoogleTests C++ test suite."""
    from dia_cli.commands.test.googletest_runner import run
    exit_code = run(repo_root=None, config=config, filter_pattern=filter_pattern,
                    verbose=verbose, docker=docker, run_all=run_all, shards=shards)
    ctx.exit(exit_code)


@cli.command("cli")
@click.option("--filter", "filter_pattern", default=None, metavar="PATTERN",
              help="Run only tests matching PATTERN (passed to pytest -k).")
@click.option("--parallel", is_flag=True, default=False,
              help="Run tests in parallel with pytest-xdist (-n auto).")
@click.option("--coverage-out", default=None, metavar="PATH",
              help="Write coverage XML report to PATH.")
@click.option("--docker", is_flag=True, default=False,
              help="Run tests inside Docker container.")
@click.pass_context
def cli_tests(ctx, filter_pattern, parallel, coverage_out, docker):
    """Run the DiaCLI pytest suite."""
    from dia_cli.commands.test.cli_runner import run
    exit_code = run(repo_root=None, filter_pattern=filter_pattern,
                    parallel=parallel, coverage_out=coverage_out, docker=docker)
    ctx.exit(exit_code)


@cli.command("env-integration")
@click.option("--skip-env", is_flag=True, default=False,
              help="Skip Stage 1 (assume container already provisioned).")
@click.option("--max-auto-fixes", default=1, metavar="N",
              help="Max automatic fix attempts before prompting operator (default: 1).")
@click.option("--no-fix", is_flag=True, default=False,
              help="Fail immediately on any error without attempting fixes.")
@click.option("--docker", is_flag=True, default=False,
              help="Run all stages inside Docker container.")
@click.option("--inject-fault", "inject_fault", default=None, metavar="TYPE", hidden=True,
              help="[test only] Inject a simulated fault (path).")
@click.pass_context
def env_integration(ctx, skip_env, max_auto_fixes, no_fix, docker, inject_fault):
    """Run the full env -> pipeline -> test validation loop inside Docker."""
    from dia_cli.commands.test.env_integration_runner import run
    exit_code = run(repo_root=None, skip_env=skip_env, max_auto_fixes=max_auto_fixes,
                    no_fix=no_fix, inject_fault=inject_fault, docker=docker)
    ctx.exit(exit_code)


def _get_e2e_root(repo_root):
    from pathlib import Path
    return Path(repo_root) / "Cluiche" / "Tests" / "E2E"


@cli.command("e2e")
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
def e2e(ctx, suite, filter_expr, scenario_glob, list_only, port, no_launch):
    """Run E2E automation scenarios against a Dia application.

    Loads plan from Cluiche/Tests/E2E/plans/<suite>.json, launches the app,
    and runs the listed scenarios through pytest.

    \b
    Examples:
        dia test e2e
        dia test e2e --suite=cluichetest/default
        dia test e2e --filter=test_boot
        dia test e2e --scenario="*boot*"
        dia test e2e --list --scenario="*rigidbody*"
        dia test e2e --no-launch --port=9876
    """
    repo_root = find_repo_root(__file__)
    e2e_root = _get_e2e_root(repo_root)

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
