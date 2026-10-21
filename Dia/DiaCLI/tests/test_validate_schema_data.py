"""Tests for dia validate schema: data-file rules (shared fixtures under Conformance/data)."""
from __future__ import annotations

import json

import pytest

from dia_cli.commands.validate.data_rules import build_schema_index, validate_data_document
from dia_cli.utils.repo_root import find_repo_root

DATA_DIR = find_repo_root(__file__) / "Dia" / "DiaValueSchema" / "Testing" / "Conformance" / "data"
CASES = sorted(DATA_DIR.glob("*.case.json"))


def _norm_expected(x: dict) -> tuple:
    return (x["severity"], x["rule"], x["file"], x["key"], x["path"], x["record"], x["suggestion"])


def _norm_actual(f) -> tuple:
    return (f.severity, f.rule, f.file, f.key, f.path, f.record, f.suggestion)


def test_fixture_cases_exist():
    assert len(CASES) >= 13


@pytest.mark.parametrize("case_file", CASES, ids=[c.name for c in CASES])
def test_case(case_file):
    case = json.loads(case_file.read_text(encoding="utf-8"))
    index = build_schema_index(case["schemas"])
    actual = []
    for fl in case["files"]:
        actual += validate_data_document(fl["content"], fl["name"], index, is_pack=fl["pack"])
    expected = case["expected"]
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
