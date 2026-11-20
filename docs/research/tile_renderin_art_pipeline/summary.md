# Research Summary — Tile Rendering & AI-Only Art Pipeline (V-1)

**Session folder:** docs/research/tile_renderin_art_pipeline/
**Date:** 2026-11-20
**Prototype:** [lookdev/](lookdev/README.md) (open `lookdev/index.html`)

## One-Line Answer

Draw the CoW map as a **3D heightfield built from the cell data**. The terrain, every simulation channel and the Hilda style are all shader code. AI art is needed only for small props, which the prototype did with **zero** AI images.

## Journey

1. **Explored.** The GDD locks the look (Hilda ink, palette tokens, season grade, depth-zone hatch and outline weight, three-layer hierarchy). Its pipeline assumes a human artist making about 180 tiles, which the user does not have. Dia already renders on bgfx in 3D and holds per-cell data on the GPU (`CellFieldRenderer`). It lacks terrain, instancing, particles and a game-side shader root.
2. **Ideated.** Ten candidates in two groups:
   - *Map approaches:* 2D field, 3D heightfield, AI stamped tiles, AI-painted macro map.
   - *Building blocks:* procedural ground, asset forge, ink pass, prop scatter, weather/water layer, look-dev spike.
3. **Evaluated.** A sixth axis, Art Feasibility (no artist, limited AI budget), carried the most weight, and missing Dia features counted as work rather than risk. The code-owned pieces scored top (ink pass 4.30, weather/water 4.30). AI stamped tiles scored last (1.80).
4. **Chose.** The Data-Driven Illustrated Map, with a browser bake-off to settle 2D vs 3D.
5. **Prototyped and decided (2026-11-20).** The user picked **3D heightfield** ("the right way") and dropped peak sprites, because the terrain carries the mountains. They also asked for **per-cell weather**, which the GDD confirms.

## Chosen Work Item

**Name:** V-1 Data-Driven Illustrated Map (3D heightfield)
**Home module:** CoW V-1 Tile Rendering testbed. The generic pieces are new or extended Dia packages (list below, each needing AD-001).
**Suggested spec type:** System (CoW V-1), plus Dia system or feature specs for the engine pieces.
**Estimated size:** XL overall. The Dia pieces are M–L each.

## What the Prototype Proved

| Question | Answer | Evidence |
|---|---|---|
| Can the map be built without an artist? | Yes. 0 Gemini images: ground, coasts, rivers, lakes, weather, hatching and seasons are code; 11 props are Claude-written SVG | `lookdev/README.md` table |
| 2D or 3D? | **3D heightfield.** 2D was cleaner but flat | User decision; shots 03, 10 |
| Peak sprites? | **No.** Terrain relief carries mountains: extra height on Mountain/Peak/Volcano cells × ridged noise | shot 10 |
| Does every channel read? | Yes: biome, ecology (Collapsed vs Thriving), succession by fade, season, snow pack, rivers (Dry→Frozen), lakes, floods, ice, clouds, rain/snow, wind | shots 04–08, 10 |
| Does it look tiled? | No. Coasts and biome borders are smooth inked curves | shots 02, 03 |
| Is weather global or local? | **Local, per cell** (GDD TileRenderingPipeline §7.2). Storms rain on lowlands and snow on high cells | shot 10 |

## Key Insights from Exploration and Prototype

- **A tile is a sample of the cell fields, not an image.** Each frame the renderer reads a handful of textures:
  - a per-texel field (elevation, landness, lakeness, temperature);
  - a nearest-cell index;
  - a per-cell data texture (biome, terrain, depth zone, health);
  - a river distance field;
  - a per-cell weather texture.

  This is the existing DiaCellLayers / `CellFieldRenderer` pattern extended, not a new idea.
- **Removing the hex staircase took three things:**
  1. Classify each texel by a Gaussian *majority* over the cell plus 2 rings (19 cells), with noise warp, not by nearest cell.
  2. Draw ink lines on *linear-filtered* fields (landness = 0.5, a biome-margin field) at a constant pixel width.
  3. Jitter the nearest-cell lookup by under one texel.

  14 texels per world unit was enough down to tactical zoom.
- **Mountains are terrain.** Smoothed S-1 elevation alone is too gentle, so the prototype adds relief by terrain type with ridged noise. Lakes must use their surface elevation (S-1 already stores it), or they render as pits.
- **The style is fully shader-owned and follows the GDD numbers:**
  - ink weight 1× / 1.2× / 1.5× by depth zone;
  - hatch coverage 15 / 40 / 70%;
  - Bloom/Hunt/Dry/Freeze grade tokens and blend modes;
  - per-biome palette tokens and season callouts;
  - ecology saturation bands.

  A full-screen post pass was **not** needed: grain and grade run per draw, so RB-001 can stand.
- **Contract with A4.** The renderer needs per cell: cloud, precipitation, snow fraction (or temperature) and gust. Cloud shadows, wet ground, particles and prop sway all read that one texture. A4's smoothed snapshot fits directly.
- **The AI budget collapses.** Props are palette-masked (R/G/B = primary/secondary/accent, black = ink) and recoloured by biome, season and ecology, so one sprite serves many looks. Gemini is now an *option* for nicer trees and rocks, not a requirement.
- **Weak spots to fix in the spec or build** (none block the direction):
  - hatching reads as mechanical stripes;
  - the Alpine palette (slate + snow + dark conifer) looks noisy;
  - texel steps remain at extreme close zoom;
  - hill and cloud sprites are placeholders;
  - field build takes 1.2 s in browser JS at Small (C++ or a GPU pass will be fine);
  - performance at Large is unmeasured (browser, Small only).

## Bigger Scope: Alive + Presentation (user, 2026-11-20, all approved)

The user asked to "go bigger". Two of the three options, *more alive* and *better looking*, were prototyped and approved ("all perfect"). Map scale was not taken. Everything below is code; no AI art.

**Alive**
- **Year clock:** seasons cross-fade over the last 30% of each season. The "living year" lets the season drive snow, lake level, river state and fire risk.
- **Wildfire:** a per-cell CA with fuel = biome × ecology, wind-biased spread and rain dousing. It leaves a charcoal scar that heals over about 3 game-years, gives smoke particles, and turns burned trees into dead trees.
- **Fauna:** herds and bird flocks. Herds avoid water and fire.
- **Time-lapse:** fast-forward of ecology drift. A stand-in for DiaCellHistory playback.

**Presentation**
- **Post pass:** tilt-shift focus band, aerial haze, painted sky, vignette, and a sea skirt to the horizon. This is the first real need for a full-screen pass, so RB-001 must be revisited (AD-001).
- **Inked depth silhouettes:** use the second difference of 1/z. That is zero on any flat surface, so the first, naive depth test's false lines on water are gone.
- **Ink on terrain:** form lines (light terminator) on slopes, optional contour lines, and line boil on terrain ink only.

**Rejected while tuning**
- Line boil on props: it read as vibration.
- Gust sway that changed frequency: it caused phase jumps.
- Sway is now a fixed frequency, with gusts scaling amplitude only.

## Tooling Is Part of the Build (user, 2026-11-20)

V-1 ships the look-dev tooling, not just the renderer. This follows the CoW rule that each testbed is the understand-and-tweak tool for its layer.
- **Commands:** every prototype control becomes a DiaAPI command, and the panel only calls those commands:
  - style parameters;
  - camera;
  - layer toggles;
  - data views;
  - stand-in ecology, water and weather drivers;
  - screenshot.
- **Persistence:** tuned values persist as a style parameter file, a DiaAttribute parameter file with a DiaValueSchema sidecar, so the art direction is data that can be reviewed and diffed.
- **Stand-in drivers:** each driver switches to the live system (A1, A4, A5) once it exists. The renderer reads the same textures either way.
- **Headless screenshots:** the prototype's capture (`tools/shot.sh`) becomes a V-1 regression and taste-check path. It renders a fixed world, camera and parameter set to an image, which gives one screenshot per feature for the user.

## Engine Pieces It Implies (Dia; all need AD-001 sign-off)

Each piece is reusable by any game; CoW owns only the data, palettes and rules.

| Piece | Likely home | Notes |
|---|---|---|
| Chunked heightfield terrain mesh from cell fields | new `DiaTerrainMap3D` (or DiaMesh3D extension) | 16-bit index limit means chunking; static per world |
| Cell-field-to-texture bake (majority classification, margin field, river distance field, smoothed fields) | extend DiaCellLayers / CellField | Per-cell data stays in R32F/RGBA8 textures (existing pattern) |
| NPR style shader library (ink-on-fields, hatch, toon shade, grade, grain) | DiaBgfx3D shader library | Plus a game-side shader root in `pipeline.toml` for CoW-specific functions |
| Instanced sprite props | DiaBgfx3D (instancing) | About 4k props at Small; `SpriteRenderer` is one draw per sprite today |
| Particles (rain/snow, world space) | new `DiaParticles` | Driven by a per-cell emitter density texture |
| Per-frame dynamic texture update | DiaBgfx (`BgfxTextureHandle::Update`) | Weather texture refreshed per frame or per smoothing step |

## GDD Fold-Back (for the GDD session)

**Paste-ready text, by GDD document, with decision and change entries: [gdd-foldback.md](gdd-foldback.md).** The list below is the short version.

- Replace TileRenderingPipeline's artist-authored 180-tile atlas with this field-driven renderer; the look rules are unchanged.
- V-1: the camera is a tilted 3D heightfield; mountains are terrain, not tiles or sprites; particles and animation are in scope.
- Settle the inconsistencies:
  - biome set (12 fantasy / 8 palette / code registry);
  - ecology state names (5 vs 4);
  - season names (Spring… vs Bloom…);
  - snow as a continuous pack, not a bool.
- Weather rendering inputs per cell: cloud, precipitation, type or temperature, gust.

## Discarded Candidates

| Candidate | Why discarded |
|---|---|
| C1 2D Illustrated Field | Lost the bake-off: cleaner, but flat; no grandeur |
| C9 AI Stamped Tiles (GDD as-is) | About 180 matching, hex-seamless AI images; can't be held by prompting; "early Civ" repetition |
| C10 AI-Painted Macro Map | Model drifts from the data; no weekly updates; map larger than one generation |
| Peak / hill / volcano sprites | Redundant on a heightfield; the weakest art in the prototype |
| Global weather sliders | Wrong per GDD; weather is per cell |

## Open Questions for the Spec

1. **Hex readability.** The map hides the grid. How do players read cells: grid overlay on demand, selection highlight, or Layer 3 icons only?
2. **Camera.** Should pitch flatten toward top-down as you zoom out (strategic) and tilt when zoomed in? And should occlusion behind ranges be handled (lower height scale at far zoom, X-ray on selection)?
3. **Props.** Is Claude SVG enough for the shipping prop set, or should Gemini produce trees and rocks through the asset forge? (A small test, about 10–20 generations, when props are specced.)

## Specs Written From This Research (all Approved 2026-11-20)

- docs/specs/applications/dia/systems/diacellfieldbake/diacellfieldbake.md (data bake)
- docs/specs/applications/dia/systems/diaterrainmap3d/diaterrainmap3d.md (3D map rendering)
- docs/specs/applications/dia/systems/diabgfx3d/diabgfx3d.md (amendment: render targets, sprites, particles, post stack)
- docs/specs/applications/cow/systems/strategicmap/strategicmap.md (CoW system)
- docs/specs/applications/cow/systems/v1-tilerendering/v1-tilerendering.md (testbed)

## References

- docs/research/tile_renderin_art_pipeline/explore.md
- docs/research/tile_renderin_art_pipeline/ideate.md
- docs/research/tile_renderin_art_pipeline/evaluate.md
- docs/research/tile_renderin_art_pipeline/choose.md
- docs/research/tile_renderin_art_pipeline/lookdev/ (prototype, screenshots in `lookdev/shots/`)
