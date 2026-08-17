# System Spec: DiaGridVisibility

## Parent Application
@docs/specs/applications/dia/dia.md

**Gameplay Domains:** ai, gameplay

## Purpose

DiaGridVisibility is the per-cell visibility system for the Dia engine. It maintains a grid of visibility states (Unexplored / Revealed / Visible) per `VisibilityGroupId`, updated each tick from registered observer entities and their sight radii. Line-of-sight (LOS) raycasting against terrain blockers is applied during the update pass so that walls and elevation correctly limit what a group can see.

The system answers three questions game and AI code routinely ask:
- "Can this group currently see this entity?" (`CanSee`)
- "What is the visibility state of this cell for this group?" (`GetCellState`)
- "Which entities are currently visible to this group?" (`GetVisibleEntities`)

Grid types are reused directly from DiaPathfinding (`SquarePathGrid`, `HexPathGrid`, `CellCoord`, `CPathGraph`) — the terrain/pathfinding grid IS the visibility grid. No new grid abstraction is introduced.

**Dependency chain:**
`DiaGridVisibility → DiaPathfinding → DiaCore, DiaMaths`
`DiaGridVisibility → DiaEntitySpatial → DiaGeometry2D → DiaCore, DiaMaths`

LOS uses shadowcasting (recursive octant algorithm, grid-native) — no direct DiaGeometry2D dependency. DiaGeometry2D is a transitive dependency via DiaEntitySpatial only.

## Responsibilities

- Define `VisibilityGroupId` as `Dia::Core::StringCRC` — any number of groups; each is an opaque key the game assigns meaning to (team, faction, player, squad)
- Define `VisibilityState` enum: `Unexplored` / `Revealed` / `Visible`
- Provide `GridVisibilitySystem<TGraph>` (templated on `CPathGraph`) that:
  - At construction, runs a consolidation pass to build a coarser visibility grid from the pathfinding grid at a caller-specified `chunkSize` (N pathfinding cells per visibility cell side); a passable visibility cell requires at least one passable pathfinding cell within its chunk
  - Owns per-group, per-cell state storage on the coarser visibility grid
  - Accepts sight-source registration: entity → (groupId, sightRadius in world units)
  - On `Update(cellSize, spatial)`: for each **dirty** sight source (moved to a new cell or radius changed), runs a shadowcasting sweep (recursive octant algorithm) against the graph's impassable cells to recompute that source's contributed visibility; merges results into the per-group cell grid; advances cells no longer covered by any source from Visible to Revealed
  - On `Update()`: rebuilds the per-group visible-entity list via DiaEntitySpatial; fires `IVisibilityChangeObserver` callbacks for any cell or entity whose state changed this tick; clears dirty flags
  - Exposes `CanSee(entity, groupId) → bool`, `GetCellState(cell, groupId) → VisibilityState`, `GetVisibleEntities(groupId) → span<Entity>`
- Provide `IVisibilityChangeObserver` interface and `AddChangeListener` / `RemoveChangeListener` for cell-state and entity-visibility change notifications (DiaCore Observer pattern)
- Provide test utilities under `DiaGridVisibility/Testing/`
- Provide `dia.gridvisibility.architecture.module.md` YAML module documentation
- Provide `DiaGridVisibility.vcxproj` static library registered in `Cluiche.sln`

## Non-Responsibilities

- DiaStreams-based events — change notifications use the DiaCore Observer pattern instead
- Fog-of-war render data / per-cell opacity for the renderer — game or rendering layer owns this
- Per-entity cone perception — that is DiaSensor (already Done)
- AI decision logic consuming visibility — game code / DiaUtilityAI scorers read the queries
- Thread-safe concurrent mutation — single-threaded; called from SimPU
- Minimap data layer — downstream consumer; reads `GetCellState` directly
- Visual debugger overlay — deferred to backlog (`DiaGridVisibilityVisualDebugger`)

## Public Interfaces

### Types

```cpp
namespace Dia::GridVisibility {

    using VisibilityGroupId = Dia::Core::StringCRC;

    enum class VisibilityState : uint8_t {
        Unexplored = 0,   // cell has never been seen by this group
        Revealed   = 1,   // cell was seen previously but is not currently visible
        Visible    = 2    // cell is within unblocked sight of at least one group observer
    };
}
```

### IVisibilityChangeObserver

```cpp
namespace Dia::GridVisibility {

    class IVisibilityChangeObserver {
    public:
        virtual ~IVisibilityChangeObserver() = default;

        // Called when a cell's state changes for a group (e.g. Unexplored→Visible, Visible→Revealed).
        virtual void OnCellStateChanged(Dia::Pathfinding::CellCoord cell,
                                        VisibilityGroupId groupId,
                                        VisibilityState oldState,
                                        VisibilityState newState) {}

        // Called when an entity transitions into or out of visibility for a group.
        virtual void OnEntityVisibilityChanged(Dia::Entity::Entity entity,
                                               VisibilityGroupId groupId,
                                               bool isNowVisible) {}
    };
}
```

### GridVisibilitySystem

```cpp
namespace Dia::GridVisibility {

    template<Dia::Pathfinding::CPathGraph TGraph>
    class GridVisibilitySystem {
    public:
        // chunkSize: number of pathfinding cells per visibility cell side (1 = full resolution).
        // A chunkSize of 4 reduces a 256×256 path grid to a 64×64 visibility grid.
        explicit GridVisibilitySystem(const TGraph& graph, int chunkSize = 1);

        // Register entity as a sight source contributing to groupId's merged visibility.
        // Re-registration updates sightRadius. Must be called from SimPU.
        void RegisterSightSource(Dia::Entity::Entity entity,
                                 VisibilityGroupId groupId,
                                 float sightRadius);
        void UnregisterSightSource(Dia::Entity::Entity entity);

        // Subscribe / unsubscribe to cell and entity visibility change notifications.
        // Callbacks fire during Update() for any state that changed this tick.
        void AddChangeListener(IVisibilityChangeObserver* listener);
        void RemoveChangeListener(IVisibilityChangeObserver* listener);

        // Advance all group visibility grids one tick.
        // cellSize: world units per grid cell (must match the graph that was provided).
        // spatial: used to locate observer world positions and to build visible-entity lists.
        // Must be called once per sim tick from SimPU before any CanSee / GetVisibleEntities query.
        void Update(float cellSize,
                    const Dia::EntitySpatial::EntitySpatialModule& spatial);

        // Can groupId currently see targetEntity?
        // Returns true iff targetEntity's current cell is Visible to groupId.
        // O(1) — result is precomputed during Update().
        bool CanSee(Dia::Entity::Entity targetEntity,
                    VisibilityGroupId groupId) const;

        // Raw per-cell visibility state for a group. O(1) lookup.
        VisibilityState GetCellState(Dia::Pathfinding::CellCoord cell,
                                     VisibilityGroupId groupId) const;

        // All entities whose current cell is Visible to groupId.
        // Span is valid until the next Update() call.
        std::span<const Dia::Entity::Entity>
            GetVisibleEntities(VisibilityGroupId groupId) const;

        int GetGroupCount() const;
    };
}
```

### Log Channel

```cpp
namespace Dia::GridVisibility {
    static constexpr Dia::Core::StringCRC kLogChannel{"GridVisibility"};
    // DIA_LOG_INFO on Update() (groups updated, cells marked visible, observers active).
}
```

## Features

| Feature | Description | Spec | Status |
|---------|-------------|------|--------|
| VisibilityGroupId + VisibilityState | Type foundation — opaque `StringCRC` group key; tri-state enum (Unexplored/Revealed/Visible) | inline | Draft |
| GridVisibilitySystem | Observer registration + per-group state grid ownership; templated on `CPathGraph` | inline | Draft |
| Update pass | Per-tick: only re-sweep **dirty** sight sources (moved cell or radius changed); shadowcasting (recursive octant) against impassable cells; merge into per-group grid; decay cells no longer covered to Revealed; clear dirty flags | inline | Draft |
| CanSee query | `CanSee(entity, groupId) → bool` — O(1) precomputed cell-visibility lookup | inline | Draft |
| GetCellState query | `GetCellState(cell, groupId) → VisibilityState` — O(1) direct grid lookup | inline | Draft |
| GetVisibleEntities query | `GetVisibleEntities(groupId) → span<Entity>` — prebuilt during Update() | inline | Draft |
| Change Notifications | `IVisibilityChangeObserver` — `OnCellStateChanged` + `OnEntityVisibilityChanged`; fired during Update() via DiaCore Observer pattern | inline | Draft |
| Test Utilities | `DiaGridVisibility/Testing/`: `MockVisibilityGraph`, `AssertCellVisible`, `AssertCellRevealed`, `AssertCellUnexplored`, `AssertCanSee`, `AssertCannotSee` | inline | Draft |

## Dependencies on Other Systems

**Required:**
- **DiaPathfinding** — `CPathGraph` concept, `CellCoord`, `SquarePathGrid`, `HexPathGrid`; the terrain grid is the visibility grid — no new grid abstraction
- **DiaEntitySpatial** — observer world-position lookup and visible-entity list construction during Update()
- **DiaCore** — `DynamicArrayC` (internal storage), `StringCRC` (`VisibilityGroupId`, log channel), `Observer`/`ObserverSubject` (change notification), `DIA_ASSERT`, `DIA_LOG_*`
- **DiaMaths** — `Vector2` (world positions, raycast direction)
- **diaentitytemplate** — `Entity` handle type

**Explicitly excluded:**
- **DiaStreams** — change notifications use Observer pattern (synchronous, in-process); no stream dependency
- **DiaSensor** — per-entity cone perception; independent system; may query DiaGridVisibility in future but not a compile-time dependency
- **DiaBlackboard** — callers read visibility queries and write to blackboard themselves
- **DiaApplicationFlow** — GridVisibilitySystem::Update called from a Module but no compile-time dependency on the phase system

**Dependents (future):**
- Game code — `CanSee` in combat resolution, AI targeting, stealth checks
- `DiaUtilityAI` scorers — `GetCellState` as a consideration input
- Minimap — `GetCellState` per cell for colour rendering
- `DiaGridVisibilityVisualDebugger` — per-cell colour overlay (backlog)
- `DiaSensor` — may delegate LOS check to `CanSee` in a future enhancement

## Out of Scope

- Visibility-change events — no UnitSpotted / UnitLost stream in v1
- Render-layer fog mask or per-cell opacity data
- Per-entity cone perception — DiaSensor
- Hierarchical visibility for very large maps
- Thread-safe concurrent Update + query

## Decisions

| ID | Decision | Rationale | Scope | Status | Binding |
|----|----------|-----------|-------|--------|---------|
| GVD-001 | `VisibilityGroupId` is `StringCRC` | Consistent with PD-001 (StringCRC for all IDs). Human-readable at authoring time, zero-cost comparison at runtime. The game assigns meaning (faction / team / player). | All APIs | Accepted | Yes |
| GVD-002 | Grid reuses `DiaPathfinding` types — `CPathGraph`, `CellCoord`, `SquarePathGrid`, `HexPathGrid` | Same grid, same blocker data. Avoids a duplicate grid abstraction. Same pattern as DiaFlowField (FD-001). | GridVisibilitySystem | Accepted | Yes |
| GVD-003 | `CanSee` uses precomputed cell-visibility state — O(1) at query time | Visibility is computed during Update(); `CanSee` is a direct cell-state lookup. Cell-granularity precision is appropriate for strategic FOW and AI queries. A precise sub-cell overload is deferred. | CanSee | Accepted | Yes |
| GVD-004 | Change notifications use DiaCore Observer pattern — not DiaStreams | `IVisibilityChangeObserver` is synchronous, zero-allocation, and requires no stream dependency. Consistent with DiaBlackboard, DiaEntitySpawner, and other gameplay systems. Callers needing cross-PU fan-out relay via DiaMessageBus in game code. | Change Notifications | Accepted | Yes |
| GVD-005 | Update is synchronous — called once per sim tick | Shadowcasting on a coarse grid is fast; dirty-flag gating keeps per-tick cost proportional to movement. Async deferred until scale demands it. | Update | Accepted | Yes |
| GVD-006 | Test utilities ship in `DiaGridVisibility/Testing/` | Platform-wide pattern (DiaPathfinding, DiaFlowField, DiaStateMachine, DiaBlackboard). | Test Utilities | Accepted | Yes |
| GVD-007 | LOS algorithm is recursive octant shadowcasting — not raycasting | Shadowcasting visits each visible cell exactly once → O(visible cells) vs O(radius²) for raycasting. Grid-native: no DiaGeometry2D dependency for the LOS pass. More accurate on grids (raycasting misses thin walls; shadowcasting does not). Standard algorithm for this problem class. | Update pass | Accepted | Yes |
| GVD-008 | Dirty-flag update: only re-sweep sight sources that moved or changed radius | Standing-still observers cost zero per tick. A sight source is marked dirty on `RegisterSightSource` and when its tracked cell position changes between ticks. The merge pass (Visible→Revealed decay) still runs every tick for all groups, but the shadowcasting sweep only runs for dirty sources. | Update pass | Accepted | Yes |
| GVD-009 | Visibility grid is built at a caller-specified `chunkSize` — coarser than the pathfinding grid | A `chunkSize` of N folds N×N pathfinding cells into one visibility cell (a 256×256 path grid at chunkSize=4 → 64×64 visibility grid). Reduces memory and shadowcasting cost proportionally. A visibility cell is passable if any pathfinding cell within its chunk is passable. `chunkSize=1` gives full resolution. | GridVisibilitySystem | Accepted | Yes |

## Inherited Binding Decisions

| ID | Source | Decision | Implication for this system |
|----|--------|----------|----------------------------|
| PD-001 | Platform | StringCRC for all entity/component IDs | `VisibilityGroupId` is `StringCRC`. Log channel key is `StringCRC`. |
| PD-002 | Platform | ProcessingUnit/Phase/Module architecture | `GridVisibilitySystem::Update` called from a Module on SimPU. No compile-time dependency on DiaApplicationFlow. |
| PD-004 | Platform | No STL containers in public APIs | Internal state storage uses `DynamicArrayC`. `GetVisibleEntities` returns `std::span` (a view, not a container — consistent with C++20 usage in other specs). |
| PD-005 | Platform | x64 only | `DiaGridVisibility.vcxproj` targets x64 exclusively. |
| PD-006 | Platform | Visual Studio project files are source of truth | `DiaGridVisibility.vcxproj` and `.vcxproj.filters` created and maintained manually. |
| PD-007 | Platform | C++20 required | Compiled under `/std:c++20`. Template concept constraint on `CPathGraph` requires C++20. |
| PD-008 | Platform | Directory.Build.props owns OutDir/IntDir/toolchain | `DiaGridVisibility.vcxproj` must NOT override `OutDir`, `IntDir`, `PlatformToolset`, `WindowsTargetPlatformVersion`, or `LanguageStandard`. |
| PD-009 | Platform | Generated output under Cluiche/out/ | Log output goes under `Cluiche/out/<AppName>/`. |
| AD-001 | Dia App | Module system with YAML frontmatter | Create `dia.gridvisibility.architecture.module.md` with public API, responsibilities, and dependency declarations. |
| AD-002 | Dia App | No STL containers in public APIs | Reinforces PD-004. Internal arrays use `DynamicArrayC`. |
| AD-003 | Dia App | Namespace `Dia::<Module>::` | All code in `Dia::GridVisibility::` namespace. |

## Resolved Design Questions

1. **Grid resolution** — `chunkSize` parameter at construction (GVD-009). Caller controls coarseness; `chunkSize=1` for full resolution, larger values for cheaper FOW on big maps. In scope for v1.

2. **Update cadence** — dirty-flag update in v1 (GVD-008). Sight sources mark themselves dirty on move or radius change; shadowcasting only runs for dirty sources that tick.

3. **LOS algorithm** — recursive octant shadowcasting (GVD-007). O(visible cells), grid-native, no DiaGeometry2D dependency. A future `CanSeePrecise(entityA, entityB)` world-space raycast overload remains deferred.

## Status

**Status:** `Done`
