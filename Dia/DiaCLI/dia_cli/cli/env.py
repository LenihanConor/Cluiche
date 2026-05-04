"""dia env command group."""
import click


@click.group()
def cli():
    """Manage the Cluiche development environment."""
    pass


@cli.command("deps")
@click.option("--dep", "dep_id", default=None, metavar="ID", help="Restore a single named dep.")
@click.option("--force", is_flag=True, default=False, help="Re-download even if sentinel present.")
@click.option("--quiet", is_flag=True, default=False, help="Suppress progress; print errors only.")
@click.pass_context
def deps(ctx, dep_id, force, quiet):
    """Restore binary SDK dependencies from deps.json."""
    from dia_cli.commands.env.deps_restore_cmd import run
    exit_code = run(repo_root=None, dep_id=dep_id, force=force, quiet=quiet)
    ctx.exit(exit_code)


@cli.command("setup")
@click.option("--toolchain", is_flag=True, default=False, help="Run toolchain install step only.")
@click.option("--deps", "deps_only", is_flag=True, default=False, help="Run deps restore step only.")
@click.option("--dep", "dep_id", default=None, metavar="ID", help="Restore a single named dep.")
@click.option("--submodules", is_flag=True, default=False, help="Run submodule init step only.")
@click.option("--claude", is_flag=True, default=False, help="Run AI context wiring step only.")
@click.option("--force", is_flag=True, default=False, help="Re-run all steps even if sentinels present.")
@click.option("--fail-fast", "fail_fast", is_flag=True, default=False, help="Abort on first step failure.")
@click.option("--quiet", is_flag=True, default=False, help="Suppress progress output.")
@click.pass_context
def setup(ctx, toolchain, deps_only, dep_id, submodules, claude, force, fail_fast, quiet):
    """Provision a fresh developer machine end-to-end."""
    from dia_cli.commands.env.setup_orchestrator import run
    exit_code = run(repo_root=None, toolchain=toolchain, deps_only=deps_only,
                    dep_id=dep_id, submodules=submodules, claude=claude,
                    force=force, fail_fast=fail_fast, quiet=quiet)
    ctx.exit(exit_code)


@cli.command("verify")
@click.option("--toolchain", is_flag=True, default=False)
@click.option("--deps", "deps_only", is_flag=True, default=False)
@click.option("--submodules", is_flag=True, default=False)
@click.option("--docker", "docker_only", is_flag=True, default=False)
@click.option("--claude", is_flag=True, default=False)
@click.option("--json", "output_json", is_flag=True, default=False, help="Machine-readable JSON output.")
@click.option("--quiet", is_flag=True, default=False, help="Print only WARNs and FAILs.")
@click.pass_context
def verify(ctx, toolchain, deps_only, submodules, docker_only, claude, output_json, quiet):
    """Check environment health (read-only, CI-safe)."""
    from dia_cli.commands.env.verify_orchestrator import run
    exit_code = run(repo_root=None, toolchain=toolchain, deps_only=deps_only,
                    submodules=submodules, docker_only=docker_only, claude=claude,
                    output_json=output_json, quiet=quiet)
    ctx.exit(exit_code)


@cli.command("claude-setup")
@click.option("--force", is_flag=True, default=False, help="Overwrite existing settings.local.json.")
@click.pass_context
def claude_setup(ctx, force):
    """Generate .claude/settings.local.json and wire memory symlink."""
    from dia_cli.commands.env.claude_context_cmd import run
    exit_code = run(repo_root=None, force=force)
    ctx.exit(exit_code)


@cli.group("docker")
def docker():
    """Manage the Docker Windows Container build environment."""
    pass


@docker.command("image")
@click.option("--force", is_flag=True, default=False)
@click.pass_context
def docker_image(ctx, force):
    """Build or pull the Docker image (MSVC + Windows SDK)."""
    from dia_cli.commands.env.docker.image_cmd import run
    exit_code = run(repo_root=None, force=force)
    ctx.exit(exit_code)


@docker.command("deps")
@click.option("--force", is_flag=True, default=False)
@click.pass_context
def docker_deps(ctx, force):
    """Restore External/ deps inside the container via deps.json."""
    from dia_cli.commands.env.docker.deps_cmd import run
    exit_code = run(repo_root=None, force=force)
    ctx.exit(exit_code)


@docker.command("paths")
@click.option("--force", is_flag=True, default=False)
@click.pass_context
def docker_paths(ctx, force):
    """Verify/configure PATH inside the container."""
    from dia_cli.commands.env.docker.paths_cmd import run
    exit_code = run(repo_root=None, force=force)
    ctx.exit(exit_code)
