"""S-1 World Generation map view: idle upload budget and slider-to-uploaded latency (worldgen-view plan task 11).

cow.worldgen.view (sim-hosted) reports the hosted viewer's upload counter and the revision whose data it last uploaded.
Pixels are not asserted here: layerVersion == status.revision means the viewer uploaded the new world to the CellField
layer; the draw that shows it happens in the same Sim frame.

Latency gate: p95 < 250 ms in Debug (2.5x the 100 ms Release target). The Release gate is documented, not enforced here.
"""
import statistics
import time

from client import AutomationError

_STAGE = "S1WorldGenerationTestbedStage"
_REPEATS = 20
_DEBUG_P95_LIMIT_MS = 250.0


def _report(request, text):
    """Print past pytest's capture so the numbers show in a passing run."""
    print(text)
    tr = request.config.pluginmanager.get_plugin("terminalreporter")
    if tr is not None:
        tr.write_line("")
        tr.write_line(text)


def _view(c):
    return c.send_command("cow.worldgen.view", timeout=5.0)


def _wait_complete(c, timeout_s=60.0, min_revision=None):
    deadline = time.time() + timeout_s
    status = None
    while time.time() < deadline:
        try:
            status = c.send_command("cow.worldgen.status")
            if status.get("state") == "Complete" and (min_revision is None or status.get("revision", 0) >= min_revision):
                return status
        except AutomationError:
            pass   # sim-hosted commands register once the module has started
        time.sleep(0.1)
    raise AssertionError(f"worldgen did not reach Complete (min_revision={min_revision}): {status!r}")


def _wait_view_current(c, timeout_s=30.0):
    """The viewer has uploaded the session's current revision (and rebuilt its grid for the current size)."""
    deadline = time.time() + timeout_s
    st = v = None
    while time.time() < deadline:
        st = c.send_command("cow.worldgen.status")
        v = _view(c)
        if (v.get("built") and st.get("state") == "Complete" and v["layerVersion"] == st["revision"] and
                v["gridWidth"] == st["dimensions"]["width"] and v["gridHeight"] == st["dimensions"]["height"]):
            return st, v
        time.sleep(0.05)
    raise AssertionError(f"viewer did not catch up: status={st!r:.300} view={v!r:.300}")


def _percentile(values, p):
    s = sorted(values)
    k = max(0, min(len(s) - 1, int(round(p / 100.0 * (len(s) - 1)))))
    return s[k]


def _start(c):
    c.navigate_to(_STAGE, timeout_s=30.0)
    deadline = time.time() + 30.0
    while time.time() < deadline:
        try:
            _wait_complete(c, timeout_s=5.0)
            if _view(c).get("built"):
                break
        except (AutomationError, AssertionError):
            pass
        time.sleep(0.2)
    return _wait_view_current(c)


def test_view_status_reports_a_built_viewer(dia_client):
    c = dia_client
    st, v = _start(c)
    assert v["built"] is True
    assert v["readFailures"] == 0, v
    assert (v["gridWidth"], v["gridHeight"]) == (st["dimensions"]["width"], st["dimensions"]["height"]), (st, v)
    assert v["colourMode"], v
    assert v["uploadCount"] >= 1, v


def test_idle_frames_upload_nothing(dia_client, request):
    from plugin import _collect_log_errors, _count_log_lines

    c = dia_client
    plan = getattr(request.config, "_dia_plan", None)
    hwm = _count_log_lines(request.config, plan) if plan else 0

    _, v0 = _start(c)
    t_end = time.time() + 10.0          # about 300 Sim frames at 30 Hz
    polls = 0
    while time.time() < t_end:
        v = _view(c)
        polls += 1
        assert v["uploadCount"] == v0["uploadCount"], f"idle upload after {polls} polls: {v0!r} -> {v!r}"
        assert v["readFailures"] == 0, v
        time.sleep(0.1)
    _report(request, f"[ measured ] idle: uploadCount stayed {v0['uploadCount']} over {polls} polls (10 s); droppedCount={v0['droppedCount']}")
    assert v0["droppedCount"] == 0, v0

    if plan:
        errors = _collect_log_errors(request.config, plan, skip_lines=hwm)
        assert not errors, "ERROR log lines: " + " | ".join(errors)


def _measure(c, size):
    """_REPEATS alternating landFraction sets; elapsed ms from the set reply to layerVersion == the new revision."""
    samples = []
    last_status = None
    values = (0.40, 0.52)
    for i in range(_REPEATS):
        st0 = c.send_command("cow.worldgen.status")
        target = st0["revision"] + 1
        r = c.send_command("cow.worldgen.set", {"landFraction": values[i % 2]})
        t0 = time.perf_counter()
        assert "error" not in r, r
        while True:
            v = _view(c)
            if v["layerVersion"] >= target:
                samples.append((time.perf_counter() - t0) * 1000.0)
                break
            if time.perf_counter() - t0 > 20.0:
                raise AssertionError(f"{size}: viewer never reached revision {target}: {v!r}")
            time.sleep(0.005)
        last_status = c.send_command("cow.worldgen.status")
        assert last_status["revision"] == target, f"{size}: expected one revision per set, got {last_status['revision']} (target {target})"
    return samples, last_status


def test_slider_to_uploaded_latency(dia_client, request):
    from plugin import _collect_log_errors, _count_log_lines

    c = dia_client
    plan = getattr(request.config, "_dia_plan", None)
    hwm = _count_log_lines(request.config, plan) if plan else 0

    _start(c)
    results = {}
    try:
        for size in ("Small", "Standard", "Large"):
            if size != "Small":
                c.send_command("cow.worldgen.set_size", {"size": size})
            _wait_view_current(c)
            time.sleep(0.5)
            samples, status = _measure(c, size)
            results[size] = (samples, status)
    finally:
        c.send_command("cow.worldgen.set_size", {"size": "Small"})
        _wait_view_current(c)

    lines = []
    failed = []
    for size, (samples, status) in results.items():
        p50, p95, mx = statistics.median(samples), _percentile(samples, 95), max(samples)
        lines.append(f"[ measured ] {size:8s} n={len(samples)} p50={p50:.0f} ms p95={p95:.0f} ms max={mx:.0f} ms | "
                     f"last gen lastMs={status.get('lastMs')} stagesRan={status.get('stagesRan')}")
        if p95 >= _DEBUG_P95_LIMIT_MS:
            failed.append(f"{size} p95 {p95:.0f} ms")
    print("\n".join(lines))
    assert not failed, "slider-to-uploaded p95 over the Debug limit of 250 ms: " + ", ".join(failed) + "\n" + "\n".join(lines)

    v = _view(c)
    assert v["readFailures"] == 0, v
    if plan:
        errors = _collect_log_errors(request.config, plan, skip_lines=hwm)
        assert not errors, "ERROR log lines: " + " | ".join(errors)
