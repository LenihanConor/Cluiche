# Feature Spec: Scalable Grid and FlowField Storage

**Parent:** @docs/specs/applications/dia/systems/diaflowfield/diaflowfield.md

**Gameplay Domains:** pathfinding, ai

**Also amends:** @docs/specs/applications/dia/systems/diapathfinding/diapathfinding.md (`SquarePathGrid` storage). The two caps are one problem: the `FlowField` limit was copied from `SquarePathGrid`, so they are fixed together.

## Summary

Two Dia classes store their cells in a fixed-capacity `DynamicArrayC<..., 4096>`: `FlowField` (`FlowCell`s, asserts above 4096 cells) and `SquarePathGrid` (`bool` passability, `kMaxCells = 4096`, **no size check at all**: its constructor adds `width * height` entries regardless, so a grid over 64×64 overruns the array). The `FlowField` limit was copied from `SquarePathGrid`'s 64×64 cap and has nothing to do with flow fields. CoW's Standard (80×52 = 4,160) and Large (104×64 = 6,656) maps are over it; only Small (66×42 = 2,772) fits.

This feature gives both `FlowField` and `SquarePathGrid` heap storage sized to the grid, with no cell cap (matching what `HexPathGrid(width, height)` already does), and makes world-position sampling work for any grid type by going through the graph's `WorldToCell`. `ComputeFlowField` is already generic over `CFlowFieldGraph` (the rectangular hex adapter needed no changes inside it), so the compute algorithm is not part of this feature unless the investigation below finds a hidden size assumption.

*Origin:* CoW needs flow fields on its hex world for dragon pathing (S-4), and "any grid size, any grid type" is the stated engine goal after the rectangular hex grid work. The change is generic engine capability, so it belongs in Dia (layer boundary rule).

## Problem

- A `FlowField` for a 4,160-cell or 6,656-cell grid cannot be constructed: the constructor asserts (and in Release would overflow the fixed array).
- A `SquarePathGrid` larger than 64×64 cannot be built: nothing checks, so it writes past a fixed array (undefined behaviour, or a silent drop depending on what `DynamicArrayC::Add` does at capacity). A square-grid game map, or a square flow field above 4096 cells, is impossible even though the flow field class would allow it.
- `FlowField::SampleWorld(worldPos, cellSize)` converts a world position to a cell with square division (`x / cellSize`, `y / cellSize`). On a hex field it returns the wrong cell, with no error. The field does not know its grid, so it cannot do better without being handed the graph.
- The existing boundary test pins the 4096 cap (`Constructor_OverCapacity_Asserts`), so the cap is currently "tested behaviour" and the change is a deliberate reversal.

## Goals

1. `FlowField(width, height)` works for any grid whose `width * height` fits `int`; storage is allocated on the heap, sized to the grid
2. No cell-count cap; `kMaxFlowFieldCells` is removed
3. `SquarePathGrid(width, height, connectivity)` stores passability on the heap, sized to the grid, with no cap; `kMaxCells` is removed. Behaviour (4/8 connectivity, neighbour order, bounds, passability) is unchanged
4. A graph-aware world sampling overload, templated on `CFlowFieldGraph`, that uses the graph's `WorldToCell`, so square, axial hex and rectangular hex grids all sample correctly through one call
5. The existing square-only `SampleWorld(worldPos, cellSize)` keeps working for existing callers, documented as square-grid-only
6. `ComputeFlowField` and `FlowFieldCache` work at the CoW map sizes (104×64) and the tested range (128×128), with no hidden fixed-size scratch buffers
7. All existing flow field and pathfinding tests pass, except the one flow field test that pins the cap, which is replaced

## Non-Goals

1. Changing the flow field algorithm (Dijkstra sweep) or direction semantics
2. Changing `SquarePathGrid`'s API, neighbour order or connectivity rules (storage only)
3. Memory pooling or sharing fields between cache entries (see Open Design Question 2)
4. Hex-specific line of sight or visibility (DiaGridVisibility's own follow-up)
5. Fixing the axial `HexFlowAdapter` grid-to-world inconsistency (separate backlog row)

## Architecture

### Storage

`FlowField` holds its cells in a heap-backed Dia container (`Dia::Core::Containers::DynamicArray<FlowCell>`, the non-`C` runtime-sized array in DiaCore) allocated once in the constructor to `width * height` and never resized. The public interface does not change except where noted. The constructor asserts `width >= 1`, `height >= 1` and that `width * height` fits `int`.

Rationale: keeps PD-004 (Dia containers in public state) and matches how the grid classes are being fixed (`HexPathGrid(width, height)` already sizes to the map).

### SquarePathGrid storage (DiaPathfinding)

`SquarePathGrid`'s `mPassable` becomes a heap-sized container allocated once in the constructor to `width * height` (all cells passable, as today). The constructor asserts `width >= 1`, `height >= 1` and that `width * height` fits `int` (the missing check). Public API and behaviour are unchanged; `kMaxCells` is removed. The DIA_DEBUG `VisitCells` helper and everything that constructs a `SquarePathGrid` keep working. `HexPathGrid`'s axial (radius) mode keeps its own small fixed array on purpose (radius is bounded by definition); only the square grid changes here.

### Graph-aware sampling

```cpp
namespace Dia::FlowField {

    class FlowField {
    public:
        // Existing, unchanged behaviour: square-grid cell size only.
        Dia::Maths::Vector2D SampleWorld(Dia::Maths::Vector2D worldPos, float cellSize) const;

        // New: converts the position with the graph's own WorldToCell, so it is correct for
        // any CFlowFieldGraph (square, axial hex, rectangular hex).
        template<CFlowFieldGraph TGraph>
        const FlowCell& Sample(const TGraph& graph, Dia::Maths::Vector2D worldPos, float cellSize) const;

        template<CFlowFieldGraph TGraph>
        Dia::Maths::Vector2D SampleWorld(const TGraph& graph, Dia::Maths::Vector2D worldPos, float cellSize) const;
    };
}
```

- `Sample(graph, worldPos, cellSize)` is `Sample(graph.WorldToCell(worldPos, cellSize))`, so out-of-bounds positions return the default unreachable cell exactly as `Sample(CellCoord)` does today.
- The graph argument is not stored: the field stays graph-agnostic, and callers keep owning the adapter they already pass to `ComputeFlowField`.

### What must not be assumed fixed-size

The implementation task starts by auditing `ComputeFlowField`, `FlowFieldCache` and the Testing helpers for fixed-capacity containers or cell-count assumptions (priority queue, visited set, scratch arrays, cache entry copies). Anything sized to the grid becomes sized to the grid. Findings are recorded in the plan notes; any change that would need a DiaPathfinding edit stops and is reported.

## Public Interfaces

The additions are the two templated overloads above. Removed: `kMaxFlowFieldCells`. Behaviour change: the `FlowField(width, height)` constructor no longer asserts on large grids.

## Dependencies

- DiaCore (`DynamicArray`, asserts), DiaMaths (`Vector2D`), DiaPathfinding (`CellCoord`, `CPathGraph`). No new module dependencies.
- Consumers: CoW S-4 (dragon pathing on the rectangular hex world), the existing DiaFlowField, DiaPathfinding and DiaGridVisibility tests, the CluicheTest pathfinding stage (uses `SampleWorld` and `SquarePathGrid`), `SquareFlowAdapter`, and `GridVisibilitySystem` (constrained on `SquarePathGrid`).

## Acceptance Criteria

Tests are GoogleTests under `Cluiche/Tests/GoogleTests/DiaFlowField/`. New behaviour follows TDD.

### AC1 — Large construction
`FlowField(104, 64)` and `FlowField(128, 128)` construct without asserting; `GetCellCount()` is `width * height`; `AccessCell` and `Sample` work on the first and last cell; an out-of-bounds `Sample` returns the default unreachable cell. **Pass:** all hold.

### AC2 — The cap test is replaced
`SLOW_FlowField_Boundary.Constructor_OverCapacity_Asserts` (which pins the 4096 cap) is replaced by a test that `FlowField(65, 64)` (4,160 cells) constructs and is usable. The stress-test comments that cite `kMaxFlowFieldCells` are updated. No other existing test is edited. **Pass:** zero other changes to pre-existing tests.

### AC3 — Compute at CoW map sizes
`ComputeFlowField` over `HexFlowAdapter` on `HexPathGrid(104, 64)` with a goal near the middle completes; every passable reachable cell has a direction that moves it strictly closer to the goal by `HexTopology::Distance`; an impassable wall with one gap routes through the gap. The same over a 128×128 rectangular hex grid completes. **Pass:** all hold.

### AC4 — Graph-aware sampling is correct on hex
For a field computed on `HexPathGrid(20, 15)`, `Sample(adapter, HexTopology::CellCenter(c, size), size)` returns the same cell as `Sample(c)` for every cell. The square-only `SampleWorld(worldPos, cellSize)` on the same hex field is shown (in a test that documents the limitation) to disagree for at least one cell, proving the new overload is needed. Out-of-bounds world positions return the default unreachable cell. **Pass:** all hold.

### AC5 — Existing behaviour unchanged
The square adapter and axial hex adapter produce the same fields and samples as before (existing tests pass unedited); `Sample(adapter, ...)` agrees with the square-only `SampleWorld` on a square grid. **Pass:** zero regressions.

### AC6 — SquarePathGrid at large sizes
`SquarePathGrid` constructs without asserting at 66×42, 80×52, 104×64, 128×128 and 256×256 (the last as the tested ceiling), for both 4- and 8-connectivity: every in-bounds cell starts passable, out-of-bounds cells are not passable, `SetPassable` out of bounds is ignored, and an interior cell has 4 or 8 neighbours in the existing order. `FindPath` on the 256×256 grid finds a corner-to-corner path of the expected length (`max(dx, dy)` steps for 8-connected, `dx + dy` for 4-connected) and fails across a full wall. `width < 1` or `height < 1` asserts. **Pass:** all hold; the existing tests pass unedited.

### AC7 — Square flow field end to end
`ComputeFlowField` over `SquareFlowAdapter` on a `SquarePathGrid(104, 64)` with a goal near the middle: every passable reachable cell's direction moves it toward the goal, a wall with a gap routes through the gap, and `Sample(adapter, worldPos, cellSize)` agrees with the square-only `SampleWorld` for every cell. `GridVisibilitySystem` still builds and runs over a `SquarePathGrid` larger than 64×64 (one visibility test at 80×52). **Pass:** all hold.

### AC8 — Cache at the large size
`FlowFieldCache` computes, caches and invalidates a field on the 104×64 rectangular hex grid, including `InvalidateRegion`. **Pass:** cache hit returns the same field, invalidation forces recompute.

### AC9 — Reference-size performance (report only)
`ComputeFlowField` on the Large grid (104×64) with scattered obstacles; the plan records the time and peak memory estimate. No fail threshold.

## Inherited Binding Decisions

| ID | Source | How it applies |
|----|--------|----------------|
| PD-004 | Platform / DiaFlowField | Cell storage stays a Dia container. `DynamicArrayC` is fixed-capacity, so the heap-backed `DynamicArray` is used instead; STL only inside `ComputeFlowField` internals as today |
| FD-001 | DiaFlowField | `CFlowFieldGraph` extends `CPathGraph`; the new overloads constrain on `CFlowFieldGraph` and add no new graph requirement |
| FD-006 | DiaFlowField | Direction vectors stay normalised `Vector2D`; sampling returns the stored direction unchanged |

No other binding constraints apply.

## Open Design Questions

| # | Question | Impact | Resolution |
|---|----------|--------|------------|
| 1 | This spec lives under DiaFlowField but changes `SquarePathGrid` in DiaPathfinding. Is that placement right? | The two caps are one defect (the flow field cap was copied from the square grid), and the tests that prove it span both modules, so one spec is simpler. DiaPathfinding's system spec gains a cross-reference row | Keep as one spec (user decision 2026-10-14). If the tasks diverge, split the `SquarePathGrid` part into a DiaPathfinding feature at plan time |
| 2 | `FlowFieldCache` stores one `FlowField` per key. At 6,656 cells a field is about 100 KB (`FlowCell` is a `Vector2D` + `bool`); is that acceptable for many cached goals? | Memory grows with the number of goals (S-4 may cache one per squad) | Accept for now and measure in AC7; add pooling or direction quantisation only if S-4's profile shows a problem |
| 3 | Should the old square-only `SampleWorld(worldPos, cellSize)` be deprecated now that a correct overload exists? | It silently misbehaves on hex fields | Keep it unchanged for existing callers (the CluicheTest stage uses it) and document it as square-only; revisit removal in a later cleanup |

## Status

`Done` — 2026-10-15

**Plan:** @docs/specs/applications/dia/systems/diaflowfield/scalable-storage.plan.md
