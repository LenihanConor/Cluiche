# Feature Spec: Rectangular Hex Grid

**Parent:** @docs/specs/applications/dia/systems/diapathfinding/diapathfinding.md

**Gameplay Domains:** pathfinding

## Summary

`HexPathGrid` today is a hexagon-shaped, radius-based axial grid with a fixed capacity of 1,681 bounding-box entries (1,261 real cells at the maximum radius of 20). That cannot hold a game map. This feature extends the same class with a second layout: a **rectangular `width × height` hex grid** in **flat-top odd-q** offset coordinates, with storage sized to the grid. It also adds `HexTopology`, the single place that knows hex neighbours, directions, distances and cell geometry, so every consumer of a rectangular hex map (the pathfinding grid, world generation, weather, renderers) uses one definition.

The existing radius constructor and axial behaviour are unchanged, so the existing DiaFlowField, DiaGridVisibility and pathfinding tests keep working. `HexFlowAdapter` gains support for the rectangular layout; DiaGridVisibility is guarded against hex grids (its square-lattice shadowcasting is wrong for hexes) until a hex line-of-sight feature exists.

*Amended 2026-10-14:* first Approved and built as pointy-top odd-r. Changed to flat-top odd-q at the owner's request (the layout was chosen without asking). Added cell geometry (`CellCenter` / `CellFromPoint`), `HexFlowAdapter` support and the GridVisibility guard.

*Origin:* CoW's A4 Weather and S-1 World Generation need a rectangular hex world (Small 66×42 = 2,772 cells, Standard 80×52 = 4,160, Large 104×64 = 6,656). CoW needs it; the implementation is generic and belongs in Dia (layer boundary rule).

## Problem

- The radius grid cannot hold even the Small map (2,772 cells vs 1,261).
- A game world is a rectangle (flat arrays indexed `y * width + x`), not a hexagon.
- Neighbour order, direction angles and hex distance would otherwise be re-derived in the generator, the weather simulation, the renderer and the path graph, and would drift.
- `FindPath`'s hex heuristic is `(dx + dy + |dx - dy|) / 2`, which is not the axial hex distance (`(|dq| + |dr| + |dq + dr|) / 2`).

## Goals

1. `HexPathGrid(int width, int height)` — rectangular, odd-q offset, flat-top, 6-connected, per-cell passability, satisfies `CPathGraph`
2. Storage sized to `width * height` (no fixed cap); tested up to 128×128 and exercised at the three CoW map sizes
3. `HexTopology` — index/coordinate conversion, `NeighbourAt`, hex `Distance`, direction-angle helpers, and cell geometry (`CellCenter`, `CellFromPoint`); deterministic and allocation-free
4. Deterministic neighbour order: direction order 0..5 (below), so path results and anything built on `GetNeighbours` are reproducible
5. `FindPath` uses the correct admissible heuristic for both hex layouts
6. Existing radius mode and its API keep working unchanged; the existing DiaFlowField and DiaGridVisibility tests pass without edits
7. `HexFlowAdapter` works on the rectangular layout (world position ↔ cell)
8. `GridVisibilitySystem` refuses hex grids at compile time instead of silently computing wrong line of sight

## Non-Goals

1. World wrap (hard edges only)
2. Pointy-top hexes or even-q / axial-rectangle layouts (one rectangular layout, flat-top odd-q)
3. Hex line-of-sight for DiaGridVisibility (needs its own feature spec; this feature only adds the compile-time guard)
4. Removing the radius constructor (can be retired later if nothing needs it)
5. Cell data beyond passability (terrain, biome etc. live with the caller)
6. Fixing the existing axial `HexFlowAdapter` grid-to-world formulas (`CellToWorldPosition` uses the pointy-top formula while `WorldToCell` and its comments use flat-top; logged separately in the backlog)

## Architecture

### Layout

- **Flat-top hexes, odd-q offset:** odd **columns** are shifted half a cell down. Row index grows downward (south).
- **`CellCoord`** in rectangular mode: `x` = column, `y` = row. Cell index = `y * width + x`.
- **Cell geometry** (world space, y down, `size` = centre-to-corner distance): the hex is `2 × size` wide and `√3 × size` tall; columns are `1.5 × size` apart; `CellCenter(col, row) = (1.5 · size · col, √3 · size · (row + 0.5 · (col & 1)))`.
- **Direction order**, `d = 0..5`, at on-screen angle `30° + d × 60°` counter-clockwise (a flat-top hex has no neighbour due east or west). `DirectionVector` uses the same y-down world space as `CellCenter`, so `CellCenter(neighbour) - CellCenter(cell)` points along `DirectionVector(d)`:

| d | Direction | Angle | Even column (dx, dy) | Odd column (dx, dy) |
|---|-----------|-------|----------------------|---------------------|
| 0 | NE | 30°  | (+1, -1) | (+1, 0)  |
| 1 | N  | 90°  | (0, -1)  | (0, -1)  |
| 2 | NW | 150° | (-1, -1) | (-1, 0)  |
| 3 | SW | 210° | (-1, 0)  | (-1, +1) |
| 4 | S  | 270° | (0, +1)  | (0, +1)  |
| 5 | SE | 330° | (+1, 0)  | (+1, +1) |

The opposite of `d` is `(d + 3) % 6`.

### API

```cpp
namespace Dia::Pathfinding {

    enum class HexLayout { kAxialHexagon, kOffsetRectangle };

    class HexPathGrid {
    public:
        explicit HexPathGrid(int radius);                 // existing: axial hexagon, unchanged
        HexPathGrid(int width, int height);               // new: rectangular flat-top odd-q offset

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

    // Rectangular flat-top odd-q topology. Pure value type; cheap to copy.
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
        static uint32_t DirectionFromAngle(float radians);                // round((angle - 30°) / 60°) mod 6; on-screen CCW angle
        static Dia::Maths::Vector2D DirectionVector(uint32_t dir);        // unit vector in y-down world space, constant table

        // Cell geometry in y-down world space; 'size' is the centre-to-corner distance.
        static Dia::Maths::Vector2D CellCenter(CellCoord coord, float size);
        // Cell containing a world point (cube-rounded). May be out of bounds; check IsInBounds.
        static CellCoord CellFromPoint(Dia::Maths::Vector2D point, float size);
    };
}
```

- `HexPathGrid(width, height)` uses `HexTopology` internally, so its `GetNeighbours` and `NeighbourAt` can never disagree.
- Public APIs use Dia containers only (PD-004). Counts are validated: `width >= 1`, `height >= 1`, and `width * height` must fit `int` (asserted).
- `NeighbourAt` is computed from column parity, not stored. A precomputed table is an optimisation to add only if a profile asks for it.

### Heuristic

`FindPath` selects the heuristic from the graph's layout:

- **Axial:** cube distance `(|dq| + |dr| + |dq + dr|) / 2` (replaces the current formula).
- **Offset rectangle:** `HexTopology::Distance`.

Both are admissible and consistent for unit edge cost. Callers with costs below 1 are unchanged from today's behaviour (the existing caveat applies).

## Dependencies

- DiaCore (`DynamicArrayC`, asserts), DiaMaths (`Vector2D`). No new module dependencies.
- The three CoW map sizes are the reference workloads. CoW's S-1 (`WorldStructure`), A4 (`WeatherSimulation`) and S-1's reachability adapter consume `HexTopology` and `HexPathGrid(width, height)` directly.

## Acceptance Criteria

Tests are GoogleTests under `Cluiche/Tests/GoogleTests/DiaPathfinding/`. New public API follows TDD.

### AC1 — Capacity and construction
Construct `HexPathGrid(66, 42)`, `(80, 52)`, `(104, 64)` and `(128, 128)`. **Pass:** each reports `GetLayout() == kOffsetRectangle` and the right width/height; every in-bounds cell starts passable; out-of-bounds cells report not passable; `SetPassable` out of bounds is ignored. Construction does not assert.

### AC2 — Neighbour table
For a small even-height and odd-height grid, check all 6 neighbours of an interior cell in an even column and in an odd column against the table above, and check direction 1 is the cell above (`y - 1`) and direction 4 the cell below (`y + 1`) in the same column. **Pass:** exact match.

### AC3 — Edges and symmetry
For several grid sizes (including 1 wide, 1 tall, 1×1): `NeighbourAt(cell, d)` is `kNoCell` exactly when the neighbour is outside the grid; otherwise `NeighbourAt(NeighbourAt(cell, d), (d + 3) % 6) == cell`. An interior cell has 6 distinct neighbours; a corner cell has fewer. **Pass:** holds for every cell and direction.

### AC4 — `GetNeighbours` matches topology and is ordered
For every cell of a 12×9 grid, `GetNeighbours` returns exactly the in-bounds passable neighbours of `NeighbourAt` in direction order 0..5. With some cells set impassable, those are omitted and the order of the rest is unchanged. **Pass:** exact match.

### AC5 — Distance
`Distance` is 0 for the same cell, 1 for each neighbour, symmetric, and equals the shortest path length found by a breadth-first search over `NeighbourAt` for every pair on a 10×8 grid. **Pass:** all pairs match.

### AC6 — Direction helpers
`DirectionFromAngle(30°) == 0`, `(90°) == 1`, `(210°) == 3`, `(270°) == 4`, `(-30°) == 5`, `(30° + 360° - ε) == 0`, `(0°) == 0` (exactly between NE and SE; ties round up to the lower index), and a negative angle wraps correctly. `DirectionVector(d)` is a unit vector at on-screen angle `30° + d × 60°` in y-down space (so `DirectionVector(1)` is `(0, -1)`). **Pass:** all hold.

### AC7 — FindPath on the rectangular grid
On a 20×20 grid: a path from corner to corner succeeds with `cells.size() - 1 == Distance(from, to)` under flat cost; a wall of impassable cells across the full width makes the path fail; a gap lets it succeed through the gap; the path found is the same on repeated runs (determinism). **Pass:** all hold.

### AC8 — Heuristic is admissible
For both layouts, A* path cost equals the breadth-first shortest path cost on random obstacle maps (≥20 maps per layout, fixed seeds). **Pass:** equal on every map. This also covers the corrected axial formula.

### AC9 — Axial mode unchanged
The existing `TestPathfinding*`, `TestFlowFieldAdapters` and DiaGridVisibility tests pass without edits. **Pass:** zero regressions. Where an existing golden depended on the old axial heuristic and legitimately changes, the change is reviewed and noted in the plan, not silently regenerated.

### AC11 — Cell geometry
`CellFromPoint(CellCenter(c, size), size) == c` for every cell of a 12×9 grid and several `size` values. A point nudged 20% of `size` toward each corner of a cell still maps to that cell; a point just beyond the shared edge maps to the neighbour. For every cell and direction, normalised `CellCenter(NeighbourAt(c, d)) - CellCenter(c)` equals `DirectionVector(d)` (EXPECT_NEAR), and the distance between the two centres is `√3 × size`. **Pass:** all hold.

### AC12 — HexFlowAdapter on the rectangular layout
`HexFlowAdapter` constructed over `HexPathGrid(20, 15)` no longer asserts. Its `CellToWorldPosition` / `WorldToCell` agree with `HexTopology::CellCenter` / `CellFromPoint`, round-trip for every cell, and a flow field computed over the adapter with a goal in the middle leads every passable reachable cell strictly closer (in `Distance`) to the goal, and routes around an impassable wall. The axial adapter tests pass unedited. **Pass:** all hold. (Replaces the earlier "asserts on rectangular grids" death test.)

### AC13 — GridVisibility refuses hex grids
Instantiating `GridVisibilitySystem` with `HexPathGrid` is ill-formed with a clear message (checked with a `static_assert`-style compile test or a `requires`-based trait: `!std::is_constructible`-style check on the concept/trait, whichever the repo's test conventions support). The misleading comment claiming `HexPathGrid` satisfies the visibility concept is corrected. Square-grid visibility tests pass unedited. **Pass:** guard in place and tested; zero regressions.

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
| 1 | Should DiaGridVisibility support hex grids? | Needs a hex line-of-sight algorithm (its recursive octant shadowcasting assumes a square lattice). CoW needs fog of war only from V-2 | Not in this feature. Guard now (AC13); write a hex line-of-sight `/spec-feature` under DiaGridVisibility when a consumer is scheduled |
| 2 | One class with two layouts, or split into `HexPathGrid` (axial) and `OffsetHexPathGrid`? | One class keeps `FindPath`'s `is_same_v<HexPathGrid>` dispatch and the existing consumers working; two classes is cleaner but breaks that dispatch | One class (user decision 2026-10-14: a single hex graph in Dia). Retiring the axial mode later would simplify it |
| 3 | Should `NeighbourAt` use a precomputed table? | 4–7k cells × 6 directions is under 200 KB; parity arithmetic is already cheap | Compute from column parity now; add a table only if a CoW profile shows it matters |

## Status

`Done` — 2026-10-14. Amended 2026-10-14: flat-top odd-q (was pointy-top odd-r), cell geometry, `HexFlowAdapter` support, GridVisibility guard (plan tasks 6–8). Full googletest suite: 9427 pass.

**Plan:** @docs/specs/applications/dia/systems/diapathfinding/hex-rectangular-grid.plan.md
