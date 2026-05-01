"""Orchestration for dia env setup command."""
import ctypes
import subprocess
from pathlib import Path
from typing import Optional

from utils.repo_root import find_repo_root

_REPO_ROOT = find_repo_root(__file__)


def _is_admin() -> bool:
    try:
        return bool(ctypes.windll.shell32.IsUserAnAdmin())
    except Exception:
        return False


def run(
    repo_root: Optional[Path],
    toolchain: bool,
    deps_only: bool,
    dep_id: Optional[str],
    submodules: bool,
    claude: bool,
    force: bool,
    fail_fast: bool,
    quiet: bool,
) -> int:
    root = repo_root if repo_root is not None else _REPO_ROOT

    # Validate --dep flag usage
    if dep_id and (toolchain or submodules or claude):
        print("ERROR: --dep can only be combined with --deps, not --toolchain/--submodules/--claude")
        return 2

    # Determine which steps to run
    run_all = not any([toolchain, deps_only, dep_id, submodules, claude])
    steps = []
    if run_all or toolchain:
        steps.append("toolchain")
    if run_all or deps_only or dep_id:
        steps.append("deps")
    if run_all or submodules:
        steps.append("submodules")
    if run_all or claude:
        steps.append("claude")

    total = len(steps)
    passed = 0
    warned = 0
    failed = 0
    admin = _is_admin()

    if not quiet:
        print("[dia env setup]")

    for i, step in enumerate(steps, 1):
        label = f"  [{i}/{total}] {step.capitalize():<12}"

        if step == "toolchain":
            if not admin:
                warned += 1
                print(f"{label} SKIPPED — not running as administrator")
                print(f"         Re-run: dia env setup --toolchain  (as Administrator)")
                if fail_fast:
                    break
                continue
            if not quiet:
                print(f"{label} installing via winget...")
            code = _run_toolchain(root, force, quiet)

        elif step == "deps":
            if not quiet:
                print(f"{label} restoring deps...")
            from commands.env.deps_restore_cmd import run as deps_run
            code = deps_run(repo_root=root, dep_id=dep_id, force=force, quiet=quiet)

        elif step == "submodules":
            if not quiet:
                print(f"{label} initialising submodules...")
            code = _run_submodules(root, quiet)

        elif step == "claude":
            if not quiet:
                print(f"{label} wiring AI context...")
            from commands.env.claude_context_cmd import run as claude_run
            code = claude_run(repo_root=root, force=force)

        else:
            continue

        if code == 0:
            passed += 1
            if not quiet:
                print(f"{label} done")
        elif code == 2:
            warned += 1
            if not quiet:
                print(f"{label} WARN")
        else:
            failed += 1
            if not quiet:
                print(f"{label} FAILED (exit {code})")
            if fail_fast:
                break

    if not quiet:
        print(f"\nSetup complete: {passed} passed, {warned} warned, {failed} failed")
        print("Run `dia env verify` to confirm environment health.")

    if failed > 0:
        return 1
    if warned > 0:
        return 2
    return 0


def _run_toolchain(repo_root: Path, force: bool, quiet: bool) -> int:
    winget_json = repo_root / "winget.json"
    if not winget_json.exists():
        print(f"ERROR: winget.json not found at {winget_json}")
        return 1
    cmd = ["winget", "import", str(winget_json),
           "--accept-package-agreements", "--accept-source-agreements"]
    try:
        result = subprocess.run(cmd, timeout=600)
        return result.returncode
    except FileNotFoundError:
        print("ERROR: winget not found on PATH")
        return 1
    except subprocess.TimeoutExpired:
        print("ERROR: winget import timed out after 10 minutes")
        return 1


def _run_submodules(repo_root: Path, quiet: bool) -> int:
    cmd = ["git", "submodule", "update", "--init", "--recursive"]
    try:
        result = subprocess.run(cmd, cwd=str(repo_root), timeout=300)
        return result.returncode
    except FileNotFoundError:
        print("ERROR: git not found on PATH")
        return 1
    except subprocess.TimeoutExpired:
        print("ERROR: git submodule update timed out after 5 minutes")
        return 1
