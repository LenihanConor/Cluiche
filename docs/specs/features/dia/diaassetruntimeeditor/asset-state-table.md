# Feature Spec: Asset State Table

## Traceability

| Level | Spec | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaAssetRuntimeEditor | @docs/specs/systems/dia/diaassetruntimeeditor.md |
| Feature | Asset State Table | (this document) |

## Summary

Live, filterable table displaying all known assets in the running game with their current state (Registered, Staged, Loaded, Unloading), scope (global/stage), ref count, and deploy path. The table auto-refreshes by polling `asset_runtime.get_all_states` at a configurable interval and supports force-refresh, state/ID filtering, row selection, and virtualized rendering for large asset counts.

## Problem

Developers debugging asset lifecycle issues need a single, always-current view of every asset the runtime knows about. Without this table there is no way to see at a glance which assets are loaded, which are stuck in an intermediate state, or what their ref counts are. Manual DiaAPI command invocation is tedious and produces raw JSON that is hard to scan.

## Acceptance Criteria

1. Table displays columns: Asset ID, State, Scope, Ref Count, Deploy Path.
2. Each row is color-coded by state: green = Loaded, yellow = Staged, grey = Registered, red = Unloading.
3. Table polls `asset_runtime.get_all_states` at a configurable interval (default 1 second).
4. A force-refresh button triggers an immediate poll regardless of the interval timer.
5. Filter by state via a dropdown (All, Registered, Staged, Loaded, Unloading).
6. Filter by asset ID via a substring search text field.
7. Filters compose (state AND ID substring both applied simultaneously).
8. Table is virtualized -- only visible rows are rendered to handle thousands of assets without UI lag.
9. Clicking a row selects the asset; selection is published so Ref Count Inspector can observe it.
10. When the game connection is inactive, the entire panel is disabled/greyed with a "Not connected" message (SD-ARED-005).
11. On reconnect, the table automatically resumes polling and displays fresh data.
12. Poll interval is persisted in the plugin's `.context.json` session file (SED-021).

## API Design

### DiaAPI Commands Used

| Command | Direction | Purpose |
|---------|-----------|---------|
| `asset_runtime.get_all_states` | Editor -> Game (via WebSocket) | Full snapshot of all assets with state, scope, ref count, deploy path |

### WebSocket Subscription

The panel subscribes to the `"game_connection"` topic from `GameConnectionController` to know when the connection is active or lost. No `subscribe_transitions` needed -- this panel uses polling, not push.

### CEF Panel

A single dockable CEF panel named "Asset State Table" rendered within the DiaAssetRuntimeEditor plugin area. Contains:
- Toolbar row: state filter dropdown, ID search field, poll interval spinner, force-refresh button.
- Virtualized table body with sortable column headers.
- Status bar showing last refresh timestamp and total asset count (filtered / total).

## Tasks

| # | Task | Description |
|---|------|-------------|
| 1 | Define AssetStateTablePanel C++ class | Implements the panel lifecycle (OnActivate, OnDeactivate, OnUpdate). Owns the poll timer and sends `get_all_states` commands via `GameConnectionManager`. |
| 2 | Parse get_all_states response | Deserialize JSON response into an internal `DynamicArrayC<AssetStateRow>` structure (asset ID as StringCRC, state enum, scope enum, ref count, deploy path string). |
| 3 | Implement connection state handling | Subscribe to `"game_connection"` topic. Disable panel and show "Not connected" overlay when disconnected. Re-enable and trigger immediate poll on reconnect. |
| 4 | Build CEF table UI | HTML/JS panel with virtualized table rendering. Columns: Asset ID, State (with color badge), Scope, Ref Count, Deploy Path. Sortable headers. |
| 5 | Implement filter controls | State dropdown filter and asset ID substring search. Filters applied client-side in the CEF panel after data is received from C++ bridge. |
| 6 | Implement row selection and publish | Click handler on table rows that stores selected asset ID and notifies other panels (Ref Count Inspector, Stage/Asset Tree View) via the plugin's shared selection state. |
| 7 | Persist poll interval in session context | Read/write poll interval from `.context.json` on load/save via SED-021 session context API. |
| 8 | Add unit tests | Test JSON parsing, filter logic, connection state transitions, poll timer reset on reconnect. |

## Dependencies

| Dependency | What this feature uses |
|------------|----------------------|
| DiaEditor | `IEditorPlugin`, `EditorModel`, `GameConnectionManager`, `GameConnectionController` (connection topic subscription) |
| DiaUICEF | CEF panel rendering |
| DiaDebugServer | WebSocket gateway for `asset_runtime.get_all_states` command |
| DiaAssetRuntime (Feature 6) | Registers the `asset_runtime.get_all_states` DiaAPI command in the game process |

## Files

| File | Action |
|------|--------|
| `Dia/DiaAssetRuntimeEditor/Panels/AssetStateTablePanel.h` | Create |
| `Dia/DiaAssetRuntimeEditor/Panels/AssetStateTablePanel.cpp` | Create |
| `Dia/DiaAssetRuntimeEditor/Panels/AssetStateRow.h` | Create |
| `Dia/DiaAssetRuntimeEditor/UI/asset-state-table.html` | Create |
| `Dia/DiaAssetRuntimeEditor/UI/asset-state-table.js` | Create |
| `Dia/DiaAssetRuntimeEditor/UI/asset-state-table.css` | Create |
| `Cluiche/Tests/UnitTests/DiaAssetRuntimeEditor/AssetStateTableTests.cpp` | Create |

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-001 | StringCRC for all IDs | Compliant -- asset IDs stored and displayed as StringCRC `type.name` format |
| PD-004 / AD-002 | No STL in public APIs | Compliant -- C++ public API uses `DynamicArrayC<AssetStateRow>`, not `std::vector`. CEF bridge uses JSON strings. |
| PD-005 | x64 Windows only | Compliant -- no cross-platform code |
| PD-007 | C++20 required | Compliant -- compiled under `/std:c++20` |
| PD-008 | Directory.Build.props owns build settings | Compliant -- no per-project build setting overrides |
| PD-009 | Generated output under Cluiche/out/ | Compliant -- session context written to `Cluiche/out/CluicheEditor/DiaAssetRuntimeEditor/` |
| AD-001 | Module system with YAML frontmatter | Compliant -- module doc will be created with DiaAssetRuntimeEditor system |
| AD-003 | Namespace Dia::\<Module\>:: | Compliant -- all code under `Dia::AssetRuntime::Editor::` |
| SD-ARED-001 | Separate from DiaAssetCatalogueEditor | Compliant -- no DiaAssetCatalogue dependency |
| SD-ARED-002 | Read-only -- no mutation | Compliant -- only sends query commands, never load/unload |
| SD-ARED-003 | Communication via DiaAPI over WebSocket | Compliant -- uses `asset_runtime.get_all_states` via WebSocket |
| SD-ARED-004 | Output to Cluiche/out/CluicheEditor/DiaAssetRuntimeEditor/ | Compliant -- session context path follows this rule |
| SD-ARED-005 | All panels disabled when disconnected | Compliant -- panel greyed with "Not connected" message on disconnect |
| SED-009 | Undo/redo | Not applicable -- read-only panel, no mutations to undo |
| SED-015 | DiaEditor is pure C++ library, no DiaApplicationFlow dependency | Compliant -- implements IEditorPlugin, no DiaApplicationFlow types |
| SED-020 | Plugin output to Cluiche/out/CluicheEditor/\<PluginName\>/ | Compliant -- SD-ARED-004 |
| SED-021 | Per-plugin session context via .context.json | Compliant -- poll interval persisted in .context.json |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Performance | Will polling every 1 second cause performance issues with thousands of assets? | No. `get_all_states` returns a single JSON snapshot. The game-side serialization cost is bounded by asset count, and typical games have hundreds to low thousands of assets. The 1s default is configurable if needed. Virtualized table rendering means the UI cost is constant regardless of asset count. |
| 2 | Consistency | Can the table show stale data between polls? | Yes, by design. The table shows a snapshot that refreshes every N seconds. State transitions between polls are captured by the State Transition Log (Feature 4). The force-refresh button lets developers get an immediate snapshot when needed. |
| 3 | Selection | How is the selected asset communicated to other panels? | Via the plugin's shared state in `DiaAssetRuntimeEditor::GetPluginData()`. The selection is a single `StringCRC` asset ID. Ref Count Inspector and Tree View observe this value. No cross-plugin communication needed since all panels belong to the same plugin. |
| 4 | Reconnection | What happens to the table on reconnect? | The panel triggers an immediate poll on reconnect (bypass the interval timer), clears stale data, and displays the fresh snapshot. The "Not connected" overlay is removed. |
| 5 | Filtering | Should filters persist across sessions? | Yes. Filters are part of the view state persisted in `.context.json` per SED-021. On load, the last-used state filter and ID search are restored. |
| 6 | Scale | Is virtualized rendering sufficient for 10,000+ assets? | Yes. Only visible rows are rendered (typically 20-50 depending on panel height). Filtering further reduces the working set. DOM element count stays constant. |
| 7 | Sort | Should the table support column sorting? | Yes. Columns are sortable by clicking headers (toggle asc/desc). Default sort is by Asset ID ascending. Sort state is part of the view state but not persisted in session context (resets to default on load). |

## Visual Reference

[mockups/diaassetruntimeeditor.html](mockups/diaassetruntimeeditor.html) — **Asset State Table** panel (top-left). Shows filterable table with state/scope/ref count columns, color-coded state badges, and row selection. Use as the visual acceptance gate after implementation.

## Status

`Approved`
