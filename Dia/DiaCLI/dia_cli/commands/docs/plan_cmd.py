"""dia docs plan — update plan task status and notes without AI."""
from __future__ import annotations

import re
from pathlib import Path
from typing import Optional

import click


_STATUS_VALUES = {"Not Started", "In Progress", "Done", "Blocked"}
_TRANSITIONS = {
    "Not Started": {"In Progress", "Blocked"},
    "In Progress": {"Done", "Blocked"},
    "Blocked": {"In Progress", "Not Started"},
    "Done": set(),
}

_TABLE_ROW_RE = re.compile(
    r"^\|\s*(\d+)\s*\|"
    r"\s*(.*?)\s*\|"
    r"\s*(.*?)\s*\|"
    r"\s*(.*?)\s*\|"
    r"\s*(.*?)\s*\|"
    r"\s*(.*?)\s*\|$"
)


def _parse_plan_row(line: str) -> Optional[dict]:
    m = _TABLE_ROW_RE.match(line)
    if not m:
        return None
    return {
        "task_num": m.group(1).strip(),
        "task": m.group(2).strip(),
        "test": m.group(3).strip(),
        "status": m.group(4).strip(),
        "model": m.group(5).strip(),
        "notes": m.group(6).strip(),
    }


def _rebuild_row(row: dict) -> str:
    return (
        f"| {row['task_num']} "
        f"| {row['task']} "
        f"| {row['test']} "
        f"| {row['status']} "
        f"| {row['model']} "
        f"| {row['notes']} |"
    )


def update_plan_task(
    plan_path: Path,
    task_num: str,
    status: Optional[str],
    notes: Optional[str],
    model: Optional[str],
    force: bool = False,
) -> str:
    if not plan_path.exists():
        raise click.ClickException(f"Plan file not found: {plan_path}")

    text = plan_path.read_text(encoding="utf-8")
    lines = text.splitlines()
    found = False
    updated_line = ""

    for i, line in enumerate(lines):
        row = _parse_plan_row(line)
        if row is None or row["task_num"] != task_num:
            continue

        found = True
        old_status = row["status"]

        if status:
            if status not in _STATUS_VALUES:
                raise click.ClickException(
                    f"Invalid status '{status}'. Must be one of: {', '.join(sorted(_STATUS_VALUES))}"
                )
            if not force and status not in _TRANSITIONS.get(old_status, set()):
                raise click.ClickException(
                    f"Invalid transition: {old_status} → {status}. "
                    f"Allowed: {_TRANSITIONS.get(old_status, set()) or 'none (task is Done)'}. "
                    f"Use --force to override."
                )
            row["status"] = status

        if notes is not None:
            if row["notes"] and notes:
                row["notes"] = f"{row['notes']}; {notes}"
            elif notes:
                row["notes"] = notes

        if model:
            row["model"] = model

        lines[i] = _rebuild_row(row)
        updated_line = lines[i]
        break

    if not found:
        raise click.ClickException(f"Task #{task_num} not found in {plan_path.name}")

    plan_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return updated_line


@click.command("plan")
@click.argument("plan_path", type=click.Path(exists=True))
@click.argument("task_num")
@click.option("--status", "-s", type=click.Choice(sorted(_STATUS_VALUES)), default=None,
              help="New status value.")
@click.option("--notes", "-n", default=None, help="Notes to append.")
@click.option("--model", "-m", default=None, help="Model override (haiku/sonnet/opus).")
@click.option("--force", is_flag=True, default=False, help="Skip status transition validation.")
def plan(plan_path: str, task_num: str, status: str, notes: str, model: str, force: bool) -> None:
    """Update a task row in a .plan.md file.

    PLAN_PATH is the path to the plan file.
    TASK_NUM is the task number (the # column).

    Examples:
        dia docs plan docs/specs/systems/dia/foo.plan.md 3 --status Done --notes "All tests pass"
        dia docs plan foo.plan.md 1 -s "In Progress"
    """
    result = update_plan_task(Path(plan_path), task_num, status, notes, model, force)
    click.echo(f"Updated: {result}")
