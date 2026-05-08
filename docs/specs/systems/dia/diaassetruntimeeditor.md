# System Spec: DiaAssetRuntimeEditor

## Parent Application
@docs/specs/applications/dia.md

## Asset System Context
@docs/specs/systems/dia/asset-system-overview.md

## Summary

DiaAssetRuntimeEditor is a live debugging editor plugin that connects to a running game instance via DiaDebugServer's WebSocket connection and displays the current state of DiaAssetRuntime. It shows which assets are loaded, staged, unloading, or registered; which Stages are active; ref counts for global assets; and state transition history.

DiaAssetRuntimeEditor is read-only — it observes runtime state but does not mutate it. It is a separate system from DiaAssetCatalogueEditor (which is a build-time manifest authoring tool with no live game connection).

DiaAssetRuntimeEditor answers: "what is the game's asset state right now?"

## Responsibilities

**Owns:**
- Live asset state display — per-asset state (Null, Staged, Loading, Loaded, Failed, Unloaded) updated in real-time via WebSocket subscription
- Stage/Asset tree view — shows which Stages are loaded and each Stage's member assets with current state
- Global asset ref count display — shows ref count per global asset and which Stages hold references
- State transition log — scrollable history of state transitions (asset ID, old state → new state, timestamp)
- Asset search/filter — filter by state, by Stage, or by asset ID substring
- Session context — persists last connection target and view state per SED-021

**Does NOT own:**
- Any mutation of runtime state — read-only observer
- Asset content loading or path resolution — that is DiaAssetRuntime
- Manifest authoring — that is DiaAssetCatalogueEditor
- WebSocket connection management — uses DiaEditor's `GameConnectionManager`
- Debug server or command registration — DiaAssetRuntime Feature 6 registers the DiaAPI commands; DiaDebugServer exposes them

## Public Interfaces

### Plugin Implementation

```cpp
namespace Dia::AssetRuntime::Editor
{
    class DiaAssetRuntimeEditor : public Dia::Editor::IEditorPlugin
    {
    public:
        const char* GetName() const override { return "DiaAssetRuntimeEditor"; }
        const char* GetVersion() const override { return "1.0.0"; }
        const char* GetDescription() const override {
            return "Live asset runtime state inspector";
        }
        const char* GetUIPath() const override;
        Dia::Editor::LayoutMode GetLayoutMode() const override {
            return Dia::Editor::LayoutMode::kDockable;
        }

        void OnLoad(Dia::Editor::EditorModel* model) override;
        void OnUnload() override;
        void OnUpdate(float deltaTime) override;
        void* GetPluginData() override;
    };
}
```

### DiaAPI Commands Consumed (via WebSocket)

These commands are registered by DiaAssetRuntime Feature 6 in the game process. DiaAssetRuntimeEditor queries them via the DiaDebugServer WebSocket gateway.

| Command | Response |
|---------|----------|
| `asset_runtime.get_loaded` | Array of asset IDs currently in `Loaded` state |
| `asset_runtime.get_staged` | Array of asset IDs currently in `Staged` state |
| `asset_runtime.get_loading` | Array of asset IDs currently in `Loading` state |
| `asset_runtime.get_failed` | Array of asset IDs currently in `Failed` state (with error info) |
| `asset_runtime.get_state { assetId }` | State, scope, ref count, deploy path for one asset |
| `asset_runtime.get_stage_deps { stageId }` | Asset IDs for a loaded Stage |
| `asset_runtime.get_all_states` | Full snapshot: all assets with state, scope, ref count |
| `asset_runtime.subscribe_transitions` | Stream of state transition events (asset ID, old→new state, timestamp) |

### UI Panels (CEF)

| Panel | Description |
|-------|-------------|
| Asset State Table | Filterable table of all known assets — ID, state, scope, ref count, deploy path. Color-coded by state (Loading=yellow, Loaded=green, Failed=red, Staged=grey). |
| Stage/Asset Tree | Hierarchical tree: Stages → Assets. Expand a Stage to see its member assets and their states. Loading/Failed indicators prominent. |
| Ref Count Inspector | Selected global asset's ref count breakdown — which Stages hold references. |
| Transition Log | Scrollable, filterable log of state transitions. Newest at top. Pause/resume. Failed transitions highlighted. |

## Dependencies

| Dependency | What DiaAssetRuntimeEditor uses from it |
|------------|----------------------------------------|
| DiaEditor | `IEditorPlugin`, `EditorPluginRegistry`, `EditorModel`, `GameConnectionManager` (WebSocket client to running game), `GameConnectionController` connection state subscription |
| DiaUICEF | CEF rendering of all editor UI panels |
| DiaDebugServer | WebSocket gateway — DiaAssetRuntimeEditor sends DiaAPI commands and receives responses/subscriptions through this connection |
| DiaAssetRuntime | Defines the DiaAPI commands (Feature 6) that this editor queries. No direct C++ dependency — communication is via JSON over WebSocket. |

**Connection state dependency:** DiaAssetRuntimeEditor subscribes to the `"game_connection"` topic (same one that drives the toolbar's green/red connection indicator, per Plugin Lifecycle Toolbar feature). All panels are disabled/greyed with a "Not connected — connect to a running game to view asset state" message when the connection is inactive. Panels activate automatically when the connection comes up.

No dependency on DiaAssetCatalogue, DiaAssetPipeline, DiaApplication, or any rendering/windowing module.

## Features

| # | Feature | Size | Description | Spec | Status |
|---|---------|------|-------------|------|--------|
| 1 | Asset State Table | S | Live table of all assets with state, scope, ref count. Filter by state/ID. Auto-refreshes via polling or subscription. | [asset-state-table.md](../../features/dia/diaassetruntimeeditor/asset-state-table.md) | Approved |
| 2 | Stage/Asset Tree View | S | Hierarchical view of loaded Stages → Assets. Expand/collapse. State indicators per node. | [stage-asset-tree-view.md](../../features/dia/diaassetruntimeeditor/stage-asset-tree-view.md) | Approved |
| 3 | Ref Count Inspector | S | Selected global asset shows ref count and which Stages hold references. | [ref-count-inspector.md](../../features/dia/diaassetruntimeeditor/ref-count-inspector.md) | Approved |
| 4 | State Transition Log | S | Scrollable log of state transitions via `asset_runtime.subscribe_transitions`. Pause, resume, filter, clear. | [state-transition-log.md](../../features/dia/diaassetruntimeeditor/state-transition-log.md) | Approved |

**Build order:** 1 → 2 → 3 → 4 (table first — provides the data foundation; tree and inspector add views on top; log is independent but benefits from the table existing)

## Design Constraints

- **Read-only.** DiaAssetRuntimeEditor does not send load/unload commands or mutate runtime state. It is a pure observer.
- **UI only — no business logic.** All data comes from DiaAPI commands over WebSocket. The editor does not interpret or transform the data beyond display formatting.
- **Requires live connection.** Subscribes to `"game_connection"` topic from `GameConnectionController`. All panels are disabled/greyed with "Not connected" message when disconnected. Panels activate automatically when connection comes up. The toolbar's global green/red connection indicator (Plugin Lifecycle Toolbar feature) already shows connection state to the user — this editor reuses the same data source.
- **No DiaAssetCatalogue dependency.** This editor works with DiaAssetRuntime's own types and debug API. It does not need the catalogue manifest or build-time type framework.
- **Debug builds only.** DiaDebugServer (and therefore DiaAPI command access) only runs in Debug builds. This editor is a debug tool.

## Decisions

| ID | Decision | Rationale | Scope | Status | Binding |
|----|----------|-----------|-------|--------|---------|
| SD-ARED-001 | DiaAssetRuntimeEditor is a separate system from DiaAssetCatalogueEditor | Different concerns: catalogue editor is build-time authoring (no game connection); runtime editor is live debugging (no manifest authoring). Separate plugins, separate UI, separate dependencies. | All features | Accepted | Yes |
| SD-ARED-002 | Read-only — no mutation of runtime state from the editor | Observer pattern prevents accidental state corruption. If command-and-control is needed later (e.g. force-unload), it becomes a new feature with explicit safeguards. | All features | Accepted | Yes |
| SD-ARED-003 | Communication via DiaAPI commands over WebSocket, not direct C++ API | Decouples editor from game process. Editor can run in CluicheEditor while game runs in CluicheTest. Same pattern as all other editor plugins. | All features | Accepted | Yes |
| SD-ARED-004 | Output written to `Cluiche/out/CluicheEditor/DiaAssetRuntimeEditor/` | Per SED-020 — all generated editor output is per-plugin under `Cluiche/out/CluicheEditor/` | All features | Accepted | Yes |
| SD-ARED-005 | All panels require live game connection — disabled when disconnected | This editor has no offline mode. Subscribes to `"game_connection"` topic (same source as toolbar's green/red indicator). Panels grey out with "Not connected" message and activate automatically on reconnect. | All features | Accepted | Yes |

## Inherited Binding Decisions

| ID | Decision | Source | Implication for DiaAssetRuntimeEditor |
|----|----------|--------|-----------------------------------------|
| PD-001 | StringCRC for all entity/component IDs | Platform | Asset IDs displayed in the editor use StringCRC `type.name` format. |
| PD-004 | No STL containers in public APIs | Platform | Plugin's C++ public interface uses DiaCore containers. CEF bridge uses JSON strings. |
| PD-005 | x64 Windows only | Platform | No cross-platform considerations. |
| PD-006 | Visual Studio project files are source of truth | Platform | DiaAssetRuntimeEditor.vcxproj maintained manually. |
| PD-007 | C++20 required | Platform | Compliant. |
| PD-008 | Directory.Build.props owns OutDir | Platform | DiaAssetRuntimeEditor.vcxproj inherits centralized build settings. |
| PD-009 | Generated output under Cluiche/out/ | Platform | Session context and logs written to `Cluiche/out/CluicheEditor/DiaAssetRuntimeEditor/` per SD-ARED-004. |
| AD-001 | Module system with YAML frontmatter | Application | `dia.assetruntimeeditor.architecture.module.md` created with this system. |
| AD-002 | No STL in public APIs | Application | Same as PD-004. |
| AD-003 | Namespace: Dia::\<Module\>:: | Application | All code under `Dia::AssetRuntime::Editor::` namespace. |
| SED-009 | Undo/redo via IEditorCommand + CommandHistory | DiaEditor | Not applicable — read-only editor, no mutations to undo. |
| SED-015 | DiaEditor is a pure C++ library, no DiaApplication dependency | DiaEditor | DiaAssetRuntimeEditor implements IEditorPlugin; no DiaApplication types used. |
| SED-020 | Plugin output to `Cluiche/out/CluicheEditor/<PluginName>/` | DiaEditor | Output path is `Cluiche/out/CluicheEditor/DiaAssetRuntimeEditor/`. SD-ARED-004. |
| SED-021 | Per-plugin session context via `.context.json` | DiaEditor | Session context (last connection target, panel layout, filters) persisted in `.context.json`. |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Connection | What happens when the game disconnects mid-session? | All panels grey out with "Disconnected" and last-known timestamp. Transition log preserves history (read-only). `GameConnectionManager` handles reconnection — when connection resumes, panels reactivate and fetch fresh state. The toolbar's global red/green indicator (Plugin Lifecycle Toolbar) already signals this to the user. |
| 2 | Performance | How often does the editor poll for state updates? | Transition log uses `subscribe_transitions` (push, not poll). Asset state table polls `get_all_states` at a configurable interval (default 1s). User can force-refresh. |
| 3 | Scale | What if there are thousands of assets? | Asset state table is virtualized (only renders visible rows). Filters reduce the working set. Tree view is lazy-loaded (expand to fetch children). |
| 4 | Scope | Should this editor show catalogue metadata (tags, type, source path) alongside runtime state? | No — this editor shows runtime state only (state, scope, ref count, deploy path). Catalogue metadata is DiaAssetCatalogueEditor's domain. Keeping them separate avoids a DiaAssetCatalogue dependency. |
| 5 | Future | Should the editor support force-loading or force-unloading assets for debugging? | Deferred (SD-ARED-002). Read-only for v1. If added later, it would be a new feature with explicit safeguards and confirmation dialogs. |

## Status

`Approved` — all 4 feature specs written and approved, ready for implementation
