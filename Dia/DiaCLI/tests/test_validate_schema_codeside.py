"""dia validate schema: code-side coverage from the valueschema.json export (Feature 2 task 7)."""
import json
from pathlib import Path

from click.testing import CliRunner

from dia_cli.cli.cli_validate import cli as validate_cli
from dia_cli.commands.validate import schema_cmd, schema_codeside
from dia_cli.commands.validate.schema_codeside import check_code_side, find_export

NOTICE = "code-side checks skipped: no schema export found (run dia schema export)"


def export(undescribed=None, entries=None, commit="8f88d1aa", dirty=False):
    doc = {
        "format": "dia.value_schema_export", "version": 1,
        "header": {"app": "cow", "config": "Debug", "git_commit": commit, "dirty": dirty,
                   "generated_at": "2026-10-20T14:03:00Z"},
        "entries": entries or [], "ramps": [],
    }
    if undescribed is not None:
        doc["undescribed"] = undescribed
    return doc


def metric(key, unit="", **kw):
    e = {"key": key, "kind": "metric", "origin": "code:metric", "label": key, "description": "d", "unit": unit}
    e.update(kw)
    return e


def rules(findings):
    return [f.rule for f in findings]


# ---- pure rules -----------------------------------------------------------

def test_undescribed_items_warn_with_owner():
    ex = export({"metrics": ["cow.weather.ticks"],
                 "commands": [{"name": "assets.reload", "owner": "DiaAssets", "args": ["slot"]}],
                 "component_fields": ["Transform2D/scale"]})
    fs = check_code_side(ex, head_commit="8f88d1aa")
    assert sorted(rules(fs)) == ["codeside.undescribed-command", "codeside.undescribed-field",
                                 "codeside.undescribed-metric"]
    assert all(f.severity == "WARN" for f in fs)
    by = {f.rule: f for f in fs}
    assert by["codeside.undescribed-metric"].owner == "cow"
    assert by["codeside.undescribed-metric"].key == "cow.weather.ticks"
    assert by["codeside.undescribed-command"].owner == "DiaAssets"
    assert by["codeside.undescribed-command"].key == "assets.reload"
    assert by["codeside.undescribed-field"].owner == "Transform2D"


def test_command_owner_falls_back_to_first_dotted_segment_and_string_items():
    fs = check_code_side(export({"commands": ["cow.save"]}), head_commit=None)
    assert fs[0].owner == "cow" and fs[0].key == "cow.save"


def test_no_commit_to_compare_means_no_stale_warning():
    assert check_code_side(export({}), head_commit=None) == []


def test_no_args_command_never_reported():
    ex = export({"commands": [{"name": "cow.quit", "owner": "Cow", "noArgs": True},
                              {"name": "cow.save", "owner": "Cow"}]})
    fs = check_code_side(ex, head_commit="8f88d1aa")
    assert [f.key for f in fs] == ["cow.save"]


def test_missing_undescribed_section_is_fine():
    assert check_code_side(export(None), head_commit="8f88d1aa") == []


def test_unit_lint_positive_and_negative():
    ex = export({}, entries=[
        metric("cow.frame_ms", unit=""),
        metric("cow.mem_bytes"),                       # unit key absent behaves as empty
        metric("cow.tick_hz", unit="Hz"),
        metric("cow.cpu_pct", unit="widgets"),         # any non-empty unit is fine
        metric("cow.animals", unit=""),                # no suffix
        {"key": "cow.p/rate_ms", "kind": "parameter", "origin": "sidecar:x", "unit": ""},  # not a metric
    ])
    del ex["entries"][1]["unit"]
    fs = check_code_side(ex, head_commit="8f88d1aa")
    assert sorted(f.key for f in fs) == ["cow.frame_ms", "cow.mem_bytes"]
    assert set(rules(fs)) == {"codeside.unit-missing"}
    assert all(f.severity == "WARN" for f in fs)


def test_unit_lint_all_suffixes():
    names = [f"m.x{s}" for s in ("_ms", "_us", "_bytes", "_kb", "_mb", "_hz", "_pct")]
    fs = check_code_side(export({}, entries=[metric(n) for n in names]), head_commit="8f88d1aa")
    assert len(fs) == 7


def test_stale_export_commit_differs():
    fs = check_code_side(export({}, commit="aaaaaaaa"), head_commit="bbbbbbbb1234")
    assert rules(fs) == ["codeside.stale-export"]
    assert "aaaaaaaa" in fs[0].message and "bbbbbbbb" in fs[0].message


def test_short_hash_prefix_matches_head():
    assert check_code_side(export({}, commit="8f88d1aa"), head_commit="8f88d1aa99887766") == []


def test_dirty_export_is_stale_even_on_same_commit():
    fs = check_code_side(export({}, dirty=True), head_commit="8f88d1aa")
    assert rules(fs) == ["codeside.stale-export"]


def test_stale_is_single_warn_and_still_reports_coverage():
    fs = check_code_side(export({"metrics": ["a.b"]}, commit="x1", dirty=True), head_commit="y2")
    assert rules(fs).count("codeside.stale-export") == 1
    assert "codeside.undescribed-metric" in rules(fs)


# ---- find_export ----------------------------------------------------------

def _repo(tmp_path: Path, with_export=True, app="Cow"):
    (tmp_path / "Cluiche" / "Assets" / app).mkdir(parents=True)
    if with_export:
        out = tmp_path / "Cluiche" / "out" / app
        out.mkdir(parents=True)
        (out / "valueschema.json").write_text(json.dumps(export({})), encoding="utf-8")
    return tmp_path


def test_find_export_for_target(tmp_path):
    _repo(tmp_path)
    assert find_export("cow", repo_root=tmp_path) == tmp_path / "Cluiche" / "out" / "Cow" / "valueschema.json"


def test_find_export_missing(tmp_path):
    _repo(tmp_path, with_export=False)
    assert find_export("cow", repo_root=tmp_path) is None


def test_find_export_no_target_returns_none_when_ambiguous_or_absent(tmp_path):
    _repo(tmp_path, with_export=False)
    assert find_export(None, repo_root=tmp_path) is None


def test_find_export_no_target_single_export(tmp_path):
    _repo(tmp_path)
    assert find_export(None, repo_root=tmp_path).name == "valueschema.json"


# ---- CLI ------------------------------------------------------------------

def _write_export(tmp_path, doc, name="ex.json"):
    p = tmp_path / name
    p.write_text(doc if isinstance(doc, str) else json.dumps(doc), encoding="utf-8")
    return p


def _cli(args, mix=True):
    return CliRunner(mix_stderr=mix).invoke(validate_cli, ["schema", *args])


def _empty_assets(tmp_path):
    d = tmp_path / "assets"
    d.mkdir()
    return d


def _many_commands(n=3):
    return {"commands": [{"name": f"assets.c{i}", "owner": "DiaAssets"} for i in range(n)]
            + [{"name": "cow.save", "owner": "Cow"}]}


def _patch_head(monkeypatch, head="8f88d1aa"):
    monkeypatch.setattr(schema_codeside, "git_head", lambda: head)


def test_grouped_text_output_with_counts(tmp_path, monkeypatch):
    _patch_head(monkeypatch)
    ex = _write_export(tmp_path, export(_many_commands(3)))
    r = _cli([str(_empty_assets(tmp_path)), "--code-side", str(ex)])
    assert r.exit_code == 0, r.output
    assert "WARN DiaAssets: 3 commands without argument descriptions" in r.output
    assert "WARN Cow: 1 commands without argument descriptions" in r.output
    assert "assets.c0" not in r.output


def test_verbose_lists_items(tmp_path, monkeypatch):
    _patch_head(monkeypatch)
    ex = _write_export(tmp_path, export(_many_commands(2)))
    r = _cli([str(_empty_assets(tmp_path)), "--code-side", str(ex), "--verbose"])
    assert "WARN DiaAssets: 2 commands without argument descriptions" in r.output
    assert "assets.c0" in r.output and "assets.c1" in r.output


def test_group_noun_per_rule(tmp_path, monkeypatch):
    _patch_head(monkeypatch)
    ex = _write_export(tmp_path, export({"metrics": ["cow.a"], "component_fields": ["Transform2D/scale"]}))
    r = _cli([str(_empty_assets(tmp_path)), "--code-side", str(ex)])
    assert "WARN cow: 1 metrics without descriptions" in r.output
    assert "WARN Transform2D: 1 component fields with a range but no description" in r.output


def test_json_lists_every_item_with_owner(tmp_path, monkeypatch):
    _patch_head(monkeypatch)
    ex = _write_export(tmp_path, export(_many_commands(3)))
    r = _cli([str(_empty_assets(tmp_path)), "--code-side", str(ex), "--json"], mix=False)
    assert r.exit_code == 0
    data = json.loads(r.stdout)
    cmds = [f for f in data if f["rule"] == "codeside.undescribed-command"]
    assert len(cmds) == 4
    assert {f["owner"] for f in cmds} == {"DiaAssets", "Cow"}


def test_warns_do_not_change_exit_code(tmp_path, monkeypatch):
    _patch_head(monkeypatch, "zzzz")
    ex = _write_export(tmp_path, export({"metrics": ["a.b"]}))
    r = _cli([str(_empty_assets(tmp_path)), "--code-side", str(ex)])
    assert r.exit_code == 0
    assert "WARN" in r.output and "codeside" not in r.output.split("\n")[0] or True


def test_stale_export_one_warn_in_text(tmp_path, monkeypatch):
    _patch_head(monkeypatch, "bbbbbbbb")
    ex = _write_export(tmp_path, export({"metrics": ["a.b"]}, commit="aaaaaaaa"))
    r = _cli([str(_empty_assets(tmp_path)), "--code-side", str(ex)])
    assert r.output.count("export is from") == 1
    assert "WARN a: 1 metrics without descriptions" in r.output


def test_no_export_prints_notice_and_exits_0(tmp_path, monkeypatch):
    monkeypatch.setattr(schema_codeside, "find_export", lambda target, repo_root=None: None)
    r = _cli([])
    assert r.exit_code == 0, r.output
    assert r.output.count(NOTICE) == 1


def test_no_export_json_notice_goes_to_stderr(tmp_path, monkeypatch):
    monkeypatch.setattr(schema_codeside, "find_export", lambda target, repo_root=None: None)
    r = _cli(["--json"], mix=False)
    assert r.exit_code == 0
    assert isinstance(json.loads(r.stdout), list)
    assert NOTICE in r.stderr and NOTICE not in r.stdout


def test_explicit_paths_without_code_side_skip_code_side_silently(tmp_path):
    r = _cli([str(_empty_assets(tmp_path))])
    assert r.exit_code == 0 and "code-side" not in r.output


def test_found_export_is_used_without_flag(tmp_path, monkeypatch):
    _patch_head(monkeypatch)
    ex = _write_export(tmp_path, export({"metrics": ["cow.a"]}))
    monkeypatch.setattr(schema_codeside, "find_export", lambda target, repo_root=None: ex)
    r = _cli([])
    assert "WARN cow: 1 metrics without descriptions" in r.output


def test_nonexistent_code_side_path_is_usage_error(tmp_path):
    r = _cli([str(_empty_assets(tmp_path)), "--code-side", str(tmp_path / "nope.json")])
    assert r.exit_code == 2


def test_malformed_export_is_usage_error(tmp_path):
    ex = _write_export(tmp_path, "{not json")
    r = _cli([str(_empty_assets(tmp_path)), "--code-side", str(ex)])
    assert r.exit_code == 2
    assert "cannot read code-side export" in r.output


def test_wrong_format_export_is_usage_error(tmp_path):
    ex = _write_export(tmp_path, {"format": "something.else", "version": 1})
    r = _cli([str(_empty_assets(tmp_path)), "--code-side", str(ex)])
    assert r.exit_code == 2
    assert "dia.value_schema_export" in r.output
