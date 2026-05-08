import click


@click.command("deploy")
@click.option("--target", required=True, help="Target name from pipeline.toml")
@click.option("--config", default="Debug", type=click.Choice(["Debug", "Release"]))
@click.option("--platform", default="x64", type=click.Choice(["x64"]))
@click.option("--force", is_flag=True, default=False,
              help="Clean deploy directory before deploying")
@click.pass_context
def deploy(ctx, target, config, platform, force):
    """Deploy assets only — copies source files directly, skipping validate and transform.

    Use 'dia asset build' for the full pipeline (validate + transform + deploy).
    """
    from .deploy_handler import handle_deploy
    ctx.exit(handle_deploy(ctx=ctx, target=target, config=config, platform=platform, force=force))
