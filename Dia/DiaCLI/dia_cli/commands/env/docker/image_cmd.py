"""Build or pull the Docker image."""
import subprocess
from pathlib import Path
from typing import Optional

from utils.repo_root import find_repo_root

_DOCKER_IMAGE = "cluiche-build-env"
_SENTINEL = ".docker-env/image.sentinel"

_REPO_ROOT = find_repo_root(__file__)


def run(repo_root: Optional[Path], force: bool) -> int:
    root = repo_root if repo_root is not None else _REPO_ROOT
    sentinel = root / _SENTINEL

    if sentinel.exists() and not force:
        check = subprocess.run(["docker", "image", "inspect", _DOCKER_IMAGE],
                               capture_output=True)
        if check.returncode == 0:
            print(f"Docker image '{_DOCKER_IMAGE}' already built (use --force to rebuild)")
            return 0

    dockerfile = root / "build-env" / "Dockerfile"
    if not dockerfile.exists():
        print(f"ERROR: Dockerfile not found at {dockerfile}")
        return 1

    print(f"Building Docker image '{_DOCKER_IMAGE}'...")
    result = subprocess.run(
        ["docker", "build", "-t", _DOCKER_IMAGE, str(root / "build-env")],
        cwd=str(root)
    )
    if result.returncode == 0:
        sentinel.parent.mkdir(parents=True, exist_ok=True)
        sentinel.write_text("ok")
        print(f"Docker image '{_DOCKER_IMAGE}' built successfully")
    return result.returncode
