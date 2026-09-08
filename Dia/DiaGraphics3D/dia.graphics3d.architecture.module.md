---
schema: dia.module.v1
module_id: dia.graphics3d
name: DiaGraphics3D
owner_team: TBD
layer: domain/visual/core
status: active
maturity: dev

path: Dia/DiaGraphics3D
language: cpp
parent_module_id: dia.root

summary: >
  3D rendering type seam — defines the per-frame types shared between 3D simulation
  modules (DiaScene3D, DiaSkinning3D) and the 3D renderer (DiaBgfx3D).

intent: >
  Provide Camera3D, DirectionalLight, PointLight, Mesh3DDrawCommand, Mesh3DFrameData,
  and FrameData3D under Dia::Graphics3D:: so that 2D-only games never transitively
  pull in DiaMaths::Matrix44 or any 3D type.

responsibilities:
  - Define Mesh3DFrameData (container for 3D draw commands, camera, lights)
  - Define Camera3D (view + projection Matrix44 + helper setters)
  - Define DirectionalLight and PointLight
  - Define Mesh3DDrawCommand (meshId, materialId, Matrix44 transform, skinningPaletteIndex, layer)
  - Define FrameData3D : FrameData + Mesh3DFrameData (full 2D+3D frame packet)
  - Provide MockMesh3DFrameData test stub in Testing/

non_responsibilities:
  - Maths primitives (Matrix44, Quaternion, Transform3D) — owned by DiaMaths
  - Geometry primitives (Frustum, AABB) — owned by DiaGeometry3D
  - Material types or shader descriptors — owned by DiaBgfx3D
  - Mesh or skeleton data structures — owned by DiaMesh3D and DiaRig3D
  - Platform-specific rendering — DiaBgfx3D

dependent_modules: []

public_api:
  headers:
    - DiaGraphics3D/Camera3D.h
    - DiaGraphics3D/Light.h
    - DiaGraphics3D/Mesh3DDrawCommand.h
    - DiaGraphics3D/Mesh3DFrameData.h
    - DiaGraphics3D/FrameData3D.h
    - DiaGraphics3D/Testing/MockMesh3DFrameData.h
  namespaces:
    - Dia::Graphics3D
    - Dia::Graphics3D::Testing
  entry_points:
    - Dia::Graphics3D::Camera3D
    - Dia::Graphics3D::DirectionalLight
    - Dia::Graphics3D::PointLight
    - Dia::Graphics3D::Mesh3DDrawCommand
    - Dia::Graphics3D::Mesh3DFrameData
    - Dia::Graphics3D::FrameData3D

dependencies:
  required:
    - dia.graphics
    - dia.maths.matrix
    - dia.maths.vector
    - dia.core
    - dia.observation
  forbidden: []
---
