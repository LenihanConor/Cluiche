"""dia check — validate structural consistency of the codebase."""
from __future__ import annotations

import json
import re
import shutil
import subprocess
import sys
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

    ctx.exit(1 if (missing or stale) else 0)


@cli.command("arch")
@click.option("--module", "module_filter", default=None, metavar="MODULE_ID",
              help="Limit scan to a specific module subtree (e.g. dia.core).")
@click.option("--summary", "summary_only", is_flag=True, default=False,
              help="Print totals only, no per-file detail.")
@click.pass_context
def arch(ctx, module_filter: str | None, summary_only: bool) -> None:
    """Check architecture layer rules and forbidden dependencies."""
    from dia_cli.commands.check.arch_checker import run_arch_check, format_violations

    repo_root = find_repo_root(__file__)
    violations, warnings, exit_code = run_arch_check(
        repo_root, module_filter=module_filter, summary_only=summary_only
    )
    output = format_violations(violations, warnings, summary_only=summary_only)
    if output:
        click.echo(output)
    ctx.exit(exit_code)


@cli.command("clones")
@click.option("--tokens", "minimum_tokens", default=100, metavar="N",
              help="Minimum token count to flag as a duplicate (default: 100).")
@click.option("--accept-baseline", "accept_baseline", is_flag=True, default=False,
              help="Save current findings as baseline and exit 0.")
@click.pass_context
def clones(ctx, minimum_tokens: int, accept_baseline: bool) -> None:
    """Detect copy-paste duplication via PMD CPD.

    Exits 1 if new duplicates are found beyond the baseline.
    Run with --accept-baseline to promote current findings to the baseline.
    Requires Java + PMD on PATH (see https://pmd.github.io/).
    """
    from dia_cli.commands.check.clone_checker import run_clone_check, _find_cpd_executable

    repo_root = find_repo_root(__file__)

    if not _find_cpd_executable(repo_root):
        click.echo(
            "ERROR: PMD CPD not found on PATH.\n"
            "Install PMD from https://pmd.github.io/ and ensure 'cpd' or 'pmd' is on PATH.",
            err=True,
        )
        ctx.exit(1)
        return

    click.echo(f"[dia check] Running clone detection (minimum-tokens={minimum_tokens})...")
    try:
        new_dups, exit_code = run_clone_check(
            repo_root,
            minimum_tokens=minimum_tokens,
            accept_baseline=accept_baseline,
        )
    except RuntimeError as e:
        click.echo(f"ERROR: {e}", err=True)
        ctx.exit(1)
        return

    if accept_baseline:
        click.echo("[dia check] Baseline accepted.")
        return

    if not new_dups:
        click.echo("[dia check] No new duplicates found.")
    else:
        click.echo(f"[dia check] {len(new_dups)} new duplicate block(s) found:")
        for dup in new_dups:
            files = ", ".join(f"{f['path']}:{f['line']}" for f in dup["files"])
            click.echo(f"  {dup['lines']} lines / {dup['tokens']} tokens — {files}")

    out_sarif = repo_root / "Cluiche" / "out" / "check" / "clones.sarif"
    click.echo(f"[dia check] Results written to {out_sarif}")
    ctx.exit(exit_code)


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


@cli.command("render-diff")
@click.option("--run-dir", default=None,
              help="Directory containing run captures (default: Cluiche/out/CluicheTest/captures/run/).")
@click.option("--ref-dir", default=None,
              help="Directory containing reference captures (default: Cluiche/Assets/CluicheTest/captures/reference/).")
@click.option("--report-dir", default=None,
              help="Output directory for diff reports.")
@click.option("--threshold", default=4, type=int, show_default=True,
              help="Per-pixel difference threshold (0-255).")
@click.option("--ssim-threshold", "ssim_threshold", default=0.95, type=float, show_default=True,
              help="Minimum SSIM score for a pair to be considered passing.")
@click.pass_context
def render_diff(ctx, run_dir, ref_dir, report_dir, threshold, ssim_threshold) -> None:
    """Run offline render diff analysis on captured frames."""
    import subprocess
    repo_root = find_repo_root(__file__)

    run_path = Path(run_dir) if run_dir else repo_root / "Cluiche" / "out" / "CluicheTest" / "captures" / "run"
    ref_path = Path(ref_dir) if ref_dir else repo_root / "Cluiche" / "Assets" / "CluicheTest" / "captures" / "reference"
    rep_path = Path(report_dir) if report_dir else run_path.parent / "reports"

    script = repo_root / "Tools" / "render_diff.py"
    if not script.exists():
        click.echo(f"[dia check render-diff] ERROR: {script} not found.")
        ctx.exit(1)
        return

    # render_diff.py requires Pillow + scikit-image which need 64-bit Python.
    # Prefer the system python3 over sys.executable (DiaCLI venv may be 32-bit).
    python_exe = _find_64bit_python() or sys.executable

    cmd = [
        python_exe, str(script),
        "--run", str(run_path),
        "--ref", str(ref_path),
        "--report-dir", str(rep_path),
        "--threshold", str(threshold),
        "--ssim-threshold", str(ssim_threshold),
    ]
    result = subprocess.run(cmd)
    ctx.exit(result.returncode)


# ---------------------------------------------------------------------------
# GDD sync helpers
# ---------------------------------------------------------------------------

_VALID_DOMAINS = {
    "movement", "physics", "collision", "animation", "rendering",
    "lighting", "camera", "ai", "pathfinding", "input", "audio",
    "ui", "scene", "persistence", "debug",
}

_VALID_STATUSES = {"built", "partial", "not-started", "out-of-scope"}

# Matches table rows: | id | ... | [Text](link) or — | `status` | notes |
# Captures: spec_link (may be empty "—"), status cell, gap notes cell
_GDD_ROW_RE = re.compile(
    r'^\|[^|]+\|[^|]+\|[^|]+\|'           # #, requirement, capability columns
    r'\s*(?:\[.*?\]\((.*?)\)|—)\s*\|'      # spec link (group 1) or dash
    r'\s*`?([a-z-]+)`?\s*\|'              # status (group 2)
    r'([^|]*)\|',                          # gap notes (group 3)
    re.MULTILINE,
)


def _resolve_spec_path(spec_link: str, gdd_path: Path, repo_root: Path) -> Path | None:
    """Return the resolved Path for a spec link, or None if it doesn't exist."""
    resolved = (gdd_path.parent / spec_link).resolve()
    if resolved.exists():
        return resolved
    resolved_root = (repo_root / spec_link).resolve()
    if resolved_root.exists():
        return resolved_root
    return None


def _read_spec_status(spec_path: Path) -> str:
    """Read the **Status:** field from a spec file, returning 'Unknown' if absent."""
    try:
        text = spec_path.read_text(encoding="utf-8", errors="ignore")
    except OSError:
        return "Unknown"
    m = _SPEC_STATUS_RE.search(text)
    return m.group(1).strip() if m else "Unknown"


# Maps GDD row status -> spec statuses that are consistent with it.
# Any spec status NOT in the allowed set triggers a status-drift warning.
_GDD_TO_SPEC_STATUSES: dict[str, set[str]] = {
    "built":       {"Done"},
    "partial":     {"Done", "In Progress"},
    "not-started": {"Draft", "Approved", "Unknown"},
    # out-of-scope rows carry no spec link, so no check is needed
}


def _check_gdd_file(
    gdd_path: Path,
    repo_root: Path,
    verbose: bool,
) -> list[dict]:
    """Parse one GDD cross-reference file and return a list of issue dicts."""
    issues = []
    try:
        text = gdd_path.read_text(encoding="utf-8", errors="ignore")
    except OSError as exc:
        return [{"file": str(gdd_path), "row": "—", "kind": "io-error", "detail": str(exc)}]

    rel = gdd_path.relative_to(repo_root)

    for match in _GDD_ROW_RE.finditer(text):
        spec_link = (match.group(1) or "").strip()
        status = (match.group(2) or "").strip()
        notes = (match.group(3) or "").strip()

        row_label = text[:match.start()].count("\n") + 1

        # 1. Status must be a valid value
        if status and status not in _VALID_STATUSES:
            issues.append({
                "file": str(rel), "row": row_label,
                "kind": "invalid-status",
                "detail": f"'{status}' is not a valid status value",
            })

        # 2. partial rows must have a non-empty Gap / Notes
        if status == "partial" and not notes:
            issues.append({
                "file": str(rel), "row": row_label,
                "kind": "missing-gap-note",
                "detail": "status is 'partial' but Gap / Notes is empty",
            })

        # 3. Spec link must resolve to a real file
        resolved_spec = None
        if spec_link:
            resolved_spec = _resolve_spec_path(spec_link, gdd_path, repo_root)
            if resolved_spec is None:
                issues.append({
                    "file": str(rel), "row": row_label,
                    "kind": "broken-link",
                    "detail": f"spec link does not exist: {spec_link}",
                })

        # 4. GDD status must be consistent with the spec's actual status
        if resolved_spec and status in _GDD_TO_SPEC_STATUSES:
            spec_status = _read_spec_status(resolved_spec)
            allowed = _GDD_TO_SPEC_STATUSES[status]
            if spec_status not in allowed:
                issues.append({
                    "file": str(rel), "row": row_label,
                    "kind": "status-drift",
                    "detail": (
                        f"GDD says '{status}' but spec is '{spec_status}' "
                        f"(expected one of: {', '.join(sorted(allowed))})"
                    ),
                })

    return issues


def _render_gdd_sync_report(
    all_issues: list[dict],
    files_checked: int,
) -> str:
    lines = [
        "# GDD Sync Status",
        "",
        "_Refresh by running `dia check gdd-sync`._",
        "",
        f"Files checked: {files_checked}",
        f"Issues found: {len(all_issues)}",
        "",
    ]
    if not all_issues:
        lines += ["All GDD cross-reference files are valid. ✅", ""]
        return "\n".join(lines)

    lines += [
        "| File | Line | Kind | Detail |",
        "|------|------|------|--------|",
    ]
    for issue in all_issues:
        lines.append(
            f"| {issue['file']} | {issue['row']} "
            f"| {issue['kind']} | {issue['detail']} |"
        )
    lines.append("")
    return "\n".join(lines)


def _find_64bit_python() -> str | None:
    """Return a Python executable that has PIL and skimage, preferring 64-bit."""
    import subprocess as _sp
    import shutil as _sh

    candidates = []
    for name in ("python3", "python"):
        found = _sh.which(name)
        if found:
            candidates.append(found)

    # Also probe well-known 64-bit install paths on Windows
    import os
    local_prog = os.path.expandvars(r"%LOCALAPPDATA%\Programs\Python")
    if os.path.isdir(local_prog):
        for entry in sorted(os.listdir(local_prog), reverse=True):
            exe = os.path.join(local_prog, entry, "python.exe")
            if os.path.isfile(exe):
                candidates.append(exe)

    for exe in candidates:
        try:
            r = _sp.run(
                [exe, "-c",
                 "import platform, PIL, skimage; print(platform.architecture()[0])"],
                capture_output=True, text=True, timeout=5,
            )
            if r.returncode == 0 and "64bit" in r.stdout:
                return exe
        except Exception:
            continue
    return None


@cli.command("gdd-sync")
@click.option("--verbose", is_flag=True, default=False,
              help="Print each GDD file being processed.")
@click.pass_context
def gdd_sync(ctx, verbose: bool) -> None:
    """Validate GDD cross-reference files in docs/gdd/.

    Checks each *.md file (except README.md) for:
      - Broken spec links (linked file does not exist)
      - Invalid status values (must be built/partial/not-started/out-of-scope)
      - Partial rows missing a Gap / Notes entry

    Writes results to docs/reference/registry/gdd-sync-status.md.
    Exits non-zero if any issues are found.
    """
    repo_root = find_repo_root(__file__)
    gdd_dir = repo_root / "docs" / "gdd"
    out_path = repo_root / "docs" / "reference" / "registry" / "gdd-sync-status.md"

    if not gdd_dir.exists():
        click.echo("ERROR: docs/gdd/ not found.", err=True)
        ctx.exit(1)
        return

    gdd_files = [
        f for f in sorted(gdd_dir.glob("*.md"))
        if f.name.lower() != "readme.md"
    ]

    if not gdd_files:
        click.echo("[dia check] No GDD cross-reference files found in docs/gdd/.")
        ctx.exit(0)
        return

    click.echo(f"[dia check] Scanning {len(gdd_files)} GDD cross-reference file(s)...")

    all_issues = []
    for gdd_path in gdd_files:
        if verbose:
            click.echo(f"  {gdd_path.relative_to(repo_root)}")
        issues = _check_gdd_file(gdd_path, repo_root, verbose)
        all_issues.extend(issues)

    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(
        _render_gdd_sync_report(all_issues, len(gdd_files)),
        encoding="utf-8",
    )

    if all_issues:
        click.echo(
            f"[dia check] {len(all_issues)} issue(s) found. "
            f"See {out_path.relative_to(repo_root)}",
            err=True,
        )
        ctx.exit(1)
    else:
        click.echo(
            f"[dia check] All {len(gdd_files)} file(s) valid. "
            f"Written to {out_path.relative_to(repo_root)}"
        )


# ---------------------------------------------------------------------------
# debugger-contract helpers
# ---------------------------------------------------------------------------

_IMGUI_RE = re.compile(r'\bImGui::')
_IDEBUGDOMAIN_RE = re.compile(r':\s*public\s+(?:Dia::VisualDebugger::)?IDebugDomain')
_GETJSONSTATE_RE = re.compile(r'\bGetJSONState\b')
_ONCOMMAND_RE = re.compile(r'\bOnCommand\b')


def _read_source_files(directory: Path, extensions: Tuple[str, ...]) -> List[Tuple[Path, str]]:
    """Read all files with given extensions from a directory; return (path, text) pairs."""
    results = []
    if not directory.exists():
        return results
    for ext in extensions:
        for f in directory.rglob(f"*{ext}"):
            try:
                results.append((f, f.read_text(encoding="utf-8", errors="ignore")))
            except OSError:
                pass
    return results


def _check_debugger_module(
    name: str,
    mod_dir: Path,
    parent_dir: Path | None,
    is_adaptor: bool,
    tests_root: Path,
    repo_root: Path,
) -> dict:
    """Run all contract checks for one visual debugger module. Returns a result dict."""
    headers = _read_source_files(mod_dir, (".h",))
    sources = _read_source_files(mod_dir, (".cpp",))
    all_files = headers + sources

    # ------------------------------------------------------------------
    # AC 2: Parent system has zero #include referencing this VD module
    # ------------------------------------------------------------------
    if is_adaptor:
        ac2 = "N/A"
        ac2_ok = True
    else:
        ac2_ok = True
        ac2_detail = ""
        if parent_dir and parent_dir.exists():
            vd_name_pattern = re.compile(
                r'#include\s+[<"][^>"]*' + re.escape(name) + r'[^>"]*[>"]'
            )
            for ext in (".h", ".cpp"):
                if not ac2_ok:
                    break
                for src in parent_dir.rglob(f"*{ext}"):
                    try:
                        text = src.read_text(encoding="utf-8", errors="ignore")
                    except OSError:
                        continue
                    if vd_name_pattern.search(text):
                        ac2_ok = False
                        try:
                            ac2_detail = str(src.relative_to(repo_root))
                        except ValueError:
                            ac2_detail = str(src)
                        break
        if ac2_ok:
            ac2 = "PASS"
        else:
            ac2 = f"FAIL (include in {ac2_detail})"

    # ------------------------------------------------------------------
    # AC 4: Inherits IDebugDomain
    # ------------------------------------------------------------------
    ac4_ok = any(_IDEBUGDOMAIN_RE.search(text) for _, text in headers)
    ac4 = "PASS" if ac4_ok else "PENDING (not yet migrated)"

    # ------------------------------------------------------------------
    # AC 8: No ImGui calls
    # ------------------------------------------------------------------
    ac8_ok = True
    ac8_detail = ""
    for path, text in all_files:
        if _IMGUI_RE.search(text):
            ac8_ok = False
            ac8_detail = path.name
            break
    ac8 = "PASS" if ac8_ok else f"FAIL ({ac8_detail})"

    # ------------------------------------------------------------------
    # AC 10: GetJSONState declared in a header
    # ------------------------------------------------------------------
    ac10_ok = any(_GETJSONSTATE_RE.search(text) for _, text in headers)
    ac10 = "PASS" if ac10_ok else "FAIL"

    # ------------------------------------------------------------------
    # AC 11-12: OnCommand declared in a header
    # ------------------------------------------------------------------
    ac1112_ok = any(_ONCOMMAND_RE.search(text) for _, text in headers)
    ac1112 = "PASS" if ac1112_ok else "FAIL"

    # ------------------------------------------------------------------
    # AC 13: Test directory exists with at least one Test*.cpp
    # ------------------------------------------------------------------
    tests_dir = tests_root / name
    ac13_ok = False
    if tests_dir.exists():
        test_files = list(tests_dir.glob("Test*.cpp"))
        if test_files:
            ac13_ok = True
    ac13 = "PASS" if ac13_ok else "FAIL"

    # ------------------------------------------------------------------
    # Overall result
    # ------------------------------------------------------------------
    real_failures = []
    if "FAIL" in ac2:
        real_failures.append("AC2")
    if "FAIL" in ac8:
        real_failures.append("AC8")
    if "FAIL" in ac10:
        real_failures.append("AC10")
    if "FAIL" in ac1112:
        real_failures.append("AC11-12")
    if "FAIL" in ac13:
        real_failures.append("AC13")

    pending_items = []
    if "PENDING" in ac4:
        pending_items.append("AC4")

    if real_failures:
        overall = f"FAIL ({', '.join(real_failures)})"
    elif pending_items:
        overall = f"PENDING ({', '.join(pending_items)})"
    else:
        overall = "PASS"

    return {
        "name": name,
        "ac2": ac2,
        "ac4": ac4,
        "ac8": ac8,
        "ac10": ac10,
        "ac1112": ac1112,
        "ac13": ac13,
        "overall": overall,
    }


@cli.command("debugger-contract")
@click.option("--verbose", is_flag=True, default=False,
              help="Show details for each failing AC check.")
@click.pass_context
def debugger_contract(ctx, verbose: bool) -> None:
    """Validate all DiaXxxVisualDebugger modules against the formal contract.

    Checks ACs 1-3 (module isolation), 4 (IDebugDomain inheritance), 8 (ImGui-free),
    10 (GetJSONState declared), 11-12 (OnCommand declared), 13-14 (test file existence).

    AC 4 failures are reported as PENDING (expected until domain migration — Task 6).
    All other failures are reported as FAIL.

    Exits non-zero if any module has a failing or pending check.
    """
    repo_root = find_repo_root(__file__)
    dia_dir = repo_root / "Dia"
    tests_root = repo_root / "Cluiche" / "Tests" / "GoogleTests"

    # ------------------------------------------------------------------
    # Module discovery
    # ------------------------------------------------------------------
    # 1. Standalone modules: Dia/DiaXxxVisualDebugger/ directories
    #    Exclude base modules (DiaVisualDebugger, DiaVisualDebuggerConsole)
    _BASE_MODULES = {"DiaVisualDebugger", "DiaVisualDebuggerConsole"}
    standalone_dirs = sorted(
        d for d in dia_dir.iterdir()
        if d.is_dir()
        and d.name.endswith("VisualDebugger")
        and d.name not in _BASE_MODULES
    )

    modules_to_check = []
    for d in standalone_dirs:
        module_name = d.name
        # Derive parent by removing "VisualDebugger" suffix
        parent_name = module_name[:-len("VisualDebugger")]  # e.g. "DiaRigidBody2D"
        parent_dir = dia_dir / parent_name if parent_name else None
        modules_to_check.append({
            "name": module_name,
            "dir": d,
            "parent_dir": parent_dir,
            "is_adaptor": False,
        })

    # 2. Adaptor-based modules (live inside parent module's Adaptors/ directory)
    adaptor_modules = [
        {
            "name": "DiaEntitySpatialVisualDebugger",
            "dir": dia_dir / "DiaEntitySpatial" / "Adaptors",
            "parent_dir": None,  # AC2 is N/A for adaptors — isolation is by construction
            "is_adaptor": True,
        },
        {
            "name": "DiaScalarFieldVisualDebugger",
            "dir": dia_dir / "DiaScalarField" / "Adaptors",
            "parent_dir": None,
            "is_adaptor": True,
        },
    ]
    modules_to_check.extend(adaptor_modules)

    click.echo(
        f"[dia check] Validating {len(modules_to_check)} DiaXxxVisualDebugger module(s)...\n"
    )

    # ------------------------------------------------------------------
    # Run checks
    # ------------------------------------------------------------------
    results = []
    for mod in modules_to_check:
        r = _check_debugger_module(
            name=mod["name"],
            mod_dir=mod["dir"],
            parent_dir=mod["parent_dir"],
            is_adaptor=mod["is_adaptor"],
            tests_root=tests_root,
            repo_root=repo_root,
        )
        results.append(r)
        if verbose and r["overall"] != "PASS":
            click.echo(f"  {r['name']}:")
            for ac_key, ac_label in [
                ("ac2", "AC2"), ("ac4", "AC4"), ("ac8", "AC8"),
                ("ac10", "AC10"), ("ac1112", "AC11-12"), ("ac13", "AC13"),
            ]:
                val = r[ac_key]
                if val not in ("PASS", "N/A"):
                    click.echo(f"    {ac_label}: {val}")

    # ------------------------------------------------------------------
    # Print table
    # ------------------------------------------------------------------
    name_w = max(len(r["name"]) for r in results) + 2
    col_w = 8  # narrow columns for PASS/FAIL/PEND/N/A

    header = (
        f"{'Module':<{name_w}}"
        f"{'AC2':<{col_w}}"
        f"{'AC4':<{col_w}}"
        f"{'AC8':<{col_w}}"
        f"{'AC10':<{col_w}}"
        f"{'AC11-12':<{col_w}}"
        f"{'AC13':<{col_w}}"
        f"Result"
    )
    separator = "-" * (name_w + col_w * 6 + 30)
    click.echo(header)
    click.echo(separator)

    def _fmt(val: str) -> str:
        if val == "N/A":
            return "N/A"
        if val == "PASS":
            return "PASS"
        if val.startswith("PENDING"):
            return "PEND"
        return "FAIL"

    for r in results:
        line = (
            f"{r['name']:<{name_w}}"
            f"{_fmt(r['ac2']):<{col_w}}"
            f"{_fmt(r['ac4']):<{col_w}}"
            f"{_fmt(r['ac8']):<{col_w}}"
            f"{_fmt(r['ac10']):<{col_w}}"
            f"{_fmt(r['ac1112']):<{col_w}}"
            f"{_fmt(r['ac13']):<{col_w}}"
            f"{r['overall']}"
        )
        click.echo(line)

    # ------------------------------------------------------------------
    # Summary
    # ------------------------------------------------------------------
    pass_count = sum(1 for r in results if r["overall"] == "PASS")
    pending_count = sum(1 for r in results if r["overall"].startswith("PENDING"))
    fail_count = sum(1 for r in results if r["overall"].startswith("FAIL"))

    click.echo(
        f"\nSummary: {len(results)} module(s) — "
        f"{pass_count} PASS, {pending_count} PENDING (AC4 migration), {fail_count} FAIL"
    )

    if fail_count > 0:
        click.echo(
            "\nFAIL: Real contract violations found. Fix before Task 6 domain migration.",
            err=True,
        )
    elif pending_count > 0:
        click.echo(
            "\nPENDING: AC4 (IDebugDomain inheritance) not yet satisfied — expected until "
            "domain migration (Task 6) is complete."
        )

    # Exit non-zero for any failure or pending check
    if fail_count > 0 or pending_count > 0:
        ctx.exit(1)
