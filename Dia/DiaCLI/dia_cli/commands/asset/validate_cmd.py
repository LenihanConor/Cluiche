import click


@click.command("validate")
@click.option("--target", required=True, help="Target name from pipeline.toml")
@click.option("--config", default="Debug", type=click.Choice(["Debug", "Release"]))
@click.option("--platform", default="x64", type=click.Choice(["x64"]))
@click.pass_context
def validate(ctx, target, config, platform):
    """Validate assets only — no file writes."""
    from .validate_handler import handle_validate
    ctx.exit(handle_validate(ctx=ctx, target=target, config=config, platform=platform))
