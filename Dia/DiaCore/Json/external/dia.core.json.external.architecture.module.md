---
schema: dia.module.v1
module_id: dia.core.json.external
name: external
owner_team: TBD
layer: platform
status: active
maturity: dev

path: Dia/DiaCore/Json/external
language: cpp
parent_module_id: dia.core.json

summary: >
  Groups external submodules: json.

intent: >
  Provide a clear module boundary and navigation surface for the external area.

responsibilities:
  - Define the module boundary and ownership for this area
  - Expose stable module identifiers for submodules that contain concrete APIs
  - Keep cross-submodule dependencies explicit via dependent_modules

non_responsibilities:
  - Implementing leaf functionality directly (owned by child modules)
  - Cross-cutting engine orchestration

dependent_modules:
  - dia.core.json.external.json

public_api:
  headers:
    - Dia/DiaCore/Json/external/json_tool.h
  namespaces: []
  entry_points: []

dependencies:
  required:
    - dia.core.json
  forbidden: []
---
