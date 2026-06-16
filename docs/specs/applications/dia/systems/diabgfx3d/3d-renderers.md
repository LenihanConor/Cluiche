# Feature Spec: 3d-renderers

**Parent:** [diabgfx3d.md](diabgfx3d.md)

**Hard dependencies:**
- `gpu-resources` (this system) — `MeshGpuCache`, `MaterialRegistry`
- `canvas-parity` (DiaBgfx, Phase 1)
- `graphics-3d-types` — `Dia::Graphics3D::Mesh3DFrameData`, `Camera3D`, lights
- `mesh-asset-and-loader` — `Mesh3DAsset` vertex/index data
- `skinning-palette` — `SkinningManager` produces per-frame `Matrix34` palettes
- `scene-graph` — produces the populated `Mesh3DFrameData`

**Status:** Approved

## Summary

Three sub-renderers for `DiaBgfx3D`: `MeshRenderer` (static meshes), `SkinnedMeshRenderer` (vertex-shader skinning, 256-bone uniform), and `ShadowRenderer` (single-cascade depth pass from directional light POV). Accompanied by six `.sc` shader files cooked by `bgfx-shader-cook`. No bgfx types appear in any public header.

## Goals

- `Dia::Bgfx3D::MeshRenderer` — walks static `Mesh3DDrawCommand`s (skinningPaletteIndex == 0); resolves material from `MaterialRegistry`; uploads transform via `bgfx::setTransform`; submits indexed draw
- `Dia::Bgfx3D::SkinnedMeshRenderer` — same shape but binds `SkinningManager` palette as `u_skinningPalette mat3x4[256]` before each draw
- `Dia::Bgfx3D::ShadowRenderer` — depth-only pass from directional light POV into a 2048×2048 depth render target; single cascade
- Six `.sc` shader files under `Dia/DiaBgfx3D/Shaders/3d/`: `vs_mesh.sc`, `vs_skinned_mesh.sc`, `fs_mesh.sc` (shared), `vs_shadow_caster.sc`, `fs_shadow_caster.sc`, `varying.def.sc`
- Lambert + ambient + single-tap shadow lookup in `fs_mesh.sc`; no PBR

## Binding Decisions

- **RB-001** — bgfx is the GPU abstraction; no secondary layer
- **RB-005** — No visitor pattern; renderers consume `Mesh3DFrameData` directly
- **BG3-004** — No visitor pattern in 3D render path
- **BG3-005** — No bgfx types in public headers; `.h` files include only engine types; bgfx headers in `.cpp` only
- **BG3-006** — Vertex-shader skinning with `u_skinningPalette mat3x4[256]`; compute skinning deferred
- **BG3-007** — Single-cascade shadow map, 2048×2048 depth target
- **BG3-011** — Namespace `Dia::Bgfx3D::`
- **DiaMaths SD-005** — Row-major matrix storage; `Matrix44::GetColumnMajor` called at upload
- **DiaMaths SD-008** — `Matrix34` is the affine skinning type; palette uploaded as `mat3x4[256]`

## Public Interfaces

```cpp
// Dia/DiaBgfx3D/Renderers/MeshRenderer.h
namespace Dia { namespace Bgfx3D {

class MeshGpuCache;
class MaterialRegistry;

class MeshRenderer
{
public:
    MeshRenderer(unsigned short viewId, MeshGpuCache* cache, MaterialRegistry* materials);

    // Draws all static draw commands (skinningPaletteIndex == kStaticPaletteIndex).
    void Draw(const Dia::Graphics3D::Mesh3DFrameData& frameData);

private:
    void           DrawCommand(const Dia::Graphics3D::Mesh3DDrawCommand& cmd);
    unsigned short mViewId;
    MeshGpuCache*  mCache;
    MaterialRegistry* mMaterials;
};

} }
```

```cpp
// Dia/DiaBgfx3D/Renderers/SkinnedMeshRenderer.h
namespace Dia { namespace Bgfx3D {

class SkinnedMeshRenderer
{
public:
    SkinnedMeshRenderer(unsigned short viewId, MeshGpuCache* cache, MaterialRegistry* materials);

    // Draws all skinned draw commands; binds SkinningManager palette before each draw.
    void Draw(const Dia::Graphics3D::Mesh3DFrameData& frameData);

private:
    unsigned short    mViewId;
    MeshGpuCache*     mCache;
    MaterialRegistry* mMaterials;
    unsigned short    mPaletteUniform;  // bgfx::UniformHandle::idx — u_skinningPalette mat3x4[256]
};

} }
```

```cpp
// Dia/DiaBgfx3D/Renderers/ShadowRenderer.h
namespace Dia { namespace Bgfx3D {

class ShadowRenderer
{
public:
    ShadowRenderer(unsigned short shadowViewId, MeshGpuCache* cache);

    // Renders all opaque meshes from the directional light's POV into mShadowFramebuffer.
    void RenderShadowMap(const Dia::Graphics3D::Mesh3DFrameData& frameData);

    unsigned short GetShadowFramebuffer() const;  // bound as sampler in main pass

private:
    unsigned short mViewId;
    unsigned short mShadowFramebuffer;  // bgfx::FrameBufferHandle::idx
    MeshGpuCache*  mCache;
};

} }
```

### Shader files

| File | Purpose |
|------|---------|
| `Dia/DiaBgfx3D/Shaders/3d/vs_mesh.sc` | Static mesh vertex shader |
| `Dia/DiaBgfx3D/Shaders/3d/vs_skinned_mesh.sc` | Skinned mesh VS — 256-bone mat3x4 palette |
| `Dia/DiaBgfx3D/Shaders/3d/fs_mesh.sc` | Shared fragment shader — lambert + ambient + single-tap shadow |
| `Dia/DiaBgfx3D/Shaders/3d/vs_shadow_caster.sc` | Shadow-caster VS |
| `Dia/DiaBgfx3D/Shaders/3d/fs_shadow_caster.sc` | Depth-only fragment shader |
| `Dia/DiaBgfx3D/Shaders/3d/varying.def.sc` | Shared varying definitions |

`bgfx-shader-cook` discovers `.sc` files recursively under `Dia/DiaBgfx3D/Shaders/` and cooks them to `Cluiche/out/<App>/shaders/<backend>/3d/` automatically.

## Acceptance Criteria

1. `MeshRenderer::Draw` submits a bgfx draw call for each static `Mesh3DDrawCommand`; skips commands where the asset is not Ready
2. `SkinnedMeshRenderer::Draw` binds `u_skinningPalette mat3x4[256]` from `SkinningManager` before each skinned draw
3. `ShadowRenderer::RenderShadowMap` renders all visible meshes from the directional light's view into a 2048×2048 depth render target on `mShadowViewId`
4. No bgfx types (`bgfx::VertexBufferHandle`, `bgfx::ProgramHandle`, etc.) appear in any of the three renderer `.h` files
5. Six `.sc` files exist under `Dia/DiaBgfx3D/Shaders/3d/`
6. `dia pipeline --target cluichetest --stage compile-code` cooks the six 3D shaders to `Cluiche/out/cluichetest/shaders/<backend>/3d/` without error
7. `fs_mesh.sc` implements lambert + ambient + directional light + single-tap shadow map sample; `u_directionalLightDir`, `u_directionalLightColour`, `u_ambient`, `u_baseColour`, and `s_shadowMap` uniforms are present
8. `vs_skinned_mesh.sc` blends 4 bone weights from `u_skinningPalette[256]` before projecting position; `a_indices` and `a_weight` attributes are declared
9. Shadow caster culling phase 2: all visible-from-camera meshes rendered from light POV (no light-frustum cull; deferred optimisation)
10. `dia run googletest --filter="DiaBgfx3D_MeshRenderer*"` and `--filter="DiaBgfx3D_SkinnedMeshRenderer*"` pass on Noop bgfx renderer

## Tasks

| # | Task | Status |
|---|------|--------|
| 1 | `Renderers/MeshRenderer.h/.cpp` — static draw command dispatch | Todo |
| 2 | `Renderers/SkinnedMeshRenderer.h/.cpp` — skinning palette uniform binding | Todo |
| 3 | `Renderers/ShadowRenderer.h/.cpp` — depth-only pass, shadow framebuffer creation | Todo |
| 4 | `Shaders/3d/` — six `.sc` files; verify cook step picks them up | Todo |
