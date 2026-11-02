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
    - dia.core.ui  # WebUIBridge depends on IJSBridge only, not the domain-tier dia.ui
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
    - DiaEditor/Project/GameScopeDiscovery.h
    - DiaEditor/Plugin/AssetRoutes.h
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

**Game-scoped plugin discovery:** Game-scoped types must be tagged before any game loads, so the cold-start state is out of scope. `GameScopeDiscovery::DiscoverAndTag(roots, maxDepth, gamePaths)` scans roots for `*.diagame` (depth-limited), unions explicit game paths, de-duplicates by `NormalizeGamePath`, and calls `TagGameScoped` without activating a scope. `CollectGameScope` is the single collection path shared with `EditorModel::LoadDiagameProject`, so the game id matches. Missing roots and malformed files are logged and skipped. CluicheEditor calls it from `EditorModelModule::DoStart` with `editor.cluicheproj` `game_roots` (default `assets` next to the exe), `last_project` and `recent_projects`.

**Game-scoped asset routes:** A plugin that should own an asset type only while loaded (AC4 of game-scoped plugins) calls `AssetRoutes::RegisterAssetRoute(bridge, assetType, editorPluginType)` in `OnLoad` and `UnregisterAssetRoute` in `OnUnload` (`DiaEditor/Plugin/AssetRoutes.h`). These invoke the `asset_catalogue.register_type_editor` / `asset_catalogue.unregister_type_editor` bridge handlers (request `{assetType, editorPluginType}`; responses carry `registered` / `removed`). If DiaAssetCatalogueEditor is not loaded the call is a no-op (Info log, returns false); the route is still held in `EditorPluginRegistry` (`AddAssetRoute` / `RemoveAssetRoute`, fixed capacity `kMaxAssetRoutes`, exact-pair duplicates rejected) and the catalogue plugin replays every held route in `OnPluginLoad`, after seeding built-ins, so load order does not matter. Existing routes (including built-ins) are never overridden, and unregister only removes a route mapped to the same editor. With no route, `asset_catalogue.open_asset` falls back to the OS default app.

**Live connection plugins:** Any plugin that observes the game connection MUST derive from `LiveConnectionPluginBase` (not `EditorPluginBase` directly). This enforces consistent connection lifecycle — `IsConnected` polling, fire-if-connected at load, `<prefix>.connection_state` push, `<prefix>.get_connection_state` handler, and topic auto-subscribe/unsubscribe on connect/disconnect. See SED-023.
