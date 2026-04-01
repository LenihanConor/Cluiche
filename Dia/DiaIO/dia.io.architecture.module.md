---
schema: dia.module.v1
module_id: dia.io
name: IO
owner_team: TBD
layer: platform
status: active
maturity: dev

path: Dia/DiaIO
language: cpp
parent_module_id: dia.root

summary: >
  Defines IO APIs (module types).

intent: >
  Provide reusable IO building blocks with consistent semantics for higher-level systems.

responsibilities:
  - Provide lightweight operations with predictable behavior

non_responsibilities:
  - Domain-specific gameplay behavior
  - Rendering or platform integration concerns (unless this module is explicitly an adapter)
  - High-level orchestration (owned by higher-layer modules)

public_api:
  headers: []
  namespaces: []
  entry_points: []

dependencies:
  required: []
  forbidden: []
---
