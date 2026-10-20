"""validate manifest warns about `world`-tagged catalogue records outside Global/ (#17 AC5)."""
import json
from pathlib import Path
from unittest.mock import patch

from click.testing import CliRunner

from dia_cli.cli.cli_validate import cli as validate_cli


def _run(tmp_path: Path, records: list):
    (tmp_path / "assets.catalogue.json").write_text(json.dumps({"assets": records}), encoding="utf-8")
    game = {"name": "G", "version": 1,
            "config": {"name": "G", "window": {"title": "G"}, "asset_catalogue": "assets.catalogue.json"},
            "imports": []}
    p = tmp_path / "g.diagame"
    p.write_text(json.dumps(game), encoding="utf-8")
    with patch("dia_cli.cli.cli_validate.find_repo_root", return_value=tmp_path):
        return CliRunner().invoke(validate_cli, ["manifest", "--path", str(p)])


def _rec(rid, source_path, tags):
    return {"id": rid, "source_path": source_path, "tags": tags, "scope": "global", "type": "json"}


def test_world_record_outside_global_warns(tmp_path):
    r = _run(tmp_path, [_rec("world.bad", "Weather/x.json", ["world"])])
    assert r.exit_code == 0
    assert "WARN" in r.output and "world.bad" in r.output and "Global/" in r.output


def test_world_record_under_global_no_warning(tmp_path):
    r = _run(tmp_path, [_rec("world.ok", "Global/Weather/x.json", ["world"])])
    assert "WARN" not in r.output


def test_non_world_record_outside_global_no_warning(tmp_path):
    r = _run(tmp_path, [_rec("misc.a", "foo/x.json", ["misc"])])
    assert "WARN" not in r.output
