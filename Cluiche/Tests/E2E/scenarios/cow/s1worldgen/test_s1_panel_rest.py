"""S-1 World Generation panel, the rest (panel-rest plan task 6): Validate, World File, Cancel, failed Regenerate, drawers.

Everything goes through the real DiaAPI commands (cow.worldgen.*) and the panel route (debug.panel_command ->
WorldGenDebugDomain::OnCommand -> debug.domain_state). The panel JS to event-stream hop and pixels are not covered here.
Each test restores the shared session (unlocked, default timeout) because the stage persists between tests.
"""
import json
import os
import time

from client import AutomationError

_STAGE = "S1WorldGenerationTestbedStage"
_SLOT_BYTES = 32 * 1024


def _cmd(c, name, args=None):
    """(ok, payload): sim-hosted handlers that return success=false surface as AutomationError."""
    try:
        r = c.send_command(name, args or {})
    except AutomationError as e:
        return False, (getattr(e, "payload", None) or {"error": str(e)})
    return r.get("success", True) is not False, r


def _status(c):
    return c.send_command("cow.worldgen.status")


def _wait_complete(c, timeout_s=30.0, min_revision=None):
    deadline = time.time() + timeout_s
    status = None
    while time.time() < deadline:
        try:
            status = _status(c)
            if status.get("state") == "Complete" and (min_revision is None or status.get("revision", 0) >= min_revision):
                return status
        except AutomationError:
            pass
        time.sleep(0.1)
    raise AssertionError(f"worldgen did not reach Complete (min_revision={min_revision}): {status!r}")


def _wait_idle(c, timeout_s=30.0):
    deadline = time.time() + timeout_s
    st = None
    while time.time() < deadline:
        try:
            st = _status(c)
            if not st.get("generating") and st.get("state") in ("Complete", "Failed"):
                return st
        except AutomationError:
            pass
        time.sleep(0.1)
    raise AssertionError(f"worldgen did not go idle: {st!r}")


def _raw(c):
    return c.send_command("debug.domain_state", {"id": "WorldGen"})


def _panel(c, timeout_s=15.0, pred=None, min_revision=None):
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
        if fresh and (min_revision is None or st["stats"]["revision"] >= min_revision) and (pred is None or pred(st)):
            return st
        time.sleep(0.05)
    raise AssertionError(f"no matching WorldGen snapshot within {timeout_s}s (progress={st.get('progress')!r:.500} stats={st.get('stats')!r:.200})")


def _panel_cmd(c, cmd, args=None):
    payload = {"domain": "WorldGen", "cmd": cmd}
    if args is not None:
        payload["args"] = args
    r = c.send_command("debug.panel_command", payload)
    assert r.get("success") is True and r.get("queued") is True, r
    return r


def _clean(c):
    """Enter the stage with an unlocked, default-timeout world."""
    c.navigate_to(_STAGE, timeout_s=30.0)
    st = _wait_complete(c)
    ok, r = _cmd(c, "cow.worldgen.regenerate", {"unlock": True, "timeoutSeconds": 30})
    assert ok, r
    st = _wait_complete(c, min_revision=st["revision"] + 1)
    assert not st["locked"], st
    return st


def _layer_hashes(c):
    pages = [c.send_command("cow.worldgen.layers", {"layer": n, "count": 1000}) for n in ("elevation", "regions")]
    for p in pages:
        p.pop("revision", None)
    return json.dumps(pages, sort_keys=True)


def _worlds_dir_from_path(path):
    """The export path is relative to the app's working directory (the deployed bin folder)."""
    if os.path.isabs(path):
        return os.path.dirname(path)
    import glob
    hits = glob.glob(os.path.join("C:/GitHub/Cluiche/Cluiche/bin/CoW", "*", "x64", os.path.normpath(path)))
    assert hits, f"cannot resolve the export path {path!r}"
    return os.path.dirname(max(hits, key=os.path.getmtime))


def test_validate_returns_per_guarantee_rows_and_keeps_revision(dia_client):
    c = dia_client
    st0 = _clean(c)
    ok, r = _cmd(c, "cow.worldgen.validate")
    assert ok and r["success"] is True, r
    assert r["guarantees"], r
    for g in r["guarantees"]:
        assert "id" in g and isinstance(g["ok"], bool), g
    assert r["revision"] == st0["revision"]
    assert _status(c)["revision"] == st0["revision"]

    # The panel route renders the same rows (AC3/AC4: validation section from fresh evidence).
    _panel_cmd(c, "validate")
    ps = _panel(c, pred=lambda s: (s.get("validation") or {}).get("source") == "validate")
    v = ps["validation"]
    assert v["fresh"] is True and v["worldRevision"] == st0["revision"], v
    assert len(v["guarantees"]) == len(r["guarantees"]), (len(v["guarantees"]), len(r["guarantees"]))
    assert ps["stats"]["revision"] == st0["revision"]


def test_export_import_round_trip_locks_and_matches(dia_client):
    c = dia_client
    try:
        st0 = _clean(c)
        before = _layer_hashes(c)
        ok, ex = _cmd(c, "cow.worldgen.export", {"name": "e2e_roundtrip", "overwrite": True})
        assert ok, ex
        assert ex["revision"] == st0["revision"]
        worlds = [w["name"] for w in c.send_command("cow.worldgen.worlds")["worlds"]]
        assert "e2e_roundtrip" in worlds, worlds

        ok, im = _cmd(c, "cow.worldgen.import", {"name": "e2e_roundtrip"})
        assert ok and im["locked"] is True and im["revision"] > st0["revision"], im
        st1 = _wait_complete(c)
        assert st1["locked"] is True and st1["source"] == "imported", st1
        assert _layer_hashes(c) == before   # cells match

        # AC: the panel shows the locked import; a change without unlock is refused.
        ps = _panel(c, pred=lambda s: s["file"]["imported"] is True)
        assert ps["file"]["locked"] is True
        assert ps["progress"]["lastOutcome"]["kind"] == "import", ps["progress"]["lastOutcome"]
        ok, r = _cmd(c, "cow.worldgen.set", {"landFraction": 0.45})
        assert not ok, r
        assert _status(c)["revision"] == st1["revision"]
    finally:
        _cmd(c, "cow.worldgen.regenerate", {"unlock": True})
        _wait_idle(c)


def test_import_of_hand_edited_bad_json_reports_line_and_column(dia_client):
    c = dia_client
    path = None
    try:
        _clean(c)
        ok, ex = _cmd(c, "cow.worldgen.export", {"name": "e2e_roundtrip", "overwrite": True})
        assert ok, ex
        path = os.path.join(_worlds_dir_from_path(ex["path"]), "e2e_bad_json.wld.json")
        with open(path, "w", encoding="utf-8") as f:
            f.write('{\n  "format": 1,\n  "dimensions": [1, 2,\n  oops\n}\n')
        rev = _status(c)["revision"]
        ok, r = _cmd(c, "cow.worldgen.import", {"name": "e2e_bad_json"})
        assert not ok, r
        assert r.get("line", 0) >= 1 and r.get("column", 0) >= 1, r
        assert _status(c)["revision"] == rev   # the world is unchanged

        # The panel route records the failure in file.lastImport and leaves the session alone.
        _panel_cmd(c, "import", {"name": "e2e_bad_json"})
        ps = _panel(c, pred=lambda s: (s["file"].get("lastImport") or {}).get("success") is False)
        assert ps["stats"]["revision"] == rev
    finally:
        if path and os.path.exists(path):
            os.remove(path)


def _send_back_to_back(c, first, first_args, second, second_args, timeout=10.0):
    """Send two commands without waiting between them so both reach the SimPU in the same frame; returns both payloads."""
    import queue
    for name, args in ((first, first_args), (second, second_args)):
        c._ws.send(json.dumps({"type": "MESSAGE_TYPE_COMMAND_REQUEST",
                               "command_request": {"command": name, "payload": args or {}}}))
    got = {}
    deadline = time.time() + timeout
    while len(got) < 2 and time.time() < deadline:
        try:
            msg = c._cmd_queue.get(timeout=0.5)
        except queue.Empty:
            continue
        resp = msg.get("command_response", {})
        if resp.get("command") in (first, second):
            got[resp["command"]] = resp
    assert len(got) == 2, got
    return got[first], got[second]


def test_cancel_during_regenerate_keeps_previous_world(dia_client):
    """A whole generation completes inside one SimPU tick on a satisfiable world, so Cancel lands only while the Regenerate is
    still pending (same frame). The in-flight multi-attempt Cancel is covered by the TestWorldGenSessionTimeout gtests."""
    c = dia_client
    _clean(c)
    cancelled_seen = False
    for i in range(30):
        _wait_idle(c)
        rev = _status(c)["revision"]
        before = _layer_hashes(c)
        _, cancel = _send_back_to_back(c, "cow.worldgen.regenerate", {"seed": 888000 + i}, "cow.worldgen.cancel", {})
        assert cancel.get("success") is True, cancel
        payload = cancel.get("payload", {})
        st = _wait_idle(c)
        if payload.get("cancelled") is True:
            cancelled_seen = True
            assert payload["outcome"] == "Cancelled", payload
            assert st["revision"] == rev, st
            assert _layer_hashes(c) == before
            assert st["state"] == "Complete" and st["seed"] != 888000 + i, st   # the pending seed reverted
            ps = _panel(c, pred=lambda s: (s["progress"].get("lastOutcome") or {}).get("outcome") == "CANCEL")
            assert ps["progress"]["lastOutcome"]["previousWorldKept"] is True, ps["progress"]
            break
    # Not cancelling is also a legal outcome of the race; the panel route must still accept the command.
    _panel_cmd(c, "cancel")
    assert cancelled_seen, "Cancel never landed on a pending Regenerate in 30 back-to-back tries"


def test_failed_regenerate_keeps_previous_world_system_test_2b(dia_client):
    c = dia_client
    try:
        st0 = _clean(c)
        before = _layer_hashes(c)
        ok, r = _cmd(c, "cow.worldgen.regenerate", {"seed": 424242, "timeoutSeconds": 0})
        assert ok, r
        st = _wait_idle(c)
        assert st["revision"] == st0["revision"], (st0, st)
        assert _layer_hashes(c) == before
        assert st["seed"] == st0["seed"], st   # the pending seed reverted
        report = c.send_command("cow.worldgen.report")
        assert report["outcome"] in ("Timeout", "Unsatisfiable"), report
        ps = _panel(c, pred=lambda s: (s["progress"].get("lastOutcome") or {}).get("kind") == "generation")
        assert ps["progress"]["state"] == "Complete", ps["progress"]   # the pill reads Complete
        card = ps["progress"]["lastOutcome"]
        assert card["previousWorldKept"] is True and card["outcome"] == "TIMEOUT", card
        assert ps["stats"]["revision"] == st0["revision"]
    finally:
        _cmd(c, "cow.worldgen.regenerate", {"timeoutSeconds": 30, "seed": 1})
        _wait_idle(c)


def test_panel_commands_route_to_session(dia_client):
    c = dia_client
    path = None
    try:
        st0 = _clean(c)
        # export -> file.lastExport and the merged world list
        _panel_cmd(c, "export", {"name": "e2e_panel", "overwrite": True})
        ps = _panel(c, pred=lambda s: (s["file"].get("lastExport") or {}).get("success") is True)
        assert any(w["name"] == "e2e_panel" for w in ps["file"]["worlds"]), ps["file"]["worlds"]

        # selectWorld listed / unlisted
        _panel_cmd(c, "selectWorld", {"name": "e2e_panel"})
        _panel(c, pred=lambda s: s["file"]["selected"] == "e2e_panel")
        _panel_cmd(c, "selectWorld", {"name": "no_such_world_zzz"})
        time.sleep(0.5)
        ps = _panel(c)
        assert ps["file"]["selected"] == "e2e_panel", ps["file"]   # unlisted name is ignored

        # discard while unlocked is a no-op
        rev = _status(c)["revision"]
        _panel_cmd(c, "discard")
        time.sleep(0.5)
        assert _status(c)["revision"] == rev

        # import -> locked; discard unlocks
        _panel_cmd(c, "import", {"name": "e2e_panel"})
        _panel(c, pred=lambda s: s["file"]["locked"] is True)
        _panel_cmd(c, "discard")
        _panel(c, pred=lambda s: s["file"]["locked"] is False)
        assert _status(c)["locked"] is False

        # cancel with nothing running is harmless
        _panel_cmd(c, "cancel")
        assert _wait_idle(c)["state"] in ("Complete", "Failed")

        # drawers need a full generation (regions and sites exist only after a Regenerate)
        rev = _status(c)["revision"]
        _cmd(c, "cow.worldgen.regenerate")
        _wait_complete(c, min_revision=rev + 1)
        # drawers: toggle flips the enabled flag, state survives a regenerate, unknown drawer is ignored
        ps = _panel(c)
        names = [d["name"] for d in ps["view"]["drawers"]]
        for expected in ("regions.edges", "guarantee.evidence"):
            assert expected in names, names
        edges = next(d for d in ps["view"]["drawers"] if d["name"] == "regions.edges")
        assert edges["available"] is True, edges
        _panel_cmd(c, "toggle", {"drawer": "regions.edges"})
        ps = _panel(c, pred=lambda s: next(d for d in s["view"]["drawers"] if d["name"] == "regions.edges")["enabled"] != edges["enabled"])
        flipped = next(d for d in ps["view"]["drawers"] if d["name"] == "regions.edges")["enabled"]
        _panel_cmd(c, "toggle", {"drawer": "no.such.drawer"})
        rev = _status(c)["revision"]
        _cmd(c, "cow.worldgen.regenerate", {"seed": 31337})
        _wait_complete(c, min_revision=rev + 1)
        ps = _panel(c, min_revision=rev + 1)
        assert next(d for d in ps["view"]["drawers"] if d["name"] == "regions.edges")["enabled"] == flipped   # survives regenerate
        _panel_cmd(c, "toggle", {"drawer": "regions.edges"})   # restore

        # expandGuarantee: a real id expands, an unknown id and a missing arg collapse
        _panel_cmd(c, "validate")
        ps = _panel(c, pred=lambda s: (s.get("validation") or {}).get("source") == "validate")
        gid = ps["validation"]["guarantees"][0]["id"]
        _panel_cmd(c, "expandGuarantee", {"id": gid})
        ps = _panel(c, pred=lambda s: s["view"].get("expandedGuarantee") == gid)
        _panel_cmd(c, "expandGuarantee", {"id": 4040404})   # unknown id: accepted, matches no guarantee, draws nothing
        _panel(c, pred=lambda s: s["view"].get("expandedGuarantee") == 4040404)
        _panel_cmd(c, "expandGuarantee")
        _panel(c, pred=lambda s: s["view"].get("expandedGuarantee") in (None, 0))
    finally:
        _cmd(c, "cow.worldgen.regenerate", {"unlock": True})
        _wait_idle(c)


def test_card_state_has_sections_and_fits_the_slot(dia_client):
    c = dia_client
    _clean(c)
    _cmd(c, "cow.worldgen.validate")
    _cmd(c, "cow.worldgen.export", {"name": "e2e_panel", "overwrite": True})
    ps = _panel(c, pred=lambda s: s.get("validation") and s.get("file") is not None and s.get("progress") is not None)
    for key in ("validation", "file", "progress"):
        assert key in ps, sorted(ps)
    assert ps["file"]["worlds"] is not None and "locked" in ps["file"]
    assert ps["progress"]["state"] in ("Complete", "Generating", "Failed")
    size = len(json.dumps(ps, separators=(",", ":")))
    assert size < _SLOT_BYTES, size
    # also with a full frame
    full = _panel(c, pred=lambda s: s.get("full") is True, timeout_s=25.0)
    assert len(json.dumps(full, separators=(",", ":"))) < _SLOT_BYTES
