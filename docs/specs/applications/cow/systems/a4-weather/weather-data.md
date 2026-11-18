# Feature Spec: A4 Weather Data & Parameters

**Parent:** @docs/specs/applications/cow/systems/a4-weather/a4-weather.md (Feature 1)
**Status:** Approved (2026-11-16). Amended 2026-11-17 (user decisions: A5 hand-off, Features 7–9): `groundWater` becomes `waterToGround`, the `ground_water` parameter group moves to A5 (20 constants, 23 table rows), `HydrologyInputs` and `WeatherHandoff` types added.
**Amended 2026-11-17 (aligned to locked GDD, CHANGE-047; row 6d):** `pressureAnomaly` joins `WeatherFields` (ten arrays); `WeatherHandoff` gains `influenceWater`; `HydrologyInputs` carries A5's firn constants; the descriptor table gains the `influence` block (GDD §10a) with `Int` and `Enum` kinds (41 rows).
**Design source:** `DesignDocs/COW/Output Docs/GDD/08_SystemsDeepDive/A4_Weather/A4_TuningParameters.md` §1–§10, §10a, §14 and `A4_TechnicalSpec.md` §1, §4.1 (all locked 2026-11-17, CHANGE-047); `A4_EdgeCaseMatrix.md` (EC-01, EC-19)
**Depends on:** S-1 Biome widening (10 → 13; built) and `WorldGeneration::BiomeFromId`. No Dia code beyond `DynamicArrayC` and `Json::Value`.
**Consumed by:** A4 Feature 2 (creates the step on top of this feature's `WeatherSimulation`), Features 3 to 6.

**Amended 2026-11-18 (world-sim review, user decisions):** see "World-sim review amendment" below.

## Summary

The data layer of the weather system, as pure logic with no module, bus or thread. It defines the weather state and terrain arrays, the 20 tuning constants (21 before the ground-water drain moved to A5; amended 2026-11-17, A5 Hydrology) plus the 18-row `influence` block (GDD §10a; amended 2026-11-17, row 6d) with their file format and validation, the seasonal temperature baseline contract with its default table, and `WeatherSimulation::Initialise`, which ingests terrain and seeds week-0 state. It is provable by GoogleTests on hand-built hex terrain.

## Goals

1. `WeatherFields` and `WeatherTerrain` exist as structure-of-arrays types, sized once at `Initialise`.
2. All 20 constants live in one **descriptor table**. Load, range validation, defaults, `set_param`, and pack override merge all walk that table, so a constant is added in one place.
3. `LoadWeatherParameters` is all-or-nothing: any invalid input returns false with `out` set to compiled defaults and an error string naming the first offending key.
4. `IBiomeSeasonalBaseline` and `TableBiomeSeasonalBaseline` cover all 13 biomes in four seasons.
5. `Initialise` ingests terrain, rejects a world with no water source (EC-01), and seeds the fields **from A4's own rules only**. It never reads S-1's climate layers (decided 2026-11-16, system Open Q1).
6. The latitude modifier is **one function** shared by seeding and Feature 2's solar pass, so week-0 temperature and the converged solar target cannot disagree.

## Non-Goals

- The weekly step, grid tables, forcings and `Capture`/`Run`/`Commit` (Feature 2).
- Reading the world from S-1, the `.diaapp` config, hot reload commands, `cow.weather.params` registration (Feature 3, Feature 5; name corrected 2026-11-18). This feature supplies the primitives they call.
- Spin-up (Feature 3; it needs the step).
- Saving fields (Feature 4).
- F-3 pack files. This feature supplies the overrides-only merge primitive; the pack participant is Feature 3 and F-3 Feature 6.

## Design

### Types (`Cluiche/CoW/Shared/Weather/`)

```cpp
namespace CoW::Weather {
    enum class Season : uint8_t { Spring, Summer, Autumn, Winter };

    struct WeatherFields {                    // N cells each; radians [0, 2π) and [0, 1] as the system spec
        DynamicArrayC<float> moisture, temperature, pressure, windDirection, windSpeed,
                             precipitation, cloudCover, waterToGround, snowPack,   // waterToGround replaces groundWater (amended 2026-11-17, A5 Hydrology)
                             pressureAnomaly;                                      // [-0.5, 0.5], 0 = none; signed, carried by the wind and decayed (GDD §4.1; amended 2026-11-17, row 6d)
        void Resize(uint32_t n);              // zero-filled
    };

    // Added 2026-11-17 (A5 Hydrology). Types only; Feature 2 fills them.
    struct WeatherHandoff   { DynamicArrayC<float> glacierInput;      // glacier-eligible cells: snow above the cap + yearly firn (GDD §10, §11.3), [0, 2]
                              DynamicArrayC<float> influenceWater;    // Feature 7's signed water weight; amended 2026-11-17, row 6d
                              void Resize(uint32_t n); };             // per-week, not saved
    struct HydrologyInputs  { DynamicArrayC<float> surfaceWetness, openWater; DynamicArrayC<uint8_t> glacierEligible;
                              uint32_t glacierCheckWeek = 26;         // A5's GLACIER_CHECK_WEEK and GLACIER_SNOW_FLOOR, supplied by the provider so A4 and A5 cannot disagree (GDD §11.3)
                              float    glacierSnowFloor = 0.2f;       //   (amended 2026-11-17, row 6d; the defaults are test values, A5's file is the source)
                              bool present = false; void Resize(uint32_t n); };              // A5's previous committed week; present == false when no A5

    struct WeatherTerrain {                   // read-only after Initialise
        DynamicArrayC<float>   elevation, latitude;     // [0, 1]; latitude 0 = equator, 1 = pole
        DynamicArrayC<uint8_t> waterSource, biome;      // WaterSource, Biome
        uint32_t width = 0, height = 0;                 // for the hex grid; width*height == cell count
        bool IsConsistent() const;                      // all four arrays equal width*height
    };
}
```

`Season` is A4's (the engine has none). Season-from-week arithmetic uses the existing `WeekOf`/`SeasonOf` helpers and is not part of this feature.

### Parameter descriptor table

```cpp
struct ParamDescriptor { const char* group; const char* key; float WeatherParameters::* member;
                         float minValue; float maxValue; float defaultValue; };
span<const ParamDescriptor> GetParamDescriptors();      // 23 rows + 18 influence rows = 41 (amended 2026-11-17, A5 Hydrology and row 6d)
```

The row set is the 19 GDD constants in the §14 groups (`solar`, `pressure`, `wind`, `evaporation`, `transport`, `orographic`, `precipitation`, `snow`; the `ground_water` group and its `drain_rate` move to A5, amended 2026-11-17, A5 Hydrology) plus `solar.cloud_decay` (0.5, [0, 1], added by Feature 2), plus the A4 extension blocks: `init.initial_moisture_water`, `init.initial_moisture_land`, and `boundary.edge_inflow`. All are `float` members, so one table type covers them.

**Influence block (amended 2026-11-17, row 6d; GDD §10a, locked).** 18 more rows in group `influence`, owned by Feature 7 and Feature 9 but loaded by this table so there is one load path. `ParamKind` gains `Int` (a `float` member that must be a whole number) and `Enum` (a JSON string mapped to an ordinal). The 13 GDD rows: `max_strength` 1.0 [0.5, 2.0]; `max_delta_pressure` 0.3, `max_delta_moisture` 0.3, `max_delta_temperature` 0.15 [0.05, 0.5]; `max_lifetime_weeks` 13 [1, 52] `Int`; `max_active` 64 [8, 256] `Int`; `preview_fade_days` 1 [0.25, 3]; `storm_pressure_seed` −0.3 [−0.5, −0.1]; `pressure_anomaly_decay` 0.7 [0.5, 0.9]; `pressure_anomaly_max` 0.5 [0.2, 0.5]; `storm_moisture` 0.2 [0.05, 0.4]; `temp` 0.1 [0.02, 0.2]; `moisture` 0.15 [0.05, 0.3]; `default_falloff` `Smooth` (`none`/`linear`/`smooth`, `Enum`). Four Cluiche additions (provisional, Feature 7): `cap_water` 1.0 [0, 2]; `max_radius` 32 [0, 64] `Int`; `max_set_cells` 8192 [64, 65536] `Int`; `ground_water_factor` 0.05 [0, 1]. `pressure_anomaly_decay` and `pressure_anomaly_max` are used by Feature 2's passes 2 and 5c whether or not Feature 7 is built. The 20-constant count of the §1–§10 groups is unchanged.

- **Ranges** are the GDD's range column (e.g. `latitude_cooling_factor` 0.0–0.5). A value outside its row's range is invalid. This is stricter than the system spec's "every rate in [0, 1]" and replaces it.
- **Cross-field rule:** `precipitation.snow_threshold <= snow.melt_threshold`. The GDD does not state it; the system spec adds it. Violation is invalid.
- **Dotted key** is `group.key` (`"precipitation.snow_threshold"`). `FindParam(dottedKey)` returns the row or null. `SetParam(WeatherParameters&, dottedKey, value, err)` range-checks, applies, and re-checks the cross-field rule on the result; on failure it changes nothing.

### Parameter file

Envelope `{"format": "cow.weather_params", "version": 1, "weather_parameters": { <groups> }, "baseline_table": {...}}`.

- `LoadWeatherParameters(const Json::Value& root, WeatherParameters& out, ParamError& err)` (the `influence` group is part of `weather_parameters`, like the others), where `ParamError` is a small struct (dotted key, reason) with no STL types: returns **false** and sets `out = DefaultWeatherParameters()` on **any** of: wrong `format` or `version`, a key not in the table (typo), a missing group is allowed (defaults), a missing key within a present group is allowed (default), a non-number, a non-finite number, a range violation, the cross-field rule, or a `baseline_table` fault. The error names the first offending dotted key.
- **Unknown keys are invalid**, at group and key level (decided 2026-11-16).
- **Overrides merge.** `ApplyOverrides(const Json::Value& overrides, WeatherParameters& inOut, err)` applies a sparse object over an existing set under the same validation, atomically (on failure `inOut` is untouched). Packs and `cow.weather.params {action: reload}` use it. A full file load is `ApplyOverrides` over defaults plus the envelope check.
- `DefaultWeatherParameters()` returns the table defaults. A test asserts it equals the GDD §14 JSON values.

### Baseline

```cpp
class IBiomeSeasonalBaseline { public: virtual ~IBiomeSeasonalBaseline() = default;
    virtual float GetSeasonalTemperature(WorldGeneration::Biome biome, Season season) const = 0; };

class TableBiomeSeasonalBaseline final : public IBiomeSeasonalBaseline {
public:
    bool Load(const Json::Value& baselineTable, err);   // keyed by BiomeId strings
    float GetSeasonalTemperature(Biome, Season) const override;   // flat float[13][4]
};
```

- `baseline_table` is keyed by `BiomeId` strings (`Biome.Grassland`, …, resolved with `BiomeFromId`; **not** the enum names). A missing biome row, an unknown id, or a value outside [0, 1] makes `Load` return false and leaves the table unchanged. All 13 biomes are required, including the three succession outputs.
- `GetSeasonalTemperature` of an out-of-range biome value returns **0.5** (EC-19). The compiled default table holds all 13 rows; **the default numbers are authored in this feature's tasks** from GDD A4 §3 and biome climate, and recorded in `weather_parameters.json`.
- `A4 asserts the result is in [0, 1]` is a Feature 2/3 concern (it applies to A1's provider as well). This feature's table cannot violate it.

### Latitude modifier (shared)

```cpp
float SeasonalTargetTemperature(float baseline, float latitude, float latitudeCoolingFactor);
// = clamp(baseline * (1 - latitudeCoolingFactor * latitude), 0, 1)
```

Latitude 1.0 cools by exactly `latitude_cooling_factor` (matches Feature 2's acceptance criterion 7). Feature 2's solar pass calls this function instead of re-deriving it.

### `WeatherSimulation::Initialise`

```cpp
bool Initialise(const WeatherTerrain& terrain, const WeatherParameters& params,
                const IBiomeSeasonalBaseline& baseline);
```

1. Reject (return false, log nothing here: the module logs) if `!terrain.IsConsistent()`, or the cell count is 0.
2. **EC-01:** return false if no cell has `waterSource != None`. State is unchanged.
3. Copy terrain into `WeatherSimulation`-owned arrays. The caller's terrain is not retained.
4. Seed each cell: `temperature = SeasonalTargetTemperature(baseline(biome, Spring), latitude, params.latitude_cooling_factor)`; `moisture = initial_moisture_water` on water-source cells, `initial_moisture_land` elsewhere; `pressure = 0.5`; every other field 0 (including `pressureAnomaly`).
5. A second `Initialise` fully replaces the first (the S-1 regeneration case): no leaked state from the previous world.

Feature 2 extends `Initialise` with the grid tables and scratch arrays; this feature leaves a named seam (`BuildGridTables()` called last, empty here).

## Public API

Files under `Cluiche/CoW/Shared/Weather/`: `WeatherTypes.h/.cpp` (`Season`, `WeatherFields`, `WeatherTerrain`), `WeatherParameters.h/.cpp` (struct, table, load, overrides, set param, defaults), `BiomeSeasonalBaseline.h/.cpp` (interface + table), `WeatherSimulation.h/.cpp` (class shell + `Initialise` + `SeasonalTargetTemperature`).

## Acceptance Criteria

### Types
1. `WeatherFields::Resize(n)` sizes all ten arrays (`waterToGround` is the ninth, amended 2026-11-17, A5 Hydrology; `pressureAnomaly` the tenth, amended 2026-11-17, row 6d) to `n`, zero-filled; a second `Resize` with a smaller `n` leaves no stale values. `WeatherHandoff::Resize` sizes `glacierInput` and `influenceWater` the same way.
2. `WeatherTerrain::IsConsistent()` is false when any array differs in length from `width*height`, and when the cell count is 0.

### Parameters
3. The descriptor table has 41 rows (20 constants, 2 `init`, 1 `boundary`, 18 `influence`; amended 2026-11-17, A5 Hydrology and row 6d; was 24), unique dotted keys, and every default lies inside its own range.
4. `DefaultWeatherParameters()` equals the GDD §14 values for all 19 remaining GDD constants, plus `cloud_decay` 0.5. There is no `ground_water.drain_rate` row; loading a file that has one is an unknown-key error (it now belongs to `hydrology_parameters.json`; amended 2026-11-17, A5 Hydrology).
4a. (amended 2026-11-17, row 6d) The `influence` defaults equal the GDD §10a values (aligned to locked GDD): `max_strength` 1.0, `max_delta_*` 0.3 / 0.3 / 0.15, `max_lifetime_weeks` 13, `max_active` 64, `preview_fade_days` 1, `storm_pressure_seed` −0.3, `pressure_anomaly_decay` 0.7, `pressure_anomaly_max` 0.5, `storm_moisture` 0.2, `temp` 0.1, `moisture` 0.15, `default_falloff` Smooth. A file carrying the removed `INFLUENCE_STORM_PRESSURE` (any spelling) is an unknown-key error.
5. Valid full file → true, values as written. Empty `weather_parameters` object → true, all defaults. A missing group, and a missing key inside a present group, take defaults.
6. Each of these returns false, sets `out` to defaults and names the key: a value below its range, a value above its range, a string where a number is expected, a NaN/Inf, `snow_threshold > melt_threshold`, an unknown key in a known group, an unknown group, wrong `format`, wrong `version`, a non-whole value on an `Int` row, an unknown string on an `Enum` row (amended 2026-11-17, row 6d).
7. A range boundary value (min and max) is **valid**.
8. `ApplyOverrides` with a sparse object changes only the named keys; with one bad key among good ones it changes **none** and leaves `inOut` byte-identical.
9. `SetParam` applies one valid value; an out-of-range value, an unknown key, or a value that would break `snow_threshold <= melt_threshold` returns false and changes nothing.

### Baseline
10. `TableBiomeSeasonalBaseline` default table: all 13 × 4 entries in [0, 1]; warmer in summer than winter for every land biome (documented exceptions: none).
11. `Load` with all 13 biomes → true. With one biome missing, an unknown `BiomeId`, an enum name instead of an id, a value of 1.2, or a duplicate → false and the table is unchanged.
12. `GetSeasonalTemperature` for a biome value ≥ 13 returns 0.5 (EC-19), and never reads out of bounds.

### Initialise
13. No water-source cell → false and the simulation is uninitialised (EC-01). Inconsistent terrain → false. Neither throws or asserts in a test build.
14. Seeding: spring baseline × latitude modifier; water-source cells hold `initial_moisture_water`; land cells `initial_moisture_land`; pressure 0.5; the other seven fields (including `waterToGround` and `pressureAnomaly`) 0. Verified for a hand-built 6×4 terrain with known answers.
15. Latitude 0 gives the unmodified baseline; latitude 1 gives `baseline × (1 − latitude_cooling_factor)`. The same helper is what Feature 2 calls (asserted by a Feature 2 test; the helper is exported).
16. Seeding ignores S-1 climate: there is no input for it, and a test builds a world with `waterSource`/`biome` only and gets valid state.
17. Re-`Initialise` on a different-sized terrain gives state identical to a fresh simulation on that terrain.
18. Initialising the same terrain twice gives bit-identical fields (determinism).

## Files Touched

New: the four file pairs above under `Cluiche/CoW/Shared/Weather/`; `Cluiche/Tests/GoogleTests/CoW/Weather/{TestWeatherTypes,TestWeatherParameters,TestBiomeSeasonalBaseline,TestWeatherInitialise}.cpp`; `Assets/CoW/Global/Weather/weather_parameters.json` (defaults file). Edited: `CoW.vcxproj(.filters)` and `GoogleTests.vcxproj(.filters)` through `dia docs vcxproj-add`. No Dia files are added or edited.

## Binding Decisions

| ID | Decision | How this feature complies |
|----|----------|---------------------------|
| PD-004 | No STL containers in public APIs | `WeatherFields`/`WeatherTerrain` use `DynamicArrayC`. Error strings use Dia's string type, not `std::string`. The descriptor span is a Dia span or `(pointer, count)` pair |
| AD-001 | Ask permission before Dia edits | None. Nothing is added to Dia |
| AD-002 | Four-tier dependency model | CoW-Shared: A1, V-1, S-2.5 and S-3 all consume these types |
| AD-003 | GDD owns design intent | Defaults and ranges are the GDD's. Differences (`cloud_decay`, range-checked keys, unknown-key rejection, `init`/`boundary`/`baseline_table` blocks) are listed as deviations below |

## GDD Deviations Introduced (for the system spec's list)

- **D-1.** Parameter ranges are enforced from the GDD range column (system spec said only [0, 1]).
- **D-2.** The file rejects unknown keys. The GDD's §14 does not define this.
- **D-3.** `init`, `boundary` and `baseline_table` blocks extend §14 (already in the system spec; restated here with their validation).

## Test Plan

All GoogleTests, no build of S-1 required. Tests are written first (RED) per the TDD skill, in the order: types, descriptor table and defaults, load and validation, overrides and `SetParam`, baseline, `Initialise`. Parameter tests run table-driven: one loop over `GetParamDescriptors()` generates the below-range and above-range cases for every row, so a new constant is covered with no new test. No mutation checks (project rule 2026-11-12). Gap audit and edge pass is the last plan task.

## Open Design Questions

1. **Is "the GDD range column" the right validity range, or too strict for tuning?** The ranges are the GDD's suggested tuning bands, not hard limits. Rejecting a value like `rain_threshold = 0.9` blocks an experiment the author may want in S-2.5. The alternative is two-tier: hard range [0, 1] (invalid beyond it) and a soft band (warn). Affects criterion 6 and `SetParam`. Recommended default: hard-reject at the GDD range for now, because Feature 6's presets must live inside it anyway, and loosen if tuning runs into it.
2. **Default baseline numbers.** The GDD gives constants but no per-biome seasonal temperatures; the 52 numbers are authored here. They set where the biomes sit relative to `snow_threshold` (0.3) and `condensation_temp` (0.4), so they steer Feature 6's seasonal-storm and snow-line ratios. If those regress, tune this table before touching GDD constants.
3. **`SetParam` and the cross-field rule.** Raising `snow_threshold` above the current `melt_threshold` is rejected even though the author might intend to raise both. The alternative is a batch `SetParams` call, which Feature 5's command could use. Defer until Feature 5.

## Gap-pass amendments (2026-11-16)

Found by the plan-writing pass; each item below **supersedes** any contradicting text above. Numbering is stable.

1. **`cloud_decay` is owned here.** The descriptor table (criteria 3 and 4) already holds it; it is the 20th constant (21st before the drain moved to A5; amended 2026-11-17, A5 Hydrology) (`solar.cloud_decay`, default 0.5, range [0, 1]). Feature 2 *uses* it and no longer "adds" it.
2. **Parameter kinds and the three extension rows.** `ParamDescriptor` gains `ParamKind kind` (`Float` or `Bool`). `init.initial_moisture_water` (default **0.8**) and `init.initial_moisture_land` (default **0.3**) are `Float`, range [0, 1]; `boundary.edge_inflow` is `Bool` (default **1**), stored as a `float` member that must be exactly 0 or 1 (any other value is invalid; a `true`/`false` JSON literal is also accepted and converted). The two initial-moisture defaults are starting values that Feature 6's regression tests may tune (tuning log). The "GDD range column" rule applies to the 19 remaining GDD constants; these rows carry the ranges stated here.
3. **Land biomes (criterion 10).** "Land" means every biome except `Ocean` and `Lake`: the 11 others, including the three succession outputs. No exemptions: every one must be warmer in Summer than in Winter.
4. **Types.** Error text uses `Dia::Core::Containers::String256` and dotted keys `String64`; `GetParamDescriptors()` returns a `(const ParamDescriptor*, uint32_t count)` pair, not a span. Plan Task 0 confirms these exist under those names and records any rename.
5. **Seeding is one function.** `WeatherSimulation` holds a private `SeedFields(Season, params, baseline)`. `Initialise` calls it with `Season::Spring`; Feature 6's `Reseed` calls it with the preset's season. No other behaviour changes.

6. (amended 2026-11-17, row 6d) **Ten field arrays and the influence block.** `pressureAnomaly` is the tenth `WeatherFields` array; the table has 41 rows (the 23 above plus the 18-row `influence` block); `ParamKind` is `Float`, `Bool`, `Int` or `Enum`. The "20 constants" count refers to the §1–§10 constants only.

## World-sim review amendment (2026-11-18)

User decisions from the world-sim review. Each item **supersedes** any contradicting text in this spec. Sources: [summary](../../../../../research/worldsim_data_driven/summary.md) §0, §0.1; [parameter-files](../../../dia/systems/diaattribute/parameter-files.md); [biome-registry](../s1-worldgeneration/biome-registry.md); [worldsim-kit](../worldstep/worldsim-kit.md); [grid-search-helpers](../../../dia/systems/diapathfinding/grid-search-helpers.md); [blackboard-scopes](../../../dia/systems/diablackboard/blackboard-scopes.md).

1. **Parameters (system item 1).** Supersedes the Parameter descriptor table and Parameter file sections, the `ParamDescriptor` / `GetParamDescriptors` / `FindParam` / `SetParam` / `LoadWeatherParameters` / `ApplyOverrides` / `DefaultWeatherParameters` signatures in Public API, and ACs 3-9 (plus 4a). Gap-pass item 2 (`ParamKind` and the extension rows) and D-3 move to the sidecar. Replacement: `weather_parameters.schema.json` declares all keys (kind, range, default, description) and the `constraints`; `WeatherParameters` is a plain struct filled by `ParamBinding<WeatherParameters>::Snapshot`. Tests cover A4's sidecar (every row complete, defaults equal the GDD values, constraint fixtures, `Snapshot` fills every member, `Hash()` stable); load, pack and set machinery is tested by DiaAttribute Feature 8.
2. **Baseline (system items 3, 13).** Supersedes the Baseline section and ACs 10-12. The table is one map per season column keyed `keyRef cow.biomes`, complete over the registry (checked by `dia validate schema`), and is a fallback only: A1's `IBiomeSeasonalBaseline` wins. An unknown or out-of-range biome returns the registry `fallback` biome's value (replaces the EC-19 constant 0.5, which stays only for `kNoBiome`).
3. **Biomes.** `WeatherTerrain` carries `BiomeIndex` (uint8). Prerequisite Task 0 (13-entry enum, `BiomeFromId`) is superseded by the registry.
4. **Naming.** Param keys are `cow.weather.params/<key>`; dotted keys in text are unchanged.
5. **Open questions.** Open Q1 (GDD range column) is resolved: ranges live in the sidecar. Open Q3 (`SetParam` and the cross-field rule) is resolved by sidecar `constraints`: a `set` that breaks one is rejected with both keys named.
6. **Influence block (system item 14).** The 18-row `influence` block splits: generic caps stay parameters; per-kind values move to the kind catalogue (Feature 7). **Westerly band and orographic factor (items 16, 17):** read from S-1's parameters, not owned here.
7. **Files Touched.** `weather_parameters.json` is replaced by the sidecar (catalogued, resolved through `PathStore`).

## Status

**Status:** `Approved` (2026-11-16). Amended 2026-11-17 (A5 hand-off). Amended 2026-11-17 (aligned to locked GDD, CHANGE-047; row 6d). Plan: [weather-data.plan.md](weather-data.plan.md) (written 2026-11-16, not started).

**Amended 2026-11-18 (world-sim review).** Status stays `Approved`. Amended 2026-11-18 (world-sim review decisions; user-approved designs in the specs referenced below).
