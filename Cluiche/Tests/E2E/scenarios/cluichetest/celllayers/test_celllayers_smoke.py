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
    finally:
        c.abort_stage()
        c.navigate_to("Boot")
