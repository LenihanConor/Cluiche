---
schema: dia.module.v1
module_id: dia.core.containers.linklist
name: LinkList
owner_team: TBD
layer: platform
status: active
maturity: dev

path: Dia/DiaCore/Containers/LinkList
language: cpp
parent_module_id: dia.core.containers

summary: >
  Groups LinkList submodules: BLAH.

intent: >
  Provide a clear module boundary and navigation surface for the LinkList area.

responsibilities:
  - Define the module boundary and ownership for this area
  - Expose stable module identifiers for submodules that contain concrete APIs
  - Keep cross-submodule dependencies explicit via dependent_modules

non_responsibilities:
  - Implementing leaf functionality directly (owned by child modules)
  - Cross-cutting engine orchestration

dependent_modules:
  - dia.core.containers.linklist.blah

public_api:
  headers:
    - Dia/DiaCore/Containers/LinkList/LinkListC.h
    - Dia/DiaCore/Containers/LinkList/LinkListNode.h
  namespaces: []
  entry_points:
    - LinkListC
    - LinkListNode

dependencies:
  required:
    - dia.core.core
    - dia.core.type
  forbidden: []
---
