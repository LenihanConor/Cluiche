"""Parity between the Python validator and the shared Conformance fixtures / C++ loader (Q1-Q3).

Q1: the fixture folder is consumed exactly: every `*.schema.json` + `expected.json` entry, every
    `data/*.case.json`; nothing in the folder is unaccounted for, nothing is silently skipped.
Q2: the Python verdict equals the recorded verdict for every consumed fixture.
Q3: intentional strictness differences between Python and the C++ ValueSchemaLoader are pinned.
"""
from __future__ import annotations

import json
import re
from pathlib import Path

import pytest

from dia_cli.commands.validate.data_rules import (
    DataContext, build_schema_index, validate_data_document, validate_schema_references)
from dia_cli.commands.validate.schema_rules import validate_schema_document, validate_schema_text
from dia_cli.utils.repo_root import find_repo_root

ROOT = find_repo_root(__file__)
CONF = ROOT / "Dia" / "DiaValueSchema" / "Testing" / "Conformance"
DATA = CONF / "data"
LOADER_CPP = ROOT / "Dia" / "DiaValueSchema" / "ValueSchemaLoader.cpp"
REGISTRY_CPP = ROOT / "Dia" / "DiaValueSchema" / "ValueSchemaRegistry.cpp"
CPP_TEST = ROOT / "Cluiche" / "Tests" / "GoogleTests" / "DiaValueSchema" / "TestValueSchemaConformance.cpp"


def folder_index() -> dict:
    """Everything the fixture folder contains, derived from the folder itself."""
    top = sorted(p.name for p in CONF.iterdir())
    return {
        "schemas": sorted(n for n in top if n.endswith(".schema.json")),
        "cases": sorted(p.name for p in DATA.iterdir() if p.name.endswith(".case.json")),
        "top_other": sorted(n for n in top if not n.endswith(".schema.json")),
        "data_other": sorted(p.name for p in DATA.iterdir() if not p.name.endswith(".case.json")),
    }


INDEX = folder_index()
EXPECTED = json.loads((CONF / "expected.json").read_text(encoding="utf-8"))


def norm(x) -> tuple:
    if isinstance(x, dict):
        return (x["severity"], x["rule"], x["file"], x["key"], x["path"], x["record"], x["suggestion"])
    return (x.severity, x.rule, x.file, x.key, x.path, x.record, x.suggestion)


def evaluate_case(case: dict) -> list:
    """The findings the Python rules produce for a data case file."""
    index = build_schema_index(case["schemas"])
    ctx = DataContext.from_files([(f["name"], f["content"], f["pack"]) for f in case["files"]])
    actual = []
    for f in case["files"]:
        actual += validate_data_document(f["content"], f["name"], index, is_pack=f["pack"], context=ctx)
    names = {id(s): f"schemas[{i}]" for i, s in enumerate(case["schemas"])}
    actual += validate_schema_references(case["schemas"], names)
    return actual


# ---------------------------------------------------------------------------
# Q1 exact consumption
# ---------------------------------------------------------------------------

def test_q1_folder_contains_only_known_kinds_of_file():
    assert INDEX["top_other"] == ["README.md", "data", "expected.json", "ramps"]
    assert INDEX["data_other"] == ["README.md"]


def test_q1_expected_json_header_and_keys_match_the_schema_files_exactly():
    assert EXPECTED["format"] == "dia.value_schema.conformance" and EXPECTED["version"] == 1
    assert sorted(EXPECTED["cases"]) == INDEX["schemas"]
    assert len(INDEX["schemas"]) == len(set(INDEX["schemas"])) > 0


def test_q1_every_expected_entry_is_well_formed():
    for name, c in EXPECTED["cases"].items():
        assert isinstance(c["ok"], bool), name
        allowed = {"ok", "entryCount", "for", "warnings"} if c["ok"] else {"ok", "failedEntry", "reasonContains"}
        assert set(c) <= allowed, (name, set(c) - allowed)
        if not c["ok"]:
            assert {"failedEntry", "reasonContains"} <= set(c), name
        assert (name.startswith("good-")) == c["ok"], name


def test_q1_both_verdicts_are_represented():
    oks = [c["ok"] for c in EXPECTED["cases"].values()]
    assert any(oks) and not all(oks)


def test_q1_case_files_are_exactly_the_data_folder_listing_and_all_are_consumed():
    consumed = []
    for name in INDEX["cases"]:
        case = json.loads((DATA / name).read_text(encoding="utf-8"))
        assert set(case) - {"schemaExpected"} == {"description", "schemas", "files", "expected"}, name
        assert case["description"].strip(), name
        got = sorted(norm(a) for a in evaluate_case(case))
        want = sorted(norm(e) for e in case["expected"] + case.get("schemaExpected", []))
        assert got == want, name
        consumed.append(name)
    assert consumed == INDEX["cases"] and len(consumed) == len(set(consumed))
    assert len(consumed) == len(list(DATA.glob("*.case.json")))


def test_q1_python_parametrised_data_test_sees_the_same_cases():
    from tests import test_validate_schema_data as data_tests
    assert [c.name for c in data_tests.CASES] == INDEX["cases"]


def test_q1_python_parametrised_rules_test_sees_the_same_schema_files():
    from tests import test_validate_schema_rules as rules_tests
    assert sorted(rules_tests._expected()) == INDEX["schemas"]


def test_q1_expected_rows_have_every_documented_field():
    keys = {"severity", "rule", "file", "key", "path", "record", "messageContains", "suggestion"}
    for name in INDEX["cases"]:
        case = json.loads((DATA / name).read_text(encoding="utf-8"))
        for row in case["expected"] + case.get("schemaExpected", []):
            assert set(row) == keys, (name, set(row) ^ keys)
            assert row["severity"] in ("FAIL", "WARN")


def test_q1_every_rule_id_in_the_readme_has_a_fixture_and_vice_versa():
    readme = (DATA / "README.md").read_text(encoding="utf-8")
    section = readme.split("## Rule ids", 1)[1].split("## Rules", 1)[0]
    documented = set(re.findall(r"`((?:data|schema)\.[a-z-]+)`", section))
    used = set()
    for name in INDEX["cases"]:
        case = json.loads((DATA / name).read_text(encoding="utf-8"))
        used |= {r["rule"] for r in case["expected"] + case.get("schemaExpected", [])}
    assert documented == used, (documented ^ used)


def test_q1_cpp_conformance_test_consumes_the_same_selection():
    """The C++ test lists `*.schema.json` in the folder top level and reads expected.json: same selection."""
    text = CPP_TEST.read_text(encoding="utf-8")
    assert '".schema.json"' in text and "expected.json" in text and "directory_iterator(dir)" in text


# ---------------------------------------------------------------------------
# Q2 verdict parity per schema fixture
# ---------------------------------------------------------------------------

@pytest.mark.parametrize("name", INDEX["schemas"])
def test_q2_python_verdict_equals_recorded_verdict(name):
    c = EXPECTED["cases"][name]
    text = (CONF / name).read_text(encoding="utf-8")
    fs = validate_schema_text(text, name)
    fails = [f for f in fs if f.severity == "FAIL"]
    warns = [f for f in fs if f.severity == "WARN"]
    assert (not fails) == c["ok"], [f.message for f in fails]
    if c["ok"]:
        doc = json.loads(text)
        assert len(warns) == c.get("warnings", 0)
        if "entryCount" in c:
            assert len(doc["entries"]) == c["entryCount"]
        if "for" in c:
            assert doc["for"] == c["for"]
    else:
        assert fails[0].key == c["failedEntry"] and c["reasonContains"] in fails[0].message
        assert fails[0].file == name


# ---------------------------------------------------------------------------
# Q3 intentional strictness differences from the C++ loader
# ---------------------------------------------------------------------------

def _entry(**o):
    e = {"key": "k", "kind": "parameter", "label": "L", "description": "D", "type": "float"}
    e.update(o)
    return e


def _doc(*entries):
    return {"format": "dia.value_schema", "version": 1, "for": "x", "entries": list(entries)}


@pytest.mark.parametrize("desc", ["", " ", "\t\n", "  "])
def test_q3_empty_or_blank_description_fails_in_python(desc):
    fs = validate_schema_document(_doc(_entry(description=desc)), "f.json")
    assert [(f.severity, f.rule, f.key) for f in fs] == [("FAIL", "schema.empty-description", "k")]


def test_q3_cpp_loader_accepts_an_empty_description_so_python_is_stricter():
    """RequireString only checks presence and string-ness; there is no emptiness check (pin the C++ side)."""
    src = LOADER_CPP.read_text(encoding="utf-8")
    body = src.split("bool RequireString", 1)[1].split("template<class T>", 1)[0]
    assert "isString" in body and "empty" not in body and "strlen" not in body


@pytest.mark.parametrize("field", ["label", "key"])
def test_q3_empty_label_and_key_are_accepted_by_python_like_cpp_pin(field):
    assert validate_schema_document(_doc(_entry(**{field: ""})), "f.json") == []


def test_q3_duplicate_full_key_in_one_file_fails_in_python():
    fs = validate_schema_document(_doc(_entry(), _entry(label="other")), "f.json")
    assert [(f.severity, f.rule, f.key) for f in fs] == [("FAIL", "schema.duplicate-key", "k")]
    assert "duplicate key 'x/k'" in fs[0].message


def test_q3_cpp_loader_has_no_duplicate_check_the_registry_resolves_it_first_wins():
    loader = LOADER_CPP.read_text(encoding="utf-8").lower()
    registry = REGISTRY_CPP.read_text(encoding="utf-8")
    assert "duplicate" not in loader
    assert "Duplicate key" in registry and "first wins" in registry


def test_q3_python_reports_every_bad_entry_where_cpp_stops_at_the_first():
    fs = validate_schema_document(_doc(_entry(type="zz"), _entry(key="k2", kind="zz"), _entry(key="k3", label=5)),
                                  "f.json")
    assert [f.key for f in fs if f.severity == "FAIL"] == ["k", "k2", "k3"]
    # the C++ loader's Fail() rejects the whole file on the first problem and clears the entries
    src = LOADER_CPP.read_text(encoding="utf-8")
    assert "r.mEntries.RemoveAll();" in src and "bool Fail(" in src


def test_q3_one_finding_per_entry_even_with_several_problems_in_it():
    fs = validate_schema_document(_doc(_entry(type="zz", kind="zz", label=1)), "f.json")
    assert len([f for f in fs if f.key == "k"]) == 1


def test_q3_conformance_fixture_with_a_later_bad_entry_agrees_on_the_verdict():
    name = "bad-later-entry.schema.json"
    c = EXPECTED["cases"][name]
    fs = [f for f in validate_schema_text((CONF / name).read_text(encoding="utf-8"), name) if f.severity == "FAIL"]
    assert not c["ok"] and fs and fs[0].key == c["failedEntry"]
