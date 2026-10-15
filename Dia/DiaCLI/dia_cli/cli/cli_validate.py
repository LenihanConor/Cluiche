"""dia validate — validate manifest files against their schemas."""
from __future__ import annotations

import json
from pathlib import Path
from typing import List, Tuple

import click

from dia_cli.utils.repo_root import find_repo_root

# Type alias for validators that return (errors, warnings)
ValidationResult = Tuple[List[str], List[str]]


# ---------------------------------------------------------------------------
# Schema validators
# ---------------------------------------------------------------------------

def _check_key(data: dict, key: str, expected_type: type, path: str = "") -> List[str]:
    """Check a key exists and has the right type. Returns list of error strings."""
    full_key = f"{path}.{key}" if path else key
    if key not in data:
        return [f"missing required key: {full_key}"]
    if not isinstance(data[key], expected_type):
        return [f"{full_key} should be {expected_type.__name__}, got {type(data[key]).__name__}"]
    return []


def _validate_diastage(data: dict) -> List[str]:
    """Validate a .diastage file."""
    errors = []
    errors += _check_key(data, "name", str)
    errors += _check_key(data, "manifest", str)
    errors += _check_key(data, "config", dict)
    if isinstance(data.get("config"), dict):
        errors += _check_key(data["config"], "path_aliases", dict, "config")
    return errors


def _validate_diagame(data: dict, file_path: Path | None = None) -> Tuple[List[str], List[str]]:
    """Validate a .diagame file. Returns (errors, warnings)."""
    errors = []
    warnings = []

    errors += _check_key(data, "name", str)
    errors += _check_key(data, "version", (str, int, float))
    errors += _check_key(data, "config", dict)
    errors += _check_key(data, "imports", list)

    if isinstance(data.get("config"), dict):
        cfg = data["config"]
        errors += _check_key(cfg, "name", str, "config")
        errors += _check_key(cfg, "window", dict, "config")
        if isinstance(cfg.get("window"), dict):
            errors += _check_key(cfg["window"], "title", str, "config.window")

    if isinstance(data.get("imports"), list):
        for i, imp in enumerate(data["imports"]):
            prefix = f"imports[{i}]"
            if not isinstance(imp, dict):
                errors.append(f"{prefix} should be object")
                continue
            errors += _check_key(imp, "path", str, prefix)
            errors += _check_key(imp, "type", str, prefix)
            if isinstance(imp.get("type"), str) and imp["type"] not in ("manifest", "stage"):
                errors.append(f"{prefix}.type must be 'manifest' or 'stage', got '{imp['type']}'")

    if file_path and isinstance(data.get("imports"), list):
        composed, compose_warnings = _compose_game_manifest(data, file_path)
        warnings += compose_warnings
        errors += _validate_service_streams(composed)

    # Check schema file if present
    if "schema" in data:
        schema_value = data["schema"]
        if isinstance(schema_value, str) and file_path:
            schema_path = file_path.parent / schema_value
            if not schema_path.exists():
                warnings.append(f"'schema' key references missing file: {schema_value}")

    return errors, warnings


def _validate_diaapp(data: dict) -> List[str]:
    """Validate a .diaapp file. Full validation for v3; minimal for older versions."""
    errors = []
    errors += _check_key(data, "version", int)

    version = data.get("version", 0)
    if isinstance(version, int) and version < 3:
        # Older format — only check it has processing_units
        errors += _check_key(data, "processing_units", list)
        return errors

    errors += _check_key(data, "processing_units", list)

    if isinstance(data.get("processing_units"), list):
        for i, pu in enumerate(data["processing_units"]):
            prefix = f"processing_units[{i}]"
            if not isinstance(pu, dict):
                errors.append(f"{prefix} should be object")
                continue
            errors += _check_key(pu, "instance_id", str, prefix)
            errors += _check_key(pu, "frequency_hz", (int, float), prefix)
            errors += _check_key(pu, "dedicated_thread", bool, prefix)
            errors += _check_key(pu, "modules", list, prefix)

            if isinstance(pu.get("modules"), list):
                for j, mod in enumerate(pu["modules"]):
                    mprefix = f"{prefix}.modules[{j}]"
                    if not isinstance(mod, dict):
                        errors.append(f"{mprefix} should be object")
                        continue
                    errors += _check_key(mod, "instance_id", str, mprefix)
                    errors += _check_key(mod, "type_id", str, mprefix)
                    errors += _check_key(mod, "stages", list, mprefix)
                    errors += _check_key(mod, "dependencies", list, mprefix)
                    errors += _check_key(mod, "channels", list, mprefix)

    # Optional: stages array
    if "stages" in data:
        if not isinstance(data["stages"], list):
            errors.append("stages should be array")
        else:
            for i, stage in enumerate(data["stages"]):
                prefix = f"stages[{i}]"
                if not isinstance(stage, dict):
                    errors.append(f"{prefix} should be object")
                    continue
                errors += _check_key(stage, "name", str, prefix)
                errors += _check_key(stage, "transitions", list, prefix)

    # Optional: streams array
    if "streams" in data:
        if not isinstance(data["streams"], list):
            errors.append("streams should be array")
        else:
            for i, stream in enumerate(data["streams"]):
                prefix = f"streams[{i}]"
                if not isinstance(stream, dict):
                    errors.append(f"{prefix} should be object")
                    continue
                errors += _check_key(stream, "id", str, prefix)
                errors += _check_key(stream, "kind", str, prefix)
                errors += _check_key(stream, "payload_type", str, prefix)

    return errors


def _load_json(path: Path) -> dict | None:
    try:
        loaded = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return None
    return loaded if isinstance(loaded, dict) else None


def _resolve_import(base_dir: Path, rel: str) -> Path:
    """Resolve an import path. Source layout differs from the deployed layout the paths are written
    for, so fall back to a unique file-name match under base_dir."""
    direct = base_dir / rel
    if direct.exists():
        return direct
    matches = list(base_dir.rglob(Path(rel).name))
    return matches[0] if len(matches) == 1 else direct


def _compose_game_manifest(data: dict, file_path: Path) -> Tuple[dict, List[str]]:
    """Merge the streams and PU modules of every manifest a .diagame imports (directly or via a
    .diastage) into one v3 dict, as the C++ loader composes them. Returns (merged, warnings)."""
    merged: dict = {"streams": [], "processing_units": []}
    warnings: List[str] = []
    imports = data.get("imports")
    if not isinstance(imports, list):
        return merged, warnings

    for imp in imports:
        if not isinstance(imp, dict) or not isinstance(imp.get("path"), str):
            continue
        imp_path = _resolve_import(file_path.parent, imp["path"])
        imp_data = _load_json(imp_path)
        if imp_data is None:
            warnings.append(f"cannot compose service streams: import not readable: {imp['path']}")
            continue
        if imp.get("type") == "stage":
            manifest_rel = imp_data.get("manifest")
            if not isinstance(manifest_rel, str):
                continue
            imp_data = _load_json(_resolve_import(file_path.parent, manifest_rel))
            if imp_data is None:
                warnings.append(f"cannot compose service streams: stage manifest not readable: {manifest_rel}")
                continue

        if isinstance(imp_data.get("streams"), list):
            merged["streams"].extend(imp_data["streams"])
        if isinstance(imp_data.get("processing_units"), list):
            for pu in imp_data["processing_units"]:
                if not isinstance(pu, dict):
                    continue
                existing = next((m for m in merged["processing_units"] if m.get("instance_id") == pu.get("instance_id")), None)
                if existing is None:
                    merged["processing_units"].append({**pu, "modules": list(pu.get("modules") or [])})
                elif isinstance(pu.get("modules"), list):
                    existing["modules"].extend(pu["modules"])
    return merged, warnings


def _validate_service_streams(data: dict) -> List[str]:
    """Mirror ManifestValidatorV2::CheckServiceStreamConstraints for a v3 .diaapp."""
    errors: List[str] = []
    streams = data.get("streams")
    pus = data.get("processing_units")
    if not isinstance(streams, list) or not isinstance(pus, list):
        return errors

    for stream in streams:
        if not isinstance(stream, dict) or stream.get("kind") != "ServiceStream":
            continue
        stream_id = stream.get("id")

        providers: List[int] = []   # PU index per provider channel
        consumers: List[int] = []   # PU index per consumer channel
        for p, pu in enumerate(pus):
            if not isinstance(pu, dict) or not isinstance(pu.get("modules"), list):
                continue
            for mod in pu["modules"]:
                if not isinstance(mod, dict) or not isinstance(mod.get("channels"), list):
                    continue
                for ch in mod["channels"]:
                    if not isinstance(ch, dict) or ch.get("id") != stream_id:
                        continue
                    if ch.get("role") == "provides":
                        providers.append(p)
                    elif ch.get("role") == "consumes":
                        consumers.append(p)

        if providers:
            for c in consumers:
                if c < providers[-1]:
                    errors.append(
                        f"ServiceStream '{stream_id}': consumer PU '{_pu_id(pus, c)}' (index {c}) appears before "
                        f"provider PU '{_pu_id(pus, providers[-1])}' (index {providers[-1]}) in config "
                        f"[SERVICE_STREAM_PROVIDER_AFTER_CONSUMER]")
        if not providers:
            errors.append(
                f"ServiceStream '{stream_id}' has no provider (no module has role=provides) "
                f"[SERVICE_STREAM_MISSING_PROVIDER]")
        elif len(providers) > 1:
            errors.append(
                f"ServiceStream '{stream_id}' has {len(providers)} providers - exactly one provider required "
                f"[SERVICE_STREAM_MULTIPLE_PROVIDERS]")
        elif not consumers:
            errors.append(
                f"ServiceStream '{stream_id}' has a provider but no consumers [SERVICE_STREAM_ORPHAN_PROVIDER]")

    return errors


def _pu_id(pus: list, index: int) -> str:
    pu = pus[index]
    return str(pu.get("instance_id", index)) if isinstance(pu, dict) else str(index)


def _validate_diagamemessages(data: dict) -> List[str]:
    """Validate a .diagamemessages file."""
    errors = []
    errors += _check_key(data, "schema", str)
    errors += _check_key(data, "namespace", str)
    errors += _check_key(data, "messages", list)

    if "includes" in data and not isinstance(data["includes"], list):
        errors.append(f"includes should be list, got {type(data['includes']).__name__}")

    if isinstance(data.get("messages"), list):
        for i, msg in enumerate(data["messages"]):
            prefix = f"messages[{i}]"
            if not isinstance(msg, dict):
                errors.append(f"{prefix} should be object")
                continue
            errors += _check_key(msg, "id", str, prefix)
            errors += _check_key(msg, "router", str, prefix)
            errors += _check_key(msg, "pass", str, prefix)
            errors += _check_key(msg, "producers", list, prefix)
            errors += _check_key(msg, "consumers", list, prefix)

            if isinstance(msg.get("router"), str) and msg["router"] not in ("broadcast", "entity"):
                errors.append(f"{prefix}.router must be 'broadcast' or 'entity', got '{msg['router']}'")

            if isinstance(msg.get("pass"), str) and msg["pass"] not in ("primary", "reaction"):
                errors.append(f"{prefix}.pass must be 'primary' or 'reaction', got '{msg['pass']}'")

            if "capacity" in msg and not isinstance(msg["capacity"], int):
                errors.append(f"{prefix}.capacity should be int, got {type(msg['capacity']).__name__}")

            if "overflow" in msg:
                overflow = msg["overflow"]
                if isinstance(overflow, str) and overflow not in ("assert", "drop_oldest"):
                    errors.append(f"{prefix}.overflow must be 'assert' or 'drop_oldest', got '{overflow}'")
                elif not isinstance(overflow, str):
                    errors.append(f"{prefix}.overflow should be str, got {type(overflow).__name__}")

            if "fields" in msg:
                if not isinstance(msg["fields"], list):
                    errors.append(f"{prefix}.fields should be list, got {type(msg['fields']).__name__}")
                else:
                    for j, field in enumerate(msg["fields"]):
                        fprefix = f"{prefix}.fields[{j}]"
                        if not isinstance(field, dict):
                            errors.append(f"{fprefix} should be object")
                            continue
                        errors += _check_key(field, "name", str, fprefix)
                        errors += _check_key(field, "type", str, fprefix)

    return errors


# ---------------------------------------------------------------------------
# Click command
# ---------------------------------------------------------------------------

@click.group("validate")
def cli():
    """Validate manifest and configuration files."""


@cli.command("manifest")
@click.pass_context
@click.option("--path", "target_path", default=None, type=click.Path(), help="Validate a specific file.")
@click.option("--verbose", is_flag=True, default=False, help="Show OK files in output.")
def manifest(ctx: click.Context, target_path: str | None, verbose: bool) -> None:
    """Validate .diaapp/.diagame/.diastage files against their schemas."""
    repo_root = find_repo_root(__file__)

    # Find files to validate
    files: List[Path] = []
    if target_path:
        p = Path(target_path)
        if not p.is_absolute():
            p = repo_root / p
        files = [p]
    else:
        for ext in ("*.diaapp", "*.diagame", "*.diastage", "*.diagamemessages"):
            files.extend(repo_root.rglob(ext))

    # Exclude worktree and build artifacts
    _EXCLUDE_DIRS = {".claude", "node_modules", "build", "out", "bin"}
    files = [f for f in files if not any(part in _EXCLUDE_DIRS for part in f.relative_to(repo_root).parts)]

    if not files:
        click.echo("No manifest files found.")
        return

    click.echo(f"Validating {len(files)} manifest(s)...\n")

    error_count = 0
    ok_count = 0

    for f in sorted(files):
        rel_path = str(f.relative_to(repo_root)).replace("\\", "/")

        try:
            data = json.loads(f.read_text(encoding="utf-8"))
        except json.JSONDecodeError as e:
            click.echo(f"ERR  {rel_path}")
            click.echo(f"     - invalid JSON: {e}")
            error_count += 1
            continue

        # Determine validator by extension
        suffix = f.suffix.lower()
        warnings = []

        if suffix == ".diastage":
            errors = _validate_diastage(data)
        elif suffix == ".diagame":
            errors, warnings = _validate_diagame(data, f)
        elif suffix == ".diaapp":
            errors = _validate_diaapp(data)
        elif suffix == ".diagamemessages":
            errors = _validate_diagamemessages(data)
        else:
            continue

        if errors:
            click.echo(f"ERR  {rel_path}")
            for err in errors:
                click.echo(f"     - {err}")
            error_count += 1
        else:
            ok_count += 1
            if verbose:
                click.echo(f"OK   {rel_path}")

        # Print warnings regardless of error status
        for warning in warnings:
            click.echo(f"WARN {rel_path}")
            click.echo(f"     - {warning}")

    click.echo(f"\nSummary: {ok_count + error_count} checked, {error_count} error(s)")
    if error_count > 0:
        ctx.exit(1)
        return
