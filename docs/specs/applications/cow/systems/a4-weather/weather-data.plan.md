# Plan: A4 Weather Data & Parameters (A4 Weather Feature 1)

**Spec:** @docs/specs/applications/cow/systems/a4-weather/weather-data.md
**Status:** Not Started
**System plan:** @docs/specs/applications/cow/systems/a4-weather/a4-weather.plan.md (row 1)

Rebuilt 2026-11-18 against the world-sim review amendment. The spec's parameters are now a **sidecar plus
DiaAttribute `ParameterFile` and `ParamBinding<WeatherParameters>`**. There is no A4 descriptor table, no
`LoadWeatherParameters` / `ApplyOverrides` / `SetParam` / `DefaultWeatherParameters`, no
`weather_parameters.json` and no compiled fallback. This feature **authors data and binds it**; it does not
re-test the load, pack, set, reload, `ToJson` or `Hash` machinery (DiaAttribute Feature 8 tests that).

Pure logic on hand-built hex terrain, with no module, bus or thread. **Needs these plans Done:**
- `parameter-files.plan.md` (Dia): `ParameterFile`, `ParamBinding<T>`, `AttributeSchema::FromValueSchema`,
  sidecar bands, map defaults and `constraints`, `Hash()`.
- `biome-registry.plan.md` (S-1): `BiomeRegistry`, `BiomeIndex`, tags and roles, the `BiomeTestRegistry.h`
  fixture, `data.incomplete-map`.

Everything else in this plan is startable on those two. The remaining foundation plans are not needed here.
`ca-pipeline.plan.md` needs this plan's types, sidecar, binding and `Initialise` tasks Done (it extends
`WeatherSimulation`). `presets-and-regression.plan.md` needs this plan fully Done for its pack and baseline
tasks.

All code is CoW (application) code under `Cluiche/CoW/Shared/Weather/`. No Dia files are added or edited
(AD-001). If a foundation gap forces a Dia edit, stop and ask. Tasks that could need more than sonnet say
"ask before escalating" (feedback_confirm_opus).

## Implementation Patterns

### Types (Task 2)
- `Cluiche/CoW/Shared/Weather/WeatherTypes.h/.cpp`, namespace `CoW::Weather`.
- `enum class Season : uint8_t { Spring, Summer, Autumn, Winter }`.
- `WeatherFields`: ten `DynamicArrayC<float>` (`moisture, temperature, pressure, windDirection,
  windSpeed, precipitation, cloudCover, waterToGround, snowPack, pressureAnomaly`), `void Resize(uint32_t n)`
  zero-fills and fully replaces (a smaller second `Resize` leaves no stale values: assign, do not just resize).
- `WeatherHandoff` (`glacierInput`, `influenceWater`; `Resize`) and `HydrologyInputs` (adds
  `glacierCheckWeek` and `glacierSnowFloor`) are types only here.
- `WeatherTerrain`: `elevation`, `latitude` (`DynamicArrayC<float>`), `waterSource` (`DynamicArrayC<uint8_t>`),
  **`biome` (`DynamicArrayC<BiomeIndex>`, from `CoW::World`; amendment 3)**, `width`, `height`,
  `bool IsConsistent() const` (false on any length mismatch with `width*height`, and on a cell count of 0).
- No `std::` type in a public signature (PD-004).

### Sidecar and binding (Tasks 3 to 7)
- The sidecar is `Cluiche/Assets/CoW/Global/Weather/weather_parameters.schema.json` (`format` and `for`
  `cow.weather_params`; param schema keys `cow.weather.params/<key>`). It is the **only** place a parameter's
  kind, range, default, bands and description live. It is catalogued in `assets.catalogue.json` and resolved
  through `PathStore` (amendment 7). No `weather_parameters.json` exists.
- Rows, as `kind: parameter` entries (counts are **recounted at build**; the old "41 rows" is gone):
  - the 19 GDD constants in the section 14 groups (`solar`, `pressure`, `wind`, `evaporation`, `transport`,
    `orographic`, `precipitation`, `snow`), ranges from the GDD range column, plus `solar.cloud_decay`
    (0.5, [0, 1]);
  - `init.initial_moisture_water` 0.8 and `init.initial_moisture_land` 0.3 (float, [0, 1]);
    `boundary.edge_inflow` (bool, default true);
  - the **generic influence caps** (amendment 6): `max_strength` 1.0 [0.5, 2.0]; `max_delta_pressure` 0.3,
    `max_delta_moisture` 0.3, `max_delta_temperature` 0.15 [0.05, 0.5]; `max_lifetime_weeks` 13 [1, 52] int;
    `max_active` 64 [8, 256] int; `preview_fade_days` 1 [0.25, 3]; `pressure_anomaly_decay` 0.7 [0.5, 0.9];
    `pressure_anomaly_max` 0.5 [0.2, 0.5]; `cap_water` 1.0 [0, 2]; `max_radius` 32 [0, 64] int;
    `max_set_cells` 8192 [64, 65536] int; `ground_water_factor` 0.05 [0, 1]. The per-kind rows
    (`storm_pressure_seed`, `storm_moisture`, `temp`, `moisture`, falloff) are **not** here: they are the
    kind catalogue, authored by `weather-influence.plan.md`;
  - the four baseline maps (Task 5).
- No `ground_water.drain_rate` row (A5 owns it) and no `INFLUENCE_STORM_PRESSURE` row in any spelling.
- Westerly band latitudes and `terrain.orographic_factor` are **read from S-1's parameters, not owned here**
  (amendment 6). No row for them in this sidecar.
- Sidecar `constraints`: `precipitation.snow_threshold <= snow.melt_threshold`. Add others only if the GDD
  states one. Bands: where the GDD range column carries named bands, record them as sidecar `bands`.
- `WeatherParameters` is a plain struct of `float`/`int32_t`/`bool` members, one per sidecar parameter. Its
  fill is `ParamBinding<WeatherParameters>::Bind(...)` calls, one `Resolve`, and `Snapshot` once per step at
  `Capture` (Feature 2/3 call it). The binding lives in `WeatherParameters.cpp` as
  `BindWeatherParameters(ParamBinding<WeatherParameters>&)`.
- The tests assert the **A4 data**, not the machinery:
  - every sidecar entry has a description and a range;
  - defaults equal the GDD section 14 values, **copied into the test as literals**, so the test cannot pass
    vacuously;
  - the constraint fixture: `Set` of `snow_threshold` above `melt_threshold` is refused naming both keys;
  - `Resolve` reports no unbound parameter and no binding without an entry;
  - `Snapshot` fills every member (each parameter set to a distinct value, then compared);
  - `Hash()` is the same across two loads and changes when the last parameter changes.
- A sidecar fault is not handled here: `ParameterFile::Load` fails and Feature 3's module goes Failing. This
  feature only supplies broken-sidecar fixtures to prove the fault names the key.

### Baseline (Tasks 5, 8)
- `Cluiche/CoW/Shared/Weather/BiomeSeasonalBaseline.h/.cpp`. `IBiomeSeasonalBaseline` with
  `GetSeasonalTemperature(BiomeIndex, Season)`; `TableBiomeSeasonalBaseline final` fills a
  `float[Season][biomeCount]` from the sidecar's four season maps. It is a **fallback only**: A1's provider
  wins when present (amendment 2).
- Each season column is one `map` entry keyed `keyRef: "cow.biomes/biomes[].id"`, with a `default` object
  that lists **every** registry biome (completeness is checked by `dia validate schema` as
  `data.incomplete-map` and by `ParameterFile::Load`). No A4 completeness code is written.
- An out-of-range or unknown `BiomeIndex` returns the registry `fallback` biome's value (logged once by the
  registry's `ResolveOrFallback` rule); **0.5 is returned only for `kNoBiome`** (EC-19, spec-6 resolution 3).
- "Land" for the summer-warmer-than-winter rule is **every biome carrying the registry tag `land`**; no
  exemptions. The test enumerates tags through the registry, never names a biome.
- The 13 x 4 default numbers are authored in the sidecar (Task 8) from GDD A4 section 3 and biome climate, with
  the reasoning per biome in the entry's `description`. They steer `snow_threshold` 0.3 and
  `condensation_temp` 0.4.
- Data-only proof: with the extra-biome fixture registry, loading the unchanged sidecar fails naming the
  missing key (completeness), and the unchanged `Initialise` code seeds an appended biome without edits.
- Exact way a per-biome map column is read at runtime is a foundation gap (open item 1); Task 0 records what
  `ParameterFile` / `ParamBinding` actually expose.

### Latitude helper (Task 9)
- `float SeasonalTargetTemperature(float baseline, float latitude, float latitudeCoolingFactor)` in
  `WeatherSimulation.h`, free function, exported. `clamp(baseline * (1 - factor * latitude), 0, 1)`.
  Feature 2's solar pass must call it (a Feature 2 test asserts that).

### WeatherSimulation::Initialise (Tasks 10 to 12)
- `Cluiche/CoW/Shared/Weather/WeatherSimulation.h/.cpp`: class shell, owned copies of terrain, committed
  `WeatherFields`, `bool mInitialised`.
- `bool Initialise(const WeatherTerrain&, const WeatherParameters&, const IBiomeSeasonalBaseline&)`.
  `WeatherParameters` here is the already-snapshotted struct. Build into a temporary and swap on success, so a
  failed call leaves the previous state unchanged (EC-01 and inconsistent-terrain cases).
- Seeding lives in one **private** `SeedFields(Season, params, baseline)`. `Initialise` calls it with
  `Season::Spring`; Feature 6's `Reseed` calls it with the preset's season. `Reseed` itself is **not** added
  here.
- `BuildGridTables()` is a named private seam, called last in `Initialise`, **empty** here. Feature 2 fills it.
- No reference to S-1 climate layers anywhere (AC 16): the signature has no input for them.

### Testing
- `Tests/GoogleTests/CoW/Weather/{TestWeatherTypes,TestWeatherParameters,TestBiomeSeasonalBaseline,
  TestWeatherInitialise}.cpp`. Suites: `WeatherTypes*`, `WeatherParameters*`, `BiomeSeasonalBaseline*`,
  `WeatherInitialise*`.
- Tests use the foundation's `BiomeTestRegistry.h` fixture (shipped registry, `B("Biome.Grassland")` lookup,
  plus the extra-biome variant). No test names a biome enum value.
- Hand-built 6 x 4 terrain helper lives inside the test file for now; Feature 6 owns the shared
  `MakeCoastRange` builder.
- Byte-identity checks use `memcmp` over the field arrays, not `==` on floats.

### Verification cadence
- Per task: narrow `dia run googletest --filter="WeatherTypes*:WeatherParameters*:BiomeSeasonalBaseline*:WeatherInitialise*" --shards 2` (always `--shards 2`).
- The feature-wide `--filter="Weather*:BiomeSeasonalBaseline*"` runs only in the scoped-verification task. The
  full suite runs once in worldstep.plan.md, Weather-stack close-out. No `SLOW_` suites. No sanitizer unless
  asked. No mutation checks. World-sim checks use cell data and stats, not screenshots.

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 0 | Prerequisite check, read only: confirm the foundation exists under the expected names. From `parameter-files.plan.md` Done: `Dia::Attribute::ParameterFile` (`Load`, `ApplyPack`, `Set`, `Reload`, `ToJson`, `Hash`), `ParamBinding<T>` (`Bind`, `BindEnum`, `Resolve`, `Snapshot`), `AttributeSchema::FromValueSchema`, `IMapKeySource`/`MapKeys`, sidecar `bands`, map `default` objects and top-level `constraints`. From `biome-registry.plan.md` Done: `CoW::World::BiomeRegistry`, `BiomeIndex`, `kNoBiome`, `Role(Fallback)`, `HasTag`/`FindTag`, `ResolveOrFallback`, `Testing/BiomeTestRegistry.h`, the `data.incomplete-map` CLI rule. Also confirm `dia validate schema` runs on a `cow.*` sidecar, `Dia::Core::Containers::String256`/`String64` names, the `WaterSource` enum location, and the `CoW.vcxproj` layout for `Shared/Weather/`. Record how a per-biome map column is read at runtime (see open item 1) | Findings recorded in notes | Not Started | haiku | Mechanical, TDD-exempt. If a named API is missing or renamed, stop and report which plan is not Done; do not reimplement it. If a Dia edit seems needed, raise it as an AD-001 ask-first item |
| 1 | Create empty file pairs `WeatherTypes`, `WeatherParameters`, `BiomeSeasonalBaseline`, `WeatherSimulation` under `Cluiche/CoW/Shared/Weather/`, the four test files under `Tests/GoogleTests/CoW/Weather/`, and the `Assets/CoW/Global/Weather/` folder; register each with `dia docs vcxproj-add CoW ...` and `dia docs vcxproj-add GoogleTests ...` | Solution builds with the empty files | Not Started | haiku | Mechanical, TDD-exempt. Idempotent; later tasks edit files in place and do not touch the vcxproj again |
| 2 | RED then GREEN: `Season`, `WeatherFields::Resize`, `WeatherHandoff`, `HydrologyInputs`, `WeatherTerrain` (with `BiomeIndex` biomes) `IsConsistent` | AC 1, 2: ten arrays sized and zero-filled; `WeatherHandoff::Resize` sizes `glacierInput` and `influenceWater`; a smaller second `Resize` leaves no stale values; each of the four terrain arrays wrong in turn, `width*height == 0`, all consistent | Not Started | sonnet | Prove RED with quoted output (tests exist, symbols missing or failing). Commit |
| 3 | RED then GREEN: sidecar core groups and the struct. Author `weather_parameters.schema.json` with the 19 GDD constants, `solar.cloud_decay`, the two `init` rows and `boundary.edge_inflow`, with kinds, ranges, defaults, bands and descriptions; define `WeatherParameters` members for them; catalogue the file | Amended AC 3, 4, 5: strict `ParameterFile::Load` of the shipped sidecar (with the biome-registry key list) succeeds; every entry has range, default and description; defaults equal the GDD section 14 literals plus `cloud_decay` 0.5, `init` 0.8 and 0.3, `edge_inflow` true; no `ground_water.drain_rate` entry; every default inside its own range | Not Started | sonnet | GDD values are literals in the test. Take every range from the GDD range column; invent none. Amendment 1 replaces the old AC 3-9, so the tests assert A4's data only. Commit |
| 4 | RED then GREEN: the generic influence caps and the cross-field constraint. Add the 13 generic `influence.*` rows (int and float kinds, defaults per the spec) and `constraints: precipitation.snow_threshold <= snow.melt_threshold` to the sidecar and the struct | Amended AC 4a, 6, 9: `influence` defaults equal the GDD section 10a values (`max_delta_*` 0.3/0.3/0.15, `max_active` 64, `max_lifetime_weeks` 13, `pressure_anomaly_decay` 0.7, `pressure_anomaly_max` 0.5, ...); int rows are whole-number kinds; no `INFLUENCE_STORM_PRESSURE` entry in any spelling; `Set` of `snow_threshold` above `melt_threshold` is refused naming both keys and changes nothing; a pack that breaks the constraint is rejected whole; range min and max boundary values accepted | Not Started | sonnet | Per-kind rows (`storm_pressure_seed`, `storm_moisture`, `temp`, `moisture`, falloff) are deliberately absent: they belong to the kind catalogue (`weather-influence.plan.md`). The tests exercise A4's sidecar through `ParameterFile`; they do not re-prove `ParameterFile` |
| 5 | RED then GREEN: baseline maps in the sidecar. Add four `map` entries (spring, summer, autumn, winter), each `keyRef: "cow.biomes/biomes[].id"`, `float` [0, 1], with a complete `default` object | Amended AC 10, 11: `Load` against the shipped registry succeeds; the same sidecar against the extra-biome fixture registry fails naming the missing biome key (completeness); a sparse pack overriding one biome is valid; a value of 1.2 in a pack is clamped with a warning; `dia validate schema` reports no `data.incomplete-map` | Not Started | sonnet | Numbers are authored in Task 8; this task uses a placeholder complete table so Task 8's tests can drive the real values RED. No A4 completeness or unknown-id code is written (the foundation owns both) |
| 6 | RED then GREEN: bind `WeatherParameters` with `ParamBinding<WeatherParameters>` (`BindWeatherParameters`) | Amended AC 5, 14-equivalent for this struct: `Resolve` lists no unbound sidecar parameter and no binding without an entry; `Snapshot` fills every member (distinct value per parameter set via `Set`, then compared member by member, including int and bool members); `Snapshot` reflects an active modifier; `Hash()` equal across two loads and different when the last parameter in schema order changes | Not Started | sonnet | The struct and the sidecar can no longer drift: the coverage test is the guard. Commit tasks 3 to 6 as one logical group |
| 7 | RED then GREEN: broken-sidecar fixtures | Amended AC 6: each fixture (a default out of range, a missing default, a violated constraint, a bad band, wrong `format`) makes `ParameterFile::Load` return false naming the first key and leaves the file unloaded; there is no fallback struct to read | Not Started | sonnet | Proves the "strict, no fallback" contract on A4's own sidecar shape. The module's health-Failing reaction is Feature 3's task, not this one |
| 8 | RED then GREEN: `IBiomeSeasonalBaseline`, `TableBiomeSeasonalBaseline` fed from the four maps; author the real default numbers | Amended AC 10, 12: every biome in every season in [0, 1]; summer warmer than winter for every biome with tag `land`; an out-of-range or unknown `BiomeIndex` returns the fallback biome's value; `kNoBiome` returns 0.5 and never reads out of bounds; an appended biome (extra-biome fixture) is served with no code change | Not Started | sonnet | **Authors the 52-ish default numbers** (13 biomes x 4 seasons today; the count is the registry's) from GDD A4 section 3 and biome climate; record the per-biome reasoning in the entry descriptions. Never name a biome in code or tests: use registry tags and `B("...")` lookup in fixtures only |
| 9 | RED then GREEN: `SeasonalTargetTemperature` | AC 15 (helper half): latitude 0 gives the baseline; latitude 1 gives `baseline * (1 - factor)`; clamps to [0, 1] | Not Started | sonnet | The helper is exported in the header; Feature 2 consumes it |
| 10 | RED then GREEN: `WeatherSimulation` shell and `Initialise` rejection paths | AC 13: no water-source cell returns false and the simulation reads uninitialised; inconsistent terrain returns false; cell count 0 returns false; none throw or assert in a test build | Not Started | sonnet | Build into a temporary and swap on success. State unchanged on failure (EC-01). Keep the "initialised" and committed-fields accessors minimal. The EC-01 cause code for the stage's `InitialiseSim` hook is Feature 3's |
| 11 | RED then GREEN: `Initialise` seeding on a hand-built 6 x 4 terrain | AC 14, 15 (seeding half), 16: spring baseline times latitude modifier per cell; water-source cells `initial_moisture_water`; land `initial_moisture_land`; pressure 0.5; the other seven fields (incl. `pressureAnomaly`) 0; a world with `waterSource` and `biome` only gives valid state | Not Started | sonnet | Known answers computed by hand in the test, not by calling the production helper. Seeding is the single private `SeedFields(Season, ...)`. Empty `BuildGridTables()` seam called last |
| 12 | RED then GREEN: re-`Initialise` and determinism | AC 17, 18: re-`Initialise` on a different-sized terrain equals a fresh simulation on that terrain (`memcmp` of every field and the terrain copies); the same terrain twice gives bit-identical fields; the caller's terrain is not retained (mutate it after the call, state unchanged) | Not Started | sonnet | Commit tasks 9 to 12 as one logical group |
| 13 | Exhaustive test pass: gap audit of AC 1 to 18 (amended set, see the coverage note) against the written tests, then edge cases the spec implies but does not number | Gap list in notes. Edge cases: a pack with a key in the wrong group; a map pack naming an unknown biome key (dropped with a warning); a map column with the registry's last row; `Initialise` after a failed `Initialise` keeping the earlier world; 1 x 1 terrain with a water source; every cell a water source; biome index at `Count()` and 255 in terrain | Not Started | sonnet | No mutation check. Add only the tests the audit shows missing. Do not add tests of `ParameterFile` internals |
| 14 | Observation Opportunity Scan over every file touched, across the five DiaObservation pillars (logs, traces, profiling, metrics, health) | Suggestions listed in notes | Not Started | haiku | Non-blocking; feeds Feature 5. The module logs, so this feature adds no `DIA_LOG_*`; record that as the finding rather than adding code |
| 15 | Scoped verification: `dia run googletest --filter="Weather*:BiomeSeasonalBaseline*" --shards 2`; `dia check deps`; `dia docs precommit` (includes `dia validate schema` on the sidecar) | All amended AC quoted; pass/fail and failing names only | Not Started | haiku | Feature-wide scoped run only; the full suite is in worldstep.plan.md Weather-stack close-out |
| 16 | Close-out: `dia docs plan` rows to Done with notes, then `dia docs spec-done docs/specs/applications/cow/systems/a4-weather/weather-data.md` | Spec status Done, plan header updated | Not Started | haiku | **Do not run until all rows above are Done.** Not to be run when this plan is written |

**AC coverage (old plan 18 tasks 0-17, now 17 tasks 0-16).** AC 1, 2: Task 2. Amended AC 3, 4, 4a, 5: Tasks 3, 4. Amended AC 6, 9 (constraint and strict-load behaviour on A4's data): Tasks 4, 7. Amended AC 7, 8 (boundary values, sparse pack) : Tasks 4, 5 (the machinery itself is DiaAttribute Feature 8's). Amended AC 10, 11, 12 (baseline): Tasks 5, 8. AC 13: Task 10. AC 14, 15, 16: Tasks 9, 11. AC 17, 18: Task 12. Amendment 1's "Snapshot fills every member, Hash stable": Task 6. Old Tasks 4 to 6 (load, `ApplyOverrides`, `SetParam`) and 13 (shipped JSON) are deleted; their ACs are covered by the foundation or by Tasks 3 to 7.

## Open items

1. **Per-biome map access at runtime.** `ParamBinding<T>` as specced binds scalar members only (`float`, `int32_t`, `bool`, enum). It does not say how a `map` column (`<key>.<mapKey>` definitions) is read into `TableBiomeSeasonalBaseline`. Needs either index access through `AttributeSchema` or a map-binding API in parameter-files. Task 0 records what exists. Not resolved here.
2. **Where per-kind influence values live** (the sidecar, or a record file) is left open by the amendment ("in the weather params sidecar or a record file"). This plan keeps only the generic caps in the sidecar and leaves the choice to `weather-influence.plan.md`. `default_falloff` is the one row that is ambiguous (generic default or per-kind column); the spec does not say.
3. **The westerly band latitudes and `terrain.orographic_factor` are S-1 parameters** (amendment 6, item 16/17), but the S-1 parameter names are not given in either spec. Feature 2 needs them by name; Task 0 of `ca-pipeline.plan.md` records them.
4. **`ParamBinding` has no stated way to report which sidecar `constraints` span a bound int and a float** (e.g. kind mixing in a constraint). Constraint `a`/`b` must be "numeric kind"; the plan assumes int/float mixing is allowed.
5. **Baseline numbers are authored here** (spec Open Q2, unchanged): they steer Feature 6's seasonal-storm and snow-line ratios. If those regress, tune this table before touching GDD constants.
6. **Amendment 5 resolves old Open Q1 and Q3** (ranges in the sidecar; `set` and the cross-field rule through `constraints`), so no open design question remains from the old spec. The amended spec still carries stale mentions of "41 rows", "ParamError" and `ground_water` text in its body and Status; the amendment wins.
7. **Review 2026-11-16 decision 11** (`weather_parameters.json` at `Assets/CoW/Global/Weather/`) now names a file that no longer exists; the sidecar lives in the same folder.
