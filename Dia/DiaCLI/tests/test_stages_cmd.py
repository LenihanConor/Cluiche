"""Tests for dia stages (list/get/results) against the shared StageCatalogue fixtures."""
import json
from pathlib import Path

import pytest
from click.testing import CliRunner

from dia_cli.cli.stages import cli

FIXTURES = Path(__file__).resolve().parents[3] / "Cluiche" / "Tests" / "Fixtures" / "StageCatalogue"
DIAGAME = str(FIXTURES / "basic" / "game.diagame")


def _results(tmp_path, body=None):
    p = tmp_path / "stage_results.json"
    if body is None:
        body = {"format": "dia.stage_results", "version": 1, "results": {
            "ShowTest": {"severity": "ok", "label": "PASS", "utc": 1, "session_id": "s", "log_path": "l"}}}
    p.write_text(body if isinstance(body, str) else json.dumps(body), encoding="utf-8")
    return str(p)


def _run(args, tmp_path, results=None):
    res = results if results is not None else str(tmp_path / "none.json")
    return CliRunner(mix_stderr=False).invoke(cli, args + ["--diagame", DIAGAME, "--results", res])


def test_list_json_shape(tmp_path):
    r = _run(["list", "--json"], tmp_path, _results(tmp_path))
    assert r.exit_code == 0, r.output
    data = json.loads(r.stdout)
    by = {e["name"]: e for e in data}
    assert set(by["ShowTest"]) >= {"name", "kinds", "tags", "description", "presets", "last_result"}
    assert by["ShowTest"]["last_result"]["label"] == "PASS"
    assert by["Plain"]["last_result"] is None
    assert by["Plain"]["presets"] == []


def test_list_kind_filter(tmp_path):
    names = [e["name"] for e in json.loads(_run(["list", "--json", "--kind", "test"], tmp_path).stdout)]
    assert "ShowTest" in names and "Plain" not in names


def test_list_kind_other(tmp_path):
    names = [e["name"] for e in json.loads(_run(["list", "--json", "--kind", "other"], tmp_path).stdout)]
    assert "Plain" in names and "ShowTest" not in names


def test_list_bad_kind(tmp_path):
    assert _run(["list", "--kind", "bogus"], tmp_path).exit_code != 0


def test_list_tag_and(tmp_path):
    one = [e["name"] for e in json.loads(_run(["list", "--json", "--tag", "test"], tmp_path).stdout)]
    two = [e["name"] for e in json.loads(_run(["list", "--json", "--tag", "test", "--tag", "showcase"], tmp_path).stdout)]
    assert "ShowTest" in one and "ShowTest" in two
    assert len(two) < len(one) or set(two) <= set(one)
    assert "TestRig" not in two or "TestRig" in one


def test_list_table(tmp_path):
    r = _run(["list"], tmp_path, _results(tmp_path))
    assert r.exit_code == 0
    head = r.stdout.splitlines()[0]
    for col in ("Stage", "Kinds", "Tags", "Result", "Description"):
        assert col in head
    assert "PASS" in r.stdout


def test_get_known(tmp_path):
    r = _run(["get", "ShowTest", "--json"], tmp_path, _results(tmp_path))
    assert r.exit_code == 0
    d = json.loads(r.stdout)
    assert d["name"] == "ShowTest" and d["presets"] == [] and d["proves"] == []


def test_get_unknown(tmp_path):
    r = _run(["get", "Nope"], tmp_path)
    assert r.exit_code != 0 and "Nope" in r.stderr


def test_results_with_file(tmp_path):
    r = _run(["results", "--json"], tmp_path, _results(tmp_path))
    assert r.exit_code == 0
    assert json.loads(r.stdout)["ShowTest"]["severity"] == "ok"


def test_results_missing_file(tmp_path):
    r = _run(["results", "--json"], tmp_path)
    assert r.exit_code == 0 and json.loads(r.stdout) == {}


def test_results_malformed(tmp_path):
    r = _run(["results", "--json"], tmp_path, _results(tmp_path, "{not json"))
    assert r.exit_code == 0 and json.loads(r.stdout) == {}


def test_results_wrong_version(tmp_path):
    body = {"format": "dia.stage_results", "version": 2, "results": {"ShowTest": {"label": "X"}}}
    r = _run(["results", "--json"], tmp_path, _results(tmp_path, body))
    assert r.exit_code == 0 and json.loads(r.stdout) == {}


def test_results_single_stage(tmp_path):
    r = _run(["results", "ShowTest", "--json"], tmp_path, _results(tmp_path))
    assert json.loads(r.stdout)["label"] == "PASS"


def test_list_and_get_json_carry_presets_without_overrides(tmp_path):
    diagame = str(FIXTURES / "presets" / "game.diagame")
    res = str(tmp_path / "none.json")
    r = CliRunner(mix_stderr=False).invoke(cli, ["list", "--json", "--diagame", diagame, "--results", res])
    assert r.exit_code == 0, r.output
    by = {e["name"]: e for e in json.loads(r.stdout)}
    assert by["WithPresets"]["presets"] == [{"name": "alpine42", "description": "Alpine, seed 42"},
                                            {"name": "bare", "description": ""}]
    assert by["NoPresets"]["presets"] == []
    r = CliRunner(mix_stderr=False).invoke(cli, ["get", "WithPresets", "--json", "--diagame", diagame, "--results", res])
    assert r.exit_code == 0, r.output
    assert "modules" not in r.stdout
    assert [p["name"] for p in json.loads(r.stdout)["presets"]] == ["alpine42", "bare"]
    r = CliRunner(mix_stderr=False).invoke(cli, ["get", "WithPresets", "--diagame", diagame, "--results", res])
    assert "alpine42, bare" in r.stdout
