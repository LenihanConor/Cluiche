# System Spec: DiaFlowField

## Parent Application
@docs/specs/applications/dia/dia.md

**Gameplay Domains:** pathfinding, ai

## Purpose

DiaFlowField is the vector-field-based navigation system for mass-unit movement in the Dia engine. Where DiaPathfinding solves optimal routes for individual entities, DiaFlowField solves for groups: given a goal cell, it computes a per-cell vector field (one Dijkstra sweep over the entire grid) that every unit in the group can follow by simply reading their current cell's direction vector. The cost is O(grid cells) once, not O(units × path length) repeatedly.

This is the correct navigation primitive for 100+ units moving toward a shared target — dragons converging on a base, armies marching to a rally point, enemies flooding toward the player. The flow field replaces per-unit A* entirely for shared-destination movement.

Flow fields reuse DiaPathfinding's grid types (`SquarePathGrid`, `HexPathGrid`) and cost interface (`IPathCostProvider`) directly — no new grid abstraction is needed. DiaFlowField depends on DiaPathfinding and extends its navigation stack.

**Dependency chain:**
`DiaFlowField → DiaPathfinding → DiaCore (containers, StringCRC, Observer), DiaMaths (Vector2)`

## Responsibilities

- Define `CFlowFieldGraph` concept — extends `CPathGraph` with `CellToWorldPosition(CellCoord, float cellSize) → Vector2` and `WorldToCell(Vector2, float cellSize) → CellCoord`; satisfied by `SquarePathGrid` and `HexPathGrid` via adapters or extension
- Provide `FlowField` — a grid-sized flat array of `FlowCell` (direction `Vector2`, `bool reachable`); `Sample(CellCoord) → Vector2` returns the normalised direction toward goal, or `{0,0}` if unreachable
- Provide `ComputeFlowField<TGraph>(graph, goalCell, costProvider) → FlowField` — synchronous Dijkstra sweep from goal outward; returns complete field; blocks until done
- Provide `FlowFieldCache` — named field store keyed by `StringCRC goalKey`; `GetOrCompute(goalKey, goalCell, graph, costProvider)` returns a cached `FlowField const*` or computes and stores one; `Invalidate(goalKey)` marks a specific field dirty; `InvalidateAll()` marks all fields dirty; dirty fields recompute on the next `GetOrCompute` call
- Provide `FlowFieldCache::SetGraph` and `SetCostProvider` for late binding — caller provides graph and cost provider once at cache init; all fields use the same graph topology and cost source
- Support both `SquarePathGrid` and `HexPathGrid` as graph types from day one
- Emit `DIA_LOG_INFO` on field compute start, completion (cells visited, ms elapsed), and invalidation
- Provide test utilities under `DiaFlowField/Testing/`: `AssertCellDirection`, `AssertReachable`, `AssertUnreachable`, `MockFlowFieldGraph` — shipped with the library, consumer opt-in via include
- Use DiaCore containers exclusively in all public APIs (AD-002 / PD-004)
- Provide `dia.flowfield.architecture.module.md` YAML module documentation
- Provide `DiaFlowField.vcxproj` static library registered in `Cluiche.sln` under `domain/gameplay/core`

## Non-Responsibilities

- Per-unit pathfinding — use DiaPathfinding for single-unit optimal routes
- Local collision avoidance between units following the same field — that is DiaSteering (C9) and RVO/ORCA (C10)
- Hierarchical flow fields for very large maps — deferred; standard Dijkstra is sufficient until scale demands it (C11 HPA*)
- Terrain cost data storage — `IPathCostProvider` is injected; terrain data lives in caller-owned structures or a future `DiaTerrainCost` system
- Path smoothing — flow vectors are grid-aligned directions; smoothing is caller's responsibility
- Visual debugger overlay — deferred to a future `DiaFlowFieldVisualDebugger` system
- Thread safety within `FlowFieldCache` — single-threaded; called from SimPU

## Public Interfaces

### CFlowFieldGraph Concept

```cpp
namespace Dia::FlowField {
    // Extends CPathGraph (GetNeighbours + IsPassable) with world-space helpers.
    // Both SquarePathGrid and HexPathGrid satisfy this via thin adapter wrappers.
    template<typename T>
    concept CFlowFieldGraph = Dia::Pathfinding::CPathGraph<T>
        && requires(const T& g,
                    Dia::Pathfinding::CellCoord c,
                    float cellSize,
                    Dia::Maths::Vector2 worldPos) {
        { g.CellToWorldPosition(c, cellSize) } -> std::convertible_to<Dia::Maths::Vector2>;
        { g.WorldToCell(worldPos, cellSize)  } -> std::convertible_to<Dia::Pathfinding::CellCoord>;
    };
}
```

### FlowCell + FlowField

```cpp
namespace Dia::FlowField {
    struct FlowCell {
        Dia::Maths::Vector2 direction;  // normalised; {0,0} if unreachable
        bool reachable = false;
    };

    class FlowField {
    public:
        // Sample the flow direction at a cell.
        // Returns {0,0} for cells that cannot reach the goal.
        const FlowCell& Sample(Dia::Pathfinding::CellCoord cell) const;

        // Convenience: sample direction from world position (requires cellSize).
        Dia::Maths::Vector2 SampleWorld(Dia::Maths::Vector2 worldPos, float cellSize) const;

        // True if every passable cell in the field can reach the goal.
        bool IsComplete() const;

        int GetCellCount() const;
    };
}
```

### ComputeFlowField (synchronous)

```cpp
namespace Dia::FlowField {
    // Runs a Dijkstra sweep from goalCell outward across the graph.
    // All passable cells receive a direction pointing toward the lowest-cost path to goal.
    // Impassable cells and cells unreachable from goal have reachable = false.
    // Synchronous: blocks until the full grid is processed.
    template<CFlowFieldGraph TGraph>
    FlowField ComputeFlowField(const TGraph&                    graph,
                               Dia::Pathfinding::CellCoord      goalCell,
                               Dia::Pathfinding::IPathCostProvider& costs);
}
```

### FlowFieldCache

```cpp
namespace Dia::FlowField {
    using FlowFieldKey = Dia::Core::StringCRC;

    // Caches multiple named flow fields over a shared graph.
    // One cache per graph instance; all fields share the same topology and cost source.
    template<CFlowFieldGraph TGraph>
    class FlowFieldCache {
    public:
        FlowFieldCache(const TGraph& graph,
                       Dia::Pathfinding::IPathCostProvider& costs);

        // Returns existing field if clean; recomputes if dirty or absent.
        const FlowField& GetOrCompute(FlowFieldKey key,
                                      Dia::Pathfinding::CellCoord goalCell);

        // Mark a specific field dirty — recomputed on next GetOrCompute.
        void Invalidate(FlowFieldKey key);

        // Mark all fields dirty — use when a broad terrain change affects many cells.
        void InvalidateAll();

        // Mark all fields dirty whose goal cell falls within [topLeft, bottomRight] inclusive,
        // OR whose field contains any cell in that region (conservative: invalidates if the
        // goal is inside the changed region; callers should also call this when an obstacle
        // change could affect routes passing through the region).
        void InvalidateRegion(Dia::Pathfinding::CellCoord topLeft,
                              Dia::Pathfinding::CellCoord bottomRight);

        int GetCachedCount() const;
    };
}
```

### Log Channel

```cpp
namespace Dia::FlowField {
    static constexpr Dia::Core::StringCRC kLogChannel{"FlowField"};
    // DIA_LOG_INFO on compute start, completion (cells + ms), and invalidation.
}
```

## Features

| Feature | Description | Spec | Status |
|---------|-------------|------|--------|
| CFlowFieldGraph Concept | C++20 concept extending `CPathGraph` with world-space helpers. Zero-overhead static polymorphism on the hot path. | inline | Approved |
| FlowField | Per-cell `FlowCell` array with `Sample(CellCoord)` and `SampleWorld(worldPos, cellSize)`. Stores direction + reachability. | inline | Approved |
| ComputeFlowField (sync) | Dijkstra sweep from goal outward. Returns complete `FlowField`. Synchronous — caller time-slices if needed. | inline | Approved |
| FlowFieldCache | Named field store. `GetOrCompute` returns cached or recomputes. `Invalidate` / `InvalidateAll` / `InvalidateRegion(topLeft, bottomRight)` for dynamic obstacle support. | inline | Approved |
| Square + Hex support | Both `SquarePathGrid` and `HexPathGrid` satisfy `CFlowFieldGraph` via thin adapters. | inline | Approved |
| Lifecycle Logging | `DIA_LOG_INFO` on compute start, completion (cells visited, ms elapsed), and invalidation. | inline | Approved |
| Test Utilities | `DiaFlowField/Testing/` — `AssertCellDirection`, `AssertReachable`, `AssertUnreachable`, `MockFlowFieldGraph`. Ships with library; consumer opt-in via include. | inline | Approved |

## Dependencies on Other Systems

**Required:**
- **DiaPathfinding** — `CPathGraph` concept, `CellCoord`, `IPathCostProvider`, `SquarePathGrid`, `HexPathGrid`; DiaFlowField builds on and extends the pathfinding stack, not beside it
- **DiaCore** — `DynamicArrayC` (field cell storage), `StringCRC` (`FlowFieldKey`, log channel), `DIA_ASSERT`, `DIA_LOG_*`
- **DiaMaths** — `Vector2` (direction vectors, world-position helpers)

**Explicitly excluded:**
- **DiaGeometry2D** — grid topology is owned by DiaPathfinding; no dependency added here (PD-003 mirror)
- **DiaBlackboard** — callers read blackboard slots to pick goal cells and inject into cache; flow field itself has no blackboard dependency
- **DiaCommand / DiaOrder** — path results fed into orders in game code, not in DiaFlowField
- **DiaApplicationFlow** — `FlowFieldCache::GetOrCompute` is called from a Module but has no compile-time dependency on the phase system
- **DiaStreams** — field-ready notification is synchronous return; stream overhead not justified

**Dependents (future):**
- `DiaSteering` (C9) — units follow the sampled flow vector; steering handles local separation
- `DiaRVO` (C10) — dense crowd avoidance layer on top of steering output
- Game code — reads `FlowField::SampleWorld()` from the movement update of each unit entity
- `DiaFlowFieldVisualDebugger` — future arrow overlay on the debug canvas

## Out of Scope

- Per-unit A* — DiaPathfinding handles single-unit optimal routes
- Local unit separation / collision avoidance — DiaSteering (C9) and RVO (C10)
- Hierarchical flow fields — not needed until maps exceed 200×200+; HPA* (C11) is the unlock
- Terrain cost data storage — injected via `IPathCostProvider`; terrain layer is caller's concern
- Path smoothing / string pulling
- Thread-safe `FlowFieldCache` — single-threaded; called from SimPU
- Visual debugger overlay — future `DiaFlowFieldVisualDebugger`

## Decisions

| ID | Decision | Rationale | Scope | Status | Binding |
|----|----------|-----------|-------|--------|---------|
| FD-001 | `CFlowFieldGraph` extends `CPathGraph` — DiaFlowField depends on DiaPathfinding | Flow fields are a layer above grid pathfinding, not beside it. Reusing `CPathGraph`, `CellCoord`, and `IPathCostProvider` avoids duplicating grid topology concepts. Dependency direction is clear: Pathfinding → FlowField → Steering → RVO. | CFlowFieldGraph, module dependency | Accepted | Yes |
| FD-002 | `IPathCostProvider` is reused from DiaPathfinding — no new cost interface | A new cost interface would be equivalent to `IPathCostProvider` and adds no value. Callers already implement `IPathCostProvider` for A*; their concrete implementations work for flow fields without change. | ComputeFlowField | Accepted | Yes |
| FD-003 | `ComputeFlowField` is synchronous only; no async variant in v1 | Flow fields are computed infrequently (goal changes, obstacle updates) relative to per-unit pathfinding requests. A single Dijkstra sweep on a reasonable grid (e.g. 100×100 = 10k cells) is fast enough to absorb in one frame. If a map is large enough to need async, HPA* (C11) is the right answer, not time-slicing the sweep. | ComputeFlowField | Accepted | Yes |
| FD-004 | `FlowFieldCache` owns invalidation — dirty flag + recompute on `GetOrCompute`; region variant for spatial obstacle changes | Callers notify `Invalidate(key)` or `InvalidateRegion(topLeft, bottomRight)` when obstacles change; the cache recomputes lazily on the next access. `InvalidateRegion` marks all cached fields dirty whose goal falls within the changed rectangle — conservative but correct. Avoids eager recomputation when multiple obstacles change in the same frame. | FlowFieldCache | Accepted | Yes |
| FD-005 | Cache key is `StringCRC` (`FlowFieldKey`) | Consistent with `PathRequestId` and all engine IDs (PD-001). Human-readable string at authoring time, zero-cost comparison at runtime. | FlowFieldCache | Accepted | Yes |
| FD-006 | Direction vectors stored as normalised `Vector2` | Units follow the direction by scaling by their speed each frame — normalised input is the natural form. Computing normalisation at query time would add cost for every unit every frame. Stored normalised once at compute time. | FlowCell | Accepted | Yes |
| FD-007 | Test utilities ship inside `DiaFlowField/Testing/` | Platform-wide pattern (DiaPathfinding PD-009, DiaStateMachine SD-017, DiaBlackboard BD-008). | Test Utilities | Accepted | Yes |

**Status values:** `Proposed` · `Accepted` · `Rejected` · `Superseded`
**Binding:** `Yes` = enforced constraint on all features in this system · `No` = guidance only

## Inherited Binding Decisions

| ID | Source | Decision | Implication for this system |
|----|--------|----------|----------------------------|
| PD-001 | Platform | StringCRC for all entity/component IDs | `FlowFieldKey` is `StringCRC`. Log channel key is `StringCRC`. |
| PD-002 | Platform | ProcessingUnit/Phase/Module architecture | `FlowFieldCache::GetOrCompute` called from a Module on SimPU. No compile-time dependency on DiaApplicationFlow. |
| PD-004 | Platform | No STL containers in public APIs | `FlowField` cell storage and all public arrays use `DiaCore::DynamicArrayC`. Internal Dijkstra priority queue may use STL. |
| PD-005 | Platform | x64 only | `DiaFlowField.vcxproj` targets x64 exclusively. |
| PD-006 | Platform | Visual Studio project files are source of truth | `DiaFlowField.vcxproj` and `.vcxproj.filters` created and maintained manually. |
| PD-007 | Platform | C++20 required | Compiled under `/std:c++20`. `CFlowFieldGraph` concept requires C++20. |
| PD-008 | Platform | Directory.Build.props owns OutDir/IntDir/toolchain | `DiaFlowField.vcxproj` must NOT override `OutDir`, `IntDir`, `PlatformToolset`, `WindowsTargetPlatformVersion`, or `LanguageStandard`. |
| PD-009 | Platform | Generated output under Cluiche/out/ | Any log output goes under `Cluiche/out/<AppName>/`. |
| AD-001 | Dia App | Module system with YAML frontmatter | Create `dia.flowfield.architecture.module.md` with public API, responsibilities, and dependency declarations. |
| AD-002 | Dia App | No STL containers in public APIs | Reinforces PD-004. Internal Dijkstra open set may use STL `priority_queue`. |
| AD-003 | Dia App | Namespace `Dia::<Module>::` | All code in `Dia::FlowField::` namespace. |

## Resolved Design Questions

1. **`CFlowFieldGraph` world-space helpers** — `SquarePathGrid` and `HexPathGrid` satisfy `CPathGraph` and will not be modified. DiaFlowField ships thin adapter wrappers `SquareFlowAdapter` and `HexFlowAdapter` that compose the existing grids and add `CellToWorldPosition` / `WorldToCell`. DiaPathfinding spec stays Done and untouched; adapters live in `DiaFlowField/`.

2. **Cache invalidation granularity** — `InvalidateRegion(topLeft, bottomRight)` is in scope. It conservatively marks all cached fields dirty whose goal cell falls within the changed rectangle. Callers that change obstacles outside a field's goal cell must call `InvalidateRegion` covering the affected area; the cache makes no attempt to determine which fields are actually affected by the route change (that would require re-running the Dijkstra to know).

## Status

**Status:** `Done`

**Plan:** @docs/specs/applications/dia/systems/diaflowfield/diaflowfield.plan.md
