# Feature Spec: debug-widget-config

## Parent System
@docs/specs/applications/dia/systems/dialighting3dvisualdebugger/dialighting3dvisualdebugger.md

**Status:** `Done`

---

## Summary

Add `LightWidgetsDrawer` — an `IVisualDebugger` subclass that reads `LightRegistry3D` each frame and draws a sphere or arrow widget at each registered light's position/direction. Registers under `LayerNames::kLightWidgets` (priority 10). Exposes a `DrawImGui()` shelf with per-type visibility toggles and a widget scale slider.

Also adds the two new `DebugLayerNames` constants (`kLightWidgets`, `kLightPathArc`) to `DebugLayerNames.h` — both are needed before either feature can build.

---

## Acceptance Criteria

1. `LightWidgetsDrawer` implements `IVisualDebugger` and lives in `DiaLighting3DVisualDebugger.vcxproj`
2. Stores `const LightRegistry3D&` and `const DebugLayerManager&` at construction — no per-call arguments
3. `GetLayerName()` returns `LayerNames::kLightWidgets`
4. `Draw()` emits a sphere for each enabled Point/Spot light and an arrow for each enabled Directional light — all sizes multiplied by `mManager.GetDebugScale() * mWidgetScale`
5. `DrawImGui()` exposes: `"Point lights"` checkbox, `"Spot lights"` checkbox, `"Directional lights"` checkbox, `"Widget scale"` SliderFloat `[0.1, 5.0]`
6. All colours use `DebugColourPalette` constants — no ad-hoc literals
7. `kLightWidgets` and `kLightPathArc` constants added to `DebugLayerNames.h`
8. `DiaVisualDebugger.lib` added as project reference in `DiaLighting3DVisualDebugger.vcxproj`
9. Module YAML doc `dia.lighting3dvisualdebugger.architecture.module.md` created
10. Build passes with no warnings; GoogleTests pass

---

## Draw Class

```cpp
namespace Dia::Lighting3D
{
    class LightWidgetsDrawer : public Dia::Debug::IVisualDebugger
    {
    public:
        LightWidgetsDrawer(const LightRegistry3D&                registry,
                           const Dia::Debug::DebugLayerManager&  manager);

        Dia::Core::StringCRC GetLayerName() const override;
        void Draw    (Dia::Graphics::FrameData& frameData) override;
        void DrawImGui() override;

    private:
        const LightRegistry3D&               mRegistry;
        const Dia::Debug::DebugLayerManager& mManager;

        bool  mShowPointLights       = true;
        bool  mShowSpotLights        = true;
        bool  mShowDirectionalLights = true;
        float mWidgetScale           = 1.0f;
    };
}
```

---

## Draw Logic

**Point lights** (`mShowPointLights == true`):
- Iterate `mRegistry.GetPointCount()` / `GetPointByIndex(i)`
- `RequestDrawSphere(light.position, kBasePointRadius * scale, DebugColourPalette::kWarning)`
- `kBasePointRadius = 0.15f`

**Spot lights** (`mShowSpotLights == true`):
- Iterate `mRegistry.GetSpotCount()` / `GetSpotByIndex(i)`
- Sphere at `light.position`: `RequestDrawSphere(light.position, kBaseSpotRadius * scale, DebugColourPalette::kWarning)`
- Arrow in `light.direction`: `RequestDrawRay(light.position, light.direction, kBaseSpotArrowLen * scale, DebugColourPalette::kWarning)`
- `kBaseSpotRadius = 0.15f`, `kBaseSpotArrowLen = 0.5f`

**Directional lights** (`mShowDirectionalLights == true`):
- Iterate `mRegistry.GetDirectionalCount()` / `GetDirectionalByIndex(i)`
- Arrow from world origin: `RequestDrawRay({0,0,0}, light.direction, kBaseDirArrowLen * scale, DebugColourPalette::kGoal)`
- `kBaseDirArrowLen = 2.0f`

Where `scale = mManager.GetDebugScale() * mWidgetScale`.

All base sizes are `static constexpr float` inside `LightWidgetsDrawer.cpp`.

---

## ImGui Shelf

```cpp
void LightWidgetsDrawer::DrawImGui()
{
    ImGui::Checkbox("Point lights",       &mShowPointLights);
    ImGui::Checkbox("Spot lights",        &mShowSpotLights);
    ImGui::Checkbox("Directional lights", &mShowDirectionalLights);
    ImGui::SliderFloat("Widget scale",    &mWidgetScale, 0.1f, 5.0f);
}
```

---

## New Layer Name Constants

Add to `Dia/DiaVisualDebugger/DebugLayerNames.h` under a new `Lighting 3D` section:

```cpp
// ----------------------------------------------------------------
// Lighting 3D (priority tier 10–19)
// ----------------------------------------------------------------
inline const Dia::Core::StringCRC kLightWidgets { "light3d.widgets"  };
inline const Dia::Core::StringCRC kLightPathArc { "light3d.path_arc" };
```

Both constants are added here (not split across features) since `DebugLayerNames.h` is the single source of truth.

---

## Tasks

| # | Task | Test |
|---|------|------|
| 1 | Add `kLightWidgets` and `kLightPathArc` to `DebugLayerNames.h` | Build passes |
| 2 | Create `Dia/DiaLighting3DVisualDebugger/` directory; create `DiaLighting3DVisualDebugger.vcxproj` and `.vcxproj.filters`; add `DiaVisualDebugger` + `DiaLighting3D` project references; register in `Cluiche.sln` | Build passes |
| 3 | Create `dia.lighting3dvisualdebugger.architecture.module.md` YAML module doc | `dia docs registry` passes |
| 4 | Implement `LightWidgetsDrawer.h/.cpp`; add to vcxproj | Build passes |
| 5 | Write `Cluiche/Tests/GoogleTests/DiaLighting3D/TestLightWidgetsDrawer.cpp`; add to `GoogleTests.vcxproj` | `dia run googletest --filter="DiaLighting3D_LightWidgets*"` all pass |

---

## Test Plan

**File:** `Cluiche/Tests/GoogleTests/DiaLighting3D/TestLightWidgetsDrawer.cpp`

| Suite | Test | What it verifies |
|-------|------|-----------------|
| LightWidgetsDrawer | `LayerName_IsLightWidgets` | `GetLayerName() == LayerNames::kLightWidgets` |
| LightWidgetsDrawer | `Draw_PointLight_EmitsSphere` | 1 point light → 1 sphere primitive |
| LightWidgetsDrawer | `Draw_PointLight_Colour_IsWarning` | Sphere colour == `kWarning` |
| LightWidgetsDrawer | `Draw_SpotLight_EmitsSphereAndArrow` | 1 spot light → sphere + arrow primitives |
| LightWidgetsDrawer | `Draw_DirectionalLight_EmitsArrow` | 1 directional light → 1 arrow from origin |
| LightWidgetsDrawer | `Draw_DirectionalLight_Colour_IsGoal` | Arrow colour == `kGoal` |
| LightWidgetsDrawer | `Draw_PointLightsDisabled_NoPrimitivesForPoint` | `mShowPointLights = false` → no point sphere primitives |
| LightWidgetsDrawer | `Draw_DebugScale_AffectsSphereRadius` | `manager.SetDebugScale(2.0f)` → sphere radius doubles |
| LightWidgetsDrawer | `Draw_Disabled_NoPrimitives` | `SetEnabled(false)` → 0 primitives |
| LightWidgetsDrawer | `Draw_EmptyRegistry_NoPrimitives` | Empty registry → 0 primitives |

---

## Inherited Binding Decisions

| ID | Decision | Compliance |
|----|----------|-----------|
| PD-001 | StringCRC for IDs | `GetLayerName()` returns `LayerNames::kLightWidgets` |
| PD-004 | No STL in public API | Constructor and `Draw()` use refs and engine types only |
| PD-007 | C++20 required | Compiled under `/std:c++20` |
| AD-001 | Module YAML frontmatter | `dia.lighting3dvisualdebugger.architecture.module.md` created in Task 3 |
| AD-003 | Namespace `Dia::<Module>::` | `Dia::Lighting3D::LightWidgetsDrawer` |
| SD-DBG-001 | Stack of focused draw classes | This is one focused draw class; `LightPathArcDrawer` is the other |
| SD-DBG-003 | Priority-ordered draw | Registered at priority 10 |
| SD-DBG-005 | Global `debugScale` | All sizes multiplied by `mManager.GetDebugScale() * mWidgetScale` |
| SD-DBG-010 | `DebugColourPalette` colours | `kWarning` for point/spot, `kGoal` for directional |
| SD-DBG-014 | Same-family classes share vcxproj | Both draw classes in `DiaLighting3DVisualDebugger.vcxproj` |
| SD-LVD-001 | Separate static library | `DiaLighting3DVisualDebugger.vcxproj` created in Task 2 |
| SD-LVD-002 | No debug fields on light structs | No changes to DiaLighting3D data types |
| SD-LVD-003 | Draw all lights | No per-light opt-in flag; layer toggle is the control |

---

## Status

`Approved`
