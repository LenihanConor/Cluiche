"""Module Tower: a self-contained HTML view of `dia check arch` results.

Floors are the layer rules, boxes are modules, pipes are includes. Dependencies may
only point down, so red pipes are violations. Modules can be dragged to another
floor in the page to see which violations a layer change would fix or cause.
"""
from __future__ import annotations

import datetime
import json
from pathlib import Path

from dia_cli.commands.check.arch_checker import ArchViolation
from dia_cli.commands.check.arch_include_parser import resolve_includes
from dia_cli.commands.check.arch_layer_rules import parse_layer
from dia_cli.commands.check.arch_module_map import build_module_map

_TEMPLATE = Path(__file__).with_name("arch_tower_template.html")


def build_tower_data(repo_root: Path, violations: list[ArchViolation]) -> dict:
    """Module layers, module-to-module include edges and the violations to draw."""
    module_map, _ = build_module_map(repo_root)
    module_dirs = {repo_root / m.path for m in module_map.values() if m.path}

    edges: dict[tuple[str, str], dict] = {}
    for module_id, info in sorted(module_map.items()):
        if not info.path:
            continue
        module_dir = repo_root / info.path
        if not module_dir.is_dir():
            continue
        child_dirs = {d for d in module_dirs if d != module_dir and module_dir in d.parents}
        files = [f for ext in ("*.h", "*.cpp", "*.inl") for f in module_dir.rglob(ext)]
        for src in sorted(files):
            if any(cd in src.parents for cd in child_dirs):
                continue
            for dep_id, _lineno, raw in resolve_includes(src, repo_root, module_map, info):
                if dep_id == module_id:
                    continue
                edge = edges.setdefault((module_id, dep_id), {
                    "f": module_id, "t": dep_id, "n": 0,
                    "file": src.relative_to(repo_root).as_posix(), "inc": raw,
                })
                edge["n"] += 1

    # Only modules with a recognised layer can be placed on a floor.
    layers = {mid: m.layer for mid, m in module_map.items() if m.layer and parse_layer(m.layer)}
    edge_list = [e for e in edges.values() if e["f"] in layers and e["t"] in layers]
    used = {e["f"] for e in edge_list} | {e["t"] for e in edge_list}

    def row(v: ArchViolation) -> dict:
        return {"f": v.from_module, "t": v.to_module, "file": v.source_file,
                "line": v.lineno, "inc": v.include_path}

    layer_violations = [v for v in violations if v.kind == "layer"]
    other = [{**row(v), "kind": v.kind, "rule": v.rule} for v in violations if v.kind != "layer"]
    # Violations whose modules lack a placeable layer can only be listed, not drawn.
    drawable = [v for v in layer_violations if v.from_module in layers and v.to_module in layers]
    other += [{**row(v), "kind": v.kind, "rule": v.rule}
              for v in layer_violations if v not in drawable]

    return {
        "docs": {mid: module_map[mid].doc_path for mid in layers if module_map[mid].doc_path},
        "layers": {k: v for k, v in layers.items() if k in used or k in {x.from_module for x in drawable}},
        "edges": edge_list,
        "violations": [row(v) for v in drawable],
        "other": other,
    }


def render_tower_html(data: dict) -> str:
    template = _TEMPLATE.read_text(encoding="utf-8")
    # "</" inside inline JSON would end the script tag early.
    payload = json.dumps(data, separators=(",", ":")).replace("</", "<" + chr(92) + "/")
    stamp = datetime.date.today().isoformat()
    return template.replace("/*DATA*/", payload).replace("/*GENERATED*/", stamp)


def write_tower_html(repo_root: Path, violations: list[ArchViolation], out_path: Path) -> Path:
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(render_tower_html(build_tower_data(repo_root, violations)), encoding="utf-8")
    return out_path
