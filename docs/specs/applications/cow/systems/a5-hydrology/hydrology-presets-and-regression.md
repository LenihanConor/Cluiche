# Feature Spec: A5 Hydrology Presets & Regression

**Parent:** @docs/specs/applications/cow/systems/a5-hydrology/a5-hydrology.md (Feature 6)
**Status:** Draft (2026-11-17)
**Design source:** the locked GDD `C:\GitHub\DesignDocs\COW\Output Docs\GDD\08_SystemsDeepDive\A5_Hydrology\` (locked 2026-11-17: `A5_AuthoringDebug.md` §5 presets and §6 regression, `A5_EdgeCaseMatrix.md` EC-15 to EC-19 reachability, `A5_TuningParameters.md` §11 pattern targets). The GDD owns design intent (AD-003). Aligned to it (aligned to locked GDD 2026-11-17). The user decisions of 2026-11-17 win where they differ (see "GDD differences").
**Depends on:**
- A5 Feature 1 (`HydrologyFields`, `HydrologyTerrain`, parameters, `Initialise`).
- A5 Feature 2 (`Step`, `WaterTotals`, `Passability`, `FixedWaterInput`, the balance tolerance, golden AC 27). Both must be **built**, not only specced: the loader and selectors need only Feature 1, but every scenario and regression test needs Feature 2's `Step`.
- S-1 World Generation (built): `ReachabilityValidator`, Small presets, `INavigationProfile`.
- S-1 consumer amendment 8 (basin table and bed elevation in the world record) for the S-1-world tests (R-Reach, goldens).
- Dia `Testing/CellLayerGolden.h` (built).
- The coupled test also needs A4 Features 1–3 and 8 and A5 Feature 3, and is `SLOW_` and blocked until they are built.

**Consumed by:** A5 Feature 5 (`cow.hydro.preset`), S-2.6 Rivers & Floods (scenario files reference presets by name; not specced), S-3.5 (R-Reach also runs there when the order includes `hydrology`).

## Summary

Four scenario presets as data files (melt pulse, wet then dry, ice age, flash flood and fords), a pure `ApplyHydrologyPreset` that puts a `HydrologySimulation` into the preset's start state, and a **scripted input driver** that plays the preset's weekly inputs in place of A4. Then the regression suite:
- per-scenario numeric checks on hand-built grids;
- **R-Reach**, a 100-year reachability run on every S-1 Small preset;
- per-scenario layer goldens;
- the water balance over every scenario.

Presets are **scenarios** (forced inputs and a forced start state). None changes a tuning constant. Packs remain the tuning path (the same rule as A4's presets).

## Goals

1. Four preset files in a **selector-based** format that is world-independent, so the same file runs on the hand-built test grids and on S-1 Small worlds and S-2.6's worlds.
2. `ApplyHydrologyPreset` is deterministic and atomic, and reaches the same state whatever the simulation held before.
3. A `ScriptedWaterInput` plays a preset's weeks with no A4 and no RNG. Real A4 can drive the same presets in S-2.6.
4. Every scenario has numeric regression checks with named thresholds. Where no number exists yet the threshold is **TBD** and the first run records the real value.
5. **R-Reach** proves floods, ice and a four-decade cold phase never cut a region off and never make a corridor or crossing `Deep`.
6. Layer goldens exist for every preset × seed, so later changes to the pipeline show as a per-layer diff.
7. The water balance closes over every scenario.

## Non-Goals

- The `cow.hydro.preset` command, its help and registration (Feature 5). This feature supplies the loader and `ApplyHydrologyPreset`.
- Applying a preset at a capture boundary while a step is in flight (Feature 3). `ApplyHydrologyPreset` runs only when no step is in flight. The module schedules it.
- The S-2.6 scenario files, check lines and walkthrough (S-2.6, using S-2.5's `cow.scenario` envelope). A scenario names a preset, for example `cow.hydro.preset name=melt_pulse`. It does not embed one.
- New hydrology mechanics. If a check fails, only the allowed tuning levers below may change.
- The base golden of Feature 2 (AC 27). This feature adds scenario goldens and does not duplicate it.
- Performance gates. Run time is reported only.

## Design

### Preset format: A5-owned files, not the scenario envelope

**Decision for this spec:** presets are A5-owned files `Cluiche/Assets/CoW/Global/Hydrology/Presets/<name>.hydropreset.json` with envelope `{"format": "cow.hydrology_preset", "version": 1}`. This follows A4's pattern (`.weatherpreset.json` under `Global/Weather/Presets/`).

Why not `.scenario.json`: S-2.5's `cow.scenario` envelope holds a flat list of **command lines**, a watch text and checks. It cannot carry a weekly input script or a start-state recipe. S-2.6's scenario files (in the `cow.scenario` envelope) reference a preset by a setup line `cow.hydro.preset name=<name>`, exactly as S-2.5's scenarios reference A4's presets. The directory name uses `Presets/` (matching A4), not the lower-case `presets/` in the brief (flagged).

Loaded through the same strict rules as Feature 1's parameter file: an unknown key is invalid and loading is all-or-nothing.

```json
{
    "format": "cow.hydrology_preset", "version": 1,
    "name": "melt_pulse",
    "startWeek": 0,
    "selectors": {
        "mountains": { "elevation": { "min": 0.6 } },
        "rivers":    { "river": true }
    },
    "fills": [
        { "field": "glacier", "value": 0.0, "where": "all" },
        { "field": "soilMoisture", "fraction": 0.8, "where": "all" }
    ],
    "phases": [
        { "weeks": 13, "temperature": { "base": 0.1 },
          "snowPack": { "base": 0.0, "where": [ { "selector": "mountains", "value": 0.9 } ] },
          "waterToGround": { "base": 0.02 } },
        { "weeks": 26, "temperature": { "base": 0.6 }, "waterToGround": { "base": 0.03 },
          "melt": { "where": "mountains", "perWeek": 0.15, "weeks": 6 } }
    ]
}
```

- **Selectors** are named predicates over the immutable terrain, AND-combined. Allowed clauses: `elevation` / `latitude` `{min, max}` (inclusive, [0, 1]); `waterSource` (list of `None|Ocean|Lake|River`); `river` (bool); `crossing` (bool, `isCrossing`); `basin` (`"lake"`, `"dry"`, `"any"`); `distanceToRiver` `{min, max}` in hex steps; `upstreamOfCrossing` `{maxSteps}` (cells whose downstream path reaches an `isCrossing` river cell within `maxSteps` steps). `"all"` is predefined. An unknown clause or selector name, `min > max`, or a selector that resolves to zero cells where a fill or phase uses it, is invalid (the last is checked at apply time, because it needs the terrain).
- **Fills** set a start-state field on a selector's cells, in file order: `soilMoisture`, `groundWater`, `surfaceWater`, `glacier`, `ice`, each as an absolute `value` or a `fraction` of the biome capacity (soil and ground only). `basinFill` sets selected basins to a fraction of their spill volume. Values are range-checked per field. Fills run after a fresh `Initialise`-seed of the terrain and clamp to capacity.
- **Phases** are played in order. Each has `weeks` and the four scripted inputs `waterToGround`, `temperature`, `snowPack`, `glacierInput` as `{ base, where: [{selector, value}] }` (later `where` entries win). `temperature` and `waterToGround` may add `seasonAmplitude`, which adds a seasonal swing from a **constant 52-entry table in code** (no libm, so results do not depend on the maths library). `melt` is a convenience for a forced warm pulse: `perWeek` extra `waterToGround` on a selector for `weeks` weeks. A preset's total length is the sum of `weeks`; the driver repeats the last week's inputs past the end unless the caller stops.
- **`startWeek`** (0–51) sets the season the script starts in. It is returned to the caller, not owned.

Limits (validated): at most 12 selectors, 16 fills, 16 phases; total weeks ≤ 5,200 (100 years).

### `ScriptedWaterInput` (the driver)

Feature 2's `FixedWaterInput` supplies **scalar** per-week values (the same for every cell) and has no `snowPack` or `glacierInput`. That is not enough for "heavy mountain snow" or "extreme rain on one catchment" (flagged). So this feature adds its own driver and leaves Feature 2 unchanged:

```cpp
namespace CoW::Hydrology {
    class ScriptedWaterInput {
    public:
        bool Build(const HydrologyPreset&, const HydrologyTerrain&, String256& err);   // resolves selectors once
        // Fills the per-cell arrays for this week and returns the WaterInput over them.
        // glacierInput is written only where sim.Fields().glacierEligible != 0.
        WaterInput ForWeek(uint32_t week, const HydrologySimulation& sim);
        uint32_t   TotalWeeks() const;
    };
}
```

It owns four per-cell float buffers and writes them each week. `extraWater` is null and `sinks` are neutral. No RNG anywhere. `glacierInput` follows A4's contract: it applies only on cells A5 has flagged glacier-eligible. In the cold phase the script keeps `snowPack` high so the glacier counters reach `glacier.onset_years`.

### `ApplyHydrologyPreset`

```cpp
struct HydrologyPresetResult { bool ok; uint8_t startWeek; HydrologyPresetError error; };   // error: key/path + reason, no STL
HydrologyPresetResult ApplyHydrologyPreset(HydrologySimulation& sim, const HydrologyPreset& preset,
                                           const HydrologyTerrain& terrain, const HydrologyParameters& params);
```

Precondition: the simulation is initialised and **no step is in flight**. A violation is an assertion failure in Debug and a returned error in Release.

1. **Validate** against the terrain: every selector used resolves to at least one cell; fills and phases are in range. Any failure returns `ok = false` and changes nothing.
2. **Reset** to a clean base: re-run Feature 1's seeding (lakes full, soil and ground at their init fractions). The result is the same whatever ran before.
3. Apply the fills in order, clamp to capacity, recompute the derived fields (levels, `openWater`, `passability`).
4. **Rebaseline** Feature 2's storage tracking so the first step's balance error does not include the fills. Feature 2 must expose this (flagged; a small addition, `RebaselineStorage()`).
5. Return `startWeek`.

### The four presets

All run on the fixed-input driver. S-2.6 can also run them **coupled with real A4**: `cow.hydro.preset name=<n> mode=coupled` applies the **fills only**, and the scenario file's setup lines supply A4's weather (for example `cow.weather.load_preset name=spring_melt`). The scripted `phases` are then ignored and the reply says so.

| Preset | Start | Phases (provisional values, tuned in S-2.6) | What to watch |
|---|---|---|---|
| `melt_pulse` | Winter start; glacier 0, dry soil | Heavy snow on the mountain selector over a cold winter; then a warm spring where it melts over a few weeks | The flood moves down the main river over weeks; a flood flag appears at and below the confluence and clears after |
| `wet_then_dry` | Non-lake basins empty | **Wet year** (rain 1.5–2× the base, 52 weeks), then a **dry decade** (rain 0.2× the base, 520 weeks) | Basins fill and a dry basin forms a lake. Then lakes shrink, rivers fall to baseflow and some run dry, lakebeds are exposed |
| `ice_age` | Soil at its init fraction | Cold phase of **20 years** (temperature low, snow pack persistent so `snowYears` accumulates, rain reduced), then a warming ramp of 5 years, then a warm melt pulse | Rivers and lakes freeze, glacier mass grows, lakes fall; on warming the ice retreats and its melt makes a long pulse |
| `flash_flood_fords` | Wet soil on the catchment | 8 normal weeks, **2 weeks of extreme rain** on `upstreamOfCrossing`, 12 recovery weeks | Floods spread over the floodplain near the crossing, the crossing cell stays passable (`Fordable` at worst), floods clear |

The numbers live in the data files, so tuning needs no code change. The 20-year cold phase is the preset's own. The R-Reach test below uses a longer, separate 4-decade cold phase (not a preset).

### Selectors and world independence

The selectors resolve against any `HydrologyTerrain`. A preset that needs a feature a world lacks (`upstreamOfCrossing` needs a river with a crossing) fails apply with the selector named and changes nothing. It does not guess (the A4 rule). Which S-1 seeds and presets give `flash_flood_fords` a valid world is checked in Task 1 (Open Question 2).

### Terrain assembly for S-1 worlds

R-Reach and the goldens need a `HydrologyTerrain` from a generated Small world. Feature 1 says Feature 3 assembles the terrain from `WorldGenerationModule`, but the parent's build order puts **this feature before Feature 3**. So this feature adds a small, pure adapter `BuildHydrologyTerrain(const WorldStructure&, const BasinData&, HydrologyTerrain&)` in `Cluiche/CoW/Shared/Hydrology/` (the same helper Feature 2 AC 22 and AC 27 need for generated Small worlds). Feature 3 reuses it. It needs amendment 8's basin data on the generated world. If amendment 8 is not built when this feature starts, the S-1-world tests are blocked and the hand-built tests proceed (flagged).

### Regression suite (all GoogleTests; serial executor unless stated)

Numeric checks run on **hand-built grids** (`HydrologyTestGrids.h`: the chain, basin and floodplain grids from Feature 2, plus a dedicated `MakeRiverWithCrossing`, `MakeColdUplands` and `MakeWetBasin`). They use known geometry, so a failure points at the model. Thresholds are **provisional** and recorded on the first run.

| ID | Scenario | Numeric checks |
|---|---|---|
| R-Melt | `melt_pulse` on a 40-cell chain with a mountain head and a lowland mouth | (a) `riverFlow` at the mouth peaks at week `Wm`, at the head at week `Wh`, and `Wm − Wh = k` with `k ≥ 0` (`k` is **recorded on the first run**. The GDD's expectation (R-Travel and its "Flood travels" target) is a lowland peak 3–10 weeks after the mountain melt peak. Feature 2's release rule moves a pulse's centre of mass about `k / (1 − k)` cells per week: about 5.7 on steep reaches, under 1 on plains, and about 3 on a mixed river (the GDD session's expectation, to confirm on the first run). See Open Question 1); (b) the mouth's peak is lower than the head's (attenuation); (c) some river cell has `flooded = 1` during the pulse and none 10 weeks after the last melt week (**10 TBD**); (d) cumulative `seaOut` over the run equals the input water to tolerance once the tail has drained |
| R-Wet | `wet_then_dry` on a hand-built world with a dry basin and two lakes | (a) the dry basin's `level` rises above its lowest bed during the wet year and its `inundatedWeeks` reaches 104 (A3's first-run Lake threshold, `lake_inundation_weeks`; the test reads A3's value when A3 is built); (b) in the dry decade the `level` of every basin at the end is below its value at the start of the decade; (c) lake cells are exposed: some cell has `exposedWeeks > 0`; (d) a river cell reaches `riverFlow < river.low_flow_fraction · meanFlow` (0.1, the same threshold as Feature 5's `river_dry`); (e) the wet-year mean mouth flow exceeds the dry-decade mean (**ratio TBD**) |
| R-Ice | `ice_age` on a cold-uplands grid | (a) during the cold phase some river cells have `ice ≥ freeze.ice_crossing_min` and some lake basins have `iceFraction ≥ freeze.ice_crossing_min`, and both show `Passability::Frozen`; (b) `glacier` mass summed over eligible cells is non-decreasing at each year's end during the cold phase and above zero after `glacier.onset_years + 1` years; (c) at least one cell's `glacierEligible` turns on; (d) in the warming phase the total glacier mass falls every year; (e) the mean mouth flow in the first year of warming exceeds the mean in the last year of the cold phase (**ratio TBD**); (f) lake `level` at the end of the cold phase is below its start (**TBD**, depends on the script's rain) |
| R-Ford | `flash_flood_fords` on a grid with a river and a corridor crossing | (a) `floodedCells > 0` in at least one week of the extreme rain; (b) the crossing cell's `Passability` is never `Deep` in any week; (c) the crossing cell's passability is not `Dry` in at least one week (a guard so the test is not vacuous: the flood reached it); (d) every flood flag is clear `recovery_clear_weeks` after the rain ends (**TBD**, 12 in the script) |
| R-Balance | Every preset, every scenario, 100 simulated years on the S-1 Small worlds for seed 1 (hand-built grids for the whole preset length) | `\|error\| ≤ 1e-4 · max(in, out, 1)` every week (Feature 2's tolerance). Cumulative `\|error\|` over the run stays under `1e-3 ·` cumulative throughput (Feature 2 AC 23) |
| R-Determinism | `melt_pulse` and `flash_flood_fords` on the hand-built grids | Bit-identical fields and totals at 1, 2, 7 and 16 ranges, and serial vs a 3-range executor, over the preset's first 104 weeks (or its whole length if shorter) |

Each check prints its **measured value** (not only pass/fail), so the margin is visible. A check that passes with under 10% margin prints a warning (the A4 rule).

### R-Reach (default GoogleTest)

`ReachLongRun` runs, **for each S-1 Small preset** (the list comes from S-1's shipped presets at Task 1; seed 1):

1. Build a Small world, `BuildHydrologyTerrain`, `Initialise`, spin up on the driver for 52 weeks.
2. Play a **scripted wet/dry/cold cycle** for **100 simulated years (5,200 weeks)**: a wet year, a dry decade, a normal decade, a 4-decade cold phase (cold temperatures, persistent snow pack, reduced rain), a warm ramp and a melt pulse, then normal weather to the end (exact weeks fixed in the plan; the 4-decade phase is the longest in a row). `ScriptedWaterInput` plays it; `ice_age`'s shorter cold phase is not enough, so this is a dedicated script in the test (`Testing/ReachCycleScript`).
3. **Every week:** assert no `isCrossing` cell (corridor, crossing, Hearth, Nexus and world-throne cells) has `Passability::Deep`, and no corridor-site cell is `Deep`.
4. **Every simulated year (week 52k):** run S-1's `ReachabilityValidator::Validate` with a **test-only `INavigationProfile` decorator** that treats `Deep` cells as water (impassable) and `Frozen` cells as passable. Assert the validation result has the same findings as the unflooded world's own validation (zero new errors): every region is reachable and every protected guarantee holds.

The decorator needs the profile interface to allow a per-cell override. If it does not, a CoW-side S-1 amendment (no Dia change) adds a small `ICellBlockMask` input to the validator. Task 1 confirms which (Open Question 3).

Run time is **reported** (5,200 steps per world; at Small this is expected to be the longest default test). It is not gated. If it is too slow for the default suite, the user decides whether to shorten the cycle or move it to `SLOW_` (Open Question 3).

### Coupled R-Reach (`SLOW_`)

`SLOW_ReachCoupledA4A5` runs the same assertions with **real A4** driving A5 through `WeatherRunner` + `HydrologyRunner` in World Step order, with the ice-age climate cycle on (A4 Feature 8), 100 years at Small. It is skipped by default (`SLOW_`) and **blocked until A4 Features 1–3 and 8 and A5 Feature 3 are built**. The plan marks it Blocked; it is not a gate for marking this feature Done.

### Goldens

`Tests/GoogleTests/CoW/Golden/hydrology_layers.golden.json`, using `CellLayerGolden.h` (`HashLayer`, `GoldenStore::Check`, update with `DIA_UPDATE_GOLDEN=1`).

- **Scope:** the eight base layers (`hydro.soil`, `.ground`, `.surface`, `.river_flow`, `.flooded`, `.ice`, `.glacier`, `.passability`) for each of the **4 presets × seeds 1–3 at Small**, fixed driver (`ScriptedWaterInput`), serial executor, after **52 weeks**: **96 hashes**.
- **Keys:** `scenario/<preset>/seed<N>/<layer>`. Feature 2's AC 27 entries (the base golden for its fixed driver) use the `pipeline/` prefix; this feature does not touch them. Each feature owns its own prefix in the shared file (Feature 2's AC 27 text should adopt the prefix; flagged).
- 52 weeks covers the whole of `melt_pulse` and `flash_flood_fords`, and only the first year of `wet_then_dry` (the wet year) and `ice_age` (the first cold year). The long behaviour is covered by R-Wet and R-Ice. That is a deliberate limit.
- A golden diff reports the preset, seed, layer, changed-cell count and first cell, and follows the repo's summary-only report.

## Public API

`CoW::Hydrology`, no STL in public APIs (`DynamicArrayC`, `String64`/`String256`, fixed-capacity arrays):

| File | Contents |
|---|---|
| `Cluiche/CoW/Shared/Hydrology/HydrologyPreset.h/.cpp` | `HydrologyPreset`, `LoadHydrologyPreset`, `ResolveSelector`, the validation rules |
| `Cluiche/CoW/Shared/Hydrology/HydrologyPresetApply.h/.cpp` | `ApplyHydrologyPreset`, `HydrologyPresetResult` |
| `Cluiche/CoW/Shared/Hydrology/ScriptedWaterInput.h/.cpp` | The driver |
| `Cluiche/CoW/Shared/Hydrology/BuildHydrologyTerrain.h/.cpp` | World-to-terrain adapter (see above) |
| `Cluiche/CoW/Shared/Hydrology/Testing/HydrologyTestGrids.h` | Gains `MakeRiverWithCrossing`, `MakeColdUplands`, `MakeWetBasin` (the file is shared with Features 1–2) |
| `Cluiche/CoW/Shared/Hydrology/Testing/ReachCycleScript.h/.cpp` | The R-Reach wet/dry/cold script and the `Deep`-blocking profile decorator |

`HydrologySimulation` gains `RebaselineStorage()` (a Feature 2 addition). Preset files live under `Cluiche/Assets/CoW/Global/Hydrology/Presets/`.

## Acceptance Criteria

### Loader and selectors
1. All four shipped preset files load. A copy with each of these edits is rejected with the offending key named: unknown top-level key, unknown selector clause, unknown selector name, `min > max`, a field value out of range, an unknown fill field, an unknown phase input, a wrong `format`, a wrong `version`, more than the stated limits, a phase with `weeks` of 0.
2. Loading never partially succeeds: on failure no `HydrologyPreset` is produced. It does not touch a `HydrologySimulation`.
3. `elevation`, `latitude` and `distanceToRiver` boundaries are inclusive. Clauses AND together. `river`, `crossing`, `basin` and `waterSource` select the expected cells on a hand-built terrain.
4. `upstreamOfCrossing {maxSteps}` selects exactly the cells whose downstream path reaches an `isCrossing` river cell within `maxSteps` steps on a hand-built river with a crossing, and nothing on a terrain with no crossing.

### `ApplyHydrologyPreset`
5. After apply the state is bit-identical whether the simulation was fresh, had run 20 weeks first, or had earlier fills (one test per case).
6. Fills apply in file order (a later fill overwrites). A `fraction` fill equals fraction × biome capacity. Fields not named hold the seeded values. Values clamp to capacity.
7. `basinFill` sets the named basins to the fraction of their spill volume and the derived fields (levels, `openWater`, `passability`) are consistent.
8. A selector used by a fill or phase that resolves to zero cells makes apply fail with the selector named and the simulation byte-identical (atomicity).
9. Apply with a step in flight returns an error without mutating.
10. The same preset applied twice gives identical state. The first `Step` after apply has `|error|` within tolerance (the storage baseline is reset).

### `ScriptedWaterInput`
11. For a hand-built preset with known phases, `ForWeek` yields the expected per-cell `waterToGround`, `temperature` and `snowPack` in each phase, with a `where` override on its selector only and the base elsewhere. `melt` adds its extra water on its weeks only.
12. `glacierInput` is written only on cells with `glacierEligible != 0`, and is zero elsewhere.
13. The seasonal term is exactly the code's 52-entry table. Two runs give identical buffers. No RNG is used.
14. Past the last phase the last week's inputs repeat. `TotalWeeks` equals the sum of the phase weeks.

### Scenarios
15. **R-Melt**: checks (a)–(d) pass with their thresholds. The measured `k`, the attenuation and the flood weeks are printed.
16. **R-Wet**: checks (a)–(e) pass. The measured levels, exposure weeks and flow ratio are printed.
17. **R-Ice**: checks (a)–(f) pass. The measured freeze weeks, glacier mass at the cold phase end and the warming melt ratio are printed.
18. **R-Ford**: checks (a)–(d) pass. The measured peak flooded count and the crossing's worst passability are printed.
19. Every preset runs its whole length on its grid with no NaN, no infinity and every field inside its range (the Finish pass clamps).
20. **R-Balance**: every scenario holds the weekly tolerance and the cumulative drift limit of Feature 2 (AC 22 and 23) for its whole length.
21. **R-Determinism**: `melt_pulse` and `flash_flood_fords` are bit-identical at 1, 2, 7 and 16 ranges and with `SerialExecutor` over their first 104 weeks (or whole length if shorter).
22. A check that passes with under 10% margin prints a warning. Each scenario prints its measured values.

### Reachability
23. `ReachLongRun` plays the scripted cycle (including the 4-decade cold phase) for 100 simulated years at Small on each S-1 Small preset, seed 1.
24. In every week of every run, no `isCrossing` cell and no corridor-site cell has `Passability::Deep`.
25. In every simulated year of every run, `ReachabilityValidator` with `Deep` cells as water and `Frozen` cells as passable reports the same findings as the unflooded world: every region reachable, no new guarantee failure.
26. The test's decorator treats `Deep` as blocked and `Frozen`, `Wet`, `Fordable` and `Dry` as passable (a unit test on a hand-built world proves a planted `Deep` ring cuts a region off and the validator reports it, so the assertion is not vacuous).
26a. **Version-1 worlds:** on a `basinsFromLakesOnly` world (lakes pinned, always `Deep`), no protected cell (corridor, crossing, Hearth, Nexus, world-throne) is `Deep`: they are `Fordable`. A hand-built lake-only world with a crossing on a lake cell proves it, and the validator still passes with `Deep` as water.
27. `SLOW_ReachCoupledA4A5` exists, is skipped by default, and is marked Blocked in the plan with the features it waits for.

### Goldens
28. The 96 layer hashes (4 presets × 3 seeds × 8 layers, 52 weeks, serial executor, `scenario/` keys) match the committed golden file. Changing a pipeline constant makes the failure name the preset, seed, layer, changed-cell count and first cell.
29. Every preset applies on each of the seeds 1–3 at Small (or the seed set is amended in Task 1 under Open Question 2).

### Terrain adapter
30. `BuildHydrologyTerrain` on a generated Small world yields a terrain that `Initialise` accepts, with `isCrossing` equal to the union of corridor sites, `Pass` cells, `RiverCrossesBarrier` cells and Hearth, Nexus and world-throne site cells (Feature 1 AC 14), and the basins from amendment 8. It is deterministic.

## Files Touched

| File | Change |
|---|---|
| The files in Public API | New |
| `Cluiche/Assets/CoW/Global/Hydrology/Presets/{melt_pulse,wet_then_dry,ice_age,flash_flood_fords}.hydropreset.json` | New |
| `Cluiche/Tests/GoogleTests/CoW/Hydrology/TestHydrologyPreset.cpp`, `TestHydrologyPresetApply.cpp`, `TestScriptedWaterInput.cpp`, `TestHydrologyRegression.cpp`, `TestHydrologyReach.cpp`, `TestHydrologyGolden.cpp` | New |
| `Cluiche/Tests/GoogleTests/CoW/Golden/hydrology_layers.golden.json` | Edited: adds the `scenario/` entries (Feature 2 creates the file with its `pipeline/` entries) |
| `HydrologySimulation` (Features 1–2's class) | Edited: `RebaselineStorage()` |
| `HydrologyTestGrids.h` | Edited: three new grids |
| `CoW.vcxproj`, `GoogleTests.vcxproj` (+ filters) | Via `dia docs vcxproj-add` |
| CoW `pipeline.toml` | Deploys `Global/Hydrology/Presets/` |

**Amends Feature 2** (to be re-approved with this spec): `RebaselineStorage()`; AC 27's golden keys take the `pipeline/` prefix. **Possible S-1 amendment** (CoW-side, Open Question 3): a per-cell block input on the validator, only if its profile interface cannot express "`Deep` is water". No Dia files are added or edited, so there is no AD-001 sign-off. A future need for a Dia change would stop and be flagged.

## Binding Decisions

| ID | Decision | How this feature complies |
|----|----------|---------------------------|
| PD-004 | No STL in public APIs | Preset, selector, result and driver types use Dia containers and string types. Test-only code may use STL privately, as S-1's tests do |
| AD-001 | Ask permission before Dia edits | None. `CellLayerGolden.h` is built and only consumed |
| AD-002 | Four-tier model | CoW-Shared, pure logic and test helpers |
| AD-003 | GDD owns design intent | Scenario shapes follow the locked GDD (2026-11-17). Thresholds the GDD does not give stay provisional and are listed for fold-back |
| DFS-002 | `Run` reads only the captured working copy | Presets and the driver act on the committed state and the `WaterInput` between steps, never inside `Step`. The driver writes its own buffers, not module state |
| Parent R-Reach (Decision 3) | Floods are temporary, corridors and crossings stay fordable, ice never blocks | ACs 23–26 prove it every week and every simulated year |

## GDD differences

| GDD draft | This spec |
|---|---|
| Presets: `melt_pulse`, `wet_year`, `drought_decade`, `ice_age`, `chain_lakes`, `fixed_input` | Four presets (user decision): `melt_pulse`, `wet_then_dry` (the GDD's `wet_year` and `drought_decade` joined into one scenario), `ice_age`, and a new `flash_flood_fords`. `chain_lakes` is covered by Feature 2's hand-built chained-basin test (AC 14), and `fixed_input` is the driver itself, not a preset |
| `ice_age` pins A4's ice-age cycle to its cold phase for 20 years, then unpins | The preset scripts its own inputs (no A4). The A4-coupled cycle is covered only by the `SLOW_` R-Reach |
| R-Balance, R-Reach, R-Determinism, R-Golden, R-Travel | R-Balance, R-Reach, R-Determinism, R-Golden as listed. R-Travel becomes **R-Melt** with a different check (see below) |
| R-Travel: lowland peak 3–10 weeks after the mountain peak (the first run records the real value) | R-Melt also records `k` on the first run and asserts no number until it is known. The GDD expectation (3–10 weeks) is the target. On a short test chain the lag can be smaller, since it scales with river length (Open Question 1) |
| R-Reach covers "every preset" with the ice-age cycle on | R-Reach runs the S-1 Small presets with a scripted 4-decade cold phase. The coupled A4+A5 version is `SLOW_` and blocked |
| R-Determinism: deferred vs synchronous with A4, 104 weeks | Here: range counts only, on the pure simulation. Deferred-vs-synchronous with A4 needs Feature 3 and A4, so it belongs to Feature 3's test plan (parent Test Strategy) |
| R-Golden: presets × seeds 1–3 at Small | As described, 52 weeks, with the prefix rule |

## Test Plan

TDD per the project rule, in this order:
1. Preset loader and selectors (no simulation).
2. `ScriptedWaterInput`.
3. `ApplyHydrologyPreset` (includes `RebaselineStorage`).
4. The terrain adapter (needs amendment 8 data).
5. R-Melt, R-Wet, R-Ice, R-Ford on hand-built grids, **recording the measured values on the first run and then fixing the thresholds from them**, with the user's sign-off on any threshold the spec leaves TBD.
6. R-Balance and R-Determinism.
7. The `Deep`-blocking decorator and its unit test, then `ReachLongRun`.
8. Goldens (`DIA_UPDATE_GOLDEN=1` once, reviewed).
9. The `SLOW_` coupled test as a skipped stub.

The scenario tests are integration-style and run serial. Scoped `--filter="*Hydrology*"` while iterating. One full run at the end with `--shards 2`. `SLOW_` tests are skipped by default. **No mutation checks.** The gap audit and edge pass is an explicit last plan task, and the Observation scan runs at verify.

### Allowed tuning levers if a check fails

In order: the preset data values; the default parameter file values (Feature 1's table); then **stop and flag** for the GDD owner (a mechanic in Feature 2). Every change is appended to a **Tuning log** in this spec's Status section (parameter, old, new, test that forced it). A changed default is a new GDD fold-back item.

## Open Design Questions

1. **Does a flood "travel for weeks" in Feature 2's model?** *Largely answered by GDD §7.2 (aligned to locked GDD 2026-11-17):* each cell passes on `k` of its water in the same week, so a pulse's centre of mass moves about `k / (1 − k)` cells per week (5.7 on steep reaches, under 1 on plains) and piles up in the lowlands. The lag therefore depends on river length and slope, and R-Melt still records it on the first run. The original worry follows. The cascade moves water down a whole river in one step: a head pulse reaches the mouth in the same week, attenuated by the release rates, with a tail over later weeks. The user's expected check is "the melt peak arrives at the mouth *k* weeks after the head". With high release rates, the peak at the mouth may land in the **same** week or one week later, so `k` could be 0 or 1. The melt itself (snow melting over several weeks, ground water baseflow) is what spreads the peak, not routing. This is the same worry as Feature 2's Open Question 3. R-Melt therefore records `k` and does not assume a value. If `k` is too small to show "floods take weeks", the answer is a tuning change (`r_max` lower), and if that is not enough, a Feature 2 design question (route a share of flow with a one-week delay).
2. **Does every S-1 Small preset × seed 1–3 have a river with a crossing for `flash_flood_fords`?** The preset needs one, and S-1 does not promise it for every seed. If a seed lacks one, options are: choose the nearest river-crossing site as a fallback, change the golden seed set for this preset (user decision, since seeds 1–3 are fixed), or accept "not applicable" for that cell of the golden grid. Task 1 reads the seeds before anything is built.
3. **How long does R-Reach take, and can the validator treat `Deep` as water?** 5,200 weekly steps on a Small world for each S-1 preset is a lot for the default suite and its time is unmeasured (report-only per the parent, but a slow default test hurts every run). If it is too slow, the user decides: fewer simulated years, fewer presets in the default run, or `SLOW_`. Separately, the validator takes an `INavigationProfile`: if it cannot express a per-cell block, a CoW-side S-1 change is needed. Both are checked in Task 1.

## Status

**Status:** Draft (2026-11-17). Written from the user's decisions of the same day:
- four shipped scenarios, all on the fixed-input driver, with S-2.6 able to run them with real A4;
- per-scenario numeric checks with thresholds provisional where no number exists;
- the default R-Reach GoogleTest (100 years, wet/dry/cold cycles including a 4-decade cold phase, every S-1 Small preset, `Deep` as water, reachability re-run every year, corridors and crossings never `Deep`) and the `SLOW_` coupled A4+A5 test, blocked until A4 is built;
- layer goldens via `CellLayerGolden.h` for the presets × seeds 1–3 at Small, 52 weeks;
- the water balance holding over every scenario.

Aligned to the locked GDD on 2026-11-17. Approval is a separate step. No plan. **Tuning log:** empty.
