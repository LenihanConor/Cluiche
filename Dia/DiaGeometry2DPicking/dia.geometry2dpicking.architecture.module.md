---
schema: dia.module.v1
module_id: dia.geometry2dpicking
name: DiaGeometry2DPicking
layer: foundation/platform
path: Dia/DiaGeometry2DPicking
status: active
maturity: dev
parent_module_id: dia.root

summary: >
  2D spatial picking — point-in-shape, ray-cast, and nearest-primitive queries
  over DiaGeometry2D primitive types. Used by editor tools and input routing.

responsibilities:
  - Point-in-shape tests for all Geometry2D primitives
  - Ray-cast intersection queries
  - Nearest-primitive query helpers

non_responsibilities:
  - 3D picking (future)
  - Input event routing (application layer)
  - Rendering (domain/visual)

dependencies:
  required:
    - dia.core
    - dia.maths
    - dia.geometry2d
    - dia.picking
  forbidden: []
---
