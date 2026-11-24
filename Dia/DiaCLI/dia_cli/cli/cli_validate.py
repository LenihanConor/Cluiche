"""dia validate — validate manifest files against their schemas."""
from __future__ import annotations

import json
import re
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


_RESERVED_SOURCE_MSG = (
    "config.path_aliases: 'source' is a reserved dev-only alias "
    "(set by `dia launch --dev-alias`); remove it"
)


def _check_reserved_aliases(cfg: dict) -> List[str]:
    aliases = cfg.get("path_aliases")
    if isinstance(aliases, dict) and "source" in aliases:
        return [_RESERVED_SOURCE_MSG]
    return []


_STAGE_KINDS_REL = "Dia/DiaGame/stage_kinds.json"
_MAX_STAGE_TAGS = 16
_MAX_TAG_LEN = 63
_MAX_DESCRIPTION_LEN = 120
_KEBAB_RE = re.compile(r"^[a-z0-9]+(-[a-z0-9]+)*$")


def _stage_kinds() -> List[str]:
    """Load the stage kind vocabulary from Dia/DiaGame/stage_kinds.json."""
    path = find_repo_root(__file__) / _STAGE_KINDS_REL
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as e:
        raise ValueError(f"cannot read stage kind vocabulary {_STAGE_KINDS_REL}: {e}") from e
    kinds = data.get("kinds") if isinstance(data, dict) else None
    if not isinstance(kinds, list) or not all(isinstance(k, str) for k in kinds):
        raise ValueError(f"{_STAGE_KINDS_REL} must be an object with a 'kinds' array of strings")
    return kinds


def _edit_distance(a: str, b: str) -> int:
    """Damerau-Levenshtein (optimal string alignment) distance."""
    prev2: List[int] = []
    prev = list(range(len(b) + 1))
    for i, ca in enumerate(a, 1):
        cur = [i]
        for j, cb in enumerate(b, 1):
            cur.append(min(prev[j] + 1, cur[j - 1] + 1, prev[j - 1] + (ca != cb)))
            if i > 1 and j > 1 and ca == b[j - 2] and a[i - 2] == cb:
                cur[j] = min(cur[j], prev2[j - 2] + 1)  # adjacent transposition
        prev2, prev = prev, cur
    return prev[-1]


def _near_kind(tag: str, kinds: List[str]) -> str | None:
    """Return the kind a non-kind tag is a case/plural/edit-distance-1 variant of, else None."""
    if tag in kinds:
        return None
    low = tag.lower()
    for kind in kinds:
        if low == kind or low == kind + "s" or _edit_distance(low, kind) <= 1:
            return kind
    return None


def _validate_stage_tags(data: dict) -> List[str]:
    errors: List[str] = []
    if "tags" in data:
        tags = data["tags"]
        if not isinstance(tags, list):
            errors.append(f"tags should be list, got {type(tags).__name__}")
        else:
            try:
                kinds = _stage_kinds()
            except ValueError as e:
                kinds = []
                errors.append(f"tags: {e}")
            if len(tags) > _MAX_STAGE_TAGS:
                errors.append(f"tags has {len(tags)} entries, at most {_MAX_STAGE_TAGS} allowed")
            seen = set()
            for i, tag in enumerate(tags):
                if not isinstance(tag, str):
                    errors.append(f"tags[{i}] should be str, got {type(tag).__name__}")
                    continue
                if tag in seen:
                    errors.append(f"tags[{i}]: duplicate tag '{tag}'")
                seen.add(tag)
                near = _near_kind(tag, kinds)
                if near is not None:
                    errors.append(f"tags[{i}]: tag '{tag}' looks like the kind '{near}'; use '{near}' exactly")
                elif not (1 <= len(tag) <= _MAX_TAG_LEN) or not _KEBAB_RE.match(tag):
                    errors.append(
                        f"tags[{i}]: tag '{tag}' must be 1-{_MAX_TAG_LEN} characters of lowercase kebab-case")
    if "description" in data:
        desc = data["description"]
        if not isinstance(desc, str):
            errors.append(f"description should be str, got {type(desc).__name__}")
        elif len(desc) > _MAX_DESCRIPTION_LEN:
            errors.append(f"description is {len(desc)} characters, at most {_MAX_DESCRIPTION_LEN} allowed")
    return errors


_MAX_STAGE_PROVES = 16
_PROVES_PREFIX = "docs/specs/"
_PROVES_SUFFIX = ".md"


def _validate_stage_proves(data: dict, repo_root: Path | None = None) -> ValidationResult:
    """Validate the optional `proves` spec-path list. Returns (errors, warnings).

    Shape problems are errors; a well-formed path whose file is missing is a warning.
    """
    errors: List[str] = []
    warnings: List[str] = []
    if "proves" not in data:
        return errors, warnings
    proves = data["proves"]
    if not isinstance(proves, list):
        return [f"proves should be list, got {type(proves).__name__}"], warnings
    if len(proves) > _MAX_STAGE_PROVES:
        errors.append(f"proves has {len(proves)} entries, at most {_MAX_STAGE_PROVES} allowed")
    if repo_root is None:
        repo_root = find_repo_root(__file__)
    seen = set()
    for i, entry in enumerate(proves):
        if not isinstance(entry, str):
            errors.append(f"proves[{i}] should be str, got {type(entry).__name__}")
            continue
        if entry in seen:
            errors.append(f"proves[{i}]: duplicate entry '{entry}'")
        seen.add(entry)
        if "\\" in entry:
            errors.append(f"proves[{i}]: '{entry}' must use forward slashes, not backslashes")
        elif not entry.startswith(_PROVES_PREFIX) or not entry.endswith(_PROVES_SUFFIX):
            errors.append(
                f"proves[{i}]: '{entry}' must be a path starting with '{_PROVES_PREFIX}' "
                f"and ending with '{_PROVES_SUFFIX}'")
        elif not (repo_root / entry).is_file():
            warnings.append(f"proves[{i}]: spec file '{entry}' does not exist")
    return errors, warnings


_MAX_STAGE_PRESETS = 16
_MAX_PRESET_NAME_LEN = 31
_MAX_PRESET_BYTES = 2048


def _load_stage_app_modules(data: dict, stage_path: Path | None):
    """Return ({instance_id: [stages]}, error) for the .diaapp a .diastage points at."""
    manifest = data.get("manifest")
    if stage_path is None or not isinstance(manifest, str) or not manifest:
        return None, "cannot resolve the stage's .diaapp (no stage file path or manifest field)"
    start = stage_path.resolve().parent
    app = None
    for base in [start, *start.parents]:
        if (base / manifest).is_file():
            app = base / manifest
            break
    if app is None:
        return None, f"cannot resolve the stage's .diaapp '{manifest}'"
    try:
        app_data = json.loads(app.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as e:
        return None, f"cannot read the stage's .diaapp '{manifest}': {e}"
    modules = {}
    for pu in app_data.get("processing_units", []) if isinstance(app_data, dict) else []:
        for mod in pu.get("modules", []) if isinstance(pu, dict) else []:
            if isinstance(mod, dict) and isinstance(mod.get("instance_id"), str):
                stages = mod.get("stages")
                modules[mod["instance_id"]] = stages if isinstance(stages, list) else []
    return modules, None


def _validate_stage_presets(data: dict, stage_path: Path | None = None) -> List[str]:
    """Validate the optional `presets` launch-preset list. Returns errors."""
    errors: List[str] = []
    if "presets" not in data:
        return errors
    presets = data["presets"]
    if not isinstance(presets, list):
        return [f"presets should be list, got {type(presets).__name__}"]
    if len(presets) > _MAX_STAGE_PRESETS:
        errors.append(f"presets has {len(presets)} entries, at most {_MAX_STAGE_PRESETS} allowed")
    app_modules = None
    app_error = None
    app_loaded = False
    stage_name = data.get("name")
    seen = set()
    for i, preset in enumerate(presets):
        prefix = f"presets[{i}]"
        if not isinstance(preset, dict):
            errors.append(f"{prefix} should be object, got {type(preset).__name__}")
            continue
        name = preset.get("name")
        if not isinstance(name, str):
            errors.append(f"{prefix}.name is required and should be str")
        else:
            if name in seen:
                errors.append(f"{prefix}: duplicate preset name '{name}'")
            seen.add(name)
            if not (1 <= len(name) <= _MAX_PRESET_NAME_LEN) or not _KEBAB_RE.match(name):
                errors.append(
                    f"{prefix}: name '{name}' must be 1-{_MAX_PRESET_NAME_LEN} characters of lowercase kebab-case")
        if "description" in preset:
            desc = preset["description"]
            if not isinstance(desc, str):
                errors.append(f"{prefix}.description should be str, got {type(desc).__name__}")
            elif len(desc) > _MAX_DESCRIPTION_LEN:
                errors.append(
                    f"{prefix}.description is {len(desc)} characters, at most {_MAX_DESCRIPTION_LEN} allowed")
        try:
            size = len(json.dumps(preset, separators=(",", ":")).encode("utf-8"))
        except (TypeError, ValueError):
            size = 0
        if size > _MAX_PRESET_BYTES:
            errors.append(f"{prefix} is {size} bytes of JSON, at most {_MAX_PRESET_BYTES} allowed")
        modules = preset.get("modules")
        if not isinstance(modules, dict):
            errors.append(f"{prefix}.modules is required and should be object")
            continue
        for inst, override in modules.items():
            if not isinstance(override, dict):
                errors.append(f"{prefix}.modules.{inst} should be object, got {type(override).__name__}")
        if not modules:
            continue
        if not app_loaded:
            app_modules, app_error = _load_stage_app_modules(data, stage_path)
            app_loaded = True
        if app_modules is None:
            errors.append(f"{prefix}.modules: {app_error}")
            continue
        for inst in modules:
            if inst not in app_modules:
                errors.append(f"{prefix}.modules: '{inst}' is not a module instance declared in the stage's .diaapp")
            elif "all" in app_modules[inst]:
                errors.append(
                    f"{prefix}.modules: '{inst}' is an all-scoped module; an all-scoped module is not "
                    "restarted on stage entry so it can't take an override")
            elif stage_name not in app_modules[inst]:
                errors.append(f"{prefix}.modules: '{inst}' is not scoped to stage '{stage_name}'")
    return errors


def _validate_diastage_full(data: dict, repo_root: Path | None = None,
                            stage_path: Path | None = None) -> ValidationResult:
    """Validate a .diastage file. Returns (errors, warnings)."""
    errors = []
    errors += _check_key(data, "name", str)
    errors += _check_key(data, "manifest", str)
    errors += _check_key(data, "config", dict)
    if isinstance(data.get("config"), dict):
        errors += _check_key(data["config"], "path_aliases", dict, "config")
        errors += _check_reserved_aliases(data["config"])
    errors += _validate_stage_tags(data)
    proves_errors, warnings = _validate_stage_proves(data, repo_root)
    errors += proves_errors
    errors += _validate_stage_presets(data, stage_path)
    return errors, warnings


def _validate_diastage(data: dict) -> List[str]:
    """Validate a .diastage file (errors only)."""
    return _validate_diastage_full(data)[0]


def _check_world_records(cfg: dict, file_path: Path) -> List[str]:
    """Warn about 'world'-tagged catalogue records outside a Global/ folder (they deploy flat)."""
    from dia_cli.commands.asset.layout import world_record_outside_global

    catalogue_name = cfg.get("asset_catalogue")
    if not isinstance(catalogue_name, str):
        return []
    catalogue = _load_json(file_path.parent / catalogue_name)
    if not isinstance(catalogue, dict):
        return []
    return [
        f"catalogue record '{rec.get('id', '')}': 'world' asset source_path '{rec.get('source_path', '')}' "
        "has no 'Global/' folder and will deploy flat; move it under 'Global/<subfolder>/'"
        for rec in catalogue.get("assets", [])
        if isinstance(rec, dict) and world_record_outside_global(rec)
    ]


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
        errors += _check_reserved_aliases(cfg)
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

    if "editor" in data:
        if not isinstance(data["editor"], dict):
            errors.append("editor should be object")
        elif "manifests" in data["editor"]:
            manifests = data["editor"]["manifests"]
            if not isinstance(manifests, list):
                errors.append("editor.manifests should be array")
            else:
                for i, entry in enumerate(manifests):
                    if not isinstance(entry, str) or not entry:
                        errors.append(f"editor.manifests[{i}] should be a non-empty string")

    if file_path and isinstance(data.get("imports"), list):
        composed, compose_warnings = _compose_game_manifest(data, file_path)
        warnings += compose_warnings
        errors += _validate_service_streams(composed)
        # Ordering across the imported manifests: the runtime composes PU modules in import order, so a
        # requires_tick_after that is satisfied inside one .diaapp can still be broken by the composition.
        for pu in composed.get("processing_units", []):
            errors += [f"{e} (composed {file_path.name})" for e in _check_requires_tick_after(pu)]
        errors += [f"{e} (composed {file_path.name})" for e in _check_dependencies_present(composed)]

    if file_path and isinstance(data.get("config"), dict):
        warnings += _check_world_records(data["config"], file_path)

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
            if isinstance(pu.get("modules"), list):
                errors += _check_requires_tick_after(pu)

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


def _check_requires_tick_after(pu: dict) -> List[str]:
    """Enforce the optional module field `requires_tick_after`: instance ids of modules in the SAME
    processing unit that must be listed earlier (ProcessingUnit ticks in list order, stops in reverse)."""
    errors: List[str] = []
    pu_id = pu.get("instance_id", "?")
    modules = [m for m in pu["modules"] if isinstance(m, dict)]
    index_of: dict = {}
    for idx, m in enumerate(modules):
        if isinstance(m.get("instance_id"), str):
            index_of.setdefault(m["instance_id"], idx)

    for idx, mod in enumerate(modules):
        if "requires_tick_after" not in mod:
            continue
        name = mod.get("instance_id", "?")
        refs = mod["requires_tick_after"]
        where = f"PU '{pu_id}' module '{name}'"
        if not isinstance(refs, list) or not all(isinstance(r, str) for r in refs):
            errors.append(f"{where}: requires_tick_after must be an array of module instance id strings")
            continue
        for ref in refs:
            if ref not in index_of:
                errors.append(f"{where} (index {idx}): requires_tick_after '{ref}' not found in PU '{pu_id}' "
                              f"module list [REQUIRES_TICK_AFTER_MISSING]")
            elif index_of[ref] >= idx:
                errors.append(f"{where} (index {idx}): requires_tick_after '{ref}' is at index {index_of[ref]}; "
                              f"it must be listed earlier [REQUIRES_TICK_AFTER_ORDER]")
    return errors


def _check_dependencies_present(composed: dict) -> List[str]:
    """Every module's declared `dependencies` must name a module instance present in the composed manifest
    (any PU). `soft_dependencies` are never required. Run on the composed .diagame only: a single .diaapp
    legitimately depends on modules supplied by sibling manifests."""
    present = {m.get("instance_id") for pu in composed.get("processing_units", []) if isinstance(pu, dict)
               for m in pu.get("modules", []) if isinstance(m, dict)}
    errors: List[str] = []
    for pu in composed.get("processing_units", []):
        if not isinstance(pu, dict):
            continue
        for mod in pu.get("modules", []):
            deps = mod.get("dependencies") if isinstance(mod, dict) else None
            if not isinstance(deps, list):
                continue
            for dep in deps:
                if isinstance(dep, str) and dep not in present:
                    errors.append(f"PU '{pu.get('instance_id', '?')}' module '{mod.get('instance_id', '?')}': "
                                  f"dependency '{dep}' is not in the composed module list "
                                  f"[REQUIRES_DEPENDENCY_MISSING]")
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
    # Test fixtures hold deliberately invalid manifests; skip them in repo-wide scans only.
    if not target_path:
        files = [f for f in files if "Fixtures" not in f.relative_to(repo_root).parts]

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
            errors, warnings = _validate_diastage_full(data, repo_root, f)
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


@cli.command("schema")
@click.pass_context
@click.argument("paths", nargs=-1)
@click.option("--target", default=None, help="App target whose path aliases define the default scope.")
@click.option("--json", "as_json", is_flag=True, default=False, help="Print findings as a JSON list.")
@click.option("--verbose", is_flag=True, default=False, help="Show OK files in output.")
@click.option("--code-side", default=None, type=click.Path(), help="Code-side command export to cross-check.")
def schema(ctx: click.Context, paths: Tuple[str, ...], target: str | None, as_json: bool,
           verbose: bool, code_side: str | None) -> None:
    """Validate *.schema.json files and the data files they describe."""
    from dia_cli.commands.validate.schema_cmd import schema_command

    schema_command(ctx, paths, target, as_json, verbose, code_side)
