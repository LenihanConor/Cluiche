"""Implementation for dia test cli command."""
import subprocess
import sys
from pathlib import Path
from typing import Optional

from utils.repo_root import find_repo_root

_DOCKER_IMAGE = "cluiche-build-env"
_CLI_SUBPATH = "Dia/DiaCLI"
_TESTS_DIR = "tests"

_REPO_ROOT = find_repo_root(__file__)


def run(
    repo_root: Optional[Path],
    filter_pattern: Optional[str],
    parallel: bool,
    coverage_out: Optional[str],
    docker: bool,
) -> int:
    root = repo_root if repo_root is not None else _REPO_ROOT

    if docker:
        return _run_docker(repo_root=root, filter_pattern=filter_pattern,
                           parallel=parallel, coverage_out=coverage_out)

    cli_dir = root / _CLI_SUBPATH
    cmd = [sys.executable, "-m", "pytest", _TESTS_DIR, "-m", "not integration"]

    if filter_pattern:
        cmd += ["-k", filter_pattern]
    if parallel:
        cmd += ["-n", "auto"]
    if coverage_out:
        cmd += ["--cov=dia_cli", f"--cov-report=xml:{coverage_out}"]

    try:
        result = subprocess.run(cmd, cwd=str(cli_dir))
        return result.returncode
    except FileNotFoundError as e:
        print(f"ERROR: command not found: {e}")
        return 1


def _run_docker(
    repo_root: Path,
    filter_pattern: Optional[str],
    parallel: bool,
    coverage_out: Optional[str],
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

    forwarded = ["test", "cli"]
    if filter_pattern:
        forwarded += ["--filter", filter_pattern]
    if parallel:
        forwarded.append("--parallel")
    if coverage_out:
        forwarded += ["--coverage-out", coverage_out]

    cmd = [
        "docker", "run", "--rm",
        "--volume", f"{repo_root}:C:/repo",
        "--workdir", "C:/repo",
        _DOCKER_IMAGE,
        "python", "-m", "dia_cli",
    ] + forwarded

    result = subprocess.run(cmd)
    return result.returncode
