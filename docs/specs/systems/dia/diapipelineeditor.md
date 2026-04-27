# System Spec: DiaPipelineEditor

## Parent Application
@docs/specs/applications/dia.md

## Purpose

DiaPipelineEditor is an editor plugin system that provides a live build pipeline viewer and trigger panel inside CluicheEditor. It tails the NDJSON event log produced by DiaCLI's OutputContext (`Cluiche/out/DiaCLI/logs/pipeline/last-run.ndjson`), renders stage-by-stage progress in real time, allows triggering builds from the editor, and keeps a history of the last 10 runs. The C++ side polls the log file and pushes parsed events to a CEF panel via DiaUICEF's message passing — no WebSocket dependency.

## Responsibilities

- **NDJSON Log Tailing** — C++ Module that monitors `Cluiche/out/DiaCLI/logs/pipeline/last-run.ndjson`, detects new/overwritten runs, and parses events line-by-line
- **Event Model** — Maintain an in-memory model of the current run (stages, steps, durations, pass/fail) plus the last 10 completed runs
- **Live UI Panel** — React/TypeScript dockable panel that renders a stage timeline with colour-coded status, expandable drill-down into log lines, and elapsed time per stage
- **Build Triggering** — Invoke `dia pipeline` from the editor via DiaAPI command dispatch (e.g. `pipeline-start --config Debug --target googletest`), with the panel auto-attaching to the resulting NDJSON log
- **Run History** — Store summary data (target, config, stages, pass/fail counts, total duration, timestamp) for the last 10 runs; selectable in the UI to review past results
- **Interrupted Run Detection** — Detect unmatched `OnStageStarted` events (no corresponding `Completed`/`Failed`) and show them as "interrupted" in the UI

## Public Interfaces

### C++ Module: PipelineLogTailer

A DiaEditor plugin module that tails the NDJSON file and feeds parsed events to the UI.

```cpp
namespace Dia::PipelineEditor {

    struct PipelineEvent {
        Dia::Core::StringCRC eventType;   // e.g. "OnStageStarted"
        Dia::Core::StringCRC system;      // "pipeline"
        Dia::Core::StringCRC stage;       // e.g. "compile-code"
        Dia::Core::StringCRC step;        // e.g. "msbuild" (empty for stage-level events)
        float timestampSec;               // Unix timestamp
        int durationMs;                   // -1 if not a Completed/Failed event
        const char* error;                // nullptr if not a Failed event
        const char* detail;               // extra info (step detail, message)
        const char* level;                // log level for OnLogLine events
    };

    struct RunSummary {
        Dia::Core::StringCRC target;
        Dia::Core::StringCRC config;
        int passCount;
        int failCount;
        int totalDurationMs;
        float startTimestamp;
        bool interrupted;                 // true if process crashed mid-run
    };

    class PipelineLogTailer {
    public:
        void Initialize(const char* logPath);
        void Shutdown();

        // Called each frame by the editor's update loop
        void Poll();

        // Current run state
        bool IsRunInProgress() const;
        const RunSummary& GetCurrentRunSummary() const;

        // Observer — notified when new events arrive
        void RegisterObserver(Dia::Core::Observer* observer);
        void UnregisterObserver(Dia::Core::Observer* observer);

    private:
        // File polling, seek position, line buffer, parse state
    };
}
```

### DiaAPI Commands

Exposed via the DiaEditor command dispatcher so the UI can trigger builds:

| Command | Arguments | Description |
|---------|-----------|-------------|
| `pipeline-start` | `--config`, `--target`, `--stage`, `--force` | Invoke `dia pipeline` as a subprocess; auto-attach the tailer to the resulting log |
| `pipeline-cancel` | (none) | Kill the running pipeline subprocess |
| `pipeline-get-targets` | (none) | Return JSON array of target names from `pipeline.toml` |
| `pipeline-history` | `--count N` | Return JSON array of the last N run summaries |

### JS/React Panel

The panel is a dockable IEditorPlugin (`LayoutMode::kDockable`) that renders via CEF.

**Panel components:**

| Component | Description |
|-----------|-------------|
| `PipelineToolbar` | Target/config dropdowns, "Build" button, force checkbox |
| `StageTimeline` | Vertical list of stages with status icons (▶ running, ✓ pass, ✗ fail, ○ skipped), elapsed time, expandable |
| `StageDetail` | Expanded view showing log lines (`OnLogLine` events) for a given stage, with level-based colouring |
| `RunSummary` | Header bar showing current run: target, config, pass/fail counts, total time |
| `HistoryDrawer` | Sidebar or dropdown listing last 10 runs; clicking one loads its summary into the panel |

**Data flow:** `NDJSON file → PipelineLogTailer (C++ poll) → WebUIBridge push → React useReducer → render`. See pipeline-panel-ui feature spec for detailed flow and state shape.

## Dependencies

### Required Systems (from Dia)

| System | Dependency Type | What is used |
|--------|----------------|-------------|
| DiaCLI | Data contract | NDJSON event schema (`dia.output.v1`) — file path and event format |
| DiaEditor | Framework | IEditorPlugin interface, EditorPluginRegistry, WebUIBridge, observer paths |
| DiaUICEF | Rendering | CEF panel hosting, `CefProcessMessage` transport for C++ → JS events |
| DiaCore | Foundation | StringCRC, Observer, DynamicArrayC, FilePath, Json |
| DiaAPI | Command dispatch | Register `pipeline-start` / `pipeline-cancel` commands |

### External Dependencies

| Dependency | Notes |
|------------|-------|
| React + TypeScript | UI component framework (shared with other editor panels) |

### NOT dependent on

- **DiaWebSocket** — events pass through CEF message bridge, not WebSocket
- **DiaPipeline** — reads the NDJSON output file, not the pipeline code itself; fully decoupled
- **DiaApplication** — pure library + plugin; no Module/Phase/ProcessingUnit subclasses (per SED-015)

## Non-Responsibilities

What DiaPipelineEditor explicitly does NOT handle:

- **Pipeline execution logic** — that belongs in DiaPipeline/DiaCLI; this system only observes output
- **NDJSON format definition** — owned by DiaCLI's cli-output feature; this system is a consumer
- **Terminal output rendering** — that's DiaCLI's `rich` layer; this system renders in CEF
- **Build configuration editing** — editing `pipeline.toml` is out of scope; this system reads targets/configs for the trigger dropdown
- **Non-pipeline logs** — env and test NDJSON logs are consumed by separate future panels, not this one

## Inherited Binding Decisions

These decisions from parent platform and application specs are binding constraints on DiaPipelineEditor:

| Source | ID | Decision | Impact on DiaPipelineEditor |
|--------|----|----------|----------------------------|
| Platform | PD-001 | Use StringCRC for all entity/component IDs | Plugin type ID, event type enums, observer data paths all use StringCRC |
| Platform | PD-002 | PU/Phase/Module architecture for app structure | Applies to consumer (CluicheEditor), not this plugin. DiaPipelineEditor is a pure library per SED-015 |
| Platform | PD-003 | Component-based entities | Plugin registers via IEditorPlugin / EditorPluginRegistry |
| Platform | PD-004 | No STL containers in public APIs | PipelineEvent uses `const char*`, StringCRC, not std::string; DynamicArrayC for collections |
| Platform | PD-005 | x64 Windows only | Compliant — Windows file I/O for log tailing |
| Platform | PD-006 | VS project files are source of truth | Built as `.vcxproj` in Dia solution |
| Platform | PD-007 | C++20 required | Uses C++20 features where beneficial |
| Platform | PD-008 | `Directory.Build.props` owns OutDir/IntDir | Compliant — reads from `Cluiche/out/`, does not modify build paths |
| Platform | PD-009 | All generated non-binary output under `Cluiche/out/<AppName>/` | Reads NDJSON from `Cluiche/out/DiaCLI/logs/pipeline/`; run history stored under `Cluiche/out/CluicheEditor/DiaPipelineEditor/` |
| Dia | AD-001 | Module system with YAML frontmatter | Documented as `dia.dia.diapipelineeditor.architecture.module.md` |
| Dia | AD-002 | No STL in public APIs | Reinforces PD-004 |
| Dia | AD-003 | Namespace convention `Dia::<Module>::` | All classes in `Dia::PipelineEditor::` namespace |
| DiaEditor | SED-001 | Minimal stable plugin interface | DiaPipelineEditor implements IEditorPlugin; keeps plugin surface small |
| DiaEditor | SED-002 | Macro-based plugin registration | Uses `REGISTER_EDITOR_PLUGIN(DiaPipelineEditor, "DiaPipelineEditor")` |
| DiaEditor | SED-003 | Plugin lives at `Dia/Dia<System>Editor/` | Located at `Dia/DiaPipelineEditor/` (top-level peer) |
| DiaEditor | SED-015 | Pure C++ library, no DiaApplication dependency | No Module/Phase/ProcessingUnit subclasses |
| DiaEditor | SED-020 | Each editor plugin writes persistent output to `Cluiche/out/CluicheEditor/<PluginName>/` | Run history at `Cluiche/out/CluicheEditor/DiaPipelineEditor/pipeline-history/history.json`; any future logs under same root |

## System-Specific Decisions

| ID | Decision | Rationale | Status | Binding |
|----|----------|-----------|--------|---------|
| SPE-001 | File polling (not filesystem events) for NDJSON tailing | `ReadDirectoryChangesW` is unreliable for rapid appends; polling at frame rate (~16ms) is simpler, portable across Docker/host, and sufficient for the update rate | Proposed | Yes |
| SPE-002 | Run history capped at 10 entries, stored as JSON array | Keeps memory and disk footprint small; 10 runs covers a typical work session; stored at `Cluiche/out/CluicheEditor/DiaPipelineEditor/pipeline-history/history.json` per SED-020 | Proposed | Yes |
| SPE-003 | CEF message passing for C++ → JS events (not WebSocket) | Events stay in-process; simpler than standing up a WebSocket channel; consistent with DiaEditor's existing `WebUIBridge::NotifyUIDataChanged` pattern | Proposed | Yes |
| SPE-004 | Pipeline subprocess launched via DiaAPI command dispatch, not direct `subprocess.run` from JS | Keeps build triggering on the C++ side where process management is robust; JS only sends a command request; follows DiaEditor's CommandDispatcher pattern (SED-007) | Proposed | Yes |
| SPE-005 | Interrupted run detection via unmatched `Started` events | Matches the contract defined in cli-output spec; no heartbeat or PID-check needed; the tailer compares Started vs Completed/Failed counts after a configurable idle timeout | Proposed | Yes |

## Features

Features within the DiaPipelineEditor system (create with `/spec-feature`):

| Feature | Description | Spec | Effort | Status |
|---------|-------------|------|--------|--------|
| ndjson-tailer | C++ file tailer that polls and parses the NDJSON pipeline log into PipelineEvent structs | [ndjson-tailer.md](../../features/dia/diapipelineeditor/ndjson-tailer.md) | 3 days | Approved |
| pipeline-panel-ui | React/TypeScript dockable panel — stage timeline, log drill-down, run summary | [pipeline-panel-ui.md](../../features/dia/diapipelineeditor/pipeline-panel-ui.md) | 5 days | Approved |
| build-trigger | DiaAPI commands to start/cancel pipeline from editor; auto-attach tailer to new run | [build-trigger.md](../../features/dia/diapipelineeditor/build-trigger.md) | 2 days | Approved |
| run-history | Store and browse last 10 pipeline runs with summary data | [run-history.md](../../features/dia/diapipelineeditor/run-history.md) | 2 days | Approved |

**Total Effort Estimate:** 12 days

**Recommended Implementation Order:**
1. ndjson-tailer (3d) — Core data layer; everything depends on parsed events
2. pipeline-panel-ui (5d) — Visual feedback; can use mock data then switch to real tailer
3. build-trigger (2d) — Wire up the "Build" button
4. run-history (2d) — Persist and browse past runs

## AI Review Questions

| # | Section | Question | Suggested Default | Answer |
|---|---------|----------|-------------------|--------|
| 1 | Tailing | What polling interval for the NDJSON file? | Every frame (~16ms); seek to last read position, read new lines | Every frame (~16ms). Cheap seek+read, matches display rate. |
| 2 | History | Where should run history JSON live? | `Cluiche/out/CluicheEditor/DiaPipelineEditor/pipeline-history/history.json` per SED-020 | Per SED-020. See run-history feature spec for full schema. |
| 3 | Triggering | Should `pipeline-start` block the editor or run async? | Async — subprocess launched in background; tailer observes output; UI stays responsive | Async. Subprocess in background, tailer observes output, UI stays responsive. Build button becomes Cancel while running. |
| 4 | Schema versioning | How should the panel handle unknown `dia.output.v2+` events? | Ignore unknown event types gracefully; show "unsupported event" in log drill-down; check `schema` field on `OnRunStarted` | Ignore unknown events gracefully. Check `schema` field on `OnRunStarted`; show warning banner if `v2+`; render known event types normally; unknown types show as "unsupported event" in drill-down. |
| 5 | Panel registration | What `GetUIPath()` should this plugin return? | `"Dia/DiaPipelineEditor/UI/dist/"` — matches DiaApplicationEditor pattern | `"Dia/DiaPipelineEditor/UI/dist/"` — follows DiaApplicationEditor convention. React app builds into `UI/dist/`, CEF loads from there. |
| 6 | Interrupted detection | How long to wait before declaring a run "interrupted"? | 5 seconds of no new events after an unmatched `Started`; configurable | 5 seconds idle after unmatched `Started`, configurable. |
| 7 | Multiple configs | When user selects "Both" (Debug+Release), should the panel show two runs or one? | Two sequential runs (DiaPipeline already loops over configs); tailer sees two `OnRunStarted` events | Two sequential runs. DiaPipeline emits two `OnRunStarted` events; both appear in history; panel shows whichever is active or most recent. |
| 8 | Error display | How should build errors (MSBuild output) be surfaced? | `OnLogLine` events with `level: "error"` shown in red in the StageDetail drill-down; clicking an error could copy the full line | `OnLogLine` with `level: "error"` shown in red in StageDetail drill-down. Warnings in yellow. Click to copy full line. Stage timeline stays clean (just pass/fail icons). |

## Status

`Approved` — All binding decisions compliant, all AI review questions answered
