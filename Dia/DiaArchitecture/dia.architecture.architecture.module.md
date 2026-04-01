---
schema: dia.module.v1
module_id: dia.architecture
name: Architecture
owner_team: TBD
layer: platform
status: active
maturity: dev

path: Dia/DiaArchitecture
language: cpp
parent_module_id: dia.root

summary: >
  Defines Architecture APIs (ApplicationStateObject) including: StateObject.

intent: >
  Provide reusable Architecture building blocks with consistent semantics for higher-level systems.

responsibilities:
  - Expose primary types: StateObject
  - Define and maintain the public header surface for this module
  - Provide lightweight operations with predictable behavior

non_responsibilities:
  - Domain-specific gameplay behavior
  - Rendering or platform integration concerns (unless this module is explicitly an adapter)
  - High-level orchestration (owned by higher-layer modules)

public_api:
  headers:
    - Dia/DiaArchitecture/ApplicationStateObject.h
  namespaces: []
  entry_points:
    - StateObject

dependencies:
  required:
    - dia.application
    - dia.core.containers.hashtables
    - dia.core.core
    - dia.core.crc
    - dia.core.type
  forbidden: []
---
