---
schema: dia.module.v1
module_id: dia.maths.matrix
name: Matrix
owner_team: TBD
layer: platform
status: active
maturity: dev

path: Dia/DiaMaths/Matrix
language: cpp
parent_module_id: dia.maths

summary: >
  Defines Matrix APIs (Matrix, Matrix22, Matrix33, Matrix44) including: Angle, Matrix, Matrix22, Vector2D.

intent: >
  Provide reusable Matrix building blocks with consistent semantics for higher-level systems.

responsibilities:
  - Expose primary types: Angle, Matrix, Matrix22, Vector2D
  - Define and maintain the public header surface for this module
  - Provide lightweight operations with predictable behavior

non_responsibilities:
  - Domain-specific gameplay behavior
  - Rendering or platform integration concerns (unless this module is explicitly an adapter)
  - High-level orchestration (owned by higher-layer modules)

public_api:
  headers:
    - Dia/DiaMaths/Matrix/Matrix.h
    - Dia/DiaMaths/Matrix/Matrix22.h
    - Dia/DiaMaths/Matrix/Matrix33.h
    - Dia/DiaMaths/Matrix/Matrix44.h
  namespaces: []
  entry_points:
    - Angle
    - Matrix
    - Matrix22
    - Vector2D

dependencies:
  required:
    - dia.core.type
    - dia.maths.core
    - dia.maths.vector
  forbidden: []
---
