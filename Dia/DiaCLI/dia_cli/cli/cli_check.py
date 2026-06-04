"""dia check — validate structural consistency of modules and dependencies."""
from __future__ import annotations

import re
from pathlib import Path
from typing import Dict, List, Set, Tuple

import click

from dia_cli.utils.repo_root import find_repo_root


# ---------------------------------------------------------------------------
# YAML frontmatter parser (no PyYAML dependency)
# ---------------------------------------------------------------------------

def _parse_frontmatter(text: str) -> dict:
    """Extract key fields from dia.module.v1 YAML frontmatter."""
    lines = text.splitlines()
    if not lines or lines[0].strip() != "---":
        return {}

    end = -1
    for i in range(1, len(lines)):
        if lines[i].strip() == "---":
            end = i
            break
    if end == -1:
        return {}

    fm_lines = lines[1:end]
    result: dict = {"dependent_modules": []}

    in_deps = False
    for line in fm_lines:
        if line.startswith("module_id:"):
            result["module_id"] = line.split(":", 1)[1].strip().strip('"').strip("'")
            in_deps = False
        elif line.startswith("path:"):
            result["path"] = line.split(":", 1)[1].strip().strip('"').strip("'")
            in_deps = False
        elif line.startswith("dependent_modules:"):
            rest = line.split(":", 1)[1].strip()
            if rest == "[]":
                result["dependent_modules"] = []
            in_deps = True
        elif in_deps and line.startswith("  - "):
            dep = line[4:].strip().strip('"').strip("'")
            result["dependent_modules"].append(dep)
        elif not line.startswith("  ") and not line.startswith("\t"):
            in_deps = False

    return result


# ---------------------------------------------------------------------------
# Include scanning
# ---------------------------------------------------------------------------

_INCLUDE_RE = re.compile(r'#include\s+[<"]([^>"]+)[>"]')


def _scan_includes(source_file: Path) -> Set[str]:
    """Return set of include paths from a source file."""
    includes = set()
    try:
        text = source_file.read_text(encoding="utf-8", errors="ignore")
    except OSError:
        return includes
    for match in _INCLUDE_RE.finditer(text):
        includes.add(match.group(1).replace("\\", "/"))
    return includes


# ---------------------------------------------------------------------------
# Path-to-module mapping
# ---------------------------------------------------------------------------

def _build_path_to_module_map(modules: List[dict]) -> Dict[str, str]:
    """Map directory prefixes (e.g. 'DiaCore/Containers/') to module_ids."""
    mapping: Dict[str, str] = {}
    for mod in modules:
        path = mod.get("path", "")
        if path.startswith("Dia/"):
            path = path[4:]  # Strip leading "Dia/" to match include style
        path = path.rstrip("/")
        if path:
            mapping[path] = mod["module_id"]
    return mapping


def _resolve_include_to_module(include_path: str, path_map: Dict[str, str]) -> str | None:
    """Given an include like 'DiaCore/Containers/Array.h', find its module_id."""
    parts = include_path.replace("\\", "/")
    # Try progressively shorter prefixes
    segments = parts.split("/")
    for i in range(len(segments), 0, -1):
        prefix = "/".join(segments[:i])
        if prefix in path_map:
            return path_map[prefix]
    return None


# ---------------------------------------------------------------------------
# Click command
# ---------------------------------------------------------------------------

@click.group("check")
def cli():
    """Validate structural consistency of the codebase."""


@cli.command("arch")
@click.option("--module", "module_filter", default=None, metavar="MODULE_ID",
              help="Scope check to one module by module_id.")
@click.option("--summary", "summary_only", is_flag=True, default=False,
              help="Print violation count only (no file paths).")
def arch(module_filter: str, summary_only: bool) -> None:
    """Audit module layer dependencies — detect forbidden deps and layer ordering violations."""
    from dia_cli.commands.check.arch_checker import run_arch_check, format_violations

    repo_root = find_repo_root(__file__)
    click.echo("[dia check] Running architecture audit...")

    violations, warnings, exit_code = run_arch_check(
        repo_root=repo_root,
        module_filter=module_filter,
        summary_only=summary_only,
    )

    # Full report always written to file; console respects --summary
    full_report = format_violations(violations, warnings, summary_only=False)
    console_report = format_violations(violations, warnings, summary_only=summary_only)

    out_dir = repo_root / "Cluiche" / "out" / "check"
    out_dir.mkdir(parents=True, exist_ok=True)
    out_file = out_dir / "arch-violations.txt"
    out_file.write_text(full_report, encoding="utf-8")

    if console_report:
        click.echo(console_report)

    click.echo(f"[dia check] arch-violations.txt written to {out_file}")

    if exit_code != 0:
        raise SystemExit(exit_code)


@cli.command("deps")
@click.option("--fix", is_flag=True, default=False, help="Auto-add missing deps to module.md files.")
@click.option("--verbose", is_flag=True, default=False, help="Show each module being checked.")
def deps(fix: bool, verbose: bool) -> None:
    """Cross-check module dependency declarations against actual #include usage."""
    repo_root = find_repo_root(__file__)

    # 1. Find all module.md files (exclude worktrees)
    module_files = [f for f in repo_root.glob("Dia/**/dia.*.architecture.module.md")
                    if ".claude" not in str(f)]
    if not module_files:
        click.echo("No module files found.")
        return

    # 2. Parse all modules
    modules: List[dict] = []
    for mf in module_files:
        fm = _parse_frontmatter(mf.read_text(encoding="utf-8", errors="ignore"))
        if fm.get("module_id") and fm.get("path"):
            fm["_file"] = mf
            modules.append(fm)

    click.echo(f"Checking {len(modules)} modules...\n")

    # 3. Build path→module map (skip modules with very short paths like "Dia/")
    modules = [m for m in modules if len(m.get("path", "").rstrip("/").split("/")) >= 3]

    path_map = _build_path_to_module_map(modules)

    # 4. For each module, scan its sources
    missing: List[Tuple[str, str, str]] = []  # (module_id, included_module, include_path)
    stale: List[Tuple[str, str]] = []  # (module_id, declared_dep)

    for mod in modules:
        module_id = mod["module_id"]
        mod_path = repo_root / mod["path"]
        declared_deps = set(mod.get("dependent_modules", []))

        if verbose:
            click.echo(f"  {module_id} ({mod_path})")

        if not mod_path.exists():
            continue

        # Scan all .h and .cpp files
        used_modules: Set[str] = set()
        for ext in ("*.h", "*.cpp"):
            for src in mod_path.rglob(ext):
                for inc in _scan_includes(src):
                    resolved = _resolve_include_to_module(inc, path_map)
                    if resolved and resolved != module_id:
                        # Don't flag parent module (e.g. dia.core includes from dia.core.containers)
                        if not resolved.startswith(module_id + ".") and not module_id.startswith(resolved + "."):
                            used_modules.add(resolved)

        # Check for missing
        for used in used_modules:
            if used not in declared_deps:
                missing.append((module_id, used, ""))

        # Check for stale
        for declared in declared_deps:
            if declared not in used_modules:
                stale.append((module_id, declared))

    # 5. Report
    if not missing and not stale:
        click.echo(f"All {len(modules)} modules OK — dependencies match includes.")
        return

    if missing:
        click.echo("MISSING DEPENDENCY:")
        for mod_id, dep_id, _ in missing:
            click.echo(f"  {mod_id} → uses {dep_id} but not in dependent_modules")

    if stale:
        click.echo("\nSTALE DEPENDENCY:")
        for mod_id, dep_id in stale:
            click.echo(f"  {mod_id} → declares {dep_id} but no includes found")

    click.echo(f"\nSummary: {len(missing) + len(stale)} issues ({len(missing)} missing, {len(stale)} stale)")

    # 6. Auto-fix if requested
    if fix and missing:
        click.echo("\nApplying fixes...")
        # Group missing by module
        fixes_by_module: Dict[str, List[str]] = {}
        for mod_id, dep_id, _ in missing:
            fixes_by_module.setdefault(mod_id, []).append(dep_id)

        for mod in modules:
            if mod["module_id"] in fixes_by_module:
                mf = mod["_file"]
                text = mf.read_text(encoding="utf-8")
                for dep_to_add in fixes_by_module[mod["module_id"]]:
                    # Insert after dependent_modules: line or after last dep
                    if "dependent_modules: []" in text:
                        text = text.replace(
                            "dependent_modules: []",
                            f"dependent_modules:\n  - {dep_to_add}"
                        )
                    elif "dependent_modules:" in text:
                        # Find last "  - " line in that block
                        lines = text.splitlines(keepends=True)
                        result = []
                        in_deps = False
                        inserted = False
                        for line in lines:
                            result.append(line)
                            if "dependent_modules:" in line:
                                in_deps = True
                            elif in_deps and line.strip().startswith("- "):
                                pass  # still in deps
                            elif in_deps and not inserted:
                                result.insert(-1, f"  - {dep_to_add}\n")
                                inserted = True
                                in_deps = False
                        text = "".join(result)
                mf.write_text(text, encoding="utf-8")
                click.echo(f"  Fixed {mod['module_id']}")
