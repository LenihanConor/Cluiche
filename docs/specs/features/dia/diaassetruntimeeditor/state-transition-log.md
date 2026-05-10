# Feature Spec: State Transition Log

## Traceability

| Level | Spec | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaAssetRuntimeEditor | @docs/specs/systems/dia/diaassetruntimeeditor.md |
| Feature | State Transition Log | (this document) |

## Summary

Scrollable, filterable log panel that displays real-time asset state transitions received via the `asset_runtime.subscribe_transitions` WebSocket stream. Supports pause/resume, clear, asset ID and state transition filtering, configurable max entry count, and preserves log history across disconnections with timestamped markers.

## Problem

The Asset State Table shows current state snapshots, but developers debugging asset lifecycle issues need to see the sequence of state transitions over time: when an asset moved from Staged to Loaded, whether it was unloaded and re-loaded, or if transitions are happening in an unexpected order. A persistent, filterable log of transitions provides the temporal dimension that snapshot views cannot.

## Acceptance Criteria

1. Panel subscribes to `asset_runtime.subscribe_transitions` WebSocket stream on connection.
2. Each log entry displays: timestamp, asset ID, old state, new state (e.g., "14:32:05.123 | texture.player | Staged -> Loaded").
3. Newest entries appear at the top (reverse chronological order).
4. Pause button stops auto-scroll and freezes the visible log position; new entries continue to accumulate but do not scroll the view.
5. Resume button re-enables auto-scroll and jumps to the newest entry.
6. Clear button removes all log entries from the display.
7. Filter by asset ID substring (text field) -- only matching entries shown.
8. Filter by specific state transition (dropdown, e.g., "Staged -> Loaded", "Any -> Unloading", "All transitions").
9. Filters apply to both existing log entries and new incoming entries.
10. Log persists during the session -- survives panel resize, tab switch, and panel minimize/restore.
11. On disconnect: log is preserved as read-only history. A "Disconnected at \<time\>" marker entry is appended. Existing entries remain visible and filterable.
12. On reconnect: a "Reconnected at \<time\>" marker entry is appended. New transition events resume appending above the marker.
13. Max log entries configurable (default 1000). When the limit is reached, the oldest entries are dropped (FIFO overflow).
14. Max entry count is persisted in the plugin's `.context.json` session file (SED-021).
15. When the game connection is inactive, the panel is disabled/greyed with a "Not connected" overlay (SD-ARED-005), but preserved log history remains visible beneath the overlay.

## API Design

### DiaAPI Commands Used

| Command | Direction | Purpose |
|---------|-----------|---------|
| `asset_runtime.subscribe_transitions` | Editor -> Game (via WebSocket) | Subscribes to a push stream of state transition events |

### WebSocket Subscription

Two subscriptions are active:
1. `"game_connection"` topic from `GameConnectionController` -- for enable/disable behavior and disconnect/reconnect markers.
2. `asset_runtime.subscribe_transitions` -- the actual transition event stream. This is a long-lived subscription that pushes events as they occur in the game process.

Each transition event arrives as a JSON message: `{ "assetId": "texture.player", "oldState": "Staged", "newState": "Loaded", "timestamp": 1714732325123 }`.

### CEF Panel

A dockable CEF panel named "Transition Log" rendered within the DiaAssetRuntimeEditor plugin area. Contains:
- Toolbar row: asset ID filter field, transition type dropdown, max entries spinner, Pause/Resume toggle button, Clear button.
- Scrollable log area with entries in reverse chronological order.
- Marker entries (disconnect/reconnect) styled distinctly (e.g., grey background, italic text).
- Status bar showing entry count (visible / total) and pause state indicator.

## Tasks

| # | Task | Description |
|---|------|-------------|
| 1 | Define StateTransitionLogPanel C++ class | Panel lifecycle class. Manages the subscription to `subscribe_transitions`, maintains the log buffer (`DynamicArrayC<TransitionLogEntry>`), and handles connection state. |
| 2 | Implement transition event parsing | Deserialize each incoming JSON transition event into a `TransitionLogEntry` struct (timestamp, asset ID as StringCRC, old state enum, new state enum). Append to the log buffer with FIFO overflow. |
| 3 | Implement connection state handling | Subscribe to `"game_connection"` topic. On disconnect: append "Disconnected" marker, stop subscription. On reconnect: append "Reconnected" marker, re-subscribe to `subscribe_transitions`. |
| 4 | Implement pause/resume logic | Pause flag in C++ class. When paused, new entries still accumulate in the buffer but the CEF panel does not auto-scroll. Resume re-enables auto-scroll and jumps to the newest entry. |
| 5 | Build CEF log UI | HTML/JS panel with scrollable log area. Each entry rendered as a row with timestamp, asset ID, old->new state with color coding. Marker entries styled distinctly. Reverse chronological order. |
| 6 | Implement filter controls | Asset ID substring filter applied client-side. Transition type dropdown with options: All, and each specific transition pair (Registered->Staged, Staged->Loaded, Loaded->Unloading, etc.). Filters apply to both visible and incoming entries. |
| 7 | Implement FIFO overflow and clear | Enforce max entry limit by dropping oldest entries when new ones arrive and the buffer is full. Clear button empties the buffer entirely. |
| 8 | Persist max entry count in session context | Read/write max entry limit from `.context.json` on load/save via SED-021 session context API. |
| 9 | Add unit tests | Test event parsing, FIFO overflow (oldest dropped), disconnect/reconnect markers, pause accumulation, filter matching, clear behavior. |

## Dependencies

| Dependency | What this feature uses |
|------------|----------------------|
| DiaEditor | `IEditorPlugin`, `EditorModel`, `GameConnectionManager`, `GameConnectionController` |
| DiaUICEF | CEF panel rendering |
| DiaDebugServer | WebSocket gateway for `asset_runtime.subscribe_transitions` stream |
| DiaAssetRuntime (Feature 6) | Registers the `asset_runtime.subscribe_transitions` DiaAPI command and broadcasts transition events |

## Files

| File | Action |
|------|--------|
| `Dia/DiaAssetRuntimeEditor/Panels/StateTransitionLogPanel.h` | Create |
| `Dia/DiaAssetRuntimeEditor/Panels/StateTransitionLogPanel.cpp` | Create |
| `Dia/DiaAssetRuntimeEditor/Panels/TransitionLogEntry.h` | Create |
| `Dia/DiaAssetRuntimeEditor/UI/state-transition-log.html` | Create |
| `Dia/DiaAssetRuntimeEditor/UI/state-transition-log.js` | Create |
| `Dia/DiaAssetRuntimeEditor/UI/state-transition-log.css` | Create |
| `Cluiche/Tests/UnitTests/DiaAssetRuntimeEditor/StateTransitionLogTests.cpp` | Create |

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-001 | StringCRC for all IDs | Compliant -- asset IDs in log entries stored as StringCRC |
| PD-004 / AD-002 | No STL in public APIs | Compliant -- log buffer uses `DynamicArrayC<TransitionLogEntry>`. CEF bridge uses JSON. |
| PD-005 | x64 Windows only | Compliant |
| PD-007 | C++20 required | Compliant |
| PD-008 | Directory.Build.props owns build settings | Compliant -- no per-project overrides |
| PD-009 | Generated output under Cluiche/out/ | Compliant -- session context at `Cluiche/out/CluicheEditor/DiaAssetRuntimeEditor/` |
| AD-001 | Module system with YAML frontmatter | Compliant -- covered by DiaAssetRuntimeEditor module doc |
| AD-003 | Namespace Dia::\<Module\>:: | Compliant -- `Dia::AssetRuntime::Editor::` |
| SD-ARED-001 | Separate from DiaAssetCatalogueEditor | Compliant -- no DiaAssetCatalogue dependency |
| SD-ARED-002 | Read-only -- no mutation | Compliant -- only subscribes to transition events, never sends commands that modify state |
| SD-ARED-003 | Communication via DiaAPI over WebSocket | Compliant -- uses `subscribe_transitions` via WebSocket |
| SD-ARED-004 | Output to Cluiche/out/CluicheEditor/DiaAssetRuntimeEditor/ | Compliant -- session context path |
| SD-ARED-005 | All panels disabled when disconnected | Compliant -- panel greyed with "Not connected" overlay, but preserved history remains visible |
| SED-009 | Undo/redo | Not applicable -- read-only log, no mutations to undo |
| SED-015 | DiaEditor is pure C++ library, no DiaApplicationFlow dependency | Compliant |
| SED-020 | Plugin output to Cluiche/out/CluicheEditor/\<PluginName\>/ | Compliant |
| SED-021 | Per-plugin session context via .context.json | Compliant -- max entry count persisted in .context.json |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Performance | Can a burst of transitions overwhelm the log? | The FIFO overflow cap (default 1000) bounds memory usage. The CEF panel only renders visible entries (scrollable viewport), so DOM cost is constant. A very fast burst may cause entries to roll off quickly, but the cap is configurable -- developers can increase it for heavy debugging sessions. |
| 2 | Reliability | What if the subscribe_transitions stream drops messages? | WebSocket is TCP-based, so messages are ordered and reliable on localhost. If the connection drops entirely, the disconnect marker captures the gap. On reconnect, the log resumes from new events -- there is no replay of missed transitions (this is acceptable for a debug tool). |
| 3 | UX | Why show history after disconnect instead of clearing? | Disconnect often happens at the interesting moment (crash, hang). Preserving the log lets developers inspect what happened leading up to the disconnection. The disconnect marker makes it clear which entries are historical vs. live. |
| 4 | Filtering | Should filter state persist across sessions? | No. Filters are transient UI state. Persisting them risks confusion (e.g., opening the editor and seeing an empty log because a narrow filter from a previous session is still active). Only the max entry count, which affects buffer behavior, is persisted. |
| 5 | Pause behavior | Should pause stop receiving events or just stop scrolling? | Just stop scrolling. Events continue to accumulate in the buffer. This prevents data loss -- the developer can resume and see everything that happened while paused. If pause stopped receiving, transitions during the pause window would be lost forever. |
| 6 | Markers | Could disconnect/reconnect markers flood the log during flaky connections? | Each marker is a single entry. Even with frequent reconnections, markers are lightweight. The FIFO cap applies to markers as well, so they cannot grow unbounded. In practice, rapid reconnection cycles would produce alternating marker pairs that are easy to spot and filter. |
| 7 | Integration | Does this panel interact with the Asset State Table selection? | Not directly. Clicking a log entry does not select the asset in the table (the log shows transitions, not current state). However, a future enhancement could add "click to select" on log entries. For v1, the log is an independent view. |

## Visual Reference

[mockups/diaassetruntimeeditor.html](mockups/diaassetruntimeeditor.html) — **State Transition Log** panel (bottom-right). Shows timestamped entries with old/new state badges, pause/resume/clear controls, and filter bar. Toggle "Simulate disconnect" checkbox to see the disconnected overlay (SD-ARED-005). Use as the visual acceptance gate after implementation.

## Status

`Approved`
