# Research: Explore — Tile Rendering & AI-Only Art Pipeline (V-1)

**Session date:** 2026-11-20
**Folder:** docs/research/tile_renderin_art_pipeline/
**Testbed:** V-1 Tile Rendering (CoW, backlog #18)

## Problem Space Overview

CoW's strategic map must be a "living illustrated map" (GDD AestheticStyleCore Part 2): a Hilda-style game board that is beautiful and readable, and that expresses the simulation through the art itself. The world-sim stack already produces rich per-cell data on a hex grid (Small to Large, up to 104×64 = 6,656 cells). That data includes terrain/elevation, biome, rivers/lakes/floods/ice (A5), ecology health and vegetation (A1), succession (A3), and weekly weather (A4). The renderer's job is to turn those fields into a picture a player reads at a glance, from a full-world strategic zoom (~18 px per cell at 1080p) down to a single cell.

The locked GDD pipeline (Phase 9 `TileRenderingPipeline.md`) assumes a human artist authoring ~180 vector tiles in Illustrator/Inkscape (12 biomes × 5 ecology states × seasons), packed into atlases and tinted by a seasonal shader. **The user has no artist and no art-production capability beyond AI (Claude, Gemini).** That breaks the *mechanism*, not the *look*. The research question is: what rendering approach plus asset pipeline delivers the locked look using only AI- and code-authored assets, scales from one cell to the full map, avoids the "tiled" look of repeated stamps, and shows every simulation layer that matters to gameplay?

The user's direction: perspective and height on the full map to make it feel grand. They are open to a hybrid (AI base assets plus code variants). Particles and animation stay **in** scope, as tools for showing data (rain, snow, wind, flow, migration).

## What the GDD Locks (and what is negotiable)

**Keep (the look, the acceptance test):**
- **Style:** Hilda: bold clean ink outlines with weight variation, flat colour fills, paper grain/ink texture, strong silhouettes. Not photoreal, no PBR, no normal-mapped detail, no bloom/lens flare. No text and no human structures on the map.
- **Three layers:** Layer 1 world art (biome, terrain, hatching, rivers/coasts/elevation, seasonal light), Layer 2 world-state expression (ecology through the art), Layer 3 actionable icons on top. Overlays (sort Layer 6) are player-toggled and must not replace the world-art signal.
- **Depth zone:**
  - Ink hatch coverage 10–20% / 30–50% / 60–80% (First Shores / Wild Reaches / Ancient Interiors).
  - Terrain outline weight 1× / 1.2× / 1.5×.
  - Hostile-extreme hue/lightness shift with numeric bounds.
- **Palette:** per-biome hex tokens (Primary 60% / Secondary 30% / Accent 10%).
- **Seasons:** a **canvas-wide colour-grading pass** (Bloom +8% gold Screen, Hunt none, Dry +6% ochre Screen, Freeze +12% blue-grey Multiply), plus a few per-biome callouts.
- **Ecology expression:**
  - Stressed: desaturate 15–25%, sparse vegetation.
  - Collapsed: desaturate 40–60%, bare terrain.
  - Thriving: lush and saturated.
  - Stable: baseline.
- **Weather:** reads A4's **smoothed** state (blend of last two weekly snapshots).
- **Water:** A5 draws lake extent, floods, ice, and river width by flow state as overlays. **Soil and ground water are not rendered** (debug-only).
- **Reference images:** ~30 AI-generated PNGs in `03_VisualBible/Visual Artifacts/` already define the target. The key one is *Strategic Map Overview*: an oblique, painted map with 3/4-view mountains, forests drawn as tree clusters, winding rivers, cliffed coasts and node icons. *Biome Palette Reference Sheet* shows 8 biomes. The Visual Artifact Manifest holds the prompts used.

**Break / reopen (the mechanism):**
- Vector authoring in Illustrator/Inkscape by an artist.
- 180 discrete tiles (biome × state × season) and tile swaps on state change.
- 1024 px per-tile PNGs packed into 12 × 4096² DDS atlases.
- Scalar cloud darkening only, binary snow, and "static tiles only, animation out of scope".

**GDD inconsistencies to resolve (fold back via the GDD session):**
- **Biome set:** the pipeline doc has 12 Phase-2 fantasy biomes, the palette doc has 8 real biomes plus Coastal Water, and the code is moving to a data-driven Biome Registry (`biomes.json`). The renderer must key off registry data, not an enum.
- **Ecology states:** 5 (Thriving/Stable/Recovering/Depleted/Barren) in the pipeline doc and V-1, against Thriving/Stable/Stressed/Collapsed in the sort layers and style core.
- **Season names:** Spring/Summer/Autumn/Winter (pipeline doc, V-1) against Bloom/Hunt/Dry/Freeze (palette and lighting).
- **Weather inputs:** the pipeline doc says `snow_cover: bool`, but A4 has a continuous snow pack.

## Existing Approaches

**Map rendering:**
- **Stamped tiles** (Civ I–IV, early 4X): one sprite per cell. Cheap, but repetition and hard hex edges read as "tiley" at zoom-out.
- **Blended continuous terrain** (Civ V/VI, Humankind, Old World): terrain is a mesh or heightfield. Per-vertex or per-texel **splat weights** blend biome textures across hex borders, with noise-warped boundaries, macro colour variation, and decals/props scattered on top. Hex identity survives only as an optional grid overlay.
- **Painted-map heightfield** (Total War campaign maps, Imperator, Old World): a low-relief heightfield with a tilted perspective camera, a stylised shader and painted textures. Mountains are geometry or billboard props. This is the "grand" look.
- **Billboard props** (Civ VI forests, Bad North): instanced camera-facing or upright sprites/meshes scattered per cell by density, with no unique art per cell.
- **Stylised / NPR shading:**
  - Toon ramps, screen-space or mesh-edge **ink outlines** (depth/normal edge detect, or outlines from data boundaries such as coast, biome border and cliff).
  - **Hatching** via tonal art maps or a screen- or world-space hatch texture thresholded by a density value.
  - **Paper grain** as a screen-space multiply.
- **Texture repetition breakers:**
  - Stochastic / hex-tiling sampling (Heitz–Neyret 2018, Mikkelsen 2022 hex-tiling).
  - Multi-scale detail blending.
  - Per-cell rotation and offset.
  - Wang tiles.
- **Data-to-visual channels:**
  - Per-cell fields uploaded as small GPU textures and sampled with bilinear filtering in the terrain shader, which gives smooth fronts for free.
  - Weather as scrolling cloud-shadow noise masked by cloud cover, and snow as a slope- and height-aware blend.
  - Water as flow-mapped river ribbons and a lake surface masked by extent.
  - Particles for precipitation and wind.

**AI asset generation (2026):**
- **Gemini image generation** (Gemini 2.5/3 image models, Imagen): strong at painterly, style-matched images when conditioned on reference images. Weak at: exact seamless tiling, consistent scale across a batch, true transparency (alpha usually needs background removal), and strict top-down orthographic views.
- **Claude** (no raster output): writes SVG, procedural texture generators (Python/numpy, noise, Voronoi), shader code, and prompt sets. Output is deterministic, versioned and re-runnable, but reads more graphic than painted.
- **Post-processing in code:** seamless-ify (offset-and-blend or a Poisson seam), palette quantise to the GDD hex tokens, background removal (rembg/U²-Net, free), alpha from a solid-colour key, and downscale/mip generation.
- **Free tools for light manual cleanup:** Krita, GIMP, Inkscape.

## Design Axes

| Axis | Options | Notes |
|------|---------|-------|
| **Map geometry** | A. flat 2D hex sprites · B. 2D blended field (today's cellfield) · C. 2.5D oblique sprites · D. 3D heightfield mesh + tilted camera | D gives perspective and height. B is already built in 2D. C is the hardest for AI consistency |
| **Ground surface** | per-cell stamped image · splat-blended biome textures · pure procedural (noise + palette, no textures) | Splat keeps the AI asset count at roughly one texture set per biome |
| **Style source** | baked into art · shader (outline, hatch, grain, palette grading) · both | A shader-held style makes the look global and survives weak AI images |
| **Art source** | Gemini raster · Claude SVG/procedural · free-tool cleanup · purchased/CC0 packs | CC0 packs rarely match Hilda; licence and style fit are both issues |
| **Features (trees, rocks, peaks)** | none (texture only) · billboard sprites · low-poly toon meshes · procedural meshes | Peaks may be geometry (heightfield), with props for character |
| **Variant strategy** | discrete per state · parametric (tint, density, blend weights from data) · cross-fade over time | Parametric collapses 180 → ~1 set per biome; cross-fade gives succession for free |
| **Data upload** | CPU per-cell lookup to sprites · per-cell GPU textures (R8/R32F) sampled in shader | Cellfield already does the second |
| **Weather presentation** | scalar tint · cloud-shadow noise · particles · animated wind on grass/props | All procedural; zero art |
| **Water presentation** | biome texture only · river ribbons (spline mesh) · river mask in terrain shader · flow-mapped water shader | River network exists from S-1 (`downstream`/flow) |
| **Hex readability** | invisible · grid overlay on demand · subtle per-cell borders | Gameplay needs to pick cells; the GDD says hexes are a functional board |
| **Zoom / LOD** | single detail level · mips + prop density by distance · macro texture at far zoom | Far zoom must keep biome regions legible (V-1 Test 1b) |

## Known Tradeoffs

- **Painted look vs consistency.** AI raster gives the closest Hilda feel, but every generated image drifts. The more unique images the pipeline needs, the worse consistency gets. Fewer, reusable, parametric assets beat many bespoke ones.
- **Hex identity vs illustration.** Blending hides the grid (good for beauty, bad for reading cell ownership). An on-demand grid overlay plus Layer 3 icons can recover it.
- **Perspective vs readability.** A tilted camera adds grandeur, but occludes far cells behind peaks and compresses the far map. Typical answers are a modest tilt (35–55°), low vertical exaggeration, and zoom-dependent pitch (more top-down when zoomed out).
- **Shader style vs authored style.** A procedural ink/hatch can look "filtered" if done naively. Driving outlines from *data* boundaries (coasts, biome borders, cliffs, rivers) reads more like illustration than screen-space edge detection.
- **Smooth data vs cell truth.** Bilinear-filtered weather and ecology look natural, but can blur sharp facts (a single burned cell). Mixed filtering per channel helps.
- **Cost of animation.** Wind sway, scrolling cloud shadows and particles are cheap on the GPU, but Dia has no particle system or instancing, so they are engine work.

## Known Pitfalls (C++ / game engine context)

- **Repetition at zoom-out.** Any per-cell stamp or tiled texture shows a grid pattern when 6,656 cells are on screen. It needs stochastic sampling or a macro variation layer.
- **Mip and filtering bleed.** Atlased tiles bleed at mip levels. Texture arrays avoid this, but Dia has no texture arrays or mips today.
- **AI seams.** Generated "seamless" textures usually aren't. The pipeline must enforce tiling in post-processing or the shader must hide seams (hex-tiling).
- **Alpha for props.** Gemini rarely outputs clean alpha. A background-removal step plus premultiplied alpha is needed to avoid halo fringes on billboards.
- **Mesh limits.** 16-bit indices mean ≤65,535 vertices per mesh. A Large heightfield at 4×4 sub-vertices per cell is ~106k vertices, so the terrain needs **chunking**.
- **Static meshes only.** Elevation is fixed after worldgen (fine), but anything per-week (water extent, snow) must go through textures, not mesh rebuilds.
- **Draw-call count.** `SpriteRenderer` submits one call per sprite. Thousands of tree billboards need instancing or a batched dynamic buffer.
- **Determinism and threading.** The renderer must read sim state through FrameStream snapshots (PU boundary rule). Weather smoothing needs the two last snapshots.
- **Depth sorting.** Alpha-blended props on a tilted map need sorting or alpha-to-coverage/cutout.

## Cluiche-Specific Opportunities

### Relevant Existing Modules (verified 2026-11-20)

| Module | Relevance |
|--------|-----------|
| **DiaBgfx / DiaBgfx3D** | The only render path (bgfx on D3D11, SDL3 window). `Canvas3D` is always created, so CoW already runs on a 3D-capable canvas |
| **DiaBgfx `CellFieldRenderer` + `cellfield` shader** | **Closest existing piece.** Per-cell values on the GPU as R32F textures refreshed with `updateTexture2D`, palette/ramp colouring, **smooth 4-cell blending**, and hillshade from an elevation texture. Cached hex geometry (`HexFlatTopOddQBuilder`). It is 2D only (z = 0, ortho camera). This is effectively a data-driven 2D splat renderer already |
| **DiaCellLayers / CellField** | Registry of per-cell data layers that CoW systems publish into (`WorldGenViewHost`). The renderer can source every channel from here |
| **DiaMesh3D / MeshGpuCache / MeshRenderer** | Procedural mesh upload (`Mesh3DAsset::Populate` + `RegisterMesh`). Fixed `Vertex3D` (pos/normal/tangent/uv0/RGBA8), 16-bit indices, static buffers, ≤4096 draws, lights and a shadow map |
| **DiaCamera3D** | Perspective and ortho cameras; Orbit (yaw/pitch/radius), SmoothDamp, BoundsClamp behaviours. A tilted strategy camera is mostly built |
| **ShaderProgram / MaterialRegistry** | A module can load any cooked vs/fs pair and register a material. The shader cook (`shaderc`, `.sc`) only scans `Dia/DiaBgfx/Shaders` and `Dia/DiaBgfx3D/Shaders`. A game-side shader root needs adding to `pipeline.toml` |
| **DiaCLI asset pipeline** | `dia pipeline` → build-assets via handlers. Texture handling is pass-through PNG (no mips, compression or texture arrays). There is a place to add an AI-asset post-processing handler (seamless, palette-lock, mips, background removal) |
| **S-1 / A4 / A1 / A3 / A5** | Producers of the data: elevation, landmass/coast, flow direction and rivers, regions/zones (depth zone), and biome (registry index). Weather fields (cloud, precipitation, snow pack, wind, temperature). Ecology (producers, groups, health index). Succession transitions. Water (lake extent, flood, ice, river flow state) |
| **Gaps (not in Dia today)** | No 3D terrain/heightfield, texture arrays, mips/DDS, dynamic vertex buffers, instancing, particle system, post-process passes (RB-001 says out of scope), or runtime texture update outside `CellFieldRenderer` |

**Doc drift found** (fix or promote separately): `pipeline.toml` still deploys `sfml-*.dll`; several module docs mention SFML. `dia.bgfx3d.architecture.module.md` lists MeshRenderer/ShadowRenderer/MeshGpuCache as "Pending" though they are built.

### Platform Decision Constraints

| Decision | Implication for this topic |
|----------|---------------------------|
| PD-001 StringCRC | Biome, prop-set and channel identifiers are StringCRC keys from registry data; no string lookups per frame |
| PD-002 PU/Phase/Module | Terrain renderer is a module on the render side. Sim data crosses via FrameStream snapshots (feedback: PU boundaries are always streams) |
| PD-004 No STL in public APIs | Terrain, splat and prop APIs use DiaCore containers |
| PD-005 x64 only / D3D11 | bgfx D3D11 path. Texture arrays and instancing are supported by bgfx on D3D11, so the gap is Dia, not the backend |
| PD-007 C++20 | No constraint beyond the norm |
| Layer boundary rule (CLAUDE.md) | Heightfield terrain, splat blending, texture arrays, instancing, particles and NPR outline/hatch are **engine** (Dia, any game could use them). CoW owns the biome-to-texture mapping data, channel-to-visual parameters, prompts and the V-1 stage. New Dia headers need escalation (AD-001) |
| AD-001 sign-offs | Every Dia addition above needs explicit sign-off before build |
| Feedback: check existing Dia packages first | Extend `CellFieldRenderer`/cellfield and DiaMesh3D before proposing a new renderer package |
| Feedback: cell data over pixels | V-1 correctness can be partly asserted on data (channel-to-parameter mapping); taste checks stay as one screenshot per feature for the user |

## Data-to-Visual Channel Inventory (first pass)

Every gameplay-relevant layer needs a channel. Nothing is dropped. Soil and ground water stay debug-only per the GDD.

| Source | Data | Candidate visual channel | Art needed |
|---|---|---|---|
| S-1 | elevation, slope | heightfield displacement, slope shading/hillshade, cliff/rock blend on steep slopes | rock texture |
| S-1 | landmass, coast | shoreline ink outline (data-driven), foam/surf band, shallow-water tint | none (procedural) |
| S-1 / A5 | river network, flow, width state (Dry/Low/Normal/Flood/Frozen) | river mask or ribbon with flow-mapped water, width from flow state | water texture or procedural |
| A5 | lake extent, flood, ice | water surface masked by extent texture, flood as translucent sheet, ice tint and crack pattern | procedural |
| S-1 / A3 | biome (registry index) | splat blend of biome ground textures with noise-warped borders, palette tokens | 1 texture per biome |
| S-1 | depth zone | hatch density threshold and outline weight; hostile-extreme hue shift | hatch texture (procedural) |
| A1 | producers / vegetation, health index | prop density (trees/grass), saturation per the GDD bands, barren blend | ~3–6 prop sprites per biome + bare-ground texture |
| A3 | succession transitions | cross-fade splat weights over weeks (no tile swap) | none |
| A4 | season / day of year | canvas colour grade (GDD tokens) + per-biome callouts | none |
| A4 | cloud cover (smoothed) | scrolling cloud-shadow noise masked by cover | none |
| A4 | precipitation type/intensity, wind | particles (rain streaks, snow flakes) with wind drift; grass/prop sway | 2 tiny sprites or procedural |
| A4 | snow pack | snow blend by pack × slope × elevation | snow texture (procedural) |
| A4 | temperature, storms | optional heat haze in Dry; storm darkening | none |
| A1 (fauna) | herds, migration fronts | Layer 3 icons / route markers (GDD), not terrain art | icon set (separate) |

### 2D growth path (user note, 2026-11-20)

Dia has a full 2D stack (DiaBgfx sprites, `CellFieldRenderer`, Ortho2D cameras, DiaGraphics2D/Geometry2D) that **could be grown** in place of going 3D. A 2D route can still fake height:
- vertical offset of cells and props by elevation in an oblique projection;
- hillshade (already built);
- y-sorted upright sprites for trees and peaks, as in the *Strategic Map Overview* reference, which is itself a 2D illustration.

This keeps the renderer on code the project already owns and tests. The cost is that true perspective (foreshortening, horizon) and camera tilt are fixed or faked.

## Open Questions for Ideation

1. **Geometry:** is a 3D heightfield (D) affordable in Dia, or does an extended 2D cellfield with oblique fake height get most of the grandeur for far less engine work?
2. **Ground textures:** can Gemini produce ~10 consistent, palette-locked biome textures conditioned on the existing reference PNGs, or should ground be fully procedural (noise + palette + hatch) and AI used only for props?
3. **Mountains:** heightfield geometry with toon/ink shading, AI billboard peaks (as in *Strategic Map Overview*), or both?
4. **Style ownership:** how much of the Hilda look can a shader pass carry (data-driven ink outlines, hatch by depth zone, paper grain, palette grading)? Is a post-process pass needed (RB-001 currently excludes it)?
5. **Props at scale:** instancing vs batched dynamic buffers vs baking props into a per-chunk texture. How many props at Large?
6. **Variant strategy:** confirm parametric (data drives tint, density and blend) over discrete tiles, so the AI asset list is roughly *biomes × 1 texture + a prop set*, not 180.
7. **Engine vs game split:** which pieces are Dia packages (terrain mesh, splat, instancing, particles, NPR) and which are CoW data (mapping, prompts)? What is the smallest Dia surface?
8. **Proof first:** what is the cheapest spike that answers "can this look like the reference?" before committing (one Gemini texture set + S-1 heightfield + outline shader, judged against *Strategic Map Overview*)?
9. **GDD fold-back:** biome set, ecology state names, season names, snow bool vs pack. These need one answer before a spec.
