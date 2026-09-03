"""dia docs spec-done — atomically mark a spec Done across all related files."""
from __future__ import annotations

import re
import shutil
from datetime import date
from pathlib import Path
from typing import List

import click

from dia_cli.utils.repo_root import find_repo_root


_STATUS_RE = re.compile(r"(\*\*Status:\*\*\s*`?)(\w[\w ]*?)(`?\s*$)", re.MULTILINE)

# Alternate convention used by many older specs: a "## Status" heading followed
# by a bare backtick-quoted value on its own line, e.g.:
#   ## Status
#
#   `Approved`
_STATUS_HEADING_RE = re.compile(
    r"(^## Status\s*\n+\s*`)(\w[\w ]*?)(`\s*$)", re.MULTILINE
)


def _update_spec_status(spec_path: Path, target_status: str = "Done") -> str:
    text = spec_path.read_text(encoding="utf-8")
    m = _STATUS_RE.search(text)
    if not m:
        m = _STATUS_HEADING_RE.search(text)
    if not m:
        raise click.ClickException(
            f"No **Status:** field or '## Status' heading found in {spec_path.name}"
        )

    current = m.group(2).strip()
    if current == target_status:
        return f"Already {target_status}"

    if current != "Approved" and target_status == "Done":
        raise click.ClickException(
            f"Spec status is '{current}', must be 'Approved' to mark Done. "
            f"(path: {spec_path})"
        )

    new_text = text[:m.start(2)] + target_status + text[m.end(2):]
    spec_path.write_text(new_text, encoding="utf-8")
    return f"{current} → {target_status}"


def _find_plan_file(spec_path: Path) -> Path | None:
    plan_path = spec_path.with_suffix(".plan.md")
    if plan_path.exists():
        return plan_path
    return None


def _update_plan_header(plan_path: Path) -> None:
    text = plan_path.read_text(encoding="utf-8")
    # Word-only status token (e.g. "In Progress", "Done") — deliberately does NOT
    # consume trailing whitespace, so any "... — <notes>" suffix on the same line
    # keeps its separating space instead of being glued to the new status word.
    text = re.sub(
        r"(\*\*Status:\*\*\s*)(\w+(?:\s+\w+)*)",
        r"\1Done",
        text,
        count=1,
    )
    plan_path.write_text(text, encoding="utf-8")


def _move_backlog_entry(repo_root: Path, spec_name: str) -> bool:
    backlog_path = repo_root / "docs" / "BACKLOG.md"
    history_path = repo_root / "docs" / "BACKLOG-HISTORY.md"

    if not backlog_path.exists():
        return False

    backlog_text = backlog_path.read_text(encoding="utf-8")
    lines = backlog_text.splitlines()

    moved_lines: List[str] = []
    remaining_lines: List[str] = []
    found = False

    for line in lines:
        if spec_name.lower() in line.lower() and "|" in line and "~~" not in line:
            struck = re.sub(
                r"\|\s*([^|]+?)\s*\|",
                lambda m: f"| ~~{m.group(1).strip()}~~ |",
                line,
                count=1,
            )
            remaining_lines.append(struck)
            moved_lines.append(line)
            found = True
        else:
            remaining_lines.append(line)

    if not found:
        return False

    backlog_path.write_text("\n".join(remaining_lines) + "\n", encoding="utf-8")

    if history_path.exists():
        history_text = history_path.read_text(encoding="utf-8")
        today = date.today().isoformat()
        entry = f"| {spec_name} | [spec](specs/) | {today} | Marked done by `dia docs spec-done` |"

        if "## Completed Systems" in history_text:
            history_text = history_text.replace(
                "## Completed Systems\n\n| System",
                f"## Completed Systems\n\n| System",
            )
            table_end = history_text.find("\n\n---", history_text.find("## Completed Systems"))
            if table_end == -1:
                table_end = len(history_text)
            history_text = history_text[:table_end] + f"\n{entry}" + history_text[table_end:]
        else:
            history_text += f"\n{entry}\n"

        history_path.write_text(history_text, encoding="utf-8")

    return True


def _archive_plan_file(repo_root: Path, plan_path: Path) -> Path:
    """Move plan_path to docs/archive/plans/ and return the destination."""
    archive_dir = repo_root / "docs" / "archive" / "plans"
    archive_dir.mkdir(parents=True, exist_ok=True)
    dest = archive_dir / plan_path.name
    shutil.move(str(plan_path), str(dest))
    return dest


@click.command("spec-done")
@click.argument("spec_path", type=click.Path(exists=True))
@click.option("--skip-backlog", is_flag=True, default=False, help="Don't touch BACKLOG.md.")
@click.option("--archive-plan", is_flag=True, default=False,
              help="Move the .plan.md to docs/archive/plans/ after marking Done.")
@click.option("--dry-run", is_flag=True, default=False, help="Show what would change without writing.")
def spec_done(spec_path: str, skip_backlog: bool, archive_plan: bool, dry_run: bool) -> None:
    """Mark a spec as Done and update all related files.

    Updates: spec status field, plan header status, backlog entry (strike-through + move to history).
    With --archive-plan, also moves the .plan.md to docs/archive/plans/.

    SPEC_PATH is the path to the spec markdown file.

    Example:
        dia docs spec-done docs/specs/applications/dia/systems/diacamera3d/diacamera3d.md
        dia docs spec-done docs/specs/applications/dia/systems/diacamera3d/diacamera3d.md --archive-plan
    """
    repo_root = find_repo_root(__file__)
    path = Path(spec_path).resolve()
    spec_name = path.stem

    actions: List[str] = []

    if dry_run:
        click.echo(f"[dry-run] Would mark {spec_name} as Done:")
        click.echo(f"  Update {path.name} status → Done")
        plan = _find_plan_file(path)
        if plan:
            click.echo(f"  Update {plan.name} header → Done")
            if archive_plan:
                archive_dir = repo_root / "docs" / "archive" / "plans"
                dest_rel = (archive_dir / plan.name).relative_to(repo_root)
                click.echo(f"  Move {plan.name} → {dest_rel}")
        if not skip_backlog:
            click.echo(f"  Strike/move backlog entry for '{spec_name}'")
        return

    result = _update_spec_status(path)
    actions.append(f"Spec: {result}")

    plan = _find_plan_file(path)
    if plan:
        _update_plan_header(plan)
        actions.append("Plan: header → Done")
        if archive_plan:
            dest = _archive_plan_file(repo_root, plan)
            actions.append(f"Plan: archived → {dest.relative_to(repo_root)}")

    if not skip_backlog:
        moved = _move_backlog_entry(repo_root, spec_name)
        if moved:
            actions.append("Backlog: entry struck through")
        else:
            actions.append("Backlog: no matching entry found (OK)")

    click.echo(f"[dia docs] spec-done {spec_name}:")
    for a in actions:
        click.echo(f"  {a}")
