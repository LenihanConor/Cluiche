# Feature Spec: DiaCellFieldBake Parallel, Tiled and Incremental Bake

## Parent System
@docs/specs/applications/dia/systems/diacellfieldbake/diacellfieldbake.md

**Gameplay Domains:** rendering
**Depends on:** @docs/specs/applications/dia/systems/diacellfieldbake/fields-and-classes.md (Feature 1, `BakeRegion`), @docs/specs/applications/dia/systems/diacellfieldbake/relief-and-rivers.md (Feature 2)
**Uses:** `IParallelExecutor` (@docs/specs/applications/dia/systems/diadeferredstep/diadeferredstep.md)

## Status
`Draft` (2026-11-20). Drafted from the Feature 1 amendment (tiled bake) and the performance discussion; awaiting user review.

## Summary

Features 1 and 2 bake any texel rectangle correctly. Feature 3 decides **how a real game uses that**:

1. **Parallel.** Split the work into tiles and run them on `IParallelExecutor`. A serial executor is the oracle.
2. **Tiled and cached.** A `TileCache` bakes only the tiles the camera needs, keeps them within a byte budget, and evicts the least recently used.
3. **Resolution policy.** A default texels-per-unit per map size, derived from a memory budget, so Large fits.
4. **Incremental.** When a few cells change class (A3 succession), rebake only the tiles they touch.
5. **Measured.** Timing and memory metrics, and goldens on S-1 Small, Standard and Large.

## Problem

- A whole-world bake at Large is about 58 MB and about a second on one thread. Fine once, wasteful if redone on every succession change or held for off-screen land.
- A succession event changes a few cells. Rebaking everything for that would stall the render side.
- Which resolution is right depends on the map size and the memory budget, and nothing picks it today.

## Goals

1. **Parallel bake over tiles** that is byte-identical to the serial bake at any worker count.
2. **A tile cache with a byte budget**, so memory is bounded however far the camera roams.
3. **Visible-tile bake**: given a texel rectangle (the camera footprint), ensure exactly its tiles are present.
4. **Incremental rebake** of only the tiles touched by changed cells, byte-identical to a full rebake.
5. **A resolution policy** that returns texels per unit for a grid and a budget.
6. **Goldens and metrics** for S-1 Small, Standard and Large.

## Non-Goals

- **GPU upload and texture streaming.** DiaTerrainMap3D consumes tiles and decides how to upload them.
- **Choosing the camera footprint.** The consumer passes the rectangle.
- **Background priority and scheduling policy.** One executor call at a time, driven by the consumer.
- **A GPU bake.** The CPU bake stays the exact reference (see parent system).

## Design

### Tiles

- `kDefaultTileSize = 256` texels, configurable. Tile `(tx, ty)` covers `[tx·S, (tx+1)·S) × [ty·S, (ty+1)·S)`, clipped at the texel grid edge (ragged edge tiles).
- A tile is one `BakedFields` from `BakeRegion`, plus its key and a last-used stamp.

### Parallel bake

```cpp
// Bakes every tile in `rect` (or all tiles) in parallel; tiles are the unit of parallelism.
bool BakeTiles(const BakeInputs&, const ITexelOwner&, const BakeParams&, uint32_t tileSize,
               TexelRect rect, IParallelExecutor&, TileSink& sink);
```

`TileSink::Accept(tileKey, BakedFields&&)` is called once per tile. The call order is not defined; sinks must be order independent. A `StitchSink` assembles a whole-world `BakedFields` for tests and small worlds.

### Tile cache

```cpp
class TileCache {
public:
    TileCache(const BakeInputs&, const ITexelOwner&, const BakeParams&, uint32_t tileSize, size_t byteBudget);
    // Makes every tile overlapping `rect` resident (baking missing ones in parallel); marks them used.
    void EnsureResident(TexelRect rect, IParallelExecutor&);
    const BakedFields* Find(TileKey) const;           // null if not resident
    uint32_t ResidentCount() const;  size_t ResidentBytes() const;
    void Invalidate(TexelRect rect);                   // drop tiles overlapping rect
    void InvalidateAll();
};
```

- **Budget.** After each `EnsureResident`, evict least-recently-used tiles that are *not* in the current `rect` until `ResidentBytes() <= byteBudget`. Tiles in the current rect are never evicted. If the rect alone exceeds the budget, they stay and a warning is logged on channel `CFB`.
- **Thread rule.** `EnsureResident` runs on the caller's thread (it may use the executor's workers inside). The cache is not thread-safe; one owner (the render module).

### Resolution policy

```cpp
struct ResolutionPolicy {
    float tMin = 4.0f, tMax = 14.0f;
    size_t budgetBytes = 64u << 20;     // bytes for a whole-world bake
};
float ChooseTexelsPerUnit(float extentX, float extentY, uint32_t scalarCount, const ResolutionPolicy&);
```

`T = clamp(sqrt(budgetBytes / (bytesPerTexel × extentX × extentY)), tMin, tMax)`, where `bytesPerTexel = 2·scalarCount + 4 + 3 + 2` (+4 when rivers are present). Small, Standard and Large therefore get the default 14 until Large's whole-world bake would exceed the budget. A consumer using only visible tiles passes a larger budget.

### Incremental rebake

```cpp
// Cells whose class or scalar inputs changed. Returns the texel rect that may differ.
TexelRect AffectedRect(const ITexelOwner&, const BakeParams&, const uint16_t* cells, uint32_t count);
// Rebakes only tiles overlapping the affected rect.
void RebakeCells(TileCache&, const uint16_t* cells, uint32_t count, IParallelExecutor&);
```

Influence of one cell on a texel is limited to: the texel's neighbourhood is the cell plus two rings (≤ 2 × √3 ≈ 3.47 units between centres), the owner is within one cell of the texel's warped point, and the warp moves the point by at most `1.35 × warpAmplitude` (the noise range). `AffectedRect` therefore dilates the changed cells' centres by `2·√3 + 1 + 1.35·warpAmplitude + margin` (default 6 units, conservative). Rivers: a changed river cell also dilates by `MAXD`. The caller must not hold stale `BakedFields` pointers across a rebake.

## Acceptance Criteria

1. **Parallel equals serial.** `BakeTiles` with the serial executor and with a multi-worker executor (1, 2, 4, 8 workers) stitch to the same bytes and the same `Hash()` for S-1 Small, Standard and Large.
2. **Tile sizes.** Tile sizes 64, 256, 1000 and one that does not divide the texel grid all stitch to the same bytes. Ragged edge tiles have the clipped size.
3. **Sink order independence.** Delivering tiles in reverse order to a stitch sink gives the same result.
4. **Cache residency.** After `EnsureResident(rect)`, exactly the tiles overlapping `rect` are resident, plus any earlier tiles within the budget.
5. **Budget and LRU.** With a budget of N tiles, after touching tiles in a known order, residency matches LRU. Tiles in the current rect are never evicted. A rect larger than the budget stays resident and logs a warning.
6. **Cache equals whole bake.** Every resident tile is byte-identical to the same region of a whole bake.
7. **Invalidate.** `Invalidate(rect)` drops only the overlapping tiles; the next `EnsureResident` rebakes them.
8. **Resolution policy.** `ChooseTexelsPerUnit` is clamped to [tMin, tMax], decreases as the grid grows, increases with the budget, and the resulting whole-world `ByteSize()` is within the budget unless clamped at `tMin`. Small gets `tMax` at the default budget; Large at 64 MB gets `tMax`.
9. **Affected rect.** For a single changed cell, `AffectedRect` contains every texel whose baked output differs between the old and new inputs, checked by baking both worlds at several seeds and warp amplitudes on hand grids.
10. **Incremental equals full.** After changing K random cells (class and scalar), `RebakeCells` leaves the cache byte-identical to a fresh `EnsureResident` over the same rect on the new inputs, for K = 1, 5 and 50.
11. **Incremental does less.** Changing one cell rebakes at most the tiles that overlap its `AffectedRect`, asserted by a counter, and far fewer than all tiles on Large.
12. **Metrics.** `cfb.tiles_baked`, `cfb.tiles_evicted`, `cfb.resident_bytes`, `cfb.tile_bake_ms` (last), `cfb.rebake_ms`; a log line per `EnsureResident` that baked anything.
13. **Goldens.** S-1 Small, Standard and Large presets (continental, highlands, archipelago, default) × seeds 1–3, whole-world hash under the policy resolution, stored in a golden file with an update switch like `CellLayerGolden`.
14. **Timing (Debug, ballpark).** Whole-world bake time at each size is logged and recorded in the plan notes. No threshold gate (quick-and-dirty perf rule); a problem is queued as a perf task, not silently accepted.

## Binding Decisions

| ID | Decision | How this feature complies |
|---|---|---|
| CFB-003 | Deterministic at any thread count | AC 1, 2, 3, 6, 10 |
| DFS-004 | Worker-capped executor | Uses `IParallelExecutor`, so the cap applies |
| PD-004 / AD-002 | No STL in public APIs | Tile table in `DynamicArrayC`/`HashTable` |
| PD-002 | PU rule | The cache has one owner thread; it does not read sim state (inputs are plain arrays) |
| Feedback: quick and dirty perf | Ballpark perf only | AC 14 records, does not gate |

## Open Design Questions

1. **Tile size.** 256 is a guess. Smaller tiles cut over-baking at the camera edge and give finer incremental rebakes, but add per-tile overhead. Measure during implementation and fix the default from the numbers.
2. **Rebake while drawing.** `RebakeCells` replaces tile contents. The renderer might still be reading the old tile. Either the cache double-buffers (a stale tile stays valid until `Commit`), or rebake runs only between frames. Decide with DiaTerrainMap3D's module spec; the cache API can add a two-phase `RebakeBegin`/`Commit` without breaking callers.
3. **Is whole-world ever needed?** If DiaTerrainMap3D always uses the visible-tile path, the whole-world `Bake` and the resolution policy become test and tooling utilities.

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Tile keys, rect clipping, `TileSink`, `StitchSink` | AC 2, 3 | Not Started | sonnet | |
| 2 | `BakeTiles` over `IParallelExecutor` | AC 1 | Not Started | sonnet | serial oracle |
| 3 | `TileCache`: residency, LRU, budget, invalidate | AC 4–7 | Not Started | sonnet | |
| 4 | `ResolutionPolicy` / `ChooseTexelsPerUnit` | AC 8 | Not Started | haiku | |
| 5 | `AffectedRect` + dilation rule | AC 9 | Not Started | sonnet | the risky part: tested by brute force |
| 6 | `RebakeCells` | AC 10, 11 | Not Started | sonnet | |
| 7 | Metrics and logs | AC 12 | Not Started | haiku | |
| 8 | S-1 goldens (Small, Standard, Large) and timing notes | AC 13, 14 | Not Started | sonnet | |
