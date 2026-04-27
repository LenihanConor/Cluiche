# Feature Spec: build-trigger

## Parent System
@docs/specs/systems/dia/diapipelineeditor.md

## Status
`Done`  
**Plan:** @docs/specs/features/dia/diapipelineeditor/build-trigger.plan.md

## Summary

DiaAPI commands and UI controls that allow triggering, monitoring, and cancelling pipeline builds from inside CluicheEditor. The "Build" button in the pipeline panel launches `dia pipeline` as an async subprocess, the tailer auto-attaches to the resulting NDJSON log, and the UI shows progress in real time. A "Cancel" button kills the running process.

## Problem

Developers currently switch to a terminal to run `dia pipeline`. The editor should let them trigger builds without leaving the tool, with the pipeline panel automatically showing progress.

## Goals

- DiaAPI commands (`pipeline-start`, `pipeline-cancel`) registered via DiaEditor's CommandDispatcher
- Toolbar UI in the pipeline panel: target/config dropdowns, "Build" button, force checkbox
- Async subprocess — editor stays responsive while building
- Auto-attach: when a build is triggered, PipelineLogTailer immediately starts watching the new log file
- "Build" button becomes "Cancel" while a build is running
- Cancel kills the subprocess and the tailer shows the run as interrupted

## Non-Goals

- Editing `pipeline.toml` — out of scope
- Queuing multiple builds — one build at a time; "Build" is disabled while a build runs
- Remote builds — subprocess is local only

## Acceptance Criteria

| # | Criterion |
|---|-----------|
| AC1 | `pipeline-start` DiaAPI command is registered and callable from CommandDispatcher |
| AC2 | `pipeline-start` accepts `--config`, `--target`, `--stage`, `--force` arguments |
| AC3 | `pipeline-start` launches `dia pipeline` as an async subprocess (editor does not block) |
| AC4 | PipelineLogTailer auto-attaches to `last-run.ndjson` when a build is triggered (detects the new run via file truncation) |
| AC5 | `pipeline-cancel` kills the running subprocess; tailer detects interrupted state |
| AC6 | Pipeline panel toolbar has target dropdown, config dropdown (Debug/Release/Both), force checkbox, and Build/Cancel button |
| AC7 | Target dropdown is populated from `pipeline.toml` target names |
| AC8 | Build button is disabled while a build is running; shows "Cancel" instead |
| AC9 | If `pipeline-start` is called while a build is already running, it returns an error (no queue) |
| AC10 | Subprocess exit code is captured; non-zero logged via DiaLogger |

## Implementation

### DiaAPI Commands

```cpp
namespace Dia::PipelineEditor {

    class PipelineBuildManager {
    public:
        void Initialize(PipelineLogTailer* tailer, const char* cliRoot);
        void Shutdown();

        // Start a build — returns 0 on success, non-zero on error
        int Start(const char* config, const char* target,
                  const char* stages, bool force);

        // Cancel running build — kills subprocess
        void Cancel();

        // State queries
        bool IsBuildRunning() const;
        int GetLastExitCode() const;

    private:
        PipelineLogTailer* mTailer;
        Dia::Core::FilePath mCliRoot;
        HANDLE mProcessHandle;       // Win32 process handle
        int mLastExitCode;
    };
}
```

### Subprocess launch

`Start()` spawns `python -m dia_cli pipeline --config <cfg> --target <tgt> [--stage <s>] [--force]` using `CreateProcess` (Win32). The working directory is set to `Dia/DiaCLI/`. The process handle is stored for `Cancel()` (`TerminateProcess`) and exit code polling.

### Toolbar UI

```
┌──────────────────────────────────────────────────────┐
│ Target: [googletest ▾]  Config: [Debug ▾]  ☐ Force  │
│                                    [ ▶ Build ]       │
└──────────────────────────────────────────────────────┘
```

The toolbar is part of the pipeline panel (not a separate plugin). It reads available targets from `pipeline.toml` via a DiaAPI request (`pipeline-get-targets`).

### Files introduced

```
Dia/DiaPipelineEditor/
├── PipelineBuildManager.h
├── PipelineBuildManager.cpp
└── UI/src/components/
    └── PipelineToolbar.tsx
```

### Files modified

```
Dia/DiaPipelineEditor/
├── PipelineEditorPlugin.cpp    # Register DiaAPI commands, wire up PipelineBuildManager
└── UI/src/components/
    └── PipelinePanel.tsx       # Add toolbar above stage timeline
```

## Dependencies

| Dependency | Type | What is used |
|------------|------|-------------|
| ndjson-tailer | Feature (same system) | PipelineLogTailer for auto-attach |
| pipeline-panel-ui | Feature (same system) | Panel hosts the toolbar |
| DiaEditor | Framework | CommandDispatcher for DiaAPI command registration |
| DiaAPI | Commands | Register `pipeline-start`, `pipeline-cancel`, `pipeline-get-targets` |
| DiaCore | Foundation | FilePath, StringCRC, Json |
| DiaCLI | Runtime | `dia pipeline` subprocess |

## Acceptance Criteria Tests

| AC | Test approach |
|----|--------------|
| AC1 | Unit test: verify `pipeline-start` is registered in command registry |
| AC2 | Unit test: call Start() with config/target/stage/force, verify subprocess command line |
| AC3 | Unit test: mock CreateProcess, verify call is non-blocking, editor continues |
| AC4 | Integration test: trigger build, verify PipelineLogTailer picks up new OnRunStarted |
| AC5 | Unit test: call Cancel(), verify TerminateProcess called, tailer reports interrupted |
| AC6 | Unit test (JS): verify toolbar renders target dropdown, config dropdown, force checkbox, Build button |
| AC7 | Unit test (JS): mock pipeline-get-targets response, verify dropdown populated |
| AC8 | Unit test (JS): set buildRunning=true, verify button shows "Cancel" and Build is disabled |
| AC9 | Unit test: call Start() while running, verify error return code |
| AC10 | Unit test: mock process exit with non-zero, verify DiaLogger warning |

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
| PD-005 | Platform | x64 Windows only | Win32 `CreateProcess`/`TerminateProcess` for subprocess management |
| SPE-004 | DiaPipelineEditor | Build via DiaAPI command dispatch, not direct JS subprocess | JS sends command request, C++ launches subprocess |

## AI Review Questions

| # | Section | Question | Suggested Default | Answer |
|---|---------|----------|-------------------|--------|
| 1 | Process management | How to detect subprocess completion? | Poll `GetExitCodeProcess` in PipelineBuildManager::Update() (called each frame alongside PipelineLogTailer::Poll). When process exits, capture exit code. | Poll `GetExitCodeProcess` each frame in Update(). Capture exit code on completion. Matches existing polling pattern. |
| 2 | Config discovery | How to read targets from pipeline.toml for the dropdown? | `pipeline-get-targets` DiaAPI command that reads pipeline.toml and returns JSON array of target names. Called once when panel loads. | `pipeline-get-targets` DiaAPI command. Returns JSON array. Called once on panel load. |
| 3 | Venv path | How does the subprocess find the right Python venv? | Use the same pattern as Directory.Build.targets: `.venv\Scripts\python.exe -m dia_cli pipeline ...` resolved relative to `Dia/DiaCLI/` | `.venv\Scripts\python.exe -m dia_cli pipeline ...` relative to `Dia/DiaCLI/`. Same proven pattern as Directory.Build.targets. |

## Open Questions

(none)
