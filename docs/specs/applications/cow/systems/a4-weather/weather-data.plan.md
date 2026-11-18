# Plan: A4 Weather Data & Parameters (A4 Weather Feature 1)

**Spec:** @docs/specs/applications/cow/systems/a4-weather/weather-data.md
**Status:** Not Started
**System plan:** @docs/specs/applications/cow/systems/a4-weather/a4-weather.plan.md (row 1)

Can start now: it is pure logic on hand-built hex terrain, with no module, bus or thread. It needs S-1's
13-biome widening (built) and `WorldGeneration::BiomeFromId`, and nothing from Dia beyond `DynamicArrayC`
and `Json::Value`. **No plan depends on this one being started; two depend on it being done.**
`ca-pipeline.plan.md` needs tasks 1 to 14 of this plan Done (it extends `WeatherSimulation` and the
parameter struct). `presets-and-regression.plan.md` needs this plan fully Done for its loader, selector
and fill tasks (it also needs ca-pipeline built for forcings and every regression test).

All code is CoW (application) code under `Cluiche/CoW/Shared/Weather/`. No Dia files are added or edited
(AD-001). Tasks that could need more than sonnet say "ask before escalating" (feedback_confirm_opus).

## Implementation Patterns

### Types (Task 2)
- `Cluiche/CoW/Shared/Weather/WeatherTypes.h/.cpp`, namespace `CoW::Weather`.
- `enum class Season : uint8_t { Spring, Summer, Autumn, Winter }`.
- `WeatherFields`: ten `DynamicArrayC<float>` (`moisture, temperature, pressure, windDirection,
  windSpeed, precipitation, cloudCover, waterToGround, snowPack, pressureAnomaly`; `waterToGround` replaces `groundWater`, amended 2026-11-17 (A5 Hydrology); `pressureAnomaly` [-0.5, 0.5] is the tenth, amended 2026-11-17 (row 6d)), `void Resize(uint32_t n)` zero-fills and
  fully replaces (a smaller second `Resize` leaves no stale values: assign, do not just resize).
- `WeatherHandoff` (`glacierInput`, `influenceWater`; `Resize`) and `HydrologyInputs` (adds `glacierCheckWeek` and `glacierSnowFloor`, amended 2026-11-17 (row 6d)) are types only here.
- `WeatherTerrain`: `elevation`, `latitude` (`DynamicArrayC<float>`), `waterSource`, `biome`
  (`DynamicArrayC<uint8_t>`), `width`, `height`, `bool IsConsistent() const` (false on any length
  mismatch with `width*height`, and on a cell count of 0).
- No `std::` type in a public signature (PD-004).

### Parameter descriptor table (Tasks 3 to 6)
- `Cluiche/CoW/Shared/Weather/WeatherParameters.h/.cpp`. `WeatherParameters` is a POD of `float` members
  only (41 members: 20 constants, 2 `init`, 1 `boundary`, 18 `influence`; amended 2026-11-17 (A5 Hydrology and row 6d), the `ground_water` group is gone). The `influence` block rows are the GDD §10a values plus four Cluiche rows (spec "Influence block"). `cloud_decay` (`solar.cloud_decay`, default 0.5,
  range [0, 1]) is **owned by this feature** (amendment 1); Feature 2 only reads it.
- `enum class ParamKind { Float, Bool, Int, Enum }` (`Int` and `Enum` added 2026-11-17 (row 6d) for `max_active` and friends and `influence.default_falloff`). `ParamDescriptor { const char* group; const char* key; ParamKind
  kind; float WeatherParameters::* member; float minValue; maxValue; defaultValue; }`. One `static const`
  table; `GetParamDescriptors()` returns a `(const ParamDescriptor*, uint32_t count)` pair, not a span
  (amendment 4).
- Extension rows (amendment 2): `init.initial_moisture_water` default 0.8 and `init.initial_moisture_land`
  default 0.3, both `Float`, range [0, 1] (starting values Feature 6 may tune); `boundary.edge_inflow` is
  `Bool`, default 1, stored as a `float` that must be exactly 0 or 1 (any other value invalid; JSON
  `true`/`false` literals also accepted and converted). The "GDD range column" rule applies only to the
  19 remaining GDD constants. Load, range check, defaults,
  `SetParam` and `ApplyOverrides` all walk this table and nothing else, so a new constant is a one-row
  change.
- `FindParam(dottedKey)` builds `group.key` comparisons (no allocation in the hot path; `StringCRC` of the
  dotted key is acceptable if the table stores it, but a plain compare is fine for a 41-row table).
- `ParamError { Dia::Core::Containers::String64 key; Dia::Core::Containers::String256 reason; }` (dotted keys
  `String64`, error text `String256`; amendment 4): no STL. Task 0 confirms these names and records any rename.
- Validation order inside one pass: envelope (`format`, `version`), unknown group, unknown key, type
  (number, or `true`/`false` for `Bool` rows), finite, range, `Bool` rows exactly 0 or 1, then the cross-field rule `snow_threshold <= melt_threshold` on the **result**,
  then `baseline_table`. The first fault names its dotted key. On any fault `out = DefaultWeatherParameters()`.
- `ApplyOverrides(overrides, inOut, err)` is the primitive: build into a local copy, validate, copy back
  only on success (atomic; `inOut` byte-identical on failure). `LoadWeatherParameters` is the envelope check
  plus `ApplyOverrides` over defaults.
- `SetParam` is a one-key `ApplyOverrides` (range-check, apply to a copy, re-check the cross-field rule).
- Range boundaries (min and max exactly) are valid.
- Table-driven test: one loop over `GetParamDescriptors()` generates the below-min and above-max case for
  every `Float` row, and invalid values (e.g. 0.5, 2, -1) for the `Bool` row, so no per-constant test is
  written.

### Baseline (Tasks 7, 8)
- `Cluiche/CoW/Shared/Weather/BiomeSeasonalBaseline.h/.cpp`. `IBiomeSeasonalBaseline` (virtual
  `GetSeasonalTemperature(Biome, Season)`), `TableBiomeSeasonalBaseline final` with a flat
  `float mTable[13][4]`.
- "Land" for the summer-warmer-than-winter rule (AC 10) means every biome except `Ocean` and `Lake`: the 11
  others, including the three succession outputs. No exemptions (amendment 3).
- `Load` is keyed by `BiomeId` strings resolved with `WorldGeneration::BiomeFromId` (`Biome.Grassland`,
  not the enum name). Build into a local `float[13][4]` plus a `bool seen[13]`; reject a missing biome,
  unknown id, enum name, duplicate or value outside [0, 1]; commit to `mTable` only after all 13 pass.
- Out-of-range biome value (>= 13) returns 0.5 (EC-19) via a bounds check before indexing.
- The compiled default table is authored in Task 7 (52 numbers) from GDD A4 section 3 and biome climate,
  with a comment block naming the sources. They are mirrored into `weather_parameters.json` in Task 12.

### Latitude helper (Task 9)
- `float SeasonalTargetTemperature(float baseline, float latitude, float latitudeCoolingFactor)` in
  `WeatherSimulation.h`, free function, exported. `clamp(baseline * (1 - factor * latitude), 0, 1)`.
  Feature 2's solar pass must call it (a Feature 2 test asserts that).

### WeatherSimulation::Initialise (Tasks 10 to 12)
- `Cluiche/CoW/Shared/Weather/WeatherSimulation.h/.cpp`: class shell, owned copies of terrain, committed
  `WeatherFields`, `bool mInitialised`.
- `bool Initialise(const WeatherTerrain&, const WeatherParameters&, const IBiomeSeasonalBaseline&)`.
  Build everything into a local `WeatherSimulation`-shaped temporary and swap on success, so a failed call
  leaves the previous state unchanged (EC-01 and inconsistent-terrain cases).
- Seeding lives in one **private** function `SeedFields(Season, params, baseline)` (amendment 5).
  `Initialise` calls it with `Season::Spring`; Feature 6's `Reseed` calls it with the preset's season. No
  other behaviour changes. Reason: Feature 6 needs a public `Reseed(Season, ...)` that does exactly this with another
  season (presets-and-regression spec, "Public API"); keeping the seed in one function now avoids a
  refactor then. `Reseed` itself is **not** added in this plan.
- `BuildGridTables()` is a named private seam, called last in `Initialise`, **empty** here. Feature 2
  fills it.
- No reference to S-1 climate layers anywhere (AC 16): the function signature has no input for them.

### Testing
- `Tests/GoogleTests/CoW/Weather/{TestWeatherTypes,TestWeatherParameters,TestBiomeSeasonalBaseline,
  TestWeatherInitialise}.cpp`. Suite names: `WeatherTypes*`, `WeatherParameters*`, `BiomeSeasonalBaseline*`,
  `WeatherInitialise*`.
- Hand-built 6 x 4 terrain helper lives inside the test file for now; Feature 6 owns the shared
  `MakeCoastRange` builder.
- Byte-identity checks use `memcmp` over the field arrays, not `==` on floats.

### Verification cadence
- Per task: narrow `dia run googletest --filter="WeatherTypes*:WeatherParameters*:BiomeSeasonalBaseline*:WeatherInitialise*" --shards 2` (always `--shards 2`).
- The feature-wide `--filter="Weather*:BiomeSeasonalBaseline*"` runs only in the verification task; the full suite runs once in worldstep.plan.md, Weather-stack close-out. No `SLOW_` suites. Sanitizer: on request only (user, 2026-11-17). No mutation checks. (2026-11-17, speed-ups)

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 0 | Prerequisite check, read only: confirm S-1's 13-biome enum and `WorldGeneration::BiomeFromId` exist and list the 13 `BiomeId` strings; confirm the `WaterSource` enum (`None, Ocean, Lake, River`) location; confirm `Dia::Core::Containers::String256` and `String64` exist under those names and record any rename (`GetParamDescriptors` is a pointer+count pair either way); confirm `Json::Value` access from `Cluiche/CoW`; check `CoW.vcxproj` layout for `Shared/Weather/` | Findings recorded in notes | Not Started | haiku | Mechanical, TDD-exempt. If String256/String64 do not exist and no other Dia string type fits PD-004 without a Dia edit, stop and raise it as an AD-001 ask-first item |
| 1 | Create empty file pairs `WeatherTypes`, `WeatherParameters`, `BiomeSeasonalBaseline`, `WeatherSimulation` under `Cluiche/CoW/Shared/Weather/`, the four test files under `Tests/GoogleTests/CoW/Weather/`, and the `Assets/CoW/Global/Weather/` folder; register each with `dia docs vcxproj-add CoW ...` and `dia docs vcxproj-add GoogleTests ...` | Solution builds with the empty files | Not Started | haiku | Mechanical, TDD-exempt. Idempotent; later tasks edit files in place and do not touch the vcxproj again |
| 2 | RED then GREEN: `Season`, `WeatherFields::Resize`, `WeatherTerrain::IsConsistent` | AC 1, 2: ten arrays (`waterToGround` is the ninth, amended 2026-11-17 (A5 Hydrology); `pressureAnomaly` the tenth, amended 2026-11-17 (row 6d)) sized and zero-filled; `WeatherHandoff::Resize` sizes `glacierInput` and `influenceWater`; a smaller second `Resize` leaves no stale values; each of the four arrays wrong in turn, `width*height == 0`, all consistent | Not Started | sonnet | Prove RED with quoted output (tests exist, symbols missing or failing). Commit |
| 3 | RED then GREEN: `ParamKind`, `WeatherParameters` struct, descriptor table of 41 rows (23 + the 18-row `influence` block; amended 2026-11-17 (row 6d)), `GetParamDescriptors` (pointer + count), `FindParam`, `DefaultWeatherParameters` | AC 3, 4, 4a: 41 rows, unique dotted keys, every default inside its range; the `influence` defaults equal the GDD §10a values (`storm_pressure_seed` -0.3, `pressure_anomaly_decay` 0.7, `pressure_anomaly_max` 0.5, `max_delta_*` 0.3/0.3/0.15, `max_active` 64, ...), `Int` rows must be whole, the `Enum` row maps `none`/`linear`/`smooth`; no `ground_water.drain_rate` row (a file carrying it is an unknown-key error); defaults equal the GDD section 14 values for the 19 remaining GDD constants plus `cloud_decay` 0.5 (owned here); `init.initial_moisture_water` 0.8 and `init.initial_moisture_land` 0.3 (`Float`, [0, 1]); `boundary.edge_inflow` `Bool` default 1 | Not Started | sonnet | The GDD section 14 values are copied into the test as literals, not read from the table, so the test cannot pass vacuously. Task text amended 2026-11-17 (A5 Hydrology) (was 24 rows / 20 GDD constants) and again (row 6d). Do not invent any range for the 19 GDD constants: take each from the GDD range column. The three extension rows take the ranges and defaults from amendment 2 |
| 4 | RED then GREEN: `LoadWeatherParameters` valid paths and the envelope | AC 5, 7: valid full file; empty `weather_parameters`; missing group; missing key in a present group; min and max boundary values valid; `edge_inflow` as 0, 1, `true` and `false` | Not Started | sonnet | Valid-path only, so Task 5 can drive the failure paths RED |
| 5 | RED then GREEN: `LoadWeatherParameters` failure paths, table-driven | AC 6: below range and above range generated from the table for every `Float` row; `edge_inflow` values other than exactly 0 or 1 rejected; string where a number is expected; NaN and Inf; `snow_threshold > melt_threshold`; unknown key in a known group; unknown group; wrong `format`; wrong `version`. Each returns false, sets `out` to defaults, names the first offending dotted key | Not Started | sonnet | Strings for NaN/Inf: JSON cannot carry them, so build the `Json::Value` programmatically. Commit |
| 6 | RED then GREEN: `ApplyOverrides` and `SetParam` | AC 8, 9: sparse object changes only the named keys; one bad key among good ones changes none (`memcmp` of `inOut` before and after); `SetParam` applies one value; out-of-range, unknown key and `snow_threshold` over `melt_threshold` return false and change nothing | Not Started | sonnet | Atomicity is the point: apply to a copy, then assign. Commit |
| 7 | RED then GREEN: `IBiomeSeasonalBaseline`, `TableBiomeSeasonalBaseline` compiled default table and `GetSeasonalTemperature` | AC 10, 12: all 13 x 4 in [0, 1]; summer warmer than winter for every land biome (every biome except `Ocean` and `Lake`, no exemptions); biome value 13 and 255 return 0.5 with no out-of-bounds read | Not Started | sonnet | **Authors the 52 default numbers** from GDD A4 section 3 and biome climate. They steer `snow_threshold` 0.3 and `condensation_temp` 0.4, so record the reasoning per biome in a comment. "Land" is defined in amendment 3 |
| 8 | RED then GREEN: `TableBiomeSeasonalBaseline::Load` | AC 11: all 13 biomes true; one missing, unknown `BiomeId`, enum name instead of id, value 1.2, duplicate: false with the table unchanged | Not Started | sonnet | Local table plus `seen[13]`, commit last. Commit tasks 7 and 8 together |
| 9 | RED then GREEN: `SeasonalTargetTemperature` | AC 15 (helper half): latitude 0 gives the baseline; latitude 1 gives `baseline * (1 - factor)`; clamps to [0, 1] | Not Started | sonnet | The helper is exported in the header; Feature 2 consumes it |
| 10 | RED then GREEN: `WeatherSimulation` shell and `Initialise` rejection paths | AC 13: no water-source cell returns false and the simulation reads uninitialised; inconsistent terrain returns false; cell count 0 returns false; none throw or assert in a test build | Not Started | sonnet | Build into a temporary and swap on success. State unchanged on failure (EC-01). An accessor for "initialised" and the committed fields is needed by the tests; keep it minimal |
| 11 | RED then GREEN: `Initialise` seeding on a hand-built 6 x 4 terrain | AC 14, 15 (seeding half), 16: spring baseline times latitude modifier per cell; water-source cells `initial_moisture_water`; land `initial_moisture_land`; pressure 0.5; the other seven fields (incl. `pressureAnomaly`) 0; a world with `waterSource` and `biome` only gives valid state | Not Started | sonnet | Known answers computed by hand in the test, not by calling the production helper. Seeding is a single private `SeedFields(Season, ...)` called with `Season::Spring`; see Implementation Patterns. Empty `BuildGridTables()` seam called last |
| 12 | RED then GREEN: re-`Initialise` and determinism | AC 17, 18: re-`Initialise` on a different-sized terrain equals a fresh simulation on that terrain (`memcmp` of every field and the terrain copies); the same terrain twice gives bit-identical fields; the caller's terrain is not retained (mutate it after the call, state unchanged) | Not Started | sonnet | Commit tasks 9 to 12 as one logical group |
| 13 | Author `Assets/CoW/Global/Weather/weather_parameters.json` with the envelope, all 41 values (the 23 plus the `influence` group; amended 2026-11-17 (row 6d)) and the 13 x 4 `baseline_table` keyed by `BiomeId`; add a test that loads the shipped file and compares it to `DefaultWeatherParameters()` and the compiled baseline | AC 4, 5, 11 against the real file | Not Started | sonnet | The compiled defaults and the file must not drift: the test is the guard. Register the asset via the usual asset path, not by hand. Commit |
| 14 | Exhaustive test pass: gap audit of AC 1 to 18 against the written tests, then edge cases the spec implies but does not number | Gap list in notes. Edge cases: overrides that name a key twice in different groups; an override of a non-object `weather_parameters`; `baseline_table` fault inside an otherwise valid file leaving `out` at defaults; a `Json::Value` integer where a float is expected; `Initialise` after a failed `Initialise` keeping the earlier world; 1 x 1 terrain with a water source; a terrain with every cell a water source | Not Started | sonnet | No mutation check (project rule 2026-11-12). Add only the tests the audit shows missing |
| 15 | Observation Opportunity Scan over every file touched, across the five DiaObservation pillars (logs, traces, profiling, metrics, health) | Suggestions listed in notes | Not Started | haiku | Non-blocking; feeds Feature 5. The spec says the module logs, so this feature adds no `DIA_LOG_*`. Record that as the finding rather than adding code |
| 16 | Scoped verification: `dia run googletest --filter="Weather*:BiomeSeasonalBaseline*" --shards 2`; `dia check deps`; `dia docs precommit` | All AC 1 to 18 quoted; pass/fail and failing names only | Not Started | haiku | Report pass/fail, failing names and errors only. Feature-wide scoped run only; the full suite is in worldstep.plan.md Weather-stack close-out (2026-11-17, speed-ups) |
| 17 | Close-out: `dia docs plan` rows to Done with notes, then `dia docs spec-done docs/specs/applications/cow/systems/a4-weather/weather-data.md` | Spec status Done, plan header updated | Not Started | haiku | **Do not run until all rows above are Done.** Not to be run when this plan is written |

## Open items

1. **Ranges for the three extension rows.** Resolved by amendment 2 (2026-11-16): moisture seeds 0.8 and 0.3, range [0, 1], `Float`; `edge_inflow` is `Bool`, default 1, exactly 0 or 1. The two moisture defaults may be tuned by Feature 6's regression tests (tuning log).
2. **Which biomes are "land" for AC 10.** Resolved by amendment 3 (2026-11-16): every biome except `Ocean` and `Lake` (11 biomes), no exemptions.
3. **String type for `ParamError`.** Resolved by amendment 4 (2026-11-16): `String256` for error text, `String64` for keys; Task 0 confirms the names.
4. **Where `weather_parameters.json` is read from** is outside this feature (Feature 3 reads the `.diaapp` config), but the asset location needs to match the asset pipeline. Task 13 follows the existing CoW `Global` asset convention unless told otherwise. Still open.
5. **`Reseed` timing.** Resolved by amendment 5 (2026-11-16): `SeedFields` is private here; `Reseed` is Feature 6's and is not added in this plan.
- **Review 2026-11-16:** `weather_parameters.json` is at `Assets/CoW/Global/Weather/` (decision 11).
