# Plan: A4 Weather Influence API (A4 Weather Feature 7)

**Spec:** @docs/specs/applications/cow/systems/a4-weather/weather-influence.md
**Status:** Not Started
**System plan:** @docs/specs/applications/cow/systems/a4-weather/a4-weather.plan.md (row 6a)

**Starts after A4 Feature 1 (`weather-data.plan.md`) tasks 1 to 14 are Done** (types, the 41-row parameter table with
the `influence` block, `WeatherTerrain`). Tasks 2 to 8 and 14 to 15 are pure logic and need nothing else. Tasks 9 to 13
need pieces of Feature 2 (`ca-pipeline.plan.md`) and are marked "Blocked until ca-pipeline task N". Tasks 16 to 20 need
Features 3 and 5 pieces and are marked the same way. **Nothing in this plan is built today** (Features 1 to 6 are all
unbuilt); every Blocked mark is a start condition, not a defect.

**Division of work with the sibling plans (no duplication).** Feature 2's plan already owns: the ten-array
`WeatherFields` with `pressureAnomaly`, `WeatherHandoff::influenceWater` (type only, `weather-data` task 2), pass 2's
anomaly term (ca-pipeline 7), pass 5a/5b anomaly transport (10), pass 5c decay and `kPassCount` 14 (10a), `Capture`
taking an `InfluenceSnapshot` (5), the pass 0 profile entry and scratch arrays (4, 22), and forcing hooks A to D (17).
This plan owns: `InfluenceBook`, the weight and falloff maths, the **body** of pass 0, the temperature-after-pass-1 and
moisture-after-pass-4 delta application, the codec, the ground-water fill, the module-facing API tests and the
`cow.weather.influence` handler. Wiring that other plans already list is referenced, not re-planned:
`weather-module.plan.md` 10a (`IWaterInputSource`, `CopyGroundWaterInfluence` entry point), 10b (module owns the book,
`Latch` in `Capture`, `Age` in `Commit`, clear on new revision), `weather-save.plan.md` 17a (influences block in the
save record), `presets-and-regression.plan.md` 10a (`ApplyPreset` clears the book), `weather-debug.plan.md` 13 and 14
(name registration, `force storm` routing).

**Order against the sibling plans:** this plan's tasks 2 to 8 and 14 can run before `weather-module.plan.md` 10b.
10b needs this plan's tasks 2 to 5 (the book) built, and `weather-smoothing.plan.md` task 7 (the preview interface
header) before this plan's task 18.

All code is CoW (application) under `Cluiche/CoW/Shared/Weather/` and `Cluiche/CoW/Modules/`. No Dia files are added or
edited (AD-001). Hex distance already exists as `Dia::Pathfinding::HexTopology::Distance(CellCoord, CellCoord)`
(static, cube coordinates, flat-top odd-q), so no A4 distance helper is written. Where a task could need more than
sonnet it says "ask before escalating" (feedback_confirm_opus).

## Implementation Patterns

### Files (Task 1)
- `Shared/Weather/WeatherInfluence.h/.cpp`: enums, `InfluenceDesc`, `InfluenceRegion`, `InfluenceInfo`, `InfluenceRules`,
  `InfluenceSnapshot`, `InfluenceBook`, `InfluenceWeight`, `InfluenceRejectReasonName`.
- `Shared/Weather/WeatherInfluencePass.cpp`: the pure per-cell evaluation function and the pass 0 body.
- `Shared/Weather/WeatherInfluenceCodec.h/.cpp`: book to and from `Json::Value`, `Validate`.
- `Modules/WeatherInfluenceCommands.h/.cpp`: `cow.weather.influence` handler over `WeatherModule`'s API.
- `Shared/Weather/Testing/InfluenceTestHelpers.h`: builders for descriptors, a small hand-built grid, a fake A5 consumer
  (tests ship with libraries).
- Tests in `Tests/GoogleTests/CoW/Weather/`: `TestWeatherInfluenceBook`, `Eval`, `Timing`, `Determinism`, `Save`,
  `Commands`, `Handoff`. Suite prefix `WeatherInfluence*` for all seven.
- Namespace `CoW::Weather`. No STL in public signatures (PD-004): `DynamicArrayC`, POD records, `StringCRC`.

### InfluenceRules (Task 2)
- `InfluenceRules` is a POD copied from `WeatherParameters` (the `influence` block): `maxStrength`, `maxLifetimeWeeks`,
  `maxActive`, `stormPressureSeed`, `stormMoisture`, `temp`, `moisture`, `maxDeltaPressure`, `maxDeltaMoisture`,
  `maxDeltaTemperature`, `capWater`, `maxRadius`, `maxSetCells`, `groundWaterFactor`, `pressureAnomalyMax`. Built by
  `InfluenceRules RulesFrom(const WeatherParameters&)`. The book never holds a pointer to the parameters. `Add` takes the
  rules by const reference on each call, so a parameter reload changes the next call only (AC 27).

### InfluenceBook (Tasks 3 to 5)
- Storage: fixed-capacity `DynamicArrayC<InfluenceInfo>` sized to the largest legal `max_active` (256), plus one pooled
  `DynamicArrayC<uint32_t>` arena for all cell-set regions (capacity 65,536 = largest legal `max_set_cells`). No
  allocation after construction. Entries stay in ascending id order (ids only grow), so `Latch` copies in id order with
  no sort.
- Ids start at 1 per world, strictly increasing, never reused; `Clear()` resets the counter to 1 only for a new world
  (module decision, Task 17); `Remove` and expiry never touch it. A rejected `Add` changes nothing, **not even the id
  counter and not the revision** (AC 4): validate everything first, mutate last.
- Validation order (first failure wins, so each reason has one trigger): `NotReady`, `BadKind`, `BadStrength`
  (`!isfinite`, `<= 0`, `> maxStrength`), `BadLifetime`, region checks (`EmptyRegion`, `CellOutOfRange`,
  `RadiusTooLarge`, `FalloffOnCellSet`, `RegionStorageFull`), then `ActiveCapReached` last. `NotReady` is passed in by the
  module (`cellCount == 0`), the book itself only sees `cellCount`.
- A cell set is sorted and de-duplicated on `Add`, then `RegionStorageFull` is checked against the **de-duplicated**
  total. Circle `centre >= cellCount` is `CellOutOfRange`.
- `Latch()` marks every live entry `inFlight` and returns an `InfluenceSnapshot`: an array of POD entries (kind, region
  mode, centre, radius, `cellsBegin`, `cellsCount` into a snapshot-owned cell pool, strength, falloff, `firesImpulse`).
  The snapshot owns its copy of cell lists, so a later `Remove` cannot change a latched step (AC 30, 36). Snapshot
  storage is sized at construction; `Latch` fills it and does no allocation.
- `Age(commitDay)` increments `weeksApplied` on every `inFlight` entry, stamps `landedDay = commitDay` when it becomes 1,
  clears `inFlight`, then deletes entries with `weeksApplied >= lifetimeWeeks`. `landedDay` is not part of the codec.
- `Revision()` bumps on add, remove, latch, age and clear and on nothing else. A rejected add does not bump it.
- A discarded step is simply a step where `Age` is never called (the module skips it, task 17); the book has no
  "discard" method.

### Weight and falloff (Task 6)
- `float InfluenceWeight(const InfluenceDesc&, uint32_t cell, const HexGridTables&)`. CellSet: binary search, `strength`
  or 0. Circle: `d = HexTopology::Distance(ToCoord(centre), ToCoord(cell))` using the grid's width and height;
  `d > radius` gives 0; `t = d / (radius + 1)` in float; `None: 1`, `Linear: 1 - t`, `Smooth: 1 - t*t*(3 - 2*t)`;
  weight is `strength * f`. Only `+ - *` and one divide by the integer `radius + 1`, no `sinf` or `cosf`.
- Task 0 confirms what `HexGridTables` (Feature 2) exposes. If it has no `width`/`height`, the weight function takes a
  `HexTopology` by const reference instead (the same object the tables were built from). Record the choice in the notes.

### Pass 0 evaluation (Tasks 7 to 9)
- `struct InfluenceCellDelta { float dPressure, dTemperature, dMoisture, waterWeight; }` and a pure
  `InfluenceCellDelta EvaluateInfluenceCell(const InfluenceSnapshot&, uint32_t cell, const InfluenceRules&, const HexGridTables&)`.
  It loops the snapshot in ascending id order and accumulates four float sums, then clamps each:
  `dPressure` to `+/-maxDeltaPressure`, `dTemperature` to `+/-maxDeltaTemperature`, `dMoisture` to
  `+/-maxDeltaMoisture`, `waterWeight` to `+/-capWater`. A storm adds `w * stormPressureSeed` and `w * stormMoisture`
  only when `firesImpulse`. Warmer `+w*temp`, Cooler `-w*temp`, Wetter `+w*moisture` and `+w` to water, Drier the
  reverse. The float sum order is fixed by the snapshot order, so the result does not depend on range count.
- The per-cell function is what the unit tests call. The pass 0 body (Task 9) is one `executor.ParallelFor(N,
  kMinRange, fn)` that calls it, writes the four results to Feature 2's pass 0 scratch arrays (allocated in
  ca-pipeline task 4; this plan never allocates), then in the same range adds `dPressure` to `pressureAnomaly` and clamps
  to `+/-pressureAnomalyMax`, and writes `waterWeight` into `WeatherHandoff::influenceWater`. A pass wraps itself in
  `DIA_PROFILE_SCOPE` named `0`. An empty snapshot skips the pass and touches nothing (AC 24).
- Cells outside every influence do no loop work beyond the snapshot scan; a bounding box per entry is **not** added
  (premature; the perf row is Feature 2's task 22 and this feature reports its pass 0 time there).

### Delta hooks (Task 10)
- `dTemperature` is added right after pass 1, clamped to [0, 1]. `dMoisture` is added right after pass 4, clamped to
  [0, 1], at the same point as forcing hook C. **Hold wins:** hook C's `MoistureHold` writes after the influence add, so
  a held cell ends at the held value. A `PressureHold` (hook A, after pass 2) already overrides the anomaly's effect on
  that cell, so no extra rule is needed for pressure.
- Pass 0 runs first in `Run`, before pass 1, so the anomaly is already in `pressureAnomaly` when pass 2 reads it.

### Storm and sustained kinds (Tasks 8, 10, 11)
- A storm acts only in the step where `firesImpulse` is true (the entry's first latch). It stays in the book for its
  lifetime, counts against `max_active` and seeds nothing after that. What it started travels as moisture and as
  `pressureAnomaly` through Feature 2's passes 5a/5b and decays in 5c. This feature does not re-test the decay
  arithmetic (ca-pipeline task 10a owns it); it tests that a **storm influence** produces a low that outlasts one week.

### A5 hand-off (Tasks 15, 16)
- `bool FillGroundWaterInfluence(const WeatherHandoff&, float groundWaterFactor, bool anyActive, DynamicArrayC<float>& out)`
  is a pure function: `out[c] = influenceWater[c] * groundWaterFactor`, returns false (and leaves `out` unchanged) when
  `anyActive` is false. `WeatherModule::CopyGroundWaterInfluence` (built in weather-module 10a) calls it. `anyActive` is
  "the captured snapshot was non-empty".
- `influenceWater` is computed with or without A5 and is never saved (`WeatherHandoff` is transient).

### Save codec (Task 14)
- `EncodeInfluences(const InfluenceBook&, Json::Value& out)` and
  `bool DecodeInfluences(const Json::Value&, uint32_t cellCount, const InfluenceRules&, InfluenceBookState& out, CodecError&)`,
  `InfluenceBook::Restore(const InfluenceBookState&)`. Decode is all-or-nothing into a scratch state; `Restore` copies
  after success. `Validate` is the decode with no restore. Rules are exactly the list in the spec's Save and load
  section. `CodecError` is Feature 4's small struct (key, reason); if it does not exist yet when this task runs, use a
  local struct with the same two fields and switch when weather-save task 4 lands (note in the row).
- `in_flight` entries restore as `inFlight` with `landedDay = kNotLanded`; an entry with `weeks_applied > 0` has no
  `landedDay` after load (Feature 9 AC 29).
- The `influences` object sits inside the weather record. Feature 4 plan row 17a wires it into the participant and owns
  the end-to-end numbers (AC 39, 40, 42 at field level). This plan proves the book half.

### Module API and commands (Tasks 17 to 20)
- `WeatherModule::AddInfluence/RemoveInfluence/ListInfluences/GetInfluenceRevision` are thin forwarders over the book
  with a SimPU assert. The book wiring itself (`Latch` in `Capture`, `Age` in `Commit`, clear on a new revision) is
  weather-module 10b; this plan's task 17 adds the feature-level timing tests on top of it.
- `GetInfluencePreviewSource()` returns a small pure adaptor, `InfluenceBookPreviewSource` (in `WeatherInfluence.h`),
  that implements Feature 9's `IInfluencePreviewSource` over `const InfluenceBook&` and the grid. The spec says the module
  implements the interface; this plan has the module own and return the adaptor so AC 51a is testable without a module
  (see open item 3).
- `cow.weather.influence` handler: parse once into a typed `InfluenceCommand` (`add`, `remove`, `list`, `clear`), then
  call the same `WeatherModule` methods. No handler logic duplicates the API (AC 51). Errors name the field and queue
  nothing. The default `source` is `"debug"`, default falloff comes from `default_falloff`.
- Metrics `cow.weather.influences_active` (gauge) and `cow.weather.influences_rejected` (counter) are registered with
  `MetricInfo` and written once per applied step on SimPU. The reject counter increments inside the module's
  `AddInfluence` wrapper, not inside the pure book. Logs on `WTH`; `DIA_TRACE_ZONE` on `AddInfluence`, the latch and the
  pass 0 body.

### Testing
- Pure tests on a hand-built terrain from `Testing/InfluenceTestHelpers.h`; pipeline tests use Feature 6's
  `MakeCoastRange` if built, else a local copy of the same shape (record in notes).
- Float equality only where the maths is exact (+, -, * with one divide); otherwise `EXPECT_FLOAT_EQ`. Determinism
  compares with `memcmp` after 52 weeks.
- Range-count tests use Feature 2's `ReverseRangeExecutor` and `FixedRangeExecutor(n)` (ca-pipeline task 2), `n` = 1, 3,
  17 and `SerialExecutor`. Never write a second set.
- AC 38 uses Feature 2's mechanism (amendment 5): record the data pointer and capacity of every pass 0 scratch array and
  of the snapshot storage before and after `Run`; no global `operator new` hook.

### Verification cadence
- Per task: `dia run googletest --filter="WeatherInfluence*" --shards 2` (add `WeatherLifecycle*` or `WeatherPass*` for
  tasks that edit Feature 2's files, to prove no behaviour change). The 52-week matrix runs once at the end of its own
  task, not per edit.
- **No full-suite run in this plan** (it happens once, in `a4-weather.plan.md` row 7). No mutation checks. No sanitizer.
- One commit per task, with the plan row updated in the same commit through `dia docs plan`.

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 0 | Prerequisite check, read only: record the build state of A4 Features 1 to 6 (all unbuilt at planning time) and which tasks therefore wait. Confirm these hooks exist or are planned: `WeatherFields::pressureAnomaly` and `WeatherHandoff::influenceWater` (weather-data 2), the `influence` rows in the parameter table (weather-data 3), pass 0 scratch and `kPassCount` 14 (ca-pipeline 4, 10a), `Capture` taking an `InfluenceSnapshot` (ca-pipeline 5), hooks A to D (ca-pipeline 17), `IWaterInputSource` (weather-module 10a), module book wiring (weather-module 10b), `influences` save block (weather-save 17a), `ApplyPreset` book clear (presets 10a), command name and `force storm` rows (weather-debug 13, 14). Confirm `HexTopology::Distance(CellCoord, CellCoord)` (found at planning time in `Dia/DiaPathfinding/HexTopology.h`) and `ToCoord`; check what `HexGridTables` exposes (width, height or a topology) | Findings recorded in notes | Not Started | haiku | Mechanical, TDD-exempt. If `HexTopology::Distance` is missing when built, stop and flag it (AD-001 ask first); do not add a Dia edit. Record the `HexGridTables` answer for Task 6 |
| 1 | Create empty `WeatherInfluence.h/.cpp`, `WeatherInfluencePass.cpp`, `WeatherInfluenceCodec.h/.cpp`, `Modules/WeatherInfluenceCommands.h/.cpp`, `Testing/InfluenceTestHelpers.h` and the seven test files; register each with `dia docs vcxproj-add CoW ...` and `dia docs vcxproj-add GoogleTests ...` (literal paths, one call per file, PowerShell) | Solution builds with the empty files | Not Started | haiku | Mechanical, TDD-exempt. Idempotent. Later tasks edit in place and do not touch the vcxproj again |
| 2 | RED then GREEN: enums, `InfluenceDesc`/`InfluenceRegion`/`InfluenceInfo`/`InfluenceResult::Ok()`, `InfluenceRules` and `RulesFrom(WeatherParameters)`, `InfluenceRejectReasonName` | AC 3 (name half): every reason has a distinct non-empty name; `RulesFrom(DefaultWeatherParameters())` equals the GDD section 10a values written as literals in the test (1.0, 13, 64, -0.3, 0.2, 0.1, 0.15, 0.3, 0.3, 0.15, 1.0, 32, 8192, 0.05, 0.5) | Not Started | sonnet | Needs Feature 1 tasks 3 and 13 Done. Literals in the test, not read from the table, so it cannot pass vacuously |
| 3 | RED then GREEN: `InfluenceBook::Add` validation, ids, sorted storage | AC 1, 2, 3, 4, 5: valid add returns id 1 then 2, listed `Pending` with `weeksRemaining == lifetimeWeeks`; each rejection reason fires on its own trigger (unknown kind, strength 0, negative, NaN, infinity, over max, lifetime 0 and over max, empty set, member index and centre `>= cellCount`, radius over max, falloff on a cell set, set past `max_set_cells`, `NotReady`); a rejected call leaves list, id counter, revision unchanged; a set `{9, 3, 3, 5}` is stored `{3, 5, 9}` and a repeated cell does not double its weight (weight check lands in Task 6, here the stored list only) | Not Started | sonnet | Pure, no grid yet. Validate everything before mutating. `ActiveCapReached` is tested in Task 4 |
| 4 | RED then GREEN: `Remove`, `RemoveBySource`, `List`, active cap, lowered `max_active`, `Clear`, `Revision` | AC 6, 25, 26, 27, 27a, 33 (add, remove, clear half): unknown or already-removed id returns false and changes nothing; the 65th add returns `ActiveCapReached` and nothing is dropped; remove frees a slot at once; lowering `max_active` below the count keeps entries and rejects new ones until the count falls; a storm counts toward the cap for its whole lifetime; revision changes on add, remove and clear only, never on a rejected add | Not Started | sonnet | Expiry freeing a slot (AC 26 second half) is proved in Task 5 once `Age` exists. Commit tasks 2 to 4 together after the scoped filter passes |
| 5 | RED then GREEN: `Latch`, `Age`, states, `landedDay`, expiry | AC 20 (book half), 28 (book half), 29 (book half), 30 (book half), 31 (book half), 33 (latch and age half), 51a groundwork: an added entry is `Pending` until `Latch`, then `InFlight`, then `Active` after `Age`; the snapshot is a copy (removing an entry or adding one after `Latch` does not change it, cell lists included); `firesImpulse` is true only while `weeksApplied == 0`; an entry lives exactly `lifetimeWeeks` latch-and-age cycles; `landedDay` is `kNotLanded` until the first `Age`, then equals its `commitDay`; `Latch` with no `Age` (a discarded step) leaves `weeksApplied` at 0; expiry frees a slot | Not Started | sonnet | The module-level versions (a real stage driver) are Task 17. Poisoning the snapshot after `Latch` is Task 12 |
| 6 | RED then GREEN: `InfluenceWeight` and falloff | AC 5 (weight half), 7, 8, 9, 10, 11: a Circle touches exactly the cells within `radius` hex steps on odd and even columns and nothing farther; `None` gives `strength` everywhere in range; `Linear` and `Smooth` give `strength` at the centre, decrease monotonically, and stay above 0 at distance `radius`; radius 0 is only the centre; a circle over the grid edge touches only in-grid cells with no wrap; a CellSet gives `strength` on members only; `BookWeightAt` equals `InfluenceWeight` | Not Started | sonnet | Known answers computed by hand (a 7 x 7 grid, distances listed in the test), not by calling `HexTopology::Distance`. Uses `HexTopology::Distance` in production code only. Record the Task 0 `HexGridTables` decision |
| 7 | RED then GREEN: `EvaluateInfluenceCell` for the sustained kinds, overlap sum and ordering | AC 16 (value half), 17 (value half), 21, 22: Warmer `+temp * w`, Cooler `-temp * w`, Wetter `+moisture * w`, Drier `-moisture * w`; two same-kind influences sum per cell; warmer and cooler on one cell sum with opposite signs, likewise wetter and drier; the same pair added in either order gives the same result because evaluation is ascending id | Not Started | sonnet | Pure, hand-built snapshot (no book needed). Float sum order is the snapshot order |
| 8 | RED then GREEN: storm impulse values, per-field caps, `waterWeight` | AC 12 (value half), 13 (value half), 23, 44 (value half): a `firesImpulse` storm at full strength at the centre gives `dPressure -0.3` and `dMoisture 0.2 * weight`; the same storm with `firesImpulse` false gives 0; 64 stacked influences on one cell never exceed `0.15`, `0.3`, `0.3` or `capWater` in absolute value; `waterWeight` is positive for wetter only, negative for drier only, the signed sum where they overlap, capped at 1.0 | Not Started | sonnet | Caps apply to the **sum**, not to each entry. Commit tasks 5 to 8 after the scoped filter passes |
| 9 | RED then GREEN: pass 0 body wired into `Run`: parallel pass, skip when empty, anomaly add and clamp, `influenceWater` write | AC 12 (pipeline half: a storm seeds `-0.3` into `pressureAnomaly` at the centre after one `Step`), 14b (anomaly clamped to +/-0.5; zero on a world that never had a storm), 18 (third part: the seeded low is in pass 2's pressure and pass 3's wind sees it), 24 (empty snapshot: fields, totals and profile bit-identical to a run without this feature), 38 (no heap allocation in pass 0: pointer and capacity of every scratch array and the snapshot unchanged across `Run`) | Not Started | sonnet | **Blocked until ca-pipeline task 4 (pass 0 scratch), 5 (`Capture` takes the snapshot) and 10a (pass 5c, `kPassCount` 14).** Pass 0's profile slot already exists there; this task fills its body. Ask before escalating |
| 10 | RED then GREEN: temperature delta after pass 1, moisture delta after pass 4, clamps, hold wins | AC 15, 16 (pipeline half), 17 (pipeline half), 18 (first two parts: a temperature delta is not erased by pass 1 relaxation, a moisture delta is not erased before transport), 19 (a cooler on a 0.0 cell leaves 0.0, a wetter on a saturated cell stays 1.0), 20 (final proof: an influence acts in each of its `lifetimeWeeks` applied steps and in no later one, driven by a test loop calling `Latch`, `Step`, `Age`) | Not Started | sonnet | **Blocked until ca-pipeline task 6 (pass 1), 9 (pass 4) and 17 (hooks A to D, so hold precedence can be shown).** Edits ca-pipeline files in place; run `WeatherPass*` in the same filter to show no behaviour change |
| 11 | RED then GREEN: storm travels and persists as a low | AC 14, 14a: after the first step both the added moisture and the anomaly move downwind (uniform terrain, steady wind, the anomaly centre shifts at the moisture transport rate); the storm region is not re-seeded in step two; the planted storm keeps a cell below the 0.3 `find storms` threshold for at least two weeks at the GDD defaults; pass 2's pressure equals `0.5 + pressure_change + anomaly`, clamped | Not Started | sonnet | **Blocked until ca-pipeline task 10 (5a/5b anomaly transport), 10a (5c decay) and 12.** Uses a storm influence, never a direct field write. Does not re-test the decay arithmetic (ca-pipeline 10a AC 12b) |
| 12 | RED then GREEN: determinism and capture isolation | AC 34 (fields, totals and `influenceWater` bit-identical at `SerialExecutor`, 1, 3 and 17 ranges and reversed ranges over 52 weeks with all five kinds, circle and cell-set regions, overlaps), 36 (poison the book, the descriptors and the cell-set source after `Capture`; `Run` unchanged), 37 (no RNG: same call sequence gives the same ids and fields twice) | Not Started | sonnet | **Blocked until ca-pipeline task 2 (test executors), 19 (purity) and 20 (determinism matrix).** Run the 52-week cases once at the end of the task. If a pair differs, stop: it is an ordering bug, not a tolerance to widen |
| 13 | RED then GREEN: deferred versus synchronous | AC 35: a `Capture` at key N and `Apply` at N+k run through DiaDeferredStep's manual launcher equals a synchronous `Step` run bit for bit over 52 weeks, including influences added between kicks | Not Started | sonnet | **Blocked until ca-pipeline task 21, weather-module task 10b and 12.** If the manual launcher is not built, leave Blocked with `dia docs plan` and continue; this is the one deferred-oracle test for the feature |
| 14 | RED then GREEN: save codec, `Validate`, restore | AC 41 (every listed defect rejected naming the key and mutating nothing: items over `max_active`, an id not strictly ascending or `>= next_id`, unknown kind or falloff, `weeks_applied >= lifetime_weeks`, lifetime or strength out of range, unsorted or duplicated or out-of-range cell list, centre `>= cell_count`, total cells over `max_set_cells`), 43 (`next_id` restored; ids after a load continue the sequence), 39 and 40 (book half: encode, real JSON text, decode and restore is bit-identical for pending, in-flight and active entries; an in-flight entry restores as in flight and `Age` ages it once) | Not Started | sonnet | Pure; needs only Task 5. The field-level N = 1, 13, 52 round trip and AC 42 (dimension mismatch discards the book) belong to weather-save 17a, which calls these functions. Round trip goes through real JSON text, never the in-memory `Json::Value` |
| 15 | RED then GREEN: `FillGroundWaterInfluence` and the sim-level no-A5 behaviour | AC 44a (pure half: output is `influenceWater * ground_water_factor`; false with no active influence and `out` untouched), 46 (with `HydrologyInputs.present == false` the A4 fields equal the with-A5 run's A4 fields; `influenceWater` is still computed; nothing is Degraded) | Not Started | sonnet | Pure part has no blocker. The 46 assertion needs ca-pipeline task 5 (`HydrologyInputs`) and is written last in the task; if still unbuilt, leave that assertion for Task 16 and note it |
| 16 | RED then GREEN: A5 hand-off through the module | AC 44a (module half: `CopyGroundWaterInfluence` called only inside `Capture` on SimPU), 45 (a fake A5 consumer receives positive values as fake `extraWater` and negative values as a fake `draw` contribution, same sign and size, in the same `Capture` phase) | Not Started | sonnet | **Blocked until weather-module task 10a.** The interface and buffers are built there; this task only asserts the values and the sign contract through that interface. Uses the fake consumer from `InfluenceTestHelpers.h` |
| 17 | RED then GREEN: public module API and feature-level timing | AC 28 (added between kicks: `Pending` until the next `Capture`, affects that step, visible after its `Commit` only), 29 (added while a step is in flight: that step unchanged, the next one affected), 30 (removed while in flight: that step unchanged, no later step), 31 (a discarded step ages and applies nothing), 32 (a new world revision, a preset load and a state import with other dimensions clear the book with one Info log each; matching dimensions leave it), 33 (revision changes on add, remove, latch, age, clear and on nothing else), `AddInfluence` asserts SimPU | Not Started | sonnet | **Blocked until weather-module task 5, 6 (stage `Capture`/`Commit`, discard) and 10b (book wiring).** 10b has its own AC 39 test of the wiring; this task asserts the feature behaviours through a stage driver and must not repeat 10b's latch-once assertion. The preset-load and state-import clear are wired by presets 10a and weather-save 11; here they are only observed, and skipped with a note if those rows are unbuilt |
| 18 | RED then GREEN: `InfluenceBookPreviewSource` adaptor and `GetInfluencePreviewSource()` | AC 51a: lists each influence's kind, `addedDay` (as `requestDay`), `landedDay` and per-cell weight; `landedDay` is `kNotLanded` until the `Commit` that first includes the influence and equals that commit's day afterwards; not saved (a codec round trip drops it) | Not Started | sonnet | **Blocked until weather-smoothing task 7** (the `IInfluencePreviewSource` and `InfluencePreviewInfo` header) and Task 5 here. The adaptor builds the sparse weight list once per influence id from `InfluenceWeight`; Feature 9 caches it. See open item 3 |
| 19 | RED then GREEN: metrics, logs, trace zones | AC 52 (`cow.weather.influences_active` gauge and `cow.weather.influences_rejected` counter register with `MetricInfo`, written only on SimPU once per applied step), 53 (each rejection logs one Warning naming the reason and the source; add, remove and expiry log Info; one Info when the book is cleared) | Not Started | sonnet | **Blocked until weather-module task 15 (health and logs) and weather-debug task 10 (schema file).** Adds the two metric entries to `weather_schema.json` through that plan's schema rather than a second file. `DIA_TRACE_ZONE` on `AddInfluence`, the latch and the pass 0 body; `DIA_PROFILE_SCOPE` on pass 0 already exists from Task 9 |
| 20 | RED then GREEN: `cow.weather.influence` handler (`add`, `remove`, `list`, `clear`) | AC 47 (registers with non-empty help of at most 256 characters, takes one name from the budget, registration stays all-or-nothing with the other weather names; the all-or-nothing failure test for N = 1 to 8 is weather-debug 13's), 48 (each action as in the spec table; `add` replies with the id and the "next weekly step" wording or the rejection reason; a bad field returns an error naming it and queues nothing), 49 (`clear` with `source` removes only that source), 51 (a direct API call and the command give identical results) | Not Started | sonnet | **Blocked until weather-debug task 13 (registration) and Task 17.** AC 50 (`force storm` becomes a thin `AddInfluence` call with source `"debug.force"`) is covered by weather-debug task 14 (its AC 13 and 13a); do not duplicate it here, only confirm in the notes that task 14 asserts it. Commit tasks 9 to 20 as their blockers clear (one commit per task) |
| 21 | Exhaustive test pass: gap audit of AC 1 to 53 (including 14a, 14b, 27a, 44a, 51a) against test names, then edge cases | Gap list in notes. Edge cases: 64 stacked influences on one cell; a cell set containing every cell; a circle with radius 0 on a corner; a circle radius 64; strength exactly `max_strength` and `max_strength + 1 ULP`; lifetime 1 and `max_lifetime_weeks`; a storm with lifetime 1; add then remove before any `Latch`; remove during the apply step; `Latch` twice without `Age`; `Age` without `Latch`; ids after 2^31 adds (counter overflow behaviour recorded, not fixed); 1-cell world; influence on a world smaller than its cell indices after a parameter reload | Not Started | sonnet | No mutation check. Add only tests the audit shows missing. Production bugs found go to a separate fix task, not inline |
| 22 | Observation Opportunity Scan over every file touched across logs, traces, profiling, metrics, health | Suggestions in notes | Not Started | haiku | Non-blocking. Expected finds: pass 0 duration metric, a book-size gauge in `info memory` (weather-debug 17 already lists the influence count), per-source reject counts |
| 23 | Scoped verification: `dia run googletest --filter="WeatherInfluence*" --shards 2`; `dia check deps`; `dia docs precommit` | Pass/fail and failing names only; AC 1 to 53 listed with the task that proved each, blocked ACs named | Not Started | haiku | **No full-suite run here** (it is `a4-weather.plan.md` row 7). Report pass/fail only. ACs still Blocked at this point are reported as Blocked, not failed |
| 24 | Close-out: `dia docs plan` rows to Done with notes, then `dia docs spec-done docs/specs/applications/cow/systems/a4-weather/weather-influence.md` | Spec status Done, plan header updated | Not Started | haiku | **Do not run until every row above is Done.** Not to be run when this plan is written |

## Open items

1. **Where the temperature and moisture hooks live.** Feature 2's plan builds hooks A to D (row 17) and passes 1 and 4
   but does not list the influence delta application points. This plan owns them (task 10) and edits Feature 2's files in
   place; it must run after ca-pipeline 17. If the Feature 2 owner prefers to add the two insertion points with a
   no-op body, say so before task 10 starts. Still open (low impact).
2. **Grid object for `InfluenceWeight`.** The spec writes `const HexGridTables&`. Whether that type exposes width and
   height is Feature 2's choice (task 0 here records it); the fallback is passing a `HexTopology`. Still open until
   task 0.
3. **Who implements `IInfluencePreviewSource`.** The spec says the module. This plan has the module return a small pure
   adaptor (`InfluenceBookPreviewSource`) so AC 51a runs without a module, and so Feature 9 can be tested against the
   real book. If the user wants the module to implement the interface directly, task 18 becomes a thin module test.
   Still open.
4. **Preview source needs an influence id.** Feature 9's `Get(index)` result should carry `InfluenceId` so the preview can
   build each sparse weight list once and notice removal. The spec's field list for `InfluencePreviewInfo` omits it.
   Raised in `weather-smoothing.plan.md` open item 5; this plan assumes the id is added.
5. **Shared AC 31 and 32 coverage with weather-module 10b.** 10b's AC 39 test also checks "latched once, aged once, cleared
   on a new revision". Task 17 here asserts the externally visible timing instead. If the two tests end up identical when
   written, keep one and cite it in the other's notes. Still open.
6. **Dragon abilities, ownership rules and quotas** stay out (spec Non-Goals and Open Design Question 3). Nothing in this
   plan enables them beyond the `source` field.
