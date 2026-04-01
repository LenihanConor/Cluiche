---
schema: dia.module.v1
module_id: dia.core.time
name: Time
owner_team: TBD
layer: platform
status: active
maturity: dev

path: Dia/DiaCore/Time
language: cpp
parent_module_id: dia.core

summary: >
  Defines Time APIs (SystemClock, TimeAbsolute, TimeAbstract, TimeRelative, TimeServer) including: SystemClock, TimeAbsolute, TimeAbstract, TimeRelative, TimeServer.

intent: >
  Provide reusable Time building blocks with consistent semantics for higher-level systems.

responsibilities:
  - Expose primary types: SystemClock, TimeAbsolute, TimeAbstract, TimeRelative, TimeServer
  - Define and maintain the public header surface for this module
  - Provide lightweight operations with predictable behavior

non_responsibilities:
  - Domain-specific gameplay behavior
  - Rendering or platform integration concerns (unless this module is explicitly an adapter)
  - High-level orchestration (owned by higher-layer modules)

public_api:
  headers:
    - Dia/DiaCore/Time/SystemClock.h
    - Dia/DiaCore/Time/TimeAbsolute.h
    - Dia/DiaCore/Time/TimeAbstract.h
    - Dia/DiaCore/Time/TimeRelative.h
    - Dia/DiaCore/Time/TimeServer.h
  namespaces: []
  entry_points:
    - SystemClock
    - TimeAbsolute
    - TimeAbstract
    - TimeRelative
    - TimeServer

dependencies:
  required:
    - dia.core.core
  forbidden: []
---
