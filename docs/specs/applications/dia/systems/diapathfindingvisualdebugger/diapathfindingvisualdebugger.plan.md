**Spec:** @docs/specs/applications/dia/systems/diapathfindingvisualdebugger/diapathfindingvisualdebugger.md
**Status:** Todo

## API Decisions

- Debugger accepts `const PathGrid&` + `const PathResult&` separately (SD-001)
- `PathPolyline` uses `PathResult::ToWorldPositions()` for world coords (SD-002)
- `GridPassability` draws ALL cells — passable + impassable (SD-003)
- If `PathGrid` already exposes a cell iteration path (e.g. public `cells` member), `VisitCells` prereq may be skipped — verify at Task 1
- Drawer enables use `std::atomic<bool>`

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Confirm/add to `DiaPathfinding`: `PathGrid::GetWidth()`, `PathGrid::GetHeight()`, `PathGrid::GetCellSize()`, and `PathGrid::VisitCells(Fn&&)` under `#ifdef DIA_DEBUG`; if already exposed, task = confirm + document | Unit test: construct a 4×4 grid, call `VisitCells`, assert 16 callbacks; `GetWidth()`=4, `GetHeight()`=4 | Todo | sonnet | Skip `VisitCells` if grid cell array is already directly iterable |
| 2 | Scaffold `DiaPathfindingVisualDebugger` module: `PathfindingVisualDebugger.h/.cpp`, `PathPolylineDrawer.h/.cpp`, `GridPassabilityDrawer.h/.cpp`; vcxproj + filters; module YAML; sln under `3.1-Gameplay-Tools` | `dia check debugger-contract` reports PENDING (not ERROR); build succeeds | Todo | haiku | |
| 3 | Implement `PathfindingVisualDebugger` identity, `HasWorldDrawers()=true`, `GetDrawerCount()=2`, `Register/Unregister`, `GetJSONState()` emitting drawers + stats (pathActive, waypointCount, totalCost), `OnCommand("toggle",...)`, `OnCommand("setScale","markerRadius")` | AC-4,10,11,12 pass | Todo | sonnet | |
| 4 | Implement `PathPolylineDrawer::Draw()`: call `PathResult::ToWorldPositions(grid.GetCellSize(), positions)`; draw N-1 line segments with colour `kDebugPathSegment`; draw start marker (filled circle, `kDebugPathStart`) at positions[0]; draw goal marker (filled circle, `kDebugPathGoal`) at positions[N-1]; marker radii × `GetDebugScale()`; draw nothing when `PathResult.success == false` | 5-waypoint path → 4 line + 2 circle primitives; start and goal colours differ | Todo | sonnet | |
| 5 | Implement `GridPassabilityDrawer::Draw()`: call `PathGrid::VisitCells()`; per cell draw translucent quad sized `cellSize × GetDebugScale()`; passable cells use `kDebugGridPassable`, impassable use `kDebugGridImpassable` | 4×4 grid → 16 quads; passable/impassable colours differ | Todo | sonnet | |
| 6 | Write mandatory test shapes in `Tests/GoogleTests/DiaPathfindingVisualDebugger/TestPathfindingVisualDebugger.cpp`: DrawerGate, PrimitiveType, ScaleSensitivity, JSONRoundTrip, OnCommandRoundTrip | All 5 shapes pass | Todo | sonnet | |
| 7 | Write domain-specific test shapes: NWaypoints_NMinusOneSegments, StartMarker_AtFirstWaypoint, GoalMarker_AtLastWaypoint, StartGoalColoursDiffer, GridPassability_CellsMatchGrid, GridPassability_PassableImpassableColoursDiffer, DrawerGate_GridPassability, Toggle_GridPassability, NoActivePath_NoSegments_NoAssert, Stats_PathActive_False_WhenNoPath, Stats_WaypointCount_Accurate, Stats_TotalCost_Accurate, MarkerRadius_Command | All 13 shapes pass | Todo | sonnet | |
| 8 | `dia check debugger-contract` full pass | Exit code 0 | Todo | haiku | |
