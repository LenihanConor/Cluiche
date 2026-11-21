"""CLI tests for `dia check stage-proves` (CliRunner against a tmp fixture tree)."""
import json

from click.testing import CliRunner

from dia_cli.commands.check.stage_proves_cmd import stage_proves_cmd
from tests.test_stage_proves import _game, _spec


def _run(root, *args):
    return CliRunner().invoke(stage_proves_cmd, ["--root", str(root), *args])


def _report(root):
    return (root / "Cluiche" / "out" / "check" / "stage-proves.md").read_text(encoding="utf-8")


def test_clean_tree_exit_0(tmp_path):
    spec = _spec(tmp_path, "a")
    _game(tmp_path, "A", {"S1": [spec]})
    r = _run(tmp_path)
    assert r.exit_code == 0, r.output


def test_broken_link_exit_1_and_listed(tmp_path):
    _game(tmp_path, "A", {"S1": ["docs/specs/nope.md"]})
    r = _run(tmp_path)
    assert r.exit_code == 1
    assert "docs/specs/nope.md" in r.output and "S1" in r.output


def test_unproven_exit_0_listed(tmp_path):
    spec = _spec(tmp_path, "a")
    _game(tmp_path, "A", {"S1": []})
    r = _run(tmp_path)
    assert r.exit_code == 0
    assert spec in r.output


def test_unproven_strict_exit_1(tmp_path):
    _spec(tmp_path, "a")
    _game(tmp_path, "A", {"S1": []})
    assert _run(tmp_path, "--strict").exit_code == 1


def test_json_has_three_sets(tmp_path):
    spec = _spec(tmp_path, "a")
    spec2 = _spec(tmp_path, "b")
    _game(tmp_path, "A", {"S1": [spec, "docs/specs/nope.md"]})
    r = _run(tmp_path, "--json")
    data = json.loads(r.output)
    assert data["broken_links"] == [{"stage": "S1", "game": "A", "path": "docs/specs/nope.md"}]
    assert data["unproven"] == [spec2]
    assert data["coverage"][spec] == ["S1"]
    assert (tmp_path / "Cluiche" / "out" / "check" / "stage-proves.md").is_file()


def test_report_sections_and_coverage(tmp_path):
    spec = _spec(tmp_path, "a")
    _spec(tmp_path, "b")
    _game(tmp_path, "A", {"S1": [spec]})
    _run(tmp_path)
    text = _report(tmp_path)
    for h in ("## Broken links", "## Unproven specs", "## Coverage"):
        assert h in text
    assert spec in text and "S1" in text


def test_game_limits_broken_links(tmp_path):
    _game(tmp_path, "A", {"S1": ["docs/specs/nope.md"]})
    _game(tmp_path, "B", {"S2": []})
    assert _run(tmp_path, "--game", "B").exit_code == 0
    assert _run(tmp_path, "--game", "A").exit_code == 1


def test_help_documents_marker():
    r = CliRunner().invoke(stage_proves_cmd, ["--help"])
    assert "**Testbed:** required" in r.output


def test_directory_named_md_ignored(tmp_path):
    (tmp_path / "docs" / "specs" / "weird.md").mkdir(parents=True)
    _game(tmp_path, "A", {"S1": []})
    assert _run(tmp_path).exit_code == 0
