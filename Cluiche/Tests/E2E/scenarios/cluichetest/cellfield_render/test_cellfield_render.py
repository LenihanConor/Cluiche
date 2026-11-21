"""CellField renderer visual proof (cell-field-renderer plan Task 22).

Runs CellFieldRenderTestStage, which shows a fixed sequence of static scenes and captures each
under its own tag, then checks the captured frames pixel-wise:

  AC9   sprite under field, debug line over field
  AC14  golden image (centre crop, per-channel tolerance 2) + hillshade light direction
  AC8   debug circle and cell centre land on the same pixel at 3 pan/zoom settings
  AC7   constant elevation -> uniform shade, strength 0 == unshaded, light change -> no upload

Golden: Cluiche/Assets/CluicheTest/captures/reference/CellFieldRender_golden.png
        (record with `dia capture bless CellFieldRender_golden` after viewing the run capture).
"""
import json
import math
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import cf_png  # noqa: E402

_STAGE = "CellFieldRenderTestStage"
_CHECKPOINT = "test.cell_field_render.passed"
_GOLDEN_TAG = "CellFieldRender_golden"
_GOLDEN_TOLERANCE = 2
_CROP = (800, 600)

_state = {"ran": False, "images": {}, "probes": None}


def _dirs(request):
    root = Path(request.config._dia_repo_root)
    run = root / "Cluiche" / "out" / "CluicheTest" / "captures" / "run"
    ref = root / "Cluiche" / "Assets" / "CluicheTest" / "captures" / "reference"
    return run, ref


def _ensure_run(dia_client, request):
    if _state["ran"]:
        return
    run_dir, _ = _dirs(request)
    for stale in list(run_dir.glob("CellFieldRender_*")):
        stale.unlink()

    dia_client.send_command("dia.automation.navigate_to", {"target": _STAGE})
    try:
        result = dia_client.poll_checkpoint(_CHECKPOINT, timeout_s=120.0)
        assert result.get("passed"), f"Stage did not pass: {result}"
        # PNG writes are asynchronous (frame readback); wait for the probes file, then every capture.
        deadline = time.time() + 40.0
        probes = run_dir / "CellFieldRender_probes.json"
        while time.time() < deadline and not probes.exists():
            time.sleep(0.5)
        tags = [s["tag"] for s in json.loads(probes.read_text())["scenes"]] if probes.exists() else []
        while time.time() < deadline and not all((run_dir / f"{t}.png").exists() for t in tags):
            time.sleep(0.5)
        time.sleep(0.5)
    finally:
        dia_client.abort_stage()
        dia_client.send_command("dia.automation.navigate_to", {"target": "Boot"})
    probes = run_dir / "CellFieldRender_probes.json"
    assert probes.exists(), f"probes file missing: {probes}"
    _state["probes"] = json.loads(probes.read_text())
    _state["ran"] = True


def _img(request, tag):
    if tag not in _state["images"]:
        run_dir, _ = _dirs(request)
        path = run_dir / f"{tag}.png"
        assert path.exists(), f"capture missing: {path}"
        _state["images"][tag] = cf_png.read_png(path)
    return _state["images"][tag]


def _scene(tag):
    for s in _state["probes"]["scenes"]:
        if s["tag"] == tag:
            return s
    raise AssertionError(f"scene {tag} not in probes")


def _to_px(img, scene, wx, wy):
    z = scene["zoom"]
    return ((wx - scene["cam"][0]) * z + img.width / 2.0,
            (wy - scene["cam"][1]) * z + img.height / 2.0)


def _near(p, rgba, tol):
    return p is not None and all(abs(p[i] - rgba[i]) <= tol for i in range(3))


def _find_colour(img, cx, cy, rx, ry, rgba, tol=2):
    for y in range(int(cy) - ry, int(cy) + ry + 1):
        for x in range(int(cx) - rx, int(cx) + rx + 1):
            if _near(img.px(x, y), rgba, tol):
                return (x, y)
    return None


def _lum(p):
    return 0.2126 * p[0] + 0.7152 * p[1] + 0.0722 * p[2]


def test_stage_runs(dia_client, request):
    _ensure_run(dia_client, request)
    run_dir, _ = _dirs(request)
    for s in _state["probes"]["scenes"]:
        assert (run_dir / f"{s['tag']}.png").exists(), f"missing capture {s['tag']}"


def test_ac9_draw_order(dia_client, request):
    _ensure_run(dia_client, request)
    tag = "CellFieldRender_draworder"
    img, sc, pr = _img(request, tag), _scene(tag), _state["probes"]["draworder"]

    ox, oy = _to_px(img, sc, *pr["overlap"])
    p_overlap = img.px(ox, oy)
    sx, sy = _to_px(img, sc, *pr["sprite_only"])
    p_sprite = img.px(sx, sy)
    print(f"overlap px=({ox:.1f},{oy:.1f}) -> {p_overlap}; sprite-only px=({sx:.1f},{sy:.1f}) -> {p_sprite}")

    assert _near(p_overlap, pr["field_rgba"], 2), f"sprite+field overlap should be field colour, got {p_overlap}"
    assert _near(p_sprite, pr["sprite_rgba"], 8), f"sprite-only should be sprite colour, got {p_sprite}"

    lx, ly = _to_px(img, sc, *pr["line_over_field"])
    hit_f = _find_colour(img, lx, ly, 1, 3, pr["line_rgba"])
    lx2, ly2 = _to_px(img, sc, *pr["line_over_sprite"])
    hit_s = _find_colour(img, lx2, ly2, 1, 3, pr["line_rgba"])
    print(f"line over field+sprite: expected px=({lx:.1f},{ly:.1f}) found {hit_f}; "
          f"over sprite only: expected ({lx2:.1f},{ly2:.1f}) found {hit_s}")
    assert hit_f is not None, "debug line not visible over the field (drawn under it?)"
    assert hit_s is not None, "debug line not visible over the sprite"


def test_ac14_golden(dia_client, request):
    _ensure_run(dia_client, request)
    _, ref_dir = _dirs(request)
    img = _img(request, _GOLDEN_TAG)
    ref_path = ref_dir / f"{_GOLDEN_TAG}.png"
    assert ref_path.exists(), (
        f"no golden at {ref_path}; view the run capture then `dia capture bless {_GOLDEN_TAG}`")
    ref = cf_png.read_png(ref_path)
    w, h = _CROP
    assert img.width >= w and img.height >= h and ref.width >= w and ref.height >= h, (
        f"capture {img.width}x{img.height} / reference {ref.width}x{ref.height} smaller than crop {w}x{h}")
    worst, count = cf_png.max_channel_delta(cf_png.crop_centre(img, w, h), cf_png.crop_centre(ref, w, h))
    print(f"golden diff: max channel delta {worst}, differing channel values {count}")
    assert worst <= _GOLDEN_TOLERANCE, (
        f"golden mismatch: max per-channel delta {worst} > {_GOLDEN_TOLERANCE} ({count} channel values differ)")


def test_ac14_structure_and_light_direction(dia_client, request):
    """Independent of the golden: backdrop in the inset gaps, and NW light brightens NW-facing slopes."""
    _ensure_run(dia_client, request)
    pr = _state["probes"]
    size = pr["hex_size_b"]
    nw, se = pr["hill"]["nw_cell"], pr["hill"]["se_cell"]

    nw_img, se_img = _img(request, _GOLDEN_TAG), _img(request, "CellFieldRender_light_se")
    sc_nw, sc_se = _scene(_GOLDEN_TAG), _scene("CellFieldRender_light_se")

    # Inset gap: midpoint between two vertically adjacent cells is backdrop colour.
    gx, gy = nw[0], nw[1] + math.sqrt(3.0) * size * 0.5
    px, py = _to_px(nw_img, sc_nw, gx, gy)
    gap = nw_img.px(px, py)
    print(f"gap px=({px:.1f},{py:.1f}) -> {gap}")
    assert _near(gap, (20, 20, 40), 6), f"inset gap should show backdrop (20,20,40), got {gap}"

    cx, cy = _to_px(nw_img, sc_nw, nw[0], nw[1])
    assert not _near(nw_img.px(cx, cy), (20, 20, 40), 6), "cell centre should not be backdrop"

    res = {}
    for name, cell in (("nw", nw), ("se", se)):
        a = nw_img.px(*_to_px(nw_img, sc_nw, *cell))
        b = se_img.px(*_to_px(se_img, sc_se, *cell))
        res[name] = (a, b, _lum(a), _lum(b))
    print("NW-facing cell: NW-light %s  SE-light %s  (lum %.1f vs %.1f)" % res["nw"])
    print("SE-facing cell: NW-light %s  SE-light %s  (lum %.1f vs %.1f)" % res["se"])
    assert res["nw"][2] > res["nw"][3], "NW-facing slope should be brighter under the NW light"
    assert res["se"][2] < res["se"][3], "SE-facing slope should be darker under the NW light"


def test_ac7_light_change_no_upload(dia_client, request):
    _ensure_run(dia_client, request)
    a, b = _scene(_GOLDEN_TAG), _scene("CellFieldRender_light_se")
    print(f"registry uploads at capture: golden={a['uploads']} light_se={b['uploads']}")
    assert a["uploads"] == b["uploads"], "changing the light must not upload value textures"


def test_ac7_flat_uniform_and_strength0(dia_client, request):
    _ensure_run(dia_client, request)
    pr = _state["probes"]
    size = pr["hex_size_b"]
    cols, rows = pr["grid_b"]

    # Constant elevation -> uniform shade over every cell centre.
    img, sc = _img(request, "CellFieldRender_flat_shaded"), _scene("CellFieldRender_flat_shaded")
    samples = set()
    for c in range(cols):
        for r in range(rows):
            wx = 1.5 * size * c
            wy = math.sqrt(3.0) * size * (r + 0.5 * (c & 1))
            samples.add(img.px(*_to_px(img, sc, wx, wy)))
    reds = [s[0] for s in samples]
    greens = [s[1] for s in samples]
    blues = [s[2] for s in samples]
    spread = max(max(reds) - min(reds), max(greens) - min(greens), max(blues) - min(blues))
    print(f"flat-shaded distinct cell-centre colours: {sorted(samples)} (spread {spread}); unshaded palette entry is (200,80,80)")
    assert spread <= 1, f"constant elevation should shade uniformly, spread {spread}"

    # Strength 0 == no elevation layer == palette colours.
    a, b = _img(request, "CellFieldRender_noshade_s0"), _img(request, "CellFieldRender_noshade_noelev")
    # Compare the centre crop only: the debug-panel UI (live log) differs between frames.
    w, h = _CROP
    worst, count = cf_png.max_channel_delta(cf_png.crop_centre(a, w, h), cf_png.crop_centre(b, w, h))
    print(f"strength0 vs no-elevation: max delta {worst}, differing values {count}")
    assert worst <= 1, "strength 0 must equal the unshaded render"
    nw = pr["hill"]["nw_cell"]
    sc0 = _scene("CellFieldRender_noshade_s0")
    p = a.px(*_to_px(a, sc0, *nw))
    expect = pr["palette6"][(2 + 2 * 2) % 6]
    print(f"unshaded NW cell colour {p}, palette entry {expect}")
    assert _near(p, expect, 1), f"unshaded cell should be the palette colour {expect}, got {p}"


def test_ac8_camera_parity(dia_client, request):
    _ensure_run(dia_client, request)
    cam = _state["probes"]["cam"]
    size = _state["probes"]["hex_size_b"]
    worst = 0.0
    for i in range(3):
        tag = f"CellFieldRender_cam{i}"
        img, sc = _img(request, tag), _scene(tag)
        ex, ey = _to_px(img, sc, *cam["cell_world"])
        rad = int(size * sc["zoom"] * 1.3) + 2

        def centroid(match):
            sx = sy = n = 0
            for y in range(int(ey) - rad, int(ey) + rad + 1):
                for x in range(int(ex) - rad, int(ex) + rad + 1):
                    p = img.px(x, y)
                    if p is not None and match(p):
                        sx += x + 0.5
                        sy += y + 0.5
                        n += 1
            return (sx / n, sy / n, n) if n else None

        circle = centroid(lambda p: _near(p, cam["circle_rgba"], 2))
        cell = centroid(lambda p: _near(p, cam["circle_rgba"], 2) or _near(p, cam["cell_rgba"], 2))
        assert circle is not None and cell is not None, f"{tag}: circle/cell colours not found near expected pixel"
        d_cc = math.hypot(circle[0] - cell[0], circle[1] - cell[1])
        d_ex = math.hypot(circle[0] - ex, circle[1] - ey)
        worst = max(worst, d_cc)
        print(f"{tag} zoom={sc['zoom']}: expected ({ex:.2f},{ey:.2f}) circle centroid ({circle[0]:.2f},{circle[1]:.2f}, n={circle[2]}) "
              f"cell centroid ({cell[0]:.2f},{cell[1]:.2f}, n={cell[2]}) circle-vs-cell {d_cc:.2f}px, circle-vs-expected {d_ex:.2f}px")
        assert d_cc <= 0.5, f"{tag}: circle centre {d_cc:.2f}px from cell centre"
        assert d_ex <= 1.0, f"{tag}: circle centre {d_ex:.2f}px from projected world point"
    print(f"worst circle-vs-cell offset {worst:.2f}px")
