---
schema: dia.module.v1
module_id: dia.maths.transform
name: Transform
owner_team: TBD
layer: platform
status: active
maturity: dev

path: Dia/DiaMaths/Transform
language: cpp
parent_module_id: dia.maths

summary: >
  2D and 3D transforms with parent-child hierarchy for scene graphs.

intent: >
  Provide tested Transform2D and Transform3D types for hierarchical transformations.
  Backs scene graphs, skeletal animation, and gameplay entity positioning.

responsibilities:
  - Transform storage (position, rotation, scale) in local and world space
  - Parent-child hierarchy via raw non-owning pointers
  - World-space getters (traverses parent chain)
  - World-space setters (back-solves to local)
  - Space conversions (TransformPoint, InverseTransformPoint)
  - Matrix generation (GetLocalMatrix, GetWorldMatrix)

non_responsibilities:
  - Ownership of parent transforms (caller manages lifetime)
  - Cycle detection in release builds (debug-only assertion)
  - 3D shape representation (see DiaGeometry3D)

dependent_modules:
  - dia.maths.vector
  - dia.maths.matrix
  - dia.maths.quaternion
  - dia.maths.core
  - dia.core.type

public_api:
  headers:
    - DiaMaths/Transform/Transform2D.h
    - DiaMaths/Transform/Transform3D.h
  namespaces:
    - Dia::Maths
  entry_points:
    - Transform2D
    - Transform3D

dependencies:
  required:
    - dia.maths.vector
    - dia.maths.matrix
    - dia.maths.quaternion
    - dia.maths.core
    - dia.core.type
  forbidden: []
---
