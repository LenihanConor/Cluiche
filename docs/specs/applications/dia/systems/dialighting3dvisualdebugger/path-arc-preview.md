# Feature Spec: path-arc-preview

## Parent System
@docs/specs/applications/dia/systems/dialighting3dvisualdebugger/dialighting3dvisualdebugger.md

**Status:** `Done`

---

## Summary

Add `LightPathArcDrawer` — an `IVisualDebugger` subclass that queries each light slot in `LightRegistry3D` for an attached `LightPathBehaviour3D` and, when one is found, samples its spline and draws a line-segment arc. Registers under `LayerNames::kLightPathArc` (priority 15). Exposes a `DrawImGui()` shelf with an arc sample count slider.

Also adds `LightRegistry3D::GetPathBehaviour(StringCRC lightId)` — the registry accessor needed to query attached path behaviours without knowing the attachment internals.

**Depends on:** `debug-widget-config` (for `kLightPathArc` constant and the vcxproj/sln setup).

---

## Acceptance Criteria

1. `LightRegistry3D::GetPathBehaviour(StringCRC lightId)` added — returns `LightPathBehaviour3D*` or `nullptr`
2. `LightPathArcDrawer` implements `IVisualDebugger` and lives in `DiaLighting3DVisualDebugger.vcxproj`
3. Stores `const LightRegistry3D&` and `const DebugLayerManager&` at construction
4. `GetLayerName()` returns `LayerNames::kLightPathArc`
5. `Draw()` iterates all light slots, calls `GetPathBehaviour()`, skips `nullptr`, samples spline at `mArcSamples + 1` points, emits one line per adjacent pair
6. Point/Spot arc colour: `DebugColourPalette::kHealthy`; Directional arc colour: `DebugColourPalette::kGoal`
7. `DrawImGui()` exposes: `"Arc samples"` SliderInt `[8, 64]`
8. Build passes with no warnings; GoogleTests pass

---

## Prerequisites

- `debug-widget-config` complete (vcxproj, sln, `kLightPathArc` constant exist)
- `LightPathBehaviour3D` feature implemented (`LightPathBehaviour3D` class + `LightBehaviourRegistry3D` registration)

---

## Registry Addition

Add to `LightRegistry3D` (`Dia/DiaLighting3D/Registry/LightRegistry3D.h/.cpp`):

```cpp
// Returns the attached LightPathBehaviour3D for the given light ID, or nullptr.
// Searches Point, Spot, and Directional slots.
LightPathBehaviour3D* GetPathBehaviour(Dia::Core::StringCRC lightId) const;
```

Implementation: iterate `mPointSlots`, `mSpotSlots`, `mDirectionalSlots`; for each slot whose `id == lightId`, scan its `behaviours[]` array; return the entry whose `GetTypeId() == LightPathBehaviour3D::kTypeId`, cast to `LightPathBehaviour3D*`. Returns `nullptr` if not found.

---

## Draw Class

```cpp
namespace Dia::Lighting3D
{
    class LightPathArcDrawer : public Dia::Debug::IVisualDebugger
    {
    public:
        LightPathArcDrawer(const LightRegistry3D&                registry,
                           const Dia::Debug::DebugLayerManager&  manager);

        Dia::Core::StringCRC GetLayerName() const override;
        void Draw    (Dia::Graphics::FrameData& frameData) override;
        void DrawImGui() override;

    private:
        const LightRegistry3D&               mRegistry;
        const Dia::Debug::DebugLayerManager& mManager;

        int mArcSamples = 32;
    };
}
```

---

## Draw Logic

For each light slot (Point, Spot, Directional):
1. Call `mRegistry.GetPathBehaviour(slotId)` — skip if `nullptr`
2. Sample `N = mArcSamples + 1` points: `p[k] = behaviour->GetSpline().Evaluate(k / float(mArcSamples))` for `k = 0..mArcSamples`
3. Emit `mArcSamples` line segments: `RequestDrawLine(p[k], p[k+1], colour)` for `k = 0..mArcSamples-1`
4. Colour: `kHealthy` (green) for Point/Spot; `kGoal` (cyan) for Directional

Line endpoint coordinates are world-space spline positions — no `debugScale` multiplication (scale applies to decorative sizes, not world-space curve positions).

---

## `GetSpline()` accessor on `LightPathBehaviour3D`

`LightPathArcDrawer` needs read access to the spline stored in `LightPathBehaviour3D`. Add a const accessor:

```cpp
// In LightPathBehaviour3D:
const Dia::Geometry3D::Spline3D& GetSpline() const;
```

This is a minor addition to the `LightPathBehaviour3D` spec — note it as a prerequisite task in the `LightPathBehaviour3D` plan when that feature is built.

---

## ImGui Shelf

```cpp
void LightPathArcDrawer::DrawImGui()
{
    ImGui::SliderInt("Arc samples", &mArcSamples, 8, 64);
}
```

---

## Tasks

| # | Task | Test |
|---|------|------|
| 1 | Add `GetPathBehaviour(StringCRC lightId) const` to `LightRegistry3D.h/.cpp` | Build passes |
| 2 | Add `GetSpline() const` to `LightPathBehaviour3D.h/.cpp` | Build passes |
| 3 | Implement `LightPathArcDrawer.h/.cpp`; add to `DiaLighting3DVisualDebugger.vcxproj` | Build passes |
| 4 | Write `Cluiche/Tests/GoogleTests/DiaLighting3D/TestLightPathArcDrawer.cpp`; add to `GoogleTests.vcxproj` | `dia run googletest --filter="DiaLighting3D_LightPathArc*"` all pass |

---

## Test Plan

**File:** `Cluiche/Tests/GoogleTests/DiaLighting3D/TestLightPathArcDrawer.cpp`

Test setup: a `LightRegistry3D` with one point light, one spot light, and one directional light. A `LightPathBehaviour3D` with a known 4-point Catmull-Rom spline attached to the point light only.

| Suite | Test | What it verifies |
|-------|------|-----------------|
| LightPathArcDrawer | `LayerName_IsLightPathArc` | `GetLayerName() == LayerNames::kLightPathArc` |
| LightPathArcDrawer | `Draw_LightWithPath_EmitsArcLines` | Point light with path → `mArcSamples` line primitives |
| LightPathArcDrawer | `Draw_LightNoBehaviour_NoPrimitives` | Spot/Directional with no path → 0 lines |
| LightPathArcDrawer | `Draw_ArcColour_PointLight_IsHealthy` | Line colour == `kHealthy` |
| LightPathArcDrawer | `Draw_ArcSamples_MatchesSliderValue` | `mArcSamples = 16` → 16 line primitives |
| LightPathArcDrawer | `Draw_Disabled_NoPrimitives` | `SetEnabled(false)` → 0 primitives |
| LightPathArcDrawer | `Draw_EmptyRegistry_NoPrimitives` | Empty registry → 0 primitives |
| LightRegistry3D | `GetPathBehaviour_ReturnsAttached` | Attach `LightPathBehaviour3D`, query by id → non-null |
| LightRegistry3D | `GetPathBehaviour_UnknownId_ReturnsNull` | Unknown id → nullptr |
| LightRegistry3D | `GetPathBehaviour_NoBehaviour_ReturnsNull` | Light registered, no behaviour → nullptr |

---

## Inherited Binding Decisions

| ID | Decision | Compliance |
|----|----------|-----------|
| PD-001 | StringCRC for IDs | `GetLayerName()` returns `LayerNames::kLightPathArc`; behaviour lookup uses `LightPathBehaviour3D::kTypeId` |
| PD-004 | No STL in public API | Constructor and `Draw()` use refs and engine types only |
| PD-007 | C++20 required | Compiled under `/std:c++20` |
| AD-003 | Namespace `Dia::<Module>::` | `Dia::Lighting3D::LightPathArcDrawer` |
| SD-DBG-001 | Stack of focused draw classes | One focused draw class for arc preview |
| SD-DBG-003 | Priority-ordered draw | Registered at priority 15 (above widgets at 10) |
| SD-DBG-005 | Global `debugScale` | Not applied to world-space arc positions (correct — only decorative sizes scale) |
| SD-DBG-010 | `DebugColourPalette` colours | `kHealthy` for point/spot arcs, `kGoal` for directional arc |
| SD-DBG-014 | Same-family classes share vcxproj | Lives in `DiaLighting3DVisualDebugger.vcxproj` alongside `LightWidgetsDrawer` |
| SD-LVD-002 | No debug fields on light structs | No changes to DiaLighting3D data types |

---

## Status

`Approved`
