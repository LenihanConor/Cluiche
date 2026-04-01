---
schema: dia.module.v1
module_id: dia.core.architecture.components.interface
name: Interface
owner_team: TBD
layer: platform
status: active
maturity: dev

path: Dia/DiaCore/Architecture/Components/Interface
language: cpp
parent_module_id: dia.core.architecture.components

summary: >
  Defines Interface APIs (ComponentID, IComponent, IComponentFactory, IComponentObject) including: CreateStruct, Donkey, DonkeyFeetComponent, DonkeyFeetCreateStruct, DonkeyFeetFactory, IComponent, IComponentFactory, IComponentObject, UpdateStruct.

intent: >
  Provide reusable Interface building blocks with consistent semantics for higher-level systems.

responsibilities:
  - Expose primary types: CreateStruct, Donkey, DonkeyFeetComponent, DonkeyFeetCreateStruct, DonkeyFeetFactory, IComponent, IComponentFactory, IComponentObject, UpdateStruct
  - Define and maintain the public header surface for this module
  - Provide lightweight operations with predictable behavior

non_responsibilities:
  - Domain-specific gameplay behavior
  - Rendering or platform integration concerns (unless this module is explicitly an adapter)
  - High-level orchestration (owned by higher-layer modules)

public_api:
  headers:
    - Dia/DiaCore/Architecture/Components/Interface/ComponentID.h
    - Dia/DiaCore/Architecture/Components/Interface/IComponent.h
    - Dia/DiaCore/Architecture/Components/Interface/IComponentFactory.h
    - Dia/DiaCore/Architecture/Components/Interface/IComponentObject.h
  namespaces: []
  entry_points:
    - CreateStruct
    - Donkey
    - DonkeyFeetComponent
    - DonkeyFeetCreateStruct
    - DonkeyFeetFactory
    - IComponent
    - IComponentFactory
    - IComponentObject
    - UpdateStruct

dependencies:
  required:
    - dia.core.core
  forbidden: []
---
