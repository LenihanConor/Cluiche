# Feature Spec: Coord2D Debug Overlay

**Parent:** @docs/specs/systems/dia/diavisualdebugger.md
**Status:** `Done`
**Plan:** [coord2d-debug-overlay.plan.md](coord2d-debug-overlay.plan.md)

---

## Summary

A "Coord2D" domain tab in DiaVisualDebuggerConsole with toggleable debug draw layers that visualize the 2D coordinate system: origin marker, colored axes, world-space grid with labels, viewport bounds with coordinate labels, and live cursor world-position display. Includes the prerequisite Camera2D and ViewportTransform classes in DiaGraphics that enable screen↔world coordinate conversion.

## Problem

When working in 2D, developers have no quick way to see where the origin is, what the coordinate extents are, or what world position the cursor is at — leading to confusion about orientation, scale, and bounds.

---

## Goals

1. Give developers instant spatial orientation in any 2D stage via toggle-on overlays
2. Provide reusable Camera2D + ViewportTransform infrastructure in DiaGraphics for screen↔world conversion
3. Integrate cleanly into the existing DiaVisualDebuggerConsole tab system

---

## Acceptance Criteria

| # | Criterion |
|---|-----------|
| AC1 | `Camera2D` class in `DiaGraphics/Camera/` — stores position, zoom, rotation |
| AC2 | `ViewportTransform` class in `DiaGraphics/Camera/` — screen→world and world→screen conversion given Camera2D + window dimensions |
| AC2b | DiaBgfx renderers (DebugRenderer, SpriteRenderer) apply Camera2D to their projection — panning/zooming the camera moves the rendered world |
| AC3 | A "Coord2D" domain tab appears in DiaVisualDebuggerConsole with layer toggles |
| AC4 | Toggle "Origin" draws a crosshair at (0,0) |
| AC5 | Toggle "Axes" draws colored axis lines (red=X, green=Y) spanning the visible viewport |
| AC6 | Toggle "Grid" draws a world-space grid with coordinate labels at adaptive intervals |
| AC7 | Toggle "Bounds" draws corner labels showing min/max world coordinates of the viewport |
| AC8 | Toggle "Cursor" displays live world coords at mouse position |
| AC9 | All overlay layers use `DebugColourPalette` colours (SD-DBG-010) |
| AC10 | Layers register at priority 50+ (overlay tier) in DebugLayerManager |

---

## Tasks

| # | Task | Description |
|---|------|-------------|
| 1 | Camera2D class | Create `DiaGraphics/Camera/Camera2D.h` — position (Vector2D), zoom (float), rotation (float). Immutable value type. |
| 2 | ViewportTransform class | Create `DiaGraphics/Camera/ViewportTransform.h` — constructed from Camera2D + window dimensions. Methods: `ScreenToWorld(Vector2D pixel)`, `WorldToScreen(Vector2D world)`, `GetWorldBounds()` returning min/max corners. |
| 3 | Layer name constants | Add `coord2d.*` layer names to `DebugLayerNames.h`: `kCoord2DOrigin`, `kCoord2DAxes`, `kCoord2DGrid`, `kCoord2DBounds`, `kCoord2DCursor` |
| 4 | Origin layer | `Coord2DOriginDrawer : IVisualDebugger` — crosshair at (0,0) using Line2D primitives |
| 5 | Axes layer | `Coord2DAxesDrawer : IVisualDebugger` — red X-axis line, green Y-axis line spanning viewport bounds. Reads ViewportTransform from DebugLayerManager. |
| 6 | Grid layer | `Coord2DGridDrawer : IVisualDebugger` — world-space grid lines with Text2D labels at intervals. Grid spacing adapts to zoom level. |
| 7 | Bounds layer | `Coord2DBoundsDrawer : IVisualDebugger` — Text2D at four viewport corners showing world coordinates |
| 8 | Cursor layer | `Coord2DCursorDrawer : IVisualDebugger` — Text2D at mouse position showing world coords. Reads mouse pixel position from a stream (cross-PU thread-safe), converts via ViewportTransform from DebugLayerManager. |
| 9 | DebugLayerManager SetViewport | Add `SetViewport(Camera2D, windowSize)` to DebugLayerManager; internally builds ViewportTransform; layers access via `GetViewportTransform()` |
| 10 | Console tab registration | Register "coord2d" domain prefix in DiaVisualDebuggerConsole so all `coord2d.*` layers appear under a "Coord2D" tab |
| 11 | DiaBgfx renderer integration | DebugRenderer and SpriteRenderer read Camera2D from FrameData (or a channel) and incorporate position/zoom/rotation into their ortho projection matrix instead of fixed 0→canvasSize |
| 12 | vcxproj updates | Add new Camera/ files to DiaGraphics.vcxproj; add new Coord2D/ drawer files to DiaVisualDebugger.vcxproj |
| 13 | Google Tests | Unit tests for Camera2D and ViewportTransform (screen↔world roundtrip, zoom, rotation, bounds calculation) |

---

## Dependencies

- **DiaGraphics** — `FrameData`, `DebugFrameData`, `Transform.h`, `RGBA` (existing); new Camera2D + ViewportTransform added here
- **DiaBgfx** — DebugRenderer + SpriteRenderer modified to apply Camera2D to their ortho projection
- **DiaVisualDebugger** — `IVisualDebugger`, `DebugLayerManager`, `DebugColourPalette`, `DebugLayerNames`
- **DiaVisualDebuggerConsole** — tab registration for "Coord2D" domain
- **DiaInput** — mouse position for cursor layer (via stream)
- **DiaMaths** — `Vector2D`, matrix operations

---

## Binding Decisions

| ID | Decision | Compliance |
|----|----------|------------|
| SD-DBG-001 | Stack of focused draw classes | Each visualization is its own `IVisualDebugger` implementation, independently toggleable |
| SD-DBG-002 | `#ifdef DIA_DEBUG` guards | All coord2d draw classes guarded; Camera2D/ViewportTransform are NOT guarded (reusable in Release) |
| SD-DBG-003 | Priority tiers | Coord2D layers at priority 50+ (overlay tier) — always on top |
| SD-DBG-006 | Layer name collision assert | All names defined as constants in `DebugLayerNames.h` with `coord2d.*` prefix |
| SD-DBG-010 | DebugColourPalette colours | All layers use palette colours exclusively |
| PD-004 | No STL in public APIs | Camera2D and ViewportTransform use DiaMaths types, not STL |

---

## Out of Scope

- **Camera controller** (pan/zoom input handling) — application-side code. CluicheTest stages wire keyboard/mouse input to Camera2D themselves. This feature provides the data types and renderer integration; the caller drives movement.

---

## Design Decisions

1. **Camera2D ownership:** Application code (test stage, game) creates a Camera2D from its view setup each frame and sets it on `DebugLayerManager` via `SetViewport(Camera2D, windowSize)`. The manager builds a `ViewportTransform` internally; overlay layers read it from the manager. No coupling to any specific stage's camera implementation.

2. **Grid density:** Minimal — single grid level only. Largest power-of-10 spacing where at least 4–5 lines are visible. No sub-grid.

3. **Cursor input:** Mouse pixel position arrives via the stream system (cross-PU thread boundary). The cursor layer reads from a stream, not directly from DiaInput. Consistent with ServiceChannel/FrameStream architecture.
