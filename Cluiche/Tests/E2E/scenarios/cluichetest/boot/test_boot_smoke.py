"""Smoke scenario: CluicheTest boots, navigates stages, and exits cleanly.

Spec: docs/specs/features/cluichetest/cluichetestscenarios/smoke-scenario.md
"""
import time


def test_smoke_journey(dia_client):
    """Boot -> navigate to DummyStage -> hold stable -> quit via fixture teardown."""
    # AC2: app is in Boot stage
    report = dia_client.report()
    assert report["stage"] == "Boot"

    # AC3: navigate to DummyStage
    dia_client.navigate_to("DummyStage")
    report = dia_client.report()
    assert report["stage"] == "DummyStage"

    # AC4: hold stable for 3 seconds — no crash, still in DummyStage
    time.sleep(3.0)
    report = dia_client.report()
    assert report["stage"] == "DummyStage"

    # AC5 / AC6 handled by fixture teardown (quit + assert_no_log_errors)
