# Plan: DiaApplicationEditor

**Spec:** @docs/specs/systems/dia/diaapplicationeditor.md  
**Status:** In Progress  
**Started:** 2026-04-23  
**Last Updated:** 2026-04-23

## Prerequisites

DiaApplicationEditor cannot start until these systems exist. They are tracked in their own plans.

| Prerequisite | Why needed | Status |
|---|---|---|
| DiaEditor framework | IEditorPlugin, EditorModel, EditorPluginRegistry, GameConnectionManager, WebUIBridge | Not Started |
| DiaUICEF | CEF embedding — required for web UI to render | Not Started |
| DiaWebSocket | WebSocket client — required for GameConnectionManager | Not Started |
| CluicheEditor (skeleton) | Host process that loads and runs the plugin | Not Started |

Implementation of DiaApplicationEditor should begin once the DiaEditor plugin interface (`IEditorPlugin`, `EditorModel`, `EditorPluginRegistry`) and the CluicheEditor skeleton are in place. Full UI features require DiaUICEF; live debugging features require DiaWebSocket.

## Implementation Phases

### Phase 1 — Plugin Skeleton
Establish the plugin class and project structure before any features land.

| # | Task | Status | Notes |
|---|------|--------|-------|
| 1.1 | Create `Dia/DiaApplicationEditor/` directory and `.vcxproj` | Done | StaticLibrary; depends on DiaEditor + DiaCore; added to solution and CluicheEditor |
| 1.2 | Implement `DiaApplicationEditor` class stub (IEditorPlugin, REGISTER_EDITOR_PLUGIN macro) | Done | OnLoad/OnUnload/OnUpdate shells; placeholder `Assets/index.html` UI |
| 1.3 | Wire plugin into CluicheEditor `.diaapp` manifest | Done | Added to `test-editor-plugins.diaapp`; assets copied to `diaapplicationeditor/` at post-build |

### Phase 2 — Static Editing Core
All features that work without a connected game.

| # | Task | Status | Notes |
|---|------|--------|-------|
| 2.1 | **Manifest Load/Save** | Done | OpenManifest/SaveManifest/SaveManifestAs/CloseManifest; ManifestSerializer for IR→JSON; .bak backup; dirty tracking; validation gate on save |
| 2.2 | **Type Discovery** | Done | TypeCache + TypeInfo in ManifestEditorData; known_types.json bundled asset; LoadStaticTypeList() on OnLoad; RefreshAvailableTypes() stub ready for Phase 3 live query |
| 2.3 | **Real-time Validation** | Done | Bridge event handlers registered in OnLoad (validate, open/save/close_manifest, refresh_types); ValidateManifest pushes full JSON payload; auto-validate on load; useValidation 500ms debounce hook; ValidationPanel; ManifestStore; standalone Vite+React build setup for plugin UI |
| 2.4 | **Tree View** | Done | TreeView.tsx + TreeNodeRenderer.tsx (react-arborist); add/remove/reorder handlers in C++ (HandleAddNode/HandleRemoveNode/HandleReorderNode); manifest_loaded pushes JSON to UI; ManifestStore.manifest field; App.tsx shows tree when manifest loaded |
| 2.5 | **View Toggle** | Done | Toolbar.tsx (Save + toggle button, Ctrl+T); currentView/isDirty/toggleView in ManifestStore; FlowView.tsx placeholder; manifest_updated C++ push with is_dirty; localStorage pref for last view |
| 2.6 | **Flow View** | Done | FlowView.tsx (reactflow + @dagrejs/dagre); PhaseNode.tsx color-coded by type; dagre auto-layout; PU group nodes; transitions as edges; phase_positions_changed C++ handler persists positions into phase.config.editor_position |
| 2.7 | **Phase Transition Editor** | Done | FlowView.tsx extended: onConnect drag-to-connect (no self-loop/duplicate), Delete key + context menu, double-click label prompt; C++ HandleTransitionAdded/Removed (ParseTransitionIds helper) |
| 2.8 | **Module Config Editor** | Done | ModuleInspector.tsx (Form/JSON toggle, 500ms debounce); FormView.tsx (bool/number/string fields); jsonLinter.ts (CodeMirror lint); @uiw/react-codemirror; C++ HandleModuleConfigChanged; inspector panel shown in App.tsx when module node selected |
| 2.9 | **File Conflict Detection** | Done | DiaCore::FileWatcher (polling, Update() called from OnUpdate); StartWatchingFile/StopWatchingFile; auto-reload if clean, conflict banner if dirty; HandleResolveConflict (keep_local/reload_disk); mIsSaving guard; ConflictBanner.tsx with side-by-side diff |

### Phase 2b — Static Editing Enhancements
Additional static editing features (no game connection required).

| # | Task | Status | Notes |
|---|------|--------|-------|
| 2b.1 | **Undo/Redo** | Not Started | @docs/specs/features/dia/diaapplicationeditor/undo-redo.md — 8-entry snapshot stack; Ctrl+Z/Y; resets on view switch |
| 2b.2 | **PU/Phase Inspector** | Not Started | @docs/specs/features/dia/diaapplicationeditor/pu-phase-inspector.md — reuse inspector panel; PU fields (frequency_hz, dedicated_thread, initial_phase dropdown); Phase fields (type_id, config, is-initial checkbox) |
| 2b.3 | **Editable Lifecycle Grid** | Not Started | @docs/specs/features/dia/diaapplicationeditor/editable-lifecycle-grid.md — click cell to toggle module↔phase; instant apply; last-phase guard |
| 2b.4 | **Import Management** | Not Started | @docs/specs/features/dia/diaapplicationeditor/import-management.md — resolve imports, show with badges, inline editing, multi-file save, add/remove imports, cycle detection |
| 2b.5 | **Validation Navigation** | Not Started | @docs/specs/features/dia/diaapplicationeditor/validation-navigation.md — click error → select + scroll to node |
| 2b.6 | **New Manifest** | Not Started | @docs/specs/features/dia/diaapplicationeditor/new-manifest.md — Ctrl+N blank manifest (1 PU, 1 Phase); Save As for untitled |

### Phase 3 — Live Debugging
All features that require a WebSocket connection to a running game. Requires DiaWebSocket to be available.

| # | Task | Status | Notes |
|---|------|--------|-------|
| 3.1 | **WebSocket Connection** | Not Started | @docs/specs/features/dia/diaapplicationeditor/websocket-connection.md — foundation for all Phase 3 features |
| 3.2 | **Runtime State Inspector** | Not Started | @docs/specs/features/dia/diaapplicationeditor/runtime-state-inspector.md — requires connection (3.1) |
| 3.3 | **Live Phase Visualization** | Not Started | @docs/specs/features/dia/diaapplicationeditor/live-phase-visualization.md — requires Flow View (2.6) + connection (3.1) |
| 3.4 | **Hot Reload Trigger** | Not Started | @docs/specs/features/dia/diaapplicationeditor/hot-reload-trigger.md — requires connection (3.1) |
| 3.5 | **Runtime Config Push** | Not Started | @docs/specs/features/dia/diaapplicationeditor/runtime-config-push.md — requires connection (3.1) + Module Config Editor (2.8) |
| 3.6 | **Risky Change Warnings** | Not Started | @docs/specs/features/dia/diaapplicationeditor/risky-change-warnings.md — requires connection (3.1); UI layer on top of 3.4 and 3.5 |
| 3.7 | Type Discovery — game registry path | Not Started | Extend 2.2 to query live game registry when connected |

## Feature Spec Status Summary

| Feature | Phase | Spec Status |
|---------|-------|-------------|
| Manifest Load/Save | 2.1 | Approved |
| Type Discovery | 2.2 / 3.7 | Approved |
| Real-time Validation | 2.3 | Approved |
| Tree View | 2.4 | Approved |
| View Toggle | 2.5 | Approved |
| Flow View | 2.6 | Approved |
| Phase Transition Editor | 2.7 | Approved |
| Module Config Editor | 2.8 | Approved |
| File Conflict Detection | 2.9 | Approved |
| Undo/Redo | 2b.1 | Approved |
| PU/Phase Inspector | 2b.2 | Approved |
| Editable Lifecycle Grid | 2b.3 | Approved |
| Import Management | 2b.4 | Approved |
| Validation Navigation | 2b.5 | Approved |
| New Manifest | 2b.6 | Approved |
| WebSocket Connection | 3.1 | Approved |
| Runtime State Inspector | 3.2 | Approved |
| Live Phase Visualization | 3.3 | Approved |
| Hot Reload Trigger | 3.4 | Approved |
| Runtime Config Push | 3.5 | Approved |
| Risky Change Warnings | 3.6 | Approved |

## Session Notes

### 2026-04-23
- Plan created. All prerequisites confirmed present in codebase (DiaEditor, DiaUICEF, DiaWebSocket, CluicheEditor).
- Resolved location ambiguity: `Dia/DiaApplicationEditor/` (top-level peer). SED-003 updated in DiaEditor spec to match.
- Phase 1 complete: plugin skeleton created, registered, wired into CluicheEditor and test manifest.
- Task 2.1 complete: ManifestEditorData, ManifestSerializer, full load/save/validate on DiaApplicationEditor. Empty ApplicationTypeRegistry used for offline structural validation; type-aware validation deferred to Phase 3 Type Discovery.
- Task 2.2 complete: TypeCache + TypeInfo; known_types.json; LoadStaticTypeList on OnLoad.
- Task 2.3 complete: ValidationPanel; ManifestStore; useValidation debounce hook; standalone Vite+React UI build.
- Fixed Clear()/PushBack() → RemoveAll()/Add() across ManifestEditorData.h and DiaApplicationEditor.cpp (DynamicArrayC has no Clear/PushBack aliases).
- Task 2.4 complete: TreeView.tsx + TreeNodeRenderer.tsx using react-arborist; C++ add/remove/reorder bridge handlers; manifest_loaded now pushes serialized manifest JSON; ManifestStore.manifest field; App.tsx conditional tree/placeholder rendering.
- Tasks 2.5–2.9 complete: View Toggle (Toolbar + Ctrl+T + localStorage pref), Flow View (reactflow + dagre), Phase Transition Editor (drag-to-connect + Delete/context menu), Module Config Editor (CodeMirror + Form View + 500ms debounce), File Conflict Detection (FileWatcher + ConflictBanner).
- Phase 2 complete. All static editing features implemented.
