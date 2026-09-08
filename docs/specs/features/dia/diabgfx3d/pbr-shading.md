# Feature Spec: DiaBgfx3D — PBR Shading

## Parent System
@docs/specs/applications/dia/systems/diabgfx3d/diabgfx3d.md

**Status:** `Done`

---

## Summary

Replace the current Lambert + flat-ambient shading model in `fs_mesh.sc` with a GGX microfacet BRDF so that metallic-roughness data authored into glTF materials (ORM texture: G = roughness, B = metallic) is actually consumed at runtime. The Avocado asset already references `Avocado_roughnessMetallic.png` in its `.gltf` — that texture is just missing from the assets directory and has never been sampled. This feature completes the lighting model: Fresnel reflection, GGX specular highlight, and roughness-driven diffuse all respond correctly to the existing oscillating directional light from `mesh-texture-pipeline`.

---

## Goals

1. `fs_mesh.sc` implements a GGX microfacet BRDF (Fresnel-Schlick, GGX NDF, Smith geometry term).
2. ORM texture slot added to `MeshRenderer` — G channel drives roughness, B channel drives metallic.
3. `MaterialDescriptor` carries fallback scalar `metallic` and `roughness` values used when no ORM texture is bound.
4. Camera world position flows from `FrameData3D` camera through `MeshPassLighting` into the shader as `u_cameraPos`.
5. `Avocado_roughnessMetallic.png` sourced from the Khronos glTF sample set and added to the assets directory.
6. Avocado renders visibly differently from a default-material box (metal sheen vs. matte) under the same directional light.
7. Box (no ORM texture) renders correctly using scalar fallback values — no regression to existing Lambert appearance.

---

## Sampler Slot Layout

Extends the layout established in `mesh-texture-pipeline`:

| Slot | Sampler | Purpose |
|------|---------|---------|
| 0 | `s_albedo` | Base colour / albedo texture |
| 1 | `s_normalMap` | Tangent-space normal map |
| 2 | `s_shadowMap` | Shadow depth map |
| 3 | `s_orm` | ORM texture (R = occlusion, G = roughness, B = metallic) |

---

## New Uniforms

| Uniform | Type | Purpose |
|---------|------|---------|
| `u_cameraPos` | vec4 | xyz = camera world position, w = 0 |
| `u_pbrParams` | vec4 | x = metallic fallback, y = roughness fallback, zw = unused |

---

## Tasks

| # | Task | Notes |
|---|------|-------|
| 1 | Source `Avocado_roughnessMetallic.png` from the Khronos glTF-Sample-Assets repository and copy to `Cluiche/Assets/CluicheTest/Stages/Mesh3DTestStage/World/Textures/` | File is already referenced by `Avocado.gltf` index 1; just missing on disk. |
| 2 | Extend `MaterialDescriptor` with `float metallic`, `float roughness`, `unsigned short ormTexture` | `ormTexture` default = `0xFFFF` (invalid). Scalar defaults: `metallic = 0.0f`, `roughness = 0.5f`. No bgfx types in header (BG3-005). |
| 3 | Add `cameraPos[4]` to `MeshPassLighting`; populate in `Canvas3D::ProcessFrame` from `FrameData3D` camera | Extract camera world position from the view matrix inverse (or store directly on `Camera3D` if available). |
| 4 | Add `s_orm` sampler (slot 3), `u_cameraPos`, and `u_pbrParams` uniform handles to `MeshRenderer`; create/destroy in `InitUniforms()` / destructor | Mirror existing `mSShadowMap` / `mUAmbient` patterns. Create a 1×1 default ORM texture `(0, 128, 0, 255)` — roughness=0.5, metallic=0 — for unbound slots. |
| 5 | Rewrite `fs_mesh.sc` to implement GGX microfacet BRDF | Fresnel-Schlick `F`, GGX NDF `D`, Smith `G`. Diffuse term: Lambertian `(1 - F) * (1 - metallic) * albedo / PI`. Specular: `D * F * G / (4 * NdotL * NdotV)`. PCF shadow attenuation multiplies the full combined result (not just diffuse). |
| 6 | Bind ORM texture and upload `u_cameraPos` / `u_pbrParams` per-draw in `MeshRenderer::DrawCommand()` | Bind 1×1 default when `ormTexture` is invalid; upload scalar fallbacks via `u_pbrParams` in both cases. |
| 7 | Add `texture.avocado_orm` entry to `assets.catalogue.json`; add reference edge from `stage.mesh3d_test_stage` | Follow the `texture.avocado_albedo` / `texture.avocado_normal` pattern established in `mesh-texture-pipeline`. |
| 8 | Load ORM texture in `RenderModule` (or `Mesh3DTestStage`); set `ormTexture` handle + scalar params on the avocado `MaterialDescriptor` | Wait for `TextureHandler` `Ready` state before setting handle; log warning and leave `0xFFFF` on `Failed`. |
| 9 | Cook shaders (`dia pipeline --target cluichetest`), run `dia run cluichetest`, visually verify Avocado shows PBR response — metallic sheen and roughness-driven highlight shape change as the light sweeps | Box must still render correctly with no regression to shadow or 2D passes. |

---

## Binding Decisions

| Decision | Implication |
|----------|-------------|
| BG3-005 — No bgfx types in public headers above DiaBgfx3D | `MaterialDescriptor` stores `unsigned short ormTexture` (handle index), not `bgfx::TextureHandle`. Same pattern as `albedoTexture` / `normalMapTexture` from `mesh-texture-pipeline`. |
| BG3-008 — `MaterialDescriptor` is a flat struct; growth path is a `params` array | Adding `float metallic`, `float roughness`, `unsigned short ormTexture` is additive and within the documented growth path. |
| PD-001 — StringCRC for asset IDs | `"texture.avocado_orm"` keyed by StringCRC in `TextureHandler`. |
| PD-004 — No STL in public APIs | No new public APIs introduce STL. |
| RB-001 — bgfx is the GPU abstraction | BRDF is implemented entirely in `.sc` GLSL; no secondary shading abstraction layer. |

---

## Open Design Questions

1. **sRGB vs. linear texture sampling**: glTF specifies that `baseColor` is sRGB-encoded but ORM is linear. If `TextureHandler` loads all textures with the same flags, albedo may be double-gamma-corrected and ORM may be incorrectly treated as sRGB. Does the current `TextureHandler` set `BGFX_TEXTURE_SRGB` on albedo loads? If not, colours will be slightly off but visually acceptable for this stage — flag as a follow-up rather than a blocker.

2. **Camera world position extraction**: `MeshPassLighting.cameraPos` needs the camera's world-space position. `Camera3D` currently exposes view + projection matrices but may not store position directly. The cleanest path is to add a `worldPosition` field to `Camera3D` in `DiaGraphics3D` — but that touches a module outside DiaBgfx3D. Alternative: extract from the view matrix inverse (`col[3]` of `view^-1`). Confirm which is preferred before task 3.

3. **Shadow attenuation scope**: In the Lambert path, PCF shadow only attenuates diffuse. Specular highlights appearing in shadowed areas is physically wrong and visually jarring — attenuating the full BRDF result (diffuse + specular) by the shadow factor is correct. This is the intended approach (task 5 notes), but confirm it's acceptable before the shader rewrite since it slightly darkens shadowed areas compared to the current look.

---

## Acceptance Criteria

- Avocado renders with a visible metallic sheen and roughness-driven specular highlight shape under the oscillating directional light.
- Avocado and Box look visually distinct under identical lighting — metallic vs. matte surfaces are immediately apparent.
- Box (no ORM texture, no texture registration) renders correctly using scalar fallback values (`metallic=0`, `roughness=0.5`) — no visual regression.
- Shadow mapping still attenuates lighting correctly on both meshes.
- No bgfx types appear in `MaterialDescriptor` public fields.
- ORM texture loaded via `DiaAssetRuntime::TextureHandler` and referenced through `assets.catalogue.json` — no hardcoded file paths in C++.
