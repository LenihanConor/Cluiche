# Feature Spec: S-2.6 Hydrology Checks

**Parent:** @docs/specs/applications/cow/systems/s26-riversfloods/s26-riversfloods.md (Feature 2)
**Status:** Approved (2026-11-24)
**Design source:** `DesignDocs/COW/Output Docs/GDD/10_PrototypeStrategy/S2.6_RiversFloods.md` AD-04, AD-05, AD-09, §5 (Tests 1–10), §6
**Depends on:** A5 Feature 1 (`HydrologySimulation` state, `HydrologyTerrain`, `HydroCell` fields, basin table, `Passability`) and Feature 6's hand-built grids (`HydrologyTestGrids.h`). S-2.5 Feature 4's `CheckRegistry` / `ICheck` shape (S-2.6 consumes the shape only; `RegisterAll` needs the registry type). S-1 `ReachabilityValidator` findings type (Test 6, as data).
**Consumed by:** Feature 1 (prechecks, live check lines), Feature 3 (the runner records series and evaluates the checks), Feature 4 (scenarios name checks), Feature 5 (series ids).

## Summary

The ten GDD tests as **pure, named checks** over a compact **per-week series** of A5's committed state, plus the helpers they share and the baseline store. A check takes a series, the world and the preset's schedule, and returns a typed result. It never touches a module, a bus or a clock. The same predicates run headless for the verdict (Feature 3) and live for understanding (Feature 1). Everything is TDD-able on hand-built hydrology grids and **scripted series** with known answers.

## Goals

1. Every GDD hard check and default magnitude is one named check with a stable id, kind, threshold and the A5 TechSpec section to reopen on a hard fail.
2. Checks are pure and deterministic: the same series gives the same result and the same `measured`, double for double.
3. A check that lacks the weeks it needs reports **Pending**, never a false pass or fail. A check that needs A4 reports **CoupledOnly** in a Scripted context.
4. A default magnitude is judged against the GDD default until a baseline exists, then against the baseline within ±10% (cell counts: ±10% or ±1 cell, whichever is larger). A miss is a **MagnitudeMiss**, not a fail.
5. Test 1's lag and Test 2's "most rivers" are settled with explicit numbers (below), so a first-run surprise goes to the user and does not become a false A5 reopen (CH-S26-03).

## Non-Goals

- Running A5, recording the series from a live module, the report and DiaAutomation (Features 1, 3).
- Scenario files, check-line UI and walkthroughs (Feature 4). Graphs and exports (Feature 5).
- World selection and the world prechecks' wiring (Feature 1). **This feature owns the pure precheck predicates** (they share the river-tree helpers), Feature 1 calls them.
- Equation correctness (A5's GoogleTests own that on hand-built grids).

## Design

### Result model

```cpp
namespace CoW::S26 {
    enum class CheckKind  : uint8_t { Hard, Default };
    enum class CheckState : uint8_t { Pass, Fail, MagnitudeMiss, Pending, InvalidWorld, CoupledOnly };

    struct CheckResult {
        StringCRC id;               // "s26.t1.flood_not_early"
        uint8_t   test;             // 1..10
        CheckKind kind;
        CheckState state;
        double    measured;         // NaN when not a magnitude
        double    threshold;        // GDD default, or the frozen baseline value
        String256 detail;           // "Wh 14, Wm 19, k 5"
        String32  reopenSection;    // "A5 TechSpec §7" on a Hard Fail, else empty
    };
}
```

`CheckKind` and `CheckState` match S-2.5's shape (`CoW::S25`), with one more state: `CoupledOnly` (the check exists but needs A4, so Scripted never evaluates it). `InvalidWorld` is returned when the world lacks what the check's precheck needs (not an A5 fail). `Pending` carries a reason in `detail` (`needs weeks 1..572, have 60`). **The check types are S-2.6's own** (`CoW::S26`); S-2.5's are not shared (AD-002: no second consumer of the *definition* yet; the **registry** shape is the shared part).

### The series (what checks read)

S-2.6's checks span decades (Test 3 about 570 weeks, Test 4 well over 1,000, Test 5 and 6 5,200), so a full-field trace like S-2.5's is not affordable. A check reads a **per-week summary series**:

```cpp
struct S26WeekSummary {                    // one per applied week, ~0.5 KB
    uint32_t week;                         // 0-based after spin-up
    WaterTotals totals;                    // A5 totals by store, in, seaOut, evap, error
    float headFlow, mouthFlow;             // longest river (S26RiverPick), committed riverFlow
    float meanRiverFlow;                   // over river cells
    uint32_t riverCellsFlowing, riverCells;
    uint32_t floodedCells;
    float glacierMass;                     // summed over glacierEligible cells
    uint32_t frozenRiverCells, frozenLakeBasins;
    FixedArray<float, kMaxBasins> basinLevel;
    FixedArray<uint16_t, kMaxBasins> basinInundatedWeeksMax;   // max over the basin's cells, A3 counter
    uint32_t maxExposedWeeks, maxLowFlowWeeks, maxGlacierWeeks, maxGlacierFreeWeeks;
    Passability crossingWorst;             // worst over isCrossing cells this week
    uint32_t protectedDeepCells;           // isCrossing + corridor-site cells with Passability::Deep
    uint64_t layerHash;                    // hash of the 8 base layers (Test 10)
};
class IHydrologySeries { /* WeekCount(), At(week), World(), Schedule(), Parameters() */ };
```

`SummariseWeek(const HydrologySimulation&, const S26WorldInfo&, S26WeekSummary&)` is pure and is the **only** place that reads A5 state. The live module (Feature 1) and the runner (Feature 3) both call it, so a live result equals a headless one for the same weeks (acceptance 14). `ScriptedSeries` ships in `S26/Testing/` for tests: a builder where a test states per-week values.

`S26WorldInfo` holds the river pick (head, mouth, path), the dry and lake basin ids, the crossing and its `upstreamOfCrossing` set, the protected-cell list, and the **preset schedule** (phase boundaries in weeks, e.g. melt end, cold-phase end, dry-decade start) the checks need to know where to measure. The schedule comes from the preset data, never from A5 state.

Every check declares `CheckRequirements { minWeeks; scope /* Scripted | Coupled */; needsReachRun; }`. `Evaluate(const IHydrologySeries&, const S26WorldInfo&)` returns `Pending` when unmet and `CoupledOnly` when `scope == Coupled` and the series is Scripted.

### Helpers (pure)

- **River pick.** `PickLongestRiver(const HydrologyTerrain&)`: the river path (head to sea or basin) with the most cells, head chosen as the cell with elevation ≥ 0.6 farthest upstream; ties go to the lowest head index. Used by Tests 1 and 8, the long-river precheck, and the series' head/mouth flow.
- **Flood lag.** `FindPeakWeek(series, field, from, to)` (first maximum in range) and `FloodLag(series)` giving `Wh`, `Wm` and `k = Wm − Wh`; attenuation ratio `mouthPeak / headPeak`.
- **Counter tracker.** `CounterModel` replays A5's counter rules (rise by one per qualifying week, reset on the documented condition, frozen weeks neither count nor reset `low_flow_weeks`, CH-A5-02) over the series, so Test 9 compares A5's reported counters with an independent replay.
- **Balance accumulator.** Weekly relative error `|error| / max(in, out, 1)` and cumulative drift over throughput, accumulated in `double` in week order.
- **Precheck predicates** (`S26Prechecks`, pure over `HydrologyTerrain` + S-1 world data; thresholds are data): `long_river` (path ≥ 20 cells from elevation ≥ 0.6 to sea or basin), `river_crossing` (an `isCrossing` river cell with an upstream path ≥ 6), `dry_and_lake_basin` (one non-lake and one lake basin, both ≥ `MIN_BASIN_CELLS`), `cold_high_cells` (≥ 20 cells A5 can mark `glacierEligible` under the preset's cold phase), `corridors_and_sites` (S-1's three guarantees hold unflooded). Each returns pass/fail plus its **measured value and margin**, so a world that barely passes is visible.
- **Aggregates** in `double`, ascending cell order, so results are bit-reproducible.

### The ten tests as checks

Weeks are 0-based after spin-up. `W` is the preset's schedule from the preset file.

| Test | Id (prefix `s26.`) | Kind | Definition | Reopen |
|---|---|---|---|---|
| 1 | `t1.flood_not_early` | Hard | `Wm ≥ Wh` | A5 §7 |
| 1 | `t1.attenuation` | Hard | mouth peak flow < head peak flow | A5 §7 |
| 1 | `t1.flood_appears_and_clears` | Hard | `floodedCells > 0` in some week of the pulse, and 0 from `recovery_clear_weeks` after the last melt week on | A5 §9 |
| 1 | `t1.sea_balance` | Hard | cumulative `seaOut` equals cumulative input within 1e-3 once the tail has drained (the last `recovery_clear_weeks` weeks add < 1e-4 of input) | A5 §7 |
| 1 | `t1.lag_weeks` | Default | `k = Wm − Wh` in 3–10. A `k < 3` is a MagnitudeMiss whose `detail` says "goes to the user: `RELEASE_MIN` / `RELEASE_SLOPE` or an A5 Feature 2 change" | |
| 1 | `t1.attenuation_ratio`, `t1.peak_flooded_cells` | Default | recorded on the first run | |
| 2 | `t2.dry_year_falls` | Hard | mean river flow in the last dry-year week window (weeks 40–52 of the dry year) < the pre-dry year's mean | A5 §5, §7 |
| 2 | `t2.recovers_with_rain` | Hard | mean flow in the final 13 weeks after rain returns > that at the dry year's end | A5 §5 |
| 2 | `t2.flow_survives` | Hard | at the dry year's end `riverCellsFlowing / riverCells` **> 0.5** ("most", decided here) | A5 §5, §7 |
| 2 | `t2.low_flow_counter_climbs` | Hard | in the dry decade some river cell's `lowFlowWeeks` rises and reaches ≥ 1 | A5 §5 |
| 2 | `t2.frozen_weeks_neutral` | Hard | in the cold-winter run the counter is unchanged across frozen weeks (CH-A5-02) | A5 §5 |
| 2 | `t2.surviving_fraction` | Default | ≥ 0.80, first run records the fraction | |
| 3 | `t3.dry_basin_rises` | Hard | the dry basin's level in the wet year exceeds its lowest bed | A5 §8 |
| 3 | `t3.inundation_reaches_lake_threshold` | Hard | the dry basin's `inundatedWeeks` reaches `lake_inundation_weeks` (104, read from A3's tuning; **not hard-coded**, the check reads the value through an `ILakeThreshold` seam) within a run of wet years | A5 §8 |
| 3 | `t3.basins_fall_in_dry_decade` | Hard | every basin's level at the decade's end < its level at the decade's start | A5 §8 |
| 3 | `t3.lakebed_exposed` | Hard | `maxExposedWeeks > 0` at some week | A5 §8 |
| 3 | `t3.wet_flow_gt_dry_flow` | Hard | wet-year mean mouth flow > dry-decade mean | A5 §8 |
| 3 | `t3.flow_ratio`, `t3.wet_years_to_lake` | Default | recorded on the first run | |
| 4 | `t4.river_frozen`, `t4.lake_frozen` | Hard | some river cells / lake basins `Frozen` in the cold phase | A5 §10 |
| 4 | `t4.glacier_nondecreasing_cold` | Hard | `glacierMass` at each year's end during the cold phase ≥ the previous year's | A5 §10, A4 §11.3 |
| 4 | `t4.glacier_present_after_onset` | Hard | `glacierMass > 0` after `glacier.onset_years + 1` years | A5 §10 |
| 4 | `t4.lake_lower_at_cold_end` | Hard | at least one lake basin's level lower at the cold phase's end than at its start | A5 §8, §10 |
| 4 | `t4.glacier_falls_warming` | Hard | `glacierMass` falls at every year's end once warming starts, until it reaches 0 | A5 §10 |
| 4 | `t4.great_thaw` | Hard | mean mouth flow, first warming year > last cold year | A5 §10 |
| 4 | `t4.years_to_clear`, `t4.melt_flow_ratio`, `t4.peak_glacier_mass` | Default | years to clear about 10; the rest recorded | |
| 4 | `t4c.*` | Hard, Coupled | the Coupled `ice_age_40y` variant of the above | A5 §10 |
| 5 | `t5.weekly_balance` | Hard | every week `|error| ≤ 1e-4 × max(in, out, 1)`; `detail` names the first breach week | A5 §2 |
| 5 | `t5.cumulative_balance` | Hard | `|Σ error| < 1e-3 × Σ throughput` | A5 §2 |
| 5 | `t5.worst_weekly_error` | Default | recorded | |
| 6 | `t6.no_protected_deep` | Hard | `protectedDeepCells == 0` in every week | A5 §14 |
| 6 | `t6.validator_unchanged` | Hard | the S-1 validator findings at each year's end equal the unflooded world's, with `Deep` as water and `Frozen` as passable | A5 §14, EC-15..19 |
| 6 | `t6.lakes_only_safe` | Hard | on a lakes-only (version 1) world no protected cell is `Deep` (a world without basins, the check still runs) | A5 §14 |
| 6 | `t6c.*` | Hard, Coupled | `SLOW_` coupled R-Reach | A5 §14 |
| 7 | `t7.flood_occurs` | Hard | `floodedCells > 0` in an extreme-rain week | A5 §9 |
| 7 | `t7.crossing_never_deep` | Hard | `crossingWorst != Deep` in every week | A5 §9, §14 |
| 7 | `t7.crossing_reached` | Hard | `crossingWorst != Dry` in at least one week (non-vacuous guard) | A5 §9 |
| 7 | `t7.floods_clear` | Hard | every flood flag clear `recovery_clear_weeks` after the rain | A5 §9 |
| 8 | `t8.wet_year_more_flow`, `t8.mouth_after_melt`, `t8.handoff_lossless` | Hard, Coupled | per the system spec table | A4 §18 / A5 §6 |
| 9 | `t9.inundated`, `t9.exposed`, `t9.low_flow`, `t9.glacier`, `t9.glacier_free` | Hard | A5's reported counter equals the `CounterModel` replay every week, so each rises by exactly one per qualifying week and resets on the documented condition | A5 §12 |
| 10 | `t10.ranges_identical` | Hard | `layerHash` equal every week between the reference run and each of ranges 1, 2, 7, 16 and serial (needs the comparison series) | iteration / routing order |
| 10 | `t10.goldens_match` | Hard | week-52 `layerHash` of each preset × seed 1–3 equals A5 Feature 6's golden file | iteration order |
| 10 | `t10c.deferred_equals_sync` | Hard, Coupled | Coupled 104 weeks identical in both World Step modes | |

**Test 9 "would trigger" lines** (A3's 104 / 52 / 26 / 260 weeks) are not checks. They are `S26WouldTrigger(counter, threshold)` readouts shown beside the counters (Feature 1) and in the report.

### Baseline store

`baseline.json` (envelope `cow.s26_baseline`, version 1, committed): `{ world: { preset: { checkId: { value } } } }`, one entry per Default check per world × preset at Small. `BaselineStore` has the same shape as S-2.5's (`Load` all-or-nothing, `Lookup`, `Compare`, `Record` refusing to overwrite without `force`, `Serialize` stable with 17-digit doubles). Differences: keyed by **preset** as well as world (one value per world × preset, per the GDD §6), and a count-valued check (`t1.peak_flooded_cells`, cell counts) uses ±10% or ±1, whichever is larger; other magnitudes ±10% relative, with ±0.01 absolute when the baseline is under 0.1. `step_ms` and `largest_tree_nodes` are stored in an `info` block that `Compare` never reads.

- With **no entry** the check is judged against the GDD default and is `Pass` if met; `Record` freezes the measured value either way.
- A check with no GDD default (recorded-only magnitudes such as `t3.flow_ratio`) is `Pass` with `detail` "unrecorded" until recorded, then compared.

## Public API

New in `Cluiche/CoW/Modules/S26/` (S26 prefix, feature-local, AD-002): `CheckTypes.h`, `HydrologySeries.h/.cpp` (`S26WeekSummary`, `SummariseWeek`, `IHydrologySeries`), `S26WorldInfo.h`, `S26Helpers.h/.cpp`, `S26Prechecks.h/.cpp`, `S26HydrologyChecks.h/.cpp` (the registered checks), `BaselineStore.h/.cpp`, `Testing/ScriptedSeries.h`. Uses S-2.5 Feature 4's `CheckRegistry` / `ICheck`.

## Acceptance Criteria

### Helpers and prechecks
1. `PickLongestRiver` returns the longest head-to-sea or head-to-basin path on a hand-built grid with several rivers, breaks ties to the lowest head index, and returns none when no river exists.
2. `FloodLag` finds `Wh`, `Wm` and `k` on scripted series, takes the first maximum on a plateau, and reports `k = 0` when the peaks coincide.
3. `CounterModel` reproduces the A5 counter rules on a scripted week list, including a frozen run that neither counts nor resets `lowFlowWeeks`.
4. The balance accumulator equals a reference double loop bit for bit.
5. Each precheck passes on a hand-built grid that has the feature and fails (naming itself) on one that lacks it; each returns measured value and margin. `long_river` fails at 19 cells and passes at 20; `river_crossing` fails at 5 upstream cells and passes at 6; `cold_high_cells` fails at 19 and passes at 20.

### Series
6. `SummariseWeek` on a hand-built simulation matches a reference computation for every field; two calls on the same state give identical bytes; the summary holds no pointers into the simulation.
7. `layerHash` changes when any one of the eight base layers changes by one ULP in one cell, and is unchanged otherwise.

### Checks: one pass case and one fail case each
8. For **every check id** in the table, a scripted series that satisfies it returns `Pass` (or `Pass` against the GDD default for Default checks) and a series that violates it returns `Fail` (Hard) or `MagnitudeMiss` (Default), with `measured` and `detail` populated; a Hard `Fail` carries its `reopenSection`.
9. A world that lacks what a check needs returns `InvalidWorld`, not `Fail`.
10. A series with too few weeks returns `Pending` with the reason, never a pass or fail. A Coupled check on a Scripted series returns `CoupledOnly`.
11. `t1.flood_not_early` fails when `Wm < Wh`; `t1.lag_weeks` returns `MagnitudeMiss` for `k = 2` with the "goes to the user" text and `Pass` for `k = 5`.
12. `t2.flow_survives` passes at 51% and fails at 50%; `t2.frozen_weeks_neutral` fails a series where a frozen week resets the counter.
13. `t3.inundation_reaches_lake_threshold` reads the threshold through `ILakeThreshold` (a test changes it to 52 and the verdict follows).
14. **Live equals headless:** over the same weeks, every single-run check evaluated on a series built live and on one built by the runner's path gives bit-identical `measured` (a scripted-driver integration test, Feature 3 owns the real one).
15. Every check is deterministic: evaluating twice gives bit-identical `measured`.

### Baseline
16. `Load` rejects an unknown key, a non-finite value and a wrong `format` or `version`, atomically.
17. A count-valued check passes at +1 cell on a baseline of 5 and misses at +2 on a baseline of 5; at a baseline of 100 the band is ±10.
18. `Record` without `force` leaves existing entries byte-identical; with `force` it replaces the world × preset's entries; a `Record` followed by a `Compare` of the same results passes; `info` values are never compared.
19. `Serialize` is stable and round-trips with `Load`.

### Registry
20. `RegisterAll` registers every id exactly once with a kind, requirements and a reopen section (or documented empty), and the id list equals the table in this spec (a test compares the two).

## Files Touched

New: the `S26/` files; tests `Tests/GoogleTests/CoW/S26/{TestS26Helpers,TestS26Prechecks,TestS26Series,TestS26Checks,TestS26Baseline}.cpp` (the checks file is table-driven over the registry, so adding a check without a pass and a fail series fails the test); `Assets/CoW/Stages/S26RiversFloods/baseline.json` (empty, `cow.s26_baseline` v1). Edited: vcxproj via `dia docs vcxproj-add`. No Dia or Draoi files are added or edited.

## Binding Decisions

| ID | Decision | How this feature complies |
|----|----------|---------------------------|
| AD-002 | Four-tier dependency model | Feature-local (`S26`); only the registry shape comes from CoW-Shared |
| PD-004 | No STL in public APIs | Results, series and store use Dia containers and fixed strings |
| GDD AD-04 / AD-05 | Directions hard, numbers default; checks numeric and scripted | Hard vs Default kinds, baseline store, pure checks |
| CH-S26-03 | A lag miss goes to the user, not straight to an A5 reopen | `t1.lag_weeks` is Default with the user-decision text |

## Test Plan

Pure GoogleTests only, no engine and no real A5 run beyond hand-built grids: scripted series for every check (one pass and one fail by construction, enforced by a registry-driven test), then the series summariser on hand-built simulations. Real-A5 behaviour is Feature 3's. TDD order: helpers, prechecks, series, checks Test 1 to 10 in order, baseline, registry. Scoped `--filter` while iterating, one full `--shards >= 2` run at the end. No mutation checks. Gap audit and edge pass is the last plan task. **Observation scan** at verify (expected: nothing; the code is pure).

## Open Design Questions

1. **Will the hard checks survive A5's real output?** Several definitions are first guesses at "the expected direction" (the 13-week windows in Test 2, "most" as more than half, the `recovery_clear_weeks` tail in Test 1). The scripted tests only prove the checks do what this spec says. Feature 3's first real run on the curated worlds is the experiment, and a hard fail there is a finding about the definition **or** A5; the report must show the measured values so you can tell which.
2. **A per-week summary series versus full fields.** It is what makes 5,200-week checks affordable, but a check can only see what `S26WeekSummary` carries. If a failing check needs a cell-level look, you re-run live with the layers on. Adding a field means a summary change plus tests, not a rewrite.
3. **Test 3 reads A3's Lake threshold through a seam.** A3 is not built, so until it is the seam returns the first-run default 104. If A3 changes the number, the check follows it silently; the report prints the value used.
4. **Check count is about 60 ids.** Each is a baseline entry candidate, report line and check line. Feature 1 can group them under the ten tests in the panel without changing ids.

## Status

**Status:** `Approved` (2026-11-24). No plan.
