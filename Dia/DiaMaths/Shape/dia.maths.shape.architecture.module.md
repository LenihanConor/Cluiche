---
schema: dia.module.v1
module_id: dia.maths.shape
name: Shape
owner_team: TBD
layer: platform
status: active
maturity: dev

path: Dia/DiaMaths/Shape
language: cpp
parent_module_id: dia.maths

summary: >
  Groups Shape submodules: 2D, Common.

intent: >
  Provide a clear module boundary and navigation surface for the Shape area.

responsibilities:
  - Define the module boundary and ownership for this area
  - Expose stable module identifiers for submodules that contain concrete APIs
  - Keep cross-submodule dependencies explicit via dependent_modules

non_responsibilities:
  - Implementing leaf functionality directly (owned by child modules)
  - Cross-cutting engine orchestration

dependent_modules:
  - dia.maths.shape.common
  - dia.maths.shape.x2d

public_api:
  headers: []
  namespaces: []
  entry_points: []

dependencies:
  required: []
  forbidden: []
---
