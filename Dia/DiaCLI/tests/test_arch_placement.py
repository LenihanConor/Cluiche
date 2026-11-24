from pathlib import Path

from dia_cli.commands.check.arch_checker import format_violations, run_arch_check
from dia_cli.commands.check.arch_placement import find_wiring, is_exempt
from dia_cli.commands.check.arch_tower import build_tower_data


def _module(repo: Path, root: str, name: str, module_id: str, layer: str, files: dict | None = None) -> None:
    d = repo / root / name
    d.mkdir(parents=True, exist_ok=True)
    (d / f"dia.{module_id.split('.', 1)[1]}.architecture.module.md").write_text(
        f"---\nschema: dia.module.v1\nmodule_id: {module_id}\nname: {name}\n"
        f"path: {root}/{name}\nlayer: {layer}\n---\n",
        encoding="utf-8",
    )
    for rel, text in (files or {}).items():
        target = d / rel
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(text, encoding="utf-8")


def _placement(violations):
    return [v for v in violations if v.kind == "placement"]


# ---- find_wiring -----------------------------------------------------------

def test_module_subclass_is_found_with_class_name_and_line():
    hits = find_wiring("#pragma once\nclass FooModule : public Dia::ApplicationFlow::SimModule\n{\n};\n")
    assert hits == [(2, "class FooModule derives from SimModule")]


def test_every_module_base_is_recognised():
    for base in ("SimModule", "RenderModule", "MainModule", "Module"):
        assert find_wiring(f"class X : public {base} {{}};")[0][1] == f"class X derives from {base}"


def test_module_base_among_several_bases():
    hits = find_wiring("class X final : private IStepSummarySink, public Dia::ApplicationFlow::Module {};")
    assert [h[1] for h in hits] == ["class X derives from Module"]


def test_unrelated_base_is_not_flagged():
    assert find_wiring("class X : public Dia::Core::Singleton<X> {};") == []
    assert find_wiring("class MyModuleHelper : public Helper {};") == []


def test_dia_module_macro_and_affinity_and_pu_include_are_found_once_each():
    text = (
        '#include <DiaApplicationFlow/ProcessingUnit.h>\n'
        "static constexpr PUAffinity kAllowedPUs = PUAffinity::kSim;\n"
        "PUAffinity a; PUAffinity b;\n"
        "DIA_MODULE(Foo_);\nDIA_MODULE(Bar_);\n"
    )
    assert [h[1] for h in find_wiring(text)] == [
        "DIA_MODULE registration", "processing-unit affinity", "reaches into a ProcessingUnit",
    ]


def test_comments_do_not_count_but_line_numbers_survive():
    text = (
        "// DIA_MODULE(X) is not used here\n"
        "/* class Y : public SimModule {\n"
        "   PUAffinity */\n"
        "class Z : public SimModule {};\n"
    )
    assert find_wiring(text) == [(4, "class Z derives from SimModule")]


# ---- exemptions ------------------------------------------------------------

def test_framework_and_testing_are_exempt_but_sibling_packages_are_not():
    assert is_exempt("Dia/DiaApplicationFlow", Path("Dia/DiaApplicationFlow/Module.h"))
    assert is_exempt("Dia/DiaApplicationFlow/Metrics", Path("Dia/DiaApplicationFlow/Metrics/X.h"))
    assert is_exempt("Dia/DiaFoo", Path("Dia/DiaFoo/Testing/FakeModule.h"))
    assert not is_exempt("Dia/DiaApplicationFlowEditor", Path("Dia/DiaApplicationFlowEditor/X.h"))
    assert not is_exempt("Dia/DiaFoo", Path("Dia/DiaFoo/FooModule.h"))


# ---- run_arch_check --------------------------------------------------------

def test_library_package_with_a_module_class_is_a_placement_violation(tmp_path):
    _module(tmp_path, "Dia", "DiaFoo", "dia.foo", "foundation/services",
            {"FooModule.h": "class FooModule : public Dia::ApplicationFlow::SimModule {};\n"})
    violations, _, code = run_arch_check(tmp_path)
    found = _placement(violations)
    assert code == 1
    assert [(v.from_module, v.source_file, v.lineno, v.include_path) for v in found] == [
        ("dia.foo", "Dia/DiaFoo/FooModule.h", 1, "class FooModule derives from SimModule")]


def test_inl_files_are_scanned(tmp_path):
    _module(tmp_path, "Dia", "DiaFoo", "dia.foo", "foundation/services",
            {"A.inl": "class A : public SimModule {};\n"})
    assert len(_placement(run_arch_check(tmp_path)[0])) == 1


def test_draoi_packages_are_checked_too(tmp_path):
    _module(tmp_path, "Draoi", "DraoiX", "draoi.x", "draoi",
            {"X.h": "class X : public SimModule {};\n"})
    assert [v.from_module for v in _placement(run_arch_check(tmp_path)[0])] == ["draoi.x"]


def test_framework_package_and_testing_helpers_pass(tmp_path):
    _module(tmp_path, "Dia", "DiaApplicationFlow", "dia.application", "foundation/application",
            {"SimModule.h": "class SimModule : public Module {};\n"})
    _module(tmp_path, "Dia", "DiaFoo", "dia.foo", "foundation/services",
            {"Testing/FakeModule.h": "class FakeModule : public SimModule {};\n"})
    assert _placement(run_arch_check(tmp_path)[0]) == []


def test_applications_are_not_checked(tmp_path):
    _module(tmp_path, "Dia", "DiaFoo", "dia.foo", "foundation/services", {"A.h": "struct A {};\n"})
    app = tmp_path / "Cluiche" / "CoW"
    app.mkdir(parents=True)
    (app / "M.h").write_text("class M : public SimModule {};\n", encoding="utf-8")
    assert _placement(run_arch_check(tmp_path)[0]) == []


def test_report_names_the_package_and_the_wiring(tmp_path):
    _module(tmp_path, "Dia", "DiaFoo", "dia.foo", "foundation/services",
            {"FooModule.h": "class FooModule : public SimModule {};\n"})
    violations, warnings, _ = run_arch_check(tmp_path)
    text = format_violations(violations, warnings)
    assert "ARCH VIOLATION: dia.foo contains application wiring: class FooModule derives from SimModule" in text
    assert "File: Dia/DiaFoo/FooModule.h:1" in text and "PD-012" in text


# ---- Tower -----------------------------------------------------------------

def test_tower_groups_placement_per_package(tmp_path):
    _module(tmp_path, "Dia", "DiaFoo", "dia.foo", "foundation/services", {
        "FooModule.h": "class FooModule : public SimModule {};\n",
        "FooModule.cpp": "DIA_MODULE(Foo_);\n",
    })
    data = build_tower_data(tmp_path, run_arch_check(tmp_path)[0])
    cards = [o for o in data["other"] if o["kind"] == "placement"]
    assert len(cards) == 1 and cards[0]["f"] == "dia.foo"
    assert "2 place(s)" in cards[0]["rule"]
    assert "class FooModule derives from SimModule" in cards[0]["rule"] and "DIA_MODULE registration" in cards[0]["rule"]
