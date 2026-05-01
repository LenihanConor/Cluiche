---
schema: dia.module.v1
module_id: dia.core
name: Core
owner_team: TBD
layer: platform
status: active
maturity: dev

path: Dia/DiaCore
language: cpp
parent_module_id: dia.root

summary: >
  Groups Core submodules: Architecture, CollectionShit, Containers, Core, CRC, FilePath, Frame, Json, LinkLists, Memory, Strings, Time, Timer, Type.

intent: >
  Provide a clear module boundary and navigation surface for the Core area.

responsibilities:
  - Define the module boundary and ownership for this area
  - Expose stable module identifiers for submodules that contain concrete APIs
  - Keep cross-submodule dependencies explicit via dependent_modules

non_responsibilities:
  - Implementing leaf functionality directly (owned by child modules)
  - Cross-cutting engine orchestration

dependent_modules:
  - dia.core.architecture
  - dia.core.collectionshit
  - dia.core.containers
  - dia.core.core
  - dia.core.crc
  - dia.core.filepath
  - dia.core.frame
  - dia.core.json
  - dia.core.linklists
  - dia.core.memory
  - dia.core.strings
  - dia.core.time
  - dia.core.timer
  - dia.core.type

public_api:
  headers: []
  namespaces: []
  entry_points: []

dependencies:
  required: []
  forbidden: []
---
