---
schema: dia.module.v1
module_id: dia.maths.shape.x2d
name: 2D
owner_team: TBD
layer: platform
status: active
maturity: dev

path: Dia/DiaMaths/Shape/2D
language: cpp
parent_module_id: dia.maths.shape

summary: >
  Defines 2D APIs (AARect2D, Arc2D, Capsule2D, Circle2D, Ellipse2D, IntersectionPoint2D, Line2D, OORect2D, Triangle2D) including: AARect2D, Angle, Arc2D, Capsule2D, Circle2D, Ellipse2D, IntersectionPoint2D, Line2D, OORect2D, Triangle2D, Vector2D.

intent: >
  Provide reusable 2D building blocks with consistent semantics for higher-level systems.

responsibilities:
  - Expose primary types: AARect2D, Angle, Arc2D, Capsule2D, Circle2D, Ellipse2D, IntersectionPoint2D, Line2D, OORect2D, Triangle2D, Vector2D
  - Define and maintain the public header surface for this module
  - Provide lightweight operations with predictable behavior

non_responsibilities:
  - Domain-specific gameplay behavior
  - Rendering or platform integration concerns (unless this module is explicitly an adapter)
  - High-level orchestration (owned by higher-layer modules)

public_api:
  headers:
    - Dia/DiaMaths/Shape/2D/AARect2D.h
    - Dia/DiaMaths/Shape/2D/Arc2D.h
    - Dia/DiaMaths/Shape/2D/Capsule2D.h
    - Dia/DiaMaths/Shape/2D/Circle2D.h
    - Dia/DiaMaths/Shape/2D/Ellipse2D.h
    - Dia/DiaMaths/Shape/2D/IntersectionPoint2D.h
    - Dia/DiaMaths/Shape/2D/Line2D.h
    - Dia/DiaMaths/Shape/2D/OORect2D.h
    - Dia/DiaMaths/Shape/2D/Triangle2D.h
  namespaces: []
  entry_points:
    - AARect2D
    - Angle
    - Arc2D
    - Capsule2D
    - Circle2D
    - Ellipse2D
    - IntersectionPoint2D
    - Line2D
    - OORect2D
    - Triangle2D
    - Vector2D

dependencies:
  required:
    - dia.core.core
    - dia.maths.core
    - dia.maths.shape.common
    - dia.maths.vector
  forbidden: []
---
