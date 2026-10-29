"""CellLayersViewerTestStage thin smoke: layer registry listing, JSON base/overlay commands, upload version gate.

Not assertable over E2E today: the viewer panel's GetJSONState (legend unit/range, category swatches,
gridSelector, empty state) -- no DebugServer query exposes IDebugDomain::GetJSONState. The active base is
asserted through cells.layer.list (activeBase / activeGridIndex / per-layer selected).
"""
import time

from client import AutomationError

_STAGE = "CellLayersViewerTestStage"
_CHECKPOINT = "test.cell_layers_viewer.passed"
_M = "dia.celllayers.viewer."


def _metrics(c):
    return {k: c.get_metric(_M + k) for k in ("reads", "uploads", "skipped_frames")}


def _selected(listing):
    """Ids of layers flagged selected across all grids."""
    return [l["id"] for g in listing["grids"] for l in g["layers"] if l.get("selected")]


def _assert_base(c, base, grid_index):
    listing = c.send_command("cells.layer.list")
    assert listing["activeBase"] == base, (base, listing["activeBase"])
    assert listing["activeGridIndex"] == grid_index, (grid_index, listing["activeGridIndex"])
    assert _selected(listing) == [base], (base, _selected(listing))


def _wait(pred, timeout_s=5.0, interval_s=0.1):
    deadline = time.time() + timeout_s
    while time.time() < deadline:
        if pred():
            return True
        time.sleep(interval_s)
    return pred()


def _cmd(c, name, params):
    """Return (success, payload_or_message); failures come back as AutomationError."""
    try:
        return True, c.send_command(name, params)
    except AutomationError as e:
        return False, str(e)


def _settled_delta(c, before, key, expected, timeout_s=5.0):
    """Wait for metric `key` to reach before+expected, then confirm it does not overshoot."""
    assert _wait(lambda: _metrics(c)[key] >= before[key] + expected, timeout_s), \
        f"{key} never grew by {expected}: before={before} now={_metrics(c)}"
    time.sleep(0.5)  # ~15 frames at 30 Hz: any spurious extra read/upload would show here
    now = _metrics(c)
    assert now[key] - before[key] == expected, f"{key} delta != {expected}: before={before} now={now}"
    return now


def test_celllayers_smoke(dia_client):
    c = dia_client
    c.navigate_to(_STAGE)
    try:
        result = c.poll_checkpoint(_CHECKPOINT, timeout_s=30.0, expected_stage=_STAGE)
        assert result.get("passed"), f"checkpoint failed: {result}"

        # 1. Registry listing: 2 grids, layers per grid, overlay disabled by default.
        listing = c.send_command("cells.layer.list")
        grids = listing["grids"]
        assert len(grids) == 2, grids
        ids = [sorted(l["id"] for l in g["layers"]) for g in grids]
        by_size = {(g["width"], g["height"]): sorted(l["id"] for l in g["layers"]) for g in grids}
        assert by_size[(66, 42)] == ["biome", "rainfall", "temperature"], ids
        assert by_size[(20, 12)] == ["elevation"], ids
        kinds = {l["id"]: l["kind"] for g in grids for l in g["layers"]}
        assert kinds["biome"] == "category" and kinds["temperature"] == "float", kinds
        overlays = {o["id"]: o["enabled"] for o in listing["overlays"]}
        assert overlays == {"grid_edges": False}, overlays
        grid_a = next(i for i, g in enumerate(grids) if g["width"] == 66)
        grid_b = next(i for i, g in enumerate(grids) if g["width"] == 20)

        # 2. Default base is temperature (ui.default_base) on the first grid; exactly one layer flagged selected.
        assert listing["activeBase"] == "temperature", listing["activeBase"]
        assert listing["activeGridIndex"] == 0 == grid_a, (listing["activeGridIndex"], grid_a)
        assert _selected(listing) == ["temperature"], _selected(listing)

        # Settle: animate is off so the initial temperature upload is done and counters are quiescent.
        time.sleep(0.5)
        m0 = _metrics(c)

        # 3. Switching base to biome causes exactly one read + one upload.
        ok, data = _cmd(c, "cells.layer.base_by_id", {"id": "biome"})
        assert ok and data["id"] == "biome" and data["grid_index"] == grid_a, data
        m1 = _settled_delta(c, m0, "uploads", 1)
        assert m1["reads"] - m0["reads"] == 1, (m0, m1)
        _assert_base(c, "biome", grid_a)

        # 4. Cross-grid switch changes the active grid; unknown id fails and leaves the base unchanged.
        ok, data = _cmd(c, "cells.layer.base_by_id", {"id": "elevation"})
        assert ok and data["grid_index"] == grid_b, data
        m2 = _settled_delta(c, m1, "uploads", 1)
        _assert_base(c, "elevation", grid_b)
        ok, msg = _cmd(c, "cells.layer.base_by_id", {"id": "nope"})
        assert not ok and "unknown layer id 'nope'" in msg, (ok, msg)
        ok, msg = _cmd(c, "cells.layer.base_by_id", {})
        assert not ok and "missing or non-string param 'id'" in msg, (ok, msg)
        _assert_base(c, "elevation", grid_b)  # failed switches leave the base unchanged
        time.sleep(0.5)
        m3 = _metrics(c)
        assert m3["uploads"] == m2["uploads"] and m3["reads"] == m2["reads"], (m2, m3)
        ok, data = _cmd(c, "cells.layer.base_by_id", {"id": "temperature"})  # back to grid A
        assert ok and data["grid_index"] == grid_a, data
        _assert_base(c, "temperature", grid_a)

        # 5. Overlay toggle on/off, unknown overlay fails.
        ok, data = _cmd(c, "cells.overlay.toggle_by_id", {"id": "grid_edges"})
        assert ok and data == {"id": "grid_edges", "enabled": True}, data
        assert {o["id"]: o["enabled"] for o in c.send_command("cells.layer.list")["overlays"]} == {"grid_edges": True}
        ok, data = _cmd(c, "cells.overlay.toggle_by_id", {"id": "grid_edges"})
        assert ok and data["enabled"] is False, data
        ok, msg = _cmd(c, "cells.overlay.toggle_by_id", {"id": "nope"})
        assert not ok and "unknown overlay id 'nope'" in msg, (ok, msg)

        # 6. Version gate (animate off): idle frames are skipped, not re-read or re-uploaded.
        time.sleep(0.5)
        g0 = _metrics(c)
        time.sleep(2.0)  # ~60 frames
        g1 = _metrics(c)
        assert g1["skipped_frames"] - g0["skipped_frames"] >= 30, (g0, g1)
        assert g1["reads"] == g0["reads"] and g1["uploads"] == g0["uploads"], (g0, g1)

        # 6b. Animation on (temperature version steps every 15 frames): reads and uploads grow; off: they stop.
        ok, msg = _cmd(c, "cellviewer.set_animate", {})
        assert not ok and "missing or non-bool param 'enabled'" in msg, (ok, msg)
        ok, msg = _cmd(c, "cellviewer.set_animate", {"enabled": "yes"})
        assert not ok, (ok, msg)
        time.sleep(0.5)
        assert _metrics(c)["uploads"] == g1["uploads"], "failed set_animate must not start animation"
        ok, data = _cmd(c, "cellviewer.set_animate", {"enabled": True})
        assert ok and data == {"enabled": True}, data
        a0 = g1
        time.sleep(2.0)  # ~60 frames: ~4 version steps
        a1 = _metrics(c)
        assert _wait(lambda: _metrics(c)["uploads"] - a0["uploads"] >= 2), (a0, _metrics(c))
        a1 = _metrics(c)
        assert a1["reads"] - a0["reads"] >= 2 and a1["uploads"] - a0["uploads"] >= 2, (a0, a1)
        ok, data = _cmd(c, "cellviewer.set_animate", {"enabled": False})
        assert ok and data == {"enabled": False}, data
        time.sleep(0.5)  # let any in-flight step land
        s0 = _metrics(c)
        time.sleep(1.0)
        s1 = _metrics(c)
        assert s1["reads"] == s0["reads"] and s1["uploads"] == s0["uploads"], (s0, s1)
        _assert_base(c, "temperature", grid_a)
    finally:
        c.abort_stage()
        c.navigate_to("Boot")
