# Feature Spec: Rectangular Hex Grid

**Parent:** @docs/specs/applications/dia/systems/diapathfinding/diapathfinding.md

**Gameplay Domains:** pathfinding

## Summary

`HexPathGrid` today is a hexagon-shaped, radius-based axial grid with a fixed capacity of 1,681 bounding-box entries (1,261 real cells at the maximum radius of 20). That cannot hold a game map. This feature extends the same class with a second layout: a **rectangular `width × height` hex grid** in odd-r offset coordinates, with storage sized to the grid. It also adds `HexTopology`, the single place that knows hex neighbours, directions and distances, so every consumer of a rectangular hex map (the pathfinding grid, world generation, weather, renderers) uses one definition.

The existing radius constructor and axial behaviour are unchanged, so DiaFlowField, DiaGridVisibility and their tests keep working.

*Origin:* CoW's A4 Weather and S-1 World Generation need a rectangular hex world (Small 66×42 = 2,772 cells, Standard 80×52 = 4,160, Large 104×64 = 6,656). CoW needs it; the implementation is generic and belongs in Dia (layer boundary rule).

## Problem

- The radius grid cannot hold even the Small map (2,772 cells vs 1,261).
- A game world is a rectangle (flat arrays indexed `y * width + x`), not a hexagon.
- Neighbour order, direction angles and hex distance would otherwise be re-derived in the generator, the weather simulation, the renderer and the path graph, and would drift.
- `FindPath`'s hex heuristic is `(dx + dy + |dx - dy|) / 2`, which is not the axial hex distance (`(|dq| + |dr| + |dq + dr|) / 2`).

## Goals

1. `HexPathGrid(int width, int height)` — rectangular, odd-r offset, pointy-top, 6-connected, per-cell passability, satisfies `CPathGraph`
2. Storage sized to `width * height` (no fixed cap); tested up to 128×128 and exercised at the three CoW map sizes
3. `HexTopology` — index/coordinate conversion, `NeighbourAt`, hex `Distance`, direction-angle helpers; deterministic and allocation-free
4. Deterministic neighbour order: direction order 0..5 (below), so path results and anything built on `GetNeighbours` are reproducible
5. `FindPath` uses the correct admissible heuristic for both hex layouts
6. Existing radius mode, its API, DiaFlowField and DiaGridVisibility keep working unchanged

## Non-Goals

1. World wrap (hard edges only)
2. Flat-top hexes or even-r / axial-rectangle layouts (one layout, pointy-top odd-r)
3. Making DiaFlowField's `HexFlowAdapter` or DiaGridVisibility support the rectangular layout (see Open Design Questions)
4. Removing the radius constructor (can be retired later if nothing needs it)
5. Cell data beyond passability (terrain, biome etc. live with the caller)

## Architecture

### Layout

- **Pointy-top hexes, odd-r offset:** odd rows are shifted half a cell to the right. Row index grows downward (south).
- **`CellCoord`** in rectangular mode: `x` = column, `y` = row. Cell index = `y * width + x`.
- **Direction order**, `d = 0..5`, at angle `d × 60°` counter-clockwise from east (screen view, y down):

| d | Direction | Even row (dx, dy) | Odd row (dx, dy) |
|---|-----------|-------------------|------------------|
| 0 | E  | (+1, 0)  | (+1, 0)  |
| 1 | NE | (0, -1)  | (+1, -1) |
| 2 | NW | (-1, -1) | (0, -1)  |
| 3 | W  | (-1, 0)  | (-1, 0)  |
| 4 | SW | (-1, +1) | (0, +1)  |
| 5 | SE | (0, +1)  | (+1, +1) |

The opposite of `d` is `(d + 3) % 6`.

### API

```cpp
namespace Dia::Pathfinding {

    enum class HexLayout { kAxialHexagon, kOffsetRectangle };

    class HexPathGrid {
    public:
        explicit HexPathGrid(int radius);                 // existing: axial hexagon, unchanged
        HexPathGrid(int width, int height);               // new: rectangular odd-r offset

        void SetPassable(CellCoord cell, bool passable);
        bool IsPassable(CellCoord cell) const;
        void GetNeighbours(CellCoord cell,
                           Dia::Core::Containers::DynamicArrayC<CellCoord, 16>& out) const;  // direction order 0..5 in offset mode

        HexLayout GetLayout() const;
        int GetRadius() const;                            // axial mode only (asserts in offset mode)
        int GetWidth() const;                             // offset mode only
        int GetHeight() const;                            // offset mode only
    };

    constexpr uint32_t kNoCell        = 0xFFFFFFFFu;
    constexpr uint32_t kHexDirections = 6;

    // Rectangular odd-r topology. Pure value type; cheap to copy.
    class HexTopology {
    public:
        HexTopology(int width, int height);

        uint32_t  CellCount() const;
        bool      IsInBounds(CellCoord coord) const;
        uint32_t  ToIndex(CellCoord coord) const;         // y * width + x
        CellCoord ToCoord(uint32_t index) const;

        // Neighbour of 'cell' in direction 'dir' (0..5); kNoCell past the edge.
        uint32_t  NeighbourAt(uint32_t cell, uint32_t dir) const;

        static uint32_t Distance(CellCoord a, CellCoord b);               // hex steps, via cube coordinates
        static uint32_t DirectionFromAngle(float radians);                // round(angle / 60°) mod 6
        static Dia::Maths::Vector2 DirectionVector(uint32_t dir);         // unit vector, constant table
    };
}
```

- `HexPathGrid(width, height)` uses `HexTopology` internally, so its `GetNeighbours` and `NeighbourAt` can never disagree.
- Public APIs use Dia containers only (PD-004). Counts are validated: `width >= 1`, `height >= 1`, and `width * height` must fit `int` (asserted).
- `NeighbourAt` is computed from row parity, not stored. A precomputed table is an optimisation to add only if a profile asks for it.

### Heuristic

`FindPath` selects the heuristic from the graph's layout:

- **Axial:** cube distance `(|dq| + |dr| + |dq + dr|) / 2` (replaces the current formula).
- **Offset rectangle:** `HexTopology::Distance`.

Both are admissible and consistent for unit edge cost. Callers with costs below 1 are unchanged from today's behaviour (the existing caveat applies).

## Dependencies

- DiaCore (`DynamicArrayC`, asserts), DiaMaths (`Vector2`). No new module dependencies.
- The three CoW map sizes are the reference workloads. CoW's S-1 (`WorldStructure`), A4 (`WeatherSimulation`) and S-1's reachability adapter consume `HexTopology` and `HexPathGrid(width, height)` directly.

## Acceptance Criteria

Tests are GoogleTests under `Cluiche/Tests/GoogleTests/DiaPathfinding/`. New public API follows TDD.

### AC1 — Capacity and construction
Construct `HexPathGrid(66, 42)`, `(80, 52)`, `(104, 64)` and `(128, 128)`. **Pass:** each reports `GetLayout() == kOffsetRectangle` and the right width/height; every in-bounds cell starts passable; out-of-bounds cells report not passable; `SetPassable` out of bounds is ignored. Construction does not assert.

### AC2 — Neighbour table
For a small even-height and odd-height grid, check all 6 neighbours of an interior cell on an even row and on an odd row against the table above, and check direction 0 is `x + 1` on the same row and direction 3 is `x - 1`. **Pass:** exact match.

### AC3 — Edges and symmetry
For several grid sizes (including 1 wide, 1 tall, 1×1): `NeighbourAt(cell, d)` is `kNoCell` exactly when the neighbour is outside the grid; otherwise `NeighbourAt(NeighbourAt(cell, d), (d + 3) % 6) == cell`. An interior cell has 6 distinct neighbours; a corner cell has fewer. **Pass:** holds for every cell and direction.

### AC4 — `GetNeighbours` matches topology and is ordered
For every cell of a 12×9 grid, `GetNeighbours` returns exactly the in-bounds passable neighbours of `NeighbourAt` in direction order 0..5. With some cells set impassable, those are omitted and the order of the rest is unchanged. **Pass:** exact match.

### AC5 — Distance
`Distance` is 0 for the same cell, 1 for each neighbour, symmetric, and equals the shortest path length found by a breadth-first search over `NeighbourAt` for every pair on a 10×8 grid. **Pass:** all pairs match.

### AC6 — Direction helpers
`DirectionFromAngle(0) == 0`, `(π/3) == 1`, `(π) == 3`, `(-π/3) == 5`, `(2π - ε) == 0`, and a negative angle wraps correctly. `DirectionVector(d)` is a unit vector at `d × 60°`. **Pass:** all hold.

### AC7 — FindPath on the rectangular grid
On a 20×20 grid: a path from corner to corner succeeds with `cells.size() - 1 == Distance(from, to)` under flat cost; a wall of impassable cells across the full width makes the path fail; a gap lets it succeed through the gap; the path found is the same on repeated runs (determinism). **Pass:** all hold.

### AC8 — Heuristic is admissible
For both layouts, A* path cost equals the breadth-first shortest path cost on random obstacle maps (≥20 maps per layout, fixed seeds). **Pass:** equal on every map. This also covers the corrected axial formula.

### AC9 — Axial mode unchanged
The existing `TestPathfinding*`, `TestFlowFieldAdapters` and DiaGridVisibility tests pass without edits. **Pass:** zero regressions. Where an existing golden depended on the old axial heuristic and legitimately changes, the change is reviewed and noted in the plan, not silently regenerated.

### AC10 — Reference-size performance
On the Large grid (104×64) with scattered obstacles, 1,000 `FindPath` calls between random passable cells complete in a time the plan records (report only; no fail threshold). The full-grid neighbour sweep (6,656 cells × 6 directions) completes in under 1 ms in Release. **Pass:** sweep under 1 ms.

## Inherited Binding Decisions

| ID | Source | How it applies |
|----|--------|----------------|
| PD-001 | DiaPathfinding | `CPathGraph` stays a concept; the new constructor does not add virtual dispatch to the hot path |
| PD-003 | DiaPathfinding | DiaPathfinding owns its grids; no dependency on DiaGeometry2D::Grid |
| PD-004 | DiaPathfinding | Heuristic is selected by layout inside `FindPath`; no injectable heuristic |
| PD-004 | Platform | Dia containers in public APIs |
| AD-003 | Dia App | Namespace `Dia::Pathfinding::` |

No other binding constraints apply.

## Open Design Questions

| # | Question | Impact | Resolution |
|---|----------|--------|------------|
| 1 | Should `HexFlowAdapter` (DiaFlowField) and DiaGridVisibility support the rectangular layout? | CoW's pathing uses `FindPath` first; FlowField on a hex world is the likely next need (S-4) | Out of scope here. Until they do, they must `DIA_ASSERT` on `GetLayout() != kAxialHexagon` rather than misbehave. Track as a follow-up row when a consumer needs it |
| 2 | One class with two layouts, or split into `HexPathGrid` (axial) and `OffsetHexPathGrid`? | One class keeps `FindPath`'s `is_same_v<HexPathGrid>` dispatch and the existing consumers working; two classes is cleaner but breaks that dispatch | One class (user decision 2026-10-14: a single hex graph in Dia). Retiring the axial mode later would simplify it |
| 3 | Should `NeighbourAt` use a precomputed table? | 4–7k cells × 6 directions is under 200 KB; parity arithmetic is already cheap | Compute from parity now; add a table only if a CoW profile shows it matters |

## Status

`Done` — 2026-10-14

**Plan:** @docs/specs/applications/dia/systems/diapathfinding/hex-rectangular-grid.plan.md
