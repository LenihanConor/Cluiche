"""Stage Catalogue F2 (launch presets) E2E: a preset launch reaches the stage with the preset's settings applied.

Each test starts its own CoW process (the shared app_launcher always launches without --stage). Paths covered:
  1. `dia launch cow --stage S --preset P` (CLI -> exe args -> Application::SetInitialLaunch -> SimNavigationHandler)
  2. `stages.launch {stage, preset}` command from Boot (StageCatalogueModule -> StageLaunchRequest stream -> SimNavigationHandler)
Both then read cow.worldgen.status for the preset's seed and size.
"""
import shutil
import subprocess
import time

import pytest

from client import AutomationError, DiaClient

_STAGE = "S1WorldGenerationTestbedStage"
_PORT = 9002


def _launch(*extra):
    dia = shutil.which("dia")
    assert dia, "'dia' not on PATH"
    return subprocess.Popen([dia, "launch", "cow", *extra], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def _stop(proc, client):
    try:
        if client is not None:
            try:
                client.quit()
            except Exception:
                pass
            client.disconnect()
        proc.wait(timeout=10)
    except Exception:
        pass
    finally:
        if proc.poll() is None:
            proc.kill()
            proc.wait()


def _stage(c):
    return c.send_command("dia.app.report").get("stage")


def _wait_stage(c, name, timeout_s=60.0):
    deadline = time.time() + timeout_s
    last = None
    while time.time() < deadline:
        try:
            last = _stage(c)
            if last == name:
                return
        except AutomationError:
            pass
        time.sleep(0.2)
    raise AssertionError(f"stage {name!r} not reached (last: {last!r})")


def _wait_world(c, seed, timeout_s=60.0):
    deadline = time.time() + timeout_s
    status = None
    while time.time() < deadline:
        try:
            status = c.send_command("cow.worldgen.status")
            if status.get("state") == "Complete" and status.get("seed") == seed:
                return status
        except AutomationError:
            pass   # sim-hosted commands register once the module has started
        time.sleep(0.2)
    raise AssertionError(f"world with seed {seed} not complete: {status!r}")


@pytest.fixture
def app():
    state = {}

    def start(*extra):
        proc = _launch(*extra)
        client = DiaClient(port=_PORT)
        state["proc"], state["client"] = proc, client
        client.connect(timeout=90.0)
        return client

    yield start
    if "proc" in state:
        _stop(state["proc"], state.get("client"))


def test_cli_stage_and_preset_reach_stage_with_preset_applied(app):
    c = app("--stage", _STAGE, "--preset", "highlands42")
    _wait_stage(c, _STAGE)
    status = _wait_world(c, 42)
    assert status.get("size") == "Small", status


def test_stage_without_preset_still_launches(app):
    c = app("--stage", _STAGE)
    _wait_stage(c, _STAGE)


def test_stages_launch_from_boot_applies_preset(app):
    c = app()
    _wait_stage(c, "Boot")
    reply = c.send_command("stages.launch", {"stage": _STAGE, "preset": "archipelago7"})
    assert reply.get("ok") is True, reply
    _wait_stage(c, _STAGE)
    status = _wait_world(c, 7)
    assert status.get("size") == "Small", status


def test_stages_launch_unknown_preset_refused(app):
    c = app()
    _wait_stage(c, "Boot")
    try:
        reply = c.send_command("stages.launch", {"stage": _STAGE, "preset": "nope"})
    except AutomationError as e:
        reply = getattr(e, "payload", None) or {"error": str(e)}
    assert "error" in reply or reply.get("success") is False, reply
    assert _stage(c) == "Boot"
