# Plan: pipeline-panel-ui

**Spec:** @docs/specs/features/dia/diapipelineeditor/pipeline-panel-ui.md  
**Status:** Done  
**Started:** 2026-04-27  
**Last Updated:** 2026-04-27

## Implementation Patterns

### C++ bridge update (PipelineEditorPlugin)

The plugin already calls `Poll()` each frame. To push events to JS:
- `PipelineEditorPlugin` implements `Dia::Core::Observer`
- On `OnLoad`: registers itself as observer on the tailer
- `ObserverNotification(subject, message)`: serializes new events to JSON array, calls `mBridge->NotifyUIDataChanged("pipeline.event", payload)`
- Payload shape: `{ "events": [ { event, system, stage, step, ts, durationMs, error, detail, level }, ... ], "summary": { target, config, passCount, failCount, totalDurationMs, interrupted } }`
- StringCRC fields serialized via `.AsChar()` (returns the original string)
- On `OnUnload`: unregisters observer before shutting down tailer

### React/TS project scaffolding

Follows DiaApplicationEditor pattern:
- `Dia/DiaPipelineEditor/UI/package.json` — React 18, Vite 4, TypeScript 5, vitest; no d3/reactflow/zustand (useReducer is sufficient)
- `Dia/DiaPipelineEditor/UI/vite.config.ts` — `root: 'src'`, `outDir: '../dist'`, vitest with jsdom
- `Dia/DiaPipelineEditor/UI/tsconfig.json` — ES2020, react-jsx, strict
- `Dia/DiaPipelineEditor/UI/src/index.html` — dark theme base, `#root` div, `<script src="/main.tsx">`
- `Dia/DiaPipelineEditor/UI/src/main.tsx` — `createRoot(getElementById('root')).render(<App />)`

### Bridge communication

- C++ → JS: `WebUIBridge::NotifyUIDataChanged("pipeline.event", json)` delivers via `window.DiaEditor_onDataChanged({topic, data})`
- JS receives: `window.addEventListener('message', handler)` checking `event.data?.__dia`
- JS → C++ (future build-trigger feature): `window.parent.postMessage({__diaFromFrame: true, payload: {type, data}}, '*')`

### State management (useReducer)

- `PipelineState`, `StageState`, `LogLine` interfaces per spec
- Reducer maps incoming event JSON to state transitions per the spec's Event Reducer table
- `usePipelineEvents` hook subscribes to `pipeline.event` messages, dispatches to reducer

### Components

- `EmptyState` — "No pipeline run yet" placeholder (AC8)
- `RunSummary` — Header bar: target, config, pass/fail, total time (AC7)
- `StageTimeline` — Vertical list of StageRow components (AC3)
- `StageRow` — Single stage: icon + name + elapsed time, click to expand (AC3, AC4, AC5)
- `StageDetail` — Expanded log lines for a stage, colour-coded by level (AC5, AC6)
- `PipelinePanel` — Container orchestrating all sub-components
- `App` — Root, mounts PipelinePanel

### CSS approach

Inline styles (matching DiaApplicationEditor pattern). CSS variables for level colours: `--color-info`, `--color-warn`, `--color-error`. Dark theme (1e1e1e background, #ccc text).

### Live timer

JS-side `setInterval(100ms)` updates elapsed display for running stages. Switches to final `durationMs` when `OnStageCompleted`/`OnStageFailed` arrives.

## Tasks

| # | Task | Status | Notes |
|---|------|--------|-------|
| 1 | Update PipelineEditorPlugin C++ to implement Observer and push events via WebUIBridge | Done | Added Observer interface, JSON serialization, incremental event push, NotifyUIDataChanged calls |
| 2 | Create React/TS project scaffolding (package.json, vite.config.ts, tsconfig.json, index.html, main.tsx) | Done | Follow DiaApplicationEditor pattern |
| 3 | Create state types and pipelineReducer (types.ts, pipelineReducer.ts) | Done | Per spec state shape and event reducer table |
| 4 | Create usePipelineEvents hook | Done | Subscribe to pipeline.event topic via message listener |
| 5 | Create UI components (EmptyState, RunSummary, StageTimeline, StageRow, StageDetail, PipelinePanel, App) | Done | All 7 components |
| 6 | npm install + vite build | Done | dist/index.html + dist/assets/index-e29fdb11.js |
| 7 | MSBuild — verify C++ compiles | Done | DiaPipelineEditor.lib + GoogleTests.exe build clean |
| 8 | Unit tests (JS): reducer + component + hook + edge case tests | Done | 53 vitest tests passing |
| 9 | Integration tests (C++): PipelineEditorPlugin → WebUIBridge push | Done | 4 integration tests passing (17 total C++ tests) |

## Session Notes

### 2026-04-27
- Created plan from approved spec
- DiaApplicationEditor UI patterns confirmed: React 18, Vite 4, inline styles, postMessage bridge, useReducer for state
- C++ Observer pattern: implement `ObserverNotification(subject, message)`, register/unregister on load/unload
- StringCRC serialization via `.AsChar()` (not `.GetString()` as initially planned)
- `react-jsx` transform means no `import React from 'react'` needed; use `import type { FC } from 'react'` instead
- All tasks complete: 53 JS tests (6 files) + 17 C++ tests (2 files) all passing
