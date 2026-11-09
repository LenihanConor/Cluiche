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


def _wait_complete(c, timeout_s=30.0, min_revision=None):
    deadline = time.time() + timeout_s
    status = None
    while time.time() < deadline:
        try:
            status = c.send_command("cow.worldgen.status")
            if status.get("state") == "Complete" and (min_revision is None or status.get("revision", 0) >= min_revision):
                return status
        except AutomationError:
            pass   # sim-hosted commands register once the module has started
        time.sleep(0.2)
    raise AssertionError(f"worldgen did not reach Complete (min_revision={min_revision}): {status!r}")


def _panel_cmd(c, cmd, args=None):
    payload = {"domain": "WorldGen", "cmd": cmd}
    if args is not None:
        payload["args"] = args
    r = c.send_command("debug.panel_command", payload)
    assert r.get("success") is True and r.get("queued") is True, r
    return r


def _control(st, key):
    return next(ctl for ctl in st["controls"] if ctl["key"] == key)


def _domain_ids(c):
    """Ids of the debug domains currently in the Main-side snapshot (debug.domain_state with no id)."""
    root = c.send_command("debug.domain_state")
    return {d.get("_id") for d in root.values() if isinstance(d, dict)}


def _wait_domain_gone(c, domain_id, timeout_s=15.0):
    deadline = time.time() + timeout_s
    ids = set()
    while time.time() < deadline:
        ids = _domain_ids(c)
        if domain_id not in ids:
            return
        time.sleep(0.2)
    raise AssertionError(f"domain {domain_id!r} still listed after leaving the stage: {sorted(i for i in ids if i)}")


def test_stage_round_trips_tear_down_and_rebuild_the_card(dia_client, request):
    from plugin import _collect_log_errors, _count_log_lines

    c = dia_client
    plan = getattr(request.config, "_dia_plan", None)
    hwm = _count_log_lines(request.config, plan) if plan else 0

    for trip in range(3):
        c.navigate_to(_STAGE, timeout_s=30.0)
        _wait_complete(c)
        st = _panel(c)
        assert len(st["controls"]) >= 19, (trip, len(st["controls"]))
        assert st["stats"]["revision"] >= 1, trip
        assert "WorldGen" in _domain_ids(c), trip

        c.navigate_to("Boot", timeout_s=30.0)
        _wait_domain_gone(c, "WorldGen")

    # The app survived all six transitions and answers.
    assert c.send_command("dia.app.report", timeout=5.0).get("stage") == "Boot"

    if plan:
        errors = _collect_log_errors(request.config, plan, skip_lines=hwm)
        assert not errors, "ERROR log lines during stage round trips: " + " | ".join(errors)


def test_panel_command_set_scale(dia_client):
    c = dia_client
    c.navigate_to(_STAGE, timeout_s=30.0)
    rev0 = _wait_complete(c)["revision"]
    st = _panel(c)
    land = _control(st, "landFraction")["value"]
    target = 0.5 if abs(land - 0.5) > 0.01 else 0.4

    _panel_cmd(c, "setScale", {"key": "landFraction", "value": target})

    status = _wait_complete(c, min_revision=rev0 + 1)
    assert status["revision"] > rev0, (rev0, status)
    st2 = _panel(c, min_revision=rev0 + 1)
    assert abs(_control(st2, "landFraction")["value"] - target) < 1e-3, st2["controls"]


def test_panel_command_set_size_standard_then_restore_small(dia_client):
    c = dia_client
    c.navigate_to(_STAGE, timeout_s=30.0)
    rev0 = _wait_complete(c)["revision"]
    try:
        _panel_cmd(c, "setSize", {"size": "Standard"})
        rev1 = _wait_complete(c, min_revision=rev0 + 1)["revision"]
        st = _panel(c, min_revision=rev1)
        assert (st["stats"]["width"], st["stats"]["height"]) == (80, 52), st["stats"]
    finally:
        _panel_cmd(c, "setSize", {"size": "Small"})
    status = _wait_complete(c, min_revision=rev0 + 1)
    st = _panel(c, min_revision=status["revision"])
    assert st["stats"]["size"] == "Small", st["stats"]


def test_panel_command_preset_load(dia_client):
    c = dia_client
    c.navigate_to(_STAGE, timeout_s=30.0)
    rev0 = _wait_complete(c)["revision"]
    current = _panel(c)["stats"]["preset"]
    presets = c.send_command("cow.worldgen.presets")["presets"]
    ids = [p["id"] for p in presets]
    assert ids, presets
    target = next((i for i in ids if i != current), ids[0])

    _panel_cmd(c, "presetLoad", {"id": target})

    status = _wait_complete(c, min_revision=rev0 + 1)
    st = _panel(c, min_revision=status["revision"])
    assert st["stats"]["preset"] == target, (st["stats"], target)


def test_panel_command_select_and_clear_cell(dia_client):
    c = dia_client
    c.navigate_to(_STAGE, timeout_s=30.0)
    _wait_complete(c)

    _panel_cmd(c, "selectCell", {"x": 3, "y": 4})
    deadline = time.time() + 15.0
    st = _panel(c)
    while time.time() < deadline and not (st.get("cell") and st["cell"].get("selected") == {"x": 3, "y": 4}):
        time.sleep(0.1)
        st = _panel(c)
    assert st["cell"]["selected"] == {"x": 3, "y": 4}, st.get("cell")

    _panel_cmd(c, "clearCell")
    deadline = time.time() + 15.0
    st = _panel(c)
    while time.time() < deadline and st.get("cell") is not None:
        time.sleep(0.1)
        st = _panel(c)
    assert st.get("cell") is None, st.get("cell")


def test_panel_command_rejects_bad_requests(dia_client):
    c = dia_client
    c.navigate_to(_STAGE, timeout_s=30.0)
    _wait_complete(c)
    for payload in ({"domain": "NoSuchDomain", "cmd": "setScale"},
                    {"domain": "WorldGen"},
                    {"cmd": "setScale"},
                    {"domain": "WorldGen", "cmd": "setScale", "args": 5}):
        r = c.send_command("debug.panel_command", payload)
        assert r.get("success") is False and r.get("error"), (payload, r)


def _wait_panel(c, predicate, what, timeout_s=15.0):
    deadline = time.time() + timeout_s
    st = _panel(c)
    while time.time() < deadline and not predicate(st):
        time.sleep(0.1)
        st = _panel(c)
    assert predicate(st), f"{what}: {st.get('view')!r:.400} cell={st.get('cell')!r:.200}"
    return st


def test_viewer_selection_colour_mode_and_size_round_trip(dia_client, request):
    """The hosted DiaCellLayers viewer owns selection and colour mode; the card follows it (worldgen-view plan tasks 8-9)."""
    from plugin import _collect_log_errors, _count_log_lines

    c = dia_client
    plan = getattr(request.config, "_dia_plan", None)
    hwm = _count_log_lines(request.config, plan) if plan else 0

    c.navigate_to(_STAGE, timeout_s=30.0)
    _wait_complete(c)
    st = _panel(c)
    assert st["view"]["colourMode"] == "terrain", st["view"]
    assert "legend" in st["view"], st["view"]   # dynamic part: present on every frame
    full = _wait_panel(c, lambda s: s.get("full") is True, "a full frame", timeout_s=20.0)   # modes ride on full frames only
    assert full["view"]["modes"][0] == "terrain" and "elevation" in full["view"]["modes"], full["view"]["modes"]

    # Sim-hosted viewer command (DiaCellLayers) -> the card's cell block shows the same cell.
    r = c.send_command("cells.select.cell", {"q": 6, "r": 7})
    assert "error" not in r, r
    st = _wait_panel(c, lambda s: s.get("cell") and s["cell"].get("selected") == {"x": 6, "y": 7}, "cell.selected after cells.select.cell")
    assert st["cell"]["x"] == 6 and st["cell"]["y"] == 7

    # Colour mode round trip through the panel command.
    _panel_cmd(c, "setColourMode", {"mode": "elevation"})
    _wait_panel(c, lambda s: s["view"]["colourMode"] == "elevation", "colourMode elevation")
    _panel_cmd(c, "setColourMode", {"mode": "terrain"})
    _wait_panel(c, lambda s: s["view"]["colourMode"] == "terrain", "colourMode terrain")

    # Size change rebuilds the viewer grid and the app restores Small cleanly.
    rev0 = _wait_complete(c)["revision"]
    try:
        _panel_cmd(c, "setSize", {"size": "Standard"})
        _wait_complete(c, min_revision=rev0 + 1)
        _panel(c)
    finally:
        _panel_cmd(c, "setSize", {"size": "Small"})
    status = _wait_complete(c, min_revision=rev0 + 1)
    st = _panel(c, min_revision=status["revision"])
    assert st["stats"]["size"] == "Small", st["stats"]
    c.send_command("cells.select.clear")
    _wait_panel(c, lambda s: s.get("cell") is None, "cell cleared")

    if plan:
        errors = _collect_log_errors(request.config, plan, skip_lines=hwm)
        assert not errors, "ERROR log lines: " + " | ".join(errors)
