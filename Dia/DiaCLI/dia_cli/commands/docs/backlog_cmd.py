"""dia docs backlog — move entries between BACKLOG.md and BACKLOG-HISTORY.md."""
from __future__ import annotations

import re
from datetime import date
from pathlib import Path
from typing import List, Tuple

import click

from dia_cli.utils.repo_root import find_repo_root


def _row_cells(line: str) -> List[str] | None:
    """Cells of a markdown table data row, or None for non-rows / separators."""
    stripped = line.strip()
    if not stripped.startswith("|") or stripped.startswith("|-") or stripped.startswith("| -"):
        return None
    return [c.strip() for c in stripped.strip("|").split("|")]


def _normalise_name(text: str) -> str:
    """Lower-case alphanumerics only, so `DiaCamera3D`, `**diacamera-3d**` and
    `` `DiaCamera3D` `` compare equal."""
    return re.sub(r"[^0-9a-z]", "", text.lower())


def _cell_names(cell: str) -> List[str]:
    """Names a cell answers to: the whole cell, and the cell without a trailing
    parenthetical (`GameSpeedController (A2)` also answers to `GameSpeedController`)."""
    names = [_normalise_name(cell)]
    bare = re.sub(r"\s*\([^)]*\)\s*$", "", cell)
    if bare != cell:
        names.append(_normalise_name(bare))
    return [n for n in names if n]


def _matched_cell(cells: List[str], name_key: str) -> int | None:
    """Index of the name cell (first or second column) equal to the name, or None.
    Struck cells (`~~...~~`) are already moved and never match."""
    for i in range(min(2, len(cells))):
        if "~~" in cells[i]:
            continue
        if name_key in _cell_names(cells[i]):
            return i
    return None


def find_entry_lines(lines: List[str], name: str) -> List[int]:
    """Rows whose Project or Item cell is the whole name, never a substring of
    any cell text. Stops unrelated rows that merely mention the name in their
    notes from being struck (2026-10-14: S-1 and A4 rows were struck when moving
    another entry)."""
    name_key = _normalise_name(name)
    if not name_key:
        return []
    indices: List[int] = []
    for i, line in enumerate(lines):
        cells = _row_cells(line)
        if cells and _matched_cell(cells, name_key) is not None:
            indices.append(i)
    return indices


def strike_row(line: str, name: str) -> str:
    """Strike the name cells up to and including the one matching `name`:
    Project and Item for `| Project | Item | ...` rows (as hand-struck rows do),
    just the first cell when the name is in the first column."""
    cells = _row_cells(line)
    matched = _matched_cell(cells, _normalise_name(name)) if cells else None
    for _ in range((matched or 0) + 1):
        line = re.sub(
            r"\|\s*([^|~]+?)\s*\|",
            lambda m: f"| ~~{m.group(1).strip()}~~ |",
            line,
            count=1,
        )
    return line


def _extract_table_row_name(line: str) -> str:
    cells = [c.strip() for c in line.split("|") if c.strip()]
    return cells[0] if cells else ""


def move_to_history(
    repo_root: Path, spec_name: str, notes: str = "", allow_multiple: bool = False
) -> Tuple[bool, str]:
    backlog_path = repo_root / "docs" / "BACKLOG.md"
    history_path = repo_root / "docs" / "BACKLOG-HISTORY.md"

    if not backlog_path.exists():
        return False, "BACKLOG.md not found"

    text = backlog_path.read_text(encoding="utf-8")
    lines = text.splitlines()

    indices = find_entry_lines(lines, spec_name)
    if not indices:
        return False, f"No entry named '{spec_name}' found in BACKLOG.md (whole Project/Item cell match)"
    if len(indices) > 1 and not allow_multiple:
        listing = "\n".join(f"  {lines[i][:120]}" for i in indices)
        return False, f"{len(indices)} entries named '{spec_name}'; pass --all to move them all:\n{listing}"

    moved_entries: List[str] = []
    for idx in sorted(indices, reverse=True):
        moved_entries.insert(0, lines[idx])
        lines[idx] = strike_row(lines[idx], spec_name)

    backlog_path.write_text("\n".join(lines) + "\n", encoding="utf-8")

    today = date.today().isoformat()
    if history_path.exists():
        hist_text = history_path.read_text(encoding="utf-8")
    else:
        hist_text = "# Cluiche Backlog History\n\nCompleted items moved from BACKLOG.md.\n\n---\n"

    entry_note = notes if notes else "Moved by `dia docs backlog move`"
    for entry_line in moved_entries:
        cells = [c.strip() for c in entry_line.split("|") if c.strip()]
        display_name = cells[0] if cells else spec_name
        hist_entry = f"| {display_name} | {cells[1] if len(cells) > 1 else ''} | {today} | {entry_note} |"

        if "## Completed Systems" in hist_text:
            marker = "## Completed Systems"
            marker_pos = hist_text.find(marker)
            table_start = hist_text.find("\n|", marker_pos)
            if table_start != -1:
                next_blank = hist_text.find("\n\n", table_start)
                if next_blank == -1:
                    next_blank = len(hist_text)
                hist_text = hist_text[:next_blank] + f"\n{hist_entry}" + hist_text[next_blank:]
            else:
                hist_text += f"\n{hist_entry}\n"
        else:
            hist_text += f"\n{hist_entry}\n"

    history_path.write_text(hist_text, encoding="utf-8")
    return True, f"Moved {len(moved_entries)} entry(s) for '{spec_name}'"


@click.command("backlog")
@click.argument("action", type=click.Choice(["move"]))
@click.argument("spec_name")
@click.option("--notes", "-n", default="", help="Notes for the history entry.")
@click.option("--all", "allow_multiple", is_flag=True, default=False,
              help="Move every row with this name (default: refuse when more than one matches).")
@click.option("--dry-run", is_flag=True, default=False, help="Show what would change.")
def backlog(action: str, spec_name: str, notes: str, allow_multiple: bool, dry_run: bool) -> None:
    """Move a backlog entry to history.

    ACTION is 'move' (more actions may be added later).
    SPEC_NAME is the whole Project or Item cell of the row (case, spacing and
    punctuation ignored; a trailing parenthetical is optional). Text in other
    cells never matches.

    Examples:
        dia docs backlog move DiaCamera3D --notes "All features implemented"
        dia docs backlog move GoogleTestSpeed
    """
    repo_root = find_repo_root(__file__)

    if dry_run:
        backlog_path = repo_root / "docs" / "BACKLOG.md"
        if not backlog_path.exists():
            click.echo("BACKLOG.md not found")
            return
        text = backlog_path.read_text(encoding="utf-8")
        lines = text.splitlines()
        indices = find_entry_lines(lines, spec_name)
        if not indices:
            click.echo(f"[dry-run] No entry named '{spec_name}' found")
        else:
            click.echo(f"[dry-run] Would move {len(indices)} entry(s):")
            for idx in indices:
                click.echo(f"  {lines[idx][:120]}")
        return

    success, message = move_to_history(repo_root, spec_name, notes, allow_multiple)
    if success:
        click.echo(f"[dia docs] {message}")
    else:
        raise click.ClickException(message)
