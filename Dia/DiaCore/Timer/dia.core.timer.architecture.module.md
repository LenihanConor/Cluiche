---
schema: dia.module.v1
module_id: dia.core.timer
name: Timer
owner_team: TBD
layer: platform
status: active
maturity: dev

path: Dia/DiaCore/Timer
language: cpp
parent_module_id: dia.core

summary: >
  Defines Timer APIs (Timer, TimerExpiry, TimerSystem, TimeThreadLimiter) including: Timer, TimerExpiry, TimerSystem, TimeThreadLimiter.

intent: >
  Provide reusable Timer building blocks with consistent semantics for higher-level systems.

responsibilities:
  - Expose primary types: Timer, TimerExpiry, TimerSystem, TimeThreadLimiter
  - Define and maintain the public header surface for this module
  - Provide lightweight operations with predictable behavior

non_responsibilities:
  - Domain-specific gameplay behavior
  - Rendering or platform integration concerns (unless this module is explicitly an adapter)
  - High-level orchestration (owned by higher-layer modules)

public_api:
  headers:
    - Dia/DiaCore/Timer/Timer.h
    - Dia/DiaCore/Timer/TimerExpiry.h
    - Dia/DiaCore/Timer/TimerSystem.h
    - Dia/DiaCore/Timer/TimeThreadLimiter.h
  namespaces: []
  entry_points:
    - Timer
    - TimerExpiry
    - TimerSystem
    - TimeThreadLimiter

dependencies:
  required:
    - dia.core.core
    - dia.core.time
  forbidden: []
---
