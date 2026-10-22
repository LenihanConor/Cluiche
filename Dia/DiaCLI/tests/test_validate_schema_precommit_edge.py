"""dia docs precommit schema section: candidate selection, reporting and heading text, P1-P7 (B1, B6)."""
from __future__ import annotations

import json
from pathlib import Path

import pytest
from click.testing import CliRunner

from dia_cli.commands.docs import precommit_cmd
from dia_cli.utils.repo_root import find_repo_root
from tests.vs_helpers import entry, put, schema

A = "Cluiche/Assets/App/"
ECO = schema("eco.params", entry("rate", min=0, max=1, default=0.5))
GOOD = {"format": "eco.params", "version": 1, "rate": 0.5}
BAD = {"format": "eco.params", "version": 1, "rate": 7}


def tree(root: Path, data=GOOD):
    return put(root, A + "eco.schema.json", ECO), put(root, A + "eco.json", data)


def cands(root, files):
    return [p.relative_to(root).as_posix() for p in precommit_cmd._schema_candidates(root, files)]


def run(tmp_path, monkeypatch, files):
    monkeypatch.setattr(precommit_cmd, "find_repo_root", lambda _f: tmp_path)
    monkeypatch.setattr(precommit_cmd, "_get_modified_files", lambda _r: files)
    return CliRunner().invoke(precommit_cmd.precommit, [])


# ---------------------------------------------------------------------------
# P1 deleted, P2 directory, P3 suffix case
# ---------------------------------------------------------------------------

def test_p1_deleted_files_are_not_candidates(tmp_path):
    s, d = tree(tmp_path)
    ghost = tmp_path / A / "ghost.json"
    ghost_schema = tmp_path / A / "ghost.schema.json"
    assert cands(tmp_path, [ghost, ghost_schema]) == []
    assert precommit_cmd._check_schema(tmp_path, [ghost, ghost_schema]) == ([], [])


def test_p1_deleted_data_file_does_not_hide_the_rest(tmp_path):
    s, d = tree(tmp_path, BAD)
    ghost = tmp_path / A / "ghost.json"
    errors, _ = precommit_cmd._check_schema(tmp_path, [ghost, d])
    assert len(errors) == 1 and "App/eco.json" in errors[0]


@pytest.mark.parametrize("name", ["x.json", "x.schema.json", "x.catalogue.json"])
def test_p2_directories_named_like_json_are_not_candidates(tmp_path, name):
    tree(tmp_path)
    d = tmp_path / A / name
    d.mkdir()
    assert cands(tmp_path, [d]) == []
    assert precommit_cmd._check_schema(tmp_path, [d]) == ([], [])


@pytest.mark.parametrize("suffix", [".JSON", ".Json", ".json"])
def test_p3_suffix_case_does_not_matter(tmp_path, suffix):
    tree(tmp_path)
    f = put(tmp_path, A + "Other" + suffix, BAD)
    assert cands(tmp_path, [f]) == ["Cluiche/Assets/App/Other" + suffix]
    errors, _ = precommit_cmd._check_schema(tmp_path, [f])
    assert len(errors) == 1 and "outside [0, 1]" in errors[0]


@pytest.mark.parametrize("name", ["X.SCHEMA.JSON", "x.Schema.Json"])
def test_p3_schema_suffix_case_does_not_matter(tmp_path, name):
    f = put(tmp_path, A + name, schema("eco.params", entry("rate", type="nonsense")))
    assert cands(tmp_path, [f]) == ["Cluiche/Assets/App/" + name]
    errors, _ = precommit_cmd._check_schema(tmp_path, [f])
    assert errors and errors[0].startswith("FAIL App/" + name)


@pytest.mark.parametrize("name", ["a.catalogue.json", "A.CATALOGUE.JSON"])
def test_p3_catalogues_are_never_candidates(tmp_path, name):
    tree(tmp_path)
    f = put(tmp_path, A + name, GOOD)
    assert cands(tmp_path, [f]) == []


@pytest.mark.parametrize("name", ["x.txt", "x.json.bak", "x.jsonc", "x", "x.diagame", "x.cpp"])
def test_p3_other_suffixes_are_not_candidates(tmp_path, name):
    tree(tmp_path)
    f = put(tmp_path, A + name, GOOD)
    assert cands(tmp_path, [f]) == []


# ---------------------------------------------------------------------------
# P4 JSON outside Assets; fixtures; same-named outside schemas (B6)
# ---------------------------------------------------------------------------

def test_p4_data_json_outside_assets_is_not_a_candidate(tmp_path):
    tree(tmp_path)
    for rel in ("docs/x.json", "Dia/x.json", "x.json", "Cluiche/Other/x.json", "Cluiche/Assets.json"):
        assert cands(tmp_path, [put(tmp_path, rel, BAD)]) == []


def test_p4_assets_root_level_json_with_format_is_a_candidate(tmp_path):
    f = put(tmp_path, "Cluiche/Assets/top.json", BAD)
    assert cands(tmp_path, [f]) == ["Cluiche/Assets/top.json"]


def test_p4_schema_outside_assets_is_validated_on_its_own(tmp_path):
    f = put(tmp_path, "docs/x.schema.json", schema("eco.params", entry("rate", type="nonsense")))
    errors, _ = precommit_cmd._check_schema(tmp_path, [f])
    assert len(errors) == 1 and errors[0].startswith("FAIL x.schema.json")


def test_b6_conformance_fixtures_are_not_candidates(tmp_path):
    conf = "Dia/DiaValueSchema/Testing/Conformance/"
    files = [put(tmp_path, conf + "bad-missing-key.schema.json", "{broken"),
             put(tmp_path, conf + "data/x.case.json", {"format": "x"}),
             put(tmp_path, "Dia/DiaValueSchema/Testing/other.schema.json", "{broken")]
    assert cands(tmp_path, files) == []
    assert precommit_cmd._check_schema(tmp_path, files) == ([], [])


def test_b6_real_repo_fixtures_are_not_candidates():
    root = find_repo_root(__file__)
    conf = root / "Dia" / "DiaValueSchema" / "Testing" / "Conformance"
    files = sorted(conf.rglob("*.json"))
    assert files, "fixtures missing"
    assert precommit_cmd._schema_candidates(root, files) == []


def test_b6_same_named_outside_schemas_do_not_collide(tmp_path):
    a = put(tmp_path, "docs/one/eco.schema.json", ECO)
    b = put(tmp_path, "docs/two/eco.schema.json", ECO)
    assert precommit_cmd._check_schema(tmp_path, [a, b]) == ([], [])


def test_b6_one_bad_of_two_same_named_outside_schemas_reports_once(tmp_path):
    a = put(tmp_path, "docs/one/eco.schema.json", ECO)
    b = put(tmp_path, "docs/two/eco.schema.json", schema("eco.params", entry("rate", type="nonsense")))
    errors, _ = precommit_cmd._check_schema(tmp_path, [a, b])
    assert len(errors) == 1


def test_b6_outside_schema_named_like_an_unchanged_asset_file_is_not_confused(tmp_path):
    put(tmp_path, "Cluiche/Assets/eco.schema.json", schema("eco.params", entry("rate", type="nonsense")))  # unchanged, bad
    good = put(tmp_path, "docs/eco.schema.json", ECO)
    assert precommit_cmd._check_schema(tmp_path, [good]) == ([], [])


def test_b6_outside_and_inside_changes_both_reported(tmp_path):
    s, d = tree(tmp_path, BAD)
    out = put(tmp_path, "docs/y.schema.json", schema("g", entry("rate", type="nonsense")))
    errors, _ = precommit_cmd._check_schema(tmp_path, [d, out])
    assert len(errors) == 2


# ---------------------------------------------------------------------------
# P5 cross-file refs with unchanged targets
# ---------------------------------------------------------------------------

BIOMES = schema("eco.biomes", entry("biomes[].species", type="ref", ref="eco.species/species[].id"))


def _ref_tree(root, species):
    put(root, A + "b.schema.json", BIOMES)
    put(root, A + "species.json", {"format": "eco.species", "version": 1, "species": [{"id": s} for s in species]})


def test_p5_pack_ref_to_unchanged_file_resolves(tmp_path):
    _ref_tree(tmp_path, ["wolf"])
    pack = put(tmp_path, A + "Packs/b.json", {"format": "eco.biomes", "version": 1, "biomes": [{"species": "wolf"}]})
    assert precommit_cmd._check_schema(tmp_path, [pack]) == ([], [])


def test_p5_pack_ref_to_unchanged_file_unresolved_is_reported(tmp_path):
    _ref_tree(tmp_path, ["wolf"])
    pack = put(tmp_path, A + "Packs/b.json", {"format": "eco.biomes", "version": 1, "biomes": [{"species": "wolfe"}]})
    errors, _ = precommit_cmd._check_schema(tmp_path, [pack])
    assert len(errors) == 1 and "Packs/b.json" in errors[0] and "wolf" in errors[0]


def test_p5_removing_an_id_breaks_an_unchanged_referrer_but_only_changed_files_are_reported(tmp_path):
    _ref_tree(tmp_path, [])
    put(tmp_path, A + "b.json", {"format": "eco.biomes", "version": 1, "biomes": [{"species": "wolf"}]})
    species = tmp_path / A / "species.json"
    errors, warnings = precommit_cmd._check_schema(tmp_path, [species])
    assert errors == []   # pin: the breakage is in unchanged b.json, which is not reported


# ---------------------------------------------------------------------------
# P6 format without schema, P7 files in schema'd folders
# ---------------------------------------------------------------------------

def test_p6_unschemad_format_is_warn_only_with_heading(tmp_path, monkeypatch):
    tree(tmp_path)
    m = put(tmp_path, A + "m.json", {"format": "mystery.thing", "version": 1})
    r = run(tmp_path, monkeypatch, [m])
    assert r.exit_code == 0, r.output
    assert "  WARN  schema:" in r.output and "FAIL  schema" not in r.output
    assert "WARN App/m.json: format `mystery.thing` has no schema" in r.output
    assert "Result: PASS (1 warning(s))" in r.output


def test_p7_json_without_format_in_schemad_folder_is_a_candidate_and_fails(tmp_path):
    s, d = tree(tmp_path)
    f = put(tmp_path, A + "loose.json", {"rate": 1})
    assert cands(tmp_path, [f]) == ["Cluiche/Assets/App/loose.json"]
    errors, _ = precommit_cmd._check_schema(tmp_path, [f])
    assert len(errors) == 1 and "no `format`" in errors[0]


@pytest.mark.parametrize("content", ["[]", "null", "42", '"s"', '{"format": 5}', "{broken", "", b"\xff\xfe"],
                         ids=["array", "null", "number", "string", "format-int", "broken", "empty", "non-utf8"])
def test_p7_odd_json_in_schemad_folder_fails_without_crashing(tmp_path, content):
    tree(tmp_path)
    f = put(tmp_path, A + "sub/odd.json", content)
    errors, warnings = precommit_cmd._check_schema(tmp_path, [f])
    assert len(errors) == 1 and errors[0].startswith("FAIL App/sub/odd.json") and warnings == []


@pytest.mark.parametrize("content", ["[]", "{broken", b"\xff\xfe", {"a": 1}, ""])
def test_p7_odd_json_outside_schemad_folders_is_not_a_candidate(tmp_path, content):
    tree(tmp_path)
    f = put(tmp_path, "Cluiche/Assets/Other/odd.json", content)
    assert cands(tmp_path, [f]) == []


def test_p7_schema_in_a_parent_folder_makes_nested_files_candidates(tmp_path):
    tree(tmp_path)
    f = put(tmp_path, A + "a/b/c/loose.json", {"x": 1})
    assert len(cands(tmp_path, [f])) == 1


def test_p7_schema_in_a_sibling_folder_does_not(tmp_path):
    put(tmp_path, A + "Eco/eco.schema.json", ECO)
    f = put(tmp_path, A + "Sib/loose.json", {"x": 1})
    assert cands(tmp_path, [f]) == []


def test_p7_assets_root_schema_covers_everything_below(tmp_path):
    put(tmp_path, "Cluiche/Assets/root.schema.json", ECO)
    f = put(tmp_path, "Cluiche/Assets/App/deep/loose.json", {"x": 1})
    assert len(cands(tmp_path, [f])) == 1


def test_p7_non_utf8_schema_is_reported_not_a_crash(tmp_path):
    s = put(tmp_path, A + "bad.schema.json", b"\xff\xfe\x00")
    errors, _ = precommit_cmd._check_schema(tmp_path, [s])
    assert len(errors) == 1 and "FAIL App/bad.schema.json" in errors[0]


def test_p7_deeply_nested_candidate_is_reported_not_a_crash(tmp_path):
    tree(tmp_path)
    f = put(tmp_path, A + "deep.json", "[" * 100000)
    s = put(tmp_path, A + "deep.schema.json", "[" * 100000)
    errors, _ = precommit_cmd._check_schema(tmp_path, [f, s])
    assert len(errors) == 2


# ---------------------------------------------------------------------------
# Headings, counts, unchanged files
# ---------------------------------------------------------------------------

def test_heading_fail_with_counts(tmp_path, monkeypatch):
    s, d = tree(tmp_path, BAD)
    r = run(tmp_path, monkeypatch, [d])
    assert r.exit_code == 1
    assert "  FAIL  schema:" in r.output
    assert "        FAIL App/eco.json: " in r.output
    assert "Result: 1 ERROR(s), 0 WARNING(s)" in r.output


def test_heading_is_fail_when_errors_and_warnings_mix(tmp_path, monkeypatch):
    s, d = tree(tmp_path, BAD)
    m = put(tmp_path, A + "m.json", {"format": "mystery.thing", "version": 1})
    r = run(tmp_path, monkeypatch, [d, m])
    assert r.exit_code == 1
    assert "  FAIL  schema:" in r.output and "  WARN  schema" not in r.output
    assert "WARN App/m.json" in r.output
    assert "Result: 1 ERROR(s), 1 WARNING(s)" in r.output


def test_heading_ok_when_clean(tmp_path, monkeypatch):
    s, d = tree(tmp_path)
    r = run(tmp_path, monkeypatch, [s, d])
    assert r.exit_code == 0
    assert "  OK    schema\n" in r.output and "Result: PASS\n" in r.output


def test_no_schema_line_at_all_without_candidates(tmp_path, monkeypatch):
    tree(tmp_path)
    r = run(tmp_path, monkeypatch, [put(tmp_path, "Dia/x.cpp", "int x;")])
    assert r.exit_code == 0 and "schema" not in r.output.replace("dia precommit", "")


def test_unchanged_file_failures_are_not_reported(tmp_path, monkeypatch):
    s, d = tree(tmp_path, BAD)
    other = put(tmp_path, A + "fine.json", GOOD)
    r = run(tmp_path, monkeypatch, [other])
    assert r.exit_code == 0 and "  OK    schema" in r.output and "eco.json" not in r.output


def test_multiple_errors_count_each_finding(tmp_path, monkeypatch):
    s = put(tmp_path, A + "eco.schema.json", ECO)
    d1 = put(tmp_path, A + "one.json", BAD)
    d2 = put(tmp_path, A + "two.json", BAD)
    r = run(tmp_path, monkeypatch, [d1, d2])
    assert "Result: 2 ERROR(s)" in r.output


def test_deleted_only_changeset_has_no_schema_section(tmp_path, monkeypatch):
    tree(tmp_path)
    r = run(tmp_path, monkeypatch, [tmp_path / A / "gone.json"])
    assert r.exit_code == 0 and "schema" not in r.output.replace("dia precommit", "")
