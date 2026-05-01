"""Implementation for dia test googletest command."""
import subprocess
from pathlib import Path
from typing import Optional

from utils.repo_root import find_repo_root

_DOCKER_IMAGE = "cluiche-build-env"
_BINARY_SUBPATH = "Cluiche/bin/{config}/x64/GoogleTests.exe"
_PYTHON_DLL = "python311.dll"

_REPO_ROOT = find_repo_root(__file__)


def find_binary(repo_root: Path, config: str) -> Optional[Path]:
    p = repo_root / _BINARY_SUBPATH.format(config=config)
    return p if p.exists() else None


def run(
    repo_root: Optional[Path],
    config: str,
    filter_pattern: Optional[str],
    verbose: bool,
    docker: bool,
) -> int:
    root = repo_root if repo_root is not None else _REPO_ROOT

    if docker:
        return _run_docker(repo_root=root, config=config,
                           filter_pattern=filter_pattern, verbose=verbose)

    binary = find_binary(root, config)
    if binary is None:
        binary_rel = _BINARY_SUBPATH.format(config=config)
        print(
            f"ERROR: GoogleTests.exe not found at {binary_rel}\n"
            f"Run: dia pipeline --stage compile-code --target googletest --config {config}"
        )
        return 2

    out_dir = binary.parent
    if not (out_dir / _PYTHON_DLL).exists():
        print(
            f"ERROR: Runtime dependencies not staged in {out_dir}\n"
            f"Run: dia pipeline --stage deploy --target googletest --config {config}"
        )
        return 2

    cmd = [str(binary)]
    if filter_pattern:
        cmd.append(f"--gtest_filter={filter_pattern}")
    if verbose:
        cmd.append("--gtest_print_time=1")

    try:
        result = subprocess.run(cmd, cwd=str(out_dir))
        return result.returncode
    except FileNotFoundError as e:
        print(f"ERROR: command not found: {e}")
        return 1


def _run_docker(repo_root: Path, config: str, filter_pattern: Optional[str], verbose: bool) -> int:
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

    forwarded = ["test", "googletest", "--config", config]
    if filter_pattern:
        forwarded += ["--filter", filter_pattern]
    if verbose:
        forwarded.append("--verbose")

    cmd = [
        "docker", "run", "--rm",
        "--volume", f"{repo_root}:C:/repo",
        "--workdir", "C:/repo",
        _DOCKER_IMAGE,
        "python", "-m", "dia_cli",
    ] + forwarded

    result = subprocess.run(cmd)
    return result.returncode
