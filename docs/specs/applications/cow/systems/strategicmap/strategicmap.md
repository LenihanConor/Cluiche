# System Spec: Strategic Map

**Parent:** @docs/specs/applications/cow/cow.md
**Testbed:** V-1 Tile Rendering (@docs/specs/applications/cow/systems/v1-tilerendering/v1-tilerendering.md)
**Design source:**
- `Output Docs/GDD/03_VisualBible/` (Locked 2026-07-18): AestheticStyleCore, ColourPaletteAndLighting, ShapeLanguageAndAnimation.
- `09_TechnicalArt/TileRenderingPipeline.md` (Locked 2026-11-17). This spec replaces its *mechanism*; GDD fold-back owed.
- `06_ScreenFlow/CanvasRenderingSortLayers.md`, Layers 1–2.

**Research:** docs/research/tile_renderin_art_pipeline/summary.md
**Renderer (Draoi):** @docs/specs/applications/draoi/systems/draoihexworldrender3d/draoihexworldrender3d.md (3D hex-world map), with @docs/specs/applications/draoi/systems/draoihexworldrelief/draoihexworldrelief.md (relief and rivers)
**Engine (Dia):** @docs/specs/applications/dia/systems/diacellfieldbake/diacellfieldbake.md (data), @docs/specs/applications/dia/systems/diatoonshading/diatoonshading.md (techniques), and the DiaBgfx3D render-target, sprite, particle, post and heightfield features

**Amendment 2026-11-23 (layer split, user).** DiaTerrainMap3D no longer exists. Three layers now share the map:
- **Dia** owns generic techniques: DiaToonShading, the DiaBgfx3D features, DiaCellFieldBake.
- **Draoi** owns the hex-world vocabulary: DraoiHexWorldRender3D (a plain library, `HexWorldMap`) and DraoiHexWorldRelief.
- **CoW (this system)** owns the look and all wiring: every number (GDD palette, season tints and names, strata ink and hatch, ecology bands, relief weights), the sim-to-channel adapter, CoW-only effects through the 4 custom slots (for example magic snow), and the module, streams, bake jobs, camera, commands, presets and capture that used to sit in DiaTerrainMap3D Feature 4 (now Feature 4 here).

Where the text below says "DiaTerrainMap3D", read "DraoiHexWorldRender3D (`HexWorldMap`)". `TMAP-*` decision ids are unchanged and now live in the Draoi spec.

**Gameplay Domains:** rendering, camera

## Summary

The Strategic Map is CoW's main screen: a "living illustrated map" (AestheticStyleCore Part 2) drawn as a tilted 3D heightfield. It covers sort Layers 1–2 of the canvas:
- **Layer 1, world art:** biome colour, terrain, ink, hatching, rivers, coasts, seasonal light.
- **Layer 2, world-state expression:** ecology, succession, weather, water and fire, shown through the art itself.

Layer 3+ icons, dragons and overlays sit on top and belong to other systems.

**Ownership split:**
- **DraoiHexWorldRender3D draws** (a Draoi library over Dia techniques). It owns the renderer, with neutral defaults only.
- **The Strategic Map owns everything CoW-specific:**
  - the biome visual table, keyed by the Biome Registry;
  - the GDD palette and seasonal tokens;
  - the depth-zone ink and hatch rules;
  - the ecology bands;
  - the prop set and placement rules;
  - the binding from each CoW simulation system to a DraoiHexWorldRender3D standard channel or custom slot;
  - the render-side module that owns the `HexWorldMap`, runs the bake jobs and the map camera;
  - the strategic camera;
  - the `cow.map.*` commands;
  - the style parameter file.

It ships its own understand-and-tweak tools (testbed philosophy; tooling ships with it, user 2026-11-20). Every control the V-1 look-dev panel needs is a `cow.map.*` DiaAPI command.

## Prototype Reference

The approved prototype (`docs/research/tile_renderin_art_pipeline/lookdev/`, user sign-off 2026-11-20) holds the first-pass CoW data this system formalises:

| Prototype code | Feature |
|---|---|
| `app.js` `BASE_PAL` and `seasonalPalette` (GDD palette tokens and season callouts) | 1 Map Visual Data |
| `props.js` `PROP_PAL`, `BIOME_PROPS` and `propColours` | 1 and 3 |
| `props.js` `PROP_SVG` (11 terrain props + cloud, beast, bird; palette-masked, Claude-authored) | 3 Props and Fauna Set |
| `alive.js` `YearClock` (season weights, living-year drivers) | 2 Channel Binding |
| `app.js` `camFor` and the input handlers (tilt, yaw, zoom, pan) | 4 Camera |
| `app.js` state defaults for tilt-shift, haze, sky, silhouettes, form lines, boil | 5 Presentation Presets |

## Goals

1. **Every gameplay channel is legible on the map.**
   - Terrain and relief, biome and depth zone.
   - Ecology health and vegetation, succession.
   - Season, weather (cloud, precipitation, snow pack, wind).
   - Water (rivers by flow state, lakes, floods, ice) and wildfire.
2. **The GDD look, with no artist.** The Visual Bible rules are implemented as data and code. AI art is optional and limited to props.
3. **Alive.**
   - Seasons cross-fade on the year clock.
   - Weather moves locally.
   - Fire spreads and scars heal.
   - Herds and birds move.
   - History can be played back as a time-lapse.
4. **Grand.**
   - A tilted perspective with real relief.
   - Tilt-shift, haze and a painted horizon.
   - Inked silhouettes and form lines.
5. **Tunable.** Every style value is a parameter (DiaAttribute + DiaValueSchema), changeable live and saved to a file.

## Non-Goals

- **Simulation.**
  - Fire spread is A1/A3's (natural ignition, BurnedAshland succession).
  - Herds are A1 Population Tokens.
  - Weather is A4's and water is A5's.
  - The Strategic Map renders their published state only.
- **Fog of war, territory and D3 consequence overlays.** These are V-2.
- **Dragons, nests and icons.** These are V-3/S-4 and Layer 3.
- **UI and HUD.** These are V-4.
- **Map scale beyond Large (104×64).** This was not taken in the 2026-11-20 scope choice.
- **Soil moisture and ground water.** These are debug-only (GDD Constitution §7). They stay as DiaCellLayers overlays.

## Channel Binding

| Map channel (DraoiHexWorldRender3D) | Source | Until the source exists |
|---|---|---|
| Elevation, relief class, coast, lakes (surface), rivers (mask, flow), depth zone, latitude, temperature | S-1 world (built) | — |
| Material class (biome) and visuals | S-1 Biome Registry (`biomes.json`) + this system's visual table | — |
| Condition (health 0..1) | A1 ecology health index | V-1 stand-in driver |
| Succession fade | A3 transition events (cross-fade the class over N weeks) | V-1 stand-in |
| Season weights | Year clock from GameSpeedController day (cross-fade over the last 30% of each season) | — (clock is built) |
| Weather (cloud, precipitation, snow fraction, gust) | A4 smoothed state (two snapshots blended by day) | V-1 storm-front stand-in |
| Snow pack | A4 snow | Year-clock driver |
| Water state (lake level, river flow state, flood, ice) | A5 | Year-clock driver ("living year") |
| Events (burn front, scar) | A1/A3 fire state | V-1 fire CA stand-in |
| Fauna sprites | A1 Population Tokens (herds); ambient birds are visual-only here | V-1 herd stand-in |

Every source implements `IMapChannelSource`, so the V-1 stand-ins and the live systems are interchangeable. The renderer can't tell them apart. Sim state crosses to the render side as FrameStream snapshots, never shared memory.

## Features

| # | Feature | Summary | Spec | Status |
|---|---|---|---|---|
| 1 | Map Visual Data | Biome visual table keyed by Biome Registry ids: palette P/S/A, season callouts, ground marks, relief weights, prop sets; depth-zone ink and hatch; GDD season grade tokens; ecology bands. Schema'd files under `Global/Map/` | [map-visual-data.md](map-visual-data.md) | Draft |
| 2 | Channel Binding | `IMapChannelSource`, S-1 binding, year clock, slots for A1/A3/A4/A5 (stand-ins provided by V-1) | [channel-binding.md](channel-binding.md) | Draft |
| 3 | Props and Fauna Set | CoW palette-masked SVG props (Claude-authored first; Gemini only if needed through the asset forge) and placement rules; herd and bird sprites; cloud sprites | [props-and-fauna-set.md](props-and-fauna-set.md) | Draft |
| 4 | StrategicMapModule and Camera | Render-side module; strategic camera (Orbit + zoom-dependent pitch, bounds); `cow.map.*` commands; style parameter file load/save/reload | [module-and-camera.md](module-and-camera.md) | Draft |
| 5 | Presentation Presets | Tilt-shift, haze, sky, silhouettes, form lines, contours, boil: tuned defaults and named presets (strategic, cinematic, debug-flat) | [presentation-presets.md](presentation-presets.md) | Draft |

Build order: 1 → 2 → 4 (first map on S-1 data), then 3 and 5. Each feature can only start once the DraoiHexWorldRender3D feature it uses exists.

## Decisions

| ID | Decision | Status | Binding |
|---|---|---|---|
| SM-001 | The Strategic Map is a CoW system; the renderer draws, the Strategic Map supplies CoW data and binding (user 2026-11-20; renderer is DraoiHexWorldRender3D since 2026-11-23) | Accepted | Yes |
| SM-007 | CoW owns the look and all wiring; Draoi ships neutral defaults; CoW-only states use the custom slots, never a Draoi change (user 2026-11-23, layer split) | Accepted | Yes |
| SM-002 | Replace the GDD's artist-authored 180-tile atlas (TileRenderingPipeline §1–4) with field-driven rendering; the Visual Bible look rules stand unchanged | Accepted | Yes |
| SM-003 | Weather is rendered per cell (TileRenderingPipeline §7.2, per-cell emitters), never from a global state | Accepted | Yes |
| SM-004 | Fire and herds are rendered from A1/A3 state; the Strategic Map does not simulate them | Accepted | Yes |
| SM-005 | Look-dev tooling ships with the system: every control is a `cow.map.*` DiaAPI command and tuned values persist in a style parameter file (user 2026-11-20) | Accepted | Yes |
| SM-006 | Biome visuals key off Biome Registry ids and tags, never the deleted `Biome` enum | Accepted | Yes |

## Inherited Binding Decisions

| ID | Decision | How it constrains the Strategic Map |
|---|---|---|
| PD-001 | StringCRC ids | Biome, prop, channel and command ids |
| PD-002 | PU/Phase/Module | Module on the render side; sim channels arrive by FrameStream |
| PD-004 | No STL in public APIs | Channel and instance interfaces |
| AD-001 | Engine changes need sign-off | Any Dia change made from this system needs sign-off; Draoi changes follow the Draoi admission test |
| AD-002 | Four-tier classification | Techniques → Dia; hex-world renderer → Draoi (TMAP-009); visual data, binding, module, camera → CoW-Shared (reused by every testbed and the game) |
| AD-003 | GDD owns design intent | Look rules come from the Visual Bible; this spec owns mechanism. Fold-back text goes to the GDD session |
| TMAP-002..006, 009..012 | DraoiHexWorldRender3D decisions | Heightfield, code-owned style, terrain mountains, per-cell channels, no prop boil; layer split, hybrid channels, plain library, neutral defaults |

## Open Design Questions

1. **Hex readability.** The art hides the grid. Should players read cells through a grid overlay on demand, a hover/selection outline, or Layer 3 icons only? This affects whether DraoiHexWorldRender3D draws a cell outline (its system ODQ 1).
2. **Camera.** Should pitch flatten automatically toward top-down as you zoom out, then tilt at regional zoom? How is occlusion behind ranges handled: lower relief at far zoom, or a see-through outline on the selected cell?
3. **GDD naming conflicts.** These must be settled before Feature 1:
   - biome set (registry vs the 12 Phase-2 names vs the 8 palette biomes);
   - ecology state names (5 vs 4);
   - season names (Bloom/Hunt/Dry/Freeze wins in the palette doc).

## Status

`Approved` (2026-11-20, user). The interview was done during the research session (2026-11-20). Specced now, built at CoW build order #18 (user decision).
