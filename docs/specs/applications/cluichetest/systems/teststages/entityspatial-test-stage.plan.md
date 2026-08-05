# Plan: EntitySpatialTestStage

**Spec:** @docs/specs/applications/cluichetest/systems/teststages/entityspatial-test-stage.md
**Status:** In Progress

---

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Create `EntitySpatialTestStageModule.h/.cpp` skeleton: DoStart/DoUpdate/DoStop, 8 checkpoints registered, 6 metrics registered, Domain + EntitySpatialModule constructed | Build passes; `entityspatial.index_initialized` fires | Done | sonnet | |
| 2 | Implement `SpawnAgents`: 32 entities with SpatialComponent (4 layer categories, sine-wave initial positions) | Agents in domain at frame 0 | Done | sonnet | Depends on T1 |
| 3 | Implement `MoveAgents`: per-frame sine-wave update + MarkDirty; destruction at frame 90 | `destroy_sweep` checkpoint passes | Pending | sonnet | Depends on T2 |
| 4 | Implement `RunQueries` + `CheckCheckpoints`: all 5 query shapes, layer-mask reduction at frame 60, knearest k=5, rotating ray, all checkpoint booleans | All 8 checkpoints pass | Pending | sonnet | Depends on T3 |
| 5 | Implement `UpdateMetrics`: emit 6 metrics per frame | Metric assertions pass | Pending | haiku | Depends on T4 |
| 6 | Implement debug visuals: grid, agent circles + trails, player diamond, 5 query overlays, destroy flash, KNearest lines | Visual overlays render | Pending | sonnet | DIA_DEBUG only; depends on T4 |
| 7 | Manifests + vcxproj: `.diastage`, `.diaapp`, `cluiche_main.diaapp`, `cluichetest.diagame`, `CluicheTest.vcxproj` | `dia validate manifest` passes; stage in Boot menu | Done | haiku | Parallel with T2–T6 |
| 8 | Write pytest scenario `entityspatial_stage/smoke.py`; register in `default.json` | Collected by `dia test e2e --list` | Done | haiku | Parallel with T1–T7 |
| 9 | `dia run cluichetest` — navigate to stage; all 8 checkpoints pass | Visual + automated gate | Pending | sonnet | Requires T1–T7 |
| 10 | `dia run e2e --scenario entityspatial_stage` — pytest green | E2E green | Pending | sonnet | Requires T9 |
| 11 | `dia docs registry` + `dia docs spec-done` + commit | Done | Pending | haiku | Requires T10 |
