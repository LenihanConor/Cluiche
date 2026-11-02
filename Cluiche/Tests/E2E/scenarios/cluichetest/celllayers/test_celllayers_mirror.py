"""CellLayersViewerTestStage editor mirror (DiaCellLayers Feature 5): topic cells.view.state and the four mirror queries
(cells.layer.list, cells.layer.values, cells.select.get, cells.select.stats). Read-only: queries never change the topic state.
"""
import base64
import json
import math
import struct
import time
from pathlib import Path

from client import AutomationError

_STAGE = "CellLayersViewerTestStage"
_CHECKPOINT = "test.cell_layers_viewer.passed"
_TOPIC = "cells.view.state"
_W, _H = 66, 42  # grid A (active at start)
_CHUNK = 1000  # per-call limit of cells.layer.values / cells.select.get (16 KB sim-hosted reply cap)
_M64 = (1 << 64) - 1


def mix(i):
    """SplitMix64 finalizer over i + golden gamma (the selection hash element)."""
    z = (i + 0x9E3779B97F4A7C15) & _M64
    z = ((z ^ (z >> 30)) * 0xBF58476D1CE4E5B9) & _M64
    z = ((z ^ (z >> 27)) * 0x94D049BB133111EB) & _M64
    return z ^ (z >> 31)


def sel_hash(indices):
    h = 0
    for i in indices:
        h ^= mix(i)
    return f"{h:016x}"


def _vectors():
    for p in Path(__file__).resolve().parents:
        f = p / "Dia" / "DiaCellLayers" / "Testing" / "Conformance" / "selection_hash.json"
        if f.is_file():
            return json.loads(f.read_text(encoding="utf-8"))["vectors"]
    raise FileNotFoundError("selection_hash.json")


def test_selection_hash_port_matches_shared_vectors():
    vectors = _vectors()
    assert len(vectors) >= 5
    for v in vectors:
        assert sel_hash(v["indices"]) == v["hash"], v["name"]


def _drain(c, quiet_s=0.5):
    """All topic payloads that arrive until the topic is quiet for quiet_s."""
    out = []
    while True:
        try:
            out.append(c.wait_for_topic(_TOPIC, timeout_s=quiet_s))
        except TimeoutError:
            return out


def _wait_state(c, pred, what, timeout_s=8.0):
    """Collect topic payloads until pred(payload) holds; returns (matching payload, all payloads seen)."""
    seen = []
    deadline = time.time() + timeout_s
    while time.time() < deadline:
        try:
            p = c.wait_for_topic(_TOPIC, timeout_s=min(1.0, max(0.05, deadline - time.time())))
        except TimeoutError:
            continue
        seen.append(p)
        if pred(p):
            return p, seen
    raise AssertionError(f"topic never reached: {what}; seen={seen[-3:]}")


def _rainfall(x, y):
    return min(400.0, max(0.0, 200.0 + 180.0 * math.sin(x * 0.2 + 1.0) * math.sin(y * 0.25 + 2.0)))


def _decode(data):
    raw = base64.b64decode(data)
    assert len(raw) % 4 == 0
    return raw, list(struct.unpack("<%df" % (len(raw) // 4), raw))


def test_celllayers_mirror(dia_client):
    c = dia_client
    c.navigate_to(_STAGE)
    try:
        result = c.poll_checkpoint(_CHECKPOINT, timeout_s=30.0, expected_stage=_STAGE)
        assert result.get("passed"), f"checkpoint failed: {result}"
        c.send_command("cells.select.clear")

        # 1. Subscribing replays the retained state; settle to the latest.
        c.subscribe_topic(_TOPIC)
        first = c.wait_for_topic(_TOPIC, timeout_s=8.0)
        tail = _drain(c, 0.6)
        st0 = tail[-1] if tail else first
        assert st0["grid"] == {"id": "test.hex", "topology": "hex.flat.oddq", "width": _W, "height": _H}, st0["grid"]
        assert st0["base"] == "temperature", st0
        assert st0["overlays"] == [], st0["overlays"]
        assert st0["selection"]["count"] == 0 and st0["selection"]["hash"] == "0000000000000000", st0["selection"]
        assert st0["history"] is None, st0
        assert st0["viewVersion"] >= 1 and st0["layersVersion"] >= 1, st0

        # 2. Base change: new update, larger viewVersion, layersVersion unchanged.
        c.send_command("cells.layer.base_by_id", {"id": "biome"})
        st1, _ = _wait_state(c, lambda p: p["base"] == "biome", "base == biome")
        assert st1["viewVersion"] > st0["viewVersion"], (st0["viewVersion"], st1["viewVersion"])
        assert st1["layersVersion"] == st0["layersVersion"], (st0["layersVersion"], st1["layersVersion"])
        assert st1["grid"] == st0["grid"]

        # 3. Selection: topic hash equals the client-side SplitMix64 XOR; cells.select.get agrees.
        cells = [[1, 1], [2, 2], [5, 7], [65, 41]]
        expected = sorted(r * _W + q for q, r in cells)
        c.send_command("cells.select.cells", {"cells": cells})
        st2, _ = _wait_state(c, lambda p: p["selection"]["count"] == len(cells), "selection count")
        sel = st2["selection"]
        assert sel["hash"] == sel_hash(expected), (sel, sel_hash(expected))
        assert st2["viewVersion"] > st1["viewVersion"]
        got = c.send_command("cells.select.get")
        assert got["indices"] == expected, got
        assert got["hash"] == sel["hash"] and got["version"] == sel["version"] and got["total"] == len(cells), (got, sel)
        part = c.send_command("cells.select.get", {"offset": 1, "count": 2})
        assert part["indices"] == expected[1:3] and part["offset"] == 1, part
        assert "error" in c.send_command("cells.select.get", {"offset": 99})

        # 4. Coalescing: a burst of changes yields <= ~10 updates per second and delivers the final state.
        _drain(c, 0.4)
        burst = [(q, 3) for q in range(10, 22)]
        t0 = time.time()
        for q, r in burst:
            c.send_command("cells.select.cell", {"q": q, "r": r})
        final_idx = [burst[-1][1] * _W + burst[-1][0]]
        last, seen = _wait_state(c, lambda p: p["selection"]["hash"] == sel_hash(final_idx), "final burst state")
        seen += _drain(c, 0.5)
        elapsed = time.time() - t0
        updates = len(seen)
        print(f"coalescing: {len(burst)} changes -> {updates} updates in {elapsed:.2f}s")
        assert updates <= 10 * elapsed + 2, (updates, elapsed)
        assert seen[-1]["selection"]["hash"] == sel_hash(final_idx), seen[-1]
        vv = [p["viewVersion"] for p in seen]
        assert vv == sorted(vv) and len(set(vv)) == len(vv), vv

        # 5. Read-only queries: state before == after, and no topic update is caused.
        before = seen[-1]
        _drain(c, 0.3)
        lst = c.send_command("cells.layer.list")
        assert lst["layersVersion"] == before["layersVersion"], (lst["layersVersion"], before["layersVersion"])
        by_size = {(g["width"], g["height"]): g for g in lst["grids"]}
        assert sorted(l["id"] for l in by_size[(_W, _H)]["layers"]) == ["biome", "rainfall", "temperature"], lst
        assert all(l["schemaKey"] and l["kind"] in ("float", "category") for g in lst["grids"] for l in g["layers"]), lst
        assert {o["id"] for o in lst["overlays"]} == {"grid_edges"}, lst["overlays"]

        # The call limit is 1,000 cells (sim-hosted replies are capped at 16 KB): no offset/count returns the first page,
        # and a larger count is clamped and echoed.
        total = _W * _H
        one = c.send_command("cells.layer.values", {"layer": "rainfall"})
        assert "error" not in one, one
        assert one["encoding"] == "f32le-base64" and one["gridId"] == "test.hex", one
        assert one["width"] == _W and one["height"] == _H and one["offset"] == 0, one
        raw_one, vals_one = _decode(one["data"])
        assert len(vals_one) == one["count"] == _CHUNK < total, (len(vals_one), total)
        assert one["total"] == total, one
        big = c.send_command("cells.layer.values", {"layer": "rainfall", "offset": 0, "count": total})
        assert big["count"] == _CHUNK and _decode(big["data"])[0] == raw_one, big["count"]

        # Page the whole layer at 1,000 cells per call (3 calls for 2772 cells).
        raw_full = b""
        vals = []
        version = None
        for off in range(0, total, _CHUNK):
            cnt = min(_CHUNK, total - off)
            ch = c.send_command("cells.layer.values", {"layer": "rainfall", "offset": off, "count": cnt})
            assert ch["encoding"] == "f32le-base64" and ch["total"] == total and ch["gridId"] == "test.hex", ch
            assert ch["offset"] == off and ch["count"] == cnt, ch
            assert ch["width"] == _W and ch["height"] == _H, ch
            version = ch["version"] if version is None else version
            assert ch["version"] == version, ch
            raw, part = _decode(ch["data"])
            assert len(part) == cnt, (len(part), cnt)
            raw_full += raw
            vals += part
        assert len(vals) == total, len(vals)
        assert raw_full[:len(raw_one)] == raw_one and vals[:_CHUNK] == vals_one, "first page must equal the default call"
        for idx in (0, 1, 67, 1000, 1999, 2000, total - 1):
            q, r = idx % _W, idx // _W
            assert abs(vals[idx] - _rainfall(q, r)) < 0.5, (idx, vals[idx], _rainfall(q, r))
        # offset == total gives an empty chunk; a chunk re-read at an arbitrary offset matches the same slice.
        end = c.send_command("cells.layer.values", {"layer": "rainfall", "offset": total})
        assert end["count"] == 0 and end["data"] == "", end
        mid = c.send_command("cells.layer.values", {"layer": "rainfall", "offset": 500, "count": 700})
        assert _decode(mid["data"])[0] == raw_full[500 * 4:1200 * 4]
        assert "error" in c.send_command("cells.layer.values", {"layer": "nope"})
        assert "error" in c.send_command("cells.layer.values", {"layer": "rainfall", "offset": _W * _H + 1})

        stats = c.send_command("cells.select.stats")
        assert "error" not in stats, stats
        assert stats["count"] == before["selection"]["count"], (stats["count"], before["selection"])
        assert stats["version"] == before["selection"]["version"] and stats["hash"] == before["selection"]["hash"], \
            (stats, before["selection"])
        all_stats = c.send_command("cells.select.stats", {"layers": "all"})
        assert "error" not in all_stats and all_stats["hash"] == stats["hash"], all_stats

        assert _drain(c, 0.6) == [], "read-only queries must not publish topic updates"
        after = c.send_command("cells.select.get")
        assert after["hash"] == before["selection"]["hash"] and after["version"] == before["selection"]["version"]

        # 6. Large selection: the whole 66x42 grid (2772 cells) is paged through cells.select.get at 1,000 indices per call.
        c.send_command("cells.select.clear")
        c.send_command("cells.select.box", {"q0": 0, "r0": 0, "q1": _W - 1, "r1": _H - 1})
        big, _ = _wait_state(c, lambda p: p["selection"]["count"] == total, "whole-grid selection")
        allsel = c.send_command("cells.select.get")
        assert "error" not in allsel, allsel
        assert allsel["total"] == total and allsel["offset"] == 0, (allsel["total"], allsel["offset"])
        assert allsel["indices"] == list(range(_CHUNK)), len(allsel["indices"])  # the first page is capped at 1,000
        assert allsel["hash"] == big["selection"]["hash"] == sel_hash(range(total)), allsel["hash"]
        # Paging the same selection at 1,000 per call concatenates to the full list; an over-limit count is clamped.
        paged = []
        for off in range(0, total, _CHUNK):
            pg = c.send_command("cells.select.get", {"offset": off, "count": 5000})
            assert pg["offset"] == off and pg["total"] == total, pg
            assert len(pg["indices"]) <= _CHUNK, len(pg["indices"])
            paged += pg["indices"]
        assert paged == list(range(total)), len(paged)

        c.send_command("cells.select.clear")
        c.send_command("cells.layer.base_by_id", {"id": "temperature"})
    finally:
        c.abort_stage()
        c.navigate_to("Boot")
