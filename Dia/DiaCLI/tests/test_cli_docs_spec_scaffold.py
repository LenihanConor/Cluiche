"""Tests for ``dia docs spec-scaffold`` parent inference, --app, and output paths."""
from __future__ import annotations

from pathlib import Path

import pytest
from click.testing import CliRunner

from dia_cli.commands.docs import spec_scaffold_cmd
from dia_cli.commands.docs.spec_scaffold_cmd import spec_scaffold


@pytest.fixture()
def repo(tmp_path, monkeypatch):
    app_dir = tmp_path / "docs" / "specs" / "applications" / "cow"
    (app_dir / "systems" / "f1-harness").mkdir(parents=True)
    (app_dir / "cow.md").write_text("# App\n", encoding="utf-8")
    (app_dir / "systems" / "f1-harness" / "f1-harness.md").write_text("# Sys\n", encoding="utf-8")
    monkeypatch.setattr(spec_scaffold_cmd, "find_repo_root", lambda _f: tmp_path)
    return tmp_path


def test_feature_lands_beside_parent_system(repo):
    result = CliRunner().invoke(spec_scaffold, ["feature", "bus-recorder", "--app", "cow", "--parent", "f1-harness"])
    assert result.exit_code == 0, result.output
    out = repo / "docs/specs/applications/cow/systems/f1-harness/bus-recorder.md"
    assert "**Parent:** @docs/specs/applications/cow/systems/f1-harness/f1-harness.md" in out.read_text(encoding="utf-8")


def test_system_parent_is_the_app_spec(repo):
    result = CliRunner().invoke(spec_scaffold, ["system", "S1-World", "--app", "cow"])
    assert result.exit_code == 0, result.output
    out = repo / "docs/specs/applications/cow/systems/s1-world/s1-world.md"
    assert "**Parent:** @docs/specs/applications/cow/cow.md" in out.read_text(encoding="utf-8")


def test_relative_output_dir(repo, monkeypatch):
    monkeypatch.chdir(repo)
    result = CliRunner().invoke(
        spec_scaffold, ["feature", "x", "--app", "cow", "--parent", "f1-harness", "--output-dir", "out"])
    assert result.exit_code == 0, result.output
    assert (repo / "out" / "x.md").is_file()


def test_unknown_parent_fails(repo):
    result = CliRunner().invoke(spec_scaffold, ["feature", "x", "--app", "cow", "--parent", "nope"])
    assert result.exit_code != 0
    assert "not found" in result.output


def test_feature_requires_parent(repo):
    result = CliRunner().invoke(spec_scaffold, ["feature", "x", "--app", "cow"])
    assert result.exit_code != 0
