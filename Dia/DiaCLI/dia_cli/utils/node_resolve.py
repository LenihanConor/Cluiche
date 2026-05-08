"""Resolve the Node.js executable path, probing well-known install locations."""
import os
import shutil
from pathlib import Path
from typing import Optional


_WELL_KNOWN_DIRS = [
    Path("C:/Program Files/nodejs"),
    Path(os.environ.get("LOCALAPPDATA", "C:/Users") + "/Programs/nodejs"),
    Path(os.environ.get("APPDATA", "") + "/nvm") if os.environ.get("APPDATA") else None,
]


def find_node() -> Optional[Path]:
    """Return the absolute path to node.exe, or None if not found."""
    on_path = shutil.which("node")
    if on_path:
        return Path(on_path)

    for d in _WELL_KNOWN_DIRS:
        if d is None:
            continue
        candidate = d / "node.exe"
        if candidate.exists():
            return candidate

        if d.is_dir():
            for sub in d.iterdir():
                if sub.is_dir():
                    candidate = sub / "node.exe"
                    if candidate.exists():
                        return candidate

    return None


def node_dir() -> Optional[Path]:
    """Return the directory containing node.exe, or None."""
    node = find_node()
    return node.parent if node else None
