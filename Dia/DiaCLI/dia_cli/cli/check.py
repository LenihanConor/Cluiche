"""dia check — validate structural consistency of the codebase."""
from __future__ import annotations

import json
import re
import shutil
import subprocess
import xml.etree.ElementTree as ET
from pathlib import Path
from typing import Dict, List, Set, Tuple

import click

from dia_cli.utils.repo_root import find_repo_root


_CONFIG_ALIASES = {"Asan": "Debug-Asan", "Ubsan": "Debug-Ubsan"}

_SANITIZER_OUTPUT_NAMES = {
    "Debug-Asan": "sanitizer-asan.txt",
    "Debug-Ubsan": "sanitizer-ubsan.txt",
}


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
# Path-to-module mapping helpers (used by deps subcommand)
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
    segments = parts.split("/")
    for i in range(len(segments), 0, -1):
        prefix = "/".join(segments[:i])
        if prefix in path_map:
            return path_map[prefix]
    return None


# ---------------------------------------------------------------------------
# cppcheck helpers
# ---------------------------------------------------------------------------

def _accept_baseline(ctx) -> None:
    repo_root = find_repo_root(__file__)
    out_dir = repo_root / "Cluiche" / "out" / "check"
    findings_path = out_dir / "findings.sarif"
    baseline_path = out_dir / "baseline.sarif"

    if not findings_path.exists():
        click.echo(
            "ERROR: findings.sarif not found. Run 'dia check cppcheck' first to generate it.",
            err=True,
        )
        ctx.exit(1); return

    try:
        data = json.loads(findings_path.read_text(encoding="utf-8"))
        runs = data.get("runs", [])
        finding_count = len(runs[0].get("results", [])) if runs else 0
    except (json.JSONDecodeError, IndexError):
        finding_count = 0

    shutil.copy2(str(findings_path), str(baseline_path))
    click.echo(
        f"[dia check] Accepted {finding_count} finding(s) as baseline → {baseline_path}"
    )


def _run_cppcheck(ctx) -> None:
    if not shutil.which("cppcheck"):
        click.echo(
            "ERROR: cppcheck not found on PATH. "
            "Install with: winget install Cppcheck.Cppcheck",
            err=True,
        )
        ctx.exit(1); return

    repo_root = find_repo_root(__file__)

    out_dir = repo_root / "Cluiche" / "out" / "check"
    out_dir.mkdir(parents=True, exist_ok=True)
    xml_path = out_dir / "findings.xml"

    cmd = [
        "cppcheck",
        "--enable=warning,performance,portability",
        f"--output-file={xml_path}",
        "--xml", "--xml-version=2",
        "--suppress=*:External/*",
        "--suppress=*:Cluiche/bin/*",
        "--suppress=*:Cluiche/out/*",
        "--max-ctu-depth=4",
        str(repo_root / "Dia"),
        str(repo_root / "Cluiche"),
    ]

    suppressions_file = repo_root / ".cppcheck-suppressions.xml"
    if suppressions_file.exists():
        cmd.insert(1, f"--suppress-xml={suppressions_file}")

    click.echo("[dia check] Running cppcheck (this may take a while)...")
    try:
        result = subprocess.run(
            cmd,
            timeout=300,
        )
    except subprocess.TimeoutExpired:
        click.echo("ERROR: cppcheck timed out after 300 seconds", err=True)
        ctx.exit(1); return

    xml_text = xml_path.read_text(encoding="utf-8", errors="replace") if xml_path.exists() else ""

    sarif = _cppcheck_xml_to_sarif(xml_text)

    sarif_path = out_dir / "findings.sarif"
    sarif_path.write_text(json.dumps(sarif, indent=2), encoding="utf-8")

    finding_count = len(sarif["runs"][0]["results"]) if sarif.get("runs") else 0
    click.echo(f"[dia check] {finding_count} finding(s) written to {sarif_path}")


def _cppcheck_xml_to_sarif(xml_text: str) -> dict:
    _SEVERITY_MAP = {
        "error": "error",
        "warning": "warning",
        "performance": "note",
        "portability": "note",
        "style": "note",
    }

    _EMPTY_SARIF = {
        "version": "2.1.0",
        "$schema": "https://raw.githubusercontent.com/oasis-tcs/sarif-spec/master/Schemata/sarif-schema-2.1.0.json",
        "runs": [{
            "tool": {
                "driver": {
                    "name": "Cppcheck",
                    "version": "unknown",
                    "rules": [],
                }
            },
            "results": [],
        }],
    }

    if not xml_text or not xml_text.strip():
        return _EMPTY_SARIF

    try:
        root = ET.fromstring(xml_text)
    except ET.ParseError:
        return _EMPTY_SARIF

    cppcheck_version = "unknown"
    cppcheck_el = root.find("cppcheck")
    if cppcheck_el is not None:
        cppcheck_version = cppcheck_el.get("version", "unknown")

    errors_el = root.find("errors")
    if errors_el is None:
        return _EMPTY_SARIF

    rules_by_id: dict = {}
    results = []

    for error in errors_el.findall("error"):
        rule_id = error.get("id", "unknown")
        severity = error.get("severity", "warning")
        msg = error.get("msg", "")

        level = _SEVERITY_MAP.get(severity, "warning")

        if rule_id not in rules_by_id:
            rules_by_id[rule_id] = {
                "id": rule_id,
                "name": rule_id,
                "shortDescription": {"text": error.get("verbose", msg) or msg},
                "defaultConfiguration": {"level": level},
            }

        location_el = error.find("location")
        if location_el is not None:
            raw_file = location_el.get("file", "")
            line_str = location_el.get("line", "1")

            try:
                repo_root = find_repo_root(__file__)
                uri = Path(raw_file).resolve().relative_to(repo_root.resolve())
                uri_str = uri.as_posix()
            except (ValueError, TypeError):
                uri_str = raw_file.replace("\\", "/")

            try:
                start_line = int(line_str)
            except (ValueError, TypeError):
                start_line = 1

            locations = [{
                "physicalLocation": {
                    "artifactLocation": {
                        "uri": uri_str,
                        "uriBaseId": "%SRCROOT%",
                    },
                    "region": {"startLine": start_line},
                }
            }]
        else:
            locations = []

        results.append({
            "ruleId": rule_id,
            "level": level,
            "message": {"text": msg},
            "locations": locations,
        })

    return {
        "version": "2.1.0",
        "$schema": "https://raw.githubusercontent.com/oasis-tcs/sarif-spec/master/Schemata/sarif-schema-2.1.0.json",
        "runs": [{
            "tool": {
                "driver": {
                    "name": "Cppcheck",
                    "version": cppcheck_version,
                    "rules": list(rules_by_id.values()),
                }
            },
            "results": results,
        }],
    }


def _run_sanitizer(config: str, ctx) -> None:
    normalized = _CONFIG_ALIASES.get(config, config)
    if normalized not in _SANITIZER_OUTPUT_NAMES:
        click.echo(
            f"ERROR: unknown sanitizer config '{config}'. Use Asan or Ubsan.",
            err=True,
        )
        ctx.exit(2); return

    repo_root = find_repo_root(__file__)

    click.echo(f"[dia check] Building googletest with config {normalized} ...")
    build_result = subprocess.run(
        ["dia", "run", "googletest", f"--config={normalized}", "--build-only"],
        cwd=str(repo_root),
    )
    if build_result.returncode != 0:
        click.echo(
            f"[dia check] Build failed (exit {build_result.returncode})", err=True
        )
        ctx.exit(build_result.returncode); return

    exe_path = (
        repo_root
        / "Cluiche"
        / "bin"
        / "GoogleTests"
        / normalized
        / "x64"
        / "GoogleTests.exe"
    )
    if not exe_path.exists():
        click.echo(f"ERROR: {exe_path} not found after build.", err=True)
        ctx.exit(1); return

    click.echo(f"[dia check] Running {exe_path.name} under {normalized} ...")
    run_result = subprocess.run(
        [str(exe_path)],
        cwd=str(exe_path.parent),
        capture_output=True,
        text=True,
    )

    out_dir = repo_root / "Cluiche" / "out" / "check"
    out_dir.mkdir(parents=True, exist_ok=True)
    out_file = out_dir / _SANITIZER_OUTPUT_NAMES[normalized]
    out_file.write_text(run_result.stderr, encoding="utf-8")

    findings = [
        line for line in run_result.stderr.splitlines()
        if "ERROR:" in line or "runtime error:" in line
    ]
    count = len(findings)

    click.echo(f"[dia check] Output written to {out_file}")
    if count == 0:
        click.echo("[dia check] No sanitizer findings detected.")
    else:
        click.echo(f"[dia check] {count} sanitizer finding(s) detected.")

    if run_result.returncode != 0:
        ctx.exit(run_result.returncode); return


# ---------------------------------------------------------------------------
# Click group + subcommands
# ---------------------------------------------------------------------------

@click.group()
def cli():
    """Validate structural consistency of the codebase."""
    pass


@cli.command("cppcheck")
@click.option("--accept-baseline", "accept_baseline", is_flag=True, default=False,
              help="Promote current findings.sarif to baseline.sarif.")
@click.pass_context
def cppcheck(ctx, accept_baseline: bool) -> None:
    """Run cppcheck static analysis and write findings to SARIF."""
    if accept_baseline:
        _accept_baseline(ctx)
        return
    _run_cppcheck(ctx)


@cli.command("sanitizer")
@click.option("--config", default="Asan", metavar="CONFIG",
              type=click.Choice(["Asan", "Ubsan"], case_sensitive=True),
              help="Sanitizer config: Asan or Ubsan (default: Asan).")
@click.pass_context
def sanitizer(ctx, config: str) -> None:
    """Build and run googletest under ASan or UBSan sanitizer."""
    _run_sanitizer(config, ctx)


@cli.command("deps")
@click.option("--fix", is_flag=True, default=False,
              help="Auto-add missing deps to module docs.")
@click.option("--verbose", is_flag=True, default=False,
              help="Show each module being checked.")
@click.pass_context
def deps(ctx, fix: bool, verbose: bool) -> None:
    """Validate module dependency declarations against actual #includes."""
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

        used_modules: Set[str] = set()
        for ext in ("*.h", "*.cpp"):
            for src in mod_path.rglob(ext):
                for inc in _scan_includes(src):
                    resolved = _resolve_include_to_module(inc, path_map)
                    if resolved and resolved != module_id:
                        if not resolved.startswith(module_id + ".") and not module_id.startswith(resolved + "."):
                            used_modules.add(resolved)

        for used in used_modules:
            if used not in declared_deps:
                missing.append((module_id, used, ""))

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
        fixes_by_module: Dict[str, List[str]] = {}
        for mod_id, dep_id, _ in missing:
            fixes_by_module.setdefault(mod_id, []).append(dep_id)

        for mod in modules:
            if mod["module_id"] in fixes_by_module:
                mf = mod["_file"]
                text = mf.read_text(encoding="utf-8")
                for dep_to_add in fixes_by_module[mod["module_id"]]:
                    if "dependent_modules: []" in text:
                        text = text.replace(
                            "dependent_modules: []",
                            f"dependent_modules:\n  - {dep_to_add}"
                        )
                    elif "dependent_modules:" in text:
                        lines = text.splitlines(keepends=True)
                        result = []
                        in_deps = False
                        inserted = False
                        for line in lines:
                            result.append(line)
                            if "dependent_modules:" in line:
                                in_deps = True
                            elif in_deps and line.strip().startswith("- "):
                                pass
                            elif in_deps and not inserted:
                                result.insert(-1, f"  - {dep_to_add}\n")
                                inserted = True
                                in_deps = False
                        text = "".join(result)
                mf.write_text(text, encoding="utf-8")
                click.echo(f"  Fixed {mod['module_id']}")


@cli.command("sln-sync")
@click.option("--dry-run", is_flag=True, default=False,
              help="Print planned folder assignments without modifying the .sln.")
@click.pass_context
def sln_sync(ctx, dry_run: bool) -> None:
    """Sync Cluiche.sln solution folders to module layer assignments."""
    from dia_cli.commands.check.sln_sync import run_sln_sync

    repo_root = find_repo_root(__file__)
    changes, warnings, exit_code = run_sln_sync(repo_root=repo_root, dry_run=dry_run)
    for c in changes:
        click.echo(c)
    for w in warnings:
        click.echo(w)
    ctx.exit(exit_code)


# ---------------------------------------------------------------------------
# spec-sync helpers
# ---------------------------------------------------------------------------

_PUBLIC_INTERFACES_RE = re.compile(r'^## Public Interfaces?\s*$', re.MULTILINE)
_CPP_BLOCK_RE = re.compile(r'```cpp\s*.*?```', re.DOTALL)
_CLASS_STRUCT_RE = re.compile(r'\b(?:class|struct)\s+([A-Z][A-Za-z0-9_]+)')
_SPEC_STATUS_RE = re.compile(r'\*\*Status:\*\*\s*`?(\w[\w ]*?)`?\s*$', re.MULTILINE)


def _extract_spec_symbols(spec_text: str) -> List[str]:
    """Return PascalCase class/struct names from the Public Interfaces section."""
    m = _PUBLIC_INTERFACES_RE.search(spec_text)
    if not m:
        return []

    start = m.end()
    next_sec = re.search(r'^## ', spec_text[start:], re.MULTILINE)
    section = spec_text[start: start + next_sec.start()] if next_sec else spec_text[start:]

    symbols: List[str] = []
    for block in _CPP_BLOCK_RE.finditer(section):
        for sym_match in _CLASS_STRUCT_RE.finditer(block.group()):
            name = sym_match.group(1)
            if name not in symbols:
                symbols.append(name)
    return symbols


def _build_header_index(dia_dir: Path) -> Set[str]:
    """Scan all .h files under dia_dir and return every PascalCase word found."""
    word_re = re.compile(r'\b([A-Z][A-Za-z0-9_]+)\b')
    found: Set[str] = set()
    for header in dia_dir.rglob("*.h"):
        try:
            text = header.read_text(encoding="utf-8", errors="ignore")
            found.update(word_re.findall(text))
        except OSError:
            pass
    return found


def _collect_specs(specs_dir: Path) -> List[Path]:
    """Return all system-level spec files (canonical <name>/<name>.md, no dots in stem)."""
    result: List[Path] = []
    for md in specs_dir.rglob("*.md"):
        if "." in md.stem:
            continue
        if md.parent.name == md.stem:
            result.append(md)
    return sorted(result)


def _spec_status(spec_text: str) -> str:
    m = _SPEC_STATUS_RE.search(spec_text)
    return m.group(1).strip() if m else "Unknown"


def _render_sync_table(rows: list) -> str:
    lines = [
        "# Spec Sync Status",
        "",
        "_Refresh by running `dia check spec-sync`. Commit the result to keep it visible in the site._",
        "",
        "Symbols are extracted from each spec's **Public Interfaces** section (class/struct names only).",
        "A symbol is ✅ if it appears anywhere under `Dia/` headers; ⚠️ if absent.",
        "",
        "| Spec | Spec Status | Sync | Symbols Found | Missing |",
        "|------|-------------|------|---------------|---------|",
    ]
    for row in rows:
        lines.append(
            f"| {row['spec']} | {row['spec_status']} | {row['sync']} "
            f"| {row['found']} | {row['missing']} |"
        )
    lines.append("")
    return "\n".join(lines)


@cli.command("spec-sync")
@click.option("--verbose", is_flag=True, default=False,
              help="Print each spec being processed.")
@click.pass_context
def spec_sync(ctx, verbose: bool) -> None:
    """Check if spec Public Interfaces symbols exist in Dia headers.

    Reads every system spec under docs/specs/applications/, extracts class/struct
    names from the Public Interfaces section, then checks whether each name appears
    in any header under Dia/. Writes the result to
    docs/reference/registry/spec-sync-status.md.

    This command is mechanical (no AI). A symbol either exists in a header or it doesn't.
    """
    repo_root = find_repo_root(__file__)
    specs_dir = repo_root / "docs" / "specs" / "applications"
    dia_dir = repo_root / "Dia"
    out_path = repo_root / "docs" / "reference" / "registry" / "spec-sync-status.md"

    if not specs_dir.exists():
        click.echo("ERROR: docs/specs/applications/ not found.", err=True)
        ctx.exit(1)
        return

    click.echo("[dia check] Indexing Dia/ headers...")
    header_index = _build_header_index(dia_dir)
    click.echo(f"[dia check] {len(header_index)} distinct PascalCase symbols indexed.")

    specs = _collect_specs(specs_dir)
    click.echo(f"[dia check] Scanning {len(specs)} system specs...")

    rows = []
    specs_with_symbols = 0
    total_missing = 0

    for spec_path in specs:
        if verbose:
            click.echo(f"  {spec_path.relative_to(repo_root)}")

        try:
            text = spec_path.read_text(encoding="utf-8", errors="ignore")
        except OSError:
            continue

        status = _spec_status(text)
        symbols = _extract_spec_symbols(text)

        if not symbols:
            rows.append({
                "spec": spec_path.stem,
                "spec_status": status,
                "sync": "—",
                "found": "—",
                "missing": "no Public Interfaces",
            })
            continue

        specs_with_symbols += 1
        found = [s for s in symbols if s in header_index]
        missing = [s for s in symbols if s not in header_index]
        total_missing += len(missing)

        if not missing:
            sync_icon = "✅"
        elif len(missing) == len(symbols):
            sync_icon = "❌"
        else:
            sync_icon = "⚠️"

        rows.append({
            "spec": spec_path.stem,
            "spec_status": status,
            "sync": sync_icon,
            "found": ", ".join(found) if found else "—",
            "missing": ", ".join(missing) if missing else "—",
        })

    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(_render_sync_table(rows), encoding="utf-8")

    click.echo(
        f"[dia check] Done. {specs_with_symbols} specs had symbols; "
        f"{total_missing} missing symbol(s). Written to {out_path}"
    )
