"""S-1 World Generation acceptance screenshots (worldgenerationpipeline plan task 18).

Standalone (not in plans/cow/default.json): one frame capture per preset at Small, seed 12345, Relief view, saved to
Cluiche/out/CoW/screenshots/s1/<preset>.png, plus <preset>-overlays.png with region edges, sites and the validate
evidence overlay on. Capture goes through cow.worldgen.capture -> DiaObservation CaptureManager -> captures/run/<tag>.png.
The PNGs are for human review; the only assertions are that each file exists and is non-trivial.
"""
import shutil
import time
from pathlib import Path

import pytest

from client import AutomationError

_STAGE = "S1WorldGenerationTestbedStage"
_SEED = 12345
_PRESETS = ("default", "continental", "archipelago", "highlands")
_OVERLAY_PRESET = "continental"


def _wait_complete(c, min_revision=0, timeout_s=60.0):
    deadline = time.time() + timeout_s
    status = None
    while time.time() < deadline:
        try:
            status = c.send_command("cow.worldgen.status")
            if status.get("state") == "Complete" and status.get("revision", 0) >= min_revision:
                return status
        except AutomationError:
            pass
        time.sleep(0.1)
    raise AssertionError(f"worldgen did not reach Complete: {status!r}")


def _panel_cmd(c, cmd, args=None):
    payload = {"domain": "WorldGen", "cmd": cmd}
    if args is not None:
        payload["args"] = args
    r = c.send_command("debug.panel_command", payload)
    assert r.get("success") is True, r


def _state(c):
    deadline = time.time() + 15.0
    while time.time() < deadline:
        st = c.send_command("debug.domain_state", {"id": "WorldGen"})
        if not st.get("pending") and "simFrame" in st:
            return st
        time.sleep(0.05)
    raise AssertionError("no WorldGen snapshot")


def _wait_view_current(c, revision, timeout_s=30.0):
    deadline = time.time() + timeout_s
    v = None
    while time.time() < deadline:
        v = c.send_command("cow.worldgen.view", timeout=5.0)
        if v.get("built") and v["layerVersion"] >= revision:
            return v
        time.sleep(0.05)
    raise AssertionError(f"viewer did not upload revision {revision}: {v!r}")


def _settle(c, frames=8):
    """Let the viewer's draw reach a presented frame before capture."""
    first = _state(c)["simFrame"]
    deadline = time.time() + 10.0
    while time.time() < deadline and _state(c)["simFrame"] < first + frames:
        time.sleep(0.05)
    time.sleep(0.3)


def _capture(c, run_dir, out_dir, name):
    tag = "S1Shot_" + name.replace("-", "_")
    src = run_dir / f"{tag}.png"
    if src.exists():
        src.unlink()
    r = c.send_command("cow.worldgen.capture", {"tag": tag})
    assert r.get("success") is True, r
    deadline = time.time() + 30.0
    while time.time() < deadline and not src.exists():
        time.sleep(0.2)
    assert src.exists(), f"capture never written: {src}"
    time.sleep(0.5)   # file is written in one fwrite, give the close a moment
    dst = out_dir / f"{name}.png"
    shutil.copyfile(src, dst)
    assert dst.stat().st_size > 10_000, f"suspiciously small PNG: {dst}"
    return dst


def _generate(c, preset):
    rev0 = _wait_complete(c)["revision"]
    _panel_cmd(c, "presetLoad", {"id": preset})   # reloading the current preset may not bump the revision
    time.sleep(1.0)
    _panel_cmd(c, "regenerate", {"seed": _SEED})
    st = _wait_complete(c, min_revision=rev0 + 1)
    assert st["seed"] == _SEED, st
    got = _state(c)["stats"]["preset"]
    # "default" is the unnamed built-in parameter set: the card reports it as "" rather than a preset id.
    assert got == preset or (preset == "default" and got == ""), _state(c)["stats"]
    _wait_view_current(c, st["revision"])
    return st


@pytest.fixture(scope="module")
def shot_dirs(request):
    root = Path(request.config._dia_repo_root)
    out = root / "Cluiche" / "out" / "CoW" / "screenshots" / "s1"
    out.mkdir(parents=True, exist_ok=True)
    return root / "Cluiche" / "out" / "CoW" / "captures" / "run", out


def _start(c):
    c.navigate_to(_STAGE, timeout_s=30.0)
    _wait_complete(c)
    _panel_cmd(c, "setSize", {"size": "Small"})
    time.sleep(0.5)
    _wait_complete(c)
    _panel_cmd(c, "setColourMode", {"mode": "terrain"})


@pytest.mark.parametrize("preset", _PRESETS)
def test_preset_relief_screenshot(dia_client, shot_dirs, preset):
    c = dia_client
    run_dir, out_dir = shot_dirs
    _start(c)
    st = _generate(c, preset)
    _settle(c)
    _capture(c, run_dir, out_dir, preset)


def test_overlays_screenshot(dia_client, shot_dirs):
    c = dia_client
    run_dir, out_dir = shot_dirs
    _start(c)
    st = _generate(c, _OVERLAY_PRESET)
    _panel_cmd(c, "validate")
    deadline = time.time() + 30.0
    panel = _state(c)
    while time.time() < deadline:
        panel = _state(c)
        v = panel.get("validation")
        if v and v.get("fresh") and v.get("source") == "validate":
            break
        time.sleep(0.1)
    drawers = {d["name"]: d for d in panel["view"]["drawers"]}
    for name in ("regions.edges", "sites", "guarantee.evidence"):
        d = drawers.get(name)
        assert d is not None, (name, sorted(drawers))
        if d["available"] and not d["enabled"]:
            _panel_cmd(c, "toggle", {"drawer": name})
    # expand the first guarantee that has proof paths, else the first one
    guarantees = panel["validation"]["guarantees"]
    pick = next((g for g in guarantees if g["counts"]["proofPaths"] > 0), guarantees[0])
    _panel_cmd(c, "expandGuarantee", {"id": pick["id"]})
    print(f"expanded guarantee {pick['name']} counts={pick['counts']}; drawers="
          f"{[(d['name'], d['enabled'], d['available']) for d in panel['view']['drawers']]}")
    _settle(c, frames=12)
    _capture(c, run_dir, out_dir, f"{_OVERLAY_PRESET}-overlays")
