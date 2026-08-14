# Feature Spec: Coord3D Debug Overlay

**Parent:** @docs/specs/applications/dia/systems/diavisualdebugger/diavisualdebugger.md
**Status:** `Done`
**Plan:** [coord3d-debug-overlay.plan.md](coord3d-debug-overlay.plan.md)

---

## Summary

A "Coord3D" domain tab in DiaVisualDebuggerConsole with toggleable debug draw layers that visualise the 3D coordinate system: an RGB axis crosshair at world origin, full-span X/Y/Z axis lines, an XZ ground-plane grid with power-of-10 spacing, and a camera-info panel showing eye position, look direction, and clip planes. Registered globally by `VisualDebuggerModule` so every 3D stage gets the tab for free.

## Problem

Developers working in 3D test stages have no shared world-space coordinate reference (axes, grid, origin) in the debug console, forcing them to reason about 3D space without visual anchors.

---

## Goals

1. Give developers instant spatial orientation in any 3D stage via toggle-on overlays — no per-stage wiring
2. Mirror the Coord2D pattern: globally registered in `VisualDebuggerModule`, off by default, independently toggleable
3. Reuse the existing 3D primitive API (`RequestDrawRay3D`, `RequestDrawLine3D`) — no new draw primitives required

---

## Acceptance Criteria

| # | Criterion |
|---|-----------|
| AC1 | A "Coord3D" domain card appears in DiaDebugPanel when the debug console is open |
| AC2 | `Coord3DOriginDrawer` draws an RGB crosshair at world (0,0,0) — X=red (`kError`), Y=green (`kHealthy`), Z=blue (`kGoal`) — via `RequestDrawRay3D` × 3 |
| AC3 | `Coord3DAxesDrawer` draws full X/Y/Z axis lines extending ±N world units from origin; each arm only drawn if its axis origin vertex is in front of the near clip plane |
| AC4 | `Coord3DGridDrawer` draws an XZ ground-plane grid with power-of-10 line spacing driven by camera distance; up to 200 lines |
| AC5 | `Coord3DCameraDrawer` emits camera eye position, look direction, FOV (degrees), and near/far clip distances via `GetJSONState()` as `camera.{eye[3], dir[3], fov_deg, near, far}`; panel stat rows show these values — no world-space geometry; `DrawImGui()` is retired |
| AC6 | All four layers are off by default; each toggled independently via the panel card |
| AC7 | Layers are registered globally by `VisualDebuggerModule::RegisterCoord3DDrawers()` — no per-stage wiring required |
| AC8 | The feature works in `Mesh3DTestStage` and any future 3D stage without additional code |
| AC9 | All geometry layers use `DebugColourPalette` colours (SD-DBG-010) |
| AC10 | Layers register at priority 50–53 (overlay tier) in `DebugLayerManager` |
| AC11 | `GetJSONState()` emits: `drawers[]` (4 entries: Origin/Axes/Grid/Camera with enabled flag), `camera.eye[3]`, `camera.dir[3]`, `camera.fov_deg`, `camera.near`, `camera.far` |
| AC12 | Camera stats are emitted every frame (live stat row updates); the Camera drawer toggle controls whether the expanded card section shows the full stats table |
| AC13 | Panel card layout complies with the spacing contract in `debugger-contract.md` AC-16: group header `padding: 5px 10px`, domain header `padding: 4px 8px`, domain body `padding: 6px 10px 8px 10px`, `margin-bottom: 3px` between cards, drawer rows `gap: 5px 10px`; accent via `var(--accent)` |

---

## Tasks

| # | Task | Description |
|---|------|-------------|
| 1 | Layer name constants | Add `coord3d.*` layer names and `kCoord3DStageTag` to `DebugLayerNames.h` |
| 2 | `SetCamera3D` / `GetCamera3D` on `DebugLayerManager` | Add `SetCamera3D(const Camera3D&)` and `GetCamera3D() const` — analogous to `SetViewport` / `GetViewportTransform` |
| 3 | `Coord3DOriginDrawer` | RGB crosshair at (0,0,0); arm length configurable via `DebugLayerManager::GetDebugScale()`; `RequestDrawRay3D` × 3 |
| 4 | `Coord3DAxesDrawer` | ±N axis lines; skip any arm whose origin vertex is behind the camera near plane; N = configurable via `DrawImGui()` slider (default 100 world units) |
| 5 | `Coord3DGridDrawer` | XZ ground-plane grid; spacing = largest power-of-10 where ≥ 4 lines fit in camera view distance; `RequestDrawLine3D` × up to 200 |
| 6 | `Coord3DCameraDrawer` | Override `GetJSONState()` to emit camera stats (eye, dir, fov_deg, near, far) from the `Camera3D` set via `SetCamera3D()`; `Draw()` is a no-op — panel stats only; `DrawImGui()` is retired |
| 7 | `VisualDebuggerModule` wiring | Add `RegisterCoord3DDrawers()`; call `mLayerManager.SetCamera3D(camera)` each frame in `DoUpdate()`; expose `SetCamera3D(const Camera3D&)` on `VisualDebuggerModule` for stage modules to call |
| 8 | vcxproj updates | Add `Coord3D/` drawer files to `DiaVisualDebugger.vcxproj` + `.vcxproj.filters` |

---

## Dependencies

- **DiaVisualDebugger** — `IVisualDebugger`, `DebugLayerManager`, `DebugColourPalette`, `DebugLayerNames`
- **DiaGraphics / DiaBgfx3D** — `Camera3D`, `DebugFrameData` (3D primitive methods: `RequestDrawRay3D`, `RequestDrawLine3D`)
- **DiaVisualDebuggerConsole** — tab rendered automatically from `kCoord3DStageTag`; no console changes required
- **CluicheGameBaseline** — `VisualDebuggerModule` wiring; stage modules call `SetCamera3D()` each frame
- **DiaMaths** — `Vector3D`, `Matrix4x4` (camera frustum clip check)

---

## Binding Decisions

| ID | Decision | Compliance |
|----|----------|------------|
| SD-DBG-001 | Stack of focused draw classes | Each visualisation is its own `IVisualDebugger`, independently toggleable |
| SD-DBG-002 | `#ifdef DIA_DEBUG` guards | All Coord3D draw classes guarded |
| SD-DBG-003 | Priority tiers | Coord3D layers at priority 50–53 (overlay tier) |
| SD-DBG-006 | Layer name collision assert | All names defined as constants in `DebugLayerNames.h` with `coord3d.*` prefix |
| SD-DBG-010 | `DebugColourPalette` colours | All geometry layers use palette colours exclusively |
| PD-004 | No STL in public APIs | All new classes use DiaMaths types and `DynamicArrayC`; no STL |

---

## Open Design Questions

1. **Camera3D push ownership.** `VisualDebuggerModule` currently has no access to `Camera3D` — it only holds a `Camera2D`. The stage module (e.g. `Mesh3DTestStageModule`) holds the live `FrameData3D` which carries the camera. The cleanest pattern is `VisualDebuggerModule::SetCamera3D(const Camera3D&)` called by the stage each frame — but this means every 3D stage must remember to call it (one line, but it is per-stage). The alternative is passing `FrameData3D` into `VisualDebuggerModule::DoUpdate()`, but that would couple a shared baseline module to a 3D-specific frame type. Preferred: the per-stage `SetCamera3D()` call.

2. **`RegisterWithoutDraw` vs normal `Register`.** Coord3D layers draw into a `FrameData3D`, not the 2D `FrameData` that `DebugLayerManager::Draw()` passes. They must use `RegisterWithoutDraw` and be driven manually by whoever calls `Draw()` in 3D space. The question is whether `VisualDebuggerModule::DoUpdate()` calls them (it has the camera, but not a `FrameData3D`) or whether each 3D stage module calls them after registering. Resolution: `VisualDebuggerModule` owns registration and camera setting; the stage's render phase calls `mVisualDebuggerModule.DrawCoord3D(frameData3D)` — a thin forwarding method that iterates and calls each Coord3D drawer's `Draw(frameData3D)` if enabled.

3. **Axis clip-plane check precision.** `Coord3DAxesDrawer` skips arms whose origin vertex is behind the near plane. A full frustum test per arm is overkill — a simple dot product of the arm direction with the camera forward vector (skip if camera is facing away from the axis half) is sufficient and avoids a full clip. Worth confirming the exact threshold during implementation.
