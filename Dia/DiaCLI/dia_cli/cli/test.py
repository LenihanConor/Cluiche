"""dia test command group."""
import click


@click.group()
def cli():
    """Run Dia test suites."""
    pass


@cli.command("editor-ui")
@click.option("--filter", "filter_pattern", default=None, metavar="PATTERN",
              help="Run only tests whose name matches PATTERN (passed to vitest -t).")
@click.option("--watch", is_flag=True, default=False,
              help="Run in watch mode (re-runs on file change).")
@click.option("--docker", is_flag=True, default=False,
              help="Re-invoke inside Docker container.")
@click.pass_context
def editor_ui(ctx, filter_pattern, watch, docker):
    """Run the DiaApplicationEditor (CEF) UI Vitest suite."""
    from commands.test.ui_runner import run
    exit_code = run(
        repo_root=None,
        ui_subpath="Dia/DiaApplicationEditor/UI",
        docker_subcmd="editor-ui",
        filter_pattern=filter_pattern,
        watch=watch,
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
    from commands.test.ui_runner import run
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
@click.pass_context
def googletest(ctx, filter_pattern, config, verbose, docker):
    """Run the GoogleTests C++ test suite."""
    from commands.test.googletest_runner import run
    exit_code = run(repo_root=None, config=config, filter_pattern=filter_pattern,
                    verbose=verbose, docker=docker)
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
    from commands.test.cli_runner import run
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
    from commands.test.env_integration_runner import run
    exit_code = run(repo_root=None, skip_env=skip_env, max_auto_fixes=max_auto_fixes,
                    no_fix=no_fix, inject_fault=inject_fault, docker=docker)
    ctx.exit(exit_code)
