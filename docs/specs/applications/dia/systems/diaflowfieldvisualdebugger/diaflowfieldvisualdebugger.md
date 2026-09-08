# System Spec: DiaFlowFieldVisualDebugger

## Parent Application
@docs/specs/applications/dia/dia.md

**Gameplay Domains:** navigation

## Purpose

DiaFlowFieldVisualDebugger is the `IDebugDomain` implementation that makes flow field state visible in `DiaDebugPanel`. It is a world-space domain (`HasWorldDrawers() = true`) with two drawers: direction arrows (one per reachable cell, pointing along `FlowCell.direction`) and a reachability overlay (per-cell coloured quad: reachable vs unreachable).

It is the direct analogue of `ScalarFieldGradientOverlay` — same per-cell iteration pattern applied to `FlowField::Sample()`.

Following the `DiaXxxVisualDebugger` contract, `DiaFlowField` has zero compile-time dependency on `DiaFlowFieldVisualDebugger`.

## Responsibilities

- **Prerequisite — add to `FlowField`**: `GetWidth()` and `GetHeight()` accessors (see Prerequisite section)
- Implement `IDebugDomain` — all 16-AC contract methods
- `HasWorldDrawers()` returns `true`; two drawers: `DirectionArrows`, `ReachabilityOverlay`
- `Register`/`Unregister` bulk-register both drawers with `DebugLayerManager`
- `GetJSONState()` emits drawer list and stats: total cell count, reachable count, completion flag
- `OnCommand("toggle", {drawer: name})` enables/disables the named drawer
- `OnCommand("setScale", {key: "arrowLength", value: float})` scales arrow display length
- All world-space sizes multiplied by `IDebugContext::GetDebugScale()`
- Entire module guarded by `#ifdef DIA_DEBUG`
- `DiaFlowFieldVisualDebugger.vcxproj` static library registered in `Cluiche.sln` under `3.1-Gameplay-Tools`
- `dia.diaflowfieldvisualdebugger.architecture.module.md` YAML module documentation

## Non-Responsibilities

- Flow field computation — DiaFlowField
- Grid geometry — DiaPathfinding (cell coordinates)
- Any runtime behaviour in Release — entire module excluded by `#ifdef DIA_DEBUG`

## Prerequisite: `FlowField` Grid Dimension Accessors

These changes live in `DiaFlowField`. Required to iterate cells by (col, row) for world-position computation.

```cpp
// DiaFlowField/FlowField.h (additions)
int GetWidth()  const;  // number of columns
int GetHeight() const;  // number of rows
```

`GetCellCount()` already exists (`= width × height`). `GetWidth`/`GetHeight` are needed to convert a flat index to a `CellCoord(col, row)` pair for world-position computation: `worldPos = {col * cellSize + cellSize*0.5f, row * cellSize + cellSize*0.5f}`.

These accessors are not `DIA_DEBUG`-only — they are useful at runtime and have zero cost.

## Public Interfaces

```cpp
// DiaFlowFieldVisualDebugger/FlowFieldVisualDebugger.h
#ifdef DIA_DEBUG

#include <DiaDebugDraw/Domain/IDebugDomain.h>
#include <DiaFlowField/FlowField.h>

namespace Dia::FlowField
{
    class FlowFieldVisualDebugger : public Dia::VisualDebugger::IDebugDomain
    {
    public:
        // field and cellSize supplied by caller; field must outlive this object.
        FlowFieldVisualDebugger(const FlowField& field, float cellSize);

        Dia::Core::StringCRC  GetDomainId()      const override; // "flowfield"
        const char*           GetDisplayName()   const override; // "Flow Field"
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
        const FlowField& mField;
        float             mCellSize;
        DirectionArrowsDrawer    mDirectionArrows;
        ReachabilityOverlayDrawer mReachabilityOverlay;
    };
}
#endif // DIA_DEBUG
```

`GetDescription()` returns: `"Flow field — per-cell direction arrows and reachability overlay"` (62 chars ≤ 80 ✓)

### JSON State Schema

```json
{
  "drawers": [
    { "name": "DirectionArrows",    "enabled": true },
    { "name": "ReachabilityOverlay","enabled": true }
  ],
  "stats": {
    "cellCount":      256,
    "reachableCount": 241,
    "isComplete":     false
  }
}
```

`isComplete` mirrors `FlowField::IsComplete()`. `reachableCount` is computed by iterating all cells during `GetJSONState()`.

### World-Space Draw Specification

| Drawer | Primitive | Colour key | Size rule |
|--------|-----------|------------|-----------|
| DirectionArrows — reachable | Arrow line | `kDebugFlowDirection` | Length = `cellSize × 0.4 × GetDebugScale()`, from cell centre |
| DirectionArrows — unreachable | (none drawn) | — | Skip cells where `FlowCell.reachable == false` |
| ReachabilityOverlay — reachable | Quad (translucent) | `kDebugGridPassable` | Cell size × `GetDebugScale()` |
| ReachabilityOverlay — unreachable | Quad (translucent) | `kDebugGridImpassable` | Cell size × `GetDebugScale()` |

Cell centre: `worldPos = {col * cellSize + cellSize*0.5f, row * cellSize + cellSize*0.5f}`. Arrow direction from `FlowCell.direction` (already normalised; scaled to display length).

### Panel Card Specification

- **Group:** Navigation — accent `DebugGroupAccents::kNavigation` (`#3b82f6`) via `var(--accent)`
- **Domain stat line:** `"Cells: R/T reachable"` where R = reachableCount, T = cellCount
- **Drawer toggles:** DirectionArrows (on), ReachabilityOverlay (on)

## Tests

`Tests/GoogleTests/DiaFlowFieldVisualDebugger/TestFlowFieldVisualDebugger.cpp`

### Mandatory shapes (AC-15)

| Shape | Description |
|-------|-------------|
| DrawerGate | Disabled `DirectionArrows` emits zero primitives; re-enabling restores output |
| PrimitiveType | `DirectionArrows` → line primitives; `ReachabilityOverlay` → quad primitives |
| ScaleSensitivity | Doubling `GetDebugScale()` doubles arrow lengths |
| JSONRoundTrip | `GetJSONState()` drawer entries match current enabled state |
| OnCommandRoundTrip | `OnCommand("toggle", {drawer:"DirectionArrows"})` toggles; second call reverts |

### Domain-specific shapes

| Shape | Description |
|-------|-------------|
| `OneArrowPerReachableCell` | Field with R reachable cells → R arrow line primitives |
| `NoArrowForUnreachableCell` | Cells with `FlowCell.reachable == false` emit no arrow |
| `ArrowDirectionMatchesFlowCell` | Arrow endpoint direction matches `FlowCell.direction` (within float tolerance) |
| `ReachabilityOverlay_OneCellPerAllCells` | N-cell field → N quad primitives from overlay |
| `ReachabilityOverlay_ColoursDiffer` | Reachable cell colour ≠ unreachable cell colour |
| `DrawerGate_ReachabilityOverlay` | Disabled `ReachabilityOverlay` emits zero quads |
| `Toggle_ReachabilityOverlay` | `OnCommand("toggle", {drawer:"ReachabilityOverlay"})` toggles it |
| `Stats_CellCount_Accurate` | `stats.cellCount` equals `FlowField::GetCellCount()` |
| `Stats_ReachableCount_Accurate` | `stats.reachableCount` matches sum of reachable cells |
| `Stats_IsComplete_Mirrors_Field` | `stats.isComplete` matches `FlowField::IsComplete()` |
| `EmptyField_ZeroCells_NoAssert` | 0-cell field → no output, no crash |
| `ArrowLength_Command` | `OnCommand("setScale", {key:"arrowLength", value:2.0})` doubles arrow lengths |

## Dependencies on Other Systems

**Required:**
- **DiaFlowField** — `FlowField`, `FlowCell`, `GetWidth()`/`GetHeight()` (prereq)
- **DiaPathfinding** — `CellCoord` (for iteration)
- **DiaVisualDebugger** — `IDebugDomain`, `DebugGroupAccents`, `DebugColourPalette`, draw infra
- **DiaCore** — `StringCRC`, `ColourRGBA`, `JsonWriter`, `JsonValue`

**Explicitly excluded:**
- **ImGui**, **DiaEntity**, **DiaApplicationFlow**, **DiaAIBudget**, **DiaSteering**

## Contract Compliance

All 16 ACs from `debugger-contract.md` apply.

| AC | Notes |
|----|-------|
| AC-1 | Standalone `DiaFlowFieldVisualDebugger.vcxproj` |
| AC-2 | `DiaFlowField` has zero dep on `DiaFlowFieldVisualDebugger` |
| AC-3 | Deps: DiaFlowField + DiaPathfinding + DiaVisualDebugger + DiaCore + draw infra only |
| AC-4 | Implements all `IDebugDomain` pure virtuals |
| AC-5 | `GetDescription()` = 62 chars ✓ |
| AC-6 | World-space colours from `DebugColourPalette` only |
| AC-7 | All sizes × `GetDebugScale()` — verified by ScaleSensitivity shape |
| AC-8 | No `ImGui::*` calls |
| AC-9 | `const FlowField&` — read-only |
| AC-10 | Emits `drawers` + `stats` minimum |
| AC-11 | `OnCommand("toggle", {drawer})` handled for both drawers |
| AC-12 | `OnCommand("setScale", ...)` handled; drawer enables use `std::atomic<bool>` |
| AC-13/14 | `Tests/GoogleTests/DiaFlowFieldVisualDebugger/TestFlowFieldVisualDebugger.cpp` |
| AC-15 | All 5 mandatory shapes present — see Tests section |
| AC-16 | Panel card complies with standard spacing; accent via `var(--accent)` |

## Decisions

| ID | Decision | Rationale | Status | Binding |
|----|----------|-----------|--------|---------|
| SD-001 | `GetWidth()`/`GetHeight()` added to `FlowField` as non-debug accessors | Needed to convert flat index to (col, row) for world-position. Not DIA_DEBUG-gated: useful at runtime (e.g. bounds checks) and cost is negligible. | Accepted | Yes |
| SD-002 | Arrow length = `cellSize × 0.4 × scale`, not full cell size | Full-size arrows on dense grids overlap badly. 0.4 leaves visible gaps between cells. Adjustable via `arrowLength` scale command. | Accepted | Yes |
| SD-003 | Unreachable cells draw no arrow (not a zero-vector line) | A zero-vector arrow at cell centre is confusing; absence is clearer. Reachability is visible via the overlay drawer. | Accepted | Yes |

## Inherited Binding Decisions

| ID | Source | Decision | Implication |
|----|--------|----------|-------------|
| PD-001 | Platform | StringCRC for all IDs | Domain ID, group, drawer names, command keys use `StringCRC` |
| PD-004 | Platform | No STL in public APIs | `DynamicArrayC` used; no `std::vector` return values |
| PD-005 | Platform | x64 only | vcxproj targets x64 |
| PD-006 | Platform | vcxproj is source of truth | `.vcxproj` + `.vcxproj.filters` maintained |
| PD-007 | Platform | C++20 | `/std:c++20` |
| PD-008 | Platform | Directory.Build.props owns toolchain | vcxproj does not override OutDir/IntDir/toolset |
| AD-001 | Dia App | Module YAML docs | `dia.diaflowfieldvisualdebugger.architecture.module.md` required |
| AD-003 | Dia App | `Dia::<Module>::` namespace | All code in `Dia::FlowField::` namespace |

## Open Design Questions

1. **Cell size source** — the debugger constructor takes `cellSize` separately. If the simulation has a canonical cell size stored in a registry or singleton, consider reading it there instead of requiring the caller to pass it. Decide at integration.

2. **Performance on large fields** — 4096 cells (64×64) is 4096 arrows + 4096 overlay quads per frame. If frame time is impacted, gate the overlay on a minimum-scale threshold or draw only cells within the visible viewport. Add a note to the stat line if cells are being culled.

## Status

**Status:** Approved
