---
schema: dia.module.v1
module_id: dia.core.containers.hashtables
name: HashTables
owner_team: TBD
layer: platform
status: active
maturity: dev

path: Dia/DiaCore/Containers/HashTables
language: cpp
parent_module_id: dia.core.containers

summary: >
  Defines HashTables APIs (HashTable, HashTableC, HashTableHashFunctionData, HashTableNode) including: HashTable, HashTableC, HashTableHashFunctionData, HashTableNode.

intent: >
  Provide reusable HashTables building blocks with consistent semantics for higher-level systems.

responsibilities:
  - Expose primary types: HashTable, HashTableC, HashTableHashFunctionData, HashTableNode
  - Define and maintain the public header surface for this module
  - Provide lightweight operations with predictable behavior

non_responsibilities:
  - Domain-specific gameplay behavior
  - Rendering or platform integration concerns (unless this module is explicitly an adapter)
  - High-level orchestration (owned by higher-layer modules)

public_api:
  headers:
    - Dia/DiaCore/Containers/HashTables/HashTable.h
    - Dia/DiaCore/Containers/HashTables/HashTableC.h
    - Dia/DiaCore/Containers/HashTables/HashTableHashFunctionData.h
    - Dia/DiaCore/Containers/HashTables/HashTableNode.h
  namespaces: []
  entry_points:
    - HashTable
    - HashTableC
    - HashTableHashFunctionData
    - HashTableNode

dependencies:
  required:
    - dia.core.containers.arrays
    - dia.core.core
  forbidden: []
---
