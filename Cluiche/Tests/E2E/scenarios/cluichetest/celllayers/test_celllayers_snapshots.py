"""CellLayersViewerTestStage snapshots (DiaCellHistory Feature 3, task 3.10a): cells.snapshot.take / list / restore / delete and
cells.history.snapshot against the stage-local store (out/CluicheTest/snapshots/<name>.state).

The store saves the stage's animated state (temperature layer version, animate counter, history sampling counter). A restore
therefore visibly rewinds cells.layer.values for "temperature" and truncates the history to the snapshot's sample.

Timing: the stage samples every 30 Sim frames; the store runs a request at the next Sim frame boundary, so every take / restore
is polled to completion. After a restore the stage may record one more sample in the same frame, so the sample count is
snapshot sample + 1 (or + 2 when the sampling cadence lands on the restore frame).
"""
import base64
import struct
import time

from client import AutomationError

_STAGE = "CellLayersViewerTestStage"
_CHECKPOINT = "test.cell_layers_viewer.passed"
_TOPIC = "cells.view.state"
_PAGE = 1000


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


def _values(c, layer):
    d = _vc(c, "cells.layer.values", {"layer": layer, "offset": 0, "count": _PAGE})
    raw = base64.b64decode(d["data"])
    return list(struct.unpack("<%df" % (len(raw) // 4), raw))


def _list(c):
    return {s["name"]: s for s in _vc(c, "cells.snapshot.list")["snapshots"]}


def _snap_status(c, name):
    return _list(c).get(name, {}).get("status")


def _wait_ready(c, name, timeout_s=10.0):
    assert _wait(lambda: _snap_status(c, name) == "ready", timeout_s), _list(c).get(name)


def _wait_gone(c, name, timeout_s=10.0):
    assert _wait(lambda: name not in _list(c), timeout_s), _list(c)


def _live(c):
    """Waits on the cells.view.state topic until the viewer is live."""
    deadline = time.time() + 8.0
    seen = []
    while time.time() < deadline:
        try:
            p = c.wait_for_topic(_TOPIC, timeout_s=0.5)
        except TimeoutError:
            continue
        h = p.get("history")
        seen.append(h)
        if h == {"mode": "live", "sample": None, "label": None}:
            return
    raise AssertionError(f"viewer never went live; seen={seen[-4:]}")


def test_celllayers_snapshots(dia_client):
    c = dia_client
    c.navigate_to(_STAGE)
    names = ("pre", "dup")
    try:
        result = c.poll_checkpoint(_CHECKPOINT, timeout_s=30.0, expected_stage=_STAGE)
        assert result.get("passed"), f"checkpoint failed: {result}"
        assert _wait(lambda: _hstatus(c)["sampleCount"] >= 3, 15.0), _hstatus(c)
        c.subscribe_topic(_TOPIC)

        # Clean slate from earlier runs (the store folder persists on disk).
        for n in names:
            try:
                _vc(c, "cells.snapshot.delete", {"name": n})
            except AutomationError:
                pass
        _wait_gone(c, "pre")

        # Take "pre" with the animation frozen so the layer values are stable and known.
        before = _values(c, "temperature")
        t = _vc(c, "cells.snapshot.take", {"name": "pre"})
        assert t["ok"] is True and t["pending"] is True, t
        _wait_ready(c, "pre")
        snap = _list(c)["pre"]
        s = snap["sample"]
        assert snap["status"] == "ready" and snap["sameRun"] is True and snap["bytes"] > 0, snap
        assert _values(c, "temperature") == before   # taking changes nothing

        # Error cases.
        assert _err(c, "cells.snapshot.take", {"name": "bad name!"})
        assert _err(c, "cells.snapshot.take", {"name": "pre"})   # duplicate without overwrite
        assert _err(c, "cells.snapshot.restore", {"name": "nope"})
        assert _err(c, "cells.snapshot.delete", {"name": "nope"})
        assert _snap_status(c, "pre") == "ready"

        # Let the animation advance and record more samples.
        _vc(c, "cellviewer.set_animate", {"enabled": True})
        assert _wait(lambda: _hstatus(c)["sampleCount"] >= s + 4, 25.0), _hstatus(c)
        _vc(c, "cellviewer.set_animate", {"enabled": False})
        time.sleep(0.5)
        moved = _values(c, "temperature")
        assert moved != before, "animation did not change the temperature layer"
        count_before = _hstatus(c)["sampleCount"]
        assert count_before >= s + 4

        # Scrub back so the restore has to send the viewer to live.
        _vc(c, "cells.history.view", {"sample": 0})

        r = _vc(c, "cells.snapshot.restore", {"name": "pre"})
        assert r["ok"] is True and r["pending"] is True, r
        assert _wait(lambda: _hstatus(c)["sampleCount"] < count_before, 10.0), _hstatus(c)
        n = _hstatus(c)["sampleCount"]
        assert n in (s + 1, s + 2), (n, s)
        _live(c)
        assert _wait(lambda: _values(c, "temperature") == before, 10.0), "temperature layer not restored to its value at take time"

        # The per-cell copy decodes to the same quantised values.
        d = _vc(c, "cells.history.snapshot", {"name": "pre", "layer": "temperature"})
        assert d["kind"] == "float" and d["encoding"], d
        assert d["total"] == len(before) and d["offset"] == 0 and d["count"] == d["total"], d
        raw = base64.b64decode(d["data"])
        codes = struct.unpack("<%dH" % (len(raw) // 2), raw)
        assert len(codes) == len(before)
        mn, mx = d["min"], d["max"]
        tol = (mx - mn) / 65535.0 + 1e-3
        for i, q in enumerate(codes):
            assert abs((mn + (q / 65535.0) * (mx - mn)) - before[i]) <= tol, (i, q, before[i])
        assert _err(c, "cells.history.snapshot", {"name": "pre", "layer": "no_such_layer"})
        assert _err(c, "cells.history.snapshot", {"name": "nope", "layer": "temperature"})

        # Delete removes both halves.
        dl = _vc(c, "cells.snapshot.delete", {"name": "pre"})
        assert dl["ok"] is True, dl
        _wait_gone(c, "pre")
        assert _err(c, "cells.snapshot.restore", {"name": "pre"})
    finally:
        for n in names:
            try:
                c.send_command("cells.snapshot.delete", {"name": n})
            except Exception:
                pass
