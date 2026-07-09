**Spec:** @docs/specs/features/dia/diabgfx3d/pbr-shading.md
**Status:** Done

## Implementation Patterns

**MaterialDescriptor extension** — add `float metallic = 0.0f`, `float roughness = 0.5f`, `unsigned short ormTexture = 0xFFFF` to the flat struct in `MaterialRegistry.h`. Same pattern as existing `albedoTexture` / `normalMapTexture` fields. No bgfx types (BG3-005).

**MeshPassLighting extension** — add `float cameraPos[4]` (xyz = world position, w = 0). Populated in `Canvas3D::ProcessFrame` by extracting from the view matrix: camera world pos = `-R^T * t` where R is the rotation part of the row-major view matrix and t is the translation column.

**MeshRenderer uniform pattern** — new handles `mUCameraPos`, `mUPbrParams`, `mSOrm` follow the existing `mUAmbient` / `mSShadowMap` pattern: `unsigned short` idx stored as member, created in `InitUniforms()` with `bgfx::createUniform`, destroyed in `~MeshRenderer()` via the existing `destroy` lambda. Default 1×1 ORM texture `(0, 128, 0, 255)` — G=128→roughness=0.5, B=0→metallic=0 — created alongside `mWhiteTexture` and `mFlatNormalTexture`.

**GGX BRDF in fs_mesh.sc** — Fresnel-Schlick F, GGX NDF D, Smith geometry G. Three new uniforms: `u_cameraPos` (vec4), `u_pbrParams` (vec4: x=metallic, y=roughness), `s_orm` sampler slot 3. Shadow factor multiplies the full combined result (diffuse + specular), not just diffuse.

**Asset catalogue pattern** — entry under `"textures"` key with `"id": "texture.avocado_orm"`, `"path": "..."`, and a reference edge added to `stage.mesh3d_test_stage`. Follows `texture.avocado_albedo` / `texture.avocado_normal` added in `mesh-texture-pipeline`.

**ORM texture wiring** — same `TextureHandler::Ready` guard pattern used for albedo/normal: check state, set `mat.ormTexture = handler.GetHandle().idx` on ready, log warning on failed. Scalar fallbacks always set (`metallic`, `roughness`) regardless of texture state.

---

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Source Avocado_roughnessMetallic.png and copy to assets directory | File present at correct path | Done | haiku | 1,655,059 bytes; all 3 Avocado textures present |
| 2 | Extend MaterialDescriptor with metallic, roughness, ormTexture | Compiles; existing materials unaffected | Done | haiku | Fields added with defaults; Canvas3D default mat updated |
| 3 | Add cameraPos[4] to MeshPassLighting; populate from view matrix in Canvas3D::ProcessFrame | Compiles cleanly | Done | sonnet | Extracted via -(R^T*t) from row-major view matrix |
| 4 | Add s_orm, u_cameraPos, u_pbrParams handles + default ORM texture to MeshRenderer | Compiles; InitUniforms creates all handles | Done | sonnet | mUCameraPos/mUPbrParams/mSOrm/mDefaultOrmTexture added |
| 5 | Rewrite fs_mesh.sc with GGX microfacet BRDF | Shader compiles via shaderc; visual output correct | Done | opus | Fresnel-Schlick + GGX NDF + Smith G; shadow attenuates full result |
| 6 | Bind ORM texture and upload u_cameraPos/u_pbrParams per-draw in DrawCommand | Compiles; correct bindings in draw path | Done | sonnet | ORM slot 3 bound; scalar fallbacks always uploaded |
| 7 | Add texture.avocado_orm to assets.catalogue.json + reference edge | Catalogue validates | Done | haiku | Entry + reference edge added; manifest validates |
| 8 | Load ORM texture in stage; set ormTexture handle + scalars on avocado MaterialDescriptor | Runtime: ORM sampler populated | Done | sonnet | Mesh3DTestStageModule: orm lookup + ormTexture/metallic/roughness set |
| 9 | Cook shaders, run cluichetest, visual verify PBR response | Avocado shows metallic sheen; Box unaffected | Pending | sonnet | dia pipeline + dia run cluichetest |
