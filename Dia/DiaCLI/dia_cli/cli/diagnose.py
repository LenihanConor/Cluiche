"""dia diagnose -- triage the last session log, E2E report, and crash dumps."""
from __future__ import annotations

import json
import os
from pathlib import Path

import click

from dia_cli.utils.repo_root import find_repo_root


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

_APP_OUT_NAMES = {
    "cluichetest":   "CluicheTest",
    "cluicheeditor": "CluicheEditor",
    "googletest":    "GoogleTests",
}


def _latest_session_log(out_root: Path, app: str) -> Path | None:
    app_out = _APP_OUT_NAMES.get(app.lower(), app)
    sessions_dir = out_root / app_out / "sessions"
    if not sessions_dir.exists():
        return None
    dirs = sorted(sessions_dir.iterdir(), key=lambda p: p.name, reverse=True)
    for d in dirs:
        log = d / "log.jsonl"
        if log.exists():
            return log
    return None


def _latest_e2e_report(out_root: Path) -> Path | None:
    report_dir = out_root / "e2e_reports"
    if not report_dir.exists():
        return None
    reports = sorted(report_dir.glob("e2e_*.json"), key=lambda p: p.name, reverse=True)
    return reports[0] if reports else None


def _crash_dumps(exe_stem: str) -> list[Path]:
    dump_dir = Path(os.environ.get("LOCALAPPDATA", "")) / "CrashDumps"
    if not dump_dir.exists():
        return []
    return sorted(dump_dir.glob(f"{exe_stem}*.dmp"), key=lambda p: p.stat().st_mtime, reverse=True)


def _parse_log(log_path: Path) -> dict:
    """Extract: last module transition, all ERROR entries, final stage."""
    errors = []
    last_module_line = None
    last_transition_line = None
    last_stage = None
    last_stage_transition_line = None

    try:
        lines = log_path.read_text(encoding="utf-8", errors="replace").splitlines()
    except Exception as e:
        return {"error": str(e)}

    for raw in lines:
        if not raw.strip():
            continue
        try:
            entry = json.loads(raw)
        except json.JSONDecodeError:
            # Truncated last line (crash mid-write) — skip
            continue

        level = entry.get("level", "")
        msg = entry.get("msg", "")
        channel = entry.get("channel", "")

        if level == "error":
            errors.append(f"[{channel}] {msg}")

        if "module.state.transition" in msg or ("Module" in msg and ("DoStart" in msg or "DoStop" in msg)):
            last_module_line = msg

        if "Stage transition:" in msg:
            last_transition_line = msg
            # Extract "to" stage: "Stage transition: 'A' -> 'B'"
            try:
                to_part = msg.split("->")[-1].strip().strip("'")
                last_stage = to_part
            except Exception:
                pass

        if "stage_transition" in msg.lower() and "->" in msg:
            last_stage_transition_line = msg

    was_truncated = False
    if lines:
        try:
            json.loads(lines[-1])
        except json.JSONDecodeError:
            was_truncated = True

    return {
        "error_count": len(errors),
        "errors": errors[-10:],  # last 10
        "last_module": last_module_line,
        "last_transition": last_transition_line,
        "last_stage": last_stage,
        "truncated": was_truncated,
        "total_lines": len(lines),
    }


# ---------------------------------------------------------------------------
# Command
# ---------------------------------------------------------------------------

@click.command("diagnose")
@click.option("--app", default="cluichetest", show_default=True,
              help="App to inspect (cluichetest, cluicheeditor, googletest)")
@click.option("--no-log", is_flag=True, default=False, help="Skip session log analysis")
@click.option("--no-e2e", is_flag=True, default=False, help="Skip E2E report")
@click.option("--no-dumps", is_flag=True, default=False, help="Skip crash dump check")
def cli(app: str, no_log: bool, no_e2e: bool, no_dumps: bool) -> None:
    """Triage the last session log, E2E report, and crash dumps.

    Surfaces: last stage transition, crash point (truncated log),
    ERROR-level log entries, incomplete E2E stages, and crash dump paths.

    \b
    Examples:
      dia diagnose
      dia diagnose --app cluicheeditor
      dia diagnose --no-dumps
    """
    repo_root = find_repo_root(__file__)
    out_root = repo_root / "Cluiche" / "out"

    any_output = False

    # ------------------------------------------------------------------
    # Session log
    # ------------------------------------------------------------------
    if not no_log:
        log_path = _latest_session_log(out_root, app)
        if log_path is None:
            click.echo(f"[session log] No session logs found for '{app}'")
        else:
            click.echo(f"[session log] {log_path.parent.name}")
            info = _parse_log(log_path)

            if "error" in info:
                click.echo(f"  parse error: {info['error']}")
            else:
                if info["truncated"]:
                    click.secho("  ⚠ Log truncated (crash mid-write — app did not exit cleanly)", fg="yellow")
                else:
                    click.secho("  ✓ Log complete (clean exit)", fg="green")

                if info["last_stage"]:
                    click.echo(f"  Last stage : {info['last_stage']}")
                if info["last_transition"]:
                    click.echo(f"  Last trans : {info['last_transition']}")
                if info["last_module"]:
                    click.echo(f"  Last module: {info['last_module']}")

                if info["error_count"]:
                    click.secho(f"\n  ERROR entries ({info['error_count']} total, showing last {len(info['errors'])}):", fg="red")
                    for e in info["errors"]:
                        click.secho(f"    {e}", fg="red")
                else:
                    click.secho("  No ERROR entries", fg="green")
        any_output = True

    # ------------------------------------------------------------------
    # E2E report
    # ------------------------------------------------------------------
    if not no_e2e:
        if any_output:
            click.echo("")
        report_path = _latest_e2e_report(out_root)
        if report_path is None:
            click.echo("[e2e report] No E2E reports found")
        else:
            click.echo(f"[e2e report] {report_path.name}")
            try:
                data = json.loads(report_path.read_text(encoding="utf-8"))
                ts = data.get("timestamp", "?")
                total = data.get("total_duration_s", "?")
                passed = data.get("passed", 0)
                skipped = data.get("skipped", 0)
                failed = data.get("failed", 0)
                stages = data.get("stages", {})
                click.echo(f"  Timestamp : {ts}  ({total}s total)")
                click.echo(f"  Results   : {passed} passed, {skipped} skipped, {failed} failed")
                if stages:
                    click.echo("  Stages:")
                    for name, v in stages.items():
                        status = v.get("status", "?")
                        dur = v.get("duration_s", 0)
                        detail = v.get("detail", "")
                        if status == "passed":
                            icon = click.style("PASS", fg="green")
                        elif status == "skipped":
                            icon = click.style("SKIP", fg="cyan")
                        else:
                            icon = click.style("FAIL", fg="red")
                        detail_str = f" — {detail}" if detail else ""
                        click.echo(f"    [{icon}] {name} ({dur}s){detail_str}")
            except Exception as e:
                click.secho(f"  Could not parse report: {e}", fg="yellow")
        any_output = True

    # ------------------------------------------------------------------
    # Crash dumps
    # ------------------------------------------------------------------
    if not no_dumps:
        if any_output:
            click.echo("")
        app_out = _APP_OUT_NAMES.get(app.lower(), app)
        dumps = _crash_dumps(app_out)
        if not dumps:
            click.secho("[crash dumps] None found", fg="green")
        else:
            click.secho(f"[crash dumps] {len(dumps)} dump(s) found:", fg="yellow")
            for d in dumps[:5]:
                mtime = d.stat().st_mtime
                import datetime
                dt = datetime.datetime.fromtimestamp(mtime).strftime("%Y-%m-%d %H:%M:%S")
                click.echo(f"  {dt}  {d.name}")
            if len(dumps) > 5:
                click.echo(f"  ... and {len(dumps) - 5} more")
