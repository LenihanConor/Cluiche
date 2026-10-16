# Feature Spec: Hex Grid Line of Sight

**Parent:** @docs/specs/applications/dia/systems/diagridvisibility/diagridvisibility.md

**Gameplay Domains:** ai, gameplay

## Summary

`GridVisibilitySystem` currently refuses `HexPathGrid` at compile time because its octant shadowcasting algorithm assumes square lattice geometry. This feature adds hex-specific line-of-sight computation so `GridVisibilitySystem<HexPathGrid>` compiles and produces correct visibility for both axial hexagon and rectangular hex grids. CoW needs this for V-2 fog of war on hex maps.

## Problem

DiaGridVisibility's `CVisibilityGraph` concept explicitly excludes `HexPathGrid` via `!std::is_same_v<T, Dia::Pathfinding::HexPathGrid>` because the existing recursive octant shadowcasting algorithm (8 octants, `TransformOctant` mapping) assumes square neighbors and square symmetry. Hexes have 6 directions, different neighbor offsets per column parity (flat-top odd-q), and different distance metrics. Applying the square algorithm to hex grids produces incorrect visibility.

Without hex support:
- CoW cannot implement fog of war (V-2 testbed blocked)
- Any hex-grid game must reimplement visibility outside the engine
- DiaGridVisibility's value proposition (reuse pathfinding grid as visibility grid, GVD-002) breaks for hex

## Goals

1. **Remove the HexPathGrid exclusion** — `GridVisibilitySystem<HexPathGrid>` compiles successfully
2. **Correct hex line-of-sight** — blockers cast shadows in the correct hex directions; all 6 neighbors are visible from origin in open terrain
3. **Support both hex layouts** — axial hexagon (`HexLayout::kAxialHexagon`) and rectangular offset (`HexLayout::kOffsetRectangle`)
4. **No square grid regression** — existing square shadowcasting tests pass unchanged
5. **Hex-aware consolidation** — topology-specific clustering (7-cell or 19-cell hex clusters for hex grids, rectangular chunks for square grids); dispatch on grid type
6. **Test coverage** — GoogleTest suite covers basic hex visibility (6-neighbor open, single blocker shadow, circular sight radius, hex-aware consolidation)

## Non-Goals

1. **Matching square algorithm complexity** — O(radius²) for hex ray-sweep is acceptable (simpler than O(visible cells) recursive shadowcasting)
2. **Unified square+hex algorithm** — separate implementations per topology are fine
3. **Per-hex-direction sight radius** — uniform radius only (same as square)
4. **Precise sub-cell line-of-sight** — cell-granularity only (deferred `CanSeePrecise` overload applies to both square and hex)

## Architecture

### Algorithm Selection

Dispatch to topology-specific implementations based on grid type:

```cpp
template<CVisibilityGraph TGraph>
void GridVisibilitySystem<TGraph>::ComputeShadow(SightSourceEntry& source,
                                                  CellCoord origin,
                                                  float sightRadiusVisCells,
                                                  int groupIndex)
{
    if constexpr (std::is_same_v<TGraph, Dia::Pathfinding::HexPathGrid>)
    {
        ComputeShadowHex(source, origin, sightRadiusVisCells, groupIndex);
    }
    else
    {
        ComputeShadowSquare(source, origin, sightRadiusVisCells, groupIndex);
    }
}
```

### Hex Algorithm

**Bresenham ray sweep** (chosen for simplicity):
- Cast 6 rays, one per hex direction (NE, N, NW, SW, S, SE)
- March along each ray using hex line-drawing (step from cell to neighbor in direction)
- Mark each cell visible until hitting a blocker (impassable cell)
- Blockers themselves are visible; cells beyond them in that direction are not
- O(radius) per ray × 6 rays = O(radius) per direction sweep
- Total per source: O(radius²) in worst case (all 6 directions reach full radius)
- For typical sight radii (5-10 cells): 25-100 cell checks per dirty source

### Hex Direction Mapping

`HexTopology` already defines the 6 directions (0=NE, 1=N, 2=NW, 3=SW, 4=S, 5=SE) and provides:
- `NeighbourAt(cell, dir)` — neighbor in direction `dir`
- `DirectionVector(dir)` — unit vector for direction `dir`
- `Distance(a, b)` — hex distance (cube-coordinate based)

The hex algorithm uses these primitives; no direction re-derivation needed.

### Chunk Consolidation

Consolidation adapts to topology:

**Square grids:** Rectangular `chunkSize × chunkSize` regions (existing behavior). A visibility cell is passable if ANY pathfinding cell in its chunk is passable.

**Hex grids:** Hex-aware super-cells. Options:
- **7-cell hex cluster:** center pathfinding cell + its 6 neighbors = 1 visibility cell (analogous to `chunkSize=3` in square grids)
- **19-cell hex cluster:** center + ring-1 (6 cells) + ring-2 (12 cells) = 1 visibility cell (analogous to `chunkSize=5`)

The consolidation pass dispatches on `graph.GetLayout()` (for `HexPathGrid`) or grid type (via `if constexpr`) to choose the pattern. Passability rule remains: a visibility cell is passable if ANY pathfinding cell in its cluster/chunk is passable.

**Rationale:** Hex geometry has natural ring-based clustering (hex flowers). Rectangular chunks break hex symmetry and would require irregular cell counts per chunk at boundaries.

### Concept Update

Remove the HexPathGrid exclusion:

```cpp
template<typename T>
concept CVisibilityGraph = Dia::Pathfinding::CPathGraph<T>
    && requires(const T& g) {
    { g.GetWidth()  } -> std::convertible_to<int>;
    { g.GetHeight() } -> std::convertible_to<int>;
};
```

`HexPathGrid` satisfies this — both layouts (`kAxialHexagon` and `kOffsetRectangle`) expose `GetWidth()` and `GetHeight()`.

### Layout Detection

The hex algorithm queries `mGraph.GetLayout()` to distinguish axial vs rectangular hex:

- **Axial hexagon:** use axial cube-coordinate logic for neighbors
- **Rectangular offset:** use `HexTopology::NeighbourAt` and column-parity offsets

Both share the same 6-direction sweep structure; only the coordinate math differs.

## Dependencies

- **DiaPathfinding** — `HexPathGrid`, `HexTopology`, `HexLayout` enum (all exist, built 2026-10-14)
- **DiaCore, DiaMaths, DiaEntitySpatial** — unchanged from square visibility

No new module dependencies.

## Acceptance Criteria

Tests under `Cluiche/Tests/GoogleTests/DiaGridVisibility/`. New public behavior follows TDD.

### AC1 — HexPathGrid compiles
Construct `GridVisibilitySystem<HexPathGrid>` with an axial grid (radius 5) and a rectangular grid (20×15). **Pass:** both compile, construction succeeds, `GetVisWidth()` and `GetVisHeight()` match expected consolidated dimensions.

### AC2 — Basic hex visibility (rectangular layout)
Rectangular 10×8 hex grid, `chunkSize=1`, one sight source at center with radius = 3 cells. All cells passable. **Pass:** the source sees itself plus all cells within hex distance 3 (19 cells total for radius 3 in flat-top hex). All 6 immediate neighbors are Visible. Cells beyond radius 3 are Unexplored.

### AC3 — Hex blocker shadow (rectangular layout)
10×8 rectangular grid, sight source at (2, 4), radius = 4 cells. Set cell (4, 4) impassable (directly NE of source, direction 0). **Pass:** cell (4, 4) is Visible (blockers are visible). Cells in the NE wedge beyond (4, 4) that would be blocked by the shadow are NOT Visible. The 5 other directions (N, NW, SW, S, SE) have full visibility up to radius 4.

### AC4 — Circular sight radius on hex grid
Rectangular 16×12 grid, sight source at (8, 6), radius = 5.0 cells. All passable. **Pass:** cells with `HexTopology::Distance(source, cell) <= 5` are Visible; cells with distance > 5 are Unexplored. The visible region approximates a circle in hex-distance metric.

### AC5 — Hex-aware consolidation
Rectangular 42×24 hex grid with 7-cell cluster consolidation (center + 6 neighbors = 1 visibility cell). Approximately 6×4 visibility grid. Sight source at a visibility cell containing pathfinding cell (12, 12), radius = 2 vis-cells. Set some pathfinding cells within one visibility cluster impassable such that the cluster has a mix (consolidation rule: passable if ANY path-cell in the cluster is passable). **Pass:** visibility sweep runs on the consolidated hex grid. The mixed cluster is treated as passable. Ray sweep respects hex topology (6 directions).

### AC6 — Square grid unchanged
Run the existing `TestGridVisibilitySystem.cpp` square-grid test suite (basic visibility, shadows, multiple groups, dirty-flag update). **Pass:** all tests pass without edits. Square shadowcasting behavior is identical to before this feature.

### AC7 — Axial hex layout
Axial hexagon grid (radius 8), `chunkSize=1`, sight source at origin (0, 0), radius = 3 cells. **Pass:** all cells within axial distance 3 are Visible (37 cells for radius 3 in axial layout). Axial `GetNeighbours` order and `Distance` metric apply.

## Performance

Hex ray-sweep is **O(radius²)** per dirty source (vs square's O(visible cells)). For typical sight radii (5-10 cells), this translates to 25-100 cell checks per source. The simplicity of the ray-sweep implementation (6 rays, Bresenham line march) has lower constant factors than recursive shadowcasting, so O(radius²) is acceptable.

**Benchmark:** 64×64 rectangular hex grid, 16 sight sources (spread across grid), radius = 8 cells each, all passable. Measure `Update()` time with all sources dirty. Target: ≤ 2ms on a typical dev machine. If slower, document the multiplier and confirm it's acceptable for CoW's use case (V-2 fog of war at 80×52 map = 4,160 cells, typical sight radius 5-8 cells).

## Open Design Questions

1. **Which hex field-of-view algorithm?**
   - **Option A:** Hex-adapted recursive shadowcasting (6 sextants) — O(visible cells), complex
   - **Option B:** Bresenham ray sweep (6 directions) — O(radius²), simple
   - **Option C:** Recursive hex wedge subdivision (angle-based) — O(visible cells), moderate complexity
   
   **DECISION (2026-10-15):** **Option B — Bresenham ray sweep.** Simplest implementation. Cast rays at 60° intervals (6 hex directions), march along each ray marking cells visible until hitting a blocker. O(radius²) complexity is acceptable per Q3.

2. **Chunk consolidation: keep rectangular or adapt to hex geometry?**
   - **Keep rectangular:** Simple, topology-agnostic, matches square grid pattern. Hex algorithm works on consolidated grid.
   - **Adapt to hex:** Hex-aware consolidation (e.g., hex "super-cells"). More complex, unclear benefit.
   
   **DECISION (2026-10-15):** **Adapt to hex geometry.** Consolidation should be dynamic to the topology shape. Use hex-aware super-cells for hex grids (e.g., 7-cell hex clusters: center + 6 neighbors). Square grids keep rectangular chunks. The consolidation logic dispatches on grid type like the shadowcasting algorithm does.

3. **Performance target: O(visible cells) required or O(radius²) acceptable?**
   - Square shadowcasting is O(visible cells) per dirty source. Hex ray-sweep (Option B) is O(radius²).
   - For typical sight radii (5-10 cells), O(radius²) may be faster in practice than complex O(visible cells) recursion due to lower constant factors.
   
   **DECISION (2026-10-15):** **O(radius²) is acceptable.** The simplicity of Option B (ray sweep) outweighs the higher complexity class. For CoW's typical sight radii (5-10 cells), O(radius²) = 25-100 cell checks per source, well within budget.

## Binding Decisions

### From Platform (PD-*)

- **PD-001 (StringCRC for IDs):** Not applicable — this feature adds an algorithm, no new ID types.
- **PD-004 (No STL in public APIs):** Complies — no API changes; internal algorithm uses existing `DynamicArrayC` containers.
- **PD-007 (C++20 required):** Complies — uses `if constexpr` for topology dispatch, requires C++20.

### From Dia Application (AD-*)

- **AD-001 (Module YAML frontmatter):** Not applicable — `dia.gridvisibility.architecture.module.md` exists; this feature updates the module, not creates a new one.
- **AD-003 (Namespace Dia::<Module>::):** Complies — all code stays in `Dia::GridVisibility::` namespace.

### From DiaGridVisibility System (GVD-*)

- **GVD-001 (VisibilityGroupId is StringCRC):** Not applicable — no ID changes.
- **GVD-002 (Reuse DiaPathfinding grid types):** **Complies** — this feature enables the reuse for `HexPathGrid`; removes the exclusion that broke GVD-002 for hex grids.
- **GVD-003 (CanSee is O(1) precomputed):** Complies — no change to query APIs.
- **GVD-004 (Observer pattern, not DiaStreams):** Not applicable — no event changes.
- **GVD-005 (Synchronous Update):** Complies — hex algorithm runs during `Update()` like square.
- **GVD-006 (Test utilities in Testing/):** Complies — new test helpers (if any) go in `DiaGridVisibility/Testing/`.
- **GVD-007 (LOS algorithm is recursive octant shadowcasting):** **CONFLICT — hex requires a different algorithm.** The decision says "recursive octant shadowcasting — not raycasting" for square grids. Hex has 6 directions, not 8 octants, so a literal octant algorithm cannot work. This feature uses Bresenham ray sweep for hex grids (6 rays, one per hex direction) — simpler than recursive shadowcasting, O(radius²) instead of O(visible cells), but acceptable per Open Question 3.
  
  **Resolution:** Amend GVD-007 to read: *"LOS algorithm is topology-specific: recursive octant shadowcasting for square grids (O(visible cells)), Bresenham ray sweep for hex grids (O(radius²)). Both are grid-native with no DiaGeometry2D dependency. O(radius²) is acceptable for hex given the simplicity gain and typical sight radii (5-10 cells)."*

- **GVD-008 (Dirty-flag update):** Complies — hex sources mark dirty the same way square sources do.
- **GVD-009 (Chunk-based consolidation):** **Extends** — consolidation is now topology-aware. Square grids use rectangular `chunkSize × chunkSize` chunks (existing behavior). Hex grids use hex-aware clustering (7-cell or 19-cell hex flowers). Both follow the same passability rule: a visibility cell is passable if ANY pathfinding cell in its cluster/chunk is passable. The spirit of GVD-009 (coarser visibility grid for performance) is preserved; the implementation adapts to topology.

## Status

**Status:** `Approved`

**Date:** 2026-10-15

**Related Research:** None — hex grid (`HexPathGrid`, `HexTopology`) was built in the rectangular hex grid feature (hex-rectangular-grid.md, Done 2026-10-14). This feature builds on that work.

**Tasks** (implementation plan created when spec is Approved):

1. Update `CVisibilityGraph` concept (remove `!std::is_same_v<T, HexPathGrid>` exclusion)
2. Add hex-aware consolidation logic (7-cell or 19-cell hex clusters, dispatch on grid type)
3. Implement `ComputeShadowHex` using Bresenham ray sweep (6 rays, one per hex direction)
4. Add layout dispatch in `ComputeShadowHex` (axial vs rectangular, via `graph.GetLayout()`)
5. Write GoogleTest coverage (AC1-AC7: compiles, basic visibility, blocker shadow, circular radius, chunk consolidation, square unchanged, axial layout)
6. Benchmark hex vs square performance (64×64 hex grid, 16 sources, radius=8; document results)
7. Update module docs (`dia.gridvisibility.architecture.module.md` — add hex support note)
8. Update parent system spec features table (already done)
