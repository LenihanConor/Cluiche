**Spec:** @docs/specs/applications/dia/systems/diavisualdebugger/coord3d-debug-overlay.md
**Status:** Done

## Implementation Patterns

- **Drawer pattern:** All four drawers inherit `IVisualDebugger` and are guarded by `#ifdef DIA_DEBUG`. They receive a `const DebugLayerManager&` in their constructor (for camera access), same as Coord2D drawers. `GetLayerName()` returns the relevant `LayerNames::kCoord3D*` constant.
- **3D draw calls:** `Draw()` casts `IDebugDraw&` to `Dia::Graphics::DebugFrameData&` (same pattern as `MeshOriginDrawer`) and calls `RequestDrawRay3D` / `RequestDrawLine3D` directly.
- **Camera access:** `DebugLayerManager::GetCamera3D()` returns the stored `Camera3D`; drawers call this in `Draw()`. `VisualDebuggerModule` calls `mLayerManager.SetCamera3D(camera)` each frame before `Draw()`.
- **Registration:** All four drawers registered with `RegisterWithoutDraw` + `kCoord3DStageTag`. `VisualDebuggerModule` exposes `SetCamera3D(const Camera3D&)` and `DrawCoord3D(FrameData3D&)` for stage modules to call each frame.
- **Stage wiring:** `Mesh3DTestStageModule::OnUpdate()` calls `vd->SetCamera3D(camera)` and `vd->DrawCoord3D(mFrame)` in the `#ifdef DIA_DEBUG` block, after the mesh draws.
- **Colours:** X=`kError` (red), Y=`kHealthy` (green), Z=`kGoal` (blue/cyan) for origin/axes. Grid=`kInactive`. Camera drawer is ImGui-only (no geometry).

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Layer name constants | Build succeeds; constants accessible | Done | haiku | |
| 2 | SetCamera3D / GetCamera3D on DebugLayerManager | Build succeeds | Done | sonnet | Camera3D.h included directly in .h (value member) |
| 3 | Coord3DOriginDrawer | Visual: RGB crosshair at world origin | Done | sonnet | |
| 4 | Coord3DAxesDrawer | Visual: X/Y/Z axis lines ±N units | Done | sonnet | |
| 5 | Coord3DGridDrawer | Visual: XZ ground plane grid | Done | sonnet | |
| 6 | Coord3DCameraDrawer | ImGui panel shows eye/dir/clip | Done | sonnet | |
| 7 | VisualDebuggerModule wiring | Console shows Coord3D tab in Mesh3DTestStage | Done | sonnet | Added imgui include path to DiaVisualDebugger.vcxproj |
| 8 | vcxproj updates | dia run cluichetest builds clean | Done | haiku | |
