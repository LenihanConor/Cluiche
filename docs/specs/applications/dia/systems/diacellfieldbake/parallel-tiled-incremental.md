# Feature Spec: DiaCellFieldBake Parallel, Tiled and Incremental Bake

## Parent System
@docs/specs/applications/dia/systems/diacellfieldbake/diacellfieldbake.md

**Gameplay Domains:** rendering
**Depends on:** @docs/specs/applications/dia/systems/diacellfieldbake/fields-and-classes.md (Feature 1, `BakeRegion`)
**Uses:** `IParallelExecutor` from the new `DiaParallelExecutor` module (namespace `Dia::ParallelExecutor::`; extracted from DiaDeferredStep, see the backlog row "DiaParallelExecutor")

## Status
`Approved` (2026-11-24, user, after spec review: tests on synthetic grids not CoW presets, `RebakeAll` added, AC 6 arithmetic corrected). Drafted 2026-11-20. **Rewritten 2026-11-22 (review):** whole-world bake at load (the visible-tile LRU cache is cut), progress callback for a load overlay, double-buffered incremental rebake, executor moved to DiaParallelExecutor. Awaiting user review. **Amended 2026-11-23 (layer split, user):** Feature 2 (relief and rivers) moved to Draoi (DraoiHexWorldRelief). This feature gains a generic tile post-pass hook (`ITilePostPass`) so Draoi relief and rivers still run in the parallel and incremental bake; river terms in the resolution policy and the dilation rule become the hook's `ExtraBytesPerTexel()` and `InfluenceRadius()`.

## Summary

Feature 1 bakes any texel rectangle correctly. Feature 3 decides **how a real game uses that**:

1. **Parallel bake of the whole world at load.** Split into tiles, run them on `IParallelExecutor`, write into one world-sized `BakedFields`. A serial executor is the oracle. A progress callback drives the load overlay ("Baking 40 of 90 tiles").
2. **Resolution policy.** A default texels-per-unit per map size, derived from a memory budget, so Large fits.
3. **Incremental rebake, double-buffered.** When a few cells change class (A3 succession) or an edit lands, rebake only the tiles they touch into a pending set, then swap them in at a frame boundary. The old data keeps drawing until the swap.
4. **Tile post-pass hook.** A consumer registers post-passes that run on each tile after the bake, inside the same parallel and incremental paths (DraoiHexWorldRelief uses it for relief and rivers).
5. **Measured.** Timing and memory metrics, and goldens on S-1 Small, Standard and Large.

## Problem

- A whole-world bake at Large is about 58 MB and about a second or more on one thread. It must not block the render thread, and the player or tool needs to see that something is happening.
- A succession event changes a few cells. Rebaking everything for that would waste seconds. A tile rebake still takes tens of milliseconds, more than a frame, so it cannot run synchronously on the render thread.
- `IParallelExecutor::ParallelFor` blocks its caller until every range is done. Any bake call that uses it must run on a worker job, never the render thread.
- Which resolution is right depends on the map size and the memory budget, and nothing picks it today.

## Goals

1. **Parallel whole-world bake** that is byte-identical to the serial bake at any worker count.
2. **Progress reporting** for a load overlay: a callback with tiles done and tiles total, called from the baking threads in any order but with a monotonic done count.
3. **Incremental rebake** of only the tiles touched by changed cells, byte-identical to a full rebake.
4. **Double buffering.** The live world is never modified while a rebake runs. A `Commit` swaps the new tiles in and reports the changed texel rectangle for upload.
5. **A resolution policy** that returns texels per unit for a grid and a budget.
6. **Goldens and metrics** on synthetic hex grids of Small, Standard and Large size (a builder in `DiaCellFieldBake/Testing/`; no CoW dependency). Goldens on the real S-1 presets live with the CoW consumer.
7. **Post-pass hook** with influence radius, extra bytes per texel, and Begin/Commit/Discard callbacks so a post-pass's own arrays are double-buffered with the bake.
8. **Rebake everything.** `RebakeAll` for a change that touches no cell but invalidates every tile (a `BakeParams` field such as warp amplitude, or a post-pass parameter). It is double-buffered like `RebakeBegin`. Bake and post-pass parameters are therefore live-tweakable attributes: the consumer binds them, coalesces edits while a slider drags, and uses a coarser `texelsPerUnit` for the preview (ODQ 2).

## Non-Goals

- **A visible-tile cache with LRU eviction** (cut 2026-11-22). The whole world is baked at load; at Large it is about 58 MB. A cache is only needed for a world much larger than Large, and is a new feature if one ever exists.
- **GPU upload and texture streaming.** The consumer (DraoiHexWorldRender3D) takes the world and the changed rectangle, and decides how to upload.
- **Choosing when to bake or showing the overlay.** The consumer (the game's map module, CoW Strategic Map Feature 4) owns the bake state and the overlay.
- **What a post-pass computes.** Relief and rivers are DraoiHexWorldRelief. This feature only provides the progress callback.
- **A GPU bake.** The CPU bake stays the exact reference (see parent system).

## Design

### Tiles

- `kDefaultTileSize = 256` texels, configurable. Tile `(tx, ty)` covers `[tx·S, (tx+1)·S) × [ty·S, (ty+1)·S)`, clipped at the texel grid edge (ragged edge tiles).
- A tile is one `BakeRegion` call. Tiles are the unit of parallelism and of incremental rebake.

### Whole-world parallel bake

```cpp
using ProgressFn = std::function<void(uint32_t tilesDone, uint32_t tilesTotal)>;   // may be null

// Bakes every tile in parallel and stitches into `out` (one world-sized allocation).
// BLOCKS until finished (ParallelFor blocks): call from a worker job, not the render thread.
bool BakeWorld(const BakeInputs&, const ITexelOwner&, const BakeParams&, uint32_t tileSize,
               ParallelExecutor::IParallelExecutor&, const ProgressFn&, BakedFields& out);
```

`ProgressFn` is called once per finished tile, from whichever thread finished it. `tilesDone` is monotonic and the last call has `tilesDone == tilesTotal`. The caller must make its own state thread safe (an atomic counter feeding the bake state is enough). A `TileSink`/`StitchSink` pair exists for tests that deliver tiles in arbitrary order.

### Tile post-pass hook

```cpp
class ITilePostPass {
public:
    virtual ~ITilePostPass() = default;
    virtual float    InfluenceRadius() const = 0;     // cell units a changed cell reaches through this pass; dilates AffectedRect
    virtual uint32_t ExtraBytesPerTexel() const = 0;  // the pass's own per-texel arrays, for the resolution policy
    virtual void     Allocate(uint32_t texW, uint32_t texH) = 0;
    // Runs after the bake of `region`, on the same worker. Reads and may modify the tile's scalars; writes its own arrays.
    virtual void     Run(const TexelRect& region, const BakeParams&, BakedFields& tile) = 0;
    virtual void     RebakeBegin() = 0;               // start writing a pending copy
    virtual void     Commit(const TexelRect& changed) = 0;
    virtual void     Discard() = 0;
    virtual uint64_t Hash() const = 0;                // folded into the world hash
};
```

`BakeWorld` and `BakedWorld::Bake` take an optional array of post-passes, run in order per tile. A pass must be tile independent (any tile order gives the same bytes); AC 13 checks that for every registered pass. `WarpedPoint` (Feature 1) is available to passes so they sample at the same warped point as the class decision.

### Resolution policy

```cpp
struct ResolutionPolicy {
    float tMin = 4.0f, tMax = 14.0f;
    size_t budgetBytes = 80u << 20;     // bytes for a whole-world bake (80 MiB)
};
float ChooseTexelsPerUnit(float extentX, float extentY, uint32_t scalarCount, uint32_t extraBytesPerTexel, const ResolutionPolicy&);
```

`T = clamp(sqrt(budgetBytes / (bytesPerTexel × extentX × extentY)), tMin, tMax)`, where `bytesPerTexel = 2·scalarCount + 4 + 3 + 2 + extraBytesPerTexel` (the sum over registered post-passes; DraoiHexWorldRelief's river field adds 4). At Large (about 3.4 M texels at T = 14), 4 scalars plus a 4-byte post-pass is 21 B per texel, about 71 MB, which fits the default 80 MiB budget. Small, Standard and Large therefore get 14 at the default budget. A consumer with a tighter budget passes it in.

### Incremental rebake (double-buffered)

```cpp
class BakedWorld {
public:
    bool Bake(const BakeInputs&, const ITexelOwner&, const BakeParams&, uint32_t tileSize,
              ParallelExecutor::IParallelExecutor&, const ProgressFn&);     // = BakeWorld, owns the result
    const BakedFields& Fields() const;                                      // the live, drawable world

    // Cells whose class or scalar inputs changed. Rebakes only the tiles overlapping their AffectedRect
    // into a PENDING set; the live world is untouched. BLOCKS (worker job only).
    bool RebakeBegin(const BakeInputs& newInputs, const uint16_t* cells, uint32_t count, ParallelExecutor::IParallelExecutor&);
    // Every tile, into the pending set: for a changed BakeParams or post-pass parameter (no cell changed). Same
    // threading, merge and Commit rules as RebakeBegin; runs the progress callback if one was given to Bake.
    bool RebakeAll(const BakeInputs& inputs, const BakeParams& newParams, ParallelExecutor::IParallelExecutor&);
    bool HasPending() const;
    // Copies the pending tiles into the live world. Cheap (memcpy of the touched tiles). Returns the texel
    // rectangle that changed, for the consumer's upload. Render thread, at a frame boundary.
    TexelRect Commit();
    void DiscardPending();
};

TexelRect AffectedRect(const ITexelOwner&, const BakeParams&, const uint16_t* cells, uint32_t count);
```

Influence of one cell on a texel is limited to: the texel's neighbourhood is the cell plus two rings (≤ 2 × √3 ≈ 3.47 units between centres), the owner is within one cell of the texel's warped point, and the warp moves the point by at most `1.35 × warpAmplitude` (the noise range). `AffectedRect` therefore dilates the changed cells' centres by `2·√3 + 1 + 1.35·warpAmplitude + margin`, computed from the actual `warpAmplitude` (margin default 1 unit), plus the largest `InfluenceRadius()` of the registered post-passes.

**Threading.** One owner (the render module). `RebakeBegin` runs on a worker job and writes only the pending set. `Fields()` may be read by the render thread throughout. `Commit` and `DiscardPending` run on the render thread when no `RebakeBegin` is in flight (the owner tracks that), and forward to every post-pass. A second `RebakeBegin` while one is pending merges into the same pending set.

## Acceptance Criteria

1. **Parallel equals serial.** `BakeWorld` with the serial executor and with a multi-worker executor (1, 2, 4, 8 workers) gives the same bytes and `Hash()` for synthetic Small, Standard and Large sized hex grids (Testing builder).
2. **Tile sizes.** Tile sizes 64, 256, 1000 and one that does not divide the texel grid all give the same bytes. Ragged edge tiles have the clipped size.
3. **Sink order independence.** Delivering tiles to a stitch sink in reverse order gives the same result.
4. **Progress.** `ProgressFn` is called exactly `tilesTotal` times, `tilesDone` never decreases, the last call has `tilesDone == tilesTotal`, and a null callback is accepted.
5. **Whole bake match.** `BakedWorld::Fields()` is byte-identical to a single `Bake` of the same inputs.
6. **Resolution policy.** `ChooseTexelsPerUnit` is clamped to [tMin, tMax], decreases as the grid grows, increases with the budget, and the resulting whole-world `ByteSize()` is within the budget unless clamped at `tMin`. At the default budget, Small, Standard and Large with 5 scalars (4 fields plus DraoiHexWorldRelief's weight layer) and 4 extra bytes per texel all get `tMax` (23 bytes per texel, about 74.6 MiB at Large).
7. **Affected rect.** For a single changed cell, `AffectedRect` contains every texel whose baked output differs between the old and new inputs, checked by baking both worlds at several seeds and warp amplitudes (including 0 and 1.5) on hand grids.
8. **Incremental equals full.** After changing K random cells (class and scalar), `RebakeBegin` + `Commit` leaves `Fields()` byte-identical to a fresh `Bake` on the new inputs, for K = 1, 5 and 50.
9. **Incremental does less.** Changing one cell rebakes at most the tiles overlapping its `AffectedRect`, asserted by a counter, and far fewer than all tiles on Large.
10. **Double buffering.** Between `RebakeBegin` and `Commit`, `Fields()` is byte-identical to before. `Commit` returns the changed rectangle, and a texel outside it is unchanged. `DiscardPending` leaves the live world untouched. A second `RebakeBegin` before `Commit` merges.
11. **Metrics.** `cfb.tiles_baked`, `cfb.resident_bytes`, `cfb.tile_bake_ms` (last), `cfb.rebake_ms`, `cfb.pending_tiles`; a log line per `BakeWorld` and per `RebakeBegin`.
12. **Goldens.** Synthetic Small, Standard and Large sized hex grids × seeds 1–3, whole-world hash under the policy resolution, stored in a golden file with an update switch like `CellLayerGolden`. Goldens on the real S-1 presets (continental, highlands, archipelago, default) belong to the CoW consumer (Strategic Map), not this Dia module.
13. **Post-pass hook.** A test post-pass (writes a per-texel hash of its warped point into its own array and adds a constant to one scalar) gives byte-identical results serial versus parallel, in any tile order, and after incremental rebake versus full bake. Its `InfluenceRadius()` grows `AffectedRect`. Its array is unchanged between `RebakeBegin` and `Commit`, and `Discard` leaves it untouched. No registered pass leaves the output byte-identical to Feature 1 alone.
14. **Timing (Debug, ballpark).** Whole-world bake time at each size, single worker and with the capped executor, is logged and recorded in the plan notes. No threshold gate (quick-and-dirty perf rule); a problem is queued as a perf task, not silently accepted.
15. **Rebake all.** After changing `warpAmplitude` (and separately a test post-pass parameter), `RebakeAll` + `Commit` leaves `Fields()` byte-identical to a fresh `Bake` with the new parameters. Between `RebakeAll` and `Commit`, `Fields()` is unchanged; `DiscardPending` drops it; a `RebakeBegin` issued while it is pending merges into the same pending set.

## Binding Decisions

| ID | Decision | How this feature complies |
|---|---|---|
| CFB-003 | Deterministic at any thread count | AC 1, 2, 3, 5, 8 |
| CFB-004 | Whole-world bake at load; incremental rebake is double-buffered (2026-11-22) | `BakeWorld`, `BakedWorld`, AC 10 |
| CFB-006 | No terrain vocabulary in the bake (2026-11-23) | Rivers are gone from this spec; consumers extend through `ITilePostPass` |
| DFS-004 | Worker-capped executor | The executor passed in decides the cap; the capped implementation lives in DiaDeferredStep |
| PD-004 / AD-002 | No STL in public APIs | Tile table in `DynamicArrayC`. `ProgressFn` uses `std::function`, matching the existing `RangeFn` in `IParallelExecutor` (pre-existing exception) |
| PD-002 | PU rule | One owner thread; the bake reads plain arrays, never sim state |
| Feedback: quick and dirty perf | Ballpark perf only | AC 13 records, does not gate |

## Open Design Questions

1. **Tile size.** 256 is a guess. Smaller tiles make incremental rebakes finer but add per-tile overhead. Measure during implementation and fix the default from the numbers.
2. **Low-resolution preview for the in-game editor.** While a slider drags, the tool wants a fast coarse bake. That is `BakeWorld` with a smaller `texelsPerUnit`, so no new API here, but the consumer must be able to swap in a world at a different resolution (DraoiHexWorldRender3D Feature 1 recreates its textures; CoW Strategic Map Feature 4 decides when).

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Tile keys, rect clipping, `TileSink`, `StitchSink` | AC 2, 3 | Not Started | sonnet | |
| 2 | `BakeWorld` over `IParallelExecutor` with `ProgressFn` | AC 1, 4 | Not Started | sonnet | serial oracle. Needs DiaParallelExecutor |
| 3 | `ResolutionPolicy` / `ChooseTexelsPerUnit` | AC 6 | Not Started | haiku | |
| 4 | `AffectedRect` + dilation rule | AC 7 | Not Started | sonnet | the risky part: tested by brute force |
| 5 | `BakedWorld`: `Bake`, `RebakeBegin`, pending set | AC 5, 8, 9 | Not Started | sonnet | |
| 6 | `Commit`, `DiscardPending`, changed rect | AC 10 | Not Started | sonnet | |
| 7 | Metrics and logs | AC 11 | Not Started | haiku | |
| 8 | Synthetic-grid builder in `Testing/`, goldens (Small, Standard, Large sized) and timing notes | AC 12, 14 | Not Started | sonnet | real S-1 preset goldens go to the CoW consumer |
| 9 | `ITilePostPass` hook through `BakeWorld`, `BakedWorld`, `AffectedRect` and the resolution policy | AC 13 | Not Started | sonnet | added 2026-11-23 (layer split) |
| 10 | `BakedWorld::RebakeAll` | AC 15 | Not Started | sonnet | added 2026-11-24 (review); after tasks 5, 6 |
