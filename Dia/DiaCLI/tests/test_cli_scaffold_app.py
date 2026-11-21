"""Tests for dia scaffold app (AC4, AC5, AC6)."""
from __future__ import annotations

from pathlib import Path

import pytest
from click.testing import CliRunner

from dia_cli.commands.scaffold import app_cmd, sln_cmd
from dia_cli.commands.scaffold.app_templates import (
    TEMPLATES, baseline_references, render_context,
)

_SLN = """Microsoft Visual Studio Solution File, Format Version 12.00
Project("{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}") = "DiaCore", "..\\Dia\\DiaCore\\DiaCore.vcxproj", "{AAAAAAAA-0000-0000-0000-000000000001}"
EndProject
Global
\tGlobalSection(ProjectConfigurationPlatforms) = postSolution
\tEndGlobalSection
\tGlobalSection(NestedProjects) = preSolution
\tEndGlobalSection
EndGlobal
"""

_BASELINE = """<?xml version="1.0" encoding="utf-8"?>
<Project xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <PropertyGroup><ProjectGuid>{11111111-2222-3333-4444-555555555555}</ProjectGuid></PropertyGroup>
  <ItemGroup>
    <ProjectReference Include="..\\..\\Dia\\DiaCore\\DiaCore.vcxproj">
      <Project>{8d41cbe3-c493-428d-95f3-627e56500667}</Project>
    </ProjectReference>
  </ItemGroup>
</Project>
"""

_TOML = '[targets.cow]\nproject = "Cluiche/CoW/CoW.vcxproj"\n'


@pytest.fixture
def repo(tmp_path, monkeypatch):
    (tmp_path / "Cluiche" / "CluicheGameBaseline" / "Modules").mkdir(parents=True)
    (tmp_path / "Cluiche" / "CluicheGameBaseline" / "CluicheGameBaseline.vcxproj").write_text(
        _BASELINE, encoding="utf-8")
    (tmp_path / "Cluiche" / "Cluiche.sln").write_text(_SLN, encoding="utf-8")
    (tmp_path / "pipeline.toml").write_text(_TOML, encoding="utf-8")
    monkeypatch.setattr(app_cmd, "find_repo_root", lambda _a: tmp_path)
    monkeypatch.setattr(sln_cmd, "find_repo_root", lambda _a: tmp_path)
    return tmp_path


def _snapshot(root: Path) -> dict:
    return {p.relative_to(root).as_posix(): p.read_bytes() for p in root.rglob("*") if p.is_file()}


def _expected(name="ScratchApp", stage="Placeholder") -> set:
    ctx = render_context(name, baseline_references_stub(), stage=stage)
    return {out.format(**ctx) for _, out in TEMPLATES}


def baseline_references_stub():
    return []


def _run(*args):
    return CliRunner().invoke(app_cmd.app, list(args))


def test_creates_exactly_the_file_list(repo):
    before = _snapshot(repo)
    result = _run("ScratchApp")
    assert result.exit_code == 0, result.output
    after = _snapshot(repo)
    changed = {k for k in after if k not in before or before[k] != after[k]}
    assert changed == _expected() | {"Cluiche/Cluiche.sln", "pipeline.toml"}
    assert set(before) - set(after) == set()
    sln = after["Cluiche/Cluiche.sln"].decode()
    assert '"ScratchApp", "ScratchApp\\ScratchApp.vcxproj"' in sln
    assert sln_cmd._FOLDER_GUIDS["executables"] in sln
    toml = after["pipeline.toml"].decode()
    assert "[targets.scratchapp]" in toml and "[targets.cow]" in toml


def test_name_derivation_and_no_unrendered_templates(repo):
    assert _run("ScratchApp").exit_code == 0
    assert (repo / "Cluiche/Assets/ScratchApp/scratchapp.diagame").exists()
    assert (repo / "Cluiche/Assets/ScratchApp/scratchapp_main.diaapp").exists()
    for rel in _expected():
        text = (repo / rel).read_text(encoding="utf-8")
        assert "{{" not in text and "{%" not in text, rel
    assert "{{" not in (repo / "pipeline.toml").read_text(encoding="utf-8")


def test_stage_option(repo):
    result = _run("ScratchApp", "--stage", "FrontEnd")
    assert result.exit_code == 0, result.output
    assert (repo / "Cluiche/ScratchApp/Modules/FrontEndModule.h").exists()
    assert (repo / "Cluiche/Assets/ScratchApp/Stages/FrontEnd/front_end.diastage").exists()
    assert not (repo / "Cluiche/ScratchApp/Modules/PlaceholderModule.h").exists()


def test_dry_run_lists_files_and_changes_nothing(repo):
    before = _snapshot(repo)
    result = _run("ScratchApp", "--dry-run")
    assert result.exit_code == 0, result.output
    assert _snapshot(repo) == before
    for rel in _expected():
        assert rel in result.output


def test_second_run_skips_existing(repo):
    assert _run("ScratchApp").exit_code == 0
    once = _snapshot(repo)
    target = repo / "Cluiche/ScratchApp/Main.cpp"
    target.write_text("// edited\n", encoding="utf-8")
    once = _snapshot(repo)
    result = _run("ScratchApp")
    assert result.exit_code == 0, result.output
    assert _snapshot(repo) == once
    assert "skipped" in result.output.lower()
    sln = (repo / "Cluiche/Cluiche.sln").read_text(encoding="utf-8")
    assert sln.count('"ScratchApp"') == 1
    assert (repo / "pipeline.toml").read_text(encoding="utf-8").count("[targets.scratchapp]") == 1


def test_force_overwrites_files_but_not_sln_or_toml(repo):
    assert _run("ScratchApp").exit_code == 0
    target = repo / "Cluiche/ScratchApp/Main.cpp"
    target.write_text("// edited\n", encoding="utf-8")
    sln_before = (repo / "Cluiche/Cluiche.sln").read_bytes()
    toml_before = (repo / "pipeline.toml").read_bytes()
    result = _run("ScratchApp", "--force")
    assert result.exit_code == 0, result.output
    assert target.read_text(encoding="utf-8") != "// edited\n"
    assert "overwritten" in result.output.lower()
    assert (repo / "Cluiche/Cluiche.sln").read_bytes() == sln_before
    assert (repo / "pipeline.toml").read_bytes() == toml_before


def test_existing_toml_target_not_duplicated_on_first_run(repo):
    (repo / "pipeline.toml").write_text(_TOML + "\n[targets.scratchapp]\nproject = \"x\"\n", encoding="utf-8")
    before = (repo / "pipeline.toml").read_bytes()
    result = _run("ScratchApp")
    assert result.exit_code == 0, result.output
    assert (repo / "pipeline.toml").read_bytes() == before


@pytest.mark.parametrize("bad", ["1App", "My App", "My-App", "", "App!"])
def test_invalid_name_rejected(repo, bad):
    before = _snapshot(repo)
    result = _run(bad)
    assert result.exit_code != 0
    assert _snapshot(repo) == before


def test_boot_stage_rejected(repo):
    result = _run("ScratchApp", "--stage", "Boot")
    assert result.exit_code != 0
    assert "Boot" in result.output


def test_invalid_stage_rejected(repo):
    assert _run("ScratchApp", "--stage", "1bad").exit_code != 0


def test_stage_module_clash_with_baseline_rejected(repo):
    (repo / "Cluiche/CluicheGameBaseline/Modules/BootMenuModule.h").write_text("//", encoding="utf-8")
    before = _snapshot(repo)
    result = _run("ScratchApp", "--stage", "BootMenu")
    assert result.exit_code != 0
    assert "BootMenuModule" in result.output
    assert _snapshot(repo) == before


def test_stage_module_clash_with_dia_module_rejected(repo):
    d = repo / "Dia" / "DiaFoo" / "Bar"
    d.mkdir(parents=True)
    (d / "ClashModule.h").write_text("//", encoding="utf-8")
    result = _run("ScratchApp", "--stage", "Clash")
    assert result.exit_code != 0
    assert "ClashModule" in result.output


def test_help_registered_in_group():
    from dia_cli.commands.scaffold.group import scaffold_group
    result = CliRunner().invoke(scaffold_group, ["--help"])
    assert "app" in result.output
