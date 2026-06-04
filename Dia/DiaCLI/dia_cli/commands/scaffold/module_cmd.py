"""dia scaffold module — create all boilerplate for a new Dia engine module."""
from __future__ import annotations

from pathlib import Path
from typing import Optional

import click

from dia_cli.utils.repo_root import find_repo_root


# ---------------------------------------------------------------------------
# Name derivation
# ---------------------------------------------------------------------------

def _derive_names(parent: str, name: str) -> dict:
    """Derive all name variants from parent (e.g. DiaCore) and name (e.g. Serializer)."""
    # Strip "Dia" prefix for IDs
    parent_short = parent[3:] if parent.startswith("Dia") else parent
    parent_id = f"dia.{parent_short.lower()}"
    module_id = f"{parent_id}.{name.lower()}"
    namespace_parent = parent_short

    return dict(
        parent=parent,
        name=name,
        parent_short=parent_short,
        parent_id=parent_id,
        module_id=module_id,
        namespace_parent=namespace_parent,
        full_path=f"Dia/{parent}/{name}/",
        header_file=f"{name}.h",
        impl_file=f"{name}.cpp",
        module_md_file=f"{module_id}.architecture.module.md",
    )


# ---------------------------------------------------------------------------
# Content generators
# ---------------------------------------------------------------------------

def _header_content(n: dict) -> str:
    return f"""\
#pragma once

namespace Dia
{{
\tnamespace {n['namespace_parent']}
\t{{
\t\tclass {n['name']}
\t\t{{
\t\tpublic:
\t\t\t{n['name']}() = default;
\t\t\t~{n['name']}() = default;
\t\t}};
\t}}
}}
"""


def _impl_content(n: dict) -> str:
    return f"""\
#include "{n['parent']}/{n['name']}/{n['name']}.h"

namespace Dia
{{
\tnamespace {n['namespace_parent']}
\t{{
\t}}
}}
"""


def _module_md_content(n: dict, layer: str) -> str:
    return f"""\
---
schema: dia.module.v1
module_id: {n['module_id']}
name: {n['name']}
layer: {layer}
status: active
maturity: dev
path: Dia/{n['parent']}/{n['name']}/
language: cpp
parent_module_id: {n['parent_id']}
summary: "TODO: describe this module"
intent: "TODO: one-sentence purpose"
responsibilities:
  - "TODO"
non_responsibilities:
  - "TODO"
dependent_modules: []
public_api:
  headers:
    - {n['parent']}/{n['name']}/{n['name']}.h
  namespaces:
    - Dia::{n['namespace_parent']}::{n['name']}
  entry_points: []
---

# {n['name']}

TODO: describe this module.
"""


# ---------------------------------------------------------------------------
# File update helpers
# ---------------------------------------------------------------------------

def _insert_after_last(text: str, search: str, insertion: str) -> str:
    """Insert insertion after the LAST line containing search."""
    lines = text.splitlines(keepends=True)
    last_idx = -1
    for i, line in enumerate(lines):
        if search in line:
            last_idx = i
    if last_idx == -1:
        raise ValueError(f"Could not find anchor '{search}' in text")
    lines.insert(last_idx + 1, insertion + "\n")
    return "".join(lines)


def _update_vcxproj(path: Path, name: str) -> None:
    text = path.read_text(encoding="utf-8")
    cpp_line = f'    <ClCompile Include="{name}\\{name}.cpp" />'
    h_line = f'    <ClInclude Include="{name}\\{name}.h" />'
    text = _insert_after_last(text, "<ClCompile Include=", cpp_line)
    text = _insert_after_last(text, "<ClInclude Include=", h_line)
    path.write_text(text, encoding="utf-8")


def _update_vcxproj_filters(path: Path, name: str) -> None:
    text = path.read_text(encoding="utf-8")
    cpp_block = (
        f'    <ClCompile Include="{name}\\{name}.cpp">\n'
        f'      <Filter>{name}</Filter>\n'
        f'    </ClCompile>'
    )
    h_block = (
        f'    <ClInclude Include="{name}\\{name}.h">\n'
        f'      <Filter>{name}</Filter>\n'
        f'    </ClInclude>'
    )
    text = _insert_after_last(text, "<ClCompile Include=", cpp_block)
    text = _insert_after_last(text, "<ClInclude Include=", h_block)
    path.write_text(text, encoding="utf-8")


# ---------------------------------------------------------------------------
# Click command
# ---------------------------------------------------------------------------

@click.command("module")
@click.argument("parent")
@click.argument("name")
@click.option(
    "--layer",
    default="platform",
    type=click.Choice(["platform", "framework", "application", "backend"]),
    show_default=True,
    help="Module layer classification.",
)
@click.option(
    "--dry-run",
    is_flag=True,
    default=False,
    help="Print what would be created/modified without writing any files.",
)
def module(parent: str, name: str, layer: str, dry_run: bool) -> None:
    """Scaffold all boilerplate for a new Dia engine module.

    PARENT is the parent project (e.g. DiaCore, DiaAnimation2D).
    NAME is PascalCase module name (e.g. Serializer, Timeline).
    """
    n = _derive_names(parent, name)

    repo_root = find_repo_root(__file__)

    # Define all paths
    module_dir = repo_root / "Dia" / parent / name
    header_path = module_dir / n["header_file"]
    impl_path = module_dir / n["impl_file"]

    docs_dir = repo_root / "Dia" / parent / "Docs"
    module_md_path = docs_dir / n["module_md_file"]

    vcxproj_path = repo_root / "Dia" / parent / f"{parent}.vcxproj"
    filters_path = repo_root / "Dia" / parent / f"{parent}.vcxproj.filters"

    rel = lambda p: str(p.relative_to(repo_root)).replace("\\", "/")

    if dry_run:
        click.echo(f"[dry-run] Would create/modify for module '{n['module_id']}':\n")
        click.echo(f"  Create  {rel(header_path)}")
        click.echo(f"  Create  {rel(impl_path)}")
        click.echo(f"  Create  {rel(module_md_path)}")
        click.echo(f"  Update  {rel(vcxproj_path)}")
        click.echo(f"  Update  {rel(filters_path)}")
        return

    created = []
    updated = []

    # 1. Create header
    module_dir.mkdir(parents=True, exist_ok=True)
    header_path.write_text(_header_content(n), encoding="utf-8")
    created.append(rel(header_path))

    # 2. Create implementation
    impl_path.write_text(_impl_content(n), encoding="utf-8")
    created.append(rel(impl_path))

    # 3. Create module doc
    docs_dir.mkdir(parents=True, exist_ok=True)
    module_md_path.write_text(_module_md_content(n, layer), encoding="utf-8")
    created.append(rel(module_md_path))

    # 4. Update vcxproj
    if vcxproj_path.exists():
        _update_vcxproj(vcxproj_path, name)
        updated.append(rel(vcxproj_path))
    else:
        click.echo(f"  SKIP  {rel(vcxproj_path)} (not found)")

    # 5. Update vcxproj.filters
    if filters_path.exists():
        _update_vcxproj_filters(filters_path, name)
        updated.append(rel(filters_path))
    else:
        click.echo(f"  SKIP  {rel(filters_path)} (not found)")

    # Summary
    click.echo("")
    for path_str in created:
        click.echo(f"  Created  {path_str}")
    for path_str in updated:
        click.echo(f"  Updated  {path_str}")
    click.echo(
        f"\nNext: update module-registry.md and fill in {n['module_md_file']}"
    )

    # Post-step: sync SLN solution folders
    try:
        from dia_cli.commands.check.sln_sync import run_sln_sync
        changes, sln_warnings, _ = run_sln_sync(repo_root=repo_root, dry_run=False)
        for w in sln_warnings:
            if w.startswith("WARN") and "missing layer" not in w:
                click.echo(f"  sln-sync: {w}")
        if any("MOVE" in c or "CREATE" in c for c in changes):
            click.echo(f"  sln-sync: {len(changes)} folder assignment(s) updated in Cluiche.sln")
    except Exception as exc:
        click.echo(f"  sln-sync: skipped ({exc})")
