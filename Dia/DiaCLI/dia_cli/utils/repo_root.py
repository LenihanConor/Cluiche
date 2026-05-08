"""Shared repo root discovery for all DiaCLI modules."""
from pathlib import Path


_MARKER = ("Cluiche", "Cluiche.sln")


def find_repo_root(anchor_file: str) -> Path:
    """Walk parents of anchor_file looking for Cluiche/Cluiche.sln.

    Raises RuntimeError if not found (never silently returns a wrong path).
    """
    for parent in Path(anchor_file).resolve().parents:
        if (parent / _MARKER[0] / _MARKER[1]).exists():
            return parent
    raise RuntimeError(
        f"Could not find repo root (marker: {_MARKER[0]}/{_MARKER[1]}) "
        f"from {anchor_file}"
    )
