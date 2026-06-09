"""dia docs backlog — move entries between BACKLOG.md and BACKLOG-HISTORY.md."""
from __future__ import annotations

import re
from datetime import date
from pathlib import Path
from typing import List, Tuple

import click

from dia_cli.utils.repo_root import find_repo_root


def _find_entry_lines(lines: List[str], spec_name: str) -> List[int]:
    indices: List[int] = []
    name_lower = spec_name.lower()
    for i, line in enumerate(lines):
        if name_lower in line.lower() and "|" in line and not line.strip().startswith("|--"):
            if "~~" not in line:
                indices.append(i)
    return indices


def _extract_table_row_name(line: str) -> str:
    cells = [c.strip() for c in line.split("|") if c.strip()]
    return cells[0] if cells else ""


def move_to_history(repo_root: Path, spec_name: str, notes: str = "") -> Tuple[bool, str]:
    backlog_path = repo_root / "docs" / "BACKLOG.md"
    history_path = repo_root / "docs" / "BACKLOG-HISTORY.md"

    if not backlog_path.exists():
        return False, "BACKLOG.md not found"

    text = backlog_path.read_text(encoding="utf-8")
    lines = text.splitlines()

    indices = _find_entry_lines(lines, spec_name)
    if not indices:
        return False, f"No entry matching '{spec_name}' found in BACKLOG.md"

    moved_entries: List[str] = []
    for idx in sorted(indices, reverse=True):
        moved_entries.insert(0, lines[idx])
        lines[idx] = re.sub(
            r"\|\s*([^|~]+?)\s*\|",
            lambda m: f"| ~~{m.group(1).strip()}~~ |",
            lines[idx],
            count=1,
        )

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
@click.option("--dry-run", is_flag=True, default=False, help="Show what would change.")
def backlog(action: str, spec_name: str, notes: str, dry_run: bool) -> None:
    """Move a backlog entry to history.

    ACTION is 'move' (more actions may be added later).
    SPEC_NAME is a substring to match in the backlog table rows.

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
        indices = _find_entry_lines(lines, spec_name)
        if not indices:
            click.echo(f"[dry-run] No entry matching '{spec_name}' found")
        else:
            click.echo(f"[dry-run] Would move {len(indices)} entry(s):")
            for idx in indices:
                click.echo(f"  {lines[idx][:120]}")
        return

    success, message = move_to_history(repo_root, spec_name, notes)
    if success:
        click.echo(f"[dia docs] {message}")
    else:
        raise click.ClickException(message)
