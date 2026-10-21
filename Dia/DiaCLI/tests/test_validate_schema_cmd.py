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
