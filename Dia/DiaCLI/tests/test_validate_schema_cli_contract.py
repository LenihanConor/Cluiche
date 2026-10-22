"""dia validate schema: the CLI contract with real (unmocked) runs, L1-L8."""
from __future__ import annotations

import json
import os
import subprocess
import sys
from pathlib import Path

import pytest

from dia_cli.cli.cli_validate import cli as validate_cli
from dia_cli.commands.validate import schema_codeside, schema_discovery
from tests.vs_helpers import basic_tree, entry, findings, invoke, put, repo, schema
from click.testing import CliRunner

CLI_DIR = Path(__file__).resolve().parent.parent
BASE_KEYS = {"severity", "rule", "file", "key", "message", "suggestion", "path", "record"}
FMT = "dia.value_schema_export"


def messy_tree(root: Path) -> None:
    """One tree that triggers a wide spread of rule ids."""
    put(root, "Eco/eco.schema.json", schema(
        "eco.params",
        entry("rate", min=0, max=1, default=0.5),
        entry("mode", type="enum", values=[{"id": "a"}, {"id": "b"}], default="a"),
        entry("count", type="int", default=1),
        entry("flag", type="bool", default=True),
        entry("req", required=True, default=1),
        entry("species[].id", type="string", unique=True),
        entry("species[].diet", type="ref", ref="eco.species2/nothing[].id"),
        entry("link", type="ref", ref="eco.params/species[].id", default="wolf"),
        entry("affects_nope", affects=["nope"], default=0),
    ))
    put(root, "Eco/eco.json", {
        "format": "eco.params", "version": 1, "rate": 7, "mode": "zz", "count": "x", "flag": 3, "extra": 1,
        "rat": 1, "species": [{"id": "a"}, {"id": "a"}], "link": "wolf"})
    put(root, "Eco/broken.schema.json", "{nope")
    put(root, "Eco/unreadable.schema.json", b"\xff\xfe")
    put(root, "Eco/loose.json", [1])
    put(root, "Eco/mystery.json", {"format": "mystery", "version": 1})
    put(root, "A/a.schema.json", schema("dup.fmt", entry("k")))
    put(root, "B/b.schema.json", schema("dup.fmt", entry("k")))


# ---------------------------------------------------------------------------
# L1 --json fields
# ---------------------------------------------------------------------------

def test_l1_every_finding_has_exactly_the_documented_fields(tmp_path):
    messy_tree(tmp_path)
    data = findings([str(tmp_path)])
    assert len(data) >= 12
    rules = {f["rule"] for f in data}
    assert {"schema.unparsable-json", "schema.unreadable", "schema.duplicate-key-across-files",
            "data.no-format", "data.no-schema-for-format", "data.out-of-range", "data.bad-enum",
            "data.wrong-type", "data.no-schema-entry"} <= rules, sorted(rules)
    for f in data:
        assert set(f) == BASE_KEYS, f      # owner only on code-side coverage findings
        assert f["severity"] in ("FAIL", "WARN")
        assert isinstance(f["rule"], str) and "." in f["rule"]
        assert isinstance(f["file"], str) and f["file"]
        assert isinstance(f["message"], str) and f["message"]
        for k in ("key", "suggestion", "path", "record"):
            assert f[k] is None or isinstance(f[k], str)


def test_l1_data_findings_carry_path_record_and_suggestion(tmp_path):
    put(tmp_path, "Eco/eco.schema.json", schema("eco.params", entry("species[].growth", min=0, max=1, default=0.5),
                                               entry("species[].id", type="string")))
    put(tmp_path, "Eco/eco.json", {"format": "eco.params", "version": 1,
                                  "species": [{"id": "wolf", "growth": 0.5}, {"id": "bear", "growht": 2, "growth": 5}]})
    data = findings([str(tmp_path)])
    rng = next(f for f in data if f["rule"] == "data.out-of-range")
    assert rng["path"] == "species[1].growth" and rng["record"] == "bear" and rng["key"] == "species[].growth"
    typo = next(f for f in data if f["rule"] == "data.no-schema-entry")
    assert typo["suggestion"] and "growth" in typo["suggestion"]


def test_l1_owner_present_only_on_codeside_coverage(tmp_path, monkeypatch):
    monkeypatch.setattr(schema_codeside, "git_head", lambda: "aaaa")
    ex = tmp_path / "ex.json"
    ex.write_text(json.dumps({"format": FMT, "version": 1, "header": {"git_commit": "bbbb", "dirty": True},
                              "undescribed": {"metrics": ["a.b"], "commands": ["c.d"], "component_fields": ["T/f"]},
                              "entries": [{"key": "x_ms", "kind": "metric"}]}))
    (tmp_path / "assets").mkdir()
    basic_tree(tmp_path / "assets", rate=7)
    data = json.loads(invoke([str(tmp_path / "assets"), "--code-side", str(ex), "--json"]).stdout)
    owners = {f["rule"]: f.get("owner") for f in data}
    assert owners["codeside.undescribed-metric"] == "a" and owners["codeside.undescribed-command"] == "c"
    assert owners["codeside.undescribed-field"] == "T"
    for f in data:
        if f["rule"] in ("codeside.stale-export", "codeside.unit-missing") or not f["rule"].startswith("codeside"):
            assert "owner" not in f, f
        else:
            assert "owner" in f


# ---------------------------------------------------------------------------
# L2 JSON stream purity
# ---------------------------------------------------------------------------

def test_l2_json_is_empty_list_when_nothing_to_report(tmp_path):
    (tmp_path / "empty").mkdir()
    r = invoke([str(tmp_path / "empty"), "--json"])
    assert r.exit_code == 0 and json.loads(r.stdout) == [] and r.stderr == ""
    assert r.stdout.strip() == "[]"


def test_l2_notice_goes_to_stderr_in_json_mode_and_stdout_in_text_mode(tmp_path, monkeypatch):
    root = repo(tmp_path, ("App",))
    basic_tree(root / "Cluiche" / "Assets" / "App", rate=7)
    monkeypatch.setattr(schema_discovery, "get_repo_root", lambda: root)
    j = invoke([], json_out=True)
    assert j.exit_code == 1
    assert schema_codeside.NOTICE in j.stderr and schema_codeside.NOTICE not in j.stdout
    assert isinstance(json.loads(j.stdout), list)
    t = invoke([])
    assert schema_codeside.NOTICE in t.stdout and t.stderr == ""


@pytest.mark.parametrize("extra", [[], ["--verbose"]])
def test_l2_json_mode_prints_nothing_but_json(tmp_path, extra):
    messy_tree(tmp_path)
    r = invoke([str(tmp_path), "--json", *extra])
    assert r.exit_code == 1 and r.stderr == ""
    assert r.stdout.lstrip().startswith("[") and r.stdout.rstrip().endswith("]")
    json.loads(r.stdout)
    assert "files checked" not in r.stdout and "OK " not in r.stdout


def test_l2_json_is_stable_between_runs(tmp_path):
    messy_tree(tmp_path)
    assert invoke([str(tmp_path), "--json"]).stdout == invoke([str(tmp_path), "--json"]).stdout


# ---------------------------------------------------------------------------
# L3 unicode
# ---------------------------------------------------------------------------

def _unicode_tree(root: Path):
    put(root, "Éco/日本.schema.json", schema("x", entry("rate", min=0, max=1, default=0.5)))
    put(root, "Éco/データ.json", {"format": "x", "version": 1, "rate": 9, "レート": 1})


def test_l3_unicode_names_and_keys_in_text_and_json(tmp_path):
    _unicode_tree(tmp_path)
    t = invoke([str(tmp_path)])
    assert t.exit_code == 1 and "FAIL Éco/データ.json" in t.stdout and "key 'レート'" in t.stdout
    data = json.loads(invoke([str(tmp_path), "--json"]).stdout)
    assert {f["file"] for f in data} == {"Éco/データ.json"}
    assert any(f["key"] == "レート" for f in data)


def test_l3_unicode_survives_a_real_process_with_a_legacy_console_encoding(tmp_path):
    _unicode_tree(tmp_path)
    env = dict(os.environ, PYTHONIOENCODING="cp1252")
    env.pop("PYTHONUTF8", None)
    for extra in ([], ["--json"]):
        p = subprocess.run([sys.executable, "-m", "dia_cli", "validate", "schema", str(tmp_path), *extra],
                           cwd=CLI_DIR, env=env, capture_output=True)
        assert p.returncode == 1, p.stderr.decode("utf-8", "replace")
        assert b"Traceback" not in p.stderr


# ---------------------------------------------------------------------------
# L4 PATHS handling
# ---------------------------------------------------------------------------

def test_l4_mixed_paths_with_a_nonexistent_one_is_exit_2_and_prints_nothing_else(tmp_path):
    messy_tree(tmp_path)
    f = tmp_path / "Eco" / "eco.json"
    for args in ([str(tmp_path), str(tmp_path / "nope.json")], [str(tmp_path / "nope"), str(tmp_path)],
                 [str(f), str(tmp_path), str(tmp_path / "x" / "y.json")]):
        for extra in ([], ["--json"], ["--verbose"]):
            r = invoke([*args, *extra])
            assert r.exit_code == 2, (args, r.stdout)
            assert r.stdout == "", "no partial output"
            assert "path does not exist" in r.stderr and "nope" in r.stderr or "y.json" in r.stderr


def test_l4_files_and_folders_mixed(tmp_path):
    basic_tree(tmp_path / "one", rate=7)
    put(tmp_path / "two", "Eco/other.schema.json", schema("other.fmt", entry("n", type="int", default=1)))
    put(tmp_path / "three", "extra.json", {"format": "eco.params", "version": 1, "rate": 9})
    s = tmp_path / "two" / "Eco" / "other.schema.json"
    data = findings([str(tmp_path / "one"), str(tmp_path / "three" / "extra.json"), str(s)])
    assert sorted((f["file"], f["rule"]) for f in data) == [("Eco/eco.json", "data.out-of-range"),
                                                          ("extra.json", "data.out-of-range")]


def test_l4_empty_paths_scope_to_every_app(tmp_path, monkeypatch):
    root = repo(tmp_path, ("A", "B"))
    basic_tree(root / "Cluiche" / "Assets" / "B", rate=7)
    monkeypatch.setattr(schema_discovery, "get_repo_root", lambda: root)
    r = invoke([])
    assert r.exit_code == 1 and "FAIL B/Eco/eco.json" in r.stdout


def test_l4_empty_string_path_means_the_current_directory_pin(tmp_path, monkeypatch):
    basic_tree(tmp_path, rate=7)
    monkeypatch.chdir(tmp_path)
    r = invoke([""])
    assert r.exit_code == 1 and "FAIL Eco/eco.json" in r.stdout


def test_l4_non_json_file_path_is_accepted_and_quiet(tmp_path):
    f = put(tmp_path, "readme.md", "# hi")
    r = invoke([str(f)])
    assert r.exit_code == 0 and "0 files checked, 0 FAIL, 0 WARN" in r.stdout


def test_l4_path_with_spaces_and_trailing_separator(tmp_path):
    basic_tree(tmp_path / "my assets" / "sub dir", rate=7)
    r = invoke([str(tmp_path / "my assets") + os.sep])
    assert r.exit_code == 1 and "FAIL sub dir/Eco/eco.json" in r.stdout


# ---------------------------------------------------------------------------
# L5/L6 option combinations
# ---------------------------------------------------------------------------

def test_l6_verbose_with_json_is_json_only(tmp_path):
    basic_tree(tmp_path)
    r = invoke([str(tmp_path), "--verbose", "--json"])
    assert json.loads(r.stdout) == [] and "OK" not in r.stdout


def test_l6_verbose_text_lists_ok_files_in_order_and_hides_failed_ones(tmp_path):
    basic_tree(tmp_path)
    put(tmp_path, "Eco/bad.json", {"format": "eco.params", "version": 1, "rate": 9})
    r = invoke([str(tmp_path), "--verbose"])
    lines = r.stdout.splitlines()
    oks = [ln for ln in lines if ln.startswith("OK   ")]
    assert oks == ["OK   Eco/eco.json (1 keys)", "OK   Eco/eco.schema.json (1 keys)"]
    assert not any(ln.startswith("OK   Eco/bad.json") for ln in lines)


def test_l6_target_with_paths_validates_target_but_scopes_to_paths(tmp_path, monkeypatch):
    root = repo(tmp_path, ("CoW",), '[targets.cow]\nproject = "x"\napp_name = "CoW"\n')
    basic_tree(root / "Cluiche" / "Assets" / "CoW", rate=7)   # target content would FAIL
    basic_tree(tmp_path / "elsewhere", rate=0.5)
    monkeypatch.setattr(schema_discovery, "get_repo_root", lambda: root)
    ok = invoke(["--target", "cow", str(tmp_path / "elsewhere")])
    assert ok.exit_code == 0, ok.stdout
    bad = invoke(["--target", "nope", str(tmp_path / "elsewhere")])
    assert bad.exit_code == 2 and bad.stdout == ""


def test_l6_target_empty_string_is_unknown_target(tmp_path, monkeypatch):
    root = repo(tmp_path, ("CoW",))
    monkeypatch.setattr(schema_discovery, "get_repo_root", lambda: root)
    r = invoke(["--target", ""])
    assert r.exit_code == 2 and "unknown --target ''" in r.stderr


def test_l6_target_with_json_is_pure_json(tmp_path, monkeypatch):
    root = repo(tmp_path, ("CoW",))
    basic_tree(root / "Cluiche" / "Assets" / "CoW", rate=7)
    monkeypatch.setattr(schema_discovery, "get_repo_root", lambda: root)
    r = invoke(["--target", "cow", "--json"])
    assert r.exit_code == 1 and isinstance(json.loads(r.stdout), list)
    assert schema_codeside.NOTICE in r.stderr


def test_l6_option_may_appear_before_or_after_paths(tmp_path):
    basic_tree(tmp_path, rate=7)
    a = invoke(["--json", str(tmp_path)])
    b = invoke([str(tmp_path), "--json"])
    assert a.stdout == b.stdout and a.exit_code == b.exit_code == 1


def test_l6_unknown_option_is_usage_error(tmp_path):
    r = invoke([str(tmp_path), "--nope"])
    assert r.exit_code == 2 and "no such option" in r.stderr.lower()


def test_l6_option_missing_its_value(tmp_path):
    assert invoke([str(tmp_path), "--target"]).exit_code == 2
    assert invoke([str(tmp_path), "--code-side"]).exit_code == 2


# ---------------------------------------------------------------------------
# L7 help
# ---------------------------------------------------------------------------

def test_l7_help_lists_every_option_and_the_paths_argument():
    r = CliRunner().invoke(validate_cli, ["schema", "--help"])
    assert r.exit_code == 0
    for token in ("--target", "--json", "--verbose", "--code-side", "[PATHS]...", "--help"):
        assert token in r.output, token
    assert "schema" in CliRunner().invoke(validate_cli, ["--help"]).output


def test_l7_help_through_the_real_entry_point():
    p = subprocess.run([sys.executable, "-m", "dia_cli", "validate", "schema", "--help"], cwd=CLI_DIR,
                       capture_output=True, text=True)
    assert p.returncode == 0 and "--code-side" in p.stdout


# ---------------------------------------------------------------------------
# L8 exit-code table, real runs
# ---------------------------------------------------------------------------

def _code(args, **kw):
    return invoke(args, **kw).exit_code


def test_l8_exit_code_table(tmp_path, monkeypatch):
    clean = tmp_path / "clean"
    basic_tree(clean)
    warn = tmp_path / "warn"
    put(warn, "m.json", {"format": "mystery", "version": 1})
    fail = tmp_path / "fail"
    basic_tree(fail, rate=7)
    bad_export = tmp_path / "bad.json"
    bad_export.write_text("[]")
    assert _code([str(clean)]) == 0
    assert _code([str(warn)]) == 0
    assert _code([str(fail)]) == 1
    assert _code([str(fail), "--json"]) == 1
    assert _code([str(tmp_path / "missing")]) == 2
    assert _code([str(clean), "--code-side", str(bad_export)]) == 2
    assert _code([str(clean), "--code-side", str(tmp_path / "missing.json")]) == 2
    assert _code(["--target", "no-such-app"]) == 2
    assert _code([str(clean), "--bogus"]) == 2


def test_l8_fail_wins_over_warn_and_codeside_warns_never_fail(tmp_path):
    basic_tree(tmp_path, rate=7)
    put(tmp_path, "m.json", {"format": "mystery", "version": 1})
    r = invoke([str(tmp_path)])
    assert r.exit_code == 1 and "1 FAIL, 1 WARN" in r.stdout


def test_l8_real_process_exit_codes(tmp_path):
    basic_tree(tmp_path / "ok")
    basic_tree(tmp_path / "bad", rate=7)

    def go(*a):
        return subprocess.run([sys.executable, "-m", "dia_cli", "validate", "schema", *a], cwd=CLI_DIR,
                              capture_output=True, text=True).returncode

    assert go(str(tmp_path / "ok")) == 0
    assert go(str(tmp_path / "bad")) == 1
    assert go(str(tmp_path / "missing")) == 2


def test_l8_findings_header_per_finding_pin(tmp_path):
    """One `FAIL <file>` header per finding, even for several findings in the same file."""
    put(tmp_path, "Eco/eco.schema.json", schema("eco.params", entry("rate", min=0, max=1, default=0.5)))
    put(tmp_path, "Eco/eco.json", {"format": "eco.params", "version": 1, "rate": 7, "other": 1})
    out = invoke([str(tmp_path)]).stdout.splitlines()
    assert out.count("FAIL Eco/eco.json") == 2


def test_l8_warn_only_file_is_listed_ok_in_verbose_pin(tmp_path):
    """A file whose only finding is a WARN counts as OK for --verbose (only FAIL hides the OK line)."""
    put(tmp_path, "Eco/eco.schema.json", schema("eco.params", entry("rate", min=0, max=1, default=0.5)))
    put(tmp_path, "Eco/eco.json", {"format": "eco.params", "version": 1, "rate": 0.5})
    put(tmp_path, "Eco/refs.schema.json", schema("r.f", entry("a[].b", type="ref", ref="gone.fmt/x[].y")))
    put(tmp_path, "Eco/refs.json", {"format": "r.f", "version": 1, "a": [{"b": "z"}]})
    out = invoke([str(tmp_path), "--verbose"]).stdout
    assert "WARN" in out and "OK   Eco/refs.json" in out


def test_l8_ok_lines_only_with_verbose_pin_differs_from_spec_example(tmp_path):
    """The spec example prints OK lines for every OK file; the implementation prints them only with --verbose."""
    basic_tree(tmp_path)
    assert "OK" not in invoke([str(tmp_path)]).stdout
    assert "OK   Eco/eco.json" in invoke([str(tmp_path), "--verbose"]).stdout


def test_l8_run_validation_ignores_code_side_parameter_pin(tmp_path, monkeypatch):
    from dia_cli.commands.validate import schema_cmd
    basic_tree(tmp_path)
    a = schema_cmd.run_validation([tmp_path], None, None)
    b = schema_cmd.run_validation([tmp_path], None, tmp_path / "whatever-is-not-read.json")
    assert a == b


def test_l8_dedup_is_per_finding_not_global_pin(tmp_path):
    """Identical findings from different files are all printed; nothing is deduplicated by message."""
    for n in ("a", "b"):
        put(tmp_path, f"{n}.json", {"format": "mystery", "version": 1})
    out = invoke([str(tmp_path)]).stdout
    assert out.count("WARN format `mystery` has no schema") == 2
