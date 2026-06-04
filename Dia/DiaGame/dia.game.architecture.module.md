---
schema: dia.module.v1
module_id: dia.game
name: DiaGame
layer: foundation/application
path: Dia/DiaGame
status: active
maturity: dev
parent_module_id: dia.root

summary: >
  Game project serialisation — loads .diagame, .diastage, .diascene manifest files
  and provides the in-memory GameProject model consumed by the editor and CLI.

responsibilities:
  - GameProject, StageDescriptor, SceneDescriptor value types
  - .diagame / .diastage / .diascene JSON load and save
  - Game project validation

non_responsibilities:
  - Application lifecycle (DiaApplicationFlow)
  - Asset loading (DiaAssetRuntime)
  - Editor UI (DiaEditor and plugins)

dependencies:
  required:
    - dia.core
    - dia.json
    - dia.observation
    - dia.serializer
    - dia.application
  forbidden: []
---
