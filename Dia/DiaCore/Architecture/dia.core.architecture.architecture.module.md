---
schema: dia.module.v1
module_id: dia.core.architecture
name: Architecture
owner_team: TBD
layer: platform
status: active
maturity: dev

path: Dia/DiaCore/Architecture
language: cpp
parent_module_id: dia.core

summary: >
  Groups Architecture submodules: Components, Singleton.

intent: >
  Provide a clear module boundary and navigation surface for the Architecture area.

responsibilities:
  - Define the module boundary and ownership for this area
  - Expose stable module identifiers for submodules that contain concrete APIs
  - Keep cross-submodule dependencies explicit via dependent_modules

non_responsibilities:
  - Implementing leaf functionality directly (owned by child modules)
  - Cross-cutting engine orchestration

dependent_modules:
  - dia.core.architecture.components
  - dia.core.architecture.singleton

public_api:
  headers:
    - Dia/DiaCore/Architecture/Observer.h
  namespaces: []
  entry_points:
    - Observer
    - ObserverSubject

dependencies:
  required:
    - dia.core.containers.arrays
    - dia.core.core
  forbidden: []
---
