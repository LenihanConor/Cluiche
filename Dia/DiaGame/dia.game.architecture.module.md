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
  - DiaStageManifest optional .diastage fields: tags (up to 16 strings, 63 chars each) and description (up to 120 chars); absent loads empty, save omits when empty, wrong type or over-limit fails naming the field
  - StageKinds.h - StageKind enum (showcase, tool, test), name table, KindFromTag and StageKindMask helpers; must match stage_kinds.json
  - DiaGameManifest::editorManifests - optional .diagame "editor.manifests" list (paths relative to the .diagame) naming editor manifests whose plugins exist only while the game is loaded

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
