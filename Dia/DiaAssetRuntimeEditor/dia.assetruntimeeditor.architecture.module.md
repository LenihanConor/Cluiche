---
schema: dia.module.v1
module_id: dia.assetruntimeeditor
name: DiaAssetRuntimeEditor
owner_team: TBD
layer: platform
status: active
maturity: dev

path: Dia/DiaAssetRuntimeEditor
language: cpp
parent_module_id: dia

summary: >
  Live debugging editor plugin that connects to a running game instance via WebSocket
  and displays the current state of DiaAssetRuntime.

intent: >
  Provides a CluicheEditor plugin implementing IEditorPlugin that observes runtime asset
  state over a live WebSocket connection. Shows which assets are loaded, staged, unloading,
  or registered; which Stages are active; ref counts for global assets; and state transition
  history. Read-only — does not mutate runtime state.

responsibilities:
  - Live asset state display (per-asset state updated via WebSocket subscription)
  - Stage/Asset tree view (Stages and their member assets with current state)
  - Global asset ref count display (which Stages hold references)
  - State transition log (scrollable history of state changes)
  - Asset search/filter (by state, Stage, or asset ID substring)
  - Session context persistence (.context.json per SED-021)

non_responsibilities:
  - Any mutation of runtime state — read-only observer
  - Asset content loading or path resolution — that is DiaAssetRuntime
  - Manifest authoring — that is DiaAssetCatalogueEditor
  - WebSocket connection management — uses DiaEditor's GameConnectionManager
  - Debug server or command registration — DiaAssetRuntime Feature 6

dependent_modules:
  - dia.editor
  - dia.core
  - dia.logger

public_api:
  headers:
    - DiaAssetRuntimeEditor/DiaAssetRuntimeEditorPlugin.h
  namespaces:
    - Dia::AssetRuntime::Editor
  entry_points:
    - DiaAssetRuntimeEditorPlugin

dependencies:
  required:
    - dia.editor
    - dia.core
    - dia.logger
  forbidden:
    - dia.assetruntime
    - dia.assetcatalogue
    - dia.application
---
