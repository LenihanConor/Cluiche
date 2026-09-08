"""dia launch <target> — execute an already-built target without rebuilding."""
import subprocess
import click
from pathlib import Path

from dia_cli.utils.repo_root import find_repo_root


_CONFIG_ALIASES = {"Asan": "Debug-Asan", "Ubsan": "Debug-Ubsan"}

_TARGET_EXE_MAP = {
    "googletest": "Cluiche/bin/GoogleTests/{config}/x64/GoogleTests.exe",
    "cluichetest": "Cluiche/bin/CluicheTest/{config}/x64/CluicheTest.exe",
    "cluicheeditor": "Cluiche/bin/CluicheEditor/{config}/x64/CluicheEditor.exe",
}


@click.command()
@click.argument("target")
@click.option("--config", default="Debug", metavar="CONFIG",
              help="Build configuration: Debug, Release, Asan, or Ubsan (default: Debug).")
@click.option("--filter", "filter_pattern", default=None, metavar="PATTERN",
              help="For googletest: pass --gtest_filter=PATTERN.")
@click.option("--verbose", is_flag=True, default=False,
              help="For googletest: show verbose output.")
@click.option("--all", "run_all", is_flag=True, default=False,
              help="For googletest: include SLOW_* suites (default excludes them).")
@click.option("--shards", default=0, metavar="N", type=int,
              help="For googletest: run in N parallel shards (0=disabled, omit for cpu_count-1).")
@click.option("--automation", "automation", is_flag=True, default=False,
              help="For cluichetest: enable timed auto-exit after each stage resolves (used by E2E runner).")
@click.pass_context
def cli(ctx, target, config, filter_pattern, verbose, run_all, shards, automation):
    """Launch an already-built target executable.

    TARGET is one of: googletest, cluichetest, cluicheeditor.

    This does NOT build — use 'dia run <target>' to pipeline + launch.
    """
    exit_code = launch_target(
        target=target,
        config=config,
        filter_pattern=filter_pattern,
        verbose=verbose,
        run_all=run_all,
        shards=shards,
        automation=automation,
    )
    ctx.exit(exit_code)


def launch_target(target: str, config: str, filter_pattern: str = None,
                  verbose: bool = False, run_all: bool = False, shards: int = 0,
                  automation: bool = False) -> int:
    config = _CONFIG_ALIASES.get(config, config)
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

    if target == "googletest" and shards > 1:
        from dia_cli.commands.test.googletest_runner import run as gtest_run
        return gtest_run(
            repo_root=repo_root,
            config=config,
            filter_pattern=filter_pattern,
            verbose=verbose,
            docker=False,
            run_all=run_all,
            shards=shards,
        )

    cmd = [str(exe_path)]
    if target == "cluichetest" and automation:
        cmd.append("--automation")
    if target == "googletest":
        from dia_cli.commands.test.googletest_runner import (
            _gtest_xml_output_path,
            _warn_untagged_slow_suites,
        )
        if filter_pattern:
            cmd.append(f"--gtest_filter={filter_pattern}")
        elif not run_all:
            cmd.append("--gtest_filter=-SLOW_*")
        if verbose:
            cmd.append("--gtest_print_time=1")
        out_xml = _gtest_xml_output_path(repo_root)
        out_xml.parent.mkdir(parents=True, exist_ok=True)
        cmd.append(f"--gtest_output=xml:{out_xml}")

    out_dir = exe_path.parent
    try:
        result = subprocess.run(cmd, cwd=str(out_dir))
        if target == "googletest":
            _warn_untagged_slow_suites(out_xml)
        if result.returncode == 0:
            click.echo(f"[dia] {target}: PASSED (exit 0)")
        else:
            click.echo(f"[dia] {target}: FAILED (exit {result.returncode})", err=True)
        return result.returncode
    except FileNotFoundError as e:
        click.echo(f"ERROR: could not launch: {e}", err=True)
        return 1
