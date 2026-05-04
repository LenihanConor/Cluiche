"""dia launch <target> — execute an already-built target without rebuilding."""
import subprocess
import click
from pathlib import Path

from dia_cli.utils.repo_root import find_repo_root


_TARGET_EXE_MAP = {
    "googletest": "Cluiche/bin/GoogleTests/{config}/x64/GoogleTests.exe",
    "cluichetest": "Cluiche/bin/CluicheTest/{config}/x64/CluicheTest.exe",
    "cluicheeditor": "Cluiche/bin/CluicheEditor/{config}/x64/CluicheEditor.exe",
}


@click.command()
@click.argument("target")
@click.option("--config", default="Debug", metavar="CONFIG",
              help="Build configuration: Debug or Release (default: Debug).")
@click.option("--filter", "filter_pattern", default=None, metavar="PATTERN",
              help="For googletest: pass --gtest_filter=PATTERN.")
@click.option("--verbose", is_flag=True, default=False,
              help="For googletest: show verbose output.")
@click.pass_context
def cli(ctx, target, config, filter_pattern, verbose):
    """Launch an already-built target executable.

    TARGET is one of: googletest, cluichetest, cluicheeditor.

    This does NOT build — use 'dia run <target>' to pipeline + launch.
    """
    exit_code = launch_target(
        target=target,
        config=config,
        filter_pattern=filter_pattern,
        verbose=verbose,
    )
    ctx.exit(exit_code)


def launch_target(target: str, config: str, filter_pattern: str = None,
                  verbose: bool = False) -> int:
    repo_root = find_repo_root(__file__)

    if target not in _TARGET_EXE_MAP:
        known = ", ".join(sorted(_TARGET_EXE_MAP.keys()))
        click.echo(f"ERROR: unknown target '{target}' (known: {known})", err=True)
        return 2

    exe_rel = _TARGET_EXE_MAP[target].format(config=config)
    exe_path = repo_root / exe_rel
    if not exe_path.exists():
        click.echo(
            f"ERROR: {exe_rel} not found.\n"
            f"Build it first with: dia run {target} --config {config}",
            err=True,
        )
        return 2

    cmd = [str(exe_path)]
    if target == "googletest":
        if filter_pattern:
            cmd.append(f"--gtest_filter={filter_pattern}")
        if verbose:
            cmd.append("--gtest_print_time=1")

    out_dir = exe_path.parent
    try:
        result = subprocess.run(cmd, cwd=str(out_dir))
        return result.returncode
    except FileNotFoundError as e:
        click.echo(f"ERROR: could not launch: {e}", err=True)
        return 1
