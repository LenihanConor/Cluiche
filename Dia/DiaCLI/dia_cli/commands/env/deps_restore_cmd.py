"""Orchestration for dia env deps command."""
from pathlib import Path
from typing import Optional

from utils.repo_root import find_repo_root

_REPO_ROOT = find_repo_root(__file__)


def run(repo_root: Optional[Path], dep_id: Optional[str], force: bool, quiet: bool = False) -> int:
    from utils.deps_restore import load_deps, restore_dep, restore_all, DepsManifestError
    root = repo_root if repo_root is not None else _REPO_ROOT
    if dep_id:
        try:
            deps = load_deps(root)
        except DepsManifestError as e:
            print(f"ERROR: {e}")
            return 3
        matches = [d for d in deps if d["id"] == dep_id]
        if not matches:
            print(f"ERROR: dep '{dep_id}' not found in deps.json")
            return 1
        return restore_dep(matches[0], root, force=force, quiet=quiet)
    else:
        return restore_all(root, force=force, quiet=quiet)
