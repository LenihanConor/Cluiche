# System Spec: DiaPathfindingVisualDebugger

## Parent Application
@docs/specs/applications/dia/dia.md

**Gameplay Domains:** navigation

## Purpose

DiaPathfindingVisualDebugger is the `IDebugDomain` implementation that makes pathfinding state visible in `DiaDebugPanel`. It is a world-space domain (`HasWorldDrawers() = true`) with two drawers: path polyline (waypoints + start/goal markers) and grid passability overlay (passable/impassable cells as coloured quads).

Following the `DiaXxxVisualDebugger` contract, `DiaPathfinding` has zero compile-time dependency on `DiaPathfindingVisualDebugger`.

## Responsibilities

- **Prerequisite — confirm `PathGrid` debug access**: `DiaPathfinding` must expose a `DIA_DEBUG` accessor for per-cell passability iteration — see Prerequisite section
- Implement `IDebugDomain` — all 16-AC contract methods
- `HasWorldDrawers()` returns `true`; two drawers: `PathPolyline`, `GridPassability`
- `Register`/`Unregister` bulk-register both drawers with `DebugLayerManager`
- `GetJSONState()` emits drawer list and `stats`: path active flag, waypoint count, total cost
- `OnCommand("toggle", {drawer: name})` enables/disables the named drawer
- `OnCommand("setScale", {key: "markerRadius", value: float})` scales start/goal marker radii
- All world-space sizes multiplied by `IDebugContext::GetDebugScale()`
- Entire module guarded by `#ifdef DIA_DEBUG`
- `DiaPathfindingVisualDebugger.vcxproj` static library registered in `Cluiche.sln` under `3.1-Gameplay-Tools`
- `dia.diapathfindingvisualdebugger.architecture.module.md` YAML module documentation

## Non-Responsibilities

- Path computation — DiaPathfinding
- Grid construction or mutation
- Any runtime behaviour in Release — entire module excluded by `#ifdef DIA_DEBUG`

## Prerequisite: `PathGrid` and `PathResult` Debug Access

The debugger takes two const references supplied by the caller. No new types are needed, but confirm the following are accessible:

```cpp
// DiaPathfinding/PathGrid.h — expected existing API (DIA_DEBUG accessor if not present)
#ifdef DIA_DEBUG
// fn: void(CellCoord coord, bool passable)
template<typename Fn>
void VisitCells(Fn&& fn) const;

int GetWidth()  const;
int GetHeight() const;
float GetCellSize() const;
#endif
```

If `PathGrid` stores cells in a flat array, `VisitCells` is trivial. Add these three accessors if absent. They are `DIA_DEBUG` only.

`PathResult` (already has `cells` + `ToWorldPositions`) requires no changes.

## Public Interfaces

```cpp
// DiaPathfindingVisualDebugger/PathfindingVisualDebugger.h
#ifdef DIA_DEBUG

#include <DiaVisualDebugger/Domain/IDebugDomain.h>
#include <DiaPathfinding/PathResult.h>

namespace Dia::Pathfinding { class PathGrid; }

namespace Dia::Pathfinding
{
    class PathfindingVisualDebugger : public Dia::VisualDebugger::IDebugDomain
    {
    public:
        // grid and result are owned by the caller (e.g. a PathfinderComponent).
        // result may be a default-constructed (empty) PathResult when no path is active.
        PathfindingVisualDebugger(const PathGrid& grid, const PathResult& result);

        Dia::Core::StringCRC  GetDomainId()      const override; // "pathfinding"
        const char*           GetDisplayName()   const override; // "Pathfinding"
        const char*           GetDescription()   const override; // see below
        Dia::Core::StringCRC  GetGroup()          const override; // "Navigation"
        Dia::Core::ColourRGBA GetAccentColour()   const override; // DebugGroupAccents::kNavigation

        bool HasWorldDrawers() const override { return true; }

        void Register(Dia::Debug::DebugLayerManager& mgr)   override;
        void Unregister(Dia::Debug::DebugLayerManager& mgr) override;

        int  GetDrawerCount() const override { return 2; }
        Dia::Debug::IVisualDebugger* GetDrawer(int index) override;

        void GetJSONState(Dia::Core::JsonWriter& writer) override;
        void OnCommand(Dia::Core::StringCRC cmd, const Dia::Core::JsonValue& args) override;

    private:
        const PathGrid&   mGrid;
        const PathResult& mResult;
        PathPolylineDrawer    mPathPolyline;
        GridPassabilityDrawer mGridPassability;
    };
}
#endif // DIA_DEBUG
```

`GetDescription()` returns: `"Pathfinding — path polyline, start/goal markers, grid passability"` (64 chars ≤ 80 ✓)

### JSON State Schema

```json
{
  "drawers": [
    { "name": "PathPolyline",    "enabled": true },
    { "name": "GridPassability", "enabled": true }
  ],
  "stats": {
    "pathActive":    true,
    "waypointCount": 7,
    "totalCost":     24.5
  }
}
```

`pathActive` is `false` and `waypointCount`/`totalCost` are `0` when `PathResult.success == false` or `cells` is empty.

### World-Space Draw Specification

| Drawer | Primitive | Colour key | Size rule |
|--------|-----------|------------|-----------|
| PathPolyline — segment | Line | `kDebugPathSegment` | Fixed 1px |
| PathPolyline — start marker | Circle (filled) | `kDebugPathStart` | Radius = `basRadius × GetDebugScale()` |
| PathPolyline — goal marker | Circle (filled) | `kDebugPathGoal` | Radius = `baseRadius × GetDebugScale()` |
| GridPassability — passable | Quad (filled, translucent) | `kDebugGridPassable` | Cell size from `PathGrid::GetCellSize()` |
| GridPassability — impassable | Quad (filled, translucent) | `kDebugGridImpassable` | Cell size from `PathGrid::GetCellSize()` |

`PathPolyline` converts `PathResult.cells` to world positions via `ToWorldPositions(grid.GetCellSize(), positions)` then draws N-1 line segments connecting them. Start marker at `positions[0]`, goal marker at `positions[N-1]`.

### Panel Card Specification

- **Group:** Navigation — accent `DebugGroupAccents::kNavigation` (`#3b82f6`) via `var(--accent)`
- **Domain stat line:** `"Path: N waypoints"` or `"No path"` when inactive
- **Drawer toggles:** PathPolyline (on), GridPassability (on)

## Tests

`Tests/GoogleTests/DiaPathfindingVisualDebugger/TestPathfindingVisualDebugger.cpp`

### Mandatory shapes (AC-15)

| Shape | Description |
|-------|-------------|
| DrawerGate | Disabled `PathPolyline` emits zero primitives; re-enabling restores output |
| PrimitiveType | `PathPolyline` → line + circle primitives; `GridPassability` → quad primitives |
| ScaleSensitivity | Doubling `GetDebugScale()` doubles start/goal marker radii |
| JSONRoundTrip | `GetJSONState()` drawer entries match current enabled state |
| OnCommandRoundTrip | `OnCommand("toggle", {drawer:"PathPolyline"})` toggles; second call reverts |

### Domain-specific shapes

| Shape | Description |
|-------|-------------|
| `NWaypoints_NMinusOneSegments` | 5-waypoint path → 4 line segment primitives from `PathPolyline` |
| `StartMarker_AtFirstWaypoint` | Start circle primitive position equals first waypoint world position |
| `GoalMarker_AtLastWaypoint` | Goal circle primitive position equals last waypoint world position |
| `StartGoalColoursDiffer` | Start marker colour ≠ goal marker colour |
| `GridPassability_CellsMatchGrid` | `GridPassability` emits one quad per grid cell (passable + impassable combined) |
| `GridPassability_PassableImpassableColoursDiffer` | Passable cell colour ≠ impassable cell colour |
| `DrawerGate_GridPassability` | Disabled `GridPassability` emits zero quads |
| `Toggle_GridPassability` | `OnCommand("toggle", {drawer:"GridPassability"})` toggles it |
| `NoActivePath_NoSegments_NoAssert` | Empty `PathResult` → `PathPolyline` emits no segments or markers, no crash |
| `Stats_PathActive_False_WhenNoPath` | `stats.pathActive == false` and `waypointCount == 0` when path is empty |
| `Stats_WaypointCount_Accurate` | `stats.waypointCount` matches actual cell count in `PathResult.cells` |
| `Stats_TotalCost_Accurate` | `stats.totalCost` matches `PathResult.totalCost` |
| `MarkerRadius_Command` | `OnCommand("setScale", {key:"markerRadius", value:2.0})` doubles marker radii |

## Dependencies on Other Systems

**Required:**
- **DiaPathfinding** — `PathGrid`, `PathResult`, `CellCoord`, `VisitCells()` (prereq if absent)
- **DiaVisualDebugger** — `IDebugDomain`, `DebugGroupAccents`, `DebugColourPalette`, draw infra
- **DiaCore** — `StringCRC`, `ColourRGBA`, `JsonWriter`, `JsonValue`

**Explicitly excluded:**
- **ImGui**, **DiaEntity**, **DiaApplicationFlow**, **DiaAIBudget**, **DiaSteering**

## Contract Compliance

All 16 ACs from `debugger-contract.md` apply.

| AC | Notes |
|----|-------|
| AC-1 | Standalone `DiaPathfindingVisualDebugger.vcxproj` |
| AC-2 | `DiaPathfinding` has zero dep on `DiaPathfindingVisualDebugger` |
| AC-3 | Deps: DiaPathfinding + DiaVisualDebugger + DiaCore + draw infra only |
| AC-4 | Implements all `IDebugDomain` pure virtuals |
| AC-5 | `GetDescription()` = 64 chars ✓ |
| AC-6 | World-space colours from `DebugColourPalette` only |
| AC-7 | All sizes × `GetDebugScale()` — verified by ScaleSensitivity shape |
| AC-8 | No `ImGui::*` calls |
| AC-9 | `const PathGrid&` + `const PathResult&` — read-only references |
| AC-10 | Emits `drawers` + `stats` minimum |
| AC-11 | `OnCommand("toggle", {drawer})` handled for both drawers |
| AC-12 | `OnCommand("setScale", ...)` handled; drawer enables use `std::atomic<bool>` |
| AC-13/14 | `Tests/GoogleTests/DiaPathfindingVisualDebugger/TestPathfindingVisualDebugger.cpp` |
| AC-15 | All 5 mandatory shapes present — see Tests section |
| AC-16 | Panel card complies with standard spacing; accent via `var(--accent)` |

## Decisions

| ID | Decision | Rationale | Status | Binding |
|----|----------|-----------|--------|---------|
| SD-001 | Debugger accepts `const PathGrid&` + `const PathResult&` separately | Grid and path result have different lifetimes; keeping them separate avoids a dependency on a higher-level component type. Caller wires both from its `PathfinderComponent`. | Accepted | Yes |
| SD-002 | `PathPolyline` uses `ToWorldPositions()` for segment rendering | Avoids duplicating the cell-to-world transform; single source of truth in `PathResult`. | Accepted | Yes |
| SD-003 | `GridPassability` draws all cells (passable + impassable) | Showing only impassable cells hides the full extent of the navigable area. Both needed to answer "can it path here?" | Accepted | Yes |

## Inherited Binding Decisions

| ID | Source | Decision | Implication |
|----|--------|----------|-------------|
| PD-001 | Platform | StringCRC for all IDs | Domain ID, group, drawer names, command keys use `StringCRC` |
| PD-004 | Platform | No STL in public APIs | `DynamicArrayC` used; no `std::vector` return values |
| PD-005 | Platform | x64 only | vcxproj targets x64 |
| PD-006 | Platform | vcxproj is source of truth | `.vcxproj` + `.vcxproj.filters` maintained |
| PD-007 | Platform | C++20 | `/std:c++20` |
| PD-008 | Platform | Directory.Build.props owns toolchain | vcxproj does not override OutDir/IntDir/toolset |
| AD-001 | Dia App | Module YAML docs | `dia.diapathfindingvisualdebugger.architecture.module.md` required |
| AD-003 | Dia App | `Dia::<Module>::` namespace | All code in `Dia::Pathfinding::` namespace |

## Open Design Questions

1. **`VisitCells` prerequisite scope** — if `PathGrid` already has a way to iterate cells (e.g. public `cells` array member), skip adding `VisitCells` and read it directly. Confirm at implementation.

2. **Large grid performance** — a 64×64 grid is 4096 quads per frame. If `GridPassability` causes visible frame-time impact, add a cell-count stat to `GetJSONState()` and document the cap. Alternative: draw only the cells within a configurable radius of the path start/goal.

## Status

**Status:** Done
