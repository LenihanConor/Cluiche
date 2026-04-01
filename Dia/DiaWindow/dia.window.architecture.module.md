---
schema: dia.module.v1
module_id: dia.window
name: Window
owner_team: TBD
layer: platform
status: active
maturity: dev

path: Dia/DiaWindow
language: cpp
parent_module_id: dia.root

summary: >
  Groups Window submodules: Interface.

intent: >
  Provide a clear module boundary and navigation surface for the Window area.

responsibilities:
  - Define the module boundary and ownership for this area
  - Expose stable module identifiers for submodules that contain concrete APIs
  - Keep cross-submodule dependencies explicit via dependent_modules

non_responsibilities:
  - Implementing leaf functionality directly (owned by child modules)
  - Cross-cutting engine orchestration

dependent_modules:
  - dia.window.interface

public_api:
  headers:
    - Dia/DiaWindow/DummyClass.h
    - Dia/DiaWindow/SystemHandle.h
  namespaces: []
  entry_points:
    - DummyClass
    - HWND__

dependencies:
  required: []
  forbidden: []
---
