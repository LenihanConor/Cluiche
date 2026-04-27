"""Restore External/ deps inside Docker container."""
import subprocess
from pathlib import Path
from typing import Optional

from utils.repo_root import find_repo_root

_DOCKER_IMAGE = "cluiche-build-env"

_REPO_ROOT = find_repo_root(__file__)


def run(repo_root: Optional[Path], force: bool) -> int:
    root = repo_root if repo_root is not None else _REPO_ROOT

    deps_json = root / "deps.json"
    if not deps_json.exists():
        print("ERROR: deps.json not found — run deps-manifest feature first")
        return 3

    check = subprocess.run(["docker", "image", "inspect", _DOCKER_IMAGE], capture_output=True)
    if check.returncode != 0:
        print(f"ERROR: Docker image '{_DOCKER_IMAGE}' not found\nRun: dia env docker image")
        return 1

    cmd = [
        "docker", "run", "--rm",
        "--volume", f"{root}:C:/repo",
        "--workdir", "C:/repo",
        _DOCKER_IMAGE,
        "python", "-m", "dia_cli", "env", "deps",
    ]
    if force:
        cmd.append("--force")

    result = subprocess.run(cmd)
    return result.returncode
