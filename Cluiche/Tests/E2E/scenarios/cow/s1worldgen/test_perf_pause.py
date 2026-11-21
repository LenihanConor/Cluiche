"""perf-live-control E2E: AC1 (module pause), AC2 (domain pause), AC4 (pause released on client disconnect).

Module id is the CoW manifest instance id "WorldGenerationModule"; domain id is "WorldGen" (WorldGenDebugDomain).
A paused WorldGenerationModule skips DoUpdate, so a queued Regenerate never completes and `revision` stays put; the
sim-hosted commands (answered by SimQueryHost) keep working. Pauses are owned by the pausing connection.
"""
import time

import pytest

from client import AutomationError, DiaClient

_STAGE = "S1WorldGenerationTestbedStage"
_MODULE = "WorldGenerationModule"
_DOMAIN = "WorldGen"


def _status(c):
    return c.send_command("cow.worldgen.status")


def _wait_complete(c, timeout_s=30.0, min_revision=None):
    deadline = time.time() + timeout_s
    st = None
    while time.time() < deadline:
        try:
            st = _status(c)
            if st.get("state") == "Complete" and not st.get("generating") \
                    and (min_revision is None or st.get("revision", 0) >= min_revision):
                return st
        except AutomationError:
            pass
        time.sleep(0.1)
    raise AssertionError(f"worldgen did not reach Complete (min_revision={min_revision}): {st!r}")


def _pause(c, kind, ident):
    r = c.send_command("perf.pause", {"kind": kind, "id": ident})
    assert r.get("paused") is True, r
    return r


def _regenerate(c):
    return c.send_command("cow.worldgen.regenerate", {})


def _revision_after(c, seconds):
    time.sleep(seconds)
    return _status(c)["revision"]


def _domain_present(c):
    try:
        r = c.send_command("debug.domain_state", {"id": _DOMAIN})
    except AutomationError:
        return False
    return "error" not in r


def _wait_domain(c, present, timeout_s=8.0):
    deadline = time.time() + timeout_s
    while time.time() < deadline:
        if _domain_present(c) == present:
            return True
        time.sleep(0.1)
    return False


@pytest.fixture
def stage_ready(dia_client):
    c = dia_client
    c.navigate_to(_STAGE, timeout_s=30.0)
    _wait_complete(c)
    yield c
    try:
        c.send_command("perf.resume_all")
    except Exception:
        pass


def test_ac1_module_pause_stops_update(stage_ready):
    c = stage_ready
    base = _status(c)["revision"]

    _pause(c, "module", _MODULE)
    _regenerate(c)                     # accepted by the sim-hosted handler, but generation runs in DoUpdate
    rev_paused = _revision_after(c, 1.5)
    print(f"\n[AC1] base={base} revision while paused (1.5 s)={rev_paused}")
    assert rev_paused == base, "revision rose while WorldGenerationModule was paused"

    r = c.send_command("perf.resume", {"kind": "module", "id": _MODULE})
    assert r.get("paused") is False, r
    st = _wait_complete(c, min_revision=base + 1)
    print(f"[AC1] revision after resume={st['revision']}")
    assert st["revision"] > base


def test_ac2_domain_pause_removes_domain_state(stage_ready):
    c = stage_ready
    assert _wait_domain(c, True), "WorldGen domain not present before pause"

    _pause(c, "domain", _DOMAIN)
    gone = _wait_domain(c, False)
    print(f"\n[AC2] domain absent from debug.domain_state while paused: {gone}")
    assert gone, "WorldGen still present in debug.domain_state while paused"

    c.send_command("perf.resume", {"kind": "domain", "id": _DOMAIN})
    back = _wait_domain(c, True)
    print(f"[AC2] domain back after resume: {back}")
    assert back


def test_ac4_disconnect_releases_pause(stage_ready):
    c1 = stage_ready
    base = _status(c1)["revision"]

    c2 = DiaClient(port=c1.port)
    c2.connect(timeout=10.0)
    try:
        _pause(c2, "module", _MODULE)
        _regenerate(c1)
        assert _revision_after(c1, 1.0) == base, "module not paused by second client"
    finally:
        c2.disconnect()                # closes the websocket: the server's disconnect observer releases c2's pauses

    st = _wait_complete(c1, timeout_s=15.0, min_revision=base + 1)
    print(f"\n[AC4] revision {base} -> {st['revision']} after paused client disconnected")
    assert st["revision"] > base
