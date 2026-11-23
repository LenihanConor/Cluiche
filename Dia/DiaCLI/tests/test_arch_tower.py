import json
import re
from pathlib import Path

from dia_cli.commands.check.arch_checker import run_arch_check
from dia_cli.commands.check.arch_tower import build_tower_data, render_tower_html, write_tower_html


def _module(repo: Path, root: str, name: str, module_id: str, layer: str, source: str = "") -> None:
    d = repo / root / name
    d.mkdir(parents=True, exist_ok=True)
    (d / f"dia.{module_id.split('.', 1)[1]}.architecture.module.md").write_text(
        f"---\nschema: dia.module.v1\nmodule_id: {module_id}\nname: {name}\n"
        f"path: {root}/{name}\nlayer: {layer}\n---\n",
        encoding="utf-8",
    )
    if source:
        (d / "A.cpp").write_text(source, encoding="utf-8")


def _repo_with_upward_include(tmp_path: Path) -> Path:
    _module(tmp_path, "Dia", "DiaLow", "dia.low", "foundation/core", source="#include <DiaHigh/X.h>\n")
    _module(tmp_path, "Dia", "DiaHigh", "dia.high", "domain/visual/core")
    return tmp_path


def test_data_contains_layers_edges_and_violations(tmp_path):
    repo = _repo_with_upward_include(tmp_path)
    violations, _, _ = run_arch_check(repo)
    data = build_tower_data(repo, violations)
    assert data["layers"] == {"dia.low": "foundation/core", "dia.high": "domain/visual/core"}
    assert [(e["f"], e["t"], e["n"]) for e in data["edges"]] == [("dia.low", "dia.high", 1)]
    assert [(v["f"], v["t"], v["line"]) for v in data["violations"]] == [("dia.low", "dia.high", 1)]
    assert data["other"] == []


def test_non_layer_violations_are_listed_not_drawn(tmp_path):
    _module(tmp_path, "Draoi", "DraoiX", "draoi.x", "draoi")
    _module(tmp_path, "Dia", "DiaA", "dia.a", "foundation/core", source="#include <DraoiX/X.h>\n")
    violations, _, _ = run_arch_check(tmp_path)
    data = build_tower_data(tmp_path, violations)
    assert [o["kind"] for o in data["other"]] == ["boundary"]
    assert data["violations"] == []


def test_render_fills_placeholders_and_survives_script_tags(tmp_path):
    repo = _repo_with_upward_include(tmp_path)
    data = build_tower_data(repo, run_arch_check(repo)[0])
    data["other"].append({"f": "a", "t": "b", "kind": "x", "rule": "</script><b>", "file": "f", "line": 1, "inc": "i"})
    html = render_tower_html(data)
    assert "/*DATA*/" not in html and "/*GENERATED*/" not in html
    payload = re.search(r"const DATA=(\{.*?\});\nconst LANES", html, re.S).group(1)
    assert json.loads(payload)["other"][0]["rule"] == "</script><b>"


def test_write_creates_parent_dirs(tmp_path):
    repo = _repo_with_upward_include(tmp_path)
    out = write_tower_html(repo, run_arch_check(repo)[0], tmp_path / "out" / "check" / "tower.html")
    assert out.read_text(encoding="utf-8").startswith("<!DOCTYPE html>")
