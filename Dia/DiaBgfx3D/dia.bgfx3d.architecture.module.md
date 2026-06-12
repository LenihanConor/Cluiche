---
schema: dia.module.v1
module_id: dia.bgfx3d
name: DiaBgfx3D
owner_team: TBD
layer: domain/visual/core
status: active
maturity: dev

path: Dia/DiaBgfx3D
language: cpp
parent_module_id: dia.bgfx

summary: >
  Phase 2 3D rendering layer extending DiaBgfx::Canvas with Canvas3D, MaterialRegistry,
  and (when upstream modules ship) MeshRenderer, SkinnedMeshRenderer, ShadowRenderer,
  and MeshGpuCache. Canvas3D dispatches 3D passes before the inherited 2D passes;
  2D rendering (sprites, debug, UI, ImGui) works without modification from day one.

intent: >
  Add 3D mesh rendering on top of the Phase 1 bgfx canvas without modifying DiaBgfx.
  2D-only games link DiaBgfx only; games needing 3D link DiaBgfx3D which transitively
  pulls in the 3D producer chain (DiaMesh3D, DiaRig3D, DiaAnimation3D, DiaSkinning3D,
  DiaScene3D) as those modules are implemented.

responsibilities:
  - Provide Dia::Bgfx3D::Canvas3D extending Dia::Bgfx::Canvas
  - Dispatch 3D sub-renderer passes before inherited 2D passes in ProcessFrame(FrameData3D)
  - Provide Dia::Bgfx3D::MaterialRegistry — StringCRC to ShaderProgram + base colour mapping
  - (Pending) MeshRenderer, SkinnedMeshRenderer, ShadowRenderer — added when DiaMesh3D/DiaSkinning3D ship
  - (Pending) MeshGpuCache — lazy vertex/index upload when DiaMesh3D ships

non_responsibilities:
  - 2D rendering — owned by DiaBgfx (inherited unchanged)
  - Window creation or input handling — owned by DiaSDL
  - Asset discovery or loading — owned by DiaAsset/DiaAssetRuntime
  - PBR shading, IBL, post-processing — out of scope (RB-001)

public_api:
  headers:
    - Dia/DiaBgfx3D/Canvas3D.h
    - Dia/DiaBgfx3D/Resources/MaterialRegistry.h
    - Dia/DiaBgfx3D/Resources/MeshGpuCache.h
  namespaces:
    - Dia::Bgfx3D
  entry_points:
    - Canvas3D
    - MaterialRegistry
    - MaterialDescriptor
    - MeshGpuCache
    - GpuMesh

dependencies:
  required:
    - dia.bgfx
    - dia.graphics.frame
    - dia.graphics.interface
    - dia.graphics3d.frame
    - dia.core.core
    - dia.core.crc
    - dia.maths.vector
    - dia.mesh3d
    - dia.observation
  forbidden: []
---
