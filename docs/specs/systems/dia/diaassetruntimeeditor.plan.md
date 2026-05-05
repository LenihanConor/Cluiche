# Plan: DiaAssetRuntimeEditor

**Spec:** @docs/specs/systems/dia/diaassetruntimeeditor.md  
**Status:** In Progress  
**Started:** 2026-05-05  
**Last Updated:** 2026-05-05

## Prerequisites

- DiaAssetRuntime Feature 6 (DiaAPI Debug Commands) — Done
- DiaEditor plugin infrastructure (IEditorPlugin, EditorPluginRegistry, GameConnectionManager) — exists
- DiaUICEF — exists
- DiaDebugServer WebSocket gateway — exists

## Tasks

| # | Task | Status | Model | Notes |
|---|------|--------|-------|-------|
| 0 | Project scaffolding | Done | sonnet | Created Dia/DiaAssetRuntimeEditor/ directory, .vcxproj, .vcxproj.filters, module architecture doc, solution integration |
| 1 | Plugin shell class | Done | sonnet | DiaAssetRuntimeEditorPlugin.h/.cpp implementing IEditorPlugin, REGISTER_EDITOR_PLUGIN macro, connect/disconnect handlers |
| 2 | Shared selection state | Done | sonnet | SharedPluginState struct with selected asset ID (StringCRC) and connection state. Expanded in later tasks. |
| 3 | F1-T1: AssetStateTablePanel class | Done | sonnet | Panel lifecycle, poll timer, sends get_all_states via GameConnectionManager |
| 4 | F1-T2: Parse get_all_states response | Done | sonnet | ParseGetAllStatesResponse static method, DynamicArrayC<AssetStateRow, 4096> |
| 5 | F1-T3: Connection state handling | Done | sonnet | OnConnectionStateChanged wires disable/enable, immediate poll on reconnect |
| 6 | F1-T4: CEF table UI | Done | sonnet | HTML/JS/CSS panel with color-coded state badges, sortable columns |
| 7 | F1-T5: Filter controls | Done | sonnet | State dropdown + ID substring search, client-side composable filters |
| 8 | F1-T6: Row selection and publish | Done | sonnet | Click handler updates SharedPluginState.mSelectedAssetId via bridge |
| 9 | F1-T7: Persist poll interval | Not Started | haiku | Read/write poll interval from .context.json |
| 10 | F1-T8: Unit tests | Not Started | sonnet | JSON parsing, filter logic, connection state transitions, poll timer reset |
| 11 | F2-T1: StageAssetTreePanel class | Not Started | sonnet | Panel lifecycle, subscribes to shared get_all_states snapshot, sends get_stage_deps on expand |
| 12 | F2-T2: Root node list from snapshot | Not Started | sonnet | Parse snapshot to identify unique Stages, group globals under "[Global]" node, preserve expand/collapse state |
| 13 | F2-T3: Lazy child loading | Not Started | sonnet | On first expand send get_stage_deps, cache results until next poll cycle invalidates |
| 14 | F2-T4: Connection state handling | Not Started | haiku | Subscribe to "game_connection", disable on disconnect, clear+rebuild on reconnect |
| 15 | F2-T5: CEF tree UI | Not Started | sonnet | HTML/JS collapsible tree, state dots, member count badges, [Global] section |
| 16 | F2-T6: Selection sync | Not Started | sonnet | Click asset node updates SharedPluginState, highlight node when selection changes externally |
| 17 | F2-T7: Unit tests | Not Started | sonnet | Root node extraction, lazy load triggering, expand/collapse preservation, global grouping, selection sync |
| 18 | F3-T1: RefCountInspectorPanel class | Not Started | sonnet | Panel lifecycle, observes shared selection + get_all_states snapshot |
| 19 | F3-T2: Reference resolution | Not Started | sonnet | For selected global asset, scan all Stages' dep lists (from get_stage_deps cache or fresh query) to find holders |
| 20 | F3-T3: Handle stage-scoped assets | Not Started | haiku | Detect stage-scoped asset, show "single reference from <stage>" message |
| 21 | F3-T4: Connection state handling | Not Started | haiku | Disable on disconnect, re-check selected asset on reconnect |
| 22 | F3-T5: CEF inspector UI | Not Started | sonnet | HTML/JS panel with asset header, scope/ref count summary, Stage reference list |
| 23 | F3-T6: Unit tests | Not Started | sonnet | Reference resolution (multi-stage, zero-stage, stage-scoped, reconnect with missing asset) |
| 24 | F4-T1: StateTransitionLogPanel class | Not Started | sonnet | Panel lifecycle, manages subscribe_transitions subscription, log buffer with FIFO overflow |
| 25 | F4-T2: Transition event parsing | Not Started | sonnet | Deserialize JSON events into TransitionLogEntry (timestamp, asset ID StringCRC, old/new state enums) |
| 26 | F4-T3: Connection state handling | Not Started | sonnet | Disconnect marker, stop subscription; reconnect marker, re-subscribe |
| 27 | F4-T4: Pause/resume logic | Not Started | sonnet | Pause flag, events accumulate but no auto-scroll, resume jumps to newest |
| 28 | F4-T5: CEF log UI | Not Started | sonnet | HTML/JS scrollable log, reverse chronological, color-coded state badges, marker entries |
| 29 | F4-T6: Filter controls | Not Started | sonnet | Asset ID substring + transition type dropdown, applied to existing + incoming entries |
| 30 | F4-T7: FIFO overflow and clear | Not Started | haiku | Enforce max entry limit, drop oldest on overflow, clear button empties buffer |
| 31 | F4-T8: Persist max entry count | Not Started | haiku | Read/write from .context.json |
| 32 | F4-T9: Unit tests | Not Started | sonnet | Event parsing, FIFO overflow, markers, pause accumulation, filter matching, clear |
| 33 | Integration test: full plugin lifecycle | Not Started | sonnet | Load plugin, simulate connection, verify all 4 panels activate/deactivate correctly |
| 34 | Visual acceptance gate | Not Started | opus | Compare running editor against HTML mockup, verify all acceptance criteria |

## Build Order & Dependencies

```
Task 0 (scaffolding)
  └─ Task 1 (plugin shell)
       └─ Task 2 (shared state)
            ├─ Feature 1: Tasks 3-10 (Asset State Table)
            │    └─ Feature 2: Tasks 11-17 (Stage/Asset Tree)
            │         └─ Feature 3: Tasks 18-23 (Ref Count Inspector)
            └─ Feature 4: Tasks 24-32 (State Transition Log) — can parallel with F2/F3
```

Feature 4 (Transition Log) has no data dependency on Features 2/3 — it only depends on the plugin shell and shared connection state. It can be built in parallel with Features 2 and 3 after Feature 1 is complete.

## Key Design Decisions for Implementation

1. **SharedPluginState** — Single struct exposed via `GetPluginData()`. Contains: selected asset ID (`StringCRC`), last `get_all_states` snapshot (`DynamicArrayC<AssetStateRow>`), stage deps cache (`HashTable<StringCRC, DynamicArrayC<StringCRC>>`), connection state flag. All panels read/write from this.

2. **No duplicate polling** — Only AssetStateTablePanel polls `get_all_states`. Other panels observe the snapshot from SharedPluginState. StageAssetTreePanel issues `get_stage_deps` only on expand, caches results.

3. **CEF bridge pattern** — Follow existing GameConnectionEditorPlugin model: C++ panel classes push data to CEF via WebUIBridge JSON messages. CEF panels send user actions (click, filter change) back via registered handlers.

4. **Project structure:**
   ```
   Dia/DiaAssetRuntimeEditor/
   ├── DiaAssetRuntimeEditorPlugin.h/.cpp
   ├── SharedPluginState.h
   ├── Panels/
   │   ├── AssetStateTablePanel.h/.cpp
   │   ├── AssetStateRow.h
   │   ├── StageAssetTreePanel.h/.cpp
   │   ├── RefCountInspectorPanel.h/.cpp
   │   ├── StateTransitionLogPanel.h/.cpp
   │   └── TransitionLogEntry.h
   ├── UI/
   │   ├── asset-state-table.html/.js/.css
   │   ├── stage-asset-tree.html/.js/.css
   │   ├── ref-count-inspector.html/.js/.css
   │   └── state-transition-log.html/.js/.css
   └── dia.assetruntimeeditor.architecture.module.md
   ```

5. **vcxproj dependencies:** DiaEditor, DiaCore, DiaLogger (static lib references). No direct dependency on DiaAssetRuntime — communication is JSON over WebSocket only.

## Session Notes

### 2026-05-05
- Plan created from approved system spec and 4 approved feature specs
- All DiaAssetRuntime prerequisites (Features 1-6) are complete
- HTML mockup exists at docs/specs/features/dia/diaassetruntimeeditor/mockups/diaassetruntimeeditor.html
- Existing plugin patterns examined: DiaAssetCatalogueEditor, GameConnectionEditorPlugin, DiaPipelineEditor
- Tasks 0-2 completed: project scaffolding, plugin shell, SharedPluginState
- Build verified: CluicheEditor compiles and links with DiaAssetRuntimeEditor
- Plugin registers via REGISTER_EDITOR_PLUGIN, owns a GameConnectionManager, exposes connect/disconnect request handlers
- Next: Task 3 (AssetStateTablePanel) — the first real feature panel
