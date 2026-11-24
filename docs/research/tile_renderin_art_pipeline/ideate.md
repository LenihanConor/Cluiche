# Research: Ideate — Tile Rendering & AI-Only Art Pipeline (V-1)

**Input:** docs/research/tile_renderin_art_pipeline/explore.md

Candidates are of two kinds:
- **Whole map approaches** (C1, C2, C3, C9, C10): how the map is drawn end to end. Pick one.
- **Building blocks** (C4 to C8): pieces any approach needs. Pick several.

Every candidate keeps every data channel from the explore inventory (terrain, water, biome, ecology, succession, season, weather). None of them needs a human artist.

## Candidates

### Candidate 1: Illustrated 2D Field (grow the 2D cellfield)
**Home module/system:** Dia: extend DiaBgfx `CellFieldRenderer` + `cellfield` shader into a terrain mode (or a sibling `TerrainField2DRenderer`). CoW: biome-to-visual data and the V-1 stage.
**Size:** L
**Description:** Keep the map 2D and ortho, and turn today's data-layer renderer into the world art.
- **Ground:** the shader already blends 4 cells smoothly. It gains a texture array of biome ground textures, splat-blended by biome weight with noise-warped borders and hex-tiling sampling to hide repeats.
- **Height:** an oblique projection offsets each vertex up the screen by elevation, plus the existing hillshade. Trees, rocks and peaks are y-sorted upright sprites, as in the *Strategic Map Overview* reference, which is itself a 2D illustration.
- **Data channels:** weather, ecology, water and snow are more R8/R32F cell textures the same shader samples.
**Primary value:** The cheapest route to the reference look, built on a renderer that already does per-cell GPU data and blending.

### Candidate 2: 3D Hex Heightfield Terrain
**Home module/system:** new Dia package `DiaTerrain3D` (chunked heightfield mesh + terrain material), consuming DiaCellLayers. Rendered via DiaBgfx3D.
**Size:** L–XL
**Description:**
- **Mesh:** generated from S-1 elevation at N×N sub-vertices per cell, smoothed across cells. Chunked to fit 16-bit indices and built once per world.
- **Terrain shader:** samples the same per-cell data textures as C1 (biome splat, wetness, snow, health), plus slope-based cliff/rock.
- **Camera:** the existing Orbit behaviour gives a tilted perspective, with pitch flattening toward top-down at strategic zoom.
- **Mountains:** real geometry, with optional peak props for Hilda character.
- **Lighting:** shadows from the existing shadow map.
**Primary value:** True perspective, horizon and height; "grand" scale with real occlusion and lighting.

### Candidate 3: Pure-Procedural Ground (zero AI textures)
**Home module/system:** Dia shader library (in C1 or C2's material) + CoW palette data.
**Size:** M
**Description:**
- **Ground:** no ground texture images. The surface is generated in the shader from the GDD palette tokens (Primary/Secondary/Accent allocation), layered noise for patches and variation, procedural hatch strokes, and grain.
- **AI's role:** limited to props and icons.
- **Fit:** works with C1 or C2 as the ground source.
**Primary value:** Removes the riskiest AI task (consistent seamless textures) and makes the look fully deterministic and tunable from JSON.

### Candidate 4: AI Asset Forge (prompt-to-engine pipeline)
**Home module/system:** DiaCLI asset handler (`ai_texture` / `ai_sprite` post-processing; engine-side because any game can use it) + CoW prompt sets and references under `Global/Art/`.
**Size:** M
**Description:** A repeatable, versioned pipeline for AI art:
- **Inputs:** prompt files generated from Visual Bible tokens, with the existing reference PNGs as style anchors.
- **Generation:** Gemini, run by the user or by script.
- **Post-processing in code:**
  - seamless enforcement (offset-blend/Poisson);
  - palette lock to the GDD hex tokens;
  - background removal and premultiplied alpha for sprites;
  - scale normalisation, mip generation and texture-array packing.
- **Outputs:** a manifest recording prompt, seed and model per asset so any asset can be regenerated.
- **Quality gate:** a contact-sheet preview for the user's taste check.
**Primary value:** Turns "I have no artist" into a deterministic build step; every visual candidate needs it.

### Candidate 5: Data-Driven Ink Pass (NPR style layer)
**Home module/system:** Dia: terrain/field shader functions + optional screen-space pass (needs RB-001 revisit). CoW: depth-zone and outline-weight data.
**Size:** M
**Description:** The Hilda look lives in code, not art:
- **Outlines:** ink outlines drawn from *data* boundaries (coastline, biome borders, cliffs from slope, river banks, lake shores), with GDD weights by depth zone (1×/1.2×/1.5×).
- **Hatching:** a hatch pattern thresholded by depth-zone coverage targets (10–20 / 30–50 / 60–80%).
- **Colour:** a paper-grain multiply, and the season colour grade (Bloom/Hunt/Dry/Freeze tokens and blend modes) plus depth-zone ambient.
**Primary value:** Guarantees style consistency over any AI asset and encodes depth zone and season exactly as the GDD specifies.

### Candidate 6: Prop Scatter (vegetation, rocks, peaks)
**Home module/system:** Dia: instanced sprite/billboard renderer (2D batched or 3D instanced, depending on C1/C2). CoW: per-biome prop sets and density rules.
**Size:** M
**Description:**
- **Placement:** deterministic per-cell scatter (seeded by cell id) of a few AI-made prop sprites per biome (trees, shrubs, rocks, peaks, reeds).
- **Density and variant from data:** A1 producers/health (Thriving lush → Collapsed bare), succession (cross-fade by species group), snow (white-capped variant via shader), season (tint).
- **Wind:** sways props from A4.
- **Engine work:** instancing or a batched dynamic buffer, since `SpriteRenderer` is one draw per sprite today.
**Primary value:** Ecology becomes visible as the land itself (dense forest vs bare ground), which is the GDD's Layer 2 intent.

### Candidate 7: Weather & Water Presentation Layer
**Home module/system:** Dia: generic particle emitter package (`DiaParticles`) + shader functions. CoW: A4/A5 channel mapping.
**Size:** L
**Description:**
- **Water:** rivers as a flow-mapped water band along S-1's `downstream` network, width from A5 flow state (Dry/Low/Normal/Flood/Frozen); lakes, floods and ice as extent-masked water, sheet and ice surfaces.
- **Weather:**
  - cloud cover as scrolling noise shadows from A4's smoothed state;
  - snow as a continuous pack × slope × elevation blend;
  - rain and snow particles with wind drift, culled to the viewport;
  - optional heat haze in Dry.
- **Art:** all procedural or near-zero assets.
**Primary value:** Weather and water become legible at a glance and animate the map, all at roughly zero art cost.

### Candidate 8: Look-Dev Spike (vertical slice before commitment)
**Home module/system:** CluicheTest or CoW throwaway stage + a few Dia shader experiments behind a flag.
**Size:** S
**Description:** One week to answer "can this look like the reference?"
- **Assets:** 3 biomes of Gemini textures and props through a manual forge (C4-lite).
- **Render:** one S-1 Small world drawn both ways, C1 (2D oblique) and a minimal C2 (heightfield, one chunk, one shader).
- **Style:** outline + hatch + grade (C5-lite).
- **Judging:** screenshots compared side by side with *Strategic Map Overview*.
**Primary value:** Decides 2D vs 3D and AI-texture vs procedural ground on evidence, not guesses.

### Candidate 9: AI Stamped Tiles (the GDD pipeline, artist replaced by Gemini)
**Home module/system:** CoW assets + Dia atlas/DDS loader.
**Size:** M (engine) + high asset effort
**Description:** Keep the locked mechanism and have Gemini generate the 180 tiles (biome × state × season) instead of an artist. Pack them into atlases and add the seasonal shader. Kept as the baseline.
**Primary value:** Least design change; matches the locked GDD.

### Candidate 10: AI-Painted Macro Map (img2img restyle of the data render)
**Home module/system:** CoW tooling + C4 forge.
**Size:** M
**Description:**
- **Generation:** a whole-world data render (biome colours, relief, rivers) per world is fed to Gemini as an image-to-image input with the Hilda references, producing a painted illustration of *that* world. The painting is used as a macro texture at far zoom, blended under the live C1/C2 layers.
- **Live state:** ecology and weather still come from the live layers.
- **Rebuild trigger:** regenerated when a world is baked (F-3), not weekly.
**Primary value:** The closest match to the reference at strategic zoom, because it *is* an AI illustration of the actual world.

## Coverage Map

- **Geometry axis:** flat/2D (C1, C9, C10), 3D (C2), and a spike that compares them (C8).
- **Ground source axis:** AI textures (C1/C2 default, C9 extreme), zero-texture procedural (C3), whole-map AI painting (C10).
- **Style ownership:** baked into art (C9, C10) vs shader-owned (C5).
- **Data channels:** ecology (C6), weather and water (C7), season and depth zone (C5), biome and terrain (C1/C2/C3). Every row in the explore inventory has an owner.
- **Art pipeline:** C4 is shared infrastructure.
- **Scope:** from S (C8) to XL (C2 full).
- **Engine vs game:** each candidate names its Dia and CoW parts. Dia work (texture arrays, instancing, particles, terrain mesh, ink functions) needs AD-001 sign-off.
- **Likely shape of the answer:** one approach (C1, C2 or a hybrid) plus building blocks C3/C4, C5, C6 and C7, sequenced behind C8.
