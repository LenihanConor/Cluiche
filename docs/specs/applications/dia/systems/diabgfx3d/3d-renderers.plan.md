**Spec:** @docs/specs/applications/dia/systems/diabgfx3d/3d-renderers.md
**Status:** In Progress

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | `Renderers/MeshRenderer.h/.cpp` — static draw command dispatch, transform upload, material resolve | Build clean; runtime needs live scene | Done | sonnet | Added Mesh3DAssetHandler* 4th param for asset lookup |
| 2 | `Renderers/SkinnedMeshRenderer.h/.cpp` — palette uniform binding, skinned draw dispatch | GoogleTest | Blocked | sonnet | Needs DiaSkinning3D::SkinningManager |
| 3 | `Renderers/ShadowRenderer.h/.cpp` — depth-only pass, 2048×2048 framebuffer, directional light view | Build clean; runtime needs bgfx init | Done | sonnet | Ortho ±50 units, placeholder program idx 0 |
| 4 | `Shaders/3d/` — six .sc files (vs_mesh, vs_skinned_mesh, fs_mesh, vs_shadow_caster, fs_shadow_caster, varying.def) | Files present; cook step TBD | Done | sonnet | Lambert + ambient + single-tap shadow in fs_mesh |
