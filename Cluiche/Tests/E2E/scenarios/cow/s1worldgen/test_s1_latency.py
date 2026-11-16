"""perf-live-control AC9: S-1 slider-to-visible latency.

Drives WorldGen setScale through debug.panel_command (the same path the panel slider uses), N times, and reads the
Sim-hosted perf.latency probe. Measured span: Sim receive -> applied on the sim tick -> first DomainStateSnapshot
published at or after that tick (the websocket hop and the Main-side read are not included; see the plan notes).
Target 100 ms p95; report always, fail only above 2x the target.
"""
import time

from client import AutomationError

_STAGE = "S1WorldGenerationTestbedStage"
_ACTION = "WorldGen.setScale"
_TARGET_P95_MS = 100.0
_FAIL_P95_MS = 2 * _TARGET_P95_MS
_N = 40


def _wait_complete(c, timeout_s=30.0):
    deadline = time.time() + timeout_s
    status = None
    while time.time() < deadline:
        try:
            status = c.send_command("cow.worldgen.status")
            if status.get("state") == "Complete":
                return status
        except AutomationError:
            pass   # sim-hosted commands register once the module has started
        time.sleep(0.2)
    raise AssertionError(f"worldgen did not reach Complete: {status!r}")


def _latency(c):
    return c.send_command("perf.latency", {"action": _ACTION})


def test_slider_to_visible_p95(dia_client):
    c = dia_client
    c.navigate_to(_STAGE, timeout_s=30.0)
    _wait_complete(c)

    done = 0
    base = None
    for i in range(_N):
        # Keep the snapshot publisher armed (a demand-driven snapshot is only published while someone reads).
        c.send_command("debug.domain_state", {"id": "WorldGen"})
        value = 0.45 + 0.1 * (i % 2)
        r = c.send_command("debug.panel_command",
                           {"domain": "WorldGen", "cmd": "setScale", "args": {"key": "landFraction", "value": value}})
        assert r.get("success") is True, r
        # Pace like a slider drag and let this probe complete before the next one.
        deadline = time.time() + 5.0
        while time.time() < deadline:
            st = _latency(c)["actions"].get(_ACTION, {})
            count = st.get("count", 0)
            if count > done:
                done = count
                break
            c.send_command("debug.domain_state", {"id": "WorldGen"})
            time.sleep(0.02)
        time.sleep(0.05)

    res = _latency(c)
    st = res["actions"][_ACTION]
    print(f"\n[AC9] {_ACTION}: count={st['count']} p50={st['p50Ms']:.1f} ms p95={st['p95Ms']:.1f} ms "
          f"min={st['minMs']:.1f} max={st['maxMs']:.1f} (target p95 {_TARGET_P95_MS:.0f} ms, fail > {_FAIL_P95_MS:.0f} ms) "
          f"counters={res['counters']} scope={res.get('scope')}")
    assert st["count"] >= _N // 2, st   # most probes must complete (ring holds the last 40)
    assert st["p95Ms"] <= _FAIL_P95_MS, f"p95 {st['p95Ms']:.1f} ms exceeds 2x target {_FAIL_P95_MS:.0f} ms"
    if st["p95Ms"] > _TARGET_P95_MS:
        print(f"[AC9] WARNING: p95 {st['p95Ms']:.1f} ms is above the {_TARGET_P95_MS:.0f} ms target")

    # Restore a sane value for later tests sharing the stage.
    c.send_command("debug.panel_command",
                   {"domain": "WorldGen", "cmd": "setScale", "args": {"key": "landFraction", "value": 0.5}})
