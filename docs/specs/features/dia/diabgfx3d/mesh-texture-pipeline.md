# Feature Spec: DiaBgfx3D — Mesh Texture Pipeline

## Parent System
@docs/specs/applications/dia/systems/diabgfx3d/diabgfx3d.md

**Status:** `Done`

---

## Summary

Extend the DiaBgfx3D mesh rendering pipeline to support albedo and normal-map textures per material. Today `MeshRenderer` shades purely from a `baseColourRGBA` uniform plus vertex colours — meshes with glTF texture data (e.g. the Avocado) render near-black. This feature wires the full texture path: sampler slots in the shader, texture handle fields in `MaterialDescriptor`, `TextureHandler` loading via `DiaAssetRuntime`, asset catalogue entries, and a moving directional light in `Mesh3DTestStage` to visually prove normal mapping is working.

---

## Goals

1. Albedo textures sample correctly in `fs_mesh.sc` — Avocado renders with its yellow-green skin texture.
2. Normal maps perturb the shading normal via a TBN transform — surface microdetail is visible under the moving light.
3. Meshes with no texture (e.g. Box) continue to render correctly using the flat-normal default and `baseColourRGBA`.
4. `MaterialDescriptor` carries texture handles without exposing bgfx types in the public header (BG3-005).
5. Textures load via the existing `DiaAssetRuntime::TextureHandler` path — no new loader.
6. All changes are wired through the asset catalogue — no hardcoded file paths in C++ stage code.

---

## Sampler Slot Layout

| Slot | Sampler | Purpose |
|------|---------|---------|
| 0 | `s_albedo` | Albedo / base colour texture |
| 1 | `s_normalMap` | Tangent-space normal map |
| 2 | `s_shadowMap` | Shadow depth map (moved from slot 0) |

The shadow slot move is already in the codebase (`fs_mesh.sc` line 10, `MeshRenderer.cpp` line 123).

---

## Tasks

| # | Task | Notes |
|---|------|-------|
| 1 | Add `albedoTexture` + `normalMapTexture` handle fields to `MaterialDescriptor` | `unsigned short` idx fields — no bgfx types in header (BG3-005). Default to `bgfx::kInvalidHandle` (0xFFFF). |
| 2 | Add `s_albedo` (slot 0) and `s_normalMap` (slot 1) samplers to `fs_mesh.sc`; add tangent inputs + TBN to `vs_mesh.sc` | Flat-normal default (1×1 `(128,128,255,255)` texture) created in `MeshRenderer::InitUniforms()`. |
| 3 | Add `mSAlbedo` + `mSNormalMap` uniform handles to `MeshRenderer`; create + destroy in `InitUniforms()` / destructor | Mirror the existing `mSShadowMap` pattern. |
| 4 | Bind albedo + normal map textures per-draw in `MeshRenderer::DrawCommand()` | Bind flat-normal default when `normalMapTexture` is invalid. Bind a 1×1 white texture when `albedoTexture` is invalid (preserves `baseColourRGBA` behaviour). |
| 5 | Copy Avocado PNG files to `Cluiche/Assets/CluicheTest/Stages/Mesh3DTestStage/World/Textures/` | Source from the textures referenced by `Avocado.gltf`. |
| 6 | Add `texture.avocado_albedo` and `texture.avocado_normal` entries to `assets.catalogue.json`; add `references` edges from the `stage.mesh3d_test_stage` entry | Follow the `texture.test_red` / DummyStage pattern exactly. |
| 7 | Load and register textures in `Mesh3DTestStage`; build a `MaterialDescriptor` with both handles and register it with `MaterialRegistry` | Wait for `TextureHandler` `Ready` state before registering; log a warning and fall back to invalid handle on `Failed`. |
| 8 | Add oscillating directional light to `Mesh3DTestStage` — sweeps left-to-right over ~4 seconds | Driven from Sim PU each frame; writes into `FrameData3D` directional light. |
| 9 | Rebuild and cook shaders; run `dia run cluichetest` and visually verify Avocado texture + normal detail under moving light | Box must still render correctly with no regression to shadow or 2D passes. |

---

## Binding Decisions

| Decision | Implication |
|----------|-------------|
| BG3-005 — No bgfx types in public headers above DiaBgfx3D | `MaterialDescriptor` stores `unsigned short` handle indices, not `bgfx::TextureHandle`. Same pattern as `GpuMesh`. |
| BG3-008 — `MaterialDescriptor` is a flat struct; growth path is a `params` array | Adding two `unsigned short` fields is additive and within the documented growth path. |
| BG3-009 — STL permitted internally, not in public surface | Texture handle fields are plain scalars — no issue. |
| PD-001 — StringCRC for asset IDs | Texture asset IDs (`"texture.avocado_albedo"`) are StringCRC-keyed in `TextureHandler`. |
| PD-004 — No STL in public APIs | No new public APIs introduce STL. |

---

## Open Design Questions

1. **Flat-normal texture ownership**: The 1×1 flat-normal default is created by `MeshRenderer::InitUniforms()` and destroyed in its destructor. If `SkinnedMeshRenderer` later needs the same default, move it up to `Canvas3D` as a shared resource. For now it lives in `MeshRenderer`.

2. **Material ↔ texture lifetime**: `MaterialDescriptor` holds a raw bgfx handle index. If `TextureHandler` unloads a texture while a material still references it, the handle becomes stale. This spec does not address dynamic texture unloading — Avocado textures are stage-scoped and never unloaded mid-run. A texture refcount or weak-handle scheme is future work.

---

## Acceptance Criteria

- Avocado renders with its yellow-green skin texture visible under directional lighting.
- Normal map surface detail is visible and changes as the directional light sweeps side-to-side over ~4 seconds.
- Box (no texture) renders correctly with `baseColourRGBA` shading — no regression.
- Shadow mapping still functions correctly on both meshes.
- No bgfx types appear in `MaterialDescriptor` public fields.
- Textures are loaded via `DiaAssetRuntime::TextureHandler` and referenced through the asset catalogue — no hardcoded file paths in C++.
