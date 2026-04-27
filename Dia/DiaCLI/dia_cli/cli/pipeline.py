"""dia pipeline command group."""
import sys
import click
from loguru import logger
from pathlib import Path

from utils.repo_root import find_repo_root

_VALID_CONFIGS = {"Debug", "Release", "Both"}


@click.command()
@click.option("--config", default=None, metavar="CONFIG",
              help="Build configuration: Debug, Release, or Both. Default from pipeline.toml.")
@click.option("--target", default=None, metavar="TARGET",
              help="Build target (googletest, cluichetest, cluicheeditor). Default from pipeline.toml.")
@click.option("--stage", default=None, metavar="STAGES",
              help="Comma-separated stages to run. Default: all stages for target.")
@click.option("--force", is_flag=True, default=False,
              help="Re-run stages even if sentinel/skip guard says up to date.")
@click.option("--docker", is_flag=True, default=False,
              help="Re-invoke all stages inside Docker container.")
@click.pass_context
def cli(ctx, config, target, stage, force, docker):
    """Run the Cluiche build pipeline."""
    from commands.pipeline.pipeline_config import load_pipeline_config, PipelineConfigError, VALID_STAGES
    from commands.pipeline.pipeline_runner import run_pipeline

    repo_root = find_repo_root(__file__)

    # Load config
    try:
        pipeline_config = load_pipeline_config(repo_root)
    except PipelineConfigError as e:
        click.echo(f"ERROR: {e}", err=True)
        ctx.exit(2)
        return

    # Resolve defaults from config
    build_config = config or pipeline_config.global_cfg.default_config
    active_target = target or pipeline_config.global_cfg.default_target

    # Validate config value
    if build_config not in _VALID_CONFIGS:
        click.echo(f"ERROR: unknown config: {build_config}  (valid: Debug, Release, Both)", err=True)
        ctx.exit(2)
        return

    # Validate target
    if active_target not in pipeline_config.targets:
        known = ", ".join(sorted(pipeline_config.targets))
        click.echo(f"ERROR: unknown target: {active_target}  (known: {known})", err=True)
        ctx.exit(2)
        return

    # Resolve active stages
    if stage:
        requested = [s.strip() for s in stage.split(",")]
        unknown = [s for s in requested if s not in VALID_STAGES]
        if unknown:
            click.echo(f"ERROR: unknown stage: {', '.join(unknown)}", err=True)
            ctx.exit(2)
            return
        active_stages = requested
    else:
        active_stages = pipeline_config.targets[active_target].stages

    if docker:
        _run_docker(repo_root=repo_root, config=build_config, target=active_target,
                    stage=stage, force=force, ctx=ctx)
        return

    # Get OutputContext from Click context obj
    output_ctx = ctx.obj.output if ctx.obj and hasattr(ctx.obj, "output") else None
    if output_ctx is None:
        from dia_cli.utils.dia_output import OutputContext
        log_dir = repo_root / "Cluiche" / "out" / "DiaCLI" / "logs"
        output_ctx = OutputContext(log_dir=log_dir)

    configs_to_run = ["Debug", "Release"] if build_config == "Both" else [build_config]
    final_exit = 0
    for cfg in configs_to_run:
        exit_code = run_pipeline(
            config=pipeline_config,
            target=active_target,
            stages=active_stages,
            build_config=cfg,
            force=force,
            output=output_ctx,
            repo_root=repo_root,
        )
        if exit_code != 0:
            final_exit = exit_code
            break

    ctx.exit(final_exit)


def _run_docker(repo_root: Path, config: str, target: str, stage, force: bool, ctx):
    import subprocess
    image = "cluiche-build-env"
    check = subprocess.run(["docker", "image", "inspect", image], capture_output=True)
    if check.returncode != 0:
        click.echo(f"ERROR: Docker image '{image}' not found. Build it with: dia env docker-build", err=True)
        ctx.exit(3)
        return

    forwarded = ["pipeline", "--config", config, "--target", target]
    if stage:
        forwarded += ["--stage", stage]
    if force:
        forwarded.append("--force")

    cmd = [
        "docker", "run", "--rm",
        "--volume", f"{repo_root}:C:/repo",
        "--workdir", "C:/repo",
        image,
        "python", "-m", "dia_cli",
    ] + forwarded

    result = subprocess.run(cmd)
    ctx.exit(result.returncode)
