# System Spec: DiaGridVisibilityVisualDebugger

## Parent Application
@docs/specs/applications/dia/dia.md

**Gameplay Domains:** ai, gameplay

## Purpose

DiaGridVisibilityVisualDebugger is the `IDebugDomain` implementation that makes the per-cell visibility grid visible in `DiaDebugPanel`. It is a world-space domain (`HasWorldDrawers() = true`) — cell states, sight radii, and shadowcast boundaries are all spatial.

The domain shows: a per-cell colour overlay (Unexplored near-black, Revealed grey, Visible accent-tinted) for the currently selected group; circles at each registered sight source at their configured sight radius; and an optional outline of the Visible/non-Visible boundary. A panel group-selector controls which `VisibilityGroupId` is displayed; per-group cell counts are shown in the stats block.

**Isolation constraint (SD-002):** `DiaGridVisibility` has zero compile-time dependency on `DiaGridVisibilityVisualDebugger`. The domain module depends on `DiaGridVisibility`; never the reverse.

## Responsibilities

- Implement `IDebugDomain` — all pure virtual methods — as `GridVisibilityDebugDomain<TGraph>`
- `HasWorldDrawers()` returns `true`
- Three world-space drawers, all registered with `DebugLayerManager` on `Register()`:
  1. **Cell State** — per-cell rectangle at the correct world-space position; colour from `DebugColourPalette` mapped to `VisibilityState` for the selected group
  2. **Sight Radii** — circle outline at each registered sight source's world position; radius = registered sight radius in world units
  3. **Shadowcast Boundary** — outline segment at each Visible cell edge where the adjacent cell is non-Visible for the selected group (disabled by default)
- `GetJSONState()` emits: `drawers[]`, `stats` (grid dimensions, chunk size, group count, sight source count), `selectedGroup`, and a `groups[]` array (id, visible / revealed / unexplored cell counts per group)
- `OnCommand("toggle", {drawer: "..."})` — enable/disable individual drawers by name
- `OnCommand("selectGroup", {groupId: "..."})` — switch which group's state is displayed
- Entire module guarded by `#ifdef DIA_DEBUG`

## Non-Responsibilities

- `GridVisibilitySystem` evaluation — `DiaGridVisibility`
- Fog-of-war render layer or minimap colour data — downstream of DiaGridVisibility
- Per-entity sprite highlighting for Visible/non-Visible status — game rendering layer
- Any runtime behaviour in Release builds

## Public Interface

```cpp
// GridVisibilityDebugDomain.h

template<Dia::Pathfinding::CPathGraph TGraph>
class GridVisibilityDebugDomain : public Dia::VisualDebugger::IDebugDomain
{
public:
    GridVisibilityDebugDomain(
        const Dia::GridVisibility::GridVisibilitySystem<TGraph>& system,
        const Dia::EntitySpatial::EntitySpatialModule&            spatial,
        float                                                     cellSize);

    Dia::Core::StringCRC  GetDomainId()    const override; // "GridVisibility"
    const char*           GetDisplayName() const override; // "Grid Visibility"
    const char*           GetDescription() const override; // see below
    Dia::Core::StringCRC  GetGroup()       const override; // "Spatial"
    Dia::Core::ColourRGBA GetAccentColour()const override; // DebugGroupAccents::kSpatial

    bool HasWorldDrawers() const override { return true; }

    void Register  (Dia::VisualDebugger::DebugLayerManager& manager) override;
    void Unregister(Dia::VisualDebugger::DebugLayerManager& manager) override;

    void GetJSONState(Dia::Core::JsonWriter& writer) override;
    void OnCommand   (Dia::Core::StringCRC cmd,
                      const Dia::Core::JsonValue& args) override;

    int           GetDrawerCount()     const override; // 3
    IDebugDrawer* GetDrawer(int index)       override;

private:
    const Dia::GridVisibility::GridVisibilitySystem<TGraph>& mSystem;
    const Dia::EntitySpatial::EntitySpatialModule&           mSpatial;
    float                                                    mCellSize;

    Dia::GridVisibility::VisibilityGroupId mSelectedGroup;

    CellStateDrawer          mCellStateDrawer;
    SightRadiiDrawer         mSightRadiiDrawer;
    ShadowcastBoundaryDrawer mBoundaryDrawer;
};
```

`GetDescription()` returns: `"Grid visibility — cell state overlay, sight radii, shadowcast boundary"` (62 chars ≤ 80 ✓)

## JSON State Schema

```json
{
  "drawers": [
    { "name": "Cell State",          "enabled": true  },
    { "name": "Sight Radii",         "enabled": true  },
    { "name": "Shadowcast Boundary", "enabled": false }
  ],
  "stats": {
    "gridWidth":       64,
    "gridHeight":      64,
    "chunkSize":        4,
    "groupCount":       2,
    "sightSourceCount": 8
  },
  "selectedGroup": "Team_A",
  "groups": [
    { "id": "Team_A", "visibleCells": 142, "revealedCells": 287, "unexploredCells": 835 },
    { "id": "Team_B", "visibleCells":  98, "revealedCells": 203, "unexploredCells": 963 }
  ]
}
```

When no groups are registered: `"groups"` is an empty array and `"selectedGroup"` is `""`.

## Panel Card Specification

- **Group:** Spatial / Geometry — accent `DebugGroupAccents::kSpatial` (`#06b6d4`) via `var(--accent)`
- **Stat line:** `"Group: <selectedGroup> — V:<visibleCells> R:<revealedCells> U:<unexploredCells>"`
- **Expanded body:** group-selector row (one button per registered group, active group highlighted in accent); drawer toggle rows; stats block

## Domain ACs

In addition to all 16 ACs in `@docs/specs/applications/dia/systems/diadebugdomain/debugger-contract.md`:

| AC | Criterion |
|----|-----------|
| D-1 | `HasWorldDrawers()` returns `true`; `Register()` adds exactly 3 drawers to `DebugLayerManager` |
| D-2 | Cell State drawer renders one coloured rectangle per visibility-grid cell for `mSelectedGroup`: Unexplored → `DebugColourPalette::kBackground`, Revealed → `DebugColourPalette::kMuted`, Visible → `DebugColourPalette::kAccent` |
| D-3 | Sight Radii drawer renders one circle outline per registered sight source, centred at its world position, with radius equal to its registered sight radius in world units |
| D-4 | Shadowcast Boundary drawer renders edge segments at the boundary between Visible and non-Visible cells for `mSelectedGroup`; starts disabled |
| D-5 | `OnCommand("selectGroup", {groupId: "..."})` updates `mSelectedGroup`; subsequent `GetJSONState()` reflects the new group's counts |
| D-6 | `"groups"` array in JSON state lists **all** registered groups with accurate visible / revealed / unexplored counts — not just the selected group |
| D-7 | All colours sourced from `DebugColourPalette` — no hardcoded RGBA values in any drawer |
| D-8 | No `DrawImGui()` call anywhere in this module |

## Features

| Feature | Description | Spec | Status |
|---------|-------------|------|--------|
| GridVisibilityDebugDomain | `IDebugDomain` implementation — domain metadata, Register/Unregister, vcxproj + module build | inline | Done |
| Cell State Drawer | World-space per-cell colour overlay: Unexplored / Revealed / Visible mapped to palette colours | inline | Done |
| Sight Radii Drawer | World-space circle outlines at each registered sight source at its configured radius | inline | Done |
| Shadowcast Boundary Drawer | Optional edge-segment outline at Visible / non-Visible cell boundaries for the selected group; off by default | inline | Done |
| Panel Stats + Group Selector | `GetJSONState()` emitting groups[], stats, selectedGroup + `selectGroup` command dispatch | inline | Done |

## Dependencies on Other Systems

**Required:**
- **DiaGridVisibility** — `GridVisibilitySystem<TGraph>`, `VisibilityGroupId`, `VisibilityState`, `CellCoord`; `VisitSightSources()` debug accessor (GVVD-006)
- **DiaDebugDomain** — `IDebugDomain`, `DebugGroupAccents`, `DebugLayerManager`, `DebugColourPalette`
- **DiaVisualDebugger** — `IVisualDebugger`, `IDebugDrawer`, draw primitives (filled rectangle, circle outline, line segment)
- **DiaEntitySpatial** — `EntitySpatialModule` for entity world positions (Sight Radii drawer)
- **DiaCore** — `StringCRC`, `JsonWriter`, `ColourRGBA`, `DIA_DEBUG`
- **DiaMaths** — `Vector2` (cell-to-world coordinate mapping)

**Explicitly excluded:**
- **DiaGeometry2D** — cell-to-world mapping uses arithmetic only; no geometry2D primitives needed
- Any gameplay or AI system — reads DiaGridVisibility query APIs only

**Dependents (future):**
- None. This is a leaf debug module.

## Out of Scope

- Entity sprite highlighting for Visible / non-Visible status — game rendering layer
- Minimap data or texture generation
- Per-entity visibility breakdown (which sight source can see which entity)
- Multiple groups displayed simultaneously (deferred; see GVVD-002)

## Decisions

| ID | Decision | Rationale | Status | Binding |
|----|----------|-----------|--------|---------|
| GVVD-001 | `HasWorldDrawers() = true` | Per-cell overlays and sight radii are inherently spatial; a panel-only view would lose most actionable information. | Accepted | Yes |
| GVVD-002 | One group at a time via panel group-selector | Displaying all groups simultaneously with blended tints causes colour confusion for 2+ groups. A selector is cleaner and scales to any number of groups. | Accepted | Yes |
| GVVD-003 | Cell colours sourced exclusively from `DebugColourPalette` | Unexplored → `kBackground`, Revealed → `kMuted`, Visible → `kAccent`. No hardcoded RGBA. Consistent with AC-4 of the debugger contract. | Accepted | Yes |
| GVVD-004 | SD-002 applies — `DiaGridVisibility` has zero compile-time dependency on this module | Module isolation rule from DiaDebugDomain. The domain depends on DiaGridVisibility; never the reverse. | Accepted | Yes |
| GVVD-005 | `GridVisibilityDebugDomain` is templated on `TGraph` | `GridVisibilitySystem<TGraph>` is a template; the domain holds a typed reference to it. This keeps the interface type-safe without requiring a separate virtual-dispatch query interface. | Accepted | Yes |
| GVVD-006 | `GridVisibilitySystem` adds a `VisitSightSources()` debug accessor guarded by `#ifdef DIA_DEBUG` | The Sight Radii drawer needs to iterate registered sight sources with entity IDs and configured radii. A `DIA_DEBUG`-only accessor keeps this out of Release builds and avoids polluting the runtime API. | Accepted | Yes |

## Inherited Binding Decisions

| ID | Source | Decision | Implication for this system |
|----|--------|----------|----------------------------|
| SD-002 | DiaDebugDomain | `DiaXxx` has zero compile-time dependency on `DiaXxxVisualDebugger` | `DiaGridVisibility.vcxproj` must not reference `DiaGridVisibilityVisualDebugger`. |
| PD-001 | Platform | StringCRC for all entity/component IDs | Domain ID, group key (`VisibilityGroupId`), and command names are all `StringCRC`. |
| PD-004 | Platform | No STL containers in public APIs | Internal drawer arrays use `DynamicArrayC`. |
| PD-005 | Platform | x64 only | `DiaGridVisibilityVisualDebugger.vcxproj` targets x64 exclusively. |
| PD-006 | Platform | Visual Studio project files are source of truth | `.vcxproj` and `.vcxproj.filters` created and maintained manually. |
| PD-007 | Platform | C++20 required | Template concept constraint on `TGraph` requires C++20. |
| PD-008 | Platform | Directory.Build.props owns OutDir/IntDir/toolchain | Must NOT override toolchain settings. |
| AD-001 | Dia App | Module system with YAML frontmatter | Create `dia.gridvisibilityvisualdebugger.architecture.module.md`. |
| AD-003 | Dia App | Namespace `Dia::<Module>::` | All code in `Dia::GridVisibilityVisualDebugger::` namespace. |

## Resolved Design Questions

1. **Cell size for world mapping** — `cellSize` is a constructor argument (`float cellSize`, stored as `mCellSize`). `GridVisibilitySystem` does not own world scale — the caller's module does. The domain is constructed once per level alongside the system and `cellSize` is constant for that lifetime. Adding `GetCellSize()` to the system would pollute its API with a concern it doesn't own.

2. **`VisitSightSources()` API shape** — all-sources only: `VisitSightSources(fn)` yields `(Entity, VisibilityGroupId, float radius)` for every registered source. The Sight Radii drawer filters on `groupId == mSelectedGroup` before emitting a draw call. A per-group overload is not needed — sight source counts are in the tens and the domain-side filter is trivially cheap.

## Status

**Status:** `Done`

Plan: [diagridvisibilityvisualdebugger.plan.md](diagridvisibilityvisualdebugger.plan.md)
