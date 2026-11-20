# System Spec: DiaCellFieldBake

## Parent Application
@docs/specs/applications/dia/dia.md

**Gameplay Domains:** rendering
**Research:** docs/research/tile_renderin_art_pipeline/summary.md (prototype bake: `lookdev/js/world.js`)
**Consumers:** DiaTerrainMap3D (3D, first consumer, @docs/specs/applications/dia/systems/diaterrainmap3d/diaterrainmap3d.md). Optional: the 2D cellfield renderer (DiaBgfx `CellFieldRenderer`), to get smooth borders that don't follow the hex outline.

## Purpose

DiaCellFieldBake turns per-cell layers on a cell grid into **continuous fields**. These are plain arrays at a chosen texel resolution, ready to upload as textures or to sample on the CPU. It contains no rendering and no GPU code.

It exists because a renderer that stamps one value per cell shows the grid: hex staircases, hard class edges, flat plateaus. The research prototype showed what fixes that, all of it data processing:
- classify each texel by a weighted majority over two rings of neighbours;
- keep a margin field so borders can be inked;
- smooth the scalar layers;
- add relief by terrain class;
- build a distance field for rivers.

Splitting it from the renderer (user decision, 2026-11-20) has three benefits:
- it can be tested in isolation, with deterministic output hashes;
- the 2D and 3D renderers can share it;
- it keeps DiaTerrainMap3D as rendering only.

**Dependency chain:**
`DiaCellFieldBake → DiaGraphics CellField (CellGridDesc: centres, neighbours, topology), DiaCellLayers (layer views), DiaDeferredStep/IParallelExecutor (parallel rows), DiaObservation, DiaCore`

## Prototype Reference

The approved browser prototype is the working reference for this system: `docs/research/tile_renderin_art_pipeline/lookdev/js/world.js`.

| Prototype code | Feature |
|---|---|
| `World.buildFields` (19-cell ring, warp, majority class, margin, smooth fields) | 1 |
| `MOUNTAIN_WEIGHT` and `ridged()` relief, lake surface height, ocean depth | 2 |
| `World.buildRivers` (wiggled segments, distance/width/direction field) | 2 |
| `cellAtWarped` (owner lookup that matches what is drawn) | 1 (`OwnerAt`) |

## Responsibilities

**Smooth fields.** Per texel, a Gaussian-weighted average over the owning cell and two rings of neighbours (19 cells on a hex grid), sampled at a noise-warped point. Any number of scalar layers can be baked, such as elevation, land fraction, lake fraction and temperature. The neighbourhood never changes abruptly at a cell edge, so the fields are continuous.

**Majority classification.**
- **Water class:** each texel's land, ocean or lake class is decided by summed weight.
- **Owning class:** within that water class, the class (for example a biome) is decided by summed weight per class.
- **Owner cell:** the highest-weighted cell of the winning class.

**Margin field.** `(best − second) / (best + second)` per texel. It is zero on a class border, so renderers can ink borders at a constant width.

**Relief.**
- **Terrain-class relief:** extra height per terrain class (for example mountain, peak, volcano, hills), shaped by ridged noise, with the weights supplied as data.
- **Lake surface:** lake cells use their surface height.
- **Ocean depth:** ocean depth is mapped below zero.

**River distance field.**
- **Segments:** from river cells and their flow direction, with deterministic hand-drawn wiggle.
- **Per texel:** signed distance, half-width from flow accumulation, and flow direction.

**Queries.** `OwnerAt(x, y)` and `SampleAt(field, x, y)`, so CPU code (prop placement, particle spawning) agrees with what is drawn.

**Determinism.** The same inputs and parameters give a bit-identical output at any thread count. Output hashes support goldens.

**Parallel bake.** Rows are baked through `IParallelExecutor`, with the serial executor as the oracle.

**Incremental rebake.** Rebuild only a cell's neighbourhood when a class changes (for example A3 succession changing a biome).

## Non-Responsibilities

- **GPU upload, textures and shaders.** These belong to the consumers: DiaTerrainMap3D, or `CellFieldRenderer`.
- **Per-frame channels.** Condition, weather and events are per cell and upload as-is. They need no bake.
- **The meaning of classes.** Biome identities and relief weights are caller data.
- **Simulation.**

## Public interfaces (sketch; finalised per feature)

```cpp
namespace Dia::CellFieldBake {
    struct BakeParams { float texelsPerUnit = 14.f; float sigma2 = 0.7f; float warpAmplitude = 0.5f; uint32_t seed; };
    struct BakeInputs {
        const Graphics::CellGridDesc* grid;
        CellLayerView classLayer, waterClass, terrainClass;   // ids per cell
        const CellLayerView* scalars; uint32_t scalarCount;   // smoothed layers
        ReliefTable relief;                                   // terrain class -> weight (data)
        RiverInputs rivers;                                   // mask, flow direction, accumulation
    };
    struct BakedFields {   // row-major, width x height texels; owned arrays, no STL in the API
        uint32_t width, height;
        float*   scalars;      // scalarCount floats per texel
        uint8_t* owner;        // owner cell (col, row), water class, terrain class
        uint8_t* margin;       // 0 on a class border
        uint8_t* river;        // signed distance, half width, flow dir (packed)
        uint64_t Hash() const;
    };
    bool Bake(const BakeInputs&, const BakeParams&, IParallelExecutor&, BakedFields& out);
    bool RebakeCells(const uint32_t* cells, uint32_t count, const BakeInputs&, const BakeParams&, BakedFields& inOut);
}
```

## Features

| # | Feature | Summary | Spec | Status |
|---|---|---|---|---|
| 1 | Fields and Classes | Two-ring Gaussian bake, warp, smooth scalar fields, majority water class and class, owner cell, margin field, `OwnerAt`/`SampleAt`, hashes | TBD | Draft |
| 2 | Relief and Rivers | Terrain-class relief with ridged noise from data, lake surface height, ocean depth; river segments with wiggle and the distance/width/direction field | TBD | Draft |
| 3 | Parallel and Incremental Bake | `IParallelExecutor` rows (serial oracle), `RebakeCells` for class changes, timing metrics, goldens on S-1 Small/Standard/Large | TBD | Draft |

Build order: 1 → 2 → 3.

## Decisions

| ID | Decision | Status | Binding |
|---|---|---|---|
| CFB-001 | The bake is its own Dia system, data-only, separate from DiaTerrainMap3D's rendering (user decision 2026-11-20) | Accepted | Yes |
| CFB-002 | Class borders come from a Gaussian majority over two rings plus a margin field, never nearest-cell ownership (was TMAP-007) | Accepted | Yes |
| CFB-003 | The bake is deterministic and hashable at any thread count | Accepted | Yes |

## Inherited Binding Decisions

| ID | Decision | How it constrains DiaCellFieldBake |
|---|---|---|
| PD-001 | StringCRC ids | Layer and relief-class ids |
| PD-004 / AD-002 | No STL in public APIs | Raw pointer + count outputs, `DynamicArrayC` internally |
| AD-001 | Module docs, sign-off | New module doc; new Dia system, approved 2026-11-20 |
| AD-003 | `Dia::<Module>::` | `Dia::CellFieldBake::` |
| DFS-004 | Worker-capped executor | Parallel bake uses `IParallelExecutor`, so it respects the worker cap |

## Test Strategy

- **GoogleTests on hand-built grids (2 to 3 biomes, a lake, a ridge, a river):**
  - the margin is zero on the border;
  - no texel takes the class of a cell outside its two rings;
  - lakes sit at surface height;
  - relief is added only on weighted classes;
  - the river distance is negative inside the channel.
- **Determinism:** the serial and parallel bakes give identical hashes, and the same hash on every run.
- **Goldens:** S-1 Small, Standard and Large presets × seeds 1–3, through the existing golden store pattern.
- **Performance (Debug, ballpark):** bake time at each size. The prototype JS took 1.2 s at Small.

## Open Design Questions

1. **Texel resolution per size.** 14 texels per unit at Large is about 2,200 × 1,700 texels. Should resolution stay fixed or scale per map size? Should consumers be allowed to ask for a coarser bake (for example the 2D debug renderer)?
2. **Does the 2D CellFieldRenderer adopt it?** Doing so would give S-1/S-2.5/S-3 debug views smooth borders, at the cost of reworking a built renderer.

## Status

`Approved` (2026-11-20, user: split out of DiaTerrainMap3D in the 3-way split). Specced now, built with CoW build order #18.
