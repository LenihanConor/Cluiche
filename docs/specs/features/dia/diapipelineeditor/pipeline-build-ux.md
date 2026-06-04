# Feature Spec: pipeline-build-ux

## Parent System
@docs/specs/systems/dia/diapipelineeditor.md

## Status
`Approved`

## Summary

Overhaul the Pipeline Editor UX to be a first-class game build tool: scoped to the loaded `.diagame` project, ghost stages visible before any run, timing estimates and relative bars during a run, failure reasons surfaced inline, raw stdout captured to disk, and a Build/Launch split-button so developers never need to leave the editor to iterate.

## Problem

The pipeline panel is a generic log viewer bolted onto a terminal workflow. It shows no stage structure before a run starts, drops raw compiler output on failure (making diagnosis require a terminal), has no timing context for running stages, and requires CLI to launch the built game. Developers context-switch out of the editor at every iteration step.

## Goals

- Scope the panel to the loaded `.diagame` — no generic target dropdown
- Show the full stage pipeline as ghost pills before a build starts
- Surface timing estimates and duration bars during a run
- Show failure reason inline without requiring drill-down
- Capture raw stdout so compiler errors are readable inside the editor
- Enable Build, Build+Launch, and standalone Launch from one button group
- History is session-only (5 runs max, no disk persistence)

## Acceptance Criteria

| # | Criterion |
|---|-----------|
| AC1 | When no `.diagame` is loaded, panel shows "No project loaded" overlay (matches SceneEditor/EntityTemplateEditor pattern) |
| AC2 | When a `.diagame` is loaded, the target name (filename without extension) is shown in the toolbar; no target dropdown |
| AC3 | Config (Debug/Release) dropdown remains; is the only user-configurable build parameter |
| AC4 | Before a build starts, all stages for the current target render as grey ghost pills in the correct order, using stage names from `pipeline.toml` |
| AC5 | A running stage shows `~Xs est.` derived from that stage's duration in the most recent history run |
| AC6 | Each step row shows a horizontal timing bar whose width is proportional to that step's duration as a fraction of the stage total |
| AC7 | When a step fails, its stage auto-expands and scrolls into view |
| AC8 | The last error-level log line from a failed step is shown inline under the step row (no drill-down required) |
| AC9 | A "Built X ago" label appears in the toolbar between builds, showing elapsed time since the last successful build |
| AC10 | Raw stdout from the build subprocess is captured to `Cluiche/out/DiaCLI/logs/pipeline/last-stdout.log` alongside the NDJSON |
| AC11 | An "Open logs folder" link opens that directory in Explorer; disabled (greyed, tooltip "No logs yet") if the path doesn't exist |
| AC12 | Primary button is `▶ Build`; a dropdown arrow beside it reveals "Build & Launch"; a standalone `↗ Launch` button appears (enabled) after a successful build |
| AC13 | History is session-only: at most 5 runs, cleared when the editor closes, not loaded from prior sessions |

## Non-Goals

- Editing `pipeline.toml` — still out of scope
- Multi-config (Debug+Release) in one run — still sequential per existing behaviour
- Build progress percentage — NDJSON doesn't carry this
- Any change to the NDJSON schema or DiaCLI event emission

## Data Models / API

### New bridge request: `pipeline.get-target-stages`

```json
// request
{}

// response
{ "stages": ["compile-code", "build-assets", "deploy"] }
```

Returns the `stages[]` array for the current `.diagame`-derived target from `pipeline.toml`. Returns `{ "stages": [] }` if no project is loaded or the target is unknown.

### Extended `pipeline.get-project-state`

```json
{ "isValid": true, "diagamePath": "C:/path/to/mygame.diagame", "target": "mygame" }
```

### Stdout log path

Captured to `{repoRoot}/Cluiche/out/DiaCLI/logs/pipeline/last-stdout.log` — sibling of `last-run.ndjson`. Overwritten on each build start.

### History cap change

`RunHistoryStore::kMaxRuns` reduced from 10 to 5. `LoadFromDisk()` no longer called on `Initialize()` — history resets each session. `SaveToDisk()`, `ArchiveStaleSession()`, `PruneSessions()`, and session archiving machinery removed.

### Ghost stage state (new reducer action)

```typescript
// New action
{ type: 'SET_STAGE_MANIFEST', stages: string[] }

// Ghost stage in StageState
{ name: string, status: 'not-started', durationMs: 0, ... }
```

`SET_STAGE_MANIFEST` fires on project load and initialises all stages as `not-started`. `OnRunStarted` resets them but preserves the order. If a run starts for a different stage count than the manifest, the run's stages win.

## Files Touched

**C++**
- `Dia/DiaPipelineEditor/Internal/PipelineTargetParser.h` — add `stages[]` per target
- `Dia/DiaPipelineEditor/PipelineEditorPlugin.h/.cpp` — project context wiring, get-target-stages handler, overlay push, diagame→target derivation
- `Dia/DiaPipelineEditor/PipelineBuildManager.h/.cpp` — stdout file handle, write to `last-stdout.log`
- `Dia/DiaPipelineEditor/RunHistoryStore.h/.cpp` — remove disk load, cap at 5, strip archive machinery

**UI (React/TypeScript)**
- `UI/src/state/types.ts` — add `lastSuccessTimestamp`, `stageDurationsFromHistory` to `PipelineState`; add ghost to `StageState`
- `UI/src/state/pipelineReducer.ts` — `SET_STAGE_MANIFEST`, `SET_PROJECT_STATE`; auto-expand on fail; `~Xs est.` derivation
- `UI/src/components/PipelinePanel.tsx` — no-project overlay
- `UI/src/components/PipelineToolbar.tsx` — remove target dropdown, show diagame name, split-button, Launch button, "Built X ago"
- `UI/src/components/StageTimeline.tsx` — pass `stageDurationsFromHistory` down
- `UI/src/components/StageRow.tsx` — ghost state, `~Xs est.`, auto-scroll ref
- `UI/src/components/StepRow.tsx` — timing bar, inline failure reason

## Binding Decisions Compliance

| ID | Source | Decision | Compliance |
|----|--------|----------|------------|
| SPE-002 | DiaPipelineEditor | Run history capped at 10, stored as JSON | **Revised by this feature**: cap lowered to 5, disk persistence removed. Session-only. SPE-002 updated in system spec. |
| SED-020 | DiaEditor | Plugin persistent output to `Cluiche/out/CluicheEditor/<Plugin>/` | stdout log goes to `Cluiche/out/DiaCLI/logs/pipeline/` alongside the existing NDJSON — same directory, same convention. |
| SED-021 | DiaEditor | Per-plugin session context via `.context.json` sidecar | Session archiving removed. `.context.json` and `.sessions/` machinery stripped from RunHistoryStore. |
| SPE-003 | DiaPipelineEditor | CEF message passing for C++→JS | New `pipeline.get-target-stages` and `pipeline.get-project-state` requests follow WebUIBridge pattern. |
| PD-004 | Platform | No STL containers in public APIs | PipelineTargetParser returns `Json::Value`; new stage data uses existing JSON types. |

## Open Design Questions

1. **diagame-to-target convention**: Target name is derived as `filename-without-extension` from the loaded `.diagame` path. If the target key in `pipeline.toml` doesn't match (e.g. game is `mygame.diagame` but target is `my-game`), `pipeline.get-target-stages` will return `[]` and ghost stages won't show. For now: silently show no ghosts and proceed. Worth adding a `"pipeline_target"` field to `.diagame` in a future spec if this becomes a friction point.

2. **Stdout file size**: `last-stdout.log` is overwritten each run. Long builds (MSBuild verbose) can produce large files. For now: no cap, whole stdout captured. Add a size cap if this causes problems in practice.

3. **Launch after Build&Launch fails mid-run**: If "Build & Launch" is chosen and the build fails, do we still launch? No — Launch only fires after `exitCode == 0`. Partially-built targets should not be launched.
