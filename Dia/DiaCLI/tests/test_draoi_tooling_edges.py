"""Edge cases across the Draoi tooling (draoi-tooling plan task 14)."""
from pathlib import Path

from click.testing import CliRunner

from dia_cli.cli.check import _collect_dependency_issues
from dia_cli.cli_main import cli
from dia_cli.commands.check.arch_checker import run_arch_check


def _doc(repo: Path, rel_dir: str, module_id: str, path: str, layer: str = "draoi", extra: str = "") -> Path:
    d = repo / rel_dir
    d.mkdir(parents=True, exist_ok=True)
    f = d / f"dia.{module_id.split('.', 1)[1]}.architecture.module.md"
    path_line = f"path: {path}\n" if path else ""
    f.write_text(f"---\nmodule_id: {module_id}\nname: {module_id}\n{path_line}layer: {layer}\n{extra}---\n", encoding="utf-8")
    return f


def test_backslash_include_resolves_to_draoi_module(tmp_path):
    _doc(tmp_path, "Draoi/DraoiX", "draoi.x", "Draoi/DraoiX")
    _doc(tmp_path, "Dia/DiaA", "dia.a", "Dia/DiaA", layer="foundation/core")
    (tmp_path / "Dia/DiaA/A.cpp").write_text(r'#include "DraoiX\Foo.h"\n', encoding="utf-8")
    violations, _, _ = run_arch_check(tmp_path)
    assert [(v.to_module, v.kind) for v in violations] == [("draoi.x", "boundary")]


def test_nested_draoi_submodule_wins_longest_prefix(tmp_path):
    _doc(tmp_path, "Draoi/DraoiX", "draoi.x", "Draoi/DraoiX")
    _doc(tmp_path, "Draoi/DraoiX/Sub", "draoi.x.sub", "Draoi/DraoiX/Sub")
    _doc(tmp_path, "Dia/DiaA", "dia.a", "Dia/DiaA", layer="foundation/core")
    (tmp_path / "Dia/DiaA/A.cpp").write_text("#include <DraoiX/Sub/Foo.h>\n", encoding="utf-8")
    violations, _, _ = run_arch_check(tmp_path)
    assert [v.to_module for v in violations] == ["draoi.x.sub"]


def test_module_doc_without_path_does_not_crash_arch_or_deps(tmp_path):
    _doc(tmp_path, "Draoi/DraoiX", "draoi.x", "")
    _doc(tmp_path, "Dia/DiaA", "dia.a", "Dia/DiaA", layer="foundation/core")
    run_arch_check(tmp_path)
    modules, missing, stale = _collect_dependency_issues(tmp_path)
    assert [m["module_id"] for m in modules] == ["dia.a"]


def test_unknown_root_directory_is_not_scanned(tmp_path):
    _doc(tmp_path, "Elsewhere/EX", "dia.ex", "Elsewhere/EX", layer="foundation/core")
    assert _collect_dependency_issues(tmp_path) == (None, [], [])


def test_malformed_deps_baseline_is_ignored(tmp_path, monkeypatch):
    _doc(tmp_path, "Dia/DiaCore", "dia.core", "Dia/DiaCore", layer="foundation/core")
    _doc(tmp_path, "Dia/DiaA", "dia.a", "Dia/DiaA", layer="foundation/core")
    (tmp_path / "Dia/DiaA/A.cpp").write_text("#include <DiaCore/X.h>\n", encoding="utf-8")
    (tmp_path / "Cluiche").mkdir()
    (tmp_path / "Cluiche" / "Cluiche.sln").write_text("", encoding="utf-8")
    bl = tmp_path / "docs" / "reference" / "registry" / "deps-baseline.json"
    bl.parent.mkdir(parents=True)
    bl.write_text("{not json", encoding="utf-8")
    monkeypatch.setattr("dia_cli.cli.check.find_repo_root", lambda _f: tmp_path)
    assert CliRunner().invoke(cli, ["check", "deps"]).exit_code == 1


def test_baseline_reports_fixed_entries(tmp_path, monkeypatch):
    _doc(tmp_path, "Dia/DiaCore", "dia.core", "Dia/DiaCore", layer="foundation/core")
    _doc(tmp_path, "Dia/DiaA", "dia.a", "Dia/DiaA", layer="foundation/core")
    (tmp_path / "Cluiche").mkdir()
    (tmp_path / "Cluiche" / "Cluiche.sln").write_text("", encoding="utf-8")
    bl = tmp_path / "docs" / "reference" / "registry" / "deps-baseline.json"
    bl.parent.mkdir(parents=True)
    bl.write_text('{"issues": ["missing|dia.a|dia.core"]}', encoding="utf-8")
    monkeypatch.setattr("dia_cli.cli.check.find_repo_root", lambda _f: tmp_path)
    result = CliRunner().invoke(cli, ["check", "deps"])
    assert result.exit_code == 0
    assert "1 baseline entries now fixed" in result.output
