# Plan: A4 Weather Influence API (A4 Weather Feature 7)

**Spec:** @docs/specs/applications/cow/systems/a4-weather/weather-influence.md
**Status:** Not Started
**System plan:** @docs/specs/applications/cow/systems/a4-weather/a4-weather.plan.md (row 6a)

Rebuilt 2026-11-18 against the world-sim review amendment and the spec-6 resolutions. Two decisions shape it:
- **Kinds are data.** The five hard-coded kinds (storm, warmer, cooler, wetter, drier) become rows of an
  **influence-kind catalogue**: `mode: impulse|sustained`, pressure, moisture and temperature deltas, falloff,
  lifetime and strength cap. The GDD section 10a values are the five shipped rows; a new kind is a data row only.
  The per-kind parameter rows (`storm_pressure_seed`, `storm_moisture`, `temp`, `moisture`) leave the parameter
  sidecar (`weather-data.plan.md` keeps only the generic caps).
- **Influences stay per-cell field forcings** (spec-6 resolution 1). Pass 0 and the temperature and moisture hooks
  are unchanged. DiaAttribute modifiers are used **only** where a kind changes a scalar parameter, applied to the
  `ParameterFile` and removed when the influence expires.

Also deleted: the `InfluenceKind` enum and every `switch` over it, the `RulesFrom(WeatherParameters)` rows for
per-kind values, and the `weather_parameters.json` / `weather_schema.json` edits.

**Needs these plans Done:**
- `weather-data.plan.md` (A4 Feature 1): types, the generic influence caps in the sidecar, `ParamBinding`.
- `parameter-files.plan.md` (Dia): `ParameterFile`, `ApplyPack`, modifier legality by kind, `Hash()`.
- `biome-registry.plan.md` (S-1): only transitively (A4 Feature 1 uses it).
- `worldsim-kit.plan.md` (World Step): the `cow.weather.records` record-reload hook, `SimCommandSet`,
  `WorldSimNames.h`, the save codec (used by the codec task).
- `ca-pipeline.plan.md` (A4 Feature 2): from the pipeline task that adds pass 0 scratch and `Capture` taking an
  `InfluenceSnapshot`.
`grid-search-helpers.plan.md` and `blackboard-scopes.plan.md` are not needed (hex distance already exists as
`Dia::Geometry2D::HexTopology::Distance`).

**Startable earlier on fakes.** The catalogue, book, weight and evaluation tasks (Tasks 2 to 8) are pure logic on
hand-built terrain and need only A4 Feature 1's types and the sidecar generic caps. Tasks that need pipeline passes,
the module, save, presets, smoothing and the command registration are marked **Blocked until <named capability>
from <plan>** and stay on fakes until then. Every Blocked mark is a start condition, not a defect.

**Division of work with the sibling plans (no duplication).** `ca-pipeline.plan.md` owns the ten-array
`WeatherFields` with `pressureAnomaly`, `WeatherHandoff::influenceWater` (type, in `weather-data.plan.md`), pass 2's
anomaly term, the 5a/5b anomaly transport, the 5c decay and the 14-entry profile, `Capture` taking an
`InfluenceSnapshot`, the pass 0 scratch arrays, and forcing hooks A to D. This plan owns: the kind catalogue,
`InfluenceBook`, the weight and falloff maths, the **body** of pass 0, the temperature-after-pass-1 and
moisture-after-pass-4 delta application, the book codec, the ground-water fill, the module-facing API tests and the
`cow.weather.influence` handler. Wiring other plans list is referenced by capability: the module owns the book
(`Latch` in `Capture`, `Age` in the commit hook, clear on a new revision) and `IWaterInputSource` in
`weather-module.plan.md`; the influences block in the save record in `weather-save.plan.md`; `ApplyPreset` clearing
the book in `presets-and-regression.plan.md`; name registration and the `force storm` routing in
`weather-debug.plan.md`; the preview interface header in `weather-smoothing.plan.md`.

All code is CoW (application) under `Cluiche/CoW/Shared/Weather/` and `Cluiche/CoW/Modules/`. No Dia files are
added or edited (AD-001). Hex distance exists as `Dia::Geometry2D::HexTopology::Distance(CellCoord, CellCoord)`
(static, cube coordinates, flat-top odd-q), so no A4 distance helper is written. Where a task could need more than
sonnet it says "ask before escalating" (feedback_confirm_opus).

## Implementation Patterns

### Files (Task 1)
- `Shared/Weather/WeatherInfluence.h/.cpp`: enums, `InfluenceDesc`, `InfluenceRegion`, `InfluenceInfo`,
  `InfluenceRules`, `InfluenceSnapshot`, `InfluenceBook`, `InfluenceWeight`, `InfluenceRejectReasonName`.
- `Shared/Weather/InfluenceKindCatalogue.h/.cpp`: the catalogue types, loader and lookup.
- `Shared/Weather/WeatherInfluencePass.cpp`: the pure per-cell evaluation and the pass 0 body.
- `Shared/Weather/WeatherInfluenceCodec.h/.cpp`: book to and from `Json::Value`, `Validate`.
- `Modules/WeatherInfluenceCommands.h/.cpp`: `cow.weather.influence` handler over `WeatherModule`'s API.
- `Shared/Weather/Testing/InfluenceTestHelpers.h`: builders for descriptors, a small hand-built grid, a fixture
  catalogue (the five shipped rows plus an extra kind) and a fake A5 consumer (tests ship with libraries).
- Data: `Assets/CoW/Global/Weather/influence_kinds.json` and `influence_kinds.schema.json` (format
  `cow.weather_influence_kinds`), catalogued and resolved through `PathStore`.
- Tests in `Tests/GoogleTests/CoW/Weather/`: `TestWeatherInfluenceBook`, `Eval`, `Timing`, `Determinism`, `Save`,
  `Commands`, `Handoff`, `Catalogue`. Suite prefix `WeatherInfluence*` for all eight.
- Namespace `CoW::Weather`. No STL in public signatures (PD-004): `DynamicArrayC`, POD records, `StringCRC`.

### Kind catalogue (Tasks 2a to 2d)
- A kind row (a DiaData record through `JsonDefinitionLoader`, validated by the sidecar): `id` (StringCRC string,
  unique), `name`, `mode` (`impulse` or `sustained`), `pressure` (the seed per weight, signed), `temperature` and
  `moisture` (deltas per weight, signed), `water` (signed water-weight per weight, handed to A5; see open item 2),
  `falloff` (default), `max_lifetime_weeks`, `max_strength`. The five shipped rows carry the GDD section 10a values:
  storm impulse (`pressure` -0.3, `moisture` 0.2); warmer `temperature` +0.1; cooler -0.1; wetter `moisture` +0.15
  and `water` +1; drier -0.15 and -1. All other rows are 0.
- Strict load (R1: fail loudly): a bad or duplicate id, an unknown `mode`, a value outside the sidecar range or an
  unknown key fails the whole file, names the file, record and field, and sets health Failing (the stage is not
  ready). There is no compiled fallback catalogue.
- Reload goes through the kit's `cow.weather.records {action: reload|list}`: queued, strict, applied at `Capture`.
  `FileWatcher` (dev builds) queues the same reload.
- The catalogue is immutable during a step: `Capture` latches a copy of the rows used by the snapshot entries.
- `InfluenceDesc::kind` is a catalogue id (StringCRC); the enum and its switch statements do not exist. Evaluation
  reads the kind row held in the snapshot entry.
- Where a kind row has an optional `parameters` list (`[{key, op, value}]`), applying the influence adds a DiaAttribute
  modifier to the weather `ParameterFile` (Add or Multiply on float and int, Override on any kind) and expiry or
  removal removes exactly that modifier. No shipped kind uses it; the fixture catalogue does. Field effects never
  use modifiers.

### InfluenceRules (Task 2)
- `InfluenceRules` is a POD copied from the snapshotted `WeatherParameters` (the **generic** `influence` block):
  `maxStrength`, `maxLifetimeWeeks`, `maxActive`, `maxDeltaPressure`, `maxDeltaMoisture`, `maxDeltaTemperature`,
  `capWater`, `maxRadius`, `maxSetCells`, `groundWaterFactor`, `pressureAnomalyMax`. Built by
  `InfluenceRules RulesFrom(const WeatherParameters&)`. The per-kind seeds and deltas come from the catalogue, not
  the rules. The book never holds a pointer to the parameters or the catalogue; `Add` takes both by const reference on
  each call, so a reload changes the next call only (AC 27). A kind's `max_strength` and `max_lifetime_weeks` cap its
  own entries within the generic caps (open item 3).

### InfluenceBook (Task 3)
- Storage: fixed-capacity `DynamicArrayC<InfluenceInfo>` sized to the largest legal `max_active` (256), plus one pooled
  `DynamicArrayC<uint32_t>` arena for all cell-set regions (capacity 65,536 = largest legal `max_set_cells`). No
  allocation after construction. Entries stay in ascending id order, so `Latch` copies in id order with no sort.
- Ids start at 1 per world, strictly increasing, never reused; `Clear()` resets the counter to 1 only for a new world;
  `Remove` and expiry never touch it. A rejected `Add` changes nothing, **not even the id counter and not the
  revision** (AC 4): validate everything first, mutate last.
- Validation order (first failure wins): `NotReady`, `BadKind` (an id not in the catalogue), `BadStrength`
  (`!isfinite`, `<= 0`, above the kind's and the generic `maxStrength`), `BadLifetime`, region checks (`EmptyRegion`,
  `CellOutOfRange`, `RadiusTooLarge`, `FalloffOnCellSet`, `RegionStorageFull`), then `ActiveCapReached` last.
- A cell set is sorted and de-duplicated on `Add`, then `RegionStorageFull` is checked against the **de-duplicated**
  total. Circle `centre >= cellCount` is `CellOutOfRange`.
- `Latch()` marks every live entry `inFlight` and returns an `InfluenceSnapshot`: POD entries (kind row copy, region
  mode, centre, radius, `cellsBegin`, `cellsCount` into a snapshot-owned cell pool, strength, falloff,
  `firesImpulse` = the kind row's mode is `impulse` and `weeksApplied == 0`). The snapshot owns its copy of the cell
  lists. Snapshot storage is sized at construction; `Latch` does no allocation.
- `Age(commitDay)` increments `weeksApplied` on every `inFlight` entry, stamps `landedDay = commitDay` when it becomes
  1, clears `inFlight`, then deletes entries with `weeksApplied >= lifetimeWeeks`. `landedDay` is not saved.
- `Revision()` bumps on add, remove, latch, age and clear and on nothing else. A discarded step is a step where `Age`
  is never called; the book has no "discard" method.

### Weight and falloff (Task 6)
- `float InfluenceWeight(const InfluenceDesc&, uint32_t cell, const HexGridTables&)`. CellSet: binary search,
  `strength` or 0. Circle: `d = HexTopology::Distance(ToCoord(centre), ToCoord(cell))`; `d > radius` gives 0;
  `t = d / (radius + 1)` in float; `None: 1`, `Linear: 1 - t`, `Smooth: 1 - t*t*(3 - 2*t)`; weight is
  `strength * f`. Only `+ - *` and one divide by the integer `radius + 1`, no `sinf` or `cosf`.
- Task 0 confirms what `HexGridTables` exposes. If it has no `width`/`height`, the function takes a `HexTopology` by
  const reference. Record the choice in the notes.

### Pass 0 evaluation (Tasks 7 to 9)
- `struct InfluenceCellDelta { float dPressure, dTemperature, dMoisture, waterWeight; }` and a pure
  `InfluenceCellDelta EvaluateInfluenceCell(const InfluenceSnapshot&, uint32_t cell, const InfluenceRules&, const HexGridTables&)`.
  It loops the snapshot in ascending id order and accumulates four float sums from each entry's **kind row**:
  `w * row.pressure` and `w * row.moisture` for an impulse kind only when `firesImpulse`; `w * row.temperature`,
  `w * row.moisture` and `w * row.water` for a sustained kind every step. Then each sum is clamped:
  `dPressure` to `+/-maxDeltaPressure`, `dTemperature` to `+/-maxDeltaTemperature`, `dMoisture` to
  `+/-maxDeltaMoisture`, `waterWeight` to `+/-capWater`. The float sum order is fixed by the snapshot order.
- The pass 0 body is one `executor.ParallelFor(N, kMinRange, fn)` calling it, writing the four results to the
  pipeline's pass 0 scratch arrays (allocated by the pipeline; this plan never allocates), then in the same range
  adding `dPressure` to `pressureAnomaly` clamped to `+/-pressureAnomalyMax` and writing `waterWeight` into
  `WeatherHandoff::influenceWater`. It wraps itself in `DIA_PROFILE_SCOPE` named `0`. An empty snapshot skips the
  pass (AC 24).

### Delta hooks (Task 9)
- `dTemperature` is added right after pass 1, clamped to [0, 1]. `dMoisture` is added right after pass 4, clamped to
  [0, 1], at the same point as forcing hook C. **Hold wins**: hook C's `MoistureHold` writes after the influence add. A
  `PressureHold` after pass 2 already overrides the anomaly on that cell.
- Pass 0 runs first in `Run`, so the anomaly is in `pressureAnomaly` when pass 2 reads it.

### Storm and sustained kinds
- An `impulse` kind acts only in the step where `firesImpulse` is true. It stays in the book for its lifetime, counts
  against `max_active` and seeds nothing after that. What it started travels as moisture and as `pressureAnomaly`
  through the pipeline's 5a/5b and decays in 5c. This plan does not re-test the decay arithmetic; it tests that an
  **impulse-kind influence** produces a low that outlasts one week.

### A5 hand-off (Tasks 15, 16)
- `bool FillGroundWaterInfluence(const WeatherHandoff&, float groundWaterFactor, bool anyActive, DynamicArrayC<float>& out)`
  is pure: `out[c] = influenceWater[c] * groundWaterFactor`, false (and `out` unchanged) when `anyActive` is false.
  `WeatherModule::CopyGroundWaterInfluence` calls it. `influenceWater` is computed with or without A5 and is never
  saved.

### Save codec (Task 14)
- `EncodeInfluences(const InfluenceBook&, Json::Value& out)` and `bool DecodeInfluences(const Json::Value&, uint32_t
  cellCount, const InfluenceRules&, const InfluenceKindCatalogue&, InfluenceBookState& out, CodecError&)`,
  `InfluenceBook::Restore(const InfluenceBookState&)`. Each item stores its kind **by catalogue id** (a string id, not
  an ordinal). Decode is all-or-nothing into a scratch state; `Restore` copies after success. `Validate` is the decode
  with no restore. `CodecError` is the kit codec's error struct (use it; a local two-field stand-in only until the kit
  lands). An unknown kind id is a `Validate` defect.
- `in_flight` entries restore as `inFlight` with `landedDay = kNotLanded`. The `influences` object sits inside the
  weather record; `weather-save.plan.md` wires it into the participant and owns the field-level numbers.

### Module API and commands (Tasks 17 to 20)
- `WeatherModule::AddInfluence/RemoveInfluence/ListInfluences/GetInfluenceRevision` are thin forwarders over the book
  with a SimPU assert. The book wiring itself is `weather-module.plan.md`; this plan adds the feature-level timing
  tests on top.
- `GetInfluencePreviewSource()` returns a small pure adaptor, `InfluenceBookPreviewSource`, that implements Feature 9's
  `IInfluencePreviewSource` over `const InfluenceBook&` and the grid (open item 5).
- `cow.weather.influence` handler: parse once into a typed `InfluenceCommand` (`add`, `remove`, `list`, `clear`, and
  listing the catalogue kinds; open item 4), then call the same module methods. No logic duplicates the API (AC 51).
  Errors name the field and queue nothing. The default `source` is `"debug"`; the default falloff is the kind row's.
- Metrics `cow.weather.influences_active` (gauge) and `cow.weather.influences_rejected` (counter) with `MetricInfo`,
  written once per applied step on SimPU. The reject counter increments inside the module's `AddInfluence` wrapper.
  Logs on `WTH`; `DIA_TRACE_ZONE` on `AddInfluence`, the latch and the pass 0 body.

### Testing
- Pure tests on a hand-built terrain from `Testing/InfluenceTestHelpers.h`; pipeline tests use the shared
  `MakeCoastRange` from `presets-and-regression.plan.md` if built, else a local copy of the same shape (record it).
- Float equality only where the maths is exact; otherwise `EXPECT_FLOAT_EQ`. Determinism compares with `memcmp`
  after 52 weeks.
- Range-count tests use the pipeline's `ReverseRangeExecutor` and `FixedRangeExecutor(n)` (n = 1, 3, 17) and
  `SerialExecutor`. Never write a second set.
- AC 38 records the data pointer and capacity of every pass 0 scratch array and of the snapshot storage before and
  after `Run`; no global `operator new` hook.

### Verification cadence
- Per task: narrow `dia run googletest --filter="WeatherInfluence*" --shards 2` (always `--shards 2`; add `WeatherLifecycle*`
  or `WeatherPass*` for tasks that edit the pipeline files, to prove no behaviour change). The 52-week matrix runs once
  at the end of its own task.
- **No full-suite run in this plan** (it happens once, in worldstep.plan.md Weather-stack close-out). No sanitizer unless
  asked. No mutation checks. No `SLOW_` suites. World-sim checks use cell data and stats, not screenshots.
- One commit per task, with the plan row updated in the same commit through `dia docs plan`.

> **Carried from CA pipeline (2026-11-24):** AC 9b (empty snapshot skips pass 0 and output is bit-identical to a run without Feature 7; with influences `dPressure` lands in `pressureAnomaly` before pass 2, `dTemperature` after pass 1, `dMoisture` after pass 4) needs the pass 0 body built here. The seams exist in `Draoi/DraoiHexWorldWeather/WeatherPipeline.cpp` (`PassId::Influence` slot, marked seams after passes 1 and 4); `InfluenceSnapshot` is a placeholder in `WeatherTypes.h`.

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 0 | Prerequisite check, read only: record the build state of A4 Features 1 to 6 and which tasks therefore wait. Confirm these exist or are planned, by name: `WeatherFields::pressureAnomaly` and `WeatherHandoff::influenceWater` and the generic `influence` sidecar rows (`weather-data.plan.md`); pass 0 scratch, `kPassCount` 14, `Capture` taking an `InfluenceSnapshot`, forcing hooks A to D, the test executors (`ca-pipeline.plan.md`); the module-owned book, `IWaterInputSource` and the commit hook (`weather-module.plan.md`); the `influences` save block (`weather-save.plan.md`); `ApplyPreset` clearing the book (`presets-and-regression.plan.md`); command registration and the `force storm` routing (`weather-debug.plan.md`); the preview interface header (`weather-smoothing.plan.md`); the kit's `cow.weather.records` hook and `JsonDefinitionLoader` record-sidecar validation; `ParameterFile` modifier legality by kind. Confirm `HexTopology::Distance(CellCoord, CellCoord)`, `ToCoord`, and what `HexGridTables` exposes (width, height or a topology) | TDD-exempt (read-only prerequisite check). Findings recorded in notes | Not Started | haiku | Mechanical, TDD-exempt. If `HexTopology::Distance` is missing when built, stop and flag it (AD-001 ask first). Record the `HexGridTables` answer for Task 6 **Shared-base audit:** confirm row 4a (shared-base audit of `WorldStepStageBase`, `WorldSimSaveCodec`, `HexWorldSaveCodec`, `WorldStepModule`) in `a4-weather.plan.md` is Done, or record that it is not and why, before the first row below that touches shared code (9 `Capture` snapshot, 14 save codec, 17 `Latch`/`Age`/`PublishCommit`). |
| 1 | Create the empty files listed in Implementation Patterns and the eight test files; register each with `dia docs vcxproj-add CoW ...` and `dia docs vcxproj-add GoogleTests ...` (one call per file); add the two data files and their catalogue entries | TDD-exempt (mechanical scaffold). Solution builds with the empty files | Not Started | haiku | Mechanical, TDD-exempt. Idempotent |
| 2 | RED then GREEN: enums (`InfluenceFalloff`, `InfluenceRejectReason`, `InfluenceState`), `InfluenceDesc`/`InfluenceRegion`/`InfluenceInfo`/`InfluenceResult::Ok()` with a catalogue-id kind, `InfluenceRules` and `RulesFrom(WeatherParameters)`, `InfluenceRejectReasonName` | AC 3 (name half): every reason has a distinct non-empty name; `RulesFrom(snapshot of the shipped sidecar)` equals the GDD section 10a generic values written as literals in the test (1.0, 13, 64, 0.3, 0.3, 0.15, 1.0 cap_water, 32, 8192, 0.05, 0.5) | Not Started | sonnet | Needs the `weather-data.plan.md` sidecar and binding Done. Literals in the test, so it cannot pass vacuously. No `InfluenceKind` enum |
| 2a | **[old 2a]** RED then GREEN: `InfluenceKindCatalogue` types and strict loader over a fixture record file and its sidecar **[old 2b]** RED then GREEN: author `influence_kinds.json` and its sidecar with the five GDD rows | [old 2a] New (amendment 1, spec-6 resolution 1): the loader accepts a valid file; each fault fails the whole file naming file, record and field: duplicate id, empty id, unknown `mode`, a value outside the sidecar range, an unknown key, a wrong `format` or `version`; lookup by id returns the row, an unknown id returns null; no compiled fallback exists; [old 2b] New (data): the shipped file loads strictly; storm is `impulse` with `pressure` -0.3 and `moisture` 0.2; warmer `temperature` +0.1; cooler -0.1; wetter `moisture` +0.15 and `water` +1; drier -0.15 and -1; every default inside its sidecar range; the GDD values are literals in the test; `dia validate schema` passes on both files | Not Started | sonnet | [old 2a] Records go through DiaData `JsonDefinitionLoader` with sidecar validation, not through `ParameterFile`. Failure maps to health Failing at the module (tested with the module) [old 2b] The removed `INFLUENCE_STORM_PRESSURE` (any spelling) is not a column. Catalogue entries through the asset path, not by hand **Merged from old rows 2a, 2b (speed-up pass 2026-11-24): one dispatch, one scoped build, one commit and one `dia docs plan` update for the merged row; write the RED tests for every merged part first, then GREEN. The `[old N]` labels name the original row so its text stays traceable.** |
| 2c | **[old 2c]** RED then GREEN: catalogue reload through the kit's records hook, and the data-only proof **[old 2e]** RED then GREEN: catalogue file identity and reload safety: the file is `Global/Weather/influence_kinds.json` (format `cow.weather_influence_kinds`) with its own sidecar, signed `water` and `default_falloff` columns; global caps read from the params sidecar only; `cow.weather.records {action: list|reload}` lists kinds; a reload removing a kind used by a live influence fails strictly | [old 2c] New: a queued reload with a fixture file that adds a sixth kind (`heat_wave`) makes `AddInfluence` of it succeed and evaluate with **no code change**; a reload with a bad record keeps the old catalogue, sets health Failing naming the file, record and field, and the stage is not ready until a good reload; a reload while influences of a removed kind are live leaves them evaluating from their latched rows and rejects a new add of that kind; [old 2e] AC 54, 55, 56: format id and columns load; a kind row carries no `max_strength` or other global cap; `list` returns every kind id; a reload with a kind still referenced by a live influence fails naming the kind and the influence ids, health Failing, and the previous catalogue is unchanged and still evaluates | Not Started | sonnet | [old 2c] **Blocked until the records-reload hook from `worldsim-kit.plan.md`.** The fixture sixth kind is the proof that a new kind is data. Whether a removed kind's live entries survive a reload is not specified (open item 6) [old 2e] Added 2026-11-18 (plan-gap resolution). Extends Tasks 2a and 2c. Uses the kit's `cow.weather.records` hook **Merged from old rows 2c, 2e (speed-up pass 2026-11-24): one dispatch, one scoped build, one commit and one `dia docs plan` update for the merged row; write the RED tests for every merged part first, then GREEN. The `[old N]` labels name the original row so its text stays traceable.** |
| 2d | RED then GREEN: kind-row parameter modifiers | New (amendment 2, spec-6 resolution 1): with a fixture kind whose row lists one scalar parameter effect, `AddInfluence` followed by the latch adds a modifier on the weather `ParameterFile`, the snapshotted `WeatherParameters` reflects it, and expiry or `RemoveInfluence` removes exactly that modifier; an illegal operation for the parameter's kind (Add on a bool) is rejected at catalogue load; none of the five shipped kinds carries one; field effects use no modifier | Not Started | sonnet | **Blocked until modifier legality by kind from `parameter-files.plan.md`.** The row's `parameters` shape is not given by the spec (open item 1). If it is judged out of scope, close this row with a note |
| 3 | **[old 3]** RED then GREEN: `InfluenceBook::Add` validation, ids, sorted storage **[old 4]** RED then GREEN: `Remove`, `RemoveBySource`, `List`, active cap, lowered `max_active`, `Clear`, `Revision` **[old 5]** RED then GREEN: `Latch`, `Age`, states, `landedDay`, expiry | [old 3] AC 1, 2, 3, 4, 5: valid add returns id 1 then 2, listed `Pending` with `weeksRemaining == lifetimeWeeks`; each rejection reason fires on its own trigger (an id not in the catalogue is `BadKind`; strength 0, negative, NaN, infinity, over the generic max, over the kind's `max_strength`; lifetime 0, over the generic max, over the kind's `max_lifetime_weeks`; empty set; member index and centre `>= cellCount`; radius over max; falloff on a cell set; set past `max_set_cells`; `NotReady`); a rejected call leaves list, id counter and revision unchanged; a set `{9, 3, 3, 5}` is stored `{3, 5, 9}` and a repeated cell does not double its weight (weight half in Task 6); [old 4] AC 6, 25, 26, 27, 27a, 33 (add, remove, clear half): an unknown or already-removed id returns false and changes nothing; the 65th add returns `ActiveCapReached` and nothing is dropped; remove frees a slot at once; lowering `max_active` below the count keeps entries and rejects new ones until the count falls; an impulse-kind entry counts toward the cap for its whole lifetime; revision changes on add, remove and clear only, never on a rejected add; [old 5] AC 20 (book half), 28, 29, 30, 31 (book halves), 33 (latch and age half), 51a groundwork: an added entry is `Pending` until `Latch`, then `InFlight`, then `Active` after `Age`; the snapshot is a copy (removing or adding after `Latch` does not change it, cell lists and kind rows included); `firesImpulse` is true only for an `impulse` kind while `weeksApplied == 0`; an entry lives exactly `lifetimeWeeks` latch-and-age cycles; `landedDay` is `kNotLanded` until the first `Age`, then equals its `commitDay`; `Latch` with no `Age` leaves `weeksApplied` at 0; expiry frees a slot | Not Started | sonnet | [old 3] Pure, no grid yet. Validate everything before mutating. `ActiveCapReached` is tested in Task 3 [old 4] Expiry freeing a slot (AC 26 second half) is proved in Task 3. [old 5] The module-level versions (a real stage driver) are Task 17. Poisoning the snapshot after `Latch` is Task 21 **Merged from old rows 3, 4, 5 (speed-up pass 2026-11-24): one dispatch, one scoped build, one commit and one `dia docs plan` update for the merged row; write the RED tests for every merged part first, then GREEN. The `[old N]` labels name the original row so its text stays traceable.** |
| 6 | RED then GREEN: `InfluenceWeight` and falloff | AC 5 (weight half), 7, 8, 9, 10, 11: a Circle touches exactly the cells within `radius` hex steps on odd and even columns and nothing farther; `None` gives `strength` everywhere in range; `Linear` and `Smooth` give `strength` at the centre, decrease monotonically, and stay above 0 at distance `radius`; radius 0 is only the centre; a circle over the grid edge touches only in-grid cells with no wrap; a CellSet gives `strength` on members only; the book's per-cell weight equals `InfluenceWeight` | Not Started | sonnet | Known answers computed by hand (a 7 x 7 grid, distances listed in the test), not by calling `HexTopology::Distance`. Record the Task 0 `HexGridTables` decision |
| 7 | **[old 7]** RED then GREEN: `EvaluateInfluenceCell` for sustained kind rows, overlap sum and ordering **[old 8]** RED then GREEN: impulse-kind values, per-field caps, `waterWeight` | [old 7] AC 16 (value half), 17 (value half), 21, 22: with the shipped rows, warmer `+0.1 * w`, cooler `-0.1 * w`, wetter `+0.15 * w`, drier `-0.15 * w`; two same-kind influences sum per cell; warmer and cooler on one cell sum with opposite signs, likewise wetter and drier; the same pair added in either order gives the same result because evaluation is ascending id; the fixture `heat_wave` row evaluates from its own values with no code change; [old 8] AC 12 (value half), 13 (value half), 23, 44 (value half): a `firesImpulse` storm at full strength at the centre gives `dPressure` -0.3 and `dMoisture` 0.2 * weight; the same storm with `firesImpulse` false gives 0; 64 stacked influences on one cell never exceed 0.15, 0.3, 0.3 or `capWater` in absolute value; `waterWeight` is positive for wetter only, negative for drier only, the signed sum where they overlap, capped at 1.0 | Not Started | sonnet | [old 7] Pure, hand-built snapshot (no book needed). Float sum order is the snapshot order [old 8] Caps apply to the **sum**, not to each entry. **Merged from old rows 7, 8 (speed-up pass 2026-11-24): one dispatch, one scoped build, one commit and one `dia docs plan` update for the merged row; write the RED tests for every merged part first, then GREEN. The `[old N]` labels name the original row so its text stays traceable.** |
| 9 | **[old 9]** RED then GREEN: pass 0 body wired into `Run`: parallel pass, skip when empty, anomaly add and clamp, `influenceWater` write **[old 10]** RED then GREEN: temperature delta after pass 1, moisture delta after pass 4, clamps, hold wins | [old 9] AC 12 (pipeline half: a storm seeds -0.3 into `pressureAnomaly` at the centre after one `Step`), 14b (anomaly clamped to +/-0.5; zero on a world that never had a storm), 18 (third part: the seeded low is in pass 2's pressure and pass 3's wind sees it), 24 (empty snapshot: fields, totals and profile bit-identical to a run without this feature), 38 (no heap allocation in pass 0: pointer and capacity of every scratch array and the snapshot unchanged across `Run`); [old 10] AC 15, 16 (pipeline half), 17 (pipeline half), 18 (first two parts: a temperature delta is not erased by pass 1 relaxation, a moisture delta is not erased before transport), 19 (a cooler on a 0.0 cell leaves 0.0, a wetter on a saturated cell stays 1.0), 20 (final proof: an influence acts in each of its `lifetimeWeeks` applied steps and in no later one, driven by a test loop calling `Latch`, `Step`, `Age`) | Not Started | sonnet | [old 9] **Blocked until pass 0 scratch, `Capture` taking the snapshot and the 5c decay (14-entry profile) from `ca-pipeline.plan.md`.** The profile slot exists there; this task fills the body. Ask before escalating [old 10] **Blocked until passes 1 and 4 and forcing hooks A to D from `ca-pipeline.plan.md`.** Edits pipeline files in place; run `WeatherPass*` in the same filter to show no behaviour change **Merged from old rows 9, 10 (speed-up pass 2026-11-24): one dispatch, one scoped build, one commit and one `dia docs plan` update for the merged row; write the RED tests for every merged part first, then GREEN. The `[old N]` labels name the original row so its text stays traceable.** |
| 14 | RED then GREEN: save codec, `Validate`, restore | AC 41 (every listed defect rejected naming the key and mutating nothing: items over `max_active`, an id not strictly ascending or `>= next_id`, an unknown kind id (against the catalogue) or falloff, `weeks_applied >= lifetime_weeks`, lifetime or strength out of range, unsorted or duplicated or out-of-range cell list, centre `>= cell_count`, total cells over `max_set_cells`), 43 (`next_id` restored), 39 and 40 (book half: encode, real JSON text, decode and restore is bit-identical for pending, in-flight and active entries; an in-flight entry restores as in flight and `Age` ages it once); kinds round trip by catalogue id and a reload that renames a kind makes the old id a `Validate` defect | Not Started | sonnet | Pure; needs only Task 3 and the catalogue. The field-level N = 1, 13, 52 round trip and AC 42 belong to `weather-save.plan.md`, which calls these functions. Round trip goes through real JSON text |
| 15 | RED then GREEN: `FillGroundWaterInfluence` and the sim-level no-A5 behaviour | AC 44a (pure half: output is `influenceWater * ground_water_factor`; false with no active influence and `out` untouched), 46 (with `HydrologyInputs.present == false` the A4 fields equal the with-A5 run's A4 fields; `influenceWater` is still computed; nothing is Degraded) | Not Started | sonnet | Pure part has no blocker. The AC 46 assertion needs `HydrologyInputs` in `Capture` from `ca-pipeline.plan.md` and is written last; if unbuilt, leave it for Task 16 |
| 16 | RED then GREEN: A5 hand-off through the module | AC 44a (module half: `CopyGroundWaterInfluence` called only inside `Capture` on SimPU), 45 (a fake A5 consumer receives positive values as fake `extraWater` and negative values as a fake `draw` contribution, same sign and size, in the same `Capture` phase) | Not Started | sonnet | **Blocked until the `IWaterInputSource` seam from `weather-module.plan.md`.** This task asserts values and the sign contract through that interface, with the fake consumer |
| 17 | RED then GREEN: public module API and feature-level timing | AC 28 (added between kicks: `Pending` until the next `Capture`, affects that step, visible after its `Commit` only), 29 (added while a step is in flight: that step unchanged, the next affected), 30 (removed while in flight: that step unchanged, no later step), 31 (a discarded step ages and applies nothing), 32 (a new world revision, a preset load and a state import with other dimensions clear the book with one Info each; matching dimensions leave it), 33 (revision changes on add, remove, latch, age, clear and nothing else), `AddInfluence` asserts SimPU | Not Started | sonnet | **Blocked until the module-owned book (latch, age, clear) and the stage `Capture`/`PublishCommit` hooks from `weather-module.plan.md`.** That plan tests the wiring (latched once, aged once); this task asserts the externally visible timing and must not repeat it. The preset-load and state-import clears are wired by `presets-and-regression.plan.md` and `weather-save.plan.md`; here they are only observed, and skipped with a note if unbuilt |
| 18 | RED then GREEN: `InfluenceBookPreviewSource` adaptor and `GetInfluencePreviewSource()` | AC 51a: lists each influence's kind (catalogue id), `addedDay` (as `requestDay`), `landedDay` and per-cell weight; `landedDay` is `kNotLanded` until the `Commit` that first includes the influence and equals that commit's day afterwards; not saved (a codec round trip drops it) | Not Started | sonnet | **Blocked until the preview interface header from `weather-smoothing.plan.md`.** The adaptor builds the sparse weight list once per influence id; Feature 9 caches it. See open items 4 and 5 |
| 19 | RED then GREEN: metrics, logs, trace zones | TDD-exempt (wiring/instrumentation: tests written with the code, no separate RED proof). AC 52 (`cow.weather.influences_active` gauge and `cow.weather.influences_rejected` counter register with `MetricInfo`, written only on SimPU once per applied step), 53 (each rejection logs one Warning naming the reason and the source; add, remove and expiry log Info; one Info when the book is cleared) | Not Started | sonnet | **Blocked until health and logs from `weather-module.plan.md` and the layer and event sidecar from `weather-debug.plan.md`.** Adds the metric entries through that plan's sidecar, not a second file. `DIA_TRACE_ZONE` on `AddInfluence`, the latch and the pass 0 body |
| 20 | RED then GREEN: `cow.weather.influence` handler (`add`, `remove`, `list`, `clear`, and listing the catalogue kinds) | AC 47 (registers with non-empty help of at most 256 characters through `SimCommandSet`, taking one name from the budget; the all-or-nothing failure test belongs to `weather-debug.plan.md`), 48 (each action as in the spec table; `add` validates the kind against the catalogue and replies with the id and the "next weekly step" wording, or the rejection reason; a bad field returns an error naming it and queues nothing; the kinds listing equals the catalogue), 49 (`clear` with `source` removes only that source), 51 (a direct API call and the command give identical results) | Not Started | sonnet | **Blocked until command registration from `weather-debug.plan.md` and Task 17.** AC 50 (`force storm` as a thin `AddInfluence` call with source `"debug.force"`) is asserted by `weather-debug.plan.md`; only confirm in the notes that it is |
| 21 | **[old 21]** Exhaustive test pass: gap audit of AC 1 to 53 (including 14a, 14b, 27a, 44a, 51a, and the new catalogue and modifier cases) against test names, then edge cases **[old 11]** RED then GREEN: an impulse kind travels and persists as a low **[old 12]** RED then GREEN: determinism and capture isolation **[old 13]** RED then GREEN: deferred versus synchronous | [old 21] Gap list in notes. Edge cases: 64 stacked influences on one cell; a cell set containing every cell; a circle with radius 0 on a corner; a circle radius 64; strength exactly `max_strength` and `max_strength + 1 ULP`; lifetime 1 and `max_lifetime_weeks`; an impulse kind with lifetime 1; add then remove before any `Latch`; remove during the apply step; `Latch` twice without `Age`; `Age` without `Latch`; ids after 2^31 adds (overflow recorded, not fixed); a 1-cell world; an influence on a world smaller than its cell indices after a reload; a catalogue with 255 kinds; two kinds with the same id; [old 11] AC 14, 14a: after the first step both the added moisture and the anomaly move downwind (uniform terrain, steady wind, the anomaly centre shifts at the moisture transport rate); the storm region is not re-seeded in step two; the planted storm keeps a cell below the 0.3 `find storms` threshold for at least two weeks at the GDD defaults; pass 2's pressure equals `0.5 + pressure_change + anomaly`, clamped; [old 12] AC 34 (fields, totals and `influenceWater` bit-identical at `SerialExecutor`, 1, 3 and 17 ranges and reversed ranges over 52 weeks with all shipped kinds, circle and cell-set regions, overlaps), 36 (poison the book, the descriptors, the cell-set source and the catalogue after `Capture`; `Run` unchanged), 37 (no RNG: the same call sequence gives the same ids and fields twice); [old 13] AC 35: a `Capture` at key N and `Apply` at N+k run through DiaDeferredStep's manual launcher equals a synchronous `Step` run bit for bit over 52 weeks, including influences added between kicks | Not Started | sonnet | [old 21] No mutation check. Add only the missing tests; production bugs go to a separate fix task [old 11] **Blocked until the 5a/5b anomaly transport and 5c decay from `ca-pipeline.plan.md`.** Uses an impulse-kind influence, never a direct field write. Does not re-test the decay arithmetic [old 12] **Blocked until the test executors, the purity task and the determinism matrix from `ca-pipeline.plan.md`.** Run the 52-week cases once at the end. If a pair differs, stop: it is an ordering bug, not a tolerance to widen [old 13] **Blocked until the deferred-vs-synchronous adaptor from `ca-pipeline.plan.md` and the module-owned book from `weather-module.plan.md`.** If the manual launcher is not built, leave Blocked with `dia docs plan` and continue **Merged from old rows 21, 11, 12, 13 (speed-up pass 2026-11-24): one dispatch, one scoped build, one commit and one `dia docs plan` update for the merged row; write the RED tests for every merged part first, then GREEN. The `[old N]` labels name the original row so its text stays traceable.** |
| 22 | Observation Opportunity Scan over every file touched across the five DiaObservation pillars | TDD-exempt (verification/close-out). Suggestions in notes | Not Started | haiku | Non-blocking. Expected finds: a pass 0 duration metric, a book-size gauge in `info memory`, per-source reject counts, a catalogue-load log |
| 23 | **[old 23]** Scoped verification: `dia run googletest --filter="WeatherInfluence*" --shards 2`; `dia check deps`; `dia docs precommit` (includes `dia validate schema` on the catalogue) **[old 24]** Close-out: `dia docs plan` rows to Done with notes, then `dia docs spec-done docs/specs/applications/cow/systems/a4-weather/weather-influence.md` | TDD-exempt (verification/close-out). [old 23] Pass/fail and failing names only; AC 1 to 53 listed with the task that proved each, blocked ACs named; [old 24] Spec status Done, plan header updated | Not Started | haiku | [old 23] **No full-suite run here** (it is the Weather-stack close-out in worldstep.plan.md). ACs still Blocked are reported as Blocked, not failed [old 24] **Do not run until every row above is Done.** **Merged from old rows 23, 24 (speed-up pass 2026-11-24): one dispatch, one scoped build, one commit and one `dia docs plan` update for the merged row; write the RED tests for every merged part first, then GREEN. The `[old N]` labels name the original row so its text stays traceable.** |

**AC coverage (was 30 rows, 0-24 plus 2a to 2e; now 20 rows after the 2026-11-24 merges: old 2b into 2a, 2e into 2c, 4 and 5 into 3, 8 into 7, 10 into 9, 11 to 13 into 21, 24 into 23; row numbers are kept, so some numbers are absent).** Every AC from 1 to 53 keeps its old task (the old numbers are unchanged apart from the four inserted rows): AC 1 to 6: Task 3. AC 7 to 11: Task 6. AC 12 to 15: Tasks 7, 9, 21. AC 16 to 20: Tasks 7, 9. AC 21 to 24: Tasks 7, 9. AC 25 to 27a: Task 3. AC 28 to 33: Tasks 3, 17. AC 34 to 38: Tasks 9, 21. AC 39 to 43: Task 14. AC 44 to 46: Tasks 7, 15, 16. AC 47 to 51: Task 20. AC 51a: Task 18. AC 52, 53: Task 19. AC 54 to 56 (added 2026-11-18): Task 2c. New, from the amendments (no spec AC numbers; stated in the amendment text): catalogue load, strict failure, data-only new kind, reload: Tasks 2a and 2c; saved by catalogue id: Task 14; parameter modifiers: Task 2d.

## Open items

1. **Shape of a kind row's parameter effects.** Amendment 2 says an influence that alters a parameter "applies as a DiaAttribute modifier ... removed when the influence expires", but neither the amendment nor the spec-6 resolutions give the row's field for it (key, operation, value), and none of the five shipped kinds needs one. Task 2d assumes `parameters: [{key, op, value}]`; it may be closed as out of scope.
2. **Resolved 2026-11-18:** the catalogue has a signed `water` column and a `default_falloff` column (AC 54, Task 2c). **Where `waterWeight` comes from per kind.** The spec's catalogue columns (pressure, moisture, temperature deltas, falloff, lifetime, strength cap) have no water-weight column, yet wetter and drier hand a signed `waterWeight` to A5. This plan adds a `water` column; the spec should confirm it (or say it is derived from the sign of `moisture`).
3. **Resolved 2026-11-18:** global caps (`max_strength`, `max_lifetime_weeks`, `max_active` and so on) stay parameters in the weather params sidecar and are not catalogue columns (AC 54); Task 2c removes any per-kind cap column. **Generic versus per-kind caps.** `max_strength` and `max_lifetime_weeks` are generic parameters and also kind-row columns. The plan applies the smaller; the spec does not say.
4. **Resolved 2026-11-18:** the catalogue is listed by `cow.weather.records {action: list}` (AC 55), not by an `influence` action. **Listing the catalogue.** The amendment says the command "validates the kind against the catalogue and lists it", but the action table has no `kinds` action. The plan adds a listing; its action name and reply shape are a spec gap.
5. **Who implements `IInfluencePreviewSource`.** The spec says the module; this plan has the module return a small pure adaptor so AC 51a runs without a module. If the module must implement it directly, Task 18 becomes a thin module test. Also, Feature 9's `Get(index)` should carry the `InfluenceId`; raised in `weather-smoothing.plan.md`.
6. **Resolved 2026-11-18:** a reload that removes a kind still used by a live influence fails strictly (R1) and leaves the catalogue unchanged (AC 56, Task 2c). **Reload semantics for live entries.** What happens to live influences when a catalogue reload removes or changes their kind is not specified. The plan evaluates from the latched row and rejects new adds of a removed kind (Task 2c) and treats a saved id missing from the catalogue as a `Validate` defect.
7. **Resolved 2026-11-18:** a record file, `Global/Weather/influence_kinds.json`, format `cow.weather_influence_kinds`, with its own sidecar (AC 54). **Catalogue location.** The amendment allows "the weather params sidecar or a record file". This plan uses a record file (the Dia parameter-files spec routes authored lists through `JsonDefinitionLoader`), and keeps only generic caps in the parameter sidecar.
8. **Where the temperature and moisture hooks live.** The pipeline plan builds hooks A to D and passes 1 and 4 but not the influence delta insertion points; this plan owns them (Task 9) and edits pipeline files in place after the hooks exist. Low impact.
9. **Grid object for `InfluenceWeight`.** The spec writes `const HexGridTables&`; whether it exposes width and height is the pipeline's choice (Task 0 records it).
10. **Dragon abilities, ownership rules and quotas** stay out (spec Non-Goals and Open Design Question 3).
