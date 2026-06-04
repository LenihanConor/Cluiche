"""arch_include_parser — walk source files and resolve #include directives to module IDs."""
from __future__ import annotations

import re
from pathlib import Path
from typing import Optional

from .arch_module_map import ModuleInfo

# Matches: #include <Dia/SomePath/File.h> or #include "Dia/SomePath/File.h"
_INCLUDE_RE = re.compile(r'#\s*include\s+[<"]([^>"]+)[>"]')

# System / SDK headers that should never be resolved to a module
_SYSTEM_PREFIXES = (
    "windows.h", "d3d11", "d3d12", "winrt", "wrl",
    "cstdlib", "cstdint", "cstring", "cstdio", "cmath", "cassert",
    "algorithm", "vector", "string", "map", "unordered_map",
    "set", "unordered_set", "memory", "functional", "utility",
    "type_traits", "stdexcept", "iostream", "sstream", "fstream",
    "thread", "mutex", "atomic", "chrono", "optional", "variant",
    "tuple", "array", "numeric", "limits", "iterator",
)


def resolve_includes(
    source_file: Path,
    repo_root: Path,
    module_map: dict[str, ModuleInfo],
    own_module: ModuleInfo,
) -> list[tuple[str, int, str]]:
    """Return list of (included_module_id, line_number, raw_include_path) for a source file.

    Excludes:
    - System headers
    - External/ includes
    - Intra-module includes (resolved to own_module)
    - Paths that cannot be resolved to any known module
    """
    try:
        lines = source_file.read_text(encoding="utf-8", errors="replace").splitlines()
    except OSError:
        return []

    # Build a lookup: path prefix → module_id for fast resolution
    path_to_module = _build_path_index(module_map)

    results: list[tuple[str, int, str]] = []
    for lineno, line in enumerate(lines, start=1):
        m = _INCLUDE_RE.search(line)
        if not m:
            continue
        include_path = m.group(1)

        if _is_excluded(include_path):
            continue

        resolved = _resolve_path(include_path, path_to_module)
        if resolved is None:
            continue
        if resolved == own_module.module_id:
            continue  # intra-module

        results.append((resolved, lineno, include_path))

    return results


def _build_path_index(module_map: dict[str, ModuleInfo]) -> dict[str, str]:
    """Map normalised module path prefix → module_id.

    For "Dia/DiaCore/Containers/Arrays" we want to resolve any include that
    starts with "DiaCore/Containers/Arrays/" to dia.core.containers.arrays.

    The `path` field in YAML is like "Dia/DiaCore" or "Dia/DiaCore/Containers/Arrays".
    Includes use the form <DiaCore/Containers/Arrays/Foo.h> (without the "Dia/" prefix).
    """
    index: dict[str, str] = {}
    for module_id, info in module_map.items():
        if not info.path:
            continue
        # Strip leading "Dia/" to match the include form
        norm = info.path
        if norm.startswith("Dia/"):
            norm = norm[4:]
        if norm:
            index[norm] = module_id
    return index


def _resolve_path(include_path: str, path_to_module: dict[str, str]) -> Optional[str]:
    """Resolve an include path like 'DiaCore/Foo.h' to a module_id.

    Tries longest-prefix match so sub-modules beat parent modules.
    """
    # Normalise separators
    norm = include_path.replace("\\", "/")

    best_len = 0
    best_id = None
    for prefix, module_id in path_to_module.items():
        prefix_slash = prefix if prefix.endswith("/") else prefix + "/"
        if norm.startswith(prefix_slash) or norm == prefix:
            if len(prefix) > best_len:
                best_len = len(prefix)
                best_id = module_id

    return best_id


def _is_excluded(include_path: str) -> bool:
    """Return True for paths that should never be flagged."""
    lower = include_path.lower()

    # External dependencies
    if lower.startswith("external/") or lower.startswith("bgfx/") or lower.startswith("bx/"):
        return True

    # Standard library / system: no path separator, or common SDK prefixes
    if "/" not in include_path and "\\" not in include_path:
        # bare include like <vector> or <cassert>
        return True

    for prefix in _SYSTEM_PREFIXES:
        if lower.startswith(prefix):
            return True

    return False
