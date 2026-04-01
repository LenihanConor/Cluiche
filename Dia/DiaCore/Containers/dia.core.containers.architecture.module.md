---
schema: dia.module.v1
module_id: dia.core.containers
name: Containers
owner_team: TBD
layer: platform
status: active
maturity: dev

path: Dia/DiaCore/Containers
language: cpp
parent_module_id: dia.core

summary: >
  Groups Containers submodules: Arrays, BitFlag, Graphs, HashTables, LinkList, Misc, Strings.

intent: >
  Provide a clear module boundary and navigation surface for the Containers area.

responsibilities:
  - Define the module boundary and ownership for this area
  - Expose stable module identifiers for submodules that contain concrete APIs
  - Keep cross-submodule dependencies explicit via dependent_modules

non_responsibilities:
  - Implementing leaf functionality directly (owned by child modules)
  - Cross-cutting engine orchestration

dependent_modules:
  - dia.core.containers.arrays
  - dia.core.containers.bitflag
  - dia.core.containers.graphs
  - dia.core.containers.hashtables
  - dia.core.containers.linklist
  - dia.core.containers.misc
  - dia.core.containers.strings

public_api:
  headers: []
  namespaces: []
  entry_points: []

dependencies:
  required: []
  forbidden: []
---
