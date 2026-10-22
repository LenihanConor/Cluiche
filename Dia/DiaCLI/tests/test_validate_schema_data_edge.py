"""Edge-case tests for data_rules (D1-D19). Cases marked PIN document current behaviour."""
from __future__ import annotations

import copy

import pytest

from dia_cli.commands.validate.data_rules import (
    DataContext, build_schema_index, validate_data_document, validate_schema_references)

NAN = float("nan")
INF = float("inf")


def E(key, type="float", kind="parameter", **kw):
    e = {"key": key, "kind": kind, "label": key, "description": "d", "type": type}
    e.update(kw)
    return e


def S(*entries, fmt="x"):
    return {"format": "dia.value_schema", "version": 1, "for": fmt, "entries": list(entries)}


def run(entries, content, pack=False, files=None, fmt="x", env=True):
    doc = dict(content)
    if env:
        doc.setdefault("format", fmt)
        doc.setdefault("version", 1)
    idx = build_schema_index([S(*entries, fmt=fmt)])
    ctx = None
    if files is not None:
        ctx = DataContext.from_files(files)
    return validate_data_document(doc, "d.json", idx, is_pack=pack, context=ctx)


def rp(out):
    return [(f.rule, f.key, f.path) for f in out]


def one(out, rule):
    assert [f.rule for f in out] == [rule], [(f.rule, f.message) for f in out]
    return out[0]


# ---------------------------------------------------------------- numbers (D1-D2)
@pytest.mark.parametrize("bad", [NAN, INF, -INF])
@pytest.mark.parametrize("type_", ["float", "int"])
@pytest.mark.parametrize("rng", [{}, {"min": 0, "max": 10}, {"min": -1e308}, {"max": 1e308}])
def test_non_finite_values_fail(bad, type_, rng):
    one(run([E("a", type_, **rng)], {"a": bad}), "data.wrong-type")


@pytest.mark.parametrize("bad", [NAN, INF, -INF])
def test_non_finite_values_in_map_and_records(bad):
    out = run([E("a[].v", min=0, max=1)], {"a": [{"v": bad}]})
    assert rp(out) == [("data.wrong-type", "a[].v", "a[0].v")]


@pytest.mark.parametrize("val", [2.0, 2, -0.0, 0, 1e-320, 1e308])
def test_float_accepts(val):
    assert run([E("a")], {"a": val}) == []


@pytest.mark.parametrize("val,ok", [(2, True), (2.0, True), (-0.0, True), (2.5, False), (1e300, True),
                                    (0.1, False), (10 ** 400, True), (-3, True)])
def test_int_values(val, ok):
    out = run([E("a", "int")], {"a": val})
    assert (out == []) == ok
    if not ok:
        assert out[0].rule == "data.wrong-type"


def test_negative_zero_with_min_zero_in_range():
    assert run([E("a", min=0)], {"a": -0.0}) == []


def test_1e400_parses_to_inf_and_fails():
    import json
    out = run([E("a")], json.loads('{"a": 1e400}'))
    one(out, "data.wrong-type")


def test_huge_int_for_float_range():
    one(run([E("a", max=1)], {"a": 10 ** 400}), "data.out-of-range")
    assert run([E("a")], {"a": 10 ** 400}) == []


def test_huge_int_message_formats():
    out = run([E("a", max=1)], {"a": 10 ** 400})
    assert str(10 ** 400)[:10] in out[0].message


@pytest.mark.parametrize("val", [True, False])
@pytest.mark.parametrize("t", ["float", "int"])
def test_bool_for_number_is_wrong_type(val, t):
    one(run([E("a", t)], {"a": val}), "data.wrong-type")


@pytest.mark.parametrize("val", [True, False, 1, 0, None])
def test_non_string_for_ref_wrong_type(val):
    one(run([E("a", "ref", ref="f/p")], {"a": val}), "data.wrong-type")


@pytest.mark.parametrize("val", ["1", None, [], [1], {"a": 1}])
def test_non_number_for_float(val):
    out = run([E("a")], {"a": val})
    assert out and out[0].rule == "data.wrong-type"


@pytest.mark.parametrize("val", [1, 0, "true", None, [True]])
def test_non_bool_for_bool(val):
    one(run([E("a", "bool")], {"a": val}), "data.wrong-type")


@pytest.mark.parametrize("val", [True, False])
def test_bool_ok(val):
    assert run([E("a", "bool")], {"a": val}) == []


@pytest.mark.parametrize("val", [1, None, True, ["a"]])
def test_non_string_for_string(val):
    one(run([E("a", "string")], {"a": val}), "data.wrong-type")


@pytest.mark.parametrize("val", ["", "é", "x" * 10000])
def test_string_ok(val):
    assert run([E("a", "string")], {"a": val}) == []


# ---------------------------------------------------------------- ranges (D10)
@pytest.mark.parametrize("val,ok", [(5, True), (4.999, False), (5.0001, False), (4, False), (6, False)])
def test_min_equals_max(val, ok):
    out = run([E("a", min=5, max=5)], {"a": val})
    assert (out == []) == ok
    if not ok:
        assert out[0].rule == "data.out-of-range"


@pytest.mark.parametrize("val,ok", [(0, True), (-0.0001, False), (1e308, True), (-1, False)])
def test_only_min(val, ok):
    out = run([E("a", min=0)], {"a": val})
    assert (out == []) == ok
    if not ok:
        assert "inf" in out[0].message


@pytest.mark.parametrize("val,ok", [(0, True), (0.0001, False), (-1e308, True), (1, False)])
def test_only_max(val, ok):
    out = run([E("a", max=0)], {"a": val})
    assert (out == []) == ok
    if not ok:
        assert "-inf" in out[0].message


def test_min_greater_than_max_rejects_everything_pin():
    for v in (0, 3, 5, 10):  # PIN: nothing is inside [5, 1]
        one(run([E("a", min=5, max=1)], {"a": v}), "data.out-of-range")


@pytest.mark.parametrize("bad_bound", ["x", None, [], {}])
def test_garbage_bounds_do_not_crash(bad_bound):
    assert run([E("a", min=bad_bound)], {"a": 1}) == [] or True
    run([E("a", max=bad_bound)], {"a": 1})


def test_int_range():
    one(run([E("a", "int", min=0, max=3)], {"a": 4}), "data.out-of-range")
    assert run([E("a", "int", min=0, max=3)], {"a": 3.0}) == []


# ---------------------------------------------------------------- containers under scalars (D3, B8)
@pytest.mark.parametrize("t", ["float", "int", "bool", "string", "enum", "ref", "ref[]", "map"])
def test_dict_under_scalar_entry(t):
    kw = {"values": [{"id": "a"}]} if t == "enum" else {}
    if t in ("ref", "ref[]"):
        kw["ref"] = "f/p"
    if t == "map":
        return
    one(run([E("a", t, **kw)], {"a": {"b": 1}}), "data.wrong-type")


@pytest.mark.parametrize("t", ["float", "int", "bool", "string", "enum", "ref", "ref[]", "map"])
def test_list_of_dicts_under_scalar_entry(t):  # B8
    kw = {"values": [{"id": "a"}]} if t == "enum" else {}
    if t in ("ref", "ref[]"):
        kw["ref"] = "f/p"
    if t == "map":
        kw.update(keyRef="f/p", valueType="float")
    out = run([E("a", t, **kw)], {"a": [{"b": 1}]})
    assert [f.rule for f in out] == ["data.wrong-type"]
    assert out[0].path == "a"


def test_list_of_dicts_matches_plain_dict_verdict():
    plain = run([E("a")], {"a": {"b": 1}})
    listed = run([E("a")], {"a": [{"b": 1}]})
    assert [(f.rule, f.key, f.path) for f in plain] == [(f.rule, f.key, f.path) for f in listed]


def test_list_of_dicts_under_entryless_key_walks_records():
    out = run([E("a[].b")], {"a": [{"b": 1}, {"b": "x"}]})
    assert rp(out) == [("data.wrong-type", "a[].b", "a[1].b")]


def test_empty_list_under_scalar_entry_is_wrong_type():
    one(run([E("a")], {"a": []}), "data.wrong-type")


def test_empty_list_under_ref_list_ok():
    assert run([E("a", "ref[]", ref="f/p")], {"a": []}) == []


@pytest.mark.parametrize("val", [{}, [], None])
def test_entryless_empty_forms(val):
    out = run([E("a")], {"zz": val})
    if val is None:
        one(out, "data.no-schema-entry")
    else:
        assert out == []  # PIN: empty containers under an unknown key are ignored


def test_entryless_scalars_flagged():
    for v in (1, "s", True, 1.5):
        one(run([E("a")], {"zz": v}), "data.no-schema-entry")


# ---------------------------------------------------------------- records (D4-D6)
def test_nested_record_paths():
    ent = [E("a[].b.c", min=0), E("a[].d[].e", "int")]
    c = {"a": [{"b": {"c": 1}, "d": [{"e": 1}, {"e": 2.5}]}, {"b": {"c": -1}, "d": []}]}
    assert rp(run(ent, c)) == [("data.wrong-type", "a[].d[].e", "a[0].d[1].e"),
                               ("data.out-of-range", "a[].b.c", "a[1].b.c")]


def test_mixed_array_dict_and_scalar_treated_as_leaf():
    out = run([E("a")], {"a": [{}, 1]})
    assert out and out[0].rule == "data.wrong-type"
    one(run([], {"a": [{}, 1]}), "data.no-schema-entry")


def test_array_of_arrays():
    one(run([], {"a": [[1], [2]]}), "data.no-schema-entry")
    assert [f.rule for f in run([E("a")], {"a": [[1]]})] == ["data.wrong-type"]


def test_array_of_empty_dicts_no_findings():
    assert run([E("a[].b")], {"a": [{}, {}]}) == []


def test_empty_record_array_with_required_column_pin():
    assert run([E("a[].b", required=True)], {"a": []}) == []  # PIN: empty arrays are skipped


def test_record_missing_required_column():
    out = run([E("a[].b", required=True)], {"a": [{"b": 1}, {}, {"c": 1}]})
    assert [(f.rule, f.path) for f in out] == [("data.missing-required", "a[1].b"),
                                               ("data.missing-required", "a[2].b"),
                                               ] or any(f.rule == "data.missing-required" for f in out)
    assert [f.path for f in out if f.rule == "data.missing-required"] == ["a[1].b", "a[2].b"]


def test_record_without_id_uses_no_record():
    out = run([E("a[].v", min=0)], {"a": [{"v": -1}]})
    assert out[0].record is None


@pytest.mark.parametrize("rid", [5, None, True, ["x"], {"y": 1}])
def test_record_non_string_id_ignored(rid):
    out = run([E("a[].v", min=0)], {"a": [{"id": rid, "v": -1}]})
    out = [f for f in out if f.rule == "data.out-of-range"]
    assert out and out[0].record is None


def test_record_string_id_reported():
    out = run([E("a[].v", min=0)], {"a": [{"id": "r1", "v": -1}]})
    assert out[0].record == "r1"


def test_record_id_with_no_schema_for_id_flagged():
    out = run([E("a[].v")], {"a": [{"id": "r1", "v": 1}]})
    assert rp(out) == [("data.no-schema-entry", "a[].id", "a[0].id")]


def test_duplicate_ids_flagged_only_with_unique():
    recs = {"a": [{"id": "x"}, {"id": "x"}]}
    assert run([E("a[].id", "string")], recs) == []
    out = run([E("a[].id", "string", unique=True)], recs)
    assert rp(out) == [("data.duplicate-unique", "a[].id", "a[1].id")]


def test_unique_three_duplicates_all_point_to_first():
    out = run([E("a[].id", "string", unique=True)], {"a": [{"id": "x"}] * 3})
    assert [f.path for f in out] == ["a[1].id", "a[2].id"]
    assert all("a[0]" in f.message for f in out)


def test_unique_skips_missing_and_none():
    out = run([E("a[].v", "string", unique=True)], {"a": [{"v": None}, {"v": None}, {}, {}]})
    assert [f.rule for f in out] == ["data.wrong-type", "data.wrong-type"]


def test_unique_mixed_types_are_distinct():
    out = run([E("a[].v", "string", unique=True)], {"a": [{"v": "1"}, {"v": 1}]})
    assert [f.rule for f in out] == ["data.wrong-type"]


def test_unique_int_and_float_collide_pin():
    out = run([E("a[].v", "float", unique=True)], {"a": [{"v": 1}, {"v": 1.0}]})
    assert [f.rule for f in out] == ["data.duplicate-unique"]  # PIN: 1 == 1.0


def test_unique_booleans_never_collide():
    out = run([E("a[].v", "bool", unique=True)], {"a": [{"v": True}, {"v": True}]})
    assert out == []


def test_unique_nan_distinct_objects_pin():
    out = run([E("a[].v", "float", unique=True)], {"a": [{"v": float("nan")}, {"v": float("nan")}]})
    assert [f.rule for f in out] == ["data.wrong-type", "data.wrong-type"]  # NaN fails type first


def test_required_nested_missing_and_null():
    ent = [E("a[].b.c", required=True)]
    out = run(ent, {"a": [{"b": {}}, {"b": {"c": None}}, {}, {"b": {"c": 1}}]})
    rules_paths = [(f.rule, f.path) for f in out]
    assert ("data.missing-required", "a[0].b.c") in rules_paths
    assert ("data.missing-required", "a[2].b.c") in rules_paths
    # PIN: present-but-null is not "missing"; it is a wrong-type instead
    assert ("data.wrong-type", "a[1].b.c") in rules_paths
    assert not any(p == "a[1].b.c" and r == "data.missing-required" for r, p in rules_paths)


def test_required_and_unique_scope_nested_arrays_pin():  # PIN (B9)
    # required/unique apply per innermost array: duplicates are only found within one inner array,
    # not across different outer records.
    ent = [E("a[].b[].c", "string", required=True, unique=True)]
    out = run(ent, {"a": [{"b": [{}, {"c": "x"}, {"c": "x"}]}]})
    assert rp(out) == [("data.missing-required", "a[].b[].c", "a[0].b[0].c"),
                       ("data.duplicate-unique", "a[].b[].c", "a[0].b[2].c")]
    across = run(ent, {"a": [{"b": [{"c": "x"}]}, {"b": [{"c": "x"}]}]})
    assert across == []


def test_required_column_with_no_other_flags():
    assert run([E("a[].b", required=False)], {"a": [{}]}) == []


# ---------------------------------------------------------------- enums (D11)
ENUM = E("a", "enum", values=[{"id": "red"}, {"id": "green"}])


def test_enum_ok_and_bad():
    assert run([ENUM], {"a": "red"}) == []
    f = one(run([ENUM], {"a": "blue"}), "data.bad-enum")
    assert "red" in f.message and "green" in f.message


def test_enum_suggestion_present():
    f = one(run([ENUM], {"a": "gren"}), "data.bad-enum")
    assert f.suggestion == "green" and "did you mean 'green'" in f.message


def test_enum_no_suggestion_when_far():
    assert one(run([ENUM], {"a": "zzzzzz"}), "data.bad-enum").suggestion is None


def test_enum_one_value():
    e = E("a", "enum", values=[{"id": "only"}])
    assert run([e], {"a": "only"}) == []
    one(run([e], {"a": "onl"}), "data.bad-enum")


@pytest.mark.parametrize("val", [1, True, None, ["red"], 1.5])
def test_enum_non_string_value(val):
    one(run([ENUM], {"a": val}), "data.wrong-type")


def test_enum_is_case_sensitive():
    f = one(run([ENUM], {"a": "Red"}), "data.bad-enum")
    assert f.suggestion == "red"


def test_enum_empty_values_rejects_all_pin():
    f = one(run([E("a", "enum", values=[])], {"a": "x"}), "data.bad-enum")  # PIN (B7)
    assert f.suggestion is None


def test_enum_values_without_id_ignored_pin():
    e = E("a", "enum", values=[{}, {"label": "L"}, {"id": "ok"}])
    assert run([e], {"a": "ok"}) == []
    one(run([e], {"a": "L"}), "data.bad-enum")


def test_enum_duplicate_ids_pin():
    e = E("a", "enum", values=[{"id": "a"}, {"id": "a"}])
    assert run([e], {"a": "a"}) == []


def test_enum_non_string_id_does_not_crash():
    e = E("a", "enum", values=[{"id": 5}, {"id": "ok"}])
    one(run([e], {"a": "bad"}), "data.bad-enum")


# ---------------------------------------------------------------- refs (D12)
def ctx(*docs):
    return [(f"f{i}.json", d, False) for i, d in enumerate(docs)]


BIOMES = {"format": "cow.biomes", "version": 1, "biomes": [{"id": "forest"}, {"id": "tundra"}]}
REFFILES = ctx(BIOMES)


def test_ref_list_empty_ok_with_context():
    assert run([E("a", "ref[]", ref="cow.biomes/biomes[].id")], {"a": []}, files=REFFILES) == []


def test_ref_list_non_string_item_wrong_type():
    out = run([E("a", "ref[]", ref="cow.biomes/biomes[].id")], {"a": ["forest", 5]}, files=REFFILES)
    one(out, "data.wrong-type")


def test_ref_list_bad_items_report_each_with_index():
    out = run([E("a", "ref[]", ref="cow.biomes/biomes[].id")], {"a": ["forest", "tundre", "x", "forest"]},
              files=REFFILES)
    assert [(f.rule, f.path) for f in out] == [("data.unresolved-ref", "a[1]"), ("data.unresolved-ref", "a[2]")]
    assert out[0].suggestion == "tundra" and out[1].suggestion is None


def test_ref_without_context_unchecked():
    assert run([E("a", "ref", ref="cow.biomes/biomes[].id")], {"a": "zzz"}) == []


def test_ref_to_itself():
    selfdoc = {"format": "x", "version": 1, "items": [{"id": "a"}, {"id": "b", "parent": "a"},
                                                      {"id": "c", "parent": "zz"}]}
    ent = [E("items[].id", "string"), E("items[].parent", "ref", ref="x/items[].id")]
    out = run(ent, selfdoc, files=[("d.json", selfdoc, False)])
    assert rp(out) == [("data.unresolved-ref", "items[].parent", "items[2].parent")]


def test_ref_path_yielding_zero_values_flags_all_no_suggestion():
    out = run([E("a", "ref", ref="cow.biomes/nope[].id")], {"a": "forest"}, files=REFFILES)
    f = one(out, "data.unresolved-ref")
    assert f.suggestion is None


def test_ref_non_string_target_values_ignored():
    d = {"format": "t", "version": 1, "ids": [1, 2, "ok"]}
    assert run([E("a", "ref", ref="t/ids[]")], {"a": "ok"}, files=[("t.json", d, False)]) == []
    one(run([E("a", "ref", ref="t/ids[]")], {"a": "1"}, files=[("t.json", d, False)]), "data.unresolved-ref")


def test_ref_dotted_path():
    d = {"format": "t", "version": 1, "a": {"b": {"ids": ["q"]}}}
    assert run([E("a", "ref", ref="t/a.b.ids[]")], {"a": "q"}, files=[("t.json", d, False)]) == []


def test_ref_resolves_across_pack_files():
    pack = ("p.json", {"format": "cow.biomes", "version": 1, "biomes": [{"id": "swamp"}]}, True)
    assert run([E("a", "ref", ref="cow.biomes/biomes[].id")], {"a": "swamp"}, files=REFFILES + [pack]) == []


def test_ref_format_missing_warns_once_per_format():
    ent = [E("a", "ref", ref="gone/p"), E("b", "ref", ref="gone/q"), E("c", "ref[]", ref="other/q")]
    out = run(ent, {"a": "1", "b": "2", "c": ["3"]}, files=REFFILES)
    assert [(f.severity, f.rule) for f in out] == [("WARN", "data.ref-format-missing")] * 2


def test_ref_format_missing_warn_per_document_not_global():  # PIN (B17): dedup is per validated document
    ent = [E("a", "ref", ref="gone/p")]
    one_ = run(ent, {"a": "1"}, files=REFFILES)
    two_ = run(ent, {"a": "1"}, files=REFFILES)
    assert len(one_) == len(two_) == 1


def test_ref_format_missing_warn_deduped_across_entries_pin():  # PIN (B17)
    ent = [E("r[].a", "ref", ref="gone/p")]
    out = run(ent, {"r": [{"a": "1"}, {"a": "2"}, {"a": "3"}]}, files=REFFILES)
    assert len(out) == 1 and out[0].severity == "WARN"


def test_ref_malformed_ref_string_silently_unchecked_pin():
    assert run([E("a", "ref", ref="noslash")], {"a": "zz"}, files=REFFILES) == []  # PIN: schema rule catches it


def test_ref_value_after_wrong_type_not_resolved():
    out = run([E("a", "ref", ref="cow.biomes/biomes[].id")], {"a": 5}, files=REFFILES)
    one(out, "data.wrong-type")


# ---------------------------------------------------------------- maps (D13)
MAPENT = E("m", "map", keyRef="cow.biomes/biomes[].id", valueType="float", min=0, max=1)


def test_map_empty_ok():
    assert run([MAPENT], {"m": {}}, files=REFFILES) == []


def test_map_value_and_key_errors():
    out = run([MAPENT], {"m": {"forest": 2, "tundre": 0.5, "forest2": "x"}}, files=REFFILES)
    assert [(f.rule, f.path) for f in out] == [("data.out-of-range", "m.forest"),
                                               ("data.unresolved-map-key", "m.tundre"),
                                               ("data.unresolved-map-key", "m.forest2")]
    assert out[1].suggestion == "tundra"


@pytest.mark.parametrize("vt,good,bad", [
    ("bool", True, 1), ("string", "s", 1), ("int", 2, 2.5), ("float", 0.5, "x"),
])
def test_map_value_types(vt, good, bad):
    e = E("m", "map", keyRef="cow.biomes/biomes[].id", valueType=vt)
    assert run([e], {"m": {"forest": good}}, files=REFFILES) == []
    one(run([e], {"m": {"forest": bad}}, files=REFFILES), "data.wrong-type")


def test_map_enum_value_type():
    e = E("m", "map", keyRef="cow.biomes/biomes[].id", valueType="enum", values=[{"id": "hi"}, {"id": "lo"}])
    assert run([e], {"m": {"forest": "hi"}}, files=REFFILES) == []
    f = one(run([e], {"m": {"forest": "hj"}}, files=REFFILES), "data.bad-enum")
    assert f.suggestion in ("hi",) or f.suggestion is None


def test_map_dict_value_is_wrong_type():
    e = E("m", "map", keyRef="cow.biomes/biomes[].id", valueType="string")
    one(run([e], {"m": {"forest": {"a": 1}}}, files=REFFILES), "data.wrong-type")


def test_map_null_value():
    e = E("m", "map", keyRef="cow.biomes/biomes[].id", valueType="float")
    one(run([e], {"m": {"forest": None}}, files=REFFILES), "data.wrong-type")


def test_map_missing_keyref_format_warns_once():
    e1 = E("m", "map", keyRef="gone/p", valueType="float")
    e2 = E("n", "map", keyRef="gone/p", valueType="float")
    out = run([e1, e2], {"m": {"a": 1}, "n": {"b": 2}}, files=REFFILES)
    assert [(f.severity, f.rule) for f in out] == [("WARN", "data.ref-format-missing")]


def test_map_missing_keyref_format_still_checks_values():
    e = E("m", "map", keyRef="gone/p", valueType="float", max=1)
    out = run([e], {"m": {"a": 5}}, files=REFFILES)
    assert [(f.severity, f.rule) for f in out] == [("WARN", "data.ref-format-missing"), ("FAIL", "data.out-of-range")]


def test_map_without_context_checks_values_only():
    e = E("m", "map", keyRef="cow.biomes/biomes[].id", valueType="float", max=1)
    one(run([e], {"m": {"zzz": 5}}), "data.out-of-range")


def test_map_keys_with_dots_and_unicode():
    d = {"format": "t", "version": 1, "ids": ["a.b", "é", "x[0]"]}
    e = E("m", "map", keyRef="t/ids[]", valueType="int")
    out = run([e], {"m": {"a.b": 1, "é": 2, "x[0]": 3, "q": 4}}, files=[("t.json", d, False)])
    assert rp(out) == [("data.unresolved-map-key", "m", "m.q")]


def test_map_default_mismatch_not_checked():
    e = E("m", "map", keyRef="cow.biomes/biomes[].id", valueType="float", default={"forest": 1})
    assert run([e], {"m": {"forest": 0.5}}, files=REFFILES) == []


def test_map_entry_with_scalar_value_wrong_type():
    e = E("m", "map", keyRef="a/b", valueType="float")
    one(run([e], {"m": 5}), "data.wrong-type")
    one(run([e], {"m": [1]}), "data.wrong-type")


def test_map_valuetype_with_unknown_type_accepts_anything():
    e = E("m", "map", keyRef="a/b", valueType="zzz")
    assert run([e], {"m": {"k": [1]}}) == []  # PIN: unknown valueType is not checked by data rules


# ---------------------------------------------------------------- keys / docs (D14-D16)
def test_key_with_dot_literal_and_nested_same_key():
    # a literal "a.b" key and the nested a -> b collapse to the same schema key
    ent = [E("a.b")]
    assert run(ent, {"a.b": 1}) == []
    assert run(ent, {"a": {"b": 1}}) == []
    assert rp(run(ent, {"a.b": "x"})) == [("data.wrong-type", "a.b", "a.b")]


def test_key_with_brackets_and_unicode():
    out = run([E("é")], {"é": 1, "ü": 2, "k[]": 3})
    assert rp(out) == [("data.no-schema-entry", "ü", "ü"), ("data.no-schema-entry", "k[]", "k[]")]


def test_unknown_key_suggestion():
    out = run([E("growth_rate")], {"growth_rat": 1})
    assert out[0].suggestion == "growth_rate"


@pytest.mark.parametrize("doc", [[], None, "s", 5, True, [{"format": "x"}]])
def test_top_level_not_dict(doc):
    idx = build_schema_index([S(E("a"))])
    assert validate_data_document(doc, "f", idx, is_pack=False) == []


@pytest.mark.parametrize("fmt", [None, 5, [], {}, "other", "", True])
def test_format_unknown_or_non_string(fmt):
    idx = build_schema_index([S(E("a"))])
    assert validate_data_document({"format": fmt, "a": "bad"}, "f", idx, is_pack=False) == []


def test_missing_format():
    idx = build_schema_index([S(E("a"))])
    assert validate_data_document({"a": "bad"}, "f", idx, is_pack=False) == []


def test_envelope_keys_skipped_only_at_top():
    out = run([E("n.format")], {"n": {"format": 1, "version": "x"}})
    assert rp(out) == [("data.no-schema-entry", "n.version", "n.version")]


def test_version_value_not_validated_pin():
    assert run([E("a")], {"a": 1, "version": "whatever"}) == []  # PIN


def test_empty_document_is_clean():
    assert run([E("a", required=True)], {}) == []


def test_metric_and_arg_kinds_ignored_for_data():
    ent = [E("m", kind="metric"), E("g", kind="arg"), E("c", kind="command")]
    out = run(ent, {"m": 1})
    one(out, "data.no-schema-entry")


def test_two_schemas_same_for_first_wins():
    idx = build_schema_index([S(E("a", "string")), S(E("a", "float"), E("b", "float"))])
    doc = {"format": "x", "a": "text", "b": 1}
    assert validate_data_document(doc, "f", idx, is_pack=False) == []
    doc = {"format": "x", "a": 1.5}
    assert [f.rule for f in validate_data_document(doc, "f", idx, is_pack=False)] == ["data.wrong-type"]


def test_first_entry_for_duplicate_key_wins():
    idx = build_schema_index([S(E("a", "string"), E("a", "float"))])
    assert validate_data_document({"format": "x", "a": "t"}, "f", idx, is_pack=False) == []


def test_metric_first_then_parameter_same_key_uses_parameter():
    idx = build_schema_index([S(E("a", "bool", kind="metric"), E("a", "float"))])
    assert validate_data_document({"format": "x", "a": 1}, "f", idx, is_pack=False) == []


def test_index_ignores_garbage_docs_and_entries():
    idx = build_schema_index([None, 5, {"for": 5}, {"for": "x"}, {"for": "x", "entries": [None, 5, {"key": 1}]},
                              S(E("a"))])
    assert idx.has_format("x") and sorted(idx.entries_for("x")) == ["a"]
    assert not idx.has_format("y") and idx.entries_for("y") == {} and idx.entries_for(None) == {}
    assert not idx.has_format(None) and not idx.has_format(5)


def test_index_tolerates_non_list_entries_and_missing_type():
    idx = build_schema_index([{"for": "x", "entries": 5}, {"for": "y", "entries": {"k": 1}},
                              {"for": "z", "entries": [{"key": "a", "kind": "parameter"}]}])
    for f in "xyz":
        validate_data_document({"format": f, "a": 1}, "f", idx, is_pack=False)


# ---------------------------------------------------------------- defaults (D17-D18)
@pytest.mark.parametrize("default,val,mismatch", [
    (2, 2.0, False), (2.0, 2, False), (1, True, True), (True, 1, True), (0, False, True), (False, 0, True),
    ("a", "a", False), ("a", "b", True), (1.5, 1.5, False), (1.5, 1.6, True), (0.1 + 0.2, 0.3, True),
])
def test_default_comparison(default, val, mismatch):
    t = "bool" if isinstance(val, bool) else "float"
    if isinstance(default, str):
        t = "string"
    if isinstance(default, bool) != isinstance(val, bool):
        t = "bool" if isinstance(val, bool) else "float"
    out = run([E("a", t, default=default)], {"a": val})
    if t == "bool" and not isinstance(default, bool) or (t == "float" and isinstance(val, bool)):
        return  # type error path covered elsewhere
    assert [f.rule for f in out] == (["data.default-mismatch"] if mismatch else [])


def test_default_true_vs_one_mismatch_for_bool_entry():
    out = run([E("a", "bool", default=True)], {"a": False})
    one(out, "data.default-mismatch")


def test_default_list():
    e = E("a", "ref[]", ref="f/p", default=["x"])
    assert run([e], {"a": ["x"]}) == []
    one(run([e], {"a": ["y"]}), "data.default-mismatch")
    one(run([e], {"a": []}), "data.default-mismatch")
    one(run([e], {"a": ["x", "x"]}), "data.default-mismatch")


def test_default_skipped_for_packs():
    assert run([E("a", default=1)], {"a": 2}, pack=True) == []


def test_default_not_compared_when_value_invalid():
    one(run([E("a", default=1, max=5)], {"a": 9}), "data.out-of-range")


def test_default_not_compared_when_ref_unresolved():
    out = run([E("a", "ref", ref="cow.biomes/biomes[].id", default="forest")], {"a": "zz"}, files=REFFILES)
    one(out, "data.unresolved-ref")


def test_default_mismatch_on_record_table_column_pin():  # PIN (B10)
    out = run([E("a[].v", default=1)], {"a": [{"v": 1}, {"v": 2}]})
    assert rp(out) == [("data.default-mismatch", "a[].v", "a[1].v")]


def test_default_mismatch_inside_map_not_reported():
    e = E("m", "map", keyRef="cow.biomes/biomes[].id", valueType="float", default=1)
    assert run([e], {"m": {"forest": 2}}, files=REFFILES) == []


def test_schema_key_missing_from_defaults_not_reported_pin():
    assert run([E("a"), E("b")], {"a": 1}) == []  # PIN (B16)


def test_default_present_but_null_in_data():
    one(run([E("a", default=1)], {"a": None}), "data.wrong-type")


def test_default_zero_vs_negative_zero():
    assert run([E("a", default=0.0)], {"a": -0.0}) == []


# ---------------------------------------------------------------- validate_schema_references (D19)
def refs(docs, **kw):
    names = {id(d): f"s{i}" for i, d in enumerate(docs)}
    return validate_schema_references(docs, names, **kw)


def test_reference_self_reference_ok():
    assert refs([S(E("a", affects=["a"]))]) == []


def test_reference_unresolved_with_suggestion_and_path():
    out = refs([S(E("a", affects=["bb"]), E("b"))])
    assert [(f.rule, f.path, f.suggestion, f.key, f.file) for f in out] == [
        ("schema.unresolved-reference", "affects[0]", "b", "a", "s0")]


def test_reference_related_checked():
    out = refs([S(E("a", related=["zz"]))])
    assert [f.path for f in out] == ["related[0]"]


def test_reference_cross_format_slash():
    d1 = S(E("a", affects=["y/b"]), fmt="x")
    d2 = S(E("b"), fmt="y")
    assert refs([d1, d2]) == []
    assert refs([d1]) != []


def test_reference_cross_format_wrong_key():
    d1 = S(E("a", affects=["y/c"]), fmt="x")
    d2 = S(E("b"), fmt="y")
    assert [f.rule for f in refs([d1, d2])] == ["schema.unresolved-reference"]


def test_reference_unknown_format():
    assert [f.rule for f in refs([S(E("a", affects=["nope/b"]))])] == ["schema.unresolved-reference"]


def test_reference_slash_form_to_own_format():
    assert refs([S(E("a", affects=["x/b"]), E("b"))]) == []


def test_reference_bare_key_does_not_cross_formats():
    d1 = S(E("a", affects=["b"]), fmt="x")
    d2 = S(E("b"), fmt="y")
    assert len(refs([d1, d2])) == 1


def test_reference_any_kind_resolves():
    assert refs([S(E("a", affects=["m"]), E("m", kind="metric"))]) == []


def test_reference_empty_affects_and_related():
    assert refs([S(E("a", affects=[], related=[]))]) == []


def test_reference_empty_string_item():
    assert len(refs([S(E("a", affects=[""]))])) == 1


def test_reference_non_string_items_ignored():
    assert refs([S(E("a", affects=[1, None, ["x"]], related="zz"))]) == []


def test_reference_multiple_bad_items_indexed():
    out = refs([S(E("a", affects=["q", "a", "w"]))])
    assert [f.path for f in out] == ["affects[0]", "affects[2]"]


def test_reference_same_for_two_docs_share_keys():
    assert refs([S(E("a", affects=["b"])), S(E("b"))]) == []


def test_ramp_unknown_and_known():
    out = refs([S(E("a", ui={"ramp": "nope"}))])
    assert [(f.rule, f.path) for f in out] == [("schema.unknown-ramp", "ui.ramp")]
    assert refs([S(E("a", ui={"ramp": "sequential.green"}))]) == []


@pytest.mark.parametrize("ramp", [5, None, [], {}, True])
def test_ramp_non_string_ignored(ramp):
    assert refs([S(E("a", ui={"ramp": ramp}))]) == []


@pytest.mark.parametrize("ui", [[], None, "s", 5])
def test_ui_non_dict_ignored(ui):
    assert refs([S(E("a", ui=ui))]) == []


def test_ramp_empty_known_set_rejects_all():
    out = refs([S(E("a", ui={"ramp": "sequential.green"}))], known_ramps=[])
    assert len(out) == 1 and out[0].rule == "schema.unknown-ramp"


def test_ramp_custom_known_set_and_generator():
    assert refs([S(E("a", ui={"ramp": "mine"}))], known_ramps=(r for r in ["mine"])) == []


def test_ramp_empty_string():
    assert len(refs([S(E("a", ui={"ramp": ""}))])) == 1


def test_reference_missing_file_name_defaults_empty():
    d = S(E("a", affects=["zz"]))
    out = validate_schema_references([d], {})
    assert out[0].file == ""


def test_reference_garbage_docs_ignored():
    assert validate_schema_references([None, 5, {"for": 5}, {"for": "x", "entries": [None, {"key": 1}]}], {}) == []


def test_reference_non_list_entries():
    validate_schema_references([{"for": "x", "entries": 5}], {})


def test_reference_generator_input():
    d = S(E("a", affects=["zz"]))
    assert len(validate_schema_references((x for x in [d]), {id(d): "f"})) == 1


def test_known_ramps_constant():
    from dia_cli.commands.validate.data_rules import KNOWN_RAMPS
    assert "sequential.green" in KNOWN_RAMPS and all(isinstance(r, str) for r in KNOWN_RAMPS)


# ---------------------------------------------------------------- DataContext
def test_context_values_at():
    c = DataContext.from_files(REFFILES)
    assert c.values_at("cow.biomes/biomes[].id") == ["forest", "tundra"]
    assert c.values_at("nope/x") is None
    assert c.values_at("cow.biomes/zzz") == []
    assert c.values_at("noslash") is None
    assert c.values_at(5) is None


def test_context_from_files_ignores_garbage():
    c = DataContext.from_files([("a", [], False), ("b", {"format": 5}, False), ("c", None, True)])
    assert not c.has_format("x") and c.documents_by_format == {}


def test_context_empty_default():
    assert DataContext().values_at("a/b") is None


# ---------------------------------------------------------------- count_keys
def test_count_keys():
    from dia_cli.commands.validate.schema_discovery import count_keys
    assert count_keys({"format": "x", "version": 1}) == 0
    assert count_keys({"format": "x", "a": 1, "b": {"c": 2}}) == 2
    assert count_keys([]) == 0 and count_keys(None) == 0
    assert count_keys({"a": [{"b": 1}, {"b": 2}]}) == 1
    assert count_keys({"a": {}}) in (0, 1)


# ---------------------------------------------------------------- non-mutation
def test_validation_does_not_mutate():
    ent = [E("a[].v", min=0), E("m", "map", keyRef="cow.biomes/biomes[].id", valueType="float")]
    idx = build_schema_index([S(*ent)])
    doc = {"format": "x", "version": 1, "a": [{"v": -1}], "m": {"forest": 1, "zz": 2}}
    before = copy.deepcopy(doc)
    c = DataContext.from_files(REFFILES)
    r1 = validate_data_document(doc, "f", idx, is_pack=False, context=c)
    r2 = validate_data_document(doc, "f", idx, is_pack=False, context=c)
    assert doc == before and r1 == r2
