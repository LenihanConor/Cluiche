---
schema: dia.module.v1
module_id: dia.mesh3d
name: DiaMesh3D
layer: foundation/assets
path: Dia/DiaMesh3D

dependent_modules:
  - dia.core
  - dia.maths
  - dia.geometry3d
  - dia.assetruntime

public_api:
  entry_points:
    - Dia::Mesh3D::Vertex3D
    - Dia::Mesh3D::Submesh
    - Dia::Mesh3D::Mesh3DAsset
    - Dia::Mesh3D::Mesh3DAssetHandler

responsibilities:
  - Own Dia::Mesh3D::Vertex3D — canonical static mesh vertex layout (position, normal, tangent, uv0, colour; 52 bytes)
  - Own Dia::Mesh3D::Submesh — index range + material id (StringCRC) within a mesh
  - Own Dia::Mesh3D::Mesh3DAsset — async-loadable mesh asset (vertex/index/submesh storage, AABB bounds, atomic Ready/Failed state)
  - Own Dia::Mesh3D::Mesh3DAssetHandler implementing IAssetTypeHandler — loads cooked .mesh3d binaries
  - Provide MeshBuilder3D test helper in Dia/DiaMesh3D/Testing/

non_responsibilities:
  - glTF parsing (DiaAssetPipeline — build-time only)
  - Skinning vertex attributes (DiaRig3D)
  - GPU buffer upload (DiaBgfx3D)
  - Material resolution (DiaBgfx3D MaterialRegistry)
  - Rendering (DiaBgfx3D)
---
