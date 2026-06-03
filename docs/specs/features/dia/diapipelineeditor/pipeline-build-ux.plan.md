# Plan: pipeline-build-ux

**Spec:** @docs/specs/features/dia/diapipelineeditor/pipeline-build-ux.md  
**Status:** Done

## Implementation Patterns

### C++ patterns

**Stdout capture** (`PipelineBuildManager`): Add a second output file alongside the NDJSON. Use `CreatePipe()` to capture the child process stdout handle, then read it in `Update()` or a dedicated thread and write to `last-stdout.log`. File path: `{repoRoot}/Cluiche/out/DiaCLI/logs/pipeline/last-stdout.log`. Truncated at `Start()`.

**Target parser extension** (`PipelineTargetParser.h`): Add `ParseTargetStages(tomlPath, targetName)` inline function returning `Json::Value` (array of strings). Parses `stages = [...]` line inside `[targets.NAME]` block. Same pattern as existing `ParsePipelineTargets`.

**Project context** (`PipelineEditorPlugin`): Follow the `DiaBlueprintEditorPlugin` pattern exactly — store `mDiagamePath[512]`, update on `OnProjectChanged`, register `pipeline.get-project-state` and `pipeline.get-target-stages` handlers. Target = filename-without-extension from `mDiagamePath`.

**RunHistoryStore simplification**: Remove `LoadFromDisk()` call from `Initialize()`. Remove `ArchiveStaleSession()`, `PruneSessions()`, `WriteContext()`, `mSessionsDir`, `mSessionId`, `mContextFilePath`. Change `kMaxRuns = 5`. Keep `SaveToDisk()` for the `Shutdown()` path (harmless to leave) OR strip entirely since history is session-only — strip is cleaner.

### UI patterns

**No-project overlay**: Same `<div class="no-project-overlay">` pattern as `DiaBlueprintEditor/UI/index.html` — rendered as React component `NoProjectOverlay`, toggled by `state.isProjectLoaded`. Subscribe to `blueprint_editor.project_changed` topic equivalent (`pipeline.project_changed`).

**Ghost stages**: `SET_STAGE_MANIFEST` action populates `state.stages` with `status: 'not-started'` entries before any run. `OnRunStarted` merges incoming stages with the manifest list (preserves order, resets status). Ghost pills render in `StageRow` when `status === 'not-started'` with grey colour and `○` icon.

**Timing estimate**: `PipelineState.stageDurationsMs: Record<string, number>` — populated from the most recent history run on `RECORD_RUN`. `StageRow` reads `stageDurationsMs[stage.name]` and shows `~Xs est.` when status is `running` and a prior duration exists.

**Timing bars**: `StepRow` receives `stageTotalDurationMs: number` prop. Bar width = `Math.min(100, (step.durationMs / stageTotalDurationMs) * 100)%`. Only shown after step completes (has `durationMs > 0`).

**Auto-expand on fail**: In the reducer, `OnStepFailed` sets `stage.expanded = true`. `StageRow` emits a `data-stage-name` attribute; `StageTimeline` holds a ref map and calls `scrollIntoView({ behavior: 'smooth', block: 'nearest' })` via `useEffect` when a stage transitions to `failed`.

**Inline failure reason**: `StepState` gains `inlineError?: string`. Populated from the last `OnLogLine` event with `level: 'error'` that arrives before or with `OnStepFailed`. `StepRow` renders it below the step name in red when present.

**Built X ago**: `PipelineState.lastSuccessTimestamp: number | null`. Set to `Date.now()` on `OnRunCompleted` with `exitCode === 0`. `PipelineToolbar` uses `useInterval(1000)` to recompute and display "Built Xs ago" / "Built Xm ago". Cleared to null on `OnRunStarted`.

**Split-button**: `PipelineToolbar` renders a flex row: `[▶ Build]` + `[▾]` (dropdown trigger). Dropdown contains "Build & Launch" option. `Launch` state: `boolean`, set true after successful build, cleared on `OnRunStarted`. Separate `↗ Launch` button disabled until `canLaunch === true`. On click: calls `pipeline.launch` request (new handler in C++).

**Launch handler** (`PipelineEditorPlugin`): `pipeline.launch` — calls `dia launch <target>` via `CreateProcessA`, no wait. Same working directory as `PipelineBuildManager`. No tailer attachment (launch is fire-and-forget from the panel's perspective).

---

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| T1 | Extend `PipelineTargetParser` to return `stages[]` per target | Unit test: parse toml fixture, verify stages array | Done | haiku | New `ParseTargetStages(tomlPath, targetName)` function |
| T2 | Add stdout capture to `PipelineBuildManager` | Manual: build fires, `last-stdout.log` populated | Done | sonnet | `CreatePipe()` + read in `Update()`, write to log file |
| T3 | Simplify `RunHistoryStore`: session-only, cap 5 | Build+run: history shows max 5, resets on restart | Done | haiku | Removed load/archive/session machinery |
| T4 | Wire project context into `PipelineEditorPlugin` | Manual: load `.diagame`, verify overlay lifts | Done | sonnet | `mDiagamePath`, `OnProjectChanged`, `get-project-state`, `get-target-stages` handlers |
| T5 | Add `pipeline.launch` handler to `PipelineEditorPlugin` | Manual: click Launch, game window opens | Done | haiku | `dia launch <target>` via `CreateProcessA` |
| T6 | UI: no-project overlay + project state wiring | Unit: renders overlay when `isProjectLoaded=false` | Done | sonnet | `usePipelineEvents` + `PipelinePanel` updated |
| T7 | UI: ghost stages (`SET_STAGE_MANIFEST`, toolbar target name, `get-target-stages` call) | Unit: reducer populates `not-started` stages; toolbar shows diagame name | Done | sonnet | Merged into state layer + T6; target dropdown removed |
| T8 | UI: `~Xs est.` timing estimate on running stages | Unit: reducer stores history durations; StageRow shows estimate | Done | sonnet | `estDurationMs` prop on `StageRow` |
| T9 | UI: relative timing bars in `StepRow` | Unit: bar width proportional to step duration fraction | Done | haiku | `stageTotalDurationMs` prop; 60×3px bar |
| T10 | UI: auto-expand + scroll to failing stage | Unit: `OnStepFailed` sets `expanded=true`; scroll fires | Done | sonnet | `scrollIntoView` via ref map in `StageTimeline` |
| T11 | UI: inline failure reason in `StepRow` | Unit: last error log line shown under failed step | Done | sonnet | `inlineError` in `StepState`; red monospace line |
| T12 | UI: "Built X ago" label in toolbar | Unit: label updates every second after success | Done | haiku | 1s interval in `PipelineToolbar` |
| T13 | UI: Build/Launch split-button + standalone Launch | Unit: Launch disabled until success; dropdown shows Build&Launch | Done | sonnet | Split-button + `↗ Launch`; `canLaunch` from state |
| T14 | UI: "Open logs folder" link | Manual: link opens Explorer to log dir; greyed if absent | Done | haiku | `pipeline.open-logs-folder` handler + `ShellExecuteA` |
| T15 | Build verifies clean (all 3 stages pass) | `dia run cluicheeditor` exits 0 | Done | — | ✓ pipeline complete 3 passed · 0 failed · 21.9s |
