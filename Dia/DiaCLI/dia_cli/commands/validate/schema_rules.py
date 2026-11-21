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
              "payload", "vocabulary", "keyRef", "keyEnum", "valueType", "bands", "valuesFrom")
UI_KEYS = ("group", "step", "decimals", "ramp", "layer", "hidden", "format", "colour", "glyph", "hotkey",
           "delta_ramp", "default_base", "inset", "backdrop", "elevation_layer", "select_group")
HISTORY_KEYS = ("track", "group", "aggregate")
FILE_KEYS = ("format", "version", "bandSchemaVersion", "for", "entries", "constraints")
KINDS = ("parameter", "field", "metric", "arg", "command", "event", "component_field")
TYPES = ("float", "int", "bool", "enum", "string", "ref", "ref[]", "map", "enum[]", "ref[2][]")
REF_TYPES = ("ref", "ref[]", "ref[2][]")
ENUM_TYPES = ("enum", "enum[]")
VALUES_FROM_REQUIRED = ("format", "records", "id")
VALUES_FROM_OPTIONAL = ("label", "colour", "description")
MAP_VALUE_TYPES = ("float", "int", "bool", "string", "enum")
NUMERIC_TYPES = ("float", "int")
# Pack envelope keys: a kind-parameter entry with one of these could never be set by a pack.
RESERVED_PARAMETER_KEYS = ("format", "version", "basedOn")
CONSTRAINT_OPS = ("<", "<=", ">", ">=", "==", "!=")


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


def _check_values_from(e: dict) -> None:
    """`valuesFrom` on an enum entry: the ids come from a data document (same rules as ValueSchemaLoader.cpp)."""
    if "valuesFrom" not in e:
        return
    vf = e["valuesFrom"]
    if not isinstance(vf, dict):
        raise _Stop("schema.wrong-type", "field 'valuesFrom' must be an object")
    if e["type"] != "enum":
        raise _Stop("schema.values-from-misuse", "field 'valuesFrom' is only valid on enum types")
    if "values" in e:
        raise _Stop("schema.values-from-misuse", "an enum entry takes 'values' or 'valuesFrom', not both")
    for n in vf:
        if n not in VALUES_FROM_REQUIRED + VALUES_FROM_OPTIONAL:
            raise _Stop("schema.unknown-entry-key", f"valuesFrom has unknown key '{n}'")
    for f in VALUES_FROM_REQUIRED:
        if not isinstance(vf.get(f), str):
            raise _Stop("schema.wrong-type", f"valuesFrom is missing string '{f}'")
    for f in VALUES_FROM_OPTIONAL:
        if f in vf and not isinstance(vf[f], str):
            raise _Stop("schema.wrong-type", f"valuesFrom field '{f}' must be a string")


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
    if t in REF_TYPES:
        _check_ref_format("ref", _require_string(e, "ref"))
    elif "ref" in e:
        raise _Stop("schema.ref-misuse", "field 'ref' is only valid on ref, ref[] and ref[2][] types")
    if t == "map":
        if "keyRef" in e and "keyEnum" in e:
            raise _Stop("schema.map-misuse", "fields 'keyRef' and 'keyEnum' are mutually exclusive on a map")
        if "keyEnum" in e:
            _require_string(e, "keyEnum")   # names an enum entry of this file; resolved after all entries are read
        else:
            _check_ref_format("keyRef", _require_string(e, "keyRef"))
        vt = _require_string(e, "valueType")
        if vt not in MAP_VALUE_TYPES:
            raise _Stop("schema.map-misuse", "field 'valueType' must be one of float, int, bool, string, enum")
    else:
        if "keyRef" in e:
            raise _Stop("schema.map-misuse", "field 'keyRef' is only valid on map types")
        if "keyEnum" in e:
            raise _Stop("schema.map-misuse", "field 'keyEnum' is only valid on map types")
        if "valueType" in e:
            raise _Stop("schema.map-misuse", "field 'valueType' is only valid on map types")
    _check_values_from(e)
    if t in ENUM_TYPES and "values" not in e and "valuesFrom" not in e:
        suffix = " (or 'valuesFrom' on enum)" if t == "enum" else ""
        raise _Stop("schema.enum-no-values", f"{t} type requires a 'values' array{suffix}")


def _check_bands(e: dict) -> None:
    if "bands" not in e:
        return
    bands = e["bands"]
    if not isinstance(bands, dict):
        raise _Stop("schema.bad-band", "field 'bands' must be an object")
    if e["type"] not in NUMERIC_TYPES:
        raise _Stop("schema.bad-band", "field 'bands' is only valid on numeric (float, int) types")
    for name, b in bands.items():
        if not isinstance(b, list) or len(b) != 2:
            raise _Stop("schema.bad-band", f"band '{name}' must be a [lo, hi] array")
        lo, hi = b
        if not _is_finite_number(lo) or not _is_finite_number(hi):
            raise _Stop("schema.bad-band", f"band '{name}' bounds must be finite numbers")
        if lo > hi:
            raise _Stop("schema.bad-band", f"band '{name}' has lo greater than hi")
        if "min" in e and lo < e["min"]:
            raise _Stop("schema.bad-band", f"band '{name}' lies below 'min'")
        if "max" in e and hi > e["max"]:
            raise _Stop("schema.bad-band", f"band '{name}' lies above 'max'")


def _check_map_default(e: dict) -> None:
    """An object `default` on a map entry: each value checked against valueType, min and max (as the C++ loader).
    An enum-valued map takes `mapKey -> enum id string`; each id must be one of the entry's `values`.

    An object default on a non-map entry is ignored, exactly as the loader ignores it."""
    d = e.get("default")
    if e["type"] != "map" or not isinstance(d, dict):
        return
    vt = e["valueType"]
    for name, v in d.items():
        what = f"map default '{name}'"
        if vt in NUMERIC_TYPES:
            if not _is_finite_number(v):
                raise _Stop("schema.bad-map-default", f"{what} must be a number (field 'default')")
            if vt == "int" and v != math.floor(v):
                raise _Stop("schema.bad-map-default", f"{what} must be a whole number (field 'default')")
            if "min" in e and v < e["min"]:
                raise _Stop("schema.bad-map-default", f"{what} (field 'default') is below 'min'")
            if "max" in e and v > e["max"]:
                raise _Stop("schema.bad-map-default", f"{what} (field 'default') is above 'max'")
        elif vt == "bool":
            if not isinstance(v, bool):
                raise _Stop("schema.bad-map-default", f"{what} must be a boolean (field 'default')")
        elif not isinstance(v, str):
            raise _Stop("schema.bad-map-default", f"{what} must be a string (field 'default')")
        elif vt == "enum":
            ids = [x.get("id") for x in e.get("values", []) if isinstance(x, dict)] if isinstance(e.get("values"), list) else []
            if v not in ids:
                raise _Stop("schema.bad-map-default", f"{what} '{v}' is not a value of the enum (field 'default')")


def _numeric_heads(entries: list) -> set:
    heads = set()
    for e in entries:
        if not isinstance(e, dict) or not isinstance(e.get("key"), str):
            continue
        t = e.get("type")
        if t in NUMERIC_TYPES or (t == "map" and e.get("valueType") in NUMERIC_TYPES):
            heads.add(e["key"])
    return heads


def _check_constraints(constraints: object, entries: list, fail) -> List[Finding]:
    if not isinstance(constraints, list):
        return [fail("schema.bad-constraint", "'constraints' must be an array")]
    heads = _numeric_heads(entries)
    out: List[Finding] = []
    for i, c in enumerate(constraints):
        name = f"constraints[{i}]"
        label = "constraint"
        problem = None
        if not isinstance(c, dict):
            problem = "must be an object {a, op, b}"
        else:
            a, b = c.get("a"), c.get("b")
            op = c.get("op")
            label = f"constraint '{a if isinstance(a, str) else '?'} {op if isinstance(op, str) else '?'} {b if isinstance(b, str) else '?'}'"
            if not isinstance(a, str) or not isinstance(b, str):
                problem = "'a' and 'b' must be strings"
            elif not isinstance(c.get("op"), str) or c["op"] not in CONSTRAINT_OPS:
                problem = "'op' must be one of <, <=, >, >=, ==, !="
            else:
                for side in (a, b):
                    if side.partition(".")[0] not in heads:
                        problem = f"'{side}' does not name a numeric entry"
                        break
        if problem:
            out.append(fail("schema.bad-constraint", f"{label}: {problem}", name))
    return out


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
        if kind == "parameter" and key in RESERVED_PARAMETER_KEYS:
            raise _Stop("schema.reserved-key",
                        f"parameter key '{key}' is reserved for the pack envelope (format, version, basedOn)")
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
        _check_bands(e)
        _check_map_default(e)
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
    if "bandSchemaVersion" in doc:
        b = doc["bandSchemaVersion"]
        whole = isinstance(b, int) or (isinstance(b, float) and math.isfinite(b) and b == int(b))
        if isinstance(b, bool) or not whole or b < 1 or b > 2147483647:
            return [fail("schema.bad-band-schema-version",
                         "'bandSchemaVersion' must be a whole number from 1 to 2147483647")]

    findings: List[Finding] = []
    warned: set = set()
    seen: set = set()
    types_by_key = {e["key"]: e.get("type") for e in doc["entries"] if isinstance(e, dict) and isinstance(e.get("key"), str)}
    for e in doc["entries"]:
        try:
            key = _check_entry(e, file, warned, findings)
        except _Stop as s:
            findings.append(fail(s.rule, s.message, getattr(s, "key", "")))
            continue
        # keyEnum must name an enum entry of this file (same wording as ValueSchemaLoader.cpp).
        if e.get("type") == "map" and isinstance(e.get("keyEnum"), str):
            target = e["keyEnum"]
            if target not in types_by_key:
                findings.append(fail("schema.map-misuse", f"keyEnum '{target}' names no entry in this file", key))
                continue
            if types_by_key[target] != "enum":
                findings.append(fail("schema.map-misuse", f"keyEnum '{target}' is not an enum entry", key))
                continue
        full = f"{doc['for']}/{key}"
        if full in seen:
            findings.append(fail("schema.duplicate-key", f"duplicate key '{full}' in this file", key))
        seen.add(full)
    if "constraints" in doc:
        findings.extend(_check_constraints(doc["constraints"], doc["entries"], fail))
    return findings


def validate_schema_text(text: str, file: str) -> List[Finding]:
    try:
        doc = json.loads(text)
    except (ValueError, RecursionError) as ex:  # RecursionError: pathologically nested text
        return [Finding("FAIL", "schema.unparsable-json", file, "", f"unparsable JSON: {ex}")]
    return validate_schema_document(doc, file)
