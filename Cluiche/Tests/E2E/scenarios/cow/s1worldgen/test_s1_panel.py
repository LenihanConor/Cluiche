"""S-1 World Generation card: the SimPU WorldGenDebugDomain reaches Main through DomainStateRelay/Bridge.

debug.domain_state (domain "WorldGen") is a demand-driven snapshot published by the SimPU, so a read can be pending or stale;
wait for a snapshot a few Sim frames newer than the first read, as the celllayers smoke does.
"""
import time

from client import AutomationError

_STAGE = "S1WorldGenerationTestbedStage"


def _raw(c):
    return c.send_command("debug.domain_state", {"id": "WorldGen"})


def _panel(c, timeout_s=15.0, min_revision=None):
    first = _raw(c)
    base = first.get("simFrame") if not first.get("pending") else None
    deadline = time.time() + timeout_s
    st = first
    while time.time() < deadline:
        st = _raw(c)
        if st.get("pending") or "simFrame" not in st:
            time.sleep(0.05)
            continue
        fresh = base is None or st["simFrame"] >= base + 3
        if fresh and (min_revision is None or st["stats"]["revision"] >= min_revision):
            return st
        time.sleep(0.05)
    raise AssertionError(f"no fresh WorldGen snapshot within {timeout_s}s (last={st!r:.300})")


def test_card_state_and_set_scale(dia_client):
    c = dia_client
    c.navigate_to(_STAGE, timeout_s=30.0)

    deadline = time.time() + 30.0
    status = None
    while time.time() < deadline:
        try:
            status = c.send_command("cow.worldgen.status")
            if status.get("state") == "Complete":
                break
        except AutomationError:
            pass   # sim-hosted commands register once the module has started
        time.sleep(0.2)
    assert status and status.get("state") == "Complete", status

    st = _panel(c)
    assert len(st["controls"]) >= 19, len(st["controls"])
    assert st["stats"]["revision"] >= 1
    rev0 = st["stats"]["revision"]
    land = next(ctl for ctl in st["controls"] if ctl["key"] == "landFraction")
    new_value = 0.5 if abs(land["value"] - 0.5) > 0.01 else 0.4

    # No DebugServer command exists for panel commands (they travel DebugPanelCommand -> adapter.OnCommand, covered by the
    # WorldGenDomainStateRelay gtest), so drive the same session through cow.worldgen.set and check the card follows it.
    r = c.send_command("cow.worldgen.set", {"landFraction": new_value})
    assert "error" not in r, r

    deadline = time.time() + 15.0
    while time.time() < deadline:
        status = c.send_command("cow.worldgen.status")
        if status.get("revision", 0) > rev0 and status.get("state") == "Complete":
            break
        time.sleep(0.2)
    assert status["revision"] > rev0, (rev0, status)

    st2 = _panel(c, min_revision=rev0 + 1)
    land2 = next(ctl for ctl in st2["controls"] if ctl["key"] == "landFraction")
    assert abs(land2["value"] - new_value) < 1e-3, (land2, new_value)
