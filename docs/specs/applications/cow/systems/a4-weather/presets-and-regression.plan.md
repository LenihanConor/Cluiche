# Plan: A4 Presets & Regression (A4 Weather Feature 6)

**Spec:** @docs/specs/applications/cow/systems/a4-weather/presets-and-regression.md
**Status:** Not Started
**System plan:** @docs/specs/applications/cow/systems/a4-weather/a4-weather.plan.md (row 6)

**Dependencies.** Tasks 2 to 7 (loader, distance field, selectors, test-terrain builder) need only A4
Feature 1 Done (`weather-data.plan.md`). Tasks 8 onward (`Reseed`, `ApplyPreset`, forcings, regression
scenarios, goldens, performance) need A4 Feature 2 **built** (`ca-pipeline.plan.md`): `Step`,
`AddForcing`, `RegisterCellSet`, `ClearCellSets`. The loader, selector and fill tasks can therefore run in
parallel with Feature 2; everything after Task 7 waits for it.

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
  ([0, 1]; `windDirection` [0, 2pi)). `PresetForcing { ForcingKind; selector index; angleRadians; WindDirectionSpec
  (Absolute | Inland | Seaward); speed; weeks }`: a `WindOverride` takes either an absolute `angleDegrees`
  (converted to radians at load) **or** a `direction` of `"inland"` or `"seaward"`, never both and never
  neither (amendment 1); `weeks == 0` allowed only for `WindOverride` (means until cleared), required > 0
  for other kinds.
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

### Inland / seaward direction (Task 6)
- `"inland"` is the on-screen direction of increasing distance from water. For each selector cell `c` the
  gradient is the sum over its six neighbours `n` of `unitVector(d) * (distance(n) - distance(c))`; the
  vectors are summed over all of the selector's cells and snapped once with `SnapWindDirection`.
  `"seaward"` is the opposite direction. A zero vector makes the preset invalid ("no inland direction").
  `coastal_storm` and `drought` both use `"inland"` (away from coast and from water is the same direction);
  `rain_shadow` keeps an absolute angle. Resolved once per selector, never per week.

### Test terrain (Task 7)
- `Cluiche/CoW/Shared/Weather/Testing/WeatherTestTerrains.h/.cpp` (ships with the library; not in
  GoogleTests). `MakeCoastRange(width, height)` is deterministic with no RNG: west third ocean, one-cell
  coast, lowland plain, north-south ridge ramping to 1.0 at about 60% of the width, leeward interior at
  elevation 0.3, no lakes; biome from elevation and `waterSource`; latitude ramps with `y`. Returns a struct
  holding the `WeatherTerrain` plus named cell lists `coastal, inland, windward, leeward, mountain,
  lowland` so tests never re-derive predicates. Sizes: 66 x 42 (goldens) and 40 x 24 (fast scenarios).
- The terrain's own tests check shape properties: counts, ocean fraction near a third, ridge column,
  sorted de-duplicated lists, named lists disjoint where they must be, `IsConsistent()`.

### Reseed and ApplyPreset (Tasks 8 to 11)
- `WeatherSimulation` gains a public `Reseed(Season, const WeatherParameters&, const IBiomeSeasonalBaseline&)`
  and `ClearForcings()` (removes every forcing, keeps registered cell sets). `Reseed` is a thin public
  wrapper over Feature 1's **private** `SeedFields(Season, ...)`; `Initialise` already calls
  `SeedFields(Season::Spring, ...)` and is not changed, so no Feature 1 refactor is needed (Feature 1
  amendment 5). `Reseed` also resets the week counter to 0 (`SetCommittedWeek(0)`) and re-zeroes
  non-seeded fields. `ClearForcings` is declared in the system spec as Feature 2's API (amendment 5): if
  Feature 2 built it, this feature only calls it; otherwise Task 8 adds it (Feature 2's amendment 3 says
  it arrives with Feature 6).
- `WeatherPresetApply.h/.cpp`: `PresetResult ApplyPreset(WeatherSimulation&, const WeatherPreset&,
  const WeatherParameters&, const IBiomeSeasonalBaseline&)`.
- Order inside the call: (1) precondition (no step in flight: debug assertion, release returns an error,
  neither mutates); (2) **validate everything first** against the simulation's terrain: every used
  selector resolves to at least one cell, forcing count after the call is at most 64, cell sets at most
  16; any failure returns before any mutation (atomicity, AC 10); (3) `ClearForcings`, `ClearCellSets`,
  `Reseed(preset.season, ...)`; (4) fills in file order (later overwrites earlier); (5) `RegisterCellSet`
  per forcing selector then `AddForcing` in order; (6) return the season. `ApplyPreset` does not own the
  clock.
- Forcing field mapping uses `WeatherForcing { kind, cell = 0, cellSet, value, value2, weeksRemaining }`:
  `WindOverride` `value` = direction in radians in [0, 2pi) (absolute angle converted, or the resolved
  inland/seaward direction), `value2` = speed in [0, 1], `weeksRemaining` = `weeks` with 0 = until cleared
  (Feature 2 amendments 1 and 2). `coastal_storm` (6 weeks) and `drought` (8 weeks) wind overrides expire.
  An unresolvable `"inland"`/`"seaward"` (zero vector) fails in the validate step of `ApplyPreset`.

### Presets (Task 12)
- Five files `Assets/CoW/Global/Weather/Presets/<name>.weatherpreset.json`, content per the spec's preset
  table. Numbers are starting points, tuned by Task 18; they live in data so tuning needs no code
  change.

### Regression scenarios (Tasks 13 to 17)
- `Tests/GoogleTests/CoW/Weather/TestWeatherRegression.cpp`, serial executor, `MakeCoastRange(40, 24)`,
  default parameters. Each test computes the GDD ratio from the builder's named lists, **prints the
  measured ratio** and a warning if the margin is under 10% (AC 16), then asserts the threshold.
- "Mean precipitation" in every ratio is the mean over every cell in the named set **and over every week
  of the scenario** (all 52 weeks for coastal rain, all 13 for rain shadow); amendment 2.
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
- `Tests/GoogleTests/CoW/Golden/weather_layers.golden.json` is **owned by this feature** (amendment 3).
  Feature 2's entries use the `pipeline/` key prefix; whichever feature lands first creates the file.
  Hashes via `CellLayerGolden.h` `HashLayer` and `GoldenStore::Check`; 5 presets x 9 fields = 45
  hashes on `MakeCoastRange(66, 42)` ("Small" in the system spec means this size) after 13 weeks, serial
  executor. Hashes compare bit for bit, so Debug and Release need `/fp:precise` with no fast-math in both
  (verified by Feature 2 Task 0; if configurations still differ, one hash set per configuration). Update only with
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
| 0 | Prerequisite check, read only: Feature 1 Done; Feature 2 built (for tasks 8 onward); the `WaterSource` and 13 `Biome` enum names and ids; `HexTopology` API for BFS; `CellLayerGolden.h` API (`HashLayer`, `GoldenStore::Check`) and whether the golden file already exists from Feature 2's golden task; how `WeekOf`/`SeasonOf` are exposed; whether Feature 2 already built `ClearForcings()`; that Feature 2's `WindOverride` mapping matches amendment 2 (`value` radians, `value2` speed, `weeksRemaining` 0 = until cleared); that Feature 2 Task 0 confirmed `/fp:precise` in both configurations | Findings recorded in notes | Not Started | haiku | TDD-exempt, mechanical. Report which tasks are unblocked now |
| 1 | Create empty `WeatherPreset`, `WeatherPresetApply`, `Testing/WeatherTestTerrains` file pairs and the three test files `TestWeatherPreset`, `TestWeatherPresetApply`, `TestWeatherRegression`; register with `dia docs vcxproj-add CoW ...` and `dia docs vcxproj-add GoogleTests ...`; create the `Presets/` asset folder | Solution builds | Not Started | haiku | Mechanical, TDD-exempt |
| 2 | RED then GREEN: `WeatherPreset` types and `LoadWeatherPreset` valid path with inline JSON, no files | AC 2, 3 (first half): selectors with each clause type, fills, forcings, `angleDegrees` converted to radians, `direction` `"inland"`/`"seaward"` parsed, `season` parsed; nothing touches a `WeatherSimulation` | Not Started | sonnet | Strict, all-or-nothing, Dia strings only. The five shipped files are loaded in Task 12 |
| 3 | RED then GREEN: loader rejection table | AC 1, 2: each edit is rejected with the offending key named: unknown top-level key, unknown clause, unknown selector name, `min > max`, field value out of range, unknown field, unknown forcing kind, wrong `format`, wrong `version`, `weeks` missing on a non-wind forcing; a wind override with both `angleDegrees` and `direction`, with neither, and with `direction` other than `"inland"`/`"seaward"`; on failure no `WeatherPreset` produced | Not Started | sonnet | Table-driven over a valid base document with one edit per row. Commit |
| 4 | RED then GREEN: `ComputeDistanceToWater` | AC 4: 0 on every water-source cell; +1 per hex step on a hand-built row, odd and even columns; corner-only water gives the expected maximum | Not Started | sonnet | Depends only on Feature 1 and `HexTopology` |
| 5 | RED then GREEN: `ResolveSelector` | AC 5, 6 (selector half): clauses AND; `waterSource`, `elevation`, `latitude` and `distanceToWater` boundaries inclusive; `"all"`; output sorted and de-duplicated; empty result reported so `ApplyPreset` can name the selector | Not Started | sonnet | The "fails with the selector named" half of AC 6 is proven in Task 9. Commit tasks 4 and 5 |
| 6 | RED then GREEN: `ResolveInlandDirection` and the preset `direction` clause | AC 13 (direction half), 6: `"inland"` at each selector cell sums over its six neighbours `unitVector(d) * (distance(n) - distance(c))`, sums over the selector's cells and snaps with `SnapWindDirection`; `"seaward"` is the opposite direction; a zero vector (e.g. a selector with no gradient) returns an error "no inland direction"; hand-built ocean-on-west row gives east (inland) and west (seaward); `kNoCell` neighbours contribute nothing; loader accepts `direction` xor `angleDegrees` on a `WindOverride` and rejects both or neither | Not Started | sonnet | Pure function over `ComputeDistanceToWater` output and Feature 2's `WindDirection`; needs Feature 2 Task 3 (`SnapWindDirection`, unit vectors) Done. Closes spec Open Q3 (amendment 1). `ApplyPreset` surfaces the zero-vector error in its validate-first step with no mutation |
| 7 | RED then GREEN: `MakeCoastRange(width, height)` and its named cell lists | Shape tests: deterministic (two calls byte-identical), `IsConsistent()`, ocean about a third, ridge column near 60%, lists sorted and in range, windward and leeward on opposite sides of the ridge, 66 x 42 and 40 x 24 both valid; every biome id is in the 13-biome enum | Not Started | sonnet | No spec AC; the tests are derived from the "Test terrains" section. Needs no `Step`, so can run before Feature 2. Commit |
| 8 | RED then GREEN: public `WeatherSimulation::Reseed(Season, ...)` as a wrapper over Feature 1's private `SeedFields`, and `ClearForcings()` unless Feature 2 already built it | AC 7 (reseed half), 9 (week 0), 12 (idempotence): reseed after 20 weeks equals a fresh `Initialise` for Spring; reseed with Winter seeds winter baseline; week counter 0; forcings cleared | Not Started | sonnet | Edits Feature 1/2's class. `Initialise` is unchanged (it already calls `SeedFields(Season::Spring, ...)`), so there is no refactor; still run `WeatherInitialise*` with the new tests. Needs Feature 2 for the week counter and forcing storage. `ClearForcings` removes forcings and keeps cell sets |
| 9 | RED then GREEN: `ApplyPreset` validation and atomicity | AC 6, 10, 11: a selector used by a fill or forcing that resolves to 0 cells fails with the selector named; a preset exceeding 64 forcings or 16 cell sets fails and leaves the simulation byte-identical; a captured-but-uncommitted step returns an error without mutating | Not Started | sonnet | Validate first, mutate after (see Implementation Patterns). Use `memcmp` on every field plus forcing and set counts before and after |
| 10 | RED then GREEN: `ApplyPreset` reset and fills | AC 7, 8, 9, 12: state bit-identical whether the sim was fresh, ran 20 weeks first, or held earlier forcings (three cases); fills in file order with later overwriting earlier; unnamed fields hold the preset-season seed values; week 0; season returned; no forcing from a prior preset; applying the same preset twice is identical | Not Started | sonnet | Uses inline test presets, not the shipped files. Commit |
| 11 | RED then GREEN: `ApplyPreset` forcings via cell sets | AC 13, 14: a `WindOverride` covers exactly its selector's cells (wind there is the override, outside is computed); an until-cleared forcing outlives 52 weeks; a timed forcing, wind overrides included, expires after its `weeks`; a wind override with `direction: "inland"` resolves to the Task 6 direction; an unresolvable direction fails in the validate step with no mutation | Not Started | sonnet | Wind override mapping: `value` radians, `value2` speed, `weeksRemaining` = `weeks` (0 = until cleared). Inland/seaward directions come from Task 6; a timed wind override (`coastal_storm` 6 weeks, `drought` 8 weeks) expires like any other forcing. Commit |
| 12 | Author the five preset files from the spec's content table; add a test that loads each shipped file | AC 1 (first sentence), 17 (loading half): all five files load; names match filenames | Not Started | sonnet | Numbers are starting points. `coastal_storm` and `drought` wind overrides use `"direction": "inland"`; `rain_shadow` keeps an absolute angle. No code change is allowed to compensate for a data value |
| 13 | RED then GREEN: regression scenario 1, coastal rain | AC 15, 16: mean precipitation(coastal) over mean precipitation(inland), each averaged over the set's cells and all 52 weeks, above 1.5 on `MakeCoastRange(40, 24)`, default parameters, Spring start, 52 weeks; ratio printed; under 10% margin warns | Not Started | sonnet | Integration test, serial executor. If it fails, enlarge the fixture first (Open Q1) before the tuning task (Task 18) |
| 14 | RED then GREEN: regression scenario 2, rain shadow | AC 15, 16: `rain_shadow`, 13 weeks, mean precipitation(windward) over leeward, each averaged over the set's cells and all 13 weeks, above 2.0 | Not Started | sonnet | A wide-margin failure is more likely the orographic pass than `cloud_decay`: a mechanic flag to the GDD owner, not a tuning change |
| 15 | RED then GREEN: regression scenario 3, snow accumulation | AC 15, 16: `winter_snow`, 13 Winter weeks, more than 70% of cells with elevation above 0.6 have `snowPack` above 0.1 | Not Started | sonnet | Percentage and margin printed |
| 16 | RED then GREEN: regression scenario 4, spring melt | AC 15, 16: `winter_snow`, 13 Winter weeks to accumulate, then 13 Spring weeks; final snow under 30% of accumulated; lowland (elevation under 0.4) `groundWater` sum over 0.5 of accumulated snow sum | Not Started | sonnet | The accumulation step is deviation D-6.2. Accumulated snow is measured after the Winter weeks, not before. Commit tasks 13 to 16 |
| 17 | RED then GREEN: all-presets health run and executor equivalence | AC 17, 18: all five presets run 13 weeks with no NaN or infinity and every field in range; serial executor and a 3-range executor give identical bytes (once, at preset level) | Not Started | sonnet | Reuse Feature 2's `FixedRangeExecutor` |
| 18 | Tuning pass (conditional): if any of Tasks 13 to 17 fails its ratio, apply allowed levers in order and log each change in the spec Status section Tuning log (parameter, old, new, forcing test); re-run the scoped regression suite after each lever | AC 15 green on all four scenarios | Not Started | sonnet; ask before escalating | One lever per iteration, one change at a time (debug skill); three failed attempts on the same ratio stop and report. Stop and flag for the GDD owner before touching any of the 20 GDD constants or any mechanic. Record "no tuning needed" if nothing fails. Commit |
| 19 | RED then GREEN: layer goldens, 5 presets x 9 fields on `MakeCoastRange(66, 42)` after 13 weeks | AC 19: 45 hashes match; a deliberately changed constant in a scratch run is reported by preset, layer, changed-cell count and first cell | Not Started | sonnet | Run after Task 18 so the Tuning log is final. First write with `DIA_UPDATE_GOLDEN=1` once, then review the diff summary only. This feature owns the file; Feature 2's `pipeline/` entries may already be there (add, never overwrite them). Debug and Release must agree under `/fp:precise` with no fast-math (Feature 2 Task 0); if they do not, follow its per-configuration decision. Commit |
| 20 | Performance: `ApplyPreset` at Large (6,656 cells) in Release | AC 20: under 5 ms reported; not gated in Debug | Not Started | sonnet | Reported, not enforced. If the distance-field BFS dominates, note it as a suggestion; cache it only if the user asks |
| 21 | Exhaustive test pass: gap audit of AC 1 to 20, then edge cases | Gap list in notes. Edge cases: a selector matching every cell; a selector matching one cell; a preset with no fills; a preset with no forcings; a fill naming the same field twice; a forcing selector equal to another's (two sets, same cells); `ApplyPreset` on a 1-cell-wide terrain; preset applied when the sim has 63 or 64 forcings already; distance field on a terrain with water in one corner only; boundary values `min == max` | Not Started | sonnet | No mutation check. Add only the missing tests |
| 22 | Observation Opportunity Scan over every file touched across logs (preset load failure and apply), traces, profiling (`ApplyPreset`), metrics, health | Suggestions in notes | Not Started | haiku | Non-blocking. Feeds Feature 5 (`cow.weather.load_preset`) |
| 23 | One full verification: `dia run googletest --shards 2`; `dia check deps`; `dia docs precommit`; Release `ApplyPreset` timing reported | All AC 1 to 20 quoted; pass/fail and failing names only | Not Started | haiku | One full run. Report the printed margins for the four scenarios |
| 24 | Close-out: `dia docs plan` rows to Done with notes, then `dia docs spec-done docs/specs/applications/cow/systems/a4-weather/presets-and-regression.md` | Spec status Done, plan header updated, Tuning log recorded in the spec | Not Started | haiku | **Do not run until every row above is Done.** Not to be run when this plan is written |

## Open items

1. **Direction for `coastal_storm` and `drought`.** Resolved by amendment 1 (2026-11-16): `"inland"` / `"seaward"` from the summed distance-to-water gradient snapped with `SnapWindDirection`; both presets use `"inland"`; zero vector is invalid (Task 6).
2. **`WeatherForcing` has no angle field.** Resolved by Feature 2 amendment 2 as referenced by amendment 4 (2026-11-16): `value` = radians, `value2` = speed.
3. **`WindOverride` timers disagree between specs.** Resolved by amendment 4 (2026-11-16): `weeks` 0 = until cleared, positive expires; `coastal_storm` and `drought` wind overrides expire and AC 14 holds.
4. **`ClearForcings()` ownership.** Resolved by amendment 5 (2026-11-16) as a documented API of Feature 2; Task 8 only adds it if Feature 2 has not built it. Genuinely unclear which feature writes it (Feature 2's own amendment 3 says "added with Feature 6"); Task 0 checks and the first to land adds it.
5. **Golden file ownership and "Small".** Resolved by amendment 3 (2026-11-16): this feature owns the file, Feature 2 uses the `pipeline/` prefix, and Small = `MakeCoastRange(66, 42)`.
6. **Meaning of "season" for the regression run.** `ApplyPreset` returns the season and does not own the clock; the tests advance it with `WeekOf`/`SeasonOf`. Whether a `startWeekOffset` is wanted here is still open (confirm; assumed not).
7. **"Mean precipitation" definition.** Resolved by amendment 2 (2026-11-16): mean over every cell in the set and every week of the scenario.
