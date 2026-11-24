"""Library roots (Dia, Draoi) shared by every command that scans module docs or sources.

Layer order is Application -> Draoi -> Dia. Both Dia and Draoi hold module docs named
``dia.*.architecture.module.md`` and are addressed by includes with the root folder stripped
(``<DiaCore/...>``, ``<DraoiHexWorld/...>``).
"""
from __future__ import annotations

from pathlib import Path
from typing import Iterator, Optional

LIBRARY_ROOTS: tuple[str, ...] = ("Dia", "Draoi")

_MODULE_DOC_GLOB = "dia.*.architecture.module.md"
_EXCLUDED_PARTS = frozenset({".claude", "node_modules", "out", "bin", "obj"})


def iter_module_docs(repo_root: Path) -> Iterator[Path]:
    """Yield module docs under every library root, sorted, skipping worktrees and build output."""
    for root in LIBRARY_ROOTS:
        base = Path(repo_root) / root
        if not base.is_dir():
            continue
        for doc in sorted(base.rglob(_MODULE_DOC_GLOB)):
            if _EXCLUDED_PARTS.intersection(doc.relative_to(repo_root).parts):
                continue
            yield doc


def root_of(path: str) -> Optional[str]:
    """Return the library root a repo-relative path belongs to, or None."""
    first = path.replace("\\", "/").strip("/").split("/", 1)[0]
    return first if first in LIBRARY_ROOTS else None


def strip_root(path: str) -> str:
    """Strip the library root and trailing slash: 'Draoi/DraoiHexWorld/' -> 'DraoiHexWorld'."""
    norm = path.replace("\\", "/").strip("/")
    root = root_of(norm)
    if root is None:
        return norm
    return norm[len(root):].lstrip("/")
