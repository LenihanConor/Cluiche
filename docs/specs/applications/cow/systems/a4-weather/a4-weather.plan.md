# Plan: A4 Weather (system)

**Spec:** @docs/specs/applications/cow/systems/a4-weather/a4-weather.md
**Status:** Not Started

System-level tracker. Each row is a feature spec with its own plan; the detailed tasks live there. A feature is Done only when its own plan's final task is Done. The three weather systems share one cross-system order: see `worldstep.plan.md` and `s25-weatherpatterns.plan.md`.

## Build order and parallelism

```
F1 data & parameters ──┬──► F2 CA pipeline ──┬──► F6 presets & regression
                       │                      │
                       │                      └──► F3 module & deferred step ──┬──► F4 save / load
                       │                              (needs World Step F1-F2    └──► F5 debug
                       └─────────────────────────────  and DiaDeferredStep F1-F4)       (F5 layers/commands need F3)
```

- **F1 is the root** and needs nothing unbuilt (the S-1 biome widening to 13 is built). Start here.
- **F2** needs F1 and DiaDeferredStep F1 Task 1 (`IParallelExecutor`, `SerialExecutor` headers).
- **F6**'s loader, selector, fill and fixture tasks need only F1; its forcing and regression tasks need F2 built. Its tests are what judge A4's default baseline numbers.
- **F3** needs F1, F2 and F6, World Step F1 and F2, and DiaDeferredStep F1 to F4. Parts testable with fakes (spin-up job, queue, stage derivation) can start earlier.
- **F4** needs F3, World Step F3 and DiaDeferredStep F3. **F5** needs F3 (layers, commands, queue) and F6 (presets); its pure parts (overlay plan, command model, detectors) can start after F2.
- **No A4 task blocks on S-1 code.** Everything runs on hand-built hex terrain until F3.

## Sign-offs (AD-001)

None new for A4. Cross-system items that touch Dia and are owned elsewhere:

| Change | Where | Status |
|---|---|---|
| `IDeferredStep::ValidatePending` | DiaDeferredStep (World Step F3 task) | Given 2026-11-16; code edit not yet made |
| Additive `SimQueryHost::Execute(name, args)` | DiaDebugServer (S-2.5 F4 spike) | **Given 2026-11-16** (only if the S-2.5 F4 spike finds no local dispatch) |

## Cross-feature rules

- **Verification cadence** (feedback_verification_cadence): scoped `dia run googletest --filter=... --shards 2` per task; each feature closes with a scoped sanitizer run where its spec asks for one. The **full** suite, Release build, and the cross-feature determinism and save/load checks run once, in the system close-out task below.
- **TDD** (`.claude/skills/tdd.md`) for all new public API. RED proven with quoted output. Mechanical tasks (vcxproj, plan updates) are exempt.
- **No mutation checks** (feedback_no_mutation_checks). **Gap audit** closes every feature plan (feedback_exhaustive_test_pass).
- **Layer goldens** (cell data over pixels): A4's golden file is owned by F6; F2 adds to it, F3 to F5 must not change a hash without regenerating deliberately (`DIA_UPDATE_GOLDEN=1`).
- **Commit after each task** with the plan row updated in the same commit (`dia docs plan`).
- **Model column**: haiku and sonnet only. No task is assigned opus; ask first if one needs it (feedback_confirm_opus).
- **Engine boundary**: all A4 code is CoW under `Cluiche/CoW`. A Dia edit found mid-task is flagged and asked about, not placed in the game (CLAUDE.md escalation rule).

## Tasks

Row number = feature number. Recommended build order: **1, 2, 6, 3, 4, 5** (see the diagram above).

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | **Feature 1 — Weather data & parameters.** @weather-data.plan.md | Its final task | Not Started | — | Pure logic. Root of A4 |
| 2 | **Feature 2 — CA pipeline.** @ca-pipeline.plan.md | Its final task | Not Started | — | Holds the design risk (10 GDD steps, determinism, region forcings). Start here after F1 |
| 3 | **Feature 3 — WeatherModule & deferred step.** @weather-module.plan.md | Its final task | Not Started | — | Deletes `JobSystemPlaceholderConsumerModule`. Needs World Step F1-F2 and DiaDeferredStep F1-F4 |
| 4 | **Feature 4 — Weather save / load.** @weather-save.plan.md | Its final task | Not Started | — | Its first task is the E1 + S-1 load event-order integration test |
| 5 | **Feature 5 — Weather debug.** @weather-debug.plan.md | Its final task | Not Started | — | 7 sim-hosted names, layers, wind overlay, events. Pure parts can start after F2 |
| 6 | **Feature 6 — Presets & regression.** @presets-and-regression.plan.md | Its final task | Not Started | — | Its regression tests decide whether defaults hold; may change the default baseline table and `cloud_decay` (tuning log) |
| 6a | **Feature 7 — Weather influence API** (added 2026-11-17). Spec not written: `/spec-feature` first | Its final task | Not Started | — | No dragon content (backlog review row). A5 applies the ground-water part. Needs F2 |
| 6b | **Feature 8 — Climate cycles** (added 2026-11-17). Spec not written: `/spec-feature` first | Its final task | Not Started | — | ~4-year and ~4-decade cycles; lengthens the coupled spin-up. Needs F2, F3 |
| 6c | **Feature 9 — Smoothing** (added 2026-11-17). Spec not written: `/spec-feature` first | Its final task | Not Started | — | Visual only, never feeds the CA; preview of F7 influences. Needs F3 |
| 7 | **System close-out.** Full `dia run googletest --shards 2`; `dia check sanitizer`; Release build of GoogleTests; the deferred-vs-synchronous determinism oracle and a save mid-step round trip over the real module (with World Step F3 built); `dia check deps`; `dia docs registry`; observation scan across all five pillars reported in the notes | All of the above | Not Started | haiku | Run and report only (pass/fail, failing names, errors). Then `dia docs spec-done` per feature and for the system once every child is Approved/Done |
| 8 | **GDD fold-back list.** Collect the deviations introduced by the feature specs (Feature 1 D-1 to D-3, Feature 6 D-6.1 to D-6.4 including the tuning log, Feature 2's 21st constant `cloud_decay` (20th after the drain moved to A5, 2026-11-17), week derived from the step day) into one note for the GDD session. **Do not edit the GDD** (feedback_gdd_edits_via_gdd_session) | — | Not Started | haiku | Docs only. Send the text to the CoW/GDD session |

## Open items carried from the specs

| Source | Item | When to decide |
|---|---|---|
| F6 ODQ 1 | Do the GDD ratios hold on a 40 × 24 hand-built terrain? Enlarge the fixture before touching tuning levers | First F6 regression run |
| F6 ODQ 3 | Direction expression for `coastal_storm` and `drought` presets (`towardLand` / `awayFromWater`) | F6 plan |
| F3 ODQ 1 | A re-announced identical world re-spins and may discard a week; needs S-1's content hash | When F-3 exposes `GetWorldContentHash` |
| F3 ODQ 2 | Spin-up wall time at the Slowest preset on a 2-worker cap is unmeasured | First S-2.5 run |
| F4 ODQ 1 | E1 load vs S-1 `WorldGenerationComplete` order: possible silent overwrite after load | F4 first task |
| F5 ODQ 3 | How the panel card gets SimPU values without the `debug.domain_state` cross-PU debt | F5 first task |

## Open-items review (2026-11-16)

All accepted by the user on 2026-11-16. These supersede the matching "Open items" lines in the feature plans.

| # | Decision |
|---|---|
| 3 | The coastal 1.5x ratio regression runs on the **66x42** fixture (the golden size), not 40x24. 40x24 stays for the fast tests |
| 5 | `WeatherStateRecord` is a plain struct created by **F3 Task 7**; F4's codec fills it (F3 precedes F4 in build order) |
| 10 | A re-announced identical world re-spins and may lose one in-flight week; accepted until F-3 exposes a content hash |
| 11 | `weather_parameters.json` lives at `Assets/CoW/Global/Weather/` |
| 12 | No `startWeekOffset` in the F6 regression tests (only the S-2.5 headless runner uses it) |
| 13 | `ca-pipeline` test terrains are hand-built at S-1's dimensions: Small 66x42, Large 104x64 |
| 18 | Test file names follow Feature 1's layout; save-field range names are checked in `weather-save` Task 4 |
| C | Settled by running: E1 load vs `WorldGenerationComplete` order (F4 Task 1), debug-panel stream and hotkey table (F5 spikes), spin-up wall time (first S-2.5 run) |
