# Plan: ScalarFieldTestStage

**Spec:** @docs/specs/applications/cluichetest/systems/teststages/scalarfield-test-stage.md
**Status:** Todo

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Create `ScalarFieldTestStageModule.h/.cpp` — `SetupGrid` (20×15 topology, walls, swamp static modifiers, all 3 field instances), register 8 checkpoints, register 6 metrics | Build passes; `fields_initialized` checkpoint fires on first Tick | Todo | sonnet | Core skeleton |
| 2 | Implement `TickFields` — `WriteRadial` sources each frame, `Tick()` on Blue + Red, `Combine` into Combined; implement burst events at frame 60 and 180 | `blue_steady_state`, `red_steady_state`, `walls_respected`, `box_write_burst` pass | Todo | sonnet | Depends on Task 1 |
| 3 | Implement `RunSpatialQueries` — `FindLocalMaxima` on Blue, `FindCellsAboveThreshold` on Combined, `GetGradient` probe; set checkpoint flags | `local_maxima_found`, `contested_zone_stable`, `gradient_non_zero_at_probe` pass | Todo | sonnet | Depends on Task 2 |
| 4 | Implement visuals — register HeatmapOverlay (Blue + Combined) and GradientOverlay (Combined) with VisualDebuggerModule; burst flash effect; source pulse markers; local maxima X markers; HUD bar | Visual inspection — all overlays render, front line visible | Todo | sonnet | Depends on Task 1 |
| 5 | Create `.diastage` + `.diaapp` manifest files | `dia validate manifest` passes | Todo | haiku | |
| 6 | Import stage in `cluichetest.diagame`; add source files to `CluicheTest.vcxproj` | Stage appears in Boot menu; clean build | Todo | haiku | Depends on Tasks 1, 5 |
| 7 | Write pytest scenario `scalarfield_stage/smoke.py`; register in `default.json` | Scenario collected by `dia test e2e --list` | Todo | haiku | Can run in parallel with Tasks 1–6 |
| 8 | `dia run cluichetest` — navigate to ScalarFieldTestStage, visual verify all 8 checkpoints pass | Manual visual gate | Todo | sonnet | Requires Tasks 1–6 |
| 9 | Commit + `dia docs spec-done` | — | Todo | haiku | Requires Tasks 7 and 8 |
