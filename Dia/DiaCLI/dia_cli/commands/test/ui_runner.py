"""Shared Vitest runner — used by both editor-ui and game-ui commands."""
import os
import subprocess
from pathlib import Path
from typing import Optional

from utils.repo_root import find_repo_root

_DOCKER_IMAGE = "cluiche-build-env"

# Node.js shipped with Visual Studio — guaranteed present without a separate install.
_VS_NODE_DIR = Path(
    "C:/Program Files/Microsoft Visual Studio/2022/Professional"
    "/MSBuild/Microsoft/VisualStudio/NodeJs"
)
_NODE_EXE = _VS_NODE_DIR / "node.exe"
_NPM_CLI = _VS_NODE_DIR / "node_modules/npm/bin/npm-cli.js"

_REPO_ROOT = find_repo_root(__file__)


def _npm_cmd(script: str, extra_args: list) -> list:
    """Build a node npm-cli.js invocation that works without cmd.exe on PATH."""
    cmd = [str(_NODE_EXE), str(_NPM_CLI), "run", script]
    if extra_args:
        cmd += extra_args
    return cmd


def check_node_modules(ui_dir: Path) -> bool:
    return (ui_dir / "node_modules").exists()


def run(
    repo_root: Optional[Path],
    ui_subpath: str,
    docker_subcmd: str,
    filter_pattern: Optional[str],
    watch: bool,
    docker: bool,
) -> int:
    # config.root_path() points at Dia/DiaCLI/, not the repo root — use _REPO_ROOT by default.
    # repo_root override is accepted for testing.
    root = repo_root if repo_root is not None else _REPO_ROOT
    ui_dir = root / ui_subpath

    if docker:
        return _run_docker(
            repo_root=root,
            ui_subpath=ui_subpath,
            docker_subcmd=docker_subcmd,
            filter_pattern=filter_pattern,
            watch=watch,
        )

    if not check_node_modules(ui_dir):
        print(
            f"ERROR: node_modules not found at {ui_dir / 'node_modules'}\n"
            f"Run: cd {ui_subpath} && npm install"
        )
        return 2

    script = "test:watch" if watch else "test"
    extra = ["--", "-t", filter_pattern] if filter_pattern else []
    cmd = _npm_cmd(script, extra)

    # Ensure VS Node.js is on PATH so npm scripts can resolve 'node'
    env = os.environ.copy()
    env["PATH"] = str(_VS_NODE_DIR) + os.pathsep + env.get("PATH", "")
    try:
        result = subprocess.run(cmd, cwd=str(ui_dir), env=env)
        return result.returncode
    except FileNotFoundError as e:
        print(f"ERROR: command not found: {e}")
        return 1


def _run_docker(
    repo_root: Path,
    ui_subpath: str,
    docker_subcmd: str,
    filter_pattern: Optional[str],
    watch: bool,
) -> int:
    check = subprocess.run(
        ["docker", "image", "inspect", _DOCKER_IMAGE],
        capture_output=True,
    )
    if check.returncode != 0:
        print(
            f"ERROR: Docker image '{_DOCKER_IMAGE}' not found.\n"
            "Build it first with: dia env docker-build"
        )
        return 3

    forwarded = ["test", docker_subcmd]
    if filter_pattern:
        forwarded += ["--filter", filter_pattern]
    if watch:
        forwarded.append("--watch")

    cmd = [
        "docker", "run", "--rm",
        "--volume", f"{repo_root}:C:/repo",
        "--workdir", "C:/repo",
        _DOCKER_IMAGE,
        "python", "-m", "dia_cli",
    ] + forwarded

    result = subprocess.run(cmd)
    return result.returncode
