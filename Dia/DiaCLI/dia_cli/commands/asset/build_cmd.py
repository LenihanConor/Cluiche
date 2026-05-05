import click


@click.command("build")
@click.option("--target", required=True, help="Target name from pipeline.toml")
@click.option("--config", default="Debug", type=click.Choice(["Debug", "Release"]),
              help="Build configuration (default: Debug)")
@click.option("--platform", default="x64", type=click.Choice(["x64"]),
              help="Target platform (default: x64)")
@click.option("--force", is_flag=True, default=False,
              help="Clean deploy directory before building")
@click.pass_context
def build(ctx, target, config, platform, force):
    """Run the full asset pipeline: validate, transform, deploy."""
    from .build_handler import handle_build
    ctx.exit(handle_build(ctx=ctx, target=target, config=config, platform=platform, force=force))
