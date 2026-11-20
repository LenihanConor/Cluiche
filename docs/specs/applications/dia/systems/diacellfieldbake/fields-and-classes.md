# Feature Spec: DiaCellFieldBake Fields and Classes

## Parent System
@docs/specs/applications/dia/systems/diacellfieldbake/diacellfieldbake.md

**Gameplay Domains:** rendering
**Research:** docs/research/tile_renderin_art_pipeline/summary.md
**Prototype:** `docs/research/tile_renderin_art_pipeline/lookdev/js/world.js` (`World.buildFields`, `pointToCell`, `warp`, `cellAtWarped`)

## Status
`Approved` (2026-11-20, user). Interview answers: inputs through DiaCellLayers views, hex flat-top odd-q with a generic seam, two generic class partitions, uint16 owner. **Amended before approval (user, 2026-11-20):** tiled bake (`BakeRegion`) and packed output (half-float scalars, 8-bit fractions), so memory at Large stays near 60 MB and a renderer can bake only the visible tiles.

## Summary

Feature 1 is the heart of DiaCellFieldBake. It turns per-cell layers on a hex grid into continuous per-texel fields, so a renderer no longer shows the grid.

For every texel it produces:
1. **Smooth scalars.** A Gaussian-weighted average over the owning cell and two rings of neighbours (19 cells), sampled at a noise-warped point.
2. **A primary class.** For example land, ocean or lake, decided by weight.
3. **A secondary class within it.** For example the biome, decided by weight among cells of the winning primary class.
4. **An owner cell.** The highest-weighted cell of the winning classes, for CPU queries.
5. **A margin.** Zero on a secondary-class border and 255 deep inside a region, so renderers can ink borders at a constant pixel width.

The output is **compact** plain arrays (no GPU, no STL in the API: half-float scalars, 8-bit fractions and classes, about 17 bytes per texel with four scalars) with a `Hash()` for goldens. It can be baked **whole or one tile at a time**, because every texel depends only on its own position. It adds two queries, `OwnerAt` and `SampleAt`, so CPU code (prop placement, particle spawning) agrees with what is drawn.

Relief, lake surface heights and the river distance field are Feature 2. Parallel and incremental bake are Feature 3. Feature 1 delivers the per-region entry point that Feature 3 parallelises and streams.

## Problem

- A renderer that draws one value per cell shows hex staircases, hard class edges and flat plateaus. The research prototype proved that the fix is data processing, not art: a two-ring weighted majority, a margin field and smoothed scalars.
- That processing currently exists only as browser JavaScript (`world.js`). The engine needs a deterministic, testable C++ version that DiaTerrainMap3D, and optionally the 2D cellfield renderer, can share.

## Goals

1. **Continuous fields from cell layers.** Texel resolution is a parameter (default 14 per world unit).
2. **Borders that ignore the hex outline.** Class edges come from weights plus a noise warp, never from nearest-cell ownership (CFB-002).
3. **Generic classes.** Two partitions (primary, secondary), each a `uint8_t` id per cell. The bake knows nothing about biomes or water.
4. **Deterministic, hashable output** (CFB-003). The same inputs give identical bytes, independent of how the texel grid is split into tiles.
5. **Compact and tileable.** Output is packed to about 17 bytes per texel. A caller can bake any texel rectangle, so a renderer can bake only the visible tiles instead of the whole world.
6. **Inputs through DiaCellLayers.** The adapter reads layers from the registry by id.
7. **CPU queries that match the drawn result.** `OwnerAt` and `SampleAt`.

## Non-Goals

1. **Relief, lake surface height, ocean depth and rivers.** Feature 2.
2. **Parallel execution, tile scheduling/caching and incremental rebake.** Feature 3, which wraps this feature's `BakeRegion`.
3. **GPU upload, textures, shaders.**
4. **Topologies other than hex flat-top odd-q** in v1. The seam (`ITexelOwner`) lets another topology be added without an API change.
5. **Bit-identical output across compilers or machines.** The bake uses `exp` and `floor`, so goldens are per platform and build (x64 MSVC). Determinism is across thread counts and runs on the same build.
6. **Meaning of classes or layers.** Callers name them.

## Design

### Units and texel grid

- Work in **cell units**: coordinates divided by `CellGridDesc::cellSize`, so the hex edge length is 1.
- The texel grid covers the grid's bounds, supplied by the topology seam: `extentX × extentY` cell units.
  - For flat-top odd-q hex: `extentX = (width − 1) × 1.5 + 2`, `extentY = height × √3 + √3/2`.
- `texelsPerUnit` (T, default 14). `TW = ceil(extentX × T)`, `TH = ceil(extentY × T)`.
- Texel (i, j) samples the point `((i + 0.5) / T, (j + 0.5) / T)`. A texel's value depends only on (i, j), the inputs and the params, never on which region it was baked in.
- **Resolution at Large** (104 × 64): T = 14 gives about 2,190 × 1,560 texels (3.4 M) and about 58 MB packed. A coarser T, or baking only visible tiles, cuts that further. Choosing T per size is Feature 3's and DiaTerrainMap3D's decision.

### Per-texel algorithm

Taken from `World.buildFields`:

1. **Warp.** `w = p + amp × noise(p)`, where `noise` is two fBm channels plus a finer value-noise octave (`warpAmplitude`, default 0.5 cell units). The noise is an integer-hash value noise with no table, so it is deterministic.
2. **Owning cell.** `c = Owner(w)` through the `ITexelOwner` seam. A point outside the grid gives `kNoOwner`, and the texel takes the *outside values* (below).
3. **Neighbourhood.** The cell plus ring 1 plus ring 2, deduplicated (19 cells in the interior). The set is precomputed once per cell from `CellGridDesc::neighbours`.
4. **Weights.** `wᵢ = exp(−|centreᵢ − w|² / σ²)` with `σ² = 0.7` cell units (`sigma2`).
5. **Smooth scalars.** `scalar = Σ wᵢ·layerᵢ / Σ wᵢ`, for each requested scalar layer.
6. **Primary class.** Per primary class, `fraction = Σ w over cells of that class / Σ w`. Then:
   - `BakeParams::primaryMajorityClass` (default 0) wins only if its fraction is **≥ 0.5**.
   - Otherwise the largest fraction among the *other* classes wins, with ties going to the lower id.
   - For land/ocean/lake this makes the coast sit exactly at land fraction 0.5, as in the prototype.
7. **Secondary class.** Sum weights per secondary id over ring cells whose primary class equals the winner. The best id wins, with ties going to the lower id. `margin = clamp(2.5 × (best − second) / (best + second + 1e−6), 0, 1)`, quantised to `uint8_t`. If there is no second class, `second = 0`.
8. **Owner cell.** The ring cell with the highest weight among cells whose primary and secondary classes equal the winners. Fallback: `c`.
9. **Fractions.** Per primary class, the fraction is stored as an 8-bit field (0..255 = 0..1) so renderers can draw coast ink at fraction 0.5 (value 128).

**Outside values.** A texel with no owner takes `outsideScalars[]` (default 0), the primary class `outsidePrimary` (default 1, the water class) and secondary 0. It has owner `kNoOwner` and margin 255.

### Data

```cpp
namespace Dia::CellFieldBake {
    static constexpr uint16_t kNoOwner = 0xFFFF;
    static constexpr uint32_t kMaxPrimary = 4;     // primary fractions emitted
    static constexpr uint32_t kMaxSecondary = 32;  // secondary ids 0..31
    static constexpr uint32_t kMaxTexels = 1u << 24;

    struct BakeParams {
        float    texelsPerUnit = 14.0f;
        float    sigma2 = 0.7f;
        float    warpAmplitude = 0.5f;
        uint8_t  primaryMajorityClass = 0;
        uint8_t  outsidePrimary = 1;
        uint32_t seed = 0;                 // offsets the warp noise
    };

    // Plain per-cell arrays, each width*height entries; borrowed, not owned.
    struct BakeInputs {
        const Dia::Graphics::CellGridDesc* grid = nullptr;
        const uint8_t*  primaryClass = nullptr;       // 0..kMaxPrimary-1
        const uint8_t*  secondaryClass = nullptr;     // 0..kMaxSecondary-1
        const float* const* scalarLayers = nullptr;   // scalarCount arrays
        const float*    outsideScalars = nullptr;     // scalarCount values, or null for 0
        uint32_t        scalarCount = 0;
    };

    // Adapter: pulls layers out of DiaCellLayers by id into caller-provided scratch (see ODQ 2).
    struct RegistryLayerIds {
        Dia::Core::StringCRC primaryClass, secondaryClass;
        const Dia::Core::StringCRC* scalars; uint32_t scalarCount;
    };
    enum class InputResult { Ok, MissingLayer, GridMismatch, TooManyClasses, NullArg };
    InputResult GatherFromRegistry(const CellLayers::CellLayerRegistry&, const RegistryLayerIds&,
                                   InputScratch& scratch, BakeInputs& out, Dia::Core::StringCRC* failedLayer);

    // Topology seam.
    class ITexelOwner {
    public:
        virtual ~ITexelOwner() {}
        virtual void     Bounds(float& extentX, float& extentY) const = 0;       // cell units
        virtual uint16_t OwnerOf(float x, float y) const = 0;                    // kNoOwner outside
        virtual void     CellCentre(uint16_t cell, float& x, float& y) const = 0;
    };
    class HexFlatTopOddQOwner : public ITexelOwner { /* closed-form cube rounding */ };

    // A rectangle of texels in the global texel grid. A full bake is {0, 0, TW, TH}.
    struct TexelRect { uint32_t x = 0, y = 0, w = 0, h = 0; };

    struct TexelGrid { uint32_t width = 0, height = 0; };            // TW x TH for given owner + params
    bool ComputeTexelGrid(const ITexelOwner&, const BakeParams&, TexelGrid& out);

    // Packed output for one rectangle, row-major w x h. Owns its memory.
    struct BakedFields {
        TexelRect rect;                         // which global texels these are
        uint32_t  scalarCount = 0;
        uint16_t* scalars = nullptr;            // IEEE half, scalarCount per texel, interleaved
        uint8_t*  primaryFraction = nullptr;    // kMaxPrimary per texel, 0..255 = 0..1
        uint8_t*  primary = nullptr;
        uint8_t*  secondary = nullptr;
        uint8_t*  margin = nullptr;
        uint16_t* owner = nullptr;
        uint64_t  Hash() const;                 // over every array in a fixed order, plus rect
        size_t    ByteSize() const;
        ~BakedFields();                         // frees
    };

    float HalfToFloat(uint16_t);  uint16_t FloatToHalf(float);        // exposed for consumers and tests

    // Bakes exactly `rect` of the global texel grid into `out` (allocated by the call). rect must lie inside the texel grid.
    bool BakeRegion(const BakeInputs&, const ITexelOwner&, const BakeParams&, TexelRect rect, BakedFields& out);
    // Whole world: ComputeTexelGrid, then BakeRegion over the full rect.
    bool Bake(const BakeInputs&, const ITexelOwner&, const BakeParams&, BakedFields& out);

    // Queries return false when (x, y) is outside the baked rect.
    bool OwnerAt(const BakedFields&, const BakeParams&, float x, float y, uint16_t& out);                    // cell units
    bool SampleAt(const BakedFields&, const BakeParams&, uint32_t scalarIndex, float x, float y, float& out); // bilinear
}
```

`BakeRegion` writes only its own output and reads only shared immutable inputs, so any split of the texel grid into rectangles, baked in any order or on any thread, gives the same bytes once stitched. That makes Feature 3's parallelism and tile streaming trivial and safe.

## Acceptance Criteria

1. **Sizes.** `ComputeTexelGrid` gives `TW = ceil(extentX × T)` and `TH = ceil(extentY × T)`. `Bake` and `BakeRegion` allocate exactly the rect's texels. A region over `kMaxTexels`, or outside the texel grid, is rejected (returns false, logs an error, allocates nothing).
2. **Packed size.** `ByteSize()` is at most `17 × texels` for 4 scalars (`2·4 + 4 + 1 + 1 + 1 + 2` bytes), so a Large world at the default resolution is under 60 MB.
3. **Constant layer.** A scalar layer with one value gives that value (within half precision, relative 1e−3) at every interior texel, and `outsideScalars` at every outside texel. `FloatToHalf`/`HalfToFloat` round-trip any value in [−2, 2] to within 1e−3.
4. **Smooth scalar.** A layer that is a linear ramp across cell columns gives a field that is monotonic along a texel row. It has no step wider than one texel's worth of change at cell boundaries (no staircase).
5. **Primary class rule.** With classes land(0), ocean(1), lake(2):
   - a texel with land fraction ≥ 0.5 is land;
   - below 0.5, the larger of ocean and lake wins, with ties going to ocean;
   - the 8-bit fraction fields sum to 255 ± 3 over interior texels.
6. **Secondary within primary.** A texel's secondary class always equals the class of at least one ring cell whose primary class equals the texel's primary class. Water cells never contribute their secondary class to a land texel.
7. **Margin.**
   - Deep inside a uniform secondary region (more than 2 cells from any border), margin is 255.
   - On a texel at the midpoint between two cells with different secondary classes, margin is < 40.
   - Margin is monotonic non-decreasing moving from the border into either region, along the line between the two cells.
8. **Owner.**
   - The owner is always within two rings of the texel's warped cell.
   - It has the texel's primary and secondary classes whenever such a cell exists in the ring.
   - Outside the grid the owner is `kNoOwner`.
9. **Borders ignore the hex outline.** On a grid with a straight two-class border aligned to hex edges, with `warpAmplitude = 0.5`, at least 30% of border texels (margin < 40) lie more than 0.1 cell units from the nearest hex edge. With `warpAmplitude = 0` and `sigma2 = 0.01` (≈ nearest-cell), at least 95% lie within 0.05 of a hex edge.
10. **Determinism and tiling.**
   - Two bakes of the same inputs give equal `Hash()`.
   - Baking tiles (for example 256 × 256, including ragged edge tiles) in any order and stitching them gives output byte-identical to the whole bake.
   - A single tile baked alone is byte-identical to the same region of the whole bake.
   - Changing `seed` changes the hash.
11. **Queries.** `OwnerAt(x, y)` equals the stored owner of the texel containing (x, y). `SampleAt` at a texel centre equals the stored scalar, and at a texel corner is the mean of the four neighbours (bilinear). Both return false outside the baked rect.
12. **Registry adapter.**
    - `GatherFromRegistry` reads the named layers through `CellLayerDesc::read` into scratch arrays.
    - A missing id returns `MissingLayer` and writes the failed id.
    - Layers on different grids return `GridMismatch`.
    - A class id ≥ the caps returns `TooManyClasses`.
    - A null argument returns `NullArg`.
    - It performs the reads on the calling thread.
13. **Topology seam.** A test `ITexelOwner` (a square grid) bakes without any hex code in the path. Only `HexFlatTopOddQOwner` is hex-specific.
14. **Parity with the prototype.** On the S-1 continental seed 1 Small world, the C++ bake's primary-class and margin fields match the prototype's visually. The user signs off a side-by-side capture. The result then becomes the golden.
15. **Observability.** A log on channel `CFB`: an Info line per bake with size and milliseconds, an Error for every rejection. A `DIA_PROFILE_SCOPE` around `Bake` and `BakeRegion`. Metrics `cfb.bake_ms` (last bake) and `cfb.bake_bytes` (last output size).

## Binding Decisions

| ID | Decision | How this feature complies |
|---|---|---|
| PD-001 | StringCRC ids | Registry layer ids are `StringCRC`. Class ids are small integers (`uint8_t`), not strings |
| PD-004 / AD-002 | No STL in public APIs | Raw pointers and counts; `BakedFields` owns raw arrays. Internals use `DynamicArrayC` or fixed arrays |
| AD-001 | Module docs; sign-off | New module `DiaCellFieldBake` documented in a `dia.cellfieldbake.architecture.module.md`; new Dia system already approved 2026-11-20 |
| AD-003 | `Dia::<Module>::` | `Dia::CellFieldBake::` |
| CFB-002 | Majority + margin, never nearest-cell borders | Steps 3–7 above; AC 9 tests it |
| CFB-003 | Deterministic, hashable at any thread count | `BakeRegion` reads shared immutable inputs and each texel depends only on its own position; AC 10 tests it |
| PD-002 | PU boundaries are streams | **Constraint, no conflict.** `CellLayerDesc::read` is owner-PU only (registry wrong-PU rule). `GatherFromRegistry` therefore runs on the layer's PU. The bake itself touches only the arrays, so a render-side module can run it after receiving them by FrameStream snapshot (ODQ 2) |

## Open Design Questions

1. **Hex seam: closed form or generic?** The prototype uses closed-form cube rounding, so `HexFlatTopOddQOwner` does too. The seam allows a generic nearest-centre owner later. Revisit when a second topology exists.
2. **Where does the gather run?** The registry's `read` delegates are owner-PU only, which here means the sim side. The bake output is wanted on the render side. Options:
   - Gather and bake on the sim PU, then publish `BakedFields` as a stream snapshot. It is large (about 2,200 × 1,700 texels × ~30 bytes ≈ 110 MB at Large, so this is the wrong choice).
   - Gather into plain arrays on the sim PU, stream the small per-cell arrays (about 6,656 cells), and bake on the render side. This is the recommended path, and `BakeInputs` is exactly that shape.
   - Decide at the DiaTerrainMap3D module feature, which owns the PU wiring.
3. **Resolution and tile policy at Large.** Moved to Feature 3 and the DiaTerrainMap3D module spec: default T per world size, tile size, visible-tile selection and cache eviction. This feature only guarantees that any texel rectangle can be baked and costs at most 17 bytes per texel.

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Module skeleton: `Dia/DiaCellFieldBake/` project, module doc, vcxproj + filters, `dia check deps` | `dia check deps` clean | Not Started | haiku | `dia scaffold module` |
| 2 | Value noise and warp (integer-hash, deterministic), unit tests | noise range, repeatability, seed changes output | Not Started | sonnet | |
| 3 | `ITexelOwner` + `HexFlatTopOddQOwner` (bounds, closed-form owner, centres) | owner of every cell centre is that cell; outside = `kNoOwner` | Not Started | sonnet | |
| 4 | Ring table (19-cell neighbourhoods, dedup, edges) | counts: interior 19, corner/edge smaller; no duplicates | Not Started | sonnet | |
| 5 | Smooth scalars (weights, average) and outside values | AC 3, 4 | Not Started | sonnet | |
| 6 | Primary class + fractions | AC 5 | Not Started | sonnet | |
| 7 | Secondary class, margin, owner | AC 6, 7, 8 | Not Started | sonnet | |
| 8 | `ComputeTexelGrid`, `BakeRegion`/`Bake`, packed allocation, texel cap, `Hash()`, half-float helpers | AC 1, 2, 3, 10 | Not Started | sonnet | tile stitching test |
| 9 | `OwnerAt` / `SampleAt` | AC 11 | Not Started | sonnet | |
| 10 | Hex-border test and square-grid seam test | AC 9, 13 | Not Started | sonnet | |
| 11 | `GatherFromRegistry` adapter | AC 12 | Not Started | sonnet | needs DiaCellLayers dependency in the vcxproj |
| 12 | Observability: log channel, scope, metric | AC 15 | Not Started | haiku | |
| 13 | S-1 golden + prototype parity capture | AC 14 | Not Started | sonnet | user signs off the side-by-side |
