"""dia docs precommit — run all pre-commit checks as a deterministic script."""
from __future__ import annotations

import json
import re
import subprocess
from pathlib import Path
from typing import List, Tuple

import click

from dia_cli.utils.repo_root import find_repo_root


_FORBIDDEN_PATTERNS = [
    (r'#include\s*<iostream>', "Use DIA_LOG_* instead of iostream", True),
    (r'using\s+namespace\s+std\s*;', "Never use 'using namespace std'", True),
    (r'\bGetStatic\s*\(', "Singleton without approval (GetStatic)", False),
    (r'\bsInstance\b', "Singleton without approval (sInstance)", False),
]


def _get_staged_files(repo_root: Path) -> List[Path]:
    result = subprocess.run(
        ["git", "diff", "--cached", "--name-only", "--diff-filter=ACMR"],
        cwd=repo_root, capture_output=True, text=True
    )
    if result.returncode != 0:
        result = subprocess.run(
            ["git", "diff", "--name-only", "--diff-filter=ACMR", "HEAD"],
            cwd=repo_root, capture_output=True, text=True
        )
    files = [repo_root / f.strip() for f in result.stdout.strip().splitlines() if f.strip()]
    return files


def _get_modified_files(repo_root: Path) -> List[Path]:
    result = subprocess.run(
        ["git", "status", "--porcelain"],
        cwd=repo_root, capture_output=True, text=True
    )
    files = []
    for line in result.stdout.strip().splitlines():
        if len(line) > 3:
            status = line[:2]
            filepath = line[3:].strip()
            if status.strip() and not status.startswith("?"):
                files.append(repo_root / filepath)
    return files


def _check_forbidden_patterns(files: List[Path]) -> List[Tuple[str, str, bool]]:
    issues: List[Tuple[str, str, bool]] = []
    cpp_files = [f for f in files if f.suffix in (".h", ".hpp", ".cpp", ".inl")]

    for f in cpp_files:
        if not f.exists():
            continue
        try:
            text = f.read_text(encoding="utf-8", errors="ignore")
        except OSError:
            continue

        for pattern, message, is_error in _FORBIDDEN_PATTERNS:
            for m in re.finditer(pattern, text):
                line_num = text[:m.start()].count("\n") + 1
                issues.append((f"{f.name}:{line_num}", message, is_error))

    return issues


def _check_vcxproj_sync(repo_root: Path, files: List[Path]) -> List[str]:
    missing: List[str] = []
    new_cpp_files = [
        f for f in files
        if f.suffix in (".h", ".hpp", ".cpp")
        and ("Dia" in str(f) or "Cluiche" in str(f))
        and f.exists()
    ]

    for f in new_cpp_files:
        proj_dir = f.parent
        while proj_dir != repo_root:
            vcxproj_candidates = list(proj_dir.glob("*.vcxproj"))
            if vcxproj_candidates:
                vcxproj = vcxproj_candidates[0]
                try:
                    vcx_text = vcxproj.read_text(encoding="utf-8")
                except OSError:
                    break
                rel = str(f.relative_to(proj_dir)).replace("/", "\\")
                if rel not in vcx_text and f.name not in vcx_text:
                    missing.append(f"{f.name} not in {vcxproj.name}")
                break
            proj_dir = proj_dir.parent

    return missing


def _check_manifests(repo_root: Path, files: List[Path]) -> List[str]:
    errors: List[str] = []
    manifest_files = [
        f for f in files
        if f.suffix in (".diaapp", ".diagame", ".diastage") and f.exists()
    ]

    for mf in manifest_files:
        try:
            import json
            text = mf.read_text(encoding="utf-8")
            json.loads(text)
        except (json.JSONDecodeError, OSError) as e:
            errors.append(f"{mf.name}: {e}")

    return errors


def _conformance_dir(repo_root: Path) -> Path:
    from dia_cli.commands.validate import schema_discovery
    return (repo_root / Path(*schema_discovery.CONFORMANCE_REL)).resolve()


def _under(path: Path, root: Path) -> bool:
    return path == root or root in path.parents


def _in_schemad_folder(f: Path, assets: Path) -> bool:
    """True when a *.schema.json sits in f's folder or any folder above it (up to the asset root)."""
    d = f.resolve().parent
    while _under(d, assets):
        try:
            if any(c.is_file() and c.name.lower().endswith(".schema.json") for c in d.iterdir()):
                return True
        except OSError:
            return False
        if d == assets:
            break
        d = d.parent
    return False


def _schema_candidates(repo_root: Path, files: List[Path]) -> List[Path]:
    """Changed *.schema.json files, plus changed asset *.json that dia validate schema would check.

    A changed asset *.json is a candidate when it carries a top-level string `format`, or when it
    sits in a schema'd folder (so a missing `format` / unparsable JSON is caught). The intentionally
    bad shared fixtures under Dia/DiaValueSchema/Testing are never candidates.
    """
    assets = (repo_root / "Cluiche" / "Assets").resolve()
    fixtures = _conformance_dir(repo_root)
    out: List[Path] = []
    for f in files:
        name = f.name.lower()
        if not name.endswith(".json") or not f.is_file() or name.endswith(".catalogue.json"):
            continue
        rf = f.resolve()
        if _under(rf, fixtures):
            continue
        if name.endswith(".schema.json"):
            out.append(f)
            continue
        if not _under(rf, assets):
            continue
        try:
            doc = json.loads(f.read_text(encoding="utf-8-sig"))
        except (OSError, ValueError, RecursionError):
            doc = None
        if (isinstance(doc, dict) and isinstance(doc.get("format"), str)) or _in_schemad_folder(f, assets):
            out.append(f)
    return out


def _check_schema(repo_root: Path, files: List[Path]) -> Tuple[List[str], List[str]]:
    """Run `dia validate schema` over changed schema/data files; returns (errors, warnings).

    The whole Cluiche/Assets tree is scanned so cross-file refs resolve, but only findings
    whose file is one of the changed files are reported. Nothing is scanned when no
    relevant file changed. A changed schema outside the asset tree is validated on its own
    (one run per file, so same-named files cannot collide).
    """
    candidates = _schema_candidates(repo_root, files)
    if not candidates:
        return [], []

    from dia_cli.commands.validate import schema_discovery

    assets = (repo_root / "Cluiche" / "Assets").resolve()
    names = set()
    outside: List[Path] = []
    for f in candidates:
        rf = f.resolve()
        if assets in rf.parents:
            names.add(rf.relative_to(assets).as_posix())
        else:
            outside.append(f)

    runs: List[Tuple[List[Path], set]] = []
    if names:
        runs.append(([assets], names))
    for f in outside:
        runs.append(([f], {f.name}))

    errors: List[str] = []
    warnings: List[str] = []
    for paths, wanted in runs:
        findings, _checked = schema_discovery.discover_and_validate(paths, None)
        for fd in findings:
            if fd.file not in wanted:
                continue
            line = f"{fd.severity} {fd.file}: {fd.message}"
            (errors if fd.severity == "FAIL" else warnings).append(line)
    return errors, warnings


@click.command("precommit")
@click.option("--scope", default=None, help="Limit to files under this path.")
@click.option("--staged", is_flag=True, default=False, help="Check only git-staged files.")
def precommit(scope: str, staged: bool) -> None:
    """Run all pre-commit validation checks (zero AI tokens).

    Checks: forbidden patterns, vcxproj sync, manifest validity.
    Equivalent to /pre-commit skill but runs instantly as a script.

    Examples:
        dia docs precommit
        dia docs precommit --staged
        dia docs precommit --scope Dia/DiaCore
    """
    repo_root = find_repo_root(__file__)

    if staged:
        files = _get_staged_files(repo_root)
    else:
        files = _get_modified_files(repo_root)

    if scope:
        scope_path = (repo_root / scope).resolve()
        files = [f for f in files if str(f.resolve()).startswith(str(scope_path))]

    if not files:
        click.echo("[dia precommit] No files in scope.")
        return

    click.echo(f"[dia precommit] Checking {len(files)} file(s)...\n")

    errors = 0
    warnings = 0

    # Check 1: Forbidden patterns
    forbidden = _check_forbidden_patterns(files)
    hard_fails = [f for f in forbidden if f[2]]
    soft_warns = [f for f in forbidden if not f[2]]

    if hard_fails:
        click.echo("  FAIL  Forbidden patterns:")
        for loc, msg, _ in hard_fails:
            click.echo(f"        {loc}: {msg}")
        errors += len(hard_fails)
    elif soft_warns:
        click.echo("  WARN  Forbidden patterns:")
        for loc, msg, _ in soft_warns:
            click.echo(f"        {loc}: {msg}")
        warnings += len(soft_warns)
    else:
        click.echo("  OK    Forbidden patterns")

    # Check 2: vcxproj sync
    vcx_missing = _check_vcxproj_sync(repo_root, files)
    if vcx_missing:
        click.echo("  FAIL  vcxproj sync:")
        for m in vcx_missing:
            click.echo(f"        {m}")
        errors += len(vcx_missing)
    else:
        click.echo("  OK    vcxproj sync")

    # Check 3: Manifest validity
    manifest_errors = _check_manifests(repo_root, files)
    if manifest_errors:
        click.echo("  FAIL  Manifest validity:")
        for e in manifest_errors:
            click.echo(f"        {e}")
        errors += len(manifest_errors)
    else:
        click.echo("  OK    Manifest validity")

    # Check 4: schema (dia validate schema on changed schema/data files)
    schema_errors, schema_warnings = _check_schema(repo_root, files)
    if schema_errors or schema_warnings:
        click.echo(f"  {'FAIL' if schema_errors else 'WARN'}  schema:")
        for line in schema_errors + schema_warnings:
            click.echo(f"        {line}")
        errors += len(schema_errors)
        warnings += len(schema_warnings)
    elif _schema_candidates(repo_root, files):
        click.echo("  OK    schema")

    # Summary
    click.echo("")
    if errors:
        click.echo(f"Result: {errors} ERROR(s), {warnings} WARNING(s) — fix before committing.")
        raise SystemExit(1)
    elif warnings:
        click.echo(f"Result: PASS ({warnings} warning(s))")
    else:
        click.echo("Result: PASS")

    click.echo("")
    click.echo("CI also runs: dia check deps   dia check arch   dia validate manifest")
