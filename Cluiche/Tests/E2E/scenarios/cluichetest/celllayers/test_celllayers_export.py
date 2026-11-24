"""CellLayersViewerTestStage history export (DiaCellHistory Feature 5, task 5.10): cells.history.export writes a
dia.cell_history v1 file, and cells.history.meta / aggregates / events / cells page the same data back to a client.

Flow: record >= 12 samples with a few cluichetest.celllayers.marker events, track "rainfall" in addition to the configured
"temperature" (so rainfall's per-cell window starts after sample 0), export with cells:true, read the file from disk, decode it
(base64, little-endian, see Dia/DiaCellHistory/Testing/Conformance/README.md) and compare every query against it. Recording
continues while the test runs, so every query is bounded with the exported sample count N (`to` is exclusive everywhere) and
compares byte-for-byte (so NaN bit patterns compare equal). The topics cells.history.aggregates / cells.history.events are
subscribed first thing and checked at the end. Every sim-hosted reply must re-serialise under 16,384 bytes.
"""
import base64
import json
import os
import re
import struct
import tempfile
import time

from client import AutomationError

_STAGE = "CellLayersViewerTestStage"
_CHECKPOINT = "test.cell_layers_viewer.passed"
_P = "cluichetest.celllayers."
_MARKER = "cluichetest.celllayers.marker"
_TOPIC_AGG = "cells.history.aggregates"
_TOPIC_EVT = "cells.history.events"
_LIMIT = 16384
_PAGE = 1000   # cells.layer.values per-call cap
_MAX_PAGES = 400


def _wait(pred, timeout_s=5.0, interval_s=0.1):
    def _try():
        try:
            return pred()
        except AutomationError:
            return False
    deadline = time.time() + timeout_s
    while time.time() < deadline:
        if _try():
            return True
        time.sleep(interval_s)
    return _try()


def _vc(c, name, params=None):
    """Sim-hosted success is the raw handler result; failure is {"error": ...}. Every reply must fit the host limit."""
    data = c.send_command(name, params)
    size = len(json.dumps(data, separators=(",", ":")))
    assert size < _LIMIT, (name, params, size)
    if isinstance(data, dict) and "error" in data:
        err = data["error"]
        if isinstance(err, dict):
            raise AutomationError(err.get("message", "error"), payload=err)
        raise AutomationError(str(err), payload={})
    return data


def _err(c, name, params=None):
    """Runs a command that must fail; returns the error text."""
    try:
        _vc(c, name, params)
    except AutomationError as e:
        return str(e)
    raise AssertionError(f"{name} {params} unexpectedly succeeded")


def _hstatus(c):
    return _vc(c, "cells.history.status")


def _values(c, layer):
    d = _vc(c, "cells.layer.values", {"layer": layer, "offset": 0, "count": _PAGE})
    raw = base64.b64decode(d["data"])
    return list(struct.unpack("<%df" % (len(raw) // 4), raw))


def _b(s):
    return base64.b64decode(s)


def _f32(raw):
    return struct.unpack("<%df" % (len(raw) // 4), raw)


def _u16(raw):
    return struct.unpack("<%dH" % (len(raw) // 2), raw)


def _same_floats(a, b):
    """Float tuples equal with NaN == NaN."""
    return len(a) == len(b) and all((x == y) or (x != x and y != y) for x, y in zip(a, b))


def _dequant(codes, mn, mx):
    return [float("nan") if q == 0xFFFF else mn + q / 65534.0 * (mx - mn) for q in codes]


def _pages(c, name, params, cursor_to_params, max_pages=_MAX_PAGES):
    """Follows `next` cursors; yields each reply. cursor_to_params(next) -> dict merged over params."""
    p = dict(params)
    for _ in range(max_pages):
        r = _vc(c, name, p)
        yield r
        nx = r.get("next")
        if not nx:
            return
        p = dict(params)
        p.update(cursor_to_params(nx))
    raise AssertionError(f"{name} did not finish within {max_pages} pages")


def _read(path):
    with open(path, "rb") as f:
        return f.read()


def _agg_flat(group_entry):
    """{stat: bytes} for a float group entry {min,mean,max}, or {"counts": bytes} for a category group (b64 string)."""
    if isinstance(group_entry, dict):
        return {k: _b(v) for k, v in group_entry.items()}
    return {"counts": _b(group_entry)}


def _check_aggregates(c, f, n):
    """Pages cells.history.aggregates (one layer at a time, [aggFirst, N)) and compares with the file's arrays."""
    hdr = f["header"]
    agg_first = hdr["aggregateFirst"]
    want = n - agg_first
    group_ids = [g["id"] for g in f["groups"]]
    assert f["aggregates"], "file has no aggregate layers"
    samples_seen = None
    sizes_seen = {g: b"" for g in group_ids}
    for layer, fl in f["aggregates"].items():
        acc = {g: {} for g in group_ids}
        count = 0
        samples = []
        sizes = {g: b"" for g in group_ids}
        for r in _pages(c, "cells.history.aggregates", {"from": agg_first, "to": n, "layers": [layer]},
                        lambda nx: {"from": nx["from"]}):
            assert r["count"] >= 1 and len(r["samples"]) == r["count"], r.get("count")
            count += r["count"]
            samples += r["samples"]
            for g in group_ids:
                sizes[g] += _b(r["groupSizes"][g])
                for k, v in _agg_flat(r["rows"][layer]["groups"][g]).items():
                    acc[g][k] = acc[g].get(k, b"") + v
        assert count == want, (layer, count, want)
        assert len(samples) == want
        for g in group_ids:
            assert len(sizes[g]) == 2 * want, (g, len(sizes[g]))
            for k, got in acc[g].items():
                exp = _agg_flat(fl["groups"][g])[k]
                # The file covers [layer first, N); the query pads the head ([aggFirst, first)) with NaN / zero counts.
                assert len(exp) <= len(got), (layer, g, k)
                assert got[len(got) - len(exp):] == exp, (layer, g, k)
                pad = got[:len(got) - len(exp)]
                if k == "counts":
                    assert set(pad) <= {0}, (layer, g)
                else:
                    assert all(x != x for x in _f32(pad)), (layer, g, k)
        # Group sizes and sample headers are layer independent.
        if samples_seen is None:
            samples_seen = samples
            sizes_seen = sizes
            for g in group_ids:
                assert sizes[g] == _b(f["groupSizes"][g]), g
            fs = f["samples"][agg_first - hdr["sampleFirst"]:]
            assert [(s["tick"], s["label"]) for s in samples] == [(s["tick"], s["label"]) for s in fs]
        else:
            assert samples == samples_seen and sizes == sizes_seen
    return len(f["aggregates"])


def _check_events(c, f, n):
    got = []
    for r in _pages(c, "cells.history.events", {"to": n}, lambda nx: {"from": nx["from"], "skip": nx["skip"]}):
        got += r["events"]
    want = [e for e in f["events"] if e["sample"] < n]
    assert len(want) >= 3, want
    assert got == want, (len(got), len(want))
    # Same events through a type filter.
    gm = []
    for r in _pages(c, "cells.history.events", {"to": n, "types": [_MARKER]},
                    lambda nx: {"from": nx["from"], "skip": nx["skip"]}):
        gm += r["events"]
    assert gm == [e for e in want if e["type"] == _MARKER] and len(gm) >= 3
    assert len({(e["sample"], e.get("payload", {}).get("note")) for e in gm}) == len(gm)   # no duplicates
    return len(got)


def _check_cells(c, f, layer, n):
    """Pages cells.history.cells over the whole retained range and compares with the file's per-cell codes."""
    fc = f["cells"][layer]
    first, count, w, h = fc["first"], fc["count"], fc["width"], fc["height"]
    assert first + count == n and fc["kind"] == "float" and w * h > 0, fc
    rows = {}
    last = None
    for r in _pages(c, "cells.history.cells", {"layer": layer, "from": first, "to": n},
                    lambda nx: {"from": nx["from"], "cellOffset": nx["cellOffset"]}):
        assert r["layer"] == layer and r["kind"] == "float" and r["cellTotal"] == w * h, r["cellTotal"]
        assert (r["min"], r["max"]) == (fc["min"], fc["max"]), (r["min"], r["max"])
        for i, d in enumerate(r["data"]):
            s = r["from"] + i
            rows[s] = rows.get(s, b"") + _b(d)
        last = r
    assert last is not None
    assert sorted(rows) == list(range(first, n)), (sorted(rows)[:3], first, n)
    for s in range(first, n):
        exp = _b(fc["data"][s - first])
        assert len(exp) == 2 * w * h and rows[s] == exp, s
    return rows


def _check_meta(c, f, tracked_now):
    groups, layers = [], []
    core = None
    for r in _pages(c, "cells.history.meta", {}, lambda nx: {"part": nx["part"], "offset": nx["offset"]}):
        if core is None:
            core = r
            assert r["part"] == "core", r["part"]
        groups += r["groups"]
        layers += r["layers"]
    assert re.fullmatch(r"[0-9a-f]{16}", core["structure"]), core["structure"]
    assert [g["id"] for g in groups] == [g["id"] for g in f["groups"]], groups
    assert [l["id"] for l in layers] == list(f["aggregates"].keys()) or \
        sorted(l["id"] for l in layers) == sorted(f["aggregates"].keys()), layers
    assert core["tracked"] == tracked_now == f["tracked"], (core["tracked"], tracked_now, f["tracked"])
    assert core["header"]["world"] == f["header"]["world"]
    assert core["header"]["sampleFirst"] == f["header"]["sampleFirst"]
    assert core["header"]["aggregateFirst"] == f["header"]["aggregateFirst"]
    assert core["header"]["cellFirst"] == f["header"]["cellFirst"]
    assert core["header"]["sampleCount"] >= f["header"]["sampleCount"]
    # Schema pages concatenate to the file's embedded schema.
    entries, ramps = [], []
    for r in _pages(c, "cells.history.meta", {"part": "schema"}, lambda nx: {"part": "schema", "offset": nx["offset"]}):
        assert r["part"] == "schema"
        entries += r["entries"]
        ramps += r["ramps"]
    assert entries == f["schema"]["entries"] and ramps == f["schema"]["ramps"]
    return core["structure"]


def _drain_topic(c, topic, timeout_s, pred):
    deadline = time.time() + timeout_s
    seen = 0
    while time.time() < deadline:
        try:
            m = c.wait_for_topic(topic, timeout_s=min(1.0, max(0.05, deadline - time.time())))
        except TimeoutError:
            continue
        seen += 1
        if pred(m):
            return m
    raise AssertionError(f"topic {topic}: no matching message in {timeout_s}s ({seen} seen)")


def test_celllayers_export(dia_client):
    c = dia_client
    c.navigate_to(_STAGE)
    name = f"e2e_export_{os.getpid()}.cell_history.json"
    abs_tmp = os.path.join(tempfile.gettempdir(), f"e2e_export_abs_{os.getpid()}.cell_history.json")
    written = [abs_tmp]
    try:
        result = c.poll_checkpoint(_CHECKPOINT, timeout_s=30.0, expected_stage=_STAGE)
        assert result.get("passed"), f"checkpoint failed: {result}"
        assert _wait(lambda: _hstatus(c)["sampleCount"] >= 2, 15.0), _hstatus(c)
        assert _hstatus(c)["tracked"] == ["temperature"], _hstatus(c)
        c.subscribe_topic(_TOPIC_AGG)
        c.subscribe_topic(_TOPIC_EVT)

        # Samples (~1/s) with marker events; the animation stays frozen (its default), so rainfall and temperature are static.
        a = _vc(c, _P + "history_emit", {"note": "a", "value": 1.0})
        assert a["ok"] is True, a
        assert _wait(lambda: _hstatus(c)["sampleCount"] >= 6, 30.0), _hstatus(c)
        b = _vc(c, _P + "history_emit", {"note": "b", "value": 2.0})
        assert b["ok"] is True, b
        assert _wait(lambda: _hstatus(c)["sampleCount"] >= 12, 40.0), _hstatus(c)

        # Track rainfall now so its per-cell window starts late; record a few more samples, then one more event.
        n_track = _hstatus(c)["sampleCount"]
        t = _vc(c, "cells.history.track", {"layers": ["rainfall"]})
        assert t["ok"] is True, t
        assert _wait(lambda: _hstatus(c)["sampleCount"] >= n_track + 3, 20.0), _hstatus(c)
        cc = _vc(c, _P + "history_emit", {"note": "c", "value": 3.0})
        assert cc["ok"] is True, cc
        assert _wait(lambda: _hstatus(c)["sampleCount"] >= cc["sample"] + 2, 20.0), _hstatus(c)
        tracked_now = _hstatus(c)["tracked"]
        assert sorted(tracked_now) == ["rainfall", "temperature"], tracked_now

        # --- Export (cells:true).
        s_before = _hstatus(c)["sampleCount"]
        ex = _vc(c, "cells.history.export", {"path": name, "cells": True})
        s_after = _hstatus(c)["sampleCount"]
        assert ex["ok"] is True and ex["bytes"] > 0 and os.path.isabs(ex["path"]), ex
        written.append(ex["path"])
        assert name in ex["path"].replace("\\", "/"), ex["path"]
        assert s_before <= ex["samples"] <= s_after, (s_before, ex["samples"], s_after)
        path = ex["path"]
        assert os.path.getsize(path) == ex["bytes"]

        # Refused without overwrite: error says the file exists, the file is untouched.
        before_bytes = _read(path)
        msg = _err(c, "cells.history.export", {"path": name, "cells": True})
        assert "exist" in msg or "overwrite" in msg, msg
        assert _read(path) == before_bytes

        # With overwrite it succeeds (it may hold more samples now).
        ex2 = _vc(c, "cells.history.export", {"path": name, "cells": True, "overwrite": True})
        assert ex2["ok"] is True and ex2["path"] == path and ex2["bytes"] > 0 and ex2["samples"] >= ex["samples"], ex2
        assert os.path.getsize(path) == ex2["bytes"]
        assert not [p for p in os.listdir(os.path.dirname(path)) if p.startswith(name) and p != name], "temp file left behind"

        # --- Decode the file.
        with open(path, "r", encoding="utf-8") as fh:
            f = json.load(fh)
        n = f["header"]["sampleCount"]
        assert f["format"] == "dia.cell_history" and f["version"] == 1, (f["format"], f["version"])
        assert n == ex2["samples"] >= 12, (n, ex2["samples"])
        assert len(f["samples"]) == n - f["header"]["sampleFirst"], (len(f["samples"]), n)
        assert f["tracked"] == tracked_now
        assert sorted(f["cells"].keys()) == sorted(tracked_now)
        agg_first = f["header"]["aggregateFirst"]
        for layer, fl in f["aggregates"].items():
            groups = fl["groups"]
            assert groups, layer
            for g, entry in groups.items():
                if fl["kind"] == "float":
                    for stat in ("min", "mean", "max"):
                        assert len(_f32(_b(entry[stat]))) == n - fl["first"], (layer, g, stat)
                else:
                    assert len(_u16(_b(entry))) == (n - fl["first"]) * len(fl["values"]), (layer, g)
        for g in f["groups"]:
            assert len(_u16(_b(f["groupSizes"][g["id"]]))) == n - agg_first, g
        assert sum(1 for e in f["events"] if e["type"] == _MARKER) >= 3, f["events"]
        assert f["cells"]["rainfall"]["first"] > 0   # tracked late
        assert f["cells"]["rainfall"]["first"] + f["cells"]["rainfall"]["count"] == n

        # The per-cell codes dequantise to the live values (static layers) and to the cells.history.cells page values.
        for layer in ("rainfall", "temperature"):
            fc = f["cells"][layer]
            cells = fc["width"] * fc["height"]
            last = _u16(_b(fc["data"][-1]))
            assert len(last) == cells
            vals = _dequant(last, fc["min"], fc["max"])
            live = _values(c, layer)
            tol = (fc["max"] - fc["min"]) / 65534.0 + 1e-3
            for i in range(min(len(live), _PAGE)):
                assert abs(vals[i] - live[i]) <= tol, (layer, i, vals[i], live[i])

        # --- Compare with the queries.
        structure = _check_meta(c, f, tracked_now)
        n_layers = _check_aggregates(c, f, n)
        n_events = _check_events(c, f, n)
        for layer in tracked_now:
            _check_cells(c, f, layer, n)
        # The aggregates reply carries the same structure hash as meta.
        r = _vc(c, "cells.history.aggregates", {"from": n - 1, "to": n, "layers": ["rainfall"]})
        assert r["structure"] == structure and r["count"] == 1 and "next" not in r, r
        # A specific cell list: two cells of one sample equal the file's codes.
        fc = f["cells"]["rainfall"]
        s_probe = n - 1
        r = _vc(c, "cells.history.cells", {"layer": "rainfall", "from": s_probe, "to": s_probe + 1, "cells": [0, 5]})
        assert r["cells"] == [0, 5] and r["cellCount"] == 2, r
        full = _u16(_b(fc["data"][s_probe - fc["first"]]))
        assert _u16(_b(r["data"][0])) == (full[0], full[5])

        # --- Error cases.
        assert _err(c, "cells.history.cells", {"layer": "biome", "from": 0, "to": 1})          # not tracked
        assert _err(c, "cells.history.cells", {"layer": "no_such_layer"})
        assert _err(c, "cells.history.cells", {})                                               # missing layer
        assert _err(c, "cells.history.cells", {"layer": "rainfall", "from": n + 100, "to": n + 200})   # past the window
        assert _err(c, "cells.history.cells", {"layer": "rainfall", "from": fc["first"] - 1, "to": fc["first"] + 1})   # before its window
        assert _err(c, "cells.history.cells", {"layer": "rainfall", "cells": []})
        assert _err(c, "cells.history.aggregates", {"from": "x"})
        assert _err(c, "cells.history.aggregates", {"layers": "temperature"})
        assert _err(c, "cells.history.aggregates", {"layers": ["no_such_layer"]})
        assert _err(c, "cells.history.events", {"types": "x"})
        assert _err(c, "cells.history.events", {"from": -1})
        assert _err(c, "cells.history.meta", {"offset": "x"})
        assert _err(c, "cells.history.export", {})
        assert _err(c, "cells.history.export", {"path": name, "cells": "yes", "overwrite": True})
        assert _err(c, "cells.history.export", {"path": name, "overwrite": "yes"})
        for bad in ("../" + name, "..\\" + name, "sub/../../" + name):
            assert _err(c, "cells.history.export", {"path": bad}), bad
        assert _read(path) == _read(ex2["path"])   # still the overwritten file, intact

        # An absolute path outside the history folder is allowed and written as given.
        ab = _vc(c, "cells.history.export", {"path": abs_tmp, "overwrite": True})
        assert ab["ok"] is True and os.path.normcase(ab["path"]) == os.path.normcase(abs_tmp) and os.path.isfile(abs_tmp), ab
        with open(abs_tmp, "r", encoding="utf-8") as fh:
            nf = json.load(fh)
        assert nf["format"] == "dia.cell_history" and "cells" not in nf   # no cells without the flag

        # --- Topics (subscribed at the start of the test; both publish only new data).
        m = _drain_topic(c, _TOPIC_AGG, 20.0, lambda p: isinstance(p, dict) and p.get("count", 0) >= 1)
        assert re.fullmatch(r"[0-9a-f]{16}", m["structure"]), m["structure"]
        assert len(m["samples"]) == m["count"] and m["fromSample"] >= 0
        assert len(_u16(_b(m["groupSizes"]["world"]))) == m["count"]
        assert set(m["rows"].keys()) == set(f["aggregates"].keys()), list(m["rows"].keys())
        e = _drain_topic(c, _TOPIC_EVT, 20.0,
                         lambda p: any(ev.get("type") == _MARKER for ev in p.get("events", [])))
        assert any(ev["type"] == _MARKER and "sample" in ev for ev in e["events"]), e
        assert n_layers >= 2 and n_events >= 3
    finally:
        for p in written:
            try:
                os.remove(p)
            except OSError:
                pass
        c.abort_stage()
        c.navigate_to("Boot")
