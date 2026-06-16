# System Spec: DiaBgfx3D

## Parent Application
@docs/specs/applications/dia/dia.md

**Status:** `Approved`

---

## Purpose

DiaBgfx3D is the Phase 2 3D rendering layer for the Dia engine. It extends `DiaBgfx::Canvas` with 3D sub-renderers — `MeshRenderer`, `SkinnedMeshRenderer`, `ShadowRenderer` — and the supporting GPU-side infrastructure: `MaterialRegistry`, `MeshGpuCache`, and six `.sc` shader files (static mesh, skinned mesh, shadow caster, shared fragment shader).

It also provides `Canvas3D : DiaBgfx::Canvas` — the concrete rendering surface that accepts `Dia::Graphics3D::FrameData3D` and dispatches the 2D+3D passes in the correct order.

After this system ships, the complete Phase 2 pipeline is live: animation → pose → skinning palette → scene submit → bgfx 3D rendering. This is the Phase 2 ship gate per RB-002.

```
DiaScene3D::Submit(scene, frameData3D)
    ↓  populates FrameData3D
DiaBgfx3D::Canvas3D::ProcessFrame(frameData3D)
    1. ShadowRenderer  — depth pass from directional light POV
    2. MeshRenderer    — static opaque meshes
    3. SkinnedMeshRenderer — skinned meshes (vertex-shader skinning)
    4. SpriteRenderer  (inherited from DiaBgfx)
    5. DebugRenderer   (inherited from DiaBgfx)
    6. UIOverlayRenderer (inherited from DiaBgfx)
    7. ImGui           (inherited from DiaBgfx, EndFrame)
    ↓  bgfx::frame()
GPU (D3D11 / D3D12 via bgfx)
```

**Dependency chain:**
`DiaBgfx3D → DiaBgfx → DiaGraphics3D → DiaGraphics → DiaMaths → DiaCore`
`DiaBgfx3D → DiaScene3D → DiaSkinning3D → DiaAnimation3D → DiaRig3D → DiaMesh3D`

---

## Responsibilities

- Provide `Dia::Bgfx3D::Canvas3D : DiaBgfx::Canvas` — extends the Phase 1 canvas with 3D sub-renderers; `ProcessFrame(const FrameData3D&)` dispatches all passes in correct order
- Provide `Dia::Bgfx3D::MeshRenderer` — walks `Mesh3DFrameData` static draw commands; sets transform, binds material, submits indexed draw
- Provide `Dia::Bgfx3D::SkinnedMeshRenderer` — walks skinned draw commands; binds `SkinningPalette` as `u_skinningPalette mat3x4[256]` uniform before each draw
- Provide `Dia::Bgfx3D::ShadowRenderer` — depth-only pass from directional light POV; single cascade; produces a depth render target sampled in the main pass
- Provide `Dia::Bgfx3D::MaterialRegistry` — `StringCRC → ShaderProgram* + base colour`; fallback default material for unresolved ids; growth path to per-material uniform arrays
- Provide `Dia::Bgfx3D::MeshGpuCache` — lazily converts `DiaMesh3D::Mesh3DAsset` vertex/index data to bgfx `VertexBufferHandle`/`IndexBufferHandle` on first Ready reference; `DestroyAll()` on shutdown
- Provide six `.sc` shader files under `Dia/DiaBgfx3D/Shaders/3d/`: `vs_mesh.sc`, `vs_skinned_mesh.sc`, `fs_mesh.sc` (shared), `vs_shadow_caster.sc`, `fs_shadow_caster.sc`, `varying.def.sc`
- Provide `DiaBgfx3D.vcxproj` static library project registered in `Cluiche.sln`
- Provide `dia.bgfx3d.architecture.module.md` YAML module documentation

## Non-Responsibilities

- **2D rendering** — owned by DiaBgfx (Phase 1); `Canvas3D` inherits it, does not re-implement it
- **Window and input** — owned by DiaSFML
- **PBR shading, IBL, post-processing** — explicitly out of scope (RB-001, "light 3D, no photorealism")
- **Multiple cascaded shadow maps** — single cascade only; CSM is a follow-up
- **Point/spot light shadows** — only directional shadow in Phase 2
- **Material parameter authoring** — `MaterialRegistry` stores `program + base colour`; full material system is post-Phase-2
- **Mesh LOD** — out of scope
- **Compute-based skinning** — vertex-shader skinning only
- **Hot-reload of shaders or materials** — out of scope
- **Terrain rendering** — Phase 3 (`DiaTerrain`), separate spec
- **3D IK** — out of scope

---

## Public Interfaces

### `Dia::Bgfx3D::Canvas3D`

```cpp
// Dia/DiaBgfx3D/Canvas3D.h
namespace Dia { namespace Bgfx3D {

class Canvas3D : public Dia::Bgfx::Canvas
{
public:
    Canvas3D();
    ~Canvas3D() override;

    // Extended entry point accepting the full 2D+3D frame packet.
    // Dispatches shadow pass → mesh passes → inherited 2D passes → ImGui (EndFrame).
    void ProcessFrame(const Dia::Graphics3D::FrameData3D& frameData);

    // App wire-up: register materials before rendering begins.
    MaterialRegistry* GetMaterialRegistry();

private:
    unsigned short          mMeshViewId;
    unsigned short          mShadowViewId;
    MeshRenderer*           mMeshRenderer;
    SkinnedMeshRenderer*    mSkinnedMeshRenderer;
    ShadowRenderer*         mShadowRenderer;
    MeshGpuCache*           mMeshGpuCache;
    MaterialRegistry        mMaterialRegistry;
    // shadow framebuffer handle (bgfx::FrameBufferHandle::idx)
    unsigned short          mShadowFramebuffer;
};

} }
```

### `Dia::Bgfx3D::MaterialRegistry`

```cpp
// Dia/DiaBgfx3D/Resources/MaterialRegistry.h
namespace Dia { namespace Bgfx3D {

class ShaderProgram;

struct MaterialDescriptor
{
    Dia::Core::StringCRC id;
    ShaderProgram*       program;        // not owned; lives in Canvas3D
    uint32_t             baseColourRGBA; // 0xFFFFFFFF default
};

class MaterialRegistry
{
public:
    static constexpr unsigned int kMaxMaterials = 256;

    MaterialRegistry();

    void                        Register(const MaterialDescriptor& desc);
    const MaterialDescriptor*   Resolve(Dia::Core::StringCRC id) const;
    const MaterialDescriptor&   GetDefault() const;

private:
    Dia::Core::Containers::DynamicArrayC<MaterialDescriptor, kMaxMaterials> mMaterials;
    MaterialDescriptor                                                       mDefault;
};

} }
```

### `Dia::Bgfx3D::MeshGpuCache`

```cpp
// Dia/DiaBgfx3D/Resources/MeshGpuCache.h
namespace Dia { namespace Bgfx3D {

struct GpuMesh
{
    unsigned short vertexBuffer; // bgfx::VertexBufferHandle::idx
    unsigned short indexBuffer;  // bgfx::IndexBufferHandle::idx
    uint32_t       indexCount;
};

class MeshGpuCache
{
public:
    MeshGpuCache();
    ~MeshGpuCache();

    const GpuMesh* GetOrUpload(const Dia::Mesh3D::Mesh3DAsset& asset);
    void DestroyAll();

private:
    // StringCRC → GpuMesh; private so STL map does not violate PD-004
};

} }
```

### `Dia::Bgfx3D::MeshRenderer`

```cpp
// Dia/DiaBgfx3D/Renderers/MeshRenderer.h
namespace Dia { namespace Bgfx3D {

class MeshRenderer
{
public:
    MeshRenderer(unsigned short viewId, MeshGpuCache* cache, MaterialRegistry* materials);
    void Draw(const Dia::Graphics3D::Mesh3DFrameData& frameData);

private:
    void DrawCommand(const Dia::Graphics3D::Mesh3DDrawCommand& cmd);

    unsigned short    mViewId;
    MeshGpuCache*     mCache;
    MaterialRegistry* mMaterials;
};

} }
```

### `Dia::Bgfx3D::SkinnedMeshRenderer`

```cpp
// Dia/DiaBgfx3D/Renderers/SkinnedMeshRenderer.h
namespace Dia { namespace Bgfx3D {

class SkinnedMeshRenderer
{
public:
    SkinnedMeshRenderer(unsigned short viewId, MeshGpuCache* cache, MaterialRegistry* materials);
    void Draw(const Dia::Graphics3D::Mesh3DFrameData& frameData);

private:
    unsigned short    mViewId;
    MeshGpuCache*     mCache;
    MaterialRegistry* mMaterials;
    unsigned short    mPaletteUniform; // bgfx::UniformHandle::idx — u_skinningPalette mat3x4[256]
};

} }
```

---

## Features

| Feature | Description | Spec | Status |
|---------|-------------|------|--------|
| diabgfx3d-gpu-resources | `MaterialRegistry` (StringCRC → shader + base colour) and `MeshGpuCache` (lazy vertex/index upload); no bgfx types in public surface | [gpu-resources.md](gpu-resources.md) | Approved |
| diabgfx3d-3d-renderers | `MeshRenderer`, `SkinnedMeshRenderer`, `ShadowRenderer`; six `.sc` shader files; lambert + shadow fragment shader | [3d-renderers.md](3d-renderers.md) | Approved |
| diabgfx3d-canvas3d | `Canvas3D : DiaBgfx::Canvas`; full 2D+3D pass dispatch; `DiaBgfx3D.vcxproj`; CluicheTest 3D demo; **Phase 2 ship gate (RB-002)** | [canvas3d.md](canvas3d.md) | Approved |

*Additional features (instanced rendering, CSM, point-light shadows, compute skinning) will be added as separate feature specs when needed.*

---

## Dependencies on Other Systems

| System | Role |
|--------|------|
| **DiaBgfx** | `Canvas3D` extends `DiaBgfx::Canvas`; inherits 2D renderers, ImGui backend, shader/texture infrastructure |
| **DiaGraphics3D** | `FrameData3D`, `Mesh3DFrameData`, `Camera3D`, lights, `Mesh3DDrawCommand` |
| **DiaGraphics** | `FrameData` (base of `FrameData3D`), `ICanvas` |
| **DiaMesh3D** | `Mesh3DAsset` — vertex/index data consumed by `MeshGpuCache` |
| **DiaRig3D** | `SkeletonComponent3D` — consumed by `DiaAnimation3D` which feeds `DiaSkinning3D` |
| **DiaAnimation3D** | Drives poses consumed by `DiaSkinning3D` |
| **DiaSkinning3D** | `SkinningManager` — produces per-frame `Matrix34` palettes read by `SkinnedMeshRenderer` |
| **DiaScene3D** | Produces the populated `FrameData3D` that `Canvas3D::ProcessFrame` consumes |
| **DiaMaths** | `Matrix44::GetColumnMajor` for transform upload; `Matrix34` for skinning palette |
| **DiaGeometry3D** | `Frustum` / `AABB` — used in shadow caster culling (future optimisation; scene-graph already culled for camera) |
| **DiaPipeline** | `bgfx-shader-cook` cooks the six `.sc` files into `Cluiche/out/<App>/shaders/<backend>/3d/` |
| **DiaCore** | `StringCRC`, `DynamicArrayC` |

**Explicitly excluded from dependencies:**
- DiaSFML (window handle already wired in Phase 1 through `DiaBgfx`)
- DiaInput (unchanged)

**Dependents:**
- **CluicheTest** — 3D demo scene rendered via `Canvas3D`
- **DiaVisualDebugger** — 3D debug draw (future feature); will emit into `DebugFrameData` which `Canvas3D` inherits from the 2D path

---

## Decisions

| ID | Decision | Rationale | Scope | Status | Binding |
|----|----------|-----------|-------|--------|---------|
| BG3-001 | DiaBgfx3D is a separate module (`Dia/DiaBgfx3D/`, own `.vcxproj`) — not an extension of DiaBgfx | 2D-only games link DiaBgfx but not DiaBgfx3D. The DiaScene3D chain (5 modules) is only pulled in by apps that use 3D rendering. Separate module = clean opt-in. | Module structure | Accepted | Yes |
| BG3-002 | `Canvas3D : DiaBgfx::Canvas` — 3D canvas extends the Phase 1 canvas rather than reimplementing it | All 2D passes (sprite, debug, UI overlay, ImGui) are inherited unchanged. Phase 2 adds only the 3D passes on top. Code reuse; consistent frame ordering. | Canvas3D | Accepted | Yes |
| BG3-003 | `Canvas3D::ProcessFrame` accepts `FrameData3D`, not the base `FrameData` | `FrameData3D` carries both 2D and 3D data. The override is a new overload — the base `ICanvas::ProcessFrame(const FrameData&)` is still satisfied by a thunk for callers that don't know about 3D. | Canvas3D API | Accepted | Yes |
| BG3-004 | No visitor pattern in the 3D render path — sub-renderers consume `Mesh3DFrameData` directly | Consistent with RB-005; direct consumption is simpler and avoids indirection overhead in the hot path. | All renderers | Accepted | Yes |
| BG3-005 | No bgfx types in DiaGraphics3D or higher public surfaces | `bgfx::VertexBufferHandle`, `bgfx::ProgramHandle`, etc. must not appear above `DiaBgfx3D`. Compliant with RB-006. | Public API boundary | Accepted | Yes |
| BG3-006 | Skinning uses vertex-shader skinning with `u_skinningPalette mat3x4[256]` uniform | Compute-based skinning deferred. 256-bone cap is the bgfx `createUniform` array limit and is sufficient for "light 3D" scope. | SkinnedMeshRenderer | Accepted | Yes |
| BG3-007 | Single-cascade shadow map from the directional light; 2048×2048 depth target | Adequate for stylized "light 3D". CSM is the correct next step when shadow quality becomes a concern; deferred. | ShadowRenderer | Accepted | Yes |
| BG3-008 | `MaterialRegistry` is a flat `DynamicArrayC` of `MaterialDescriptor`; growth path is a `params` array per descriptor | PD-004: no STL map. 256 materials is generous for Phase 2. The growth path (per-material uniform arrays) is documented but out of scope. | MaterialRegistry | Accepted | Yes |
| BG3-009 | `MeshGpuCache` uses a private STL map internally; public surface is clean (PD-004) | Internal implementation may use STL; only the public API must be STL-free. StringCRC → GpuMesh lookup benefits from O(1) hash map. | MeshGpuCache | Accepted | Yes |
| BG3-010 | Phase 2 ship gate: CluicheTest renders a glTF skinned character animated at 60 FPS (64 characters target); all existing 2D tests remain green | Hard checkpoint — Phase 2 is only Done when this is visually verified. Per RB-002. | Phase 2 ship gate | Accepted | Yes |
| BG3-011 | Namespace is `Dia::Bgfx3D::` | Consistent with AD-003 and clearly distinct from `Dia::Bgfx::`. | All DiaBgfx3D types | Accepted | Yes |

---

## Inherited Binding Decisions

| ID | Source | Decision | Implication for this system |
|----|--------|----------|-----------------------------|
| PD-001 | Platform | StringCRC for all entity/component IDs | `meshId`, `materialId` are `StringCRC`; `MeshGpuCache` and `MaterialRegistry` keyed by `StringCRC` |
| PD-002 | Platform | ProcessingUnit/Phase/Module architecture | `Canvas3D::ProcessFrame` called from the Render `ProcessingUnit` phase; `bgfx::frame()` in `EndFrame()`; cross-thread record/submit respects Phase boundaries |
| PD-004 | Platform | No STL containers in public APIs | `MaterialRegistry` uses `DynamicArrayC`; `MeshGpuCache` public API is clean (STL permitted internally per BG3-009) |
| PD-005 | Platform | x64 only | All `.vcxproj` targets x64 |
| PD-006 | Platform | Visual Studio project files are source of truth | `DiaBgfx3D.vcxproj` + `.vcxproj.filters` maintained manually; registered in `Cluiche.sln` |
| PD-007 | Platform | C++20 | All sources compiled under `/std:c++20` |
| PD-008 | Platform | `Directory.Build.props` owns OutDir/IntDir | `DiaBgfx3D.vcxproj` inherits; no per-project overrides |
| PD-009 | Platform | Generated output under `Cluiche/out/<AppName>/` | Cooked shader binaries land under `Cluiche/out/<AppName>/shaders/<backend>/3d/` |
| AD-001 | Dia App | Module YAML frontmatter documentation | `dia.bgfx3d.architecture.module.md` required |
| AD-002 | Dia App | No STL containers in public APIs | Reinforces PD-004 |
| AD-003 | Dia App | Namespace `Dia::<Module>::` | `Dia::Bgfx3D::` per BG3-011 |
| **RB-001** | RenderBackend | bgfx is the GPU abstraction | DiaBgfx3D builds directly on bgfx; no secondary abstraction |
| **RB-002** | RenderBackend | Two-phase delivery; Phase 2 ship gate | DiaBgfx3D is the Phase 2 ship gate — system is Done when BG3-010 is verified |
| **RB-004** | RenderBackend | Canvas implements ICanvas only — no window/input conflation | `Canvas3D` extends `Canvas`; adds no window or input surface |
| **RB-005** | RenderBackend | No visitor pattern in production render path | All sub-renderers consume `Mesh3DFrameData` directly (BG3-004) |
| **RB-006** | RenderBackend | No bgfx types in public surface above DiaBgfx/DiaBgfx3D | Enforced by BG3-005 |
| **RB-013** | RenderBackend | Phase 2 module decomposition mirrors 2D family | DiaBgfx3D is the renderer; the five upstream modules are the data producers |
| **DiaMaths SD-005** | DiaMaths | Row-major `float m[N][N]` matrix layout | `Matrix44::GetColumnMajor(float[16])` called once per transform at upload; bgfx receives column-major |
| **DiaMaths SD-008** | DiaMaths | `Matrix34` is the affine skinning type | Skinning palette is `Matrix34[]`; `SkinnedMeshRenderer` uploads as `u_skinningPalette mat3x4[256]` |
| **G3D-002** | DiaGraphics3D | `FrameData3D` is the 2D+3D frame packet | `Canvas3D::ProcessFrame` accepts `FrameData3D` (BG3-003) |

---

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Module split from DiaBgfx | Why not extend DiaBgfx directly with the 3D renderers rather than a new module? | BG3-001: a 2D-only game links DiaBgfx. If 3D renderers were inside DiaBgfx, that game would transitively pull in DiaMesh3D, DiaRig3D, DiaAnimation3D, DiaSkinning3D, DiaScene3D — five modules it never uses. Separate module = clean opt-in, matching the DiaGraphics3D decision. |
| 2 | Canvas3D ICanvas conformance | `ICanvas::ProcessFrame` takes `FrameData`. `Canvas3D::ProcessFrame` takes `FrameData3D`. How does `Canvas3D` satisfy `ICanvas`? | Canvas3D inherits the base `DiaBgfx::Canvas::ProcessFrame(const FrameData&)` virtual which is already a valid `ICanvas` implementation. Canvas3D adds a non-virtual `ProcessFrame(const FrameData3D&)` overload. Callers that know about 3D call the overload directly; callers that only know `ICanvas` call the base. If a future `ICanvas3D` interface is needed, it can be added then. |
| 3 | Frame pass ordering | Why is shadow pass first, before the mesh pass? | The shadow map depth buffer must be populated before the main mesh pass samples from it. bgfx view ordering enforces this: `mShadowViewId < mMeshViewId`. |
| 4 | MeshGpuCache eviction | When is GPU memory reclaimed? | On `DestroyAll()` called by `Canvas3D` destructor / shutdown. Phase 2 does not unload meshes mid-game; eviction on asset Unload is a TODO flagged in the feature spec. |
| 5 | SkinningManager coupling | `SkinnedMeshRenderer` reads from `DiaSkinning3D::SkinningManager::Instance()`. Is this a singleton coupling concern? | Yes — it's the accepted Phase 2 approach. `SkinningManager` follows the Dia singleton pattern (`Dia::Core::Singleton<T>`). A cleaner injection path (pass palette pointer per draw command) is a post-Phase-2 refactor. |
| 6 | Shadow caster culling | Does `ShadowRenderer` cull against the light frustum? | Phase 2: no optimisation — all visible-from-camera meshes are also rendered into the shadow map. Light-frustum culling is a future optimisation. Captured as a follow-up. |
| 7 | bgfx dependency in DiaBgfx3D | DiaBgfx3D depends on bgfx directly (for buffer creation, uniform submission). Does that mean bgfx types appear in DiaBgfx3D headers? | `GpuMesh` stores `unsigned short` indices (bgfx handle `.idx` fields), not `bgfx::VertexBufferHandle` structs. This keeps bgfx types internal per BG3-005. The `.h` files include only engine types; `.cpp` files include bgfx headers. |
| 8 | 3d-renderers feature spec re-homing | The existing `3d-renderers.md` was under `diabgfx/`. It needs to move to `diabgfx3d/`. | Correct — file moves to `docs/specs/features/dia/diabgfx3d/3d-renderers.md`; parent system reference and namespace updated from `Dia::Bgfx::` to `Dia::Bgfx3D::`. This is captured as a required amendment in the feature spec. |
| 9 | Performance bar | 64 animated characters at 60 FPS is the bar. Is that achievable? | Per the original feature spec AR-11: 64 × 2 draws × ~10 μs CPU submit ≈ 1.3 ms render submission; ~1 MB uniform traffic per frame; well within headroom for D3D11. Bar is conservative. |
| 10 | Phase 2 status gate | When does this system flip to Done? | When BG3-010 is verified: CluicheTest 3D demo renders a glTF skinned character animated, all 2D tests remain green, and the system spec + render-backend spec both flip to Done. |

---

## Status

`Approved` — All 5 spec steps complete. System cannot be marked `Done` until `diabgfx3d-canvas3d` feature spec is `Done` (Phase 2 ship gate, RB-002). Phase 2 stays `Approved` (not `In Progress`) until Phase 1 ships (RB-003).
