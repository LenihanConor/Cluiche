---
schema: dia.module.v1
module_id: dia.maths.quaternion
name: Quaternion
owner_team: TBD
layer: platform
status: active
maturity: dev

path: Dia/DiaMaths/Quaternion
language: cpp
parent_module_id: dia.maths

summary: >
  Unit quaternion for 3D rotation in the Dia maths library.

intent: >
  Provide a single, tested quaternion type for 3D rotation composition,
  interpolation, and conversion. Backs Transform3D, OOBB orientation, and
  Matrix44::FromRotation.

responsibilities:
  - Quaternion storage (xyzw, Hamilton convention)
  - Rotation of vectors via optimised cross-product form
  - Conversion to/from Matrix33 and Euler angles (YXZ)
  - Slerp/Nlerp interpolation with shortest-path guarantee

non_responsibilities:
  - 3D shape representation (see DiaGeometry3D)
  - Scene graph hierarchy (see Transform3D)

dependent_modules:
  - dia.maths.vector
  - dia.maths.matrix
  - dia.core

public_api:
  headers:
    - DiaMaths/Quaternion/Quaternion.h
  namespaces:
    - Dia::Maths
  entry_points:
    - Quaternion

dependencies:
  required:
    - dia.maths.vector
    - dia.maths.matrix
    - dia.core
  forbidden: []
---
