"""Verify/configure PATH inside the Docker container."""
import subprocess
from pathlib import Path
from typing import Optional

from dia_cli.utils.repo_root import find_repo_root

_DOCKER_IMAGE = "cluiche-build-env"

_REPO_ROOT = find_repo_root(__file__)


def run(repo_root: Optional[Path], force: bool) -> int:
    root = repo_root if repo_root is not None else _REPO_ROOT

    check = subprocess.run(["docker", "image", "inspect", _DOCKER_IMAGE], capture_output=True)
    if check.returncode != 0:
        print(f"ERROR: Docker image '{_DOCKER_IMAGE}' not found\nRun: dia env docker image")
        return 1

    script = root / "build-env" / "scripts" / "configure-paths.ps1"
    if not script.exists():
        print(f"ERROR: configure-paths.ps1 not found at {script}")
        return 1

    cmd = [
        "docker", "run", "--rm",
        "--volume", f"{root}:C:/repo",
        "--workdir", "C:/repo",
        _DOCKER_IMAGE,
        "powershell", "-File", "build-env/scripts/configure-paths.ps1",
    ]
    result = subprocess.run(cmd)
    return result.returncode
