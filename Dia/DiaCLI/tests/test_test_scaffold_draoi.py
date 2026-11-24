from click.testing import CliRunner

from dia_cli.cli_main import cli

_HEADER = """#pragma once
namespace Draoi {
namespace HexWorld {
class WidgetThing {
public:
    int Count() const;
    void Reset();
};
}
}
"""


def _repo(tmp_path, monkeypatch):
    (tmp_path / "Cluiche").mkdir()
    (tmp_path / "Cluiche" / "Cluiche.sln").write_text("", encoding="utf-8")
    monkeypatch.setattr("dia_cli.commands.docs.test_scaffold_cmd.find_repo_root", lambda _f: tmp_path)


def test_draoi_header_scaffolds_with_draoi_namespace_and_path(tmp_path, monkeypatch):
    _repo(tmp_path, monkeypatch)
    h = tmp_path / "Draoi" / "DraoiHexWorld" / "WidgetThing.h"
    h.parent.mkdir(parents=True)
    h.write_text(_HEADER, encoding="utf-8")
    r = CliRunner().invoke(cli, ["docs", "test-scaffold", str(h)])
    assert r.exit_code == 0, r.output
    out = tmp_path / "Cluiche" / "Tests" / "GoogleTests" / "Draoi" / "TestWidgetThing.cpp"
    text = out.read_text(encoding="utf-8")
    assert "#include <DraoiHexWorld/WidgetThing.h>" in text
    assert "Draoi::HexWorld" in text


def test_dia_header_unchanged(tmp_path, monkeypatch):
    _repo(tmp_path, monkeypatch)
    h = tmp_path / "Dia" / "DiaFoo" / "Foo.h"
    h.parent.mkdir(parents=True)
    h.write_text("namespace Dia { class Foo { public: void Go(); }; }\n", encoding="utf-8")
    r = CliRunner().invoke(cli, ["docs", "test-scaffold", str(h), "--dry-run"])
    assert r.exit_code == 0, r.output
    assert "#include <DiaFoo/Foo.h>" in r.output


def test_header_outside_library_roots_is_a_clear_error(tmp_path, monkeypatch):
    _repo(tmp_path, monkeypatch)
    h = tmp_path / "Cluiche" / "CoW" / "X.h"
    h.parent.mkdir(parents=True)
    h.write_text("class X {};\n", encoding="utf-8")
    r = CliRunner().invoke(cli, ["docs", "test-scaffold", str(h), "--dry-run"])
    assert r.exit_code != 0
    assert "library root" in r.output
