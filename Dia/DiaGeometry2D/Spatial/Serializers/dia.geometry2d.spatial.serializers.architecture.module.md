---
schema: dia.module.v1
module_id: dia.geometry2d.spatial.serializers
name: DiaGeometry2D.Spatial.Serializers
owner_team: TBD
layer: foundation/platform
status: active
maturity: dev

path: Dia/DiaGeometry2D/Spatial/Serializers
language: cpp
parent_module_id: dia.geometry2d

summary: >
  JSON serializers for DiaGeometry2D spatial structures (SpatialGrid, HexGrid).
  Sits at foundation/platform so it can depend on both dia.geometry2d (maths) and
  dia.serializer (services) without cross-group violations.

dependent_modules:
  - dia.core.json.external.json

dependencies:
  required:
    - dia.geometry2d
    - dia.serializer
    - dia.observation
    - dia.core.json.external.json
---
