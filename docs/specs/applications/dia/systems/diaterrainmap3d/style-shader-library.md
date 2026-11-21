# Feature Spec: DiaTerrainMap3D Style Shader Library

## Parent System
@docs/specs/applications/dia/systems/diaterrainmap3d/diaterrainmap3d.md

**Gameplay Domains:** rendering
**Depends on:** DiaTerrainMap3D Heightfield Terrain (textures, geometry)
**Research:** docs/research/tile_renderin_art_pipeline/summary.md
**Prototype:** `docs/research/tile_renderin_art_pipeline/lookdev/js/shaders.js` (`COMMON`, `GROUND_FS`), screenshots in `lookdev/shots/`

## Status
`Draft` (2026-11-20). Drafted from the approved prototype; awaiting user review. **Needs AD-001 sign-off** for the game-side shader root in `pipeline.toml`.

## Summary

This is the look. One terrain material, written as a library of small shader functions plus a parameter block, that draws the Hilda-style illustrated map from baked fields and per-cell channels. Every rule is driven by data and parameters; no hand-made per-cell art is involved.

The research proved the full set in the browser prototype. This feature ports it to bgfx shaders (`.sc`), splits it into reusable functions, and defines the parameter surface that the Strategic Map's style file will populate.

## Problem

- The look must be reproducible and tunable without an artist: ink, hatching, palette, lighting, season and condition grading, snow, water, weather and events are all shader rules.
- bgfx shaders cook from `.sc` files under the pipeline's `source_roots`; today only `Dia/DiaBgfx/Shaders` and `Dia/DiaBgfx3D/Shaders` are scanned. The material needs its own root, and CoW needs a game-side root for its overrides.
- The prototype is one large fragment shader. It must become maintainable functions, each testable by a capture on a synthetic input.

## Goals

1. **The functions**, each with a single responsibility and a parameter group:
   1. **Class palette**: primary/secondary/accent allocation by two noise bands, seasonal palette blend by weight.
   2. **Coast and class-border ink** on linear-filtered fields at constant pixel width: coast at land fraction 0.5, lake shore at the lake level, class borders from the margin field, weight by depth zone (1× / 1.2× / 1.5×), varying width by noise.
   3. **Hatching** by depth-zone coverage (about 15 / 40 / 70%), driven by shading and noise, with zoom-aware spacing (doubling by LOD) and cross-hatch in the deepest zone.
   4. **Toon hillshade** (light direction, banding amount) from the field's gradient; steep-slope rock blend with ink ticks.
   5. **Form lines**: ink where the light terminator crosses the surface (3D only); optional **contours**.
   6. **Condition grade**: saturation bands (Collapsed / Stressed / Stable / Thriving) and a bare-ground blend with cracks.
   7. **Snow** by temperature and elevation with a pack parameter; snow tint on lit/shaded areas.
   8. **Water**: ocean depth tint, wave marks, coast foam; lake surface; **ice**.
   9. **Rivers** from the baked river field: water, flow streaks (animated), inked banks, width by flow state, **flood** band, frozen.
   10. **Weather channel**: cloud shadows (field × drifting noise), wet-ground darkening.
   11. **Events channel**: burn glow along a front, charcoal scar that heals.
   12. **Season grade**: GDD canvas modifiers (Screen/Multiply, per-season strengths) cross-faded by weight; depth-zone ambient (−5%/−10% saturation, −8% lightness).
   13. **Ground marks** per class (grass ticks, tree dots, reed strokes, rock specks, lava veins).
   14. **Line boil** on terrain ink and **paper grain**.
2. A **`TerrainMaterial`** that binds all textures and a parameter block, and a **style parameter struct** whose every field is documented and range-checked (DiaAttribute + DiaValueSchema sidecar).
3. Shaders cooked by the standard pipeline from a new **`Dia/DiaTerrainMap3D/Shaders`** root, and a **game-side shader root** hook so CoW can supply overrides.
4. **Data views**: debug outputs for class, elevation, condition, depth zone, temperature, margin.

## Non-Goals

- Props, particles, post pass, sky (other features).
- Dynamic lights and shadows. The sun is a fixed direction parameter.
- Texture-based ground art. Everything is procedural from noise and palette (TMAP-003). A game may add detail textures later through the override hook.
- Per-class custom shaders. The class table parameterises one shader.

## Design

### Parameter surface

`TerrainMapStyle` (everything tunable; DiaAttribute parameter file with sidecar ranges):

| Group | Parameters |
|---|---|
| Geometry | `heightScale`, `lightDir`, `toonBands` |
| Ink | `inkOn`, `inkWeight`, `zoneWeight[3]`, `boilOn`, `inkColour` |
| Hatch | `hatchOn`, `zoneCoverage[3]`, `hatchSpacing` |
| Lines | `formLinesOn`, `contoursOn`, `contourInterval` |
| Season | `seasonWeights[4]`, `gradeOn`, `gradeStrength`, `zoneAmbient[3]` |
| Condition | `bands[3]` (thresholds), `bareBlend`, `crackStrength` |
| Water | `seaShallow`, `seaDeep`, `foamOn`, `lakeLevel`, `iceOn`, `flowState`, `floodOn` |
| Snow | `snowPack`, `snowLineBias` |
| Weather | `cloudShadowOn`, `wetStrength` |
| Marks/grain | `marksOn`, `grainOn`, `grainStrength` |
| Debug | `dataView` |

`TerrainMapVisuals` (per-class data from the game): `palette[class] = {primary, secondary, accent}`, per season callout overrides, ground-mark kind, relief weights (consumed by the bake). Uploaded as a small uniform array / texture.

### Shader structure

```
Dia/DiaTerrainMap3D/Shaders/
  varying.def.sc
  common/ noise.sh  colour.sh  ink.sh  hatch.sh  weather.sh  season.sh  water.sh  events.sh
  vs_terrain.sc   fs_terrain.sc      // composes the functions
```

`fs_terrain.sc` is a short composition of the function includes. Each include has a documented input/output and no hidden uniforms beyond its parameter group, so a function can be captured on a synthetic field (the golden-image tests).

### Cook roots

`pipeline.toml [bgfx_shaders] source_roots` gains `"Dia/DiaTerrainMap3D/Shaders"` and a game-side entry resolved per application (`Cluiche/CoW/Shaders`, optional). A game shader with the same name overrides the library's.

## Acceptance Criteria

1. **Cooks.** `dia pipeline` cooks the terrain shaders for dx11/dx12/vulkan with no warnings; a game-side override is picked over the library's when present.
2. **Synthetic captures.** For each function, a headless capture on a synthetic input matches a stored golden within tolerance (1/255 per channel on a 3×3 mean): a class border, a coast, a depth-zone patch for each of the three zones, a slope, a snow gradient, a river segment in each flow state, a burn front with scar, a cloud-shadow field.
3. **Ink width.** Coast and border ink is the same pixel width at three zoom levels (within 1 px), scaled by zone weight 1 : 1.2 : 1.5 (±10%).
4. **Hatch coverage.** The measured ink coverage of the hatch layer in a flat patch is within the GDD bands: First Shores 10–20%, Wild Reaches 30–50%, Ancient Interiors 60–80% (counts ink pixels over land pixels on a synthetic world).
5. **Season grade.** For a known grey input, Bloom, Dry and Freeze produce the GDD tints (Screen and Multiply as specified, strengths 8%, 6%, 12%); Hunt leaves it unchanged; a half-way blend equals the average of the two grades.
6. **Condition bands.** Saturation at health 0.15 / 0.5 / 0.9 falls inside the GDD bands (Collapsed −40–60%, Stressed −15–25%, Thriving up to +15%); a barren cell shows the bare blend.
7. **Snow.** Snow cover is monotonic in pack, increasing with elevation and decreasing with temperature, and zero when pack is 0.
8. **Rivers.** Flow states Dry, Low, Normal, Flood, Frozen change width and colour as specified; flow streaks move in the flow direction; ink banks exist at every state except Dry's lowest width (banks never vanish while the river is drawn).
9. **Events.** A burn texel of 1.0 is emissive; a scar of 1.0 is charcoal; scar fading to 0 returns the underlying ground colour.
10. **Weather.** Cloud shadow darkens terrain only where the cloud channel is above its threshold; wet darkening scales with the precipitation channel and is zero for snow.
11. **Boil.** Terrain ink changes with the boil frame and is stable within a frame; with boil off it never changes.
12. **No false lines.** No ink appears across a flat region of one class (water, plain) at any zoom or tilt in the synthetic captures.
13. **Data views.** Each data view outputs the raw channel in a documented colour mapping.
14. **Parameters.** Every `TerrainMapStyle` field has a sidecar range and default; out-of-range values are clamped with a warning; the defaults reproduce the approved prototype's look (side-by-side capture signed off by the user).
15. **Performance (Debug, ballpark).** Fragment cost at 1080p on the dev GPU is recorded. If Large with everything on misses 30 fps (GDD V-1 Test 7), a perf task is queued (cheaper noise octaves, early water exit, cached static layers), not silently accepted.
16. **Observability.** Shader compile failures log the file and error; profile scope `Terrain.Material`; the style parameter file hash is logged on load.

## Binding Decisions

| ID | Decision | How this feature complies |
|---|---|---|
| TMAP-003 | Style in shader code and parameter files, no per-state images | The whole library is procedural |
| TMAP-006 | No boil on props | Boil here applies to terrain ink only |
| TMAP-005 | Per-cell channels, no global weather | Cloud, wet, events come from channel textures |
| AD-001 | Engine and pipeline changes need sign-off | **Sign-off for the new shader root in `pipeline.toml`** |
| Platform: DiaAttribute / DiaValueSchema | Parameters are data | Style struct is a parameter file with sidecar |
| GDD Visual Bible | Hilda look: bold ink, flat colour, paper grain; hatch/outline by zone; season tokens | ACs 3–6 are the GDD numbers |

## Open Design Questions

1. **Cost of one big fragment shader.** The prototype does many noise lookups per pixel. If the cost is too high, split into a cached static layer (marks, hatch, palette at current season) rendered to a texture only when its inputs change, then lit and composited per frame. Decide from AC 15.
2. **Shader language features.** bgfx `.sc` is GLSL-like with limits (no dynamic loops over runtime counts, restricted array indexing). The prototype's 8-iteration outline loop was replaced by field-based ink; the remaining loops are fixed. Verify early that every function compiles for dx11 and vulkan.
3. **Where does CoW's palette live?** The library takes a palette table as data; the Strategic Map owns the values (Feature 1 there).

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Shader root, cook config, override hook, varying defs | AC 1 | Not Started | sonnet | AD-001 |
| 2 | Common includes: noise, colour, parameter block | AC 14 | Not Started | sonnet | |
| 3 | Palette, season grade, condition grade | AC 5, 6 | Not Started | sonnet | |
| 4 | Ink on fields (coast, lake, border), boil | AC 3, 11, 12 | Not Started | sonnet | |
| 5 | Hatching, toon shade, form lines, contours, slope rock | AC 4 | Not Started | sonnet | |
| 6 | Snow, water, ice, rivers, flood | AC 7, 8 | Not Started | sonnet | |
| 7 | Weather and events | AC 9, 10 | Not Started | sonnet | |
| 8 | Ground marks, grain, data views | AC 13 | Not Started | sonnet | |
| 9 | Golden captures per function; prototype parity sign-off | AC 2, 14 | Not Started | sonnet | user signs off |
| 10 | Cost measurement and perf notes | AC 15, 16 | Not Started | haiku | |
