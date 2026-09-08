# System Spec: DiaScene2DVisualDebugger

## Parent Application
@docs/specs/applications/dia/dia.md

**Gameplay Domains:** debug

## Purpose

DiaScene2DVisualDebugger is the `IDebugDomain` implementation that makes 2D scene structure visible in `DiaDebugPanel`. It surfaces active camera state, 2D light positions, and per-layer bounds as world-space overlays, and emits scene stats to the panel.

The live implementation is `Scene2DDebugDomain` in `Dia/DiaScene2DVisualDebugger/`. The current `SceneOverviewDrawer` is monolithic (cameras + lights + layers in one class). This spec calls for splitting it into three focused drawers so each concern can be toggled independently.

**Migration:** See `@docs/specs/applications/dia/systems/diadebugdomain/domain-migration.md`.

## Domain Identity

| Property | Value |
|----------|-------|
| Domain ID | `"Scene2D"` |
| Display name | `"Scene2D"` |
| Description | `"2D scene — cameras, lights, layer bounds, active camera info"` |
| Group | `"Rendering"` |
| Accent | `DebugGroupAccents::kRendering` (`#8b5cf6`) |
| `HasWorldDrawers()` | `true` |
| Drawers | Cameras, Lights, LayerBounds |

## Panel Card Specification

Per `debugger-contract.md` AC-16 and `docs/research/visual_debugger_redesign/mockup.html`:

- **Group:** Rendering — accent `DebugGroupAccents::kRendering` (`#8b5cf6`) via `var(--accent)`
- **Spacing:** group header `padding: 5px 10px`, domain header `padding: 4px 8px`, domain body `padding: 6px 10px 8px 10px`, `margin-bottom: 3px` between cards, drawer rows `gap: 5px 10px`
- **Stat line:** `"Cam: <activeCam>"`
- **Expanded body:**
  - Drawer toggles: Cameras, Lights, LayerBounds
  - Stats table: Camera count · Light count · Layer count
  - Camera info row: pos (X.x, Y.y) · zoom N.n · rot N.n°

## JSON State Schema

```json
{
  "drawers": [
    { "name": "Cameras",    "enabled": true  },
    { "name": "Lights",     "enabled": false },
    { "name": "LayerBounds","enabled": false }
  ],
  "stats": {
    "activeCam":   "main",
    "cameraCount": 2,
    "lightCount":  4,
    "layerCount":  5
  },
  "camera": {
    "pos":      [120.0, 80.0],
    "zoom":     1.0,
    "rotation": 0.0
  }
}
```

## Drawer Specifications

**CamerasDrawer:** draws a viewport-bounds wireframe for each registered camera in world space; active camera wireframe uses `var(--accent)` color; inactive cameras use a dimmed palette color.

**LightsDrawer:** draws a cross/icon at each `LightRegistry2D` light position, sized by `debugScale`.

**LayerBoundsDrawer:** draws world-space bounding rects for each layer defined in `LayerTable`; labels each rect with the layer name via `RequestDrawText`.

## Domain ACs

In addition to all 16 ACs in `@docs/specs/applications/dia/systems/diadebugdomain/debugger-contract.md`:

| AC | Criterion |
|----|-----------|
| D-1 | `"stats.activeCam"` = StringCRC name of the camera registered as active in `CameraRegistry2D`; `"(none)"` when no active camera |
| D-2 | `"camera.pos"`, `"camera.zoom"`, `"camera.rotation"` reflect the active camera's values this frame; updated every frame |
| D-3 | `CamerasDrawer` draws a viewport-bounds wireframe for every registered camera; active camera uses accent color |
| D-4 | `LightsDrawer` draws a cross icon at each `LightRegistry2D` position |
| D-5 | `LayerBoundsDrawer` draws a labeled bounding rect for each `LayerTable` layer |
| D-6 | Splitting `SceneOverviewDrawer` into three drawers (Cameras, Lights, LayerBounds) — each independently toggleable |
| D-7 | No `DrawImGui()` call anywhere in this module |
| D-8 | Panel card layout complies with AC-16 spacing contract (see Panel Card Specification above) |

## Status

**Status:** Approved
