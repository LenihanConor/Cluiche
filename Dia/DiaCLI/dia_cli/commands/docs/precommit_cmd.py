"""dia docs precommit — run all pre-commit checks as a deterministic script."""
from __future__ import annotations

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
