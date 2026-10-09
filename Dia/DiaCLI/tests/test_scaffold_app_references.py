"""baseline_references also picks up Dia libs the baseline includes but never references."""
from __future__ import annotations

from dia_cli.commands.scaffold.app_templates import baseline_references

NS = "http://schemas.microsoft.com/developer/msbuild/2003"


def _proj(guid, refs=""):
    return (f'<Project xmlns="{NS}"><PropertyGroup><ProjectGuid>{guid}</ProjectGuid></PropertyGroup>'
            f"<ItemGroup>{refs}</ItemGroup></Project>")


def test_includes_of_unreferenced_dia_lib_are_linked(tmp_path):
    base = tmp_path / "Cluiche" / "CluicheGameBaseline"
    base.mkdir(parents=True)
    ref = '<ProjectReference Include="..\..\Dia\DiaCore\DiaCore.vcxproj"><Project>{core}</Project></ProjectReference>'
    (base / "CluicheGameBaseline.vcxproj").write_text(_proj("{base}", ref), encoding="utf-8")
    (base / "Thing.cpp").write_text("#include <DiaBlackboard/X.h>\n#include <DiaCore/Y.h>\n#include <DiaHeaderOnly/Z.h>\n")
    for lib, g in (("DiaCore", "{core}"), ("DiaBlackboard", "{bb}")):
        d = tmp_path / "Dia" / lib
        d.mkdir(parents=True)
        (d / f"{lib}.vcxproj").write_text(_proj(g), encoding="utf-8")

    libs = [r["link_lib"] for r in baseline_references(tmp_path)]

    assert libs == ["CluicheGameBaseline", "DiaCore", "DiaBlackboard"]


def test_default_project_guid_matches_solution_guid():
    from dia_cli.commands.scaffold.app_templates import default_project_guid
    from dia_cli.commands.scaffold.sln_cmd import _make_project_guid

    assert default_project_guid("ScratchApp") == _make_project_guid("ScratchApp")
