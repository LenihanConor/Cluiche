**Spec:** @docs/specs/applications/dia/systems/diavisualdebugger/debug-3d-primitives.md
**Status:** Done

---

## Implementation Patterns

- **DebugPrimitive.h** — add 5 enum values (Line3D=7..Arrow3D=11) and 5 POD structs to the union. All structs must be trivially copyable. `Vector3D` is in `<DiaMaths/Maths/Vector3D.h>`.
- **DebugFrameData** — mirror existing 2D pattern: `DynamicArrayC<DebugPrimitive, kDebug3DCapacity>`, `mDroppedDebug3DCount`, `RequestDraw*3D` methods all guarded by `#ifdef DIA_DEBUG`, `GetDebug3DPrimitive()`/`GetDebug3DPrimitiveCount()`/`DroppedDebug3DCount()` accessors.
- **DebugRenderer** — add `case Line3D: case Ray3D: case Box3D: case Sphere3D: case Arrow3D: break;` (or `default: break`) to the switch in `Draw()`. No other changes.
- **Shaders** — `vs_debug3d.sc` uses `u_viewProj` (not `u_modelViewProj`) with `vec4(a_position, 1.0)` for full 3D position; `fs_debug3d.sc` identical to `fs_debug.sc`. Compile via `shaderc` for dx11/dx12/vulkan; output `.bin` files in `Dia/DiaBgfx3D/Shaders/3d/`.
- **DebugGeometry3DRenderer** — vertex layout `(x,y,z,abgr)`, `TransientVertexBuffer` + `BGFX_STATE_PT_LINES`. Primitives expanded inline: Line3D→2v, Ray3D→6v, Box3D→24v, Sphere3D→144v, Arrow3D→10v. View transform set from `Camera3D.view`/`.projection` via `GetColumnMajor()`.
- **Canvas3D** — `kDebug3DViewId = 3`, new owned `DebugGeometry3DRenderer*`, init in `Init3DPrograms()`, called in `ProcessFrame()` after mesh pass, before `Canvas::ProcessFrame()`.
- **GoogleTests** — struct layout assertions (`sizeof`, `offsetof`), slot counting (single RequestDraw bumps count by 1), dropped-count increments on overflow at `kDebug3DCapacity`.

---

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Add 5 enum values + 5 POD structs to `DebugPrimitive.h`; add `kDebug3DCapacity`, second `DynamicArrayC`, 5 `RequestDraw*3D`, accessors to `DebugFrameData.h/.cpp` | Build passes; slot counting correct | Done | sonnet | Build clean |
| 2 | Add silent no-op fallthrough for Line3D/Ray3D/Box3D/Sphere3D/Arrow3D to `DiaBgfx::DebugRenderer::Draw()` | Build passes; existing tests unaffected | Done | haiku | Build clean |
| 3 | Write `vs_debug3d.sc` / `fs_debug3d.sc`; compile to `.bin` for dx11/dx12/vulkan | Shader files exist; Canvas3D loads them | Done | sonnet | All 6 .bin files cooked by pipeline |
| 4 | Implement `DebugGeometry3DRenderer.h/.cpp`; add to `DiaBgfx3D.vcxproj` | Build passes | Done | sonnet | Build clean; Vector3D API confirmed (Dot/Cross/AsNormal) |
| 5 | Wire into `Canvas3D`: add member, init, call in `ProcessFrame()` | Build passes; debug geo visible in app | Done | sonnet | Build clean; kDebug3DViewId=3 |
| 6 | Write `TestDebugPrimitive3D.cpp`; add to `GoogleTests.vcxproj` | `dia run googletest --filter="DebugPrimitive3D*:DebugFrameData3D*"` all pass | Done | sonnet | 12/12 pass |
