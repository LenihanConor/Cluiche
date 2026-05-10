# Feature Spec: Stage/Asset Tree View

## Traceability

| Level | Spec | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaAssetRuntimeEditor | @docs/specs/systems/dia/diaassetruntimeeditor.md |
| Feature | Stage/Asset Tree View | (this document) |

## Summary

Hierarchical tree panel showing loaded Stages as root nodes with their member assets as children. Each node displays a color-coded state indicator. Children are lazy-loaded on expand via `asset_runtime.get_stage_deps`, and clicking an asset node selects it, synchronizing with the Asset State Table's selection.

## Problem

The Asset State Table shows a flat list of all assets, but developers often need to understand the Stage-level grouping: which Stages are loaded, which assets belong to each Stage, and whether a global asset is referenced by multiple Stages. A hierarchical view makes Stage ownership and asset membership immediately visible without manual filtering or cross-referencing.

## Acceptance Criteria

1. Root nodes represent loaded Stages, derived from the `get_all_states` response (assets grouped by their stage scope).
2. Expanding a Stage node fetches its member assets via `asset_runtime.get_stage_deps { stageId }`.
3. Children are lazy-loaded -- the `get_stage_deps` call is only made when a Stage node is expanded for the first time (or after data is invalidated by a new poll cycle).
4. Each asset node displays a colored dot matching its state (green = Loaded, yellow = Staged, grey = Registered, red = Unloading).
5. Stage nodes show the count of member assets in parentheses (e.g., "MainStage (42)").
6. Global assets appear under a synthetic "[Global]" root node and show their total ref count.
7. Expand/collapse state is preserved across poll refreshes (nodes stay expanded if they were expanded before the refresh).
8. Clicking an asset node selects it and synchronizes the selection with the Asset State Table and Ref Count Inspector.
9. The tree view refreshes its root nodes on each poll cycle from the Asset State Table's data (reuses the same `get_all_states` response, no separate polling).
10. When the game connection is inactive, the panel is disabled/greyed with a "Not connected" message (SD-ARED-005).
11. On reconnect, the tree clears and rebuilds from the first fresh `get_all_states` snapshot.

## API Design

### DiaAPI Commands Used

| Command | Direction | Purpose |
|---------|-----------|---------|
| `asset_runtime.get_all_states` | Editor -> Game (via WebSocket) | Root node construction -- identifies which Stages exist and global assets (reuses Asset State Table's poll, no duplicate request) |
| `asset_runtime.get_stage_deps { stageId }` | Editor -> Game (via WebSocket) | Fetches asset IDs belonging to a specific Stage on expand |

### WebSocket Subscription

Subscribes to `"game_connection"` topic from `GameConnectionController` for enable/disable behavior. Does not independently poll -- it reacts to data already fetched by the Asset State Table panel.

### CEF Panel

A dockable CEF panel named "Stage/Asset Tree" rendered within the DiaAssetRuntimeEditor plugin area. Contains:
- Tree control with expand/collapse toggles on Stage nodes.
- Each asset node: colored state dot + asset ID text.
- Each Stage node: stage name + member count badge.
- "[Global]" synthetic root node for global-scope assets.

## Tasks

| # | Task | Description |
|---|------|-------------|
| 1 | Define StageAssetTreePanel C++ class | Panel lifecycle class. Subscribes to the shared `get_all_states` snapshot from Asset State Table (no independent polling). Sends `get_stage_deps` commands on node expand. |
| 2 | Build root node list from snapshot | Parse `get_all_states` response to identify unique Stages and group global assets under a synthetic "[Global]" node. Preserve expand/collapse state across refreshes. |
| 3 | Implement lazy child loading | On first expand of a Stage node, send `asset_runtime.get_stage_deps { stageId }` and populate children from the response. Cache results until the next poll cycle invalidates them. |
| 4 | Implement connection state handling | Subscribe to `"game_connection"` topic. Disable panel and show "Not connected" overlay when disconnected. Clear tree and rebuild on reconnect. |
| 5 | Build CEF tree UI | HTML/JS panel with collapsible tree nodes. State dot color per asset node. Stage node expand/collapse with member count badge. |
| 6 | Implement selection sync | Clicking an asset node updates the plugin's shared selection state (same mechanism as Asset State Table row click). Selection changes from Asset State Table are reflected as a highlighted node in the tree. |
| 7 | Add unit tests | Test root node extraction from snapshot, lazy load triggering, expand/collapse state preservation, global asset grouping, selection sync. |

## Dependencies

| Dependency | What this feature uses |
|------------|----------------------|
| DiaEditor | `IEditorPlugin`, `EditorModel`, `GameConnectionManager`, `GameConnectionController` |
| DiaUICEF | CEF panel rendering |
| DiaDebugServer | WebSocket gateway for `asset_runtime.get_stage_deps` command |
| DiaAssetRuntime (Feature 6) | Registers the `asset_runtime.get_stage_deps` DiaAPI command |
| Asset State Table (Feature 1) | Reuses the `get_all_states` snapshot data and shared selection state |

## Files

| File | Action |
|------|--------|
| `Dia/DiaAssetRuntimeEditor/Panels/StageAssetTreePanel.h` | Create |
| `Dia/DiaAssetRuntimeEditor/Panels/StageAssetTreePanel.cpp` | Create |
| `Dia/DiaAssetRuntimeEditor/UI/stage-asset-tree.html` | Create |
| `Dia/DiaAssetRuntimeEditor/UI/stage-asset-tree.js` | Create |
| `Dia/DiaAssetRuntimeEditor/UI/stage-asset-tree.css` | Create |
| `Cluiche/Tests/UnitTests/DiaAssetRuntimeEditor/StageAssetTreeTests.cpp` | Create |

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-001 | StringCRC for all IDs | Compliant -- Stage IDs and asset IDs stored as StringCRC |
| PD-004 / AD-002 | No STL in public APIs | Compliant -- C++ API uses DiaCore containers. CEF bridge uses JSON. |
| PD-005 | x64 Windows only | Compliant -- no cross-platform code |
| PD-007 | C++20 required | Compliant |
| PD-008 | Directory.Build.props owns build settings | Compliant -- no per-project overrides |
| PD-009 | Generated output under Cluiche/out/ | Compliant -- no additional output beyond shared session context |
| AD-001 | Module system with YAML frontmatter | Compliant -- covered by DiaAssetRuntimeEditor module doc |
| AD-003 | Namespace Dia::\<Module\>:: | Compliant -- `Dia::AssetRuntime::Editor::` |
| SD-ARED-001 | Separate from DiaAssetCatalogueEditor | Compliant -- no DiaAssetCatalogue dependency |
| SD-ARED-002 | Read-only -- no mutation | Compliant -- only sends query commands |
| SD-ARED-003 | Communication via DiaAPI over WebSocket | Compliant -- uses `get_stage_deps` via WebSocket |
| SD-ARED-004 | Output to Cluiche/out/CluicheEditor/DiaAssetRuntimeEditor/ | Compliant |
| SD-ARED-005 | All panels disabled when disconnected | Compliant -- panel greyed with "Not connected" message |
| SED-009 | Undo/redo | Not applicable -- read-only panel |
| SED-015 | DiaEditor is pure C++ library, no DiaApplicationFlow dependency | Compliant |
| SED-020 | Plugin output to Cluiche/out/CluicheEditor/\<PluginName\>/ | Compliant |
| SED-021 | Per-plugin session context via .context.json | Compliant -- expand/collapse state could be persisted but is not required (resets on load) |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Performance | Does lazy loading create a visible delay when expanding a Stage node? | The `get_stage_deps` call returns a small array of asset IDs (tens to hundreds). Round-trip over localhost WebSocket is sub-millisecond. A "loading..." placeholder is shown during the fetch, but the delay should be imperceptible in practice. |
| 2 | Data freshness | Can the tree show stale children after a poll refresh? | Children are invalidated on each poll cycle. If a Stage node is expanded and the poll brings new data, the children are re-fetched on the next expand toggle or automatically refreshed if the node is already expanded. This ensures children stay consistent with the root node data. |
| 3 | Global assets | Why use a synthetic "[Global]" node instead of showing global assets at root level? | Consistency. Every asset appears under a parent node (either a Stage or "[Global]"), making the tree structure uniform. It also visually separates global assets from stage-scoped ones, which is the key information this panel provides. |
| 4 | Selection sync | Can selection desync between the tree and the Asset State Table? | No. Both panels write to and read from the same shared selection state in `DiaAssetRuntimeEditor::GetPluginData()`. A selection change from either panel is immediately visible to the other. |
| 5 | Reconnection | Should expand/collapse state be preserved across reconnect? | No. On reconnect, the Stage topology may have changed (different Stages loaded). The tree rebuilds from scratch. This is safer than trying to match old expand state to a potentially different set of Stages. |
| 6 | Duplicate assets | Can an asset appear under multiple Stage nodes? | A stage-scoped asset belongs to exactly one Stage. A global asset appears under "[Global]" only, with its ref count indicating how many Stages reference it. The Ref Count Inspector (Feature 3) shows the detailed per-Stage breakdown. |

## Visual Reference

[mockups/diaassetruntimeeditor.html](mockups/diaassetruntimeeditor.html) — **Stage / Asset Tree** panel (top-right). Shows expandable Stage nodes with child asset nodes, state dots, global asset section, and asset count per stage. Use as the visual acceptance gate after implementation.

## Status

`Approved`
