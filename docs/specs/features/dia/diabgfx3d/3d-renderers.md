# Feature Spec: 3d-renderers

## Parent System
@docs/specs/systems/dia/diabgfx3d.md

**Cross-cutting system:** @docs/specs/systems/dia/render-backend.md (Phase 2 ship gate — RB-002)

> **Amendment note (2026-05-18):** Re-homed from `DiaBgfx` to `DiaBgfx3D` per BG3-001. Module is now `Dia/DiaBgfx3D/`, namespace is `Dia::Bgfx3D::`, canvas is `Canvas3D : DiaBgfx::Canvas`. All ACs and shader specs unchanged.

**Hard dependencies (this batch):**
- `canvas-parity` (Approved) — `Dia::Bgfx::Canvas` exists (Phase 1)
- `imgui-backend` (Approved) — ImGui works (Phase 1)
- `graphics-3d-types` (Approved) — `Dia::Graphics3D::Mesh3DFrameData`, `Camera3D`, lights
- `mesh-asset-and-loader` (Approved) — Mesh3DAsset for vertex/index data
- `skeleton-and-pose` (Approved) — SkeletonComponent3D
- `clip-and-player` (Approved) — animation drives the pose
- `skinning-palette` (Approved) — SkinningManager produces per-frame palettes
- `scene-graph` (Approved) — produces the populated Mesh3DFrameData

**Hard dependencies (already specced separately):**
- @docs/specs/systems/dia/diamaths.md — `Matrix44::GetColumnMajor`
- @docs/specs/systems/dia/diageometry3d.md — `Frustum`, `AABB`

**Research:** @docs/research/render_backend_swap/summary.md

## Status
`Approved`

## Summary

Create `Dia::Bgfx3D::Canvas3D` (extends `Dia::Bgfx::Canvas`) with Phase 2 sub-renderers: `MeshRenderer` (static meshes), `SkinnedMeshRenderer` (skinned meshes via vertex-shader skinning), a simple **forward shader** (one directional + N point lights, no PBR), and an optional **single-cascade shadow map** for the directional light. Adds a `MaterialRegistry` that resolves `materialId` (StringCRC) to shader programs + uniform values. Adds the cooked shader files for these passes (mesh, skinned mesh, shadow caster) under `Dia/DiaBgfx3D/Shaders/3d/`.

After this feature, the **complete Phase 2 pipeline** lights up: animation → pose → skinning palette → scene submit → bgfx mesh rendering. The renderer consumes `Mesh3DFrameData` (populated by DiaScene3D) and produces visible 3D output. CluicheTest gains a 3D demo scene that loads a glTF character + animation and renders it.

## Problem

`canvas-parity` shipped 2D rendering on bgfx. `graphics-3d-types` defined the per-frame 3D type seam in `DiaGraphics3D`. Every Phase 2 producer module (`diamesh3d`, `diarig3d`, `diaanimation3d`, `diaskinning3d`, `diascene3d`) populates `Mesh3DFrameData` — but **nothing consumes it yet**. This feature is the consumer: `Bgfx3D::Canvas3D`, `Bgfx3D::MeshRenderer`, and `Bgfx3D::SkinnedMeshRenderer` walk the Mesh3DFrameData's draw commands and submit them to bgfx with the right shader program, transform, and (for skinned) skinning palette uniform.

The shader story is bgfx-shaderc-cooked `.sc` files (per `bgfx-shader-cook`). A `MaterialRegistry` maps the abstract `materialId` to a shader program + per-material uniforms (base colour, etc.). Phase 2's brief is "no PBR" — the shader is intentionally simple: lambert + ambient + optional shadow.

## Goals

- Create `Dia::Bgfx3D::Canvas3D : DiaBgfx::Canvas` — new module entry point; `ProcessFrame(const FrameData3D&)` dispatches shadow → mesh → inherited 2D passes
- Add `Dia::Bgfx3D::MeshRenderer` — consumes static `Mesh3DDrawCommand`s, uploads per-mesh transform via `bgfx::setTransform`, binds material program, draws indexed primitives
- Add `Dia::Bgfx3D::SkinnedMeshRenderer` — same shape but binds the per-frame `SkinningPalette` as a `mat3x4 u_skinningPalette[256]` uniform before each draw
- Add `Dia::Bgfx3D::MaterialRegistry` — `Register(StringCRC id, ShaderProgram*)`; `Resolve(StringCRC id) → ShaderProgram*`
- Add `Dia::Bgfx3D::MeshGpuCache` — converts `Dia::Mesh3D::Mesh3DAsset` (host-side vertex/index data) to bgfx `VertexBufferHandle`/`IndexBufferHandle` lazily on first reference; observes asset State transitions to Ready before uploading
- Add forward-shader `.sc` files: `vs_mesh.sc`, `fs_mesh.sc` (static), `vs_skinned_mesh.sc`, `fs_mesh.sc` (shared), and `vs_shadow_caster.sc`, `fs_shadow_caster.sc` (depth-only pass for shadow map)
- Reserve a shadow-map view in `Bgfx3D::Canvas3D` (a 5th view id beyond the 4 inherited from `DiaBgfx::Canvas`)
- Render flow per frame:
  1. Shadow pass — render all opaque meshes from directional light's POV into a depth-only render target (cascaded shadow map deferred; single cascade for now)
  2. Main pass — for each `Mesh3DDrawCommand`: resolve material → shader program; if `skinningPaletteIndex != 0`, bind palette and use skinned VS; else use static VS; bind shadow map for sampling; draw indexed
- Performance bar: 64 visible animated characters at 60 FPS on dev hardware (D3D11)
- Tests under `Dia/DiaBgfx/Testing/` for `MaterialRegistry` (lookup miss returns default material) and `MeshGpuCache` (state transitions; reupload on asset reload — out of scope but seam reserved)

## Non-Goals

- **PBR shading** — RB-001 explicit; lambert + ambient + (optional) directional shadow only
- **IBL / image-based lighting** — out of scope
- **Multiple cascaded shadow maps** — single cascade for the directional light. Cascaded shadow maps are a follow-up
- **Point/spot light shadows** — out of scope; only directional shadow
- **Post-processing chain** — bloom, tonemap, AA — out of scope. Linear-space lighting is fine but no HDR pipeline
- **Material instancing / bgfx::setInstanceDataBuffer** — submit per-mesh; instancing is a future optimisation
- **Compute-based skinning** — vertex-shader skinning per Phase 2 brief
- **Custom material parameters** — material is just `shaderProgram + base colour`; per-material uniforms beyond base colour are out of scope. The MaterialRegistry's API supports growth to full material params later
- **Render graph** — straight-line render passes; full render graph is post-Phase-2
- **Hot-reload** — out of scope (matches `bgfx-shader-cook`)
- **Mesh LOD** — out of scope
- **Frustum culling** — already done in `scene-graph` (DiaScene3D); renderer trusts the input
- **Terrain rendering** — Phase 3

## Public Interfaces

### `Dia::Bgfx::MaterialRegistry`

```cpp
// Dia/DiaBgfx/Resources/MaterialRegistry.h
#pragma once

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

namespace Dia { namespace Bgfx {

class ShaderProgram;

struct MaterialDescriptor
{
    Dia::Core::StringCRC id;
    ShaderProgram*       program;     // not owned; lives in Canvas
    uint32_t             baseColourRGBA;  // 0xFFFFFFFF default
};

class MaterialRegistry
{
public:
    static constexpr unsigned int kMaxMaterials = 256;

    MaterialRegistry();

    void                       Register(const MaterialDescriptor& desc);
    const MaterialDescriptor*  Resolve(Dia::Core::StringCRC id) const;
    const MaterialDescriptor&  GetDefault() const;   // fallback for unresolved ids

private:
    Dia::Core::Containers::DynamicArrayC<MaterialDescriptor, kMaxMaterials> mMaterials;
    MaterialDescriptor                                                      mDefault;
};

} }
```

### `Dia::Bgfx::MeshGpuCache`

```cpp
// Dia/DiaBgfx/Resources/MeshGpuCache.h
#pragma once

#include <DiaCore/CRC/StringCRC.h>

namespace Dia { namespace Mesh3D { class Mesh3DAsset; } }

namespace Dia { namespace Bgfx {

struct GpuMesh
{
    unsigned short vertexBuffer;     // bgfx::VertexBufferHandle::idx
    unsigned short indexBuffer;      // bgfx::IndexBufferHandle::idx
    uint32_t       indexCount;       // total
    // Per-submesh start/count carried via Submesh — looked up on submit
};

class MeshGpuCache
{
public:
    MeshGpuCache();
    ~MeshGpuCache();

    // Returns a GpuMesh for the asset, uploading on first request if the asset
    // is Ready. Returns nullptr if the asset is not Ready (renderer skips draw).
    const GpuMesh* GetOrUpload(const Dia::Mesh3D::Mesh3DAsset& asset);

    // Tear down all GPU resources (called by Canvas::Shutdown).
    void DestroyAll();

private:
    // assetId → GpuMesh
    // (uses unordered_map internally; private so PD-004 not violated)
};

} }
```

### `Dia::Bgfx::MeshRenderer`

```cpp
// Dia/DiaBgfx/Renderers/MeshRenderer.h
#pragma once

namespace Dia { namespace Graphics { class Mesh3DFrameData; struct Mesh3DDrawCommand; } }

namespace Dia { namespace Bgfx {

class MeshGpuCache;
class MaterialRegistry;

class MeshRenderer
{
public:
    MeshRenderer(unsigned short viewId, MeshGpuCache* cache, MaterialRegistry* materials);

    // Draws all *static* mesh draw commands from frameData (skinningPaletteIndex == 0).
    void Draw(const Dia::Graphics::Mesh3DFrameData& frameData);

private:
    void DrawCommand(const Dia::Graphics::Mesh3DDrawCommand& cmd);

    unsigned short    mViewId;
    MeshGpuCache*     mCache;          // not owned
    MaterialRegistry* mMaterials;      // not owned
};

} }
```

### `Dia::Bgfx::SkinnedMeshRenderer`

```cpp
// Dia/DiaBgfx/Renderers/SkinnedMeshRenderer.h
#pragma once

namespace Dia { namespace Bgfx {

class SkinnedMeshRenderer
{
public:
    SkinnedMeshRenderer(unsigned short viewId, MeshGpuCache* cache, MaterialRegistry* materials);

    // Draws all *skinned* mesh draw commands (skinningPaletteIndex != 0).
    // Reads the palette from Skinning3D::SkinningManager::Instance().
    void Draw(const Dia::Graphics::Mesh3DFrameData& frameData);

private:
    unsigned short    mViewId;
    MeshGpuCache*     mCache;
    MaterialRegistry* mMaterials;
    unsigned short    mPaletteUniform; // bgfx::UniformHandle::idx — u_skinningPalette mat3x4[256]
};

} }
```

### `Dia::Bgfx::Canvas` (extended)

```cpp
// Add to Canvas:
class Canvas /* existing inheritance */
{
public:
    // ... existing API ...

    MaterialRegistry* GetMaterialRegistry();   // app wire-up registers materials here

private:
    // existing fields...
    unsigned short        mMeshViewId;          // Phase 2: 3D opaque pass
    unsigned short        mShadowViewId;        // Phase 2: shadow caster pass
    MeshRenderer*         mMeshRenderer;        // Phase 2
    SkinnedMeshRenderer*  mSkinnedMeshRenderer; // Phase 2
    MeshGpuCache*         mMeshGpuCache;        // Phase 2
    MaterialRegistry      mMaterialRegistry;    // Phase 2
    // shadow framebuffer, programs...
};
```

`Canvas::ProcessFrame` ordering becomes:
1. `mShadowRenderer->RenderShadowMap(frameData)` — depth-only pass
2. `mMeshRenderer->Draw(frameData)` — static meshes (main pass)
3. `mSkinnedMeshRenderer->Draw(frameData)` — skinned meshes (main pass)
4. `mSpriteRenderer->Draw(frameData.GetEntityFrameData())` — 2D sprites (existing)
5. `mDebugRenderer->Draw(frameData.GetDebugFrameData())` — debug primitives (existing)
6. `mUIOverlayRenderer->Composite(frameData.GetUIData())` — UI overlay (existing)

ImGui still draws last in `EndFrame`.

## Implementation

### Files introduced

```
Dia/DiaBgfx3D/Canvas3D.h                        NEW
Dia/DiaBgfx3D/Canvas3D.cpp                      NEW
Dia/DiaBgfx3D/Renderers/MeshRenderer.h          NEW
Dia/DiaBgfx3D/Renderers/MeshRenderer.cpp        NEW
Dia/DiaBgfx3D/Renderers/SkinnedMeshRenderer.h   NEW
Dia/DiaBgfx3D/Renderers/SkinnedMeshRenderer.cpp NEW
Dia/DiaBgfx3D/Renderers/ShadowRenderer.h        NEW
Dia/DiaBgfx3D/Renderers/ShadowRenderer.cpp      NEW
Dia/DiaBgfx3D/Resources/MeshGpuCache.h          NEW
Dia/DiaBgfx3D/Resources/MeshGpuCache.cpp        NEW
Dia/DiaBgfx3D/Resources/MaterialRegistry.h      NEW
Dia/DiaBgfx3D/Resources/MaterialRegistry.cpp    NEW
Dia/DiaBgfx3D/DiaBgfx3D.vcxproj                 NEW
Dia/DiaBgfx3D/DiaBgfx3D.vcxproj.filters         NEW
Dia/DiaBgfx3D/dia.bgfx3d.architecture.module.md NEW

Dia/DiaBgfx3D/Shaders/3d/varying.def.sc         NEW
Dia/DiaBgfx3D/Shaders/3d/vs_mesh.sc             NEW
Dia/DiaBgfx3D/Shaders/3d/vs_skinned_mesh.sc     NEW
Dia/DiaBgfx3D/Shaders/3d/fs_mesh.sc             NEW (shared by static + skinned)
Dia/DiaBgfx3D/Shaders/3d/vs_shadow_caster.sc    NEW
Dia/DiaBgfx3D/Shaders/3d/fs_shadow_caster.sc    NEW
```

### Files modified

```
Dia/DiaBgfx/Canvas.h / .cpp
   - NO CHANGE — Canvas3D extends Canvas; no modifications to Phase 1 canvas

Cluiche/Cluiche.sln
   - Register DiaBgfx3D.vcxproj

Cluiche/CluicheTest/... (kernel module or main)
   - Register a default material on Canvas3D: lambert + ambient + base colour
   - Construct a Scene3D, populate with a sample skinned glTF asset, drive AnimationComponent3D::Update each tick
   - Use Canvas3D instead of Canvas for the 3D demo scene

Cluiche/Tests/GoogleTests/DiaBgfx3D/
   - TestMeshRenderer (Noop renderer)
   - TestSkinnedMeshRenderer (Noop)
   - TestMaterialRegistry
```

### Forward-shader pseudocode (`fs_mesh.sc`)

```glsl
// Inputs from VS
varying vec3 v_worldPos;
varying vec3 v_worldNormal;
varying vec2 v_uv0;

uniform vec4  u_baseColour;
uniform vec4  u_directionalLightDir;       // xyz = direction (unit), w unused
uniform vec4  u_directionalLightColour;    // rgb = colour * intensity, w unused
uniform vec4  u_ambient;                   // rgb, w unused
SAMPLER2D(s_shadowMap, 0);
uniform mat4  u_lightViewProj;             // for shadow lookup

void main()
{
    vec3 N = normalize(v_worldNormal);
    vec3 L = -u_directionalLightDir.xyz;
    float NdotL = max(0.0, dot(N, L));

    // Shadow lookup
    vec4 lightSpacePos = mul(u_lightViewProj, vec4(v_worldPos, 1.0));
    vec3 lightSpaceUV = lightSpacePos.xyz / lightSpacePos.w;
    lightSpaceUV.xy = lightSpaceUV.xy * 0.5 + 0.5;
    float depthInLight = lightSpaceUV.z;
    float shadowDepth = texture2D(s_shadowMap, lightSpaceUV.xy).r;
    float shadow = (depthInLight - 0.001 > shadowDepth) ? 0.5 : 1.0;

    vec3 radiance = u_directionalLightColour.rgb * NdotL * shadow + u_ambient.rgb;
    gl_FragColor = vec4(u_baseColour.rgb * radiance, 1.0);
}
```

(Shadow lookup is naive — single-tap, fixed bias. Adequate for "light 3D".)

### Vertex shader skinning (`vs_skinned_mesh.sc`)

```glsl
attribute vec3  a_position;
attribute vec3  a_normal;
attribute vec2  a_texcoord0;
attribute uvec4 a_indices;       // bone indices (4)
attribute vec4  a_weight;        // bone weights (4)

uniform mat4 u_modelViewProj;
uniform mat4 u_model;
uniform mat3x4 u_skinningPalette[256];

void main()
{
    mat3x4 boneMatrix =
        u_skinningPalette[a_indices.x] * a_weight.x +
        u_skinningPalette[a_indices.y] * a_weight.y +
        u_skinningPalette[a_indices.z] * a_weight.z +
        u_skinningPalette[a_indices.w] * a_weight.w;

    vec4 skinnedPos = vec4(mul(boneMatrix, vec4(a_position, 1.0)), 1.0);
    gl_Position = mul(u_modelViewProj, skinnedPos);
    v_worldPos    = mul(u_model, skinnedPos).xyz;
    v_worldNormal = mul(boneMatrix, vec4(a_normal, 0.0)).xyz;
    v_uv0         = a_texcoord0;
}
```

`vs_mesh.sc` is the same minus the skinning matrix multiply.

### Per-draw-command flow (MeshRenderer)

```cpp
void MeshRenderer::DrawCommand(const Mesh3DDrawCommand& cmd)
{
    if (cmd.skinningPaletteIndex != SkinningManager::kStaticPaletteIndex)
        return;  // SkinnedMeshRenderer handles these

    const Mesh3DAsset* asset = AssetRuntime::Get<Mesh3DAsset>(cmd.meshId);
    if (!asset || !asset->IsReady()) return;

    const GpuMesh* gpu = mCache->GetOrUpload(*asset);
    if (!gpu) return;

    const MaterialDescriptor* mat = mMaterials->Resolve(cmd.materialId);
    if (!mat) mat = &mMaterials->GetDefault();

    float transformCM[16];
    cmd.transform.GetColumnMajor(transformCM);
    bgfx::setTransform(transformCM);

    // Bind buffers + state
    bgfx::setVertexBuffer(0, bgfx::VertexBufferHandle{gpu->vertexBuffer});
    bgfx::setIndexBuffer(bgfx::IndexBufferHandle{gpu->indexBuffer});
    bgfx::setState(BGFX_STATE_DEFAULT);

    // Material uniforms (base colour + light + shadow)
    SetMaterialUniforms(mat);

    bgfx::submit(mViewId, bgfx::ProgramHandle{mat->program->GetProgramHandle()});
}
```

`SkinnedMeshRenderer::DrawCommand` is identical except for the inverse skip-condition and an extra `bgfx::setUniform(mPaletteUniform, palette.matrices.Data(), 256)` before submit.

## Files Introduced / Modified

| File | Change |
|------|--------|
| `Dia/DiaBgfx3D/Canvas3D.{h,cpp}` | NEW |
| `Dia/DiaBgfx3D/Renderers/{Mesh,SkinnedMesh,Shadow}Renderer.{h,cpp}` | NEW |
| `Dia/DiaBgfx3D/Resources/{MeshGpuCache,MaterialRegistry}.{h,cpp}` | NEW |
| `Dia/DiaBgfx3D/Shaders/3d/` (six .sc + varying.def.sc) | NEW |
| `Dia/DiaBgfx3D/DiaBgfx3D.vcxproj{,.filters}` | NEW |
| `Dia/DiaBgfx3D/dia.bgfx3d.architecture.module.md` | NEW |
| `Dia/DiaBgfx/Canvas.{h,cpp}` | **No change** — Canvas3D extends Canvas without modifying it |
| `Cluiche/Cluiche.sln` | Register DiaBgfx3D.vcxproj |
| `Cluiche/CluicheTest/...` | Default material + sample skinned scene using Canvas3D |

## Dependencies

| Dependency | Type | Notes |
|------------|------|-------|
| All Phase 2 producer features | Hard | This feature is the consumer |
| **DiaMaths `matrix44`** | Hard | `Matrix44::GetColumnMajor` for upload |
| **DiaGeometry3D `intersection-tests`** | Soft | Culling already done in scene-graph |
| `bgfx-shader-cook` (Approved) | Hard | Six new `.sc` files cooked into `Cluiche/out/<App>/shaders/<backend>/3d/` |
| Phase 1 features | Hard | Phase 1 must ship first per RB-003 |

## Acceptance Criteria

1. `Dia::Bgfx::MeshRenderer` consumes static `Mesh3DDrawCommand`s (skinningPaletteIndex == 0); produces correct bgfx draw calls
2. `Dia::Bgfx::SkinnedMeshRenderer` consumes skinned commands; binds the SkinningManager palette as `u_skinningPalette` uniform before each draw
3. `Dia::Bgfx::MaterialRegistry` exposes `Register`/`Resolve`/`GetDefault`; Resolve returns nullptr for unknown ids; default material has lambert + ambient
4. `Dia::Bgfx::MeshGpuCache::GetOrUpload` lazily uploads vertex/index buffers when a Mesh3DAsset becomes Ready; returns nullptr if asset not Ready (caller skips draw)
5. Six `.sc` shader files (`vs_mesh`, `vs_skinned_mesh`, `fs_mesh`, `vs_shadow_caster`, `fs_shadow_caster`, plus `varying.def.sc`) exist under `Dia/DiaBgfx/Shaders/3d/`
6. `dia pipeline --target cluichetest --stage compile-code` cooks the six 3D shaders into `Cluiche/out/cluichetest/shaders/<backend>/3d/`
7. Shadow pass renders meshes from directional light POV into a depth render target reserved on `mShadowViewId`
8. Main pass binds the shadow map as a sampler in `fs_mesh`; lit fragments compare against shadow depth
9. CluicheTest's sample 3D scene loads a glTF skinned character (test fixture or cube) and renders it animated; visible in `dia run cluichetest --3d-demo`
10. `dia run googletest` is green; new TestMeshRenderer/TestSkinnedMeshRenderer/TestMaterialRegistry suites run on bgfx Noop renderer
11. PD-004 audit: no STL containers in any DiaBgfx public header (existing requirement; verified after additions)
12. `bgfx::TextureHandle`, `bgfx::ProgramHandle`, etc. do not appear in any DiaGraphics-or-higher public header (RB-006)
13. Performance bar: 64 visible animated characters at 60 FPS on D3D11 (median frame time ≤ 16ms)
14. `dia.bgfx.architecture.module.md` updated with new entry points and DiaMesh3D/Rig3D/Animation3D/Skinning3D/Scene3D dep edges; validates via `python Tools/dia_modules.py --validate`
15. ImGui debug overlay (DiaVisualDebuggerConsole) continues to work alongside 3D rendering — confirmed by enabling layer toggles while 3D scene renders
16. **Phase 2 ship gate (RB-002):** light 3D renders end-to-end. System spec status flips from "In Progress (Phase 1 done)" to "Done"

## Traceability

| Level | Spec | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System (primary) | DiaBgfx3D | @docs/specs/systems/dia/diabgfx3d.md |
| System (cross-cutting) | RenderBackend | @docs/specs/systems/dia/render-backend.md |

## Binding Decisions Compliance

| ID | Source | Decision | Compliance |
|----|--------|----------|------------|
| PD-001 | Platform | StringCRC for IDs | Compliant — meshId, materialId, palette index lookup |
| PD-004 | Platform | No STL in public APIs | Compliant — DynamicArrayC, raw structs |
| PD-005 | Platform | x64 only | Compliant |
| PD-006 | Platform | VS project files | Compliant |
| PD-007 | Platform | C++20 | Compliant |
| PD-008 | Platform | Directory.Build.props | Compliant |
| PD-009 | Platform | Generated output under Cluiche/out | Compliant — shaders cook to Cluiche/out/<App>/shaders/<backend>/3d/ |
| AD-001 | Dia App | Module YAML | Compliant — dia.bgfx.architecture.module.md updated |
| AD-003 | Dia App | Namespace Dia::<Module>:: | Compliant — `Dia::Bgfx::` |
| AD-005 | Dia App | Component-based entities | Compliant — consumes IComponent-based scene |
| **DiaMaths SD-005** | DiaMaths | Row-major matrix | Compliant — Matrix44 row-major; GetColumnMajor used at upload |
| **DiaMaths SD-008** | DiaMaths | Matrix34 affine for skinning | Compliant — palette is Matrix34 |
| RB-001 | RenderBackend | bgfx adoption | Compliant |
| RB-002 | RenderBackend | Two-phase delivery | **Compliant — this feature closes Phase 2** |
| RB-003 | RenderBackend | Phase 2 specs Approved, not in progress | Compliant |
| RB-004 | RenderBackend | Canvas implements ICanvas only | Compliant — extends Canvas without changing its interface contract |
| RB-005 | RenderBackend | No visitor pattern | Compliant — direct sub-renderers |
| RB-006 | RenderBackend | No backend types in DiaGraphics public surface | Compliant |
| RB-007 | RenderBackend | StringCRC keying | Compliant |
| RB-013 | RenderBackend | Module decomposition mirrors 2D family | Compliant |
| RB-014 | RenderBackend | DiaScene3D introduced | Compliant — this feature consumes DiaScene3D output |
| RB-015 | RenderBackend | glTF runtime parse | Compliant transitively via DiaMesh3D |

## AI Review Questions

| # | Question | Answer |
|---|----------|--------|
| 1 | Why a separate ShadowRenderer rather than folding into MeshRenderer? | Shadow pass is a *different view* (depth-only camera at directional light POV) over the same mesh data. Rendering the same data twice with different shaders is the classic shadow-map idiom. Separate class makes that explicit and clean to enable/disable. |
| 2 | Single-cascade shadow map quality | Stylized "light 3D" tolerates noticeable shadow aliasing without it screaming "low budget"? | Yes for the brief; CSM (cascaded shadow maps) is the right next step when budget allows. Single-cascade with a 2048×2048 depth target serves the demo. |
| 3 | MaterialRegistry growth path | Today: program + base colour. Tomorrow: arbitrary uniform values (specular, roughness, etc.). How does that grow? | Add a `MaterialDescriptor::params` `DynamicArrayC<MaterialParam, kMaxParamsPerMaterial>` field with `{StringCRC name, Vec4 value}` entries. Renderer iterates and calls `bgfx::setUniform`. Out of scope today; documented as the growth shape. |
| 4 | MeshGpuCache lifetime | When does a mesh get unloaded from GPU? | When the corresponding Mesh3DAsset is Unloaded by AssetRuntime. The cache observes asset Unload via a callback (TODO: wire up; defer implementation until needed). Phase 2 doesn't unload meshes mid-game. |
| 5 | Renderer thread | Does MeshRenderer run on render thread? | Yes — same as `canvas-parity`'s renderers. bgfx submission is API-thread (= render PU). |
| 6 | u_skinningPalette uniform binding | bgfx uniforms have a max array size (typically 256 mat3x4 = 12KB). Confirm bgfx supports this. | bgfx supports `bgfx::createUniform("u_skinningPalette", UniformType::Mat3x4, 256)` — the standard skeletal-animation upper bound. Verified against bgfx docs. |
| 7 | Linear-space lighting | Are textures sRGB-decoded before lighting math? | Yes — bgfx supports sRGB texture views via `BGFX_TEXTURE_SRGB`. Mesh material's base colour texture (when added) flagged sRGB; lighting math in linear; final output flagged sRGB if framebuffer writes are sRGB-correct. Implementation detail; "no PBR" doesn't mean "skip linear-space lighting." |
| 8 | Frustum culling redundancy | scene-graph already culled. Does MeshRenderer cull again? | No — trust the input. If a draw command reaches MeshRenderer, it's visible. Avoids redundant frustum tests. |
| 9 | Drawing order | Static meshes before skinned, or interleaved? | bgfx sorts by view + state automatically. Splitting static and skinned across two renderer classes preserves the bgfx state grouping (different shader programs cluster naturally). |
| 10 | Shadow caster culling | Does the shadow pass also cull? | Yes, against the *light's* frustum (not the camera's). For Phase 2: skip the optimisation; render every visible-from-camera mesh from the light's view too. Cost: small. Captured as a follow-up. |
| 11 | Performance bar | 64 characters at 60 FPS — realistic? | Conservative. Each character is one skinned draw + one shadow draw + 256-bone uniform upload. 64 × 2 = 128 draws × ~8 KB uniform ≈ 1 MB/frame uniform traffic. Negligible for D3D11. Frame time bound is per-CPU side: 128 draws × ~10 μs = 1.3 ms render submission. Headroom for sim + animation. |
| 12 | DiaBgfx vcxproj dep growth | DiaBgfx now depends on DiaMesh3D/Rig3D/Animation3D/Skinning3D/Scene3D. Cyclic? | None of those depend on DiaBgfx. DiaBgfx is bottom-of-renderer; it consumes the producer modules. Acyclic. Validated by module graph tool. |
| 13 | Shader cook side effects | Six new `.sc` files: does the cook step automatically pick them up? | Yes — `bgfx-shader-cook` discovers `.sc` files recursively under `source_root` (`Dia/DiaBgfx/Shaders/`). New files in `3d/` subfolder are picked up automatically and cooked to `Cluiche/out/<App>/shaders/<backend>/3d/`. |
| 14 | RB-016 ship gate | Phase 1 ship gate (`diasfml-render-removal`). Does this feature have an analogous Phase 2 gate? | Yes — RB-002 implies a Phase 2 ship gate. This feature's acceptance criterion 16 captures it: light 3D renders end-to-end; system status flips to Done. The CluicheTest 3D demo scene is the visible-from-outside proof. |

---
