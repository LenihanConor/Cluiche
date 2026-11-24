# Plan: A4 Weather Data & Parameters (A4 Weather Feature 1)

**Spec:** @docs/specs/applications/cow/systems/a4-weather/weather-data.md
**Status:** In Progress
**System plan:** @docs/specs/applications/cow/systems/a4-weather/a4-weather.plan.md (row 1)

**Re-planned 2026-11-24 against the Draoi layer amendment (2026-11-21) and the world-sim review amendment (2026-11-18).** Supersedes the 2026-11-18 plan, which put this feature in `CoW/Shared/Weather/`. 17 rows became 11 (see "Optimizations").

## What this feature is now

Pure logic: the weather data types, the parameter struct and its binding, `SeasonalTargetTemperature`, and `WeatherSimulation::Initialise`. The code is a **Draoi package, `DraoiHexWorldWeather`**; the data (parameter sidecar, baseline numbers) stays in CoW.

| Piece | Lives in | Tests live in |
|---|---|---|
| `Season`, `WeatherFields`, `WeatherHandoff`, `HydrologyInputs`, `WeatherTerrain`, `SeasonalBaselineInput` | `Draoi/DraoiHexWorldWeather/` (`WeatherTypes.h/.cpp`) | `Tests/GoogleTests/Draoi/TestWeatherTypes.cpp` |
| `WeatherParameters` (plain struct), `BindWeatherParameters` | `Draoi/DraoiHexWorldWeather/WeatherParameters.h/.cpp` | CoW sidecar tests (the binding is checked against the shipped sidecar) |
| `SeasonalTargetTemperature`, `WeatherSimulation` shell, `Initialise`, private `SeedFields`, empty `BuildGridTables()` seam | `Draoi/DraoiHexWorldWeather/WeatherSimulation.h/.cpp` | `Tests/GoogleTests/Draoi/TestWeatherInitialise.cpp` |
| Parameter sidecar `weather_parameters.schema.json` (ranges, defaults, bands, constraints, the 4 baseline season maps) | `Cluiche/Assets/CoW/Global/Weather/` | `Tests/GoogleTests/CoW/Weather/TestWeatherSidecar.cpp` (asserts A4's GDD data, so it is CoW content) |

Namespace `Draoi::HexWorld::Weather`. No `std::` type in a public signature (PD-004). Package depends on `DraoiHexWorld` core and Dia only (not the terrain generator, not DiaDeferredStep). No `SimModule`, bus, save or debug wiring (PD-012/013).

## Changes from the old plan (amendment rewording)

- `IBiomeSeasonalBaseline` / `TableBiomeSeasonalBaseline` are **deleted**. Replaced by `SeasonalBaselineInput { DynamicArray<float> temperature; }`, an optional per-cell baseline array for one season (A1's package fills it at `Capture`). When it is absent, `Initialise` computes the baseline from the sidecar table carried in `WeatherParameters` (`seasonalBaseline[season][biome]`).
- Biomes are `BiomeIndex` and a `BiomeRegistry` from `DraoiHexWorld`. `Initialise` takes the registry so an out-of-range or unknown biome returns the registry's `Role(BiomeRole::Fallback)` value; `kNoBiome` returns 0.5 (EC-19).
- `HydrologyInputs` is `{ surfaceWetness, openWater, glacierEligible }` owned here (per the A5 amendment); the old `glacierCheckWeek` / `glacierSnowFloor` / `present` members are kept as types-only until A5's plan says otherwise (check at Task 2).
- The IParallelExecutor move is `Dia/DiaParallelExecutor` (Done 2026-11-23), not DiaThreading. Not used by this feature.

## Foundation findings (was Task 0; done 2026-11-24, read-only)

Confirmed present: `Dia::Attribute::ParameterFile` (`Load`, `ApplyPack`, `Set`, `Reload`, `ToJson`, `Hash`, `Values`, `Schema`), `ParamBinding<T>` (`Bind` float/int32/bool, `BindEnum`, `BindMap` array and first-element forms, `Resolve`, `Snapshot`, `BindingReport::IsClean`), `Draoi::HexWorld::BiomeRegistry` (`Count`, `Find`, `Role`, `HasTag`/`FindTag`, `TagCount`/`TagName`, `ResolveOrFallback`, `kMaxBiomes` 254), `WaterSource` and `BiomeIndex`/`kNoBiome` in `DraoiHexWorld/CellVocabulary.h`, `DraoiHexWorld/Testing/BiomeTestRegistry.h` (`SetTestAssetPaths` precedent), `dia scaffold module` with a Draoi parent. Sidecar precedent: `Cluiche/Assets/CoW/Global/WorldGen/Rules/terrain_rules.schema.json` and `TerrainRules.cpp` (`BindMap` for fixed-size terrain-class maps).

**One gap (old open item 1, now answered):** `BindMap` needs the array length **at compile time** equal to the schema's expansion (`countMismatches` otherwise). The four baseline season columns have a **runtime** length (registry count, 13 today). The first-element overload takes `float T::*`, which cannot name element 0 of an array member. So the baseline columns cannot be bound with today's API.
- **Proposed fix (AD-001, additive Dia edit, ask first):** a `ParamBinding::BindMap(key, E (T::*arr)[N], uint32_t count)` overload (count at most N; N is the capacity), used here as `float seasonalBaseline[4][...]`-style rows of capacity `BiomeRegistry::kMaxBiomes`. Task 7 is blocked until this is signed off. If refused, fall back: A4 reads the four columns from `ParameterFile::Values()` by attribute index at `Capture` and the coverage test excludes them from `unboundParameters` (worse: `Resolve` stops being clean).
- Tasks 1 to 6 do not depend on it.

## Implementation Patterns

### Types (Task 2)
- `enum class Season : uint8_t { Spring, Summer, Autumn, Winter }`.
- `WeatherFields`: ten `DynamicArray<float>` (`moisture, temperature, pressure, windDirection, windSpeed, precipitation, cloudCover, waterToGround, snowPack, pressureAnomaly`), heap arrays sized at `Resize(uint32_t n)`, as `WorldCellFields` does (confirm the container choice against it). `Resize` assigns zeros: a smaller second `Resize` leaves no stale values.
- `WeatherHandoff` (`glacierInput`, `influenceWater`; `Resize`) and `HydrologyInputs` are types only.
- `WeatherTerrain`: `elevation`, `latitude`, `waterSource` (`WaterSource`), `biome` (`BiomeIndex`), `width`, `height`, `IsConsistent()` (false on any length mismatch and on a cell count of 0).

### Parameters (Tasks 5 to 8)
- The sidecar is the **only** place a parameter's kind, range, default, bands and description live (`format` and `for` `cow.weather_params`; keys `cow.weather.params/<key>`). Catalogued in `assets.catalogue.json`, resolved through `PathStore`. No `weather_parameters.json`, no compiled fallback, no A4 load/override/set code (DiaAttribute Feature 8 tests that machinery; this feature tests A4's data).
- Rows: the 19 GDD constants in the section 14 groups plus `solar.cloud_decay` 0.5 [0, 1]; `init.initial_moisture_water` 0.8, `init.initial_moisture_land` 0.3 [0, 1]; `boundary.edge_inflow` bool true; the 13 generic `influence.*` caps (the exact list and defaults in the old spec section "Influence block": `max_strength`, `max_delta_pressure/moisture/temperature`, `max_lifetime_weeks`, `max_active`, `preview_fade_days`, `pressure_anomaly_decay`, `pressure_anomaly_max`, `cap_water`, `max_radius`, `max_set_cells`, `ground_water_factor`); four baseline `map` entries keyed `keyRef: "cow.biomes/biomes[].id"`. Counts are recounted at build.
- Not here: `ground_water.drain_rate` (A5), any `INFLUENCE_STORM_PRESSURE` row, per-kind influence values (Feature 7's catalogue), westerly band and orographic factor (S-1's parameters).
- Constraint: `precipitation.snow_threshold <= snow.melt_threshold`.
- `WeatherParameters` is a standard-layout struct of `float`/`int32_t`/`bool` plus the baseline table. `BindWeatherParameters(ParamBinding<WeatherParameters>&, uint32_t biomeCount)` is the only fill. `Snapshot` runs once per step at `Capture` (Feature 2/3 call it).

### Seeding (Tasks 3, 4)
- `bool Initialise(const WeatherTerrain&, const WeatherParameters&, const BiomeRegistry&, const SeasonalBaselineInput* baseline = nullptr)`.
- Builds into a temporary and swaps on success: a failed call leaves the previous state unchanged (EC-01, inconsistent terrain, a baseline array whose length is not the cell count).
- One private `SeedFields(Season, params, registry, baseline)`; `Initialise` calls it with `Season::Spring`; Feature 6's `Reseed` will call it with the preset's season (not added here).
- `BuildGridTables()` is a private seam called last in `Initialise`, empty here (Feature 2 fills it).
- No input for S-1 climate layers anywhere (AC 16).

### Testing
- Draoi tests build `WeatherParameters` by hand (only the fields they read) and use `BiomeTestRegistry.h`; they never load the sidecar, so they need no CoW path.
- CoW sidecar tests use one process-wide cached `ParameterFile` (load once), asserted against GDD literals copied into the test.
- Byte identity uses `memcmp` over field arrays, not float `==`.

### Verification cadence
- Per task: `dia run googletest --filter="WeatherTypes*:WeatherInitialise*:WeatherSidecar*" --shards 2`.
- Feature-wide `--filter="Weather*"` once in Task 10. Full suite once in `worldstep.plan.md` Weather-stack close-out. No `SLOW_` suites, sanitizer on request only, no mutation checks. World-sim checks use cell data, not screenshots.

## Optimizations (2026-11-24)

1. **17 rows to 11.** Task 0 (read-only prerequisite check) is done in this re-plan. Old tasks 3, 4 and 5 (three sidecar authoring passes with a placeholder baseline table) are two: the real baseline numbers are authored once, so there is no throwaway table. Old 9 to 12 are two cycles. Old 14 and 15 and the vcxproj task are folded into neighbours. Fewer rows means fewer GoogleTests link cycles (the exe is large).
2. **No sidecar in the Draoi tests.** Seeding, types and the latitude helper are tested with hand-built `WeatherParameters`. This also removes the need for an injected-path test host in this package.
3. **Per-biome lookup built once.** `SeedFields` resolves each biome to its baseline value once (a small array of at most 254 floats), then fills cells by index. No per-cell virtual call (the old interface) and no per-cell registry fallback call.
4. **One cached `ParameterFile` per process** in the CoW sidecar tests instead of a load per test; the table-driven loops over `Schema()` generate the range, default, description and bound-coverage cases for every row, so adding a constant needs no new test.
5. **Write each group's tests together, prove RED once, then GREEN** (one build per group instead of one per sub-test).
6. **Drop duplicated coverage.** The old plan's `Hash()`/`Snapshot` checks stay (they guard struct-versus-sidecar drift) but are one table-driven test; broken-sidecar fixtures are one data table.
7. **Types and `Initialise` can be built and verified before the AD-001 answer**, so the sign-off wait costs nothing.

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Create package `DraoiHexWorldWeather` **by hand, modelled on `DraoiHexWorldTerrain`**: `dia scaffold module` only adds a sub-module inside an existing package (dry-run 2026-11-24: `DraoiHexWorld Weather` gives `Draoi/DraoiHexWorld/Weather/`), it does not create a new package project. Hand-made: `DraoiHexWorldWeather.vcxproj` + `.filters`, module doc `dia.draoi.hexworldweather` (dependencies `draoi.hexworld`, `dia.core`, `dia.maths`, `dia.attribute`; `layer: draoi`), a `Cluiche.sln` entry and the GoogleTests/CoW project references and include dirs (copy the terrain package's; the recipe is commit `57047287e`, "add empty DraoiHexWorldTerrain project": the `.vcxproj`, `Cluiche.sln` project entry, and `DraoiHexWorldTerrain.lib` in the `AdditionalDependencies` of `CoW.vcxproj`, `GoogleTests.vcxproj` and any other consumer). Create the empty file pairs, the three test files (`TestWeatherTypes`, `TestWeatherInitialise` in `Tests/GoogleTests/Draoi/`; `TestWeatherSidecar` in `Tests/GoogleTests/CoW/Weather/`) and the `Assets/CoW/Global/Weather/` folder; register with `dia docs vcxproj-add`; `dia check sln-sync`, `dia docs registry` | Solution builds with the empty files; `dia check deps` and `dia check arch` clean | Done | haiku | Mechanical, TDD-exempt. New Draoi package needs no AD-001 sign-off. The GoogleTests project must reference the new package; Package built by hand (scaffold only adds sub-modules); sln, GoogleTests refs, root dependent_modules, registry done; deps+arch clean; GoogleTests builds |
| 2 | RED then GREEN: `Season`, `WeatherFields::Resize`, `WeatherHandoff`, `HydrologyInputs`, `WeatherTerrain::IsConsistent`, `SeasonalBaselineInput` | AC 1, 2: ten arrays sized and zero-filled; a smaller second `Resize` leaves no stale values; `WeatherHandoff::Resize`; each terrain array wrong in turn, `width*height == 0`, all consistent | Not Started | sonnet | Check A5's amended `HydrologyInputs` field list before fixing the members. Prove RED with quoted output |
| 3 | RED then GREEN: `WeatherParameters` struct (every sidecar row as a member, baseline table of capacity `kMaxBiomes` per season), `SeasonalTargetTemperature`, `WeatherSimulation` shell and the `Initialise` rejection paths | AC 13, 15 (helper): latitude 0 gives the baseline, latitude 1 gives `baseline * (1 - factor)`, clamps to [0, 1]; no water source returns false and the simulation reads uninitialised; inconsistent terrain, cell count 0 and a wrong-length `SeasonalBaselineInput` return false; none throws or asserts in a test build | Not Started | sonnet | Build into a temporary, swap on success (EC-01 state unchanged). The helper is exported; Feature 2 consumes it |
| 4 | RED then GREEN: `SeedFields` and re-`Initialise` | AC 14, 15 (seeding), 16, 17, 18 on a hand-built 6 x 4 terrain: spring baseline times the latitude modifier (answers computed by hand in the test); water-source cells `initial_moisture_water`, land `initial_moisture_land`; pressure 0.5; the other seven fields 0; baseline from `SeasonalBaselineInput` wins over the table; out-of-range and unknown `BiomeIndex` return the fallback biome value, `kNoBiome` 0.5; re-`Initialise` on a different-sized terrain equals a fresh simulation (`memcmp`); same terrain twice is bit-identical; caller's terrain is not retained | Not Started | sonnet | One private `SeedFields(Season, ...)`, empty `BuildGridTables()` last. Biome-to-baseline lookup built once (Optimization 3). Commit tasks 3 and 4 as one logical group |
| 5 | RED then GREEN: author `weather_parameters.schema.json` core rows and the sidecar test host: the 19 GDD constants, `solar.cloud_decay`, the two `init` rows, `boundary.edge_inflow`, the 13 generic `influence.*` caps, the `snow_threshold <= melt_threshold` constraint, bands where the GDD names them; catalogue the file | Amended AC 3, 4, 4a, 5, 6, 9 (table-driven over `Schema()`): strict `ParameterFile::Load` of the shipped sidecar succeeds; every entry has a range, default and description; defaults equal GDD section 14 literals copied into the test, plus `cloud_decay` 0.5, `init` 0.8 and 0.3, `edge_inflow` true, influence defaults per GDD 10a; no `ground_water.drain_rate`, no `INFLUENCE_STORM_PRESSURE` in any spelling; every default inside its range; range min/max accepted; `Set` of `snow_threshold` above `melt_threshold` refused naming both keys, a pack breaking the constraint rejected whole | Not Started | sonnet | Take every range from the GDD range column; invent none. GDD values are literals in the test. Tests assert A4's data, not `ParameterFile` internals |
| 6 | RED then GREEN: the four baseline season `map` entries with the real numbers | Amended AC 10, 11, 12: `Load` against the shipped registry succeeds; against the extra-biome fixture registry it fails naming the missing biome key; a sparse pack overriding one biome is valid; 1.2 in a pack is clamped with a warning; every value in [0, 1]; summer warmer than winter for every biome carrying registry tag `land` (enumerate tags through the registry, never name a biome); `dia validate schema` reports no `data.incomplete-map` | Not Started | sonnet | Authors the 13 x 4 numbers once (no placeholder table) from GDD A4 section 3 and biome climate, reasoning per biome in the entry description; they steer `snow_threshold` 0.3 and `condensation_temp` 0.4 (spec ODQ 2). Never name a biome in code or tests |
| 7 | RED then GREEN: `BindWeatherParameters` over scalars and the baseline columns | Amended AC 5: `Resolve` reports clean (no unbound parameter, no binding without an entry, no count or kind mismatch); `Snapshot` fills every member (a distinct value per parameter via `Set`, then compared member by member, int and bool included); `Snapshot` reflects an active modifier; `Hash()` equal across two loads and different when the last parameter in schema order changes; appended biome (extra-biome fixture) is served with no code change | Not Started | sonnet | **Blocked on the AD-001 answer for the `BindMap(key, arr, count)` overload (see Foundation findings).** The scalar half can go first. The struct and sidecar can no longer drift: the coverage test is the guard |
| 8 | RED then GREEN: broken-sidecar fixtures (one data table) | Amended AC 6: a default out of range, a missing default, a violated constraint, a bad band and a wrong `format` each make `ParameterFile::Load` return false naming the first key and leave the file unloaded | Not Started | sonnet | Proves strict-no-fallback on A4's own sidecar shape. The module's Failing health reaction is Feature 3's. Commit tasks 5 to 8 as one logical group |
| 9 | Exhaustive pass: gap audit of AC 1 to 18 (amended set) against the written tests, then spec-implied edges: a pack with a key in the wrong group, a map pack naming an unknown biome key (dropped with a warning), a map column with the registry's last row, `Initialise` after a failed `Initialise` keeping the earlier world, 1 x 1 terrain with a water source, every cell a water source, biome index at `Count()` and at 255 | Gap list in notes; added tests pass | Not Started | sonnet | No mutation check. Add only what the audit shows missing. No `ParameterFile` internals |
| 10 | Scoped verification and checks: `dia run googletest --filter="Weather*" --shards 2`; `dia check deps`; `dia check arch`; `dia docs precommit` (includes `dia validate schema`); Observation Opportunity Scan over touched files | Pass/fail and failing names only; scan suggestions in notes | Not Started | haiku | The package logs nothing (the module logs), so the expected scan finding is "none added". Full suite is in worldstep.plan.md close-out |
| 11 | Close-out: `dia docs plan` rows to Done; `dia docs spec-done docs/specs/applications/cow/systems/a4-weather/weather-data.md`; update the system plan row 1 and the backlog row | Spec Done, plan header updated | Not Started | haiku | Do not run until every row above is Done |

**AC coverage.** AC 1, 2: Task 2. Amended AC 3, 4, 4a, 5, 6, 9: Tasks 5, 8. Amended AC 7, 8 (boundaries, sparse pack): Tasks 5, 6. Amended AC 10, 11, 12 (baseline): Tasks 4, 6. AC 13: Task 3. AC 14, 15, 16: Tasks 3, 4. AC 17, 18: Task 4. Snapshot fills every member, Hash stable: Task 7.

## Open items

1. **AD-001: additive `ParamBinding::BindMap(key, arr, count)` overload** (Task 7). Ask the user before Task 7.
2. Where per-kind influence values live is Feature 7's call; this plan keeps only the generic caps. `default_falloff` is ambiguous (generic default or per-kind column); the spec does not say.
3. The westerly band latitudes and `terrain.orographic_factor` are S-1 parameters; Feature 2's Task 0 records their names.
4. Baseline numbers are authored here (spec ODQ 2). If Feature 6's ratios regress, tune this table before touching GDD constants.
5. Amendment 5 resolves old ODQ 1 and 3; no open design question remains from the spec. The spec body still carries stale "41 rows", `ParamError` and `ground_water` text; the amendment wins.
