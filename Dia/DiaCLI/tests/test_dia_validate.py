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


# ---------------------------------------------------------------------------
# ServiceStream composition (.diagame + imported manifests)
# ---------------------------------------------------------------------------

def _service_game(tmp_path: Path, consumer: bool) -> Path:
    def module(role: str) -> dict:
        return {"instance_id": role, "type_id": role, "stages": [], "dependencies": [],
                "channels": [{"id": "Svc", "role": role}]}

    main = {
        "version": 3,
        "streams": [{"id": "Svc", "kind": "ServiceStream", "payload_type": "P"}],
        "processing_units": [{"instance_id": "PU", "frequency_hz": 30, "dedicated_thread": False,
                              "modules": [module("provides")]}],
    }
    (tmp_path / "main.diaapp").write_text(json.dumps(main), encoding="utf-8")
    imports = [{"path": "main.diaapp", "type": "manifest"}]
    if consumer:
        stage = {"version": 3, "processing_units": [{"instance_id": "PU", "frequency_hz": 30,
                 "dedicated_thread": False, "modules": [module("consumes")]}]}
        (tmp_path / "stage.diaapp").write_text(json.dumps(stage), encoding="utf-8")
        (tmp_path / "s.diastage").write_text(json.dumps(
            {"name": "S", "manifest": "stage.diaapp", "config": {"path_aliases": {}}}), encoding="utf-8")
        imports.append({"path": "s.diastage", "type": "stage"})
    game = {"name": "G", "version": 1, "config": {"name": "G", "window": {"title": "G"}}, "imports": imports}
    return _write_stage(tmp_path, "g.diagame", game)


def test_validate_manifest_flags_orphan_service_provider(tmp_path):
    game = _service_game(tmp_path, consumer=False)
    with patch("dia_cli.cli.cli_validate.find_repo_root", return_value=tmp_path):
        result = CliRunner().invoke(validate_cli, ["manifest", "--path", str(game)])
    assert result.exit_code == 1
    assert "SERVICE_STREAM_ORPHAN_PROVIDER" in result.output


def test_validate_manifest_service_consumer_in_stage_import_is_ok(tmp_path):
    game = _service_game(tmp_path, consumer=True)
    with patch("dia_cli.cli.cli_validate.find_repo_root", return_value=tmp_path):
        result = CliRunner().invoke(validate_cli, ["manifest", "--path", str(game)])
    assert result.exit_code == 0, result.output
