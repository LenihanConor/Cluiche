#!/usr/bin/env python
"""Dia module graph utilities.

Reads `Dia/**/dia.*.architecture.module.md` files and provides:
- Listing and inspection of modules
- Validation of references
- Graph output (Mermaid, DOT, JSON)

This parser intentionally supports only the YAML subset used by the module docs.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import sys
from dataclasses import dataclass, asdict
from pathlib import Path
from typing import Dict, Iterable, List, Optional, Set, Tuple


FRONTMATTER_RE = re.compile(r"(?s)^---\s*\r?\n(.*?)\r?\n---\s*(?:\r?\n|$)")


@dataclass(frozen=True)
class ModuleDoc:
    module_id: str
    name: str
    path: str
    file: str
    parent_module_id: Optional[str]
    dependent_modules: List[str]
    public_headers: List[str]
    public_namespaces: List[str]
    public_entry_points: List[str]
    dependencies_required: List[str]


def _extract_frontmatter(text: str) -> str:
    m = FRONTMATTER_RE.match(text)
    if not m:
        return ""
    return m.group(1)


def _parse_scalar(line: str) -> Optional[Tuple[str, str]]:
    m = re.match(r"^([a-zA-Z0-9_]+):\s*(.*)$", line)
    if not m:
        return None
    return m.group(1), m.group(2).strip()


def _parse_list_block(lines: List[str], start_idx: int, item_indent: str) -> Tuple[List[str], int]:
    out: List[str] = []
    i = start_idx
    prefix = item_indent + "- "
    while i < len(lines):
        line = lines[i]
        if line.startswith(prefix):
            out.append(line[len(prefix) :].strip())
            i += 1
            continue
        # stop when leaving the list indentation
        if line.strip() == "":
            i += 1
            continue
        if not line.startswith(item_indent):
            break
        # Non-list line at same indentation => stop.
        break
    return out, i


def _parse_block_scalar(lines: List[str], start_idx: int, indent: str) -> Tuple[str, int]:
    # YAML '>' block: consume subsequent lines that are indented more than the key.
    out_lines: List[str] = []
    i = start_idx
    while i < len(lines):
        line = lines[i]
        if line.startswith(indent):
            out_lines.append(line[len(indent) :].rstrip())
            i += 1
            continue
        if line.strip() == "":
            i += 1
            continue
        break
    # Fold newlines into spaces.
    text = " ".join([s.strip() for s in out_lines if s.strip()])
    return text, i


def parse_module_doc(file_path: Path) -> Optional[ModuleDoc]:
    raw = file_path.read_text(encoding="utf-8", errors="replace")
    fm = _extract_frontmatter(raw)
    if not fm:
        return None

    lines = fm.splitlines()
    module_id = ""
    name = ""
    path = ""
    parent: Optional[str] = None
    dependent_modules: List[str] = []
    public_headers: List[str] = []
    public_namespaces: List[str] = []
    public_entry_points: List[str] = []
    deps_required: List[str] = []

    i = 0
    while i < len(lines):
        line = lines[i]
        if not line.strip() or line.strip().startswith("#"):
            i += 1
            continue

        kv = _parse_scalar(line)
        if not kv:
            i += 1
            continue
        key, value = kv

        if key == "module_id":
            module_id = value
            i += 1
            continue
        if key == "name":
            name = value
            i += 1
            continue
        if key == "path":
            path = value
            i += 1
            continue
        if key == "parent_module_id":
            parent = value
            i += 1
            continue

        if key == "dependent_modules":
            if value == "[]":
                dependent_modules = []
                i += 1
                continue
            items, nxt = _parse_list_block(lines, i + 1, "  ")
            dependent_modules = items
            i = nxt
            continue

        if key == "public_api":
            # parse nested block until next top-level key
            i += 1
            while i < len(lines):
                subline = lines[i]
                if not subline.strip():
                    i += 1
                    continue
                if re.match(r"^[a-zA-Z0-9_]+:", subline):
                    break
                m = re.match(r"^\s{2}([a-zA-Z0-9_]+):\s*(.*)$", subline)
                if not m:
                    i += 1
                    continue
                subkey, subval = m.group(1), m.group(2).strip()

                if subkey == "headers":
                    if subval == "[]":
                        public_headers = []
                        i += 1
                        continue
                    items, nxt = _parse_list_block(lines, i + 1, "    ")
                    public_headers = items
                    i = nxt
                    continue

                if subkey == "namespaces":
                    if subval == "[]":
                        public_namespaces = []
                        i += 1
                        continue
                    items, nxt = _parse_list_block(lines, i + 1, "    ")
                    public_namespaces = items
                    i = nxt
                    continue

                if subkey == "entry_points":
                    if subval == "[]":
                        public_entry_points = []
                        i += 1
                        continue
                    items, nxt = _parse_list_block(lines, i + 1, "    ")
                    public_entry_points = items
                    i = nxt
                    continue

                i += 1
            continue

        if key == "dependencies":
            i += 1
            while i < len(lines):
                subline = lines[i]
                if not subline.strip():
                    i += 1
                    continue
                if re.match(r"^[a-zA-Z0-9_]+:", subline):
                    break
                m = re.match(r"^\s{2}([a-zA-Z0-9_]+):\s*(.*)$", subline)
                if not m:
                    i += 1
                    continue
                subkey, subval = m.group(1), m.group(2).strip()

                if subkey == "required":
                    if subval == "[]":
                        deps_required = []
                        i += 1
                        continue
                    items, nxt = _parse_list_block(lines, i + 1, "    ")
                    deps_required = items
                    i = nxt
                    continue

                i += 1
            continue

        i += 1

    if not module_id:
        return None

    return ModuleDoc(
        module_id=module_id,
        name=name or module_id,
        path=path,
        file=str(file_path),
        parent_module_id=parent,
        dependent_modules=dependent_modules,
        public_headers=public_headers,
        public_namespaces=public_namespaces,
        public_entry_points=public_entry_points,
        dependencies_required=deps_required,
    )


def load_all_modules(dia_dir: Path) -> Dict[str, ModuleDoc]:
    docs: Dict[str, ModuleDoc] = {}
    for p in dia_dir.rglob("*.architecture.module.md"):
        d = parse_module_doc(p)
        if not d:
            continue
        docs[d.module_id] = d
    return docs


def _filter_ids(mods: Dict[str, ModuleDoc], prefix: Optional[str]) -> List[str]:
    ids = sorted(mods.keys())
    if prefix:
        ids = [i for i in ids if i.startswith(prefix)]
    return ids


def cmd_list(mods: Dict[str, ModuleDoc], args: argparse.Namespace) -> int:
    ids = _filter_ids(mods, args.prefix)
    for mid in ids:
        m = mods[mid]
        print(f"{m.module_id}\t{m.name}\t{m.path}")
    return 0


def cmd_show(mods: Dict[str, ModuleDoc], args: argparse.Namespace) -> int:
    m = mods.get(args.module_id)
    if not m:
        print(f"error: module not found: {args.module_id}", file=sys.stderr)
        return 2

    print(f"module_id: {m.module_id}")
    print(f"name: {m.name}")
    if m.parent_module_id:
        print(f"parent_module_id: {m.parent_module_id}")
    print(f"path: {m.path}")
    print(f"file: {m.file}")

    if m.dependent_modules:
        print("dependent_modules:")
        for x in m.dependent_modules:
            print(f"  - {x}")
    if m.dependencies_required:
        print("dependencies.required:")
        for x in m.dependencies_required:
            print(f"  - {x}")
    if m.public_headers:
        print("public_api.headers:")
        for x in m.public_headers:
            print(f"  - {x}")
    if m.public_namespaces:
        print("public_api.namespaces:")
        for x in m.public_namespaces:
            print(f"  - {x}")
    if m.public_entry_points:
        print("public_api.entry_points:")
        for x in m.public_entry_points:
            print(f"  - {x}")
    return 0


def cmd_validate(mods: Dict[str, ModuleDoc], args: argparse.Namespace) -> int:
    ids = set(mods.keys())
    bad: List[str] = []
    for mid, m in mods.items():
        for child in m.dependent_modules:
            if child not in ids:
                bad.append(f"{mid}: dependent_modules references missing {child}")
        if m.parent_module_id and m.parent_module_id not in ids and m.parent_module_id != "dia.root":
            bad.append(f"{mid}: parent_module_id references missing {m.parent_module_id}")
        for dep in m.dependencies_required:
            if dep not in ids:
                bad.append(f"{mid}: dependencies.required references missing {dep}")

    if bad:
        for line in bad:
            print(line)
        return 1
    print("OK")
    return 0


def _edge_list(mods: Dict[str, ModuleDoc], prefix: Optional[str]) -> Tuple[Set[str], List[Tuple[str, str, str]]]:
    nodes: Set[str] = set()
    edges: List[Tuple[str, str, str]] = []

    for mid in _filter_ids(mods, prefix):
        m = mods[mid]
        nodes.add(mid)
        if m.parent_module_id and (not prefix or m.parent_module_id.startswith(prefix)):
            nodes.add(m.parent_module_id)
            edges.append((m.parent_module_id, mid, "contains"))
        for child in m.dependent_modules:
            if prefix and not child.startswith(prefix):
                continue
            nodes.add(child)
            edges.append((mid, child, "contains"))
        for dep in m.dependencies_required:
            if prefix and not dep.startswith(prefix):
                continue
            nodes.add(dep)
            edges.append((mid, dep, "depends"))
    return nodes, edges


def cmd_graph(mods: Dict[str, ModuleDoc], args: argparse.Namespace) -> int:
    nodes, edges = _edge_list(mods, args.prefix)
    # De-dupe edges while preserving order.
    seen: Set[Tuple[str, str, str]] = set()
    deduped: List[Tuple[str, str, str]] = []
    for e in edges:
        if e in seen:
            continue
        seen.add(e)
        deduped.append(e)
    edges = deduped

    fmt = args.format
    out_text = ""
    if fmt == "json":
        payload = {
            "nodes": [asdict(mods[n]) if n in mods else {"module_id": n} for n in sorted(nodes)],
            "edges": [{"from": a, "to": b, "kind": k} for (a, b, k) in edges],
        }
        out_text = json.dumps(payload, indent=2)
    elif fmt == "dot":
        lines = ["digraph dia_modules {", "  rankdir=LR;"]
        for n in sorted(nodes):
            label = n
            if n in mods:
                label = f"{mods[n].name}\\n{n}"
            lines.append(f"  \"{n}\" [label=\"{label}\"]; ")
        for a, b, k in edges:
            style = "solid" if k == "depends" else "dashed"
            lines.append(f"  \"{a}\" -> \"{b}\" [style={style}, label=\"{k}\"]; ")
        lines.append("}")
        out_text = "\n".join(lines)
    else:
        # mermaid
        def mid_to_mermaid_id(mid: str) -> str:
            # Mermaid node ids cannot contain dots.
            return re.sub(r"[^A-Za-z0-9_]", "_", mid)

        lines = ["graph TD"]

        # Declare nodes with labels for readability.
        for mid in sorted(nodes):
            nid = mid_to_mermaid_id(mid)
            label = mid
            if mid in mods:
                label = f"{mods[mid].name}\\n{mid}"
            # Use bracket notation to preserve label and special chars.
            lines.append(f"  {nid}[\"{label}\"]")

        for a, b, k in edges:
            a_id = mid_to_mermaid_id(a)
            b_id = mid_to_mermaid_id(b)
            if k == "depends":
                lines.append(f"  {a_id} --> {b_id}")
            else:
                lines.append(f"  {a_id} -.-> {b_id}")
        out_text = "\n".join(lines)

    if args.out:
        Path(args.out).parent.mkdir(parents=True, exist_ok=True)
        Path(args.out).write_text(out_text + "\n", encoding="utf-8")
    else:
        print(out_text)
    return 0


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(prog="dia_modules", description="View Dia module graph")
    p.add_argument("--dia", default=None, help="Path to Dia root (defaults to <repo>/Dia)")

    sub = p.add_subparsers(dest="cmd", required=True)

    sp = sub.add_parser("list", help="List modules")
    sp.add_argument("--prefix", default=None, help="Filter module_id prefix")
    sp.set_defaults(func=cmd_list)

    sp = sub.add_parser("show", help="Show module details")
    sp.add_argument("module_id")
    sp.set_defaults(func=cmd_show)

    sp = sub.add_parser("validate", help="Validate references")
    sp.set_defaults(func=cmd_validate)

    sp = sub.add_parser("graph", help="Output a module graph")
    sp.add_argument("--prefix", default=None, help="Filter module_id prefix")
    sp.add_argument("--format", choices=["mermaid", "dot", "json"], default="mermaid")
    sp.add_argument("--out", default=None, help="Write output to a file")
    sp.set_defaults(func=cmd_graph)

    return p


def main(argv: List[str]) -> int:
    args = build_parser().parse_args(argv)
    script_dir = Path(__file__).resolve().parent
    repo_root = script_dir.parent
    dia_dir = Path(args.dia) if args.dia else (repo_root / "Dia")
    if not dia_dir.exists():
        print(f"error: Dia dir not found: {dia_dir}", file=sys.stderr)
        return 2

    mods = load_all_modules(dia_dir)
    return int(args.func(mods, args))


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
