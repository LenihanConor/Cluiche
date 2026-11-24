from pathlib import Path

from dia_cli.commands.docs.registry_cmd import generate_registry


def _doc(repo: Path, rel: str, module_id: str, parent: str = "", layer: str = "foundation/core") -> None:
    p = repo / rel
    p.parent.mkdir(parents=True, exist_ok=True)
    parent_line = f"parent_module_id: {parent}\n" if parent else ""
    p.write_text(f"---\nmodule_id: {module_id}\nname: {module_id}\nlayer: {layer}\n{parent_line}path: {rel}\n---\n",
                 encoding="utf-8")


def test_each_module_listed_once_with_several_roots(tmp_path):
    _doc(tmp_path, "Dia/dia.root.architecture.module.md", "dia.root")
    _doc(tmp_path, "Dia/DiaA/dia.a.architecture.module.md", "dia.a", parent="dia.root")
    _doc(tmp_path, "Draoi/dia.draoi.root.architecture.module.md", "draoi.root", layer="draoi")
    _doc(tmp_path, "Draoi/DraoiX/dia.draoi.x.architecture.module.md", "draoi.x", parent="draoi.root", layer="draoi")
    _doc(tmp_path, "Dia/DiaEd/dia.ed.architecture.module.md", "dia.ed", layer="tools/editor")  # parentless
    text = generate_registry(tmp_path)
    hierarchy = text.split("## Module Hierarchy", 1)[1]
    for mid in ("dia.root", "dia.a", "draoi.root", "draoi.x", "dia.ed"):
        assert hierarchy.count(f"**{mid}**") == 1, mid


def test_draoi_nests_under_draoi_root_and_layer_summary_lists_draoi(tmp_path):
    _doc(tmp_path, "Draoi/dia.draoi.root.architecture.module.md", "draoi.root", layer="draoi")
    _doc(tmp_path, "Draoi/DraoiX/dia.draoi.x.architecture.module.md", "draoi.x", parent="draoi.root", layer="draoi")
    text = generate_registry(tmp_path)
    assert "| draoi | 2 |" in text
    hierarchy = text.split("## Module Hierarchy", 1)[1].splitlines()
    root_i = next(i for i, l in enumerate(hierarchy) if "**draoi.root**" in l)
    child_line = next(l for l in hierarchy if "**draoi.x**" in l)
    assert len(child_line) - len(child_line.lstrip()) > len(hierarchy[root_i]) - len(hierarchy[root_i].lstrip())


def test_orphan_module_still_rendered(tmp_path):
    _doc(tmp_path, "Dia/dia.root.architecture.module.md", "dia.root")
    _doc(tmp_path, "Dia/DiaO/dia.o.architecture.module.md", "dia.o", parent="dia.missing")
    text = generate_registry(tmp_path)
    assert text.count("**dia.o**") == 1
    assert "dia.missing (parent not found)" in text
