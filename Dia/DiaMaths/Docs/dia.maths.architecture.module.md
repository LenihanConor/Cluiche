---
schema: dia.module.v1
module_id: dia.maths
name: Maths
owner_team: TBD
layer: platform
status: active
maturity: dev

path: Dia/DiaMaths
language: cpp
parent_module_id: dia.root

summary: >
  Groups Maths submodules: Core, Matrix, Shape, Vector.

intent: >
  Provide a clear module boundary and navigation surface for the Maths area.

responsibilities:
  - Define the module boundary and ownership for this area
  - Expose stable module identifiers for submodules that contain concrete APIs
  - Keep cross-submodule dependencies explicit via dependent_modules

non_responsibilities:
  - Implementing leaf functionality directly (owned by child modules)
  - Cross-cutting engine orchestration

dependent_modules:
  - dia.maths.core
  - dia.maths.matrix
  - dia.maths.quaternion
  - dia.maths.shape
  - dia.maths.transform
  - dia.maths.vector

public_api:
  headers: []
  namespaces: []
  entry_points: []

dependencies:
  required: []
  forbidden: []
---
