# System Spec: DiaPathfinding

## Parent Application
@docs/specs/applications/dia/dia.md

## Purpose

DiaPathfinding is the grid-based pathfinding system for the Dia engine. It provides A* pathfinding over concept-constrained graph types — `SquarePathGrid` (4/8-connected) and `HexPathGrid` (6-connected axial) — with an injected cost provider interface so terrain costs, influence maps, and unit-type modifiers can be applied without coupling pathfinding to any specific data source.

The system ships both a synchronous `FindPath()` for immediate single-unit queries and an asynchronous `RequestPath()` for time-sliced multi-unit use. Cost injection and graph topology are decoupled: `IPathCostProvider` is a virtual interface (called once per edge — vtable cost negligible); graph neighbour enumeration is a concept-constrained template (called O(nodes) times — inlined, zero overhead).

The design follows the pattern established by Godot's `AStarGrid2D`: pathfinding owns its own grid types, cost is injectable, output is cell coordinates with a world-position helper.

**Dependency chain:**
`DiaPathfinding → DiaCore (containers, StringCRC, Observer)`

## Responsibilities

- Define `CPathGraph` concept — requires `GetNeighbours(CellCoord, DynamicArrayC<CellCoord>&)` and `IsPassable(CellCoord)` on the graph type
- Provide `SquarePathGrid` — rectangular grid, configurable 4-connected or 8-connected neighbour mode, passability per cell
- Provide `HexPathGrid` — hexagonal grid using axial coordinates, 6-connected neighbours, passability per cell
- Provide `IPathCostProvider` — virtual interface with `GetCost(CellCoord from, CellCoord to)` returning `float`; default `FlatCostProvider` returns `1.0f` (ships with DiaPathfinding, no external dependency)
- Provide `FindPath<TGraph>(graph, from, to, costProvider)` — synchronous A* returning `PathResult`; blocks until complete
- Provide `PathResult` — `bool success`, `float totalCost`, `DynamicArrayC<CellCoord> cells`, `ToWorldPositions(float cellSize, DynamicArrayC<Vector2>&)` helper
- Provide `PathfindingSystem` — owns an async request queue; `RequestPath(PathRequest)` submits a request; system processes one or more requests per `Update(float budget)` tick (time-sliced by budget in milliseconds); result delivered via `IPathResultObserver`
- Provide `IPathResultObserver` — `OnPathFound(PathRequestId, PathResult)` and `OnPathFailed(PathRequestId)` callbacks
- Provide `PathRequest` — `PathRequestId id`, graph reference, `CellCoord from/to`, `IPathCostProvider&`; returns `PathRequestId` for cancellation
- Support `CancelRequest(PathRequestId)` on `PathfindingSystem`
- Emit `DIA_LOG_INFO` on path request submission, completion, and failure — keyed by `PathRequestId`
- Provide test utilities under `DiaPathfinding/Testing/`: `AssertPathFound`, `AssertPathCells`, `MockCostProvider` — shipped with the library, consumer opt-in via include
- Identify grid cells via `CellCoord` (int row, int col for square; int q, int r for hex axial)
- Use DiaCore containers exclusively in all public APIs (AD-002 / PD-004)
- Provide `dia.pathfinding.architecture.module.md` YAML module documentation
- Provide `DiaPathfinding.vcxproj` static library registered in `Cluiche.sln` under `domain/gameplay/core`

## Non-Responsibilities

- Terrain cost data storage — `IPathCostProvider` is injected; terrain data lives in C26 (DiaTerrainCost) or caller-owned structures
- Flow fields — Tier 3 (C8), separate future system
- Hierarchical pathfinding (HPA*) — Tier 4 (C11), separate future system
- Steering / local movement — pathfinding provides waypoints; local movement is caller's concern (C9 Steering)
- NavMesh pathfinding — out of scope; grid only in v1
- Path smoothing / string pulling — caller's responsibility; `ToWorldPositions()` provides raw waypoints
- Thread safety within `PathfindingSystem` — single-threaded; `RequestPath()` / `Update()` called from SimPU
- Visual debugger overlay — deferred to a future `DiaPathfindingVisualDebugger` system

## Public Interfaces

### CPathGraph Concept

```cpp
namespace Dia::Pathfinding {
    template<typename T>
    concept CPathGraph = requires(const T& g, CellCoord c,
                                   Dia::Core::DynamicArrayC<CellCoord>& out) {
        { g.GetNeighbours(c, out) } -> std::same_as<void>;
        { g.IsPassable(c) }         -> std::convertible_to<bool>;
    };
}
```

### CellCoord

```cpp
namespace Dia::Pathfinding {
    // Square grid: row/col integers.
    // Hex grid: axial q/r coordinates.
    // Same struct — interpretation depends on graph type.
    struct CellCoord {
        int x;  // col (square) or q (hex axial)
        int y;  // row (square) or r (hex axial)

        bool operator==(const CellCoord&) const = default;
    };
}
```

### SquarePathGrid

```cpp
namespace Dia::Pathfinding {
    enum class SquareConnectivity { k4Connected, k8Connected };

    class SquarePathGrid {
    public:
        SquarePathGrid(int width, int height,
                       SquareConnectivity connectivity = SquareConnectivity::k8Connected);

        void SetPassable(CellCoord cell, bool passable);
        bool IsPassable(CellCoord cell) const;

        void GetNeighbours(CellCoord cell,
                           Dia::Core::DynamicArrayC<CellCoord>& outNeighbours) const;

        int GetWidth() const;
        int GetHeight() const;
    };
}
```

### HexPathGrid

```cpp
namespace Dia::Pathfinding {
    class HexPathGrid {
    public:
        // radius: distance from centre to edge in cells
        explicit HexPathGrid(int radius);

        void SetPassable(CellCoord cell, bool passable);
        bool IsPassable(CellCoord cell) const;

        void GetNeighbours(CellCoord cell,
                           Dia::Core::DynamicArrayC<CellCoord>& outNeighbours) const;

        int GetRadius() const;
    };
}
```

### IPathCostProvider

```cpp
namespace Dia::Pathfinding {
    class IPathCostProvider {
    public:
        virtual ~IPathCostProvider() = default;
        // Return the cost to move from 'from' to 'to' (adjacent cells).
        // Return kImpassableCost to mark the edge as blocked.
        virtual float GetCost(CellCoord from, CellCoord to) const = 0;

        static constexpr float kImpassableCost = -1.0f;
    };

    // Default: uniform cost 1.0f for all edges.
    class FlatCostProvider : public IPathCostProvider {
    public:
        float GetCost(CellCoord from, CellCoord to) const override { return 1.0f; }
    };
}
```

### PathResult

```cpp
namespace Dia::Pathfinding {
    struct PathResult {
        bool  success    = false;
        float totalCost  = 0.0f;
        Dia::Core::DynamicArrayC<CellCoord> cells;  // from → to inclusive; empty on failure

        // Convert cell coords to world-space positions.
        // cellSize: world units per cell edge.
        void ToWorldPositions(float cellSize,
                              Dia::Core::DynamicArrayC<Dia::Maths::Vector2>& outPositions) const;
    };
}
```

### FindPath (synchronous)

```cpp
namespace Dia::Pathfinding {
    template<CPathGraph TGraph>
    PathResult FindPath(const TGraph&       graph,
                        CellCoord           from,
                        CellCoord           to,
                        IPathCostProvider&  costs);
}
```

### PathfindingSystem (asynchronous)

```cpp
namespace Dia::Pathfinding {
    using PathRequestId = Dia::Core::StringCRC;

    struct PathRequest {
        PathRequestId       id;
        CellCoord           from;
        CellCoord           to;
        IPathCostProvider*  costs;      // non-owning; must outlive the request
    };

    class IPathResultObserver {
    public:
        virtual ~IPathResultObserver() = default;
        virtual void OnPathFound(PathRequestId id, const PathResult& result) = 0;
        virtual void OnPathFailed(PathRequestId id) = 0;
    };

    // Graph type must be provided at PathfindingSystem construction.
    // One PathfindingSystem per graph type (use two instances for square + hex).
    template<CPathGraph TGraph>
    class PathfindingSystem {
    public:
        explicit PathfindingSystem(const TGraph& graph);

        // Submit an async request; observer called on completion.
        PathRequestId RequestPath(const PathRequest& request,
                                  IPathResultObserver& observer);

        // Cancel a pending request (no-op if already complete).
        void CancelRequest(PathRequestId id);

        // Process pending requests up to budgetMs milliseconds.
        // Call once per SimPU tick from the owning Module.
        void Update(float budgetMs);

        // Inspection
        int GetPendingCount() const;
    };
}
```

### Log Channel

```cpp
namespace Dia::Pathfinding {
    static constexpr Dia::Core::StringCRC kLogChannel{"Pathfinding"};
    // DIA_LOG_INFO on request submit, completion, failure.
}
```

## Features

| Feature | Description | Spec | Status |
|---------|-------------|------|--------|
| CPathGraph Concept | C++20 concept constraining graph types to `GetNeighbours` + `IsPassable`. Zero-overhead static polymorphism — no vtable on the hot path. | inline | Draft |
| SquarePathGrid | Rectangular grid, 4 or 8-connected, per-cell passability. Satisfies `CPathGraph`. | inline | Draft |
| HexPathGrid | Hexagonal grid (axial coordinates), 6-connected, per-cell passability. Satisfies `CPathGraph`. | inline | Draft |
| IPathCostProvider | Virtual cost injection interface. `FlatCostProvider` (1.0f uniform) ships as default. `kImpassableCost` sentinel blocks edges. | inline | Draft |
| Synchronous FindPath | `FindPath<TGraph>()` — A* returning `PathResult` immediately. No allocation beyond output array. | inline | Draft |
| PathResult | `success` + `totalCost` + `DynamicArrayC<CellCoord>` + `ToWorldPositions()` helper. | inline | Draft |
| Asynchronous PathfindingSystem | `RequestPath()` queues request; `Update(budgetMs)` time-slices work; `IPathResultObserver` delivers result. `CancelRequest()` drops pending work. | inline | Draft |
| Lifecycle Logging | `DIA_LOG_INFO` on request submit, path found, and path failed. | inline | Draft |
| Test Utilities | `DiaPathfinding/Testing/` — `AssertPathFound`, `AssertPathCells`, `MockCostProvider`. Ships with library; consumer opt-in via include. | inline | Draft |

## Dependencies on Other Systems

**Required:**
- **DiaCore** — `DynamicArrayC` (path cell storage, neighbour lists, observer list), `StringCRC` (`PathRequestId`, log channel), `Observer` / `ObserverSubject` (path result callbacks), `DIA_ASSERT`, `DIA_LOG_*`
- **DiaMaths** — `Vector2` (used in `ToWorldPositions()` output only)

**Explicitly excluded:**
- **DiaGeometry2D** — pathfinding owns its own grid types; no dependency on `DiaGeometry2D::Grid`; avoids coupling and keeps the module self-contained
- **DiaBlackboard** — callers read blackboard slots to select from/to coords and choose cost provider; pathfinding itself has no blackboard dependency
- **DiaCommand** — path results are typically fed into a `MoveCommand`; that wiring lives in game code, not in DiaPathfinding
- **DiaApplicationFlow** — `PathfindingSystem::Update()` is called from a Module but has no compile-time dependency on the phase system
- **DiaStreams** — result delivery goes through `IPathResultObserver`; stream overhead not justified for what is a per-request callback

**Dependents (future):**
- `DiaTerrainCost` (C26) — implements `IPathCostProvider` backed by terrain data
- `DiaInfluenceMaps` (C13) — may wrap `IPathCostProvider` to bias paths away from threats
- `DiaSteeringBehaviours` (C9) — consumes path waypoints for local movement
- `DiaRallyPoint` (C24) — issues path requests to navigate units to waypoints
- Game code — wraps `PathResult` into a `MoveCommand` for the `DiaCommand` queue

## Out of Scope

- Flow fields — Tier 3 (C8); future `DiaFlowField` system
- HPA* / hierarchical pathfinding — Tier 4 (C11)
- Steering / local avoidance — C9, separate concern
- NavMesh pathfinding
- Path smoothing / string pulling
- Thread-safe `PathfindingSystem` — single-threaded; async = time-sliced on SimPU, not multi-threaded
- Visual debugger overlay — future `DiaPathfindingVisualDebugger`

## Decisions

| ID | Decision | Rationale | Scope | Status | Binding |
|----|----------|-----------|-------|--------|---------|
| PD-001 | `CPathGraph` is a C++20 concept, not a virtual base | `GetNeighbours` is called O(nodes × branching_factor) times per search — vtable dispatch here is measurable overhead on large grids. Concepts inline the call; compiler generates a specialised `FindPath` per graph type. Same flexibility as a virtual interface, zero runtime cost. | CPathGraph Concept, FindPath | Accepted | Yes |
| PD-002 | `IPathCostProvider` is a virtual interface | Cost lookup is called once per edge, not per node expansion. Vtable overhead here is negligible. Virtual dispatch allows terrain cost, influence maps, and unit-type modifiers to be swapped or composed at runtime without recompiling pathfinding code. | IPathCostProvider | Accepted | Yes |
| PD-003 | DiaPathfinding owns `SquarePathGrid` and `HexPathGrid` — no dependency on DiaGeometry2D::Grid | Pathfinding grids carry pathfinding-specific data (passability, connectivity mode). Reusing a geometry grid would couple pathfinding to DiaGeometry2D and force geometry primitives into the pathfinding hot path. Follows Godot `AStarGrid2D` precedent. | SquarePathGrid, HexPathGrid | Accepted | Yes |
| PD-004 | Path result is `DynamicArrayC<CellCoord>` with a `ToWorldPositions()` helper | Cell coords are the natural output of grid A*. World-space conversion requires a cell size parameter — coupling it into the search would bake a spatial assumption into the algorithm. `ToWorldPositions()` is a post-process helper; callers that don't need world positions pay no cost. | PathResult | Accepted | Yes |
| PD-005 | Async `PathfindingSystem` is time-sliced via `Update(budgetMs)`, not multi-threaded | SimPU runs game logic single-threaded (per CLAUDE.md: game logic on SimPU). Time-slicing on SimPU avoids thread synchronisation complexity. Budget parameter lets the owning module tune latency vs frame cost. True background threading deferred until scale demands it. | PathfindingSystem | Accepted | Yes |
| PD-006 | A* only in v1; JPS deferred | JPS is a grid-specific optimisation (5–10× faster on uniform-cost open grids). Both algorithms use the same `CPathGraph` + `IPathCostProvider` interface — JPS can be added as a second `FindPath` overload later without any API change. Avoids premature optimisation. | Synchronous FindPath | Accepted | Yes |
| PD-007 | One `PathfindingSystem` instance per graph type | `PathfindingSystem<TGraph>` is templated; mixing graph types in a single queue would require type erasure and lose the zero-overhead concept benefit. Game code creates separate systems for square and hex maps. | PathfindingSystem | Accepted | Yes |
| PD-008 | `kImpassableCost = -1.0f` sentinel on `IPathCostProvider` | Cost providers signal impassable edges via a sentinel rather than a separate `IsPassable(from, to)` query. One call per edge instead of two. Cost < 0 is unambiguously invalid in a non-negative cost graph. | IPathCostProvider | Accepted | Yes |
| PD-009 | Test utilities ship inside `DiaPathfinding/Testing/` | Platform-wide pattern (DiaStateMachine SD-017, DiaBlackboard BD-008, DiaCommand CD-008). | Test Utilities | Accepted | Yes |

**Status values:** `Proposed` · `Accepted` · `Rejected` · `Superseded`
**Binding:** `Yes` = enforced constraint on all features in this system · `No` = guidance only

## Inherited Binding Decisions

| ID | Source | Decision | Implication for this system |
|----|--------|----------|----------------------------|
| PD-001 | Platform | StringCRC for all entity/component IDs | `PathRequestId` is a `StringCRC`. Log channel key is `StringCRC`. |
| PD-002 | Platform | ProcessingUnit/Phase/Module architecture | `PathfindingSystem::Update()` is called from a Module on SimPU. No compile-time dependency on DiaApplicationFlow. |
| PD-004 | Platform | No STL containers in public APIs | `PathResult::cells`, neighbour lists, and observer lists use `DiaCore::DynamicArrayC`. Internal open/closed sets may use STL. |
| PD-005 | Platform | x64 only | `DiaPathfinding.vcxproj` targets x64 exclusively. |
| PD-006 | Platform | Visual Studio project files are source of truth | `DiaPathfinding.vcxproj` and `.vcxproj.filters` created and maintained manually. |
| PD-007 | Platform | C++20 required | Compiled under `/std:c++20`. `CPathGraph` concept requires C++20. |
| PD-008 | Platform | Directory.Build.props owns OutDir/IntDir/toolchain | `DiaPathfinding.vcxproj` must NOT override `OutDir`, `IntDir`, `PlatformToolset`, `WindowsTargetPlatformVersion`, or `LanguageStandard`. |
| PD-009 | Platform | Generated output under Cluiche/out/ | Any log output goes under `Cluiche/out/<AppName>/`. |
| AD-001 | Dia App | Module system with YAML frontmatter | Create `dia.pathfinding.architecture.module.md` with public API, responsibilities, and dependency declarations. |
| AD-002 | Dia App | No STL containers in public APIs | Reinforces PD-004. Internal A* open/closed sets may use STL priority_queue. |
| AD-003 | Dia App | Namespace `Dia::<Module>::` | All code in `Dia::Pathfinding::` namespace. |

## Resolved Design Questions

1. **Heuristic selection** — Fixed heuristics per graph type: Manhattan for 4-connected, Chebyshev for 8-connected, cube-distance for hex. Selected automatically inside `FindPath<TGraph>` via type dispatch. Rationale: correct for all standard cases; injectable heuristics deferred until a concrete non-standard use case exists (PD-006 anti-premature-optimisation).

2. **`PathfindingSystem` graph ownership** — `PathfindingSystem<TGraph>` holds `const TGraph&`; the caller is responsible for not mutating the grid while requests are pending. Rationale: single-threaded SimPU use makes this trivially manageable; copying the grid would be expensive for large grids and is not needed in v1.

## Status

`Approved`

**Plan:** @docs/specs/applications/dia/systems/diapathfinding/diapathfinding.plan.md
