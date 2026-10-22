"""Tests for dia validate schema: schema-file rules (mirror of ValueSchemaLoader)."""
from __future__ import annotations

import copy
import json
from pathlib import Path

import pytest

from dia_cli.commands.validate.schema_rules import (
    validate_schema_document,
    validate_schema_text,
)
from dia_cli.utils.repo_root import find_repo_root

CONF_DIR = find_repo_root(__file__) / "Dia" / "DiaValueSchema" / "Testing" / "Conformance"


def _entry(**over):
    e = {"key": "k", "kind": "parameter", "label": "L", "description": "D", "type": "float"}
    e.update(over)
    return e


def _doc(*entries, **over):
    d = {"format": "dia.value_schema", "version": 1, "for": "x", "entries": list(entries)}
    d.update(over)
    return d


def _fails(doc):
    return [f for f in validate_schema_document(doc, "f.json") if f.severity == "FAIL"]


def _warns(doc):
    return [f for f in validate_schema_document(doc, "f.json") if f.severity == "WARN"]


# ---- (a) conformance --------------------------------------------------------

def _expected():
    return json.loads((CONF_DIR / "expected.json").read_text(encoding="utf-8"))["cases"]


def test_conformance_files_and_cases_match():
    files = {p.name for p in CONF_DIR.glob("*.schema.json")}
    assert files == set(_expected().keys())


@pytest.mark.parametrize("name", sorted(_expected().keys()))
def test_conformance_case(name):
    case = _expected()[name]
    text = (CONF_DIR / name).read_text(encoding="utf-8")
    findings = validate_schema_text(text, name)
    fails = [f for f in findings if f.severity == "FAIL"]
    warns = [f for f in findings if f.severity == "WARN"]
    assert (not fails) == case["ok"], [f.message for f in fails]
    if case["ok"]:
        assert len(warns) == case.get("warnings", 0)
    else:
        assert fails[0].key == case["failedEntry"]
        assert case["reasonContains"] in fails[0].message
        assert fails[0].file == name
        assert fails[0].rule.startswith("schema.")


# ---- (b) unit tests ---------------------------------------------------------

def test_good_minimal():
    assert validate_schema_document(_doc(_entry()), "f") == []


def test_root_not_object():
    f = _fails([])
    assert len(f) == 1 and f[0].rule == "schema.root-not-object"


def test_unparsable_text():
    f = validate_schema_text("{nope", "f.json")
    assert len(f) == 1 and f[0].severity == "FAIL" and f[0].rule == "schema.unparsable-json"
    assert "unparsable" in f[0].message


def test_bad_format_and_version():
    assert _fails(_doc(format="x"))[0].rule == "schema.bad-format"
    assert _fails(_doc(version="1"))[0].rule == "schema.bad-version"
    assert _fails(_doc(version=True))[0].rule == "schema.bad-version"
    assert _fails(_doc(version=2))[0].rule == "schema.bad-version"


def test_missing_for_and_entries():
    d = _doc(_entry())
    del d["for"]
    assert _fails(d)[0].rule == "schema.missing-for"
    d = _doc(_entry())
    d["entries"] = {}
    assert _fails(d)[0].rule == "schema.missing-entries"


def test_unknown_file_key():
    f = _fails(_doc(_entry(), extra=1))
    assert f[0].rule == "schema.unknown-file-key" and "extra" in f[0].message and f[0].key == ""


@pytest.mark.parametrize("field", ["key", "kind", "label", "description", "type"])
def test_missing_required(field):
    e = _entry()
    del e[field]
    f = _fails(_doc(e))
    assert f[0].rule == "schema.missing-key" and field in f[0].message
    assert f[0].key == ("" if field == "key" else "k")


@pytest.mark.parametrize("field,val", [("kind", 1), ("label", 2), ("unit", 3), ("hint", 1), ("source", 1),
                                       ("min", "a"), ("max", True), ("unique", "y"), ("required", 1),
                                       ("ui", []), ("history", 1), ("affects", "a"), ("related", [1])])
def test_wrong_json_type(field, val):
    f = _fails(_doc(_entry(**{field: val})))
    assert f and f[0].rule == "schema.wrong-type" and field in f[0].message


def test_unknown_kind_type_entry_key():
    assert _fails(_doc(_entry(kind="zzz")))[0].rule == "schema.unknown-kind"
    assert _fails(_doc(_entry(type="zzz")))[0].rule == "schema.unknown-type"
    f = _fails(_doc(_entry(bogus=1)))
    assert f[0].rule == "schema.unknown-entry-key" and "bogus" in f[0].message


@pytest.mark.parametrize("kind", ["parameter", "field", "metric", "arg", "command", "event", "component_field"])
def test_all_kinds_accepted(kind):
    assert _fails(_doc(_entry(kind=kind))) == []


def test_enum_requires_values():
    f = _fails(_doc(_entry(type="enum")))
    assert f[0].rule == "schema.enum-no-values" and "values" in f[0].message
    assert _fails(_doc(_entry(type="enum", values=[{"id": "a"}]))) == []


def test_enum_values_shape():
    assert _fails(_doc(_entry(type="enum", values=["a"])))[0].rule == "schema.wrong-type"
    assert _fails(_doc(_entry(type="enum", values=[{"id": 1}])))[0].rule == "schema.wrong-type"


def test_empty_or_blank_description_fails():
    for d in ("", "   "):
        f = _fails(_doc(_entry(description=d)))
        assert f[0].rule == "schema.empty-description" and f[0].key == "k"


def test_payload_and_vocabulary_only_on_events():
    assert _fails(_doc(_entry(payload=[])))[0].rule == "schema.event-only-field"
    assert _fails(_doc(_entry(vocabulary=[])))[0].rule == "schema.event-only-field"
    ev = _entry(kind="event", payload=[{"name": "a", "type": "int", "description": "d"}],
                vocabulary=[{"id": "v", "description": "d"}])
    assert _fails(_doc(ev)) == []


def test_payload_and_vocabulary_items():
    ev = _entry(kind="event", payload=[{"type": "int", "description": "d"}])
    f = _fails(_doc(ev))
    assert f[0].rule == "schema.missing-key" and "name" in f[0].message
    ev = _entry(kind="event", payload=[{"name": "a", "type": "zz", "description": "d"}])
    assert _fails(_doc(ev))[0].rule == "schema.unknown-type"
    ev = _entry(kind="event", vocabulary=[{"description": "d"}])
    f = _fails(_doc(ev))
    assert f[0].rule == "schema.missing-key" and "id" in f[0].message


def test_ref_rules():
    assert "ref" in _fails(_doc(_entry(type="ref")))[0].message
    assert "ref" in _fails(_doc(_entry(type="ref[]")))[0].message
    assert _fails(_doc(_entry(type="ref", ref="a/b"))) == []
    f = _fails(_doc(_entry(ref="a")))
    assert f[0].rule == "schema.ref-misuse"


def test_map_rules():
    ok = _entry(type="map", keyRef="a/b", valueType="float")
    assert _fails(_doc(ok)) == []
    assert "keyRef" in _fails(_doc(_entry(type="map", valueType="float")))[0].message
    assert "valueType" in _fails(_doc(_entry(type="map", keyRef="a/b")))[0].message
    for bad in ("map", "ref", "ref[]", "zzz"):
        f = _fails(_doc(_entry(type="map", keyRef="a/b", valueType=bad)))
        assert f[0].rule == "schema.map-misuse" and "valueType" in f[0].message
    assert "keyRef" in _fails(_doc(_entry(keyRef="a/b")))[0].message
    assert "valueType" in _fails(_doc(_entry(valueType="float")))[0].message


def test_duplicate_full_key():
    f = _fails(_doc(_entry(), _entry()))
    assert len(f) == 1 and f[0].rule == "schema.duplicate-key"
    assert f[0].key == "k" and "x/k" in f[0].message


def test_later_entry_failure_reports_that_entry():
    f = _fails(_doc(_entry(), _entry(key="second", label=1)))
    assert f[0].key == "second"


def test_unknown_ui_history_keys_warn_once_per_block_key():
    d = _doc(_entry(key="a", ui={"zz": 1, "group": "g"}, history={"qq": 1}),
             _entry(key="b", ui={"zz": 2}, history={"qq": 2}))
    assert _fails(d) == []
    w = _warns(d)
    assert len(w) == 2 and all(x.rule == "schema.unknown-block-key" for x in w)
    assert all(x.severity == "WARN" for x in w)


def test_known_ui_history_keys_no_warning():
    ui = {k: 1 for k in "group step decimals ramp layer hidden format colour glyph hotkey delta_ramp".split()}
    d = _doc(_entry(ui=ui, history={"track": True, "group": "g"}))
    assert validate_schema_document(d, "f") == []


def test_does_not_mutate_input():
    d = _doc(_entry())
    before = copy.deepcopy(d)
    validate_schema_document(d, "f")
    assert d == before
