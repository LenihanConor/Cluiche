# Feature Spec: S-2.6 Headless Runner & Report

**Parent:** @docs/specs/applications/cow/systems/s26-riversfloods/s26-riversfloods.md (Feature 3)
**Status:** Approved (2026-11-24)
**Design source:** `DesignDocs/COW/Output Docs/GDD/10_PrototypeStrategy/S2.6_RiversFloods.md` AD-03, AD-05, AD-07, AD-09, §5, §6
**Depends on:** S-2.6 Feature 1 (stage, worlds, prechecks, module) and Feature 2 (checks, series, baseline store); A5 Feature 2 (`HydrologySimulation`, `WaterInput`), Feature 3 (`HydrologyRunner`, `BuildHydrologyTerrain`), Feature 5 (the typed `HydrologyCommand` and its **direct sink**) and Feature 6 (presets, `ScriptedWaterInput`, `Testing/ReachCycleScript`, goldens); S-1 blocking `GenerateWorld` with a cancel flag and `ReachabilityValidator` (built); DiaAutomation; `JobSystemService` `TrySubmit` (built).
**Consumed by:** S-3 (trusts Living mode's A5 reads only after this passes), S-3.5, the `SLOW_` suite.

## Summary

The water testbed's verdict machine. `S26HeadlessRunner` generates the curated worlds itself, drives A5's **pure** `HydrologySimulation` through `HydrologyRunner` and the A5 direct command sink on a worker job, records a per-week series for each run, runs Feature 2's checks over it, measures performance, and writes a report. **Scripted only** (the verdict never depends on A4). It needs no stage, no rendering and no bus, so it runs three ways: from the stage (`cow.s26.run`, with progress and cancel), from DiaAutomation, and from a `SLOW_` GoogleTest, so a plain test run gives a water verdict on every A5 change.

## Goals

1. One call produces a complete Scripted verdict for both curated worlds: every check, the prechecks, performance numbers and the edge-case coverage table, in one JSON report.
2. The runner shares **no state** with the live stage: it generates its own worlds and owns its own simulations.
3. A run is cancellable at any week and reports progress by phase.
4. The verdict follows one stated rule from the check states; performance never changes it.
5. The live module and the runner agree: the same world and weeks give byte-identical committed fields and identical series.
6. Coupled-only checks are **listed** as `CoupledOnly`, never silently dropped, and never fail the Scripted verdict.

## Non-Goals

- The checks, series and baseline store (Feature 2). Check lines and scenarios (Feature 4). History export (Feature 5).
- The stage and world picker (Feature 1).
- Coupled runs: Test 8, the Coupled Test 4 variant, Coupled determinism and the `SLOW_` coupled Test 6 are stage or `SLOW_` GoogleTest work, blocked on A4 (system spec).
- Verdict-grade results at Standard or Large (performance only).

## Design

### The test plan (code-defined table)

`S26TestPlan` is a code table, not scenario files (Feature 4's guided scenarios later reuse the same setup commands and a test asserts agreement). Presets and forced years are applied through **A5's command executor bound to the run's own `HydrologySimulation`** (A5 Feature 5's direct sink), so a plan setup means the same live and headless. Run lengths come from the preset data (`ScriptedWaterInput::TotalWeeks`).

| Run | World | Script | Weeks | Executor | Feeds |
|---|---|---|---|---|---|
| H1 melt | `s25_coast_range` | `melt_pulse` | preset length (+ spin-up) | capped | T1, T9 (counters), T10 reference |
| H2 dry-year | `s25_coast_range` | normal year, then `forced dry` (rain 0.2×), then a rain-returns year | 3 × 52 | capped | T2 (dry year) |
| H3 dry-interior | `s25_dry_interior` | same as H2 | 3 × 52 | capped | T2 (drought world), T3 prechecked |
| H4 wet-dry | worlds passing `dry_and_lake_basin` | `wet_then_dry` | preset length (~572) | capped | T2 (dry decade), T3, T9 |
| H5 ice | `s25_coast_range` | `ice_age` | preset length (~1,300+) | capped | T4, T9 |
| H6 cold-winter | `s25_coast_range` | a scripted frozen run (cold winter, low-flow river) | 104 | capped | T2 frozen-weeks neutrality |
| H7 ford | `s25_coast_range` | `flash_flood_fords` | 22 | capped | T7, T10 reference |
| H8 balance ×4 | each curated world | every preset, then base weather to **5,200 weeks** | 5,200 each | capped | T5 |
| H9 reach | each curated world | `ReachCycleScript` (A5 Feature 6) with the validator yearly | 5,200 | capped | T6 |
| D1–D10 determinism | `s25_coast_range` | `melt_pulse` and `flash_flood_fords` at ranges 1, 2, 7, 16 and serial | 104 or full length | each | T10 `ranges_identical` |
| G golden | Small hand-built preset × seeds 1–3 | A5 Feature 6's golden set | 52 | serial | T10 `goldens_match` |
| P1–P3 perf | `s25_coast_range` Small, Standard, Large | `melt_pulse`, trace recording off | spin-up + 52, exclusive | capped | performance only |

- **After a preset ends**, H8 continues on a **base-weather tail** (`TailedWaterInput`, feature-local, repeats the preset's base-week values) to 5,200 weeks, since A5's presets run 22 to about 1,300 weeks and R-Balance wants 100 years.
- **H4 and H3 world choice.** A run list names a world only if its precheck passes (Feature 1's table). A world whose precheck fails turns its dependent checks into `InvalidWorld` without running the run. A test with both worlds invalid reports "no valid world" (system spec).
- The **ice-age run's frozen-weeks check** (H6) is separate from H5 because CH-A5-02 needs a controlled cold winter with a low-flow river, not the full ice age.
- Each world is generated once per `(world, seed, size)` and shared by its runs. Runs execute **sequentially**; H8 and H9 are the long ones. Performance runs execute last and **exclusively**.
- **Run time.** H8 (4 presets × 5,200 weeks × 2 worlds) and H9 (5,200 × 2) are about 52,000 weekly steps. Their combined time is unmeasured; A5 Feature 6 Open Question 3 decides which of these are default and which `SLOW_` (Open Design Question 1).

### Runner

```cpp
struct S26RunParams  { StringCRC world;        // curated id, or empty for both
                       uint32_t seed;          // 0 = the curated seed
                       WorldSize size;         // default Small
                       bool recordBaseline;
                       S26PlanFilter plan; };  // all | subset of run ids

class S26RunHandle {
public:
    S26RunProgress GetProgress() const;        // phase, run id, week, total weeks, fraction, elapsed ms (atomics)
    void           Cancel();                   // honoured between weeks and between S-1 generation steps
    bool           IsDone() const;
    const S26RunReport& GetReport() const;     // valid when IsDone()
};
class S26HeadlessRunner { public: S26RunHandle Start(const S26RunParams&); };
```

Phases: `Generating`, `Prechecks`, `Running(<id>)`, `Evaluating`, `Performance`, `WritingReport`, `Done | Cancelled`.

- **Worker job** through `JobSystemService` `TrySubmit(fn, tag)` with an `s26` tag; a **synchronous launcher seam** runs the same code inline for GoogleTests. The job never touches the live `HydrologyModule`, the bus or `GetCurrentWorld()`.
- **World.** S-1's blocking `GenerateWorld(seed, params)` on the worker; `BuildHydrologyTerrain` builds the terrain. Generation failure or timeout is `InvalidWorld` with the S-1 reason, never an A5 fail.
- **Simulation.** `HydrologyRunner(sim)` with the same initialise, spin-up and step sequence `HydrologyModule` uses, so the runner cannot drift. The ranges per run come from the plan; the reverse-order test executor from A5 Feature 2 and DiaDeferredStep's `Testing/` equivalents are reused.
- **Series.** After every week the runner calls Feature 2's `SummariseWeek` (the same function the live module calls).
- **Reach.** H9 calls S-1's `ReachabilityValidator` at each year's end with `Deep` as water and `Frozen` as passable, through the same `Deep`-blocking profile decorator A5 Feature 6 ships in `Testing/ReachCycleScript`.
- **Evaluate.** Feature 2's checks run per their runs; `BaselineStore::Compare` judges Default checks; `recordBaseline` records absent entries (never overwriting).
- Cancel at any point yields verdict `Cancelled`, the checks completed so far, and no baseline write.

### Verdict rule

| Verdict | When |
|---|---|
| `Pass` | every Hard check is `Pass` (`CoupledOnly` is neither pass nor fail and is listed separately) and none is `InvalidWorld` or `Pending` |
| `HardFail` | any Hard check is `Fail`; each lists its `reopenSection` |
| `MagnitudeMisses` | no Hard fail, but a Default check is `MagnitudeMiss`: **goes to the user** (adjust the threshold or retune A5 in the order: preset data values, A5's default parameter file, then stop and flag; GDD §6), not an automatic fail |
| `InvalidWorld` | a precheck failed for a needed world, or both worlds invalid for a test |
| `Incomplete` | a check was `Pending` |
| `Cancelled` | the run was cancelled |

Precedence: `Cancelled`, `InvalidWorld`, `HardFail`, `Incomplete`, `MagnitudeMisses`, `Pass`. **Performance never changes the verdict** (GDD AD-07). A `Pass` report always carries a `coupledOnly[]` list so a green Scripted verdict is not mistaken for a full A4+A5 pass.

### Performance block

From A5's `cow.hydro.step_ms` per step and wall clock: P1 (Small), P2 (Standard), P3 (Large) record spin-up wall ms, 52-week wall ms, per-step CPU max, mean and 95th percentile, the **largest river-tree size**, range count, executor, build configuration, core count and worker cap. Each is labelled `Reported` or `NotMeasured (Debug)`; no budget is gated (DECISION-130, A5 §9). They are listed first in the report and written to the baseline's `info` block. The stage panel warns, but does not fail, when A5's step plus A4's step exceeds the World Step slack (Feature 1 shows it).

### Live smoke (`cow.s26.livesmoke`)

In the stage: set Fastest, year-step 1 year in the current mode, record `cow.hydro.step_ms` per applied step, wall time and, when Deferred, the stall delta; write `out/CoW/s26/livesmoke.json` (envelope `cow.s26_livesmoke` v1). Informational.

### Report

`out/CoW/s26/report_<world>_<seed>.json` (PD-009), one per world, plus `report_summary.json`. Envelope `cow.s26_report` version 1:

```
verdict, world {id, preset, seed, size, revision}, prechecks[{id, state, value, margin}],
performance {p1, p2, p3}, runs[{id, weeks, executor, elapsedMs}],
checks[{id, test, kind, state, measured, threshold, detail, reopenSection, runs[]}],
coupledOnly[{id}], baseline {recorded[], missed[], unrecorded[]},
edgeCases[{id, status, runs|reason}], environment {build, cores, workerCap, gitRevision}
```

- **Runs per check** (`runs[]`) come from the plan table (T-1 consumers read it, as for S-2.5).
- **Archive.** A completed run (not `Cancelled`) also writes `out/CoW/s26/archive/report_<world>_<seed>_<utcCompact>_<gitrev7>.json`, atomically, and keeps at most 50 per world and seed.
- **Edge-case coverage.** `Stages/S26RiversFloods/edge_cases.json` (`cow.s26_edge_cases` v1) lists each A5 edge case from `A5_EdgeCaseMatrix.md` exactly once, `covered` with the run ids and setup, or `untested` with a reason. A validation test loads it and fails on a missing, extra, duplicate or reason-less entry; `covered` entries must name a run id from the plan table. The report reproduces it. Authoring the mapping is a plan task.
- Writing is atomic (temp then rename).

### Commands (4 names, registered all-or-nothing, on top of Feature 1's six)

| Name | Kind | Arguments |
|---|---|---|
| `cow.s26.run` | command | `{"action":"start","world":"<id>\|both","seed":0,"size":"Small","record_baseline":false}` or `{"action":"cancel"}` |
| `cow.s26.report` | query | `{"what":"status"\|"latest","world":"<id>"}`: progress while running, else the summary and report path |
| `cow.s26.baseline` | command | `{"action":"record","force":false}`: freeze absent (or with `force` all) default magnitudes of the latest report; refused while a run is active |
| `cow.s26.livesmoke` | command | `{"action":"start"\|"cancel"}` |

A single `S26CommandRegistrar` owned by `S26RiversFloodsModule` collects every `cow.s26.*` name (Feature 1's six and these four) and rolls all back if any registration fails. DiaAutomation follows F-2's pattern: trigger `cow.s26.run`, poll `cow.s26.report`, read the report file.

### The `SLOW_` GoogleTest

`SLOW_S26Verdict.CuratedWorldsSmall` runs the Scripted plan (synchronous launcher, no stage, **no performance runs**) on both curated worlds. It **fails** on `HardFail` (naming checks and `reopenSection`), `InvalidWorld` (naming the failing precheck: "re-pick the seed") and `Incomplete`; it **prints** `MagnitudeMisses` as warnings; it writes reports under `out/`. `SLOW_S26Verdict.Coupled*` tests are **blocked** on A4 and listed by the report as `CoupledOnly`.

## Public API

New in `Cluiche/CoW/Modules/` (S26 prefix): `S26HeadlessRunner.h/.cpp`, `S26TestPlan.h/.cpp`, `S26Report.h/.cpp`, `S26LiveSmoke.h/.cpp`, `S26/TailedWaterInput.h/.cpp`, `S26CommandRegistrar.h/.cpp`. Edited: `S26RiversFloodsModule` (commands, handle), `Assets/CoW/Stages/S26RiversFloods/edge_cases.json` (new).

## Acceptance Criteria

### Plan and runner
1. The plan table has exactly the runs above; each feeds the checks listed, and every Scripted check id from Feature 2 is fed by at least one run (a registry-versus-plan test); every `CoupledOnly` id is listed as such and fed by none.
2. With the synchronous launcher a plan run on hand-built terrain with a stub world source completes and produces a report with every section present.
3. The runner shares nothing with the live stage: a run started while the stage is stepping leaves the stage's committed fields bit-identical to a stage-only run.
4. Cancel during `Generating`, `Running` and `Evaluating` each stop within one week or one generation step, yield `Cancelled` and write no baseline.
5. `GetProgress` is readable from another thread without a data race (sanitizer clean) and reaches `Done` or `Cancelled`.
6. A failed generation or timeout is `InvalidWorld` with the S-1 reason, not an A5 fail.
7. A failed precheck makes dependent checks `InvalidWorld` and does not run their runs; a test with both worlds invalid reports "no valid world".
8. A preset run followed by the base-weather tail reaches exactly 5,200 weeks, and the tail repeats the preset's base values.
9. Setup lines for presets and forced years are applied through A5's direct sink bound to the run's simulation, and equal the live module's queued sink for the same lines (a sink-equivalence test).

### Determinism and agreement
10. Reference and range-1, 2, 7, 16 and serial runs give identical `layerHash` every week of `melt_pulse` and `flash_flood_fords` on a real generated world.
11. **Live-vs-runner:** the stage's live module (Scripted) and the runner on the same world and preset give byte-identical committed fields and an identical series for weeks 0–52.
12. The same plan run twice gives identical `measured` for every check and an identical report apart from timings and the environment block.

### Verdict and baseline
13. Each verdict row is produced by a scripted check set (all Pass → `Pass`; one Hard Fail → `HardFail`; only a MagnitudeMiss → `MagnitudeMisses`; one `InvalidWorld` → `InvalidWorld`; a `Pending` → `Incomplete`; a cancel → `Cancelled`) and the precedence order holds when several apply.
14. A `Pass` report lists every `CoupledOnly` check under `coupledOnly[]`; no `CoupledOnly` check changes the verdict.
15. Performance numbers are present and never alter the verdict, including an artificially over-slow run.
16. `record_baseline` writes absent entries only; a second run compares within the stated bands; `cow.s26.baseline record` without `force` leaves existing entries byte-identical and is refused while a run is active.

### Report and edge cases
17. The report validates against `cow.s26_report` v1, carries every section and each check's non-empty `runs[]` matching the plan table, and is written atomically (a test interrupts the write and finds no partial file).
18. A completed run writes one archive file with the stated name; a `Cancelled` run writes none; the 51st file for a world and seed removes the oldest.
19. `edge_cases.json` validation rejects a missing, extra, duplicate or reason-less entry and a `covered` entry naming an unknown run; the report reproduces the table.
20. Hard-fail entries carry their `reopenSection`; a `t1.lag_weeks` miss carries the "goes to the user" text.

### Commands and automation
21. The four names register with non-empty help; a forced failure of any leaves **no** names behind (including Feature 1's six).
22. Invalid arguments (unknown world, size, action; a start while a run is active) return an error naming the field and start nothing; `cancel` with no run is a no-op error.
23. A DiaAutomation scenario enters the stage, starts the run, polls `cow.s26.report` to completion, and reads `Pass` or the expected verdict from the report file.
24. `cow.s26.livesmoke` records 52 weeks of step times into `livesmoke.json` and validates against `cow.s26_livesmoke` v1; a Debug build labels the numbers `NotMeasured`.

### GoogleTest hook
25. `SLOW_S26Verdict` fails on a scripted `HardFail`, an `InvalidWorld` and an `Incomplete`, warns on `MagnitudeMisses`, and writes reports under `out/`.

## Files Touched

New: the module file pairs; `edge_cases.json`; tests `Tests/GoogleTests/CoW/S26/{TestS26Plan,TestS26Runner,TestS26Report,TestS26EdgeCases}.cpp` and the `SLOW_S26Verdict` suite; a DiaAutomation scenario. Edited: `S26RiversFloodsModule`, vcxproj. **Spec edits on approval:** the system spec's Headless runner section (the run table, `CoupledOnly` handling, the `SLOW_` hook). No Dia or Draoi files are added or edited.

## Binding Decisions

| ID | Decision | How this feature complies |
|----|----------|---------------------------|
| AD-002 | Four-tier dependency model | Feature-local S26 files; the plan table is not shared |
| AD-001 | Ask permission before Dia edits | None; S-1's blocking `GenerateWorld`, `TrySubmit`, DiaAutomation are used as built |
| PD-009 | Outputs under `out/` | Reports, archive and livesmoke under `out/CoW/s26/` |
| PD-004 | No STL in public APIs | Reports and results use Dia containers; jsoncpp only at the serialization edge |
| GDD AD-03 / AD-05 | Verdict runs Scripted; checks numeric and scripted | Scripted-only runner, DiaAutomation trigger |
| GDD AD-07 | Performance report-only | Performance block, no gate |
| DFS-002 | `Run` reads only the captured copy | The runner uses `HydrologyRunner`, which already satisfies it |
| Editor-actions rule | Every tool action is a command | Four names; the card posts the same commands |

## Test Plan

Layered so most of it needs no real A5 run: scripted check sets for the verdict rows, a stub world source and the synchronous launcher for the runner mechanics, then **one** real-A5 run on hand-built terrain for determinism and live-vs-runner agreement, and `SLOW_S26Verdict` for real generated worlds. TDD order: plan table, report and verdict rules, edge-case file, runner mechanics and cancel, commands, then the real runs. Scoped `--filter` while iterating, one full `--shards >= 2` run at the end. No mutation checks. Gap audit and edge pass is the last plan task. **Observation scan** at verify. The **first real run on the curated worlds** is the proof task: results (including any Hard fails, which are findings) go in the plan, and every Default magnitude is recorded with the user's sign-off on any miss.

## GDD Deviations Introduced

- **D-3.1.** Coupled-only checks are listed as `CoupledOnly` and are outside the Scripted verdict (the GDD's AD-03 allows one Coupled smoke test; this lists exactly four).
- **D-3.2.** H8 plays 5,200 weeks per preset with a base-weather tail after the preset; the GDD says "100 simulated years" without defining the tail.

## Open Design Questions

1. **How long do H8 and H9 take?** About 52,000 weekly steps with the S-1 validator yearly. If a default `SLOW_`-free run is too long, A5 Feature 6's answer decides it (fewer presets in the default run, fewer years, or `SLOW_`). Measure on the first run before deciding; run time is reported (CH-S26-06).
2. **Hard fails in the `SLOW_` test.** If `t1.lag_weeks`-style findings or a hard check fail by design on the first run, `SLOW_S26Verdict` fails on every run until you decide. A known-failures list that downgrades a listed Hard fail to a warning in the **test only** would also hide a regression of that very check. Decide when the first run shows what fails; until then the test is strict.
3. **Is the runner's world the stage's world?** Both generate from the same preset and seed, so they should agree, and criterion 11 proves the hydrology does. This is the first consumer to rely on both generator forms agreeing for water as well as weather.
4. **Report size.** `detail` is a short string and full series are not written to the report; the series are exported by Feature 5. If a failing check needs the underlying weekly numbers, that export is the place.

## Status

**Status:** `Approved` (2026-11-24). No plan.
