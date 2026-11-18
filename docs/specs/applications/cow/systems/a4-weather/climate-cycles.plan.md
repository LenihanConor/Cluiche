# Plan: A4 Climate Cycles (A4 Weather Feature 8)

**Spec:** @docs/specs/applications/cow/systems/a4-weather/climate-cycles.md
**Status:** Not Started
**System plan:** @docs/specs/applications/cow/systems/a4-weather/a4-weather.plan.md (row 6b)

Rebuilt 2026-11-18 against the world-sim review amendment and the spec-6 resolutions. Decisions that shape it:
- **Cycles are data.** The cycle definitions (period, amplitude, phase, shape, pattern and what each cycle
  modulates) are a **cycle catalogue**: records in `climate_cycles.json`, validated by `climate_cycles.schema.json`
  and loaded through DiaData `JsonDefinitionLoader`. The hand-written strict file loader, its `ParamError` rejection
  table and `ApplyCycleOverrides` are **deleted**; reload is the kit's records reload.
- **Cycles stay per-cell field forcings** (spec-6 resolution 1). The three effects (temperature offset,
  ocean-evaporation multiplier, wind-latitude shift) are latched at `Capture` and applied in passes 1, 3 and 4 as
  today. DiaAttribute modifiers are used **only** where a cycle changes a scalar parameter, applied to the weather
  `ParameterFile`.
- **The wind-latitude shift is relative to S-1's westerly-band parameters**, not a 30/60 degree constant.
- **`cycle` is an action of `cow.weather.params`** (built with `ParamCommandHelper`), not a separate name.
- **Saved state** (`enabled`, `phase_days`, `pinned_phase_days`, `cycles_hash`) uses the kit save codec, with no
  in-record `version`. Cycle state is not modifier state.
- **Data I/O:** the file and sidecar are catalogued and resolved through `PathStore`; `FileWatcher` reload is dev-only.

**Needs these plans Done:**
- `weather-data.plan.md` (A4 Feature 1): types, `WeatherTerrain`, the sidecar and `ParamBinding`.
- `parameter-files.plan.md` (Dia): `ParameterFile`, modifier legality by kind.
- `grid-search-helpers.plan.md` (Dia): `MultiSourceSteps` over `HexNeighbours`, for the `COAST_INLAND` distance.
- `worldsim-kit.plan.md` (World Step): the records reload hook and the save codec.
- `ca-pipeline.plan.md` (A4 Feature 2), from the tasks that add the zero-default reads in passes 1, 3 and 4.
`biome-registry.plan.md` is only transitive; `blackboard-scopes.plan.md` is used only for the world-board `Set` of the
climate phase, which `weather-module.plan.md` writes.

**Startable earlier on fakes.** The catalogue, wave and phase maths, patterns, evaluation, start modes, coldness and
the save codec (Tasks 2 to 11, 19, 21) are pure logic on hand-built terrain and need only A4 Feature 1's types plus
the foundation record loader. Tasks needing pipeline passes, the runner, the module, presets or the command
registration are marked **Blocked until <named capability> from <plan>** and stay on fakes. Every Blocked mark is a
start condition, not a defect.

**Division of work with the sibling plans (no duplication).** Not repeated here:
- the pipeline edits: `ca-pipeline.plan.md` adds the zero-default reads of `tempOffset` (pass 1), the effective
  latitude in `PrevailingDirection` (pass 3) and `evapMultiplier` (pass 4). This plan supplies the non-zero values and
  the tests that prove they act;
- the module wiring: `weather-module.plan.md` has the module own `ClimateCycles`, call `LatchClimate` in `Capture`,
  read the `climate` config block and run the frozen-climate spin-up job. This plan builds the classes and the
  `WeatherRunner` additions those tasks call;
- the save block: `weather-save.plan.md` wires the `climate` block into the record; this plan builds the pure codec;
- `ApplyPreset`'s cycle reset and the preset `climate` block: `presets-and-regression.plan.md` (AC 32 and 33 are
  proved there). This plan supplies `DisableAll`, `SetEnabled` and `Pin`;
- the `cow.weather.info what:cycles` handler, the `params action:cycle|reload` registration and the
  `weather.climate_offset` layer: `weather-debug.plan.md`. This plan builds the report, the queue entries and the
  committed offset accessor they call.

**Order against the sibling plans:** the mutators (Task 9) before the preset plan's cycle task; the types (Tasks 2 to 11)
before the module's book-and-climate task; the runner (Task 17) before the module's coupled spin-up task; the codec
(Task 19) before the save plan's influences-and-climate task.

All code is CoW (application) under `Cluiche/CoW/Shared/Weather/`, `Cluiche/CoW/Modules/` and
`Cluiche/Assets/CoW/Global/Weather/`. No Dia files are added or edited (AD-001). If Task 21 finds DiaCellHistory has no
scalar-series API and S-2.5 wants one, that is a Dia change: flag it and ask. Where a task could need more than sonnet
it says "ask before escalating" (feedback_confirm_opus).

## Implementation Patterns

### Files (Task 1)
- `Shared/Weather/ClimateCycles.h/.cpp`: defs, the `ClimateCycles` class, wave and phase maths, patterns, `Evaluate`,
  `Coldness`, start spec, phase state.
- `Shared/Weather/ClimateCycleFile.h/.cpp`: `LoadClimateCycles`, `ApplyCycleOverrides`, `HashClimateCycles`,
  `ParseClimateConfig`, the save codec (Task 19). (`LoadClimateCycles` and `ApplyCycleOverrides` are deleted: the catalogue loader is the foundation's.)
- `Shared/Weather/Testing/ClimateTestCycles.h`: hand-built cycle sets (default pair, single sine, single keyframe, 16
  max-amplitude cycles) and small terrains (reuse Feature 6's `MakeCoastRange` when built, else a local copy of the same
  shape, recorded in notes).
- Tests in `Tests/GoogleTests/CoW/Weather/`: `TestClimateCyclesFile`, `Wave`, `Pattern`, `Eval`, `Start`, `Pipeline`,
  `Spinup`, `Save`, `Debug`. Suite prefix `ClimateCycles*` (spec scoped filter `*Climate*`).
- Asset `Cluiche/Assets/CoW/Global/Weather/climate_cycles.json` (Task 18).
- Namespace `CoW::Weather`. No STL in public signatures (PD-004): `DynamicArrayC`, `StringCRC`, fixed `char idText[32]`.

### Cycle catalogue (Tasks 2 to 4)
- Files: `Assets/CoW/Global/Weather/climate_cycles.json` (`format` `cow.climate_cycles`, key `climate_cycles`, the GDD's
  field names) and `climate_cycles.schema.json` (record entries: `id` 1 to 31 chars `[a-z0-9_]` unique and required,
  `enabled`, `period_days` [7, 36400], `phase_days`, `shape`, `amplitude.*` ranges, `pattern.*`, optional
  `pinned_phase_days`). Loaded through DiaData `JsonDefinitionLoader`. Unknown keys (including the removed `period`,
  `phase_offset` and any moisture amplitude), wrong `format` or `version`, range faults and malformed ids are rejected by
  the sidecar, **not by A4 code**; A4 tests only that its sidecar encodes them.
- A4 writes only the rules a sidecar cannot express: at most 16 cycles, a unique id across the file if the loader does not
  already enforce it, `phase_days < period_days`, and the keyframe rules (2 to 64 points, first `u` 0, last `u` 1, `u`
  strictly increasing, first value equals last, values in [-1, 1]). These run after load and fail the whole file naming
  the record path (`climate_cycles[1].amplitude.temperature`).
- **Strict load** (R1): a bad file fails loudly, health Failing naming file, record and field, and no cycles are active.
  (This replaces the old "Degraded and no active cycles"; whether a bad optional file should stay Degraded is open
  item 10.) A missing file means no cycles and an Info log.
- Reload goes through the kit's `cow.weather.records {action: reload|list}`: queued, strict, atomic (a failed reload keeps
  the previous set), applied at `Capture`. `FileWatcher` queues the same reload in dev builds. `ApplyCycleOverrides` does
  not exist.
- `HashClimateCycles`: FNV-1a 64-bit over the bit patterns of every definition field in a fixed order, so adding a field
  changes the hash by construction. It is saved as `cycles_hash`.
- Cycle targets: each record names its effects as the three amplitudes (field forcings). A record may also carry
  `parameters: [{key, op, amplitude}]`, a scalar parameter effect applied as a DiaAttribute modifier whose value is
  `amplitude * weight * wave` (Add) or `1 + ...` (Multiply), refreshed at `Capture`; no shipped cycle uses it (open item 1).
- Defaults are the GDD section 10b values: `oscillation_4y` (period 1,456, SINE, amplitudes 0.03, 0.15, 0.03,
  `COAST_INLAND` coast 1.0 inland 0.5) and `ice_age_40y` (period 14,560, KEYFRAME `[[0,0],[0.6,1],[1,0]]`, amplitudes
  -0.12, -0.2, -0.05, `LATITUDE` 0.6 to 1.0). The tests assert the periods against `kDaysPerYear` (364), not the other
  way round.

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
  `Dia::Pathfinding::MultiSourceSteps` over `HexNeighbours`, seeded with every water-source cell (the same helper the
  kit's `distance` derived field uses). **No hand-written breadth-first search.** Its unreached value (`kUnreached`)
  is clamped by the formula; Task 8 checks it, and a no-water map is already rejected by `WeatherSimulation::Initialise`.
  `REGIONS`: `weight` for cells whose region id (from the optional
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
  Warning counter (the caller logs). Calling them mid-step is the module's responsibility (Task 20 routes them through the
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
  `Capture` and before `Run` (as the module's latch hook writes it) and sets it. A week where cycles are disabled therefore never
  inherits last week's offsets. `Run` reads only the latched arrays and the `active` flag (DFS-002).
- The passes already read the offsets with zero defaults (`ca-pipeline.plan.md`). These tasks only assert the effect; they
  change a pass only if the read is missing. The wind shift is applied relative to the westerly-band latitudes S-1 owns and
  the pipeline latches at `Capture`.

### Frozen-phase spin-up (Task 17)
- `WeatherRunner::Start(..., uint64_t climateFreezeDay = 0)`: each spin-up week calls `LatchClimate(climateFreezeDay)`
  (the same day every week, pure, so the arrays are identical for all 52 weeks). `StartColdness(cell)` returns
  `climate.Coldness(cell, climateFreezeDay)`. The runner's week count stays `spinup_weeks`; nothing extends it.
- (seam fix 2026-11-17, user) `StartSnowSurvivesSummer(cell)`: 1 when `SeasonalTargetTemperature(cell, summer peak week)`
  plus the latched cycle temperature offset at `climateFreezeDay` is below `snow.melt_threshold`. Pure and computed before
  week 1. The module's coupled spin-up job passes both per-cell arrays to A5's `BeginSpinUp(rev, SpinUpStart{coldness, mask})`.

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
- `WeatherEntry` (the stage's tagged queue entry, an A4 type) gains a `CycleChange { Kind { Enable, Disable, Pin, Unpin,
  Start }; StringCRC id; uint32_t value; }` kind, and the reload entry gains a `Target { Params, Cycles }`. Both validate
  before enqueue (an unknown id returns false, nothing queued) and apply in `ApplyEntry` at the next `Capture`, never
  mid-`Run` (AC 38). They are the entries the `cycle` action of `cow.weather.params` enqueues (open item 4).

### Verification cadence
- Per task: narrow `dia run googletest --filter="ClimateCycles*" --shards 2` (always `--shards 2`; add `WeatherPass*` or `WeatherLifecycle*` for tasks
  that touch Feature 2's files). The 104-week cases run once at the end of their own task, not per edit.
- **No full-suite run in this plan** (it is `worldstep.plan.md` Weather-stack close-out). No mutation checks. Sanitizer: on request only (user, 2026-11-17). The feature-wide filter runs only in the scoped-verification task; the full suite and the single Release build run once in worldstep.plan.md, Weather-stack close-out. No `SLOW_` suites (longest run is 104 weeks). (2026-11-17, speed-ups)
- One commit per task, with the plan row updated in the same commit through `dia docs plan`.

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 0 | Prerequisite check, read only: record the build state of A4 Features 1 to 6 and which tasks wait. Confirm, by name: the pipeline's three offset inputs read by passes 1, 4 and 3 and that `Capture` resets them (`ca-pipeline.plan.md`); the westerly-band parameter names S-1 exposes and how A4 receives them; `MultiSourceSteps`/`HexNeighbours` and `kUnreached` (`grid-search-helpers.plan.md`); the kit's records reload hook, `JsonDefinitionLoader` record-sidecar validation and save codec (`worldsim-kit.plan.md`); `ParameterFile` modifier legality (`parameter-files.plan.md`); where `WeatherRunner` lives and which plan builds it; `kDaysPerYear` in `GameSpeedController.h`; whether DiaCellHistory exposes a scalar series; A5's `SpinUpStart` shape (`hydrology-module.md`) | Findings recorded in notes | Not Started | haiku | Mechanical, TDD-exempt. If DiaCellHistory has no scalar series, record it for Task 21 and do not build one (Dia change, ask first). If `WeatherRunner` has no owner plan, flag it before Task 17 |
| 1 | Create empty `ClimateCycles.h/.cpp`, `ClimateCycleFile.h/.cpp`, `Testing/ClimateTestCycles.h` and the nine test files; register each with `dia docs vcxproj-add CoW ...` and `dia docs vcxproj-add GoogleTests ...` (literal paths, one call per file, PowerShell) | Solution builds with the empty files | Not Started | haiku | Mechanical, TDD-exempt. Idempotent. Later tasks edit in place and do not touch the vcxproj again |
| 2 | RED then GREEN: cycle types, the catalogue sidecar and the `JsonDefinitionLoader` load of the shipped file; `DefaultCycles()` | AC 1 (first half: a valid file loads; the shipped file loads strictly and equals the GDD section 10b values written as literals in the test), 5 (periods taken as written; 1,456 and 14,560 are whole multiples of `kDaysPerYear`, asserted against the constant) | Not Started | sonnet | Needs the foundation record loader. Prove RED with quoted output. Literals in the test, not read from the loaded data, so it cannot pass vacuously. Catalogue the files; `dia validate schema` passes on both |
| 3 | RED then GREEN: sidecar coverage and A4's cross-record rules | AC 2, 3: the sidecar rejects each listed fault at load, naming the record path: wrong `format` or `version`; an unknown key at every level (including the removed `period`, `phase_offset` and a moisture amplitude); a duplicate or malformed `id`; an unknown `shape` or pattern `type`; `period_days` out of range; an amplitude out of range; a non-finite number (built programmatically); A4's own checks reject `phase_days >= period_days`, 17 cycles and each keyframe rule (first `u` not 0, last not 1, `u` not strictly increasing, first value not equal to last, fewer than 2 or more than 64 points, a value outside [-1, 1]); each failure fails the whole file, activates no cycle and sets health Failing | Not Started | sonnet | Table-driven. Tests that the sidecar encodes each rule, not the loader's own behaviour. Commit tasks 2 and 3 together |
| 4 | RED then GREEN: `HashClimateCycles` and reload atomicity | AC 4 (a reload with a bad file leaves the previous set active and byte-identical; the failure is reported), 6 (hash stable for equal sets and changes when any single definition field changes, one assertion per field in a loop over the struct members in hash order, so a new field fails the test until it is hashed) | Not Started | sonnet | Reload goes through the kit's records hook (Blocked until it exists: until then the test calls the pure load-and-swap function the hook will call). `ApplyCycleOverrides` is deleted |
| 5 | RED then GREEN: SINE wave, integer phase formula, pin | AC 7 (`WaveAt` is 1 at `u = 0`, -1 at 0.5, about 0 at 0.25 and 0.75 within `1e-12`), 10 (`WaveAt(d)` equals `WaveAt(d + period)` bit for bit for 1,000 sampled days), 11 (`phase_days` and `d` shift `u` as the formula says and wrap), 16 (wave-level: a pinned cycle gives the same wave every day, equal to `u = ((pinned + phase) mod period) / period`; `Unpin` restores the day-driven wave) | Not Started | sonnet | Pure. The pin functions here are the definition-level fields; the runtime mutators land in task 9 |
| 6 | RED then GREEN: KEYFRAME wave and the default ice-age curve | AC 8 (values at points exact; midway is the average; the curve wraps at 1; a slow-rise fast-fall curve is asymmetric), 9 (`(0,0), (0.6,1), (1,0)`: wave 0 at `u = 0`, 0.5 at 0.3, 1 at 0.6, 0.5 at 0.8, 0 at 1; with amplitude -0.12 the offset at `u = 0.6` is -0.12 at weight 1) | Not Started | sonnet | Pure. Known answers by hand. Commit tasks 5 and 6 together |
| 7 | RED then GREEN: `ClimateCycles::Initialise` and the `GLOBAL`, `LATITUDE`, `REGIONS` patterns | AC 12 (first three bullets plus the region-absent rule): `GLOBAL` weight 1 everywhere and stores no array; `LATITUDE` weight `weight_at_equator` (0.6) at latitude 0, `weight_at_pole` (1.0) at 1, linear between; `REGIONS` weight on listed regions and 0 elsewhere; a region id absent from the world makes `Initialise` return false and change nothing | Not Started | sonnet | Hand-built terrain. A `REGIONS` cycle with no `regionOfCell` array supplied also returns false (decision recorded in the test name) |
| 8 | RED then GREEN: `COAST_INLAND` pattern using the grid-search distance | AC 12 (coast bullet): weight `coast` on a coastal cell, `inland` at distance 6 or more, linear between, with a hand-built row of known distances; a cell the search leaves `kUnreached` gets `inland` | Not Started | sonnet | **Blocked until `MultiSourceSteps` and `HexNeighbours` from `grid-search-helpers.plan.md`.** Do not write a breadth-first search |
| 9 | RED then GREEN: `Evaluate`, summing, clamps, disable, mutators | AC 13 (two cycles sum per field in file order), 14 (the three totals never leave their clamps with 16 maximum-amplitude cycles), 15 (a disabled cycle gives bit-identical output to the cycle being absent from the file), 16 (pin and `Unpin` at `Evaluate` level), 17 (first half: with no enabled cycle `active` is false and the arrays are untouched), plus `DisableAll`, `SetEnabled`, `Pin`, `Unpin` on known and unknown ids | Not Started | sonnet | Pure. `Evaluate` allocates nothing (output sized by the caller). These mutators are what the preset plan's cycle-reset task calls (AC 32 and 33 are proved there). Commit tasks 7 to 9 |
| 10 | RED then GREEN: start modes, precedence, config parse | AC 18 (warm: saved `phase_days` is the file's for every cycle and day 0 gives wave 1 for `oscillation_4y` and wave 0 for `ice_age_40y`), 19 (fraction: `round(f * period)` per id, an unknown id warns and is ignored), 20 (seeded: the same seed and id always give the same `phase_days`; different seeds differ for at least one cycle across a set of 8 seeds; no RNG state touched), 21 (precedence: preset beats config beats default), `ParseClimateConfig` valid and each invalid case names the key | Not Started | sonnet | Seeded hash is the fixed FNV-1a described in Implementation Patterns, so the golden `phase_days` can be written as literals in the test. An absent `climate` block is not an error |
| 11 | RED then GREEN: `Coldness` | AC 30: 0 on the default warm start at day 0; 1 for a cell when every cycle is pinned at its coldest wave; always in [0, 1] over a sweep of days and cells; 0 when `Tmax == Tmin` (a single zero-amplitude cycle); deterministic (two calls equal) | Not Started | sonnet | Hand-compute the default-start cases. A negative amplitude swaps which wave extreme is cold, so include a case with a negative-amplitude KEYFRAME and a positive-amplitude SINE together |
| 12 | RED then GREEN: `SetClimate`, `LatchClimate`, committed offset, capture isolation, no-cycle regression | AC 17 (second half: **104 weeks bit-identical** between a simulation with no climate set and one with an empty or all-disabled climate), 26 (after `LatchClimate`, poisoning the `ClimateCycles` object does not change `Run`), 39 (the committed offset accessor equals the latched `tempOffset` of the committed week and is all zero with no cycle; the layer registration itself is `weather-debug.plan.md`'s), plus the latch rule: a week with cycles disabled after a week with cycles on carries no offset | Not Started | sonnet | **Blocked until the lifecycle and the determinism harness from `ca-pipeline.plan.md`.** Write the AC 17 test **first**, against the unmodified pipeline, so it protects everything already built. Edits the pipeline's `WeatherSimulation` in place; run `WeatherLifecycle*` in the same filter. Ask before escalating |
| 13 | RED then GREEN: temperature and evaporation entry points | AC 22 (a uniform `tempOffset` of +0.05 on a no-wind, no-moisture world raises the pass 1 target by 0.05 before the clamp), 23 (`evapMultiplier = 2` doubles primary ocean evaporation unless saturated; secondary evaporation unchanged) | Not Started | sonnet | **Blocked until the solar and evaporation passes from `ca-pipeline.plan.md`.** Those tasks add the zero-default reads; this task proves the non-zero values act. If a read is missing, fix it in the pass and say so in the notes |
| 14 | RED then GREEN: wind shift entry point | AC 24: with `windShift = +0.1` the latitude band boundary moves poleward by 0.1 including the transition lerp, **relative to the westerly-band latitudes supplied by S-1** (a test with non-default band latitudes moves the bands with them; S-1's shipped values reproduce the old output bit for bit); with shift 0 the directions equal the existing function exactly; the column-parity tie rule is unchanged | Not Started | sonnet | **Blocked until `PrevailingDirection` taking an effective latitude and the latched westerly band from `ca-pipeline.plan.md`.** Effective latitude is `clamp(latitude - windShift, 0, 1)`. Commit tasks 12 to 14 |
| 15 | RED then GREEN: range independence with both default cycles on | AC 25: all fields bit-identical at 1, 3, 7 and 17 ranges and with ranges in reverse order over 104 weeks | Not Started | sonnet | **Blocked until the test executors and the determinism matrix from `ca-pipeline.plan.md`.** Use those executors, never a second set. Run the 104-week cases once at the end of the task. If a pair differs, stop: it is an ordering bug |
| 16 | RED then GREEN: deferred versus synchronous with cycles on | AC 27: a `WeatherStage` driven through the manual executor matches `WeatherRunner` bit for bit over 104 weeks with cycles on | Not Started | sonnet | **Blocked until the module-owned `ClimateCycles` from `weather-module.plan.md` and the deferred-vs-synchronous adaptor from `ca-pipeline.plan.md`.** If either is unbuilt, leave Blocked with `dia docs plan` and continue |
| 17 | RED then GREEN: `WeatherRunner` frozen-phase spin-up, `StartColdness` and `StartSnowSurvivesSummer` | AC 28 (spin-up offsets are identical for all 52 weeks and equal `Evaluate(freezeDay)`), 29 (the offsets of the last spin-up week equal those of the first live week for warm, fraction and seeded starts), 31 (spin-up length stays `spinup_weeks` with cycles on or off; nothing extends it), 31a (`StartColdness` and `StartSnowSurvivesSummer` are the only values handed to A5), 31b (the mask: 0 on a warm temperate start, 1 on a cold high cell below `snow.melt_threshold` with the ice age pinned, 0 just above, unchanged across spin-up) | Not Started | sonnet | **Blocked until `WeatherRunner` is built (Task 0 finds its owner) and the spin-up job from `weather-module.plan.md`.** The module-level assertions (that plan's AC 37, 38) belong there. `snow.melt_threshold` is read from the snapshotted parameters |
| 18 | Author `climate_cycles.json` and `climate_cycles.schema.json` with the two default cycles; a test loads the shipped files through the catalogue and `PathStore` and compares to the GDD literals; add the `climate` config block to the WeatherModule instance when its manifest row lands | AC 1 (second half: the shipped file equals the GDD values), 5, 18 against the real file; the default start is warm | Not Started | sonnet | The tests are the drift guard. Register the assets the usual way. **The `.diaapp` `climate` block is blocked until the hosting-stage manifest task in `weather-module.plan.md`** (no hosting instance exists yet); leave a one-line note on this row rather than editing a manifest with no WeatherModule |
| 19 | RED then GREEN: climate save codec over the kit codec | AC 34 (codec half: `enabled`, `phase_days`, `pinned_phase_days` round trip through real JSON text with a seeded start and a pin both set, and a `cycles_hash` above 2^53 survives), 35 (a `cycles_hash` mismatch reports and keeps the saved phases for matching ids; a saved id missing from the file is dropped and reported; a file cycle missing from the save keeps the start-spec phase; a save with no `climate` block loads with no active cycles and no warning; `phase_days >= period` is reduced modulo with one reported warning), `Validate` rejects each listed defect and mutates nothing; no `version` key | Not Started | sonnet | Pure; needs only Tasks 4 and 10. **The end-to-end halves are `weather-save.plan.md`'s** (continued run bit-identical, and a save with a step in flight). Do not repeat them here |
| 20 | RED then GREEN: cycle queue entries | AC 38: the `CycleChange` entries (enable, disable, pin, unpin, start) and the cycles reload target apply at the next `Capture` and never mid-`Run`; an unknown id returns false and queues nothing; a failed cycle-file reload keeps the previous set and logs Failing; a `cycle` action reaches these entries through the one `cow.weather.params` name (the registered name count is `weather-debug.plan.md`'s) | Not Started | sonnet | **Blocked until the stage and its `ApplyEntry` hook from `weather-module.plan.md`.** The `params` registration and the `cycle` handler are `weather-debug.plan.md`'s; this task supplies the entries they enqueue |
| 21 | RED then GREEN: cycle report for `info what:cycles` | AC 37: per cycle `u` and `wave` equal `PhaseAt` and `WaveAt`; with `day: D` they equal the values at `D` whatever the current day; the report carries enabled, pinned, `periodDays`, the global-weight contributions, totals and the chosen cell's coldness | Not Started | sonnet | Pure; no blocker. The handler and the panel card rows are `weather-debug.plan.md`'s. If task 0 shows DiaCellHistory has no scalar series, record that the per-cycle wave history is **not** sampled into it (spec marks it uncertain) and flag any Dia change before doing it |
| 22 | RED then GREEN: gauges, logs, health for cycles | AC 40: at most one gauge per cycle named `cow.weather.cycle.<id>.wave` registered with `MetricInfo` and written once per `Capture` on SimPU; a bad file gives health Failing and no active cycles (see open item 10); a missing file gives no cycles and one Info log; Info on load, reload and runtime change; Warning on a clamp, an unknown preset cycle id or a hash mismatch; Error on a bad file | Not Started | sonnet | **Blocked until the module-owned `ClimateCycles` and the health causes from `weather-module.plan.md`.** Gauges are unregistered and re-registered on reload so a removed cycle leaves no stale gauge. `DIA_TRACE_ZONE` on `LatchClimate` |
| 22a | RED then GREEN: a bad cycle catalogue fails loudly (R1), superseding "Degraded" | AC 41: a malformed file at load or reload gives health Failing naming file, record and field; no active cycles on a first load; a failed reload keeps the previous set; a missing file gives no cycles and one Info | Not Started | sonnet | Added 2026-11-18 (plan-gap resolution). Replaces the Degraded wording in Task 22's AC 40 assertion; edit that assertion to Failing in the same commit |
| 23 | Exhaustive test pass: gap audit of AC 1 to 40 (including 31a) against test names, then edge cases | Gap list in notes. Edge cases: a cycle with `period_days` 7 and 36,400; `phase_days` at `period - 1`; `pinned_phase_days` at or past the period; keyframe `u` values 1 ULP apart; 16 cycles all `REGIONS` with an empty id list; a 1-cell world; every cycle disabled then re-enabled between steps; two cycles with equal ids across a reload; day values near 2^32 and 2^63 (integer wrap behaviour recorded); `fraction` 0 and 1; a seeded start for a 7-day period; `Coldness` with every cycle disabled | Not Started | sonnet | No mutation check. Add only tests the audit shows missing. Production bugs found go to a separate fix task |
| 24 | Observation Opportunity Scan over every file touched across logs, traces, profiling, metrics, health | Suggestions in notes | Not Started | haiku | Non-blocking. Expected finds: `Evaluate` duration, a cycles-active gauge, reload outcome counter |
| 25 | Scoped verification: `dia run googletest --filter="ClimateCycles*" --shards 2`; `dia check deps`; `dia docs precommit` (includes `dia validate schema` on the catalogue) | Pass/fail and failing names only; AC 1 to 40 listed with the task that proved each (AC 32 and 33 cited from the presets plan), blocked ACs named | Not Started | haiku | **No full-suite run here** (it is the Weather-stack close-out in worldstep.plan.md). ACs still Blocked at this point are reported as Blocked, not failed |
| 26 | Close-out: `dia docs plan` rows to Done with notes, then `dia docs spec-done docs/specs/applications/cow/systems/a4-weather/climate-cycles.md` | Spec status Done, plan header updated | Not Started | haiku | **Do not run until every row above is Done.** Not to be run when this plan is written. The spec's Status says approval waits on Features 2 and 3 being built and on A5 agreeing the hand-off: confirm both before closing |

**AC coverage (old plan 27 rows, 0-26; now 28 rows with 22a).** AC 41 (added 2026-11-18): Task 22a. AC 1: Tasks 2, 18. AC 2, 3: Task 3 (sidecar coverage plus A4's keyframe and cap rules). AC 4, 6: Task 4. AC 5: Tasks 2, 18. AC 7, 10, 11, 16 (wave level): Task 5. AC 8, 9: Task 6. AC 12: Tasks 7, 8. AC 13 to 16, 17 (first half): Task 9. AC 17 (second half), 26, 39: Task 12. AC 18 to 21: Task 10. AC 22, 23: Task 13. AC 24: Task 14. AC 25: Task 15. AC 27: Task 16. AC 28, 29, 31, 31a, 31b: Task 17. AC 30: Task 11. AC 32, 33: `presets-and-regression.plan.md`. AC 34, 35: Task 19 (36 end to end: `weather-save.plan.md`). AC 37: Task 21. AC 38: Task 20. AC 40: Task 22. Deleted: `LoadClimateCycles`' hand-written rejection table, `ApplyCycleOverrides`, the `ParamError` reuse.

## Open items

1. **Shape of a cycle's parameter effect.** The amendment and resolution 1 say a cycle that changes a scalar parameter does so with a DiaAttribute modifier, but no spec gives the record field (`parameters: [{key, op, amplitude}]` is this plan's assumption) or which parameters a cycle may target. None of the two default cycles needs one. If out of scope, drop the `parameters` field and its test case.
2. **Climate latch reset.** The spec says `Capture` calls `LatchClimate`; the module's hook calls it as a separate step after `sim.Capture`. For that order to be safe `Capture` must reset the climate latch, otherwise a disabled cycle would reuse last week's offsets. Task 12 adds the reset and a test; confirm with the pipeline plan if already built.
3. ~~A5's seed hand-off.~~ **Resolved 2026-11-17 (user):** `SpinUpStart` is two per-cell arrays; Task 17 builds `StartColdness` and `StartSnowSurvivesSummer`.
4. **Cycle entries versus the kit's queue.** The stage's `WeatherEntry` and the `params` helper's system-action hook are what the `cycle` action reaches; the kit spec says only that system actions are "passed to a system handler registered with the helper". Whether a system action can enqueue a typed entry is assumed.
5. **`WeatherRunner` has no owner plan in A4.** The module plan delegates to it and the spec names S-2.5 Feature 3. Task 17 needs it built; Task 0 records where it lives.
6. **Trigonometric exactness.** `cos(2 * pi * 0.25)` is not exactly 0 in `double`; AC 7 uses a `1e-12` tolerance at 0.25 and 0.75 and is exact at 0 and 0.5. AC 10 stays bit-exact (integer arithmetic).
7. **Per-configuration drift.** Wave maths uses `double` with one formula (PD-005); if Debug and Release differ, record it against the pipeline's `/fp:precise` finding rather than widening a tolerance.
8. **`cycles_hash` as a JSON number.** A 64-bit hash exceeds 2^53; Task 19 stores it as `UInt64` and proves the text round trip; the fallback is a 16-digit hex string (which changes the save example). Confirm if the fallback is taken.
9. **Spec Open Design Questions stay open by design:** CA damping of amplitudes, whether `seeded` starts should avoid the coldest phase, and linear keyframe corners.
10. **Resolved 2026-11-18:** a bad cycle catalogue fails loudly (R1): health Failing; the spec's "Degraded" is superseded (AC 41, Task 22a). **Bad cycle file: Degraded or Failing.** The spec says a bad file is "Degraded and no active cycles" (cycles are optional); the amendment says every sidecar or record fault is strict, loud, health Failing (R1). This plan follows R1; the spec should say whether an optional file differs.
11. **The record sidecar and `keyframes`.** A keyframe point list nested in a record is an array of `[u, value]` pairs; whether the DiaValueSchema record entries can express a pair array is not shown in the foundation specs, so the keyframe shape rules are A4's (Task 3).
12. **Westerly-band source.** Task 14 needs S-1's band latitude parameters by name; neither spec names them. Task 0 records it.
13. **Counts.** The old plan's command count stays "8" is no longer asserted here: `weather-debug.plan.md` owns the registered-name count, which it recounts at build.
