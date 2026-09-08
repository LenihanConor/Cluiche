# System Spec: DiaSteeringVisualDebugger

## Parent Application
@docs/specs/applications/dia/dia.md

**Gameplay Domains:** navigation

## Purpose

DiaSteeringVisualDebugger is the `IDebugDomain` implementation that makes per-agent steering behaviour visible in `DiaDebugPanel`. It is a world-space domain (`HasWorldDrawers() = true`) with three drawers: velocity arrows (current velocity + desired velocity), separation radius circles, and detection boxes.

All primitives are drawn at each agent's world position. Sizes are multiplied by `IDebugContext::GetDebugScale()`. Colours come exclusively from `DebugColourPalette`.

Following the `DiaXxxVisualDebugger` contract, `DiaSteering` has zero compile-time dependency on `DiaSteeringVisualDebugger`.

## Responsibilities

- **Prerequisite — add to `DiaSteering`**: `SteeringSystem::VisitAgents(fn)` debug accessor (`#ifdef DIA_DEBUG`) — see Prerequisite section
- Implement `IDebugDomain` — all 16-AC contract methods
- `HasWorldDrawers()` returns `true`; three drawers: `VelocityArrows`, `SeparationRadius`, `DetectionBoxes`
- `Register`/`Unregister` bulk-register all three drawers with `DebugLayerManager`
- `GetJSONState()` emits drawer list and `stats.agentCount`
- `OnCommand("toggle", {drawer: name})` enables/disables the named drawer via `std::atomic<bool>`
- `OnCommand("setScale", {key: "arrowScale", value: float})` — multiplier on arrow display length
- All world-space sizes multiplied by `IDebugContext::GetDebugScale()`
- Entire module guarded by `#ifdef DIA_DEBUG`
- `DiaSteeringVisualDebugger.vcxproj` static library registered in `Cluiche.sln` under `3.1-Gameplay-Tools`
- `dia.diasteeringvisualdebugger.architecture.module.md` YAML module documentation

## Non-Responsibilities

- Steering pipeline execution — DiaSteering
- Entity position tracking beyond what `SteeringAgent` provides
- Any runtime behaviour in Release — entire module excluded by `#ifdef DIA_DEBUG`

## Prerequisite: `SteeringSystem` Debug Accessor

These changes live in `DiaSteering`. Must be implemented before building the debugger.

```cpp
// DiaSteering/SteeringSystem.h (addition, DIA_DEBUG only)
#ifdef DIA_DEBUG
// fn: void(StringCRC id, const SteeringAgent& state, Vector2D desiredVelocity)
template<typename Fn>
void VisitAgents(Fn&& fn) const;
#endif
```

`VisitAgents` iterates the internal agent map and calls `fn` with each agent's ID, its cached `SteeringAgent` (position, velocity, separationRadius, detectionBox), and the desired velocity computed by the last `Update()` call.

## Public Interfaces

```cpp
// DiaSteeringVisualDebugger/SteeringVisualDebugger.h
#ifdef DIA_DEBUG

#include <DiaDebugDraw/Domain/IDebugDomain.h>

namespace Dia::Steering { class SteeringSystem; }

namespace Dia::Steering
{
    class SteeringVisualDebugger : public Dia::VisualDebugger::IDebugDomain
    {
    public:
        explicit SteeringVisualDebugger(const SteeringSystem& system);

        Dia::Core::StringCRC  GetDomainId()      const override; // "steering"
        const char*           GetDisplayName()   const override; // "Steering"
        const char*           GetDescription()   const override; // see below
        Dia::Core::StringCRC  GetGroup()          const override; // "Navigation"
        Dia::Core::ColourRGBA GetAccentColour()   const override; // DebugGroupAccents::kNavigation

        bool HasWorldDrawers() const override { return true; }

        void Register(Dia::Debug::DebugLayerManager& mgr)   override;
        void Unregister(Dia::Debug::DebugLayerManager& mgr) override;

        int  GetDrawerCount() const override { return 3; }
        Dia::Debug::IVisualDebugger* GetDrawer(int index) override;

        void GetJSONState(Dia::Core::JsonWriter& writer) override;
        void OnCommand(Dia::Core::StringCRC cmd, const Dia::Core::JsonValue& args) override;

    private:
        const SteeringSystem& mSystem;
        VelocityArrowsDrawer   mVelocityArrows;
        SeparationRadiusDrawer mSeparationRadius;
        DetectionBoxDrawer     mDetectionBoxes;
    };
}
#endif // DIA_DEBUG
```

`GetDescription()` returns: `"Steering agents — velocity arrows, separation radii, detection boxes"` (62 chars ≤ 80 ✓)

### JSON State Schema

```json
{
  "drawers": [
    { "name": "VelocityArrows",   "enabled": true  },
    { "name": "SeparationRadius", "enabled": true  },
    { "name": "DetectionBoxes",   "enabled": false }
  ],
  "stats": {
    "agentCount": 4
  }
}
```

`DetectionBoxes` is off by default to reduce visual noise (SD-002).

### World-Space Draw Specification

| Drawer | Primitive | Colour key | Size rule |
|--------|-----------|------------|-----------|
| VelocityArrows — current | Arrow line | `kDebugVelocityCurrent` | Length = `velocity.Length() × GetDebugScale()` |
| VelocityArrows — desired | Arrow line | `kDebugVelocityDesired` | Length = `desiredVelocity.Length() × GetDebugScale()` |
| SeparationRadius | Circle (no fill) | `kDebugSeparation` | Radius = `separationRadius × GetDebugScale()` |
| DetectionBoxes | Quad outline | `kDebugDetection` | Half-extents × `GetDebugScale()` |

Arrows originate at agent world position and point along the velocity direction. Current and desired arrow colours must be distinct.

### Panel Card Specification

- **Group:** Navigation — accent `DebugGroupAccents::kNavigation` (`#3b82f6`) via `var(--accent)`
- **Domain stat line:** `"Agents: N"` where N = `stats.agentCount`
- **Drawer toggles:** VelocityArrows (on), SeparationRadius (on), DetectionBoxes (off)

## Tests

`Tests/GoogleTests/DiaSteeringVisualDebugger/TestSteeringVisualDebugger.cpp`

### Mandatory shapes (AC-15)

| Shape | Description |
|-------|-------------|
| DrawerGate | Disabled `VelocityArrows` emits zero primitives; re-enabling restores output |
| PrimitiveType | `VelocityArrows` → line primitives; `SeparationRadius` → circle primitives; `DetectionBoxes` → quad primitives |
| ScaleSensitivity | Doubling `GetDebugScale()` doubles arrow lengths and circle radii |
| JSONRoundTrip | `GetJSONState()` drawer entries match current enabled state for all three drawers |
| OnCommandRoundTrip | `OnCommand("toggle", {drawer:"VelocityArrows"})` toggles enabled; second call reverts |

### Domain-specific shapes

| Shape | Description |
|-------|-------------|
| `TwoArrowsPerAgent` | N agents → 2N arrow lines from `VelocityArrows` (one current, one desired each) |
| `ColoursDiffer` | Current velocity arrow colour ≠ desired velocity arrow colour |
| `OneCirclePerAgent` | N agents → N circle primitives from `SeparationRadius` |
| `OneBoxPerAgent` | N agents → N quad primitives from `DetectionBoxes` |
| `DrawerGate_SeparationRadius` | Disabled `SeparationRadius` emits zero circles |
| `DrawerGate_DetectionBoxes` | Disabled `DetectionBoxes` emits zero quads |
| `Toggle_SeparationRadius` | `OnCommand("toggle", {drawer:"SeparationRadius"})` toggles it |
| `Toggle_DetectionBoxes` | `OnCommand("toggle", {drawer:"DetectionBoxes"})` toggles it; starts disabled |
| `DetectionBoxes_OffByDefault` | Fresh `SteeringVisualDebugger` has `DetectionBoxes.enabled == false` |
| `AgentCount_Accurate` | `stats.agentCount` in JSON equals actual registered agent count |
| `NoAgents_NoOutput_NoAssert` | Empty system → all drawers emit nothing without assertion or crash |
| `ArrowScale_Command` | `OnCommand("setScale", {key:"arrowScale", value:2.0})` doubles arrow lengths |

## Dependencies on Other Systems

**Required:**
- **DiaSteering** — `SteeringSystem`, `SteeringAgent`, `VisitAgents()` (prereq)
- **DiaVisualDebugger** — `IDebugDomain`, `DebugGroupAccents`, `DebugColourPalette`, draw infra
- **DiaCore** — `StringCRC`, `ColourRGBA`, `JsonWriter`, `JsonValue`

**Explicitly excluded:**
- **ImGui**, **DiaEntity**, **DiaApplicationFlow**, **DiaAIBudget**

## Contract Compliance

All 16 ACs from `debugger-contract.md` apply.

| AC | Notes |
|----|-------|
| AC-1 | Standalone `DiaSteeringVisualDebugger.vcxproj` |
| AC-2 | `DiaSteering` has zero dep on `DiaSteeringVisualDebugger` |
| AC-3 | Deps: DiaSteering + DiaVisualDebugger + DiaCore + draw infra only |
| AC-4 | Implements all `IDebugDomain` pure virtuals |
| AC-5 | `GetDescription()` = 62 chars ✓ |
| AC-6 | World-space colours from `DebugColourPalette` only |
| AC-7 | All sizes × `GetDebugScale()` — verified by ScaleSensitivity shape |
| AC-8 | No `ImGui::*` calls |
| AC-9 | `const SteeringSystem&` read-only via `VisitAgents` |
| AC-10 | Emits `drawers` + `stats` minimum |
| AC-11 | `OnCommand("toggle", {drawer})` handled for all three drawers |
| AC-12 | `OnCommand("setScale", ...)` handled; drawer enables use `std::atomic<bool>` |
| AC-13/14 | `Tests/GoogleTests/DiaSteeringVisualDebugger/TestSteeringVisualDebugger.cpp` |
| AC-15 | All 5 mandatory shapes present — see Tests section |
| AC-16 | Panel card complies with standard spacing; accent via `var(--accent)` |

## Decisions

| ID | Decision | Rationale | Status | Binding |
|----|----------|-----------|--------|---------|
| SD-001 | `VisitAgents()` debug accessor added to `DiaSteering` | Debugger cannot reach the internal agent map; template visitor costs nothing in Release. Mirrors `GetLastFireReport` pattern. | Accepted | Yes |
| SD-002 | `DetectionBoxes` drawer disabled by default | Detection boxes clutter the view; on by demand. VelocityArrows and SeparationRadius on by default. | Accepted | Yes |
| SD-003 | Arrow length encodes speed magnitude, not normalised | Shows actual speed, not just direction. Scale command adjusts display length without changing meaning. | Accepted | Yes |

## Inherited Binding Decisions

| ID | Source | Decision | Implication |
|----|--------|----------|-------------|
| PD-001 | Platform | StringCRC for all IDs | Domain ID, group, drawer names, command keys use `StringCRC` |
| PD-004 | Platform | No STL in public APIs | `DynamicArrayC` used; no `std::vector` return values |
| PD-005 | Platform | x64 only | vcxproj targets x64 |
| PD-006 | Platform | vcxproj is source of truth | `.vcxproj` + `.vcxproj.filters` maintained |
| PD-007 | Platform | C++20 | `/std:c++20` |
| PD-008 | Platform | Directory.Build.props owns toolchain | vcxproj does not override OutDir/IntDir/toolset |
| AD-001 | Dia App | Module YAML docs | `dia.diasteeringvisualdebugger.architecture.module.md` required |
| AD-003 | Dia App | `Dia::<Module>::` namespace | All code in `Dia::Steering::` namespace |

## Open Design Questions

1. **`VisitAgents` template vs C-style callback** — the template form `VisitAgents(Fn&&)` is ideal for zero-cost production builds. If test isolation requires a mock-injectable override, provide a parallel C-style `VisitAgents(void(*fn)(...), void* userdata)` overload. Decide at implementation.

2. **Arrow head cap** — do velocity arrows need a rendered arrowhead, or is a plain line sufficient? The existing draw infra may not have a native arrowhead primitive. Fall back to line-only if not supported; a second short line at an angle is an acceptable substitute.

## Status

**Status:** Done
