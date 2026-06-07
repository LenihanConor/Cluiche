# Feature Spec: graphics-3d-types

## Parent System
@docs/specs/systems/dia/diagraphics3d.md

**Cross-cutting system:** @docs/specs/systems/dia/render-backend.md (Phase 2 — specced now, implemented later per RB-003)

**Hard dependencies (already specced, implemented separately):**
- @docs/specs/systems/dia/diamaths.md — provides `Vector3D::Cross`, `Matrix44`, `Matrix34`, `Quaternion`, `Transform3D` (all under `Dia::Maths::`)
- @docs/specs/systems/dia/diageometry3d.md — provides `Dia::Geometry3D::Frustum`, `AABB`, etc.

**Research:** @docs/research/render_backend_swap/summary.md

> **Amendment note (2026-05-18):** This feature was re-homed from `DiaGraphics` to `DiaGraphics3D` per decisions G3D-001/G3D-002/G3D-003. The types now live in `Dia/DiaGraphics3D/` under namespace `Dia::Graphics3D::`. `FrameData` (in DiaGraphics) does NOT gain a Mesh3D mix-in; instead `FrameData3D : FrameData + Mesh3DFrameData` is the 2D+3D frame packet and lives here. All ACs, maths ownership rules, and capacity constants are unchanged.

## Status
`Done`

## Summary

Create the new `Dia/DiaGraphics3D/` module and populate it with the rendering-side 3D primitive types that every Phase 2 module produces or consumes: `Camera3D`, `DirectionalLight`, `PointLight`, `Mesh3DDrawCommand`, `Mesh3DFrameData`, and `FrameData3D`. All types live under namespace `Dia::Graphics3D::`.

`FrameData3D : Dia::Graphics::FrameData, Mesh3DFrameData` is the full 2D+3D frame packet. `DiaGraphics::FrameData` is **not modified** — it remains 2D-only (decisions G3D-002/G3D-003). This keeps 2D-only games free of any Matrix44 transitive dependency.

This feature is **purely the rendering-side type seam**. It defines no maths primitives — it consumes `Dia::Maths::Matrix44`, `Dia::Maths::Quaternion`, `Dia::Maths::Vector3D`, `Dia::Maths::Transform3D` from DiaMaths and `Dia::Geometry3D::Frustum` / `AABB` from DiaGeometry3D. Those systems own the underlying types and have separate Approved specs already.

After this feature, the *contract* between simulation-side 3D code (DiaMesh3D, DiaScene3D, DiaSkinning3D) and renderer-side 3D code (`DiaBgfx3D::MeshRenderer`, `SkinnedMeshRenderer`) is fully defined. No 3D rendering happens yet — that's `diabgfx-3d-renderers`. No 3D mesh loading happens yet — that's `diamesh3d`.

## Problem

Phase 2 modules (`diamesh3d`, `diarig3d`, `diaanimation3d`, `diaskinning3d`, `diascene3d`, `diabgfx-3d-renderers`) all need to agree on:
- How a 3D draw is expressed at the renderer boundary (mesh+material id, transform, skinning index, layer)
- How a camera is expressed (view + projection — derived from `Dia::Maths::Matrix44`)
- How lights are expressed (direction or position, colour, intensity)
- How per-frame 3D data flows from sim to renderer (analogous to how `EntityFrameData` carries `SpriteDrawCommand` for 2D)

Without this feature, every Phase 2 module would either invent its own version or import from `DiaBgfx` (violating PD-004). Defining them in `DiaGraphics` keeps the asymmetry consistent with 2D: `Dia::Graphics::SpriteDrawCommand` lives in DiaGraphics today; `Dia::Graphics::Mesh3DDrawCommand` is its 3D peer.

The maths primitives this depends on (`Matrix44`, `Quaternion`, `Transform3D`, `Vector3D::Cross`, `Frustum`, `AABB`) are owned by DiaMaths and DiaGeometry3D (Approved system specs). This feature **does not redefine them** — it consumes them.

## Goals

- Create new module `Dia/DiaGraphics3D/` with `DiaGraphics3D.vcxproj` + `DiaGraphics3D.vcxproj.filters`
- Add `Dia::Graphics3D::Camera3D` — view + projection (`Dia::Maths::Matrix44`-typed) + viewport
- Add `Dia::Graphics3D::DirectionalLight` and `Dia::Graphics3D::PointLight`
- Add `Dia::Graphics3D::Mesh3DDrawCommand` — `meshId` + `materialId` + transform + skinning palette index + layer
- Add `Dia::Graphics3D::Mesh3DFrameData` — container for 3D draw commands + camera + lights, mirrors `EntityFrameData`'s shape
- Add `Dia::Graphics3D::FrameData3D : Dia::Graphics::FrameData, Mesh3DFrameData` — 2D+3D frame packet (does NOT modify DiaGraphics::FrameData per G3D-003)
- Provide `MockMesh3DFrameData` in `Dia/DiaGraphics3D/Testing/` for unit tests
- All new types are PD-004 compliant (no STL in public APIs, `DynamicArrayC` used internally with appropriate capacity constants)
- `dia.graphics3d.architecture.module.md` created; dependency edges to `dia.graphics`, `dia.maths.matrix`, `dia.maths.vector`, `dia.core`

## Non-Goals

- **Adding any maths primitives to DiaMaths** — `Matrix44`, `Quaternion`, `Transform3D`, `Vector3D::Cross`, `Matrix34` are owned by DiaMaths and are tracked under the `diamaths.md` system spec's six in-flight feature specs. This feature is a *consumer*, not a producer.
- **Adding `Frustum` or `AABB` to DiaGraphics** — those live in `Dia::Geometry3D::` per the DiaGeometry3D system spec.
- **Material types (`Material3D`, `MaterialDescriptor`)** — Phase 2 punts on a typed material system; meshes carry a `materialId` (StringCRC) that the renderer resolves. Material authoring is post-Phase-2 work
- **Mesh data structures** — `DiaMesh3D` owns vertex/index buffer types; `Mesh3DDrawCommand` only carries a `meshId` (StringCRC asset id)
- **Skeleton / animation runtime** — `DiaRig3D` and `DiaAnimation3D` own those; the only thing this feature carries is the `skinningPaletteIndex` field on `Mesh3DDrawCommand`
- **PBR-specific types** — no `RoughnessMetallic`, `IBL`, etc.; brief is "no photorealism" (RB-001)
- **Particle / spline / line-strip 3D primitives** — sprite/mesh is the only 3D draw type covered; 3D debug primitives extend `DebugPrimitive` separately if/when needed
- **3D culling** — `Mesh3DFrameData` accumulates draw commands; frustum culling happens at consumption time in `DiaScene3D` or the renderer, not here. The feature *enables* culling by carrying the `Camera3D` (and its derivable `Frustum` via DiaGeometry3D), but performs none

## Public Interfaces

### `Dia::Graphics::Camera3D`

```cpp
// Dia/DiaGraphics/Mesh3D/Camera3D.h
#pragma once

#include <DiaMaths/Matrix/Matrix44.h>
#include <DiaMaths/Vector/Vector3D.h>
#include <DiaMaths/Core/Angle.h>

namespace Dia { namespace Graphics {

struct Camera3D
{
    Camera3D();

    Dia::Maths::Matrix44 view;        // world → view (row-major per DiaMaths SD-005)
    Dia::Maths::Matrix44 projection;  // view → clip

    // Helpers built from DiaMaths factories.
    void SetPerspective(const Dia::Maths::Angle& fovY, float aspect, float nearZ, float farZ);
    void SetOrthographic(float left, float right, float bottom, float top, float nearZ, float farZ);
    void SetView(const Dia::Maths::Vector3D& eye,
                 const Dia::Maths::Vector3D& target,
                 const Dia::Maths::Vector3D& up);
};

} }
```

`Camera3D::SetPerspective` calls `Dia::Maths::Matrix44::Perspective` (defined in DiaMaths). `SetView` calls `Dia::Maths::Matrix44::LookAt`. The struct is a thin convenience wrapper; the maths is owned by DiaMaths.

A consumer that needs a `Frustum` for culling derives it from `view * projection` via `Dia::Geometry3D::Frustum::FromViewProjection(...)` (which DiaGeometry3D's `shape-primitives.md` feature is expected to provide; if not yet, this feature requests its addition there).

### `Dia::Graphics::DirectionalLight` / `PointLight`

```cpp
// Dia/DiaGraphics/Mesh3D/Light.h
#pragma once

#include <DiaMaths/Vector/Vector3D.h>
#include <DiaGraphics/Misc/RGBA.h>

namespace Dia { namespace Graphics {

struct DirectionalLight
{
    Dia::Maths::Vector3D direction;     // unit vector, points away from surface
    RGBA                 colour;
    float                intensity;     // multiplied by colour at shading time
};

struct PointLight
{
    Dia::Maths::Vector3D position;
    RGBA                 colour;
    float                intensity;
    float                range;         // beyond which contribution is zero
};

} }
```

### `Dia::Graphics::Mesh3DDrawCommand`

```cpp
// Dia/DiaGraphics/Mesh3D/Mesh3DDrawCommand.h
#pragma once

#include <DiaCore/CRC/StringCRC.h>
#include <DiaMaths/Matrix/Matrix44.h>

namespace Dia { namespace Graphics {

struct Mesh3DDrawCommand
{
    Mesh3DDrawCommand();

    Dia::Core::StringCRC meshId;        // resolved by renderer to vertex/index buffers
    Dia::Core::StringCRC materialId;    // resolved by renderer to shader + material params
    Dia::Maths::Matrix44 transform;     // model → world (row-major)

    // 0 = static mesh (no skinning).
    // Non-zero = index into the per-frame skinning palette buffer (DiaSkinning3D).
    // The palette itself is uploaded out-of-band by DiaSkinning3D before rendering.
    uint32_t             skinningPaletteIndex;

    // Render layer / sort key — small integer (analogous to SpriteDrawCommand::layer).
    int16_t              layer;
};

} }
```

The `transform` field is a row-major `Matrix44` (per DiaMaths SD-005). Renderer-side code uploads it to bgfx via `Dia::Maths::Matrix44::GetColumnMajor(out16)` followed by `bgfx::setTransform(out16)` — one transpose at upload, negligible cost.

### `Dia::Graphics::Mesh3DFrameData`

```cpp
// Dia/DiaGraphics/Frame/Mesh3DFrameData.h
#pragma once

#include "DiaGraphics/Mesh3D/Camera3D.h"
#include "DiaGraphics/Mesh3D/Light.h"
#include "DiaGraphics/Mesh3D/Mesh3DDrawCommand.h"
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

namespace Dia { namespace Graphics {

class Mesh3DFrameData
{
public:
    static constexpr uint32_t kMaxMeshDraws = 4096;
    static constexpr uint32_t kMaxLights    = 32;

    Mesh3DFrameData();

    void RequestDrawMesh(const Mesh3DDrawCommand& cmd);
    void SetCamera(const Camera3D& camera);
    void AddDirectionalLight(const DirectionalLight& light);
    void AddPointLight(const PointLight& light);

    void Clear();
    void Copy(const Mesh3DFrameData& rhs);

    const Camera3D&                                        GetCamera()        const;
    const Dia::Core::Containers::DynamicArrayC<Mesh3DDrawCommand,  kMaxMeshDraws>& GetMeshDraws()         const;
    const Dia::Core::Containers::DynamicArrayC<DirectionalLight,   kMaxLights>&    GetDirectionalLights() const;
    const Dia::Core::Containers::DynamicArrayC<PointLight,         kMaxLights>&    GetPointLights()       const;

    uint32_t DroppedMeshCount()  const { return mDroppedMeshes; }
    uint32_t DroppedLightCount() const { return mDroppedLights; }

private:
    Camera3D                                                                        mCamera;
    Dia::Core::Containers::DynamicArrayC<Mesh3DDrawCommand, kMaxMeshDraws>          mMeshDraws;
    Dia::Core::Containers::DynamicArrayC<DirectionalLight,  kMaxLights>             mDirectionalLights;
    Dia::Core::Containers::DynamicArrayC<PointLight,        kMaxLights>             mPointLights;
    uint32_t                                                                        mDroppedMeshes;
    uint32_t                                                                        mDroppedLights;
};

} }
```

Capacity-overflow behaviour mirrors `DebugFrameData`: drop-and-count rather than allocate. `DroppedMeshCount()` / `DroppedLightCount()` surface the count for diagnostic use.

### `Dia::Graphics::FrameData` (extended)

```cpp
// Dia/DiaGraphics/Frame/FrameData.h
class FrameData
    : public DebugFrameData
    , public UIFrameData
    , public EntityFrameData
    , public Mesh3DFrameData     // NEW
{
    // ... Clear() / Copy() updated to include mesh3D mix-in
};
```

## Implementation

### Files introduced

```
Dia/DiaGraphics3D/Camera3D.h             NEW
Dia/DiaGraphics3D/Camera3D.cpp           NEW
Dia/DiaGraphics3D/Light.h                NEW
Dia/DiaGraphics3D/Light.cpp              NEW
Dia/DiaGraphics3D/Mesh3DDrawCommand.h    NEW
Dia/DiaGraphics3D/Mesh3DDrawCommand.cpp  NEW
Dia/DiaGraphics3D/Mesh3DFrameData.h      NEW
Dia/DiaGraphics3D/Mesh3DFrameData.cpp    NEW
Dia/DiaGraphics3D/FrameData3D.h          NEW
Dia/DiaGraphics3D/FrameData3D.cpp        NEW
Dia/DiaGraphics3D/Testing/MockMesh3DFrameData.h  NEW
Dia/DiaGraphics3D/DiaGraphics3D.vcxproj          NEW
Dia/DiaGraphics3D/DiaGraphics3D.vcxproj.filters  NEW
Dia/DiaGraphics3D/dia.graphics3d.architecture.module.md  NEW
```

### Files modified

```
Dia/DiaGraphics/Frame/FrameData.h / .cpp
   - NO CHANGE — FrameData stays 2D-only per G3D-003

Cluiche/Cluiche.sln
   - Register DiaGraphics3D.vcxproj

Cluiche/Tests/GoogleTests/Graphics3D/TestMesh3DFrameData.cpp  NEW
   — unit tests for Mesh3DFrameData + capacity overflow + Camera3D helpers + FrameData3D copy
```

### Capacity constants justification

- `kMaxMeshDraws = 4096` — supports a "small modern 3D scene" without forcing eviction. Memory cost: 4096 × ~96 bytes/cmd ≈ 384 KB per FrameData (Matrix44 is 64 bytes; meshId+materialId+skinningIdx+layer ≈ 16 bytes; padding). FrameData is copied across the frame stream once per frame; ~400 KB copy is acceptable. Tunable later.
- `kMaxLights = 32` — light count is forward-shading-bound; 32 is generous for "light 3D" scope.

### Coexistence with DiaMaths / DiaGeometry3D in-flight work

DiaMaths system spec is `Approved` (2026-05-17) but its child features (`vector3d-cross`, `matrix44`, `matrix34`, `quaternion`, `transform3d`, `shape-cleanup`) are still `Draft`. DiaGeometry3D system spec is `Approved` and its three child features are `Draft`.

This feature has a hard prerequisite on the DiaMaths Matrix44 feature being implemented (because `Camera3D` and `Mesh3DDrawCommand` carry `Matrix44` by value). It does **not** depend on `Quaternion`, `Transform3D`, or `Matrix34` — those are out-of-scope here (Mesh3DDrawCommand carries a Matrix44 transform, not a Transform3D, because the renderer needs the final composed matrix; Transform3D's `GetWorldMatrix()` is called by DiaScene3D before submission).

Implementation order for Phase 2:
1. DiaMaths features: `vector3d-cross` + `matrix44` minimum (blocking)
2. This feature (`graphics-3d-types`) becomes implementable
3. DiaMaths `quaternion` + `transform3d` + `matrix34` follow (used by DiaScene3D / DiaRig3D, not by this feature directly)
4. DiaGeometry3D features become implementable (after `vector3d-cross` + `matrix44`)
5. The remaining Phase 2 modules (DiaMesh3D, DiaRig3D, DiaAnimation3D, DiaSkinning3D, DiaScene3D, DiaBgfx 3D renderers) light up

## Files Introduced / Modified

| File | Change |
|------|--------|
| `Dia/DiaGraphics3D/Camera3D.{h,cpp}` | NEW |
| `Dia/DiaGraphics3D/Light.{h,cpp}` | NEW |
| `Dia/DiaGraphics3D/Mesh3DDrawCommand.{h,cpp}` | NEW |
| `Dia/DiaGraphics3D/Mesh3DFrameData.{h,cpp}` | NEW |
| `Dia/DiaGraphics3D/FrameData3D.{h,cpp}` | NEW |
| `Dia/DiaGraphics3D/Testing/MockMesh3DFrameData.h` | NEW |
| `Dia/DiaGraphics3D/DiaGraphics3D.vcxproj{,.filters}` | NEW |
| `Dia/DiaGraphics3D/dia.graphics3d.architecture.module.md` | NEW |
| `Dia/DiaGraphics/Frame/FrameData.{h,cpp}` | **No change** — 2D only (G3D-003) |
| `Cluiche/Cluiche.sln` | Register DiaGraphics3D.vcxproj |
| `Cluiche/Tests/GoogleTests/Graphics3D/TestMesh3DFrameData.cpp` | NEW |

No `Matrix44` source files — that's a DiaMaths concern.

## Dependencies

| Dependency | Type | Notes |
|------------|------|-------|
| **DiaMaths `matrix44`** feature | Hard | `Camera3D` and `Mesh3DDrawCommand` carry `Dia::Maths::Matrix44` by value |
| **DiaMaths `vector3d-cross`** feature | Soft | Not used directly by this feature, but a Phase 2 prerequisite for DiaScene3D / DiaGeometry3D |
| `texture-handle-stringcrc` (Approved) | Soft | Same StringCRC pattern reused for meshId/materialId |
| Phase 1 features | Soft | Phase 1 must ship first per RB-003; this feature is implemented after Phase 1 lands |
| `diamesh3d` | Reverse | Consumes Mesh3DDrawCommand |
| `diaskinning3d` | Reverse | Writes the palette index |
| `diascene3d` | Reverse | Produces FrameData with Mesh3DFrameData populated |
| `diabgfx-3d-renderers` | Reverse | Consumes Mesh3DFrameData |

## Acceptance Criteria

1. `Dia::Graphics::Camera3D` exists with `view` (Matrix44) + `projection` (Matrix44) + helper setters (`SetPerspective`, `SetOrthographic`, `SetView`)
2. `Dia::Graphics::DirectionalLight` and `PointLight` structs exist with documented fields
3. `Dia::Graphics::Mesh3DDrawCommand` exists with `meshId`, `materialId`, `transform`, `skinningPaletteIndex`, `layer`
4. `Dia::Graphics::Mesh3DFrameData` exists; supports `RequestDrawMesh`, `SetCamera`, `AddDirectionalLight`, `AddPointLight`, `Clear`, `Copy`, accessors, and drop-counters
5. `Dia::Graphics3D::FrameData3D` inherits from both `Dia::Graphics::FrameData` and `Mesh3DFrameData` (G3D-003 — `FrameData` itself is NOT modified); `Clear` and `Copy` delegate to both bases
6. `Mesh3DFrameData::kMaxMeshDraws = 4096`, `kMaxLights = 32` constants honoured
7. All new public APIs PD-004 compliant — no STL types in any header (verified by inspection)
8. `Mesh3DFrameData` unit tests cover request/clear, capacity overflow drops, camera storage, light storage
9. `Camera3D` unit tests verify the helper setters produce matrices that round-trip with `Matrix44::Inverse` for `SetView` (camera origin recovers from inverse view matrix)
10. `dia.graphics.architecture.module.md` lists all five new types in `public_api.entry_points`; `dependencies.required` includes `dia.maths.matrix`
11. `python Tools/dia_modules.py --validate` passes; the new DiaGraphics → DiaMaths::Matrix dep is acyclic
12. Build clean under `/std:c++20` with zero new warnings
13. `MockMesh3DFrameData` test stub usable from any DiaGraphics test
14. FrameData per-frame copy size measured; ≤400 KB grew (acceptable per Goals)
15. **No Matrix44 source files added by this feature** (verified — matrix is owned by DiaMaths)
16. `transform` field upload path documented: consumers call `transform.GetColumnMajor(out16)` before `bgfx::setTransform(out16)`. This is captured in `diabgfx-3d-renderers`, but referenced here so it's not lost

## Traceability

| Level | Spec | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System (primary) | DiaGraphics3D | @docs/specs/systems/dia/diagraphics3d.md |
| System (cross-cutting) | RenderBackend | @docs/specs/systems/dia/render-backend.md |
| System (consumed) | DiaMaths | @docs/specs/systems/dia/diamaths.md |
| System (consumed, transitively) | DiaGeometry3D | @docs/specs/systems/dia/diageometry3d.md |

## Binding Decisions Compliance

| ID | Source | Decision | Compliance |
|----|--------|----------|------------|
| PD-001 | Platform | StringCRC for IDs | Compliant — `meshId`, `materialId` are `StringCRC` |
| PD-002 | Platform | ProcessingUnit/Phase/Module | N/A — pure type definitions |
| PD-003 | Platform | Component-based entities | N/A |
| PD-004 | Platform | No STL in public APIs | Compliant — uses `DynamicArrayC`, `Matrix44`, `Vector3D`, `RGBA`, `StringCRC` |
| PD-005 | Platform | x64 only | Compliant |
| PD-006 | Platform | Visual Studio project files | Compliant — `.vcxproj{,.filters}` updated |
| PD-007 | Platform | C++20 | Compliant |
| PD-008 | Platform | Directory.Build.props ownership | Compliant |
| PD-009 | Platform | Generated output under Cluiche/out | N/A |
| AD-001 | Dia App | Module YAML frontmatter | Compliant |
| AD-002 | Dia App | No STL in public APIs | Reinforces PD-004 |
| AD-003 | Dia App | Namespace Dia::<Module>:: | Compliant — `Dia::Graphics::` |
| GD-002 | DiaGraphics | FrameData copy must be trivially correct (no pointers in debug buffers) | Compliant — Mesh3DFrameData has no pointer members; trivially copyable preserved (Matrix44 is float[16], StringCRC is uint32, Vector3D is 3 floats) |
| GD-003 | DiaGraphics | Debug renderer separate concern | N/A |
| GD-004 | DiaGraphics | Debug primitives in insertion order | N/A |
| **DiaMaths SD-005** | DiaMaths | All DiaMaths matrices share row-major `float m[N][N]` layout | **Compliant — `Camera3D`, `Mesh3DDrawCommand` carry `Matrix44` by value; layout is owned by DiaMaths and consumed unchanged. Renderer transposes once at upload via `Matrix44::GetColumnMajor`** |
| **DiaMaths SD-007** | DiaMaths | Y-up right-handed convention | Compliant — `Camera3D::SetView` documents the `up` parameter as Y-up convention; `Matrix44::Perspective` is Y-up RH per DiaMaths SD-007 |
| RB-002 | RenderBackend | Two-phase delivery | Compliant — Phase 2 |
| RB-003 | RenderBackend | Phase 2 specs Approved, not in progress | Compliant — this feature stays Approved until Phase 1 ships |
| RB-013 | RenderBackend | Phase 2 module decomposition mirrors 2D family | Compliant — Mesh3D types parallel SpriteDrawCommand etc. |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Maths ownership | Why does this feature consume `Matrix44` instead of defining its own? | DiaMaths owns all linear-algebra primitives per DiaMaths SD-001. Defining `Matrix44` here would duplicate it, fork the storage layout, and break the cross-dimension consistency rule (DiaMaths SD-005). DiaMaths' `matrix44.md` feature spec (Draft) is the authoritative definition; this feature is a *consumer*. |
| 2 | Storage layout | DiaMaths binds row-major. Does the renderer-side use cause any awkwardness? | One transpose per matrix at upload time via `Matrix44::GetColumnMajor(float[16])`. Negligible vs frame work. bgfx accepts column-major float[16] via `bgfx::setTransform`. Captured in `diabgfx-3d-renderers` implementation. |
| 3 | Transform vs Matrix | Why does `Mesh3DDrawCommand` carry a `Matrix44` rather than a `Transform3D`? | `Transform3D` is the *authoring/hierarchy* type (parent pointer, local/world separation). The render-side draw command is a flat, frame-final value: parent chain already resolved, world matrix already composed. `Mesh3DDrawCommand` is what gets memcpy'd across the frame stream — a value, not a hierarchy node. DiaScene3D calls `Transform3D::GetWorldMatrix()` and stores the result in the draw command. |
| 4 | Frustum derivation | Where does the renderer get a `Frustum` from `Camera3D`? | Derive from `view * projection` matrix via `Dia::Geometry3D::Frustum::FromViewProjection(...)`. If that constructor doesn't yet exist in DiaGeometry3D's `shape-primitives` feature, this feature requests its addition (it's a one-line algorithmic addition; standard frustum extraction). DiaGeometry3D's spec already lists `Frustum` as in scope; this is just a constructor preference. |
| 5 | Quaternion absence | Why no Quaternion field on Mesh3DDrawCommand? | DiaScene3D / DiaRig3D / DiaAnimation3D author rotations as quaternions and compose into Matrix44 before submitting to the renderer. The render-side type carries the final composed transform. If a future feature needs per-instance dynamic rotation editing in FrameData, it can add a Quaternion field; for now, Matrix44 is sufficient and matches bgfx's expectation. |
| 6 | Capacity headroom | 4096 mesh draws and 32 lights are caps. What happens on overflow? | `RequestDrawMesh` and `AddDirectionalLight`/`AddPointLight` increment `mDroppedMeshes`/`mDroppedLights`. No allocation; matches `DebugFrameData` pattern. Diagnostic counters surfaced via `DroppedMeshCount()` / `DroppedLightCount()`. Logged once per frame on first overflow (see Implementation). |
| 7 | FrameData copy size | DiaGraphics SD-002 forbids pointer members. Adding ~400 KB to the FrameData copy: still fits the trivially-copyable invariant? | Yes. `Mesh3DFrameData` has no pointer members; `Matrix44` is `float[16]`, `StringCRC` is `uint32_t`, `Vector3D` is 3 floats, `DynamicArrayC` is fixed-capacity inline storage with no heap allocations. `memcpy(FrameData, FrameData)` works correctly. Frame stream cost grows ~400 KB but stays trivially correct. |
| 8 | DiaGraphics dep edge | Adding DiaGraphics → DiaMaths::Matrix is a new dep edge. Confirm acyclic. | DiaMaths is at the bottom of the engine stack (per `diamaths.md`'s "Dependency chain" line). DiaGraphics already transitively depends on DiaMaths via Vector2D. Adding the explicit `dia.maths.matrix` edge is a no-op for the cycle check; `python Tools/dia_modules.py --validate` confirms. |
| 9 | Light intensity format | Why `float intensity` instead of pre-multiplied `RGBA` colour? | Two reasons: HDR-style lighting often expresses intensity > 1.0 (a "bright sunlight" might be intensity 5.0 with white tint); RGBA's 8-bit channels can't represent intensities > 1.0. Keeping intensity separate also lets shaders modulate it independently of colour. If we ever go full HDR (post-Phase-2), `colour` may become `float3` — defer that change. |
| 10 | Layer field | `int16_t layer` — 65,535 layer levels enough? | Generous. Most engines use ~16–256 distinct layers. int16 saves 2 bytes per command vs int32 — at 4096 commands, 8 KB saved per FrameData. |
| 11 | MockMesh3DFrameData | What does the test stub provide? | Empty inherit from `Mesh3DFrameData` with public access to internals for tests that need to inspect state. Mirrors `MockVisitors.h` pattern in `Dia/DiaGraphics/Testing/`. |
| 12 | DiaMaths feature dependency | This feature is Approved but blocked on DiaMaths' `matrix44` feature being Approved (Draft today). Acceptable? | Yes — it's a Phase 2 feature staying Approved until both Phase 1 ships *and* DiaMaths matrix44 ships. Spec authoring is independent of implementation gating. The plan tracks the prerequisite. |
| 13 | Architecture module deps | What DiaMaths submodule edges does DiaGraphics gain? | At minimum `dia.maths.matrix` (Matrix44). Vector3D is already transitively in via Vector2D's submodule path; `dia.maths.vector` is already an existing edge. No need to add `dia.maths.quaternion` or `dia.maths.transform` because this feature does not consume them directly (Phase 2 modules that *do* consume them will add their own edges). |
| 14 | Consistency with 2D | `EntityFrameData`'s `SpriteDrawCommand` does not carry a Camera. Why does `Mesh3DFrameData` carry one? | 2D's projection is implicit (orthographic, screen-aligned, defaults handled by SFML/bgfx). 3D's view+projection is explicit and varies per frame. The asymmetry follows the asymmetry between 2D and 3D rendering — captured in RB-014 (DiaScene3D scene-graph) and reinforced here at the frame-data level. |
| 15 | DiaScene3D vs Mesh3DFrameData | Phase 2 also has `diascene3d`. What's the boundary? | `Mesh3DFrameData` is the *flat, frame-final* state: every draw is independent, every light is global, the camera is set. `DiaScene3D` is the *authoring/composition* layer: scene graph nodes, parent-child transforms, dynamic visibility, light-mesh associations. DiaScene3D *produces* `Mesh3DFrameData` at frame-end by walking its graph. This feature owns the consumer-side type; DiaScene3D's spec owns the producer. |

---
