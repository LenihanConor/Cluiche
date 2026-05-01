---
schema: dia.module.v1
module_id: dia.core.json
name: Json
owner_team: TBD
layer: platform
status: active
maturity: dev

path: Dia/DiaCore/Json
language: cpp
parent_module_id: dia.core

summary: >
  Groups Json submodules: external.

intent: >
  Provide a clear module boundary and navigation surface for the Json area.

responsibilities:
  - Define the module boundary and ownership for this area
  - Expose stable module identifiers for submodules that contain concrete APIs
  - Keep cross-submodule dependencies explicit via dependent_modules

non_responsibilities:
  - Implementing leaf functionality directly (owned by child modules)
  - Cross-cutting engine orchestration

dependent_modules:
  - dia.core.json.external

public_api:
  headers: []
  namespaces: []
  entry_points: []

dependencies:
  required: []
  forbidden: []
---
