"""dia validate schema: command skeleton, output format, exit codes (DiaValueSchema Feature 2)."""
import json

from click.testing import CliRunner

from dia_cli.cli.cli_validate import cli as validate_cli
from dia_cli.commands.validate import schema_cmd
from dia_cli.commands.validate.schema_cmd import Finding


def _invoke(args):
    return CliRunner().invoke(validate_cli, ["schema", *args])


def _patch(monkeypatch, findings, files=None):
    monkeypatch.setattr(schema_cmd, "run_validation",
                        lambda paths, target, code_side: (findings, files if files is not None else []))
    # keep these skeleton tests independent of any real export under Cluiche/out
    from dia_cli.commands.validate import schema_codeside
    monkeypatch.setattr(schema_codeside, "run_code_side", lambda target, code_path: ([], None))


def test_registered_under_validate():
    r = CliRunner().invoke(validate_cli, ["--help"])
    assert r.exit_code == 0 and "schema" in r.output


def test_no_findings_exit_0_with_summary(monkeypatch):
    _patch(monkeypatch, [], [("a.json", 3)])
    r = _invoke([])
    assert r.exit_code == 0
    assert "1 files checked, 0 FAIL, 0 WARN" in r.output
    assert "OK" not in r.output


def test_verbose_prints_ok_lines(monkeypatch):
    _patch(monkeypatch, [], [("a.json", 3)])
    r = _invoke(["--verbose"])
    assert "OK   a.json (3 keys)" in r.output


def test_fail_exit_1_and_format(monkeypatch):
    _patch(monkeypatch, [Finding("FAIL", "R1", "a.json", "k", "bad key 'k'")], [("a.json", 1)])
    r = _invoke([])
    assert r.exit_code == 1
    lines = r.output.splitlines()
    assert "FAIL a.json" in lines
    assert "     bad key 'k'" in lines
    assert "1 files checked, 1 FAIL, 0 WARN" in r.output


def test_warn_only_exit_0(monkeypatch):
    _patch(monkeypatch, [Finding("WARN", "W1", "a.json", None, "just a warning")], [("a.json", 1)])
    r = _invoke([])
    assert r.exit_code == 0
    assert "WARN just a warning" in r.output
    assert "0 FAIL, 1 WARN" in r.output


def test_json_output_shape(monkeypatch):
    _patch(monkeypatch, [Finding("FAIL", "R1", "a.json", "k", "msg", suggestion="s")])
    r = _invoke(["--json"])
    data = json.loads(r.output)
    assert data == [{"severity": "FAIL", "rule": "R1", "file": "a.json", "key": "k",
                     "message": "msg", "suggestion": "s", "path": None, "record": None}]
    assert r.exit_code == 1


def test_nonexistent_path_exit_2(tmp_path):
    r = _invoke([str(tmp_path / "nope.json")])
    assert r.exit_code == 2


# ---- rendering / serialisation units (no filesystem) -------------------------

def test_to_json_drops_owner_only_when_none():
    assert "owner" not in schema_cmd._to_json(Finding("WARN", "R", "f", None, "m"))
    assert schema_cmd._to_json(Finding("WARN", "R", "f", None, "m", owner=""))["owner"] == ""
    assert schema_cmd._to_json(Finding("WARN", "R", "f", None, "m", owner="O"))["owner"] == "O"


def test_owner_on_a_rule_without_a_group_noun_prints_ungrouped_pin(monkeypatch):
    _patch(monkeypatch, [Finding("WARN", "other.rule", "valueschema.json", "k", "plain message", owner="O")])
    r = _invoke([])
    assert "WARN plain message" in r.output and "O:" not in r.output


def test_fail_with_owner_still_prints_as_fail(monkeypatch):
    _patch(monkeypatch, [Finding("FAIL", "codeside.undescribed-metric", "f", "k", "boom", owner="O")])
    r = _invoke([])
    assert r.exit_code == 1 and "FAIL f" in r.output and "     boom" in r.output


def test_ok_line_hidden_for_failed_files_only(monkeypatch):
    _patch(monkeypatch, [Finding("FAIL", "R", "bad.json", "k", "m"), Finding("WARN", "W", "warn.json", None, "w")],
           [("bad.json", 1), ("warn.json", 2), ("good.json", 3)])
    r = _invoke(["--verbose"])
    assert "OK   good.json (3 keys)" in r.output and "OK   warn.json (2 keys)" in r.output
    assert "OK   bad.json" not in r.output
    assert "3 files checked, 1 FAIL, 1 WARN" in r.output


def test_json_mode_with_failure_still_exits_1_and_prints_valid_json(monkeypatch):
    _patch(monkeypatch, [Finding("FAIL", "R", "a", "k", "m \u00e9\u65e5"), Finding("WARN", "W", "b", None, "w")])
    r = _invoke(["--json"])
    assert r.exit_code == 1
    assert [f["severity"] for f in json.loads(r.output)] == ["FAIL", "WARN"]


def test_run_validation_receives_resolved_args(monkeypatch, tmp_path):
    seen = {}

    def fake(paths, target, code_side):
        seen.update(paths=paths, target=target, code_side=code_side)
        return [], []

    monkeypatch.setattr(schema_cmd, "run_validation", fake)
    ex = tmp_path / "ex.json"
    ex.write_text(json.dumps({"format": "dia.value_schema_export", "version": 1}))
    r = _invoke([str(tmp_path), "--code-side", str(ex)])
    assert r.exit_code == 0
    assert seen["paths"] == [tmp_path] and seen["target"] is None and seen["code_side"] == ex
