"""dia validate schema: code-side coverage from the valueschema.json export (Feature 2 task 7).

The export (`dia.value_schema_export` v1, Feature 6) is read, never produced, here.
"""
from __future__ import annotations

import json
import subprocess
from pathlib import Path
from typing import Any, Dict, List, Optional

import click

from dia_cli.commands.validate.schema_cmd import Finding

EXPORT_FORMAT = "dia.value_schema_export"
EXPORT_FILE = "valueschema.json"
NOTICE = "code-side checks skipped: no schema export found (run dia schema export)"
UNIT_SUFFIXES = ("_ms", "_us", "_bytes", "_kb", "_mb", "_hz", "_pct")

RULE_METRIC = "codeside.undescribed-metric"
RULE_COMMAND = "codeside.undescribed-command"
RULE_FIELD = "codeside.undescribed-field"
RULE_UNIT = "codeside.unit-missing"
RULE_STALE = "codeside.stale-export"

# rule -> noun phrase for the grouped text line "<owner>: <n> <noun>"
GROUP_NOUNS = {
    RULE_METRIC: "metrics without descriptions",
    RULE_COMMAND: "commands without argument descriptions",
    RULE_FIELD: "component fields with a range but no description",
}


def git_head() -> Optional[str]:
    """HEAD of the Cluiche repository (not of whatever directory the CLI was launched from)."""
    try:
        from dia_cli.commands.validate import schema_discovery as sd
        out = subprocess.run(["git", "rev-parse", "HEAD"], capture_output=True, text=True, timeout=10,
                             cwd=sd.get_repo_root())
    except (OSError, subprocess.SubprocessError, ValueError):
        return None
    text = out.stdout.strip()
    return text if out.returncode == 0 and text else None


def git_dirty() -> Optional[bool]:
    """True when the Cluiche repository has uncommitted changes; None when git is unavailable."""
    try:
        from dia_cli.commands.validate import schema_discovery as sd
        out = subprocess.run(["git", "status", "--porcelain"], capture_output=True, text=True, timeout=30,
                             cwd=sd.get_repo_root())
    except (OSError, subprocess.SubprocessError, ValueError):
        return None
    return bool(out.stdout.strip()) if out.returncode == 0 else None


# ---------------------------------------------------------------------------
# Locating and loading
# ---------------------------------------------------------------------------

def find_export(target: Optional[str], repo_root: Optional[Path] = None) -> Optional[Path]:
    """Cluiche/out/<App>/valueschema.json for the target's app, or None.

    Without a target: the only export under Cluiche/out, or None when there are none or several.
    """
    from dia_cli.commands.validate import schema_discovery as sd
    root = repo_root if repo_root is not None else sd.get_repo_root()
    out_dir = root / "Cluiche" / "out"
    if target is None:
        found = sorted(out_dir.glob(f"*/{EXPORT_FILE}")) if out_dir.is_dir() else []
        return found[0] if len(found) == 1 else None
    app_name = sd._pipeline_app_name(target, root) or target
    app_dir = next((d for d in sd._app_dirs(root) if d.name.lower() == app_name.lower()), None)
    name = app_dir.name if app_dir is not None else app_name
    cand = out_dir / name / EXPORT_FILE
    return cand if cand.is_file() else None


def load_export(path: Path) -> Dict[str, Any]:
    """Parse an export file; anything unusable is a usage-level error (exit 2)."""
    try:
        doc = json.loads(path.read_text(encoding="utf-8-sig"))
    except (OSError, ValueError, RecursionError) as ex:
        raise click.UsageError(f"cannot read code-side export {path}: {ex}")
    if not isinstance(doc, dict) or doc.get("format") != EXPORT_FORMAT:
        raise click.UsageError(f"cannot read code-side export {path}: expected format `{EXPORT_FORMAT}`")
    return doc


# ---------------------------------------------------------------------------
# Rules
# ---------------------------------------------------------------------------

def _first_segment(name: str, sep: str) -> str:
    return (name.split(sep, 1)[0] if sep in name else name) or "(unknown)"


def _as_list(value: object) -> list:
    """A JSON array, or [] for anything else (never iterate a string's characters or a dict's keys)."""
    return value if isinstance(value, list) else []


def _same_commit(a: str, b: str) -> bool:
    a, b = a.lower(), b.lower()
    return a.startswith(b) or b.startswith(a)


def _stale(header: dict, head_commit: Optional[str]) -> Optional[Finding]:
    commit = header.get("git_commit")
    reasons = []
    if isinstance(commit, str) and commit and head_commit and not _same_commit(commit, head_commit):
        reasons.append(f"export is from `{commit}`, HEAD is `{head_commit[:8]}`")
    if header.get("dirty") is True:
        reasons.append("export was generated from a dirty working tree")
    if not reasons:
        return None
    return Finding("WARN", RULE_STALE, EXPORT_FILE, None, "; ".join(reasons))


def check_code_side(export: dict, *, head_commit: Optional[str]) -> List[Finding]:
    findings: List[Finding] = []
    header = export.get("header")
    stale = _stale(header if isinstance(header, dict) else {}, head_commit)
    if stale:
        findings.append(stale)

    und = export.get("undescribed")
    und = und if isinstance(und, dict) else {}

    for item in _as_list(und.get("metrics")):
        if not _usable(item):
            continue
        name, owner = _item(item, "name", ".")
        findings.append(Finding("WARN", RULE_METRIC, EXPORT_FILE, name,
                                f"metric `{name}` has no MetricInfo description", owner=owner))
    for item in _as_list(und.get("commands")):
        if not _usable(item) or (isinstance(item, dict) and item.get("noArgs") is True):
            continue
        name, owner = _item(item, "name", ".")
        findings.append(Finding("WARN", RULE_COMMAND, EXPORT_FILE, name,
                                f"command `{name}` has no argument descriptions (ValueParam)", owner=owner))
    for item in _as_list(und.get("component_fields")):
        if not _usable(item):
            continue
        name, owner = _item(item, "name", "/")
        findings.append(Finding("WARN", RULE_FIELD, EXPORT_FILE, name,
                                f"component field `{name}` has a range but no description", owner=owner))

    for e in _as_list(export.get("entries")):
        if not isinstance(e, dict) or e.get("kind") != "metric":
            continue
        key = e.get("key")
        if isinstance(key, str) and key.endswith(UNIT_SUFFIXES) and not e.get("unit"):
            findings.append(Finding("WARN", RULE_UNIT, EXPORT_FILE, key,
                                    f"metric `{key}` looks like a measured quantity but has no unit"))
    return findings


def _usable(item: object) -> bool:
    """Undescribed items are plain strings or objects; null, numbers and nested arrays are skipped."""
    return isinstance(item, (str, dict))


def _item(item: object, name_key: str, sep: str):
    """(name, owner) of an undescribed item: a plain string or {name, owner?, ...}."""
    if isinstance(item, dict):
        raw = item.get(name_key)
        name = raw if isinstance(raw, str) else ""
        owner = item.get("owner")
        return name, owner if isinstance(owner, str) and owner else _first_segment(name, sep)
    name = str(item)
    return name, _first_segment(name, sep)


# ---------------------------------------------------------------------------
# Command wiring
# ---------------------------------------------------------------------------

def run_code_side(target: Optional[str], code_path: Optional[Path]):
    """-> (findings, notice). notice is a one-line info string, or None."""
    path = code_path if code_path is not None else find_export(target)
    if path is None:
        return [], NOTICE
    export = load_export(path)
    header = export.get("header")
    if isinstance(header, dict) and header.get("code_side") is False:
        return [], NOTICE  # a --sidecars-only export carries no code-side data
    return check_code_side(export, head_commit=git_head()), None
