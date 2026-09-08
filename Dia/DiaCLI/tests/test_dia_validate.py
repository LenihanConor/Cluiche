"""Unit tests for dia validate manifest command."""
import json
from pathlib import Path
from unittest.mock import patch

import pytest
from click.testing import CliRunner

from dia_cli.cli.cli_validate import cli as validate_cli


def _write_stage(tmp_path: Path, name: str, data: dict) -> Path:
    p = tmp_path / name
    p.write_text(json.dumps(data), encoding="utf-8")
    return p


# ---------------------------------------------------------------------------
# CLI discovery
# ---------------------------------------------------------------------------

def test_validate_command_discovered():
    runner = CliRunner()
    result = runner.invoke(validate_cli, ["--help"])
    assert result.exit_code == 0
    assert "manifest" in result.output


def test_validate_manifest_help():
    runner = CliRunner()
    result = runner.invoke(validate_cli, ["manifest", "--help"])
    assert result.exit_code == 0
    assert "--path" in result.output
    assert "--verbose" in result.output


# ---------------------------------------------------------------------------
# Valid .diastage
# ---------------------------------------------------------------------------

def test_validate_manifest_valid_diastage(tmp_path):
    stage_file = _write_stage(
        tmp_path,
        "teststage.diastage",
        {"name": "TestStage", "manifest": "test.diagame", "config": {"path_aliases": {}}},
    )
    runner = CliRunner()
    with patch("dia_cli.cli.cli_validate.find_repo_root", return_value=tmp_path):
        result = runner.invoke(validate_cli, ["manifest", "--path", str(stage_file)])
    assert result.exit_code == 0


# ---------------------------------------------------------------------------
# Invalid .diastage (missing required key)
# ---------------------------------------------------------------------------

def test_validate_manifest_invalid_diastage(tmp_path):
    stage_file = _write_stage(
        tmp_path,
        "bad.diastage",
        {"manifest": "test.diagame", "config": {"path_aliases": {}}},
    )
    runner = CliRunner()
    with patch("dia_cli.cli.cli_validate.find_repo_root", return_value=tmp_path):
        result = runner.invoke(validate_cli, ["manifest", "--path", str(stage_file)])
    assert result.exit_code == 1


# ---------------------------------------------------------------------------
# --verbose shows OK for valid file
# ---------------------------------------------------------------------------

def test_validate_manifest_verbose_shows_ok(tmp_path):
    stage_file = _write_stage(
        tmp_path,
        "good.diastage",
        {"name": "TestStage", "manifest": "test.diagame", "config": {"path_aliases": {}}},
    )
    runner = CliRunner()
    with patch("dia_cli.cli.cli_validate.find_repo_root", return_value=tmp_path):
        result = runner.invoke(validate_cli, ["manifest", "--path", str(stage_file), "--verbose"])
    assert result.exit_code == 0
    assert "OK" in result.output
