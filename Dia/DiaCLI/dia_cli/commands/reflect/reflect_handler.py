"""Business logic for 'dia reflect' — dump-schema invocation, diff, versioning, write."""
from __future__ import annotations

import json
import subprocess
from pathlib import Path
from typing import Any

import click

from dia_cli.commands.pipeline.pipeline_config import load_pipeline_config, PipelineConfigError
from dia_cli.commands.pipeline.path_resolver import resolve_out_dir


# ---------------------------------------------------------------------------
# Public entry point
# ---------------------------------------------------------------------------

def handle_reflect(
    repo_root: Path,
    target_name: str,
    config: str,
    is_breaking: bool,
) -> int:
    """Run the reflect pipeline. Returns 0 on success, non-zero on failure."""

    # 1. Load pipeline config and resolve target
    try:
        pipeline_cfg = load_pipeline_config(repo_root)
    except PipelineConfigError as e:
        click.echo(f"ERROR: {e}", err=True)
        return 2

    if target_name not in pipeline_cfg.targets:
        known = ", ".join(sorted(pipeline_cfg.targets))
        click.echo(f"ERROR: unknown target '{target_name}' (known: {known})", err=True)
        return 2

    target_config = pipeline_cfg.targets[target_name]
    app_name = target_config.app_name or target_name

    # 2. Resolve binary path
    binary_dir = resolve_out_dir(repo_root, config, "x64", app_name)
    binary_path = binary_dir / f"{app_name}.exe"

    if not binary_path.exists():
        click.echo(
            f"ERROR: binary not found: {binary_path}\n"
            f"  Run 'dia run {target_name} --build-only --config {config}' first.",
            err=True,
        )
        return 1

    # 3. Run --dump-schema
    click.echo(f"Running: {binary_path} --dump-schema")
    try:
        result = subprocess.run(
            [str(binary_path), "--dump-schema"],
            capture_output=True,
            text=True,
            cwd=str(binary_dir),
        )
    except OSError as e:
        click.echo(f"ERROR: failed to launch binary: {e}", err=True)
        return 1

    if result.returncode != 0:
        click.echo(
            f"ERROR: binary exited with code {result.returncode}.\n"
            f"  stderr: {result.stderr.strip()}",
            err=True,
        )
        return 1

    if not result.stdout.strip():
        click.echo("ERROR: binary produced no output on stdout.", err=True)
        return 1

    # 4. Parse and validate JSON
    try:
        new_schema: dict[str, Any] = json.loads(result.stdout)
    except json.JSONDecodeError as e:
        click.echo(f"ERROR: binary output is not valid JSON: {e}", err=True)
        return 1

    required_keys = {"components", "modules", "processing_units"}
    missing_keys = required_keys - new_schema.keys()
    if missing_keys:
        click.echo(
            f"ERROR: schema output missing required keys: {', '.join(sorted(missing_keys))}",
            err=True,
        )
        return 1

    # 5. Determine schema output path
    assets_dir = repo_root / "Cluiche" / "Assets" / app_name
    schema_path = assets_dir / "registeredtypes.diaschema"

    # 6. Versioning
    version = _compute_version(schema_path, new_schema, is_breaking)
    if version is None:
        # No change detected
        click.echo("Schema unchanged — not written")
        return 0

    new_schema["version"] = version

    # 7. Write file
    assets_dir.mkdir(parents=True, exist_ok=True)
    schema_path.write_text(
        json.dumps(new_schema, indent=2),
        encoding="utf-8",
    )
    click.echo(f"Schema written: {schema_path} (v{version['major']}.{version['minor']})")
    return 0


# ---------------------------------------------------------------------------
# Versioning helpers
# ---------------------------------------------------------------------------

def _extract_type_ids(schema: dict[str, Any]) -> set[str]:
    """Build a flat set of type_id strings from components + modules lists."""
    ids: set[str] = set()
    for key in ("components", "modules"):
        for entry in schema.get(key, []):
            if isinstance(entry, dict) and "type_id" in entry:
                ids.add(str(entry["type_id"]))
    return ids


def _extract_fields_per_type(schema: dict[str, Any]) -> dict[str, frozenset[str]]:
    """Return a mapping of type_id → frozenset of field names."""
    result: dict[str, frozenset[str]] = {}
    for key in ("components", "modules"):
        for entry in schema.get(key, []):
            if not isinstance(entry, dict):
                continue
            type_id = str(entry.get("type_id", ""))
            if not type_id:
                continue
            fields_raw = entry.get("fields", [])
            if isinstance(fields_raw, list):
                field_names: set[str] = set()
                for f in fields_raw:
                    if isinstance(f, dict):
                        field_names.add(str(f.get("name", "")))
                    elif isinstance(f, str):
                        field_names.add(f)
                result[type_id] = frozenset(field_names)
            else:
                result[type_id] = frozenset()
    return result


def _compute_version(
    schema_path: Path,
    new_schema: dict[str, Any],
    is_breaking: bool,
) -> dict[str, int] | None:
    """
    Compute the new version dict, or return None if there is no change.

    Rules:
      - No previous file          → start at 1.0
      - No change detected        → return None (caller skips write)
      - Any type removed          → warn; if --breaking: major += 1, minor = 0
      - Types/fields added        → minor += 1
      - --breaking without removal→ major += 1, minor = 0
    """
    if not schema_path.exists():
        return {"major": 1, "minor": 0}

    # Load previous schema
    try:
        prev_schema: dict[str, Any] = json.loads(schema_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        # Corrupt previous file — treat as fresh start
        return {"major": 1, "minor": 0}

    prev_version = prev_schema.get("version", {})
    major = int(prev_version.get("major", 1))
    minor = int(prev_version.get("minor", 0))

    # Compare type sets
    prev_ids = _extract_type_ids(prev_schema)
    new_ids = _extract_type_ids(new_schema)

    removed_ids = prev_ids - new_ids
    added_ids = new_ids - prev_ids

    # Compare fields for types present in both
    prev_fields = _extract_fields_per_type(prev_schema)
    new_fields = _extract_fields_per_type(new_schema)

    fields_added = False
    fields_removed = False
    for type_id in prev_ids & new_ids:
        prev_f = prev_fields.get(type_id, frozenset())
        new_f = new_fields.get(type_id, frozenset())
        if new_f - prev_f:
            fields_added = True
        if prev_f - new_f:
            fields_removed = True

    # Determine change category
    has_removals = bool(removed_ids) or fields_removed
    has_additions = bool(added_ids) or fields_added
    no_change = not has_removals and not has_additions and not is_breaking

    if no_change:
        return None  # Signal: skip write

    if has_removals:
        click.echo(
            "WARNING: types or fields removed — breaking change detected "
            f"({', '.join(sorted(removed_ids)) or 'field-level removal'})"
        )

    if is_breaking or has_removals:
        major += 1
        minor = 0
    elif has_additions:
        minor += 1

    return {"major": major, "minor": minor}
