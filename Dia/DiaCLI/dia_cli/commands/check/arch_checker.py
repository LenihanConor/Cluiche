"""arch_checker — orchestrate the architecture audit (tasks 13-17)."""
from __future__ import annotations

import re
from dataclasses import dataclass
from pathlib import Path

from dia_cli.utils.library_roots import root_of

from .arch_module_map import ModuleInfo, build_module_map
from .arch_include_parser import resolve_includes
from .arch_layer_rules import check_layer_violation


@dataclass
class ArchViolation:
    from_module: str
    to_module: str
    rule: str
    source_file: str    # repo-relative path
    lineno: int
    include_path: str
    kind: str           # "forbidden" | "boundary" | "layer"


_NON_APP_DIRS = {"assets", "bin", "out", "external"}
_INCLUDE_RE = re.compile(r'#\s*include\s+[<"]([^>"]+)[>"]')


def application_names(repo_root: Path) -> set[str]:
    """Top-level folders under Cluiche/ that hold application code (CoW, CluicheTest, ...)."""
    base = repo_root / "Cluiche"
    if not base.is_dir():
        return set()
    return {
        p.name for p in base.iterdir()
        if p.is_dir() and p.name.lower() not in _NON_APP_DIRS
    }


def _application_include(include_path: str, apps: set[str]) -> str | None:
    """Return the application an include path points into, or None.

    Matches both ``CoW/...`` (Cluiche/ on the include path) and ``Cluiche/CoW/...``.
    """
    parts = include_path.replace("\\", "/").lstrip("/").split("/")
    if len(parts) < 2:
        return None
    if parts[0] == "Cluiche" and parts[1] in apps:
        return parts[1]
    if parts[0] in apps:
        return parts[0]
    return None


def _application_violations(
    module_id: str, rel_src: str, src: Path, apps: set[str],
) -> list[ArchViolation]:
    try:
        lines = src.read_text(encoding="utf-8", errors="replace").splitlines()
    except OSError:
        return []
    out: list[ArchViolation] = []
    for lineno, line in enumerate(lines, start=1):
        m = _INCLUDE_RE.search(line)
        if not m:
            continue
        app = _application_include(m.group(1), apps)
        if app:
            out.append(ArchViolation(
                from_module=module_id,
                to_module=f"application:{app}",
                rule="Dia and Draoi may not depend on application code (layer order: Application -> Draoi -> Dia)",
                source_file=rel_src,
                lineno=lineno,
                include_path=m.group(1),
                kind="boundary",
            ))
    return out


def run_arch_check(
    repo_root: Path,
    module_filter: str | None = None,
    summary_only: bool = False,
) -> tuple[list[ArchViolation], list[str], int]:
    """Run the full architecture audit.

    Returns (violations, warnings, exit_code).
    exit_code is 0 if clean, 1 if violations found.
    """
    # Build module map
    module_map, warnings = build_module_map(repo_root)

    violations: list[ArchViolation] = []
    apps = application_names(repo_root)

    # Select modules to scan
    if module_filter:
        targets = {
            mid: info
            for mid, info in module_map.items()
            if mid == module_filter or mid.startswith(module_filter + ".")
        }
        if not targets:
            warnings.append(
                f"WARN: --module '{module_filter}' matched no known modules"
            )
    else:
        targets = module_map

    # Pre-compute all registered module directories so broad-path modules (e.g.
    # dia.root with path "Dia") don't claim files that belong to a more-specific
    # child module (e.g. DiaWebSocket, DiaWindow).
    all_module_dirs = {
        repo_root / info.path
        for info in module_map.values()
        if info.path
    }

    for module_id, module_info in sorted(targets.items()):
        if not module_info.path:
            continue

        module_dir = repo_root / module_info.path
        if not module_dir.is_dir():
            continue

        # Directories of modules that are proper subdirectories of this one.
        child_module_dirs = {
            p for p in all_module_dirs
            if p != module_dir and module_dir in p.parents
        }

        raw_files = list(module_dir.rglob("*.h")) + list(module_dir.rglob("*.cpp"))
        source_files = (
            [f for f in raw_files if not any(cd in f.parents for cd in child_module_dirs)]
            if child_module_dirs else raw_files
        )

        for src in sorted(source_files):
            rel_src = src.relative_to(repo_root).as_posix()
            includes = resolve_includes(src, repo_root, module_map, module_info)

            if apps and root_of(module_info.path) in ("Dia", "Draoi"):
                violations.extend(
                    _application_violations(module_id, rel_src, src, apps)
                )

            for dep_id, lineno, raw_path in includes:
                dep_info = module_map.get(dep_id)

                # Check 1: forbidden deps
                if dep_id in module_info.forbidden:
                    violations.append(ArchViolation(
                        from_module=module_id,
                        to_module=dep_id,
                        rule="listed in dependencies.forbidden",
                        source_file=rel_src,
                        lineno=lineno,
                        include_path=raw_path,
                        kind="forbidden",
                    ))
                    continue  # don't double-report the same include

                # Check 2: root boundary. Dia must never depend on Draoi. Keyed on the
                # folder, not the layer, so Dia modules with an unparsed layer are covered.
                if (dep_info and root_of(module_info.path) == "Dia"
                        and root_of(dep_info.path) == "Draoi"):
                    violations.append(ArchViolation(
                        from_module=module_id,
                        to_module=dep_id,
                        rule="Dia may not depend on Draoi (layer order: Application -> Draoi -> Dia)",
                        source_file=rel_src,
                        lineno=lineno,
                        include_path=raw_path,
                        kind="boundary",
                    ))
                    continue

                # Check 3: layer ordering
                if module_info.layer and dep_info and dep_info.layer:
                    rule_violation = check_layer_violation(
                        module_info.layer, dep_info.layer
                    )
                    if rule_violation:
                        violations.append(ArchViolation(
                            from_module=module_id,
                            to_module=dep_id,
                            rule=rule_violation,
                            source_file=rel_src,
                            lineno=lineno,
                            include_path=raw_path,
                            kind="layer",
                        ))

    exit_code = 1 if violations else 0
    return violations, warnings, exit_code


def format_violations(
    violations: list[ArchViolation],
    warnings: list[str],
    summary_only: bool = False,
) -> str:
    """Format violations and warnings into a human-readable report string."""
    lines: list[str] = []

    for w in warnings:
        lines.append(w)

    if warnings and not summary_only:
        lines.append("")

    if not summary_only:
        for v in violations:
            lines.append(
                f"ARCH VIOLATION: {v.from_module} depends on {v.to_module}"
            )
            lines.append(f"  File: {v.source_file}:{v.lineno}")
            lines.append(f"  Include: <{v.include_path}>")
            lines.append(f"  Rule: {v.rule}")
            lines.append("")

    total = len(violations)
    if total == 0:
        lines.append("Total: 0 violations — architecture is clean")
    else:
        lines.append(f"Total: {total} violation(s) found")

    return "\n".join(lines)
