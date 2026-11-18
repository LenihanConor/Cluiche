# Plan: A4 Climate Cycles (A4 Weather Feature 8)

**Spec:** @docs/specs/applications/cow/systems/a4-weather/climate-cycles.md
**Status:** Not Started
**System plan:** @docs/specs/applications/cow/systems/a4-weather/a4-weather.plan.md (row 6b)

**Starts after A4 Feature 1 (`weather-data.plan.md`) tasks 1 to 14 are Done** (`WeatherTerrain`, `ParamError`, the file
envelope conventions). Tasks 2 to 11 and 19 are pure logic on hand-built terrain and need nothing else except task 8
(needs `presets-and-regression.plan.md` task 4). Tasks 12 to 17 need pieces of Features 2 and 3 and are marked "Blocked
until ... task N". Tasks 20 to 22 need Features 3 and 5 pieces. **Nothing in this plan is built today** (Features 1 to 6
are all unbuilt); every Blocked mark is a start condition, not a defect.

**Division of work with the sibling plans (no duplication).** Already planned and **not repeated here**:
- the F2 pass edits: ca-pipeline 6 (pass 1 reads `tempOffset`, zero by default), 8 (`PrevailingDirection` takes an
  effective latitude), 9 (pass 4 reads `evapMultiplier`, 1 by default). This plan supplies the non-zero values and the
  tests that prove they act (tasks 12 to 14);
- the module wiring: weather-module 10b (module owns `ClimateCycles`, calls `LatchClimate` in `Capture`, the `climate`
  config block) and 12 (the spin-up job with the frozen climate day, AC 37 and 38 of that spec). This plan builds the
  classes and the `WeatherRunner` additions those rows call;
- the save block: weather-save 17a (`climate` block wired into the record, AC 23 of that spec). This plan builds the
  pure codec it calls (task 19);
- `ApplyPreset`'s cycle reset and the preset `climate` block: presets-and-regression 10a (AC 7a, 7b). This plan only
  supplies `DisableAll`, `SetEnabled` and `Pin` (task 9). **AC 32 and 33 of this spec are proved there**;
- the `cow.weather.info what:cycles` and `params action:cycle|reload` handlers and the `weather.climate_offset` layer
  registration: weather-debug 14, 17 and 10. This plan builds the report and queue-entry functions they call (tasks 20,
  21) and the committed offset accessor the layer reads (task 12).

**Order against the sibling plans:** task 9 (the mutators) before presets 10a; tasks 2 to 11 before weather-module 10b
(it needs the types); task 17 (runner) before weather-module 12; task 19 before weather-save 17a.

All code is CoW (application) under `Cluiche/CoW/Shared/Weather/`, `Cluiche/CoW/Modules/` and
`Cluiche/Assets/CoW/Global/Weather/`. No Dia files are added or edited (AD-001). If task 21 finds DiaCellHistory has no
scalar-series API and S-2.5 wants one, that is a Dia change: flag it and ask, do not build it. Where a task could need more
than sonnet it says "ask before escalating" (feedback_confirm_opus).

## Implementation Patterns

### Files (Task 1)
- `Shared/Weather/ClimateCycles.h/.cpp`: defs, the `ClimateCycles` class, wave and phase maths, patterns, `Evaluate`,
  `Coldness`, start spec, phase state.
- `Shared/Weather/ClimateCycleFile.h/.cpp`: `LoadClimateCycles`, `ApplyCycleOverrides`, `HashClimateCycles`,
  `ParseClimateConfig`, the save codec (task 19).
- `Shared/Weather/Testing/ClimateTestCycles.h`: hand-built cycle sets (default pair, single sine, single keyframe, 16
  max-amplitude cycles) and small terrains (reuse Feature 6's `MakeCoastRange` when built, else a local copy of the same
  shape, recorded in notes).
- Tests in `Tests/GoogleTests/CoW/Weather/`: `TestClimateCyclesFile`, `Wave`, `Pattern`, `Eval`, `Start`, `Pipeline`,
  `Spinup`, `Save`, `Debug`. Suite prefix `ClimateCycles*` (spec scoped filter `*Climate*`).
- Asset `Cluiche/Assets/CoW/Global/Weather/climate_cycles.json` (task 18).
- Namespace `CoW::Weather`. No STL in public signatures (PD-004): `DynamicArrayC`, `StringCRC`, fixed `char idText[32]`.

### File loader (Tasks 2 to 4)
- Envelope `{"format":"cow.climate_cycles","version":1,"climate_cycles":[...]}`. Same strict style as Feature 1: unknown
  key invalid at every level (including the removed `period`, `phase_offset` and any moisture amplitude), all or
  nothing, the first fault names its path (`climate_cycles[1].amplitude.temperature`). `ParamError` (key `String64`,
  reason `String256`) is Feature 1's type, reused.
- Validation order within one cycle: keys, `id` (1 to 31 chars, `[a-z0-9_]`, unique across the file), `enabled` bool,
  `period_days` integer in [7, 36,400], `phase_days` integer in [0, period), optional `pinned_phase_days` integer, `shape`
  (`"SINE"` string or `{"type":"KEYFRAME","points":[[u,v],...]}`), amplitudes in range, `pattern`. Numbers must be finite;
  integers are rejected when they carry a fraction. At most 16 cycles. Keyframes: 2 to 64 points, first `u` 0, last `u` 1,
  `u` strictly increasing, first value equals last value, values in [-1, 1].
- `ApplyCycleOverrides` builds into a local `ClimateCycleSet`, validates, then moves; on failure `inOut` is byte-identical.
  Overrides use the same cycle JSON shape keyed by `id`; an unknown id is an error.
- `HashClimateCycles`: FNV-1a 64-bit over the bit patterns of every definition field in a fixed order (id bytes,
  `enabled`, period, phase, pinned flag and value, shape, keyframe floats, amplitudes, pattern fields), so adding a field
  changes the hash by construction. The test changes each field in turn and expects a different hash.
- Defaults are authored once as `DefaultCycles()` in the testing header from the GDD section 10b values: `oscillation_4y`
  (period 1,456, SINE, amplitudes 0.03, 0.15, 0.03, `COAST_INLAND` coast 1.0 inland 0.5) and `ice_age_40y` (period 14,560,
  KEYFRAME `[[0,0],[0.6,1],[1,0]]`, amplitudes -0.12, -0.2, -0.05, `LATITUDE` 0.6 to 1.0). The test also asserts the
  periods against `kDaysPerYear` (364), not the other way round.

### Wave and phase maths (Tasks 5, 6)
- `uint64_t pos = (hasPinned ? pinnedPhaseDays : day) + phaseDays; pos %= periodDays; u = double(pos) / double(periodDays)`:
  integer arithmetic first, **one** divide, so `WaveAt(d) == WaveAt(d + periodDays)` is exact (AC 10).
- `SINE`: `cos(2 * pi * u)` in `double`, with `pi` a single literal. Exactly 1 at `u = 0` and exactly -1 at `u = 0.5`;
  at `u = 0.25` and `0.75` the value is about `6e-17`, so those two assertions use a `1e-12` tolerance (open item 6).
- `KEYFRAME`: find the segment with a linear scan over at most 64 points (cheaper to read than a binary search at this
  size), `v0 + (v1 - v0) * ((u - u0) / (u1 - u0))` in `double`. Exact at the points. The curve wraps by construction
  (first value equals last).
- `WaveAt` and `PhaseAt` are `const` and read nothing but the definition: no state, no RNG.

### Patterns (Tasks 7, 8)
- Weights computed once in `Initialise` (and on reload), stored per cycle as `DynamicArrayC<float>` sized N, except
  `GLOBAL` which stores nothing and reads as 1. Worst case 16 arrays at Large (about 430 KB).
- `LATITUDE`: `a + (b - a) * latitude[c]`. `COAST_INLAND`: `coast + (inland - coast) * clamp(d / 6, 0, 1)` with `d` from
  Feature 6's `ComputeDistanceToWater(terrain, topology)` (presets plan task 4). **No second breadth-first search is
  written.** Its unreachable sentinel is clamped by the formula; task 8 checks the sentinel value and a no-water map is
  already rejected by `WeatherSimulation::Initialise`. `REGIONS`: `weight` for cells whose region id (from the optional
  `regionOfCell` array) is in `ids`, 0 elsewhere. `Initialise` returns false, changes nothing and names the id when a
  `REGIONS` cycle lists an id absent from `regionOfCell`.

### Evaluation (Task 9)
- `Evaluate(day, out)`: compute each enabled cycle's wave **once**, then for each cell loop the cycles in file order,
  accumulating `double` sums of `amp * weight * wave`; clamp `tempOffset` to [-0.3, 0.3], `evapMultiplier = 1 + sum`
  to [0.25, 2.0], `windShift` to [-0.25, 0.25]; store `float`. A disabled cycle is **skipped by a branch before any
  multiply**, so the result is bit-identical to the cycle being absent (AC 15). With no enabled cycle `out.active` is
  false and the arrays are not written.
- Output arrays belong to a `ClimateOffsets` sized at `Initialise` by the caller (`ClimateOffsets::Resize(n)`); `Evaluate`
  allocates nothing.
- Mutators: `SetEnabled`, `Pin`, `Unpin`, `DisableAll`, `RestorePhaseState`. Unknown id: no-op returning false and one
  Warning counter (the caller logs). Calling them mid-step is the module's responsibility (task 20 routes them through the
  mutation queue); they never run inside `Run`.

### Start phase and config (Task 10)
- `ClimateStartSpec` and `ClimatePhaseState` as in the spec. `phase_days` per cycle: `warm` the file's value;
  `fraction` `round(f * period) % period` for `f` in [0, 1] (an unknown id warns and is ignored, reported through a
  returned count); `seeded` `Fnv1a64(worldSeed as 8 little-endian bytes, then idText bytes) % periodDays`. No RNG state.
- `ChooseStartSpec(presetSpec*, configSpec*)` returns the first non-null, else `warm` (AC 21).
- `ParseClimateConfig(const Json::Value& block, ClimateConfig& out, ParamError&)` for
  `"climate": {"file": "...", "start": "warm" | "seeded" | {"<id>": <fraction>}}`. A missing block is "no cycles" and not an
  error. An unknown key, a wrong type or a fraction outside [0, 1] is an error naming the key.

### Coldness (Task 11)
- `Coldness(cell, day)`: `T` is the cell's current signed temperature sum (enabled cycles, using the pin if set).
  `Tmin` and `Tmax` use each cycle's own wave extremes: `SINE` -1 and 1; `KEYFRAME` the min and max of its point values
  (the curve is linear, so extremes are at points). For each cycle the contribution is `amp * weight * wave`, so a negative
  amplitude swaps which extreme is cold. `coldness = (Tmax - T) / (Tmax - Tmin)`, 0 when `Tmax == Tmin`, clamped to
  [0, 1]. Computed on the fly (16 cycles at most, called a handful of times at spin-up start), so no extra arrays.
- On the default warm start at day 0 this is 0 for every cell: `oscillation_4y` is at its warm peak (+0.03 at weight w) and
  `ice_age_40y` is at wave 0 with a negative amplitude, whose maximum is 0. The test hand-computes both.

### WeatherSimulation additions (Tasks 12 to 15)
- `SetClimate(const ClimateCycles*)` (null means no cycles), `LatchClimate(uint64_t evalDay)` which calls `Evaluate` into a
  working `ClimateOffsets` owned by the simulation (sized at `Initialise`), and a committed copy that `Commit` swaps with
  the working one (swap, no copy) so `GetCommittedClimateOffset()` returns the temperature offset of the committed week for
  the `weather.climate_offset` layer.
- **Latch rule (open item 1):** `Capture` resets the working climate latch to inactive; `LatchClimate` is called **after**
  `Capture` and before `Run` (as weather-module 4 writes it) and sets it. A week where cycles are disabled therefore never
  inherits last week's offsets. `Run` reads only the latched arrays and the `active` flag (DFS-002).
- The passes already read the offsets with zero defaults (ca-pipeline 6, 8, 9). These tasks only assert the effect; they
  change a pass only if the read is missing.

### Frozen-phase spin-up (Task 17)
- `WeatherRunner::Start(..., uint64_t climateFreezeDay = 0)`: each spin-up week calls `LatchClimate(climateFreezeDay)`
  (the same day every week, pure, so the arrays are identical for all 52 weeks). `StartColdness(cell)` returns
  `climate.Coldness(cell, climateFreezeDay)`. The runner's week count stays `spinup_weeks`; nothing extends it.
- (seam fix 2026-11-17, user) `StartSnowSurvivesSummer(cell)`: 1 when `SeasonalTargetTemperature(cell, summer peak week)`
  plus the latched cycle temperature offset at `climateFreezeDay` is below `snow.melt_threshold`. Pure and computed before
  week 1. Feature 3 (weather-module 12) passes both per-cell arrays to A5's `BeginSpinUp(rev, SpinUpStart{coldness, mask})`.

### Save codec (Task 19)
- `EncodeClimateState(const ClimateCycles&, Json::Value& out)` writes `{"cycles_hash": <uint64>, "cycles": [{"id", "enabled",
  "phase_days", "pinned_phase_days"?}]}`. `cycles_hash` is a 64-bit value; jsoncpp writes it as `UInt64`, and the test proves
  a hash above 2^53 survives real JSON text (open item 8).
- `bool DecodeClimateState(const Json::Value&, ClimateCycles&, ClimateRestoreReport&, CodecError&)` and `Validate`
  (at most 16 cycles, unique ids of 1 to 31 chars, non-negative integers; mutates nothing). Matching is by `id`: a saved id
  missing from the current file is dropped with a warning counted in the report; a file cycle missing from the save keeps
  the phase from the current `ClimateStartSpec`; a `phase_days` at or past the current period is reduced modulo the period
  with one Warning (weather-save spec); a `cycles_hash` mismatch sets `report.hashMismatch` (the caller warns) and keeps the
  saved phases for matching ids. An absent block restores as "no active cycles", no warning.

### Debug and queue entries (Tasks 20, 21)
- `CycleReport BuildCycleReport(const ClimateCycles&, uint64_t day, int64_t cell /* -1 = none */)`: per cycle id, enabled,
  pinned, `periodDays`, `u`, `wave`, the global-weight contribution to each of the three fields, plus totals and coldness for
  the chosen cell. Pure; the `info` handler calls it with the scrubbed `day`, so no recording is ever needed.
- `MutationQueue` gains a `CycleChange { Kind { Enable, Disable, Pin, Unpin, Start }; StringCRC id; uint32_t value; }`
  entry and `QueueReload` gains a `Target { Params, Cycles }` argument. Both validate before enqueue (unknown id returns
  false, nothing queued) and apply at the next `Capture`, never mid-`Run` (AC 38). This edits `weather-module.plan.md`'s task
  7 file set; weather-module 7's row does not list these entries (open item 4).

### Verification cadence
- Per task: narrow `dia run googletest --filter="ClimateCycles*" --shards 2` (always `--shards 2`; add `WeatherPass*` or `WeatherLifecycle*` for tasks
  that touch Feature 2's files). The 104-week cases run once at the end of their own task, not per edit.
- **No full-suite run in this plan** (it is `worldstep.plan.md` Weather-stack close-out). No mutation checks. Sanitizer: on request only (user, 2026-11-17). The feature-wide filter runs only in the scoped-verification task; the full suite and the single Release build run once in worldstep.plan.md, Weather-stack close-out. No `SLOW_` suites (longest run is 104 weeks). (2026-11-17, speed-ups)
- One commit per task, with the plan row updated in the same commit through `dia docs plan`.

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 0 | Prerequisite check, read only: record the build state of A4 Features 1 to 6 (all unbuilt at planning time) and which tasks wait. Confirm: Feature 2's names for the three offset inputs read by passes 1, 4 and 3 (ca-pipeline 6, 8, 9) and that `Capture` resets them; presets plan task 4's `ComputeDistanceToWater` signature and its unreachable sentinel; where `WeatherRunner` lives and which plan builds it (weather-module 3 delegates to it, the spec names S-2.5 Feature 3); `kDaysPerYear` in `GameSpeedController.h`; whether DiaCellHistory exposes a scalar series (spec Files Touched); A5's `SpinUpStart` shape (`hydrology-module.md`, lines near 224) | Findings recorded in notes | Not Started | haiku | Mechanical, TDD-exempt. If DiaCellHistory has no scalar series, record it for task 21 and do not build one (Dia change, ask first). If `WeatherRunner` has no owner plan, flag it before task 17 |
| 1 | Create empty `ClimateCycles.h/.cpp`, `ClimateCycleFile.h/.cpp`, `Testing/ClimateTestCycles.h` and the nine test files; register each with `dia docs vcxproj-add CoW ...` and `dia docs vcxproj-add GoogleTests ...` (literal paths, one call per file, PowerShell) | Solution builds with the empty files | Not Started | haiku | Mechanical, TDD-exempt. Idempotent. Later tasks edit in place and do not touch the vcxproj again |
| 2 | RED then GREEN: cycle types and `LoadClimateCycles` valid path, `DefaultCycles()` | AC 1 (first half: a valid file loads; `DefaultCycles()` loads and equals the GDD section 10b values written as literals in the test), 5 (periods taken as written; 1,456 and 14,560 are whole multiples of `kDaysPerYear`, asserted against the constant) | Not Started | sonnet | Needs Feature 1 tasks 3 to 6 (`ParamError`). Prove RED with quoted output. Literals in the test, not read from `DefaultCycles()`, so it cannot pass vacuously |
| 3 | RED then GREEN: `LoadClimateCycles` rejection table | AC 2, 3: wrong `format` or `version`; an unknown key at every level (including the removed `period`, `phase_offset` and a moisture amplitude); duplicate or malformed `id`; unknown `shape` or pattern `type`; `period_days` out of range; `phase_days >= period_days`; non-finite numbers (built programmatically, JSON cannot carry them); an amplitude out of range; 17 cycles; each keyframe rule (first `u` not 0, last not 1, `u` not strictly increasing, first value not equal to last, fewer than 2 or more than 64 points, a value outside [-1, 1]). Each returns false, leaves `out` untouched and names the offending path | Not Started | sonnet | Table-driven where the same edit applies to several fields. Commit tasks 2 and 3 together |
| 4 | RED then GREEN: `ApplyCycleOverrides` and `HashClimateCycles` | AC 4 (a failed override leaves `inOut` byte-identical; a rejected load leaves the previous set active), 6 (hash stable for equal sets; changes when any single definition field changes, one assertion per field in a loop) | Not Started | sonnet | Atomic by building into a local set. The field loop is over the definition struct's members in the hash order, so a new field fails the test until it is hashed |
| 5 | RED then GREEN: SINE wave, integer phase formula, pin | AC 7 (`WaveAt` is 1 at `u = 0`, -1 at 0.5, about 0 at 0.25 and 0.75 within `1e-12`), 10 (`WaveAt(d)` equals `WaveAt(d + period)` bit for bit for 1,000 sampled days), 11 (`phase_days` and `d` shift `u` as the formula says and wrap), 16 (wave-level: a pinned cycle gives the same wave every day, equal to `u = ((pinned + phase) mod period) / period`; `Unpin` restores the day-driven wave) | Not Started | sonnet | Pure. The pin functions here are the definition-level fields; the runtime mutators land in task 9 |
| 6 | RED then GREEN: KEYFRAME wave and the default ice-age curve | AC 8 (values at points exact; midway is the average; the curve wraps at 1; a slow-rise fast-fall curve is asymmetric), 9 (`(0,0), (0.6,1), (1,0)`: wave 0 at `u = 0`, 0.5 at 0.3, 1 at 0.6, 0.5 at 0.8, 0 at 1; with amplitude -0.12 the offset at `u = 0.6` is -0.12 at weight 1) | Not Started | sonnet | Pure. Known answers by hand. Commit tasks 5 and 6 together |
| 7 | RED then GREEN: `ClimateCycles::Initialise` and the `GLOBAL`, `LATITUDE`, `REGIONS` patterns | AC 12 (first three bullets plus the region-absent rule): `GLOBAL` weight 1 everywhere and stores no array; `LATITUDE` weight `weight_at_equator` (0.6) at latitude 0, `weight_at_pole` (1.0) at 1, linear between; `REGIONS` weight on listed regions and 0 elsewhere; a region id absent from the world makes `Initialise` return false and change nothing | Not Started | sonnet | Hand-built terrain. A `REGIONS` cycle with no `regionOfCell` array supplied also returns false (decision recorded in the test name) |
| 8 | RED then GREEN: `COAST_INLAND` pattern using Feature 6's distance field | AC 12 (coast bullet): weight `coast` on a coastal cell, `inland` at distance 6 or more, linear between, with a hand-built row of known distances; a cell with an unreachable-sentinel distance gets `inland` | Not Started | sonnet | **Blocked until presets-and-regression task 4 (`ComputeDistanceToWater`) is Done.** Do not write a second breadth-first search; if task 4 is not built, build it there first (it needs only Feature 1) |
| 9 | RED then GREEN: `Evaluate`, summing, clamps, disable, mutators | AC 13 (two cycles sum per field in file order), 14 (the three totals never leave their clamps with 16 maximum-amplitude cycles), 15 (a disabled cycle gives bit-identical output to the cycle being absent from the file), 16 (pin and `Unpin` at `Evaluate` level), 17 (first half: with no enabled cycle `active` is false and the arrays are untouched), plus `DisableAll`, `SetEnabled`, `Pin`, `Unpin` on known and unknown ids | Not Started | sonnet | Pure. `Evaluate` allocates nothing (output sized by the caller). These mutators are what presets-and-regression 10a calls (AC 32 and 33 are proved there). Commit tasks 7 to 9 |
| 10 | RED then GREEN: start modes, precedence, config parse | AC 18 (warm: saved `phase_days` is the file's for every cycle and day 0 gives wave 1 for `oscillation_4y` and wave 0 for `ice_age_40y`), 19 (fraction: `round(f * period)` per id, an unknown id warns and is ignored), 20 (seeded: the same seed and id always give the same `phase_days`; different seeds differ for at least one cycle across a set of 8 seeds; no RNG state touched), 21 (precedence: preset beats config beats default), `ParseClimateConfig` valid and each invalid case names the key | Not Started | sonnet | Seeded hash is the fixed FNV-1a described in Implementation Patterns, so the golden `phase_days` can be written as literals in the test. An absent `climate` block is not an error |
| 11 | RED then GREEN: `Coldness` | AC 30: 0 on the default warm start at day 0; 1 for a cell when every cycle is pinned at its coldest wave; always in [0, 1] over a sweep of days and cells; 0 when `Tmax == Tmin` (a single zero-amplitude cycle); deterministic (two calls equal) | Not Started | sonnet | Hand-compute the default-start cases. A negative amplitude swaps which wave extreme is cold, so include a case with a negative-amplitude KEYFRAME and a positive-amplitude SINE together |
| 12 | RED then GREEN: `SetClimate`, `LatchClimate`, committed offset, capture isolation, no-cycle regression | AC 17 (second half: **104 weeks bit-identical** between a simulation with no climate set and one with an empty or all-disabled climate), 26 (after `LatchClimate`, poisoning the `ClimateCycles` object does not change `Run`), 39 (the committed offset accessor equals the latched `tempOffset` of the committed week and is all zero with no cycle; the layer registration itself is weather-debug 10), plus the latch rule: a week with cycles disabled after a week with cycles on carries no offset | Not Started | sonnet | **Blocked until ca-pipeline task 5 (lifecycle) and task 20 (determinism harness).** Write the AC 17 test **first**, against the unmodified pipeline, so it protects everything already built. Edits Feature 2's `WeatherSimulation` in place; run `WeatherLifecycle*` in the same filter. Ask before escalating |
| 13 | RED then GREEN: temperature and evaporation entry points | AC 22 (a uniform `tempOffset` of +0.05 on a no-wind, no-moisture world raises the pass 1 target by 0.05 before the clamp), 23 (`evapMultiplier = 2` doubles primary ocean evaporation unless saturated; secondary evaporation unchanged) | Not Started | sonnet | **Blocked until ca-pipeline task 6 (pass 1) and 9 (pass 4).** Those rows add the zero-default reads; this task proves the non-zero values act. If a read is missing, fix it in the pass and say so in the notes |
| 14 | RED then GREEN: wind shift entry point | AC 24: with `windShift = +0.1` the latitude band boundary moves poleward by 0.1 including the transition lerp; with shift 0 the directions equal the existing function exactly; the column-parity tie rule is unchanged | Not Started | sonnet | **Blocked until ca-pipeline task 8 (`PrevailingDirection` takes an effective latitude).** Effective latitude is `clamp(latitude - windShift, 0, 1)`. Commit tasks 12 to 14 |
| 15 | RED then GREEN: range independence with both default cycles on | AC 25: all fields bit-identical at 1, 3, 7 and 17 ranges and with ranges in reverse order over 104 weeks | Not Started | sonnet | **Blocked until ca-pipeline task 2 (`ReverseRangeExecutor`, `FixedRangeExecutor`) and task 20.** Use those executors, never a second set. Run the 104-week cases once at the end of the task. If a pair differs, stop: it is an ordering bug |
| 16 | RED then GREEN: deferred versus synchronous with cycles on | AC 27: a `WeatherStage` driven through the manual executor matches `WeatherRunner` bit for bit over 104 weeks with cycles on | Not Started | sonnet | **Blocked until weather-module task 10b (module owns the cycles) and ca-pipeline task 21 (manual-launcher adaptor).** If either is unbuilt, leave Blocked with `dia docs plan` and continue |
| 17 | RED then GREEN: `WeatherRunner` frozen-phase spin-up, `StartColdness` and `StartSnowSurvivesSummer` (seam fix 2026-11-17) | AC 28 (spin-up offsets are identical for all 52 weeks and equal `Evaluate(freezeDay)`), 29 (the offsets of the last spin-up week equal those of the first live week for warm, fraction and seeded starts), 31 (spin-up length stays `spinup_weeks` with cycles on or off; nothing extends it), 31a (`StartColdness` and `StartSnowSurvivesSummer` are the only values handed to A5), 31b (the mask: 0 on a warm temperate start, 1 on a cold high cell below `melt_threshold` with the ice age pinned, 0 just above, unchanged across spin-up) | Not Started | sonnet | **Blocked until `WeatherRunner` is built (task 0 finds its owner) and weather-module task 3.** The module-level assertions (weather-module AC 37, 38) are weather-module 12's. Does not build the snow-survival mask (open item 2) |
| 18 | Author `Assets/CoW/Global/Weather/climate_cycles.json` with the two default cycles; a test loads the shipped file and compares it to `DefaultCycles()`; add the `climate` config block to the WeatherModule instance when its manifest row lands | AC 1 (second half: the shipped file equals the GDD values), 5, 18 against the real file; the default start is warm | Not Started | sonnet | The compiled defaults and the file must not drift: the test is the guard. Register the asset the usual way. **The `.diaapp` `climate` block is blocked until weather-module task 14** (no hosting instance exists yet); leave a one-line note on that row rather than editing a manifest that has no WeatherModule |
| 19 | RED then GREEN: climate save codec | AC 34 (codec half: `enabled`, `phase_days`, `pinned_phase_days` round trip through real JSON text with a seeded start and a pin both set, and a `cycles_hash` above 2^53 survives), 35 (a `cycles_hash` mismatch reports and keeps the saved phases for matching ids; a saved id missing from the file is dropped and reported; a file cycle missing from the save keeps the start-spec phase; a save with no `climate` block loads with no active cycles and no warning; `phase_days >= period` is reduced modulo with one reported warning), `Validate` rejects each listed defect and mutates nothing | Not Started | sonnet | Pure; needs only tasks 4 and 10. **The end-to-end halves are weather-save task 17a** (its AC 23: continued run bit-identical to the uninterrupted run, and AC 36 a save taken with a step in flight reloads identically). Do not repeat them here |
| 20 | RED then GREEN: cycle queue entries | AC 38: `QueueCycleChange` (enable, disable, pin, unpin, start) and `QueueReload(Target::Cycles)` apply at the next `Capture` and never mid-`Run`; an unknown id returns false and queues nothing; a failed cycle-file reload keeps the previous set and logs; the weather command count stays 8 (that count is asserted in weather-debug 13) | Not Started | sonnet | **Blocked until weather-module task 7 (`MutationQueue`).** Edits Feature 3's files in place (open item 4). The `params action:cycle` and `reload target:cycles` handlers are weather-debug 14's; this task supplies the entries they enqueue |
| 21 | RED then GREEN: cycle report for `info what:cycles` | AC 37: per cycle `u` and `wave` equal `PhaseAt` and `WaveAt`; with `day: D` they equal the values at `D` whatever the current day; the report carries enabled, pinned, `periodDays`, the global-weight contributions, totals and the chosen cell's coldness | Not Started | sonnet | Pure; no blocker. The handler is weather-debug 17's and the panel card rows are weather-debug 20's. If task 0 shows DiaCellHistory has no scalar series, record that the per-cycle wave history is **not** sampled into it (spec marks it uncertain) and flag any Dia change before doing it |
| 22 | RED then GREEN: gauges, logs, health for cycles | AC 40: at most one gauge per cycle named `cow.weather.cycle.<id>.wave` registered with `MetricInfo` and written once per `Capture` on SimPU; an invalid file gives Degraded and no active cycles; a missing file gives no cycles and one Info log; Info on load, reload and runtime change; Warning on a clamp, an unknown preset cycle id or a hash mismatch; Error on a bad file | Not Started | sonnet | **Blocked until weather-module task 10b and 15 (health causes and logs).** Gauges are unregistered and re-registered on reload so a removed cycle leaves no stale gauge. `DIA_TRACE_ZONE` on `LatchClimate` |
| 23 | Exhaustive test pass: gap audit of AC 1 to 40 (including 31a) against test names, then edge cases | Gap list in notes. Edge cases: a cycle with `period_days` 7 and 36,400; `phase_days` at `period - 1`; `pinned_phase_days` at or past the period; keyframe `u` values 1 ULP apart; 16 cycles all `REGIONS` with an empty id list; a 1-cell world; every cycle disabled then re-enabled between steps; two cycles with equal ids across a reload; day values near 2^32 and 2^63 (integer wrap behaviour recorded); `fraction` 0 and 1; a seeded start for a 7-day period; `Coldness` with every cycle disabled | Not Started | sonnet | No mutation check. Add only tests the audit shows missing. Production bugs found go to a separate fix task |
| 24 | Observation Opportunity Scan over every file touched across logs, traces, profiling, metrics, health | Suggestions in notes | Not Started | haiku | Non-blocking. Expected finds: `Evaluate` duration, a cycles-active gauge, reload outcome counter |
| 25 | Scoped verification: `dia run googletest --filter="ClimateCycles*" --shards 2`; `dia check deps`; `dia docs precommit` | Pass/fail and failing names only; AC 1 to 40 listed with the task that proved each (AC 32 and 33 cited from presets 10a), blocked ACs named | Not Started | haiku | **No full-suite run here** (it is `worldstep.plan.md` Weather-stack close-out). Report pass/fail only. ACs still Blocked at this point are reported as Blocked, not failed |
| 26 | Close-out: `dia docs plan` rows to Done with notes, then `dia docs spec-done docs/specs/applications/cow/systems/a4-weather/climate-cycles.md` | Spec status Done, plan header updated | Not Started | haiku | **Do not run until every row above is Done.** Not to be run when this plan is written. The spec's Status says approval waits on Features 2 and 3 being built and on A5 agreeing the hand-off: confirm both before closing |

## Open items

1. **Climate latch reset.** The spec says `Capture` calls `LatchClimate`; weather-module 4 writes `sim.LatchClimate(stepDay)`
   as a separate call after `sim.Capture`. For that order to be safe, `Capture` must reset the climate latch, otherwise a
   disabled cycle would reuse last week's offsets. Task 12 adds the reset and a test. Confirm with the Feature 2 owner if
   ca-pipeline task 5 is already built.
2. ~~**A5's seed hand-off does not match.**~~ **Resolved 2026-11-17 (user):** `SpinUpStart` is now two per-cell arrays (A5
   `hydrology-module.md`). This plan builds `StartColdness` and `StartSnowSurvivesSummer` (task 17, AC 31b). A5 seeds lakes
   from basin-mean coldness.
3. **`WeatherRunner` has no owner row in A4's plans.** weather-module 3 delegates to it and the spec names S-2.5 Feature 3.
   Task 17 needs it built and adds `climateFreezeDay` and `StartColdness`. Task 0 records where it lives.
4. **No cycle entries in weather-module 7.** The mutation queue row lists `SetParam`, overrides, forcings, biome, preset,
   reload, `StateReplace` and `ClearWindOverrides` only. Task 20 adds `CycleChange` and the reload target; adding a line
   to that row is a docs edit needing the user's agreement.
5. **Distance field reuse.** `COAST_INLAND` uses presets task 4's `ComputeDistanceToWater` with `dMax` 6 (the same inland
   cut-off Feature 6 uses). If that function's unreachable sentinel is not 65535 or similar, task 8's clamp test records
   it.
6. **Trigonometric exactness.** `cos(2 * pi * 0.25)` is not exactly 0 in `double`. AC 7 is asserted with a `1e-12`
   tolerance at 0.25 and 0.75 and exactly at 0 and 0.5. AC 10 stays bit-exact because `u` comes from integer arithmetic.
7. **Per-configuration drift.** Wave maths uses `double` with one fixed formula (PD-005). If task 15 or the goldens ever
   differ between Debug and Release, record it against ca-pipeline task 0's `/fp:precise` finding, do not widen a tolerance.
8. **`cycles_hash` as a JSON number.** A 64-bit hash exceeds 2^53. Task 19 stores it as `UInt64` through jsoncpp and proves
   the text round trip; if that fails the fallback is a 16-digit hex string, which also changes weather-save's example
   (`"cycles_hash": 1234`). Confirm if the fallback is taken.
9. **Spec Open Design Questions stay open by design:** whether the CA damps or amplifies the amplitudes (decided at the
   first S-2.5 run), whether `seeded` starts should avoid the coldest phase, and linear keyframe corners (spec questions 1,
   2 and 4). Nothing in this plan pre-empts them.
