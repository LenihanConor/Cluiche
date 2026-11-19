"""StageCatalogue F1 E2E: a stage run persists its PASS to out/CluicheTest/stage_results.json and
the live `stages.results` / `stages.list` queries report it.

Spec: docs/specs/applications/dia/systems/stagecatalogue/catalogue-and-results.md (AC 11, 13, 21).
"""
import json
import time
from pathlib import Path

_STAGE = "MessageBusTestStage"
_RESULTS = Path(__file__).resolve().parents[5] / "out" / "CluicheTest" / "stage_results.json"


def _results(c):
    return c.send_command("stages.results")


def _wait_label(c, label, timeout_s):
    deadline = time.time() + timeout_s
    entry = {}
    while time.time() < deadline:
        entry = _results(c).get(_STAGE, {})
        if entry.get("label") == label:
            return entry
        time.sleep(0.5)
    raise AssertionError(f"{_STAGE} never reached {label}; last={entry}")


def test_stage_run_persists_pass_and_answers_query(dia_client):
    c = dia_client
    c.navigate_to(_STAGE)
    try:
        entry = _wait_label(c, "PASS", 40.0)
        assert entry["severity"] == "ok", entry
    finally:
        c.abort_stage()
        c.navigate_to("Boot")

    # File written (at most once per frame) with the same PASS entry.
    deadline = time.time() + 5.0
    while time.time() < deadline and not _RESULTS.exists():
        time.sleep(0.2)
    assert _RESULTS.exists(), _RESULTS
    body = json.loads(_RESULTS.read_text(encoding="utf-8"))
    assert body["format"] == "dia.stage_results" and body["version"] == 1, body
    assert body["results"][_STAGE]["label"] == "PASS", body

    # The catalogue query answers too (live session shows the same last_result).
    got = c.send_command("stages.get", {"stage": _STAGE})
    assert got.get("name") == _STAGE and got["last_result"]["label"] == "PASS", got


def test_second_launch_reports_persisted_result(dia_client):
    """Run after test_stage_run_persists_pass_and_answers_query (earlier launch): the persisted PASS is loaded at start.

    Skips when no earlier run has written the file, so it is order-safe in a fresh checkout.
    """
    import pytest
    if not _RESULTS.exists():
        pytest.skip("no stage_results.json yet; first-launch test creates it")
    res = _results(dia_client)
    assert res.get(_STAGE, {}).get("label") == "PASS", res
