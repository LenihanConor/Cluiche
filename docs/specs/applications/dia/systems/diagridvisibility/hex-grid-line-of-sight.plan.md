# Implementation Plan: Hex Grid Line of Sight

**Spec:** @docs/specs/applications/dia/systems/diagridvisibility/hex-grid-line-of-sight.md

**Status:** In Progress

**Started:** 2026-10-15

---

## Implementation Patterns

### Topology Dispatch Pattern
All topology-specific logic uses `if constexpr` to dispatch at compile time:
```cpp
if constexpr (std::is_same_v<TGraph, Dia::Pathfinding::HexPathGrid>) {
    // Hex-specific implementation
} else {
    // Square-specific implementation (existing)
}
```

### Hex Consolidation Pattern
Hex consolidation uses ring-based clustering:
- 7-cell cluster: center + ring-1 (6 neighbors)
- Each pathfinding cell belongs to exactly one visibility cell (the nearest cluster center)
- Cluster centers are spaced at regular intervals (e.g., every 3rd cell in a hex lattice pattern)
- Passability: visibility cell passable if ANY pathfinding cell in cluster is passable

### Ray Sweep Algorithm Pattern
```cpp
void ComputeShadowHex(SightSourceEntry& source, CellCoord origin, float radius, int groupIndex) {
    // 1. Mark origin visible
    MarkVisCell(origin, source, groupIndex);
    
    // 2. For each of 6 hex directions (0..5):
    for (int dir = 0; dir < 6; ++dir) {
        // 3. March along ray using HexTopology::NeighbourAt
        CellCoord current = origin;
        for (int step = 0; step < maxSteps; ++step) {
            uint32_t nextIdx = topology.NeighbourAt(ToIndex(current), dir);
            if (nextIdx == kNoCell) break;  // edge
            current = ToCoord(nextIdx);
            
            // 4. Check distance
            if (Distance(origin, current) > radius) break;
            
            // 5. Mark visible
            MarkVisCell(current, source, groupIndex);
            
            // 6. Stop if blocker
            if (!IsVisCellPassable(current)) break;
        }
    }
}
```

### Layout Detection Pattern
For HexPathGrid, query the layout at runtime:
```cpp
if (mGraph.GetLayout() == Dia::Pathfinding::HexLayout::kAxialHexagon) {
    // Axial layout logic
} else {
    // Rectangular layout logic
}
```

---

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Update CVisibilityGraph concept | Compile check | Done | haiku | Removed `!std::is_same_v<T, HexPathGrid>` exclusion; concept now allows hex grids |
| 2 | Add hex consolidation | AC1, AC5 | Done | sonnet | Constructor dispatches on grid type; axial grids use centered coordinates; rectangular use direct mapping; both layouts compile and construct successfully |
| 3 | Implement ComputeShadowHex (ray sweep) | AC2, AC3, AC4 | Done | sonnet | 6-ray Bresenham sweep implemented; dispatch from ComputeShadow via if constexpr; all shadowcasting tests pass |
| 4 | Add layout dispatch (axial vs rectangular) | AC7 | Done | sonnet | ComputeShadowHex queries GetLayout(); axial layout test passes |
| 5 | Write GoogleTests (AC1-AC7) | All ACs | Done | sonnet | TestHexGridVisibility.cpp: 8 tests covering all ACs; all pass; added to GoogleTests.vcxproj |
| 6 | Benchmark hex vs square | Performance target | Pending | haiku | 64×64 hex, 16 sources, radius=8; measure Update() time; document |
| 7 | Update module docs | — | Done | haiku | Updated summary and responsibilities to note hex support (both axial and rectangular layouts) |
| 8 | Verify square tests unchanged | AC6 | Done | haiku | 82 total tests pass (74 square + 8 hex); square behavior confirmed unchanged |

---

## Notes

- **TDD:** Tasks 2-4 should write tests first (RED), then implement (GREEN)
- **Verification:** Each task needs `dia run googletest --filter="HexGrid*"` with quoted output before marking Done
- **Square regression:** Task 8 gates final completion — existing tests must pass unchanged
