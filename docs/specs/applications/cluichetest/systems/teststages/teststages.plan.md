# Implementation Plan: TestStages System

## Spec
@docs/specs/applications/cluichetest/systems/teststages/teststages.md

## Session Notes — Spec Decisions Summary

**Key constraints from the spec chain:**
- All modules use StringCRC IDs, no STL in public APIs (PD-001, PD-004)
- Stages are self-contained: own their scene, own their World/solver, register checkpoints in DoStart, auto-clear on stop (SD-TS-001, SD-TS-002)
- Star topology: every stage transitions to/from Boot only (SD-TS-004)
- Metrics emitted for threshold assertions, not per-frame (SD-TS-003)
- Fixed timestep on SimPU for physics/animation stages; MainPU for asset loading
- Visual HUD hidden when orchestrator is connected (CI-safe)
- TestResultsRegistry is session-scoped singleton, thread-safe (SimPU writes, MainPU reads)
- Boot menu queries manifest for stage list (DiaAPI bridge, not direct HTML→API); badges from registry
- `.diastage` loader must parse `transitions[]` before Boot menu filtering works
- Orchestrator CLI `--scenario` filters the plan JSON array, `--list` prints plan contents

## Strategy

**Phase 1 — Vertical Slice** (Infrastructure + RigidBody2D + Visual Feedback)
Prove the full pipeline end-to-end: manifest loader → Boot menu → stage navigation → checkpoint → HUD → return to Boot → badge. One stage is enough to validate all infrastructure paths.

**Phase 2 — Go Wide** (remaining 4 stages in parallel)
Once Phase 1 is green, stages are independent — different modules, different assets, no shared headers. All 4 can be dispatched in parallel.

## Implementation Patterns

### Phase 1 Patterns

**Manifest Loader Update:**
- File: `Dia/DiaApplicationFlow/Manifest/ApplicationManifestLoader.cpp`
- Pattern: In the stage parsing loop, read optional `"transitions"` JSON array → populate `StageDeclaration::transitions` DynamicArrayC. Read optional `"auto_advance"` bool. Default: empty transitions, false auto_advance.

**DiaAPI Command (`dia.manifest.stages`):**
- File: `Dia/DiaApplicationFlow/Application.cpp` in `RegisterBaselineCommands()`
- Pattern: Same as `dia.app.report` — `RegisterCommandJson` with category `"dia.manifest"`. Iterate loaded stages, filter where transitions contains `"Boot"`, return JSON array of names.

**BootUIPageModule Bridge:**
- Pattern: In `DoStart()`, after UIModule ready, call `dia.manifest.stages` internally (or read manifest directly). Store stage names. Expose to JS via Ultralight `JSObject` binding (same pattern as existing `RequestLaunchLevel` binding but in reverse — C++ pushes data to JS).

**TestResultsRegistry:**
- Pattern: `Dia::Core::Singleton<TestResultsRegistry>`. `DynamicArrayC<StageResultEntry, 16>` storage. Mutex-protected writes (infrequent). Enum state: kNotRun/kRunning/kPassed/kFailed/kTimeout.

**TestStageHUDModule (ImGui bottom bar):**
- Pattern: Module on MainPU. `DoUpdate` checks `IsHeartbeatActive()` — if true, return early. Otherwise query `AutomationService::GetRegisteredCheckpoints()` for current stage, query `TestResultsRegistry` for frame/budget, render ImGui window docked to bottom.

**RigidBody2DTestModule:**
- Pattern: Module on SimPU. Own `Dia::RigidBody2D::World`. Create 10 circles + 1 ground in `SetupScene()`. `DoUpdate` increments frame counter, checks `AreAllBodiesAsleep()`. On settle: `SetPassed` on registry, emit metrics.

### Phase 2 Patterns

**StateMachineAnimStageModule:**
- SimPU. 3 entities × (SM instance + AnimController). Frame-based triggers at 50/100. Asset-loaded clips (kLoading until ready). Two checkpoints polled in sequence.

**Animation2DStageModule:**
- SimPU. 1 Rig2D + 3 sequential clips. Completion callback chains next clip. Golden pose comparison at end (tolerance 0.001). kLoading until rig + clips loaded.

**AssetRuntimeStageModule:**
- MainPU. 4 concurrent async loads. Entry counter persists across DoStop/DoStart. First entry: snapshot. Second entry: compare. Two-entry orchestrator scenario.

**SoftBody2DStageModule:**
- SimPU. Rope (12 particles, chain, RB anchor) + Cloth (4×4, structural+shear, corner pins). Solver with 4 iterations. Velocity-threshold settle detection. Rope settles before cloth (assertion).

## Tasks

### Phase 1 — Vertical Slice

| # | Task | Spec Source | Status | Model | Notes |
|---|------|-------------|--------|-------|-------|
| 1.1 | Update `.diastage` loader to parse `transitions[]` and `auto_advance` | Infrastructure T1 | Todo | sonnet | Additive — existing files without field still load |
| 1.2 | Add `"transitions": ["Boot"]` to `dummy_stage.diastage` | Infrastructure T2 | Todo | haiku | First consumer of 1.1 |
| 1.3 | Implement `dia.manifest.stages` DiaAPI command | Infrastructure T3 | Todo | sonnet | In RegisterBaselineCommands(), filter by Boot in transitions |
| 1.4 | Update BootUIPageModule: query stages at DoStart, expose to JS | Infrastructure T4 | Todo | sonnet | Bridge: C++ reads manifest, passes array to JS binding |
| 1.5 | Update Boot UI HTML: render dynamic menu from binding | Infrastructure T5 | Todo | sonnet | Receive stage list from C++, render buttons |
| 1.6 | Fix `RequestLaunchLevel` to use `levelName` param | Infrastructure T6 | Todo | haiku | One-line: `TransitionTo(StringCRC(levelName))` |
| 1.7 | Add `--scenario` glob filter to orchestrator CLI | Infrastructure T7 | Todo | sonnet | fnmatch on plan's scenario array |
| 1.8 | Add `--list` flag to orchestrator CLI | Infrastructure T8 | Todo | haiku | Print plan entries, exit 0 |
| 1.9 | Create TestResultsRegistry (.h/.cpp) | Visual Feedback T1 | Todo | sonnet | Singleton, thread-safe, session-scoped |
| 1.10 | Create TestStageHUDModule (.h/.cpp) + bottom bar rendering | Visual Feedback T2-4 | Todo | opus | ImGui overlay, checkpoint icons, frame counter, PASS/TIMEOUT states |
| 1.11 | Implement orchestrator suppression in HUD | Visual Feedback T5 | Todo | haiku | Check IsHeartbeatActive() |
| 1.12 | Create RigidBody2DTestModule (.h/.cpp) | RigidBody2D T1-4 | Todo | sonnet | Full module: setup, update, settle detection, checkpoints, metrics, registry calls |
| 1.13 | Create RigidBody2D stage manifests (.diastage + .diaapp) | RigidBody2D T5-6 | Todo | haiku | transitions: ["Boot"], includes HUD module |
| 1.14 | Add RigidBody2D import to cluichetest.diagame | RigidBody2D T6 | Todo | haiku | |
| 1.15 | Add all new files to CluicheTest.vcxproj + filters | Infrastructure T11, VF T11, RB T7 | Todo | haiku | Registry, HUD, RigidBody2D module files |
| 1.16 | Update BootUIPageModule to query registry + expose badges to JS | Visual Feedback T8 | Todo | sonnet | Badge data alongside stage list |
| 1.17 | Update Boot UI HTML: render badges + summary line | Visual Feedback T9 | Todo | sonnet | ✓/✗/— icons, "N/M passed" |
| 1.18 | Write RigidBody2D pytest scenario | RigidBody2D T8 | Todo | sonnet | poll_checkpoint + metric assertion |
| 1.19 | Add scenario to plan JSON | RigidBody2D T9 | Todo | haiku | |
| 1.20 | Verify: Infrastructure smoke test (Boot menu shows stages) | Infrastructure T9 | Todo | sonnet | dia run cluichetest → Boot shows RigidBody2D |
| 1.21 | Verify: Manual run (Boot → RB2D → HUD → settle → PASS → Boot → badge) | RB2D T10, VF T12 | Todo | opus | Full vertical slice validation |
| 1.22 | Verify: Orchestrator run (dia orchestrate passes) | RB2D T10 | Todo | sonnet | Automated E2E green |

### Phase 2 — Go Wide (after Phase 1 verified)

| # | Task | Spec Source | Status | Model | Notes |
|---|------|-------------|--------|-------|-------|
| 2.1 | StateMachineAnim Stage — full implementation | statemachineanim-stage.md T1-11 | Todo | sonnet | 3 entities, asset clips, frame triggers, 2 checkpoints |
| 2.2 | Animation2D Stage — full implementation | animation2d-stage.md T1-13 | Todo | sonnet | Rig2D, sequential clips, golden pose, 2 checkpoints |
| 2.3 | AssetRuntime Stage — full implementation | assetruntime-stage.md T1-12 | Todo | sonnet | 4 asset types, two-entry reload, MainPU |
| 2.4 | SoftBody2D Stage — full implementation | softbody2d-stage.md T1-12 | Todo | sonnet | Rope + cloth, RB anchor, settle detection |
| 2.5 | Add registry calls + HUD module to all Phase 2 stages | Visual Feedback T6, T10 | Todo | haiku | SetRunning/SetPassed in each stage, HUD in each .diaapp |
| 2.6 | Add all Phase 2 scenarios to plan JSON | All stage specs | Todo | haiku | Plan ordering: smoke → rb2d → anim → sm_anim → asset → soft |
| 2.7 | Verify: Full suite orchestrator run (all stages green) | All specs | Todo | sonnet | dia orchestrate --suite=cluichetest/default |
| 2.8 | Verify: Manual run-all (Boot → each stage → badge wall) | Visual Feedback | Todo | sonnet | All badges green after manual run-through |

## Parallelization Notes

**Phase 1 — mostly sequential:**
- 1.1-1.2 (manifest loader) → 1.3 (DiaAPI) → 1.4-1.5 (Boot UI) — dependency chain
- 1.6 can run in parallel with 1.3-1.5
- 1.7-1.8 (CLI) independent of 1.1-1.6
- 1.9-1.11 (registry + HUD) can start alongside 1.3-1.5 (no dependency)
- 1.12-1.15 (RigidBody2D module) can start after 1.9 (needs registry)
- 1.16-1.17 (Boot badges) after 1.9 (needs registry) and 1.4-1.5 (needs Boot menu)
- 1.18-1.19 (pytest) independent
- 1.20-1.22 (verify) last — needs everything

**Phase 2 — fully parallel:**
- 2.1, 2.2, 2.3, 2.4 are independent (different modules, different assets, no shared headers)
- 2.5 touches each stage's .diaapp but no code conflicts
- 2.6-2.8 are verification gates after all stages complete

## Blockers — Verified 2026-05-22

| # | Question | Result | Action |
|---|----------|--------|--------|
| B1 | Does ImGui exist in CluicheTest? | **Available** — `DiaImGui` module exists (`DiaImGuiManager`, `SFMLImGuiBackend`). Not yet referenced by CluicheTest. | Add DiaImGui project reference to CluicheTest.vcxproj (Task 1.15) |
| B2 | Does `ApplicationManifestLoader` have a stage-parsing loop? | **Yes** — `ApplicationManifestLoaderV2::LoadFromString` parses into `ApplicationManifestV3` which has `StageDeclaration` with `transitions` + `autoAdvance` fields. JSON parser may not populate them yet. | Task 1.1: add parsing of `transitions[]` and `auto_advance` from JSON |
| B3 | Does DiaRigidBody2D have `IsAsleep()` on bodies? | **Yes** — `Body2DBase::IsAwake()` (line 57). Sleep system fully implemented with `SleepState`, timers, `UpdateSleepTimers()`. | Spec says `IsAsleep()`; map to `!body->IsAwake()` in implementation |
| B4 | Does AutomationService expose `IsHeartbeatActive()`? | **Missing** — `mHeartbeatEnabled` is private, no getter. | Add `bool IsHeartbeatEnabled() const { return mHeartbeatEnabled; }` to AutomationService.h (one-line, Task 1.11) |

## Status

`Active` — Phase 1 in progress
