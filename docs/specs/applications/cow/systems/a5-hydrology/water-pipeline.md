# Feature Spec: A5 Water Pipeline

**Parent:** @docs/specs/applications/cow/systems/a5-hydrology/a5-hydrology.md (Feature 2)
**Status:** Draft (2026-11-17)
**Design source:** the A5 brief in `Cluiche/out/CoW/gdd-foldback-hydrology-weather-2026-11-17.md`. GDD `A5_Hydrology/` is not locked yet (the GDD session `cow-68` has it), so this spec cannot be Approved before then (AD-003).
**Depends on:** A5 Feature 1 (`HydrologyFields`, `HydrologyTerrain`, `HydrologyParameters`, `Initialise`); DiaDeferredStep Feature 1 Task 1 (`IParallelExecutor`, `SerialExecutor` headers, built `c83a94ed`).

## Summary

The weekly step of A5 Hydrology, as pure logic. `HydrologySimulation::Step(input, executor)` takes one week of water reaching the ground (from A4, or from a fixed-input driver in tests) and moves it through the land:
- **freeze and thaw**
- **glacier gain and melt**
- **infiltration** into soil, percolation to ground water
- **evapotranspiration and injected water sinks**
- **runoff and baseflow**
- a **topological cascade** that carries water downhill along S-1's flow directions and through lake basins in the same week
- **basin levels and inundation**
- **floodplain floods** judged against each river's own running mean
- **passability**
- **glacier counters**
- **double-precision water totals** that prove conservation

It reads nothing but its captured inputs. It has no module, no messages, no save and no debug surface: those are Features 3–5.

**Decoupling.** The pipeline knows nothing about A1, dragons or settlements. Anything that consumes water does it through an injected **water sinks** block in the weekly input:
- a per-cell evapotranspiration multiplier;
- a per-cell absolute draw.

Feature 3's `HydrologyModule` fills the block from registered providers (A1 vegetation later, or anything else). With no providers, the block is neutral and the biome table alone drives the land.

## Goals

1. One week of hydrology as a pure function of `(committed fields, WaterInput, parameters)`.
2. A flood pulse travels down a whole river in one step, attenuated by per-cell release rates: a peak this week and a tail over the following weeks. Lake basins sit inside the cascade, so chained lakes resolve in the same week.
3. Water is conserved. Every source and sink is a named total, summed in double in range order. The balance closes every week within tolerance.
4. The result is bit-identical at any range count, and matches the serial executor.
5. Consumers stay decoupled through the water-sinks block.

## Non-Goals

- **Where the inputs come from.** Feature 3 latches A4's output, sink providers and influences into `WaterInput`. Feature 2 only reads it.
- **Floodplain water storage.** A flood is a flag on cells this week. Floodplain cells do not hold extra water, which keeps conservation and routing simple (Open Design Question 1).
- **Biome change.** The pipeline updates `inundatedWeeks`, `exposedWeeks`, `snowYears` and glacier mass. A3 reads them.
- **Air moisture.** A4's evaporation into the air is a separate, non-conserving model. A5's open-water evaporation and evapotranspiration are A5 sinks. They are not added to A4's moisture, so nothing is double-counted inside A5.
- **Lateral aquifer flow.** Ground water is local; it reaches rivers only through baseflow (user decision 2026-11-17).

## Design

### Units (user decision 2026-11-17)

All water is in **cell-depth units**: 1.0 is one cell covered to the depth of A4's `waterToGround` scale.
- `surfaceWater`, `soilMoisture` and `groundWater` are bounded by per-biome capacities.
- `riverStore`, `ice`, `glacier` and basin `volume` are unbounded sums.
- Clamping never destroys water. A store over capacity spills into the next store, and only the final totals check proves nothing was lost.

### Grid tables (built once at `Initialise`, Feature 1 data, Feature 2 logic)

- **Downstream index per land cell** from S-1's full-grid flow direction on the filled surface. Ocean cells, and land no ocean reaches (`kFlowSink`), are terminals.
- **Basin contraction.** Every cell of an S-1 basin maps to one **basin node**. Water reaching any basin cell enters the basin's volume. The node's outflow leaves from its spill cell to that cell's downstream. Flow on the filled surface always passes through a basin to its spill cell, so a basin is always inside one tree.
- **Topological order.** Nodes (cells and basin nodes) are sorted by descending filled elevation, ties by ascending index (S-1's accumulation order), and grouped into **trees** by terminal. Trees are independent: no water crosses between them.
- **Release rate per river cell:** `r = clamp(r_min + r_slope · drop, r_min, r_max)`, where `drop` is the elevation fall to the downstream cell. Non-river land cells use `r = 1`, so hillslopes deliver all their water downhill the same week and only rivers attenuate.
- **Basin capacity curve:** basin cells sorted by bed elevation, giving cumulative volume as a function of level. The level for a volume is a binary search plus linear interpolation.

### Inputs (`WaterInput`)

Pointers into Feature 3's captured copies, N cells each unless noted:

| Field | Meaning |
|---|---|
| `waterToGround` | Rain + snowmelt from A4 this week |
| `temperature` | A4 air temperature [0, 1] |
| `snowPack` | A4 snow pack [0, 1] (glacier counters only) |
| `glacierInput` | A4 snow above its cap, on glacier-eligible cells only |
| `extraWater` | Signed per-cell water from influences ("wetter" adds; "drier" goes through sinks). Zero when none |
| `sinks.etMultiplier` | Product of all providers' ET multipliers; 1.0 when none |
| `sinks.draw` | Sum of all providers' absolute draws, in cell-depth units; 0 when none |
| `week` | `WeekOfYear` (the season boundary for the glacier counters) |

### The pipeline

Local passes run in parallel over cell ranges. The cascade runs in parallel over tree ranges, and each tree is serial.

| # | Pass | Rule |
|---|---|---|
| 1 | Freeze/thaw | Below `freeze_threshold`, `freeze_rate` of `surfaceWater` (and of `riverStore` on river cells) moves to `ice`. Above it, `thaw_rate` of `ice` returns to where it came from (surface on land, store on river cells). Frozen water is not routed |
| 2 | Glacier | `glacier += glacierInput`. Above `glacier_melt_threshold`, `glacier_melt_rate · (temperature − threshold)` of glacier moves to `surfaceWater` (capped at the glacier mass) |
| 3 | Infiltration | `available = waterToGround + extraWater⁺ + surfaceWater`. Soil takes `min(available · infil_rate[biome], soil_capacity[biome] − soilMoisture)`; the rest stays surface. Then `percolation_rate[biome] · soilMoisture` moves to `groundWater`, capped at `ground_capacity[biome]`, with any excess staying in soil |
| 4 | ET and sinks | `et = et_rate[biome] · temperature · sinks.etMultiplier`, taken from surface first, then soil. `sinks.draw` is taken from surface, then soil, then `riverStore` on river cells, clipped to what is there. Each amount goes to its own total (`etOut`, `drawOut`). A draw that couldn't be met is reported, not borrowed |
| 5 | Runoff and baseflow | `runoff = max(0, surfaceWater − retention[biome])` leaves the cell. `baseflow = baseflow_rate · groundWater` leaves too. `localOut = runoff + baseflow` |
| 6 | Cascade | Per tree, in topological order:<br>• **Cell node:** `store += inflow + localOut`; `release = r · store`; `store −= release`; `riverFlow = release`; add `release` to the downstream node's `inflow`.<br>• **Basin node:** `volume += inflow + Σ localOut of its cells`; open-water evaporation `basin_evap_rate · mean temperature over open cells · openArea` leaves to `basinEvapOut`; `overflow = max(0, volume − spillVolume)` goes to the spill cell's downstream.<br>• **Terminal:** ocean gives `seaOut`; a `kFlowSink` land terminal keeps its water as `surfaceWater`, which is lost through ET |
| 7 | Basin level | Per basin (parallel), the level comes from the volume and the capacity curve. A cell is inundated when its bed is below the level. Then `inundatedWeeks` / `exposedWeeks` update (a counter resets when its state flips) and `openWater` is set per cell. With `basinsFromLakesOnly`, each lake's volume is pinned at its spill volume. Inflow passes straight through, and any evaporation shortfall is a named source `fixedLakeMakeup` |
| 8 | Floodplain | River cells first update `meanFlow = meanFlow + α · (riverFlow − meanFlow)`, with α from `flood_mean_weeks` (~104). A river cell is in flood when `riverFlow > bankfull_factor · meanFlow` and `meanFlow > min_flood_flow`. A non-river cell is flooded when any neighbour is a river cell in flood whose elevation is ≥ its own minus `floodplain_rise`. This is a gather in direction order, so the result is order-independent |
| 9 | Passability | `Deep` where inundation depth > `ford_depth`, except corridor and crossing cells, which are capped at `Fordable`. Otherwise `Fordable` where inundated, `Wet` where flooded, `Frozen` where `ice` > `ice_crossable`, `Dry` elsewhere |
| 10 | Glacier counters | On the last week of Summer only: `snowYears++` where the season's minimum snow pack ≥ `glacier_snow_floor`, else it resets to 0. `glacierEligible = snowYears ≥ glacier_onset_years`. The season minimum is tracked weekly in `seasonMinSnow` |
| 11 | Totals | Clamp every field (non-negative, with excess pushed into the next store, never dropped). Then sum every store and every named flow in **double**, per range, combined in range order |

### Water balance (user decision 2026-11-17)

```
in      = Σ waterToGround + Σ glacierInput + Σ extraWater⁺ + fixedLakeMakeup
out     = seaOut + etOut + drawOut + basinEvapOut
storage = Σ (surface + soil + ground + riverStore + ice + glacier) + Σ basin volume
error   = in − out − (storage − storagePrev)
```

The pipeline reports `error` every week. The test is `|error| ≤ 1e-4 · max(in, out, 1)` per week, and the cumulative drift over 100 simulated years stays under `1e-3 ·` cumulative throughput. Feature 5 turns a breach into a health warning.

### Determinism

- Local passes write only their own cells.
- Cascade ranges own whole trees, and every tree is processed in a fixed order. Inflow into a node with several upstream neighbours is summed in topological order, which is the same at any range count.
- Basins are inside trees, so no basin is touched by two ranges.
- The floodplain is a gather.
- Totals are reduced in double per range, combined in range order, and never feed back.

The result is bit-identical at 1, 2, 7 and 16 ranges and with `SerialExecutor`.

### Profile

`DIA_PROFILE_SCOPE` per pass. Performance is report-only (parent decision 7). The cascade's parallel speed-up is bounded by the largest tree, and the pipeline reports the largest tree's node count so S-2.6 can show it.

## Public API (additions to `Cluiche/CoW/Shared/Hydrology/`)

```cpp
namespace CoW::Hydrology {

    struct WaterSinks {                     // filled by Feature 3 from providers; neutral when empty
        const float* etMultiplier = nullptr;    // N, product over providers, 1.0 neutral
        const float* draw         = nullptr;    // N, sum over providers, cell-depth units, 0 neutral
    };

    struct WaterInput {
        const float* waterToGround = nullptr;
        const float* temperature   = nullptr;
        const float* snowPack      = nullptr;
        const float* glacierInput  = nullptr;
        const float* extraWater    = nullptr;   // may be null = zero
        WaterSinks   sinks;                     // null arrays = neutral
        uint8_t      week = 0;                  // 0..51
    };

    struct WaterTotals {                    // double, one week
        double in, seaOut, etOut, drawOut, drawUnmet, basinEvapOut, fixedLakeMakeup;
        double storage, error;
        uint32_t floodedCells, deepCells, glacierCells, largestTreeNodes;
    };

    // Added to HydrologySimulation (Feature 1 creates the class)
    //   void              Step(const WaterInput&, Dia::DeferredStep::IParallelExecutor&);
    //   const WaterTotals& LastTotals() const;

    // Fixed-input driver for tests and A4-less stages
    class FixedWaterInput {
    public:
        void SetConstant(float waterToGround, float temperature);
        void SetScript(const DynamicArrayC<float>& weeklyWater, const DynamicArrayC<float>& weeklyTemp);
        WaterInput ForWeek(uint32_t week) const;
    };
}
```

Feature 1 owes three fields for this feature: `meanFlow` (float, saved), `seasonMinSnow` (float, saved), `glacierEligible` (uint8, saved). Feature 1 also owns the per-biome parameter table (`soil_capacity`, `ground_capacity`, `infil_rate`, `percolation_rate`, `et_rate`, `retention`), plus the scalars named above.

## Acceptance Criteria

### Purity and lifecycle
1. `Step` reads only `WaterInput`, the committed fields and the parameters. Poisoning A4's source arrays after Feature 3's capture changes nothing (DFS-002).
2. `Step` before `Initialise` asserts in Debug and is a no-op in Release.
3. A `WaterInput` with null `extraWater` or null sink arrays behaves exactly as if it held zeros and ones.

### Per-pass behaviour (hand-built grids)
4. **Freeze:** below the threshold, surface water and river store move into `ice` and are not routed. Above it they return to where they came from. Total water is unchanged.
5. **Glacier:** `glacierInput` adds to glacier mass. There is no melt below the threshold. Melt goes to surface water and routes the same week. Melt never exceeds the mass.
6. **Infiltration:** soil fills to capacity and no further. Percolation moves water to ground water up to its capacity. The excess stays in the store above.
7. **ET:** zero at temperature 0. Surface is drained before soil. With `etMultiplier = 2`, exactly twice the neutral ET is removed (unless clipped).
8. **Draw:** taken from surface, then soil, then river store. A draw larger than the water available removes everything and reports the rest in `drawUnmet`.
9. **Runoff:** zero below retention. Baseflow keeps a river cell flowing for many weeks after rain stops.
10. **Cascade, single chain:** a one-week pulse at the head of a 40-cell chain produces flow at the mouth in the same week. The peak is attenuated by the product of the release rates, and the tail decays over the following weeks. The pulse's water reaches `seaOut` in full (to tolerance) as weeks → ∞.
11. **Cascade, confluence:** two tributaries feeding one cell give the same result whichever is listed first in the input.
12. **Steeper is faster:** with everything else equal, a chain with larger drops delivers a higher same-week fraction to the mouth.
13. **Basin fill and spill:** inflow fills a basin and raises its level along the capacity curve. Above spill volume, the overflow leaves through the spill cell the same week.
14. **Chained basins:** three basins in series, all at spill, pass an inflow pulse to the sea in one week.
15. **Basin dry-down:** with no inflow, evaporation lowers the level. Cells above the level become exposed and `exposedWeeks` counts up. A refill resets it and counts `inundatedWeeks` instead.
16. **Lake-only worlds:** with `basinsFromLakesOnly`, a lake never changes level, inflow passes through, and an evaporation shortfall shows as `fixedLakeMakeup`.
17. **Terminal land sink:** water reaching a `kFlowSink` cell stays as its surface water and leaves only through ET.
18. **Flood:** a river cell at steady flow never floods. A pulse above `bankfull_factor ·` mean floods it and its low neighbours that week. The flag clears the week after the flow drops. A neighbour higher than `floodplain_rise` never floods.
19. **Self-calibration:** a small stream and a large river, each given a pulse proportional to its own mean, both flood.
20. **Passability:**
    - Deep inundation gives `Deep`.
    - A corridor or crossing cell at the same depth gives `Fordable`.
    - Shallow inundation gives `Fordable`, a flooded cell `Wet`, ice above threshold `Frozen`, and anything else `Dry`.
21. **Glacier counters:** these only change on the last week of Summer. `snowYears` increments when the season minimum is ≥ the floor and resets otherwise. Eligibility turns on at `glacier_onset_years` and off when the snow clears.

### Conservation and determinism
22. **Balance:** `|error| ≤ 1e-4 · max(in, out, 1)` every week, over 520 weeks on each hand-built grid and over 104 weeks on a generated Small world.
23. **Drift:** over 100 simulated years at Small (scripted input with a 4-year and a 40-year cycle), cumulative `|error|` stays under `1e-3 ·` cumulative throughput.
24. **No negatives:** no field is ever negative, and clamping never changes the balance.
25. **Range independence:** every field and total is bit-identical at 1, 2, 7 and 16 ranges and with `SerialExecutor`, over 104 weeks.
26. **Sinks are additive:** two providers' draws summed into `sinks.draw` give the same result as one provider supplying the sum. Two multipliers likewise give the same result as their product.

### Golden
27. Layer hashes (via `CellLayerGolden.h`) for the fixed-input driver on S-1 presets × seeds 1–3 at Small after 52 weeks. Stored in `Tests/GoogleTests/CoW/Golden/hydrology_layers.golden.json`.

## Files Touched

| File | Change |
|---|---|
| `Cluiche/CoW/Shared/Hydrology/HydrologyPipeline.h/.cpp` | New: passes 1–5 and 7–11 |
| `Cluiche/CoW/Shared/Hydrology/HydrologyRouting.h/.cpp` | New: downstream table, basin contraction, topological trees, release rates, cascade (pass 6) |
| `Cluiche/CoW/Shared/Hydrology/BasinCurve.h/.cpp` | New: capacity curve, level ↔ volume |
| `Cluiche/CoW/Shared/Hydrology/WaterInput.h` | New: `WaterInput`, `WaterSinks`, `WaterTotals`, `FixedWaterInput` |
| `Cluiche/CoW/Shared/Hydrology/HydrologySimulation.h/.cpp` | Add `Step`, `LastTotals` (Feature 1 creates the class) |
| `Tests/GoogleTests/CoW/Hydrology/TestHydrologyPipeline*.cpp` | New: ACs 1–26, by pass group |
| `Tests/GoogleTests/CoW/Hydrology/HydrologyTestGrids.h` | New: hand-built chain, confluence, basin-chain and floodplain grids. Shipped in `Cluiche/CoW/Shared/Hydrology/Testing/` per the test-utilities rule |
| `Tests/GoogleTests/CoW/Golden/hydrology_layers.golden.json` | New (AC 27) |
| `CoW.vcxproj` (+ filters), `GoogleTests.vcxproj` (+ filters) | Via `dia docs vcxproj-add` |

No Dia files, so no AD-001 sign-off.

## Binding Decisions

| ID | Decision | How this feature complies |
|----|----------|---------------------------|
| DFS-002 | `Run` reads only the captured working copy | `Step` takes `WaterInput` pointers into Feature 3's captured copies and reads no module state (AC 1) |
| PD-004 | No STL containers in public APIs | `DynamicArrayC` and raw pointers; the cascade's internal trees may use STL privately, as S-1 does |
| AD-002 | Four-tier model | CoW-Shared, pure logic |
| AD-003 | GDD owns design intent | Draft until GDD A5 is locked; the constants are provisional and live in Feature 1's parameter file |

## Test Plan

TDD per pass group on hand-built grids (`HydrologyTestGrids.h`), in this order:
1. Basin curve
2. Routing tables
3. Cascade
4. Local passes
5. Floodplain
6. Passability
7. Glacier counters
8. Totals

Then the conservation and determinism suites, then the golden. Scoped filter `*Hydrology*` while iterating; one full `--shards 2` run at the end. The exhaustive gap pass is a plan task. No mutation checks.

## Open Design Questions

1. **Should floods hold water?** Floods are flags today. Letting a flooded floodplain store water (recharging soil, delaying the downstream peak) is more realistic, and it makes floods matter to ecology through soil moisture rather than only the flag. It costs a store per cell, and a rule for when the water returns to the river.
2. **Draw order.** Taking draws from surface, then soil, then river means a thirsty consumer drains soil before it touches rivers. If A1 vegetation draws heavily, that could dry soils in a way that looks wrong next to a full river. Revisit once A1's provider exists.
3. **Same-week crossing on short, steep rivers.** With `r` near `r_max` on a short steep river, almost the whole pulse reaches the sea in one week and nothing floods. `r_max` may need to be well below 1, or tied to river length. This is a tuning check for S-2.6.

## Status

**Status:** Draft (2026-11-17). Interview done with the user the same day: topological cascade, basins inside the cascade, cell-depth units, local ground water with baseflow, injected water sinks (multiplier + draw), floods judged against each river's running mean, and double totals with a tight check. Approval waits on GDD A5's lock.
