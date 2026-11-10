"""Adapters that turn a completed execution's output into typed ``ResultRecord``s.

``build_results`` is the single entry point (SD-CONSOLE-005: only this module
and ``dia_console/web/app.py`` know it exists -- ``app.js`` only ever calls
``GET /api/executions/{id}/results``). Results are computed fresh on every
call, consistent with the registry's own no-cache rule (SD-CONSOLE-010) --
re-parsing a small XML file per request is negligible cost.

Exactly two adapters exist in v1:

* ``test.googletest`` -- parses the gtest XML report at a fixed path
  (:data:`_GTEST_XML_PATH`). **Known v1 limitation, not a bug:** this path is
  shared across all executions, not per-execution like the NDJSON log, so two
  concurrent ``test.googletest`` runs would race on it. This is an accepted
  limitation (see the feature spec's Goal 3), not something to fix here.
* the generic fallback -- used for every other ``command_id``, reporting the
  process's exit code and a tail of its merged stdout/stderr.
"""
from __future__ import annotations

import re
import xml.etree.ElementTree as ET
from pathlib import Path
from typing import Callable

from dia_cli.utils.repo_root import find_repo_root
from dia_console.execution import ExecutionHandle
from dia_console.model import ResultRecord

_REPO_ROOT = find_repo_root(__file__)

#: Fixed path googletest_runner.py writes its XML report to. Not
#: per-execution -- see the module docstring's "Known v1 limitation".
_GTEST_XML_PATH: Path = _REPO_ROOT / "Cluiche" / "out" / "GoogleTests" / "last_run.xml"

#: gtest's own convention for prefixing a failure message with the source
#: location: the first line is ``<path>:<line>`` (optionally followed by a
#: trailing colon). Defensive: anything that doesn't match yields (None, None)
#: rather than raising.
_FILE_LINE_RE = re.compile(r"^(?P<file>.+):(?P<line>\d+):?\s*$")

#: Number of trailing log lines the generic fallback reports.
_DEFAULT_LOG_TAIL_LINES = 20

ResultAdapter = Callable[[ExecutionHandle], list[ResultRecord]]


def _parse_file_line(message: str | None) -> tuple[str | None, int | None]:
    """Extract ``(file, line)`` from a gtest failure message's first line.

    Never raises: any message that doesn't match the ``file:line`` prefix
    convention (or is empty/``None``) yields ``(None, None)``.
    """
    if not message:
        return None, None
    first_line = message.strip().splitlines()[0].strip() if message.strip() else ""
    match = _FILE_LINE_RE.match(first_line)
    if not match:
        return None, None
    try:
        return match.group("file"), int(match.group("line"))
    except (TypeError, ValueError):  # pragma: no cover - defensive
        return None, None


def _artifact_path_str(path: Path) -> str:
    """Repo-relative path string if ``path`` is under the repo root, else absolute."""
    try:
        return str(path.relative_to(_REPO_ROOT))
    except ValueError:
        return str(path)


def _failure_message(failure_el: ET.Element) -> str:
    message = failure_el.get("message")
    if message:
        return message
    return (failure_el.text or "").strip()


def _parse_gtest_xml_file(xml_path: Path) -> list[ResultRecord] | None:
    """Parse a gtest XML report into ``ResultRecord``s, or ``None`` if unusable.

    ``None`` (rather than raising) signals the caller to fall through to the
    generic fallback adapter -- covers both "file doesn't exist" and "file
    exists but isn't valid gtest XML".
    """
    if not xml_path.exists():
        return None
    try:
        tree = ET.parse(str(xml_path))
    except ET.ParseError:
        return None

    root = tree.getroot()
    pass_count = 0
    failure_records: list[ResultRecord] = []

    for testsuite in root.iter("testsuite"):
        suite_name = testsuite.get("name", "")
        for testcase in testsuite.iter("testcase"):
            case_name = testcase.get("name", "")
            failure_el = testcase.find("failure")
            if failure_el is None:
                pass_count += 1
                continue
            message = _failure_message(failure_el)
            file_, line_ = _parse_file_line(message)
            try:
                duration = float(testcase.get("time", "0"))
            except (TypeError, ValueError):
                duration = 0.0
            failure_records.append(
                ResultRecord(
                    kind="TestResult",
                    severity="error",
                    title=f"{suite_name}.{case_name}",
                    summary=message or None,
                    payload={
                        "suite": suite_name,
                        "testcase": case_name,
                        "durationSeconds": duration,
                        "file": file_,
                        "line": line_,
                    },
                )
            )

    fail_count = len(failure_records)
    total_count = pass_count + fail_count

    records: list[ResultRecord] = [
        ResultRecord(
            kind="TestResult",
            severity=None if fail_count == 0 else "error",
            title=f"{pass_count}/{total_count} tests passed" if total_count else "No tests ran",
            summary=None,
            payload={"passCount": pass_count, "failCount": fail_count, "totalCount": total_count},
        ),
        *failure_records,
        ResultRecord(
            kind="Artifact",
            severity=None,
            title="gtest-results.xml",
            summary=None,
            payload={"path": _artifact_path_str(xml_path)},
        ),
    ]
    return records


def _generic_fallback_adapter(handle: ExecutionHandle) -> list[ResultRecord]:
    """Every command without a specific adapter: exit code + tail of merged log.

    Never raises -- a command with no output at all still gets a
    ``GenericResult`` with a ``returncode`` and an empty ``logTail``.
    """
    return [
        ResultRecord(
            kind="GenericResult",
            severity=None,
            title="Execution finished",
            summary=None,
            payload={
                "returncode": handle.returncode,
                "logTail": handle.log_tail(_DEFAULT_LOG_TAIL_LINES),
            },
        )
    ]


def _googletest_adapter(handle: ExecutionHandle) -> list[ResultRecord]:
    """Parse the fixed gtest XML report; fall through to the generic fallback
    if it doesn't exist or fails to parse (a crashed run still needs *some*
    result -- see the feature spec's Task 2 note).
    """
    records = _parse_gtest_xml_file(_GTEST_XML_PATH)
    if records is None:
        return _generic_fallback_adapter(handle)
    return records


_ADAPTERS: dict[str, ResultAdapter] = {
    "test.googletest": _googletest_adapter,
}


def build_results(command_id: str, handle: ExecutionHandle) -> list[ResultRecord]:
    """Turn ``handle``'s output into typed results, dispatched by ``command_id``.

    Computed fresh every call -- no caching (SD-CONSOLE-010). Falls back to
    the generic adapter for any ``command_id`` without a specific one, so no
    execution is ever left with only raw log text (Goal 7).
    """
    adapter = _ADAPTERS.get(command_id, _generic_fallback_adapter)
    return adapter(handle)
