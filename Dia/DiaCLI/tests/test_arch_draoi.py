from pathlib import Path

from dia_cli.commands.check.arch_checker import run_arch_check
from dia_cli.commands.check.arch_layer_rules import check_layer_violation, parse_layer


def _module(repo: Path, root: str, name: str, module_id: str, layer: str, source: str = "") -> None:
    mod_dir = repo / root / name
    mod_dir.mkdir(parents=True, exist_ok=True)
    (mod_dir / f"dia.{module_id.split('.', 1)[1]}.architecture.module.md").write_text(
        f"---\nschema: dia.module.v1\nmodule_id: {module_id}\nname: {name}\n"
        f"path: {root}/{name}\nlayer: {layer}\n---\n",
        encoding="utf-8",
    )
    if source:
        (mod_dir / "A.cpp").write_text(source, encoding="utf-8")


def test_draoi_layer_parses_above_dia():
    assert parse_layer("draoi") == (5, 0, "draoi", "")


def test_dia_layer_may_not_depend_on_draoi_layer():
    assert check_layer_violation("foundation/core", "draoi") is not None
    assert check_layer_violation("draoi", "foundation/core") is None
    assert check_layer_violation("draoi", "draoi") is None


def test_dia_including_draoi_is_boundary_violation(tmp_path):
    _module(tmp_path, "Draoi", "DraoiX", "draoi.x", "draoi")
    _module(tmp_path, "Dia", "DiaA", "dia.a", "domain/gameplay/core", '#include <DraoiX/Foo.h>\n')
    violations, _, code = run_arch_check(tmp_path)
    assert code == 1
    assert [(v.from_module, v.to_module, v.kind) for v in violations] == [("dia.a", "draoi.x", "boundary")]


def test_boundary_catches_layers_the_parser_skips(tmp_path):
    _module(tmp_path, "Draoi", "DraoiX", "draoi.x", "draoi")
    _module(tmp_path, "Dia", "DiaEd", "dia.ed", "tools/editor", '#include <DraoiX/Foo.h>\n')
    violations, _, _ = run_arch_check(tmp_path)
    assert [v.kind for v in violations] == ["boundary"]


def test_draoi_including_dia_is_allowed(tmp_path):
    _module(tmp_path, "Dia", "DiaCore", "dia.core", "foundation/core")
    _module(tmp_path, "Draoi", "DraoiX", "draoi.x", "draoi", '#include <DiaCore/Foo.h>\n')
    violations, _, code = run_arch_check(tmp_path)
    assert violations == [] and code == 0


def test_draoi_include_resolves_to_draoi_module(tmp_path):
    _module(tmp_path, "Draoi", "DraoiX", "draoi.x", "draoi")
    _module(tmp_path, "Draoi", "DraoiY", "draoi.y", "draoi", '#include <DraoiX/Foo.h>\n')
    # Draoi -> Draoi is allowed; make it forbidden to prove the include resolved to draoi.x.
    doc = tmp_path / "Draoi" / "DraoiY" / "dia.y.architecture.module.md"
    doc.write_text(doc.read_text().replace("---\n", "---\n", 1).rstrip("-\n") +
                   "\ndependencies:\n  forbidden:\n    - draoi.x\n---\n", encoding="utf-8")
    violations, _, _ = run_arch_check(tmp_path)
    assert [(v.to_module, v.kind) for v in violations] == [("draoi.x", "forbidden")]
