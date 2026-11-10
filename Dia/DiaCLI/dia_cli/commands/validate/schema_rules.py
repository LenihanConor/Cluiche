"""dia validate schema: pure schema-file rules mirroring ValueSchemaLoader.cpp.

No file IO. The shared conformance fixtures under
Dia/DiaValueSchema/Testing/Conformance keep this and the C++ loader in agreement.
Unlike the loader (which stops at the first problem), each entry reports its first
problem and validation continues with the next entry.
"""
from __future__ import annotations

import json
import math
from typing import List, Optional

from dia_cli.commands.validate.schema_cmd import Finding

ENTRY_KEYS = ("key", "kind", "label", "description", "hint", "unit", "type", "min", "max", "default",
              "values", "affects", "related", "source", "unique", "required", "ref", "ui", "history",
              "payload", "vocabulary", "keyRef", "valueType")
UI_KEYS = ("group", "step", "decimals", "ramp", "layer", "hidden", "format", "colour", "glyph", "hotkey",
           "delta_ramp")
HISTORY_KEYS = ("track", "group", "aggregate")
FILE_KEYS = ("format", "version", "for", "entries")
KINDS = ("parameter", "field", "metric", "arg", "command", "event", "component_field")
TYPES = ("float", "int", "bool", "enum", "string", "ref", "ref[]", "map")
MAP_VALUE_TYPES = ("float", "int", "bool", "string", "enum")


class _Stop(Exception):
    """First problem in the current scope found."""

    def __init__(self, rule: str, message: str):
        super().__init__(message)
        self.rule = rule
        self.message = message


def _is_number(v: object) -> bool:
    return isinstance(v, (int, float)) and not isinstance(v, bool)


def _is_finite_number(v: object) -> bool:
    """A JSON number that is not NaN/Infinity (Python's json accepts those literals; jsoncpp does not)."""
    return _is_number(v) and (isinstance(v, int) or math.isfinite(v))


def _check_ref_format(field: str, value: str) -> None:
    fmt, sep, path = value.partition("/")
    if not sep or not fmt or not path:
        raise _Stop("schema.ref-malformed", f"field '{field}' must be '<format>/<record path>'")


def _require_string(obj: dict, field: str) -> str:
    if field not in obj:
        raise _Stop("schema.missing-key", f"missing required field '{field}'")
    if not isinstance(obj[field], str):
        raise _Stop("schema.wrong-type", f"field '{field}' must be a string")
    return obj[field]


def _optional_string(obj: dict, field: str) -> None:
    if field in obj and not isinstance(obj[field], str):
        raise _Stop("schema.wrong-type", f"field '{field}' must be a string")


def _string_list(obj: dict, field: str) -> None:
    if field not in obj:
        return
    if not isinstance(obj[field], list):
        raise _Stop("schema.wrong-type", f"field '{field}' must be an array")
    if not all(isinstance(x, str) for x in obj[field]):
        raise _Stop("schema.wrong-type", f"field '{field}' must contain strings")


def _check_enum_values(e: dict) -> None:
    if "values" not in e:
        return
    arr = e["values"]
    if not isinstance(arr, list):
        raise _Stop("schema.wrong-type", "field 'values' must be an array")
    for v in arr:
        if not isinstance(v, dict):
            raise _Stop("schema.wrong-type", "enum value must be an object")
        for f in ("id", "label", "description", "colour"):
            _optional_string(v, f)


def _check_event_list(e: dict, field: str, item_name: str, required: tuple, optional: tuple,
                      check_type: bool) -> None:
    if field not in e:
        return
    if e["kind"] != "event":
        raise _Stop("schema.event-only-field", f"field '{field}' is only valid on event entries")
    arr = e[field]
    if not isinstance(arr, list):
        raise _Stop("schema.wrong-type", f"field '{field}' must be an array")
    for v in arr:
        if not isinstance(v, dict):
            raise _Stop("schema.wrong-type", f"{item_name} item must be an object")
        for f in required:
            t = _require_string(v, f)
            if check_type and f == "type" and t not in TYPES:
                raise _Stop("schema.unknown-type", f"{item_name} item has unknown 'type'")
        for f in optional:
            _optional_string(v, f)


def _check_type_specific(e: dict) -> None:
    t = e["type"]
    if t in ("ref", "ref[]"):
        _check_ref_format("ref", _require_string(e, "ref"))
    elif "ref" in e:
        raise _Stop("schema.ref-misuse", "field 'ref' is only valid on ref and ref[] types")
    if t == "map":
        _check_ref_format("keyRef", _require_string(e, "keyRef"))
        vt = _require_string(e, "valueType")
        if vt not in MAP_VALUE_TYPES:
            raise _Stop("schema.map-misuse", "field 'valueType' must be one of float, int, bool, string, enum")
    else:
        if "keyRef" in e:
            raise _Stop("schema.map-misuse", "field 'keyRef' is only valid on map types")
        if "valueType" in e:
            raise _Stop("schema.map-misuse", "field 'valueType' is only valid on map types")
    if t == "enum" and "values" not in e:
        raise _Stop("schema.enum-no-values", "enum type requires a 'values' array")


def _check_entry(e: object, file: str, warned: set, warnings: List[Finding]) -> str:
    """Validate one entry. Returns its key (or '') ; raises _Stop with the entry key attached."""
    if not isinstance(e, dict):
        raise _Stop("schema.entry-not-object", "entry must be an object")
    if not isinstance(e.get("key"), str):
        raise _Stop("schema.missing-key", "entry is missing string 'key'")
    key = e["key"]
    try:
        for n in e:
            if n not in ENTRY_KEYS:
                raise _Stop("schema.unknown-entry-key", f"unknown key '{n}'")
        kind = _require_string(e, "kind")
        if kind not in KINDS:
            raise _Stop("schema.unknown-kind", "unknown 'kind'")
        _require_string(e, "label")
        desc = _require_string(e, "description")
        if not desc.strip():
            raise _Stop("schema.empty-description", "field 'description' must not be empty or blank")
        t = _require_string(e, "type")
        if t not in TYPES:
            raise _Stop("schema.unknown-type", "unknown 'type'")
        for f in ("unit", "hint", "source"):
            _optional_string(e, f)
        for f in ("min", "max"):
            if f in e and not _is_finite_number(e[f]):
                raise _Stop("schema.wrong-type", f"field '{f}' must be a finite number")
        for f in ("unique", "required"):
            if f in e and not isinstance(e[f], bool):
                raise _Stop("schema.wrong-type", f"field '{f}' must be a boolean")
        for block, known in (("ui", UI_KEYS), ("history", HISTORY_KEYS)):
            if block not in e:
                continue
            if not isinstance(e[block], dict):
                raise _Stop("schema.wrong-type", f"field '{block}' must be an object")
            for n in e[block]:
                if n in known or (block, n) in warned:
                    continue
                warned.add((block, n))
                warnings.append(Finding(
                    "WARN", "schema.unknown-block-key", file, key,
                    f"Schema '{file}' entry '{key}': unknown {block} key '{n}' (kept)"))
        _string_list(e, "affects")
        _string_list(e, "related")
        _check_type_specific(e)
        _check_enum_values(e)
        _check_event_list(e, "payload", "payload", ("name", "type", "description"), ("unit",), True)
        _check_event_list(e, "vocabulary", "vocabulary", ("id", "description"), ("label", "unit"), False)
    except _Stop as s:
        s.key = key  # type: ignore[attr-defined]
        raise
    return key


def validate_schema_document(doc: object, file: str) -> List[Finding]:
    def fail(rule: str, msg: str, key: str = "") -> Finding:
        return Finding("FAIL", rule, file, key, msg)

    if not isinstance(doc, dict):
        return [fail("schema.root-not-object", "root must be an object")]
    if not isinstance(doc.get("format"), str) or doc["format"] != "dia.value_schema":
        return [fail("schema.bad-format", "'format' must be \"dia.value_schema\"")]
    v = doc.get("version")
    if isinstance(v, bool) or not isinstance(v, int) or v != 1:
        return [fail("schema.bad-version", "'version' must be the integer 1")]
    if not isinstance(doc.get("for"), str):
        return [fail("schema.missing-for", "missing string 'for'")]
    if not isinstance(doc.get("entries"), list):
        return [fail("schema.missing-entries", "missing 'entries' array")]
    for n in doc:
        if n not in FILE_KEYS:
            return [fail("schema.unknown-file-key", f"unknown file-level key '{n}'")]

    findings: List[Finding] = []
    warned: set = set()
    seen: set = set()
    for e in doc["entries"]:
        try:
            key = _check_entry(e, file, warned, findings)
        except _Stop as s:
            findings.append(fail(s.rule, s.message, getattr(s, "key", "")))
            continue
        full = f"{doc['for']}/{key}"
        if full in seen:
            findings.append(fail("schema.duplicate-key", f"duplicate key '{full}' in this file", key))
        seen.add(full)
    return findings


def validate_schema_text(text: str, file: str) -> List[Finding]:
    try:
        doc = json.loads(text)
    except (ValueError, RecursionError) as ex:  # RecursionError: pathologically nested text
        return [Finding("FAIL", "schema.unparsable-json", file, "", f"unparsable JSON: {ex}")]
    return validate_schema_document(doc, file)
