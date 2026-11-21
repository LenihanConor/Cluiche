"""CoW S-1 launch smoke: enter the world-generation testbed, wait for generation to finish, check the result is sane.

Replaces the throwaway launch check. Deliberately minimal: the panel, render and latency scenarios cover behaviour;
this one only proves "the stage launches and world generation completes" so a CoW regression is named by one test.
"""
import time

from client import AutomationError

_STAGE = "S1WorldGenerationTestbedStage"
_GENERATION_TIMEOUT_S = 60.0


def _wait_complete(c):
    deadline = time.time() + _GENERATION_TIMEOUT_S
    status = None
    while time.time() < deadline:
        try:
            status = c.send_command("cow.worldgen.status")
            if status.get("state") in ("Complete", "Failed"):
                return status
        except AutomationError:
            pass   # sim-hosted commands register once the module has started
        time.sleep(0.2)
    raise AssertionError(f"world generation did not finish within {_GENERATION_TIMEOUT_S}s (last status={status!r:.300})")


def test_s1_stage_launches_and_world_generation_completes(dia_client):
    c = dia_client
    c.navigate_to(_STAGE, timeout_s=30.0)

    status = _wait_complete(c)
    assert status["state"] == "Complete", status
    assert status["revision"] >= 1, status

    params = c.send_command("cow.worldgen.params")
    assert "error" not in params, params

    c.abort_stage()
    c.navigate_to("Boot", timeout_s=30.0)
