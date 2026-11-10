"""'source' is a reserved dev-only path alias (set by `dia launch --dev-alias`)."""
import json
from pathlib import Path
from unittest.mock import patch

from click.testing import CliRunner

from dia_cli.cli.cli_validate import cli as validate_cli


def _run(tmp_path: Path, name: str, data: dict):
    p = tmp_path / name
    p.write_text(json.dumps(data), encoding="utf-8")
    with patch("dia_cli.cli.cli_validate.find_repo_root", return_value=tmp_path):
        return CliRunner().invoke(validate_cli, ["manifest", "--path", str(p)])


def _game(aliases):
    return {"name": "G", "version": 1,
            "config": {"name": "G", "window": {"title": "G"}, "path_aliases": aliases},
            "imports": []}


def _stage(aliases):
    return {"name": "S", "manifest": "t.diagame", "config": {"path_aliases": aliases}}


def test_diagame_rejects_source_alias(tmp_path):
    r = _run(tmp_path, "g.diagame", _game({"source": "Assets"}))
    assert r.exit_code == 1
    assert "reserved" in r.output


def test_diastage_rejects_source_alias(tmp_path):
    r = _run(tmp_path, "s.diastage", _stage({"source": "Assets"}))
    assert r.exit_code == 1
    assert "reserved" in r.output


def test_diagame_without_source_passes(tmp_path):
    assert _run(tmp_path, "g.diagame", _game({"assets": "Assets"})).exit_code == 0


def test_diastage_without_source_passes(tmp_path):
    assert _run(tmp_path, "s.diastage", _stage({"assets": "Assets"})).exit_code == 0
