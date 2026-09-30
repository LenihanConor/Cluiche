"""Tests for dia_console/results.py (console-typed-results).

The googletest adapter's parsing tests point ``results._GTEST_XML_PATH`` at a
tmp_path fixture (monkeypatched) instead of the real, fixed
``Cluiche/out/GoogleTests/last_run.xml`` -- this is the "known v1 limitation"
path described in the module docstring; tests exercise the parsing logic
without touching a real build. The one test that runs a real
``dia test googletest`` through the full stack against the real path is
marked ``integration`` (bottom of file).
"""
from __future__ import annotations

import pytest

from dia_console import results as results_module
from dia_console.execution import ExecutionHandle
from dia_console.results import (
    _generic_fallback_adapter,
    _googletest_adapter,
    _parse_file_line,
    build_results,
)


# ---------------------------------------------------------------------------
# Fakes
# ---------------------------------------------------------------------------

class _StubProcess:
    """Minimal stand-in for asyncio.subprocess.Process -- no real process."""

    def __init__(self, returncode=None, pid=999_999):
        self.returncode = returncode
        self.stdout = None
        self.pid = pid


def make_handle(tmp_path, *, returncode=0, log_lines=()):
    """A real ExecutionHandle against a stub process -- no real subprocess.

    Follows the same pattern as test_dia_console_execution.py's make_handle
    and test_dia_console_web_app.py's FakeExecutionService: the real
    ExecutionHandle class is used (never re-implemented), just with a stub
    process, so results.py's adapters run against the real handle API.
    """
    handle = ExecutionHandle(
        execution_id="exec1",
        process=_StubProcess(returncode=returncode),
        log_path=tmp_path / "exec1.ndjson",
        event_timeout_seconds=0.1,
        poll_interval_seconds=0.02,
    )
    handle._log_buffer.extend(log_lines)
    return handle


ALL_PASS_XML = """<?xml version="1.0" encoding="UTF-8"?>
<testsuites tests="2" failures="0" name="AllTests">
  <testsuite name="SuiteName" tests="2" failures="0">
    <testcase name="PassingTestOne" classname="SuiteName" time="0.001" status="run" result="COMPLETED" />
    <testcase name="PassingTestTwo" classname="SuiteName" time="0.003" status="run" result="COMPLETED" />
  </testsuite>
</testsuites>
"""

WITH_FAILURES_XML = """<?xml version="1.0" encoding="UTF-8"?>
<testsuites tests="2" failures="1" name="AllTests">
  <testsuite name="SuiteName" tests="2" failures="1">
    <testcase name="PassingTest" classname="SuiteName" time="0.001" status="run" result="COMPLETED" />
    <testcase name="FailingTest" classname="SuiteName" time="0.002" status="run" result="COMPLETED">
      <failure message="Cluiche/Tests/GoogleTests/Foo/Bar.cpp:88&#10;Expected sleep within 12 frames, got 41 frames." type="">
        <![CDATA[Cluiche/Tests/GoogleTests/Foo/Bar.cpp:88
Expected sleep within 12 frames, got 41 frames.]]>
      </failure>
    </testcase>
  </testsuite>
</testsuites>
"""

NO_LOCATION_FAILURE_XML = """<?xml version="1.0" encoding="UTF-8"?>
<testsuites tests="1" failures="1" name="AllTests">
  <testsuite name="SuiteName" tests="1" failures="1">
    <testcase name="FailingTest" classname="SuiteName" time="0.002" status="run" result="COMPLETED">
      <failure message="assertion failed, no location prefix here" type="" />
    </testcase>
  </testsuite>
</testsuites>
"""

MALFORMED_XML = "<testsuites><testsuite><testcase not-valid-xml"


# ---------------------------------------------------------------------------
# _parse_file_line
# ---------------------------------------------------------------------------

def test_parse_file_line_extracts_path_and_line():
    file_, line_ = _parse_file_line("Cluiche/Tests/GoogleTests/Foo/Bar.cpp:88\nExpected X, got Y.")
    assert file_ == "Cluiche/Tests/GoogleTests/Foo/Bar.cpp"
    assert line_ == 88


def test_parse_file_line_handles_trailing_colon():
    file_, line_ = _parse_file_line("some/path/File.cpp:42:\nmessage text")
    assert file_ == "some/path/File.cpp"
    assert line_ == 42


def test_parse_file_line_returns_none_none_when_no_convention_match():
    file_, line_ = _parse_file_line("assertion failed, no location prefix here")
    assert (file_, line_) == (None, None)


def test_parse_file_line_never_raises_on_empty_or_none():
    assert _parse_file_line("") == (None, None)
    assert _parse_file_line(None) == (None, None)


# ---------------------------------------------------------------------------
# _googletest_adapter -- all pass
# ---------------------------------------------------------------------------

def test_googletest_adapter_all_pass_produces_summary_with_no_severity(tmp_path, monkeypatch):
    xml_path = tmp_path / "last_run.xml"
    xml_path.write_text(ALL_PASS_XML, encoding="utf-8")
    monkeypatch.setattr(results_module, "_GTEST_XML_PATH", xml_path)

    handle = make_handle(tmp_path)
    records = _googletest_adapter(handle)

    summary = records[0]
    assert summary.kind == "TestResult"
    assert summary.severity is None
    assert summary.payload == {"passCount": 2, "failCount": 0, "totalCount": 2}


def test_googletest_adapter_all_pass_produces_artifact_record(tmp_path, monkeypatch):
    xml_path = tmp_path / "last_run.xml"
    xml_path.write_text(ALL_PASS_XML, encoding="utf-8")
    monkeypatch.setattr(results_module, "_GTEST_XML_PATH", xml_path)

    handle = make_handle(tmp_path)
    records = _googletest_adapter(handle)

    artifacts = [r for r in records if r.kind == "Artifact"]
    assert len(artifacts) == 1
    assert artifacts[0].title == "gtest-results.xml"
    assert artifacts[0].payload["path"]


# ---------------------------------------------------------------------------
# _googletest_adapter -- with failures
# ---------------------------------------------------------------------------

def test_googletest_adapter_with_failures_summary_is_error_severity(tmp_path, monkeypatch):
    xml_path = tmp_path / "last_run.xml"
    xml_path.write_text(WITH_FAILURES_XML, encoding="utf-8")
    monkeypatch.setattr(results_module, "_GTEST_XML_PATH", xml_path)

    handle = make_handle(tmp_path)
    records = _googletest_adapter(handle)

    summary = records[0]
    assert summary.severity == "error"
    assert summary.payload == {"passCount": 1, "failCount": 1, "totalCount": 2}


def test_googletest_adapter_with_failures_produces_one_record_per_failing_testcase(tmp_path, monkeypatch):
    xml_path = tmp_path / "last_run.xml"
    xml_path.write_text(WITH_FAILURES_XML, encoding="utf-8")
    monkeypatch.setattr(results_module, "_GTEST_XML_PATH", xml_path)

    handle = make_handle(tmp_path)
    records = _googletest_adapter(handle)

    failure_records = [r for r in records if r.kind == "TestResult" and "testcase" in r.payload]
    assert len(failure_records) == 1
    failure = failure_records[0]
    assert failure.title == "SuiteName.FailingTest"
    assert "Expected sleep within 12 frames" in failure.summary
    assert failure.payload["suite"] == "SuiteName"
    assert failure.payload["testcase"] == "FailingTest"
    assert failure.payload["durationSeconds"] == pytest.approx(0.002)
    assert failure.payload["file"] == "Cluiche/Tests/GoogleTests/Foo/Bar.cpp"
    assert failure.payload["line"] == 88


def test_googletest_adapter_failure_without_location_prefix_has_none_file_line(tmp_path, monkeypatch):
    xml_path = tmp_path / "last_run.xml"
    xml_path.write_text(NO_LOCATION_FAILURE_XML, encoding="utf-8")
    monkeypatch.setattr(results_module, "_GTEST_XML_PATH", xml_path)

    handle = make_handle(tmp_path)
    records = _googletest_adapter(handle)

    failure = next(r for r in records if r.kind == "TestResult" and "testcase" in r.payload)
    assert failure.payload["file"] is None
    assert failure.payload["line"] is None


# ---------------------------------------------------------------------------
# _googletest_adapter -- missing / malformed XML falls through to generic
# ---------------------------------------------------------------------------

def test_googletest_adapter_falls_back_to_generic_when_xml_missing(tmp_path, monkeypatch):
    monkeypatch.setattr(results_module, "_GTEST_XML_PATH", tmp_path / "does-not-exist.xml")

    handle = make_handle(tmp_path, returncode=1, log_lines=["a line"])
    records = _googletest_adapter(handle)

    assert len(records) == 1
    assert records[0].kind == "GenericResult"
    assert records[0].payload["returncode"] == 1


def test_googletest_adapter_falls_back_to_generic_when_xml_malformed(tmp_path, monkeypatch):
    xml_path = tmp_path / "last_run.xml"
    xml_path.write_text(MALFORMED_XML, encoding="utf-8")
    monkeypatch.setattr(results_module, "_GTEST_XML_PATH", xml_path)

    handle = make_handle(tmp_path, returncode=1)
    records = _googletest_adapter(handle)

    assert len(records) == 1
    assert records[0].kind == "GenericResult"


# ---------------------------------------------------------------------------
# _generic_fallback_adapter
# ---------------------------------------------------------------------------

def test_generic_fallback_reports_returncode_and_log_tail(tmp_path):
    handle = make_handle(tmp_path, returncode=1, log_lines=[f"line {i}" for i in range(30)])
    records = _generic_fallback_adapter(handle)

    assert len(records) == 1
    record = records[0]
    assert record.kind == "GenericResult"
    assert record.payload["returncode"] == 1
    assert record.payload["logTail"] == [f"line {i}" for i in range(10, 30)]
    assert len(record.payload["logTail"]) == 20


def test_generic_fallback_never_raises_with_no_output_at_all(tmp_path):
    handle = make_handle(tmp_path, returncode=None, log_lines=[])
    records = _generic_fallback_adapter(handle)

    assert len(records) == 1
    assert records[0].payload["returncode"] is None
    assert records[0].payload["logTail"] == []


# ---------------------------------------------------------------------------
# build_results -- dispatch
# ---------------------------------------------------------------------------

def test_build_results_dispatches_unregistered_command_id_to_generic_fallback(tmp_path):
    handle = make_handle(tmp_path, returncode=0, log_lines=["hello"])
    records = build_results("some.unregistered.command", handle)

    assert len(records) == 1
    assert records[0].kind == "GenericResult"


def test_build_results_dispatches_test_googletest_to_googletest_adapter(tmp_path, monkeypatch):
    xml_path = tmp_path / "last_run.xml"
    xml_path.write_text(ALL_PASS_XML, encoding="utf-8")
    monkeypatch.setattr(results_module, "_GTEST_XML_PATH", xml_path)

    handle = make_handle(tmp_path)
    records = build_results("test.googletest", handle)

    assert records[0].kind == "TestResult"


def test_build_results_never_raises_for_arbitrary_command_id_and_fresh_handle(tmp_path):
    handle = make_handle(tmp_path)
    # Should not raise for any random command_id, even with zero output.
    records = build_results("totally.made.up", handle)
    assert records


# ---------------------------------------------------------------------------
# Integration: a real `dia test googletest` run through build_results
# ---------------------------------------------------------------------------

@pytest.mark.integration
def test_real_googletest_run_produces_results_matching_actual_xml():
    """The one test in this feature that touches a real subprocess and a
    real build artifact -- confirms build_results("test.googletest", ...)
    reflects whatever `dia test googletest` actually produced at the real,
    fixed Cluiche/out/GoogleTests/last_run.xml path.
    """
    import shutil
    import subprocess
    import sys

    from dia_cli.utils.repo_root import find_repo_root

    repo_root = find_repo_root(__file__)
    executable = shutil.which("dia")
    argv = [executable] if executable else [sys.executable, "-m", "dia_cli.cli_main"]
    subprocess.run([*argv, "test", "googletest"], cwd=str(repo_root), check=False)

    xml_path = repo_root / "Cluiche" / "out" / "GoogleTests" / "last_run.xml"
    assert xml_path.exists(), "expected a real run to produce last_run.xml"

    class _RealRunStub:
        returncode = 0

        async def wait(self):
            return 0

    handle = ExecutionHandle(
        execution_id="real-exec",
        process=_RealRunStub(),
        log_path=repo_root / "Cluiche" / "out" / "DiaCLI" / "logs" / "console" / "real-exec.ndjson",
    )
    records = build_results("test.googletest", handle)
    assert any(r.kind == "TestResult" for r in records)
    assert any(r.kind == "Artifact" for r in records)
