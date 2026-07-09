**Spec:** @docs/specs/features/dia/diabgfx3d/mesh-texture-pipeline.md
**Status:** Done

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Add `albedoTexture` + `normalMapTexture` fields to `MaterialDescriptor` | Build passes, no regressions | Done | haiku | Both fields `unsigned short` 0xFFFFu default; Canvas3D + MaterialRegistry.cpp updated |
| 2 | Add `s_albedo`/`s_normalMap` samplers to shaders; TBN in `vs_mesh.sc`; flat-normal 1×1 texture in `MeshRenderer::InitUniforms()` | Build passes | Done | sonnet | TBN via a_tangent.w sign; slots 0/1/2; mFlatNormalTexture (128,128,255,255) |
| 3 | Add `mSAlbedo` + `mSNormalMap` uniform handles to `MeshRenderer` | Build passes | Done | haiku | Mirrors mSShadowMap pattern; both init to kInvalidHandle |
| 4 | Bind albedo + normal map per-draw in `MeshRenderer::DrawCommand()` | Build passes | Done | sonnet | mWhiteTexture added; bindings inside submesh loop at slots 0/1 |
| 5 | Copy Avocado PNG files to `Stages/Mesh3DTestStage/World/Textures/` | Files present | Done | haiku | Downloaded from KhronosGroup/glTF-Sample-Assets (CC0) |
| 6 | Add `texture.avocado_albedo` + `texture.avocado_normal` catalogue entries + stage references | `dia validate manifest` passes | Done | haiku | Both entries stage-scoped; stage.mesh3d_test_stage references updated |
| 7 | Load textures in `Mesh3DTestStage`; build + register `MaterialDescriptor` | `dia run cluichetest` launches | Done | sonnet | Implemented: NullTextureCallback polling pattern, avocado_material registered once all 3 textures settle; sweeping directional light wired in OnUpdate |
| 8 | ~~Add oscillating directional light to `Mesh3DTestStage`~~ | — | Dropped | haiku | Out of scope |
| 9 | Cook shaders + full visual verify | Avocado textured + normal detail visible | Done | sonnet | `dia pipeline --target cluichetest` passed; `dia run cluichetest` PASSED exit 0; no texture load warnings |
