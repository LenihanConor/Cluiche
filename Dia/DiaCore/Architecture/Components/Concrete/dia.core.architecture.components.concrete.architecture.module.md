---
schema: dia.module.v1
module_id: dia.core.architecture.components.concrete
name: Concrete
owner_team: TBD
layer: platform
status: active
maturity: dev

path: Dia/DiaCore/Architecture/Components/Concrete
language: cpp
parent_module_id: dia.core.architecture.components

summary: >
  Defines Concrete APIs (DynamicComponentFactory, StaticPooledComponentFactory, StaticSizedComponentObject) including: ComponentManager, ComponentMetaData, DynamicComponentFactory, factoryName, StaticPooledComponentFactory, StaticSizedComponentObject.

intent: >
  Provide reusable Concrete building blocks with consistent semantics for higher-level systems.

responsibilities:
  - Expose primary types: ComponentManager, ComponentMetaData, DynamicComponentFactory, factoryName, StaticPooledComponentFactory, StaticSizedComponentObject
  - Define and maintain the public header surface for this module
  - Provide lightweight operations with predictable behavior

non_responsibilities:
  - Domain-specific gameplay behavior
  - Rendering or platform integration concerns (unless this module is explicitly an adapter)
  - High-level orchestration (owned by higher-layer modules)

public_api:
  headers:
    - Dia/DiaCore/Architecture/Components/Concrete/DynamicComponentFactory.h
    - Dia/DiaCore/Architecture/Components/Concrete/StaticPooledComponentFactory.h
    - Dia/DiaCore/Architecture/Components/Concrete/StaticSizedComponentObject.h
  namespaces: []
  entry_points:
    - ComponentManager
    - ComponentMetaData
    - DynamicComponentFactory
    - factoryName
    - StaticPooledComponentFactory
    - StaticSizedComponentObject

dependencies:
  required:
    - dia.core.architecture.components.interface
    - dia.core.containers.arrays
    - dia.core.core
  forbidden: []
---
