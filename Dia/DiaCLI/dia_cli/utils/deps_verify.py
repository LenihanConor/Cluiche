"""Sentinel-based dep verification for dia env verify."""
import json
from pathlib import Path

from utils.check_result import CheckResult


def check_deps(repo_root: Path) -> list:
    from utils.deps_restore import load_deps, get_sentinel_path, DepsManifestError
    try:
        deps = load_deps(repo_root)
    except DepsManifestError as e:
        return [CheckResult("deps.json", "deps", "fail", str(e), "dia env deps")]

    results = []
    for dep in deps:
        dep_id = dep["id"]
        sentinel = get_sentinel_path(repo_root, dep_id)
        unzip_to = dep.get("unzip_to")
        install_to = dep.get("install_to")
        artifact_exists = (
            (repo_root / unzip_to).exists() if unzip_to
            else (repo_root / install_to).exists() if install_to
            else False
        )

        if sentinel.exists():
            try:
                data = json.loads(sentinel.read_text(encoding="utf-8"))
                if data.get("version") == dep["version"]:
                    results.append(CheckResult(dep_id, "deps", "pass", dep["version"]))
                else:
                    results.append(CheckResult(dep_id, "deps", "warn",
                                               f"sentinel version {data.get('version')} != {dep['version']}",
                                               f"dia env deps --dep {dep_id} --force"))
            except Exception:
                results.append(CheckResult(dep_id, "deps", "warn",
                                           "sentinel unreadable",
                                           f"dia env deps --dep {dep_id} --force"))
        elif artifact_exists:
            results.append(CheckResult(dep_id, "deps", "warn",
                                       "artifact exists but no sentinel (manually placed)",
                                       f"dia env deps --dep {dep_id} --force"))
        else:
            results.append(CheckResult(dep_id, "deps", "fail",
                                       "not restored",
                                       f"dia env deps --dep {dep_id}"))
    return results
