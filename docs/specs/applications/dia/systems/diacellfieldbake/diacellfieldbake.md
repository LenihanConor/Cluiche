# System Spec: DiaCellFieldBake

## Parent Application
@docs/specs/applications/dia/dia.md

**Gameplay Domains:** rendering
**Research:** docs/research/tile_renderin_art_pipeline/summary.md (prototype bake: `lookdev/js/world.js`)
**Consumers:** DraoiHexWorldRender3D (3D map, first consumer, @docs/specs/applications/draoi/systems/draoihexworldrender3d/draoihexworldrender3d.md) and DraoiHexWorldRelief (relief and rivers through the post-pass hook, @docs/specs/applications/draoi/systems/draoihexworldrelief/draoihexworldrelief.md). Optional: the 2D cellfield renderer (DiaBgfx `CellFieldRenderer`), to get smooth borders that don't follow the hex outline.

**Layer split (2026-11-23, user):** the name stays (`Dia*` marks a project; `Bake` is the precompute step in the CellField family: DiaGraphics `CellField` module = grid, `DiaCellFieldBake` = fields, DiaBgfx `CellFieldRenderer` = 2D drawing). Feature 2 Relief and Rivers moved to Draoi as DraoiHexWorldRelief, because relief classes, lakes, sea and rivers are terrain vocabulary (CFB-006).

## Purpose

DiaCellFieldBake turns per-cell layers on a cell grid into **continuous fields**. These are plain arrays at a chosen texel resolution, ready to upload as textures or to sample on the CPU. It contains no rendering and no GPU code.

It exists because a renderer that stamps one value per cell shows the grid: hex staircases, hard class edges, flat plateaus. The research prototype showed what fixes that, all of it data processing:
- classify each texel by a weighted majority over two rings of neighbours;
- keep a margin field so borders can be inked;
- smooth the scalar layers.

Relief by terrain class and the river distance field were also part of the prototype bake. They are terrain vocabulary, so they live in DraoiHexWorldRelief and plug in through this system's tile post-pass hook.

Splitting it from the renderer (user decision, 2026-11-20) has three benefits:
- it can be tested in isolation, with deterministic output hashes;
- the 2D and 3D renderers can share it;
- it keeps the renderer (DraoiHexWorldRender3D) as rendering only.

**Dependency chain:**
`DiaCellFieldBake → DiaGraphics CellField (CellGridDesc: centres, neighbours, topology), DiaCellLayers (layer views), DiaParallelExecutor (`IParallelExecutor`, new module extracted from DiaDeferredStep, see backlog), DiaObservation, DiaCore`

## Prototype Reference

The approved browser prototype is the working reference for this system: `docs/research/tile_renderin_art_pipeline/lookdev/js/world.js`.

| Prototype code | Feature |
|---|---|
| `World.buildFields` (19-cell ring, warp, majority class, margin, smooth fields) | 1 |
| `MOUNTAIN_WEIGHT` and `ridged()` relief, lake surface height, ocean depth | moved to DraoiHexWorldRelief |
| `World.buildRivers` (wiggled segments, distance/width/direction field) | moved to DraoiHexWorldRelief |
| `cellAtWarped` (owner lookup that matches what is drawn) | 1 (`OwnerAt`) |

## Responsibilities

**Smooth fields.** Per texel, a Gaussian-weighted average over the owning cell and two rings of neighbours (19 cells on a hex grid), sampled at a noise-warped point. Any number of scalar layers can be baked, such as elevation, land fraction, lake fraction and temperature. The neighbourhood never changes abruptly at a cell edge, so the fields are continuous.

**Majority classification.**
- **Primary class:** each texel's primary class (for a hex terrain consumer: land, ocean or lake) is decided by summed weight.
- **Secondary class:** within that primary class, the secondary class (for example a biome) is decided by summed weight per class.
- **Owner cell:** the highest-weighted cell of the winning class.

**Margin field.** `(best − second) / (best + second)` per texel. It is zero on a class border, so renderers can ink borders at a constant width.

**Extension hooks.** `WarpedPoint(params, x, y)` exposes the warped sample point the bake used for a texel, and `ITilePostPass` lets a consumer run its own per-tile pass inside the parallel and incremental bake (Feature 3). DraoiHexWorldRelief uses both for relief and rivers.

**Queries.** `OwnerAt(x, y)` and `SampleAt(field, x, y)`, so CPU code (prop placement, particle spawning) agrees with what is drawn.

**Determinism.** The same inputs and parameters give a bit-identical output at any thread count. Output hashes support goldens.

**Parallel bake.** The whole world is baked at load in tiles through `IParallelExecutor`, with the serial executor as the oracle and a progress callback for a load overlay.

**Incremental rebake.** Rebuild only the tiles around changed cells (for example A3 succession changing a biome), into a pending set that is swapped in at a frame boundary, so the live world keeps drawing.

**What is baked.** Only layers that are static between class or terrain changes. Per-tick data (weather, condition, events) is a renderer channel and is never baked.

## Non-Responsibilities

- **GPU upload, textures and shaders.** These belong to the consumers: DraoiHexWorldRender3D, or `CellFieldRenderer`.
- **Terrain vocabulary.** Relief, lakes, sea and rivers are DraoiHexWorldRelief (CFB-006).
- **Per-frame channels.** Condition, weather and events are per cell and upload as-is. They need no bake.
- **The meaning of classes.** Biome identities are caller data.
- **Simulation.**

## Public interfaces

The features own the signatures: [fields-and-classes.md](fields-and-classes.md) (`BakeInputs`, `BakeParams`, `BakedFields`, `BakeRegion`, `OwnerAt`, `SampleAt`, `GatherFromRegistry`), [parallel-tiled-incremental.md](parallel-tiled-incremental.md) (`BakeWorld`, `BakedWorld`, `ResolutionPolicy`, `ITilePostPass`); Feature 1 also gains `WarpedPoint`. All in `Dia::CellFieldBake::`. Output is packed (half-float scalars, 8-bit classes, 16-bit owner) at about 17 bytes per texel.

## Features

| # | Feature | Summary | Spec | Status |
|---|---|---|---|---|
| 1 | Fields and Classes | Two-ring Gaussian bake, warp, smooth scalar fields, majority primary and secondary class, owner cell, margin field, `OwnerAt`/`SampleAt`, hashes; DiaCellLayers adapter | [fields-and-classes.md](fields-and-classes.md) | Approved 2026-11-20 |
| 2 | ~~Relief and Rivers~~ | Moved 2026-11-23 to DraoiHexWorldRelief ([relief-and-rivers.md](../../../draoi/systems/draoihexworldrelief/relief-and-rivers.md)) | — | Moved |
| 3 | Parallel, Tiled and Incremental Bake | Whole-world bake at load over `IParallelExecutor` (serial oracle) with a progress callback, per-size default resolution, double-buffered incremental rebake (`RebakeBegin`/`Commit`), `ITilePostPass` hook, metrics, goldens on S-1 Small/Standard/Large | [parallel-tiled-incremental.md](parallel-tiled-incremental.md) | Draft |

Build order: 1 → 3. DraoiHexWorldRelief builds on both.

## Decisions

| ID | Decision | Status | Binding |
|---|---|---|---|
| CFB-001 | The bake is its own Dia system, data-only, separate from the map's rendering (user decision 2026-11-20) | Accepted | Yes |
| CFB-002 | Class borders come from a Gaussian majority over two rings plus a margin field, never nearest-cell ownership (was TMAP-007) | Accepted | Yes |
| CFB-003 | The bake is deterministic and hashable at any thread count | Accepted | Yes |
| CFB-004 | The whole world is baked at load, with a progress callback; incremental rebakes are double-buffered and committed at a frame boundary (user, 2026-11-22) | Accepted | Yes |
| CFB-005 | Only layers static between class/terrain changes are baked; per-tick data is a renderer channel (user, 2026-11-22) | Accepted | Yes |
| CFB-006 | No terrain vocabulary: relief, lakes, sea and rivers are DraoiHexWorldRelief; consumers extend the bake only through scalar layers, `WarpedPoint` and `ITilePostPass` (user, 2026-11-23, layer split) | Accepted | Yes |

## Inherited Binding Decisions

| ID | Decision | How it constrains DiaCellFieldBake |
|---|---|---|
| PD-001 | StringCRC ids | Layer and relief-class ids |
| PD-004 / AD-002 | No STL in public APIs | Raw pointer + count outputs, `DynamicArrayC` internally |
| AD-001 | Module docs, sign-off | New module doc; new Dia system, approved 2026-11-20 |
| AD-003 | `Dia::<Module>::` | `Dia::CellFieldBake::` |
| DFS-004 | Worker-capped executor | The bake takes an `IParallelExecutor`; the capped implementation (DiaDeferredStep) enforces the cap |

## Test Strategy

- **GoogleTests on hand-built grids (2 to 3 biomes, a lake, a ridge, a river):**
  - the margin is zero on the border;
  - no texel takes the class of a cell outside its two rings;
  - a test post-pass is tile independent and rebake safe.
- Relief, lake and river tests moved to DraoiHexWorldRelief.
- **Determinism:** the serial and parallel bakes give identical hashes, and the same hash on every run.
- **Goldens:** S-1 Small, Standard and Large presets × seeds 1–3, through the existing golden store pattern.
- **Performance (Debug, ballpark):** bake time at each size. The prototype JS took 1.2 s at Small.

## Open Design Questions

1. ~~Texel resolution per size.~~ Answered in Feature 3 (`ResolutionPolicy`; consumers may ask for a coarser bake, which also serves the in-game editor's low-res preview).
2. **Does the 2D CellFieldRenderer adopt it?** Doing so would give S-1/S-2.5/S-3 debug views smooth borders, at the cost of reworking a built renderer.

## Status

`Approved` (2026-11-20, user: split out of DiaTerrainMap3D in the 3-way split). Specced now, built with CoW build order #18. Reviewed 2026-11-22: CFB-004/005 added, public sketch replaced by feature pointers, executor moved to DiaParallelExecutor; Features 2 and 3 revised and awaiting approval. **Amended 2026-11-23 (layer split, user):** Feature 2 moved to Draoi (DraoiHexWorldRelief); CFB-006 added; Feature 3 gains `ITilePostPass`; Feature 1 gains `WarpedPoint`. Feature 3 still awaits approval; the Feature 1 amendment was re-confirmed 2026-11-23.
