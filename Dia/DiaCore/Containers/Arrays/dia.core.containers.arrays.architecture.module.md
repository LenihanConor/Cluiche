---
schema: dia.module.v1
module_id: dia.core.containers.arrays
name: Arrays
owner_team: TBD
layer: platform
status: active
maturity: dev

path: Dia/DiaCore/Containers/Arrays
language: cpp
parent_module_id: dia.core.containers

summary: >
  Defines Arrays APIs (Array, ArrayC, ArrayIterator, DynamicArray, DynamicArrayC, ReverseArrayIterator) including: Array, ArrayC, ArrayConstIterator, ArrayIterator, DynamicArray, DynamicArrayC, ReverseArrayConstIterator, ReverseArrayIterator.

intent: >
  Provide reusable Arrays building blocks with consistent semantics for higher-level systems.

responsibilities:
  - Expose primary types: Array, ArrayC, ArrayConstIterator, ArrayIterator, DynamicArray, DynamicArrayC, ReverseArrayConstIterator, ReverseArrayIterator
  - Define and maintain the public header surface for this module
  - Provide lightweight operations with predictable behavior

non_responsibilities:
  - Domain-specific gameplay behavior
  - Rendering or platform integration concerns (unless this module is explicitly an adapter)
  - High-level orchestration (owned by higher-layer modules)

public_api:
  headers:
    - Dia/DiaCore/Containers/Arrays/Array.h
    - Dia/DiaCore/Containers/Arrays/ArrayC.h
    - Dia/DiaCore/Containers/Arrays/ArrayIterator.h
    - Dia/DiaCore/Containers/Arrays/DynamicArray.h
    - Dia/DiaCore/Containers/Arrays/DynamicArrayC.h
    - Dia/DiaCore/Containers/Arrays/ReverseArrayIterator.h
  namespaces: []
  entry_points:
    - Array
    - ArrayC
    - ArrayConstIterator
    - ArrayIterator
    - DynamicArray
    - DynamicArrayC
    - ReverseArrayConstIterator
    - ReverseArrayIterator

dependencies:
  required:
    - dia.core.core
    - dia.core.memory
    - dia.core.type
  forbidden: []
---
