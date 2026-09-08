---
schema: dia.module.v1
module_id: dia.picking
name: DiaPicking
layer: foundation/services
path: Dia/DiaPicking
status: active
maturity: dev
parent_module_id: dia.root

summary: >
  Abstract picking primitives — PickRay, PickResult, IPickable interface.
  Domain-agnostic base consumed by DiaGeometry2DPicking and future 3D pickers.

responsibilities:
  - PickRay and PickResult value types
  - IPickable interface

non_responsibilities:
  - Geometry-specific intersection math (DiaGeometry2DPicking)
  - Input routing (application layer)

dependencies:
  required:
    - dia.core
    - dia.mailbox
  forbidden: []
---
