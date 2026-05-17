---
schema: dia.module.v1
module_id: dia.geometry2d.random
name: Random
owner_team: TBD
layer: platform
status: active
maturity: dev

path: Dia/DiaGeometry2D/Random
language: cpp
parent_module_id: dia.geometry2d

summary: >
  Random point generation utilities for 2D geometric shapes.

intent: >
  Provide shape-aware random point generation for 2D shapes (Circle, AARect).
  Moved from DiaMaths::Random to honor DiaMaths SD-001 (pure linear algebra only).

responsibilities:
  - Random point generation inside/on 2D shapes (Circle, AARect)
  - Uniform distribution sampling for 2D shapes

non_responsibilities:
  - Basic random number generation (see DiaMaths::Random)
  - 3D shape random point generation (future DiaGeometry3D::Random)

dependent_modules: []

public_api:
  headers:
    - DiaGeometry2D/Random/Random.h
  namespaces:
    - Dia::Geometry2D::Random
  entry_points:
    - RandomPointInCircle
    - RandomPointOnCircle
    - RandomPointInRect

dependencies:
  required:
    - dia.maths.core
    - dia.maths.vector
    - dia.geometry2d.shapes
  forbidden: []
---
