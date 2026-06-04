---
schema: dia.module.v1
module_id: dia.editor
name: DiaEditor
layer: foundation/services
path: Dia/DiaEditor
status: active
maturity: dev
parent_module_id: dia.root

summary: >
  CluicheEditor host framework — plugin registration, CEF shell, WebSocket game connection,
  layout persistence, and the IEditorPlugin contract all editor plugins implement.

responsibilities:
  - IEditorPlugin interface and REGISTER_EDITOR_PLUGIN macro
  - CEF browser shell and JS↔C++ bridge
  - GameConnectionManager (WebSocket connection to running game)
  - Layout save/restore
  - Toast notification service
  - Plugin lifecycle (init / update / shutdown)

non_responsibilities:
  - Game simulation (CluicheTest / CluicheEditor app)
  - Individual plugin implementations (DiaBlueprintEditor, DiaSceneEditor, etc.)

dependencies:
  required:
    - dia.json
    - dia.observation
    - dia.debugprotocol
    - dia.game
    - dia.protobuf
  forbidden: []
---
