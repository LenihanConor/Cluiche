# Plan: S-2.5 Headless Runner & Report (S-2.5 Feature 3)

**Spec:** @docs/specs/applications/cow/systems/s25-weatherpatterns/headless-runner.md
**Status:** Not Started
**System plan:** @docs/specs/applications/cow/systems/s25-weatherpatterns/s25-weatherpatterns.plan.md (row 3)

**Start conditions.** Needs **Feature 1 (stage-and-world)** built (the prechecks, `S25WeatherModule`,
`S25WorldTable`, the stage for the DiaAutomation task) and **Feature 2 (weather-checks)** built (the
checks, `IWeatherTrace`, `BaselineStore`, `ScriptedTrace`), plus A4 Features 2, 3, 5 and 6
(`WeatherSimulation`, `WeatherRunner`, the direct command sink, presets, `ApplyPreset`). Also built:
S-1's blocking `GenerateWorld` with a cancel flag, DiaAutomation, `JobSystemService` `TrySubmit`. The
mechanics tasks (1-13) need only Feature 2 and A4 pure code and use stubs for the world source, so they
can start before Feature 1's stage exists; tasks that use `S25WeatherModule` (16-18, 20) need Feature 1.

**Cross-plan dependencies.**
- Task 2 edits A4 Feature 3's `WeatherRunner` (additive start-week offset on `Start`). That is an **A4
  Feature 3 file change**; coordinate with that feature's plan and record the signature change there.
  CoW-only, no Dia edit (AD-001 N/A).
- Tasks 16 and 17 edit `S25WeatherModule` (Feature 1's file) to add the runner handle and commands.
- The system spec edits listed in the spec's Files Touched (Headless runner and Performance sections,
  A4 Feature 3's `WeatherRunner` signature, Feature 2's check-to-run mapping note) are made by the
  owner of those docs; task 25 lists them for the close-out, nothing here edits a spec.

No Dia files are added or edited.

## Implementation Patterns

### Files (namespace `CoW::S25`, S25 prefix, feature-local per AD-002)
```
Cluiche/CoW/Modules/S25HeadlessRunner.h/.cpp
Cluiche/CoW/Modules/S25TestPlan.h/.cpp
Cluiche/CoW/Modules/S25Report.h/.cpp          (build, verdict, serialize; atomic write)
Cluiche/CoW/Modules/S25LiveSmoke.h/.cpp
Cluiche/CoW/Shared/Weather/S25/Testing/{StubWorldSource,ScriptedCheckSet,InterruptingWriter}.h
Cluiche/Assets/CoW/Stages/S25WeatherPatterns/edge_cases.json        (cow.s25_edge_cases v1)
Cluiche/Tests/GoogleTests/CoW/S25/TestS25{Plan,Runner,Report,EdgeCases}.cpp, SLOW_S25Verdict suite
a DiaAutomation scenario (follow the F-2 pattern, location per that feature)
```
Register each file with `dia docs vcxproj-add CoW "<path>" --filter S25` when created. Test helpers
ship under `S25/Testing/` (feedback_testing_utilities).

### Conventions
- Public APIs use Dia containers and fixed strings; jsoncpp only at serialization edges (PD-004).
- The runner **shares no state with the live stage**: its own `WeatherSimulation`, its own world, no
  `ModuleRef`, no bus, no `GetCurrentWorld()`. Collaborators are seams: `IS25WorldSource`
  (blocking `GenerateWorld(seed, params, cancelFlag)` returning a `WeatherTerrain`+prechecks input),
  `IS25Launcher` (`JobSystemService::TrySubmit(fn, tag)` with tag `s25`, versus a **synchronous
  launcher** that runs the same function inline), and `IS25Clock`/`IS25Writer` for timing and file IO.
- Progress is a set of `std::atomic` fields in `S25RunHandle` (phase, run id, week, total, elapsed ms);
  `GetProgress` assembles a struct by atomic loads. This is an existing job-to-caller pattern (state
  private to one handle), not a cross-PU mutex read.
- Cancel is a `std::atomic<bool>` polled between weeks and between generation steps; cancelled runs
  never write the baseline and never write a report that claims a verdict other than `Cancelled`.
- Determinism: report fields other than timings and the `environment` block are reproducible;
  doubles are written at 17 digits with stable key order.

### Test plan table (tasks 1, 11-12)
`S25TestPlan` is a code table of ten `S25PlanRun { id, world, spinupWeeks, runWeeks, holdWeeks,
presetOrSetup, executorKind, feedsTests[], recordTrace, exclusive }` rows R1-R7, P1, P2 exactly as in
the spec (R5a/R5b, R4's 3 hold weeks, R3's `edge_inflow = 0` applied from the start including spin-up,
R7 52 + 208, P1/P2 exclusive and last with trace recording off). A test (AC 1) loads Feature 2's
registered ids and asserts every id is fed by at least one run. Setup commands are strings applied
through A4's direct command sink between weeks; the table also exposes each run's setup list so
Feature 4 can assert scenario setups equal plan setups (that assertion is Feature 4's).

### Runner (tasks 7-11)
- `Start(params)` returns an `S25RunHandle` and launches `RunPlan` through the launcher seam.
- Phases: Generating, Prechecks, Running(R#), Evaluating, Performance, WritingReport, Done|Cancelled.
- Worlds are generated once per `(world, seed, size)` and reused; runs execute sequentially; P1/P2 run
  last and exclusively.
- Simulation per run: `WeatherRunner::Start(terrain, params, baseline, spinupWeeks, executor, startWeek)`
  with `DefaultWeatherParameters()` plus the run's override, then `StepWeek` per week; setup commands
  applied through the direct sink; every committed week copied into a `RecordedTrace` (Feature 2) for
  runs with `recordTrace`.
- Executors: capped default, and the deterministic reverse-order 7-range executor for R2/R5b. Per amendment 3, reuse A4 Feature 2's
  test-local reverse-order and fixed-range executors; DiaDeferredStep's `Testing/` equivalents are used instead when they exist
  (consuming a Dia test helper is not a Dia edit). No new CoW-side executor adapter is written.
- Failure handling: generation failure/timeout -> `InvalidWorld` with the S-1 reason; a failed
  precheck -> dependent checks `InvalidWorld`, their runs skipped, verdict `InvalidWorld`.
- P2 (Large) runs **no prechecks and no checks** (amendment 5); it is performance only.
- Storm cell chooser: pure function over terrain: ocean cell nearest the map centre at least 6 steps
  from the edge, lowest index on ties, `InvalidWorld` if none.

### Verdict and report (tasks 3-6, 12-13)
- `DeriveVerdict(const DynamicArrayC<CheckResult>&, bool cancelled, bool invalidWorld)` with the exact
  precedence: Cancelled, InvalidWorld, HardFail, Incomplete, MagnitudeMisses, Pass. Performance is never
  an input. `Pass` (amendment 1): every Hard check is `Pass` and none is `InvalidWorld` or `Pending`; a Default check with no baseline
  entry is judged against the GDD default, and unrecorded entries are listed under `baseline.unrecorded[]` in the report without
  preventing `Pass`. Table-driven tests: one row per verdict plus precedence pairs (AC 14, 15).
- `S25RunReport` plus `BuildReportJson` produce the `cow.s25_report` v1 envelope with all spec sections;
  `WriteReportAtomically(path, json, IS25Writer&)` writes a temp file then renames. Output under
  `out/CoW/s25/` (PD-009), `report_<world>_<seed>.json` and `report_summary.json`.
- Performance block: `S25PerfBlock` with `Within|Over|NotMeasured (Debug)` labels, build config, core
  count and worker cap alongside; the CPU report goes to the report's `performance` section and the `INFO` log, and `cow.s25.status`
  shows the last reported values (amendment 4); budgets 5 s at Small for 52 weeks incl. spin-up, 4.2 ms Small and
  10 ms Large step CPU (A4's gate). Reported first, never a check.
- Edge cases: `S25EdgeCaseTable::Load` validates the 20 EC-01..EC-20 entries (missing, extra,
  duplicate, reason-less, `covered` naming an unknown run id all rejected) and the report embeds it.

### Commands (task 16)
Four sim-hosted names with help, category `s25`: `cow.s25.run` (`action` `start`/`cancel`), `cow.s25.report`, `cow.s25.baseline` (`record`),
`cow.s25.livesmoke` (amendment 6; the system spec's `cow.s25.run.cancel` and `cow.s25.baseline.record` are superseded); registered
**all-or-nothing together with Feature 1's two names** (AC 20) through `S25CommandRegistrar` (amendment 2), which `S25WeatherModule` owns
and Feature 1 task 17 creates: a forced failure of any removes all six. Feature 4's three `cow.scenario.*` names belong to `ScenarioModule`
and roll back separately. Each command
parses a JSON object, validates and names the offending field, and starts nothing on error.

### Live smoke (task 17)
`S25LiveSmoke` state machine on SimPU: Fastest speed, 52 weeks in the current mode, sample
`cow.weather.step_ms` per applied step and the `cow.worldstep.stalls` delta in Deferred, write
`out/CoW/s25/livesmoke.json`; assert only a sanity range for wall time (expected ~12.1 s, speed-capped);
Debug labels the budget `NotMeasured`.

### Verification cadence
- Per task: `dia run googletest --filter="S25Plan*:S25Runner*:S25Report*:S25EdgeCases*" --shards 2`.
- `SLOW_S25Verdict*` runs only in tasks 19, 21 and the final run (it generates real worlds).
- One full run in task 24. No mutation checks. Release perf gates are reported, not enforced.
- Always `dia run` / `dia launch`; kill leftover `CluicheTest.exe` before any automation run.

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 0 | Create the module and test files and `Testing/` helper stubs; `dia docs vcxproj-add` each; confirm Feature 1/2 headers (`S25WorldPrechecks`, `IWeatherTrace`, `RecordedTrace` interface, `BaselineStore`) and A4 `WeatherRunner`, direct sink, presets exist | Project compiles | Not Started | haiku | Mechanical, TDD-exempt. Mark blocked in the system plan if a prerequisite is absent |
| 1 | RED -> GREEN: `S25TestPlan` code table (ten runs R1-R7, P1, P2) with per-run setup lists, and the registry-versus-plan test that every Feature 2 check id is fed by a run | AC 1 | Not Started | sonnet | Table values copied exactly from the spec; Test 5 runs on R1's natural 65 weeks (deviation D-3.3) |
| 2 | RED -> GREEN: **`WeatherRunner` start-week offset** (additive `Start` parameter defaulting to 0): the offset test, a `winter_snow` preset run's first week is Winter and the calendar advances from it | AC 9 (offset half) | Not Started | sonnet | **Cross-plan: A4 Feature 3 file change.** Existing A4 runner tests must stay green (run `WeatherRunner*` too). Update the A4 Feature 3 plan notes |
| 3 | RED -> GREEN: `DeriveVerdict` with the six verdict rows and the precedence order via `ScriptedCheckSet` | AC 14 | Not Started | sonnet | Pure. One test per row plus every adjacent precedence pair. `Pass` follows amendment 1: all Hard checks `Pass`, none `InvalidWorld`/`Pending`; a Default check with no baseline entry does not prevent `Pass` (add a row: unrecorded Default met against the GDD default gives `Pass`) |
| 4 | RED -> GREEN: `S25RunReport` model and `BuildReportJson` (`cow.s25_report` v1, all sections), `reopenSection` on Hard fails and the Test 7 "goes to the user" text, performance block labels | AC 17 (envelope), AC 19, AC 15 (over-budget run keeps verdict) | Not Started | sonnet | Report lists unrecorded Default checks under `baseline.unrecorded[]` as informational (amendment 1). Reports 17-digit doubles, stable key order; perf block listed first. An artificially over-budget perf block must not change the verdict |
| 5 | RED -> GREEN: atomic report writer (temp file then rename) and the interrupted-write test via `InterruptingWriter` finding no partial file | AC 17 (atomic) | Not Started | sonnet | IO behind `IS25Writer` so the test needs no real disk fault |
| 6 | RED -> GREEN: `S25EdgeCaseTable` loader/validator (missing, extra, duplicate, reason-less entry; `covered` naming an unknown plan run id) and the report reproducing the table | AC 18 | Not Started | sonnet | Validator tests use inline JSON for 20-entry fixtures; the real file is task 14 |
| 7 | RED -> GREEN: runner mechanics with `StubWorldSource` and the synchronous launcher: a full plan on hand-built terrain completes and the report has every section | AC 2 | Not Started | sonnet | First end-to-end slice; fake clock and stub `WeatherRunner` setup commands as needed. World generated once per `(world, seed, size)` |
| 8 | RED -> GREEN: progress and cancel: `GetProgress` readable from another thread, phases advance, cancel during Generating, Running and Evaluating stops within one week or one generation step, verdict `Cancelled`, no baseline write | AC 4, AC 5 | Not Started | sonnet | AC 5 also gets a sanitizer check in the final run (`dia check sanitizer` with the `S25Runner*` filter). Use a counter-gated stub world source, never sleeps |
| 9 | RED -> GREEN: world failure paths: generation failure or timeout -> `InvalidWorld` with the S-1 reason; failed precheck -> dependent checks `InvalidWorld` and their runs not executed | AC 6, AC 7 | Not Started | sonnet | Uses Feature 1's prechecks on terrain that fails. Verify run list in the report shows skipped runs |
| 10 | RED -> GREEN: deterministic storm-cell chooser (nearest centre, >= 6 from edge, lowest index tie, `InvalidWorld` when none) | AC 8 | Not Started | sonnet | Pure function on hand-built terrain; include an exact-tie fixture |
| 11 | RED -> GREEN: preset runs skip spin-up and start the calendar at the preset season (using the task 2 offset); R3 applies `edge_inflow = 0` from the start including spin-up | AC 9, AC 10 | Not Started | sonnet | R3 state after spin-up must differ from R1's on a hand-built terrain with an upwind edge |
| 12 | RED -> GREEN: performance runs P1/P2: exclusive and last, trace recording off, timings and per-step CPU statistics (max/mean/p95) from `GetLastProfile()`, budgets labelled `Within`/`Over`/`NotMeasured (Debug)` with environment block | AC 15, AC 2 (performance section) | Not Started | sonnet | P2 (Large) runs no prechecks and no checks (amendment 5): test that no precheck/check result is produced for P2. Figures go to the `performance` section and the `INFO` log; `cow.s25.status` shows the last values (amendment 4). Report-only (deviation D-3.1). Large uses the same preset+seed at `Large`; unmeasured in Debug |
| 13 | RED -> GREEN: baseline handling in the runner: `record_baseline` writes absent entries only, a second run compares within +-10%, cancelled run writes none, `cow.s25.baseline record` semantics (no force leaves existing byte-identical; refused while a run is active) | AC 16, AC 4 (no baseline on cancel) | Not Started | sonnet | Uses Feature 2's `BaselineStore::Record`; command wiring is task 16 |
| 14 | **Author `edge_cases.json`** (`cow.s25_edge_cases` v1) from `A4_EdgeCaseMatrix.md` (`C:\GitHub\DesignDocs\COW\Output Docs\GDD\08_SystemsDeepDive\A4_Weather\`): all 20 cases EC-01..EC-20 exactly once, each `covered` (run ids from the plan table plus the setup that exercises it) or `untested` with a reason | AC 18 (the shipped file passes the task 6 validator) | Not Started | sonnet | Content task: needs the matrix's 20 sections read, then mapped to R1-R7/P1/P2. Do not claim coverage a run does not provide; unclear mappings go in the notes for the user. Register the asset |
| 15 | RED -> GREEN: real determinism on hand-built terrain with real A4: R1 vs R2 byte-identical committed fields at weeks 13, 26, 39, 52 (capped vs reverse 7-range), and the same plan twice gives identical `measured` and report apart from timings/environment | AC 11, AC 13 | Not Started | sonnet | Uses A4 Feature 2's test-local reverse-order and fixed-range executors (or DiaDeferredStep's `Testing/` equivalents when they exist; amendment 3). One real-A4 run, small terrain. Any difference is an A4 or executor bug: stop and report, never widen a tolerance |
| 16 | RED -> GREEN: the four commands (`cow.s25.run` with `action` `start`/`cancel`, `cow.s25.report`, `cow.s25.baseline` with `record`, `cow.s25.livesmoke`) added to `S25WeatherModule`'s `S25CommandRegistrar`, all-or-nothing together with Feature 1's two names, argument validation naming the field, start-while-active and cancel-with-no-run errors | AC 20, AC 21 | Not Started | sonnet | **Edits Feature 1's `S25WeatherModule`.** Forced failure of any of the six leaves none registered (amendment 2). Extend Feature 1 task 17's `S25CommandRegistrar`; no second helper. The superseded dotted names are not registered |
| 17 | RED -> GREEN: `S25LiveSmoke` and `cow.s25.livesmoke` writing `out/CoW/s25/livesmoke.json` (52 weeks of step times, wall time, stall delta in Deferred, Debug label `NotMeasured`) | AC 23 | Not Started | sonnet | Needs Feature 1's step modes. Wall-time assertion is a sanity range only |
| 18 | RED -> GREEN: live-vs-runner agreement and isolation: the stage's Sync module and the runner on the same world and parameters give byte-identical committed fields for weeks 1-13; a run started while the stage steps leaves the stage's committed fields bit-identical to a stage-only run | AC 12, AC 3 | Not Started | sonnet | Needs Feature 1's module. Where the live side changes size, it waits on `WeatherModule::IsReady(revision)` for the new revision (amendment 7). Open Design Question 2 (blocking vs stepped generation agreement) is exercised here: record the outcome |
| 19 | RED -> GREEN: `SLOW_S25Verdict.CuratedWorldsSmall`: runs the whole plan (synchronous launcher, serial and reverse executors, no performance runs) on both curated worlds, fails on `HardFail`/`InvalidWorld`/`Incomplete` naming checks, `reopenSection` and the failing precheck, warns on `MagnitudeMisses`, writes reports under `out/`; scripted fail/warn variants prove the behaviour | AC 24 | Not Started | sonnet | Green means `Pass`, or `MagnitudeMisses` reported as warnings (amendment 1); an unrecorded-baseline `Pass` is green, so the test needs no `record_baseline`. The scripted variants run in the fast suite; only the real-world run is `SLOW_`. Strict (no known-failures list) per Open Design Question 1 |
| 20 | RED -> GREEN: DiaAutomation scenario: enter the stage, `cow.s25.run`, poll `cow.s25.report` to completion, read the verdict from the report file and compare with the expected verdict | AC 22 | Not Started | sonnet | Follow F-2's pattern; kill leftover `CluicheTest.exe`; E2E does not build, so build with `dia run` first. Needs the curated picks from Feature 1 task 23 |
| 21 | **PROOF: first real run on the curated worlds**: `dia run googletest --filter="SLOW_S25Verdict*" --shards 2`, then `cow.s25.run` with `record_baseline: true` on both worlds and the P1/P2 performance runs (Release if available, else Debug labelled); record in this plan: verdict per world, every check state with `measured` and `detail`, every Hard `Fail` with its `reopenSection`, `MagnitudeMisses`, `InvalidWorld`, perf numbers and the baseline written | Results recorded below under "Proof results"; AC 24 on real worlds, AC 16 baseline recorded | Not Started | sonnet | **Hard fails here are findings, not task failures**: do not tune A4, the thresholds or the checks to make them pass. A Test 7 `years_differ` Fail "goes to the user". Record raw numbers so the user can tell definition vs A4. Verify weather content by report data, not screenshots (feedback_cell_data_over_pixels). Ask before escalating |
| 22 | Exhaustive test pass: gap audit of every AC (1-24); edge pass (empty plan filter, unknown world/size, run started twice, cancel at week 0 and at the last week, report path unwritable, a world with no ocean, zero hold weeks, edge_cases entry referencing a skipped run, resumed-after-cancel handle reuse) | Gap list in notes; every AC mapped to a test or a stated deferral | Not Started | sonnet | No mutation checks. Scoped filter `S25*` |
| 23 | Observation Opportunity Scan over all touched files (logs for phase changes and failures, `DIA_TRACE_ZONE` per run, profile scopes, metrics for run duration, health for failed runs) | Suggestions in notes | Not Started | sonnet | Suggestions do not block Done |
| 24 | One full verification run: `dia run googletest --shards 2` (full suite incl. `SLOW_S25Verdict`), `dia check sanitizer` over `S25Runner*`, `dia check deps`, `dia check cppcheck`, `dia validate manifest` | All green or failures named; AC 5 sanitizer clean | Not Started | sonnet | Single full run. Report pass/fail and failing names only |
| 25 | List the doc edits owed by this feature (system spec Headless runner and Performance sections, A4 Feature 3 `WeatherRunner` signature, Feature 2 check-to-run mapping note) in the system plan | Entries present in the system plan notes | Not Started | haiku | Docs edit only; specs are edited by their owners, not this task |
| 26 | Close-out: `dia docs plan docs/specs/applications/cow/systems/s25-weatherpatterns/headless-runner.plan.md <#> --status Done` per row; `dia docs spec-done docs/specs/applications/cow/systems/s25-weatherpatterns/headless-runner.md`; update system plan row 3 | Spec status Done | Not Started | haiku | **Do not run until all prior rows are Done.** |

## Proof results

Filled in at task 21; empty until then.

| World | Verdict | Hard fails (check, measured, reopen) | Magnitude misses | Invalid / pending | Perf (P1, P2) | Baseline written |
|---|---|---|---|---|---|---|
| `s25_coast_range` | | | | | | |
| `s25_dry_interior` | | | | | | |

## Open items

Resolved items are kept for traceability.
- **First-run `Pass`**: **Resolved by amendment 1 (2026-11-16)**: a Default check with no baseline entry is judged against the GDD default; `baseline.unrecorded[]` is informational. Task 21 still records the baseline with `record_baseline: true` as part of the proof.
- **Cross-feature all-or-nothing registration**: **Resolved by amendment 2 (2026-11-16)**: `S25CommandRegistrar` owned by `S25WeatherModule`.
- **Two-run checks**: **Resolved by Feature 2 amendment 1 (2026-11-16)**: `CheckInputs.comparison`; the runner pairs R1/R2, R5a/R5b and R1/R3 into `CheckInputs`.
- **Test 3 release week and `WorldInfo`**: **Resolved by Feature 2 amendment 2 (2026-11-16)**: R4's `holdWeeks`, release week and spawn cell fill `WorldInfo`.
- **Command naming**: **Resolved by amendment 6 (2026-11-16)**.
- **P2 prechecks**: **Resolved by amendment 5 (2026-11-16)**: none.
- **Where the reordering executor lives**: **Resolved by amendment 3 (2026-11-16)**: reuse A4 Feature 2's, or DiaDeferredStep's `Testing/` when it exists.
- **Where the CPU report goes**: **Resolved by amendment 4 (2026-11-16)**.
- **DiaAutomation scenario file location and format** are "follow F-2's pattern" only (still open).
- **Open Design Questions 1, 3 and 4** (known-failures list, exclusive perf runs, trace export) stay open; the plan builds the strict behaviour.
