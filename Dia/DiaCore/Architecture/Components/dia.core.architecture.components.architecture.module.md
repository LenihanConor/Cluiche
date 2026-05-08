---
schema: dia.module.v1
module_id: dia.core.architecture.components
name: Components
owner_team: TBD
layer: platform
status: active
maturity: dev

path: Dia/DiaCore/Architecture/Components
language: cpp
parent_module_id: dia.core.architecture

summary: >
  Groups Components submodules: Concrete, Interface.

intent: >
  Provide a clear module boundary and navigation surface for the Components area.

responsibilities:
  - Define the module boundary and ownership for this area
  - Expose stable module identifiers for submodules that contain concrete APIs
  - Keep cross-submodule dependencies explicit via dependent_modules

non_responsibilities:
  - Implementing leaf functionality directly (owned by child modules)
  - Cross-cutting engine orchestration

dependent_modules:
  - dia.core.architecture.components.concrete
  - dia.core.architecture.components.interface

public_api:
  headers:
    - Dia/DiaCore/Architecture/Components/ComponentFactoryRegistry.h
  namespaces: []
  entry_points:
    - ComponentFactoryRegistry

dependencies:
  required:
    - dia.core.architecture.components.interface
    - dia.core.core
  forbidden: []
---
