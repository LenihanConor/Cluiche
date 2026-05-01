---
schema: dia.module.v1
module_id: dia.maths.shape.common
name: Common
owner_team: TBD
layer: platform
status: active
maturity: dev

path: Dia/DiaMaths/Shape/Common
language: cpp
parent_module_id: dia.maths.shape

summary: >
  Defines Common APIs (IntersectionClassify, IntersectionTests) including: AARect2D, Arc2D, Capsule2D, Circle2D, IntersectionClassify, IntersectionTests, Line2D, OORect2D, Triangle2D, Vector2D.

intent: >
  Provide reusable Common building blocks with consistent semantics for higher-level systems.

responsibilities:
  - Expose primary types: AARect2D, Arc2D, Capsule2D, Circle2D, IntersectionClassify, IntersectionTests, Line2D, OORect2D, Triangle2D, Vector2D
  - Define and maintain the public header surface for this module
  - Provide lightweight operations with predictable behavior

non_responsibilities:
  - Domain-specific gameplay behavior
  - Rendering or platform integration concerns (unless this module is explicitly an adapter)
  - High-level orchestration (owned by higher-layer modules)

public_api:
  headers:
    - Dia/DiaMaths/Shape/Common/IntersectionClassify.h
    - Dia/DiaMaths/Shape/Common/IntersectionTests.h
  namespaces: []
  entry_points:
    - AARect2D
    - Arc2D
    - Capsule2D
    - Circle2D
    - IntersectionClassify
    - IntersectionTests
    - Line2D
    - OORect2D
    - Triangle2D
    - Vector2D

dependencies:
  required:
    - dia.maths.core
    - dia.maths.shape.x2d
  forbidden: []
---
