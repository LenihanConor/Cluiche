"""CellLayersViewerTestStage history scrub (DiaCellHistory Feature 2, task 2.10): cells.history.view / live / step / event,
the viewed values through cells.layer.values, cells.view.state.history, the untracked "no data" rule and the "cellhistory"
panel domain (banner + timeline card).

Fixture: the stage samples every 30 Sim frames (~1 s). "temperature" is tracked (diaapp config); "rainfall" and "biome" are not.
Temperature animates only while cellviewer.set_animate is on: value = clamp(static(q, r) + 8 * sin((version - 1) * 0.35)), so a
sample recorded while animating differs from the live (frozen) values by a uniform offset, which the test recovers and checks
against the generator. The first samples are recorded before animation starts (offset 0).
"""
import base64
import math
import struct
import time

from client import AutomationError

_STAGE = "CellLayersViewerTestStage"
_CHECKPOINT = "test.cell_layers_viewer.passed"
_TOPIC = "cells.view.state"
_P = "cluichetest.celllayers."
_W, _H = 66, 42
_PAGE = 1000   # cells.layer.values per-call cap; the first page covers cells 0..999
_EVERY = 30    # stage sample period in Sim frames (sample s has label "F<30*s>")


def _wait(pred, timeout_s=5.0, interval_s=0.1):
    """Polls pred; an AutomationError (e.g. 'Unknown command' before the sim-hosted announcement reaches Main) counts as not yet."""
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
    """Sim-hosted commands answer success with the raw handler result; a failure is a payload {"error": ...}."""
    data = c.send_command(name, params)
    if isinstance(data, dict) and "error" in data:
        err = data["error"]
        if isinstance(err, dict):
            raise AutomationError(err.get("message", "error"), payload=err)
        raise AutomationError(str(err), payload={})
    return data


def _cmd(c, name, params=None):
    try:
        return True, _vc(c, name, params)
    except AutomationError as e:
        return False, str(e)


def _clampf(v, lo, hi):
    return lo if v < lo else (hi if v > hi else v)


def _static_temperature(q, r):
    lat = r / (_H - 1) - 0.5
    return _clampf(16.0 + 20.0 * math.sin(q * 0.11) * math.cos(r * 0.14) - 18.0 * lat, -20.0, 40.0)


def _values(c, layer):
    d = _vc(c, "cells.layer.values", {"layer": layer, "offset": 0, "count": _PAGE})
    raw = base64.b64decode(d["data"])
    return list(struct.unpack("<%df" % (len(raw) // 4), raw))


def _offset_of(vals):
    """Uniform animation offset of a temperature page: vals[i] == clamp(static_i + offset). Cells whose static value is far
    from the clamp limits determine it; every such cell must agree."""
    free = [i for i in range(len(vals)) if -10.0 < _static_temperature(i % _W, i // _W) < 30.0]
    assert len(free) > 200, len(free)
    offs = [vals[i] - _static_temperature(i % _W, i // _W) for i in free]
    off = offs[0]
    assert max(abs(o - off) for o in offs) < 0.05, (min(offs), max(offs))
    return off


def _is_anim_offset(off):
    """True when off is 8 * sin(k * 0.35) for an integer version step k (the generator's possible offsets)."""
    return any(abs(off - 8.0 * math.sin(k * 0.35)) < 0.05 for k in range(0, 4000))


def _hstatus(c):
    return _vc(c, "cells.history.status")


def _state_history(c, pred, what, timeout_s=8.0):
    """Waits on the cells.view.state topic until its history field satisfies pred; returns that payload."""
    seen = []
    deadline = time.time() + timeout_s
    while time.time() < deadline:
        try:
            p = c.wait_for_topic(_TOPIC, timeout_s=min(1.0, max(0.05, deadline - time.time())))
        except TimeoutError:
            continue
        seen.append(p.get("history"))
        if pred(p.get("history")):
            return p
    raise AssertionError(f"cells.view.state.history never reached: {what}; seen={seen[-4:]}")


def _card(c, pred, what, timeout_s=10.0):
    """The cellhistory panel domain state. It is a SimPU snapshot published while requested, so a read may be pending or stale;
    poll until a snapshot satisfies pred."""
    deadline = time.time() + timeout_s
    st = None
    while time.time() < deadline:
        st = c.send_command("debug.domain_state", {"id": "cellhistory"})
        if isinstance(st, dict) and not st.get("pending") and st.get("available") and pred(st):
            return st
        time.sleep(0.1)
    raise AssertionError(f"cellhistory card never reached: {what}; last={st}")


def _view(c, sample):
    return _vc(c, "cells.history.view", {"sample": sample})


def test_celllayers_history_scrub(dia_client):
    c = dia_client
    c.navigate_to(_STAGE)
    try:
        result = c.poll_checkpoint(_CHECKPOINT, timeout_s=30.0, expected_stage=_STAGE)
        assert result.get("passed"), f"checkpoint failed: {result}"

        # Live baseline: not scrubbed, the card is available and the banner hidden.
        assert _wait(lambda: _hstatus(c)["sampleCount"] >= 3, 15.0), _hstatus(c)
        assert _hstatus(c)["tracked"] == ["temperature"], _hstatus(c)
        c.subscribe_topic(_TOPIC)
        _state_history(c, lambda h: h == {"mode": "live", "sample": None, "label": None}, "live")
        live0 = _vc(c, "cells.history.live")
        assert live0["ok"] is True and live0["mode"] == "live", live0   # already live: no-op
        card = _card(c, lambda s: s["banner"]["visible"] is False and s["banner"]["state"] == "live", "live banner")
        assert card["baseLayer"] == "temperature" and card["history"]["mode"] == "live", card
        assert card["timeline"]["range"]["count"] >= 3, card["timeline"]["range"]
        assert card["timeline"]["position"]["mode"] == "live", card["timeline"]["position"]

        # Record samples while animating (offset != 0), then freeze the live values.
        n0 = _hstatus(c)["sampleCount"]
        _vc(c, "cellviewer.set_animate", {"enabled": True})
        assert _wait(lambda: _hstatus(c)["sampleCount"] >= n0 + 5, 20.0), _hstatus(c)
        _vc(c, "cellviewer.set_animate", {"enabled": False})
        time.sleep(0.5)   # the frozen version is read on the next upload
        # Two more samples with the frozen value so the latest samples equal the live values.
        n1 = _hstatus(c)["sampleCount"]
        assert _wait(lambda: _hstatus(c)["sampleCount"] >= n1 + 2, 15.0), _hstatus(c)
        total = _hstatus(c)["sampleCount"]

        live_vals = _values(c, "temperature")
        live_off = _offset_of(live_vals)
        assert _is_anim_offset(live_off), live_off

        # 1. view: an older sample. State, topic, banner and values all show that sample.
        offsets = {}
        for s in (0, n0, n0 + 2):
            r = _view(c, s)
            assert r == {"ok": True, "mode": "sample", "sample": s, "label": f"F{_EVERY * s}"}, (s, r)
            _state_history(c, lambda h, s=s: h == {"mode": "sample", "sample": s, "label": f"F{_EVERY * s}"}, f"sample {s}")
            off = _offset_of(_values(c, "temperature"))
            assert _is_anim_offset(off), (s, off)
            offsets[s] = off
        assert abs(offsets[0]) < 0.05, offsets            # recorded before animation: the static field
        assert abs(offsets[n0 + 2] - offsets[0]) > 0.05, offsets   # a later animated sample differs from sample 0
        assert abs(offsets[0] - live_off) > 0.05, (offsets, live_off)   # scrubbed values are not the live values

        # The viewed sample stays put as new samples arrive.
        c_before = _hstatus(c)["sampleCount"]
        assert _wait(lambda: _hstatus(c)["sampleCount"] > c_before, 10.0)
        assert _vc(c, "cells.history.step", {"n": 0})["sample"] == n0 + 2

        # 2. Panel domain: banner shows the sample, the timeline reports the position.
        card = _card(c, lambda s: s["banner"]["visible"] is True and s["banner"]["sample"] == n0 + 2, "scrubbed banner")
        assert card["banner"]["state"] == "data" and card["banner"]["layer"] == "temperature", card["banner"]
        assert card["banner"]["label"] == f"F{_EVERY * (n0 + 2)}", card["banner"]
        assert card["history"] == {"mode": "sample", "sample": n0 + 2, "label": f"F{_EVERY * (n0 + 2)}"}, card["history"]
        assert card["timeline"]["position"] == {"mode": "sample", "sample": n0 + 2}, card["timeline"]["position"]
        assert card["timeline"]["range"]["first"] == 0 and card["timeline"]["range"]["count"] >= total, card["timeline"]["range"]
        assert len(card["timeline"]["buckets"]) > 0, card["timeline"]

        # 3. step -1 / +1 (and clamping) move the viewed sample; the values follow.
        s = n0 + 2
        r = _vc(c, "cells.history.step", {"n": -1})
        assert r["sample"] == s - 1 and r["mode"] == "sample" and r["label"] == f"F{_EVERY * (s - 1)}", r
        off = _offset_of(_values(c, "temperature"))
        assert abs(off - offsets[n0]) < 0.05 if s - 1 == n0 else _is_anim_offset(off), (off, offsets)
        _state_history(c, lambda h: h and h["sample"] == s - 1, "step -1")
        r = _vc(c, "cells.history.step", {"n": 1})
        assert r["sample"] == s, r
        assert abs(_offset_of(_values(c, "temperature")) - offsets[s]) < 0.05
        r = _vc(c, "cells.history.step", {"n": -100000})
        assert r["sample"] == 0, r
        assert abs(_offset_of(_values(c, "temperature")) - offsets[0]) < 0.05
        r = _vc(c, "cells.history.step", {"n": 100000})
        assert r["mode"] == "sample" and r["sample"] == _hstatus(c)["sampleCount"] - 1, r   # clamped to the latest, still scrubbed

        # 4. Error cases leave the view unchanged.
        _view(c, n0)
        for name, params, text in (
            ("cells.history.view", {"sample": 100000}, "out of range"),
            ("cells.history.view", {"sample": -1}, "out of range"),
            ("cells.history.view", {}, "sample"),
            ("cells.history.view", {"sample": "x"}, "sample"),
            ("cells.history.step", {}, "'n'"),
            ("cells.history.step", {"n": "x"}, "'n'"),
            ("cells.history.step", {"n": 1, "band": "nope"}, "band"),
            ("cells.history.event", {}, "direction"),
            ("cells.history.event", {"direction": "sideways"}, "prev or next"),
        ):
            ok, msg = _cmd(c, name, params)
            assert not ok and text in msg, (name, params, ok, msg)
            assert _vc(c, "cells.history.step", {"n": 0})["sample"] == n0, (name, params)
        assert abs(_offset_of(_values(c, "temperature")) - offsets[n0]) < 0.05

        # 5. Untracked layer while scrubbed: no data, never live values. Tracked layer still has data.
        ok, msg = _cmd(c, "cells.layer.values", {"layer": "rainfall", "offset": 0, "count": 10})
        assert not ok and "no data" in msg, (ok, msg)
        ok, msg = _cmd(c, "cells.layer.values", {"layer": "biome", "offset": 0, "count": 10})
        assert not ok and "no data" in msg, (ok, msg)
        assert len(_values(c, "temperature")) == _PAGE
        _vc(c, "cells.layer.base_by_id", {"id": "rainfall"})
        card = _card(c, lambda s: s["banner"]["state"] == "untracked", "untracked banner")
        assert card["banner"]["visible"] is True and card["banner"]["layer"] == "rainfall", card["banner"]
        _vc(c, "cells.layer.base_by_id", {"id": "temperature"})
        _card(c, lambda s: s["banner"]["state"] == "data", "data banner after switching back")

        # 6. Events. The test emitter attaches a marker to the next sample.
        _vc(c, "cells.history.live")
        a = _vc(c, _P + "history_emit", {"note": "a", "value": 1.0})
        assert a["ok"] is True, a
        assert _wait(lambda: _hstatus(c)["sampleCount"] > a["sample"], 10.0)
        assert _wait(lambda: _hstatus(c)["sampleCount"] > a["sample"] + 2, 10.0)
        b = _vc(c, _P + "history_emit", {"note": "b", "value": 2.0})
        assert b["ok"] is True and b["sample"] > a["sample"], (a, b)
        assert _wait(lambda: _hstatus(c)["sampleCount"] > b["sample"] + 1, 10.0)

        r = _vc(c, "cells.history.event", {"direction": "prev"})   # from live: the latest event
        assert r["mode"] == "sample" and r["sample"] == b["sample"], (r, a, b)
        r = _vc(c, "cells.history.event", {"direction": "prev", "type": "cluichetest.celllayers.marker"})
        assert r["sample"] == a["sample"], (r, a)
        ok, msg = _cmd(c, "cells.history.event", {"direction": "prev"})
        assert not ok and "no earlier event" in msg, (ok, msg)
        assert _vc(c, "cells.history.step", {"n": 0})["sample"] == a["sample"]   # unchanged
        r = _vc(c, "cells.history.event", {"direction": "next"})
        assert r["sample"] == b["sample"], (r, b)
        ok, msg = _cmd(c, "cells.history.event", {"direction": "next"})
        assert not ok and "no later event" in msg, (ok, msg)
        assert _vc(c, "cells.history.step", {"n": 0})["sample"] == b["sample"]
        ok, msg = _cmd(c, "cells.history.event", {"direction": "prev", "type": "no.such.type"})
        assert not ok, msg

        # 7. Go live: normal values, history live, banner hidden.
        r = _vc(c, "cells.history.live")
        assert r["ok"] is True and r["mode"] == "live" and r["sample"] >= total, r
        _state_history(c, lambda h: h == {"mode": "live", "sample": None, "label": None}, "live again")
        assert abs(_offset_of(_values(c, "temperature")) - live_off) < 0.05
        ok, data = _cmd(c, "cells.layer.values", {"layer": "rainfall", "offset": 0, "count": 10})
        assert ok and data["count"] == 10, (ok, data)
        _card(c, lambda s: s["banner"]["visible"] is False and s["banner"]["state"] == "live", "banner hidden when live")
    finally:
        c.abort_stage()
        c.navigate_to("Boot")


def test_celllayers_history_scrub_stage_reentry(dia_client):
    """Boot -> CellLayersViewer -> Boot -> CellLayersViewer: the scrub wiring is rebuilt on a fresh recorder (view starts live,
    commands registered again, the domain answers) with no leftover scrub state."""
    c = dia_client
    for round_no in range(2):
        c.navigate_to(_STAGE)
        try:
            result = c.poll_checkpoint(_CHECKPOINT, timeout_s=30.0, expected_stage=_STAGE)
            assert result.get("passed"), f"checkpoint failed (round {round_no}): {result}"
            assert _wait(lambda: _hstatus(c)["sampleCount"] >= 2, 15.0), _hstatus(c)
            r = _view(c, 0)
            assert r["mode"] == "sample" and r["sample"] == 0, (round_no, r)
            _card(c, lambda s: s["banner"]["visible"] is True, "scrubbed banner")
            assert _vc(c, "cells.history.live")["mode"] == "live"
        finally:
            c.abort_stage()
            c.navigate_to("Boot")
