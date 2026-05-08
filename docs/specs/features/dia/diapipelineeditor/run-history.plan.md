# Plan: run-history

**Spec:** @docs/specs/features/dia/diapipelineeditor/run-history.md  
**Status:** Done  
**Started:** 2026-04-27  
**Last Updated:** 2026-04-27

## Implementation Patterns

### RunHistoryStore (C++)

- `Initialize(pluginRootPath, sessionId)` — sets root path, handles session context (.context.json archive), calls LoadFromDisk()
- `Shutdown()` — final SaveToDisk() for safety
- `RecordRun(RunSummary)` — prepend to mRuns (newest-first), evict oldest if >10, SaveToDisk()
- `LoadFromDisk()` — reads `pipeline-history/history.json`, parses JSON, populates mRuns; logs warning on corrupt file, starts fresh
- `SaveToDisk()` — atomic write (temp + rename) to `pipeline-history/history.json`; creates directory if needed
- `GetCount()` / `GetRun(index)` — 0 = newest
- Uses `DynamicArrayC<RunSummary, 10>` per SPE-002
- Session management: reads `.context.json`, archives old session data to `.sessions/<oldId>/`, writes new context
- `SerializeRunToJson(RunSummary)` / `DeserializeRunFromJson(Json::Value)` helpers

### JSON schema for history.json

```json
{"version": 1, "runs": [{"target": "...", "config": "...", "passCount": N, ...}]}
```

Newest-first ordering in runs array.

### PipelineEditorPlugin integration

- Create RunHistoryStore in OnLoad, pass plugin root path
- In ObserverNotification: on kRunCompleted or kRunInterrupted, call RecordRun() with current RunSummary
- Push history to UI via `NotifyUIDataChanged("pipeline.history", historyJson)` after each RecordRun
- Register `pipeline.history` request handler (returns JSON array of runs)

### HistoryDrawer (React)

- Renders list of past runs: target, config, pass/fail, total time, timestamp
- Clicking entry dispatches VIEW_HISTORY action with run index
- "Back to current" button dispatches CLEAR_HISTORY_VIEW

### State changes

- Add `historyRuns: HistoryRun[]` and `viewingHistoryIndex: number | null` to app-level state
- New reducer actions: SET_HISTORY, VIEW_HISTORY, CLEAR_HISTORY_VIEW
- When viewingHistoryIndex is set, PipelinePanel shows that run's summary instead of live data

## Tasks

| # | Task | Status | Notes |
|---|------|--------|-------|
| 1 | Create RunHistoryStore.h/.cpp | Done | JSON load/save, atomic write, session management (archive/prune/context), DIA_LOG_WARNING on corrupt |
| 2 | Wire RunHistoryStore in PipelineEditorPlugin | Done | Records on kRunCompleted/kRunInterrupted, pushes pipeline.history, request handler, generates sessionId from timestamp |
| 3 | Add history types and reducer actions (JS) | Done | HistoryRun type, SET_HISTORY/VIEW_HISTORY/CLEAR_HISTORY_VIEW, OnRunStarted preserves history |
| 4 | Create HistoryDrawer.tsx | Done | Past runs list with status dots, click to view, "Back to current" |
| 5 | Update PipelinePanel and App for history | Done | HistorySummary component, drawer below content, usePipelineEvents fetches history on mount |
| 6 | Update vcxproj files | Done | RunHistoryStore.h/.cpp added to DiaPipelineEditor + GoogleTests |
| 7 | Write C++ unit tests | Done | 13 tests: record, eviction, save/load, corrupt, directory, ToJson, context.json, archive, same-session |
| 8 | Write JS unit tests | Done | 16 tests: HistoryDrawer (7), HistorySummary (4), reducer history (5) |
| 9 | Build + test | Done | C++ 40/40 pass, JS 78/78 pass, vite build OK |

## Session Notes

### 2026-04-27
- Created plan from approved spec
- RunSummary struct already exists in PipelineEvent.h — reuse it directly
- StringCRC.AsChar() for JSON serialization (same pattern as PushEventsToUI)
- Plugin root path: Cluiche/out/CluicheEditor/DiaPipelineEditor/ per SED-020
- Atomic write via temp file + rename per AI Review Q1
