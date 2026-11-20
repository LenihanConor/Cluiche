"""dia validate schema: pure data-file rules (coverage, type, range, enum, defaults,
refs, map keys, unique, required) and schema-level reference checks.

No file IO. Shared fixtures under Dia/DiaValueSchema/Testing/Conformance/data keep this
and T-2's TypeScript checks in agreement. Cross-record and cross-format rules (ref, map
keys) resolve against a DataContext holding every paired data document.
"""
from __future__ import annotations

import math
from typing import Dict, Iterable, List, Mapping, Optional, Set, Tuple

from dia_cli.commands.validate.schema_cmd import Finding
from dia_cli.commands.validate.schema_suggest import format_suggestion, suggest

ENVELOPE_KEYS = ("format", "version")
DATA_KINDS = ("parameter", "field")

# Fallback when the shipped ramp library cannot be read (the CLI installed away from the repo).
_FALLBACK_RAMPS = {"sequential.green", "sequential.blue", "sequential.heat", "sequential.grey", "sequential.purple",
                   "sequential.terrain", "diverging.blue_red", "categorical.12"}


def _load_known_ramps() -> set:
    """Ramp ids from the shipped library (Dia/DiaValueSchema/Data/ramps.json), so new ramps never drift from the validator."""
    import json
    from pathlib import Path
    try:
        path = Path(__file__).resolve().parents[4] / "DiaValueSchema" / "Data" / "ramps.json"
        doc = json.loads(path.read_text(encoding="utf-8"))
        ids = {r["id"] for r in doc.get("ramps", []) if isinstance(r, dict) and isinstance(r.get("id"), str)}
        return ids or set(_FALLBACK_RAMPS)
    except (OSError, ValueError, IndexError, KeyError):
        return set(_FALLBACK_RAMPS)


KNOWN_RAMPS = _load_known_ramps()


def _collect(node: object, segs: List[str]) -> List[object]:
    """Values at a record path ('biomes[].id', 'a.b') below node."""
    if not segs:
        return [node]
    if not isinstance(node, dict):
        return []
    seg, rest = segs[0], segs[1:]
    if seg.endswith("[]"):
        arr = node.get(seg[:-2])
        if not isinstance(arr, list):
            return []
        out: List[object] = []
        for item in arr:
            out.extend(_collect(item, rest))
        return out
    return _collect(node[seg], rest) if seg in node else []


class DataContext:
    """Every data document by format, so refs and map keys can resolve across formats."""

    def __init__(self, documents_by_format: Optional[Dict[str, List[Tuple[str, object, bool]]]] = None) -> None:
        self.documents_by_format: Dict[str, List[Tuple[str, object, bool]]] = documents_by_format or {}

    @classmethod
    def from_files(cls, files: Iterable[Tuple[str, object, bool]]) -> "DataContext":
        by_fmt: Dict[str, List[Tuple[str, object, bool]]] = {}
        for name, doc, is_pack in files:
            if isinstance(doc, dict) and isinstance(doc.get("format"), str):
                by_fmt.setdefault(doc["format"], []).append((name, doc, is_pack))
        return cls(by_fmt)

    def has_format(self, fmt: str) -> bool:
        return bool(self.documents_by_format.get(fmt))

    def values_at(self, ref: object) -> Optional[List[object]]:
        """Values at '<format>/<record path>' in defaults and packs; None if the format has no documents."""
        if not isinstance(ref, str) or "/" not in ref:
            return None
        fmt, _, path = ref.partition("/")
        if not self.has_format(fmt):
            return None
        out: List[object] = []
        for _name, doc, _pack in self.documents_by_format[fmt]:
            out.extend(_collect(doc, path.split(".")))
        return out


CONSTRAINT_OPS = {
    "<": lambda a, b: a < b, "<=": lambda a, b: a <= b, ">": lambda a, b: a > b,
    ">=": lambda a, b: a >= b, "==": lambda a, b: a == b, "!=": lambda a, b: a != b,
}


def _well_formed_constraints(doc: dict) -> List[dict]:
    """Constraints with string a/b and a known op; malformed ones are schema.bad-constraint's concern."""
    cs = doc.get("constraints")
    if not isinstance(cs, list):
        return []
    return [c for c in cs if isinstance(c, dict) and isinstance(c.get("a"), str) and isinstance(c.get("b"), str)
            and c.get("op") in CONSTRAINT_OPS]


def _numeric(v: object) -> bool:
    return _is_number(v) and _is_finite(v)


def _default_values(entries: Iterable[dict]) -> Dict[str, float]:
    """Numeric defaults by operand key: '<key>' for scalars, '<key>.<mapKey>' for object map defaults."""
    out: Dict[str, float] = {}
    for e in entries:
        if not isinstance(e, dict) or not isinstance(e.get("key"), str) or e.get("kind") not in DATA_KINDS:
            continue
        d = e.get("default")
        if isinstance(d, dict) and e.get("type") == "map":
            out.update({f"{e['key']}.{k}": v for k, v in d.items() if _numeric(v)})
        elif not isinstance(d, dict) and _numeric(d):
            out[e["key"]] = d
    return out


def _flatten(node: dict, prefix: str = "") -> Dict[str, float]:
    """Numeric leaves of a data document by dotted key (map columns come out as '<key>.<mapKey>')."""
    out: Dict[str, float] = {}
    for name, v in node.items():
        if not prefix and name in ENVELOPE_KEYS + ("basedOn",):
            continue
        key = f"{prefix}{name}"
        if isinstance(v, dict):
            out.update(_flatten(v, key + "."))
        elif _numeric(v):
            out[key] = v
    return out


def _violated(c: dict, values: Mapping[str, float]) -> bool:
    a, b = values.get(c["a"]), values.get(c["b"])
    return a is not None and b is not None and not CONSTRAINT_OPS[c["op"]](a, b)


def _constraint_message(c: dict, values: Mapping[str, float]) -> str:
    return (f"constraint '{c['a']} {c['op']} {c['b']}' violated: "
            f"'{c['a']}' = {_fmt(values[c['a']])}, '{c['b']}' = {_fmt(values[c['b']])}")


class SchemaIndex:
    """Parameter/field entries per data-file format, keyed by full (record-path) key."""

    def __init__(self) -> None:
        self._by_format: Dict[str, Dict[str, dict]] = {}
        self._constraints: Dict[str, List[dict]] = {}

    def add(self, doc: dict) -> None:
        table = self._by_format.setdefault(doc["for"], {})
        self._constraints.setdefault(doc["for"], []).extend(_well_formed_constraints(doc))
        entries = doc.get("entries")
        for e in entries if isinstance(entries, list) else []:
            if isinstance(e, dict) and e.get("kind") in DATA_KINDS and isinstance(e.get("key"), str)                     and isinstance(e.get("type"), str):
                table.setdefault(e["key"], e)

    def has_format(self, fmt: object) -> bool:
        return isinstance(fmt, str) and fmt in self._by_format

    def constraints_for(self, fmt: object) -> List[dict]:
        return self._constraints.get(fmt, []) if isinstance(fmt, str) else []

    def entries_for(self, fmt: object) -> Dict[str, dict]:
        return self._by_format.get(fmt, {}) if isinstance(fmt, str) else {}


def build_schema_index(schema_docs: Iterable[dict]) -> SchemaIndex:
    idx = SchemaIndex()
    for d in schema_docs:
        if isinstance(d, dict) and isinstance(d.get("for"), str):
            idx.add(d)
    return idx


def _is_number(v: object) -> bool:
    return isinstance(v, (int, float)) and not isinstance(v, bool)


def _is_finite(v: object) -> bool:
    return isinstance(v, int) or (isinstance(v, float) and math.isfinite(v))


def _fmt(v: object) -> str:
    if isinstance(v, float) and v.is_integer():
        return str(int(v))
    if isinstance(v, str):
        return f"'{v}'"
    if isinstance(v, bool):
        return "true" if v else "false"
    return str(v)


def _same(a: object, b: object) -> bool:
    if isinstance(a, bool) or isinstance(b, bool):
        return isinstance(a, bool) and isinstance(b, bool) and a == b
    return a == b


def _type_ok(t: str, v: object) -> bool:
    # NaN / +-Infinity (Python's json accepts the literals; 1e400 parses to inf) are never valid numbers.
    if t == "float":
        return _is_number(v) and _is_finite(v)
    if t == "int":
        return _is_number(v) and _is_finite(v) and (isinstance(v, int) or v.is_integer())
    if t == "bool":
        return isinstance(v, bool)
    if t in ("string", "enum", "ref"):
        return isinstance(v, str)
    if t == "ref[]":
        return isinstance(v, list) and all(isinstance(x, str) for x in v)
    return True


class _Walker:
    def __init__(self, file: str, entries: Dict[str, dict], is_pack: bool,
                 context: Optional[DataContext] = None) -> None:
        self.file = file
        self.entries = entries
        self.is_pack = is_pack
        self.context = context
        self.known = sorted(entries)
        self.out: List[Finding] = []
        self._warned: Set[str] = set()

    def _add(self, rule: str, key: str, path: str, record: Optional[str], msg: str,
             suggestion: Optional[str] = None) -> None:
        self.out.append(Finding("FAIL", rule, self.file, key, msg, suggestion, path, record))

    @staticmethod
    def _rid(rec: dict, fallback: Optional[str]) -> Optional[str]:
        rid = rec.get("id")
        return rid if isinstance(rid, str) else fallback

    def walk(self, node: dict, key_prefix: str, path_prefix: str, record: Optional[str]) -> None:
        for name, val in node.items():
            if not key_prefix and not path_prefix and (
                    name in ENVELOPE_KEYS or (name == "basedOn" and self.is_pack and name not in self.entries)):
                continue   # a pack's `basedOn` (schema hash it was written against) is envelope, not a parameter
            key = f"{key_prefix}{name}"
            path = f"{path_prefix}{name}"
            entry = self.entries.get(key)
            if isinstance(val, dict) and (entry is None or entry["type"] != "map"):
                if entry is not None:
                    self._add("data.wrong-type", key, path, record,
                              f"'{key}' must be {entry['type']}, found an object")
                    continue
                self.walk(val, key + ".", path + ".", record)
            elif isinstance(val, dict):
                self.check_map(key, path, record, val, entry)
            elif isinstance(val, list) and val and all(isinstance(x, dict) for x in val) \
                    and entry is None:
                for i, rec in enumerate(val):
                    self.walk(rec, key + "[].", f"{path}[{i}].", self._rid(rec, record))
                self.check_columns(key, path, val, record)
            elif isinstance(val, list) and not val and entry is None:
                continue
            else:
                self.leaf(key, path, record, val, entry)

    def _ref_values(self, key: str, path: str, record: Optional[str], ref: object) -> Optional[List[object]]:
        """Referenced values, or None (one WARN per format) when they cannot be known."""
        if self.context is None or not isinstance(ref, str) or "/" not in ref:
            return None
        values = self.context.values_at(ref)
        if values is None:
            fmt = ref.partition("/")[0]
            if fmt not in self._warned:
                self._warned.add(fmt)
                self.out.append(Finding("WARN", "data.ref-format-missing", self.file, key,
                                        f"'{key}' refers to format '{fmt}' which has no data documents; "
                                        f"references to it are not checked", None, path, record))
        return values

    def check_columns(self, key: str, path: str, recs: List[dict], record: Optional[str]) -> None:
        """unique / required columns of one record array."""
        prefix = key + "[]."
        for ekey, entry in self.entries.items():
            if not ekey.startswith(prefix) or "[]" in ekey[len(prefix):]:
                continue
            required, unique = entry.get("required") is True, entry.get("unique") is True
            if not (required or unique):
                continue
            segs = ekey[len(prefix):].split(".")
            seen: Dict[object, int] = {}
            for i, rec in enumerate(recs):
                found = _collect(rec, segs)
                rid = self._rid(rec, record)
                rpath = f"{path}[{i}]." + ".".join(segs)
                if not found:
                    if required:
                        self._add("data.missing-required", ekey, rpath, rid,
                                  f"record {path}[{i}]" + (f" ('{rid}')" if rid else "") +
                                  f" is missing required column '{ekey}'")
                    continue
                v = found[0]
                if unique and isinstance(v, (str, int, float)) and not isinstance(v, bool):
                    if v in seen:
                        self._add("data.duplicate-unique", ekey, rpath, rid,
                                  f"'{ekey}' value {_fmt(v)} in {path}[{i}] duplicates {path}[{seen[v]}]")
                    else:
                        seen[v] = i

    def check_map(self, key: str, path: str, record: Optional[str], val: dict, entry: dict) -> None:
        ids = self._ref_values(key, path, record, entry.get("keyRef"))
        id_strs = [x for x in ids if isinstance(x, str)] if ids is not None else None
        vt = entry.get("valueType")
        ventry = {"type": vt, "min": entry.get("min"), "max": entry.get("max"), "values": entry.get("values", [])}
        for mk, mv in val.items():
            mpath = f"{path}.{mk}"
            if id_strs is not None and mk not in id_strs:
                s = suggest(mk, id_strs)
                self._add("data.unresolved-map-key", key, mpath, record,
                          f"'{key}' key '{mk}' matches no id in {entry.get('keyRef')}" + format_suggestion(s), s)
                continue
            self.check_value(key, mpath, record, mv, ventry, f"'{key}' entry '{mk}'")

    def leaf(self, key: str, path: str, record: Optional[str], val: object, entry: Optional[dict]) -> None:
        if entry is None:
            self._add("data.no-schema-entry", key, path, record,
                      f"key '{key}' has no schema entry" + format_suggestion(suggest(key, self.known)),
                      suggest(key, self.known))
            return
        t = entry["type"]
        if t == "map":
            self._add("data.wrong-type", key, path, record, f"'{key}' must be map (an object), found {val}")
            return
        if not self.check_value(key, path, record, val, entry, f"'{key}'"):
            return
        if t in ("ref", "ref[]"):
            ids = self._ref_values(key, path, record, entry.get("ref"))
            if ids is not None:
                id_strs = [x for x in ids if isinstance(x, str)]
                items = [(path, val)] if t == "ref" else [(f"{path}[{i}]", x) for i, x in enumerate(val)]
                bad = False
                for ipath, item in items:
                    if item not in id_strs:
                        bad = True
                        s = suggest(item, id_strs)
                        self._add("data.unresolved-ref", key, ipath, record,
                                  f"'{key}' = {_fmt(item)} matches no id in {entry.get('ref')}"
                                  + format_suggestion(s), s)
                if bad:
                    return
        if not self.is_pack and entry.get("kind") != "parameter" and "default" in entry \
                and not _same(entry["default"], val):
            self._add("data.default-mismatch", key, path, record,
                      f"'{key}' = {_fmt(val)} differs from schema default {_fmt(entry['default'])}")

    def check_value(self, key: str, path: str, record: Optional[str], val: object, entry: dict,
                    label: str) -> bool:
        """Type, range and enum checks; True when the value is acceptable."""
        t = entry["type"]
        if not _type_ok(t, val):
            self._add("data.wrong-type", key, path, record,
                      f"{label} = {_fmt(val) if not isinstance(val, (list, dict)) else val} is not a valid {t}")
            return False
        if t in ("float", "int"):
            lo, hi = entry.get("min"), entry.get("max")
            lo = lo if _is_number(lo) else None
            hi = hi if _is_number(hi) else None
            if (lo is not None and val < lo) or (hi is not None and val > hi):
                lo_s = _fmt(lo) if lo is not None else "-inf"
                hi_s = _fmt(hi) if hi is not None else "inf"
                self._add("data.out-of-range", key, path, record, f"{label} = {_fmt(val)} outside [{lo_s}, {hi_s}]")
                return False
        elif t == "enum":
            vals = entry.get("values", [])
            ids = [v["id"] for v in vals if isinstance(v, dict) and isinstance(v.get("id"), str)]                 if isinstance(vals, list) else []
            if val not in ids:
                s = suggest(val, ids)
                self._add("data.bad-enum", key, path, record,
                          f"{label} = {_fmt(val)} is not one of {', '.join(ids)}" + format_suggestion(s), s)
                return False
        return True


def validate_data_document(doc: object, file: str, schemas: SchemaIndex, *, is_pack: bool,
                           context: Optional[DataContext] = None) -> List[Finding]:
    """Findings for one data document. Without a context, refs and map keys are not resolved."""
    if not isinstance(doc, dict):
        return []
    entries = schemas.entries_for(doc.get("format"))
    if not schemas.has_format(doc.get("format")):
        return []
    w = _Walker(file, entries, is_pack, context)
    w.walk(doc, "", "", None)
    if is_pack:
        w.out.extend(_pack_constraint_findings(doc, file, entries, schemas.constraints_for(doc.get("format"))))
    return w.out


def _pack_constraint_findings(doc: dict, file: str, entries: Dict[str, dict],
                              constraints: List[dict]) -> List[Finding]:
    """Constraints on the pack merged over the schema defaults; only those a pack value takes part in."""
    pack = _flatten(doc)
    merged = {**_default_values(entries.values()), **pack}
    out: List[Finding] = []
    for c in constraints:
        if (c["a"] in pack or c["b"] in pack) and _violated(c, merged):
            out.append(Finding("FAIL", "data.constraint-violated", file, c["a"], _constraint_message(c, merged),
                               None, c["a"] if c["a"] in pack else c["b"], None))
    return out


def _entries_of(doc: dict) -> list:
    entries = doc.get("entries")
    return entries if isinstance(entries, list) else []


def validate_schema_references(schema_docs: Iterable[dict], file_by_doc: Mapping[int, str],
                               known_ramps: Iterable[str] = KNOWN_RAMPS) -> List[Finding]:
    """Schema-level checks: affects/related resolve, ui.ramp is known.

    file_by_doc maps id(schema_doc) to the file name for findings (missing -> '').
    A reference resolves to an entry key (any kind) in the same `for` format, or as
    '<for>/<key>' to an entry in any given schema.
    """
    docs = [d for d in schema_docs if isinstance(d, dict) and isinstance(d.get("for"), str)]
    keys_by_for: Dict[str, List[str]] = {}
    for d in docs:
        for e in _entries_of(d):
            if isinstance(e, dict) and isinstance(e.get("key"), str):
                keys_by_for.setdefault(d["for"], []).append(e["key"])
    ramps = set(known_ramps)
    out: List[Finding] = []
    for d in docs:
        file = file_by_doc.get(id(d), "")
        own = keys_by_for.get(d["for"], [])
        for e in _entries_of(d):
            if not isinstance(e, dict) or not isinstance(e.get("key"), str):
                continue
            for field in ("affects", "related"):
                refs = e.get(field)
                if not isinstance(refs, list):
                    continue
                for i, r in enumerate(refs):
                    if not isinstance(r, str):
                        continue
                    fmt, sep, k = r.partition("/")
                    if (sep and k in keys_by_for.get(fmt, [])) or (not sep and r in own):
                        continue
                    s = suggest(r, own)
                    out.append(Finding("FAIL", "schema.unresolved-reference", file, e["key"],
                                       f"'{e['key']}' {field}[{i}] '{r}' matches no entry key" + format_suggestion(s),
                                       s, f"{field}[{i}]", None))
            ui = e.get("ui")
            ramp = ui.get("ramp") if isinstance(ui, dict) else None
            if isinstance(ramp, str) and ramp not in ramps:
                out.append(Finding("FAIL", "schema.unknown-ramp", file, e["key"],
                                   f"'{e['key']}' ui.ramp '{ramp}' is not a known ramp ({', '.join(sorted(ramps))})",
                                   None, "ui.ramp", None))
    return out


def validate_schema_defaults(schema_docs: Iterable[dict], file_by_doc: Mapping[int, str],
                             context: Optional[DataContext] = None) -> List[Finding]:
    """Schema-level checks on defaults: object map default keys resolve through keyRef
    (data.unresolved-map-key) and the defaults satisfy the constraints (data.constraint-violated).

    A map default is only checked when its keyRef format has documents in the context.
    """
    out: List[Finding] = []
    for d in schema_docs:
        if not isinstance(d, dict) or not isinstance(d.get("for"), str):
            continue
        file = file_by_doc.get(id(d), "")
        for e in _entries_of(d):
            if not isinstance(e, dict) or e.get("type") != "map" or not isinstance(e.get("default"), dict) \
                    or not isinstance(e.get("key"), str) or context is None:
                continue
            values = context.values_at(e.get("keyRef"))
            if values is None:
                continue
            ids = [x for x in values if isinstance(x, str)]
            for mk in e["default"]:
                if mk in ids:
                    continue
                s = suggest(mk, ids)
                out.append(Finding("FAIL", "data.unresolved-map-key", file, e["key"],
                                   f"'{e['key']}' default key '{mk}' matches no id in {e.get('keyRef')}"
                                   + format_suggestion(s), s, f"default.{mk}", None))
        values = _default_values(_entries_of(d))
        cs = d.get("constraints")
        for i, c in enumerate(cs if isinstance(cs, list) else []):
            if isinstance(c, dict) and isinstance(c.get("a"), str) and isinstance(c.get("b"), str) \
                    and c.get("op") in CONSTRAINT_OPS and _violated(c, values):
                out.append(Finding("FAIL", "data.constraint-violated", file, c["a"], _constraint_message(c, values),
                                   None, f"constraints[{i}]", None))
    return out
