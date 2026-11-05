"""CellLayersViewerTestStage delta mode (DiaCellHistory Feature 4, delta end to end): cells.history.delta against recorded samples,
the live "now" side and a snapshot side, through cells.layer.values and the cells.view.state.history field.

Fixture: the stage samples every 30 Sim frames; "temperature" is tracked and animates (a near uniform offset per animation step)
only while cellviewer.set_animate is on. Samples taken before the animation started are static; samples taken while it ran
differ, so the test picks side B as the recorded sample whose per-cell values differ most from side A.

Expected values come from the recorder's own per-cell copy (cells.history.cells, 16-bit codes), so the check is independent of the
generator. Quantisation: a recorded side is within range/65,535 of the live value; a delta of two recorded sides within twice that.
"""
import base64
import struct
import time

from client import AutomationError

_STAGE = "CellLayersViewerTestStage"
_CHECKPOINT = "test.cell_layers_viewer.passed"
_TOPIC = "cells.view.state"
_PAGE = 1000      # cells.layer.values per-call cap
_TOL = 0.02       # > 2 * (60 / 65,535) plus float32 slack; far below the 8-degree animation swing
_CELLS = list(range(0, _PAGE, 5))   # the first page, every fifth cell


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
    data = c.send_command(name, params)
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


def _values(c, layer="temperature"):
    d = _vc(c, "cells.layer.values", {"layer": layer, "offset": 0, "count": _PAGE})
    raw = base64.b64decode(d["data"])
    return list(struct.unpack("<%df" % (len(raw) // 4), raw))


def _state_history(c, pred, what, timeout_s=8.0):
    """Waits on the cells.view.state topic until its history field satisfies pred; returns that history."""
    seen = []
    deadline = time.time() + timeout_s
    while time.time() < deadline:
        try:
            p = c.wait_for_topic(_TOPIC, timeout_s=min(1.0, max(0.05, deadline - time.time())))
        except TimeoutError:
            continue
        h = p.get("history")
        seen.append(h)
        if pred(h):
            return h
    raise AssertionError(f"cells.view.state.history never reached: {what}; seen={seen[-4:]}")


def _still(c, a, b):
    """The topic only publishes on a change, so unchanged is asserted through a no-op command that reports the state."""
    r = _vc(c, "cells.history.delta", {"groupside": "B"})
    assert r["mode"] == "delta" and r["a"] == a and r["b"] == b and r["groupside"] == "B", r


def _is_delta(a, b):
    return lambda h: isinstance(h, dict) and h.get("mode") == "delta" and h.get("a") == a and h.get("b") == b


def _is_live(h):
    return isinstance(h, dict) and h.get("mode") == "live"


def _sample_values(c, lo, hi):
    """Per-sample dequantised temperature at _CELLS for samples lo..hi-1, from the recorder's per-cell copy."""
    out = {}
    nxt = {"from": lo, "to": hi, "cells": _CELLS}
    while nxt is not None:
        r = _vc(c, "cells.history.cells", dict(layer="temperature", **nxt))
        mn, mx = r["min"], r["max"]
        for i, d in enumerate(r["data"]):
            raw = base64.b64decode(d)
            codes = struct.unpack("<%dH" % (len(raw) // 2), raw)
            out.setdefault(r["from"] + i, []).extend(mn + q * (mx - mn) / 65535.0 for q in codes)
        nx = r.get("next")
        nxt = None if nx is None else {"from": nx["from"], "to": hi, "cells": _CELLS, "cellOffset": nx.get("cellOffset", 0)}
    return out


def _close(got, want, what):
    assert len(got) == len(want), (what, len(got), len(want))
    worst = max(abs(g - w) for g, w in zip(got, want))
    assert worst <= _TOL, f"{what}: worst per-cell difference {worst}"


def _delta_expected(vb, va):
    return [b - a for a, b in zip(va, vb)]


def _at_cells(vals):
    return [vals[i] for i in _CELLS]


def _animate(c, seconds):
    _vc(c, "cellviewer.set_animate", {"enabled": True})
    time.sleep(seconds)
    _vc(c, "cellviewer.set_animate", {"enabled": False})
    time.sleep(0.6)   # let the last animation step settle on the Sim thread


def _list(c):
    return {s["name"]: s for s in _vc(c, "cells.snapshot.list")["snapshots"]}


def test_celllayers_delta(dia_client):
    c = dia_client
    c.navigate_to(_STAGE)
    try:
        result = c.poll_checkpoint(_CHECKPOINT, timeout_s=30.0, expected_stage=_STAGE)
        assert result.get("passed"), f"checkpoint failed: {result}"
        assert _wait(lambda: _hstatus(c)["sampleCount"] >= 2, 15.0), _hstatus(c)
        assert "temperature" in _hstatus(c)["tracked"], _hstatus(c)
        c.subscribe_topic(_TOPIC)
        try:
            _vc(c, "cells.snapshot.delete", {"name": "dl"})
        except AutomationError:
            pass
        _vc(c, "cells.history.live")

        # Record >= 6 samples with the temperature changing between them (the first ones are static, the rest animated).
        n0 = _hstatus(c)["sampleCount"]
        _vc(c, "cellviewer.set_animate", {"enabled": True})
        assert _wait(lambda: _hstatus(c)["sampleCount"] >= max(n0 + 5, 7), 40.0), _hstatus(c)
        _vc(c, "cellviewer.set_animate", {"enabled": False})
        time.sleep(0.6)
        n = _hstatus(c)["sampleCount"]
        assert n >= 7, n
        sv = _sample_values(c, 0, n)
        assert sorted(sv) == list(range(n)), sorted(sv)

        # Side A = the first sample (static baseline); side B = the recorded sample that differs from it the most.
        sa = 1
        sb = max(range(n), key=lambda s: max(abs(x - y) for x, y in zip(sv[s], sv[sa])))
        assert max(abs(x - y) for x, y in zip(sv[sb], sv[sa])) > 0.5, "recorded samples do not differ; the animation did not run"
        A, B = f"sample:{sa}", f"sample:{sb}"

        # --- Errors while live: each says why and changes nothing.
        for params, frag in (
                ({"layer": "biome", "a": A, "b": B}, "categor"),
                ({"layer": "no_such_layer", "a": A, "b": B}, "unknown layer"),
                ({"layer": "temperature", "a": A, "b": A}, "identical"),
                ({"layer": "temperature", "a": "sample:x", "b": B}, ""),
                ({"layer": "temperature", "a": "yesterday", "b": B}, ""),
                ({"layer": "temperature", "a": A, "b": f"sample:{n + 1000}"}, "out of range"),
                ({"layer": "temperature", "a": A, "b": "snapshot:nope"}, ""),
                ({"layer": "temperature", "a": A}, ""),
                ({"scale": 5}, "delta"),
                ({"off": True}, "delta")):
            msg = _err(c, "cells.history.delta", params)
            assert frag in msg, (params, msg)
        _state_history(c, _is_live, "still live after the rejected deltas")

        # --- Enter: sample vs sample.
        r = _vc(c, "cells.history.delta", {"layer": "temperature", "a": A, "b": B})
        assert r["ok"] is True and r["mode"] == "delta" and r["a"] == A and r["b"] == B and r["scale"] is None, r
        assert r["layer"] == "temperature" and r["groupside"] == "B", r
        h = _state_history(c, _is_delta(A, B), "delta state")
        assert h.get("mode") == "delta", h    # not live
        got = _values(c)
        _close(_at_cells(got), _delta_expected(sv[sb], sv[sa]), "B - A")

        # --- Rejections inside delta change nothing.
        assert "out of range" in _err(c, "cells.history.delta", {"layer": "temperature", "a": A, "b": f"sample:{n + 1000}"})
        assert "identical" in _err(c, "cells.history.delta", {"layer": "temperature", "a": B, "b": B})
        _still(c, A, B)
        _close(_at_cells(_values(c)), _delta_expected(sv[sb], sv[sa]), "B - A after rejections")
        assert "leave delta mode first" in _err(c, "cells.history.step", {"n": 1})
        assert "leave delta mode first" in _err(c, "cells.history.event", {"direction": "next"})
        _still(c, A, B)

        # --- Scale: fixed, then auto, then the group side.
        r = _vc(c, "cells.history.delta", {"scale": 5})
        assert r["ok"] is True and r["mode"] == "delta" and r["scale"] == 5, r
        h = _state_history(c, lambda h: isinstance(h, dict) and h.get("mode") == "delta" and h.get("scale") == 5, "fixed scale")
        r = _vc(c, "cells.history.delta", {"scale": "auto"})
        assert r["scale"] is None, r
        assert "must be" in _err(c, "cells.history.delta", {"scale": 0})
        assert "must be" in _err(c, "cells.history.delta", {"scale": "wide"})
        r = _vc(c, "cells.history.delta", {"groupside": "A"})
        assert r["ok"] is True and r["groupside"] == "A", r
        assert _err(c, "cells.history.delta", {"groupside": "C"})
        r = _vc(c, "cells.history.delta", {"groupside": "B"})
        assert r["groupside"] == "B", r
        _close(_at_cells(_values(c)), _delta_expected(sv[sb], sv[sa]), "B - A after scale / groupside")

        # --- off returns to the held view; view and live leave delta mode.
        r = _vc(c, "cells.history.delta", {"off": True})
        assert r["ok"] is True and r["mode"] == "live", r
        _state_history(c, _is_live, "live after off")
        _vc(c, "cells.history.view", {"sample": sb})
        r = _vc(c, "cells.history.delta", {"layer": "temperature", "a": A, "b": "now"})
        assert r["mode"] == "delta" and r["b"] == "now", r
        r = _vc(c, "cells.history.delta", {"off": True})
        assert r["mode"] == "sample" and r["sample"] == sb, r      # the held scrub view comes back
        _vc(c, "cells.history.delta", {"layer": "temperature", "a": A, "b": B})
        r = _vc(c, "cells.history.view", {"sample": sa})
        assert r["mode"] == "sample" and r["sample"] == sa, r
        _state_history(c, lambda h: isinstance(h, dict) and h.get("mode") == "sample", "scrub after view")
        _vc(c, "cells.history.delta", {"layer": "temperature", "a": A, "b": B})
        r = _vc(c, "cells.history.live")
        assert r["mode"] == "live", r
        _state_history(c, _is_live, "live after live")

        # --- The now side follows the live values.
        _vc(c, "cells.history.delta", {"layer": "temperature", "a": A, "b": "now"})
        _state_history(c, _is_delta(A, "now"), "delta vs now")
        d1 = _values(c)
        _animate(c, 2.0)                       # live temperature moves
        d2 = _values(c)
        assert max(abs(x - y) for x, y in zip(d1, d2)) > 0.1, "the now side did not follow the animation"
        _vc(c, "cells.history.live")
        _state_history(c, _is_live, "live to read the frozen values")
        live = _values(c)
        _vc(c, "cells.history.delta", {"layer": "temperature", "a": A, "b": "now"})
        _close(_at_cells(_values(c)), _delta_expected(_at_cells(live), sv[sa]), "now - sample")
        _vc(c, "cells.history.live")

        # --- A snapshot side: now - snapshot.
        _vc(c, "cells.snapshot.take", {"name": "dl"})
        assert _wait(lambda: _list(c).get("dl", {}).get("status") == "ready", 10.0), _list(c).get("dl")
        snap = _vc(c, "cells.history.snapshot", {"name": "dl", "layer": "temperature", "offset": 0, "count": _PAGE})
        raw = base64.b64decode(snap["data"])
        mn, mx = snap["min"], snap["max"]
        snap_vals = [mn + q * (mx - mn) / 65535.0 for q in struct.unpack("<%dH" % (len(raw) // 2), raw)]
        _animate(c, 2.0)                       # now moves away from the snapshot
        _vc(c, "cells.history.live")
        _state_history(c, _is_live, "live before the snapshot delta")
        live2 = _values(c)
        assert max(abs(x - y) for x, y in zip(live2, snap_vals)) > 0.1, "now did not move away from the snapshot"
        r = _vc(c, "cells.history.delta", {"layer": "temperature", "a": "snapshot:dl", "b": "now"})
        assert r["ok"] is True and r["a"] == "snapshot:dl" and r["b"] == "now", r
        _state_history(c, _is_delta("snapshot:dl", "now"), "snapshot delta state")
        got = _values(c)
        _close(got, _delta_expected(live2, snap_vals), "now - snapshot")

        # --- Deleting the snapshot while it is the active side: no data, no crash, still in delta mode.
        dl = _vc(c, "cells.snapshot.delete", {"name": "dl"})
        assert dl["ok"] is True, dl
        assert _wait(lambda: "dl" not in _list(c), 10.0), _list(c)
        assert _wait(lambda: "no data" in _err(c, "cells.layer.values", {"layer": "temperature", "offset": 0, "count": 10}), 8.0)
        _state_history(c, lambda h: isinstance(h, dict) and h.get("mode") == "delta", "still in delta mode after the delete")
        assert "snapshot" in _err(c, "cells.history.delta", {"layer": "temperature", "a": "snapshot:dl", "b": "now"})
        r = _vc(c, "cells.history.delta", {"off": True})
        assert r["ok"] is True and r["mode"] == "live", r
        _state_history(c, _is_live, "live after leaving the stale snapshot delta")
        assert len(_values(c)) == _PAGE
    finally:
        for name, params in (("cells.history.live", None), ("cells.snapshot.delete", {"name": "dl"}),
                             ("cellviewer.set_animate", {"enabled": False})):
            try:
                c.send_command(name, params)
            except Exception:
                pass
