# Feature Spec: ndjson-tailer

## Parent System
@docs/specs/systems/dia/diapipelineeditor.md

## Status
`Approved`

## Summary

C++ file tailer that polls `Cluiche/out/DiaCLI/logs/pipeline/last-run.ndjson` at frame rate, parses each line into a `PipelineEvent` struct, maintains an in-memory model of the current run, detects interrupted runs, and notifies observers when new events arrive. This is the data layer that all other DiaPipelineEditor features depend on.

## Problem

The editor has no way to observe DiaCLI pipeline progress in real time. The NDJSON log is written by a separate Python process and the editor needs to read it incrementally, parse structured events, and expose them to the UI layer. Without a dedicated tailer, every consumer would need to implement its own file I/O and JSON parsing.

## Goals

- Poll `last-run.ndjson` every frame (~16ms) with minimal overhead
- Parse the `dia.output.v1` NDJSON schema into typed C++ structs
- Detect new runs (file truncation/overwrite) and reset state automatically
- Track current run progress (which stages are running, passed, failed)
- Detect interrupted runs (unmatched `Started` events after idle timeout)
- Notify observers when new events arrive so the UI can react
- Handle edge cases gracefully: missing file, malformed JSON, unknown schema version

## Non-Goals

- Rendering UI — that's the pipeline-panel-ui feature
- Triggering builds — that's the build-trigger feature
- Persisting run history to disk — that's the run-history feature
- Tailing env or test logs — this feature only handles `pipeline/last-run.ndjson`

## Acceptance Criteria

| # | Criterion |
|---|-----------|
| AC1 | Polls `last-run.ndjson` every frame (~16ms); reads only new bytes since last seek position |
| AC2 | Parses each NDJSON line into a `PipelineEvent` struct with all fields from the `dia.output.v1` schema (`eventType`, `system`, `stage`, `step`, `timestampSec`, `durationMs`, `error`, `detail`, `level`) |
| AC3 | Detects file truncation/overwrite (new `OnRunStarted` at a smaller file size resets state) and starts tracking the new run |
| AC4 | Maintains current `RunSummary` (target, config, pass/fail counts, total duration, interrupted flag) updated as events arrive |
| AC5 | Notifies registered `Dia::Core::Observer` instances when new events arrive |
| AC6 | Detects interrupted runs after 5 seconds idle with unmatched `Started` events; marks `RunSummary::interrupted = true` |
| AC7 | Handles missing file gracefully — no crash; polls until file appears |
| AC8 | Checks `schema` field on `OnRunStarted`; logs warning via DiaLogger if not `dia.output.v1` |
| AC9 | Handles malformed JSON lines without crashing — skips the line and logs a warning |

## Data Models

All structs defined in the system spec (`Dia::PipelineEditor::` namespace):

### PipelineEvent

```cpp
struct PipelineEvent {
    Dia::Core::StringCRC eventType;   // "OnStageStarted", "OnRunCompleted", etc.
    Dia::Core::StringCRC system;      // "pipeline"
    Dia::Core::StringCRC stage;       // "compile-code" (empty for run-level events)
    Dia::Core::StringCRC step;        // "msbuild" (empty for stage/run-level events)
    float timestampSec;               // Unix timestamp (seconds, fractional)
    int durationMs;                   // -1 if not a Completed/Failed event
    const char* error;                // nullptr if not a Failed event
    const char* detail;               // extra info (step detail, log message)
    const char* level;                // "info"/"warn"/"error"/"debug" for OnLogLine; nullptr otherwise
};
```

### RunSummary

```cpp
struct RunSummary {
    Dia::Core::StringCRC target;      // "googletest"
    Dia::Core::StringCRC config;      // "Debug"
    int passCount;
    int failCount;
    int totalDurationMs;
    float startTimestamp;
    bool interrupted;                 // true if process crashed mid-run
};
```

### PipelineLogTailer

```cpp
class PipelineLogTailer {
public:
    void Initialize(const char* logPath);
    void Shutdown();

    // Called each frame by the editor's update loop
    void Poll();

    // Current run state
    bool IsRunInProgress() const;
    const RunSummary& GetCurrentRunSummary() const;

    // Events in current run (for UI drill-down)
    int GetEventCount() const;
    const PipelineEvent& GetEvent(int index) const;

    // Observer — notified when new events arrive
    void RegisterObserver(Dia::Core::Observer* observer);
    void UnregisterObserver(Dia::Core::Observer* observer);

private:
    Dia::Core::FilePath mLogPath;
    std::streampos mLastReadPos;          // seek position for incremental reads
    uint64_t mLastFileSize;               // detect truncation
    float mLastEventTime;                 // for interrupted detection
    RunSummary mCurrentRun;
    DynamicArrayC<PipelineEvent, 256> mEvents;
    int mUnmatchedStartedCount;           // track Started without Completed/Failed
    Dia::Core::ObserverSubject mObserverSubject;
};
```

### String ownership (Option C: hybrid)

Identifier fields (`eventType`, `system`, `stage`, `step`) are `StringCRC` — self-contained, fast to compare, no ownership concerns. These are a fixed vocabulary ("OnStageStarted", "compile-code", etc.).

Display text fields (`error`, `detail`, `level`) are `const char*` pointing into a string pool owned by the `PipelineLogTailer`. The pool is cleared when a new run starts (file truncation detected). Consumers must not hold pointers across frames — copy if needed. This avoids per-event heap allocation while keeping freeform text (error messages, log lines) available verbatim for UI rendering.

## Implementation

### Files introduced

```
Dia/DiaPipelineEditor/
├── PipelineEvent.h              # PipelineEvent and RunSummary structs
├── PipelineLogTailer.h          # PipelineLogTailer class declaration
├── PipelineLogTailer.cpp        # Polling, parsing, state management
└── Internal/
    └── NdjsonLineParser.h       # Single-line JSON → PipelineEvent parser (header-only)
```

### Polling logic (PipelineLogTailer::Poll)

```
1. If file doesn't exist → return (AC7)
2. Get current file size
3. If file size < mLastReadPos → file was truncated (new run)
   → Clear mEvents, reset mCurrentRun, reset mLastReadPos to 0 (AC3)
4. If file size == mLastReadPos → no new data
   → Check idle timeout for interrupted detection (AC6)
   → return
5. Open file, seek to mLastReadPos
6. Read all new complete lines (up to last newline)
7. For each line:
   a. Parse JSON (skip + warn on malformed) (AC9)
   b. Convert to PipelineEvent (AC2)
   c. If OnRunStarted: check schema field (AC8), init RunSummary (AC4)
   d. If OnStageStarted: increment mUnmatchedStartedCount
   e. If OnStageCompleted/Failed: decrement mUnmatchedStartedCount, update pass/fail
   f. If OnRunCompleted/Failed: finalize RunSummary
   g. Append to mEvents
8. Update mLastReadPos
9. Update mLastEventTime
10. Notify observers (AC5)
```

### Interrupted run detection

After step 4 (no new data), check:
- `mUnmatchedStartedCount > 0` AND `currentTime - mLastEventTime > 5.0f`
- If true: set `mCurrentRun.interrupted = true`, notify observers

The 5-second threshold is a compile-time constant (`kInterruptedTimeoutSec`) that can be made configurable later.

## Dependencies

| Dependency | Type | What is used |
|------------|------|-------------|
| DiaCore | Required | StringCRC, Observer/ObserverSubject, DynamicArrayC, FilePath, Json (jsoncpp) |
| DiaLogger | Required | `DIA_LOG_WARN` for malformed lines and unknown schema versions |
| DiaCLI cli-output | Data contract | NDJSON event schema `dia.output.v1` — file path and field names |

## Acceptance Criteria Tests

| AC | Test approach |
|----|--------------|
| AC1 | Unit test: write lines to temp file, call Poll(), verify events parsed; write more lines, Poll() again, verify only new lines parsed |
| AC2 | Unit test: write each event type, verify all PipelineEvent fields populated correctly |
| AC3 | Unit test: write a run, then truncate file and write new OnRunStarted; verify state resets |
| AC4 | Unit test: write a sequence of events, verify RunSummary fields (passCount, failCount, duration) update correctly |
| AC5 | Unit test: register mock Observer, write events, verify observer notified after Poll() |
| AC6 | Unit test: write OnStageStarted but no Completed; advance time past 5s; call Poll(); verify interrupted=true |
| AC7 | Unit test: Initialize with non-existent path; call Poll(); no crash; create file later; call Poll(); events parsed |
| AC8 | Unit test: write OnRunStarted with `schema: "dia.output.v2"`; verify warning logged |
| AC9 | Unit test: write malformed JSON line between valid lines; verify malformed line skipped, valid lines parsed |

## Traceability

| Level | Spec | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaPipelineEditor | @docs/specs/systems/dia/diapipelineeditor.md |

## Binding Decisions Compliance

| ID | Source | Decision | Compliance |
|----|--------|----------|------------|
| PD-001 | Platform | Use StringCRC for all entity/component IDs | Compliant — `PipelineEvent.eventType`, `.system`, `.stage`, `.step` all use StringCRC |
| PD-002 | Platform | PU/Phase/Module architecture for app structure | Compliant — PipelineLogTailer is a plain class, not a Module; driven by the consumer's update loop per SED-015 |
| PD-003 | Platform | Component-based entities | N/A — no entities in this feature |
| PD-004 | Platform | No STL containers in public APIs | Compliant — uses `DynamicArrayC`, `const char*`, StringCRC; `std::streampos` is internal only |
| PD-005 | Platform | x64 Windows only | Compliant — Windows file I/O |
| PD-006 | Platform | VS project files are source of truth | Compliant — new `.vcxproj` for DiaPipelineEditor |
| PD-007 | Platform | C++20 required | Compliant |
| PD-008 | Platform | `Directory.Build.props` owns OutDir/IntDir | Compliant — reads from `Cluiche/out/`, does not modify build paths |
| PD-009 | Platform | All generated non-binary output under `Cluiche/out/<AppName>/` | Compliant — reads from `Cluiche/out/DiaCLI/logs/pipeline/` |
| AD-001 | Dia App | Module system with YAML frontmatter | Compliant — will have `dia.dia.diapipelineeditor.architecture.module.md` |
| AD-002 | Dia App | No STL in public APIs | Compliant — reinforces PD-004 |
| AD-003 | Dia App | Namespace `Dia::<Module>::` | Compliant — all types in `Dia::PipelineEditor::` |
| SPE-001 | DiaPipelineEditor | File polling, not filesystem events | Compliant — `Poll()` called each frame, no `ReadDirectoryChangesW` |
| SPE-005 | DiaPipelineEditor | Interrupted detection via unmatched Started events | Compliant — idle timeout after 5 seconds of no new events with unmatched Started count > 0 |
| SED-015 | DiaEditor | Pure C++ library, no DiaApplication dependency | Compliant — PipelineLogTailer is a plain class |

## AI Review Questions

| # | Section | Question | Suggested Default | Answer |
|---|---------|----------|-------------------|--------|
| 1 | String ownership | Should PipelineEvent own its strings or point into a pool? | String pool owned by PipelineLogTailer; cleared on new run; consumers copy if needed across frames | Hybrid (Option C): StringCRC for identifiers (`eventType`, `system`, `stage`, `step`), string pool for display text (`error`, `detail`, `level`/`message`). Pool cleared on new run; consumers copy if holding across frames. |
| 2 | Max events | Should there be a cap on mEvents size (memory bound)? | 256 initial capacity; grow if needed; cap at 10,000 events per run (log warning if exceeded) | 256 initial capacity, grow if needed, hard cap at 10,000. Log warning if exceeded. |
| 3 | File open strategy | Keep file handle open or open/close each Poll()? | Open/close each Poll() — avoids holding a file lock that could block DiaCLI from writing | Open/close each Poll(). Avoids file lock conflicts with DiaCLI; overhead negligible with OS file caching. |
| 4 | Thread safety | Does Poll() need to be thread-safe? | No — called from the editor's main thread update loop only; no locking needed | No. Main thread only; no locking. |
| 5 | Log path discovery | How does the tailer know the NDJSON file path? | Hardcoded relative path `Cluiche/out/DiaCLI/logs/pipeline/last-run.ndjson` resolved from repo root; Initialize() takes the full path | Initialize() takes the full path as parameter. Caller resolves from repo root. Tailer is decoupled from path discovery. |

## Open Questions

(none)
