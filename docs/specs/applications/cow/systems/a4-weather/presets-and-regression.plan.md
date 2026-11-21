# Plan: A4 Presets & Regression (A4 Weather Feature 6)

**Spec:** @docs/specs/applications/cow/systems/a4-weather/presets-and-regression.md
**Status:** Not Started
**System plan:** @docs/specs/applications/cow/systems/a4-weather/a4-weather.plan.md (row 6)

Rebuilt 2026-11-18 against the world-sim review amendment and the spec-6 resolutions. What changed:
- **Selectors are `CellSelectorSet`** (DiaCondition `ConditionExpr` over `WorldCellFields`, with `derived`
  fields). The A4 selector struct, `ResolveSelector` and the hand-built `ComputeDistanceToWater` with its test are
  **deleted**; preset selectors are written as `ConditionExpr` JSON, and `distanceToWater` is a derived
  `distance {from: water}` plus `>=` / `<=`.
- **Preset parameters are packs**, applied lenient and atomic through `ParameterFile::ApplyPack`.
- **Preset files** use format `cow.weather_preset` at `Global/Weather/Presets/<id>.weather_preset.json`
  (not `.weatherpreset.json`), catalogued and resolved through `PathStore`.
- **Biomes** are `BiomeIndex` with registry tags and roles; the test terrain names no biome.

**Dependencies.**
- Loader, `ApplyPreset` shell and test-terrain tasks need A4 Feature 1 Done (`weather-data.plan.md`), plus
  `worldsim-kit.plan.md` (World Step; `CellSelectorSet`, `WorldCellFields`), `grid-search-helpers.plan.md` (Dia;
  `HexNeighbours`, `MultiSourceSteps`, used by the kit's derived fields and by the inland-direction gradient),
  `biome-registry.plan.md` (S-1; tags, `BiomeTestRegistry.h`) and `parameter-files.plan.md` (Dia; `ApplyPack`).
- Tasks from `Reseed` onward (forcings, regression scenarios, goldens, performance) need A4 Feature 2 **built**
  (`ca-pipeline.plan.md`): `Step`, `AddForcing`, `RegisterCellSet`, `ClearCellSets`.
- `blackboard-scopes.plan.md` is not needed.

**Startable earlier on fakes.** The loader, preset types and rejection table (Tasks 2, 3) need no foundation code:
they parse JSON and call the kit's `Validate`, which has a fake registry. `MakeCoastRange` (Task 7) needs only
Feature 1 and the biome fixture. Selector resolution runs on the kit's `WorldCellFields` over hand-built terrain
once the kit exists.

All code is CoW (application): `Cluiche/CoW/Shared/Weather/` and `.../Testing/`. `CellLayerGolden.h` is built Dia
code and only consumed. No Dia edits (AD-001). Tasks that could need more than sonnet say "ask before escalating"
(feedback_confirm_opus).

## Implementation Patterns

### Preset data and loader (Tasks 2, 3)
- `Cluiche/CoW/Shared/Weather/WeatherPreset.h/.cpp`. `WeatherPreset { name; Season season; CellSelectorSet
  selectors (the preset's own `derived` and `selectors` block, kit format); fills; forcings; optional climate;
  optional parameters pack }`, all `DynamicArrayC` and Dia strings (PD-004).
- Selectors are `ConditionExpr` JSON exactly as the kit defines them; `all` is predefined. Example:
  `"selectors": { "high": { "op": ">=", "slot": "terrain", "field": "elevation", "value": 0.6 } }`. A4 writes **no
  clause grammar** of its own. Validation at load is the kit's (`ConditionExpr::Validate(IConditionContext&)`,
  derived-field cycles, `maxSteps`), plus A4's: a selector name used by a fill or forcing must exist.
- `PresetFill { field (enum of the ten fields); value; selector name }`, range-checked per field ([0, 1];
  `windDirection` [0, 2pi); `pressureAnomaly` [-0.5, 0.5]). Ranges come from the weather layer sidecar entry for the
  field, not a local table. `PresetForcing { ForcingKind; selector name; WindDirectionSpec (Absolute | Inland |
  Seaward); angleRadians; speed; weeks }`: a `WindOverride` takes either an absolute `angleDegrees` (converted at
  load) **or** a `direction` of `"inland"` or `"seaward"`, never both and never neither; `weeks == 0` is allowed only
  for `WindOverride`.
- Optional `climate` block (cycle id to `{enabled, pinned_phase_days}`) and optional `parameters` (a sparse pack
  object; its `format` is the weather sidecar's `for`).
- `bool LoadWeatherPreset(const Json::Value&, WeatherPreset& out, PresetError& err)`: strict like the kit's loaders
  (unknown key invalid at every level, all-or-nothing); the `parameters` pack alone is lenient and applied through
  `ApplyPack` at apply time. Build into a local and move on success (AC 2). Touches no `WeatherSimulation` (AC 3).

### Inland / seaward direction (Task 6)
- `"inland"` is the on-screen direction of increasing distance from water. The distance field comes from
  `Dia::Pathfinding::MultiSourceSteps` over `HexNeighbours` seeded with every water-source cell (the same helper the
  kit's `distance` derived field uses), **not** a hand-written BFS. For each selector cell `c` the gradient is the sum
  over its six neighbours `n` of `unitVector(d) * (distance(n) - distance(c))`; the vectors are summed over the
  selector's cells and snapped once with `SnapWindDirection`. `"seaward"` is the opposite. A zero vector makes the
  preset invalid ("no inland direction"), reported in `ApplyPreset`'s validate-first step.

### Test terrain (Task 7)
- `Cluiche/CoW/Shared/Weather/Testing/WeatherTestTerrains.h/.cpp` (ships with the library). `MakeCoastRange(width,
  height)` is deterministic, no RNG: west third ocean, one-cell coast, lowland plain, north-south ridge ramping to
  1.0 at about 60% of the width, leeward interior at elevation 0.3, no lakes. **Biome per cell is chosen from
  elevation and `waterSource` through registry roles and tags (`ocean` role, `fallback` role, a `forest` tag, a
  `mountain` tag) via `BiomeTestRegistry.h`; no biome id is named.** Latitude ramps with `y`. It also returns the named
  cell lists (`coastal, inland, windward, leeward, mountain, lowland`) and a `WorldCellFields` over the terrain
  (`terrain.*`, `biome.index`, `biome.tag.<tag>`) so selectors run on it. Sizes: 66 x 42 (goldens), 40 x 24 (fast).

### Reseed and ApplyPreset (Tasks 8 to 11)
- `WeatherSimulation` gains public `Reseed(Season, const WeatherParameters&, const IBiomeSeasonalBaseline&)` (a thin
  wrapper over Feature 1's private `SeedFields`) and `ClearForcings()` (keeps cell sets; unless Feature 2 built it).
- `PresetResult ApplyPreset(WeatherSimulation&, const WeatherPreset&, const WeatherParameters&, const
  IBiomeSeasonalBaseline&, const CellSelectorSet& resolved, ParameterFile* params = nullptr, InfluenceBook* book =
  nullptr, ClimateCycles* climate = nullptr)`; null optional pointers in pure tests.
- Order: (1) precondition (no step in flight); (2) **validate everything first**: every used selector resolves to at
  least one cell (consumer's check), forcing count after the call at most 64, cell sets at most 16, an unresolvable
  inland direction, an unknown cycle id, and a parameter pack that would break a constraint; any failure returns
  before any mutation; (3) `ClearForcings`, `ClearCellSets`, `Reseed(preset.season, ...)`, `book->Clear()` and
  `climate->DisableAll()` when passed; (4) the parameter pack through `ApplyPack` when present; (5) fills in file
  order; (6) `RegisterCellSet` per forcing selector then `AddForcing` in order, then the `climate` block; (7) return
  the season.
- Forcing mapping uses `WeatherForcing { kind, cell = 0, cellSet, value, value2, weeksRemaining }`: `WindOverride`
  `value` radians in [0, 2pi), `value2` speed in [0, 1], `weeksRemaining` = `weeks` with 0 = until cleared. A timed
  wind override (`coastal_storm` 6 weeks, `drought` 8 weeks) expires.

### Presets (Task 12)
- Five files `Assets/CoW/Global/Weather/Presets/<name>.weather_preset.json` per the spec's content table, with
  selectors written as `ConditionExpr` and `derived` fields (for example `dist_water` as `distance {from: water}`).
  Numbers are starting points, tuned in Task 18 and kept in data. None carries a `parameters` block (presets are
  scenarios, not packs).

### Regression scenarios (Tasks 13 to 17)
- `Tests/GoogleTests/CoW/Weather/TestWeatherRegression.cpp`, serial executor, `MakeCoastRange(40, 24)` for the fast
  tests and **66 x 42 for the coastal 1.5x ratio** (system open-items decision 3), default parameters (loaded from the
  shipped sidecar through `ParameterFile` and `Snapshot`). Each test computes the GDD ratio from the builder's named
  lists, prints the measured ratio and a warning if the margin is under 10% (AC 16), then asserts the threshold.
- "Mean precipitation" is the mean over every cell of the set and every week of the scenario.
- Scenarios exactly as the spec table (coastal rain, rain shadow, snow accumulation, spring melt with the 13-week
  winter accumulation step, deviation D-6.2). The test advances the season with `WeekOf`/`SeasonOf`.

### Tuning (Task 18)
- Allowed levers in order: preset data values; the **sidecar's baseline map defaults** (`weather_parameters.schema.json`
  per-season columns; a default edit, not a code edit); `cloud_decay` (a sidecar default); then stop and flag to the
  GDD owner. Every change is appended to the spec's Tuning log (parameter, old, new, test) and a changed default is a
  new GDD deviation D-6.4. Enlarge the fixture first (spec Open Q1).

### Goldens (Task 19)
- `Tests/GoogleTests/CoW/Golden/weather_layers.golden.json` is **owned by this feature**; Feature 2's entries use the
  `pipeline/` prefix and whichever lands first creates the file. `CellLayerGolden.h` `HashLayer`/`GoldenStore::Check`;
  5 presets x 10 fields = 50 hashes on `MakeCoastRange(66, 42)` after 13 weeks, serial executor. The selector rewrite
  must not change any selected set: Task 5 pins each preset's selected set (count and first and last cell) before
  the goldens are recorded, and **the 50 hashes are re-recorded only if a set changed, with the reason in notes**
  (amendment 5). Update only with `DIA_UPDATE_GOLDEN=1`, after the Tuning log is final.

### Verification cadence
- Per task: narrow `dia run googletest --filter="WeatherPreset*:WeatherTestTerrain*" --shards 2` (always `--shards 2`).
  Regression and golden suites run serial and slower (`WeatherRegression*`, `WeatherPresetGolden*`).
- The feature-wide `--filter="Weather*"` runs only in the scoped-verification task; the full suite and the Release
  check run once in worldstep.plan.md Weather-stack close-out. No `SLOW_` suites (longest run is 52 weeks). No
  sanitizer unless asked. No mutation checks. The AC 20 timing is a Debug ballpark only. Verification uses cell data and
  printed ratios, not screenshots.

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 0 | Prerequisite check, read only: Feature 1 Done; Feature 2 built (for Task 8 onward); from `worldsim-kit.plan.md` `CellSelectorSet` (load, validate, resolve, caps 32 selectors and 16 derived fields), `WorldCellFields` (and how a test registers `terrain.*` fields over hand-built terrain), `ConditionExpr::Validate(IConditionContext&)`; from `grid-search-helpers.plan.md` `MultiSourceSteps` and `HexNeighbours`; from `biome-registry.plan.md` `BiomeTestRegistry.h` and tag ids; from `parameter-files.plan.md` `ApplyPack`; `CellLayerGolden.h` API and whether the golden file already exists; whether Feature 2 built `ClearForcings()`; how `WeekOf`/`SeasonOf` are exposed; that Feature 2 Task 0 confirmed `/fp:precise` in both configurations. Record whether derived-field values are readable outside a selector (open item 2) | Findings recorded in notes | Not Started | haiku | TDD-exempt, mechanical. Report which tasks are unblocked now. If a foundation name differs, record it; do not reimplement |
| 1 | Create empty `WeatherPreset`, `WeatherPresetApply`, `Testing/WeatherTestTerrains` file pairs and the three test files `TestWeatherPreset`, `TestWeatherPresetApply`, `TestWeatherRegression`; register with `dia docs vcxproj-add CoW ...` and `dia docs vcxproj-add GoogleTests ...`; create the `Presets/` asset folder; add the catalogue entries | Solution builds | Not Started | haiku | Mechanical, TDD-exempt |
| 2 | RED then GREEN: `WeatherPreset` types and `LoadWeatherPreset` valid path with inline JSON, no files | AC 2, 3 (first half): selectors as `ConditionExpr` JSON with a `derived` field, fills, forcings, `angleDegrees` converted to radians, `direction` `"inland"`/`"seaward"` parsed, `season` parsed, the optional `climate` and `parameters` blocks parsed; nothing touches a `WeatherSimulation` | Not Started | sonnet | Strict, all-or-nothing, Dia strings only. Format `cow.weather_preset`. The five shipped files are loaded in Task 12 |
| 3 | RED then GREEN: loader rejection table | AC 1, 2: each edit is rejected with the offending key named: unknown top-level key, a selector that fails the kit's `Validate` (unknown slot or field), a fill or forcing naming an unknown selector, a derived-field cycle, `min > max` in a range value, field value out of range, unknown field, unknown forcing kind, wrong `format`, wrong `version`, `weeks` missing on a non-wind forcing; a wind override with both `angleDegrees` and `direction`, with neither, or a `direction` other than `"inland"`/`"seaward"`; a malformed `climate` block (non-object, unknown key, negative or non-integer `pinned_phase_days`); on failure no `WeatherPreset` produced | Not Started | sonnet | Table-driven over a valid base document, one edit per row. A4 asserts only that the kit's errors surface with the preset key named; the kit's own validation cases are not repeated. Commit |
| 4 | RED then GREEN: A4's `WorldCellFields` terrain fixture and the five presets' derived distance field | AC 4 (replaced): on a hand-built row, a preset's `dist_water` derived field (`distance {from: water}`) is 0 on every water-source cell and +1 per hex step, on odd and even columns, and corner-only water gives the expected maximum | Not Started | sonnet | Replaces the deleted `ComputeDistanceToWater` test. The helper is the kit's; this test only proves A4's preset block asks for it correctly |
| 5 | RED then GREEN: each shipped preset's selectors resolve to the expected sets on `MakeCoastRange` | AC 5, 6 (selector half): for every selector used in the five presets, the resolved cell list is sorted, de-duplicated, equals the builder's hand-derived list or a recorded count with first and last cell, and clauses AND (`elevation`, `latitude` and distance boundaries inclusive); a selector used by a fill or forcing that resolves to 0 cells is reported naming the selector | Not Started | sonnet | Pins the sets so the golden decision in Task 19 is mechanical. The "fails with the selector named" half of AC 6 is proven in Task 9. Needs Task 7. Commit tasks 4 and 5 |
| 6 | RED then GREEN: `ResolveInlandDirection` and the preset `direction` clause | AC 13 (direction half), 6: `"inland"` sums over the six neighbours `unitVector(d) * (distance(n) - distance(c))`, over the selector's cells, snapped with `SnapWindDirection`; `"seaward"` is the opposite; a zero vector returns "no inland direction"; hand-built ocean-on-west row gives east (inland) and west (seaward); `kNoCell` neighbours contribute nothing; the distance is the `MultiSourceSteps` result | Not Started | sonnet | Pure function over the helper's output and Feature 2's `WindDirection`; needs Feature 2's `SnapWindDirection` and unit vectors. `ApplyPreset` surfaces the zero-vector error in its validate-first step with no mutation. No hand-written BFS |
| 7 | RED then GREEN: `MakeCoastRange(width, height)`, its named cell lists and its `WorldCellFields` | Shape tests: deterministic (two calls byte-identical), `IsConsistent()`, ocean about a third, ridge column near 60%, lists sorted and in range, windward and leeward on opposite sides of the ridge, 66 x 42 and 40 x 24 both valid; every biome index is valid in the fixture registry and chosen through roles and tags, a grep test finds no biome id literal in the builder; the fields registered evaluate `terrain.elevation >= 0.6` to the `mountain` list | Not Started | sonnet | No spec AC; tests derived from the "Test terrains" section. Needs no `Step`, so can run before Feature 2. Commit |
| 8 | RED then GREEN: public `WeatherSimulation::Reseed(Season, ...)` as a wrapper over Feature 1's private `SeedFields`, and `ClearForcings()` unless Feature 2 built it | AC 7 (reseed half), 9 (week 0), 12 (idempotence): reseed after 20 weeks equals a fresh `Initialise` for Spring; reseed with Winter seeds winter baseline; week counter 0; forcings cleared; `pressureAnomaly` reseeds to 0 | Not Started | sonnet | Edits Feature 1/2's class. `Initialise` is unchanged; run `WeatherInitialise*` with the new tests. Needs Feature 2 for the week counter and forcing storage |
| 9 | RED then GREEN: `ApplyPreset` validation and atomicity | AC 6, 10, 11: a selector used by a fill or forcing that resolves to 0 cells fails with the selector named; a preset exceeding 64 forcings or 16 cell sets fails and leaves the simulation byte-identical; a captured-but-uncommitted step returns an error without mutating; an unresolvable inland direction fails with no mutation; a `parameters` pack that breaks a sidecar constraint rejects the whole preset and changes nothing | Not Started | sonnet | Validate first, mutate after. `memcmp` on every field plus forcing and set counts and the `ParameterFile` hash before and after |
| 10 | RED then GREEN: `ApplyPreset` reset, fills and the parameter pack | AC 7, 8, 9, 12: state bit-identical whether the sim was fresh, ran 20 weeks first, or held earlier forcings; fills in file order with later overwriting earlier; unnamed fields hold the preset-season seed values; week 0; season returned; no forcing from a prior preset; the same preset twice is identical; a fixture preset with a `parameters` pack changes only its keys through `ApplyPack` (a clamped key warns, an unknown key is dropped with a warning) | Not Started | sonnet | Uses inline test presets, not the shipped files. See open item 3 for what a following preset without a pack does to the pack's values. Commit |
| 10a | RED then GREEN: `ApplyPreset` clears the influence book and disables every cycle, plus the preset `climate` block | AC 7a, 7b: the state is bit-identical with earlier influences in the book and with cycles enabled or pinned beforehand; the book is empty after the call (one Info) and every cycle is disabled unless the `climate` block enables or pins it (`pinned_phase_days` gives the pinned wave on every day); an unknown cycle id fails in the validate-first step naming the id and changes nothing; a malformed block is rejected at load naming the key; null `book`/`climate` behaves as before | Not Started | sonnet | Needs Feature 7's `InfluenceBook` and Feature 8's `ClimateCycles` (their test fakes if only headers exist). The loader half needs only this feature. Commit with Task 10 |
| 11 | RED then GREEN: `ApplyPreset` forcings via cell sets | AC 13, 14: a `WindOverride` covers exactly its selector's cells; an until-cleared forcing outlives 52 weeks; a timed forcing, wind overrides included, expires after its `weeks`; `direction: "inland"` resolves to the Task 6 direction | Not Started | sonnet | Mapping: `value` radians, `value2` speed, `weeksRemaining` = `weeks`. Commit |
| 12 | Author the five preset files at `Global/Weather/Presets/<name>.weather_preset.json` from the spec's content table, selectors as `ConditionExpr`; add a test that loads each shipped file through the catalogue and `PathStore` | AC 1 (first sentence), 17 (loading half): all five files load; names match filenames; each file's selectors validate against the kit's `WorldCellFields` | Not Started | sonnet | Numbers are starting points. `coastal_storm` and `drought` use `"direction": "inland"`; `rain_shadow` keeps an absolute angle. No code change may compensate for a data value |
| 13 | RED then GREEN: regression scenario 1, coastal rain | AC 15, 16: mean precipitation(coastal) over mean precipitation(inland), each over the set's cells and all 52 weeks, above 1.5 on **`MakeCoastRange(66, 42)`**, default parameters, Spring start; ratio printed; under 10% margin warns | Not Started | sonnet | Integration test, serial executor. 40 x 24 stays for the other fast scenarios. If it fails, enlarge the fixture first before Task 18 |
| 14 | RED then GREEN: regression scenario 2, rain shadow | AC 15, 16: `rain_shadow`, 13 weeks, mean precipitation(windward) over leeward, each over the set's cells and all 13 weeks, above 2.0 | Not Started | sonnet | A wide-margin failure is more likely the orographic pass (or its S-1 factor) than `cloud_decay`: a mechanic flag to the GDD owner, not a tuning change |
| 15 | RED then GREEN: regression scenario 3, snow accumulation | AC 15, 16: `winter_snow`, 13 Winter weeks, more than 70% of cells with elevation above 0.6 have `snowPack` above 0.1 | Not Started | sonnet | Percentage and margin printed |
| 16 | RED then GREEN: regression scenario 4, spring melt | AC 15, 16: `winter_snow`, 13 Winter weeks to accumulate, then 13 Spring weeks; final snow under 30% of accumulated; lowland (elevation under 0.4) cumulative `waterToGround` over the 13 Spring weeks over 0.5 of accumulated snow sum | Not Started | sonnet | Accumulation step is deviation D-6.2. Accumulated snow is measured after the Winter weeks. Commit tasks 13 to 16 |
| 17 | RED then GREEN: all-presets health run and executor equivalence | AC 17, 18: all five presets run 13 weeks with no NaN or infinity and every field in range; serial executor and a 3-range executor give identical bytes (once, at preset level) | Not Started | sonnet | Reuse Feature 2's `FixedRangeExecutor` |
| 18 | Tuning pass (conditional): if any of Tasks 13 to 17 fails its ratio, apply the allowed levers in order and log each change in the spec Status Tuning log; re-run the scoped regression suite after each lever | AC 15 green on all four scenarios | Not Started | sonnet; ask before escalating | One lever, one change at a time. Three failed attempts on the same ratio stop and report. The baseline lever edits the sidecar's map defaults. Stop and flag for the GDD owner before touching any of the 19 GDD constants or a mechanic. Record "no tuning needed" if nothing fails. This row edits the spec's Tuning log only |
| 19 | RED then GREEN: layer goldens, 5 presets x 10 fields on `MakeCoastRange(66, 42)` after 13 weeks | AC 19: 50 hashes match; a deliberately changed constant in a scratch run is reported by preset, layer, changed-cell count and first cell | Not Started | sonnet | Run after Task 18 so the Tuning log is final. First write with `DIA_UPDATE_GOLDEN=1` once, then review the diff summary only. Re-record only if Task 5's pinned sets changed. Feature 2's `pipeline/` entries may already be there (add, never overwrite). Debug and Release agree under `/fp:precise` or follow Feature 2 Task 0's per-configuration decision |
| 20 | Performance: `ApplyPreset` at Large (6,656 cells), Debug ballpark | AC 20: under 5 ms target reported as a Debug ballpark; never gated | Not Started | sonnet | Reported, not enforced. If selector resolution dominates, note it as a suggestion |
| 21 | Exhaustive test pass: gap audit of AC 1 to 20 (amended set), then edge cases | Gap list in notes. Edge cases: a selector matching every cell; one cell; a preset with no fills; no forcings; a fill naming the same field twice; two forcing selectors with the same cells (two sets); a 1-cell-wide terrain; a sim already holding 63 or 64 forcings; a derived field with water in one corner only; a selector over the 32-selector cap; a preset naming a tag the registry lacks | Not Started | sonnet | No mutation check. Add only the missing tests |
| 22 | Observation Opportunity Scan over every file touched across the five DiaObservation pillars | Suggestions in notes | Not Started | haiku | Non-blocking. Feeds Feature 5 (`cow.weather.load_preset`) |
| 23 | Scoped verification: `dia run googletest --filter="Weather*" --shards 2`; `dia check deps`; `dia docs precommit` (includes `dia validate schema` for the preset files); Debug `ApplyPreset` timing reported | All amended AC 1 to 20 quoted; pass/fail and failing names only | Not Started | haiku | Feature-wide scoped run; the full suite and Release are in the worldstep.plan.md Weather-stack close-out. Report the printed margins for the four scenarios |
| 24 | Close-out: `dia docs plan` rows to Done with notes, then `dia docs spec-done docs/specs/applications/cow/systems/a4-weather/presets-and-regression.md` | Spec status Done, plan header updated, Tuning log recorded in the spec | Not Started | haiku | **Do not run until all rows above are Done.** |

**AC coverage (old plan 26 rows, 0-24 plus 10a; now 26 rows, 0-24 plus 10a, same count).** AC 1, 2, 3: Tasks 2, 3, 12. AC 4: Task 4 (replaced by the derived-field test; the helper test is deleted). AC 5, 6: Tasks 5, 9. AC 7, 8, 9, 10, 11, 12: Tasks 8 to 10. AC 7a, 7b: Task 10a. AC 13, 14: Tasks 6, 11. AC 15, 16: Tasks 13 to 16. AC 17, 18: Tasks 12, 17. AC 19: Task 19. AC 20: Task 20. Deleted: `ComputeDistanceToWater`, the hand-written `ResolveSelector` and its clause tests (the kit's AC 12 and 13 cover them). Task 7's old "13-biome enum" assertion is replaced by a registry-based one.

## Open items

1. **Direction for `coastal_storm` and `drought`.** Resolved by gap amendment 1 (2026-11-16): `"inland"` / `"seaward"`; zero vector invalid (Task 6).
2. **Distance values outside a selector.** The kit computes `distance` as a derived field "once at resolve time" for selectors; the inland-direction gradient needs the distance values themselves. The kit spec does not say derived values are readable by consumers. The plan calls `MultiSourceSteps` directly; if derived values are readable, this duplicates one helper call (Task 0 records it).
3. **Does a preset reset a parameter pack?** The spec says a preset reaches the same state whatever ran before, but `ApplyPack` mutates the `ParameterFile` base values, and the spec does not say whether `ApplyPreset` first reloads the file (`Reload` discards `Set` values and re-applies the active packs) or leaves earlier pack values in place. The five shipped presets carry no pack, so Task 10 tests the mechanism on a fixture only.
4. **Selector caps** (32 selectors, 16 derived fields per set) are the kit's; whether the five presets' `derived` fields fit is checked in Task 5. Cell-set caps (16) are Feature 2's.
5. **Presets use `ConditionExpr` JSON with `slot`/`field` pairs,** while the older spec's example shows named range clauses. The spec's preset file example is stale; the amendment wins and the plan follows it.
6. **Golden hash stability under the selector rewrite** is checked, not assumed (Task 5 pins sets, Task 19 decides). Whether `waterSource` cells should be `==` an enum ordinal or a tag is the kit's coverage table; the plan follows it.
7. **Meaning of "season" for the regression run.** `ApplyPreset` returns the season and does not own the clock; no `startWeekOffset` here (system decision 12).
- **Review 2026-11-16:** the coastal 1.5x ratio test uses the 66x42 fixture, 40x24 only for the fast tests (decision 3).
