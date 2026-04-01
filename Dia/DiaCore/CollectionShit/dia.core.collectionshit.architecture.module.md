---
schema: dia.module.v1
module_id: dia.core.collectionshit
name: CollectionShit
owner_team: TBD
layer: platform
status: active
maturity: dev

path: Dia/DiaCore/CollectionShit
language: cpp
parent_module_id: dia.core

summary: >
  Defines CollectionShit APIs (Factory, Functor, LookupTables, ObjectPool, Observer, Singleton) including: ComparisonFunctor, CreatorObjectType, EqualityFunctor, EvaluateFunctor, Factory, Functor, LookupTables, Singleton.

intent: >
  Provide reusable CollectionShit building blocks with consistent semantics for higher-level systems.

responsibilities:
  - Expose primary types: ComparisonFunctor, CreatorObjectType, EqualityFunctor, EvaluateFunctor, Factory, Functor, LookupTables, Singleton
  - Define and maintain the public header surface for this module
  - Provide lightweight operations with predictable behavior

non_responsibilities:
  - Domain-specific gameplay behavior
  - Rendering or platform integration concerns (unless this module is explicitly an adapter)
  - High-level orchestration (owned by higher-layer modules)

public_api:
  headers:
    - Dia/DiaCore/CollectionShit/Factory.h
    - Dia/DiaCore/CollectionShit/Functor.h
    - Dia/DiaCore/CollectionShit/LookupTables.h
    - Dia/DiaCore/CollectionShit/ObjectPool.h
    - Dia/DiaCore/CollectionShit/Observer.h
    - Dia/DiaCore/CollectionShit/Singleton.h
  namespaces: []
  entry_points:
    - ComparisonFunctor
    - CreatorObjectType
    - EqualityFunctor
    - EvaluateFunctor
    - Factory
    - Functor
    - LookupTables
    - Singleton

dependencies:
  required:
    - dia.collections.arrays
    - dia.collections.common
    - dia.core.core
    - dia.core.datastructures
    - dia.core.memory
  forbidden: []
---
