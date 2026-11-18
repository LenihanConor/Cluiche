# Feature Spec: S-2.5 Headless Runner & Report

**Parent:** @docs/specs/applications/cow/systems/s25-weatherpatterns/s25-weatherpatterns.md (Feature 3)
**Status:** Approved (2026-11-16)
**Design source:** `DesignDocs/COW/Output Docs/GDD/10_PrototypeStrategy/S2.5_WeatherPatterns.md` AD-06, AD-07, AD-09, §5, §6 (CHANGE-043)
**Depends on:** S-2.5 Feature 1 (stage, worlds, prechecks, module) and Feature 2 (checks, trace, baseline store); A4 Features 2, 3, 5, 6 (`WeatherRunner`, the direct command sink, presets); S-1 blocking `GenerateWorld` with a cancel flag (built); DiaAutomation (built); `JobSystemService` `TrySubmit` (built).
**Consumed by:** S-3 (trusts Living mode only after this passes), S-3.5 (uses the report's world and weather evidence), the SLOW test suite.

**Amended 2026-11-18 (world-sim review, user decisions):** see "World-sim review amendment" below.

## Summary

The weather testbed's verdict machine. `S25HeadlessRunner` generates the curated worlds itself, drives A4's **pure** `WeatherSimulation` through `WeatherRunner` and A4's direct command sink on a worker job, records every committed week into a trace, runs Feature 2's checks over it, measures performance, and writes a report. It needs no stage, no rendering and no bus, so it runs three ways: from the stage (`cow.s25.run`, with progress and cancel), from DiaAutomation, and from a `SLOW_` GoogleTest so a plain test run gives a weather verdict on every A4 change.

## Goals

1. One call produces a complete verdict for both curated worlds: every check, the world prechecks, performance numbers, and the edge-case coverage table, in one JSON report.
2. The runner shares **no state** with the live stage: it generates its own worlds and owns its own simulations, so a run cannot disturb what is on screen, and the stage cannot disturb the verdict.
3. A run is cancellable at any week and reports progress by phase.
4. The verdict is derived by one stated rule from the check states; performance never changes it (decided 2026-11-16).
5. The live module and the runner agree: the same world and weeks give byte-identical committed fields.
6. The same plan runs headless in a `SLOW_` GoogleTest with no stage.

## Non-Goals

- The checks, trace and baseline store (Feature 2). The check-line panel and scenarios (Feature 4). Graphs (Feature 5).
- The stage and world picker (Feature 1).
- Equation correctness (A4's GoogleTests).
- Verdict-grade results at Standard or Large (the Large run is performance only, per the system spec).

## Design

### The test plan (code-defined table)

`S25TestPlan` is a code table, not scenario files: Feature 4's scenario format does not exist yet, and the plan must be runnable without it. Feature 4's guided scenarios later reuse the same setup commands, and a test asserts each scenario's setup equals its plan run's.

| Run | World | Weeks | Setup | Executor | Feeds |
|---|---|---|---|---|---|
| R1 base-A | `s25_coast_range` | spin-up 52 + **65** | none | capped, default ranges | T1, T4, T5, T9, T8 (the "on" run) |
| R2 base-B | `s25_coast_range` | spin-up 52 + 65 | none | reverse order, 7 ranges | T9 |
| R3 edge-off | `s25_coast_range` | spin-up 52 + 52 | `boundary.edge_inflow = 0` from the **start** (including spin-up) | capped | T8 |
| R4 storm | `s25_coast_range` | spin-up 52 + 3 hold + 20 | storm at the chosen cell, held 3 weeks, then released | capped | T3 |
| R5a shadow-A | `s25_coast_range` | 13 | preset `rain_shadow`, no spin-up | capped | T2 |
| R5b shadow-B | `s25_coast_range` | 13 | preset `rain_shadow`, no spin-up | reverse order, 7 ranges | T2 (repeatable) |
| R6 drought | `s25_dry_interior` | 52 | preset `drought`, no spin-up | capped | T6 |
| R7 years | `s25_dry_interior` | spin-up 52 + **208** | none, no forcing | capped | T7 |
| P1 perf-small | `s25_coast_range` | spin-up 52 + 52, exclusive | none; trace recording off | capped | perf only |
| P2 perf-large | same preset + seed, `Large` | spin-up 52 + 52, exclusive | none | capped | perf only |

- **Preset runs skip the spin-up** (a preset reseeds every field, so spin-up would be discarded) and start the calendar at the preset's season through a new `WeatherRunner` start-week offset (additive amendment to A4 Feature 3's runner, below).
- **Test 5** runs on R1's **natural** 65 weeks (emergence, stronger than the GDD's preset-assisted setup); the `winter_snow`/`spring_melt` presets are the guided versions in Feature 4.
- **Storm cell** is chosen deterministically: the ocean cell nearest the map centre that is at least 6 steps from the map edge, lowest index on ties. If none exists the run reports `InvalidWorld`.
- **Test 9 / Test 2 repeatability** compare an executor with real parallel ranges (A) against a deterministic reverse-order 7-range executor (B); passing proves A4's bit-identity on a real world, which is stronger than two identical runs.
- Each run's world is generated once per `(world, seed, size)` and reused by every run on it. Runs execute **sequentially**; the two performance runs execute last and **exclusively** (nothing else running on the pool) so timings are not contended.
- Runs R1–R7 together are about 800 weeks including spin-ups; at A4's budgets that is seconds of CPU, which keeps "a full plan in seconds".

### Runner

```cpp
struct S25RunParams  { StringCRC world;                 // curated id, or empty for both
                       uint32_t  seed;                  // 0 = the curated seed
                       WorldSize size;                  // default Small
                       bool      recordBaseline;        // freeze absent default magnitudes from this run
                       S25PlanFilter plan; };           // all | a subset of run ids (for tests)

class S25RunHandle {
public:
    S25RunProgress  GetProgress() const;   // phase, run id, week, total weeks, fraction, elapsed ms (atomics, read from any thread)
    void            Cancel();              // honoured between weeks and between S-1 generation steps
    bool            IsDone() const;
    const S25RunReport& GetReport() const; // valid when IsDone()
};

class S25HeadlessRunner { public: S25RunHandle Start(const S25RunParams&); };
```

Phases: `Generating`, `Prechecks`, `Running(R#)` (week counter), `Evaluating`, `Performance`, `WritingReport`, `Done | Cancelled`.

- **Worker job.** Launched through `JobSystemService` `TrySubmit(fn, tag)` with a `s25` attribution tag. A **synchronous launcher seam** runs the same code inline for GoogleTests and headless use (as A4 Feature 3). The job never touches the live `WeatherModule`, the bus, a module or `GetCurrentWorld()`.
- **World.** S-1's blocking `GenerateWorld(seed, params)` (the injectable-clock, cancel-flag form it keeps for tests and headless use) produces a `WorldStructure` on the worker; a `WeatherTerrain` copy is taken from it. Generation failure or timeout is reported as `InvalidWorld` with the S-1 reason, never as an A4 fail.
- **Prechecks** (Feature 1) run on the generated terrain. A failed precheck marks the world invalid: its dependent checks return `InvalidWorld` without running the weather, and the verdict becomes `InvalidWorld`.
- **Simulation.** `WeatherRunner::Start(terrain, params, baseline, spinupWeeks, executor)` with the table baseline and parameters from `DefaultWeatherParameters()` plus the run's override, then `StepWeek` per week. Setup commands are applied through A4's **direct sink** (A4 Feature 5) between weeks, so the commands mean exactly what they mean live.
- **Trace.** A `RecordedTrace` (Feature 2) keeps every committed week of the nine fields for runs that feed checks; P1 and P2 record nothing but timings.
- **Evaluate.** Feature 2's `RegisterAll` checks are evaluated per their runs; `BaselineStore::Compare` judges Default checks; when `recordBaseline` is set, absent entries are recorded (never overwritten).
- Cancel at any point yields a report with verdict `Cancelled`, the checks completed so far, and no baseline write.

### Verdict rule

| Verdict | When |
|---|---|
| `Pass` | every Hard check is `Pass`, no check is `InvalidWorld` or `Pending`, and every Default check has a recorded or just-recorded value |
| `HardFail` | any Hard check is `Fail`; each lists its `reopenSection` |
| `MagnitudeMisses` | no Hard fail, but at least one Default check is `MagnitudeMiss`: **goes to the user** (adjust the threshold or reopen A4), not an automatic fail (GDD AD-06) |
| `InvalidWorld` | a precheck failed or a band was empty: re-pick the seed |
| `Incomplete` | a check was `Pending` (e.g. a cancelled run) |
| `Cancelled` | the run was cancelled |

Precedence: `Cancelled`, then `InvalidWorld`, then `HardFail`, then `Incomplete`, then `MagnitudeMisses`, then `Pass`. **Performance never changes the verdict** (decided 2026-11-16: report only, which loosens the GDD's two hard performance fails; recorded as a deviation).

### Performance block

From A4's `GetLastProfile()` per step and wall clock: P1 (Small) records spin-up wall ms, 52-week wall ms, per-step CPU max, mean and 95th percentile, range count and executor; P2 (Large) records the same for 104 × 64. Each is compared with its budget (**52 weeks incl. spin-up ≤ 5 s** at Small; step CPU **≤ 4.2 ms** Small, **≤ 10 ms** Large, per A4's gate) and labelled `Within`, `Over` or `NotMeasured (Debug)`, with the build configuration, machine core count and worker cap written alongside. These are **informational** and listed first in the report so a regression is obvious, but they are not checks.

### Live smoke (`cow.s25.livesmoke`)

In the stage: set Fastest, run 52 weeks in the current step mode, record `cow.weather.step_ms` per applied step, wall time, and in Deferred mode the `cow.worldstep.stalls` delta. Writes `out/CoW/s25/livesmoke.json` with the maximum step against the Small budget, the wall time (expected near 12.1 s, capped by the speed controller, so only a sanity range is asserted), and any stall. Informational, like the performance block.

### Report

`out/CoW/s25/report_<world>_<seed>.json` (PD-009), one per world, plus `out/CoW/s25/report_summary.json` with the overall verdict. Envelope `cow.s25_report` version 1:

```
verdict, world {id, preset, seed, size, revision}, prechecks[{id, state, value, margin}],
performance {p1, p2}, runs[{id, weeks, executor, elapsedMs}],
checks[{id, test, kind, state, measured, threshold, detail, reopenSection}],
baseline {recorded[], missed[]}, edgeCases[{id, status, runs|reason}],
environment {build, cores, workerCap, gitRevision}
```

- **Edge-case coverage.** `Stages/S25WeatherPatterns/edge_cases.json` (`cow.s25_edge_cases` v1) lists exactly the 20 A4 edge cases (EC-01 to EC-20) once each, `covered` with the run ids and the setup that exercises it, or `untested` with a reason. A validation test loads the file and fails on a missing, extra, duplicate or reason-less entry, and `covered` entries must name a run id from the plan table. The report reproduces it, so nothing is silently skipped. Authoring the mapping from `A4_EdgeCaseMatrix.md` is a plan task.
- Writing is atomic (write a temp file then rename) so an interrupted run never leaves a half report.

### Commands (4 names, registered all-or-nothing, on top of Feature 1's two)

| Name | Kind | Arguments |
|---|---|---|
| `cow.s25.run` | command | `{"action":"start","world":"<id>|both","seed":0,"size":"Small","record_baseline":false}` or `{"action":"cancel"}` |
| `cow.s25.report` | query | `{"what":"status"|"latest","world":"<id>"}`: progress while running, else the summary and report path |
| `cow.s25.baseline` | command | `{"action":"record","force":false}`: freeze absent (or with `force`, all) default magnitudes of the latest report; refused while a run is active |
| `cow.s25.livesmoke` | command | `{"action":"start"|"cancel"}` |

All sim-hosted with mandatory help, category `s25`, JSON-object arguments. A bad world, size or action returns an error and starts nothing. DiaAutomation follows F-2's pattern: it triggers `cow.s25.run`, polls `cow.s25.report`, and reads the report file.

### The `SLOW_` GoogleTest

`SLOW_S25Verdict.CuratedWorldsSmall` runs the whole plan (synchronous launcher, no stage, serial and reverse executors only, **no performance runs**) on both curated worlds. It **fails** on `HardFail` (naming checks and `reopenSection`), `InvalidWorld` (naming the failing precheck: "re-pick the seed") and `Incomplete`; it **prints** `MagnitudeMisses` as warnings; it writes the reports to `out/`. It is the "every A4 change" hook.

## Public API

New in `Cluiche/CoW/Modules/` (S25 prefix): `S25HeadlessRunner.h/.cpp`, `S25TestPlan.h/.cpp`, `S25Report.h/.cpp` (build and serialize), `S25LiveSmoke.h/.cpp`. Edited: `S25WeatherModule` (commands, handle), A4's `WeatherRunner` (additive `Start` start-week offset), `Assets/CoW/Stages/S25WeatherPatterns/edge_cases.json`.

## Acceptance Criteria

### Plan and runner
1. The plan table has exactly the ten runs above; each run feeds the checks listed, and every check id from Feature 2 is fed by at least one run (a registry-versus-plan test).
2. With the synchronous launcher a plan run on hand-built terrain with a stub world source completes and produces a report with every section present.
3. The runner shares nothing with the live stage: a run started while the stage is stepping leaves the stage's committed fields bit-identical to a stage-only run.
4. Cancel during `Generating`, `Running` and `Evaluating` each stop within one week or one generation step, yield verdict `Cancelled`, and write no baseline.
5. `GetProgress` is readable from another thread without a data race (sanitizer clean) and reaches `Done` or `Cancelled`.
6. A failed generation or a timeout is `InvalidWorld` with the S-1 reason, not an A4 fail.
7. A failed precheck makes dependent checks `InvalidWorld` and does not run their runs.
8. The storm cell chooser is deterministic, honours the edge margin, and returns `InvalidWorld` when no candidate exists.
9. Preset runs skip spin-up and start the calendar at the preset's season (the offset test: a `winter_snow` run's first week is Winter).
10. R3 applies `edge_inflow = 0` from the start including spin-up (the state after spin-up differs from R1's).

### Determinism and agreement
11. R1 and R2 give byte-identical committed fields at weeks 13, 26, 39 and 52 (executor A vs B on a real generated world).
12. **Live-vs-runner:** the stage's live module (Sync mode) and the runner on the same world and parameters give byte-identical committed fields for weeks 1–13.
13. The same plan run twice produces identical `measured` values for every check and an identical report apart from timings and the environment block.

### Verdict and baseline
14. Each verdict row is produced by a scripted check set (all Pass → `Pass`; one Hard Fail → `HardFail`; only a MagnitudeMiss → `MagnitudeMisses`; one `InvalidWorld` → `InvalidWorld`; a `Pending` → `Incomplete`; a cancel → `Cancelled`), and the precedence order holds when several apply.
15. Performance numbers are present in the report and never alter the verdict, including an artificially over-budget run.
16. `record_baseline` writes absent entries only; a second run with an existing baseline compares within ±10%; `cow.s25.baseline record` without `force` leaves existing entries byte-identical and is refused while a run is active.

### Report and edge cases
17. The report validates against its envelope (`cow.s25_report` v1) and carries every section; it is written atomically (a test interrupts the write and finds no partial file).
18. `edge_cases.json` validation rejects a missing, extra, duplicate or reason-less entry and a `covered` entry naming an unknown run; the report reproduces the table.
19. Hard-fail report entries carry their `reopenSection`; Test 7's fail carries the "goes to the user" text.

### Commands and automation
20. The four names register with non-empty help; a forced failure of any leaves **no** names behind (including Feature 1's two).
21. Invalid arguments (unknown world, size, action, a start while a run is active) return an error naming the field and start nothing; `cancel` with no run is a no-op error.
22. A DiaAutomation scenario enters the stage, starts the run, polls `cow.s25.report` to completion, and reads a `Pass` or the expected verdict from the report file.
23. `cow.s25.livesmoke` records 52 weeks of step times and wall time into `livesmoke.json`; in Deferred mode it also records the stall delta; a Debug build labels the budget `NotMeasured`.

### GoogleTest hook
24. `SLOW_S25Verdict` fails on a scripted `HardFail`, an `InvalidWorld` and an `Incomplete`, warns on `MagnitudeMisses`, and writes reports under `out/`.

## Files Touched

New: the four module file pairs; `edge_cases.json`; tests `Tests/GoogleTests/CoW/S25/{TestS25Plan,TestS25Runner,TestS25Report,TestS25EdgeCases}.cpp` and the `SLOW_S25Verdict` suite; a DiaAutomation scenario. Edited: `S25WeatherModule`, `WeatherRunner` (start-week offset, additive), vcxproj. **Spec edits on approval:** the system spec's Headless runner and Performance sections (A4's CPU budgets, report-only performance, the run table, the `SLOW_` hook); A4 Feature 3's `WeatherRunner` signature (start-week offset); Feature 2's check-to-run mapping note. No Dia files are added or edited.

## Binding Decisions

| ID | Decision | How this feature complies |
|----|----------|---------------------------|
| AD-002 | Four-tier dependency model | Feature-local `S25` files; the plan table is not shared |
| AD-001 | Ask permission before Dia edits | None. S-1's blocking `GenerateWorld`, `TrySubmit` and DiaAutomation are used as built |
| PD-009 | Outputs under `out/` | Reports and livesmoke under `out/CoW/s25/` |
| PD-004 | No STL in public APIs | Reports and results use Dia containers; jsoncpp only at the serialization edge |
| GDD AD-06 / AD-07 | Directions hard, numbers default; checks numeric and scripted | Verdict rule and DiaAutomation trigger |
| DFS-002 | `Run` reads only the captured copy | The runner uses A4's `WeatherRunner` which already satisfies it; nothing of the live stage is read |
| Editor-actions rule | Every tool action is a command | Four names; the card posts the same commands |

## Test Plan

Layered so most of it needs no real A4 run: scripted check sets for the verdict rows, a stub world source and the synchronous launcher for the runner mechanics, then **one** real-A4 run on hand-built terrain for determinism and live-vs-runner agreement, and the `SLOW_S25Verdict` suite for real generated worlds. TDD order: plan table, report and verdict rules, edge-case file, runner mechanics and cancel, commands, then the real runs. Layer goldens are not used (checks are the verdict). Scoped `--filter` while iterating, one full `--shards >= 2` run at the end. No mutation checks. Gap audit and edge pass is the last plan task. **Observation scan** at verify. The **first real run on the curated worlds** is the proof task: results (including any Hard fails, which are findings) go in the plan.

## GDD Deviations Introduced

- **D-3.1.** Performance results are report-only; the GDD lists the 52-week and weekly-tick budgets as hard fails.
- **D-3.2.** The weekly-tick budget is A4's CPU-time gate (Small 4.2, Standard 6.3, Large 10 ms), not the GDD's 16 ms frame budget (already superseded by the 2026-10-20 fold-back).
- **D-3.3.** Test 5 runs on the natural 65 weeks; the presets are the guided versions.

## Open Design Questions

1. **Hard fails in the SLOW test.** If Test 7 fails by design (A4 has no RNG, system Resolved Decision 3), `SLOW_S25Verdict` fails on every run until you decide what to do. A known-failures list (id, reason, date) that downgrades a listed Hard fail to a warning in the **test only** is the obvious fix, but it also hides a regression of that very check. Decide when the first real run shows what actually fails; until then the test is strict.
2. **Is the runner's world the stage's world?** Both generate from the same preset and seed, so they should agree, and criterion 12 proves the weather does. If S-1's generator is not deterministic across a blocking and a stepped run on the same seed, they could differ; S-1's own tests say it is, but this is the first consumer to rely on both forms agreeing.
3. **Exclusive perf runs.** Running P1 and P2 last and alone keeps timings clean but means a cancelled or failed earlier run never produces perf numbers. Acceptable for report-only perf; revisit if perf becomes a gate.
4. **Report size.** Each check's `detail` is a short string, and full traces are not written. If a failing check needs the underlying weekly series to debug, a `--keep-trace` option writing a `dia.cell_history` export is the natural addition; not specified now.

## Gap-pass amendments (2026-11-16)

Found by the plan-writing pass; each item below **supersedes** any contradicting text above. Numbering is stable.

1. **Verdict rule row `Pass`** reads: every Hard check is `Pass` and none is `InvalidWorld` or `Pending`. A Default check with no baseline entry is judged against the GDD default; unrecorded entries are listed under `baseline.unrecorded[]` and do not prevent `Pass`. The `SLOW_` test treats `Pass` and `MagnitudeMisses` (as warnings) as green.
2. **Cross-feature command registration.** `S25WeatherModule` owns one `S25CommandRegistrar` that collects every `cow.s25.*` name (Feature 1's two and this feature's four) and rolls all of them back if any registration fails. Feature 4's three `cow.scenario.*` names belong to `ScenarioModule` and roll back separately.
3. **Test executors.** A4 Feature 2's test-local reverse-order and fixed-range executors are reused; DiaDeferredStep's `Testing/` equivalents are used instead when they exist (consuming a Dia test helper is not a Dia edit).
4. **Where the CPU report goes (criterion 26 of Feature 1 and this feature's performance block):** the report's `performance` section and the `INFO` log; `cow.s25.status` shows the last reported values.
5. **P2 (Large)** runs no prechecks and no checks (performance only).
6. **Command names** are this feature's: `cow.s25.run` (`start`/`cancel`), `cow.s25.report`, `cow.s25.baseline` (`record`), `cow.s25.livesmoke`. The system spec's `cow.s25.run.cancel` and `cow.s25.baseline.record` are superseded.
7. **Readiness after a size change** uses `WeatherModule::IsReady(revision)` for the new revision (A4 Feature 3).

## T-1 consumer amendments (2026-11-16, approved with T-1 Feature 9)

Requested by @docs/specs/applications/cow/systems/t1-worldsiminspector/s25-checks-panel.md. All are additive; the report stays `cow.s25_report` version 1.

- **A. Runs per check.** Each `checks[]` entry gains `runs: ["R1", ...]`, the ids of the runs that fed it, taken from the plan table (T1, T4, T5 → R1; T2 → R5a, R5b; T3 → R4; T6 → R6; T7 → R7; T8 → R1, R3; T9 → R1, R2). The registry-versus-plan test (criterion 1) checks that the lists match the table.
- **B. Report archive.** Every completed run (verdict other than `Cancelled`) also writes `out/CoW/s25/archive/report_<world>_<seed>_<utcCompact>_<gitrev7>.json`, atomically like the main report, and deletes the oldest files so that at most 50 remain per world and seed. The `SLOW_` test writes there too. `gitrev7` is `environment.gitRevision` cut to 7 characters, or `nogit`.
- **C. Live smoke envelope.** `livesmoke.json` carries `format: "cow.s25_livesmoke"`, `version: 1`.

Added criteria:

25. Every report check carries a non-empty `runs[]` whose ids exist in the plan table and match it.
26. A completed run writes one archive file with the stated name; a `Cancelled` run writes none; the 51st file for a world and seed removes the oldest.
27. `livesmoke.json` validates against `cow.s25_livesmoke` v1.

**Plan impact:** the plan (written, not started) needs these three folded into its report task before it starts.

## World-sim review amendment (2026-11-18)

User decisions from the world-sim review. Each item **supersedes** any contradicting text in this spec. Sources: [summary](../../../../../research/worldsim_data_driven/summary.md) §0, §0.1; [parameter-files](../../../dia/systems/diaattribute/parameter-files.md); [biome-registry](../s1-worldgeneration/biome-registry.md); [worldsim-kit](../worldstep/worldsim-kit.md); [grid-search-helpers](../../../dia/systems/diapathfinding/grid-search-helpers.md); [blackboard-scopes](../../../dia/systems/diablackboard/blackboard-scopes.md).

1. `WeatherRunner::Start(terrain, params, baseline, ...)` takes parameters produced from the A4 sidecar (`ParameterFile` + `ParamBinding::Snapshot`); the `DefaultWeatherParameters()` source is superseded. A sidecar fault fails the run (no compiled fallback).
2. The baseline is the registry-keyed fallback table (A1 absent), per A4 item 3; biomes are `BiomeIndex`.
3. Metric `cow.weather.step_ms` is registered by the stage base; the headless runner records its own timing as before.

## Status

**Status:** `Approved` (2026-11-16; T-1 consumer amendments A-C added the same day). Plan: [headless-runner.plan.md](headless-runner.plan.md) (written 2026-11-16, not started; fold in amendments A-C).

**Amended 2026-11-18 (world-sim review).** Status stays `Approved`. Amended 2026-11-18 (world-sim review decisions; user-approved designs in the specs referenced below).
