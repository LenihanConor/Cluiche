# Feature Spec: pipeline-panel-ui

## Parent System
@docs/specs/systems/dia/diapipelineeditor.md

## Status
`Done`

## Summary

React/TypeScript dockable panel that renders the live pipeline state inside CluicheEditor. Receives parsed `PipelineEvent` data from `PipelineLogTailer` via CEF message passing and renders a stage timeline with colour-coded status icons, expandable drill-down into log lines per stage, elapsed time, and a run summary header. This is the primary visual surface of DiaPipelineEditor.

## Problem

Developers currently have no visibility into pipeline progress from the editor — they switch to a terminal and watch raw output scroll. The editor should show pipeline state in a structured, glanceable panel that updates in real time without requiring context-switching.

## Goals

- Dockable panel registered as an IEditorPlugin with `LayoutMode::kDockable`
- Real-time updates as events arrive from PipelineLogTailer via `DiaEditor_onDataChanged("pipeline.event", ...)`
- Stage timeline: vertical list of stages with status icons, elapsed time, expandable
- Stage drill-down: expand a stage to see its log lines (`OnLogLine` events), colour-coded by level
- Run summary header: target, config, overall pass/fail counts, total elapsed time
- Empty state when no run data exists yet
- Interrupted run shown with a distinct visual indicator
- Log lines also forwarded to a "Pipeline" tab in the Output Console for continuous stream view

## Non-Goals

- Build triggering UI (toolbar, dropdowns, "Build" button) — that's the build-trigger feature
- Run history browsing — that's the run-history feature
- Editing pipeline.toml — out of scope for this system entirely

## Acceptance Criteria

| # | Criterion |
|---|-----------|
| AC1 | Panel registers as a dockable IEditorPlugin via `REGISTER_EDITOR_PLUGIN` macro |
| AC2 | Panel subscribes to `pipeline.event` topic via `DiaEditor_onDataChanged` and updates on each new event |
| AC3 | Stage timeline renders one row per stage with status icon: `▶` running (blue), `✓` passed (green), `✗` failed (red), `○` not started (grey) |
| AC4 | Each stage row shows elapsed time (updating live while running, final on completion) |
| AC5 | Clicking a stage row expands it to show log lines (`OnLogLine` events) for that stage |
| AC6 | Log lines are colour-coded by level: info (default), warn (yellow), error (red) |
| AC7 | Run summary header shows target name, config, pass/fail counts, total elapsed time |
| AC8 | When no run data exists, panel shows an empty state message ("No pipeline run yet") |
| AC9 | Interrupted runs show a distinct indicator (e.g. warning icon + "interrupted" label on the unfinished stage) |
| AC10 | Panel re-renders correctly when a new run starts (previous state clears, fresh timeline appears) |
| AC11 | Log lines are also forwarded to a "Pipeline" tab in the Output Console for continuous sequential viewing |

## UI Layout

```
┌─────────────────────────────────────────────┐
│  Pipeline                          00:05.1  │  ← RunSummary header
│  googletest · Debug · 3 passed              │
├─────────────────────────────────────────────┤
│  ✓ proto-compile                     0.8s   │  ← stage row (collapsed)
│  ▶ compile-code                      4.2s   │  ← stage row (running)
│  ├ [info] Building GoogleTests.vcxproj...   │  ← expanded log lines
│  ├ [info] msbuild GoogleTests.vcxproj ...   │
│  └ [error] C2065: undeclared identifier     │
│  ○ asset-build                         —    │  ← not started
│  ○ package                             —    │
└─────────────────────────────────────────────┘
```

## Data Flow

```
PipelineLogTailer (C++)
  → Poll() parses new events
  → Notifies observers
  → C++ bridge pushes events via WebUIBridge::NotifyUIDataChanged("pipeline.event", eventJson)
  → CEF delivers to JS via DiaEditor_onDataChanged({topic: "pipeline.event", data: eventJson})
  → React component receives event, dispatches to useReducer
  → StageTimeline / StageDetail re-render
```

### React State Shape

```typescript
interface PipelineState {
  runInProgress: boolean;
  target: string;
  config: string;
  passCount: number;
  failCount: number;
  totalDurationMs: number;
  interrupted: boolean;
  stages: StageState[];
}

interface StageState {
  name: string;
  status: 'not-started' | 'running' | 'passed' | 'failed' | 'interrupted';
  durationMs: number;
  logLines: LogLine[];
  expanded: boolean;
}

interface LogLine {
  level: 'info' | 'warn' | 'error' | 'debug';
  message: string;
  timestamp: number;
}
```

### Event Reducer

The `useReducer` maps incoming `PipelineEvent` JSON to state transitions:

| Event | State change |
|-------|-------------|
| `OnRunStarted` | Reset state; populate target, config, stage list (all `not-started`) |
| `OnStageStarted` | Set stage status to `running` |
| `OnStageCompleted` | Set stage status to `passed`; set durationMs |
| `OnStageFailed` | Set stage status to `failed`; set durationMs |
| `OnLogLine` | Append to matching stage's `logLines` |
| `OnRunCompleted` | Set `runInProgress = false`; update passCount, failCount, totalDurationMs |
| `OnRunFailed` | Same as OnRunCompleted but with failure data |

## Implementation

### Files introduced

```
Dia/DiaPipelineEditor/
├── UI/
│   ├── src/
│   │   ├── App.tsx                    # Root component
│   │   ├── components/
│   │   │   ├── PipelinePanel.tsx      # Main panel container
│   │   │   ├── RunSummary.tsx         # Header bar (target, config, counts, time)
│   │   │   ├── StageTimeline.tsx      # Vertical stage list
│   │   │   ├── StageRow.tsx           # Single stage row (icon, name, time, expand)
│   │   │   ├── StageDetail.tsx        # Expanded log lines for a stage
│   │   │   └── EmptyState.tsx         # "No pipeline run yet" placeholder
│   │   ├── state/
│   │   │   ├── pipelineReducer.ts     # useReducer logic for event → state
│   │   │   └── types.ts              # PipelineState, StageState, LogLine
│   │   └── hooks/
│   │       └── usePipelineEvents.ts   # Subscribe to pipeline.event topic
│   ├── package.json
│   ├── tsconfig.json
│   └── vite.config.ts
├── PipelineEditorPlugin.h             # IEditorPlugin implementation
└── PipelineEditorPlugin.cpp           # OnLoad wires up PipelineLogTailer → WebUIBridge push
```

### C++ bridge (PipelineEditorPlugin)

The plugin's `OnUpdate` calls `PipelineLogTailer::Poll()`. When the tailer's observer fires, the plugin serialises new events to JSON and pushes them via `WebUIBridge::NotifyUIDataChanged("pipeline.event", eventJson)`.

## Dependencies

| Dependency | Type | What is used |
|------------|------|-------------|
| ndjson-tailer | Feature (same system) | PipelineLogTailer provides parsed events |
| DiaEditor | Framework | IEditorPlugin, REGISTER_EDITOR_PLUGIN, WebUIBridge |
| DiaUICEF | Rendering | CEF panel hosting, message transport |
| DiaCore | Foundation | StringCRC, Observer, Json |
| React + TypeScript | External | UI component framework |
| Vite | External (dev) | Build tooling for the React app |

## Acceptance Criteria Tests

| AC | Test approach |
|----|--------------|
| AC1 | Integration test: verify plugin appears in EditorPluginRegistry after macro registration |
| AC2 | Unit test (JS): mock `DiaEditor_onDataChanged`, send event, verify state update |
| AC3 | Unit test (JS): send stage events, verify correct status icons rendered |
| AC4 | Unit test (JS): send OnStageStarted + OnStageCompleted, verify elapsed time display |
| AC5 | Unit test (JS): click stage row, verify log lines panel appears |
| AC6 | Unit test (JS): send OnLogLine with different levels, verify colour classes |
| AC7 | Unit test (JS): send OnRunStarted + stages, verify header shows target/config/counts |
| AC8 | Unit test (JS): render with empty state, verify placeholder message |
| AC9 | Unit test (JS): set interrupted=true on a stage, verify warning indicator |
| AC10 | Unit test (JS): send two OnRunStarted in sequence, verify state resets on second |

## Traceability

| Level | Spec | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaPipelineEditor | @docs/specs/systems/dia/diapipelineeditor.md |

## Binding Decisions Compliance

All inherited decisions per system spec. Feature-specific compliance:

| ID | Source | Decision | Compliance |
|----|--------|----------|------------|
| SED-002 | DiaEditor | Macro-based plugin registration | `REGISTER_EDITOR_PLUGIN(PipelineEditorPlugin, "DiaPipelineEditor")` |
| SPE-003 | DiaPipelineEditor | CEF message passing, not WebSocket | Uses `WebUIBridge::NotifyUIDataChanged` |

## AI Review Questions

| # | Section | Question | Suggested Default | Answer |
|---|---------|----------|-------------------|--------|
| 1 | Event batching | Should the C++ bridge push events one-by-one or batch per frame? | Batch per frame — collect all events from a single Poll() into a JSON array, push once. Reduces CEF IPC overhead. | Batch per frame. Single JSON array push per Poll() cycle. |
| 2 | Live timer | How should the "running" stage show a ticking elapsed time? | JS-side `setInterval(100ms)` updates display using `Date.now() - stageStartTimestamp`. Stops when Completed/Failed arrives. | JS-side `setInterval(100ms)`. Switches to final `durationMs` from event on completion. |
| 3 | Max log lines | Should stage drill-down cap the number of visible log lines? | Show all log lines; virtualise the list if >100 lines (react-window or similar) to keep DOM light. | Show all; virtualise if >100 lines. Also forward log lines to the Output Console's "Pipeline" tab for continuous stream view. |
| 4 | Colour theme | Should colours match DiaCLI terminal colours or use editor theme? | Editor theme — CSS variables for status colours so they adapt to light/dark mode. | Editor theme. CSS variables for status/level colours; adapts to light/dark mode. |
| 5 | Panel sizing | Default panel size and position? | 300px wide, docked right. User can resize/move per DiaEditor docking layout. | 300px wide, docked right. Resizable/movable via react-mosaic. |

## Open Questions

(none)
