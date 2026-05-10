# Feature Spec: Ref Count Inspector

## Traceability

| Level | Spec | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaAssetRuntimeEditor | @docs/specs/systems/dia/diaassetruntimeeditor.md |
| Feature | Ref Count Inspector | (this document) |

## Summary

Detail panel that activates when a global-scope asset is selected (from the Asset State Table or Stage/Asset Tree View). Displays the asset's ID, current ref count, scope, and a list of which Stages hold references. For stage-scoped assets, shows a single-reference summary instead. Updates on each poll cycle.

## Problem

Global assets are shared across multiple Stages, and their ref count determines when they can be safely unloaded. When debugging asset leaks or unexpected unload behavior, developers need to see exactly which Stages hold references to a global asset. The Asset State Table shows the total ref count but not the per-Stage breakdown. The Ref Count Inspector fills this gap by cross-referencing stage dependency data to identify all holders.

## Acceptance Criteria

1. Panel activates when a global-scope asset is selected via the shared selection state (from Asset State Table or Tree View).
2. Displays: Asset ID, current state, scope ("Global"), and total ref count.
3. Lists all Stages that hold a reference to the selected global asset, derived by scanning `get_stage_deps` results for each loaded Stage.
4. For stage-scoped assets: displays "Stage-scoped -- single reference from \<stage name\>" instead of a Stage list.
5. Panel content updates on each poll cycle (when the Asset State Table receives a new `get_all_states` snapshot).
6. When no asset is selected, the panel shows "Select an asset to inspect ref counts."
7. When the game connection is inactive, the panel is disabled/greyed with a "Not connected" message (SD-ARED-005).
8. When the game connection is inactive and no asset is selected, the connection message takes priority.
9. On reconnect, if an asset was previously selected and still exists in the new snapshot, the panel re-populates with updated data. If the asset no longer exists, the panel clears with "Asset no longer present in runtime."

## API Design

### DiaAPI Commands Used

| Command | Direction | Purpose |
|---------|-----------|---------|
| `asset_runtime.get_all_states` | Editor -> Game (via WebSocket) | Provides the selected asset's current state, scope, and ref count (reuses Asset State Table's poll) |
| `asset_runtime.get_stage_deps { stageId }` | Editor -> Game (via WebSocket) | Determines which Stages reference the selected asset (reuses Stage/Asset Tree View's cached results where available) |

The Ref Count Inspector does not issue its own DiaAPI commands. It derives all data from the shared snapshot (Asset State Table) and the Stage dependency cache (Stage/Asset Tree View). When the tree has not yet fetched a Stage's deps, the inspector issues `get_stage_deps` for each loaded Stage to build the reference list.

### WebSocket Subscription

Subscribes to `"game_connection"` topic from `GameConnectionController` for enable/disable behavior.

### CEF Panel

A dockable CEF panel named "Ref Count Inspector" rendered within the DiaAssetRuntimeEditor plugin area. Contains:
- Header: Asset ID and state badge.
- Summary row: Scope label and total ref count.
- Reference list: each row shows a Stage name that holds a reference, with the Stage's own state indicator.
- Empty state: contextual message when no asset selected or asset is stage-scoped.

## Tasks

| # | Task | Description |
|---|------|-------------|
| 1 | Define RefCountInspectorPanel C++ class | Panel lifecycle class. Observes the shared selection state and the `get_all_states` snapshot. Computes the per-Stage reference list. |
| 2 | Implement reference resolution | For a selected global asset, iterate over all loaded Stages and check each Stage's dependency list (from `get_stage_deps` cache or fresh query) to determine which Stages reference the asset. Build a `DynamicArrayC<StageReference>` result. |
| 3 | Handle stage-scoped assets | Detect when the selected asset is stage-scoped (scope field from snapshot). Display simplified "single reference" message instead of the Stage list. |
| 4 | Implement connection state handling | Subscribe to `"game_connection"` topic. Disable panel on disconnect. On reconnect, re-check if the previously selected asset still exists. |
| 5 | Build CEF inspector UI | HTML/JS panel with asset header, scope/ref count summary, and Stage reference list. Color-coded state badges. Contextual empty-state messages. |
| 6 | Add unit tests | Test reference resolution logic (asset in multiple Stages, asset in zero Stages after unload, stage-scoped asset, reconnect with missing asset). |

## Dependencies

| Dependency | What this feature uses |
|------------|----------------------|
| DiaEditor | `IEditorPlugin`, `EditorModel`, `GameConnectionManager`, `GameConnectionController` |
| DiaUICEF | CEF panel rendering |
| DiaDebugServer | WebSocket gateway for `asset_runtime.get_stage_deps` (when cache miss) |
| DiaAssetRuntime (Feature 6) | Registers the `asset_runtime.get_stage_deps` DiaAPI command |
| Asset State Table (Feature 1) | Provides `get_all_states` snapshot and shared selection state |
| Stage/Asset Tree View (Feature 2) | Provides cached `get_stage_deps` results to avoid duplicate WebSocket calls |

## Files

| File | Action |
|------|--------|
| `Dia/DiaAssetRuntimeEditor/Panels/RefCountInspectorPanel.h` | Create |
| `Dia/DiaAssetRuntimeEditor/Panels/RefCountInspectorPanel.cpp` | Create |
| `Dia/DiaAssetRuntimeEditor/UI/ref-count-inspector.html` | Create |
| `Dia/DiaAssetRuntimeEditor/UI/ref-count-inspector.js` | Create |
| `Dia/DiaAssetRuntimeEditor/UI/ref-count-inspector.css` | Create |
| `Cluiche/Tests/UnitTests/DiaAssetRuntimeEditor/RefCountInspectorTests.cpp` | Create |

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-001 | StringCRC for all IDs | Compliant -- asset IDs and Stage IDs stored as StringCRC |
| PD-004 / AD-002 | No STL in public APIs | Compliant -- C++ API uses `DynamicArrayC<StageReference>`. CEF bridge uses JSON. |
| PD-005 | x64 Windows only | Compliant |
| PD-007 | C++20 required | Compliant |
| PD-008 | Directory.Build.props owns build settings | Compliant -- no per-project overrides |
| PD-009 | Generated output under Cluiche/out/ | Compliant |
| AD-001 | Module system with YAML frontmatter | Compliant -- covered by DiaAssetRuntimeEditor module doc |
| AD-003 | Namespace Dia::\<Module\>:: | Compliant -- `Dia::AssetRuntime::Editor::` |
| SD-ARED-001 | Separate from DiaAssetCatalogueEditor | Compliant -- no DiaAssetCatalogue dependency |
| SD-ARED-002 | Read-only -- no mutation | Compliant -- only reads snapshot and stage dep data |
| SD-ARED-003 | Communication via DiaAPI over WebSocket | Compliant -- all data from DiaAPI commands over WebSocket |
| SD-ARED-004 | Output to Cluiche/out/CluicheEditor/DiaAssetRuntimeEditor/ | Compliant |
| SD-ARED-005 | All panels disabled when disconnected | Compliant -- panel greyed with "Not connected" message |
| SED-009 | Undo/redo | Not applicable -- read-only panel |
| SED-015 | DiaEditor is pure C++ library, no DiaApplicationFlow dependency | Compliant |
| SED-020 | Plugin output to Cluiche/out/CluicheEditor/\<PluginName\>/ | Compliant |
| SED-021 | Per-plugin session context via .context.json | Compliant -- selected asset ID not persisted (transient selection) |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Performance | Is scanning all Stages for references expensive? | No. The number of loaded Stages is small (typically 1-5). For each Stage, checking if a given asset ID is in its dependency list is a linear scan over a small set. The stage dep data is cached by the Tree View, so no extra WebSocket calls in the common case. |
| 2 | Accuracy | Can the reference list be stale? | It refreshes on each poll cycle. Between polls, a Stage could load or unload an asset reference without the inspector knowing. This is acceptable -- the polling interval (default 1s) provides near-real-time accuracy, and the State Transition Log captures the exact moment of change. |
| 3 | Edge case | What if a global asset has ref count 0? | This is a valid transient state (asset is about to be unloaded). The inspector shows ref count 0 and an empty Stage list. The state should transition to Unloading shortly after, which the Asset State Table will reflect on the next poll. |
| 4 | UX | Should the inspector show the deploy path? | No. The Asset State Table already shows the deploy path. The inspector focuses specifically on the ref count breakdown, which is the information the flat table cannot provide. Duplicating deploy path would add clutter without value. |
| 5 | Reconnection | What if the selected asset disappears after reconnect? | The inspector detects this by checking the new `get_all_states` snapshot. If the asset ID is not present, the panel shows "Asset no longer present in runtime" and clears the reference list. The shared selection state is cleared so other panels also deselect. |

## Visual Reference

[mockups/diaassetruntimeeditor.html](mockups/diaassetruntimeeditor.html) — **Ref Count Inspector** panel (bottom-left). Shows selected global asset with ref count, scope, deploy path, and per-Stage reference breakdown. Use as the visual acceptance gate after implementation.

## Status

`Approved`
