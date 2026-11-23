from pathlib import Path

from dia_cli.utils.library_roots import (
    LIBRARY_ROOTS,
    iter_module_docs,
    root_of,
    strip_root,
)


def _doc(path: Path) -> Path:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("---\nmodule_id: x\n---\n", encoding="utf-8")
    return path


def test_roots_are_dia_and_draoi():
    assert LIBRARY_ROOTS == ("Dia", "Draoi")


def test_finds_docs_under_both_roots(tmp_path):
    a = _doc(tmp_path / "Dia" / "DiaCore" / "Docs" / "dia.core.architecture.module.md")
    b = _doc(tmp_path / "Draoi" / "DraoiHexWorld" / "dia.draoi.hexworld.architecture.module.md")
    assert set(iter_module_docs(tmp_path)) == {a, b}


def test_excludes_worktrees_and_node_modules(tmp_path):
    _doc(tmp_path / "Dia" / ".claude" / "worktrees" / "w" / "dia.a.architecture.module.md")
    _doc(tmp_path / "Dia" / "X" / "node_modules" / "dia.b.architecture.module.md")
    _doc(tmp_path / "Dia" / "out" / "dia.c.architecture.module.md")
    keep = _doc(tmp_path / "Dia" / "X" / "dia.d.architecture.module.md")
    assert list(iter_module_docs(tmp_path)) == [keep]


def test_missing_root_is_ignored(tmp_path):
    keep = _doc(tmp_path / "Dia" / "X" / "dia.d.architecture.module.md")
    assert list(iter_module_docs(tmp_path)) == [keep]


def test_root_of():
    assert root_of("Draoi/DraoiHexWorld/Foo") == "Draoi"
    assert root_of("Dia/DiaCore") == "Dia"
    assert root_of("Dia\DiaCore") == "Dia"
    assert root_of("Cluiche/CoW") is None
    assert root_of("") is None


def test_strip_root():
    assert strip_root("Draoi/DraoiHexWorld") == "DraoiHexWorld"
    assert strip_root("Dia/DiaCore/Containers/") == "DiaCore/Containers"
    assert strip_root("Cluiche/CoW") == "Cluiche/CoW"
    assert strip_root("Dia") == ""
