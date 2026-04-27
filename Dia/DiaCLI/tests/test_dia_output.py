"""Unit tests for cli-output feature (dia_cli/utils/dia_output.py).

Each test maps to a numbered acceptance criterion from the spec.
"""
import json
import time
from pathlib import Path

import pytest
from click.testing import CliRunner

from dia_cli.utils.dia_output import OutputContext
from dia_cli.cli_main import cli


# ---------------------------------------------------------------------------
# Fixtures
# ---------------------------------------------------------------------------

@pytest.fixture
def log_dir(tmp_path):
    return tmp_path / "logs"


@pytest.fixture
def ctx(log_dir):
    return OutputContext(log_dir=log_dir)


@pytest.fixture
def ndjson_lines(log_dir, ctx):
    """Run a minimal successful pipeline and return parsed NDJSON lines."""
    ctx.run_started(system="pipeline", target="googletest", config="Debug", stages=["compile-code"])
    ctx.stage_started(system="pipeline", stage="compile-code")
    ctx.step_started(system="pipeline", stage="compile-code", step="msbuild", detail="GoogleTests.vcxproj")
    ctx.step_completed(system="pipeline", stage="compile-code", step="msbuild")
    ctx.stage_completed(system="pipeline", stage="compile-code")
    ctx.run_completed(system="pipeline", pass_count=1, fail_count=0)

    log_path = log_dir / "pipeline" / "last-run.ndjson"
    return [json.loads(line) for line in log_path.read_text(encoding="utf-8").strip().splitlines()]


# ---------------------------------------------------------------------------
# AC2: log file created/overwritten at run start
# ---------------------------------------------------------------------------

def test_log_file_created_on_run_started(log_dir, ctx):
    """AC2: last-run.ndjson is created when run_started is called."""
    ctx.run_started(system="pipeline", target="gt", config="Debug", stages=[])
    ctx.run_completed(system="pipeline", pass_count=0, fail_count=0)
    assert (log_dir / "pipeline" / "last-run.ndjson").exists()


def test_log_file_overwritten_on_new_run(log_dir):
    """AC2: second run overwrites the file from the first run."""
    ctx1 = OutputContext(log_dir=log_dir)
    ctx1.run_started(system="pipeline", target="first", config="Debug", stages=[])
    ctx1.run_completed(system="pipeline", pass_count=0, fail_count=0)

    ctx2 = OutputContext(log_dir=log_dir)
    ctx2.run_started(system="pipeline", target="second", config="Debug", stages=[])
    ctx2.run_completed(system="pipeline", pass_count=0, fail_count=0)

    log_path = log_dir / "pipeline" / "last-run.ndjson"
    lines = [json.loads(l) for l in log_path.read_text(encoding="utf-8").strip().splitlines()]
    # Only one run's events — "first" target must not appear
    targets = [l.get("target") for l in lines if "target" in l]
    assert "first" not in targets
    assert "second" in targets


# ---------------------------------------------------------------------------
# AC3: events written immediately (not buffered)
# ---------------------------------------------------------------------------

def test_events_written_immediately(log_dir):
    """AC3: each event is flushed to the file as it fires."""
    ctx = OutputContext(log_dir=log_dir)
    log_path = log_dir / "pipeline" / "last-run.ndjson"

    ctx.run_started(system="pipeline", target="gt", config="Debug", stages=["compile-code"])
    # File must exist and have content before run_completed is called
    assert log_path.exists()
    content_mid_run = log_path.read_text(encoding="utf-8")
    assert "OnRunStarted" in content_mid_run

    ctx.stage_started(system="pipeline", stage="compile-code")
    content_after_stage = log_path.read_text(encoding="utf-8")
    assert "OnStageStarted" in content_after_stage

    ctx.stage_completed(system="pipeline", stage="compile-code")
    ctx.run_completed(system="pipeline", pass_count=1, fail_count=0)


# ---------------------------------------------------------------------------
# AC4: OnRunStarted first, OnRunCompleted/Failed last
# ---------------------------------------------------------------------------

def test_run_started_is_first_event(ndjson_lines):
    """AC4: OnRunStarted is always the first line."""
    assert ndjson_lines[0]["event"] == "OnRunStarted"


def test_run_completed_is_last_event(ndjson_lines):
    """AC4: OnRunCompleted is always the last line."""
    assert ndjson_lines[-1]["event"] == "OnRunCompleted"


def test_run_failed_is_last_event(log_dir):
    """AC4: OnRunFailed is the last line when a stage fails."""
    ctx = OutputContext(log_dir=log_dir)
    ctx.run_started(system="pipeline", target="gt", config="Debug", stages=["compile-code"])
    ctx.stage_started(system="pipeline", stage="compile-code")
    ctx.stage_failed(system="pipeline", stage="compile-code", error="msbuild exited 1")
    ctx.run_failed(system="pipeline", pass_count=0, fail_count=1)

    log_path = log_dir / "pipeline" / "last-run.ndjson"
    lines = [json.loads(l) for l in log_path.read_text(encoding="utf-8").strip().splitlines()]
    assert lines[-1]["event"] == "OnRunFailed"


# ---------------------------------------------------------------------------
# AC5: every Started has a matching Completed or Failed
# ---------------------------------------------------------------------------

def test_every_started_has_matching_completed(ndjson_lines):
    """AC5: OnXStarted events each have a matching OnXCompleted or OnXFailed."""
    started = {e["event"] for e in ndjson_lines if e["event"].endswith("Started")}
    completed_or_failed = {e["event"].replace("Completed", "Started").replace("Failed", "Started")
                           for e in ndjson_lines
                           if e["event"].endswith("Completed") or e["event"].endswith("Failed")}
    assert started == completed_or_failed


# ---------------------------------------------------------------------------
# AC6: --no-color produces plain text (no ANSI escape codes)
# ---------------------------------------------------------------------------

def test_no_color_flag_no_ansi(log_dir, capsys):
    """AC6: --no-color output contains no ANSI escape codes."""
    ctx = OutputContext(log_dir=log_dir, no_color=True)
    ctx.run_started(system="pipeline", target="gt", config="Debug", stages=[])
    ctx.run_completed(system="pipeline", pass_count=0, fail_count=0)
    captured = capsys.readouterr()
    assert "\x1b[" not in captured.out
    assert "\x1b[" not in captured.err


# ---------------------------------------------------------------------------
# AC7: --quiet suppresses terminal output but still writes NDJSON
# ---------------------------------------------------------------------------

def test_quiet_suppresses_terminal_output(log_dir, capsys):
    """AC7: --quiet produces no terminal output."""
    ctx = OutputContext(log_dir=log_dir, quiet=True)
    ctx.run_started(system="pipeline", target="gt", config="Debug", stages=[])
    ctx.stage_started(system="pipeline", stage="compile-code")
    ctx.stage_completed(system="pipeline", stage="compile-code")
    ctx.run_completed(system="pipeline", pass_count=1, fail_count=0)
    captured = capsys.readouterr()
    assert captured.out == ""
    assert captured.err == ""


def test_quiet_still_writes_ndjson(log_dir):
    """AC7: --quiet still writes the NDJSON log."""
    ctx = OutputContext(log_dir=log_dir, quiet=True)
    ctx.run_started(system="pipeline", target="gt", config="Debug", stages=[])
    ctx.run_completed(system="pipeline", pass_count=0, fail_count=0)
    log_path = log_dir / "pipeline" / "last-run.ndjson"
    assert log_path.exists()
    lines = [json.loads(l) for l in log_path.read_text(encoding="utf-8").strip().splitlines()]
    assert len(lines) == 2  # OnRunStarted + OnRunCompleted


# ---------------------------------------------------------------------------
# AC8: --log-json override writes to specified path
# ---------------------------------------------------------------------------

def test_log_json_override(log_dir, tmp_path):
    """AC8: --log-json writes to the specified path, not the default."""
    override_path = tmp_path / "custom" / "my-run.ndjson"
    ctx = OutputContext(log_dir=log_dir, log_json_override=override_path)
    ctx.run_started(system="pipeline", target="gt", config="Debug", stages=[])
    ctx.run_completed(system="pipeline", pass_count=0, fail_count=0)

    assert override_path.exists()
    # Default path must NOT be created
    assert not (log_dir / "pipeline" / "last-run.ndjson").exists()


# ---------------------------------------------------------------------------
# NDJSON schema correctness
# ---------------------------------------------------------------------------

def test_schema_version_in_run_started(ndjson_lines):
    """OnRunStarted must include schema: dia.output.v1."""
    run_started = next(e for e in ndjson_lines if e["event"] == "OnRunStarted")
    assert run_started["schema"] == "dia.output.v1"


def test_all_events_have_ts(ndjson_lines):
    """Every event must have a ts (Unix timestamp float) field."""
    for event in ndjson_lines:
        assert "ts" in event
        assert isinstance(event["ts"], float)


def test_completed_events_have_duration_ms(ndjson_lines):
    """Completed and Failed events must have durationMs."""
    for event in ndjson_lines:
        if event["event"].endswith("Completed") or event["event"].endswith("Failed"):
            assert "durationMs" in event, f"{event['event']} missing durationMs"
            assert isinstance(event["durationMs"], int)


def test_duration_ms_is_non_negative(log_dir):
    """durationMs must be >= 0."""
    ctx = OutputContext(log_dir=log_dir)
    ctx.run_started(system="pipeline", target="gt", config="Debug", stages=["s"])
    ctx.stage_started(system="pipeline", stage="s")
    time.sleep(0.01)
    ctx.stage_completed(system="pipeline", stage="s")
    ctx.run_completed(system="pipeline", pass_count=1, fail_count=0)

    log_path = log_dir / "pipeline" / "last-run.ndjson"
    lines = [json.loads(l) for l in log_path.read_text(encoding="utf-8").strip().splitlines()]
    for event in lines:
        if "durationMs" in event:
            assert event["durationMs"] >= 0


def test_stage_failed_includes_error_field(log_dir):
    """OnStageFailed must include the error field."""
    ctx = OutputContext(log_dir=log_dir)
    ctx.run_started(system="pipeline", target="gt", config="Debug", stages=["s"])
    ctx.stage_started(system="pipeline", stage="s")
    ctx.stage_failed(system="pipeline", stage="s", error="something broke")
    ctx.run_failed(system="pipeline", pass_count=0, fail_count=1)

    log_path = log_dir / "pipeline" / "last-run.ndjson"
    lines = [json.loads(l) for l in log_path.read_text(encoding="utf-8").strip().splitlines()]
    failed = next(e for e in lines if e["event"] == "OnStageFailed")
    assert failed["error"] == "something broke"


def test_log_line_event_fields(log_dir):
    """OnLogLine must include system, level, and message."""
    ctx = OutputContext(log_dir=log_dir)
    ctx.run_started(system="env", target=None, config=None, stages=[])
    ctx.log(system="env", level="warn", message="SHA-256 mismatch", stage="deps")
    ctx.run_completed(system="env", pass_count=0, fail_count=0)

    log_path = log_dir / "env" / "last-run.ndjson"
    lines = [json.loads(l) for l in log_path.read_text(encoding="utf-8").strip().splitlines()]
    log_event = next(e for e in lines if e["event"] == "OnLogLine")
    assert log_event["level"] == "warn"
    assert log_event["message"] == "SHA-256 mismatch"
    assert log_event["stage"] == "deps"


def test_per_system_log_paths(log_dir):
    """Each system writes to its own subdirectory."""
    for system in ("pipeline", "env", "test"):
        ctx = OutputContext(log_dir=log_dir)
        ctx.run_started(system=system, target=None, config=None, stages=[])
        ctx.run_completed(system=system, pass_count=0, fail_count=0)
        assert (log_dir / system / "last-run.ndjson").exists()


# ---------------------------------------------------------------------------
# AC10: interrupted run leaves unmatched OnStageStarted
# ---------------------------------------------------------------------------

def test_interrupted_run_leaves_unmatched_started(log_dir):
    """AC10: crash mid-run leaves an unmatched OnStageStarted in the log."""
    ctx = OutputContext(log_dir=log_dir)
    ctx.run_started(system="pipeline", target="gt", config="Debug", stages=["compile-code"])
    ctx.stage_started(system="pipeline", stage="compile-code")
    # Simulate crash: close log without completing
    ctx._close_log()

    log_path = log_dir / "pipeline" / "last-run.ndjson"
    lines = [json.loads(l) for l in log_path.read_text(encoding="utf-8").strip().splitlines()]

    started = [e for e in lines if e["event"] == "OnStageStarted"]
    completed_or_failed = [e for e in lines if e["event"] in ("OnStageCompleted", "OnStageFailed")]
    assert len(started) > len(completed_or_failed)


# ---------------------------------------------------------------------------
# Global flags wired into CLI
# ---------------------------------------------------------------------------

def test_cli_help_shows_global_flags():
    """--no-color, --quiet, --log-json appear in dia --help."""
    runner = CliRunner()
    result = runner.invoke(cli, ["--help"])
    assert result.exit_code == 0
    assert "--no-color" in result.output
    assert "--quiet" in result.output
    assert "--log-json" in result.output
