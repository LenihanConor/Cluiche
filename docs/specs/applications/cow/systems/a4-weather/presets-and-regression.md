# Feature Spec: A4 Presets & Regression

**Parent:** @docs/specs/applications/cow/systems/a4-weather/a4-weather.md (Feature 6)
**Status:** Approved (2026-11-16)
**Design source:** `DesignDocs/COW/Output Docs/GDD/08_SystemsDeepDive/A4_Weather/A4_AuthoringDebug.md` §5 (presets), §6.1–6.4 (regression), `A4_TuningParameters.md` §12 (emergent pattern targets)
**Depends on:** A4 Feature 1 (types, parameters, `Initialise`, seeding); A4 Feature 2 (`Step`, forcings, `RegisterCellSet`). Both must be **built**, not only specced: the loader, predicates and fills need only Feature 1, but forcings and every regression test need Feature 2's `Step`.
**Consumed by:** A4 Feature 5 (`cow.weather.load_preset`), S-2.5 Feature 4 (scenarios reference presets by name), S-2.5 Feature 1 (curated worlds).

## Summary

The five GDD scenario presets as data files, a pure `ApplyPreset` that puts a `WeatherSimulation` into the preset's state, and the four GDD regression scenarios as GoogleTests over hand-built hex terrains with the GDD's success ratios. It also owns the shared hand-built test terrains and the A4 layer goldens.

The presets are **scenarios** (forced state), not weather packs (decided 2026-11-16, closing F-3 Open Q1). None changes a tuning constant. Packs remain the tuning path.

## Goals

1. Five preset files (`coastal_storm`, `rain_shadow`, `winter_snow`, `spring_melt`, `drought`) in a **region-predicate recipe** format that is world-independent: the same file runs on the hand-built test terrains and on S-2.5's `s25_coast_range`.
2. `ApplyPreset(WeatherSimulation&, const WeatherPreset&)` is deterministic, atomic, and reaches the same state regardless of what the simulation held before.
3. The four GDD regression scenarios pass with the GDD's ratios on the default parameters.
4. A shared, deterministic test-terrain builder (`MakeCoastRange`) that every later A4 and S-2.5 test can reuse.
5. A4's layer goldens exist, so later changes to the pipeline show up as a per-layer diff.

## Non-Goals

- The `cow.weather.load_preset` command, its help text and sim-hosted registration (Feature 5). This feature supplies `ApplyPreset` and the preset loader it calls.
- Applying a preset *at the next capture boundary* while a deferred step is in flight (Feature 3). `ApplyPreset` itself runs only when no step is in flight; the module schedules it.
- The S-2.5 scenario format, check lines and walkthrough (S-2.5 Feature 4). A scenario names a preset; it does not embed one.
- Any new weather mechanics. If a ratio fails, only the allowed tuning levers below may change.

## Design

### Preset file

`Assets/CoW/Global/Weather/Presets/<name>.weatherpreset.json`, envelope `{"format": "cow.weather_preset", "version": 1}`. Loaded through the same strict rules as Feature 1's parameter file (unknown key = invalid; all-or-nothing).

```json
{
    "format": "cow.weather_preset", "version": 1,
    "name": "rain_shadow",
    "season": "Autumn",
    "selectors": {
        "ranges": { "elevation": { "min": 0.6 } },
        "ocean":  { "waterSource": ["Ocean"] }
    },
    "fills": [
        { "field": "moisture", "value": 0.8, "where": "all" },
        { "field": "snowPack", "value": 0.0, "where": "all" }
    ],
    "forcings": [
        { "kind": "WindOverride", "where": "ranges", "angleDegrees": 270, "speed": 0.7, "weeks": 0 }
    ]
}
```

- **Selectors** are named predicates over the immutable terrain. Allowed clauses, AND-combined: `waterSource` (list of `None|Ocean|Lake|River`), `elevation` / `latitude` `{min, max}` (inclusive, [0, 1]), `distanceToWater` `{min, max}` in hex steps (multi-source breadth-first over the hex topology from every water-source cell; water cells are 0). `"all"` is predefined. An unknown clause, an unknown selector name, an empty-result selector used by a fill or forcing, or `min > max` is invalid.
- **Fills** set one `WeatherFields` field to a value on a selector's cells. Values are range-checked per field ([0, 1]; `windDirection` [0, 2π)). Fills apply in file order, so a later fill overwrites an earlier one.
- **Forcings** name a Feature 2 `ForcingKind` and a selector. The selector's cells are registered as one **cell set** and the forcing is one record (this is the Feature 2 amendment of 2026-11-16). `WindOverride` takes `angleDegrees` as an **on-screen counter-clockwise angle in y-down world space**, the same convention as the system spec's wind snap, and `speed` in [0, 1]; the loader converts to radians. `weeks` is 0 for "until cleared" on wind overrides, and required > 0 for the other kinds.
- **`season`** is returned to the caller. `ApplyPreset` does not own the clock (Feature 3 and the runner pass the season to `Step`).

### `ApplyPreset`

```cpp
struct PresetResult { bool ok; Season season; PresetError error; };   // error: key/path + reason, no STL types
PresetResult ApplyPreset(WeatherSimulation& sim, const WeatherPreset& preset,
                         const WeatherParameters& params, const IBiomeSeasonalBaseline& baseline);
```

Precondition: the simulation is initialised and **no step is in flight** (`Capture` called without `Commit` is an assertion failure in debug and a returned error in release).

1. **Validate** against the simulation's terrain: every selector resolves to at least one cell where used; forcing count after the call is ≤ 64; cell sets ≤ 16. Any failure returns `ok = false` and changes nothing.
2. **Reset** to a clean base: clear forcings and cell sets, then reseed fields exactly as `Initialise` does (Feature 1's seeding function, called with the **preset's season** instead of Spring), and reset the week counter to 0. Result: a preset reaches the same state whatever ran before it.
3. Apply fills in order, then register cell sets and add forcings in order.
4. Return the season.

`Reseed` is a new public method on `WeatherSimulation` that Feature 1's `Initialise` also calls internally.

### Preset content

| Preset | Season | Fills | Forcings |
|---|---|---|---|
| `coastal_storm` | Autumn | moisture 0.8 on ocean and `distanceToWater` ≤ 3; pressure 0.2 on ocean and `distanceToWater` ≤ 3 | `PressureHold` 0.2, 6 weeks, over the ocean selector within `distanceToWater` ≤ 3; `WindOverride` toward land (angle chosen per terrain: see Open Q3), 6 weeks |
| `rain_shadow` | Autumn | moisture 0.8 everywhere | `WindOverride` 270°, 0.7, until cleared, over the `elevation ≥ 0.6` selector widened by 4 hex steps (see Open Q3) |
| `winter_snow` | Winter | temperature from the seasonal baseline (reseed does this); moisture 0.7 on `elevation ≥ 0.4` | none |
| `spring_melt` | Spring | snowPack 0.8 on `elevation ≥ 0.6`; snowPack 0.0 elsewhere; groundWater 0.1 on `elevation < 0.4` | none |
| `drought` | Summer | moisture 0.05 on `distanceToWater ≥ 6`; groundWater 0.0 on `distanceToWater ≥ 6` | `PressureHold` 0.8, 8 weeks, over `distanceToWater ≥ 6`; `WindOverride` away from the nearest coast, 8 weeks (Open Q3) |

The numbers are starting points, tuned by the regression tests. They live in the data files, so tuning needs no code change.

### Test terrains (`Cluiche/CoW/Shared/Weather/Testing/WeatherTestTerrains.h`)

`WeatherTerrain MakeCoastRange(uint32_t width, uint32_t height)`: deterministic, no RNG. West third ocean, a one-cell-wide coast, a lowland plain, a north-south mountain ridge (elevation ramp to 1.0) at about 60% of the width, a leeward interior to the east with elevation 0.3, a lake-free interior. Biome per cell from elevation and `waterSource` (Ocean, GrasslandSteppe, TemperateForest, AlpineMountain). Latitude ramps with `y`. It also returns named cell lists (`coastal`, `inland`, `windward`, `leeward`, `mountain`, `lowland`) so tests read their sets from the builder, not from re-derived predicates. Sizes tested: the preset size (66 × 42) for goldens and a 40 × 24 for fast scenario tests.

Test helpers ship with the library (project rule), so they are under `Shared/Weather/Testing/`, not in GoogleTests.

### Regression scenarios (GDD §6.1–6.4)

| Test | Setup | Pass condition (GDD ratio) |
|---|---|---|
| Coastal rain | Default parameters, no preset, Spring start, 52 weeks, `coastal` (≤ 2 hex steps from ocean) versus `inland` (≥ 6) | mean precipitation(coastal) > 1.5 × mean precipitation(inland) |
| Rain shadow | `rain_shadow` preset, 13 weeks | mean precipitation(windward) > 2.0 × mean precipitation(leeward) |
| Snow accumulation | `winter_snow` preset, 13 weeks of Winter | more than 70% of cells with elevation > 0.6 have `snowPack > 0.1` |
| Spring melt | `winter_snow`, 13 Winter weeks to accumulate (**this accumulation step is added; the GDD pseudocode reads the snow before any has fallen**), then 13 Spring weeks | final snow < 30% of the accumulated snow; lowland (elevation < 0.4) groundWater sum > 0.5 × the accumulated snow sum |

Additionally: every preset loads and runs 13 weeks on the terrain without a non-finite value and without a range violation (Finish pass clamps).

### Allowed tuning levers if a ratio fails

In order: the preset data values; the default baseline table; `cloud_decay`; then **stop and flag** for the GDD owner (any of the 20 GDD constants, a mechanic). Every change is appended to a **Tuning log** in this spec's Status section (parameter, old, new, test that forced it), and a changed default is a new GDD deviation.

### Goldens

`Tests/GoogleTests/CoW/Golden/weather_layers.golden.json`, using `CellLayerGolden.h` (`HashLayer`, `GoldenStore::Check`, update with `DIA_UPDATE_GOLDEN=1`): all 9 fields of `MakeCoastRange(66, 42)` after 13 weeks for each of the 5 presets (45 hashes), serial executor. Golden diffs follow the repo's summary-only report.

## Public API

New in `Cluiche/CoW/Shared/Weather/`: `WeatherPreset.h/.cpp` (struct, `LoadWeatherPreset`, `ResolveSelector`, distance field), `WeatherPresetApply.h/.cpp` (`ApplyPreset`), `Testing/WeatherTestTerrains.h/.cpp`. `WeatherSimulation` gains `Reseed(Season, const WeatherParameters&, const IBiomeSeasonalBaseline&)` and `ClearForcings()`. Preset files under `Assets/CoW/Global/Weather/Presets/`.

## Acceptance Criteria

### Loader
1. All five shipped preset files load. A copy with each of these edits is rejected with the offending key named: unknown top-level key, unknown clause, unknown selector name, `min > max`, field value out of range, unknown field, unknown forcing kind, wrong `format`, wrong `version`, `weeks` missing on a non-wind forcing.
2. Preset loading never partially succeeds: on failure no `WeatherPreset` is produced.
3. Loading is allocation-light and does not touch `WeatherSimulation`.

### Selectors and distance
4. `distanceToWater` is 0 on every water-source cell and increments by 1 per hex step on a hand-built row, on both odd and even columns. A grid with water only at one corner gives the expected maximum.
5. Clauses AND together. `waterSource`, `elevation`, `latitude` boundaries are inclusive.
6. A selector used by a fill or forcing that resolves to 0 cells makes `ApplyPreset` fail with the selector named.

### `ApplyPreset`
7. After `ApplyPreset(P)` the state is bit-identical whether the simulation was fresh, ran 20 weeks first, or had earlier forcings (a test per case).
8. Fills apply in file order (a later fill overwrites). Fields not named hold the seed values for the preset's season.
9. Each preset leaves the week counter at 0, returns its season, and leaves no forcing from a prior preset.
10. A preset that would exceed 64 forcings or 16 cell sets fails and leaves the simulation byte-identical (atomicity).
11. `ApplyPreset` with a step captured and not committed returns an error without mutating.
12. The same preset applied twice gives identical state.

### Forcings from presets
13. A `WindOverride` preset forcing covers exactly its selector's cells: wind there is the override, and wind outside is computed.
14. An `until cleared` forcing outlives 52 weeks. A timed forcing expires after its `weeks`.

### Regression scenarios
15. Coastal rain, rain shadow, snow accumulation and spring melt (table above) pass on `MakeCoastRange(40, 24)` with default parameters, each with at least the stated ratio.
16. For each scenario the test prints the measured ratio (not only pass/fail) so margin is visible. A scenario that passes with less than 10% margin is reported as a warning in the test output.
17. All five presets run 13 weeks with no NaN, no infinity, and every field inside its range.
18. Results are identical with a serial executor and a 3-range executor (the F2 determinism guarantee, re-asserted at preset level once).

### Goldens
19. The 45 layer hashes match the committed golden file. Changing a pipeline constant makes the failure name the preset, the layer, the changed-cell count and the first cell.

### Performance
20. `ApplyPreset` at Large (6,656 cells) takes under 5 ms in Release (reported, not gated in Debug).

## Files Touched

New: the `WeatherPreset*` and `Testing/WeatherTestTerrains*` pairs, five preset JSON files, `Tests/GoogleTests/CoW/Weather/{TestWeatherPreset,TestWeatherPresetApply,TestWeatherRegression}.cpp`, the golden file. Edited: `WeatherSimulation` (Feature 1/2's class: `Reseed`, `ClearForcings`), vcxproj entries via `dia docs vcxproj-add`. No Dia files are added or edited.

**Amends Approved Feature 2** (done 2026-11-16 alongside this draft, to be re-approved with this spec): `WeatherForcing` gains `cellSet`, `WeatherSimulation` gains `RegisterCellSet` / `ClearCellSets`, a Region forcings paragraph and acceptance criterion 36a.

## Binding Decisions

| ID | Decision | How this feature complies |
|----|----------|---------------------------|
| PD-004 | No STL in public APIs | Preset, selector and result types use `DynamicArrayC` and Dia string types |
| AD-001 | Ask permission before Dia edits | None. `CellLayerGolden.h` is built and only consumed |
| AD-003 | GDD owns design intent | Ratios and preset intent are the GDD's. The added accumulation step in the spring-melt test and any tuned default are listed as deviations |
| DFS-002 | `Run` reads only the captured working copy | Presets act on the committed state and cell sets between steps, never inside `Run`. Cell sets and forcings are latched at `Capture` by Feature 2 |

## GDD Deviations Introduced (for the system spec's list)

- **D-6.1.** Presets are scenarios (forced state), not packs.
- **D-6.2.** The spring-melt test adds 13 Winter weeks of accumulation before measuring initial snow.
- **D-6.3.** Region forcings (cell sets) replace per-cell wind overrides: the GDD's `set_wind_region` needs a region.
- **D-6.4 (reserved).** Any default changed to meet a ratio (Tuning log).

## Test Plan

TDD per the project rule: loader, selectors and distance first (no simulation), then `ApplyPreset`, then the four scenarios and the goldens. The regression tests are integration-style GoogleTests and run serial. Scoped `--filter` while iterating, one full run at the end with `--shards >= 2`. No mutation checks. The gap audit and edge pass is an explicit last plan task.

## Open Design Questions

1. **Will the GDD ratios hold on a 40 × 24 hand-built terrain?** The ratios were written against generated 200 × 200 worlds. A small terrain with a one-cell coast and a single ridge may give a narrow inland band, so coastal-over-inland can fail from geometry rather than from the model. If so, enlarge the fixture before touching tuning levers; the fixture is cheap and the levers change defaults for every world.
2. **Is `cloud_decay` = 0.5 enough of a lever?** It is Feature 2's open question and this feature is where it is answered. If rain shadow fails by a wide margin, the cause is more likely the orographic pass than cloud decay, which would be a mechanic flag to the GDD owner and not a tuning change.
3. **How is "toward land" / "away from the coast" expressed in a world-independent preset?** An angle in degrees is terrain-specific. `coastal_storm` and `drought` need a direction derived from the terrain (for example, the gradient of `distanceToWater`). The cleanest option is a `direction` of `"towardLand"` / `"awayFromWater"`, resolved once per cell set to the mean gradient direction snapped to a wind angle. Absolute angles stay available for `rain_shadow`. Decide when writing the plan; the fixture's geometry (ocean on the west) means a fixed 0° would work for tests but break on `s25_coast_range`.

## Gap-pass amendments (2026-11-16)

Found by the plan-writing pass; each item below **supersedes** any contradicting text above. Numbering is stable.

1. **Direction expression (closes Open Question 3).** A preset `WindOverride` takes either an absolute `angleDegrees` or a `direction` of `"inland"` or `"seaward"`. `"inland"` is the on-screen direction of increasing distance from water: at each selector cell the gradient vector is the sum over its six neighbours of `unitVector(d) * (distance(n) - distance(c))`, the vectors are summed over the selector's cells and snapped with `SnapWindDirection`. `"seaward"` is its opposite. A zero vector makes the preset invalid ("no inland direction") at load. `coastal_storm` uses `"inland"`; `drought` uses `"inland"` (away from the coast and from water is the same direction).
2. **"Mean precipitation" in the regression ratios** is the mean over every cell in the set and over **every week of the scenario** (for the 52-week coastal test, all 52 weeks; for the 13-week shadow test, all 13).
3. **Golden file ownership.** This feature owns `weather_layers.golden.json`; Feature 2's entries use the `pipeline/` prefix (see Feature 2's amendment 6). The goldens are recorded at `MakeCoastRange(66, 42)`; "at Small" in the system spec means that size.
4. **Wind-override timers** follow Feature 2's amendment 1: `weeks` of 0 is until cleared; `coastal_storm` (6 weeks) and `drought` (8 weeks) wind overrides therefore expire, and criterion 14 holds.
5. **`ClearForcings()` is added by this feature** to `WeatherSimulation` (Feature 2's class), as Feature 2's amendment 3 says; it is not part of Feature 2's own delivery. `ApplyPreset` step 2 calls it. The earlier wording "Feature 2's API" is superseded.
6. **The `direction` zero-vector check (item 1) is made in `ApplyPreset`'s validate-first step**, not at preset load, because the gradient needs the terrain. A preset with an unresolvable direction returns `ok = false` and changes nothing.

## Status

**Status:** `Approved` (2026-11-16). Plan: [presets-and-regression.plan.md](presets-and-regression.plan.md) (written 2026-11-16, not started). **Tuning log:** empty.
