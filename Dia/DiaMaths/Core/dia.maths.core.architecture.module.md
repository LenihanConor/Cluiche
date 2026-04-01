---
schema: dia.module.v1
module_id: dia.maths.core
name: Core
owner_team: TBD
layer: platform
status: active
maturity: dev

path: Dia/DiaMaths/Core
language: cpp
parent_module_id: dia.maths

summary: >
  Defines Core APIs (Angle, CoreMaths, FloatMaths, HalfFloat, MathsDefines, Trigonometry) including: Angle, HalfFloat.

intent: >
  Provide reusable Core building blocks with consistent semantics for higher-level systems.

responsibilities:
  - Expose primary types: Angle, HalfFloat
  - Define and maintain the public header surface for this module
  - Provide lightweight operations with predictable behavior

non_responsibilities:
  - Domain-specific gameplay behavior
  - Rendering or platform integration concerns (unless this module is explicitly an adapter)
  - High-level orchestration (owned by higher-layer modules)

public_api:
  headers:
    - Dia/DiaMaths/Core/Angle.h
    - Dia/DiaMaths/Core/CoreMaths.h
    - Dia/DiaMaths/Core/FloatMaths.h
    - Dia/DiaMaths/Core/HalfFloat.h
    - Dia/DiaMaths/Core/MathsDefines.h
    - Dia/DiaMaths/Core/Trigonometry.h
  namespaces: []
  entry_points:
    - Angle
    - HalfFloat

dependencies:
  required:
    - dia.core.core
  forbidden: []
---
