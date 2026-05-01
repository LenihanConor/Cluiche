# Feature Spec: sub-step-visibility

## Parent System
@docs/specs/systems/dia/diapipelineeditor.md

## Status
`Done`

## Summary

Emit `OnStepStarted`/`OnStepCompleted`/`OnStepFailed` NDJSON events from DiaCLI stage handlers, forward them through the C++ tailer, add `StepState` to the React state model, and render a `StepRow` per step inside the expanded `StageDetail` view. One step failure cascades to stage failure (stage handler returns non-zero); interrupt detection remains stage-based only.

## Problem

Pipeline stages (compile-code, deploy) perform multiple distinct sub-operations (protobuf, cef-wrapper, msbuild, ui-builds, copy-files) but the editor showed only a single row per stage. When a build failed, users had no visibility into which internal step failed without reading the raw log. The `OutputContext` already had `step_started/completed/failed` methods but they were never called.

## Goals

- Emit step events from all stage handlers with correct `stage` + `step` labels
- Parse step events in the C++ tailer and store them in `mEvents` without corrupting `mUnmatchedStartedCount`
- Track `StepState[]` within each `StageState` in the React reducer
- Render steps as `StepRow` components inside `StageDetail`, above log lines
- Mark running steps as `interrupted` when an interrupted run is detected

## Non-Goals

- Config-driven steps (steps are hardcoded per handler, not in `pipeline.toml`)
- Step-level pass/fail counts in `RunSummary` (stage-level counting is sufficient)
- Log lines per step (steps show name, status icon, duration only)

## Steps by Stage Handler

| Stage | Step name | Condition |
|-------|-----------|-----------|
| `compile-code` | `protobuf` | Only when `build_deps.protobuf = true` and sentinel not present (or `--force`) |
| `compile-code` | `cef-wrapper` | Only when `build_deps.cef_wrapper = true` and lib not present |
| `compile-code` | `msbuild` | Always (main build step) |
| `deploy` | `ui-builds` | Only when `deploy.ui_builds` is non-empty |
| `deploy` | `copy-files` | Always (even when skipped due to staging; step started then completed immediately) |

## Acceptance Criteria

| # | Criterion |
|---|-----------|
| AC1 | `compile_code_stage.run()` emits `OnStepStarted`/`OnStepCompleted` for `msbuild` on success |
| AC2 | `compile_code_stage.run()` emits `OnStepStarted`/`OnStepFailed` for `msbuild` on non-zero exit |
| AC3 | `_build_protobuf()` emits `OnStepStarted`/`OnStepCompleted` on success; `OnStepFailed` on error |
| AC4 | `_build_cef_wrapper()` emits `OnStepStarted`/`OnStepCompleted` on success; `OnStepFailed` on error |
| AC5 | `package_stage.run()` emits `OnStepStarted`/`OnStepCompleted` for `copy-files` and `ui-builds` |
| AC6 | `OnStepStarted`/`OnStepCompleted`/`OnStepFailed` events appended to `mEvents` by C++ tailer |
| AC7 | C++ tailer `mUnmatchedStartedCount` is NOT affected by step events; interrupt detection unaffected |
| AC8 | `OnStepFailed` does not increment `RunSummary.failCount` (only `OnStageFailed` does) |
| AC9 | Reducer handles `OnStepStarted` → step `running`; `OnStepCompleted` → `passed`; `OnStepFailed` → `failed` |
| AC10 | Running steps are marked `interrupted` when `UPDATE_SUMMARY` sets `interrupted: true` |
| AC11 | `StageDetail` renders `StepRow` for each step above log lines when steps are present |
| AC12 | `StageDetail` still shows "No log output" only when both steps and log lines are empty |

## Tasks

| # | Task | Status |
|---|------|--------|
| 1 | Thread `output`/`system` through `pipeline_runner.py` handler calls | Done |
| 2 | Add step events to `compile_code_stage.py` (`protobuf`, `cef-wrapper`, `msbuild`) | Done |
| 3 | Add step events to `package_stage.py` (`ui-builds`, `copy-files`) | Done |
| 4 | Accept `output`/`system` kwargs in `asset_build_stage.py` stub | Done |
| 5 | Add `OnStep*` StringCRC constants and pass-through handling in `PipelineLogTailer.cpp` | Done |
| 6 | Add `StepState` interface and `steps: StepState[]` to `StageState` in `types.ts` | Done |
| 7 | Add `OnStepStarted`/`OnStepCompleted`/`OnStepFailed` reducer cases + `findOrCreateStep` helper | Done |
| 8 | Update `UPDATE_SUMMARY` interrupted path to mark running steps interrupted | Done |
| 9 | Create `StepRow.tsx` component | Done |
| 10 | Update `StageDetail.tsx` to accept and render steps | Done |
| 11 | Update `StageRow.tsx` to pass `steps` to `StageDetail` | Done |
| 12 | Python unit tests for step events (compile, deploy, runner forwarding) | Done |
| 13 | C++ tailer unit tests for step event storage and interrupt safety | Done |
| 14 | Reducer unit tests (`pipelineReducer.steps.test.ts`) | Done |
| 15 | `StageDetail` component tests updated for `steps` prop | Done |

## Binding Decisions Compliance

| Source | ID | Decision | Compliance |
|--------|----|----------|------------|
| Platform | PD-001 | StringCRC for IDs | `kEventOnStepStarted` etc. use StringCRC |
| Platform | PD-004 | No STL in public APIs | Step name in `PipelineEvent.step` is StringCRC |
| Dia | AD-003 | `Dia::<Module>::` namespace | Unchanged |
| DiaPipelineEditor | SPE-005 | Interrupted detection via unmatched Stage Started | Step events explicitly excluded from `mUnmatchedStartedCount` |

## AI Review Questions

| # | Question | Answer |
|---|----------|--------|
| 1 | Could step events exhaust the 64KB string pool? | Each step emits at most 3 events (started/completed/failed); step names are short (<20 chars). Even 100 steps would add ~6KB to the pool, well within limits. |
| 2 | Should protobuf/cef-wrapper steps be skipped (no started/completed) when already up to date? | Yes — skipped sub-steps emit no events. The sentinel/lib check returns 0 before `step_started` is called, so no ghost steps appear in the UI. |
| 3 | Does `run_deploy()` (called by `dia pipeline deploy`) need step events? | No — `run_deploy` is a standalone MSBuild post-build action, not driven through the pipeline runner's OutputContext. It has no `output` parameter and step events are omitted there intentionally. |
