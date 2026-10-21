"""Tests for dia validate schema: data-file rules (shared fixtures under Conformance/data)."""
from __future__ import annotations

import json

import pytest

from dia_cli.commands.validate.data_rules import (
    KNOWN_RAMPS, DataContext, build_schema_index, validate_data_document, validate_schema_references)
from dia_cli.utils.repo_root import find_repo_root

DATA_DIR = find_repo_root(__file__) / "Dia" / "DiaValueSchema" / "Testing" / "Conformance" / "data"
CASES = sorted(DATA_DIR.glob("*.case.json"))


def _norm_expected(x: dict) -> tuple:
    return (x["severity"], x["rule"], x["file"], x["key"], x["path"], x["record"], x["suggestion"])


def _norm_actual(f) -> tuple:
    return (f.severity, f.rule, f.file, f.key, f.path, f.record, f.suggestion)


def test_fixture_cases_exist():
    assert len(CASES) >= 24


@pytest.mark.parametrize("case_file", CASES, ids=[c.name for c in CASES])
def test_case(case_file):
    case = json.loads(case_file.read_text(encoding="utf-8"))
    index = build_schema_index(case["schemas"])
    ctx = DataContext.from_files([(fl["name"], fl["content"], fl["pack"]) for fl in case["files"]])
    actual = []
    for fl in case["files"]:
        actual += validate_data_document(fl["content"], fl["name"], index, is_pack=fl["pack"], context=ctx)
    names = {id(s): f"schemas[{i}]" for i, s in enumerate(case["schemas"])}
    actual += validate_schema_references(case["schemas"], names)
    expected = case["expected"] + case.get("schemaExpected", [])
    assert len(actual) == len(expected), [(_norm_actual(a), a.message) for a in actual]
    remaining = list(actual)
    for exp in expected:
        match = next((a for a in remaining
                      if _norm_actual(a) == _norm_expected(exp) and exp["messageContains"] in a.message), None)
        assert match is not None, (exp, [(_norm_actual(a), a.message) for a in remaining])
        remaining.remove(match)


# ---- unit tests for helpers -------------------------------------------------

def _schema(*entries, for_="f.x"):
    return {"format": "dia.value_schema", "version": 1, "for": for_, "entries": list(entries)}


def _e(key, type="float", kind="parameter", **kw):
    d = {"key": key, "kind": kind, "label": key, "description": "d", "type": type}
    d.update(kw)
    return d


def test_index_groups_by_format_and_ignores_other_kinds():
    idx = build_schema_index([_schema(_e("a"), _e("m", kind="metric"), _e("c", kind="field")),
                              _schema(_e("z"), for_="g.y")])
    assert set(idx.entries_for("f.x")) == {"a", "c"}
    assert set(idx.entries_for("g.y")) == {"z"}
    assert idx.entries_for("nope") == {}


def test_unknown_format_yields_no_findings():
    idx = build_schema_index([_schema(_e("a"))])
    assert validate_data_document({"format": "other", "version": 1, "q": 1}, "d.json", idx, is_pack=False) == []


def test_bool_is_not_a_number_and_int_must_be_integral():
    idx = build_schema_index([_schema(_e("a"), _e("n", "int"))])
    out = validate_data_document({"format": "f.x", "a": True, "n": 2.0}, "d.json", idx, is_pack=True)
    assert [(f.rule, f.key) for f in out] == [("data.wrong-type", "a")]


def test_range_bounds_inclusive_and_optional():
    idx = build_schema_index([_schema(_e("a", min=0, max=2), _e("b", min=5))])
    ok = validate_data_document({"format": "f.x", "a": 2, "b": 1000}, "d.json", idx, is_pack=True)
    assert ok == []
    bad = validate_data_document({"format": "f.x", "a": 0, "b": 4}, "d.json", idx, is_pack=True)
    assert [f.key for f in bad] == ["b"]


def test_ref_types_only_type_checked():
    idx = build_schema_index([_schema(_e("r", "ref", ref="x"), _e("rs", "ref[]", ref="x"))])
    ok = validate_data_document({"format": "f.x", "r": "any", "rs": ["a", "b"]}, "d.json", idx, is_pack=False)
    assert ok == []
    bad = validate_data_document({"format": "f.x", "r": 3, "rs": ["a", 1]}, "d.json", idx, is_pack=False)
    assert sorted(f.key for f in bad) == ["r", "rs"]
    assert all(f.rule == "data.wrong-type" for f in bad)


def test_missing_default_key_not_reported_in_defaults_file():
    idx = build_schema_index([_schema(_e("a", default=1))])
    assert validate_data_document({"format": "f.x", "version": 1}, "d.json", idx, is_pack=False) == []


# ---- cross-record / cross-format rules --------------------------------------

def _ctx(*docs):
    return DataContext.from_files([(f"f{i}.json", d, False) for i, d in enumerate(docs)])


def test_ref_without_context_is_type_checked_only():
    idx = build_schema_index([_schema(_e("r", "ref", ref="x/y[].id"))])
    assert validate_data_document({"format": "f.x", "r": "anything"}, "d.json", idx, is_pack=False) == []


def test_ref_resolves_in_pack_of_referenced_format():
    idx = build_schema_index([_schema(_e("r", "ref", ref="g.y/items[].id"))])
    defaults = {"format": "g.y", "items": [{"id": "a"}]}
    pack = {"format": "g.y", "items": [{"id": "b"}]}
    ctx = DataContext.from_files([("d.json", defaults, False), ("Packs/p.json", pack, True)])
    doc = {"format": "f.x", "r": "b"}
    assert validate_data_document(doc, "x.json", idx, is_pack=False, context=ctx) == []


def test_ref_format_missing_is_one_warn_per_file_and_format():
    idx = build_schema_index([_schema(_e("r", "ref", ref="g.y/items[].id"), _e("s", "ref", ref="g.y/items[].id"))])
    out = validate_data_document({"format": "f.x", "r": "a", "s": "b"}, "x.json", idx, is_pack=False,
                                 context=_ctx({"format": "other"}))
    assert [(f.severity, f.rule) for f in out] == [("WARN", "data.ref-format-missing")]


def test_unique_ignores_missing_values_and_other_arrays():
    idx = build_schema_index([_schema(_e("a[].id", "string", unique=True))])
    doc = {"format": "f.x", "a": [{"id": "p"}, {}, {"id": "q"}, {}]}
    assert validate_data_document(doc, "d.json", idx, is_pack=False) == []


def test_required_applies_in_packs_too():
    idx = build_schema_index([_schema(_e("a[].id", "string"), _e("a[].v", "float", required=True))])
    doc = {"format": "f.x", "a": [{"id": "p"}]}
    out = validate_data_document(doc, "d.json", idx, is_pack=True)
    assert [(f.rule, f.path, f.record) for f in out] == [("data.missing-required", "a[0].v", "p")]


def test_map_must_be_an_object():
    idx = build_schema_index([_schema(_e("m", "map", keyRef="g.y/items[].id", valueType="float"))])
    out = validate_data_document({"format": "f.x", "m": 3}, "d.json", idx, is_pack=False,
                                 context=_ctx({"format": "g.y", "items": [{"id": "a"}]}))
    assert [f.rule for f in out] == ["data.wrong-type"]


def test_map_enum_and_bool_values():
    idx = build_schema_index([_schema(
        _e("m", "map", keyRef="g.y/items[].id", valueType="enum", values=[{"id": "hi"}, {"id": "lo"}]),
        _e("b", "map", keyRef="g.y/items[].id", valueType="bool"))])
    ctx = _ctx({"format": "g.y", "items": [{"id": "a"}]})
    out = validate_data_document({"format": "f.x", "m": {"a": "mid"}, "b": {"a": 1}}, "d.json", idx,
                                 is_pack=False, context=ctx)
    assert sorted(f.rule for f in out) == ["data.bad-enum", "data.wrong-type"]


def test_schema_references_resolve_within_for_format_or_full_key():
    docs = [_schema(_e("a", affects=["b"], related=["g.y/z", "g.y/q"]), _e("b"), for_="f.x"),
            _schema(_e("z"), for_="g.y")]
    out = validate_schema_references(docs, {})
    assert [(f.rule, f.key, f.path) for f in out] == [("schema.unresolved-reference", "a", "related[1]")]


def test_schema_unknown_ramp_and_custom_known_set():
    docs = [_schema(_e("a", ui={"ramp": "mine"}))]
    assert [f.rule for f in validate_schema_references(docs, {})] == ["schema.unknown-ramp"]
    assert validate_schema_references(docs, {}, known_ramps={"mine"}) == []
    assert "categorical.12" in KNOWN_RAMPS
