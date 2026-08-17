**Spec:** @docs/specs/applications/dia/systems/diagridvisibilityvisualdebugger/diagridvisibilityvisualdebugger.md
**Status:** Done

---

## Implementation Patterns

### Header-only template class

`GridVisibilityDebugDomain<TGraph>` is templated — the full implementation lives in the `.h` file (no `.cpp`). Drawer subclasses are declared in the same header. The vcxproj has only one compile unit: a thin `.cpp` that does nothing but satisfy the build (or omit entirely and mark the project header-only in the YAML).

### Drawer class structure

Each drawer is a private inner class (or named inner type) inside `GridVisibilityDebugDomain<TGraph>` inheriting `IDebugDrawer`. Drawers hold a `const GridVisibilitySystem<TGraph>&` reference (passed from the domain on construction) plus any per-drawer state (`bool mEnabled`, selected group mirror). Registered/unregistered with `DebugLayerManager` in `Register()`/`Unregister()`.

### Cell-to-world mapping

```cpp
Vector2 cellOrigin(static_cast<float>(cell.x) * mCellSize,
                   static_cast<float>(cell.y) * mCellSize);
// Rectangle: origin = cellOrigin, size = (mCellSize, mCellSize)
```

Grid origin is (0,0). This matches the convention used by DiaPathfinding — confirm with `SquarePathGrid` at implementation time.

### Palette mapping

```
Unexplored → DebugColourPalette::kBackground   (near-black)
Revealed   → DebugColourPalette::kMuted        (grey)
Visible    → DebugColourPalette::kAccent       (cyan tint)
```

No hardcoded RGBA anywhere in drawer code.

### `VisitSightSources` accessor (added to DiaGridVisibility)

```cpp
#ifdef DIA_DEBUG
void VisitSightSources(
    std::function<void(Dia::Entity::Entity, VisibilityGroupId, float radius)> fn) const;
#endif
```

Lives in `GridVisibilitySystem<TGraph>` alongside the existing API. The Sight Radii drawer calls this and skips entries whose `groupId != mSelectedGroup`.

### `GetJSONState` emission order

```
drawers[] → stats{gridWidth, gridHeight, chunkSize, groupCount, sightSourceCount}
         → selectedGroup (string key of mSelectedGroup)
         → groups[] (id, visibleCells, revealedCells, unexploredCells per group)
```

### Shadowcast Boundary edge detection

Walk the visibility grid. For each cell that is `Visible` for the selected group, test its 4 cardinal neighbours. For each neighbour that is not `Visible` (or is out-of-bounds), emit a world-space line segment along the shared edge. Disabled by default (`mEnabled = false`).

---

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Module scaffold — `Dia/DiaGridVisibilityVisualDebugger/` directory, `DiaGridVisibilityVisualDebugger.vcxproj` + `.vcxproj.filters`, `dia.gridvisibilityvisualdebugger.architecture.module.md` YAML, sln entry under `2.0-Gameplay` | Build passes with empty module | Done | haiku | Added to 3.1-Visual-Tools in sln; GUID {F2E1D0C9-B8A7-4356-FEDC-BA9876543210} |
| 2 | `VisitSightSources()` debug accessor — add `#ifdef DIA_DEBUG void VisitSightSources(fn) const` to `GridVisibilitySystem<TGraph>` in DiaGridVisibility; iterates internal sight-source registry | Compile only (accessor exists under DIA_DEBUG) | Done | haiku | Also added VisitGroups + GetGroupCellCounts (needed by Tasks 5 and 7) |
| 3 | `GridVisibilityDebugDomain<TGraph>` skeleton — header with all `IDebugDomain` pure virtuals stubbed (return defaults), constructor taking `(system, spatial, cellSize)`, `mSelectedGroup` defaulting to first registered group or empty, three drawer members declared | Compile only | Done | sonnet | Header-only template; CVisibilityGraph concept in Dia::GridVisibility namespace |
| 4 | Cell State Drawer — iterate all visibility-grid cells via `GetCellState()` for `mSelectedGroup`, map to `DebugColourPalette` (kBackground/kMuted/kAccent), emit filled rectangle per cell using cell-to-world mapping | Unit test: 3×3 grid with known states → correct colour per cell | Done | sonnet | DebugColourPalette: kGoal=Visible, kInactive=Revealed, kDeepSleep=Unexplored (kBackground/kMuted/kAccent don't exist) |
| 5 | Sight Radii Drawer — call `VisitSightSources()`, skip entries where `groupId != mSelectedGroup`, look up world position from `EntitySpatialModule`, emit circle outline with radius = registered sight radius | Unit test: 2 groups registered, only selected group's circles emitted | Done | sonnet | World pos derived from lastVisCell (mSpatial not needed in current paths) |
| 6 | Shadowcast Boundary Drawer — walk grid for Visible cells, emit edge segments at boundaries with non-Visible neighbours; starts disabled (`mEnabled = false`) | Unit test: 3×3 grid with a 1×1 visible interior → 4 edge segments emitted | Done | sonnet | SetEnabled(false) in constructor; emits kWarning lines at visibility boundaries |
| 7 | `GetJSONState()` + group selector — emit full JSON schema (drawers[], stats, selectedGroup, groups[]); `OnCommand("selectGroup", {groupId})` updates `mSelectedGroup`; `OnCommand("toggle", {drawer})` flips drawer enabled flag | JSON round-trip test; selectGroup changes per-group stats in next GetJSONState call | Done | sonnet | 59 tests passed after Task 7 |
| 8 | Unit tests — `GoogleTests/DiaGridVisibilityVisualDebugger/TestGridVisibilityDebugDomain.cpp`; covers Domain ACs D-1 through D-8 plus GetDrawerCount=3, group list completeness, no hardcoded RGBA | `dia run googletest --filter="GridVisibilityDebugDomain*"` passes | Done | sonnet | 23/23 tests pass across 5 suites (Identity, Lifecycle, DrawerGate, JSONState, OnCommand) |
| 9 | Build + contract check — `dia run googletest`, `dia check debugger-contract` | All tests pass; contract check clean for this module | Done | haiku | Full suite passes; contract check clean |
