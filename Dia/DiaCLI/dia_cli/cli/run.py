"""dia run <target> — pipeline (compile + deploy) then launch the target."""
import click
from pathlib import Path

from dia_cli.utils.repo_root import find_repo_root


_CONFIG_ALIASES = {"Asan": "Debug-Asan", "Ubsan": "Debug-Ubsan"}


def _resolve_full_suite_config(repo_root: Path, target: str) -> str:
    """Return full_suite_config for target from pipeline.toml, or 'Release' on any failure."""
    try:
        from dia_cli.commands.pipeline.pipeline_config import load_pipeline_config
        pipeline_config = load_pipeline_config(repo_root)
        t = pipeline_config.targets.get(target)
        return t.full_suite_config if t else "Release"
    except Exception:
        return "Release"


@click.command()
@click.argument("target")
@click.option("--config", default=None, metavar="CONFIG",
              help="Build configuration: Debug, Release, Asan, or Ubsan (default: Debug, or Release with --all).")
@click.option("--filter", "filter_pattern", default=None, metavar="PATTERN",
              help="For googletest: pass --gtest_filter=PATTERN.")
@click.option("--verbose", is_flag=True, default=False,
              help="For googletest: show verbose output.")
@click.option("--no-build", "no_build", is_flag=True, default=False,
              help="Skip pipeline, just launch (same as 'dia launch').")
@click.option("--build-only", "build_only", is_flag=True, default=False,
              help="Run pipeline without launching.")
@click.option("--force", is_flag=True, default=False,
              help="Force pipeline rebuild even if up to date.")
@click.option("--all", "run_all", is_flag=True, default=False,
              help="For googletest: include SLOW_* suites (default excludes them).")
@click.pass_context
def cli(ctx, target, config, filter_pattern, verbose, no_build, build_only, force, run_all):
    """Run the full pipeline then launch a target.

    TARGET is one of: googletest, cluichetest, cluicheeditor.

    Equivalent to: dia pipeline --target TARGET && dia launch TARGET
    """
    # Resolve config: explicit wins; --all without explicit → full_suite_config; else "Debug"
    if config is None:
        if run_all and not no_build:
            config = _resolve_full_suite_config(find_repo_root(__file__), target) or "Release"
        else:
            config = "Debug"
    config = _CONFIG_ALIASES.get(config, config)
    repo_root = find_repo_root(__file__)

    if not no_build:
        exit_code = _run_pipeline(repo_root, target, config, force)
        if exit_code != 0:
            ctx.exit(exit_code)
            return

    if build_only:
        ctx.exit(0)
        return

    from dia_cli.cli.launch import launch_target
    exit_code = launch_target(
        target=target,
        config=config,
        filter_pattern=filter_pattern,
        verbose=verbose,
        run_all=run_all,
    )
    ctx.exit(exit_code)


def _run_pipeline(repo_root: Path, target: str, config: str, force: bool) -> int:
    from dia_cli.commands.pipeline.pipeline_config import load_pipeline_config, PipelineConfigError
    from dia_cli.commands.pipeline.pipeline_runner import run_pipeline
    from dia_cli.utils.dia_output import OutputContext

    try:
        pipeline_config = load_pipeline_config(repo_root)
    except PipelineConfigError as e:
        click.echo(f"ERROR: {e}", err=True)
        return 2

    if target not in pipeline_config.targets:
        known = ", ".join(sorted(pipeline_config.targets))
        click.echo(f"ERROR: unknown target '{target}' (known: {known})", err=True)
        return 2

    log_dir = repo_root / "Cluiche" / "out" / "DiaCLI" / "logs"
    output = OutputContext(log_dir=log_dir)

    stages = pipeline_config.targets[target].stages

    return run_pipeline(
        config=pipeline_config,
        target=target,
        stages=stages,
        build_config=config,
        force=force,
        output=output,
        repo_root=repo_root,
    )
