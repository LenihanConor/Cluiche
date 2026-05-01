---
schema: dia.module.v1
module_id: dia.core.memory
name: Memory
owner_team: TBD
layer: platform
status: active
maturity: dev

path: Dia/DiaCore/Memory
language: cpp
parent_module_id: dia.core

summary: >
  Defines Memory APIs (Memory).

intent: >
  Provide reusable Memory building blocks with consistent semantics for higher-level systems.

responsibilities:
  - Define and maintain the public header surface for this module
  - Provide lightweight operations with predictable behavior

non_responsibilities:
  - Domain-specific gameplay behavior
  - Rendering or platform integration concerns (unless this module is explicitly an adapter)
  - High-level orchestration (owned by higher-layer modules)

public_api:
  headers:
    - Dia/DiaCore/Memory/Memory.h
  namespaces: []
  entry_points: []

dependencies:
  required:
    - dia.core.core
  forbidden: []
---
