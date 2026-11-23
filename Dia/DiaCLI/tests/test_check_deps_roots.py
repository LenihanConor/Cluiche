import json
from pathlib import Path

from click.testing import CliRunner

from dia_cli.cli.check import _collect_dependency_issues, _parse_frontmatter
from dia_cli.cli_main import cli


def _module(repo: Path, root: str, name: str, module_id: str, deps_block: str = "", source: str = "") -> None:
    d = repo / root / name
    d.mkdir(parents=True, exist_ok=True)
    (d / f"dia.{module_id.split('.', 1)[1]}.architecture.module.md").write_text(
        f"---\nschema: dia.module.v1\nmodule_id: {module_id}\nname: {name}\npath: {root}/{name}\n{deps_block}---\n",
        encoding="utf-8",
    )
    if source:
        (d / "A.cpp").write_text(source, encoding="utf-8")


def test_parser_reads_dependencies_required_and_optional():
    text = ("---\nmodule_id: m\npath: Draoi/M\ndependencies:\n  required:\n    - dia.core\n"
            "  optional:\n    - dia.maths\n  forbidden:\n    - dia.ui\n---\n")
    assert _parse_frontmatter(text)["dependent_modules"] == ["dia.core", "dia.maths"]


def test_top_level_module_is_checked(tmp_path):
    _module(tmp_path, "Dia", "DiaCore", "dia.core")
    _module(tmp_path, "Dia", "DiaA", "dia.a", source="#include <DiaCore/X.h>\n")
    modules, missing, stale = _collect_dependency_issues(tmp_path)
    assert missing == [("dia.a", "dia.core", "")]
    assert stale == []


def test_draoi_include_resolves_and_required_counts_as_declared(tmp_path):
    _module(tmp_path, "Dia", "DiaCore", "dia.core")
    _module(tmp_path, "Draoi", "DraoiX", "draoi.x", source="#include <DiaCore/X.h>\n")
    _module(tmp_path, "Draoi", "DraoiY", "draoi.y",
            deps_block="dependencies:\n  required:\n    - draoi.x\n    - dia.core\n",
            source="#include <DraoiX/F.h>\n#include <DiaCore/X.h>\n")
    _, missing, stale = _collect_dependency_issues(tmp_path)
    assert ("draoi.y", "draoi.x", "") not in missing
    assert not [s for s in stale if s[0] == "draoi.y"]
    assert ("draoi.x", "dia.core", "") in missing


def test_declared_parent_covers_submodule(tmp_path):
    _module(tmp_path, "Dia", "DiaCore", "dia.core")
    sub = tmp_path / "Dia" / "DiaCore" / "Crc"
    sub.mkdir()
    (sub / "dia.core.crc.architecture.module.md").write_text(
        "---\nmodule_id: dia.core.crc\npath: Dia/DiaCore/Crc\n---\n", encoding="utf-8")
    _module(tmp_path, "Draoi", "DraoiX", "draoi.x",
            deps_block="dependencies:\n  required:\n    - dia.core\n",
            source="#include <DiaCore/Crc/C.h>\n")
    _, missing, stale = _collect_dependency_issues(tmp_path)
    assert not [m for m in missing if m[0] == "draoi.x"]
    assert not [s for s in stale if s[0] == "draoi.x"]


def test_root_docs_skipped_and_child_sources_not_charged_to_parent(tmp_path):
    (tmp_path / "Draoi").mkdir()
    (tmp_path / "Draoi" / "dia.draoi.root.architecture.module.md").write_text(
        "---\nmodule_id: draoi.root\npath: Draoi\n---\n", encoding="utf-8")
    _module(tmp_path, "Dia", "DiaCore", "dia.core")
    _module(tmp_path, "Dia", "DiaP", "dia.p")
    child = tmp_path / "Dia" / "DiaP" / "Child"
    child.mkdir()
    (child / "dia.p.child.architecture.module.md").write_text(
        "---\nmodule_id: dia.p.child\npath: Dia/DiaP/Child\n---\n", encoding="utf-8")
    (child / "B.cpp").write_text("#include <DiaCore/X.h>\n", encoding="utf-8")
    modules, missing, _ = _collect_dependency_issues(tmp_path)
    assert "draoi.root" not in [m["module_id"] for m in modules]
    assert [m[0] for m in missing] == ["dia.p.child"]


def test_baseline_accept_then_clean(tmp_path, monkeypatch):
    _module(tmp_path, "Dia", "DiaCore", "dia.core")
    _module(tmp_path, "Dia", "DiaA", "dia.a", source="#include <DiaCore/X.h>\n")
    (tmp_path / "Cluiche").mkdir()
    (tmp_path / "Cluiche" / "Cluiche.sln").write_text("", encoding="utf-8")
    monkeypatch.setattr("dia_cli.cli.check.find_repo_root", lambda _f: tmp_path)
    runner = CliRunner()
    assert runner.invoke(cli, ["check", "deps"]).exit_code == 1
    accepted = runner.invoke(cli, ["check", "deps", "--accept-baseline"])
    assert accepted.exit_code == 0
    baseline = tmp_path / "docs" / "reference" / "registry" / "deps-baseline.json"
    assert json.loads(baseline.read_text())["issues"] == ["missing|dia.a|dia.core"]
    assert runner.invoke(cli, ["check", "deps"]).exit_code == 0
    # A new issue beyond the baseline fails again.
    _module(tmp_path, "Dia", "DiaB", "dia.b", source="#include <DiaCore/X.h>\n")
    assert runner.invoke(cli, ["check", "deps"]).exit_code == 1
