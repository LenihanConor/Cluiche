---
schema: dia.module.v1
module_id: dia.core.core
name: Core
owner_team: TBD
layer: platform
status: active
maturity: dev

path: Dia/DiaCore/Core
language: cpp
parent_module_id: dia.core

summary: >
  Defines Core APIs (Assert, CallStack, EnumClass, Functor, Log, MetaLogic, System) including: A, CallstackEntry, ComparisonFunctor, EnumDescription, EqualityFunctor, EvaluateFunctor, Functor, Log, MetaAnd, MetaIf, MetaMax, MetaMax_First.

intent: >
  Provide reusable Core building blocks with consistent semantics for higher-level systems.

responsibilities:
  - Expose primary types: A, CallstackEntry, ComparisonFunctor, EnumDescription, EqualityFunctor, EvaluateFunctor, Functor, Log, MetaAnd, MetaIf, MetaMax, MetaMax_First
  - Define and maintain the public header surface for this module
  - Provide lightweight operations with predictable behavior

non_responsibilities:
  - Domain-specific gameplay behavior
  - Rendering or platform integration concerns (unless this module is explicitly an adapter)
  - High-level orchestration (owned by higher-layer modules)

public_api:
  headers:
    - Dia/DiaCore/Core/Assert.h
    - Dia/DiaCore/Core/CallStack.h
    - Dia/DiaCore/Core/EnumClass.h
    - Dia/DiaCore/Core/Functor.h
    - Dia/DiaCore/Core/Log.h
    - Dia/DiaCore/Core/MetaLogic.h
    - Dia/DiaCore/Core/System.h
  namespaces: []
  entry_points:
    - A
    - CallstackEntry
    - ComparisonFunctor
    - EnumDescription
    - EqualityFunctor
    - EvaluateFunctor
    - Functor
    - Log
    - MetaAnd
    - MetaIf
    - MetaMax
    - MetaMax_First

dependencies:
  required:
    - dia.core.memory
    - dia.core.strings
    - dia.core.type
  forbidden: []
---
