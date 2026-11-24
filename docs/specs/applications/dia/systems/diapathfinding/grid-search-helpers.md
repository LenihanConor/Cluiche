# Feature Spec: DiaPathfinding Grid Search Helpers

**Parent:** @docs/specs/applications/dia/systems/diapathfinding/diapathfinding.md
**Status:** `Done`
**Research:** @docs/research/worldsim_data_driven/summary.md (§0: D4; §5b "Gaps where Dia has nothing")

## Summary

This feature adds four generic, deterministic grid searches to DiaPathfinding, over any index-based graph:

1. multi-source step distance
2. flood fill and connected components
3. multi-source Dijkstra, with an optional settle hook
4. priority flood

A `HexNeighbours` adapter runs them on `HexTopology`. A caller-supplied neighbour source runs them on other graphs, such as a river flow tree.

S-1 migrates all nine of its hand-written searches onto these helpers, with bit-identical goldens. The World-Sim Kit's derived `distance` and `upstream` fields, A1's `edge_distance` and A5's basin work use them from the start.

## Problem

S-1 has nine hand-written searches over `HexTopology`:

| Kind | Where |
|---|---|
| Multi-source unit-step search | `BiomeClimate.cpp:118` (with start offsets: rivers start at `kRiverPenalty`), `TerrainLandmass.cpp:217`, `TerrainLandmass.cpp:497` (twice, ocean→land then land→ocean), `TerrainMountains.cpp:210` (component labelling) |
| Flood fill | `TerrainLandmass.cpp:410` |
| Tree depth walk over a child graph | `NodesRiver.cpp:61` |
| Multi-source Dijkstra | `TerrainDepthZones.cpp:63` (double cost), `NavigationQuery.cpp:86` (float cost, early exit, `prev`), `RegionPartition.cpp:276` (seed-labelled, capped, two-phase floor) |
| Priority flood | `TerrainDrainage.cpp:72` |

A1, A5 and the World-Sim Kit each need more searches. DiaFlowField doesn't fit: it is single-goal and outputs directions only.

## Goals

1. One tested implementation of each search, with a documented, deterministic order.
2. Works on hex neighbours and on caller-defined graphs, with zero overhead (a concept, PD-001).
3. Every S-1 search migrates with **bit-identical** results.

## Non-Goals

- A* / `FindPath` changes. `PathfindingSystem`.
- Flow fields (DiaFlowField).
- Changing any S-1 algorithm's behaviour.
- Making the searches multi-threaded. Each helper runs on the calling thread.

## Design (`Dia/DiaPathfinding/GridSearch.h`, `GridSearchScratch.h`)

```cpp
namespace Dia::Pathfinding {
    template<class G> concept CIndexGraph = requires(const G& g, uint32_t c) {
        { g.CellCount() } -> std::convertible_to<uint32_t>;
        g.ForEachNeighbour(c, [](uint32_t /*nb*/) {});        // visits neighbours in a fixed order
    };
    class HexNeighbours {                                     // satisfies CIndexGraph
    public: explicit HexNeighbours(const HexTopology&);       // direction order 0..5, kNoCell skipped
    };

    constexpr uint32_t kUnreached = 0xFFFFFFFFu;
    struct StepSeed { uint32_t cell; uint32_t startSteps; uint32_t id; };
    template<class Cost> struct CostSeed { uint32_t cell; Cost startCost; uint32_t id; };

    // 1. Bucketed BFS. Lower start offsets win; ties keep the lowest seed index, then the lowest cell index.
    template<CIndexGraph G, class Passable>
    void MultiSourceSteps(const G&, const StepSeed* seeds, uint32_t seedCount, Passable passable,
                          uint32_t maxSteps, uint32_t* outSteps, uint32_t* outSeedId /*nullable*/,
                          GridSearchScratch&);

    // 2. Flood fill / connected components.
    template<CIndexGraph G, class InRegion>
    uint32_t FloodRegion(const G&, uint32_t start, InRegion inRegion, uint8_t* seen,
                         uint32_t* outCells, GridSearchScratch&);                 // returns the cell count, in BFS order
    template<CIndexGraph G, class InRegion>
    uint32_t ConnectedComponents(const G&, InRegion inRegion, uint32_t* outLabel /*kUnreached outside*/,
                                 uint32_t* outSizes /*nullable, by label*/, GridSearchScratch&);   // labels in ascending-start-cell order

    // 3. Dijkstra. Pop order is (cost, cell index), a strict total order. stepCost(from, to) < 0 blocks the edge (PD-008).
    enum class Settle : uint8_t { Accept, Skip, Park };
    template<class Cost> struct DijkstraOptions {
        Cost*     outCost      = nullptr;     // +inf when unreached
        uint32_t* outPrev      = nullptr;     // kNoCell
        uint32_t* outSeedId    = nullptr;     // which seed reached the cell
        const uint8_t* stopAt  = nullptr;     // stop when the first settled cell has stopAt[c] != 0
        uint32_t* outStoppedAt = nullptr;
    };
    template<class Cost, CIndexGraph G, class StepCost, class OnSettle = AcceptAll>
    void MultiSourceDijkstra(const G&, const CostSeed<Cost>* seeds, uint32_t seedCount, StepCost stepCost,
                             const DijkstraOptions<Cost>&, GridSearchScratch&, OnSettle onSettle = {});
    //   onSettle(cell, cost, seedId) -> Settle::Accept (settle and expand) | Skip (drop the entry)
    //                                 | Park (hold the entry; it rejoins with ResumeParked)
    //   ResumeParked(scratch): parked entries rejoin the queue, and the search continues (RegionPartition's phase 2).

    // 4. Priority flood (depression filling) from outlet cells.
    template<CIndexGraph G>
    void PriorityFlood(const G&, const float* elevation, const uint8_t* isOutlet, float epsilon,
                       float* outFilled, float* outLevel, GridSearchScratch&);
}
```

- **Determinism.** Every helper documents its visit and tie order, and that order matches today's S-1 code: neighbour direction order 0..5, then `(cost, cell index)` or `(level, filled, cell)` heap orders. Results are bit-identical across runs and builds. `Cost` is a template parameter, so a `double` caller (depth zones) and a `float` caller (navigation, regions) keep their own arithmetic.
- **Memory.** `GridSearchScratch` owns the queues, buckets and heaps as Dia containers, reused across calls, so there is no per-call allocation once warm. Outputs are caller-owned arrays.
- **PD-004.** Pointers and counts in the API; no STL in public headers. The heap is a Dia-owned binary heap over a `DynamicArray`.
- **Testing helpers** (PD-009). `DiaPathfinding/Testing/GridSearchTestGrids.h` provides small hand-built hex grids with known answers, and a `ChainGraph` / `TreeGraph` for non-hex neighbour sources.

### S-1 migration

| Call site | Helper |
|---|---|
| `BiomeClimate.cpp:118` water distance (river start offset) | `MultiSourceSteps` with `startSteps` |
| `TerrainLandmass.cpp:217` distance to the reference mainland | `MultiSourceSteps` |
| `TerrainLandmass.cpp:497` coast distance, both passes | `MultiSourceSteps` ×2 |
| `TerrainLandmass.cpp:410` `Flood` and landmass labelling | `FloodRegion` / `ConnectedComponents` |
| `TerrainMountains.cpp:210` ridge groups | `ConnectedComponents` (sizes) |
| `NodesRiver.cpp:61` longest upstream path | `MultiSourceSteps` over an `AdjacencyGraph` of `children` (deepest, lowest index) |
| `TerrainDepthZones.cpp:63` zone cost | `MultiSourceDijkstra<double>` |
| `NavigationQuery.cpp:86` reachability | `MultiSourceDijkstra<float>` with `stopAt` + `outPrev`; cost from `IPathCostProvider` through an adapter |
| `RegionPartition.cpp:276` capped, floored region growth | `MultiSourceDijkstra<float>` with `outSeedId` and an `onSettle` that skips claimed or capped cells and parks satisfied regions in phase 0, then `ResumeParked` for phase 1 |
| `TerrainDrainage.cpp:72` fill | `PriorityFlood` (`kFillEpsilon`) |

## Acceptance Criteria

1. **Concept and adapter.** `CIndexGraph` and `HexNeighbours` exist. `HexNeighbours` visits neighbours in direction order 0..5 and skips edges. A static assert shows `HexNeighbours`, `ChainGraph` and `TreeGraph` satisfy the concept.
2. **`MultiSourceSteps`.** On hand-built grids it gives exact step counts:
   - start offsets are honoured; `maxSteps` stops the search; impassable cells stay `kUnreached`;
   - `outSeedId` reports the nearest seed, with ties going to the lower seed index;
   - no seeds means everything stays `kUnreached`.
3. **`FloodRegion` and `ConnectedComponents`.** Both return cells in BFS order. Labels are assigned in ascending order of each component's first cell, and sizes are correct. A predicate that's false everywhere gives 0 components.
4. **`MultiSourceDijkstra`.**
   - Costs are exact on hand-built weighted grids, for `float` and `double`.
   - A negative step cost blocks the edge.
   - The pop order is `(cost, cell index)`, shown by a tie-heavy grid with a fixed expected order.
   - `stopAt` stops at the first target; `outPrev` rebuilds the path; `outSeedId` labels cells.
   - The settle hook works: `Skip` drops an entry; `Park` holds it; `ResumeParked` resumes it; and a fixture reproducing RegionPartition's cap and floor gives the expected labels.
5. **`PriorityFlood`.** It fills depressions to their spill level (+ epsilon per step for `filled`, exact for `level`). Cells never reached keep their raw elevation, matching the current drainage behaviour on a hand-built basin grid.
6. **No allocation once warm.** Reusing one `GridSearchScratch` for a second call of the same size allocates nothing (allocation-counting test).
7. **S-1 migration.** All nine call sites in the table use the helpers, and their local search code is deleted. S-1 layer goldens (4 presets × seeds 1–3, Small, every layer) are **bit-identical**. Reachability, validation, region and S-1 tests pass unchanged. The `SLOW_` region test is no worse than before (its existing failure is tracked separately).
8. **Perf.** (amended 2026-11-19, user: measured, the 1 ms target did not fit a heap search in Debug) On a Large map (104×64) in Debug, as a ballpark check (quick-and-dirty perf rule, no Release benchmark): `MultiSourceSteps` runs in under 1 ms (measured 0.92 ms), and `MultiSourceDijkstra<float>` runs within 1.5× of the equivalent `std::priority_queue` Dijkstra (measured 3.6 ms vs 7.1 ms, 0.52×). The test logs both and fails only at regression guards.

9. **`AdjacencyGraph` build and order.** (added 2026-11-18, plan-gap resolution) `Build(cellCount, edges, edgeCount)` produces CSR offsets plus neighbour indices; edges are sorted by `(from, to)`, so neighbour order is deterministic whatever the input order. Isolated cells and cells with no edges have empty ranges; duplicate edges are kept.
10. **`AdjacencyGraph` conformance.** (added 2026-11-18, plan-gap resolution) A static assert shows `AdjacencyGraph` satisfies `CIndexGraph`, and `MultiSourceSteps` over one gives exact step counts on a hand-built tree and a forest.
11. **`NodesRiver` migration.** (added 2026-11-18, plan-gap resolution) The `NodesRiver.cpp` longest-upstream-path site builds an `AdjacencyGraph` from the river tree and runs `MultiSourceSteps` over it; no CoW-local adapter remains, and S-1 goldens stay bit-identical.

## Files Touched

| File | Change |
|---|---|
| `Dia/DiaPathfinding/GridSearch.h` (+ `.inl`), `GridSearchScratch.h/.cpp`, `HexNeighbours.h` | **New** |
| `Dia/DiaPathfinding/AdjacencyGraph.h` (+ `.cpp` if needed) | **New**, production type (plan-gap resolution) |
| `Dia/DiaPathfinding/Testing/GridSearchTestGrids.h` | **New**; `TreeGraph` is a thin test fixture over `AdjacencyGraph` |
| `Dia/DiaPathfinding/Docs/*.module.md` | Responsibilities |
| `diapathfinding.md` | Feature row |
| `Cluiche/CoW/Shared/WorldGeneration/{BiomeClimate,TerrainLandmass,TerrainMountains,NodesRiver,TerrainDepthZones,NavigationQuery,RegionPartition,TerrainDrainage}.cpp` | Migrated |
| `Cluiche/Tests/GoogleTests/DiaPathfinding/TestGridSearch*.cpp` | **New** |
| vcxproj | Via `dia docs vcxproj-add` |

## Binding Decisions

| ID | Decision | How this feature complies |
|----|----------|---------------------------|
| PD-001 (DiaPathfinding) | Graphs are a C++20 concept, not a virtual base | `CIndexGraph` is a concept, and the helpers are templates. It is index-based (not `CellCoord`) because these are whole-grid passes; `CPathGraph` is unchanged. |
| PD-002 / PD-008 (DiaPathfinding) | Costs through `IPathCostProvider`; `kImpassableCost < 0` blocks | Step cost is any callable, so S-1 lambdas keep their exact arithmetic. A negative value blocks the edge. An `IPathCostProvider` adapter is provided for navigation. |
| PD-005 (DiaPathfinding) | Single-threaded on SimPU | The helpers are synchronous on the calling thread (SimPU or a deferred-step worker, each with its own scratch). |
| PD-009 (DiaPathfinding) | Test utilities in `Testing/` | `GridSearchTestGrids.h`. |
| PD-004 (Platform) | No STL in public APIs | Pointer and count arguments; Dia containers in the scratch. |
| AD-001 (CoW) | Ask before Dia edits | Signed off 2026-11-18 (research D4). The production `AdjacencyGraph` header (Plan-gap resolutions) was signed off by the user on 2026-11-18. |

## Test Plan

TDD per `.claude/skills/tdd.md`:
1. The concept and adapter.
2. Each helper on hand-built grids, including tie-order fixtures.
3. The settle hook and the RegionPartition fixture.
4. Scratch reuse with no allocation.
5. Migrate S-1 call sites one at a time, each followed by a golden check.
6. Perf ballpark.

Scoped runs use `--filter="*GridSearch*:*Pathfinding*:*WorldGen*:*Region*:*Reachability*:*Navigation*:*Biome*:*Terrain*"` with `--shards 2`. There is one full suite run at close-out.

## Open Design Questions

1. **Hidden float order in S-1.** Some S-1 searches may depend on accumulation order in ways a shared helper changes subtly, for example DepthZones adds a source cost of 1.0 regardless of the cell's terms. **Recommended:** migrate one call site at a time with a golden check after each. If one can't be made bit-identical, stop and report rather than re-baseline.

## Plan-gap resolutions (2026-11-18, user)

Each item supersedes contradicting text above.

1. **`AdjacencyGraph` is a production type** in `Dia/DiaPathfinding/AdjacencyGraph.h`, not a test helper.
   - CSR storage: offsets plus neighbour indices, in Dia containers.
   - `Build(cellCount, edges (from, to) pointer + count)`, with deterministic neighbour order (edges sorted by `(from, to)`).
   - It satisfies `CIndexGraph`.
2. **Consumers:**
   - the S-1 `NodesRiver` river tree (replacing the plan's CoW-local adapter);
   - the World-Sim Kit's derived `upstream` field;
   - A5's downstream graph.
3. `TreeGraph` in `Testing/` may stay as a thin test fixture over `AdjacencyGraph`; `ChainGraph` is unchanged.
4. New ACs 9-11 (above).

## Status

**Status:** `Done`

Approved 2026-11-18 (user; design accepted in one step, with the settle hook chosen so RegionPartition migrates too). Plan: @docs/specs/applications/dia/systems/diapathfinding/grid-search-helpers.plan.md. Built 2026-11-19: additive `DijkstraOptions::pushAll` and a (cost, cell, seedId) heap key were needed for RegionPartition; continuation is `ResumeParked` + `ContinueDijkstra`. AC 8 amended after measurement.
