"""Smoke scenario: app boots and responds to basic commands.

Item #5 — CluicheTest smoke scenario (spec: docs/specs/features/cluichetest/cluichetestscenarios/smoke-scenario.md).
"""


def test_app_boots_and_reports(dia_client):
    result = dia_client.report()
    assert "stage" in result
    assert len(result.get("modules", [])) > 0


def test_app_quit(dia_client):
    dia_client.quit()
