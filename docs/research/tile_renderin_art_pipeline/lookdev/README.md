# V-1 look-dev prototype (approved reference; not shipping code)

This is a browser bake-off for the research in `../choose.md`: a 2D illustrated field vs a 3D heightfield. Both use one shared style library, so the comparison is about geometry and camera, not shading. It is not engine code and nothing here ships.

**Specs built on this prototype:** DiaCellFieldBake, DiaTerrainMap3D, the DiaBgfx3D amendment, Strategic Map and V-1. See `../summary.md`, section "Specs Written From This Research". Each spec has a Prototype Reference table that maps this code to its features.

## Run it

Open `index.html` in Edge or Chrome by double-clicking it (`file://` is fine; no server needed). WebGL2 is required.

- **Mouse:** drag to pan, scroll to zoom, right-drag (or shift-drag) to tilt and rotate the 3D view.
- **View:** choose `split`, `2d` or `3d`.
- **Panel sliders:** season, ecology health, water, weather, and every style layer. The `Data view` dropdown shows the raw cell data behind the art.
- **References:** the thumbnails bottom-left are the GDD Visual Bible references (*Strategic Map Overview* and others). Click one to overlay it.
- **Screenshot:** saves a PNG.

## Data

The page reads real S-1 Small worlds (66×42): continental, highlands, archipelago and default, seeds 1–3. `tools/convert_golden.py` turns `Cluiche/out/CoW/golden-dumps/*.json` into `worlds/*.js`. Re-run it after S-1 changes:

```
python -I docs/research/tile_renderin_art_pipeline/lookdev/tools/convert_golden.py
```

A1, A4 and A5 do not exist yet, so the ecology, weather and water panels are sliders standing in for them.

## What is code vs art

| Element | Source | AI images |
|---|---|---|
| Ground colour per biome | GDD palette tokens + noise in the shader, recoloured live by season and ecology | 0 |
| Coasts, lake shores, biome borders | Ink lines drawn on smooth data fields, weighted by depth zone (1× / 1.2× / 1.5×) | 0 |
| Hatching | Depth-zone coverage (15% / 40% / 70%) in the shader | 0 |
| Rivers | Distance field from S-1 flow; Dry/Low/Normal/Flood/Frozen states; animated flow | 0 |
| Lakes, floods, ice | Lake level, floodplain and ice from data | 0 |
| Season | GDD canvas grade (Bloom/Hunt/Dry/Freeze tokens and blend modes) + per-biome callouts | 0 |
| Clouds, rain, snow, wind | Procedural cloud shadows, particles and prop sway | 0 |
| Snow pack | Continuous, by temperature and elevation | 0 |
| Props (11 types) | **Claude-written SVG**, palette-masked (red/green/blue = primary/secondary/accent, black = ink), recoloured per biome, season and ecology | 0 |

| Year clock | Continuous day of year; season palette and grade cross-fade; the "living year" drives snow, lake level, river state and fire risk | 0 |
| Wildfire | Per-cell CA: fuel from biome × ecology, wind-biased spread, rain douses it, charcoal scar heals over about 3 game-years, smoke particles | 0 |
| Herds and birds | Wandering herds avoid water and fire; birds circle at height; two tiny SVG sprites | 0 |
| Post pass | Tilt-shift focus band, aerial haze, painted sky, inked depth silhouettes, vignette, sea skirt to the horizon | 0 |
| Form lines, contours, line boil | Terminator ink on slopes, optional old-map contours, ink that redraws 12 times a second | 0 |

Total Gemini images used: **0**.

## Files

- `index.html`: page and panel.
- `js/world.js`: cell layers to continuous fields.
- `js/shaders.js`: the style library.
- `js/props.js`: SVG props, palettes and placement rules.
- `js/app.js`: rendering, camera and UI.
- `tools/shot.sh <name> "<hash args>"`: headless screenshot via Edge. The same `#key=value` arguments work in the browser URL, for example `index.html#view=3d&season=Freeze&snow=0.6`.
- `shots/`: reference screenshots from the first pass.
