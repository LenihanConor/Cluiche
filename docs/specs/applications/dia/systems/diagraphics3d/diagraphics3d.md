# System Spec: DiaGraphics3D

## Parent Application
@docs/specs/applications/dia/dia.md

**Status:** `Approved`

---

## Purpose

DiaGraphics3D is the 3D rendering type layer for the Dia engine. It extends the 2D abstraction defined by `DiaGraphics` with the per-frame types that Phase 2 modules produce and consume: `Camera3D`, `DirectionalLight`, `PointLight`, `Mesh3DDrawCommand`, `Mesh3DFrameData`, and `FrameData3D`.

This system is a **type-seam library only** — it defines no renderer, no platform code, and no maths primitives. It sits between the 3D simulation modules (`DiaMesh3D`, `DiaScene3D`, etc.) and the 3D renderer (`DiaBgfx3D`), providing the shared vocabulary both sides agree on.

Keeping 3D types in a separate module from `DiaGraphics` ensures that 2D-only games never transitively pull in `DiaMaths::Matrix44` or the 3D frame-data types.

```
DiaScene3D / DiaSkinning3D / simulation
    ↓  fills FrameData3D
DiaGraphics3D (FrameData3D, Mesh3DFrameData, Camera3D, lights, draw commands)
    ↓  passed to ProcessFrame3D
DiaBgfx3D::Canvas3D (MeshRenderer, SkinnedMeshRenderer, ShadowRenderer)
    ↓  bgfx::frame()
GPU
```

**Dependency chain:**
`DiaGraphics3D → DiaGraphics → DiaMaths → DiaCore`

---

## Responsibilities

- Define `Mesh3DFrameData` — container for 3D draw commands + camera + lights; mirrors `EntityFrameData`'s shape
- Define `Camera3D` — view + projection (`Matrix44`-typed) + helper setters; this is a **render snapshot** distinct from `DiaCamera3D::Camera3D` (the live sim type)
- Define `DirectionalLight` and `PointLight`
- Define `Mesh3DDrawCommand` — per-draw command: `meshId`, `materialId`, `transform` (Matrix44), `skinningPaletteIndex`, `layer`
- Define `FrameData3D : FrameData, Mesh3DFrameData` — the full 2D+3D frame packet used by the 3D renderer; lives here so DiaGraphics (2D) has no 3D dep
- Provide `MockMesh3DFrameData` and test helpers in `Dia/DiaGraphics3D/Testing/` for unit tests
- Provide `DiaGraphics3D.vcxproj` static library project registered in `Cluiche.sln`
- Provide `dia.graphics3d.architecture.module.md` YAML module documentation

## Non-Responsibilities

- Any maths primitives (`Matrix44`, `Quaternion`, `Transform3D`) — owned by DiaMaths
- Live camera simulation (position, orientation, behaviours) — owned by DiaCamera3D; frame-building code converts via `ViewportTransform3D` before calling `SetCamera()`
- Any geometry primitives (`Frustum`, `AABB`) — owned by DiaGeometry3D
- Material types or shader descriptors — owned by DiaBgfx3D
- Mesh or skeleton data structures — owned by DiaMesh3D and DiaRig3D respectively
- Platform-specific rendering — DiaBgfx3D
- 3D debug draw primitives (future) — will be a separate feature when needed

---

## Public Interfaces

### `Dia::Graphics3D::Camera3D`

```cpp
// Dia/DiaGraphics3D/Camera3D.h
namespace Dia { namespace Graphics3D {

struct Camera3D
{
    Camera3D();

    Dia::Maths::Matrix44 view;        // world → view (row-major per DiaMaths SD-005)
    Dia::Maths::Matrix44 projection;  // view → clip

    void SetPerspective(const Dia::Maths::Angle& fovY, float aspect, float nearZ, float farZ);
    void SetOrthographic(float left, float right, float bottom, float top, float nearZ, float farZ);
    void SetView(const Dia::Maths::Vector3D& eye,
                 const Dia::Maths::Vector3D& target,
                 const Dia::Maths::Vector3D& up);
};

} }
```

### `Dia::Graphics3D::DirectionalLight` / `PointLight`

```cpp
// Dia/DiaGraphics3D/Light.h
namespace Dia { namespace Graphics3D {

struct DirectionalLight
{
    Dia::Maths::Vector3D direction;  // unit vector, points away from surface
    Dia::Graphics::RGBA  colour;
    float                intensity;  // HDR-range multiplier
};

struct PointLight
{
    Dia::Maths::Vector3D position;
    Dia::Graphics::RGBA  colour;
    float                intensity;
    float                range;      // contribution zero beyond this
};

} }
```

### `Dia::Graphics3D::Mesh3DDrawCommand`

```cpp
// Dia/DiaGraphics3D/Mesh3DDrawCommand.h
namespace Dia { namespace Graphics3D {

struct Mesh3DDrawCommand
{
    Mesh3DDrawCommand();

    Dia::Core::StringCRC meshId;               // asset id → vertex/index buffers
    Dia::Core::StringCRC materialId;           // resolved by renderer to shader + params
    Dia::Maths::Matrix44 transform;            // model → world (row-major)
    uint32_t             skinningPaletteIndex; // 0 = static mesh
    int16_t              layer;
};

} }
```

### `Dia::Graphics3D::Mesh3DFrameData`

```cpp
// Dia/DiaGraphics3D/Mesh3DFrameData.h
namespace Dia { namespace Graphics3D {

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

    const Camera3D&                                                                     GetCamera()            const;
    const Dia::Core::Containers::DynamicArrayC<Mesh3DDrawCommand, kMaxMeshDraws>&       GetMeshDraws()         const;
    const Dia::Core::Containers::DynamicArrayC<DirectionalLight,  kMaxLights>&          GetDirectionalLights() const;
    const Dia::Core::Containers::DynamicArrayC<PointLight,        kMaxLights>&          GetPointLights()       const;

    uint32_t DroppedMeshCount()  const;
    uint32_t DroppedLightCount() const;

private:
    Camera3D                                                                             mCamera;
    Dia::Core::Containers::DynamicArrayC<Mesh3DDrawCommand, kMaxMeshDraws>               mMeshDraws;
    Dia::Core::Containers::DynamicArrayC<DirectionalLight,  kMaxLights>                  mDirectionalLights;
    Dia::Core::Containers::DynamicArrayC<PointLight,        kMaxLights>                  mPointLights;
    uint32_t                                                                             mDroppedMeshes;
    uint32_t                                                                             mDroppedLights;
};

} }
```

### `Dia::Graphics3D::FrameData3D`

```cpp
// Dia/DiaGraphics3D/FrameData3D.h
namespace Dia { namespace Graphics3D {

// Full 2D+3D frame packet. 3D canvases (DiaBgfx3D::Canvas3D) accept this type.
// DiaGraphics::FrameData (2D-only) is the base — unmodified.
class FrameData3D
    : public Dia::Graphics::FrameData
    , public Mesh3DFrameData
{
public:
    FrameData3D();
    FrameData3D& operator=(const FrameData3D& rhs);
    void Clear();
    void Copy(const FrameData3D& rhs);
};

} }
```

---

## Features

| Feature | Description | Spec | Status |
|---------|-------------|------|--------|
| graphics-3d-types | `Camera3D`, lights, `Mesh3DDrawCommand`, `Mesh3DFrameData`, `FrameData3D`; new `Dia/DiaGraphics3D/` module; no maths primitives defined here | [graphics-3d-types.md](graphics-3d-types.md) | Approved |

*Additional features (3D debug primitive types, particle draw command types, material descriptor types) will be added as separate feature specs when needed.*

---

## Dependencies on Other Systems

| System | Role |
|--------|------|
| **DiaGraphics** | `FrameData`, `RGBA` — `FrameData3D` inherits `FrameData` |
| **DiaMaths** | `Matrix44`, `Vector3D`, `Angle` — consumed by `Camera3D` and `Mesh3DDrawCommand` |
| **DiaCore** | `StringCRC`, `DynamicArrayC` |
| **DiaGeometry3D** | *Not a direct dep* — consumers derive `Frustum` from `Camera3D` externally |

**Dependents:**
- **DiaBgfx3D** — `Canvas3D` consumes `FrameData3D`
- **DiaScene3D** — fills `FrameData3D` at frame end
- **DiaSkinning3D** — writes `skinningPaletteIndex` on `Mesh3DDrawCommand`

---

## Decisions

| ID | Decision | Rationale | Scope | Status | Binding |
|----|----------|-----------|-------|--------|---------|
| G3D-001 | DiaGraphics3D is a separate module (`Dia/DiaGraphics3D/`, own `.vcxproj`) — not a subdirectory of DiaGraphics | 2D-only games must not transitively link DiaMaths::Matrix44 or any 3D type. A separate module gives a clean dependency opt-in. | Module structure | Accepted | Yes |
| G3D-002 | `FrameData3D` lives in DiaGraphics3D, not in DiaGraphics | DiaGraphics must remain free of any DiaMaths::Matrix44 dependency so that 2D-only consumers stay lean. `FrameData3D : FrameData + Mesh3DFrameData` is the junction point and belongs with the 3D types. | FrameData split | Accepted | Yes |
| G3D-003 | DiaGraphics::FrameData is unchanged — it does NOT inherit Mesh3DFrameData | Reverses the original `graphics-3d-types` spec's plan. Amending that feature spec is required as part of `graphics-3d-types` implementation. | DiaGraphics, DiaGraphics3D | Accepted | Yes |
| G3D-004 | `Mesh3DFrameData` drop-on-overflow semantics match `DebugFrameData` — increment counter, no allocation | Frame data is copied across the frame stream; heap allocation in the hot path is excluded (GD-002). `kMaxMeshDraws = 4096`, `kMaxLights = 32`. | Mesh3DFrameData | Accepted | Yes |
| G3D-005 | `Mesh3DDrawCommand::transform` carries a pre-composed `Matrix44` (not `Transform3D`) | Draw commands are flat, frame-final values. DiaScene3D resolves the parent chain before submission. The renderer memcpy's the command — no hierarchy traversal. | Mesh3DDrawCommand | Accepted | Yes |
| G3D-006 | `Camera3D::SetView/SetPerspective/SetOrthographic` are thin convenience wrappers — all maths delegated to DiaMaths `Matrix44` factories | DiaGraphics3D does not own maths; it owns the rendering-side contract. | Camera3D | Accepted | Yes |
| G3D-007 | Light intensity is a separate `float` (not pre-multiplied into RGBA) | Supports HDR intensity > 1.0; keeps colour and brightness independently adjustable. | Light types | Accepted | Yes |
| G3D-008 | Namespace is `Dia::Graphics3D::` | Consistent with AD-003 (`Dia::<Module>::`) and clearly distinct from `Dia::Graphics::` | All DiaGraphics3D types | Accepted | Yes |

---

## Inherited Binding Decisions

| ID | Source | Decision | Implication for this system |
|----|--------|----------|-----------------------------|
| PD-001 | Platform | StringCRC for all entity/component IDs | `meshId`, `materialId` on `Mesh3DDrawCommand` are `StringCRC` |
| PD-004 | Platform | No STL containers in public APIs | All public types use `DynamicArrayC`; no `std::vector`, `std::string`, or `std::variant` |
| PD-005 | Platform | x64 only | `.vcxproj` targets x64 only |
| PD-006 | Platform | Visual Studio project files are source of truth | `DiaGraphics3D.vcxproj` + `.vcxproj.filters` maintained manually; registered in `Cluiche.sln` |
| PD-007 | Platform | C++20 | All sources compiled under `/std:c++20` |
| PD-008 | Platform | `Directory.Build.props` owns OutDir/IntDir | `DiaGraphics3D.vcxproj` inherits; no per-project overrides |
| AD-001 | Dia App | Module YAML frontmatter documentation | `dia.graphics3d.architecture.module.md` required |
| AD-002 | Dia App | No STL containers in public APIs | Reinforces PD-004 |
| AD-003 | Dia App | Namespace `Dia::<Module>::` | `Dia::Graphics3D::` per G3D-008 |
| GD-002 | DiaGraphics | FrameData copy must be trivially correct — no pointer members | `FrameData3D` and `Mesh3DFrameData` have no pointer members; all fields are value types (`Matrix44` = `float[16]`, `StringCRC` = `uint32_t`, `Vector3D` = 3 floats, `DynamicArrayC` = inline fixed-capacity storage). Per-frame copy cost ≈ 400 KB; acceptable. |
| **DiaMaths SD-005** | DiaMaths | All matrices share row-major `float m[N][N]` layout | `Camera3D` and `Mesh3DDrawCommand` carry `Matrix44` by value; layout owned by DiaMaths unchanged. Renderer transposes once at upload via `Matrix44::GetColumnMajor`. |
| **DiaMaths SD-007** | DiaMaths | Y-up right-handed convention | `Camera3D::SetView` documents `up` as Y-up; `Matrix44::Perspective` is Y-up RH per DiaMaths SD-007 |

---

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Module split | Why not just put 3D types in a `Mesh3D/` subdirectory of DiaGraphics as the original spec planned? | G3D-001: any subdirectory inside DiaGraphics still gets compiled into `DiaGraphics.lib`, so every 2D-only consumer (a 2D game, a tool that uses sprites) would transitively link DiaMaths::Matrix44. A separate `.vcxproj` is the only way to make the 3D dep truly opt-in. |
| 2 | FrameData3D necessity | Why does `FrameData3D` exist rather than just having the renderer reach into `Mesh3DFrameData` directly? | The renderer's `ProcessFrame` entry point needs a single typed argument. `FrameData3D` bundles the full 2D+3D packet: the 3D renderer needs both 2D state (sprites, debug) and 3D state (meshes, camera) in the same frame. Without `FrameData3D`, callers would have to pass both separately. |
| 3 | ICanvas not updated | DiaBgfx3D needs a different `ProcessFrame` signature than `ICanvas`. Doesn't this break the ICanvas contract? | DiaBgfx3D::Canvas3D overrides `ProcessFrame(const FrameData3D&)` as a new overload, not a virtual override. The `ICanvas::ProcessFrame(const FrameData&)` virtual is still satisfied by a thin thunk in Canvas3D that ignores the 2D-only call path. An `ICanvas3D` interface could be added later if multi-backend 3D is needed; premature for Phase 2 scope. |
| 4 | graphics-3d-types spec amendment | The existing `graphics-3d-types` feature spec says types land in `DiaGraphics` and `FrameData` inherits `Mesh3DFrameData`. That must be amended. Is that in scope now? | Yes — the `graphics-3d-types` spec's parent system and all file paths must be updated to reflect DiaGraphics3D. This is captured as an amendment task in that feature spec. The ACs and binding decisions remain valid; only the module home and `FrameData` change (G3D-003). |
| 5 | Namespace collision | `Dia::Graphics3D::` vs `Dia::Graphics::` — can these coexist cleanly? | Yes. They are distinct C++ namespaces; no collision. Headers are in different directories (`DiaGraphics/` vs `DiaGraphics3D/`). The `3D` suffix in the namespace name makes the ownership clear at every callsite. |
| 6 | `MockMesh3DFrameData` location | Previously spec'd in `DiaGraphics/Testing/`. Should it move? | Yes — it moves to `DiaGraphics3D/Testing/MockMesh3DFrameData.h`, since it depends on `Mesh3DFrameData` which is now in DiaGraphics3D. Any test that used the old location must update its include path. |
| 7 | Future features | What other features might land in DiaGraphics3D? | Candidate: `3d-debug-primitive-types` (equivalent of `DebugPrimitive` for 3D — AABB wireframe, 3D line, sphere outline), `particle-draw-command` (if a particle system is ever added), `material-descriptor-types` (if material authoring gets formalized). All deferred; system is open-ended. |
| 8 | Dependency on GD-002 | DiaGraphics3D inherits from DiaGraphics and must honour GD-002 (trivially-correct copy). Is `FrameData3D::operator=` safe? | Yes — `FrameData3D` has no pointer members. `Matrix44` is `float[16]`, `StringCRC` is `uint32_t`, `DynamicArrayC` is fixed-capacity inline storage. `memcpy(FrameData3D, FrameData3D)` is safe; default `operator=` can be used if the inherited base implementations are correct. |

---

## Status

`Done` — `graphics-3d-types` feature implemented and all 19 tests pass. Plan: @docs/specs/applications/dia/systems/diagraphics3d/diagraphics3d.plan.md
