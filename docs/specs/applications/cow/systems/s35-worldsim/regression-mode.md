# Feature Spec: S-3.5 Regression Mode

**Parent:** @docs/specs/applications/cow/systems/s35-worldsim/s35-worldsim.md
**Builds on:** @docs/specs/applications/cow/systems/s35-worldsim/stage-and-run-mode.md (Feature 1), @docs/specs/applications/cow/systems/f3-worldassets/save-as.md, @docs/specs/applications/dia/systems/diacellhistory/snapshots.md, @docs/specs/applications/dia/systems/diacellhistory/export.md
**Mockup:** @docs/specs/applications/cow/systems/s35-worldsim/mockups/s35-world-sim.html

**Gameplay Domains:** debug

## Summary

The whole-world proof. Regression mode runs six checks on the real world, headless, across Small, Standard and Large, and writes a report and a `dia.cell_history` export:

| Check | Proves |
|---|---|
| R1 | The same world runs to the same state, at any thread-range count and on repeat |
| R2 | A save in the middle of a season, loaded, continues to the same state |
| R3 | A snapshot restored mid-run continues to the same state |
| R4 | A tuned world saved as packs, start state and bundle loads identically in fresh processes, and a mismatched start state is refused |
| R5 | Large stays inside the time budget, per system and coinciding |
| R6 | Every value, layer and event is described |

It has two parts: an **in-game runner** (R1 to R3, R5 and the in-game part of R6) driven by the game's own clock, and an **outer driver** (a Python E2E scenario) for what needs fresh processes or the CLI (R4 and the file part of R6). R1, R2 and R5 gate S-4 Dragon's Live mode (CH-S35-05); the others report.

## Problem

Each system has its own tests, but nothing proves that weather, ecology, succession and the asset pipeline, running together, are deterministic, restorable and fast enough, which is exactly what the dragon layer will build on. These properties break silently: a system reads wall time, an executor changes an iteration order, a save misses a field, a pack loads differently on the second launch. A regression run that names the first field that differs turns those into findings, not hunts.

## Goals

1. **Deterministic proof.** Identical state hashes across runs, thread-range counts, save/load and snapshot restore.
2. **First difference named.** A failure names the participant and the JSON path of the first differing field, with both values.
3. **Authoring proof.** The save-as and bundle path reproduces a tuned world across fresh processes without touching the repository.
4. **Performance proof** at Large, on a recorded reference machine.
5. **Coverage proof.** Nothing the world-sim stack loads, registers or emits is undescribed.
6. **CI-sized.** Small runs in CI; Standard and Large run nightly.

## Non-Goals

- Per-layer proofs (S-1 guarantees, S-2.5's nine checks, S-3's scenarios, equation tests).
- Dragons, real art, player-facing modes.
- Rendering or UI performance (render and tooling overhead are excluded).
- New simulation code or new state hashing inside systems.

## Architecture

```
Outer driver (Python E2E scenario, Cluiche/Tests/E2E/scenarios/cow/s35_world_sim/regression.py)
  copies assets to scratch · launches the stage through `dia launch` · R4 phases A/B/C · R6 `dia validate schema` · merges the report
In-game runner (S35RegressionRunner inside S35WorldSimModule, SimPU)
  R1 R2 R3 R5 and in-game R6 · writes the report and the cell_history export
  S35StateCapture (fingerprint, compare, first-difference path) · S35PerfStats (median, worst)
```

### Configuration

```json
"s35": { "mode": "Run",
         "regression": { "years": 20, "sizes": ["Small", "Standard", "Large"], "ranges": [1, 4, "max"], "seed": 42,
                         "saveDay": 3503, "bundles": { "Small": "regression_small", "Standard": "regression_standard", "Large": "regression_large" } } }
```

`saveDay` 3503 is year 10, week 33, day 4: mid-year, mid-season and mid-week (`9 × 364 + 32 × 7 + 3`). The bundles are **committed, fixed bundles**, one per size, each with its baked terrain and packs under `Stages/S35WorldSim/regression/`, so the gate does not move when `default_world` or S-1's rules change; replacing one is a deliberate commit. (The system spec's single `regression.cowworld` becomes one per size; amended.)

### Driving the clock

The runner steps the game clock one day per simulation frame through GameSpeedController's manual step (so every tick is dispatched fully, with A2's one-tick-per-frame guarantee) and never uses wall time. The regression launch uses a manifest variant, `s35_regression.diaapp`, whose SimPU frequency is high enough that frames are not the bottleneck; wall time is therefore dominated by simulation cost. The first Small run records the expected wall times in the report, and the plan sets the CI budget from it.

### State capture and comparison

- **Capture.** E1's `SaveLoadModule` gains a read-only `CaptureState()` returning each registered participant's id and the `Json::Value` it would write, without writing a slot (E1 amended). `S35StateCapture` canonicalises each participant's JSON (sorted keys, no whitespace), hashes it (FNV-1a 64), and combines the per-participant hashes in participant order into the **combined state hash**. It keeps the canonical text of checkpoint captures in memory (about 15 MB per Large capture, at most four live).
- **Compare.** `Compare(a, b)` returns identical, or the first differing participant and the JSON path of the first differing field (for example `ecology.cells[1234].grass`) with both values, found by walking the two JSON texts.
- Per-participant hashes are also written to the report so a later run can see which participant moved.

### R1: reproducible world

For each size, for each range count in `ranges`, load the size's bundle (`cow.world.bundle.load`), set A4's job range count, run to the end of `years`, and capture the end hash. The first configuration of a size is run a second time as the repeat. Pass: all end hashes for the size are identical. If A4's `JobSystemService` is not built (serial executor, dia-needs #12), every range count runs serially, the cross-range part cannot differ, and the check reports `pass` with the note "ranges not exercised (serial executor)", never a false claim.

### R2: save and load

In the first R1 run of each size, at `saveDay`: `cow.save s35reg`, then continue to the end (the uninterrupted hash `F_end`). Then `cow.load s35reg`, run to the end again, and compare with `F_end`. Pass: identical.

### R3: snapshot restore

At `saveDay` of the same run, also take `cells.snapshot.take s35reg` (before the first continuation). After the R2 replay, `cells.snapshot.restore s35reg`, run to the end, and compare with `F_end`. If no snapshot store exists (the E1 adapter is not built), R3 is `skipped` with that reason.

### R4: authoring round trip (outer driver)

All phases run against a **scratch copy** of the CoW assets in `out/CoW/s35/scratch_assets/`, made by the driver, so the repository is never written. `dia launch` and `dia run` gain a `--source <path>` override of the computed `source` alias (an extension of dia-needs #16, amended with this feature, needing AD-001 sign-off). Each phase is a fresh process of the stage that runs one `cow.s35.regression.phase <A|B|C>` command, writes `phase_<x>.json` and exits:

| Phase | Does |
|---|---|
| A | Loads the Small regression bundle; applies a fixed tuning delta and a fixed set of painted cells (from the config); `pack.save_as weather s35reg_w`, `pack.save_as ecology s35reg_e`, `startstate.save_as s35reg_s`, `bundle.save_as s35reg_b` |
| B (run twice, two fresh processes) | `bundle.load s35reg_b`; records the **resolved parameters** and the combined state hash after `Initialise` and the start state (day 0) |
| C | Edits the recorded terrain hash in the scratch start state, then `bundle.load s35reg_b`: the start state must be refused with a Warning, the bundle must load without it, and the day-0 hash must equal the hash of the same bundle with no start state |

Pass: both B runs have identical resolved parameters and identical hashes, and C refuses and loads as described.

### R5: performance at Large

Release builds only (a Debug build reports `skipped`). During the Large run with the highest range count, the runner records the duration of every A4 weekly step and A1 band step (from the systems' last-step gauges), computes the **median and worst** for each, and the worst **coinciding** tick (the A4 and A1 durations added on the frame where both ran, which is the last week of a Season). Pass thresholds (first-run defaults, tunable in the config): A4 week ≤ 10 ms, A1 season ≤ 16 ms, coinciding ≤ 26 ms. The report records the machine (CPU name, core count, OS, build config). **Only a machine listed in `Stages/S35WorldSim/reference_machine.json` can gate**; elsewhere R5 reports `info` with the numbers and does not pass or fail.

### R6: coverage

- **In game:** every layer in `CellLayerRegistry` has a `ValueSchemaService` entry for its `schemaKey`; every event type the recorder saw during the runs resolves through `FindEvent`.
- **Driver:** `dia validate schema --target cow --json` over every file the stack loads: zero FAIL findings and zero coverage WARN findings for data files (code-side metric and command coverage is not part of R6).
- Pass: all three are clean. Failures list the key, layer or event type.

### Report and exports

- **`out/CoW/s35/regression_<seed>.json`** (`format: cow.s35_regression`, version 1): seed, git commit, build config, machine (and whether it is the reference), the bundles used, and one block per check with `status` (`pass`, `fail`, `skipped`, `info`), per-size detail and, for failures, the first difference. Per-participant hashes per run, the R5 numbers, and the wall times.
- **`out/CoW/s35/regression_<seed>_<size>.cell_history.json`**: the `dia.cell_history` export of each size's base run (DiaCellHistory Feature 5), for T-1. (One file per size; the system spec's single name is amended.)
- **Gate summary:** `gate: { R1, R2, R5, met }`. `met` is true only if R1 and R2 passed and R5 passed on a reference machine; otherwise it is false with the reason (for example "R5 not evaluated: not a reference machine"), so a missing measurement never looks like a pass.
- **Exit code:** the driver exits non-zero when any check is `fail`. Gating (for S-4) is the report's `gate.met`; the other checks fail the run too, so CI notices them, but they do not decide S-4.

### Running it

- CI: `dia run cow --stage s35_world_sim --automation regression --sizes Small`.
- Nightly: the same with all sizes, under the `SLOW_` convention.
- From the panel (Small only): the Regression mode area starts the in-game checks (R1 to R3, in-game R6); R4 and the CLI part of R6 need the driver and show "run from the command line".
- DiaAutomation triggers `cow.s35.regression.run` and reads the result, following F-2's pattern. A run shows progress in the S35 card and can be cancelled.
- **Panel check lines:** pass, fail, skipped or info per check and size, with the first difference on a fail.

### Logging and observation

- **Channel:** `S35`.
- **Info:** run start and finish, each check's result, wall time per size.
- **Error:** a failed check with the first differing participant, path and values.
- **Metrics:** per-check duration, worst coinciding tick, runs completed.
- **Health:** a failed check is Degraded until the next run.

## Acceptance Criteria

1. The runner steps one day per frame through the manual step, never reads wall time, and a 20-year run at each size completes with one `SimulationTick` per day.
2. `CaptureState()` returns every E1 participant's id and JSON without writing a slot; the combined hash is stable across identical runs and changes when any one field changes.
3. `Compare` returns the first differing participant and JSON path with both values, and identical otherwise.
4. R1 compares end hashes across all range counts and the repeat for each size; with a serial executor it passes with the "ranges not exercised" note.
5. R2 and R3 save and snapshot at `saveDay` (Y10 W33 d4), continue to the end, restore, continue again, and compare with the uninterrupted hash; R3 is `skipped` with a reason when no snapshot store exists.
6. R4 runs all phases against a scratch copy in `out/CoW/s35/scratch_assets/`, leaves the repository untouched (checked by `git status`), proves identical resolved parameters and day-0 hashes across two fresh processes, and proves a mismatched start-state hash is refused with the bundle loading without it.
7. `dia launch` / `dia run` accept `--source <path>` overriding the computed `source` alias, and a manifest `source` is still rejected (dia-needs #16 amended).
8. R5 reports median and worst for the A4 week and A1 band steps and the worst coinciding tick, writes the machine description, gates only on a machine listed in `reference_machine.json`, and is `skipped` in Debug builds.
9. R6 passes only when every registered layer has a schema entry, every emitted event type is described, and `dia validate schema` is clean for the world-sim data files; failures list the offending items.
10. The report has every field above, `gate.met` is true only under the rule above, and the exit code is non-zero on any `fail`.
11. One `dia.cell_history` export per size is written and opens in T-1.
12. The regression bundles are committed, one per size, and the run uses them rather than `default_world`.
13. Small runs end to end under the CI command; the wall times of the first run are recorded in the report.
14. The panel shows pass, fail, skipped or info lines per check and size, shows progress, and cancels a running regression.
15. Switching to Regression with unsaved parts requires confirmation (Feature 1), and a regression run replaces the loaded world.

## Test Strategy

- **GoogleTests (TDD):**
  - `S35StateCapture`: canonicalisation, hash stability, sensitivity to a single field, first-difference path over nested objects and arrays, truncation of long values.
  - `S35PerfStats`: median, worst and coinciding tick from scripted samples; the reference-machine rule with a fixture machine list.
  - The runner with fake systems and a scripted clock: R1 to R3 flows (identical, one-field divergence, serial-executor note, missing snapshot store), cancellation, report content against a golden file, `gate.met` truth table.
  - E1 `CaptureState()` returns the participants without writing.
- **pytest (outer driver, stub launcher):** the R4 phase choreography, scratch copy creation and `git status` cleanliness check, the R6 CLI step and its parsing, report merge, exit code.
- **DiaCLI pytest:** `--source` override and the manifest-`source` rejection.
- **E2E:** the CI command at Small, expecting a report with all checks `pass` or an explained `skipped`/`info`.
- **Nightly:** Standard and Large with the `SLOW_` convention, on the reference machine.
- **Visual gate:** the Regression area of the mockup.

## Binding Decisions

| Source | Decision | How this feature complies |
|---|---|---|
| CH-S35-03 | First-run numeric defaults (20 years, ranges 1/4/max, year-10 mid-season save, two fresh relaunches, median and worst on a recorded reference machine, zero undescribed) | The configuration and rules above |
| CH-S35-04 | A start-state hash mismatch is refused and the bundle loads without it | R4 phase C |
| CH-S35-05 | R1, R2 and R5 gate S-4 Live mode; R3, R4, R6 report | `gate.met` |
| System Resolved Decision 2 | The combined hash comes from E1 participant output | `CaptureState()` and the hash above |
| System Resolved Decision 3 | A fixed committed bundle | One committed bundle per size |
| PD-009 | Generated output under `Cluiche/out/<App>/` | Report, exports and scratch under `out/CoW/s35/` |
| DiaCLI rule | Never run executables directly; use `dia` | The driver launches only through `dia launch` / `dia run` |
| A2 | One `SimulationTick` per frame | Manual stepping one day per frame |
| AD-001 | Ask before Dia and shared edits | The `--source` option (Dia) and E1's `CaptureState()` need sign-off; E1 is CoW |
| No new simulation code | S-3.5 only wires modules | The runner reads state and gauges only |

No conflicts.

## Dependencies

| Dependency | Use | Status |
|---|---|---|
| Feature 1 | Stage, modes, history driver | Approved |
| F-3 Features 1 to 4 | Bundles, packs, start states, save-as, locator | Approved, not built |
| DiaCellHistory Features 1, 3 and 5 | Recorder, snapshots, export | Approved, not built |
| DiaValueSchema Features 1, 2, 6 | Coverage checks, `FindEvent` | Approved, not built |
| E1 (`CaptureState()`), A2 manual step | State capture and clock | Done; E1 amended |
| dia-needs #16 (`source` alias) with the `--source` override | R4 scratch root | Approved; amended; needs sign-off |
| A4 and A1 last-step gauges | R5 timings | Consumer requirement (below) |
| CoW `ISnapshotStore` adapter | R3 | Backlog task, not built |
| S-1 Features 1 and 8, A4 Features 1 to 3, A1 Features 1 to 7 | The world under test | Specced, not built |
| A3 | Adds its state to the hash and its layers to R6 when built | Specced, not built |

**Consumer requirements:** A4 and A1 each expose a gauge holding the duration of their most recent step (A4 `cow.weather.step_ms`, A1 `cow.ecology.band_ms`), registered with `MetricInfo`, so R5 and the S-2.5 live smoke read one source.

## Resolved Decisions

1. **R4 runs on a scratch copy** (2026-10-20), through a `--source` override on `dia launch` and `dia run`, so a regression run never changes the repository and a crash leaves nothing to clean up.
2. **One committed bundle per size** (2026-10-20), because a bundle holds one baked terrain; the gate still does not move with `default_world` or S-1's rules.
3. **R5 gates only on the recorded reference machine** (2026-10-20); elsewhere it reports numbers as `info`, so a slow laptop cannot fail the gate and a missing measurement cannot pass it.
4. **`gate.met` is conservative** (2026-10-20): true only when R1 and R2 pass and R5 passed on a reference machine.
5. **A serial executor passes R1 with a note** (2026-10-20), instead of pretending the range counts were tested.
6. **Any failed check fails the run's exit code** (2026-10-20); only R1, R2 and R5 decide S-4 gating.

## Open Design Questions

_None remaining._

## Status

`Approved` (2026-10-20). Plan not yet created. Needs AD-001 sign-off for the `dia launch|run --source` override; E1 `CaptureState()` is a CoW-side amendment.
