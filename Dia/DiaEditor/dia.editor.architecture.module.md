---
schema: dia.module.v1
module_id: dia.editor
name: DiaEditor
layer: foundation/application
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
  - Individual plugin implementations (DiaEntityTemplateEditor, DiaSceneEditor, etc.)

dependencies:
  required:
    - dia.json
    - dia.observation
    - dia.debugprotocol
    - dia.game
    - dia.protobuf
  forbidden: []

public_api:
  headers:
    - DiaEditor/Plugin/IEditorPlugin.h
    - DiaEditor/Plugin/EditorPluginBase.h
    - DiaEditor/Plugin/LiveConnectionPluginBase.h
    - DiaEditor/Plugin/EditorPluginRegistrationMacros.h
    - DiaEditor/Plugin/EditorPluginContext.h
    - DiaEditor/Plugin/EditorPluginRegistry.h
    - DiaEditor/Plugin/GameConnectionEditorPlugin.h
  namespaces:
    - Dia::Editor
  entry_points:
    - IEditorPlugin (plugin contract)
    - EditorPluginBase (base class for non-connection plugins)
    - LiveConnectionPluginBase (base class for plugins that observe game connection)
    - REGISTER_EDITOR_PLUGIN (registration macro)
---

# DiaEditor

CluicheEditor host framework — plugin registration, CEF shell, WebSocket game connection, layout persistence, and the IEditorPlugin contract all editor plugins implement.

## Conventions

**Live connection plugins:** Any plugin that observes the game connection MUST derive from `LiveConnectionPluginBase` (not `EditorPluginBase` directly). This enforces consistent connection lifecycle — `IsConnected` polling, fire-if-connected at load, `<prefix>.connection_state` push, `<prefix>.get_connection_state` handler, and topic auto-subscribe/unsubscribe on connect/disconnect. See SED-023.
