---
schema: dia.module.v1
module_id: dia.uiawesomium
name: UIAwesomium
owner_team: TBD
layer: platform
status: active
maturity: dev

path: Dia/DiaUIAwesomium
language: cpp
parent_module_id: dia.root

summary: >
  Groups UIAwesomium submodules: Deploy, External.

intent: >
  Provide a clear module boundary and navigation surface for the UIAwesomium area.

responsibilities:
  - Define the module boundary and ownership for this area
  - Expose stable module identifiers for submodules that contain concrete APIs
  - Keep cross-submodule dependencies explicit via dependent_modules

non_responsibilities:
  - Implementing leaf functionality directly (owned by child modules)
  - Cross-cutting engine orchestration

dependent_modules:
  - dia.uiawesomium.deploy
  - dia.uiawesomium.external

public_api:
  headers:
    - Dia/DiaUIAwesomium/AwesomiumUISystem.h
    - Dia/DiaUIAwesomium/Conversion.h
  namespaces: []
  entry_points:
    - IWindow
    - UIDataBuffer
    - UISystem
    - UISystemImpl

dependencies:
  required:
    - dia.core.core
    - dia.core.memory
    - dia.core.strings
    - dia.ui
    - dia.uiawesomium.external
    - dia.window
    - dia.window.interface
  forbidden: []
---
