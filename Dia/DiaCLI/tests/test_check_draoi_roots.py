from pathlib import Path
from unittest.mock import MagicMock

from click.testing import CliRunner

from dia_cli.cli_main import cli
from dia_cli.commands.check.clone_checker import _collect_source_files


def test_clone_checker_collects_draoi_sources(tmp_path):
    for rel in ("Dia/DiaA/A.cpp", "Draoi/DraoiX/X.h", "Cluiche/CoW/C.cpp", "Draoi/DraoiX/out/Skip.cpp"):
        f = tmp_path / rel
        f.parent.mkdir(parents=True, exist_ok=True)
        f.write_text("", encoding="utf-8")
    names = sorted(p.relative_to(tmp_path).as_posix() for p in _collect_source_files(tmp_path))
    assert names == ["Cluiche/CoW/C.cpp", "Dia/DiaA/A.cpp", "Draoi/DraoiX/X.h"]


def test_cppcheck_scans_draoi(tmp_path, monkeypatch):
    (tmp_path / "Cluiche").mkdir()
    (tmp_path / "Cluiche" / "Cluiche.sln").write_text("", encoding="utf-8")
    monkeypatch.setattr("dia_cli.cli.check.find_repo_root", lambda _f: tmp_path)
    monkeypatch.setattr("dia_cli.cli.check.shutil.which", lambda _n: "cppcheck")
    run = MagicMock(return_value=MagicMock(returncode=0))
    monkeypatch.setattr("dia_cli.cli.check.subprocess.run", run)
    CliRunner().invoke(cli, ["check", "cppcheck"])
    cmd = run.call_args[0][0]
    assert str(tmp_path / "Draoi") in cmd
    assert str(tmp_path / "Dia") in cmd and str(tmp_path / "Cluiche") in cmd
