---
schema: dia.module.v1
module_id: dia.core.architecture.singleton
name: Singleton
owner_team: TBD
layer: platform
status: active
maturity: dev

path: Dia/DiaCore/Architecture/Singleton
language: cpp
parent_module_id: dia.core.architecture

summary: >
  Defines Singleton APIs (Singleton) including: Singleton.

intent: >
  Provide reusable Singleton building blocks with consistent semantics for higher-level systems.

responsibilities:
  - Expose primary types: Singleton
  - Define and maintain the public header surface for this module
  - Provide lightweight operations with predictable behavior

non_responsibilities:
  - Domain-specific gameplay behavior
  - Rendering or platform integration concerns (unless this module is explicitly an adapter)
  - High-level orchestration (owned by higher-layer modules)

public_api:
  headers:
    - Dia/DiaCore/Architecture/Singleton/Singleton.h
  namespaces: []
  entry_points:
    - Singleton

dependencies:
  required:
    - dia.core.core
    - dia.core.memory
  forbidden: []
---
