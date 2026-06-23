# Feature Spec: debug-3d-primitives

## Parent System
@docs/specs/applications/dia/systems/diavisualdebugger/diavisualdebugger.md

**Status:** `Approved`

---

## Summary

Add five 3D debug primitive types (`Line3D`, `Ray3D`, `Box3D`, `Sphere3D`, `Arrow3D`) to `DebugFrameData` in `DiaGraphics`, and add a `DebugGeometry3DRenderer` in `DiaBgfx3D` that consumes them each frame. This unblocks all 3D visual debugger systems (`DiaMesh3DVisualDebugger`, `DiaLighting3DVisualDebugger`) which require world-space 3D geometry drawing but currently have no rendering path.

`IVisualDebugger::Draw(Dia::Graphics::FrameData&)` is unchanged — draw classes submit 3D primitives through the same `FrameData` they already receive. The 2D `DiaBgfx::DebugRenderer` adds silent fallthrough cases for the new enum values; only `Canvas3D`/`DebugGeometry3DRenderer` in `DiaBgfx3D` renders them.

---

## Acceptance Criteria

1. Five new `DebugPrimitiveType` enum values: `Line3D`, `Ray3D3D`, `Box3D`, `Sphere3D`, `Arrow3D`
2. Five new POD structs in `DebugPrimitive.h` — all trivially copyable, fitting the existing union
3. `DebugFrameData` gains five `RequestDraw*3D` overloads; each increments `mDroppedCount` on overflow
4. `DiaBgfx::DebugRenderer::Draw()` has silent no-op fallthrough for all five new type values
5. `DebugGeometry3DRenderer` exists in `DiaBgfx3D`, consuming 3D primitives from `DebugFrameData`
6. `DebugGeometry3DRenderer` uses the perspective `view`/`projection` from `Mesh3DFrameData::GetCamera()`
7. `Canvas3D::ProcessFrame()` calls `DebugGeometry3DRenderer::Draw()` after the mesh pass and before the 2D `Canvas::ProcessFrame()` call
8. `vs_debug3d.sc` / `fs_debug3d.sc` shader sources and pre-compiled `.bin` files exist in `Dia/DiaBgfx3D/Shaders/3d/`
9. `DebugFrameData` has a separate `kDebug3DCapacity = 2048` for 3D primitives — separate array from the existing 2D budget
10. `Box3D` occupies **one** slot in the 3D budget (not 12 line slots) — wireframe expansion is done in the renderer
11. `Sphere3D` occupies **one** slot (renderer emits 3 great-circle polylines at draw time)
12. `Arrow3D` occupies **one** slot (renderer emits shaft + 4 cone lines at draw time)
13. GoogleTests: `TestDebugPrimitive3D.cpp` — struct layout, `RequestDraw*3D` slot counting, dropped-count behaviour
14. Build passes with no warnings in Debug|x64

---

## Primitive Types

### Data structures (add to `DebugPrimitive.h`)

```cpp
// DebugPrimitiveType enum additions
Line3D  = 7,
Ray3D   = 8,
Box3D   = 9,
Sphere3D = 10,
Arrow3D  = 11,

// New POD structs
struct DebugPrimitiveLine3D {
    Dia::Maths::Vector3D from;
    Dia::Maths::Vector3D to;
    RGBA                 colour;
};

struct DebugPrimitiveRay3D {
    Dia::Maths::Vector3D origin;
    Dia::Maths::Vector3D direction;  // caller must supply unit vector
    float                length;
    RGBA                 colour;
    // Renderer draws shaft (line) + small 2-line arrowhead (fixed angle, fixed fraction of length)
};

struct DebugPrimitiveBox3D {
    Dia::Maths::Vector3D min;
    Dia::Maths::Vector3D max;
    RGBA                 colour;
    // Renderer expands to 12 edges (24 vertices) at draw time
};

struct DebugPrimitiveSphere3D {
    Dia::Maths::Vector3D center;
    float                radius;
    RGBA                 colour;
    // Renderer emits 3 great circles (XY, XZ, YZ planes) × 24 segments each
};

struct DebugPrimitiveArrow3D {
    Dia::Maths::Vector3D origin;
    Dia::Maths::Vector3D direction;  // caller must supply unit vector
    float                length;
    float                headSize;   // cone radius as fraction of length (e.g. 0.15)
    RGBA                 colour;
    // Renderer draws shaft + 4 cone lines from tip back to cone base circle
};
```

All five fit in the existing `DebugPrimitive` union — each is smaller than the existing largest member.

### `DebugFrameData` additions

```cpp
// New separate 3D budget — independent of kGeometryCapacity
static constexpr uint32_t kDebug3DCapacity = 2048u;

void RequestDrawLine3D  (const Maths::Vector3D& from, const Maths::Vector3D& to, RGBA colour);
void RequestDrawRay3D   (const Maths::Vector3D& origin, const Maths::Vector3D& direction,
                         float length, RGBA colour);
void RequestDrawBox3D   (const Maths::Vector3D& min, const Maths::Vector3D& max, RGBA colour);
void RequestDrawSphere3D(const Maths::Vector3D& center, float radius, RGBA colour);
void RequestDrawArrow3D (const Maths::Vector3D& origin, const Maths::Vector3D& direction,
                         float length, float headSize, RGBA colour);

uint32_t GetDebug3DPrimitiveCount()          const;
const DebugPrimitive& GetDebug3DPrimitive(uint32_t index) const;
uint32_t DroppedDebug3DCount()               const;
```

3D primitives are stored in a **separate** `DynamicArrayC<DebugPrimitive, kDebug3DCapacity>` so overflow in one budget does not starve the other.

---

## Renderer: `DebugGeometry3DRenderer`

Lives in `Dia/DiaBgfx3D/Renderers/DebugGeometry3DRenderer.h/.cpp`.

```cpp
namespace Dia::Bgfx3D
{
    class DebugGeometry3DRenderer
    {
    public:
        DebugGeometry3DRenderer(unsigned short viewId, Dia::Bgfx::ShaderProgram* debugProgram);
        ~DebugGeometry3DRenderer();

        void Draw(const Dia::Graphics::DebugFrameData& debugData,
                  const Dia::Graphics3D::Camera3D& camera,
                  const Dia::Maths::Vector2D& canvasSize);
    private:
        unsigned short         mViewId;
        Dia::Bgfx::ShaderProgram* mDebugProgram; // not owned
    };
}
```

**Vertex layout:** `(x, y, z, abgr)` — same as `DebugRenderer` but with 3 floats for position.

**Per-primitive draw logic:**
- `Line3D` → 2 vertices
- `Ray3D` → shaft (2 verts) + arrowhead (4 verts = 2 lines ±30° back from tip, in the XZ plane relative to direction)
- `Box3D` → 24 vertices (all 12 edges inline-expanded per primitive)
- `Sphere3D` → 144 vertices (3 great circles × 24 segments × 2 verts each)
- `Arrow3D` → shaft (2 verts) + 4 cone lines (8 verts) from tip back to 4 base points

All vertices flushed as a single `TransientVertexBuffer` with `BGFX_STATE_PT_LINES`.

**View setup:**
```cpp
float viewMtx[16], projMtx[16];
camera.view.GetColumnMajor(viewMtx);
camera.projection.GetColumnMajor(projMtx);
bgfx::setViewTransform(mViewId, viewMtx, projMtx);
bgfx::setViewRect(mViewId, 0, 0, w, h);
// No clear — overlays on top of mesh pass
```

---

## Shaders

**`vs_debug3d.sc`** — trivial: transform position by `u_viewProj`, pass colour through.
```glsl
$input a_position, a_color0
$output v_color0

#include <bgfx_shader.sh>

void main()
{
    gl_Position = mul(u_viewProj, vec4(a_position, 1.0));
    v_color0    = a_color0;
}
```

**`fs_debug3d.sc`** — identical to `fs_debug.sc`: output `v_color0` as fragment colour.

Pre-compiled `.bin` files for `dx11`, `dx12`, `vulkan` live alongside the `.sc` sources at `Dia/DiaBgfx3D/Shaders/3d/`.

---

## Canvas3D integration

`Canvas3D` gains a `DebugGeometry3DRenderer* mDebugGeometry3DRenderer` member (owned).
`Init3DPrograms()` creates it with a new `debug3d` view ID (`kDebug3DViewId = 3`) and loads `vs_debug3d`/`fs_debug3d`.
`ProcessFrame()` calls `mDebugGeometry3DRenderer->Draw()` after step 2 (mesh pass), before step 4 (`Canvas::ProcessFrame`).

---

## Tasks

| # | Task | Test |
|---|------|------|
| 1 | Add 5 enum values + 5 POD structs to `DebugPrimitive.h`; add second `DynamicArrayC` + 5 `RequestDraw*3D` + accessors to `DebugFrameData.h/.cpp` | Build passes; `kDebug3DCapacity` slot counting correct |
| 2 | Add silent no-op fallthrough cases for Line3D/Ray3D/Box3D/Sphere3D/Arrow3D to `DiaBgfx::DebugRenderer::Draw()` | Build passes; existing tests unaffected |
| 3 | Write `vs_debug3d.sc` / `fs_debug3d.sc`; compile to `.bin` for dx11/dx12/vulkan | Shader files exist; Canvas3D loads them without warning |
| 4 | Implement `DebugGeometry3DRenderer.h/.cpp`; add to `DiaBgfx3D.vcxproj` | Build passes |
| 5 | Wire into `Canvas3D`: add member, init in `Init3DPrograms()`, call in `ProcessFrame()` | Visible debug geometry in running app |
| 6 | Write `Cluiche/Tests/GoogleTests/DiaVisualDebugger/TestDebugPrimitive3D.cpp`; add to `GoogleTests.vcxproj` | `dia run googletest --filter="DebugPrimitive3D*"` all pass |

---

## Binding Decisions

| ID | Decision | How this feature complies |
|----|----------|--------------------------|
| SD-DBG-002 | `#ifdef DIA_DEBUG` guards | All new `RequestDraw*3D` methods, `DebugGeometry3DRenderer`, and shader loading guarded by `DIA_DEBUG` |
| SD-DBG-004 | First-come-first-served budget; `DroppedCount()` logged on overflow | 3D budget is separate (`kDebug3DCapacity`); `DroppedDebug3DCount()` available |
| SD-DBG-009 | `DebugFrameData` must remain trivially copyable | All five new structs are POD; `DynamicArrayC` is trivially copyable |
| SD-DBG-010 | `DebugColourPalette` constants | All draw classes calling these methods pass palette constants |
| PD-004 | No STL in public API | All new `DebugFrameData` methods use engine types only (`Vector3D`, `RGBA`, `DynamicArrayC`) |
| PD-007 | C++20 required | No issue — no new language features required |

---

## Design Decisions

1. **View ID block documented in `Canvas3D.h`.** All `kXxxViewId` constants live in `Canvas3D.cpp` and are annotated in `Canvas3D.h` as a comment block (`// View IDs: 1=shadow 2=mesh 3=debug3d`) so they are visible at the header level.

2. **`DiaGraphics` gains `Vector3D` dependency.** Accepted — `Vector3D.h` is in the same `DiaMaths` module as `Vector2D.h`, no new library link required.

---

## Status

`Approved`
