"""Conformance fixtures for sidecar types `enum[]` and `ref[2][]` (biome-registry plan Task 7a).

The fixtures live under Conformance/pending-cpp/ because the C++ ValueSchemaLoader does not know the new types yet
(Task 7b). The C++ conformance GoogleTest only reads the Conformance top level and data/, so it ignores this folder.
Task 7b moves the files into the top level / data/ (merging expected.json) and deletes this test.
"""
from __future__ import annotations

import json

import pytest

from dia_cli.commands.validate.data_rules import DataContext, build_schema_index, validate_data_document
from dia_cli.commands.validate.schema_rules import validate_schema_text
from dia_cli.utils.repo_root import find_repo_root

PENDING = find_repo_root(__file__) / "Dia" / "DiaValueSchema" / "Testing" / "Conformance" / "pending-cpp"
EXPECTED = json.loads((PENDING / "expected.json").read_text(encoding="utf-8"))["cases"]
CASES = sorted((PENDING / "data").glob("*.case.json"))


def test_expected_matches_files():
    assert {p.name for p in PENDING.glob("*.schema.json")} == set(EXPECTED)
    assert len(CASES) == 7


@pytest.mark.parametrize("name", sorted(EXPECTED))
def test_schema_fixture(name):
    c = EXPECTED[name]
    text = (PENDING / name).read_text(encoding="utf-8")
    fails = [f for f in validate_schema_text(text, name) if f.severity == "FAIL"]
    assert (not fails) == c["ok"], [f.message for f in fails]
    if c["ok"]:
        assert len(json.loads(text)["entries"]) == c["entryCount"]
    else:
        assert len(fails) == 1 and fails[0].key == c["failedEntry"]
        assert c["reasonContains"] in fails[0].message


@pytest.mark.parametrize("case_file", CASES, ids=[c.name for c in CASES])
def test_data_case(case_file):
    case = json.loads(case_file.read_text(encoding="utf-8"))
    for s in case["schemas"]:
        assert [f for f in validate_schema_text(json.dumps(s), "s") if f.severity == "FAIL"] == []
    index = build_schema_index(case["schemas"])
    ctx = DataContext.from_files([(fl["name"], fl["content"], fl["pack"]) for fl in case["files"]])
    actual = []
    for fl in case["files"]:
        actual += validate_data_document(fl["content"], fl["name"], index, is_pack=fl["pack"], context=ctx)
    got = sorted((f.severity, f.rule, f.file, f.key, f.path, f.record, f.suggestion) for f in actual)
    want = sorted((e["severity"], e["rule"], e["file"], e["key"], e["path"], e["record"], e["suggestion"])
                  for e in case["expected"])
    assert got == want, [(f.path, f.message) for f in actual]
    for exp in case["expected"]:
        assert any(exp["messageContains"] in f.message and f.path == exp["path"] for f in actual), exp


def _doc(entry):
    return json.dumps({"format": "dia.value_schema", "version": 1, "for": "t", "entries": [entry]})


def _entry(**kw):
    d = {"key": "k", "kind": "parameter", "label": "L", "description": "D"}
    d.update(kw)
    return d


def test_enum_list_requires_values_and_rejects_ref():
    f = [x for x in validate_schema_text(_doc(_entry(type="enum[]")), "f") if x.severity == "FAIL"]
    assert f[0].rule == "schema.enum-no-values"
    f = [x for x in validate_schema_text(_doc(_entry(type="enum[]", values=[{"id": "a"}], ref="a/b")), "f")
         if x.severity == "FAIL"]
    assert f[0].rule == "schema.ref-misuse"


def test_ref_pair_requires_wellformed_ref():
    f = [x for x in validate_schema_text(_doc(_entry(type="ref[2][]", ref="nofmt")), "f") if x.severity == "FAIL"]
    assert f[0].rule == "schema.ref-malformed"


def test_enum_list_non_string_member_is_wrong_type():
    s = json.loads(_doc(_entry(key="t", type="enum[]", values=[{"id": "a"}])))
    s["for"] = "x.y"
    idx = build_schema_index([s])
    out = validate_data_document({"format": "x.y", "version": 1, "t": ["a", 3]}, "d.json", idx, is_pack=False)
    assert [(f.rule, f.path) for f in out] == [("data.wrong-type", "t[1]")]


def test_ref_pair_non_string_element_is_wrong_type():
    s = json.loads(_doc(_entry(key="p", type="ref[2][]", ref="x.y/ids[].id")))
    s["for"] = "x.y"
    idx = build_schema_index([s])
    out = validate_data_document({"format": "x.y", "version": 1, "p": [["a", 1]]}, "d.json", idx, is_pack=False)
    assert [(f.rule, f.path) for f in out] == [("data.wrong-type", "p[0]")]
