"""dia validate schema: code-side export robustness and output forms, C1-C11 (B1, B2, B13, B14)."""
from __future__ import annotations

import json
import subprocess
from pathlib import Path

import pytest

from dia_cli.commands.validate import schema_codeside, schema_discovery
from dia_cli.commands.validate.schema_codeside import check_code_side, find_export, git_head, run_code_side
from tests.vs_helpers import invoke, put, repo

FMT = "dia.value_schema_export"
HEAD = "8f88d1aa"


def export(undescribed=None, entries=None, header=None, **extra):
    doc = {"format": FMT, "version": 1,
           "header": {"app": "cow", "git_commit": HEAD, "dirty": False} if header is None else header,
           "entries": entries if entries is not None else []}
    if undescribed is not None:
        doc["undescribed"] = undescribed
    doc.update(extra)
    return doc


def rules(fs):
    return [f.rule for f in fs]


def run_cli(tmp_path, ex, *more, raw=None):
    p = tmp_path / "ex.json"
    if raw is not None:
        p.write_bytes(raw)
    else:
        p.write_text(ex if isinstance(ex, str) else json.dumps(ex), encoding="utf-8")
    assets = tmp_path / "assets"
    assets.mkdir(exist_ok=True)
    return invoke([str(assets), "--code-side", str(p), *more])


@pytest.fixture(autouse=True)
def fixed_head(monkeypatch):
    monkeypatch.setattr(schema_codeside, "git_head", lambda: HEAD)


# ---------------------------------------------------------------------------
# C1 malformed export forms -> clean exit 2, never a traceback, no partial output (B1, B2)
# ---------------------------------------------------------------------------

MALFORMED = [
    ("empty", b""), ("spaces", b"   \n"), ("bom-only", b"\xef\xbb\xbf"), ("truncated", b"{"),
    ("array", b"[]"), ("null", b"null"), ("number", b"42"), ("string", b'"s"'), ("true", b"true"),
    ("empty-object", b"{}"), ("wrong-format", b'{"format": "other"}'), ("format-null", b'{"format": null}'),
    ("format-list", b'{"format": ["dia.value_schema_export"]}'), ("format-case", b'{"format": "DIA.VALUE_SCHEMA_EXPORT"}'),
    ("non-utf8", b"\xff\xfe\x00{"), ("latin1", b'{"format": "\xe9"}'),
    ("deep-array", b"[" * 100000), ("deep-object", b'{"a":' * 5000 + b"1" + b"}" * 5000),
    ("trailing-garbage", b'{"format": "dia.value_schema_export"} x'),
    ("nan-like", b'{"format": "dia.value_schema_export", "x": NaN, "y": '),
]


@pytest.mark.parametrize("raw", [m[1] for m in MALFORMED], ids=[m[0] for m in MALFORMED])
def test_c1_malformed_export_is_exit_2_with_message_and_no_summary(tmp_path, raw):
    r = run_cli(tmp_path, None, raw=raw)
    assert r.exit_code == 2, (r.stdout, r.stderr, r.exception)
    assert isinstance(r.exception, SystemExit)
    assert "cannot read code-side export" in r.stderr
    assert "files checked" not in r.stdout and "Traceback" not in r.stdout + r.stderr


@pytest.mark.parametrize("raw", [b"\xef\xbb\xbf" + json.dumps(export()).encode(),
                                 json.dumps(export(), indent=2).replace("\n", "\r\n").encode(),
                                 json.dumps(export(), ensure_ascii=False).encode("utf-8"),
                                 json.dumps(export(zzz=[1, {"a": None}])).encode()],
                         ids=["bom", "crlf", "utf8", "unknown-extra-keys"])
def test_c1_valid_export_variants_read_cleanly(tmp_path, raw):
    r = run_cli(tmp_path, None, raw=raw)
    assert r.exit_code == 0, (r.stdout, r.stderr)
    assert "0 FAIL, 0 WARN" in r.stdout


def test_c1_export_path_is_a_directory_exits_2(tmp_path):
    d = tmp_path / "adir"
    d.mkdir()
    r = invoke([str(tmp_path), "--code-side", str(d)])
    assert r.exit_code == 2 and "cannot read code-side export" in r.stderr


def test_c1_missing_export_path_exits_2(tmp_path):
    r = invoke([str(tmp_path), "--code-side", str(tmp_path / "nope.json")])
    assert r.exit_code == 2 and "--code-side path does not exist" in r.stderr


def test_c1_malformed_export_with_json_flag_prints_no_json(tmp_path):
    r = run_cli(tmp_path, None, "--json", raw=b"[]")
    assert r.exit_code == 2 and r.stdout == ""


def test_c1_validation_failures_do_not_mask_a_malformed_export(tmp_path):
    put(tmp_path, "Eco/bad.schema.json", "{broken")
    (tmp_path / "ex.json").write_text("[]")
    r = invoke([str(tmp_path), "--code-side", str(tmp_path / "ex.json")])
    assert r.exit_code == 2 and r.stdout == ""


# ---------------------------------------------------------------------------
# C2 header variants / stale
# ---------------------------------------------------------------------------

@pytest.mark.parametrize("header", [None, "x", 5, [], [1], {}, {"git_commit": None}, {"git_commit": ""},
                                    {"git_commit": 123}, {"git_commit": ["a"]}, {"dirty": "true"},
                                    {"dirty": 1}, {"dirty": None}, {"dirty": False}, {"dirty": "yes"}])
def test_c2_header_variants_that_are_not_stale(header):
    ex = export(header=header)
    if header is None:
        del ex["header"]
    assert check_code_side(ex, head_commit=HEAD) == []


@pytest.mark.parametrize("commit,head,stale", [
    ("8f88d1aa", "8f88d1aa99887766", False), ("8F88D1AA", "8f88d1aa", False), ("8f88d1aa", "8F88D1AA", False),
    ("8f88d1aa99887766", "8f88d1aa", False), ("8f88d1ab", "8f88d1aa", True), ("abc", "def", True),
    ("8f88d1aa", None, False), ("8f88d1aa", "", False), ("", "abc", False),
    ("8", "8f88d1aa", False),   # a one-character commit is a prefix of half of all heads (pin)
])
def test_c2_commit_comparison(commit, head, stale):
    fs = check_code_side(export(header={"git_commit": commit}), head_commit=head)
    assert (rules(fs) == ["codeside.stale-export"]) == stale


def test_c2_commit_and_dirty_combine_into_one_warn():
    fs = check_code_side(export(header={"git_commit": "aaaa", "dirty": True}), head_commit="bbbbbbbbcc")
    assert len(fs) == 1 and fs[0].severity == "WARN" and fs[0].rule == "codeside.stale-export"
    assert "aaaa" in fs[0].message and "bbbbbbbb" in fs[0].message and "dirty" in fs[0].message and "; " in fs[0].message
    assert fs[0].file == "valueschema.json" and fs[0].owner is None and fs[0].key is None


def test_c2_head_in_message_is_truncated_to_8():
    fs = check_code_side(export(header={"git_commit": "aaaa"}), head_commit="b" * 40)
    assert "`bbbbbbbb`" in fs[0].message and "b" * 9 not in fs[0].message


# ---------------------------------------------------------------------------
# C3 git unavailable / cwd (B13)
# ---------------------------------------------------------------------------

def _git_head_with(monkeypatch, behaviour):
    monkeypatch.undo()  # drop the autouse git_head patch; subprocess is patched below
    monkeypatch.setattr(subprocess, "run", behaviour)
    return git_head()


def _completed(code, out):
    return lambda *a, **k: subprocess.CompletedProcess(a, code, stdout=out, stderr="")


@pytest.mark.parametrize("behaviour,expected", [
    (_completed(0, "abc123\n"), "abc123"), (_completed(0, ""), None), (_completed(0, "  \n"), None),
    (_completed(128, "abc\n"), None), (_completed(1, ""), None),
    (lambda *a, **k: (_ for _ in ()).throw(FileNotFoundError("git")), None),
    (lambda *a, **k: (_ for _ in ()).throw(PermissionError("git")), None),
    (lambda *a, **k: (_ for _ in ()).throw(subprocess.TimeoutExpired("git", 10)), None),
    (lambda *a, **k: (_ for _ in ()).throw(subprocess.SubprocessError()), None),
], ids=["ok", "empty", "blank", "rc128", "rc1", "no-git", "perm", "timeout", "suberror"])
def test_c3_git_head_never_raises(monkeypatch, behaviour, expected):
    assert _git_head_with(monkeypatch, behaviour) == expected


def test_c3_unavailable_git_means_no_stale_warning_and_clean_run(tmp_path, monkeypatch):
    monkeypatch.undo()
    monkeypatch.setattr(subprocess, "run", lambda *a, **k: (_ for _ in ()).throw(FileNotFoundError("git")))
    r = run_cli(tmp_path, export(header={"git_commit": "aaaa", "dirty": False}))
    assert r.exit_code == 0 and "export is from" not in r.stdout


def test_b13_git_head_runs_in_the_repo_root(monkeypatch, tmp_path):
    seen = {}

    def fake(*a, **k):
        seen.update(k)
        return subprocess.CompletedProcess(a, 0, stdout="abc\n", stderr="")

    monkeypatch.undo()
    monkeypatch.setattr(schema_discovery, "get_repo_root", lambda: tmp_path)
    monkeypatch.setattr(subprocess, "run", fake)
    assert git_head() == "abc"
    assert Path(seen["cwd"]) == tmp_path


def test_b13_stale_check_works_from_another_cwd(tmp_path, monkeypatch):
    root = schema_discovery.get_repo_root()
    real = subprocess.run(["git", "rev-parse", "HEAD"], cwd=root, capture_output=True, text=True)
    if real.returncode != 0:
        pytest.skip("not a git checkout")
    monkeypatch.undo()
    elsewhere = tmp_path / "elsewhere"
    elsewhere.mkdir()
    monkeypatch.chdir(elsewhere)
    assert git_head() == real.stdout.strip()
    ex = tmp_path / "ex.json"
    ex.write_text(json.dumps(export(header={"git_commit": real.stdout.strip(), "dirty": False})))
    r = invoke([str(elsewhere), "--code-side", str(ex)])
    assert r.exit_code == 0 and "export is from" not in r.stdout


# ---------------------------------------------------------------------------
# C4 noArgs forms (B14)
# ---------------------------------------------------------------------------

@pytest.mark.parametrize("value,reported", [
    (True, False), (False, True), ("false", True), ("true", True), ("", True), (1, True), (0, True),
    (None, True), ([], True), ([1], True), ({}, True), ({"a": 1}, True)])
def test_c4_no_args_only_honours_real_booleans(value, reported):
    ex = export({"commands": [{"name": "a.b", "noArgs": value}]})
    assert (len(check_code_side(ex, head_commit=HEAD)) == 1) == reported


def test_c4_missing_no_args_is_reported_and_string_items_have_none():
    fs = check_code_side(export({"commands": [{"name": "a.b"}, "c.d"]}), head_commit=HEAD)
    assert [f.key for f in fs] == ["a.b", "c.d"]


# ---------------------------------------------------------------------------
# C5 owner fallbacks and wrong-typed items (B14)
# ---------------------------------------------------------------------------

@pytest.mark.parametrize("item,owner,key", [
    ("cow.save", "cow", "cow.save"),
    ("plain", "plain", "plain"),
    (".lead", "(unknown)", ".lead"),
    ("", "(unknown)", ""),
    ("a.b.c", "a", "a.b.c"),
    ("a.", "a", "a."),
    ({"name": "x.y", "owner": "Own"}, "Own", "x.y"),
    ({"name": "x.y", "owner": ""}, "x", "x.y"),
    ({"name": "x.y", "owner": None}, "x", "x.y"),
    ({"name": "x.y", "owner": 5}, "x", "x.y"),
    ({"name": "x.y", "owner": ["Own"]}, "x", "x.y"),
    ({"name": "plain"}, "plain", "plain"),
    ({"name": ""}, "(unknown)", ""),
    ({}, "(unknown)", ""),
    ({"name": None}, "(unknown)", ""),
    ({"name": 5, "owner": "O"}, "O", ""),
    ({"owner": "O"}, "O", ""),
    ({"name": ".lead"}, "(unknown)", ".lead"),
])
@pytest.mark.parametrize("section,rule", [("metrics", "codeside.undescribed-metric"),
                                          ("commands", "codeside.undescribed-command")])
def test_c5_owner_fallback_dot_sections(section, rule, item, owner, key):
    fs = check_code_side(export({section: [item]}), head_commit=HEAD)
    assert [(f.rule, f.owner, f.key) for f in fs] == [(rule, owner, key)]


@pytest.mark.parametrize("item,owner,key", [
    ("Transform2D/scale", "Transform2D", "Transform2D/scale"), ("noslash", "noslash", "noslash"),
    ("/lead", "(unknown)", "/lead"), ("a.b/c", "a.b", "a.b/c"), ({"name": "T/s", "owner": "X"}, "X", "T/s"),
    ({"name": "T/s", "owner": ""}, "T", "T/s")])
def test_c5_owner_fallback_slash_section(item, owner, key):
    fs = check_code_side(export({"component_fields": [item]}), head_commit=HEAD)
    assert [(f.rule, f.owner, f.key) for f in fs] == [("codeside.undescribed-field", owner, key)]


BAD_ITEMS = [None, 5, 1.5, True, [], ["a.b"], [["a.b"]]]


@pytest.mark.parametrize("section", ["metrics", "commands", "component_fields"])
@pytest.mark.parametrize("bad", BAD_ITEMS, ids=lambda v: repr(v))
def test_c5_non_string_non_object_items_are_skipped(section, bad):
    fs = check_code_side(export({section: [bad, "ok.name"]}), head_commit=HEAD)
    assert [f.key for f in fs] == ["ok.name"]


@pytest.mark.parametrize("section", ["metrics", "commands", "component_fields"])
@pytest.mark.parametrize("container", ["abc", "a.b", {"a.b": 1}, {"name": "a.b"}, 5, 1.5, True, None, ""],
                         ids=lambda v: repr(v))
def test_c5_wrong_type_section_neither_crashes_nor_iterates_characters(section, container):
    assert check_code_side(export({section: container}), head_commit=HEAD) == []


@pytest.mark.parametrize("und", ["abc", ["metrics"], 5, None, True, [{"metrics": ["a"]}]], ids=lambda v: repr(v))
def test_c5_wrong_type_undescribed_is_ignored(und):
    ex = export()
    ex["undescribed"] = und
    assert check_code_side(ex, head_commit=HEAD) == []


@pytest.mark.parametrize("entries", ["abc", {"key": "a_ms", "kind": "metric"}, 5, None, True, [None, 5, "a_ms", [], [[]]]],
                         ids=lambda v: repr(v))
def test_c5_wrong_type_entries_is_ignored(entries):
    ex = export()
    ex["entries"] = entries
    assert check_code_side(ex, head_commit=HEAD) == []


def test_c5_missing_entries_and_sections():
    ex = export()
    del ex["entries"]
    assert check_code_side(ex, head_commit=HEAD) == []
    assert check_code_side({"format": FMT}, head_commit=None) == []


def test_c5_duplicate_items_are_each_reported_pin():
    fs = check_code_side(export({"metrics": ["a.b", "a.b"]}), head_commit=HEAD)
    assert len(fs) == 2


# ---------------------------------------------------------------------------
# C6 unit lint
# ---------------------------------------------------------------------------

def metric(key, **kw):
    e = {"key": key, "kind": "metric", "label": key, "description": "d"}
    e.update(kw)
    return e


@pytest.mark.parametrize("e,warns", [
    (metric("a_ms", unit=""), True), (metric("a_ms"), True), (metric("a_ms", unit=None), True),
    (metric("a_ms", unit=0), True), (metric("a_ms", unit=False), True),
    (metric("a_ms", unit="ms"), False), (metric("a_ms", unit="widgets"), False),
    (metric("a_ms", unit="  "), False),            # pin: whitespace counts as a unit
    (metric("a_ms", unit=5), False),               # pin: any truthy value counts as a unit
    (metric("ms", unit=""), False),                # no underscore -> not a unit-suffixed key
    (metric("_ms", unit=""), True),                # key exactly the suffix
    (metric("a_MS", unit=""), False),              # pin: suffixes are case sensitive
    (metric("a_Ms", unit=""), False), (metric("a_ms ", unit=""), False), (metric("a_msx", unit=""), False),
    (metric("a_ms/x", unit=""), False), (metric("a_s", unit=""), False),
    ({"key": "a_ms", "kind": "parameter", "unit": ""}, False),
    ({"key": "a_ms", "kind": "field"}, False), ({"key": "a_ms", "kind": "event"}, False),
    ({"key": "a_ms", "kind": "METRIC"}, False), ({"key": "a_ms"}, False),
    ({"key": 5, "kind": "metric"}, False), ({"key": None, "kind": "metric"}, False),
    ({"key": ["a_ms"], "kind": "metric"}, False), ({"kind": "metric"}, False),
])
def test_c6_unit_lint_table(e, warns):
    fs = check_code_side(export(entries=[e]), head_commit=HEAD)
    assert (rules(fs) == ["codeside.unit-missing"]) == warns
    assert len(fs) == (1 if warns else 0)


@pytest.mark.parametrize("suffix", ["_ms", "_us", "_bytes", "_kb", "_mb", "_hz", "_pct"])
def test_c6_every_suffix_warns_without_unit_and_not_with(suffix):
    assert len(check_code_side(export(entries=[metric("m.x" + suffix)]), head_commit=HEAD)) == 1
    assert check_code_side(export(entries=[metric("m.x" + suffix, unit="u")]), head_commit=HEAD) == []


def test_c6_unit_finding_has_no_owner_so_it_prints_ungrouped():
    fs = check_code_side(export(entries=[metric("m.x_ms")]), head_commit=HEAD)
    assert fs[0].owner is None and fs[0].key == "m.x_ms" and fs[0].severity == "WARN"


def test_c6_unit_warns_print_one_per_line(tmp_path):
    r = run_cli(tmp_path, export(entries=[metric("a_ms"), metric("b_ms")]))
    assert r.stdout.count("WARN metric `") == 2 and "no unit" in r.stdout


# ---------------------------------------------------------------------------
# C7 grouped vs verbose vs json
# ---------------------------------------------------------------------------

MULTI = {"metrics": ["cow.a", "cow.b", "sim.c"], "commands": [{"name": "x.one", "owner": "X"}],
         "component_fields": ["T/a", "T/b"]}


def test_c7_grouped_text_singular_plural_and_order(tmp_path):
    r = run_cli(tmp_path, export(MULTI, header={"git_commit": "zz", "dirty": True}))
    lines = [ln for ln in r.stdout.splitlines() if ln.startswith("WARN")]
    # ungrouped WARNs (stale) first, then groups in first-seen order of the findings
    assert lines[0].startswith("WARN export is from") or "dirty" in lines[0]
    assert lines[1:] == ["WARN cow: 2 metrics without descriptions", "WARN sim: 1 metrics without descriptions",
                         "WARN X: 1 commands without argument descriptions",
                         "WARN T: 2 component fields with a range but no description"]
    assert "5 WARN" not in r.stdout and "7 files" not in r.stdout
    assert "0 files checked, 0 FAIL, 7 WARN" in r.stdout   # counts every item, not every group


def test_c7_verbose_lists_each_item_indented_under_its_group(tmp_path):
    r = run_cli(tmp_path, export(MULTI), "--verbose")
    out = r.stdout.splitlines()
    i = out.index("WARN cow: 2 metrics without descriptions")
    assert out[i + 1:i + 3] == ["       cow.a", "       cow.b"]
    j = out.index("WARN sim: 1 metrics without descriptions")
    assert out[j + 1] == "       sim.c"


def test_c7_one_owner_one_item_is_still_grouped_pin_no_singular_form(tmp_path):
    r = run_cli(tmp_path, export({"metrics": ["solo.one"]}))
    assert "WARN solo: 1 metrics without descriptions" in r.stdout


def test_c7_json_has_every_item_owner_only_on_coverage_findings(tmp_path):
    r = run_cli(tmp_path, export(MULTI, header={"git_commit": "zz", "dirty": True},
                                 entries=[metric("m_ms")]), "--json")
    data = json.loads(r.stdout)
    assert len(data) == 1 + 3 + 1 + 2 + 1
    by_rule = {}
    for f in data:
        by_rule.setdefault(f["rule"], []).append(f)
    assert all("owner" in f for k in ("codeside.undescribed-metric", "codeside.undescribed-command",
                                      "codeside.undescribed-field") for f in by_rule[k])
    assert all("owner" not in f for k in ("codeside.stale-export", "codeside.unit-missing") for f in by_rule[k])
    assert {f["owner"] for f in by_rule["codeside.undescribed-metric"]} == {"cow", "sim"}
    assert all(f["file"] == "valueschema.json" for f in data)


def test_c7_verbose_plus_json_is_pure_json(tmp_path):
    r = run_cli(tmp_path, export(MULTI), "--verbose", "--json")
    assert isinstance(json.loads(r.stdout), list) and "OK" not in r.stdout


def test_c7_unicode_owner_and_key(tmp_path):
    r = run_cli(tmp_path, export({"metrics": [{"name": "café.täick", "owner": "École"}]}), "--verbose")
    assert "WARN École: 1 metrics" in r.stdout or r.exit_code == 0
    assert r.exit_code == 0


# ---------------------------------------------------------------------------
# C8-C11 find_export and wiring
# ---------------------------------------------------------------------------

def _fake_repo(tmp_path, apps, exports=()):
    root = repo(tmp_path, apps, '[targets.cow]\nproject = "x.vcxproj"\napp_name = "CoW"\n')
    for e in exports:
        put(root / "Cluiche" / "out" / e, "valueschema.json", export())
    return root


def test_c8_find_export_with_several_exports(tmp_path):
    root = _fake_repo(tmp_path, ("CoW", "Other"), ("CoW", "Other"))
    assert find_export(None, repo_root=root) is None             # ambiguous without a target
    assert find_export("cow", repo_root=root).parent.name == "CoW"
    assert find_export("OTHER", repo_root=root).parent.name == "Other"
    assert find_export("nonesuch", repo_root=root) is None


def test_c8_find_export_target_resolves_via_pipeline_and_case(tmp_path):
    root = _fake_repo(tmp_path, ("CoW",), ("CoW",))
    for t in ("cow", "COW", "CoW"):
        assert find_export(t, repo_root=root) == root / "Cluiche" / "out" / "CoW" / "valueschema.json"


def test_c8_find_export_no_out_dir(tmp_path):
    root = _fake_repo(tmp_path, ("CoW",))
    assert find_export(None, repo_root=root) is None and find_export("cow", repo_root=root) is None


def test_c8_find_export_directory_named_like_export(tmp_path):
    root = _fake_repo(tmp_path, ("CoW",))
    (root / "Cluiche" / "out" / "CoW" / "valueschema.json").mkdir(parents=True)
    assert find_export("cow", repo_root=root) is None            # not a file
    # without a target the glob returns the directory; loading then fails cleanly (exit 2), pin
    found = find_export(None, repo_root=root)
    assert found is not None and found.is_dir()


def test_c8_single_export_for_other_app_is_used_without_target_pin(tmp_path):
    root = _fake_repo(tmp_path, ("CoW", "Other"), ("Other",))
    assert find_export(None, repo_root=root).parent.name == "Other"
    assert find_export("cow", repo_root=root) is None


def test_c9_explicit_code_side_wins_over_found_export(tmp_path, monkeypatch):
    called = []
    monkeypatch.setattr(schema_codeside, "find_export", lambda *a, **k: called.append(1))
    p = tmp_path / "ex.json"
    p.write_text(json.dumps(export({"metrics": ["explicit.one"]})))
    fs, notice = run_code_side("whatever", p)
    assert called == [] and notice is None and [f.key for f in fs] == ["explicit.one"]


def test_c9_found_export_used_when_no_flag(tmp_path, monkeypatch):
    p = tmp_path / "found.json"
    p.write_text(json.dumps(export({"metrics": ["found.one"]})))
    monkeypatch.setattr(schema_codeside, "find_export", lambda t, repo_root=None: p)
    fs, notice = run_code_side(None, None)
    assert [f.key for f in fs] == ["found.one"] and notice is None


def test_c9_notice_when_nothing_found(monkeypatch):
    monkeypatch.setattr(schema_codeside, "find_export", lambda t, repo_root=None: None)
    assert run_code_side(None, None) == ([], schema_codeside.NOTICE)


def test_c10_code_side_with_paths_runs_code_side_but_paths_alone_do_not(tmp_path, monkeypatch):
    monkeypatch.setattr(schema_codeside, "find_export", lambda *a, **k: pytest.fail("must not search"))
    (tmp_path / "assets").mkdir()
    p = tmp_path / "ex.json"
    p.write_text(json.dumps(export({"metrics": ["a.b"]})))
    with_flag = invoke([str(tmp_path / "assets"), "--code-side", str(p)])
    without = invoke([str(tmp_path / "assets")])
    assert "WARN a: 1 metrics" in with_flag.stdout
    assert "code-side" not in without.stdout + without.stderr and "WARN a:" not in without.stdout


def test_c10_relative_code_side_path(tmp_path, monkeypatch):
    (tmp_path / "ex.json").write_text(json.dumps(export({"metrics": ["a.b"]})))
    (tmp_path / "assets").mkdir()
    monkeypatch.chdir(tmp_path)
    r = invoke(["assets", "--code-side", "ex.json"])
    assert r.exit_code == 0 and "WARN a: 1 metrics" in r.stdout


def test_c11_target_with_code_side_validates_target_but_uses_explicit_export(tmp_path, monkeypatch):
    root = _fake_repo(tmp_path, ("CoW",))
    monkeypatch.setattr(schema_discovery, "get_repo_root", lambda: root)
    p = tmp_path / "ex.json"
    p.write_text(json.dumps(export({"metrics": ["a.b"]})))
    ok = invoke(["--target", "cow", "--code-side", str(p)])
    assert ok.exit_code == 0 and "WARN a: 1 metrics" in ok.stdout
    bad = invoke(["--target", "nope", "--code-side", str(p)])
    assert bad.exit_code == 2


def test_c11_target_finds_its_own_export(tmp_path, monkeypatch):
    root = _fake_repo(tmp_path, ("CoW",))
    put(root / "Cluiche" / "out" / "CoW", "valueschema.json", export({"metrics": ["found.x"]}))
    monkeypatch.setattr(schema_discovery, "get_repo_root", lambda: root)
    r = invoke(["--target", "cow"])
    assert r.exit_code == 0 and "WARN found: 1 metrics" in r.stdout
