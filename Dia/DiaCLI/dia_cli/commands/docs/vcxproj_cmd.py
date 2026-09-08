"""dia docs vcxproj-add — add source files to vcxproj + filters without AI."""
from __future__ import annotations

import uuid
from pathlib import Path

import click

from dia_cli.utils.repo_root import find_repo_root


def _find_last_entry_end(lines: list[str], tag: str) -> int:
    """Return the line index of the last complete <tag>...</tag> (or self-closing
    <tag ... />) entry, so an insertion can be placed cleanly after it.

    Searching only for lines containing the opening `<tag Include=` (the previous
    behavior) is wrong for multi-line entries: it inserts BETWEEN an existing
    entry's opening tag and its own closing lines instead of after the whole
    entry, corrupting the file. A multi-line entry ends on a line that is exactly
    `</tag>` (own line); a single-line entry ends on the same line it starts,
    matching `<tag Include="..." />`.
    """
    last_idx = -1
    for i, line in enumerate(lines):
        stripped = line.strip()
        if stripped == f"</{tag}>" or (
            stripped.startswith(f"<{tag} Include=") and stripped.endswith("/>")
        ):
            last_idx = i
    return last_idx


def _already_present(text: str, filename: str) -> bool:
    return f'Include="{filename}"' in text or f"Include='{filename}'" in text


def add_to_vcxproj(vcxproj_path: Path, relative_path: str, is_header: bool) -> bool:
    if not vcxproj_path.exists():
        raise click.ClickException(f"vcxproj not found: {vcxproj_path}")

    text = vcxproj_path.read_text(encoding="utf-8")

    if _already_present(text, relative_path):
        return False

    tag = "ClInclude" if is_header else "ClCompile"
    line = f'    <{tag} Include="{relative_path}" />'

    lines = text.splitlines(keepends=True)
    last_idx = _find_last_entry_end(lines, tag)
    if last_idx == -1:
        raise click.ClickException(f"Could not find an existing <{tag}> entry to anchor after")
    lines.insert(last_idx + 1, line + "\n")
    vcxproj_path.write_text("".join(lines), encoding="utf-8")
    return True


def _ensure_filter_declared(filters_path: Path, filter_name: str) -> bool:
    """Ensure <Filter Include="{filter_name}"> exists in the top Filter-declaration
    block. Returns True if a new declaration was inserted."""
    text = filters_path.read_text(encoding="utf-8")

    if f'<Filter Include="{filter_name}">' in text or f'<Filter Include="{filter_name}" ' in text:
        return False

    lines = text.splitlines(keepends=True)
    last_idx = _find_last_entry_end(lines, "Filter")
    if last_idx == -1:
        raise click.ClickException("Could not find an existing <Filter> declaration to anchor after")

    new_guid = str(uuid.uuid4())
    block = (
        f'    <Filter Include="{filter_name}">\n'
        f'      <UniqueIdentifier>{{{new_guid}}}</UniqueIdentifier>\n'
        f'    </Filter>\n'
    )
    lines.insert(last_idx + 1, block)
    filters_path.write_text("".join(lines), encoding="utf-8")
    return True


def add_to_filters(filters_path: Path, relative_path: str, is_header: bool, filter_name: str | None) -> bool:
    if not filters_path.exists():
        raise click.ClickException(f"filters file not found: {filters_path}")

    if filter_name:
        _ensure_filter_declared(filters_path, filter_name)

    text = filters_path.read_text(encoding="utf-8")

    if _already_present(text, relative_path):
        return False

    tag = "ClInclude" if is_header else "ClCompile"

    if filter_name:
        block = (
            f'    <{tag} Include="{relative_path}">\n'
            f'      <Filter>{filter_name}</Filter>\n'
            f'    </{tag}>'
        )
    else:
        block = f'    <{tag} Include="{relative_path}" />'

    lines = text.splitlines(keepends=True)
    last_idx = _find_last_entry_end(lines, tag)
    if last_idx == -1:
        raise click.ClickException(f"Could not find an existing <{tag}> entry to anchor after")
    lines.insert(last_idx + 1, block + "\n")
    filters_path.write_text("".join(lines), encoding="utf-8")
    return True


@click.command("vcxproj-add")
@click.argument("project")
@click.argument("file_path")
@click.option("--filter", "filter_name", default=None, help="Filter folder name (for .vcxproj.filters).")
@click.option("--dry-run", is_flag=True, default=False, help="Show what would change.")
def vcxproj_add(project: str, file_path: str, filter_name: str, dry_run: bool) -> None:
    """Add a source file to a vcxproj and its .filters file.

    PROJECT is the vcxproj name (e.g. DiaCore, GoogleTests, CluicheTest).
    FILE_PATH is the relative path from the vcxproj location (e.g. Containers\\Array.h).

    Auto-detects header vs source from extension. Idempotent — skips if already present.

    Examples:
        dia docs vcxproj-add DiaCore "NewModule\\NewModule.h" --filter NewModule
        dia docs vcxproj-add DiaCore "NewModule\\NewModule.cpp" --filter NewModule
        dia docs vcxproj-add GoogleTests "DiaCore\\TestNewModule.cpp"
    """
    repo_root = find_repo_root(__file__)
    is_header = file_path.lower().endswith((".h", ".hpp", ".hxx"))

    # Find the vcxproj file
    candidates = list(repo_root.rglob(f"{project}.vcxproj"))
    candidates = [c for c in candidates if ".claude" not in str(c) and "node_modules" not in str(c)]

    if not candidates:
        raise click.ClickException(f"Could not find {project}.vcxproj in repo")
    if len(candidates) > 1:
        click.echo(f"Multiple matches found, using first: {candidates[0]}")

    vcxproj_path = candidates[0]
    filters_path = vcxproj_path.with_suffix(".vcxproj.filters")

    if not filter_name:
        parts = file_path.replace("/", "\\").split("\\")
        if len(parts) > 1:
            filter_name = parts[0]

    if dry_run:
        tag = "ClInclude" if is_header else "ClCompile"
        click.echo(f"[dry-run] Would add to {vcxproj_path.name}:")
        click.echo(f"  <{tag} Include=\"{file_path}\" />")
        if filters_path.exists():
            click.echo(f"  + filters entry under '{filter_name or '(root)'}'")
        return

    added_proj = add_to_vcxproj(vcxproj_path, file_path, is_header)
    added_filt = False
    if filters_path.exists():
        added_filt = add_to_filters(filters_path, file_path, is_header, filter_name)

    if added_proj or added_filt:
        click.echo(f"[dia docs] Added {file_path} to {project}:")
        if added_proj:
            click.echo(f"  vcxproj: {'ClInclude' if is_header else 'ClCompile'} entry added")
        if added_filt:
            click.echo(f"  filters: entry added (filter: {filter_name or 'root'})")
    else:
        click.echo(f"[dia docs] {file_path} already present in {project} — no changes.")
