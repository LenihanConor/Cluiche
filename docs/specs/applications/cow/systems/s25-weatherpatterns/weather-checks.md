# Feature Spec: S-2.5 Weather Checks

**Parent:** @docs/specs/applications/cow/systems/s25-weatherpatterns/s25-weatherpatterns.md (Feature 2)
**Status:** Approved (2026-11-16)
**Design source:** `DesignDocs/COW/Output Docs/GDD/10_PrototypeStrategy/S2.5_WeatherPatterns.md` §4 (AD-06 to AD-09), §5 (Tests 1–9), §6 (pass/fail)
**Depends on:** A4 Feature 1 (`WeatherFields`, `WeatherTerrain`); A4 Feature 5's pure `DroughtDetector` and `StormDetector` and A4 Feature 6's `ComputeWaterDistance` (this feature adds one additive overload, below); GameSpeedController's calendar (an agreement test only). Terrain-dependent prechecks are Feature 1's.
**Consumed by:** Feature 3 (the headless runner supplies a recorded trace, the report and `cow.s25.baseline.record`), Feature 4 (check lines, `CheckRegistry`, scenarios name checks), Feature 5.

## Summary

The nine GDD tests as **pure, named checks** over a trace of A4's committed state, plus the helpers they share and the baseline store. A check takes a trace, the terrain and the world, and returns a typed result; it never touches a module, a bus or a clock. The same predicates run headless for the verdict (Feature 3) and live for understanding (Feature 4). Everything is TDD-able on hand-built hex terrain and **scripted traces** with known answers.

## Goals

1. Every GDD hard check and default magnitude is one named check with a stable id, kind, threshold and the A4 TechSpec section to reopen on a hard fail.
2. Checks are pure and deterministic: the same trace gives the same result and the same `measured` double-for-double.
3. A check that lacks the data it needs (live, with only three tracked layers) reports **Pending**, never a false pass or fail.
4. A default magnitude is compared to the GDD default until a baseline exists, then to the baseline within ±10% relative; a miss is a **MagnitudeMiss**, not a fail.
5. Test 7's and Test 3's definitions are settled here with explicit numbers (decided 2026-11-16).

## Non-Goals

- Running A4, the trace recording, the report, DiaAutomation and performance checks (Feature 3).
- Scenario files, `CheckRegistry` plumbing, check-line UI and walkthroughs (Feature 4).
- Graphs (Feature 5), world selection and prechecks (Feature 1).
- Equation correctness (A4's GoogleTests).

## Design

### Result model

```cpp
namespace CoW::S25 {
    enum class CheckKind  : uint8_t { Hard, Default };
    enum class CheckState : uint8_t { Pass, Fail, MagnitudeMiss, Pending, InvalidWorld };

    struct CheckResult {
        StringCRC  id;                 // "s25.t1.moisture_monotonic"
        uint8_t    test;               // 1..9
        CheckKind  kind;
        CheckState state;
        double     measured;           // NaN when not a magnitude
        double     threshold;          // the GDD default, or the frozen baseline value
        String256  detail;             // "ocean 0.83 > coast 0.71 > inland 0.32"
        String32   reopenSection;      // "A4 TechSpec §5" on a Hard Fail, else empty
    };
}
```

This replaces the system spec's `bool passed`. `InvalidWorld` is returned when a band a check depends on is empty (not an A4 fail; "re-pick the seed"). `Pending` carries a reason in `detail` (`needs weeks 1..52, have 9`).

### The trace

```cpp
class IWeatherTrace {
public:
    virtual uint32_t WeekCount() const = 0;                        // weeks recorded, 1-based week 1 = first published week
    virtual bool     Has(WeatherLayer layer, uint32_t week) const = 0;
    virtual float    Cell(WeatherLayer layer, uint32_t week, uint32_t cell) const = 0;
    virtual const WeatherTerrain& Terrain() const = 0;
    virtual const IRegionMap&     Regions() const = 0;             // S-1 Cell::region; one region if absent
    virtual const WeatherParameters& Parameters() const = 0;
};
```

Two implementers: `RecordedTrace` (Feature 3: the runner copies the committed fields of every week; 52 weeks of 9 fields at Large is about 12.5 MB, a 4-year run about 50 MB, headless only) and `HistoryTrace` (Feature 4's live view over DiaCellHistory's tracked layers, `Has` false for untracked layers or evicted weeks). `ScriptedTrace` ships in `S25/Testing/` for tests: a builder where a test states per-week values for named cell sets.

Every check declares `CheckRequirements { layers; minWeeks; firstWeek; }`. `Evaluate(const IWeatherTrace&, const WorldInfo&)` returns `Pending` when the trace does not satisfy them.

### Helpers (pure)

- **Season table (AD-08).** `SeasonOfWeek(week)` for 1-based week-of-year: Spring 1–13, Summer 14–26, Autumn 27–39, Winter 40–52. An integration test asserts agreement with GameSpeedController: for days 0–363, `week = day / 7 + 1` maps to `Season(SeasonOf(day) % 4)`.
- **Distance fields.** `ComputeDistance(const WeatherTerrain&, SourcePredicate)`: hex BFS over `HexTopology` from every cell the predicate selects (additive overload over A4 Feature 6's `ComputeWaterDistance`, which becomes a call into it). Predicates: `OceanOnly`, `AnyWater`, `MapEdge`. Unreachable cells hold a sentinel and are excluded from bands.
- **Aggregates.** Global and per-region means, snow coverage % (cells with `snowPack > 0.1`, A4's `kSnowCoverThreshold`), always accumulated in **double**, in ascending cell order, so results are bit-reproducible.
- **Storm tracker.** Uses A4's `StormDetector` (connected cells with `pressure < 0.3`). Per week, the **cluster centroid** is the pressure-weighted centroid of the cluster, snapped to the nearest cell (ties to the lowest index). Clusters link week to week by nearest centroid within 6 steps. A cluster lost for 1 week and found again within 2 weeks keeps its track.
- **Dry spells.** Uses A4's `DroughtDetector` with `dry_threshold` 0.02, per S-1 region. One shared definition of "dry" with `cow.weather.find`.

### The nine tests as checks

Bands use Small-map thresholds (data in the checks file, defaults below). Weeks are 1-based weeks after spin-up.

| Test | Id | Kind | Definition | Reopen |
|---|---|---|---|---|
| 1 | `t1.moisture_monotonic` | Hard | 52-week mean moisture: ocean cells (distance 0 from **ocean**) > coast (land, distance 1–2) > inland (distance ≥ 8) | A4 §3–§4 |
| 1 | `t1.precip_weeks` | Hard | mean count of weeks with `precipitation > 0.02` per cell: coast > inland | A4 §3–§4 |
| 1 | `t1.ocean_moisture` / `t1.coast_moisture` / `t1.inland_moisture` | Default | band means ≥ 0.8 / ≥ 0.6 / ≤ 0.4 | |
| 2 | `t2.shadow_present` | Hard | among **crest cells** (land, elevation ≥ 0.6, lower than the prevailing-upwind neighbour's and the downwind neighbour's elevation is lower than the crest's), at least `minShadowCrests` (default 3) have `leeward ≤ 0.5 × windward` (A4's `find rain_shadow` definition, the GDD §6.2 ratio) | A4 §5 |
| 2 | `t2.windward_gt_leeward` | Hard | sum of windward means > sum of leeward means over all crests | A4 §5 |
| 2 | `t2.shadow_repeatable` | Hard | the set of shadow crests is identical in two runs of the same seed (compared by sorted list, run B uses a different range count) | A4 §5 |
| 3 | `t3.centre_moves` | Hard | the centroid differs between at least two of the 6 weeks after release | A4 §2 |
| 3 | `t3.wind_converges` | Hard | at the storm's peak week, at least 4 of the centroid's 6 neighbours have wind within ±60° of the direction toward the centre | A4 §2 |
| 3 | `t3.not_stationary` | Hard | the centroid is not within 1 cell for 10 consecutive weeks after release | A4 §2 |
| 3 | `t3.ends_or_intensifies` | Hard | by the last tracked week the cluster has dissipated (size 0) or its minimum pressure is lower than at release | A4 §2 |
| 3 | `t3.travel_5_cells` | Default | hex distance between centroids at release+5 and release ≥ 5 | |
| 4 | `t4.temp_extremes` | Hard | the 3-week moving mean of global mean temperature peaks in weeks 14–26 and bottoms in weeks 40–52 | A4 §3 |
| 4 | `t4.temp_smooth` | Hard | at most 2 direction changes in the smoothed curve (changes smaller than 1e-4 ignored) | A4 §3 |
| 4 | `t4.winter_wetter` | Hard | mean global precipitation, Winter > Summer | A4 §3 |
| 4 | `t4.melt_crossing` | Hard | mean temperature of mountain cells (elevation ≥ 0.6) crosses from below to above `snow.melt_threshold` during weeks 1–13 of the following Spring | A4 §3 |
| 4 | `t4.winter_summer_ratio` | Default | Winter ÷ Summer precipitation ≥ 1.5 | |
| 5 | `t5.snow_builds_winter` | Hard | mountain mean `snowPack` at week 52 > at week 40 | A4 §7 |
| 5 | `t5.snow_falls_spring` | Hard | mountain mean `snowPack` at week 65 (end of the following Spring) < at week 52 | A4 §7 |
| 5 | `t5.flood_follows_melt` | Hard | the **mountain** `waterToGround` peak week in 53–65 has mean mountain snow below week 52's, and the peak value exceeds week 52's (aligned to locked GDD 2026-11-17, CHANGE-048: GDD Test 5 measures the mountain melt pulse; the downstream lowland flood moved to S-2.6. The id is kept for GDD test-number stability) | A4 §7 |
| 5 | `t5.gw_peak_week` | Default | the mountain `waterToGround` melt peak lies in Spring (53–65); the measured value is the week number. The first run records it (aligned to locked GDD 2026-11-17, CHANGE-048; the id is kept, a rename is TBD) | A4 §7 |
| 6 | `t6.droughts_found` | Hard | `DroughtDetector` at 2 weeks finds at least one region | A4 §4–§6 |
| 6 | `t6.inland_drier` | Hard | mean precipitation of the most distant cells (within 2 of the maximum distance from water) < mean of cells within 2 of water | A4 §4–§6 |
| 6 | `t6.dry_spell_4w` | Default | at least one region has ≥ 4 consecutive dry weeks starting in weeks 14–39 | |
| 7 | `t7.years_differ` | Hard | over 4 years the largest pairwise relative difference in annual global precipitation is ≥ **0.5%** | none: goes to the user (Resolved Decision 3) |
| 7 | `t7.seasonal_shape` | Hard | each year's 4 seasonal-mean precipitation values correlate with the 4-year mean pattern at r ≥ **0.9** | A4 §3 |
| 8 | `t8.toggle_effective` | Hard | upwind-edge cells' mean moisture, weeks 27–52, differs between on and off by ≥ 0.01 | A4 boundary |
| 8 | `t8.edge_on_gt_off` | Hard | that mean is **higher with `edge_inflow` on** (upwind-edge cells keep their moisture; off loses it, A4 Feature 2 AC 25–26) | A4 boundary |
| 8 | `t8.interior_tolerance` | Default | mean absolute moisture difference over interior cells (≥ 8 from the map edge) ≤ 0.05 | |
| 9 | `t9.determinism` | Hard | committed fields byte-identical at weeks 13, 26, 39, 52 between a run with executor A and a run with a different range count; `detail` names the first differing week, field and cell | RNG/iteration order |

**Upwind-edge cells** (Test 8) are cells within 2 of the map edge whose wind-snapped upwind neighbour is off-map in the final week of the on run (A4's `dirIndex`, `NeighbourAt(c, (dirIndex+3)%6) == kNoCell`). The set is computed from the **on** run and used for both.

**Test 3 timing** (decided 2026-11-16). The setup holds the storm for `holdWeeks` (the scenario's, Feature 4) then releases it; every Test 3 check measures **only weeks after release**, from the cluster containing or nearest the spawn cell. Before release the centre is held by the forcing and says nothing about A4.

**Test 7 thresholds** (decided 2026-11-16): 0.5% and r ≥ 0.9, both recorded in the baseline as tunable thresholds (not GDD values). `years_differ` is the check A4's known risk (no RNG) most likely fails; if it does, the result is reported as a Fail with `reopenSection` empty and the report text "goes to the user: add a forcing, accept periodic weather, or drop the test", per the system spec. S-2.5 never invents variability.

### Baseline store

`baseline.json` (envelope `cow.s25_baseline`, version 1, committed to the repo): `{ world: { checkId: { value, thresholdsOnly? } } }`, one entry per Default check per curated world at Small.

```cpp
class BaselineStore {
    bool Load(const Json::Value&, BaselineError&);                  // all-or-nothing, unknown keys invalid
    bool Lookup(StringCRC world, StringCRC check, double& out) const;
    CheckState Compare(const CheckResult& defaultCheck, StringCRC world) const;   // Pass / MagnitudeMiss
    bool Record(StringCRC world, const DynamicArrayC<CheckResult>&, bool force, BaselineError&);   // refuses to overwrite without force
    bool Serialize(Json::Value& out) const;                          // stable key order, 17-digit doubles
};
```

- With **no baseline entry** the check is judged against the GDD default and is `Pass` if met, `MagnitudeMiss` if not; `Record` freezes the measured value either way (the first run records "actual", AD-06).
- With an entry, the check passes when `measured` is within **±10% relative** of the baseline (absolute ±0.01 when the baseline is under 0.1, so a near-zero value can't make the band vanish). Beyond it is `MagnitudeMiss`.
- `Record` without `force` writes only absent entries and leaves existing ones byte-identical; with `force` it replaces all for that world. The command is Feature 3's.

### Check registry hook

`S25WeatherChecks::RegisterAll(CheckRegistry&)` registers every id above with its requirements and evaluate function, as the system spec's interface already sketches. The registry type is Feature 4's; this feature defines the `ICheck` shape it consumes (`Id`, `Kind`, `Requirements`, `Evaluate`).

## Public API

New in `Cluiche/CoW/Shared/Weather/S25/`: `CheckTypes.h` (results), `WeatherTrace.h` (`IWeatherTrace`, `RegionMap`), `S25Helpers.h/.cpp` (season, distance, aggregates, storm tracker, dry spells), `S25WeatherChecks.h/.cpp` (the registered checks), `BaselineStore.h/.cpp`, `Testing/ScriptedTrace.h`. Edited: A4 Feature 6's `ComputeWaterDistance` (additive `ComputeDistance` overload).

## Acceptance Criteria

### Helpers
1. `SeasonOfWeek` maps weeks 1–13/14–26/27–39/40–52 to the four seasons and rejects 0 and 53; for every day 0–363, the season agrees with GameSpeedController's `Season(SeasonOf(day) % 4)`.
2. `ComputeDistance` on a hand-built grid gives exact hex distances for `OceanOnly`, `AnyWater` and `MapEdge`, on odd and even columns; lakes and rivers are not sources for `OceanOnly`; unreachable cells hold the sentinel.
3. Aggregates equal a reference double-precision loop bit for bit; snow coverage counts exactly `snowPack > 0.1`.
4. The storm tracker follows one scripted cluster across 8 weeks, keeps a track across a 1-week gap, and starts a new track when the gap is 3 weeks; centroid ties go to the lowest index.

### Checks: one pass case and one fail case each
5. For **every check id** in the table, a scripted trace that satisfies it returns `Pass` (or `Pass` against the GDD default for Default checks) and a trace that violates it returns `Fail` (Hard) or `MagnitudeMiss` (Default), with `measured` and `detail` populated; a Hard `Fail` carries its `reopenSection` (empty for Test 7 and Test 9's stated cases).
6. A band a check needs being empty returns `InvalidWorld`, not `Fail`.
7. A trace missing a required layer, or with fewer than `minWeeks`, returns `Pending` with the reason, and never a pass or fail.
8. `t1.moisture_monotonic` fails when coast ≥ ocean, and when inland ≥ coast; each failing order is reported in `detail`.
9. `t3.*` ignore every week up to release (a scripted trace whose held centre is stationary before release and moves after passes `centre_moves`); a storm that never leaves a 1-cell radius for 10 weeks fails `not_stationary`.
10. `t4.temp_smooth` ignores a direction change below 1e-4 and counts one above it; a saw-tooth curve fails.
11. `t5.*` use weeks 40, 52 and 65 exactly as defined; a mountain `waterToGround` peak outside 53–65 fails `flood_follows_melt` and records the actual week in `gw_peak_week` (aligned to locked GDD 2026-11-17, CHANGE-048: mountain, not lowland).
12. `t7.years_differ` fails four bit-identical years and passes years differing by 0.6%; `t7.seasonal_shape` fails a year whose seasonal pattern is inverted.
13. `t8.edge_on_gt_off` fails when off is higher than on; `t8.toggle_effective` fails when the two runs are identical; the upwind-edge set is computed from the on run and reused for both.
14. `t9.determinism` passes identical traces and fails a trace differing in one ULP of one cell, reporting the first week, field and cell.
15. Every check is deterministic: evaluating twice gives bit-identical `measured`.

### Baseline
16. `Load` rejects an unknown key, a non-finite value and a wrong `format` or `version`, atomically.
17. With no entry a Default check is judged against the GDD default; with an entry, a value within ±10% (or ±0.01 under 0.1) passes and beyond it is `MagnitudeMiss`.
18. `Record` without `force` leaves existing entries byte-identical and adds absent ones; with `force` it replaces the world's entries; a `Record` followed by a `Compare` of the same results passes.
19. `Serialize` is stable (the same store gives the same bytes) and round-trips with `Load`.

### Registry
20. `RegisterAll` registers every id exactly once, each with a kind, requirements and a reopen section (or the documented empty), and the id list matches the table in this spec (a test compares the two).

## Files Touched

New: the `S25/` files above; tests `Tests/GoogleTests/CoW/S25/{TestS25Helpers,TestS25Checks,TestS25Baseline}.cpp` (the checks file is table-driven over the registry, so adding a check without both a pass and a fail trace fails the test); `Assets/CoW/Stages/S25WeatherPatterns/baseline.json` (empty, `cow.s25_baseline` v1). Edited: A4 Feature 6 (additive overload), vcxproj via `dia docs vcxproj-add`. **Spec edits on approval:** the system spec's `CheckResult` (state, reopen section) and Checks section (pointers to this spec, `find_storms`/`find_droughts` become the A4 names, Test 3 timing, Test 7 thresholds). No Dia files are added or edited.

## Binding Decisions

| ID | Decision | How this feature complies |
|----|----------|---------------------------|
| AD-002 | Four-tier dependency model | Feature-local (`S25` prefix); nothing shared except through Feature 4's registry |
| PD-004 | No STL in public APIs | Results, trace, store use Dia containers and fixed strings |
| AD-001 | Ask permission before Dia edits | None |
| GDD AD-06 / AD-07 | Directions hard, numbers default; checks numeric | Hard vs Default kinds, baseline store, checks pure and scripted |
| Resolved Decision 3 (system) | Test 7 stays hard; a fail goes to the user | `t7.years_differ` reports the user-decision text |

## Test Plan

Pure GoogleTests only, no engine, no A4 run: scripted traces over hand-built hex terrains, one pass and one fail per check by construction (a registry-driven test enforces it). Real-A4 behaviour is Feature 3's. TDD order: helpers, then checks Test 1 to 9 in order, then the baseline store, then the registry. Scoped `--filter` while iterating, one full `--shards >= 2` run at the end. No mutation checks. Gap audit and edge pass is the last plan task. **Observation scan** at verify (expected: nothing; the code is pure).

## Open Design Questions

1. **Will the hard direction checks survive A4's real output?** Several definitions are first guesses at "the expected direction" (Test 2's crest set, Test 8's upwind-edge band, Test 3's convergence ±60° and 4-of-6, the 0.01 and 0.05 numbers). The scripted tests only prove the checks do what this spec says, not that A4 meets them. Feature 3's first real run on the curated worlds is the experiment, and a hard fail there is a finding about the definition **or** A4; the report must show the measured values so you can tell which.
2. **Test 5 needs a 65-week run.** The GDD says "weeks 1–13 of the year after the snow builds", so the run is 52 + 13 weeks, not 52. That makes the 52-week performance gate a different run from Test 5's. Feature 3 should run Test 5's 65 weeks as a separate plan entry rather than stretching the 52-week one.
3. **One Large world at all?** All checks are defined on Small bands. The Large headless run in the system spec is performance only; if you want checks at Large the band thresholds (≥ 8 from ocean) need scaling rules, which are not defined here.
4. **Per-test check ids are many (about 34).** Each is its own baseline entry, report line and check line. If that is too noisy live, Feature 4 can group them under the nine tests in the panel without changing the ids.

## Gap-pass amendments (2026-11-16)

Found by the plan-writing pass; each item below **supersedes** any contradicting text above. Numbering is stable.

1. **Evaluate takes inputs, not one trace.** `Evaluate(const CheckInputs&)` with `CheckInputs { const IWeatherTrace& primary; const IWeatherTrace* comparison; const WorldInfo& world; }`. `comparison` is the second run for `t2.shadow_repeatable`, `t8.*` (the other edge setting) and `t9.determinism`. `CheckRequirements` is `{ layers, minWeeks, firstWeek, needsComparison }`; with `needsComparison` true and a null `comparison` the check returns `Pending`, and a **live** context reports it as `HeadlessOnly` (Feature 4).
2. **`WorldInfo`:** `{ StringCRC world; WorldSize size; const WeatherTerrain* terrain; uint32_t stormReleaseWeek; uint32_t stormSpawnCell; uint32_t holdWeeks; }`. The storm fields are used by Test 3 only and are 0 elsewhere.
3. **Test 2 crest definition.** A **crest cell** is a land cell with elevation >= 0.6 whose elevation is >= both its prevailing-upwind and its downwind neighbour's and strictly greater than at least one of them (a local maximum along the wind line). The earlier wording ("lower than the prevailing-upwind neighbour's") is superseded.
4. **`minWeeks`.** `t4.melt_crossing` uses weeks 1..13 (the first Spring after spin-up, minWeeks 13). `t5.*` use minWeeks 65 and firstWeek 40. Test 4's other checks use 52.
5. **First run and `Pass`.** A Default check with no baseline entry is judged against the GDD default and is `Pass` if met. `Pass` does **not** require a recorded baseline; the report lists unrecorded Default checks as informational (`baseline.unrecorded[]`).
6. **`RegisterAll(CheckRegistry&)`.** The check objects are function-local statics owned by `S25WeatherChecks`; the registry stores non-owning pointers valid for the process lifetime.
7. **`measured` and `threshold` are `double`** (the system spec's `float` is superseded).

## Status

**Status:** `Approved` (2026-11-16). **Aligned to locked GDD 2026-11-17 (CHANGE-048):** baselines need re-recording under the new A4 (A4 evaporation reads A5 `open_water` / `surface_wetness`, which are absent or 0 in S-2.5; `SOIL_WETNESS_WEIGHT` 0.25; new influences and climate cycles). The GDD S-2.5 Test 5 now measures the **mountain** `waterToGround` melt pulse, matching the GDD (resolved 2026-11-17). **Amended 2026-11-17 (A5 Hydrology):** Test 5's measure is mountain `waterToGround`, not `groundWater` (A4 amendment); the flood-travel half of the GDD test moves to S-2.6. Plan: [weather-checks.plan.md](weather-checks.plan.md) (written 2026-11-16, not started).
