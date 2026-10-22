"""dia validate schema: discovery edge cases X1-X19 (encodings, packs, ordering, aliases, targets, scale)."""
from __future__ import annotations

import json
import os
import time
from pathlib import Path

import pytest

from dia_cli.commands.validate import schema_discovery
from tests.vs_helpers import basic_tree, entry, findings, invoke, put, repo, rules_by_file, schema

ECO = schema("eco.params", entry("rate", min=0, max=1, default=0.5))
DATA = {"format": "eco.params", "version": 1, "rate": 0.5}


def patch_repo(monkeypatch, root):
    monkeypatch.setattr(schema_discovery, "get_repo_root", lambda: root)


# ---------------------------------------------------------------------------
# X1 non-UTF8, X2 BOM / CRLF, X3 empty
# ---------------------------------------------------------------------------

BAD_BYTES = [b"\xff\xfe\x00{", b"\x80\x81\x82", b'{"format": "\xe9"}', b"\xef\xbb\xbf\xff"]


@pytest.mark.parametrize("raw", BAD_BYTES, ids=["utf16-bom", "continuation", "latin1-e-acute", "bom-then-bad"])
def test_x1_non_utf8_schema_is_fail_and_run_continues(tmp_path, raw):
    basic_tree(tmp_path, rate=7)  # a real failure elsewhere must still be reported
    put(tmp_path, "Eco/broken.schema.json", raw)
    r = invoke([str(tmp_path)], json_out=True)
    assert r.exit_code == 1, (r.stdout, r.stderr, r.exception)
    assert r.exception is None or isinstance(r.exception, SystemExit)
    by = rules_by_file(json.loads(r.stdout))
    assert by["Eco/broken.schema.json"] == ["schema.unreadable"]
    assert "data.out-of-range" in by["Eco/eco.json"]


@pytest.mark.parametrize("raw", BAD_BYTES, ids=["utf16-bom", "continuation", "latin1-e-acute", "bom-then-bad"])
def test_x1_non_utf8_data_in_schemad_folder_is_fail(tmp_path, raw):
    basic_tree(tmp_path)
    put(tmp_path, "Eco/bad.json", raw)
    by = rules_by_file(findings([str(tmp_path)]))
    assert by == {"Eco/bad.json": ["data.invalid-json"]}


def test_x1_non_utf8_data_outside_schemad_folder_is_ignored(tmp_path):
    basic_tree(tmp_path)
    put(tmp_path, "Other/bad.json", b"\xff\xff")
    assert findings([str(tmp_path)]) == []


@pytest.mark.parametrize("name", ["x.catalogue.json", "node_modules/x.json"])
def test_x1_non_utf8_in_ignored_files_is_not_read(tmp_path, name):
    basic_tree(tmp_path)
    put(tmp_path, "Eco/" + name, b"\xff\xff")
    assert findings([str(tmp_path)]) == []


@pytest.mark.parametrize("sep", ["\n", "\r\n"])
@pytest.mark.parametrize("bom", [b"", b"\xef\xbb\xbf"], ids=["nobom", "bom"])
def test_x2_bom_and_crlf_are_clean(tmp_path, bom, sep):
    for rel, doc in (("Eco/eco.schema.json", ECO), ("Eco/eco.json", DATA)):
        text = json.dumps(doc, indent=2).replace("\n", sep)
        put(tmp_path, rel, bom + text.encode("utf-8"))
    r = invoke([str(tmp_path), "--verbose"])
    assert r.exit_code == 0, r.stdout
    assert "OK   Eco/eco.json (1 keys)" in r.stdout
    assert "0 FAIL, 0 WARN" in r.stdout


@pytest.mark.parametrize("content", ["", "   ", "\n\r\n", "﻿"], ids=["empty", "spaces", "newlines", "bom-only"])
def test_x3_empty_schema_fails_unparsable(tmp_path, content):
    basic_tree(tmp_path)
    put(tmp_path, "Eco/empty.schema.json", content)
    by = rules_by_file(findings([str(tmp_path)]))
    assert by == {"Eco/empty.schema.json": ["schema.unparsable-json"]}


@pytest.mark.parametrize("content", ["", "   ", "\n\r\n", "﻿"], ids=["empty", "spaces", "newlines", "bom-only"])
def test_x3_empty_data_in_schemad_folder_fails_empty_elsewhere_ignored(tmp_path, content):
    basic_tree(tmp_path)
    put(tmp_path, "Eco/empty.json", content)
    put(tmp_path, "Other/empty.json", content)
    assert rules_by_file(findings([str(tmp_path)])) == {"Eco/empty.json": ["data.invalid-json"]}


@pytest.mark.parametrize("depth", [2000, 100000])
def test_b2_deeply_nested_json_is_unparsable_not_a_crash(tmp_path, depth):
    basic_tree(tmp_path)
    deep = "[" * depth + "]" * depth
    put(tmp_path, "Eco/deep.json", deep)
    put(tmp_path, "Eco/deep.schema.json", deep)
    r = invoke([str(tmp_path)], json_out=True)
    assert r.exit_code == 1, (r.stdout[:300], r.exception)
    by = rules_by_file(json.loads(r.stdout))
    assert by["Eco/deep.json"] == ["data.invalid-json"]
    assert by["Eco/deep.schema.json"] == ["schema.unparsable-json"]


def test_b2_deeply_nested_manifest_is_skipped(tmp_path, monkeypatch):
    root = repo(tmp_path, ("App",))
    app = root / "Cluiche" / "Assets" / "App"
    put(app, "deep.diagame", "[" * 100000)
    basic_tree(app)
    patch_repo(monkeypatch, root)
    r = invoke(["--target", "App"])
    assert r.exit_code == 0, (r.stdout, r.exception)


def test_b2_nested_but_parsable_data_does_not_crash(tmp_path):
    basic_tree(tmp_path)
    depth = 900
    doc = {"format": "eco.params", "version": 1, "rate": 0.5, "x": json.loads("[" * depth + "]" * depth)}
    put(tmp_path, "Eco/nest.json", json.dumps(doc))
    r = invoke([str(tmp_path)], json_out=True)
    assert r.exit_code in (0, 1) and (r.exception is None or isinstance(r.exception, SystemExit)), r.exception


# ---------------------------------------------------------------------------
# X4 non-object data in a schema'd folder
# ---------------------------------------------------------------------------

@pytest.mark.parametrize("text", ["[]", "[1,2]", "null", "42", '"str"', "true", '{"format": 5}', '{"format": null}',
                                  '{"format": ["a"]}', "{}"])
def test_x4_non_object_or_bad_format_in_schemad_folder_is_no_format(tmp_path, text):
    basic_tree(tmp_path)
    put(tmp_path, "Eco/odd.json", text)
    assert rules_by_file(findings([str(tmp_path)])) == {"Eco/odd.json": ["data.no-format"]}


# ---------------------------------------------------------------------------
# X5 Packs detection (B4)
# ---------------------------------------------------------------------------

PACK_DATA = {"format": "eco.params", "version": 1, "rate": 0.9}


def _mismatch_files(args):
    return sorted(f["file"] for f in findings(args) if f["rule"] == "data.default-mismatch")


@pytest.mark.parametrize("seg", ["Packs", "packs", "PACKS", "PaCkS"])
def test_x5_packs_segment_case_insensitive(tmp_path, seg):
    put(tmp_path, "Eco/eco.schema.json", schema("eco.params", entry("rate", min=0, max=1, default=0.5)))
    put(tmp_path, f"Eco/{seg}/p.json", PACK_DATA)
    put(tmp_path, f"Eco/{seg}/deep/er/q.json", PACK_DATA)
    put(tmp_path, "Eco/plain.json", PACK_DATA)
    assert _mismatch_files([str(tmp_path)]) == ["Eco/plain.json"]


@pytest.mark.parametrize("seg", ["Packs.json", "NotPacks", "Packs2", "MyPacks", "Pack", "packs.d"])
def test_x5_only_a_whole_segment_named_packs_is_a_pack(tmp_path, seg):
    put(tmp_path, "Eco/eco.schema.json", schema("eco.params", entry("rate", min=0, max=1, default=0.5)))
    if seg.endswith(".json"):
        put(tmp_path, f"Eco/{seg}", PACK_DATA)
    else:
        put(tmp_path, f"Eco/{seg}/p.json", PACK_DATA)
    assert len(_mismatch_files([str(tmp_path)])) == 1


@pytest.mark.parametrize("name", ["Packs", "packs", "PACKS"])
def test_b4_folder_path_that_is_itself_packs(tmp_path, name):
    folder = tmp_path / name
    put(folder, "eco.schema.json", schema("eco.params", entry("rate", min=0, max=1, default=0.5)))
    put(folder, "p.json", PACK_DATA)
    put(folder, "sub/q.json", PACK_DATA)
    assert _mismatch_files([str(folder)]) == []


def test_b4_folder_not_named_packs_still_checks_defaults(tmp_path):
    folder = tmp_path / "Data"
    put(folder, "eco.schema.json", schema("eco.params", entry("rate", min=0, max=1, default=0.5)))
    put(folder, "p.json", PACK_DATA)
    assert _mismatch_files([str(folder)]) == ["p.json"]


def test_b4_explicit_file_under_packs_subfolder_matches_folder_mode(tmp_path):
    sch = put(tmp_path, "Eco/eco.schema.json", schema("eco.params", entry("rate", min=0, max=1, default=0.5)))
    f = put(tmp_path, "Eco/Packs/sub/x.json", PACK_DATA)
    g = put(tmp_path, "Eco/Other/y.json", PACK_DATA)
    assert _mismatch_files([str(tmp_path)]) == ["Eco/Other/y.json"]
    assert _mismatch_files([str(sch), str(f)]) == []        # consistent with folder mode: pack
    assert _mismatch_files([str(sch), str(g)]) == ["y.json"]  # and a plain file is a defaults file


def test_b4_explicit_file_directly_in_packs(tmp_path):
    sch = put(tmp_path, "eco.schema.json", schema("eco.params", entry("rate", min=0, max=1, default=0.5)))
    f = put(tmp_path, "Packs/x.json", PACK_DATA)
    assert _mismatch_files([str(sch), str(f)]) == []


# ---------------------------------------------------------------------------
# X6 cross-format ref to a format with data but no schema (B5)
# ---------------------------------------------------------------------------

BIOMES = schema("eco.biomes", entry("biomes[].species", type="ref", ref="eco.species/species[].id"))


def test_b5_data_without_schema_is_still_an_id_source(tmp_path):
    put(tmp_path, "A/b.schema.json", BIOMES)
    put(tmp_path, "A/species.json", {"format": "eco.species", "version": 1, "species": [{"id": "wolf"}]})
    put(tmp_path, "A/biomes.json", {"format": "eco.biomes", "version": 1,
                                    "biomes": [{"species": "wolf"}, {"species": "wolfe"}]})
    data = findings([str(tmp_path)])
    by = {(f["file"], f["rule"]) for f in data}
    assert ("A/species.json", "data.no-schema-for-format") in by          # the WARN stays
    unresolved = [f for f in data if f["rule"] == "data.unresolved-ref"]
    assert len(unresolved) == 1 and "wolfe" in unresolved[0]["message"]    # only the typo; wolf resolves
    assert not any(f["rule"] == "data.ref-format-missing" for f in data)


def test_b5_pack_without_schema_also_provides_ids(tmp_path):
    put(tmp_path, "A/b.schema.json", BIOMES)
    put(tmp_path, "A/Packs/species.json", {"format": "eco.species", "version": 1, "species": [{"id": "wolf"}]})
    put(tmp_path, "A/biomes.json", {"format": "eco.biomes", "version": 1, "biomes": [{"species": "wolf"}]})
    data = findings([str(tmp_path)])
    assert [f["rule"] for f in data if f["severity"] == "FAIL"] == []


def test_b5_no_data_at_all_gives_ref_format_missing_warn(tmp_path):
    put(tmp_path, "A/b.schema.json", BIOMES)
    put(tmp_path, "A/biomes.json", {"format": "eco.biomes", "version": 1, "biomes": [{"species": "wolf"}]})
    data = findings([str(tmp_path)])
    assert [f["rule"] for f in data] == ["data.ref-format-missing"]


# ---------------------------------------------------------------------------
# X7 same `for` in two schema files; X19 same format in two folders
# ---------------------------------------------------------------------------

def test_x7_same_for_merges_first_by_sorted_name_wins_and_cross_file_duplicate_fails(tmp_path):
    put(tmp_path, "A/a.schema.json", schema("eco.params", entry("rate", min=0, max=1, default=0.5)))
    put(tmp_path, "B/b.schema.json", schema("eco.params", entry("rate", min=0, max=5, default=0.5),
                                            entry("extra", min=0, max=1, default=0.5)))
    put(tmp_path, "C/d.json", {"format": "eco.params", "version": 1, "rate": 3, "extra": 0.5})
    data = findings([str(tmp_path)])
    got = sorted((f["file"], f["rule"]) for f in data)
    assert ("B/b.schema.json", "schema.duplicate-key-across-files") in got
    # first wins: rate max is 1 (from A), so 3 is out of range; `extra` (only in B) is merged in and fine
    assert ("C/d.json", "data.out-of-range") in got
    assert not any(f["file"] == "C/d.json" and f["key"] == "extra" for f in data)
    assert not any(f["file"] == "A/a.schema.json" for f in data)


def test_x7_duplicate_between_same_named_explicit_files_in_different_dirs(tmp_path):
    a = put(tmp_path, "one/eco.schema.json", ECO)
    b = put(tmp_path, "two/eco.schema.json", ECO)
    data = findings([str(a), str(b)])
    assert [f["rule"] for f in data] == ["schema.duplicate-key-across-files"]


def test_x19_same_format_in_two_folders_both_validated_by_format(tmp_path):
    basic_tree(tmp_path, rate=7, folder="Eco")
    put(tmp_path, "Elsewhere/also.json", {"format": "eco.params", "version": 1, "rate": 9})
    by = rules_by_file(findings([str(tmp_path)]))
    assert by["Eco/eco.json"] == ["data.out-of-range"]
    assert by["Elsewhere/also.json"] == ["data.out-of-range"]


def test_x19_unschemad_folder_garbage_is_ignored_even_if_format_has_schema_elsewhere(tmp_path):
    basic_tree(tmp_path)
    put(tmp_path, "Elsewhere/broken.json", "{nope")
    put(tmp_path, "Elsewhere/noformat.json", {"a": 1})
    assert findings([str(tmp_path)]) == []


# ---------------------------------------------------------------------------
# X8 nested schema'd folders
# ---------------------------------------------------------------------------

def test_x8_nested_schemad_folders(tmp_path):
    put(tmp_path, "Eco/eco.schema.json", ECO)
    put(tmp_path, "Eco/Sub/sub.schema.json", schema("sub.params", entry("n", type="int", default=0)))
    put(tmp_path, "Eco/Sub/deep/loose.json", {"x": 1})
    put(tmp_path, "Eco/Sub/sub.json", {"format": "sub.params", "version": 1, "n": 1.5})
    put(tmp_path, "Eco/Other/loose.json", {"x": 1})   # below Eco -> schema'd by inheritance
    put(tmp_path, "Top/loose.json", {"x": 1})         # sibling of Eco -> not schema'd
    by = rules_by_file(findings([str(tmp_path)]))
    assert by == {"Eco/Sub/deep/loose.json": ["data.no-format"], "Eco/Sub/sub.json": ["data.wrong-type"],
                  "Eco/Other/loose.json": ["data.no-format"]}


def test_x8_schema_at_scan_root_makes_every_file_schemad(tmp_path):
    put(tmp_path, "root.schema.json", ECO)
    put(tmp_path, "a/b/c/loose.json", {"x": 1})
    assert rules_by_file(findings([str(tmp_path)])) == {"a/b/c/loose.json": ["data.no-format"]}


# ---------------------------------------------------------------------------
# X9 catalogue names, X10 skipped dirs, X11 directories named *.json, X12 symlinks
# ---------------------------------------------------------------------------

@pytest.mark.parametrize("name", ["x.catalogue.json", "a.b.catalogue.json", "assets.catalogue.json"])
def test_x9_catalogue_files_are_ignored_in_schemad_folder(tmp_path, name):
    basic_tree(tmp_path)
    put(tmp_path, f"Eco/{name}", "{broken")
    assert findings([str(tmp_path)]) == []
    f = put(tmp_path, f"Eco/{name}", {"nothing": 1})
    assert findings([str(f)]) == []


def test_x9_bare_catalogue_json_is_not_a_catalogue_pin(tmp_path):
    basic_tree(tmp_path)
    put(tmp_path, "Eco/catalogue.json", {"records": []})
    assert rules_by_file(findings([str(tmp_path)])) == {"Eco/catalogue.json": ["data.no-format"]}


@pytest.mark.parametrize("d", [".hidden", ".git", "out", "node_modules", "bin", "obj", "__pycache__"])
def test_x10_skipped_dirs_at_any_depth(tmp_path, d):
    basic_tree(tmp_path)
    put(tmp_path, f"Eco/{d}/bad.json", "{broken")
    put(tmp_path, f"Eco/sub/{d}/deeper/bad.json", {"x": 1})
    put(tmp_path, f"Eco/{d}/bad.schema.json", "{broken")
    assert findings([str(tmp_path)]) == []


@pytest.mark.parametrize("d", ["Out", "OUT", "Bin", "Node_Modules"])
def test_x10_skip_dirs_are_case_sensitive_pin(tmp_path, d):
    basic_tree(tmp_path)
    put(tmp_path, f"Eco/{d}/bad.json", {"x": 1})
    assert rules_by_file(findings([str(tmp_path)])) == {f"Eco/{d}/bad.json": ["data.no-format"]}


def test_x10_scan_root_that_is_itself_hidden_or_named_out_is_scanned(tmp_path):
    for name in (".hid", "out"):
        root = tmp_path / name
        basic_tree(root, rate=7)
        assert rules_by_file(findings([str(root)])) == {"Eco/eco.json": ["data.out-of-range"]}


def test_x11_directory_named_json_is_ignored(tmp_path):
    basic_tree(tmp_path)
    (tmp_path / "Eco" / "x.json").mkdir()
    (tmp_path / "Eco" / "y.schema.json").mkdir()
    put(tmp_path, "Eco/x.json/inner.json", {"x": 1})  # inside a dir named x.json: still a data file there
    r = invoke([str(tmp_path)], json_out=True)
    assert r.exit_code == 1 and r.exception is None or isinstance(r.exception, SystemExit)
    assert rules_by_file(json.loads(r.stdout)) == {"Eco/x.json/inner.json": ["data.no-format"]}


def test_x11_directory_given_as_explicit_json_path_is_walked(tmp_path):
    d = tmp_path / "weird.json"
    basic_tree(d, rate=7)
    assert rules_by_file(findings([str(d)])) == {"Eco/eco.json": ["data.out-of-range"]}


def _symlink(link: Path, target: Path, is_dir: bool):
    try:
        os.symlink(target, link, target_is_directory=is_dir)
    except (OSError, NotImplementedError, AttributeError):
        pytest.skip("symlinks not available on this platform/user")


def test_x12_symlinked_file_is_read_as_a_normal_file(tmp_path):
    basic_tree(tmp_path)
    real = put(tmp_path, "Real/real.json", {"format": "eco.params", "version": 1, "rate": 9})
    _symlink(tmp_path / "Eco" / "link.json", real, False)
    by = rules_by_file(findings([str(tmp_path)]))
    assert by["Eco/link.json"] == ["data.out-of-range"]
    assert "Real/real.json" in by


def test_x12_symlink_loop_terminates(tmp_path):
    basic_tree(tmp_path)
    _symlink(tmp_path / "Eco" / "loop", tmp_path, True)
    r = invoke([str(tmp_path)], json_out=True)
    assert r.exit_code in (0, 1) and (r.exception is None or isinstance(r.exception, SystemExit))


def test_x12_broken_symlink_is_ignored(tmp_path):
    basic_tree(tmp_path)
    _symlink(tmp_path / "Eco" / "dangling.json", tmp_path / "missing.json", False)
    assert findings([str(tmp_path)]) == []


# ---------------------------------------------------------------------------
# X13/X14 ordering, duplicates
# ---------------------------------------------------------------------------

def _out(args):
    r = invoke(args)
    return r.exit_code, r.stdout


def test_x13_files_sort_case_insensitively_pin(tmp_path):
    for n in ("B", "a", "C", "d"):
        put(tmp_path, f"{n}.json", {"format": f"x.{n}", "version": 1})
    out = invoke([str(tmp_path)]).stdout
    order = [out.index(f"x.{n}") for n in ("a", "B", "C", "d")]
    assert order == sorted(order)


def test_x13_dot_sorts_before_slash_pin(tmp_path):
    put(tmp_path, "a/b.json", {"format": "x.nested", "version": 1})
    put(tmp_path, "a.json", {"format": "x.flat", "version": 1})
    out = invoke([str(tmp_path)]).stdout
    assert out.index("x.flat") < out.index("x.nested")


@pytest.mark.skipif(os.name == "nt", reason="needs a case-sensitive file system")
def test_x13_names_differing_only_by_case_have_stable_order(tmp_path):
    for n in ("a.json", "A.json"):
        put(tmp_path, n, {"format": f"x.{n}", "version": 1})
    outs = {invoke([str(tmp_path)]).stdout for _ in range(3)}
    assert len(outs) == 1


def test_x13_repeated_runs_identical(tmp_path):
    basic_tree(tmp_path, rate=7)
    for n in "dcba":
        put(tmp_path, f"{n}/{n}.json", {"format": f"x.{n}", "version": 1})
    assert len({_out([str(tmp_path)]) for _ in range(4)}) == 1


def test_x13_paths_reordered_identical(tmp_path):
    basic_tree(tmp_path / "one", rate=7)
    basic_tree(tmp_path / "two", rate=8, folder="Zoo")
    put(tmp_path / "three", "t.json", {"format": "x.t", "version": 1})
    paths = [str(tmp_path / n) for n in ("one", "two", "three")]
    outs = {_out(list(p)) for p in ([paths[0], paths[1], paths[2]], paths[::-1], [paths[1], paths[2], paths[0]])}
    assert len(outs) == 1


def test_x14_duplicate_paths_equal_single(tmp_path):
    basic_tree(tmp_path, rate=7)
    assert _out([str(tmp_path), str(tmp_path)]) == _out([str(tmp_path)])
    f = tmp_path / "Eco" / "eco.json"
    s = tmp_path / "Eco" / "eco.schema.json"
    assert _out([str(s), str(f), str(f)]) == _out([str(s), str(f)])


def test_x14_file_and_its_parent_folder_either_order_identical(tmp_path):
    basic_tree(tmp_path, rate=7)
    f = str(tmp_path / "Eco" / "eco.json")
    d = str(tmp_path)
    a, b = _out([f, d]), _out([d, f])
    assert a == b
    assert a[0] == 1 and "FAIL Eco/eco.json" in a[1]


def test_x14_parent_and_child_folder_either_order_identical(tmp_path):
    basic_tree(tmp_path, rate=7)
    a = _out([str(tmp_path), str(tmp_path / "Eco")])
    b = _out([str(tmp_path / "Eco"), str(tmp_path)])
    assert a == b


def test_x14_file_counted_once_when_given_twice(tmp_path):
    basic_tree(tmp_path)
    f, s = str(tmp_path / "Eco" / "eco.json"), str(tmp_path / "Eco" / "eco.schema.json")
    assert "2 files checked" not in invoke([s, s]).stdout
    assert "1 files checked" in invoke([s, s]).stdout


# ---------------------------------------------------------------------------
# X15 multi-root prefix, X16 manifest aliases, X17 target forms
# ---------------------------------------------------------------------------

def test_x15_single_app_no_prefix_multi_app_prefix(tmp_path, monkeypatch):
    root = repo(tmp_path, ("OnlyApp",))
    basic_tree(root / "Cluiche" / "Assets" / "OnlyApp", rate=7)
    patch_repo(monkeypatch, root)
    assert "FAIL Eco/eco.json" in invoke([]).stdout
    basic_tree(root / "Cluiche" / "Assets" / "Second", rate=7)
    out = invoke([]).stdout
    assert "FAIL OnlyApp/Eco/eco.json" in out and "FAIL Second/Eco/eco.json" in out


def test_x15_explicit_paths_never_prefixed(tmp_path, monkeypatch):
    root = repo(tmp_path, ("A", "B"))
    basic_tree(root / "Cluiche" / "Assets" / "A", rate=7)
    patch_repo(monkeypatch, root)
    out = invoke([str(root / "Cluiche" / "Assets" / "A")]).stdout
    assert "FAIL Eco/eco.json" in out


def _app(tmp_path, monkeypatch, manifest, name="cow.diagame", pipeline='[targets.cow]\nproject = "x.vcxproj"\napp_name = "CoW"\n'):
    root = repo(tmp_path, ("CoW",), pipeline)
    app = root / "Cluiche" / "Assets" / "CoW"
    if manifest is not None:
        put(app, name, manifest)
    patch_repo(monkeypatch, root)
    return root, app


@pytest.mark.parametrize("manifest", [
    "{not json", "[]", "null", "42", {"config": None}, {"config": []}, {"config": {"path_aliases": []}},
    {"config": {"path_aliases": None}}, {"config": {"path_aliases": {"a": 5, "b": None, "c": ["x"], "d": {}}}},
    b"\xff\xfe", "", {"config": {"path_aliases": {"gone": "../../Nope"}}},
], ids=["badjson", "array", "null", "number", "cfg-null", "cfg-list", "aliases-list", "aliases-null",
        "alias-nonstr", "nonutf8", "empty", "alias-missing-dir"])
def test_x16_bad_manifests_are_tolerated(tmp_path, monkeypatch, manifest):
    root, app = _app(tmp_path, monkeypatch, manifest)
    basic_tree(app, rate=7)
    r = invoke(["--target", "cow"])
    assert r.exit_code == 1 and "FAIL Eco/eco.json" in r.stdout, (r.stdout, r.exception)
    assert schema_discovery.target_roots("cow", root) == [app.resolve()]


def test_x16_alias_to_a_file_is_not_a_root(tmp_path, monkeypatch):
    root, app = _app(tmp_path, monkeypatch, {"config": {"path_aliases": {"f": "notes.txt"}}})
    (app / "notes.txt").write_text("x")
    assert schema_discovery.target_roots("cow", root) == [app.resolve()]


def test_x16_alias_outside_app_is_added_with_prefix(tmp_path, monkeypatch):
    root, app = _app(tmp_path, monkeypatch, {"config": {"path_aliases": {"e": "../../Elsewhere"}}})
    basic_tree(app)
    basic_tree(root / "Cluiche" / "Elsewhere", rate=7)
    out = invoke(["--target", "cow"]).stdout
    assert "FAIL Elsewhere/Eco/eco.json" in out
    assert [p.name for p in schema_discovery.target_roots("cow", root)] == ["CoW", "Elsewhere"]


def test_x16_alias_inside_app_adds_nothing(tmp_path, monkeypatch):
    root, app = _app(tmp_path, monkeypatch, {"config": {"path_aliases": {"in": "Global", "self": "."}}})
    (app / "Global").mkdir()
    assert schema_discovery.target_roots("cow", root) == [app.resolve()]


def test_x16_alias_that_contains_the_app_replaces_it(tmp_path, monkeypatch):
    root, app = _app(tmp_path, monkeypatch, {"config": {"path_aliases": {"up": ".."}}})
    assert schema_discovery.target_roots("cow", root) == [app.parent.resolve()]


def test_x16_manifests_in_skipped_dirs_and_other_suffixes(tmp_path, monkeypatch):
    root, app = _app(tmp_path, monkeypatch, None)
    (root / "Cluiche" / "Elsewhere").mkdir()
    for rel in ("out/a.diagame", ".hid/b.diaapp", "node_modules/c.diastage"):
        put(app, rel, {"config": {"path_aliases": {"e": "../../../Elsewhere"}}})
    put(app, "d.json", {"config": {"path_aliases": {"e": "../../Elsewhere"}}})
    assert schema_discovery.target_roots("cow", root) == [app.resolve()]


@pytest.mark.parametrize("suffix", [".diagame", ".diaapp", ".diastage"])
def test_x16_all_three_manifest_suffixes(tmp_path, monkeypatch, suffix):
    root, app = _app(tmp_path, monkeypatch, {"config": {"path_aliases": {"e": "../../Elsewhere"}}}, name="m" + suffix)
    (root / "Cluiche" / "Elsewhere").mkdir()
    assert len(schema_discovery.target_roots("cow", root)) == 2


@pytest.mark.parametrize("target", ["cow", "COW", "CoW", "Cow"])
def test_x17_pipeline_name_and_dir_name_any_case(tmp_path, monkeypatch, target):
    root, app = _app(tmp_path, monkeypatch, None)
    basic_tree(app, rate=7)
    r = invoke(["--target", target])
    assert r.exit_code == 1 and "FAIL Eco/eco.json" in r.stdout


def test_x17_dir_name_works_without_pipeline_toml(tmp_path, monkeypatch):
    root = tmp_path
    basic_tree(root / "Cluiche" / "Assets" / "Solo", rate=7)
    patch_repo(monkeypatch, root)
    r = invoke(["--target", "solo"])
    assert r.exit_code == 1 and r.exception is None or isinstance(r.exception, SystemExit)
    assert "FAIL Eco/eco.json" in r.stdout


def test_x17_pipeline_target_without_app_name_uses_target_name(tmp_path, monkeypatch):
    root = repo(tmp_path, ("tool",), '[targets.tool]\nproject = "x.vcxproj"\n')
    basic_tree(root / "Cluiche" / "Assets" / "tool", rate=7)
    patch_repo(monkeypatch, root)
    assert invoke(["--target", "TOOL"]).exit_code == 1


@pytest.mark.parametrize("target", ["", "nonesuch", " cow", "cow ", "co/w", ".."])
def test_x17_unknown_target_exits_2_and_lists_known_apps(tmp_path, monkeypatch, target):
    root, app = _app(tmp_path, monkeypatch, None)
    r = invoke(["--target", target])
    assert r.exit_code == 2, (r.stdout, r.stderr)
    assert "unknown --target" in (r.stderr + r.stdout) and "CoW" in (r.stderr + r.stdout)
    assert "files checked" not in r.stdout


def test_x17_no_assets_dir_default_run_is_empty_not_a_crash(tmp_path, monkeypatch):
    patch_repo(monkeypatch, tmp_path)
    r = invoke([])
    assert r.exit_code == 0 and "0 files checked" in r.stdout


def test_x17_target_scope_excludes_conformance_dir(tmp_path, monkeypatch):
    root, app = _app(tmp_path, monkeypatch, {"config": {"path_aliases": {"c": "../../../Dia/DiaValueSchema/Testing"}}})
    put(root / "Dia" / "DiaValueSchema" / "Testing", "Conformance/bad.schema.json", "{broken")
    r = invoke(["--target", "cow"])
    assert r.exit_code == 0, r.stdout


# ---------------------------------------------------------------------------
# X18 unparsable schema plus its data; explicit data file alone
# ---------------------------------------------------------------------------

def test_x18_unparsable_schema_makes_its_data_a_warn_not_a_cascade(tmp_path):
    put(tmp_path, "Eco/eco.schema.json", "{broken")
    put(tmp_path, "Eco/eco.json", {"format": "eco.params", "version": 1, "rate": 99})
    data = findings([str(tmp_path)])
    assert sorted((f["file"], f["rule"], f["severity"]) for f in data) == [
        ("Eco/eco.json", "data.no-schema-for-format", "WARN"),
        ("Eco/eco.schema.json", "schema.unparsable-json", "FAIL")]


def test_x18_schema_with_invalid_entry_drops_whole_schema(tmp_path):
    put(tmp_path, "Eco/eco.schema.json", schema("eco.params", entry("rate", type="nonsense")))
    put(tmp_path, "Eco/eco.json", {"format": "eco.params", "version": 1, "rate": 99})
    by = rules_by_file(findings([str(tmp_path)]))
    assert by["Eco/eco.json"] == ["data.no-schema-for-format"]


def test_explicit_data_file_alone_is_warn_only_even_with_sibling_schema(tmp_path):
    basic_tree(tmp_path, rate=7)
    r = invoke([str(tmp_path / "Eco" / "eco.json")], json_out=True)
    assert r.exit_code == 0
    assert [f["rule"] for f in json.loads(r.stdout)] == ["data.no-schema-for-format"]


def test_explicit_unformatted_or_unparsable_data_file_is_silently_ignored_pin(tmp_path):
    basic_tree(tmp_path)
    a = put(tmp_path, "Eco/loose.json", {"x": 1})
    b = put(tmp_path, "Eco/broken.json", "{nope")
    s = tmp_path / "Eco" / "eco.schema.json"
    assert findings([str(a), str(b), str(s)]) == []


def test_explicit_file_names_are_basenames(tmp_path):
    basic_tree(tmp_path, rate=7)
    out = invoke([str(tmp_path / "Eco" / "eco.schema.json"), str(tmp_path / "Eco" / "eco.json")]).stdout
    assert "FAIL eco.json" in out


def test_non_json_explicit_file_is_not_special_cased_pin(tmp_path):
    p = put(tmp_path, "notes.txt", "hello")
    r = invoke([str(p)])
    assert r.exit_code == 0 and "0 files checked" in r.stdout
    q = put(tmp_path, "data.txt", {"format": "mystery", "version": 1})
    assert "has no schema" in invoke([str(q)]).stdout


# ---------------------------------------------------------------------------
# Scale
# ---------------------------------------------------------------------------

def test_thousand_file_tree_completes_quickly(tmp_path):
    put(tmp_path, "Eco/eco.schema.json", ECO)
    for i in range(1000):
        put(tmp_path, f"Eco/d{i % 20}/f{i}.json", {"format": "eco.params", "version": 1, "rate": 0.5})
    t = time.time()
    r = invoke([str(tmp_path)])
    assert r.exit_code == 0, r.stdout[-300:]
    assert "1001 files checked" in r.stdout
    assert time.time() - t < 30
