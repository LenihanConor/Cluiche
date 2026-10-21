"""dia validate schema: pure data-file rules (coverage, type, range, enum, defaults).

No file IO. Shared fixtures under Dia/DiaValueSchema/Testing/Conformance/data keep this
and T-2's TypeScript checks in agreement. Refs are only type-checked here.
"""
from __future__ import annotations

from typing import Dict, Iterable, List, Optional

from dia_cli.commands.validate.schema_cmd import Finding
from dia_cli.commands.validate.schema_suggest import format_suggestion, suggest

ENVELOPE_KEYS = ("format", "version")
DATA_KINDS = ("parameter", "field")


class SchemaIndex:
    """Parameter/field entries per data-file format, keyed by full (record-path) key."""

    def __init__(self) -> None:
        self._by_format: Dict[str, Dict[str, dict]] = {}

    def add(self, doc: dict) -> None:
        table = self._by_format.setdefault(doc["for"], {})
        for e in doc.get("entries", []):
            if isinstance(e, dict) and e.get("kind") in DATA_KINDS and isinstance(e.get("key"), str):
                table.setdefault(e["key"], e)

    def has_format(self, fmt: object) -> bool:
        return isinstance(fmt, str) and fmt in self._by_format

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
    if t == "float":
        return _is_number(v)
    if t == "int":
        return _is_number(v) and (isinstance(v, int) or v.is_integer())
    if t == "bool":
        return isinstance(v, bool)
    if t in ("string", "enum", "ref"):
        return isinstance(v, str)
    if t == "ref[]":
        return isinstance(v, list) and all(isinstance(x, str) for x in v)
    return True


class _Walker:
    def __init__(self, file: str, entries: Dict[str, dict], is_pack: bool) -> None:
        self.file = file
        self.entries = entries
        self.is_pack = is_pack
        self.known = sorted(entries)
        self.out: List[Finding] = []

    def _add(self, rule: str, key: str, path: str, record: Optional[str], msg: str,
             suggestion: Optional[str] = None) -> None:
        self.out.append(Finding("FAIL", rule, self.file, key, msg, suggestion, path, record))

    def walk(self, node: dict, key_prefix: str, path_prefix: str, record: Optional[str]) -> None:
        for name, val in node.items():
            if not key_prefix and not path_prefix and name in ENVELOPE_KEYS:
                continue
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
                continue  # map values are checked by a later rule
            elif isinstance(val, list) and val and all(isinstance(x, dict) for x in val) \
                    and (entry is None or entry["type"] != "ref[]"):
                for i, rec in enumerate(val):
                    rid = rec.get("id") if isinstance(rec.get("id"), str) else None
                    self.walk(rec, key + "[].", f"{path}[{i}].", rid if rid is not None else record)
            elif isinstance(val, list) and not val and entry is None:
                continue
            else:
                self.leaf(key, path, record, val, entry)

    def leaf(self, key: str, path: str, record: Optional[str], val: object, entry: Optional[dict]) -> None:
        if entry is None:
            self._add("data.no-schema-entry", key, path, record,
                      f"key '{key}' has no schema entry" + format_suggestion(suggest(key, self.known)),
                      suggest(key, self.known))
            return
        t = entry["type"]
        if t == "map":
            return
        if not _type_ok(t, val):
            self._add("data.wrong-type", key, path, record,
                      f"'{key}' = {_fmt(val) if not isinstance(val, list) else val} is not a valid {t}")
            return
        if t in ("float", "int"):
            lo, hi = entry.get("min"), entry.get("max")
            if (lo is not None and val < lo) or (hi is not None and val > hi):
                lo_s = _fmt(lo) if lo is not None else "-inf"
                hi_s = _fmt(hi) if hi is not None else "inf"
                self._add("data.out-of-range", key, path, record,
                          f"'{key}' = {_fmt(val)} outside [{lo_s}, {hi_s}]")
                return
        elif t == "enum":
            ids = [v["id"] for v in entry.get("values", []) if isinstance(v, dict) and "id" in v]
            if val not in ids:
                s = suggest(val, ids)
                self._add("data.bad-enum", key, path, record,
                          f"'{key}' = {_fmt(val)} is not one of {', '.join(ids)}" + format_suggestion(s), s)
                return
        if not self.is_pack and "default" in entry and not _same(entry["default"], val):
            self._add("data.default-mismatch", key, path, record,
                      f"'{key}' = {_fmt(val)} differs from schema default {_fmt(entry['default'])}")


def validate_data_document(doc: object, file: str, schemas: SchemaIndex, *, is_pack: bool) -> List[Finding]:
    if not isinstance(doc, dict):
        return []
    entries = schemas.entries_for(doc.get("format"))
    if not schemas.has_format(doc.get("format")):
        return []
    w = _Walker(file, entries, is_pack)
    w.walk(doc, "", "", None)
    return w.out
