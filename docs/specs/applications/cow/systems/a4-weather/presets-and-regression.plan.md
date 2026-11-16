# Plan: A4 Presets & Regression (A4 Weather Feature 6)

**Spec:** @docs/specs/applications/cow/systems/a4-weather/presets-and-regression.md
**Status:** Not Started
**System plan:** @docs/specs/applications/cow/systems/a4-weather/a4-weather.plan.md (row 6)

**Dependencies.** Tasks 2 to 6 (loader, distance field, selectors, test-terrain builder) need only A4
Feature 1 Done (`weather-data.plan.md`). Tasks 7 onward (`Reseed`, `ApplyPreset`, forcings, regression
scenarios, goldens, performance) need A4 Feature 2 **built** (`ca-pipeline.plan.md`): `Step`,
`AddForcing`, `RegisterCellSet`, `ClearCellSets`. The loader, selector and fill tasks can therefore run in
parallel with Feature 2; everything after Task 6 waits for it.

All code is CoW (application): `Cluiche/CoW/Shared/Weather/` and `.../Testing/`. `CellLayerGolden.h` is
built Dia code and only consumed. No Dia edits (AD-001). Tasks that could need more than sonnet say "ask
before escalating" (feedback_confirm_opus).

## Implementation Patterns

### Preset data and loader (Tasks 2, 3)
- `Cluiche/CoW/Shared/Weather/WeatherPreset.h/.cpp`. `WeatherPreset { name; Season season; selectors
  (named predicates); fills; forcings }`, all `DynamicArrayC` and Dia strings (PD-004).
- `Selector { waterSourceMask (bitmask over None/Ocean/Lake/River); optional elevation {min,max};
  optional latitude {min,max}; optional distanceToWater {min,max} }`. Clauses AND. `"all"` is predefined.
  Range clauses inclusive, [0, 1] for elevation and latitude; `min > max` invalid.
- `PresetFill { field (enum of the nine fields); value; selector index }`, range-checked per field
  ([0, 1]; `windDirection` [0, 2pi)). `PresetForcing { ForcingKind; selector index; angleRadians; speed;
  weeks }`: `angleDegrees` is converted to radians at load; `weeks == 0` allowed only for `WindOverride`
  (means until cleared), required > 0 for other kinds.
- `bool LoadWeatherPreset(const Json::Value&, WeatherPreset& out, PresetError& err)`: strict, same style
  as Feature 1's parameter loader (unknown key invalid at every level, all-or-nothing). Build into a local
  and move to `out` only on success (AC 2). Touches no `WeatherSimulation` (AC 3).
- `PresetError { <Dia string> path; <Dia string> reason; }` shared with `PresetResult`.

### Distance field and selectors (Tasks 4, 5)
- `ComputeDistanceToWater(terrain, topology) -> DynamicArrayC<uint16_t>`: multi-source breadth-first over
  `Dia::Pathfinding::HexTopology` from every water-source cell, water cells 0, unreachable cells a
  documented sentinel (a map with no water is already rejected by `Initialise`). Fixed-size queue, no
  recursion.
- `ResolveSelector(const Selector&, const WeatherTerrain&, const DynamicArrayC<uint16_t>& distance,
  DynamicArrayC<uint32_t>& outSortedCells)`: output sorted ascending and de-duplicated, which is exactly
  the form `RegisterCellSet` accepts.

### Test terrain (Task 6)
- `Cluiche/CoW/Shared/Weather/Testing/WeatherTestTerrains.h/.cpp` (ships with the library; not in
  GoogleTests). `MakeCoastRange(width, height)` is deterministic with no RNG: west third ocean, one-cell
  coast, lowland plain, north-south ridge ramping to 1.0 at about 60% of the width, leeward interior at
  elevation 0.3, no lakes; biome from elevation and `waterSource`; latitude ramps with `y`. Returns a struct
  holding the `WeatherTerrain` plus named cell lists `coastal, inland, windward, leeward, mountain,
  lowland` so tests never re-derive predicates. Sizes: 66 x 42 (goldens) and 40 x 24 (fast scenarios).
- The terrain's own tests check shape properties: counts, ocean fraction near a third, ridge column,
  sorted de-duplicated lists, named lists disjoint where they must be, `IsConsistent()`.

### Reseed and ApplyPreset (Tasks 7 to 10)
- `WeatherSimulation` gains `Reseed(Season, const WeatherParameters&, const IBiomeSeasonalBaseline&)` and
  `ClearForcings()`. `Reseed` is the seed function Feature 1 already factored; `Initialise` calls
  `Reseed(Spring, ...)` and existing Feature 1 tests are the regression cover for that refactor. `Reseed`
  also resets the week counter to 0 (`SetCommittedWeek(0)`) and re-zeroes non-seeded fields.
- `WeatherPresetApply.h/.cpp`: `PresetResult ApplyPreset(WeatherSimulation&, const WeatherPreset&,
  const WeatherParameters&, const IBiomeSeasonalBaseline&)`.
- Order inside the call: (1) precondition (no step in flight: debug assertion, release returns an error,
  neither mutates); (2) **validate everything first** against the simulation's terrain: every used
  selector resolves to at least one cell, forcing count after the call is at most 64, cell sets at most
  16; any failure returns before any mutation (atomicity, AC 10); (3) `ClearForcings`, `ClearCellSets`,
  `Reseed(preset.season, ...)`; (4) fills in file order (later overwrites earlier); (5) `RegisterCellSet`
  per forcing selector then `AddForcing` in order; (6) return the season. `ApplyPreset` does not own the
  clock.
- Forcing field mapping uses `WeatherForcing { kind, cell = 0, cellSet, value, value2, weeksRemaining }`;
  see Open items 2 and 3.

### Presets (Task 11)
- Five files `Assets/CoW/Global/Weather/Presets/<name>.weatherpreset.json`, content per the spec's preset
  table. Numbers are starting points, tuned by Tasks 17 and 18; they live in data so tuning needs no code
  change.

### Regression scenarios (Tasks 12 to 17)
- `Tests/GoogleTests/CoW/Weather/TestWeatherRegression.cpp`, serial executor, `MakeCoastRange(40, 24)`,
  default parameters. Each test computes the GDD ratio from the builder's named lists, **prints the
  measured ratio** and a warning if the margin is under 10% (AC 16), then asserts the threshold.
- Scenarios exactly as the spec table: coastal rain (no preset, Spring start, 52 weeks, coastal over inland
  greater than 1.5); rain shadow (13 weeks, windward over leeward greater than 2.0); snow accumulation
  (`winter_snow`, 13 Winter weeks, more than 70% of elevation > 0.6 cells have `snowPack > 0.1`); spring
  melt (`winter_snow`, 13 Winter weeks to accumulate, then 13 Spring weeks, final snow below 30% of
  accumulated, lowland `groundWater` sum above 0.5 of accumulated snow sum).
- Season for each week is advanced by the test (season-from-week helpers `WeekOf`/`SeasonOf`), because
  `ApplyPreset` does not own the clock.

### Tuning (Task 18)
- Only allowed levers, in order: preset data values, the default baseline table, `cloud_decay`; then stop
  and flag to the GDD owner (any of the 20 GDD constants or a mechanic). Every change is appended to the
  Tuning log in the spec's Status section with parameter, old, new and the forcing test. A changed default
  is a new GDD deviation (D-6.4). Open Q1: enlarge the fixture before touching any lever.

### Goldens (Task 19)
- `Tests/GoogleTests/CoW/Golden/weather_layers.golden.json` (created by Feature 2's golden task, else
  created here) via `CellLayerGolden.h` `HashLayer` and `GoldenStore::Check`; 5 presets x 9 fields = 45
  hashes on `MakeCoastRange(66, 42)` after 13 weeks, serial executor. Update only with
  `DIA_UPDATE_GOLDEN=1`, and only after the Tuning log is final.

### Verification cadence
- Per task: scoped `dia run googletest --filter="WeatherPreset*:WeatherTestTerrain*" --shards 2` (or the
  task's suite). Regression and golden suites run serial and slower: `WeatherRegression*`,
  `WeatherPresetGolden*`.
- One full `dia run googletest --shards 2` in the verification task. No mutation checks. Release gates
  (AC 20) are reported only.

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 0 | Prerequisite check, read only: Feature 1 Done; Feature 2 built (for tasks 7 onward); the `WaterSource` and 13 `Biome` enum names and ids; `HexTopology` API for BFS; `CellLayerGolden.h` API (`HashLayer`, `GoldenStore::Check`) and whether the golden file already exists from Feature 2's golden task; how `WeekOf`/`SeasonOf` are exposed; the state of Feature 2's `WindOverride` field mapping | Findings recorded in notes | Not Started | haiku | TDD-exempt, mechanical. Report which tasks are unblocked now |
| 1 | Create empty `WeatherPreset`, `WeatherPresetApply`, `Testing/WeatherTestTerrains` file pairs and the three test files `TestWeatherPreset`, `TestWeatherPresetApply`, `TestWeatherRegression`; register with `dia docs vcxproj-add CoW ...` and `dia docs vcxproj-add GoogleTests ...`; create the `Presets/` asset folder | Solution builds | Not Started | haiku | Mechanical, TDD-exempt |
| 2 | RED then GREEN: `WeatherPreset` types and `LoadWeatherPreset` valid path with inline JSON, no files | AC 2, 3 (first half): selectors with each clause type, fills, forcings, `angleDegrees` converted to radians, `season` parsed; nothing touches a `WeatherSimulation` | Not Started | sonnet | Strict, all-or-nothing, Dia strings only. The five shipped files are loaded in Task 11 |
| 3 | RED then GREEN: loader rejection table | AC 1, 2: each edit is rejected with the offending key named: unknown top-level key, unknown clause, unknown selector name, `min > max`, field value out of range, unknown field, unknown forcing kind, wrong `format`, wrong `version`, `weeks` missing on a non-wind forcing; on failure no `WeatherPreset` produced | Not Started | sonnet | Table-driven over a valid base document with one edit per row. Commit |
| 4 | RED then GREEN: `ComputeDistanceToWater` | AC 4: 0 on every water-source cell; +1 per hex step on a hand-built row, odd and even columns; corner-only water gives the expected maximum | Not Started | sonnet | Depends only on Feature 1 and `HexTopology` |
| 5 | RED then GREEN: `ResolveSelector` | AC 5, 6 (selector half): clauses AND; `waterSource`, `elevation`, `latitude` and `distanceToWater` boundaries inclusive; `"all"`; output sorted and de-duplicated; empty result reported so `ApplyPreset` can name the selector | Not Started | sonnet | The "fails with the selector named" half of AC 6 is proven in Task 8. Commit tasks 4 and 5 |
| 6 | RED then GREEN: `MakeCoastRange(width, height)` and its named cell lists | Shape tests: deterministic (two calls byte-identical), `IsConsistent()`, ocean about a third, ridge column near 60%, lists sorted and in range, windward and leeward on opposite sides of the ridge, 66 x 42 and 40 x 24 both valid; every biome id is in the 13-biome enum | Not Started | sonnet | No spec AC; the tests are derived from the "Test terrains" section. Needs no `Step`, so can run before Feature 2. Commit |
| 7 | RED then GREEN: `WeatherSimulation::Reseed(Season, ...)` and `ClearForcings()`; make `Initialise` call `Reseed(Spring, ...)` | AC 7 (reseed half), 9 (week 0), 12 (idempotence): reseed after 20 weeks equals a fresh `Initialise` for Spring; reseed with Winter seeds winter baseline; week counter 0; forcings cleared | Not Started | sonnet | Edits Feature 1/2's class. Feature 1's existing tests are the regression cover for the `Initialise` refactor: run `WeatherInitialise*` with the new tests. Needs Feature 2 for `ClearForcings` and week counter |
| 8 | RED then GREEN: `ApplyPreset` validation and atomicity | AC 6, 10, 11: a selector used by a fill or forcing that resolves to 0 cells fails with the selector named; a preset exceeding 64 forcings or 16 cell sets fails and leaves the simulation byte-identical; a captured-but-uncommitted step returns an error without mutating | Not Started | sonnet | Validate first, mutate after (see Implementation Patterns). Use `memcmp` on every field plus forcing and set counts before and after |
| 9 | RED then GREEN: `ApplyPreset` reset and fills | AC 7, 8, 9, 12: state bit-identical whether the sim was fresh, ran 20 weeks first, or held earlier forcings (three cases); fills in file order with later overwriting earlier; unnamed fields hold the preset-season seed values; week 0; season returned; no forcing from a prior preset; applying the same preset twice is identical | Not Started | sonnet | Uses inline test presets, not the shipped files. Commit |
| 10 | RED then GREEN: `ApplyPreset` forcings via cell sets | AC 13, 14: a `WindOverride` covers exactly its selector's cells (wind there is the override, outside is computed); an until-cleared forcing outlives 52 weeks; a timed forcing expires after its `weeks` | Not Started | sonnet | **Direction for `coastal_storm` and `drought` is undecided** (Open item 1). This task implements absolute angles only; a derived direction waits for the decision. Commit |
| 11 | Author the five preset files from the spec's content table; add a test that loads each shipped file | AC 1 (first sentence), 17 (loading half): all five files load; names match filenames | Not Started | sonnet | Numbers are starting points. For `coastal_storm` and `drought` direction see Open item 1; write them with the chosen form or leave the forcing out and note it. No code change is allowed to compensate for a data value |
| 12 | RED then GREEN: regression scenario 1, coastal rain | AC 15, 16: mean precipitation(coastal) over mean precipitation(inland) above 1.5 on `MakeCoastRange(40, 24)`, default parameters, Spring start, 52 weeks; ratio printed; under 10% margin warns | Not Started | sonnet | Integration test, serial executor. If it fails, enlarge the fixture first (Open Q1) before Task 18 |
| 13 | RED then GREEN: regression scenario 2, rain shadow | AC 15, 16: `rain_shadow`, 13 weeks, mean precipitation(windward) over leeward above 2.0 | Not Started | sonnet | A wide-margin failure is more likely the orographic pass than `cloud_decay`: a mechanic flag to the GDD owner, not a tuning change |
| 14 | RED then GREEN: regression scenario 3, snow accumulation | AC 15, 16: `winter_snow`, 13 Winter weeks, more than 70% of cells with elevation above 0.6 have `snowPack` above 0.1 | Not Started | sonnet | Percentage and margin printed |
| 15 | RED then GREEN: regression scenario 4, spring melt | AC 15, 16: `winter_snow`, 13 Winter weeks to accumulate, then 13 Spring weeks; final snow under 30% of accumulated; lowland (elevation under 0.4) `groundWater` sum over 0.5 of accumulated snow sum | Not Started | sonnet | The accumulation step is deviation D-6.2. Accumulated snow is measured after the Winter weeks, not before. Commit tasks 12 to 15 |
| 16 | RED then GREEN: all-presets health run and executor equivalence | AC 17, 18: all five presets run 13 weeks with no NaN or infinity and every field in range; serial executor and a 3-range executor give identical bytes (once, at preset level) | Not Started | sonnet | Reuse Feature 2's `FixedRangeExecutor` |
| 17 | Tuning pass (conditional): if any of Tasks 12 to 16 fails its ratio, apply allowed levers in order and log each change in the spec Status section Tuning log (parameter, old, new, forcing test); re-run the scoped regression suite after each lever | AC 15 green on all four scenarios | Not Started | sonnet; ask before escalating | One lever per iteration, one change at a time (debug skill); three failed attempts on the same ratio stop and report. Stop and flag for the GDD owner before touching any of the 20 GDD constants or any mechanic. Record "no tuning needed" if nothing fails. Commit |
| 18 | RED then GREEN: layer goldens, 5 presets x 9 fields on `MakeCoastRange(66, 42)` after 13 weeks | AC 19: 45 hashes match; a deliberately changed constant in a scratch run is reported by preset, layer, changed-cell count and first cell | Not Started | sonnet | Run after Task 17 so the Tuning log is final. First write with `DIA_UPDATE_GOLDEN=1` once, then review the diff summary only. If Feature 2 found exact-bit hashing is configuration-specific, follow its per-configuration decision. Commit |
| 19 | Performance: `ApplyPreset` at Large (6,656 cells) in Release | AC 20: under 5 ms reported; not gated in Debug | Not Started | sonnet | Reported, not enforced. If the distance-field BFS dominates, note it as a suggestion; cache it only if the user asks |
| 20 | Exhaustive test pass: gap audit of AC 1 to 20, then edge cases | Gap list in notes. Edge cases: a selector matching every cell; a selector matching one cell; a preset with no fills; a preset with no forcings; a fill naming the same field twice; a forcing selector equal to another's (two sets, same cells); `ApplyPreset` on a 1-cell-wide terrain; preset applied when the sim has 63 or 64 forcings already; distance field on a terrain with water in one corner only; boundary values `min == max` | Not Started | sonnet | No mutation check. Add only the missing tests |
| 21 | Observation Opportunity Scan over every file touched across logs (preset load failure and apply), traces, profiling (`ApplyPreset`), metrics, health | Suggestions in notes | Not Started | haiku | Non-blocking. Feeds Feature 5 (`cow.weather.load_preset`) |
| 22 | One full verification: `dia run googletest --shards 2`; `dia check deps`; `dia docs precommit`; Release `ApplyPreset` timing reported | All AC 1 to 20 quoted; pass/fail and failing names only | Not Started | haiku | One full run. Report the printed margins for the four scenarios |
| 23 | Close-out: `dia docs plan` rows to Done with notes, then `dia docs spec-done docs/specs/applications/cow/systems/a4-weather/presets-and-regression.md` | Spec status Done, plan header updated, Tuning log recorded in the spec | Not Started | haiku | **Do not run until every row above is Done.** Not to be run when this plan is written |

## Open items

1. **Direction for `coastal_storm` ("toward land") and `drought` ("away from the nearest coast")** in a world-independent preset (spec Open Q3). The spec offers `"towardLand"` / `"awayFromWater"` resolved from the mean `distanceToWater` gradient snapped to a wind angle, but does not decide it and gives no resolution rule beyond that. The decision is needed before Task 10 and Task 11 can finish those two presets. This plan implements absolute angles only until then.
2. **`WeatherForcing` has no angle field.** The preset forcing supplies `angleDegrees` and `speed`, but the Feature 2 record has `value` and `value2`. The mapping (angle in radians to `value`, speed to `value2`) is not stated in either spec.
3. **`WindOverride` timers disagree between specs.** Feature 2 says wind overrides carry no timer and persist until `clear_wind_overrides`; this spec's table gives `coastal_storm` and `drought` wind overrides of 6 and 8 weeks, and its loader treats `weeks: 0` as "until cleared" with a positive value implied to expire. Task 10 AC 14 (timed forcing expires) cannot hold for a wind override under Feature 2 as written.
4. **`ClearForcings()` is new here but not in Feature 2's API**, which has only `ClearCellSets()` (also drops forcings). Task 7 adds it; confirm that adding to Feature 2's class from this feature is intended (the spec's Files Touched says so).
5. **Golden file ownership.** Feature 2 "registers the golden file and the first entry"; this feature adds 45 hashes. Whichever lands first creates the file; Task 0 checks. The system spec's goldens line says "at Small", this spec says 66 x 42; confirm that 66 x 42 is the Small size.
6. **Meaning of "season" for the regression run.** `ApplyPreset` returns the season and does not own the clock; the tests advance it themselves. The coastal-rain test's "Spring start" and its season progression over 52 weeks is assumed to follow the same `WeekOf`/`SeasonOf` helpers; confirm if a week-offset (the `startWeekOffset` parameter mentioned in the system spec) is expected here.
7. **"Mean precipitation" definition** for the ratios (per-week average over the run versus last-week value; cells averaged over the named list) is not specified. The plan averages per-cell, per-week precipitation over the named list and the run length.
