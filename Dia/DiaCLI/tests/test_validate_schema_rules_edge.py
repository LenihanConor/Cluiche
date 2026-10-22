"""Edge-case tests for schema_rules (R1-R14). Cases marked PIN document current behaviour."""
from __future__ import annotations

import copy
import json
import re
from pathlib import Path

import pytest

import dia_cli.commands.validate.schema_rules as sr
from dia_cli.commands.validate.schema_rules import validate_schema_document, validate_schema_text

NAN = float("nan")
INF = float("inf")


def E(**over):
    e = {"key": "k", "kind": "parameter", "label": "L", "description": "D", "type": "float"}
    e.update(over)
    return e


def D(*entries, **over):
    d = {"format": "dia.value_schema", "version": 1, "for": "x", "entries": list(entries)}
    d.update(over)
    return d


def rules(doc):
    return [(f.severity, f.rule) for f in validate_schema_document(doc, "f.json")]


def fail_rules(doc):
    return [r for s, r in rules(doc) if s == "FAIL"]


def only(doc):
    out = fail_rules(doc)
    assert len(out) == 1, out
    return out[0]


# ---------------------------------------------------------------- text level (R1)
GOOD_TEXT = json.dumps(D(E()))


@pytest.mark.parametrize("text,expected", [
    ("", "schema.unparsable-json"),
    ("   \n\t ", "schema.unparsable-json"),
    ("{", "schema.unparsable-json"),
    ("﻿" + GOOD_TEXT, "schema.unparsable-json"),  # PIN: BOM rejected by json.loads
    ("[]", "schema.root-not-object"),
    ("[" + GOOD_TEXT + "]", "schema.root-not-object"),
    ("null", "schema.root-not-object"),
    ("true", "schema.root-not-object"),
    ("5", "schema.root-not-object"),
    ('"s"', "schema.root-not-object"),
    ("{'format': 1}", "schema.unparsable-json"),
    ('{"a": 1,}', "schema.unparsable-json"),
])
def test_text_level_failures(text, expected):
    out = validate_schema_text(text, "f.json")
    assert [f.rule for f in out] == [expected]
    assert out[0].severity == "FAIL" and out[0].file == "f.json"


def test_text_crlf_ok():
    assert validate_schema_text(json.dumps(D(E()), indent=2).replace("\n", "\r\n"), "f") == []


def test_text_trailing_whitespace_ok():
    assert validate_schema_text(GOOD_TEXT + "\n\n  ", "f") == []


def test_text_nan_literal_is_python_only_pass_of_json():
    # PIN: Python json accepts NaN literal (jsoncpp does not); min NaN is then rejected by the rules.
    text = GOOD_TEXT.replace('"type": "float"', '"type": "float", "min": NaN')
    assert [f.rule for f in validate_schema_text(text, "f")] == ["schema.wrong-type"]


@pytest.mark.parametrize("depth", [5000, 100000])
def test_deeply_nested_text_is_a_fail_not_a_crash(depth):
    out = validate_schema_text("[" * depth, "f.json")
    assert [f.rule for f in out] == ["schema.unparsable-json"]


def test_deeply_nested_objects_text():
    out = validate_schema_text('{"a":' * 50000 + "1" + "}" * 50000, "f.json")
    assert [f.rule for f in out] == ["schema.unparsable-json"]


# ---------------------------------------------------------------- file level (R2-R5)
@pytest.mark.parametrize("fmt", ["dia.value_schema2", "", "DIA.VALUE_SCHEMA", None, 1, [], {}, True])
def test_bad_format(fmt):
    d = D(E())
    d["format"] = fmt
    assert rules(d) == [("FAIL", "schema.bad-format")]


def test_missing_format():
    d = D(E())
    del d["format"]
    assert rules(d) == [("FAIL", "schema.bad-format")]


@pytest.mark.parametrize("ver", [1.0, "1", True, False, 2, 0, -1, None, [], {}, 1.5, NAN])
def test_bad_version(ver):  # 1.0 is a PIN: rejected as a float
    d = D(E())
    d["version"] = ver
    assert rules(d) == [("FAIL", "schema.bad-version")]


def test_missing_version():
    d = D(E())
    del d["version"]
    assert rules(d) == [("FAIL", "schema.bad-version")]


@pytest.mark.parametrize("for_", [5, None, [], {}, True, 1.5])
def test_for_non_string(for_):
    d = D(E())
    d["for"] = for_
    assert rules(d) == [("FAIL", "schema.missing-for")]


def test_for_missing():
    d = D(E())
    del d["for"]
    assert rules(d) == [("FAIL", "schema.missing-for")]


@pytest.mark.parametrize("for_", ["", " ", "a/b", "a.b", "éè", "x" * 5000])
def test_for_string_forms_accepted(for_):  # PIN: any string accepted, including empty
    assert rules(D(E(), **{"for": for_})) == []


@pytest.mark.parametrize("entries", [{}, None, "e", 5, True, {"key": "k"}])
def test_entries_wrong_type(entries):
    d = D()
    d["entries"] = entries
    assert rules(d) == [("FAIL", "schema.missing-entries")]


def test_entries_missing():
    d = D()
    del d["entries"]
    assert rules(d) == [("FAIL", "schema.missing-entries")]


def test_entries_empty_ok():
    assert rules(D()) == []


@pytest.mark.parametrize("extra", ["x", "Format", "", "entries2", "$schema", "é"])
def test_unknown_file_key(extra):
    d = D(E())
    d[extra] = 1
    assert rules(d) == [("FAIL", "schema.unknown-file-key")]


def test_file_level_error_suppresses_entry_errors():  # PIN
    d = D(5, E(type="nope"), extra=1)
    assert rules(d) == [("FAIL", "schema.unknown-file-key")]


def test_file_level_precedence_order():
    d = {"format": "bad", "version": 2, "entries": 5, "x": 1}
    assert rules(d) == [("FAIL", "schema.bad-format")]
    d["format"] = "dia.value_schema"
    assert rules(d) == [("FAIL", "schema.bad-version")]
    d["version"] = 1
    assert rules(d) == [("FAIL", "schema.missing-for")]
    d["for"] = "x"
    assert rules(d) == [("FAIL", "schema.missing-entries")]
    d["entries"] = []
    assert rules(d) == [("FAIL", "schema.unknown-file-key")]


@pytest.mark.parametrize("root", [None, [], [D()], "s", 5, 1.5, True])
def test_root_not_object_document(root):
    assert rules(root) == [("FAIL", "schema.root-not-object")]


# ---------------------------------------------------------------- entries (R6)
@pytest.mark.parametrize("entry", [5, None, "s", [], [E()], True, 1.5])
def test_entry_not_object(entry):
    out = validate_schema_document(D(entry), "f")
    assert [(f.rule, f.key) for f in out] == [("schema.entry-not-object", "")]


@pytest.mark.parametrize("key", [None, 5, [], {}, True])
def test_entry_key_not_string(key):
    out = validate_schema_document(D(E(key=key)), "f")
    assert [(f.rule, f.key) for f in out] == [("schema.missing-key", "")]


def test_entry_without_key():
    e = E()
    del e["key"]
    assert only(D(e)) == "schema.missing-key"


@pytest.mark.parametrize("key", [
    "", " ", "a.b", "a[].b", "a[]", "[]", "a/b", "éè中", "\U0001f600", "x" * 20000, "a b", "a\nb",
])
def test_entry_key_forms_accepted(key):  # PIN: no key-syntax rule exists
    assert rules(D(E(key=key))) == []


@pytest.mark.parametrize("field", ["kind", "label", "description", "type"])
def test_required_field_missing_and_wrong_type(field):
    e = E()
    del e[field]
    assert only(D(e)) == "schema.missing-key"
    for bad in (5, None, [], True):
        assert only(D(E(**{field: bad}))) == "schema.wrong-type"


@pytest.mark.parametrize("kind", ["Parameter", "", "param", "PARAMETER", "x"])
def test_unknown_kind(kind):
    assert only(D(E(kind=kind))) == "schema.unknown-kind"


@pytest.mark.parametrize("kind", ["parameter", "field", "metric", "arg", "command", "event", "component_field"])
def test_every_kind_accepted(kind):
    assert rules(D(E(kind=kind))) == []


@pytest.mark.parametrize("t", ["Float", "", "double", "ref[ ]", "map "])
def test_unknown_type(t):
    assert only(D(E(type=t))) == "schema.unknown-type"


@pytest.mark.parametrize("desc", ["", " ", "\t\n", "   \r\n  "])
def test_empty_description(desc):
    assert only(D(E(description=desc))) == "schema.empty-description"


def test_label_empty_is_accepted():  # PIN: only description is checked for blankness
    assert rules(D(E(label=""))) == []


@pytest.mark.parametrize("field", ["unit", "hint", "source"])
@pytest.mark.parametrize("bad", [5, None, [], {}, True])
def test_optional_string_wrong_type(field, bad):
    assert only(D(E(**{field: bad}))) == "schema.wrong-type"


@pytest.mark.parametrize("field", ["unit", "hint", "source"])
def test_optional_string_ok(field):
    assert rules(D(E(**{field: ""}))) == []


@pytest.mark.parametrize("name", ["extra", "Key", "", "ui2", "default2"])
def test_unknown_entry_key(name):
    assert only(D(E(**{name: 1}))) == "schema.unknown-entry-key"


def test_unknown_entry_key_beats_other_errors():
    assert only(D(E(type="nope", zzz=1))) == "schema.unknown-entry-key"


@pytest.mark.parametrize("name", ["default", "ui", "history", "affects", "related", "min", "max", "unique",
                                  "required", "unit", "hint", "source", "values", "payload", "vocabulary"])
def test_known_entry_keys_not_flagged_unknown(name):
    assert "schema.unknown-entry-key" not in fail_rules(D(E(**{name: None})))


# ---------------------------------------------------------------- min / max
@pytest.mark.parametrize("field", ["min", "max"])
@pytest.mark.parametrize("bad", [True, False, "1", None, [], {}, NAN, INF, -INF])
def test_min_max_rejected(field, bad):
    assert only(D(E(**{field: bad}))) == "schema.wrong-type"


@pytest.mark.parametrize("field", ["min", "max"])
@pytest.mark.parametrize("good", [0, -0.0, 1, -5, 1.5, 1e308, -1e308, 10 ** 400, -(10 ** 400), 1e-320])
def test_min_max_accepted(field, good):
    assert rules(D(E(**{field: good}))) == []


def test_min_greater_than_max_pin():
    assert rules(D(E(min=5, max=1))) == []  # PIN: no ordering rule


def test_min_equal_max_pin():
    assert rules(D(E(min=2, max=2))) == []  # PIN


@pytest.mark.parametrize("field", ["unique", "required"])
@pytest.mark.parametrize("bad", [1, 0, "true", None, [], {}])
def test_flags_must_be_bool(field, bad):
    assert only(D(E(**{field: bad}))) == "schema.wrong-type"


@pytest.mark.parametrize("field", ["unique", "required"])
@pytest.mark.parametrize("good", [True, False])
def test_flags_bool_ok(field, good):
    assert rules(D(E(**{field: good}))) == []


# ---------------------------------------------------------------- affects/related (R7)
@pytest.mark.parametrize("field", ["affects", "related"])
@pytest.mark.parametrize("bad", ["x", {}, 5, None, True, [1], [None], ["a", 2], [["a"]], [{}]])
def test_string_list_wrong_type(field, bad):
    assert only(D(E(**{field: bad}))) == "schema.wrong-type"


@pytest.mark.parametrize("field", ["affects", "related"])
@pytest.mark.parametrize("good", [[], ["a"], ["a", "a"], [""], ["x/y", "é"]])
def test_string_list_ok(field, good):
    assert rules(D(E(**{field: good}))) == []


# ---------------------------------------------------------------- ui / history (R8)
@pytest.mark.parametrize("block", ["ui", "history"])
@pytest.mark.parametrize("bad", [[], None, "s", 5, True, [{}]])
def test_block_not_object(block, bad):
    assert only(D(E(**{block: bad}))) == "schema.wrong-type"


@pytest.mark.parametrize("block", ["ui", "history"])
def test_block_empty_ok(block):
    assert rules(D(E(**{block: {}}))) == []


def test_block_known_keys_no_warning():
    d = D(E(ui={"group": 1, "step": "x", "ramp": 5, "hidden": None}, history={"track": 1, "group": 2}))
    assert rules(d) == []


@pytest.mark.parametrize("block", ["ui", "history"])
def test_block_unknown_key_warns_once(block):
    d = D(E(key="a", **{block: {"zzz": 1}}), E(key="b", **{block: {"zzz": 2}}))
    assert rules(d) == [("WARN", "schema.unknown-block-key")]


def test_block_unknown_key_warning_scoped_per_block():
    d = D(E(key="a", ui={"zzz": 1}, history={"zzz": 1}))
    assert rules(d) == [("WARN", "schema.unknown-block-key")] * 2


def test_block_unknown_keys_each_warn():
    d = D(E(ui={"p": 1, "q": 1, "r": 1}))
    assert rules(d) == [("WARN", "schema.unknown-block-key")] * 3


def test_block_warn_dedup_is_per_document():
    d = D(E(ui={"zzz": 1}))
    assert len(rules(d)) == 1 and len(rules(d)) == 1


def test_block_warn_not_issued_when_entry_fails_before_block():
    assert fail_rules(D(E(type="nope", ui={"zzz": 1}))) == ["schema.unknown-type"]
    assert rules(D(E(type="nope", ui={"zzz": 1}))) == [("FAIL", "schema.unknown-type")]


def test_block_warn_issued_then_entry_fails_later():
    r = rules(D(E(ui={"zzz": 1}, affects="bad")))
    assert r == [("WARN", "schema.unknown-block-key"), ("FAIL", "schema.wrong-type")]


# ---------------------------------------------------------------- ref / keyRef / map (R9)
@pytest.mark.parametrize("t", ["ref", "ref[]"])
def test_ref_missing(t):
    assert only(D(E(type=t))) == "schema.missing-key"


@pytest.mark.parametrize("t", ["ref", "ref[]"])
@pytest.mark.parametrize("bad", [5, None, [], {}, True])
def test_ref_wrong_type(t, bad):
    assert only(D(E(type=t, ref=bad))) == "schema.wrong-type"


@pytest.mark.parametrize("t", ["float", "int", "bool", "string", "enum", "map"])
def test_ref_misuse(t):
    extra = {"values": []} if t == "enum" else {}
    if t == "map":
        extra = {"keyRef": "a/b", "valueType": "float"}
    assert only(D(E(type=t, ref="a/b", **extra))) == "schema.ref-misuse"


@pytest.mark.parametrize("t", ["float", "ref", "ref[]", "enum", "string"])
def test_keyref_and_valuetype_misuse(t):
    base = {"ref": "a/b"} if t.startswith("ref") else ({"values": []} if t == "enum" else {})
    assert only(D(E(type=t, keyRef="a/b", **base))) == "schema.map-misuse"
    assert only(D(E(type=t, valueType="float", **base))) == "schema.map-misuse"


def test_map_missing_fields():
    assert only(D(E(type="map", valueType="float"))) == "schema.missing-key"
    assert only(D(E(type="map", keyRef="a/b"))) == "schema.missing-key"
    assert only(D(E(type="map"))) == "schema.missing-key"


@pytest.mark.parametrize("bad", [5, None, [], True])
def test_map_field_wrong_type(bad):
    assert only(D(E(type="map", keyRef=bad, valueType="float"))) == "schema.wrong-type"
    assert only(D(E(type="map", keyRef="a/b", valueType=bad))) == "schema.wrong-type"


@pytest.mark.parametrize("vt", ["ref", "map", "ref[]", "Float", "", "double"])
def test_map_bad_value_type(vt):
    assert only(D(E(type="map", keyRef="a/b", valueType=vt))) == "schema.map-misuse"


@pytest.mark.parametrize("vt", ["float", "int", "bool", "string", "enum"])
def test_map_value_types_ok(vt):
    assert rules(D(E(type="map", keyRef="a/b", valueType=vt))) == []


@pytest.mark.parametrize("bad", ["x", "", "/", "/x", "x/", "//", " ", "abc"])
@pytest.mark.parametrize("t", ["ref", "ref[]"])
def test_ref_malformed(t, bad):  # Python-stricter than the C++ loader (which checks presence only)
    assert only(D(E(type=t, ref=bad))) == "schema.ref-malformed"


@pytest.mark.parametrize("bad", ["x", "", "/", "/x", "x/", "//"])
def test_keyref_malformed(bad):
    assert only(D(E(type="map", keyRef=bad, valueType="float"))) == "schema.ref-malformed"


@pytest.mark.parametrize("good", ["a/b", "cow.biomes/biomes[].id", "a/b/c", "a//b", " /x", "é/è"])
def test_ref_wellformed(good):
    assert rules(D(E(type="ref", ref=good))) == []
    assert rules(D(E(type="map", keyRef=good, valueType="int"))) == []


# ---------------------------------------------------------------- enum values (R10)
def test_enum_requires_values():
    assert only(D(E(type="enum"))) == "schema.enum-no-values"


@pytest.mark.parametrize("bad", ["x", {}, None, 5, True])
def test_enum_values_not_array(bad):
    assert only(D(E(type="enum", values=bad))) == "schema.wrong-type"


@pytest.mark.parametrize("item", [5, None, "s", [], True])
def test_enum_value_not_object(item):
    assert only(D(E(type="enum", values=[{"id": "a"}, item]))) == "schema.wrong-type"


@pytest.mark.parametrize("field", ["id", "label", "description", "colour"])
@pytest.mark.parametrize("bad", [5, None, [], True])
def test_enum_value_field_wrong_type(field, bad):
    assert only(D(E(type="enum", values=[{"id": "a", field: bad}]))) == "schema.wrong-type"


def test_enum_empty_values_pin():
    assert rules(D(E(type="enum", values=[]))) == []  # PIN (B7)


def test_enum_duplicate_ids_pin():
    assert rules(D(E(type="enum", values=[{"id": "a"}, {"id": "a"}]))) == []  # PIN (B7)


def test_enum_value_without_id_pin():
    assert rules(D(E(type="enum", values=[{}, {"label": "L"}]))) == []  # PIN (B7)


def test_values_on_non_enum_pin():
    assert rules(D(E(type="float", values=[{"id": "a"}]))) == []  # PIN
    assert only(D(E(type="float", values=5))) == "schema.wrong-type"


def test_enum_value_extra_fields_ignored_pin():
    assert rules(D(E(type="enum", values=[{"id": "a", "zzz": 1}]))) == []  # PIN


# ---------------------------------------------------------------- payload / vocabulary (R11)
def ev(**over):
    return E(kind="event", type="string", **over)


@pytest.mark.parametrize("field", ["payload", "vocabulary"])
@pytest.mark.parametrize("kind", ["parameter", "field", "metric", "arg", "command", "component_field"])
def test_event_only_field(field, kind):
    assert only(D(E(kind=kind, **{field: []}))) == "schema.event-only-field"


@pytest.mark.parametrize("field", ["payload", "vocabulary"])
@pytest.mark.parametrize("bad", ["x", {}, None, 5, True])
def test_event_list_not_array(field, bad):
    assert only(D(ev(**{field: bad}))) == "schema.wrong-type"


@pytest.mark.parametrize("field", ["payload", "vocabulary"])
@pytest.mark.parametrize("item", [5, None, "s", [], True])
def test_event_list_item_not_object(field, item):
    assert only(D(ev(**{field: [item]}))) == "schema.wrong-type"


@pytest.mark.parametrize("field", ["payload", "vocabulary"])
def test_event_list_empty_ok(field):
    assert rules(D(ev(**{field: []}))) == []


@pytest.mark.parametrize("missing", ["name", "type", "description"])
def test_payload_missing_required(missing):
    item = {"name": "n", "type": "float", "description": "d"}
    del item[missing]
    assert only(D(ev(payload=[item]))) == "schema.missing-key"


@pytest.mark.parametrize("field", ["name", "type", "description"])
@pytest.mark.parametrize("bad", [5, None, [], True])
def test_payload_required_wrong_type(field, bad):
    item = {"name": "n", "type": "float", "description": "d", field: bad}
    assert only(D(ev(payload=[item]))) == "schema.wrong-type"


@pytest.mark.parametrize("t", ["Float", "", "double"])
def test_payload_unknown_type(t):
    assert only(D(ev(payload=[{"name": "n", "type": t, "description": "d"}]))) == "schema.unknown-type"


@pytest.mark.parametrize("t", ["float", "int", "bool", "enum", "string", "ref", "ref[]", "map"])
def test_payload_every_type_ok(t):
    assert rules(D(ev(payload=[{"name": "n", "type": t, "description": "d"}]))) == []


def test_payload_unit_wrong_type():
    assert only(D(ev(payload=[{"name": "n", "type": "float", "description": "d", "unit": 5}]))) == "schema.wrong-type"


def test_payload_empty_description_ok_pin():
    assert rules(D(ev(payload=[{"name": "n", "type": "float", "description": ""}]))) == []  # PIN


@pytest.mark.parametrize("missing", ["id", "description"])
def test_vocabulary_missing_required(missing):
    item = {"id": "i", "description": "d"}
    del item[missing]
    assert only(D(ev(vocabulary=[item]))) == "schema.missing-key"


@pytest.mark.parametrize("field", ["id", "description", "label", "unit"])
@pytest.mark.parametrize("bad", [5, None, [], True])
def test_vocabulary_field_wrong_type(field, bad):
    item = {"id": "i", "description": "d", field: bad}
    assert only(D(ev(vocabulary=[item]))) == "schema.wrong-type"


def test_vocabulary_type_not_validated_pin():
    assert rules(D(ev(vocabulary=[{"id": "i", "description": "d", "type": "zzz"}]))) == []  # PIN


def test_payload_second_item_bad_reports_one():
    items = [{"name": "n", "type": "float", "description": "d"}, {"name": "m"}, {"zzz": 1}]
    assert only(D(ev(payload=items))) == "schema.missing-key"


# ---------------------------------------------------------------- continue after bad entry (R12)
def test_all_entries_after_bad_one_reported():
    d = D(
        E(key="a", type="nope"),
        E(key="b"),
        5,
        E(key="c", min="x"),
        E(key="b"),
        E(key="d", kind="zzz"),
        E(key="e", description=" "),
        None,
        E(key="f"),
    )
    out = validate_schema_document(d, "f")
    assert [(f.rule, f.key) for f in out] == [
        ("schema.unknown-type", "a"),
        ("schema.entry-not-object", ""),
        ("schema.wrong-type", "c"),
        ("schema.duplicate-key", "b"),
        ("schema.unknown-kind", "d"),
        ("schema.empty-description", "e"),
        ("schema.entry-not-object", ""),
    ]


def test_only_first_problem_per_entry():
    assert len(validate_schema_document(D(E(type="nope", kind="zzz", description="")), "f")) == 1


def test_many_bad_entries_all_reported():
    out = validate_schema_document(D(*[E(key=f"k{i}", type="nope") for i in range(500)]), "f")
    assert len(out) == 500


def test_finding_carries_file_and_key():
    out = validate_schema_document(D(E(key="kk", type="nope")), "some/file.json")
    assert (out[0].file, out[0].key, out[0].severity) == ("some/file.json", "kk", "FAIL")


# ---------------------------------------------------------------- duplicates (R13)
def test_duplicate_second_and_third_flagged():
    out = validate_schema_document(D(E(key="a"), E(key="a"), E(key="a")), "f")
    assert [f.rule for f in out] == ["schema.duplicate-key"] * 2


def test_duplicate_across_different_kinds():
    assert only(D(E(key="a"), E(key="a", kind="metric"))) == "schema.duplicate-key"


def test_duplicate_is_case_sensitive():
    assert rules(D(E(key="a"), E(key="A"))) == []


def test_failed_entry_key_not_counted_as_seen_pin():
    assert fail_rules(D(E(key="a", type="nope"), E(key="a"))) == ["schema.unknown-type"]  # PIN


def test_duplicate_empty_keys():
    assert only(D(E(key=""), E(key=""))) == "schema.duplicate-key"


# ---------------------------------------------------------------- other
def test_default_value_not_validated_pin():
    assert rules(D(E(default="str"), E(key="b", type="bool", default=5))) == []  # PIN


def test_does_not_mutate_input():
    d = D(E(key="a", ui={"zzz": 1}), E(key="a", type="nope"), 5)
    before = copy.deepcopy(d)
    validate_schema_document(d, "f")
    assert d == before


# ---------------------------------------------------------------- every rule id is produced (R14)
RULE_SRC = Path(sr.__file__).read_text(encoding="utf-8")
ALL_RULE_IDS = sorted(set(re.findall(r'"(schema\.[a-z-]+)"', RULE_SRC)))

PRODUCING_CASES = [
    D(E(type="nope")),
    D(E(kind="zzz")),
    D(E(description=" ")),
    D(E(zzz=1)),
    D(E(ref="a/b")),
    D(E(keyRef="a/b")),
    D(E(type="ref")),
    D(E(type="ref", ref="x")),
    D(E(type="map", keyRef="a/b", valueType="ref")),
    D(E(type="enum")),
    D(E(payload=[])),
    D(E(min=NAN)),
    D(E(ui={"zzz": 1})),
    D(E(), E()),
    D(5),
    D(E(key=5)),
    D(E(), zzz=1),
    D(E(), entries=5),
    D(E(), **{"for": 5}),
    {"format": "x"},
    D(E(), version=2),
    [],
]


def _produced_ids():
    out = set()
    for doc in PRODUCING_CASES:
        out.update(f.rule for f in validate_schema_document(doc, "f"))
    out.update(f.rule for f in validate_schema_text("{", "f"))
    return out


def test_rule_ids_found_in_source():
    assert len(ALL_RULE_IDS) >= 20, ALL_RULE_IDS


@pytest.mark.parametrize("rule_id", ALL_RULE_IDS)
def test_every_rule_id_is_produced_by_some_case(rule_id):
    assert rule_id in _produced_ids()


def test_conformance_schema_fixtures_still_agree():
    # sanity: fixtures that the C++ loader accepts have no FAIL from the (stricter) Python rules
    from dia_cli.utils.repo_root import find_repo_root
    conf = find_repo_root(__file__) / "Dia" / "DiaValueSchema" / "Testing" / "Conformance"
    expected = json.loads((conf / "expected.json").read_text(encoding="utf-8"))
    for name, exp in expected.items():
        if not name.endswith(".schema.json") or not isinstance(exp, dict) or "ok" not in exp:
            continue
        out = [f for f in validate_schema_text((conf / name).read_text(encoding="utf-8"), name) if f.severity == "FAIL"]
        assert bool(out) == (not exp["ok"]), (name, [f.rule for f in out])
