# System Spec: DiaLighting3DVisualDebugger

## Parent Application
@docs/specs/applications/dia/dia.md

**Status:** `Done`

---

## Purpose

DiaLighting3DVisualDebugger is the visual debug rendering module for 3D lights. It reads from `LightRegistry3D` and draws per-light debug widgets using DiaVisualDebugger primitives. It follows the same sibling pattern as `DiaRigidBody2DVisualDebugger` and `DiaGeometry2DVisualDebugger` — a **separate static library** that bridges DiaLighting3D (no graphics dependency) and DiaVisualDebugger, leaving zero debug footprint in the main lighting library.

Two focused `IVisualDebugger` draw classes replace the monolithic design:
- `LightWidgetsDrawer` — sphere/arrow widgets per light type
- `LightPathArcDrawer` — spline arc preview when a `LightPathBehaviour3D` is attached

Each stores dependencies at construction (registry ref + manager ref), implements `GetLayerName()` / `Draw(FrameData&)` / `DrawImGui()`, and is registered independently with `DebugLayerManager`.

**Dependency chain:**
`DiaLighting3DVisualDebugger → DiaLighting3D + DiaGeometry3D + DiaVisualDebugger → DiaMaths → DiaCore`

---

## Responsibilities

- Read `LightRegistry3D` each frame and submit debug geometry to DiaVisualDebugger
- Draw per-light widgets: sphere at position (Point/Spot), arrow from origin in direction (Directional)
- Draw spline arc (N sampled segments) when a `LightPathBehaviour3D` is attached to the light
- Expose an ImGui shelf (`DrawImGui()`) per drawer for per-type toggles and tunable parameters
- Use `DebugColourPalette` constants — no ad-hoc colour literals
- Multiply all visual sizes by `mManager.GetDebugScale()`
- Use `DebugLayerNames::kLight*` canonical layer name constants
- Provide `DiaLighting3DVisualDebugger.vcxproj` static library project registered in `Cluiche.sln`
- Provide `dia.lighting3dvisualdebugger.architecture.module.md` YAML module documentation

## Non-Responsibilities

- Any mutation of lights — read-only consumer
- Rendering of light volumes, shadow cones, or falloff visualisation (belongs in DiaBgfx3D debug layer)
- Editor UI panels — those belong in a future editor plugin
- 2D light visualisation — DiaLighting2D is a separate system
- Adding debug fields to `PointLight3D` / `SpotLight3D` / `DirectionalLight3D` structs — no debug data bleeds into the main library

---

## Public Interfaces

### `LightWidgetsDrawer`

```cpp
namespace Dia::Lighting3D
{
    class LightWidgetsDrawer : public Dia::Debug::IVisualDebugger
    {
    public:
        LightWidgetsDrawer(const LightRegistry3D&            registry,
                           const Dia::Debug::DebugLayerManager& manager);

        Dia::Core::StringCRC GetLayerName() const override;   // LayerNames::kLightWidgets
        void Draw    (Dia::Graphics::FrameData& frameData) override;
        void DrawImGui() override;

    private:
        const LightRegistry3D&               mRegistry;
        const Dia::Debug::DebugLayerManager& mManager;

        bool mShowPointLights       = true;
        bool mShowSpotLights        = true;
        bool mShowDirectionalLights = true;
        float mWidgetScale          = 1.0f;   // multiplies base sizes before debugScale
    };
}
```

**Widget visuals per light type:**

| Light type | Widget | Colour |
|---|---|---|
| `PointLight3D` | Sphere at `position`, base radius = 0.15 m × debugScale × widgetScale | `kWarning` (yellow) |
| `SpotLight3D` | Sphere at `position` + arrow in `direction`, base arrow length = 0.5 m | `kWarning` (yellow) |
| `DirectionalLight3D` | Arrow from `(0,0,0)` in `direction`, base length = 2.0 m | `kGoal` (cyan) |

`AmbientLight3D` has no position or direction — no widget drawn.

**ImGui shelf (`DrawImGui()`):**
- Checkbox `"Point lights"` → `mShowPointLights`
- Checkbox `"Spot lights"` → `mShowSpotLights`
- Checkbox `"Directional lights"` → `mShowDirectionalLights`
- SliderFloat `"Widget scale"` → `mWidgetScale` in `[0.1, 5.0]`

---

### `LightPathArcDrawer`

```cpp
namespace Dia::Lighting3D
{
    class LightPathArcDrawer : public Dia::Debug::IVisualDebugger
    {
    public:
        LightPathArcDrawer(const LightRegistry3D&            registry,
                           const Dia::Debug::DebugLayerManager& manager);

        Dia::Core::StringCRC GetLayerName() const override;   // LayerNames::kLightPathArc
        void Draw    (Dia::Graphics::FrameData& frameData) override;
        void DrawImGui() override;

    private:
        const LightRegistry3D&               mRegistry;
        const Dia::Debug::DebugLayerManager& mManager;

        int  mArcSamples = 32;   // number of line segments along sampled spline
    };
}
```

**Draw logic:**
- For each light in the registry, calls `registry.GetPathBehaviour(lightId)` (accessor added in `LightPathBehaviour3D` feature)
- If nullptr, skips
- Samples `Spline3D::Evaluate(t)` at `mArcSamples + 1` evenly spaced `t` values over `[0, 1]`
- Emits one `RequestDrawLine(p0, p1, colour)` per adjacent pair
- Point/Spot lights: `kHealthy` (green) arc
- Directional lights: `kGoal` (cyan) arc (sweeps direction in XZ plane)

**ImGui shelf (`DrawImGui()`):**
- SliderInt `"Arc samples"` → `mArcSamples` in `[8, 64]`

---

## New Layer Name Constants (add to `DebugLayerNames.h`)

```cpp
// Lighting 3D (priority tier 10–19)
inline const Dia::Core::StringCRC kLightWidgets { "light3d.widgets"  };
inline const Dia::Core::StringCRC kLightPathArc { "light3d.path_arc" };
```

---

## Registration Example (game / test stage code)

```cpp
static Dia::Lighting3D::LightWidgetsDrawer  widgetsDrawer (lightRegistry, debugManager);
static Dia::Lighting3D::LightPathArcDrawer  pathArcDrawer (lightRegistry, debugManager);

debugManager.Register(&widgetsDrawer, 10);
debugManager.Register(&pathArcDrawer, 15);

// Each frame, after LightRegistry3D::UpdateAll(dt):
debugManager.Draw(frameData);
```

---

## Features

| Feature | Description | Spec |
|---|---|---|
| `debug-widget-config` | Add `LightWidgetsDrawer` — sphere/arrow widgets per light type with ImGui shelf | [debug-widget-config.md](debug-widget-config.md) |
| `path-arc-preview` | Add `LightPathArcDrawer` — sample spline and render arc when `LightPathBehaviour3D` is attached | [path-arc-preview.md](path-arc-preview.md) |

---

## Dependencies

| Dependency | What's used |
|---|---|
| DiaLighting3D | `LightRegistry3D`, light value types, `LightPathBehaviour3D`, `registry.GetPathBehaviour()` |
| DiaGeometry3D | `Spline3D::Evaluate()` for arc sampling |
| DiaVisualDebugger | `IVisualDebugger`, `DebugLayerManager`, `DebugColourPalette`, `DebugLayerNames` |
| DiaMaths | `Vector3D` |
| DiaCore | `StringCRC` |

### Does NOT depend on

- DiaGraphics / DiaGraphics3D (DiaVisualDebugger abstracts drawing)
- DiaBgfx / DiaBgfx3D
- DiaApplicationFlow
- DiaScene3D

---

## System Decisions

| ID | Decision | Rationale | Binding |
|----|----------|-----------|---------|
| SD-LVD-001 | Separate static library, not a subdirectory of DiaLighting3D | DiaLighting3D must have zero DiaVisualDebugger dependency; this is the only place lighting + debug concerns mix | Yes |
| SD-LVD-002 | No debug fields on light structs | `DebugWidgetConfig` field on light types was considered and rejected — it bleeds debug data into release builds. Draw-all-visible is simpler and zero-cost in Release since the whole library is excluded | Yes |
| SD-LVD-003 | Draw all lights (no opt-in flag per light) | An opt-in `enabled` flag on each light struct would require touching DiaLighting3D. The layer enable/disable toggle on `DebugLayerManager` provides sufficient granularity | Yes |
| SD-LVD-004 | Arc sampled as line segments, not sphere markers | Line segments are cheaper and cleaner for a path preview; sphere markers were in the original design but are redundant given the widget sphere at the light's current position | No |

---

## Inherited Binding Decisions

| ID | Decision | How it applies |
|---|---|---|
| PD-001 | StringCRC for IDs | Layer names are `LayerNames::kLight*` constants |
| PD-004 | No STL in public API | `Draw()` takes `FrameData&` by reference; constructors take refs; no STL in signatures |
| PD-005 | x64 only | Single build target |
| PD-007 | C++20 required | Compiled under `/std:c++20` |
| PD-008 | Directory.Build.props owns build paths | No per-project OutDir/IntDir overrides |
| AD-001 | Module system with YAML frontmatter | `dia.lighting3dvisualdebugger.architecture.module.md` required |
| AD-003 | Namespace `Dia::<Module>::` | `Dia::Lighting3D::` (companion to DiaLighting3D, same domain namespace) |
| SD-DBG-001 | Stack of focused draw classes | Two focused draw classes replace the monolithic `LightingDebugRenderer3D` |
| SD-DBG-003 | Priority-ordered draw | `kLightWidgets` at priority 10, `kLightPathArc` at priority 15 |
| SD-DBG-005 | Global `debugScale` | All base sizes (sphere radius, arrow length, arc segment lengths) multiplied by `mManager.GetDebugScale()` |
| SD-DBG-010 | `DebugColourPalette` colours | All colours use palette constants — `kWarning` for point/spot widgets, `kGoal` for directional, `kHealthy` for path arcs |
| SD-DBG-014 | Same-family classes share vcxproj | Both draw classes live in `DiaLighting3DVisualDebugger.vcxproj` |

---

## Resolved Design Questions

1. **No debug fields on light structs.** The original spec added `DebugWidgetConfig` to `PointLight3D` etc. inside DiaLighting3D. Rejected: it adds debug data to release builds and couples the main library to a debug concern. The debugger draws all registered lights; `DebugLayerManager`'s enable/disable toggle is the visibility control.

2. **`GetPathBehaviour()` on `LightRegistry3D` is a prerequisite.** This accessor must be added in the `LightPathBehaviour3D` feature spec (already listed as a resolved design question there). `LightPathArcDrawer` depends on it; it is a prerequisite task in the `path-arc-preview` feature.

3. **Directional light arc visualisation: arc line in XZ plane.** A single continuous sampled line arc communicates the sweep range. No ghost arrows. N=32 default samples, tunable via ImGui shelf.

4. **`DrawImGui()` shelf per drawer, not a shared console.** Each drawer owns its own controls in the layer list entry. `LightWidgetsDrawer` exposes type toggles and widget scale; `LightPathArcDrawer` exposes sample count. This matches the per-drawer shelf pattern used by `AnimClipCursorDrawer`, `VelocityArrowsDrawer`, etc.

---

## Status

`Approved`
