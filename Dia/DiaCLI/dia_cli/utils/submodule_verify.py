"""Submodule health checks for dia env verify."""
import configparser
import subprocess
from pathlib import Path
from typing import Optional

from dia_cli.utils.check_result import CheckResult


def _parse_gitmodules(repo_root: Path) -> dict:
    """Returns {path: url} from .gitmodules."""
    gitmodules = repo_root / ".gitmodules"
    if not gitmodules.exists():
        return {}
    cfg = configparser.ConfigParser()
    cfg.read(str(gitmodules), encoding="utf-8")
    result = {}
    for section in cfg.sections():
        if section.startswith("submodule "):
            path = cfg.get(section, "path", fallback=None)
            url = cfg.get(section, "url", fallback=None)
            if path:
                result[path] = url
    return result


def _get_submodule_status(repo_root: Path) -> dict:
    """Returns {path: status_char} from `git submodule status`."""
    try:
        result = subprocess.run(
            ["git", "submodule", "status"],
            capture_output=True, text=True, cwd=str(repo_root)
        )
        statuses = {}
        for line in result.stdout.splitlines():
            if not line.strip():
                continue
            status_char = line[0]  # ' ', '-', '+', 'U'
            rest = line[1:].strip()
            parts = rest.split()
            # git submodule status format: <status><hash> <path> [(<description>)]
            # rest.split()[0] is the hash, rest.split()[1] is the path
            path = parts[1] if len(parts) >= 2 else (parts[0] if parts else "")
            if path:
                statuses[path] = status_char
        return statuses
    except Exception:
        return {}


def check_submodules(repo_root: Path) -> list:
    modules = _parse_gitmodules(repo_root)
    if not modules:
        return [CheckResult("submodules", "submodules", "fail",
                            ".gitmodules not found or empty",
                            "Run submodule-migration procedure")]

    statuses = _get_submodule_status(repo_root)
    results = []
    for path, url in modules.items():
        name = Path(path).name
        char = statuses.get(path)
        if char is None:
            results.append(CheckResult(name, "submodules", "fail",
                                       "not registered in git index",
                                       "git submodule update --init --recursive"))
        elif char == "-":
            results.append(CheckResult(name, "submodules", "fail",
                                       "not initialised",
                                       "git submodule update --init --recursive"))
        elif char == "+":
            results.append(CheckResult(name, "submodules", "warn",
                                       "initialised but on wrong commit",
                                       "git submodule update --recursive"))
        elif char == "U":
            results.append(CheckResult(name, "submodules", "warn",
                                       "merge conflict",
                                       "git submodule update --recursive"))
        else:
            results.append(CheckResult(name, "submodules", "pass"))
    return results
