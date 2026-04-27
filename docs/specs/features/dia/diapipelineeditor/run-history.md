# Feature Spec: run-history

## Parent System
@docs/specs/systems/dia/diapipelineeditor.md

## Status
`Approved`

## Summary

Persist and browse the last 10 pipeline run summaries. When a run completes (or is detected as interrupted), its `RunSummary` is appended to a JSON history file at `Cluiche/out/CluicheEditor/DiaPipelineEditor/pipeline-history/history.json`. The pipeline panel's HistoryDrawer lets users select a past run to review its summary. Keeps the cap at 10 entries — oldest is evicted when a new one arrives.

## Problem

The NDJSON log is overwritten on each run, so previous run data is lost. Developers need to see whether their last few builds passed or failed without re-running them, especially when debugging intermittent build issues.

## Goals

- Persist `RunSummary` for the last 10 completed/interrupted runs
- Evict oldest entry when cap is reached
- UI to browse history and view summary of each past run
- History survives editor restart (disk-backed)
- DiaAPI command to query history programmatically

## Non-Goals

- Storing full event logs for past runs — only the `RunSummary` is kept
- Replaying past runs in the stage timeline — history shows summary only, not the full drill-down
- Configurable history depth — hardcoded at 10 per SPE-002
- Exporting or sharing history

## Acceptance Criteria

| # | Criterion |
|---|-----------|
| AC1 | When a run completes (`OnRunCompleted`/`OnRunFailed`) or is detected as interrupted, its `RunSummary` is appended to `history.json` |
| AC2 | `history.json` contains at most 10 entries; oldest entry is evicted when an 11th arrives |
| AC3 | `history.json` is written to `Cluiche/out/CluicheEditor/DiaPipelineEditor/pipeline-history/history.json` per SED-020 |
| AC4 | History is loaded from disk on plugin startup — survives editor restart |
| AC5 | HistoryDrawer UI component shows a list of past runs with: target, config, pass/fail, total time, timestamp |
| AC6 | Clicking a history entry shows its RunSummary details in the panel (replaces the current run view temporarily) |
| AC7 | Clicking "Back to current" or triggering a new build returns to the live view |
| AC8 | `pipeline-history` DiaAPI command returns JSON array of the last N run summaries |
| AC9 | Handles missing/corrupt `history.json` gracefully — starts fresh, logs warning |
| AC10 | History directory is created automatically if it doesn't exist |

## Data Model

### history.json schema

```json
{
  "version": 1,
  "runs": [
    {
      "target": "googletest",
      "config": "Debug",
      "passCount": 3,
      "failCount": 0,
      "totalDurationMs": 5100,
      "startTimestamp": 1714123456.0,
      "interrupted": false
    }
  ]
}
```

`runs` is ordered newest-first. Maximum 10 entries.

### C++ API

```cpp
namespace Dia::PipelineEditor {

    class RunHistoryStore {
    public:
        void Initialize(const char* historyFilePath);
        void Shutdown();

        // Append a completed/interrupted run — evicts oldest if at cap
        void RecordRun(const RunSummary& summary);

        // Query
        int GetCount() const;
        const RunSummary& GetRun(int index) const;  // 0 = newest

        // Load/save
        void LoadFromDisk();
        void SaveToDisk();

    private:
        Dia::Core::FilePath mFilePath;
        DynamicArrayC<RunSummary, 10> mRuns;
    };
}
```

## Implementation

### Files introduced

```
Dia/DiaPipelineEditor/
├── RunHistoryStore.h
├── RunHistoryStore.cpp
└── UI/src/components/
    └── HistoryDrawer.tsx
```

### Files modified

```
Dia/DiaPipelineEditor/
├── PipelineEditorPlugin.cpp      # Wire RunHistoryStore to PipelineLogTailer observer
└── UI/src/components/
    └── PipelinePanel.tsx          # Add history toggle / drawer
```

### Integration flow

1. `PipelineEditorPlugin::OnLoad` creates `RunHistoryStore`, calls `LoadFromDisk()`
2. Plugin registers as observer on `PipelineLogTailer`
3. When tailer emits `OnRunCompleted`/`OnRunFailed` or detects interrupted: plugin calls `RunHistoryStore::RecordRun()` then `SaveToDisk()`
4. Plugin pushes updated history to JS via `WebUIBridge::NotifyUIDataChanged("pipeline.history", historyJson)`
5. HistoryDrawer renders the list

## Dependencies

| Dependency | Type | What is used |
|------------|------|-------------|
| ndjson-tailer | Feature (same system) | PipelineLogTailer provides RunSummary on run completion |
| pipeline-panel-ui | Feature (same system) | Panel hosts the HistoryDrawer |
| DiaCore | Foundation | FilePath, Json, DynamicArrayC, StringCRC |
| DiaLogger | Diagnostics | Warning on corrupt history file |
| DiaAPI | Commands | Register `pipeline-history` command |

## Acceptance Criteria Tests

| AC | Test approach |
|----|--------------|
| AC1 | Unit test: call RecordRun() with a RunSummary, verify it appears in GetRun(0) |
| AC2 | Unit test: record 11 runs, verify GetCount()==10 and oldest is evicted |
| AC3 | Unit test: verify SaveToDisk() writes to the expected path |
| AC4 | Unit test: write history.json, call LoadFromDisk(), verify runs loaded |
| AC5 | Unit test (JS): pass history data, verify list renders with correct fields |
| AC6 | Unit test (JS): click history entry, verify RunSummary detail view shown |
| AC7 | Unit test (JS): click "Back to current", verify live view restored |
| AC8 | Unit test: call pipeline-history command, verify JSON array returned |
| AC9 | Unit test: write corrupt JSON to history.json, call LoadFromDisk(), verify empty store + warning logged |
| AC10 | Unit test: Initialize with non-existent directory, call SaveToDisk(), verify directory created and file written |

## Traceability

| Level | Spec | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaPipelineEditor | @docs/specs/systems/dia/diapipelineeditor.md |

## Binding Decisions Compliance

| ID | Source | Decision | Compliance |
|----|--------|----------|------------|
| PD-001 | Platform | Use StringCRC for IDs | Compliant — target/config stored as StringCRC in RunSummary |
| PD-004 | Platform | No STL in public APIs | Compliant — `DynamicArrayC<RunSummary, 10>`, `const char*` |
| PD-005 | Platform | x64 Windows only | Compliant |
| PD-006 | Platform | VS project files | Compliant — same .vcxproj |
| PD-007 | Platform | C++20 | Compliant |
| PD-009 | Platform | Generated output under `Cluiche/out/` | Compliant — writes to `Cluiche/out/CluicheEditor/DiaPipelineEditor/pipeline-history/` |
| AD-003 | Dia App | Namespace `Dia::<Module>::` | Compliant — `Dia::PipelineEditor::RunHistoryStore` |
| SED-015 | DiaEditor | Pure C++ library | Compliant — no Module/Phase subclasses |
| SED-020 | DiaEditor | Plugin output under `Cluiche/out/CluicheEditor/<PluginName>/` | Compliant — `Cluiche/out/CluicheEditor/DiaPipelineEditor/pipeline-history/history.json` |
| SPE-002 | DiaPipelineEditor | Run history capped at 10 | Compliant — `DynamicArrayC<RunSummary, 10>`, eviction on overflow |

## AI Review Questions

| # | Section | Question | Suggested Default | Answer |
|---|---------|----------|-------------------|--------|
| 1 | Write safety | Should SaveToDisk() use atomic write (temp + rename)? | Yes — write to `.tmp`, rename on success. Prevents corrupt file if editor crashes mid-write. | Yes. Write to `.tmp`, rename on success. Prevents corruption on crash. |
| 2 | History ordering | Newest-first or oldest-first in the JSON array? | Newest-first — matches UI display order, simplest for eviction (pop back). | Newest-first. Matches UI display order; eviction pops the back. |

## Open Questions

(none)
