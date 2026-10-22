"""dia schema export: produce the dia.value_schema_export v1 file (DiaValueSchema Feature 6).

Default mode launches the target through the `dia launch` helper with `--describe-schema <path>` and
completes the header (git_commit, dirty) that only the CLI can know. `--sidecars-only` builds the
export in Python from sidecars and ramps with no launch.
"""
from __future__ import annotations

import json
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Dict, List, Optional

import click

from dia_cli.cli.launch import TIMEOUT_EXIT_CODE, launch_target, supports_automation
from dia_cli.commands.validate import schema_discovery as sd
from dia_cli.commands.validate.schema_codeside import EXPORT_FILE, EXPORT_FORMAT, git_dirty, git_head

SIDECAR_FORMAT = "dia.value_schema"
RAMPS_FORMAT = "dia.ramps"
BASE_RAMPS_REL = ("Dia", "DiaValueSchema", "Data", "ramps.json")
SIDECAR_FIELDS = ("for", "key", "kind", "label", "description", "hint", "unit", "type", "min", "max",
                  "default", "affects", "related", "source", "values", "ui")
NOTICE_SIDECARS_ONLY = "code-side coverage omitted: --sidecars-only"


def get_repo_root() -> Path:
    return sd.get_repo_root()


def _app_dir_name(target: str, repo_root: Path) -> str:
    app_name = sd._pipeline_app_name(target, repo_root) or target
    app_dir = next((d for d in sd._app_dirs(repo_root) if d.name.lower() == app_name.lower()), None)
    return app_dir.name if app_dir is not None else app_name


def _default_out(target: str, stage: Optional[str], repo_root: Path) -> Path:
    name = f"valueschema.{stage}.json" if stage else EXPORT_FILE
    return repo_root / "Cluiche" / "out" / _app_dir_name(target, repo_root) / name


def _read_json(path: Path) -> Any:
    try:
        return json.loads(path.read_text(encoding="utf-8-sig"))
    except (OSError, ValueError, RecursionError):
        return None


def _git_header() -> Dict[str, Any]:
    commit = git_head()
    dirty = git_dirty()
    return {"git_commit": commit[:8] if commit else "", "dirty": bool(dirty)}


def _now() -> str:
    return datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


def _write(path: Path, doc: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(doc, indent=2) + "\n", encoding="utf-8")


# ---------------------------------------------------------------------------
# Default mode: launch, then complete the header
# ---------------------------------------------------------------------------

def _load_written(path: Path) -> dict:
    if not path.is_file():
        raise click.ClickException(f"the target did not produce an export: {path} was not written "
                                   f"(does the target support --describe-schema?)")
    try:
        doc = json.loads(path.read_text(encoding="utf-8-sig"))
    except (OSError, ValueError, RecursionError) as ex:
        raise click.ClickException(f"the export {path} is not valid JSON: {ex}")
    if not isinstance(doc, dict) or doc.get("format") != EXPORT_FORMAT:
        raise click.ClickException(f"the export {path} is invalid: expected format `{EXPORT_FORMAT}`")
    if not isinstance(doc.get("entries"), list):
        raise click.ClickException(f"the export {path} is invalid: `entries` must be an array")
    return doc


DEFAULT_TIMEOUT_SEC = 180.0


def export_via_launch(target: str, config: str, stage: Optional[str], out: Path,
                      timeout: float = DEFAULT_TIMEOUT_SEC) -> dict:
    args = ["--describe-schema", str(out)]
    if stage:
        args += ["--stage", stage]
    out.parent.mkdir(parents=True, exist_ok=True)
    # `--automation` is passed where the target's Main parses it (launch.AUTOMATION_TARGETS) and no --stage is requested: it
    # makes the test-stage launcher pick and then auto-release stages, which fights describe mode's own TransitionTo(--stage).
    # launch_target enforces the timeout.
    code = launch_target(target, config, automation=supports_automation(target) and not stage, extra_args=args, timeout=timeout)
    if code == TIMEOUT_EXIT_CODE:
        raise click.ClickException(f"describe mode did not finish within {timeout:g}s: "
                                   f"is ValueSchemaModule hosted by {target}?")
    if code != 0:
        raise click.ClickException(f"launching `{target}` for the schema export failed (exit {code})")
    doc = _load_written(out)
    header = doc.get("header")
    header = dict(header) if isinstance(header, dict) else {}
    header.update(_git_header())
    doc["header"] = header
    _write(out, doc)
    return doc


# ---------------------------------------------------------------------------
# --sidecars-only
# ---------------------------------------------------------------------------

def _sidecar_entries(roots: List[Path]) -> List[dict]:
    entries: List[dict] = []
    seen = set()
    for root in roots:
        for p in sd._walk(root, None):
            if not p.name.lower().endswith(".schema.json") or p.resolve() in seen:
                continue
            seen.add(p.resolve())
            doc = _read_json(p)
            if (not isinstance(doc, dict) or doc.get("format") != SIDECAR_FORMAT
                    or not isinstance(doc.get("entries"), list)):
                continue
            fmt = doc.get("for")
            origin = "sidecar:" + p.relative_to(root).as_posix()
            for e in doc["entries"]:
                if not isinstance(e, dict) or not isinstance(e.get("key"), str):
                    continue
                out: Dict[str, Any] = {k: e[k] for k in SIDECAR_FIELDS if k in e}
                out["for"] = fmt
                out["key"] = f"{fmt}/{e['key']}"
                out["origin"] = origin
                entries.append(out)
    return entries


def _ramps(roots: List[Path], repo_root: Path) -> List[dict]:
    files = [repo_root.joinpath(*BASE_RAMPS_REL)]
    for root in roots:
        files += [p for p in sd._walk(root, None) if p.name.lower().endswith(".ramps.json")]
    ramps: List[dict] = []
    seen_ids = set()
    for f in files:
        doc = _read_json(f) if f.is_file() else None
        if not isinstance(doc, dict) or doc.get("format") != RAMPS_FORMAT:
            continue
        for r in doc.get("ramps") or []:
            if isinstance(r, dict) and isinstance(r.get("id"), str) and r["id"] not in seen_ids:
                seen_ids.add(r["id"])
                ramps.append(r)
    return ramps


def export_sidecars_only(target: str, config: str, repo_root: Path) -> dict:
    roots = sd.target_roots(target, repo_root)
    header = {"app": target.lower(), "config": config, "generated_at": _now(), "code_side": False}
    header.update(_git_header())
    return {"format": EXPORT_FORMAT, "version": 1, "header": header,
            "entries": _sidecar_entries(roots), "ramps": _ramps(roots, repo_root)}


# ---------------------------------------------------------------------------
# Command
# ---------------------------------------------------------------------------

def _undescribed_count(doc: dict) -> int:
    und = doc.get("undescribed")
    return sum(len(v) for v in und.values() if isinstance(v, list)) if isinstance(und, dict) else 0


def export_command(target: str, stage: Optional[str], out: Optional[str], sidecars_only: bool,
                   config: str, timeout: float = DEFAULT_TIMEOUT_SEC) -> None:
    repo_root = get_repo_root()
    out_path = Path(out).absolute() if out else _default_out(target, stage, repo_root)
    if sidecars_only:
        if stage:
            raise click.UsageError("--stage needs the running target; it cannot be combined with --sidecars-only")
        doc = export_sidecars_only(target, config, repo_root)
        _write(out_path, doc)
        click.echo(f"exported {len(doc['entries'])} entries, {len(doc['ramps'])} ramps -> {out_path}")
        click.echo(NOTICE_SIDECARS_ONLY)
        return
    doc = export_via_launch(target, config, stage, out_path, timeout)
    click.echo(f"exported {len(doc['entries'])} entries, {_undescribed_count(doc)} undescribed -> {out_path}")
