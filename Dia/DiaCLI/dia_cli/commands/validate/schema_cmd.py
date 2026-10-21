"""dia validate schema: click command, scope resolution and output."""
from __future__ import annotations

import json
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import List, Optional, Sequence, Tuple

import click


@dataclass
class Finding:
    severity: str  # 'FAIL' | 'WARN'
    rule: str
    file: str
    key: Optional[str]
    message: str
    suggestion: Optional[str] = None
    path: Optional[str] = None
    record: Optional[str] = None
    owner: Optional[str] = None  # set on grouped code-side coverage findings


# (file, key_count) per checked file.
CheckedFiles = List[Tuple[str, int]]


def run_validation(paths: Sequence[Path], target: Optional[str],
                   code_side: Optional[Path]) -> Tuple[List[Finding], CheckedFiles]:
    """Discover and validate schema/data files (see schema_discovery)."""
    from dia_cli.commands.validate.schema_discovery import discover_and_validate
    return discover_and_validate(paths, target)


def _to_json(f: Finding) -> dict:
    d = asdict(f)
    if d.get("owner") is None:
        del d["owner"]  # only code-side coverage findings carry an owner
    return d


def render_text(findings: List[Finding], files: CheckedFiles, verbose: bool) -> None:
    failed = {f.file for f in findings if f.severity == "FAIL"}
    if verbose:
        for name, n_keys in files:
            if name not in failed:
                click.echo(f"OK   {name} ({n_keys} keys)")
    from dia_cli.commands.validate.schema_codeside import GROUP_NOUNS
    groups: dict = {}
    for f in findings:
        if f.severity == "FAIL":
            click.echo(f"FAIL {f.file}")
            click.echo(f"     {f.message}")
        elif f.owner is not None and f.rule in GROUP_NOUNS:
            gk = (f.rule, f.owner)
            if gk not in groups:
                groups[gk] = []
            groups[gk].append(f)
        else:
            click.echo(f"WARN {f.message}")
    for (rule, owner), items in groups.items():
        click.echo(f"WARN {owner}: {len(items)} {GROUP_NOUNS[rule]}")
        if verbose:
            for it in items:
                click.echo(f"       {it.key}")
    n_fail = sum(1 for f in findings if f.severity == "FAIL")
    n_warn = sum(1 for f in findings if f.severity == "WARN")
    click.echo(f"{len(files)} files checked, {n_fail} FAIL, {n_warn} WARN")


def schema_command(ctx: click.Context, paths: Tuple[str, ...], target: Optional[str],
                   as_json: bool, verbose: bool, code_side: Optional[str]) -> None:
    resolved = [Path(p) for p in paths]
    for p in resolved:
        if not p.exists():
            raise click.UsageError(f"path does not exist: {p}")
    code_path = Path(code_side) if code_side else None
    if code_path is not None and not code_path.exists():
        raise click.UsageError(f"--code-side path does not exist: {code_path}")

    findings, files = run_validation(resolved, target, code_path)

    # Code-side coverage runs with an explicit --code-side, or when no PATHS scope the run.
    notice: Optional[str] = None
    if code_path is not None or not resolved:
        from dia_cli.commands.validate.schema_codeside import run_code_side
        extra, notice = run_code_side(target, code_path)
        findings = list(findings) + extra

    if notice:
        click.echo(notice, err=as_json)
    if as_json:
        click.echo(json.dumps([_to_json(f) for f in findings], indent=2))
    else:
        render_text(findings, files, verbose)
    if any(f.severity == "FAIL" for f in findings):
        ctx.exit(1)
