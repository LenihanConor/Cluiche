from dia_cli.commands.check.sln_sync import run_sln_sync

_CPP = "{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}"
_FOLDER = "{2150E333-8FDC-42A3-9474-1A3956D46DE8}"
_DIA, _DRAOI, _P_DRAOI, _P_DIA = "{AAAA0000-0000-0000-0000-000000000001}", "{AAAA0000-0000-0000-0000-000000000002}", "{BBBB0000-0000-0000-0000-000000000001}", "{BBBB0000-0000-0000-0000-000000000002}"
_CORE = "{AAAA0000-0000-0000-0000-000000000003}"

_SLN = f"""
Microsoft Visual Studio Solution File, Format Version 12.00
Project("{_FOLDER}") = "Dia", "Dia", "{_DIA}"
EndProject
Project("{_FOLDER}") = "Draoi", "Draoi", "{_DRAOI}"
EndProject
Project("{_FOLDER}") = "1.0-Core", "1.0-Core", "{_CORE}"
EndProject
Project("{_CPP}") = "DraoiX", "..\Draoi\DraoiX\DraoiX.vcxproj", "{_P_DRAOI}"
EndProject
Project("{_CPP}") = "DiaCore", "..\Dia\DiaCore\DiaCore.vcxproj", "{_P_DIA}"
EndProject
Global
\tGlobalSection(NestedProjects) = preSolution
\t\t{_CORE} = {_DIA}
\tEndGlobalSection
EndGlobal
"""


def _doc(repo, root, name, module_id, layer):
    d = repo / root / name
    d.mkdir(parents=True)
    (d / f"dia.{module_id.split('.', 1)[1]}.architecture.module.md").write_text(
        f"---\nmodule_id: {module_id}\npath: {root}/{name}\nlayer: {layer}\n---\n", encoding="utf-8")


def _repo(tmp_path):
    (tmp_path / "Cluiche").mkdir()
    (tmp_path / "Cluiche" / "Cluiche.sln").write_text(_SLN, encoding="utf-8")
    (tmp_path / "docs/specs/applications/dia/systems/diaarchitecture").mkdir(parents=True)
    _doc(tmp_path, "Draoi", "DraoiX", "draoi.x", "draoi")
    _doc(tmp_path, "Dia", "DiaCore", "dia.core", "foundation/core")
    return tmp_path


def test_draoi_project_goes_to_top_level_draoi_folder(tmp_path):
    repo = _repo(tmp_path)
    changes, _, code = run_sln_sync(repo, dry_run=True)
    assert code == 0
    assert "MOVE   DraoiX: <none> → Draoi" in changes
    assert not any(c.startswith("NEST   Draoi") for c in changes)  # not nested under Dia
    assert not any(c.startswith("CREATE folder: Draoi") for c in changes)  # reuses existing folder


def test_apply_nests_draoi_project_under_existing_folder_guid(tmp_path):
    repo = _repo(tmp_path)
    run_sln_sync(repo, dry_run=False)
    text = (repo / "Cluiche" / "Cluiche.sln").read_text(encoding="utf-8")
    assert f"{_P_DRAOI} = {_DRAOI}" in text
    assert f"{_DRAOI} = {_DIA}" not in text
