"""Tests for dia validate schema: data-file rules (shared fixtures under Conformance/data)."""
from __future__ import annotations

import json

import pytest

from dia_cli.commands.validate.data_rules import (
    KNOWN_RAMPS, DataContext, build_schema_index, validate_data_document, validate_schema_defaults,
    validate_schema_references)
from dia_cli.utils.repo_root import find_repo_root

DATA_DIR = find_repo_root(__file__) / "Dia" / "DiaValueSchema" / "Testing" / "Conformance" / "data"
CASES = sorted(DATA_DIR.glob("*.case.json"))


def _norm_expected(x: dict) -> tuple:
    return (x["severity"], x["rule"], x["file"], x["key"], x["path"], x["record"], x["suggestion"])


def _norm_actual(f) -> tuple:
    return (f.severity, f.rule, f.file, f.key, f.path, f.record, f.suggestion)


def test_fixture_cases_exist():
    assert len(CASES) >= 30


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
    actual += validate_schema_defaults(case["schemas"], names, ctx)
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


# ---- constraints and map defaults (parameter files, plan task 6) -------------

def _cons(*cs):
    return [{"a": a, "op": op, "b": b} for a, op, b in cs]


def _sch(*entries, constraints=None):
    d = _schema(*entries, for_="f.x")
    if constraints is not None:
        d["constraints"] = _cons(*constraints)
    return d


def _pack(schema, content):
    idx = build_schema_index([schema])
    return validate_data_document(dict({"format": "f.x", "version": 1}, **content), "p.json", idx, is_pack=True)


def _defaults(schema, files=()):
    ctx = DataContext.from_files(files)
    return validate_schema_defaults([schema], {id(schema): "s.json"}, ctx)


@pytest.mark.parametrize("op,a,b,violated", [
    ("<", 1, 2, False), ("<", 2, 2, True), ("<=", 2, 2, False), ("<=", 3, 2, True),
    (">", 2, 1, False), (">", 2, 2, True), (">=", 2, 2, False), (">=", 1, 2, True),
    ("==", 2, 2, False), ("==", 1, 2, True), ("!=", 1, 2, False), ("!=", 2, 2, True),
])
def test_constraint_ops_on_defaults(op, a, b, violated):
    s = _sch(_e("a", default=a), _e("b", default=b), constraints=[("a", op, "b")])
    out = _defaults(s)
    assert [f.rule for f in out] == (["data.constraint-violated"] if violated else [])
    if violated:
        assert out[0].key == "a" and "'a'" in out[0].message and "'b'" in out[0].message


def test_constraint_with_missing_default_operand_is_not_checked():
    assert _defaults(_sch(_e("a", default=5), _e("b"), constraints=[("a", "<", "b")])) == []


def test_pack_checks_merge_of_defaults_and_pack():
    s = _sch(_e("a", default=1), _e("b", default=3), constraints=[("a", "<=", "b")])
    assert _pack(s, {"b": 2}) == []
    out = _pack(s, {"b": 0.5})
    assert [(f.rule, f.key) for f in out] == [("data.constraint-violated", "a")]
    assert "'b'" in out[0].message
    out = _pack(s, {"a": 9})
    assert [f.rule for f in out] == ["data.constraint-violated"]


def test_pack_without_either_operand_is_not_blamed_for_default_breakage():
    s = _sch(_e("a", default=5), _e("b", default=3), _e("c", default=1), constraints=[("a", "<=", "b")])
    assert _pack(s, {"c": 2}) == []


def test_pack_both_operands_from_pack_without_defaults():
    s = _sch(_e("a"), _e("b"), constraints=[("a", "<", "b")])
    assert [f.rule for f in _pack(s, {"a": 2, "b": 1})] == ["data.constraint-violated"]
    assert _pack(s, {"a": 2}) == []


def test_pack_flat_dotted_keys_and_map_columns():
    # Packs use flat keys (2026-11-20 user decision); a map is an object keyed by map key.
    s = _sch(_e("g.lo", default=1), _e("m", "map", keyRef="f.k/ids", valueType="float", default={"X": 5}),
             constraints=[("g.lo", "<", "m.X")])
    assert _pack(s, {"g.lo": 2}) == []
    assert [f.rule for f in _pack(s, {"g.lo": 9})] == ["data.constraint-violated"]
    assert [f.rule for f in _pack(s, {"m": {"X": 0}})] == ["data.constraint-violated"]


def test_pack_nested_group_object_is_rejected_naming_the_flat_keys():
    s = _sch(_e("g.lo", default=1), _e("g.hi", default=3), _e("m", "map", keyRef="f.k/ids", valueType="float", default={"X": 5}),
             constraints=[("g.lo", "<", "m.X")])
    out = _pack(s, {"g": {"lo": 2, "hi": 4}, "m": {"X": 6}})
    assert [(f.rule, f.severity, f.key, f.path, f.record) for f in out] == [("data.nested-group-pack", "FAIL", "g", "g", None)]
    assert "'g.lo'" in out[0].message and "'g.hi'" in out[0].message


def test_pack_nested_group_depth_reports_once_and_a_single_leaf_names_the_key():
    s = _sch(_e("a.b.c"))
    out = _pack(s, {"a": {"b": {"c": 1}}})
    assert [(f.rule, f.key) for f in out] == [("data.nested-group-pack", "a")]
    assert "'a.b.c'" in out[0].message
    assert [f.rule for f in _pack(s, {"a": {}})] == ["data.nested-group-pack"]


def test_pack_nested_group_values_do_not_take_part_in_constraints():
    # ApplyPack drops the nested form, so it must not violate a constraint either.
    s = _sch(_e("g.lo", default=1), _e("g.hi", default=3), constraints=[("g.lo", "<", "g.hi")])
    assert [f.rule for f in _pack(s, {"g": {"lo": 9}})] == ["data.nested-group-pack"]


def test_nested_group_object_stays_valid_in_a_defaults_file_and_inside_pack_records():
    s = _sch(_e("g.lo", default=1), _e("sp[].id", "string"), _e("sp[].growth.rate"))
    idx = build_schema_index([s])
    assert validate_data_document({"format": "f.x", "version": 1, "g": {"lo": 1}}, "d.json", idx, is_pack=False) == []
    doc = {"format": "f.x", "version": 1, "sp": [{"id": "a", "growth": {"rate": 1}}]}
    assert validate_data_document(doc, "p.json", idx, is_pack=True) == []


def test_pack_non_numeric_values_are_ignored_by_constraints():
    s = _sch(_e("a", default=1), _e("b", default=3), constraints=[("a", "<=", "b")])
    assert [f.rule for f in _pack(s, {"b": "x"})] == ["data.wrong-type"]
    assert [f.rule for f in _pack(s, {"b": True})] == ["data.wrong-type"]


def test_map_default_unresolved_key_with_suggestion_and_resolved_ok():
    s = _sch(_e("m", "map", keyRef="f.k/ids[]", valueType="float", default={"Desert": 1, "Tundr": 2}))
    files = [("k.json", {"format": "f.k", "ids": ["Desert", "Tundra"]}, False)]
    out = _defaults(s, files)
    assert [(f.rule, f.file, f.key, f.path, f.suggestion) for f in out if f.rule == "data.unresolved-map-key"] ==         [("data.unresolved-map-key", "s.json", "m", "default.Tundr", "Tundra")]
    full = _sch(_e("m", "map", keyRef="f.k/ids[]", valueType="float", default={"Desert": 1, "Tundra": 2}))
    assert _defaults(full, files) == []


def test_map_default_omitting_a_keyref_key_is_reported_once_naming_every_missing_key():
    s = _sch(_e("m", "map", keyRef="f.k/ids[]", valueType="float", default={"Desert": 1}))
    files = [("k.json", {"format": "f.k", "ids": ["Desert", "Tundra", "Forest"]}, False)]
    out = _defaults(s, files)
    assert [(f.rule, f.severity, f.file, f.key, f.path, f.suggestion) for f in out] == [
        ("data.incomplete-map", "FAIL", "s.json", "m", "default", None)]
    assert "m" in out[0].message and "Tundra, Forest" in out[0].message


def test_map_default_missing_key_is_silent_without_keyref_data():
    s = _sch(_e("m", "map", keyRef="f.k/ids[]", valueType="float", default={"Desert": 1}))
    assert _defaults(s) == []
    assert _defaults(s, [("o.json", {"format": "f.other", "ids": ["A"]}, False)]) == []


def test_map_default_missing_key_not_reported_for_scalar_or_absent_default():
    files = [("k.json", {"format": "f.k", "ids": ["Desert", "Tundra"]}, False)]
    assert _defaults(_sch(_e("m", "map", keyRef="f.k/ids[]", valueType="float", default=0.5)), files) == []
    assert _defaults(_sch(_e("m", "map", keyRef="f.k/ids[]", valueType="float")), files) == []


def test_map_default_reports_both_unresolved_and_missing_keys():
    s = _sch(_e("m", "map", keyRef="f.k/ids[]", valueType="float", default={"Desert": 1, "Tundr": 2}))
    files = [("k.json", {"format": "f.k", "ids": ["Desert", "Tundra"]}, False)]
    assert sorted((f.rule, f.path) for f in _defaults(s, files)) == [
        ("data.incomplete-map", "default"), ("data.unresolved-map-key", "default.Tundr")]


def test_map_default_keys_not_checked_without_key_data_and_scalar_defaults_ignored():
    s = _sch(_e("m", "map", keyRef="f.k/ids[]", valueType="float", default={"Z": 1}))
    assert _defaults(s) == []
    assert _defaults(_sch(_e("m", "map", keyRef="f.k/ids[]", valueType="float", default=1)),
                     [("k.json", {"format": "f.k", "ids": ["A"]}, False)]) == []


def test_default_mismatch_only_for_field_entries():
    idx = build_schema_index([_sch(_e("p", default=1), _e("f", kind="field", default=1))])
    out = validate_data_document({"format": "f.x", "version": 1, "p": 2, "f": 2}, "d.json", idx, is_pack=False)
    assert [(f.rule, f.key) for f in out] == [("data.default-mismatch", "f")]


def test_parameter_file_to_json_diff_fixture_is_a_clean_pack():
    # Output of the C++ ParameterFile::ToJson(Diff) (compared in TestDiaAttributeParameterFile.cpp): the CLI must accept it.
    case = json.loads((DATA_DIR / "parameter-file-to-json-diff.case.json").read_text(encoding="utf-8"))
    packs = [fl for fl in case["files"] if fl["pack"]]
    assert len(packs) == 1
    content = packs[0]["content"]
    assert content["format"] == case["schemas"][0]["for"]
    assert content["version"] == 1
    assert isinstance(content["basedOn"], str) and len(content["basedOn"]) == 16
    index = build_schema_index(case["schemas"])
    ctx = DataContext.from_files([(fl["name"], fl["content"], fl["pack"]) for fl in case["files"]])
    findings = validate_data_document(content, packs[0]["name"], index, is_pack=True, context=ctx)
    assert [(f.severity, f.rule, f.key) for f in findings] == []
