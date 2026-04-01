---
schema: dia.module.v1
module_id: dia.root
name: Dia
owner_team: TBD
layer: platform
status: active
maturity: dev

path: Dia
language: cpp

summary: >
  Groups Dia submodules: AI, Application, Architecture, Core, Graphics, Input, IO, Maths, Physics, SFML, UI, UIAwesomium, Window.

intent: >
  Provide a clear module boundary and navigation surface for the Dia area.

responsibilities:
  - Define the module boundary and ownership for this area
  - Expose stable module identifiers for submodules that contain concrete APIs
  - Keep cross-submodule dependencies explicit via dependent_modules

non_responsibilities:
  - Implementing leaf functionality directly (owned by child modules)
  - Cross-cutting engine orchestration

dependent_modules:
  - dia.ai
  - dia.application
  - dia.architecture
  - dia.core
  - dia.graphics
  - dia.input
  - dia.io
  - dia.maths
  - dia.physics
  - dia.sfml
  - dia.ui
  - dia.uiawesomium
  - dia.window

public_api:
  headers: []
  namespaces: []
  entry_points: []

dependencies:
  required: []
  forbidden: []
---
