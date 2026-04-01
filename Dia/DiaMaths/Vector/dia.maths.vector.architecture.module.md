---
schema: dia.module.v1
module_id: dia.maths.vector
name: Vector
owner_team: TBD
layer: platform
status: active
maturity: dev

path: Dia/DiaMaths/Vector
language: cpp
parent_module_id: dia.maths

summary: >
  Defines Vector APIs (Vector2D, Vector2DAdapter, Vector3D, Vector3DAdapter, Vector4D, Vector4DAdapter, VectorHalf2D, VectorUtils) including: Angle, Matrix22, Vector2D, Vector2DAdapter, Vector3D, Vector3DAdapter, Vector4D, Vector4DAdapter, VectorHalf2D.

intent: >
  Provide reusable Vector building blocks with consistent semantics for higher-level systems.

responsibilities:
  - Expose primary types: Angle, Matrix22, Vector2D, Vector2DAdapter, Vector3D, Vector3DAdapter, Vector4D, Vector4DAdapter, VectorHalf2D
  - Define and maintain the public header surface for this module
  - Provide lightweight operations with predictable behavior

non_responsibilities:
  - Domain-specific gameplay behavior
  - Rendering or platform integration concerns (unless this module is explicitly an adapter)
  - High-level orchestration (owned by higher-layer modules)

public_api:
  headers:
    - Dia/DiaMaths/Vector/Vector2D.h
    - Dia/DiaMaths/Vector/Vector2DAdapter.h
    - Dia/DiaMaths/Vector/Vector3D.h
    - Dia/DiaMaths/Vector/Vector3DAdapter.h
    - Dia/DiaMaths/Vector/Vector4D.h
    - Dia/DiaMaths/Vector/Vector4DAdapter.h
    - Dia/DiaMaths/Vector/VectorHalf2D.h
    - Dia/DiaMaths/Vector/VectorUtils.h
  namespaces: []
  entry_points:
    - Angle
    - Matrix22
    - Vector2D
    - Vector2DAdapter
    - Vector3D
    - Vector3DAdapter
    - Vector4D
    - Vector4DAdapter
    - VectorHalf2D

dependencies:
  required:
    - dia.core.core
    - dia.core.type
    - dia.maths.core
    - dia.maths.matrix
  forbidden: []
---
