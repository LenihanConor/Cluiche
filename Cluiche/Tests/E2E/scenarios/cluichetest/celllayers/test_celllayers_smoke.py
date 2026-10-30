"""CellLayersViewerTestStage thin smoke: layer registry listing, JSON base/overlay commands, upload version gate.

The viewer panel's GetJSONState (activeBase, legend, category swatches, gridSelector, empty, layer descriptions,
hotkeys) is asserted through the debug.domain_state query. Hover needs an E2E mouse-move command and is not covered.
"""
import math
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


def _panel(c):
    return c.send_command("debug.domain_state", {"id": "celllayers"})


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


def _sel_metrics(c):
    return {k: c.get_metric(_M + k) for k in ("select_commands", "select_rejected")}


def _neighbours(q, r):
    """Flat-top odd-q hex neighbours (odd columns sit half a cell lower), unbounded."""
    if q % 2 == 0:
        d = [(1, -1), (1, 0), (-1, -1), (-1, 0), (0, -1), (0, 1)]
    else:
        d = [(1, 0), (1, 1), (-1, 0), (-1, 1), (0, -1), (0, 1)]
    return [(q + dq, r + dr) for dq, dr in d]


def _edge_count(cells, w, h):
    """Outline edges: sides between a selected cell and a non-selected or off-grid neighbour."""
    return sum(1 for (q, r) in cells for n in _neighbours(q, r) if n not in cells)


def _independent_set(w, h):
    """Greedy (row-major) set of mutually non-adjacent cells."""
    taken = set()
    for q in range(w):
        for r in range(h):
            if not any(n in taken for n in _neighbours(q, r)):
                taken.add((q, r))
    return sorted(taken)


def _centre(q, r, s):
    return 1.5 * s * q, math.sqrt(3) * s * (r + 0.5 * (q & 1))


def _in_triangle(p, tri):
    def side(a, b, c):
        return (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0])
    d = [side(tri[i], tri[(i + 1) % 3], p) for i in range(3)]
    return all(x > 0 for x in d) or all(x < 0 for x in d)


def _settled_delta(c, before, key, expected, timeout_s=5.0):
    """Wait for metric `key` to reach before+expected, then confirm it does not overshoot."""
    assert _wait(lambda: _metrics(c)[key] >= before[key] + expected, timeout_s), \
        f"{key} never grew by {expected}: before={before} now={_metrics(c)}"
    time.sleep(0.5)  # ~15 frames at 30 Hz: any spurious extra read/upload would show here
    now = _metrics(c)
    assert now[key] - before[key] == expected, f"{key} delta != {expected}: before={before} now={now}"
    return now


_P = "cluichetest.celllayers."
_FLAGS = ("left", "middle", "right", "shift", "ctrl", "alt", "esc")


def _cell_pixel(c, q, r):
    """Pixel (top-left origin, Y down) of the centre of cell (q, r) on the active grid; q, r may be outside the grid."""
    d = c.send_command(_P + "cell_pixel", {"q": q, "r": r})
    return d["x"], d["y"]


def _frame(x, y, **flags):
    assert set(flags) <= set(_FLAGS), flags
    f = {"x": x, "y": y}
    f.update({k: bool(v) for k, v in flags.items()})
    return f


def _frames_click(px, py, **mods):
    """move, press, release. Modifiers sit on the release frame (that is where they are read) and on the press frame."""
    return [_frame(px, py, left=False), _frame(px, py, left=True, **mods), _frame(px, py, left=False, **mods)]


def _frames_drag(p0, p1, steps=4, button="left", **mods):
    """move to p0, press, `steps` interpolated held moves, release at p1 (modifiers on press, moves and release)."""
    held = {button: True, **mods}
    out = [_frame(p0[0], p0[1], **{button: False}), _frame(p0[0], p0[1], **held)]
    for i in range(1, steps + 1):
        t = i / steps
        out.append(_frame(p0[0] + (p1[0] - p0[0]) * t, p0[1] + (p1[1] - p0[1]) * t, **held))
    out.append(_frame(p1[0], p1[1], **{button: False, **mods}))
    return out


def _send_frames(c, frames, timeout_s=5.0):
    """Enqueue the frames (one is consumed per Sim frame) and wait until the queue has drained."""
    for i in range(0, len(frames), 200):
        chunk = frames[i:i + 200]
        assert _wait(lambda: c.send_command(_P + "pointer_pending")["pending"] + len(chunk) <= 512, timeout_s)
        d = c.send_command(_P + "pointer_frames", {"frames": chunk})
        assert d["ok"] is True and d["queued"] == len(chunk), d
    assert _wait(lambda: c.send_command(_P + "pointer_pending")["pending"] == 0, timeout_s), \
        c.send_command(_P + "pointer_pending")
    time.sleep(0.2)  # the last popped frame is consumed by the viewer update; let metrics publish


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

        # 2b. Panel state (debug.domain_state): same JSON the debug panel receives.
        st = _panel(c)
        assert st["_id"] == "celllayers" and st["empty"] is False and "noData" not in st, st
        assert st["activeBase"] == "temperature", st["activeBase"]
        assert len(st["gridSelector"]) == 2 and [g["index"] for g in st["gridSelector"] if g["active"]] == [grid_a], st["gridSelector"]
        panel_layers = {l["id"]: l for l in st["layers"]}
        assert set(panel_layers) == {"biome", "rainfall", "temperature"}, panel_layers.keys()  # active grid only
        assert [i for i, l in panel_layers.items() if l["selected"]] == ["temperature"], panel_layers
        hotkeys = [l["hotkey"] for l in panel_layers.values()]
        assert all(len(h) == 1 for h in hotkeys) and len(set(hotkeys)) == len(hotkeys), hotkeys
        legend = st["legend"]
        assert legend["available"] and not legend["category"], legend
        assert legend["unit"] and legend["min"] < legend["max"], legend
        assert "error" in c.send_command("debug.domain_state", {"id": "nope"})
        assert "error" in c.send_command("debug.domain_state", {"id": 5})
        assert "celllayers" in {d.get("_id") for d in c.send_command("debug.domain_state").values()}

        # Settle: animate is off so the initial temperature upload is done and counters are quiescent.
        time.sleep(0.5)
        m0 = _metrics(c)

        # 3. Switching base to biome causes exactly one read + one upload.
        ok, data = _cmd(c, "cells.layer.base_by_id", {"id": "biome"})
        assert ok and data["id"] == "biome" and data["grid_index"] == grid_a, data
        m1 = _settled_delta(c, m0, "uploads", 1)
        assert m1["reads"] - m0["reads"] == 1, (m0, m1)
        _assert_base(c, "biome", grid_a)
        st = _panel(c)
        assert st["activeBase"] == "biome" and st["legend"]["category"], st["legend"]
        assert st["legend"]["swatches"], st["legend"]  # category legend lists a swatch per biome

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

        # 7. Selection (Feature 2). Active grid is A (66x42); the stage publishes per-biome counts in its checkpoint message.
        biome_counts = [int(n) for n in result["message"].split("biome_counts=")[1].split(",")]
        assert sum(biome_counts) == 66 * 42 and sum(1 for n in biome_counts if n > 0) >= 3, biome_counts
        biome_ids = ["tundra", "forest", "desert", "jungle"]  # schema enum order

        sm0 = _sel_metrics(c)
        ok_calls = 0
        bad_calls = 0

        def sel(name, params=None):
            nonlocal ok_calls
            data = c.send_command(name, params)
            ok_calls += 1
            return data

        def sel_err(name, params, text):
            nonlocal bad_calls
            before = _panel(c)["selection"]["count"]
            try:
                c.send_command(name, params)
            except AutomationError as e:
                assert text in str(e), (name, params, str(e))
            else:
                raise AssertionError(f"{name} {params} should have failed")
            bad_calls += 1
            assert _panel(c)["selection"]["count"] == before, (name, params)

        def panel_count():
            return _panel(c)["selection"]["count"]

        # 7a. cell / toggle / box / cells / poly / category / clear
        assert panel_count() == 0
        assert sel("cells.select.cell", {"q": 4, "r": 5})["count"] == 1 and panel_count() == 1
        assert sel("cells.select.toggle", {"q": 4, "r": 5})["count"] == 0 and panel_count() == 0
        data = sel("cells.select.box", {"q0": 9, "r0": 6, "q1": 5, "r1": 3})  # reversed corners: 5 cols x 4 rows
        assert data["count"] == 5 * 4 and panel_count() == 20, data
        box_cells = {(q, r) for q in range(5, 10) for r in range(3, 7)}
        assert _wait(lambda: c.get_metric(_M + "selection_size") == 20), c.get_metric(_M + "selection_size")
        assert _wait(lambda: c.get_metric(_M + "outline_edges") == _edge_count(box_cells, 66, 42)), \
            (c.get_metric(_M + "outline_edges"), _edge_count(box_cells, 66, 42))

        data = sel("cells.select.cells", {"cells": [[1, 1], [1, 1], [-1, 0], [70, 3], [2, 2]]})
        assert data["requested"] == 5 and sorted(map(tuple, data["accepted"])) == [(1, 1), (2, 2)], data
        assert data["count"] == 2 and panel_count() == 2, data

        s = 24.0  # stage kCellSize
        cx, cy = 1.5 * s * 10, math.sqrt(3) * s * 10
        tri = [(cx - 57.3, cy - 57.7), (cx + 57.3, cy - 57.7), (cx + 0.4, cy + 71.9)]
        expected_poly = {(q, r) for q in range(66) for r in range(42) if _in_triangle(_centre(q, r, s), tri)}
        assert (10, 10) in expected_poly and len(expected_poly) > 1, expected_poly
        data = sel("cells.select.poly", {"points": [list(p) for p in tri]})
        assert data["count"] == len(expected_poly) and panel_count() == len(expected_poly), (data, len(expected_poly))

        for i, bid in enumerate(biome_ids):
            data = sel("cells.select.category", {"layer": "biome", "value": bid})
            assert data["count"] == biome_counts[i] and panel_count() == biome_counts[i], (bid, data, biome_counts)
            data = sel("cells.select.category", {"layer": "biome", "value": i})  # numeric form
            assert data["count"] == biome_counts[i], (i, data, biome_counts)

        assert sel("cells.select.clear")["count"] == 0 and panel_count() == 0

        # 7b. Error cases leave the selection untouched.
        sel("cells.select.cell", {"q": 3, "r": 3})
        sel_err("cells.select.cell", {"r": 1}, "missing or non-integer param 'q'")
        sel_err("cells.select.toggle", {"q": 1, "r": "x"}, "missing or non-integer param 'r'")
        sel_err("cells.select.box", {"q0": 1, "r0": 1, "q1": 2}, "missing or non-integer param 'r1'")
        sel_err("cells.select.poly", {"points": [[0, 0], [1, 1]]}, "needs at least 3 points")
        sel_err("cells.select.category", {"layer": "biome", "value": "nope"}, "unknown value")
        sel_err("cells.select.category", {"layer": "rainfall", "value": "x"}, "layer 'rainfall' is not a category layer")
        sel_err("cells.select.cells", {"cells": [[1, 2], [3]]}, "is not an integer [q, r] pair")
        assert panel_count() == 1

        # 7c. Tool and group.
        st = _panel(c)["selection"]
        assert st["tool"] == "select" and st["group"] == "biome", st
        assert [g["id"] for g in st["groups"]] == ["biome"] and [g["selected"] for g in st["groups"]] == [True], st
        assert sel("cells.select.tool", {"tool": "pan"})["tool"] == "pan"
        assert _panel(c)["selection"]["tool"] == "pan"
        sel_err("cells.select.tool", {"tool": "lasso"}, "unknown tool")
        assert _panel(c)["selection"]["tool"] == "pan"
        assert sel("cells.select.tool", {"tool": "select"})["tool"] == "select"
        assert sel("cells.select.group", {"layer": "biome"})["group"] == "biome"
        sel_err("cells.select.group", {"layer": "rainfall"}, "not a category layer")
        assert _panel(c)["selection"]["group"] == "biome"

        # 7d. Switching grid clears the selection (both ways).
        assert panel_count() == 1
        ok, data = _cmd(c, "cells.layer.base_by_id", {"id": "elevation"})
        assert ok and data["grid_index"] == grid_b, data
        st = _panel(c)["selection"]
        assert st["count"] == 0 and st["groups"] == [] and st["group"] == "", st
        ok, data = _cmd(c, "cells.layer.base_by_id", {"id": "temperature"})
        assert ok and data["grid_index"] == grid_a, data
        assert panel_count() == 0
        sel("cells.select.box", {"q0": 0, "r0": 0, "q1": 2, "r1": 2})
        assert panel_count() == 9
        _cmd(c, "cells.layer.base_by_id", {"id": "elevation"})
        assert panel_count() == 0
        _cmd(c, "cells.layer.base_by_id", {"id": "temperature"})
        assert panel_count() == 0

        # 7e. Outline cap: 6 sides per isolated cell, so > 4000 edges needs > 666 mutually non-adjacent cells.
        iso = _independent_set(66, 42)
        assert len(iso) > 4000 // 6 and len(iso) <= 2048, len(iso)
        assert _edge_count(set(iso), 66, 42) == 6 * len(iso) > 4000
        data = sel("cells.select.cells", {"cells": [list(p) for p in iso]})
        assert data["count"] == len(iso) and "overCap" not in data, data
        assert _wait(lambda: _panel(c)["selection"]["tooLarge"] is True), _panel(c)["selection"]
        st = _panel(c)["selection"]
        assert st["tooLargeText"] == f"Selection too large to outline ({len(iso)} cells)", st
        assert _wait(lambda: c.get_metric(_M + "outline_edges") == 0), c.get_metric(_M + "outline_edges")
        assert c.get_metric(_M + "selection_size") == len(iso)  # 7f: size follows the selection even when not outlined
        sel("cells.select.clear")
        assert _wait(lambda: _panel(c)["selection"]["tooLarge"] is False and c.get_metric(_M + "selection_size") == 0)
        assert "tooLargeText" not in _panel(c)["selection"]

        # 7f. Command counters: every success counted in select_commands, every failure in select_rejected.
        # (+ the tool/group/clear/base calls above that went through sel(); base_by_id is not a select command.)
        sm1 = _sel_metrics(c)
        assert sm1["select_commands"] - sm0["select_commands"] == ok_calls, (sm0, sm1, ok_calls)
        assert sm1["select_rejected"] - sm0["select_rejected"] == bad_calls, (sm0, sm1, bad_calls)

        # 8. Gestures via injected pointer frames (test-only stage commands; one frame is consumed per Sim frame).
        assert sel("cells.select.tool", {"tool": "select"})["tool"] == "select"
        sel("cells.select.clear")
        ok, data = _cmd(c, "cells.layer.base_by_id", {"id": "temperature"})
        assert ok and data["grid_index"] == grid_a, data
        assert _wait(lambda: panel_count() == 0)

        # 8-pre. Injection command validation.
        for params, text in (({}, "missing or non-array param 'frames'"), ({"frames": []}, "frames"),
                             ({"frames": [{"y": 1}]}, "frame 0: missing or non-numeric 'x'"),
                             ({"frames": [{"x": 1, "y": 1}, {"x": 1, "y": "a"}]}, "frame 1: missing or non-numeric 'y'"),
                             ({"frames": [{"x": 1, "y": 1, "left": 1}]}, "non-bool")):
            ok, msg = _cmd(c, _P + "pointer_frames", params)
            assert not ok and text in msg, (params, ok, msg)
        ok, msg = _cmd(c, _P + "pointer_frames", {"frames": [{"x": 1, "y": 1}] * 257})
        assert not ok and "too many" in msg, (ok, msg)
        ok, msg = _cmd(c, _P + "cell_pixel", {"q": 1})
        assert not ok and "non-integer param 'q'" not in msg and "non-integer param 'r'" in msg, (ok, msg)
        assert c.send_command(_P + "pointer_pending") == {"pending": 0}

        def gest_metrics():
            time.sleep(0.2)
            return _sel_metrics(c)

        def expect_count(n, what):
            assert _wait(lambda: panel_count() == n, 3.0), f"{what}: expected count {n}, got {panel_count()}"

        def clear_sel():
            sel("cells.select.clear")
            expect_count(0, "clear")

        A, B, C = (12, 10), (30, 20), (45, 28)
        pa, pb, pc = (_cell_pixel(c, *A), _cell_pixel(c, *B), _cell_pixel(c, *C))

        # 8a. Plain click (no modifiers, AC 12) selects the cell under the pointer: exactly one new select command.
        g0 = gest_metrics()
        _send_frames(c, _frames_click(*pa))
        expect_count(1, "click")
        g1 = gest_metrics()
        assert g1["select_commands"] - g0["select_commands"] == 1 and g1["select_rejected"] == g0["select_rejected"], (g0, g1)
        assert _wait(lambda: c.get_metric(_M + "selection_size") == 1)
        # Same cell via the typed command toggles it off: proves the click selected exactly cell A.
        assert sel("cells.select.toggle", {"q": A[0], "r": A[1]})["count"] == 0
        clear_sel()

        # 8b. Shift-click toggles; a plain click elsewhere replaces.
        _send_frames(c, _frames_click(*pa))
        expect_count(1, "click A")
        _send_frames(c, _frames_click(*pa, shift=True))
        expect_count(0, "shift-click A toggles off")
        _send_frames(c, _frames_click(*pb, shift=True))
        expect_count(1, "shift-click B")
        _send_frames(c, _frames_click(*pc))
        expect_count(1, "click C replaces")
        _send_frames(c, _frames_click(*pc, shift=True))
        expect_count(0, "shift-click C removes the one selected cell, so the plain click had selected C and not B")
        clear_sel()

        # 8c. Box drag: inclusive offset rectangle (cols x rows), either direction.
        q0, r0, q1, r1 = 10, 8, 15, 12
        want = (abs(q1 - q0) + 1) * (abs(r1 - r0) + 1)
        p_a, p_b = _cell_pixel(c, q0, r0), _cell_pixel(c, q1, r1)
        _send_frames(c, _frames_drag(p_a, p_b))
        expect_count(want, "box A->B")
        clear_sel()
        _send_frames(c, _frames_drag(p_b, p_a))
        expect_count(want, "box B->A")
        box_cells = {(q, r) for q in range(q0, q1 + 1) for r in range(r0, r1 + 1)}
        assert _wait(lambda: c.get_metric(_M + "outline_edges") == _edge_count(box_cells, 66, 42)), \
            (c.get_metric(_M + "outline_edges"), _edge_count(box_cells, 66, 42))
        clear_sel()
        _send_frames(c, _frames_drag(_cell_pixel(c, 20, 15), _cell_pixel(c, 24, 15)))
        expect_count(5, "single-row box")
        clear_sel()

        # 8d. Alt-drag lasso: 5/6 circle of radius ~2.5 cells around cell L, released on a cell other than the start.
        L = (30, 20)
        pl = _cell_pixel(c, *L)
        spacing = _cell_pixel(c, L[0] + 2, L[1])[0] - pl[0]  # 3 cell sizes in pixels
        rad = spacing * 2.5 / 3.0
        pts = [(pl[0] + rad * math.cos(math.radians(a)), pl[1] + rad * math.sin(math.radians(a))) for a in range(0, 301, 20)]
        lasso = [_frame(pts[0][0], pts[0][1], left=False)]
        lasso += [_frame(x, y, left=True, alt=True) for x, y in pts]
        lasso.append(_frame(pts[-1][0], pts[-1][1], left=False, alt=True))
        _send_frames(c, lasso)
        assert _wait(lambda: panel_count() >= 1, 3.0), "lasso selected nothing"
        lasso_n = panel_count()
        assert lasso_n >= 3, f"lasso selected {lasso_n}"
        assert _wait(lambda: c.get_metric(_M + "selection_size") == lasso_n)
        # 8f. Esc clears.
        _send_frames(c, [_frame(pl[0], pl[1], esc=True)])
        expect_count(0, "Esc after lasso")

        # 8e. Ctrl-click selects the whole category (selection group = biome); different biomes give their own counts.
        seen = {}
        for q, r in ((5, 5), (30, 5), (55, 5), (5, 20), (30, 20), (55, 20), (5, 36), (30, 36), (60, 36), (20, 12), (45, 30)):
            _send_frames(c, _frames_click(*_cell_pixel(c, q, r), ctrl=True))
            _wait(lambda: panel_count() > 0, 3.0)
            n = panel_count()
            assert n in biome_counts and n > 0, ((q, r), n, biome_counts)
            seen[(q, r)] = n
            clear_sel()
            if len(set(seen.values())) >= 3:
                break
        assert len(set(seen.values())) >= 2, f"ctrl-click never reached two different biomes: {seen}"

        # 8f. Esc with a selection present clears it (a bare Esc with nothing selected is harmless).
        _send_frames(c, _frames_click(*pa))
        expect_count(1, "click before Esc")
        _send_frames(c, [_frame(pa[0], pa[1], esc=True)])
        expect_count(0, "Esc")
        _send_frames(c, [_frame(pa[0], pa[1], esc=True)])
        expect_count(0, "Esc on empty")

        # 8g. Pressing outside the grid starts no gesture: no command, selection unchanged.
        _send_frames(c, _frames_click(*pa))
        expect_count(1, "click before out-of-grid")
        g0 = gest_metrics()
        for q, r in ((-5, 10), (70, 10), (10, 50), (10, -4)):
            _send_frames(c, _frames_click(*_cell_pixel(c, q, r)))
            _send_frames(c, _frames_click(*_cell_pixel(c, q, r), shift=True))
            _send_frames(c, _frames_click(*_cell_pixel(c, q, r), ctrl=True))
        g1 = gest_metrics()
        assert panel_count() == 1 and g1 == g0, (g0, g1, panel_count())

        # 8h. Middle and right buttons never select, even dragged across the grid.
        for button in ("middle", "right"):
            _send_frames(c, _frames_drag(pb, pc, button=button))
            _send_frames(c, _frames_drag(pb, pc, button=button, shift=True))
        g2 = gest_metrics()
        assert panel_count() == 1 and g2 == g0, (g0, g2, panel_count())

        # 8i. Pan tool ignores the left button.
        assert sel("cells.select.tool", {"tool": "pan"})["tool"] == "pan"
        g0 = gest_metrics()
        _send_frames(c, _frames_click(*pb))
        _send_frames(c, _frames_drag(pb, pc))
        _send_frames(c, _frames_click(*pc, shift=True))
        g1 = gest_metrics()
        assert panel_count() == 1 and g1 == g0, (g0, g1, panel_count())
        assert sel("cells.select.tool", {"tool": "select"})["tool"] == "select"

        # 8j. Modifiers are read at release: shift only on the PRESS frame is a plain click (replaces, not toggles).
        _send_frames(c, _frames_click(*pa))
        expect_count(1, "click A")
        _send_frames(c, [_frame(*pb, left=False), _frame(*pb, left=True, shift=True), _frame(*pb, left=False)])
        expect_count(1, "shift on press only: plain click replaces A with B")
        sel("cells.select.toggle", {"q": B[0], "r": B[1]})
        expect_count(0, "selection was exactly B")
        # ... and shift only on the RELEASE frame toggles.
        _send_frames(c, _frames_click(*pa))
        _send_frames(c, [_frame(*pb, left=False), _frame(*pb, left=True), _frame(*pb, left=False, shift=True)])
        expect_count(2, "shift on release only: toggles B in alongside A")

        # Leave the viewer as found.
        clear_sel()
        assert sel("cells.select.tool", {"tool": "select"})["tool"] == "select"
        assert c.send_command(_P + "pointer_pending") == {"pending": 0}
    finally:
        c.abort_stage()
        c.navigate_to("Boot")


# ---------------------------------------------------------------------------------------------------------------------
# Selection Inspector (Feature 4). Expected numbers are derived from the stage's deterministic generators, replicated here
# (CellLayersViewerTestStageModule.cpp: StaticTemperature / StaticRainfall / ReadBiome). Temperature is only static while the
# animation is off and its version is 1, so this test navigates to a fresh stage and never turns the animation on.
# ---------------------------------------------------------------------------------------------------------------------
_BIG_W, _BIG_H = 66, 42
_BIOME_IDS = ["tundra", "forest", "desert", "jungle"]
_BIOME_LABELS = ["Tundra", "Forest", "Desert", "Jungle"]


def _clampf(v, lo, hi):
    return lo if v < lo else (hi if v > hi else v)


def _temperature(q, r):
    lat = r / (_BIG_H - 1) - 0.5
    return _clampf(16.0 + 20.0 * math.sin(q * 0.11) * math.cos(r * 0.14) - 18.0 * lat, -20.0, 40.0)


def _rainfall(q, r):
    return _clampf(200.0 + 180.0 * math.sin(q * 0.2 + 1.0) * math.sin(r * 0.25 + 2.0), 0.0, 400.0)


def _biome(q, r):
    return (2 if _temperature(q, r) >= 15.0 else 0) + (1 if _rainfall(q, r) >= 200.0 else 0)


def _box(q0, r0, q1, r1):
    return [(q, r) for q in range(q0, q1 + 1) for r in range(r0, r1 + 1)]


def _expected(cells):
    """Expected inspector stats of `cells` for each layer of grid A (float32 vs float64 noise is covered by _approx)."""
    # Biome thresholds must not sit within float32 noise of any selected cell, or the category counts would be flaky.
    margin = min(min(abs(_temperature(*c) - 15.0), abs(_rainfall(*c) - 200.0)) for c in cells)
    assert margin > 1e-3, margin
    out = {}
    for lid, fn in (("temperature", _temperature), ("rainfall", _rainfall)):
        vals = [fn(*c) for c in cells]
        out[lid] = {"count": len(vals), "min": min(vals), "mean": sum(vals) / len(vals), "max": max(vals)}
    cats = [0, 0, 0, 0]
    for c in cells:
        cats[_biome(*c)] += 1
    out["biome"] = {"count": len(cells), "categories": cats}
    return out


def _approx(a, b):
    return abs(a - b) <= 1e-3 + 1e-4 * abs(b)


def _inspect(c, **params):
    return c.send_command("cells.inspect", params or None)


def _inspect_layers(res):
    return {l["layer"]: l for l in res["layers"]}


def _assert_float_stats(stats, exp, what):
    assert stats["hasData"] is True and stats["nanCount"] == 0, (what, stats)
    assert stats["count"] == exp["count"], (what, stats, exp)
    for k in ("min", "mean", "max"):
        assert _approx(stats[k], exp[k]), (what, k, stats[k], exp[k])


def _assert_category_stats(stats, exp, what):
    assert stats["count"] == exp["count"] and stats["nanCount"] == 0 and stats["other"] == 0, (what, stats)
    assert [x["id"] for x in stats["categories"]] == _BIOME_IDS, (what, stats)
    assert [x["label"] for x in stats["categories"]] == _BIOME_LABELS, (what, stats)
    assert [x["count"] for x in stats["categories"]] == exp["categories"], (what, stats, exp)


def _assert_all_stats(res, cells, what):
    exp = _expected(cells)
    assert res["ok"] is True and res["selected"] == len(cells), (what, res)
    layers = _inspect_layers(res)
    assert set(layers) == {"temperature", "rainfall", "biome"}, (what, list(layers))
    _assert_float_stats(layers["temperature"], exp["temperature"], what + " temperature")
    _assert_float_stats(layers["rainfall"], exp["rainfall"], what + " rainfall")
    _assert_category_stats(layers["biome"], exp["biome"], what + " biome")
    return exp, layers


def test_celllayers_inspector(dia_client):
    c = dia_client
    c.navigate_to(_STAGE)
    try:
        result = c.poll_checkpoint(_CHECKPOINT, timeout_s=30.0, expected_stage=_STAGE)
        assert result.get("passed"), f"checkpoint failed: {result}"
        assert c.send_command("cells.layer.list")["activeBase"] == "temperature"
        assert c.send_command("cells.select.tool", {"tool": "select"})["tool"] == "select"
        c.send_command("cells.select.clear")

        def synced():
            ok, _ = _cmd(c, "cells.inspect", None)
            return ok
        assert _wait(synced), ("inspector never synced to the active grid", _cmd(c, "cells.inspect", None))

        # 1. Empty selection: nothing computed; pin has nothing to pin.
        res = _inspect(c)
        assert res["ok"] is True and res["selected"] == 0 and res["layers"] == [], res
        assert "pinned" not in res and "cell" not in res, res
        assert _inspect(c, all=True)["layers"] == []
        ok, msg = _cmd(c, "cells.select.pin", None)
        assert not ok and "nothing selected to pin" in msg, (ok, msg)
        ok, msg = _cmd(c, "cells.select.pin", {"freeze": "yes"})
        assert not ok and "non-boolean param 'freeze'" in msg, (ok, msg)

        # 2. Region A: box q5..9 x r3..6 (20 cells). Derivation: _expected() evaluates the stage generators per cell;
        #    A is warm (T 29.3..39.0 >= 15) and half wet / half dry -> desert 10, jungle 10, tundra = forest = 0.
        box_a = _box(5, 3, 9, 6)
        assert c.send_command("cells.select.box", {"q0": 5, "r0": 3, "q1": 9, "r1": 6})["count"] == 20
        res = _inspect(c, all=True)
        exp_a, layers = _assert_all_stats(res, box_a, "A all")
        assert exp_a["biome"]["categories"] == [0, 0, 10, 10], exp_a  # hand-checked: the derivation above
        assert res["grid"] == {"width": _BIG_W, "height": _BIG_H}, res
        assert layers["temperature"]["base"] is True and layers["rainfall"]["base"] is False, layers
        assert layers["temperature"]["unit"] == "C" and layers["temperature"]["label"] == "Temperature", layers["temperature"]
        assert layers["rainfall"]["unit"] == "mm" and layers["biome"]["kind"] == "category", layers
        assert layers["temperature"]["schemaKey"] == "cluichetest.celllayers/temperature", layers["temperature"]
        assert "histogram" in res and res["base"] == "cluichetest.celllayers/temperature", res
        assert "pinned" not in res and "cell" not in res, res

        # 2b. Request forms: group, layers[] (request order), layerId, shown set (default), and their errors.
        res = _inspect(c, group="Climate")
        assert [l["layer"] for l in res["layers"]] == ["temperature", "rainfall"], res
        res = _inspect(c, group="Terrain")
        assert [l["layer"] for l in res["layers"]] == ["biome"], res
        res = _inspect(c, layers=["rainfall", "temperature"])
        assert [l["layer"] for l in res["layers"]] == ["rainfall", "temperature"], res
        _assert_float_stats(res["layers"][0], _expected(box_a)["rainfall"], "layers[] rainfall")
        res = _inspect(c, layerId="biome")
        assert [l["layer"] for l in res["layers"]] == ["biome"], res
        for params, text in (({"group": "Nope"}, "no layers in group 'Nope'"), ({"layers": ["nope"]}, "unknown layer 'nope'"),
                             ({"layers": ["elevation"]}, "not a layer of the active grid"),
                             ({"all": True, "group": "Climate"}, "conflicting params"), ({"layers": []}, "non-empty array")):
            ok, msg = _cmd(c, "cells.inspect", params)
            assert not ok and text in msg, (params, ok, msg)
        # Default form = shown set: the base and the expanded Climate group; Terrain is collapsed until expanded.
        assert _wait(lambda: [l["layer"] for l in _inspect(c)["layers"]] == ["temperature", "rainfall"]), _inspect(c)
        ok, msg = _cmd(c, "cells.inspector.expand", {"group": "Nope", "expanded": True})
        assert not ok and "no layers in group 'Nope'" in msg, (ok, msg)
        ok, msg = _cmd(c, "cells.inspector.expand", {"group": "Terrain"})
        assert not ok and "expanded" in msg, (ok, msg)
        assert c.send_command("cells.inspector.expand", {"group": "Terrain", "expanded": True}) == \
            {"ok": True, "group": "Terrain", "expanded": True}
        assert _wait(lambda: sorted(l["layer"] for l in _inspect(c)["layers"]) == ["biome", "rainfall", "temperature"]), _inspect(c)
        assert _inspect(c)["layers"][0]["layer"] == "temperature"  # base first
        assert c.send_command("cells.inspector.expand", {"group": "Terrain", "expanded": False})["expanded"] is False
        assert _wait(lambda: [l["layer"] for l in _inspect(c)["layers"]] == ["temperature", "rainfall"]), _inspect(c)

        # 3. Pin A (live), then select B = box q40..49 x r25..34 (100 cells; all four biomes): delta = B minus A.
        pin = c.send_command("cells.select.pin")
        assert pin["ok"] is True and pin["count"] == 20 and pin["frozen"] is False and "layers" not in pin, pin
        box_b = _box(40, 25, 49, 34)
        assert c.send_command("cells.select.box", {"q0": 40, "r0": 25, "q1": 49, "r1": 34})["count"] == 100

        def live_a_ready():
            ls = _inspect_layers(_inspect(c, all=True))
            return "count" in ls["temperature"].get("a", {}) and "count" in ls["rainfall"].get("a", {})
        assert _wait(live_a_ready), _inspect(c, all=True)
        res = _inspect(c, all=True)
        exp_b, layers = _assert_all_stats(res, box_b, "B all")
        assert exp_b["biome"]["categories"] == [6, 16, 44, 34], exp_b
        assert res["pinned"] == {"count": 20, "frozen": False}, res
        for lid in ("temperature", "rainfall"):
            a, d = layers[lid]["a"], layers[lid]["delta"]
            _assert_float_stats(a, exp_a[lid], "A " + lid)
            assert d["count"] == 100 - 20, d
            for k in ("min", "mean", "max"):
                assert _approx(d[k], exp_b[lid][k] - exp_a[lid][k]), (lid, k, d, exp_a[lid], exp_b[lid])
        # Terrain is collapsed, so A is live but not computed for biome until the group is shown.
        assert layers["biome"]["a"] == {"notComputed": True} and "delta" not in layers["biome"], layers["biome"]
        c.send_command("cells.inspector.expand", {"group": "Terrain", "expanded": True})
        assert _wait(lambda: "categories" in _inspect_layers(_inspect(c, all=True))["biome"].get("a", {})), _inspect(c, all=True)
        bio = _inspect_layers(_inspect(c, all=True))["biome"]
        _assert_category_stats(bio["a"], exp_a["biome"], "A biome")
        assert bio["delta"] == {"count": 80}, bio["delta"]  # category layers have no min / mean / max delta
        c.send_command("cells.inspector.expand", {"group": "Terrain", "expanded": False})

        # 3b. Re-select A while A is pinned: B == A so every delta is zero.
        c.send_command("cells.select.box", {"q0": 5, "r0": 3, "q1": 9, "r1": 6})
        assert _wait(live_a_ready)
        for lid in ("temperature", "rainfall"):
            d = _inspect_layers(_inspect(c, all=True))[lid]["delta"]
            assert d["count"] == 0 and all(abs(d[k]) < 1e-9 for k in ("min", "mean", "max")), d

        # 4. Unpin clears A.
        assert c.send_command("cells.select.unpin") == {"ok": True}
        res = _inspect(c, all=True)
        assert "pinned" not in res and all("a" not in l and "delta" not in l for l in res["layers"]), res
        assert c.send_command("cells.select.unpin") == {"ok": True}  # idempotent

        # 5. Pin with freeze: A's stats are stored for every layer of the grid (including collapsed Terrain) and never move.
        pin = c.send_command("cells.select.pin", {"freeze": True})
        assert pin["ok"] is True and pin["count"] == 20 and pin["frozen"] is True and pin["layers"] == 3, pin
        c.send_command("cells.select.box", {"q0": 40, "r0": 25, "q1": 49, "r1": 34})
        res = _inspect(c, all=True)  # frozen A is available at once: no waiting for a pass
        assert res["pinned"] == {"count": 20, "frozen": True}, res
        layers = _inspect_layers(res)
        _assert_float_stats(layers["temperature"]["a"], exp_a["temperature"], "frozen A temperature")
        _assert_category_stats(layers["biome"]["a"], exp_a["biome"], "frozen A biome")
        assert layers["biome"]["delta"] == {"count": 80}, layers["biome"]
        assert _approx(layers["rainfall"]["delta"]["mean"], exp_b["rainfall"]["mean"] - exp_a["rainfall"]["mean"]), layers["rainfall"]
        assert all("notFrozen" not in l["a"] for l in res["layers"]), res  # no layer was registered after the freeze
        c.send_command("cells.select.unpin")

        # 6. Single cell (7,4): index 4 * 66 + 7. The cell block has a row per shown layer plus the stage provider's details.
        cell = (7, 4)
        idx = cell[0] + cell[1] * _BIG_W
        assert c.send_command("cells.select.cell", {"q": cell[0], "r": cell[1]})["count"] == 1
        res = _inspect(c, all=True)
        assert res["selected"] == 1 and "cell" in res, res
        blk = res["cell"]
        assert (blk["index"], blk["q"], blk["r"]) == (idx, 7, 4), blk
        rows = {row["layer"]: row for row in blk["rows"]}
        assert _approx(rows["temperature"]["value"], _temperature(*cell)) and rows["temperature"]["unit"] == "C", rows["temperature"]
        assert _approx(rows["rainfall"]["value"], _rainfall(*cell)) and rows["rainfall"]["label"] == "Rainfall", rows["rainfall"]
        assert rows["temperature"]["hasData"] is True and rows["temperature"]["isNaN"] is False, rows["temperature"]
        want_b = _biome(*cell)  # (7,4) is warm -> desert (2) or jungle (3)
        assert want_b >= 2, want_b
        if "biome" in rows:
            assert rows["biome"]["value"] == want_b and rows["biome"]["enumId"] == _BIOME_IDS[want_b], rows["biome"]
            assert rows["biome"]["enumLabel"] == _BIOME_LABELS[want_b] and rows["biome"]["otherValue"] is False, rows["biome"]
        # Details: Section "Stage" (provider name), then the provider's Section / Row / RowValue (rainfall schema key, index % 97).
        details = blk["details"]
        assert [d["kind"] for d in details] == ["section", "section", "text", "value"], details
        assert details[0]["label"] == "Stage" and details[1]["label"] == "Position", details
        assert details[2]["label"] == "Cell" and details[2]["text"] == "7,4", details[2]
        assert details[3]["resolved"] is True and details[3]["label"] == "Rainfall" and details[3]["unit"] == "mm", details[3]
        assert details[3]["value"] == idx % 97, details[3]
        assert "detailsTruncated" not in blk, blk
        # The aggregates over the one cell equal its own values.
        _assert_all_stats(res, [cell], "single")

        # Another cell gives other details (the provider is invoked per cell).
        c.send_command("cells.select.cell", {"q": 30, "r": 20})
        det = _inspect(c)["cell"]["details"]
        assert det[2]["text"] == "30,20" and det[3]["value"] == (30 + 20 * _BIG_W) % 97, det

        # 7. Two cells: aggregates but no cell block.
        c.send_command("cells.select.cells", {"cells": [[7, 4], [30, 20]]})
        res = _inspect(c, all=True)
        assert res["selected"] == 2 and "cell" not in res, res
        _assert_all_stats(res, [(7, 4), (30, 20)], "two cells")

        # 8. The panel's inspector block (debug.domain_state) agrees with cells.inspect after a tick.
        def panel_inspector_ready():
            ins = _panel(c).get("inspector", {})
            if not ins.get("hasSelection"):
                return False
            pl = {l["id"]: l for l in ins["layers"]}
            return "temperature" in pl and pl["temperature"]["count"] == 2
        assert _wait(panel_inspector_ready), _panel(c).get("inspector")
        pl = {l["id"]: l for l in _panel(c)["inspector"]["layers"]}
        fresh = _inspect_layers(_inspect(c, all=True))
        for lid in ("temperature", "rainfall"):
            assert pl[lid]["count"] == fresh[lid]["count"] and _approx(pl[lid]["mean"], fresh[lid]["mean"]), (pl[lid], fresh[lid])
        assert "cell" not in _panel(c)["inspector"], _panel(c)["inspector"]

        # 9. Clearing the selection empties the result again.
        c.send_command("cells.select.clear")
        res = _inspect(c, all=True)
        assert res["selected"] == 0 and res["layers"] == [] and "pinned" not in res, res
    finally:
        c.abort_stage()
        c.navigate_to("Boot")


# ---------------------------------------------------------------------------------------------------------------------
# Query (cells.select.query / cells.query.check). Expected counts replicate the stage generators (see the inspector section
# above). A fresh stage keeps temperature static. The selection *version* is not exposed through the panel state or metrics,
# so "unchanged" is asserted via the selection count (and the selection's exact membership is probed with toggles).
# ---------------------------------------------------------------------------------------------------------------------
_Q = "dia.celllayers.query."


def _q_metrics(c):
    return {k: c.get_metric(_Q + k) for k in ("run", "rejected")}


def _all_cells():
    return [(q, r) for q in range(_BIG_W) for r in range(_BIG_H)]


def test_celllayers_query(dia_client):
    c = dia_client
    c.navigate_to(_STAGE)
    try:
        result = c.poll_checkpoint(_CHECKPOINT, timeout_s=30.0, expected_stage=_STAGE)
        assert result.get("passed"), f"checkpoint failed: {result}"
        c.send_command("cells.select.clear")

        cells = _all_cells()
        # No cell may sit within float32 noise of a threshold used below.
        for fn, th in ((_rainfall, 150.0), (_rainfall, 250.0), (_rainfall, 200.0), (_temperature, 15.0)):
            assert min(abs(fn(*x) - th) for x in cells) > 1e-3, (fn.__name__, th)

        cases = [
            ("rainfall < 150", {x for x in cells if _rainfall(*x) < 150.0}),
            ("biome = desert", {x for x in cells if _biome(*x) == 2}),
            ("biome == jungle or biome == tundra", {x for x in cells if _biome(*x) in (3, 0)}),
            ("(biome = desert or biome = jungle) and rainfall >= 250",
             {x for x in cells if _biome(*x) in (2, 3) and _rainfall(*x) >= 250.0}),
            ("not biome == tundra and temperature >= 15", {x for x in cells if _biome(*x) != 0 and _temperature(*x) >= 15.0}),
        ]

        def sel_count():
            return _panel(c)["selection"]["count"]

        q0 = _q_metrics(c)
        runs = 0
        for expr, exp in cases:
            assert len(exp) > 0, expr
            before = sel_count()
            chk = c.send_command("cells.query.check", {"expr": expr})
            assert chk["ok"] is True and chk["count"] == len(exp), (expr, chk, len(exp))
            assert sel_count() == before, ("check changed the selection", expr)
            assert _panel(c)["query"]["check"] == {"ok": True, "count": len(exp), "expr": expr}, _panel(c)["query"]
            run = c.send_command("cells.select.query", {"expr": expr})
            runs += 1
            assert run["ok"] is True and run["count"] == chk["count"], (expr, run, chk)
            assert sel_count() == len(exp), (expr, sel_count(), len(exp))
            assert _panel(c)["query"]["last"] == {"ok": True, "count": len(exp), "expr": expr}, _panel(c)["query"]

        # A repeated identical select leaves the selection as it was (count asserted; version is not exposed).
        expr, exp = cases[1]
        again = c.send_command("cells.select.query", {"expr": expr})
        runs += 1
        assert again["count"] == len(exp) and sel_count() == len(exp), (again, len(exp))
        # Probe exact membership: toggling one matching cell removes exactly it.
        member = sorted(exp)[0]
        assert c.send_command("cells.select.toggle", {"q": member[0], "r": member[1]})["count"] == len(exp) - 1
        c.send_command("cells.select.query", {"expr": expr})
        runs += 1
        assert sel_count() == len(exp)

        # Errors: AutomationError carries the message and the structured object (.payload); the panel record mirrors it.
        rejected = 0
        bad_cases = [
            ("rainfall <", None, False),                    # parse error
            ("rainfal < 100", "rainfall", True),            # unknown layer, close suggestion
            ("biome > 1", None, False),                     # numeric operator on a category layer
            ("biome = nope", None, False),                  # unknown enum id
            ("", "empty query", False),
        ]
        for cmd in ("cells.query.check", "cells.select.query"):
            for expr, text, has_sugg in bad_cases:
                before = sel_count()
                try:
                    c.send_command(cmd, {"expr": expr})
                except AutomationError as e:
                    # The structured error object reaches the client: message in the exception, members in e.payload.
                    assert e.payload.get("message") and str(e) == e.payload["message"], (cmd, expr, str(e), e.payload)
                    assert isinstance(e.payload.get("column"), int) and isinstance(e.payload.get("length"), int), e.payload
                    assert ("suggestion" in e.payload) == has_sugg, e.payload
                    if has_sugg:
                        assert e.payload["suggestion"] == text, e.payload
                    elif text:
                        assert text in e.payload["message"], e.payload
                else:
                    raise AssertionError(f"{cmd} {expr!r} should have failed")
                if cmd == "cells.select.query":
                    rejected += 1
                assert sel_count() == before == len(exp), (cmd, expr)
                rec = _panel(c)["query"]["check" if cmd == "cells.query.check" else "last"]
                assert rec["expr"] == expr and rec.get("ok") is not True, rec
                err = rec["error"]
                assert err["message"] and isinstance(err["column"], int) and isinstance(err["length"], int), rec
                assert ("suggestion" in err) == has_sugg, rec
                if has_sugg:
                    assert err["suggestion"] == text, rec
                elif text:
                    assert text in err["message"], rec
                if expr == "rainfall <":
                    assert err["column"] >= len("rainfall"), rec  # points at the missing operand, not the start

        # last (select) is not clobbered by check, and vice versa.
        c.send_command("cells.query.check", {"expr": "rainfall < 150"})
        q = _panel(c)["query"]
        assert q["available"] is True and q["check"]["ok"] is True and q["last"].get("ok") is not True, q

        # Metrics: select successes -> run, select rejects -> rejected; check never counts.
        q1 = _q_metrics(c)
        assert q1["run"] - q0["run"] == runs, (q0, q1, runs)
        assert q1["rejected"] - q0["rejected"] == rejected, (q0, q1, rejected)
        assert c.get_metric(_Q + "last_match_count") == len([x for x in cells if _rainfall(*x) < 150.0]), \
            c.get_metric(_Q + "last_match_count")
        assert c.get_metric(_Q + "evaluate_ms") >= 0.0

        c.send_command("cells.select.clear")
        assert sel_count() == 0
    finally:
        c.abort_stage()
        c.navigate_to("Boot")
