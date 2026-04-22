---
schema: dia.module.v1
module_id: dia.application
name: Application
owner_team: TBD
layer: platform
status: active
maturity: dev

path: Dia/DiaApplication
language: cpp
parent_module_id: dia.root

summary: >
  Defines Application APIs (ApplicationModule, ApplicationPhase, ApplicationProcessingUnit, ApplicationStateObject) including: BuildDependencyData, IBuildDependencyData, IStartData, Module, Phase, ProcessingUnit, StateObject.

intent: >
  Provide reusable Application building blocks with consistent semantics for higher-level systems.

responsibilities:
  - Expose primary types: BuildDependencyData, IBuildDependencyData, IStartData, Module, Phase, ProcessingUnit, StateObject
  - Define and maintain the public header surface for this module
  - Provide lightweight operations with predictable behavior

non_responsibilities:
  - Domain-specific gameplay behavior
  - Rendering or platform integration concerns (unless this module is explicitly an adapter)
  - High-level orchestration (owned by higher-layer modules)

public_api:
  headers:
    - Dia/DiaApplication/ApplicationModule.h
    - Dia/DiaApplication/ApplicationPhase.h
    - Dia/DiaApplication/ApplicationProcessingUnit.h
    - Dia/DiaApplication/ApplicationStateObject.h
    - Dia/DiaApplication/ModuleRef.h
  namespaces: []
  entry_points:
    - BuildDependencyData
    - IBuildDependencyData
    - IStartData
    - Module
    - ModuleRef
    - Phase
    - ProcessingUnit
    - StateObject

dependencies:
  required:
    - dia.core.containers.arrays
    - dia.core.containers.hashtables
    - dia.core.core
    - dia.core.crc
    - dia.core.strings
    - dia.core.time
    - dia.core.timer
    - dia.core.type
  forbidden: []
---
