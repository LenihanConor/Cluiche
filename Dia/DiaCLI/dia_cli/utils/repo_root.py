"""Shared repo root discovery for all DiaCLI modules."""
from pathlib import Path


_MARKER = ("Cluiche", "Cluiche.sln")


def find_repo_root(anchor_file: str) -> Path:
    """Walk parents of anchor_file looking for Cluiche/Cluiche.sln.

    Falls back to cwd when anchor_file is inside site-packages (installed mode).
    Raises RuntimeError if not found (never silently returns a wrong path).
    """
    candidates = list(Path(anchor_file).resolve().parents)
    # When installed via pip the anchor is deep in site-packages; also try cwd.
    cwd = Path.cwd().resolve()
    if cwd not in candidates:
        candidates.append(cwd)
        candidates.extend(cwd.parents)
    for parent in candidates:
        if (parent / _MARKER[0] / _MARKER[1]).exists():
            return parent
    raise RuntimeError(
        f"Could not find repo root (marker: {_MARKER[0]}/{_MARKER[1]}) "
        f"from {anchor_file}"
    )
