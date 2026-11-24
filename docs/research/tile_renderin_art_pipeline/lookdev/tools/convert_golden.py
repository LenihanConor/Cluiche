"""Convert S-1 golden layer dumps into JS data files the look-dev page can load from file://.

Golden dumps (Cluiche/out/CoW/golden-dumps/<preset>_small_seed<N>.json) store every worldgen layer as
float32 bit patterns ({"bits": [uint32...], "count": N}). This writes lookdev/worlds/<name>.js containing
window.WORLDS[name] = {width, height, layers: {...}} with values rounded for size.

Usage (from repo root):
    python -I docs/research/tile_renderin_art_pipeline/lookdev/tools/convert_golden.py
"""
import json
import struct
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[5]
DUMPS = REPO / "Cluiche" / "out" / "CoW" / "golden-dumps"
OUT = Path(__file__).resolve().parents[1] / "worlds"

# Small worlds are 66 x 42 (S-1 size policy).
WIDTH, HEIGHT = 66, 42

LAYERS = {
    # name in dump -> (name in page, decimals)
    "elevation": ("elevation", 4),
    "oceanDepth": ("oceanDepth", 3),
    "biomeIndex": ("biome", 0),
    "terrainType": ("terrain", 0),
    "depthZone": ("depthZone", 0),
    "riverMask": ("river", 0),
    "flowDirection": ("flowDir", 0),
    "flowAccumulation": ("flowAcc", 2),
    "lakeMask": ("lake", 0),
    "waterSource": ("waterSource", 0),
    "moisture": ("moisture", 3),
    "temperature": ("temperature", 3),
    "slope": ("slope", 3),
    "latitude": ("latitude", 3),
    "coastDistance": ("coastDistance", 0),
    "volcanic": ("volcanic", 0),
    "ridgeMask": ("ridge", 3),
}


def decode(entry):
    vals = [struct.unpack("<f", struct.pack("<I", b))[0] for b in entry["bits"]]
    return vals


def convert(path: Path):
    d = json.loads(path.read_text())
    layers = {}
    for src, (dst, dec) in LAYERS.items():
        if src not in d:
            continue
        v = decode(d[src])
        if len(v) != WIDTH * HEIGHT:
            raise SystemExit(f"{path.name}: {src} has {len(v)} cells, expected {WIDTH * HEIGHT}")
        layers[dst] = [int(round(x)) for x in v] if dec == 0 else [round(x, dec) for x in v]
    name = path.stem.replace("_small", "")
    payload = {"name": name, "width": WIDTH, "height": HEIGHT, "topology": "hex_flat_odd_q", "layers": layers}
    OUT.mkdir(parents=True, exist_ok=True)
    js = "window.WORLDS = window.WORLDS || {};\nwindow.WORLDS[" + json.dumps(name) + "] = " + \
        json.dumps(payload, separators=(",", ":")) + ";\n"
    (OUT / f"{name}.js").write_text(js)
    return name


def main():
    files = sorted(DUMPS.glob("*_small_seed*.json"))
    if not files:
        sys.exit(f"no golden dumps in {DUMPS}")
    names = [convert(f) for f in files]
    (OUT / "index.js").write_text("window.WORLD_NAMES = " + json.dumps(names) + ";\n")
    print(f"wrote {len(names)} worlds to {OUT}")


if __name__ == "__main__":
    main()
