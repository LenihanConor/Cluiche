# Feature Spec: static-mesh-demo

**Parent:** [diabgfx3d.md](diabgfx3d.md)

**Hard dependencies:**
- `gpu-resources` (Done) — `MaterialRegistry`, `MeshGpuCache`
- `3d-renderers` (Done except SkinnedMeshRenderer) — `MeshRenderer`, `ShadowRenderer`
- `canvas3d` (Approved, Tasks 1–3 Done) — `Canvas3D` exists
- `mesh-asset-and-loader` (Done) — `Mesh3DAssetHandler` loads `.mesh3d` files

**Status:** Done

## Summary

End-to-end static mesh rendering: fill the 3 remaining gaps so that `dia run cluichetest` can show a lit, shadowed 3D cube on screen. Skinning and rigging are out of scope. This proves the DiaBgfx3D pipeline works for static geometry with materials.

## Goals

1. **Shader cook discovers DiaBgfx3D shaders** — `pipeline.toml` gains a second source root (or the cook supports multiple roots) so `Dia/DiaBgfx3D/Shaders/3d/*.sc` files are compiled to per-backend `.bin` at pipeline time
2. **3D shader programs are loaded at runtime** — `Canvas3D::DeferredInit()` (or an Init3D helper) loads `vs_mesh + fs_mesh` and `vs_shadow_caster + fs_shadow_caster` and registers a default material in `MaterialRegistry`
3. **CluicheTest 3D test stage** — a new `Mesh3DTestStage` module drives `Canvas3D` with a camera, directional light, and a procedural unit cube; visually verified as a lit, shadowed box

## Binding Decisions

- **BG3-005** — No bgfx types in public DiaBgfx3D headers (reinforced)
- **BG3-009** — STL OK internally via pimpl (shader load paths use std::string internally)
- **BG3-011** — Namespace `Dia::Bgfx3D::`
- **PD-004** — No STL in public APIs

## Acceptance Criteria

1. `dia pipeline --target cluichetest` cooks `vs_mesh.sc`, `fs_mesh.sc`, `vs_shadow_caster.sc`, `fs_shadow_caster.sc` for all backends (dx11, dx12, vulkan) — output appears in `Cluiche/bin/CluicheTest/.../assets/shaders/3d/`
2. `Canvas3D` loads the compiled mesh and shadow-caster programs during deferred init; `MaterialRegistry::GetDefault().program` is valid
3. A `Mesh3DTestStage` module renders a unit cube with a directional light; the shadow pass fires (even if shadow not visually verified yet)
4. `dia run googletest` remains fully green
5. No new singletons introduced

## Tasks

| # | Task | Status |
|---|------|--------|
| 1 | Extend shader cook to support multiple source roots; update `pipeline.toml` | Done |
| 2 | `Canvas3D` shader program loading + default material registration | Done |
| 3 | Procedural unit cube `Mesh3DAsset` helper (generates VB/IB in memory) | Done |
| 4 | `Mesh3DTestStage` module + stage scaffold (camera, light, cube draw) | Done |
| 5 | Verify: `dia pipeline --target cluichetest` + `dia run cluichetest` shows cube | Done |
