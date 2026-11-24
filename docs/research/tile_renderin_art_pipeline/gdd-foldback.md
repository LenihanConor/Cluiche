# GDD Fold-Back: V-1 Map Rendering (paste-ready for the GDD session)

**From:** CoW build session, 2026-11-20
**Research:** `docs/research/tile_renderin_art_pipeline/summary.md` (browser prototype, user-approved 2026-11-20)
**Specs written from it (all in the Cluiche repo):**
- `docs/specs/applications/cow/systems/strategicmap/strategicmap.md` (5 features)
- `docs/specs/applications/cow/systems/v1-tilerendering/v1-tilerendering.md` (4 features)
- `docs/specs/applications/dia/systems/diaterrainmap3d/`, `diacellfieldbake/`, `diabgfx3d/` (engine)

**How to use this file:** each section below names the GDD document it changes and gives the replacement text. Nothing here has been edited in the GDD. The Visual Bible look rules (style, palette, ink, hatching, season tokens, three layers) are **unchanged**; this changes the *mechanism* and a handful of V-1 scope lines.

---

## 0. Why (one paragraph for the DecisionLog and ChangeLog)

CoW has no artist, and generated art cannot hold 180 matching, hex-seamless tile images by prompting. Research and a user-approved prototype showed the map can be drawn as a **tilted 3D heightfield built from the simulation's cell data**. The Hilda look (bold ink, flat colour, paper grain, hatching by depth zone) and every simulation channel (terrain, biome, ecology, succession, season, weather, water) are produced by shader code and parameter files. AI-generated art is optional and limited to small props. Weather is rendered **per cell**. Mountains are terrain relief, not tiles. Motion (particles, sway, flowing water, seasonal cross-fades) is in scope.

---

## 1. DecisionLog: proposed entries (IDs to be assigned)

| Proposed ID | Decision | Rationale |
|---|---|---|
| DECISION-V1-A | The strategic map is rendered as a tilted 3D heightfield drawn from per-cell simulation data, not from authored tiles or a tile atlas. | No artist; tile atlas needs 180+ matching images and repeats visibly at zoom-out; field rendering shows every simulation channel (research 2026-11-20). |
| DECISION-V1-B | The Phase 3 look is produced in code and parameters: outline weight and hatch coverage by depth zone, season grade tokens, palette tokens, ecology saturation bands. The Visual Bible numbers are the acceptance values. | Keeps the locked look; makes it tunable and testable. |
| DECISION-V1-C | Weather, ecology condition, water state and fire are drawn **per cell**, never as a global state. | Matches TileRenderingPipeline §7.2 (per-cell emitters) and A4's per-cell model. |
| DECISION-V1-D | Mountains are terrain relief (class-weighted, ridged), not peak tiles or sprites. | The heightfield already is the mountains; peak sprites looked weakest in review. |
| DECISION-V1-E | Presentation effects permitted on the strategic map: tilt-shift depth of field, aerial haze, painted sky above the horizon, inked depth silhouettes, vignette, line boil on terrain ink. Still forbidden: bloom, lens flare, PBR, normal-mapped detail. | Gives depth and grandeur while staying an illustration (user, 2026-11-20). |
| DECISION-V1-F | Particles, sway, flowing rivers, season and succession cross-fades, wildfire fronts and ambient fauna are in scope for the map. They support the illustration (Style Core: particles are supporting texture). | Reverses V-1's "static tile appearance only" scope line. |

## 2. ChangeLog: proposed entry

> **CHANGE-TBD (2026-11-20): V-1 map rendering replaced.** `TileRenderingPipeline.md` re-locked: artist-authored vector tiles, 1024 px exports, atlas packing, `tiles.json` and the 180-tile checklist are replaced by field-driven 3D rendering (see below). `V1_TileRendering.md` re-locked: scope, test restatements, camera and motion. Visual Bible look rules unchanged. Naming reconciled (biomes, ecology states, seasons, snow pack). Affects: V-1, V-2 (overlays sit on the heightfield), S-2.5/S-3/S-3.5 (they can host the map instead of debug overlays).

---

## 3. `09_TechnicalArt/TileRenderingPipeline.md`

### 3.1 Replace the Pipeline Overview and §1–§5

**Replace** "Artist (Vector) → Export (Raster) → Atlas Packing → Manifest → Runtime Rendering" and sections 1 (Artist Authoring), 2 (Export), 3 (Atlas Packing), 4 (Tile Manifest) and 5 (Runtime Rendering) with:

> **Pipeline overview.** Cell data → bake → GPU fields → terrain material → scene → presentation pass.
>
> 1. **Cell data.** Per cell, from the simulation: terrain class and elevation (S-1), biome (registry id), depth zone, temperature, rivers (mask, flow direction, accumulation), ecology condition and succession (A1/A3), weather (A4, smoothed), water state (A5), fire (events) and fauna (when specified).
> 2. **Bake.** The cell data is turned into continuous per-texel fields (smooth values; biome, water class, owner cell and a border margin from a weighted majority over two rings of neighbours; terrain relief; a river distance field). The grid is not visible in the result. Resolution is a parameter; the bake can run for the whole world or only for visible tiles.
> 3. **Terrain.** A chunked heightfield mesh takes height from the baked elevation.
> 4. **Terrain material.** One shader draws the style from the fields and from the per-cell channels: palette (primary/secondary/accent allocation), ink on coasts, lakes and biome borders (weight by depth zone), hatching (coverage by depth zone), toon hillshade and form lines, ecology condition grade, snow, water and rivers, cloud shadows, wet ground, fire and scars, ground marks and paper grain.
> 5. **Props.** Instanced, palette-masked sprites scattered deterministically from per-biome rules, scaled by ecology condition. A sprite stores colour *slots* (primary, secondary, accent, ink), so one sprite serves every biome and season.
> 6. **Particles and fauna.** Rain, snow and smoke particles spawned only where the per-cell channels say so; herds, birds and cloud puffs as dynamic sprites.
> 7. **Presentation.** Tilt-shift focus band, aerial haze, painted sky, inked depth silhouettes and vignette, over the scene.
>
> **No hand-authored per-tile art is required.** The only authored art is the small prop set (about 14 sprites), written as SVG (colour-slot masks) and rasterised into one atlas by the asset pipeline. A prop can be replaced by an AI-generated image that follows the same slot contract.

### 3.2 §6 Seasonal Shader: keep the idea, change the form

Replace the HSV-shift-plus-tint per tile with: "Seasons are a **canvas-wide grade** (the Visual Bible season tokens: Bloom +8% gold Screen, Hunt none, Dry +6% ochre Screen, Freeze +12% blue-grey Multiply) plus the per-biome palette callouts. Season weights **cross-fade** over the last 30% of each season, so the world never snaps at a season boundary. Depth zone adds the ambient modifier (−5% saturation, −10% saturation and −8% lightness)."

### 3.3 §7 Weather Rendering: three changes

1. **State is per cell and continuous.** Replace the `WeatherRenderState` struct with: `cloud 0..1`, `precipitation 0..1`, `snowFraction 0..1` (per cell, from the cell's own temperature), `gust 0..1`, and a continuous **snow pack** (no `snow_cover: bool`). Rain and snow fall in the same storm according to each cell's temperature.
2. **Cloud cover** is a local **cloud shadow** (about 5–30% darkening, drifting with the wind), not a uniform 40% multiply. Optional translucent cloud puffs mark storm fronts.
3. **Particles** are world-space, spawned per cell by the precipitation channel, culled to the view, with a fixed budget. They are in scope (see V-1).

§7.5 (smoothed weather) stands: the weather channel is A4's blend of the last two weekly snapshots by day.

### 3.4 §7.6 A5 Water Overlays: resolved, no longer "follow-up art"

Lake extent and shoreline, flood, ice and river width by flow state (Dry / Low / Normal / Flood / Frozen) are drawn by the terrain material from per-cell water channels and the baked river field. No art is needed. Soil and ground water remain debug-only (Constitution §7).

### 3.5 §8 Asset Production Checklist and §9: replace

> **Authored assets:** one prop atlas (about 14 SVG sprites in the slot contract) and the data files under `Global/Map/` (visuals, style, presets, prop rules). **No tile variants, no atlas packing of terrain, no weather atlas.**
> **Engine:** terrain bake, heightfield, style shader library, instanced sprites, particles, presentation pass (specified in the Dia specs listed at the top).

---

## 4. `10_PrototypeStrategy/V1_TileRendering.md`

### 4.1 Scope

- **In scope (add):** tilted 3D camera (pan, zoom, orbit, zoom-dependent tilt), relief, rivers, lakes, ice and floods, cross-fading seasons and succession, wildfire and scars, ambient fauna, particles and sway, presentation effects (DECISION-V1-E), a look-dev tool panel.
- **Out of scope (change):** remove "Animation (biome transition effects, weather particle systems): static tile appearance only". Replace with "Dragons, clans, territory, fog of war, overlays and UI remain out of scope (V-2/V-3/V-4)."
- Remove "Tile Variants Required" (12 biomes × 5 states × 4 seasons = 180) entirely. Replace with: "V-1 covers every registry biome, four condition bands and four seasons. Variants are produced by parameters, not images."

### 4.2 Tests (restated as measurable checks; thresholds are starting values to calibrate)

| Test | Restatement |
|---|---|
| 1 Biome Visual Distinction | Rendered flat patches of every pair of land biomes differ by ΔE ≥ 15 at strategic zoom (Hunt) |
| 1b Extreme zoom-out | Same, cells 4 px wide then downsampled, ΔE ≥ 10 |
| 2 Ecology State Distinction | Four condition bands (Collapsed, Stressed, Stable, Thriving) differ by ΔE ≥ 8 per adjacent band; saturation and prop density fall monotonically |
| 3 Seasonal Cycle | Every biome's seasons differ from Hunt by ΔE ≥ 5; the cross-fade has no jump between adjacent days |
| 4 Weather Rendering | Particles exist only where the per-cell precipitation channel is above 0; cloud shadow darkens by 5–30%; wet ground under rain; snow follows pack and temperature |
| 5 / 5b Succession | Condition and class changes fade over the configured weeks without a single-frame jump; the new palette is reached at the end |
| 6 Continuous Zoom | 24 zoom levels, overview to single cell: no blank frames, constant ink pixel width, no more than one hatch spacing switch per octave |
| 7 Full-Scale Rendering | Large 104×64, everything on: median frame ≤ 33.3 ms |
| 8 Full-Scale Zoom | Same through the zoom sweep; tile cache within its budget |
| 9 Full-Scale Simulation | One game year at Fast: 30 fps; a condition change is visible within 1 frame; a weather update is visible on the frame its channel changes |
| 9b Weather Particle Performance | 30%+ precipitation coverage at overview: 30 fps; live particles never exceed the budget and spawn only in the viewed rect |

Pass/Fail Metrics table: replace "All 12 biomes", "All 5 states", "Manual inspection" with the rows above. Pixel measurements are used only for colour distinctness and zoom; every other check asserts on data and counters. The user reviews a fixed capture set once per feature.

---

## 5. Naming reconciliation (decided by the user 2026-11-24 as proposed; the GDD session applies it)

| Item | The conflict | Proposal |
|---|---|---|
| **Biome set** | `TileRenderingPipeline`/`V1_TileRendering`: 12 Phase-2 fantasy biomes. `ColourPaletteAndLighting`: 8 palette biomes + coastal water. Code: a data-driven Biome Registry (10 generated biomes + 3 succession biomes). | The **registry is the truth**. The 8 palette biomes (Grassland/Steppe, Temperate Forest, Wetland/Delta, Desert/Dry Basin, Alpine/Mountain, Tundra/Boreal, Rainforest, Volcanic/Ashland) map onto the generated land biomes; Ocean and Lake use the water table. Drop the 12 fantasy names from V-1 and the pipeline doc, or keep them as lore names only. |
| **Succession biomes** (Scrub, Burned Ashland, Desertified Plain) | No GDD palette. | Provisional: Scrub = Grassland primary desaturated 25% with Desert accent; Burned Ashland = Volcanic primary with Alpine secondary; Desertified Plain = Desert palette. **Confirmed by the user 2026-11-24; stands until the GDD supplies its own.** |
| **Season names** | Spring/Summer/Autumn/Winter (V-1, pipeline) vs Bloom/Hunt/Dry/Freeze (palette and lighting). | **Bloom/Hunt/Dry/Freeze** everywhere; the palette doc is the locked colour source. |
| **Ecology states** | Five (Thriving, Stable, Recovering, Depleted, Barren) in V-1/pipeline vs four (Thriving, Stable, Stressed, Collapsed) in Style Core and sort layers. | The map renders a **continuous health value in four bands** (Collapsed < 0.33, Stressed < 0.66, Stable, Thriving) matching Style Core's saturation rules. The five-state names are A1's hysteretic labels; they are not separate visuals. A1 owns the label set. |
| **Snow** | `snow_cover: bool` in pipeline §7.1/7.2 vs A4's continuous snow pack. | Continuous pack, blended by slope, elevation and temperature (see §3.3). |
| **Weather darkening** | Uniform 40% multiply (§7.2) | Local cloud shadow, 5–30% (see §3.3). |

---

## 6. `03_VisualBible/AestheticStyleCore.md` and `ColourPaletteAndLighting.md`

No change to any locked number. Add:

- **Style Core, Part 3 ("What CoW visually IS NOT")**, exception note: "Post-processing that fights the flat illustration (lens flare, bloom overload) remains forbidden. Permitted: tilt-shift depth of field, aerial haze toward paper tone, a painted sky, inked depth silhouettes, a vignette, and boil on terrain ink (the ink redraws about 12 times a second). They are part of the illustration."
- **Style Core, Part 4**, Layer 2: "Ecology, succession, weather, water and fire are expressed through the world art itself, per cell, as the Visual Bible requires; the map renders them with no HUD."
- **Palette doc, Part 7**: "Heat haze (Dry season, Grassland and Forest) is an optional per-cell UV shimmer; not required for V-1."
- **Shape Language, Part 9 (outline weight)**: unchanged. The renderer implements it as ink width by depth zone (1× / 1.2× / 1.5×).

---

## 7. Open questions for the GDD session

1. **Succession biome palettes** (§5): confirm or replace the provisional ones.
2. **Fire.** Wildfire fronts and scars are drawn on the map, but no GDD system owns a per-cell burn intensity and scar age. A1's natural ignition and A3's Burned Ashland imply one. Should A1 publish it, or is fire a future system? (V-1 uses a stand-in until decided.)
3. **Herds on the map.** A1 Population Tokens will define herd positions. Until then, the map shows stand-in herds and ambient birds. Is an ambient-wildlife layer wanted as a visual-only system, or only the simulated one?
4. **Time of day.** The Visual Bible has seasonal lighting only. V-1 has no day/night. Confirm that is intended.
5. **Hex readability.** The map hides the hex grid. How should players read cells (on-demand grid, hover outline, Layer 3 icons)? Proposed for V-2/V-4, but it affects the map camera and selection design.
6. **Boil and tilt-shift as options.** Both are strong stylistic choices; do you want them as player-facing settings or fixed?
