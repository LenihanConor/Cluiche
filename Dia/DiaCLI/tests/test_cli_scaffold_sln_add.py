"""dia scaffold sln-add: project root option (default Dia) for projects outside Dia/."""
from __future__ import annotations

from pathlib import Path

import pytest
from click.testing import CliRunner

from dia_cli.commands.scaffold import sln_cmd

_SLN = """Microsoft Visual Studio Solution File, Format Version 12.00
Project("{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}") = "DiaCore", "..\Dia\DiaCore\DiaCore.vcxproj", "{AAAAAAAA-0000-0000-0000-000000000001}"
EndProject
Global
\tGlobalSection(ProjectConfigurationPlatforms) = postSolution
\tEndGlobalSection
\tGlobalSection(NestedProjects) = preSolution
\tEndGlobalSection
EndGlobal
"""


@pytest.fixture
def repo(tmp_path, monkeypatch):
    (tmp_path / "Cluiche").mkdir()
    (tmp_path / "Cluiche" / "Cluiche.sln").write_text(_SLN, encoding="utf-8")
    monkeypatch.setattr(sln_cmd, "find_repo_root", lambda _anchor: tmp_path)
    return tmp_path


def _make_vcxproj(repo: Path, root: str, name: str) -> None:
    d = repo / root / name
    d.mkdir(parents=True)
    (d / f"{name}.vcxproj").write_text("<Project/>", encoding="utf-8")


def _sln(repo: Path) -> str:
    return (repo / "Cluiche" / "Cluiche.sln").read_text(encoding="utf-8")


def test_default_root_is_dia(repo):
    _make_vcxproj(repo, "Dia", "DiaThing")
    result = CliRunner().invoke(sln_cmd.sln_add, ["DiaThing"])
    assert result.exit_code == 0, result.output
    assert '"DiaThing", "..\Dia\DiaThing\DiaThing.vcxproj"' in _sln(repo)


def test_root_cluiche_writes_sln_relative_path(repo):
    _make_vcxproj(repo, "Cluiche", "CoW")
    result = CliRunner().invoke(sln_cmd.sln_add, ["CoW", "--root", "Cluiche", "--folder", "executables"])
    assert result.exit_code == 0, result.output
    text = _sln(repo)
    assert '"CoW", "CoW\CoW.vcxproj"' in text
    assert sln_cmd._FOLDER_GUIDS["executables"] in text


def test_missing_vcxproj_under_root_errors(repo):
    _make_vcxproj(repo, "Dia", "CoW")  # exists, but under the wrong root
    result = CliRunner().invoke(sln_cmd.sln_add, ["CoW", "--root", "Cluiche"])
    assert result.exit_code != 0
    assert "Cluiche" in result.output and "CoW.vcxproj" in result.output


def test_dry_run_reports_root_path_and_changes_nothing(repo):
    before = _sln(repo)
    result = CliRunner().invoke(sln_cmd.sln_add, ["CoW", "--root", "Cluiche", "--dry-run"])
    assert result.exit_code == 0, result.output
    assert "CoW\CoW.vcxproj" in result.output
    assert _sln(repo) == before


def test_second_add_is_idempotent(repo):
    _make_vcxproj(repo, "Cluiche", "CoW")
    runner = CliRunner()
    runner.invoke(sln_cmd.sln_add, ["CoW", "--root", "Cluiche"])
    once = _sln(repo)
    result = runner.invoke(sln_cmd.sln_add, ["CoW", "--root", "Cluiche"])
    assert "already in solution" in result.output
    assert _sln(repo) == once
