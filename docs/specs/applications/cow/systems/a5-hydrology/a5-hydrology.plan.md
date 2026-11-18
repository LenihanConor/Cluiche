# Plan: A5 Hydrology (system)

**Spec:** @docs/specs/applications/cow/systems/a5-hydrology/a5-hydrology.md
**Status:** Not Started

This is the system-level tracker. Each row is a feature spec with its own plan, and the detailed tasks live there. A feature is Done only when its own plan's final task is Done.

A5 sits in the weekly world step between A4 and A1, so the order inside one step is A4 → A5 → A1, with A3 inline. The cross-system order lives in `worldstep.plan.md` and `a4-weather.plan.md`.

## Build order and parallelism

```
F1 data & parameters ──► F2 water pipeline ──┬──► F6 presets & regression (pure parts, R-Reach on the fixed driver)
                                             │
                                             └──► F3 module & World Step stage ──┬──► F4 save / load
                                                    (needs World Step F1-F2,      └──► F5 debug & history
                                                     A4 F3, DiaDeferredStep F1-F4)       (layers/commands need F3)
```

- **F1 is the root.** Its own unit tests need nothing unbuilt, because they use hand-built terrain. Real worlds need S-1 consumer amendment 8 (the basin table in `.wld.json`), which is first needed in F3.
- **F2** needs F1 and DiaDeferredStep F1 Task 1 (`IParallelExecutor`, `SerialExecutor` headers, built in `c83a94ed`).
- **F6** can be built straight after F2:
  - its loader, scripted driver, per-scenario checks and R-Reach all run on the fixed-input driver;
  - its `SLOW_` coupled A4+A5 R-Reach waits for A4 F3 and A5 F3.
- **F3** needs:
  - F1, F2;
  - World Step F1–F2, A4 F3 (`IWaterInputSource`, `IHydrologySpinUpHost`) and DiaDeferredStep F1–F4;
  - S-1 amendment 8 for real worlds.

  Its pure parts can start earlier: sink aggregation, influence translation, snapshot and the spin-up ticket, all tested with fakes.
- **F4** needs F3, World Step F3 and DiaDeferredStep F3. **F5** needs F3 for layers, commands and history wiring. Its pure parts (the command model, event detection on snapshots and health evaluation) can start after F2.
- **A4 coupling.** A4 Features 1–3 are amended for the hand-off (`waterToGround`, `glacierInput`, firn, A5 inputs latched at `Capture`). A5 F3 and A4 F3 meet at `SetHydrologyProvider(provider, host)` and `SetWaterSource`. Build them in either order behind fakes, then integrate.

## Sign-offs (AD-001)

None needed. All A5 code is CoW under `Cluiche/CoW`. Engine pieces are consumed, not changed: DiaDeferredStep, DiaCellLayers, DiaCellHistory, DiaValueSchema, `HexTopology`, `CellLayerGolden.h` and `SimQueryHost`.

A Dia need found mid-task is flagged and asked about, not placed in the game. An example is a scalar-series API in DiaCellHistory, which F5 deliberately avoids by recording totals as events.

## Cross-feature rules

- **Verification cadence** (feedback_verification_cadence): each task runs a narrow `dia run googletest --filter="<suite under work>" --shards 2` (for example `*HydrologyPipeline*`, always `--shards 2`); the feature-wide `*Hydrology*` filter runs only in that feature's scoped-verification task (2026-11-17, speed-ups). `SLOW_` suites (100-year runs, R-Reach, any run of 10 or more simulated years) are skipped by default and run once in the feature's scoped-verification task with `--filter="SLOW_*"` and once in the weather-stack close-out. The **full** suite, the single Release build and the cross-feature checks run once, in `worldstep.plan.md`, Weather-stack close-out. This plan's row 7 executes its cross-feature checks there:
  - deferred vs synchronous over three stages;
  - save mid-step;
  - R-Reach coupled with A4.
- Sanitizer: on request only (user, 2026-11-17). **No mutation checks.**
- **TDD** for all new public API (`.claude/skills/tdd.md`). RED is proven with quoted output. Mechanical tasks are exempt.
- **Gap audit** closes every feature plan (feedback_exhaustive_test_pass).
- **Layer goldens** (cell data over pixels): `hydrology_layers.golden.json`. F2 owns the `pipeline/` keys and F6 the `scenario/` keys. Regenerate only deliberately (`DIA_UPDATE_GOLDEN=1`).
- **Water balance** is checked in every pipeline-touching task (Feature 2's tolerance). A balance regression is a stop-and-debug, not a tolerance bump.
- **Commit after each task**, with the plan row updated in the same commit (`dia docs plan`).
- **Model column:** haiku and sonnet only. Ask before any opus dispatch (feedback_confirm_opus).
- **Perf is report-only, Debug ballpark** (`cow.hydro.step_ms`; feedback_quick_dirty_perf). No Release benchmarking rigs; a bad ballpark queues a perf pass, it does not gate. The one Release build is in the weather-stack close-out (2026-11-17, speed-ups).

## Tasks

Row number = feature number. Recommended build order: **1, 2, 6, 3, 4, 5**.

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | **Feature 1: Data & Parameters.** @hydrology-data.plan.md | Its final task | Not Started | — | Root. Pure logic on hand-built terrain |
| 2 | **Feature 2: Water Pipeline.** @water-pipeline.plan.md | Its final task | Not Started | — | Holds the design risk (cascade, basins, balance). Start after F1 |
| 3 | **Feature 3: Module & World Step Stage.** @hydrology-module.plan.md | Its final task | Not Started | — | Needs World Step F1–F2, A4 F3, DiaDeferredStep F1–F4 and S-1 amendment 8. Pure parts can start after F2 |
| 4 | **Feature 4: Save / Load.** @hydrology-save.plan.md | Its final task | Not Started | — | Needs F3, World Step F3 and DiaDeferredStep F3 |
| 5 | **Feature 5: Debug & History.** @hydrology-debug.plan.md | Its final task | Not Started | — | 8 layers, 6 sim-hosted names, history defaults, health. Pure parts can start after F2 |
| 6 | **Feature 6: Presets & Regression.** @hydrology-presets-and-regression.plan.md | Its final task | Not Started | — | Fixed-driver parts start after F2. The coupled `SLOW_` R-Reach waits for A4 F3 + A5 F3 |
| 7 | **System close-out.** System checks moved to worldstep.plan.md Weather-stack close-out (2026-11-17): this row is Done when that row is Done for this system's items. Cross-feature checks (executed in that combined close-out): the 104-week deferred-vs-synchronous oracle over `["weather","hydrology"]` and `["weather","hydrology","ecology"]` (once A1 is in). A save taken mid-step and reloaded is bit-identical. The coupled `SLOW_` R-Reach. `dia check deps`, `dia docs precommit`, `dia check spec-sync`. Full suite, Release build and sanitizer are not run by this row | All feature plans Done; pass/fail and failing names only | Not Started | haiku | Full run is the combined close-out's (feedback_scoped_gtest_runs). Report a failing cross-feature check, don't fix it in this row |
| 8 | **S-2.6 handoff.** When the GDD locks S-2.6 Rivers & Floods (its interview was requested 2026-11-17), write the Cluiche S-2.6 testbed spec (`/spec-system`) and add it to `BACKLOG.md` | Spec drafted and approved | Not Started | sonnet | Not an A5 deliverable. It's here so it isn't lost |
| 9 | **Close-out:** `dia docs spec-done docs/specs/applications/cow/systems/a5-hydrology/a5-hydrology.md --archive-plan` | Spec Done | Not Started | haiku | **Do not run until rows 1–7 are Done** |

## Open items

1. **The S-1 amendment 8 build sits outside this plan** (S-1 `consumer-amendments.plan.md` row 14). F3's real-world ingest needs it, so schedule it before A5 row 3.
2. **The A1 water-sink provider** (vegetation ET) is A1's to build, against A5 F3's `IWaterSinkProvider`. It isn't tracked here.
3. **The GDD open item:** the DRY_RIVERBED marker duration (20–100 years) is likely too long. Revisit it in S-2.6.
