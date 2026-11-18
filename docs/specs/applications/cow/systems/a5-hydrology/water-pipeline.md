# Feature Spec: A5 Water Pipeline

**Parent:** @docs/specs/applications/cow/systems/a5-hydrology/a5-hydrology.md (Feature 2)
**Status:** Approved (2026-11-17, user, after the locked-GDD alignment)
**Design source:** the locked GDD `C:\GitHub\DesignDocs\COW\Output Docs\GDD\08_SystemsDeepDive\A5_Hydrology\` (locked 2026-11-17: `A5_TechnicalSpec.md` §2–§12, DECISION-129..134, CHANGE-047/048). The GDD owns design intent and constants (AD-003). Amended to match it (aligned to locked GDD 2026-11-17); the differences that remain are listed under "GDD differences".
**Depends on:** A5 Feature 1 (`HydrologyFields`, `HydrologyTerrain`, `HydrologyParameters`, `Initialise`); DiaDeferredStep Feature 1 Task 1 (`IParallelExecutor`, `SerialExecutor` headers, built `c83a94ed`).

## Summary

The weekly step of A5 Hydrology, as pure logic. `HydrologySimulation::Step(input, executor)` takes one week of water reaching the ground (from A4, or from a fixed-input driver in tests) and moves it through the land:
- **freeze and thaw** (cells and basin surfaces)
- **glacier gain and melt**
- **infiltration** into soil, percolation to ground water, springs
- **evapotranspiration and injected water sinks**
- **runoff and baseflow**
- a **topological cascade** that carries water downhill along S-1's flow directions and through lake basins in the same week
- **basin levels and inundation**
- **floodplain floods** judged against each river's own running mean, with soil recharge
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
- **Standing flood water.** A flood is a flag on cells this week plus a recharge of floodplain **soil** from the river's channel store (GDD §9). Floodplain cells hold no extra surface water, which keeps conservation and routing simple (Open Design Question 1, resolved by the GDD).
- **Biome change.** The pipeline updates `inundatedWeeks`, `exposedWeeks`, `snowYears` and glacier mass. A3 reads them.
- **Air moisture.** A4's evaporation into the air is a separate, non-conserving model. A5's open-water evaporation and evapotranspiration are A5 sinks. They are not added to A4's moisture, so nothing is double-counted inside A5.
- **Lateral aquifer flow.** Ground water is local; it reaches rivers only through baseflow (user decision 2026-11-17).

## Design

### Units (user decision 2026-11-17)

All water is in **cell-depth units**: 1.0 is one cell covered to the depth of A4's `waterToGround` scale. This is the GDD's "water unit (wu)" under another name. There is no conversion.
- `soilMoisture` is bounded by the biome's `soil_capacity`, and `groundWater` by `soil.gw_capacity`. `surfaceWater` is unbounded but runs off above `routing.surface_retention`.
- `riverStore` (the GDD's channel store), `ice`, `glacier` and basin `volume` are unbounded sums.
- Clamping never destroys water. A store over capacity spills into the next store, and only the final totals check proves nothing was lost.
- Basin volume is `Σ (level − bed) · basin.depth_scale` over cells with bed below the level (GDD §8.1).

### Grid tables (built once at `Initialise`, Feature 1 data, Feature 2 logic)

- **Downstream index per land cell** from S-1's full-grid flow direction on the filled surface, **except inside a real basin**, where Feature 1 has replaced it with steepest descent on bed elevation toward the pit. Ocean cells, and land no ocean reaches (`kFlowSink`), are terminals. Tiny depressions are not basins (Feature 1): their cells keep S-1's directions.
- **Basins are nodes, footprints are not contracted** (GDD §7.2, aligned to locked GDD 2026-11-17). A real basin is one node that holds the volume. Its footprint cells stay individual cells: a footprint cell that is not inundated routes along the in-basin directions like any land cell. Water goes into the basin's `inflow` when it is routed to an inundated cell or to the pit. The node's overflow leaves through its spill cell.
- **Topological order.** One order over cells and basin nodes, by a topological sort of the routing graph (Kahn's algorithm, ties by ascending cell index). Every footprint cell and every cell upstream of the basin comes before the basin node, and the spill cell comes after it. The order is not "descending filled elevation" any more: in-basin directions break that rule. Nodes are grouped into **trees** by terminal. Trees are independent: no water crosses between them.
- **Release rate `k` per land cell** (GDD §7.2):
  - river cell: `k = clamp(release_min + release_slope · drop, release_floor, release_ceil)`, where `drop = max(0, elevation − downstream elevation)`. A terminal's `drop` is 0;
  - non-river cell: `k = routing.hillslope_release` (0.9; it was a fixed 1.0).
  - Defaults give 0.3–0.85 on rivers.
- **Basin capacity curve:** basin cells sorted by bed elevation, giving cumulative volume `V(level)` with `basin.depth_scale`. The level for a volume is a binary search plus linear interpolation. `capacity = V(spillHeight)`.

**Travel time** (GDD §7.2). A fraction `k` of the inflow moves on in the same week, so after `n` cells about `k^n` of a pulse has gone through. The pulse's centre of mass moves about `k / (1 − k)` cells per week: about 5.7 on steep reaches (`k` = 0.85), 1 at `k` = 0.5, and 0.4 at `k` = 0.3. A pulse races out of the mountains, then slows and piles up in the flat lowlands, where floods happen. This resolves the earlier worry that the whole river was crossed in one step (Open Design Question 3).

### Inputs (`WaterInput`)

Pointers into Feature 3's captured copies, N cells each unless noted:

| Field | Meaning |
|---|---|
| `waterToGround` | Rain + snowmelt from A4 this week |
| `temperature` | A4 air temperature [0, 1] |
| `snowPack` | A4 snow pack [0, 1], read once a year at `glacier.check_week` (glacier counters only) |
| `glacierInput` | A4's glacier ice for this week, on glacier-eligible cells only. It has two parts, both computed by A4: snowfall above A4's 1.0 cap, and, once a year at `glacier.check_week`, the **firn hand-off**: snow above `glacier.snow_floor` that A4 moves out of its own `snow_pack` (A4 TechSpec §11.3, CH-A5-01). A5 only adds it to `glacier` |
| `extraWater` | Signed per-cell water from influences ("wetter" adds; "drier" goes through sinks). Zero when none |
| `sinks.etMultiplier` | Product of all providers' ET multipliers; 1.0 when none |
| `sinks.draw` | Sum of all providers' absolute draws, in cell-depth units; 0 when none |
| `week` | `WeekOfYear` (the season boundary for the glacier counters) |

### The pipeline

Local passes run in parallel over cell ranges. The cascade runs in parallel over tree ranges, and each tree is serial. "Inundated" in a pass means **last week's committed flags** (the stable copy). A5 never writes A4's `snowPack`.

**Inundated cells (GDD CH-A5-06, aligned to locked GDD 2026-11-17).** An inundated cell keeps no land stores. Water that arrives on it (rain, melt, springs, influence) goes straight into its basin's volume. It skips passes 3, 4, 5, 6 (as a node) and 8 (floodplain).

| # | Pass | Rule |
|---|---|---|
| 0 | Inputs | On land cells that are not inundated: `surfaceWater += waterToGround + extraWater⁺`. On inundated cells the same amount is added to the basin's direct input instead. `glacier += glacierInput` (pass 2) |
| 1 | Freeze/thaw | **Cells.** If `T < freeze.threshold`: `f = freeze.rate · (threshold − T) / threshold`, and `f` of `surfaceWater` and `f` of `riverStore` move to `ice` (every land cell, not only rivers). Else if `T > freeze.thaw_temp` and `ice > 0`: `melt = min(ice, thaw_rate · (T − thaw_temp) · (ice + ice_melt_floor))` moves from `ice` to `surfaceWater`. Between the two temperatures nothing happens (hysteresis). Frozen water is not routed. **Basins.** `iceFraction` rises by `freeze.rate` per week (to at most 1) while the mean `T` over the basin's inundated cells is below `freeze.threshold`, and falls by `thaw_rate` (to at least 0) above `thaw_temp`. With no inundated cells it falls. Basin ice is a state of the surface only: the volume stays in `volume`, so nothing is counted twice |
| 2 | Glacier | `glacier += glacierInput`. Above `glacier.melt_threshold`: `melt = min(glacier, melt_rate · (T − melt_threshold) · (glacier + ice_melt_floor))` moves to `surfaceWater` (or to the basin on an inundated cell) and routes **this week** |
| 3 | Infiltration | `cap = soil_capacity[biome]`, `rate = infil_rate[biome]`, times `soil.frozen_ground_factor` if `T < freeze.threshold`. `infil = min(surfaceWater, rate, cap − soilMoisture)` moves to soil. Percolation: `excess = max(0, soilMoisture − field_capacity · cap)`, and `percolation_rate · excess` moves to `groundWater`. **Springs:** if `groundWater > soil.gw_capacity`, the excess returns to `surfaceWater` and ground water is set to the capacity |
| 4 | ET and sinks | `etSoil = min(soil, evap.et_rate · T · veg_factor[biome] · sinks.etMultiplier · soil)`. `etSurface = min(surface, evap.surface_rate · T · surface)`. `sinks.draw` is taken from surface, then soil, never from the channel store (GDD §6.1, CHANGE-049), clipped to what is there. Each amount goes to its own total (`etOut`, `drawOut`). A draw that couldn't be met is reported, not borrowed. A draw on an inundated cell is unmet |
| 5 | Runoff and baseflow | `runoff = max(0, surfaceWater − routing.surface_retention)` and `baseflow = baseflow_rate · groundWater` both move into `riverStore` (the channel store) of the same cell, on **every** land cell |
| 6 | Cascade | Per tree, in topological order. `route(amount, target)` adds to the target's `inflow`, except that a target which is an **inundated cell or a basin's pit** adds to that basin's `inflow`.<br>• **Cell node (not inundated):** `total = riverStore + inflow`; `out = k · total`; `riverStore = total − out`; `riverFlow = out` (river cells; 0 elsewhere); `route(out, downstream)`. This is the same number as the earlier "`store += inflow + localOut`, then `release = r · store`", because pass 5 now puts runoff and baseflow into `riverStore` before the cascade.<br>• **Basin node:** `volume += inflow + direct input on inundated cells`; open-water evaporation `basin.evap_rate · mean T over inundated cells · openCells` leaves to `basinEvapOut`, where `openCells = inundatedCount · (1 − iceFraction)`; `overflow = max(0, volume − capacity)` is routed to the spill cell **in this same pass**, and `volume = min(volume, capacity)`; `inflow = 0`.<br>• **Terminal:** ocean gives `seaOut`; a `kFlowSink` land terminal keeps its water as `surfaceWater`, which is lost through ET (the GDD does not say what a `NONE` terminal does; this rule is kept) |
| 7 | Basin level | Per basin (parallel): `level = V⁻¹(volume)`. A cell is inundated when its bed is below the level, and its `depth = (level − bed) · basin.depth_scale`. A cell **newly inundated** moves its `surfaceWater`, `soilMoisture`, `groundWater` and `riverStore` into the basin volume (its `ice` stays on the cell and thaws into the basin). A cell **newly exposed** starts with empty land stores. Then `inundatedWeeks` and `exposedWeeks` update (`exposedWeeks` counts only where the cached biome is Lake and the cell is not inundated; each counter resets when its condition breaks). `openWater` per cell: Ocean 1; inundated `1 − iceFraction`; river `wetness.river_open_water · min(1, riverFlow / max(meanFlow, flood.min_flow))`; else 0. With `basinsFromLakesOnly`, each lake's volume is pinned at its capacity, inflow passes straight through, and any evaporation shortfall is a named source `fixedLakeMakeup` |
| 8 | Floodplain and counters | River cells: `meanFlow += α · (riverFlow − meanFlow)` with `α = 1 − 0.5^(1/flood.mean_weeks)`. Then, for each river cell, `bankfull = bankfull_factor · max(meanFlow, flood.min_flow)` and the river is **in flood** when `riverFlow > bankfull`. **Flags** (a gather in direction order): the river cell is `flooded`, and so is every non-inundated neighbour whose elevation is at most the river's plus `floodplain_rise`. **Recharge** (GDD §9): for each river cell in flood, in fixed cell-id order, `surplus = riverFlow − bankfull` and `recharge = min(recharge_share · surplus, riverStore)`. The plain is the river cell plus those neighbours. `recharge` is split equally across the plain's land cells, each taking at most its soil room (`soil_capacity − soilMoisture`). Anything a full soil can't take stays in the river. `riverStore` drops by what was given. Counters: `lowFlowWeeks` (river cells) increments when `riverFlow < low_flow_fraction · meanFlow` and the cell is **not frozen** (`ice < ice_crossing_min`), and resets when the flow recovers. **A frozen week neither counts nor resets** (GDD CH-A5-02). `glacierWeeks` increments when `glacier ≥ glacier.biome_min`, else resets to 0. `glacierFreeWeeks` is the mirror: it increments while `glacier < glacier.biome_min` and resets to 0 at or above it (A3's glacial retreat reads `glacierFreeWeeks ≥ 260`; GDD §12.1). Inundated cells skip the flood part |
| 9 | Passability | First match wins (GDD §11): (1) inundated and `basin.iceFraction ≥ freeze.ice_crossing_min`, or a river cell with `ice ≥ ice_crossing_min`: `Frozen`. (2) Inundated, `depth > basin.ford_depth` and **not** protected (`isCrossing`): `Deep`. (3) Inundated, any other case: `Fordable`. (4) `flooded`: `Wet`. (5) `Dry`. On `basinsFromLakesOnly` worlds the bed equals the surface, so a depth can't be computed. S-1 lake cells are always `Deep` there (step 2 for every lake cell), **except protected cells (corridor, crossing and fixed-site cells), which stay `Fordable`**, so the protected-cell rule holds (user decision, matched by GDD EC-15) |
| 10 | Glacier counters | Only when the week of the year is `glacier.check_week` (end of Summer): `snowYears++` where `snowPack ≥ glacier.snow_floor`, else it resets to 0. `glacierEligible = snowYears ≥ glacier.onset_years`. A4 runs first in the week, and after the firn hand-off its snow pack sits at the floor, which still counts as survived. When snow fails to survive a summer, eligibility ends, but the glacier mass stays and keeps melting (the long melt pulse at the end of an ice age). There is no `seasonMinSnow` any more (aligned to locked GDD 2026-11-17) |
| 11 | Totals | Clamp every field (non-negative, with excess pushed into the next store, never dropped). Then sum every store and every named flow in **double**, per range, combined in range order |

**Protected cells** (corridors, crossings, Hearth, Nexus and world-throne cells) are never `Deep` (invariant, GDD §11). Floods make cells `Wet`, never blocked, and ice makes water easier to cross, never harder.

### Water balance (user decision 2026-11-17)

```
in      = Σ waterToGround + Σ glacierInput + Σ extraWater⁺ + fixedLakeMakeup
out     = seaOut + etOut + drawOut + basinEvapOut
storage = Σ (surface + soil + ground + riverStore + ice + glacier) + Σ basin volume
error   = in − out − (storage − storagePrev)
```

The pipeline reports `error` every week. The test is `|error| ≤ 1e-4 · max(in, out, 1)` per week, and the cumulative drift over 100 simulated years stays under `1e-3 ·` cumulative throughput. Feature 5 turns a breach into a health warning. A breach never "corrects" the error by adding or removing water (GDD EC-12). Ice and glacier count as stored water. `basin.iceFraction` is not water. The firn hand-off is part of `glacierInput`, so it is an `in`; A4's snow pack is outside A5's balance.

### Determinism

- Local passes write only their own cells.
- Cascade ranges own whole trees, and every tree is processed in a fixed order. Inflow into a node with several upstream neighbours is summed in topological order, which is the same at any range count.
- Basins are inside trees, so no basin is touched by two ranges.
- The floodplain flags are a gather. The recharge is a serial loop in cell-id order over only the rivers in flood, so overlapping plains give the same result at any range count.
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
        const float* glacierInput  = nullptr;   // above-cap snowfall + the yearly firn hand-off (A4)
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

    //   void              RebaselineStorage();   // after a preset or state edit, so the edit isn't counted as balance error (Feature 6)

    // Fixed-input driver for tests and A4-less stages (uniform per week). Feature 6 adds
    // ScriptedWaterInput for per-cell scripted inputs, including snowPack and glacierInput.
    class FixedWaterInput {
    public:
        void SetConstant(float waterToGround, float temperature);
        void SetScript(const DynamicArrayC<float>& weeklyWater, const DynamicArrayC<float>& weeklyTemp);
        WaterInput ForWeek(uint32_t week) const;
    };
}
```

Feature 1 owns the fields this feature needs: `meanFlow`, `glacierEligible`, `lowFlowWeeks`, `glacierWeeks`, `glacierFreeWeeks`, and each basin's `iceFraction` (all saved). There is no `seasonMinSnow`. Feature 1 also owns the per-biome table (`soil_capacity`, `infil_rate`, `veg_factor`) and every scalar named above (43 rows, GDD names mapped there).

## Acceptance Criteria

### Purity and lifecycle
1. `Step` reads only `WaterInput`, the committed fields and the parameters. Poisoning A4's source arrays after Feature 3's capture changes nothing (DFS-002).
2. `Step` before `Initialise` asserts in Debug and is a no-op in Release.
3. A `WaterInput` with null `extraWater` or null sink arrays behaves exactly as if it held zeros and ones.

### Per-pass behaviour (hand-built grids)
4. **Freeze:** below `freeze.threshold`, a share of surface water and channel store (`freeze.rate · (threshold − T) / threshold`) moves into `ice` and is not routed. Above `freeze.thaw_temp`, ice melts to surface water by the GDD formula, and the last thin ice melts in finite time. Between the two temperatures nothing changes. Total water is unchanged.
4a. **Basin ice:** a basin's `iceFraction` rises at `freeze.rate` per week while the mean temperature over its inundated cells is below the threshold, falls at `thaw_rate` above `thaw_temp`, stays in [0, 1], and does not change `volume`.
5. **Glacier:** `glacierInput` (above-cap snowfall and the firn hand-off alike) adds to glacier mass. There is no melt at or below `glacier.melt_threshold`. Melt follows `melt_rate · (T − threshold) · (glacier + ice_melt_floor)`, goes to surface water and routes the same week. Melt never exceeds the mass.
6. **Infiltration:** soil takes at most `infil_rate` per week and fills to capacity and no further, and frozen ground takes `frozen_ground_factor` of that. Soil above field capacity percolates at `percolation_rate`. Ground water over `gw_capacity` comes back up as surface water (springs), and no water is lost.
7. **ET:** zero at temperature 0. Soil ET scales with `veg_factor`, so a forest loses more than a desert of equal soil. With `etMultiplier = 2`, exactly twice the neutral soil ET is removed (unless clipped). Surface evaporation is separate and ignores the multiplier.
8. **Draw:** taken from surface, then soil, never from the channel store (GDD §6.1). A draw larger than the water available removes everything and reports the rest in `drawUnmet`.
9. **Runoff:** zero below `surface_retention`. Baseflow keeps a river cell flowing for many weeks after rain stops. Runoff and baseflow go into the cell's own channel store, on hillslope cells as well as river cells.
10. **Cascade, single chain:** a one-week pulse at the head of a 40-cell chain produces flow at the mouth in the same week. The peak is attenuated by the product of the release rates, and the tail decays over the following weeks. The pulse's water reaches `seaOut` in full (to tolerance) as weeks → ∞.
10a. **Release and travel:** on a uniform river chain, `k = clamp(release_min + release_slope · drop, release_floor, release_ceil)` per cell, and non-river cells use `hillslope_release`. The pulse's centre of mass advances about `k / (1 − k)` cells per week (checked at `k` = 0.5 and `k` = 0.85, with a tolerance). The result equals an independent hand calculation of `out = k · (store + inflow)`.
11. **Cascade, confluence:** two tributaries feeding one cell give the same result whichever is listed first in the input.
12. **Steeper is faster:** with everything else equal, a chain with larger drops delivers a higher same-week fraction to the mouth.
13. **Basin fill and spill:** inflow fills a basin and raises its level along the capacity curve. Above capacity, the overflow leaves through the spill cell the same week.
13a. **Dry basin:** water landing on a dry basin's footprint flows along the in-basin directions to the pit, enters the basin volume, and the level rises from the lowest bed. It does not cross the basin to the spill.
13b. **Inundated cells (CH-A5-06):** an inundated cell's soil, infiltration, ET, runoff and flood flag do not change. Its rain goes into the basin volume. A cell that becomes inundated moves its land stores into the volume (the balance still closes), and a cell that is exposed again starts empty.
13c. **Tiny depression:** a depression below `min_cells` or `min_capacity` is never inundated and water routes through it at the S-1 directions.
14. **Chained basins:** three basins in series, all at spill, pass an inflow pulse to the sea in one week.
15. **Basin dry-down:** with no inflow, evaporation lowers the level. Cells above the level become exposed and `exposedWeeks` counts up. A refill resets it and counts `inundatedWeeks` instead.
16. **Lake-only worlds:** with `basinsFromLakesOnly`, a lake never changes level, inflow passes through, and an evaporation shortfall shows as `fixedLakeMakeup`. Lake cells are `Deep`, except protected cells, which are `Fordable`.
17. **Terminal land sink:** water reaching a `kFlowSink` cell stays as its surface water and leaves only through ET.
18. **Flood:** a river cell at steady flow never floods. A pulse above `bankfull_factor ·` mean floods it and its low neighbours that week. The flag clears the week after the flow drops. A neighbour higher than `floodplain_rise`, or inundated, never floods.
18a. **Recharge:** in a flood week, floodplain soil gains `min(recharge_share · surplus, riverStore)` split equally, each cell limited by its soil room. The river's `riverStore` drops by what was given, and the balance closes. Two overlapping plains give the same result at any range count.
19. **Self-calibration:** a small stream and a large river, each given a pulse proportional to its own mean, both flood.
20. **Passability** (first match wins):
    - Inundated with `iceFraction ≥ ice_crossing_min`, or a river cell with `ice ≥ ice_crossing_min`, gives `Frozen`. This beats `Deep`.
    - Inundation deeper than `ford_depth` gives `Deep`. A protected cell (corridor, crossing, Hearth, Nexus, world-throne) at the same depth gives `Fordable`.
    - Shallower inundation gives `Fordable`, a flooded cell `Wet`, and anything else `Dry`. Ice on a dry land cell does not make it `Frozen`.
21. **Glacier counters:** `snowYears` and `glacierEligible` only change on the week of the year equal to `glacier.check_week`. `snowYears` increments when `snowPack ≥ glacier.snow_floor` at that week (a value exactly at the floor counts) and resets otherwise. Eligibility turns on at `glacier.onset_years` and off when the snow clears. Existing glacier mass stays and keeps melting.
21a. **Frozen low flow (CH-A5-02):** a river week with `ice ≥ ice_crossing_min` neither increments nor resets `lowFlowWeeks`. A non-frozen week with `riverFlow < low_flow_fraction · meanFlow` increments it, and a recovered flow resets it.
21b. **Glacier weeks:** `glacierWeeks` counts consecutive weeks with `glacier ≥ glacier.biome_min` and resets below it. `glacierFreeWeeks` counts consecutive weeks below it and resets at or above it. Exactly one of the two is 0 every week. A cell at exactly `biome_min` counts as glaciated.
21c. **Open water:** a river cell's `openWater` is `wetness.river_open_water · min(1, riverFlow / max(meanFlow, flood.min_flow))`, an inundated cell's is `1 − iceFraction`, and Ocean is 1.

### Conservation and determinism
22. **Balance:** `|error| ≤ 1e-4 · max(in, out, 1)` every week, over 520 weeks on each hand-built grid and over 104 weeks on a generated Small world.
23. **Drift:** over 100 simulated years at Small (scripted input with a 4-year and a 40-year cycle), cumulative `|error|` stays under `1e-3 ·` cumulative throughput.
24. **No negatives:** no field is ever negative, and clamping never changes the balance.
25. **Range independence:** every field and total is bit-identical at 1, 2, 7 and 16 ranges and with `SerialExecutor`, over 104 weeks.
26. **Sinks are additive:** two providers' draws summed into `sinks.draw` give the same result as one provider supplying the sum. Two multipliers likewise give the same result as their product.

### Golden
27. Layer hashes (via `CellLayerGolden.h`) for the fixed-input driver on S-1 presets × seeds 1–3 at Small after 52 weeks. Stored in `Tests/GoogleTests/CoW/Golden/hydrology_layers.golden.json` under keys prefixed `pipeline/`. Feature 6 uses `scenario/`.

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
| AD-003 | GDD owns design intent | The passes and constants follow the locked GDD (2026-11-17); the constants live in Feature 1's parameter file. Remaining differences are listed below |

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

## GDD differences

Only real remaining differences (aligned to locked GDD 2026-11-17):

| Topic | GDD | This spec |
|---|---|---|
| Units | water units (wu) | Same unit, called cell-depth (user decision) |
| Balance check | `BALANCE_TOLERANCE` 1e-5 × stored | `1e-4 · max(in, out, 1)` per week plus a cumulative drift bound, in double totals (user decision) |
| Water sinks | None: A1 reads soil wetness only. "Drier" influence takes from soil, clipped | `IWaterSinkProvider` ET multiplier and absolute draw (surface, soil, then channel store). A5 never depends on A1 (user decision) |
| Glacier melt timing | Melt is pass 8 (after routing) and "joins the surface, then runoff" | Melt is pass 2, so it routes the same week. The end-of-ice-age melt pulse starts a week sooner |
| `NONE` terminal | Not stated | A land terminal keeps its water as surface water and loses it through ET |
| Snow survival test | Sample of `snowPack` at the check week | Same. The Cluiche season-minimum counter is gone |
| Ice on a lake-only world | Not stated | Frozen beats the lake-only `Deep` (pass 9 order) |

## Open Design Questions

1. ~~**Should floods hold water?**~~ **Resolved by GDD §9:** a flood is a flag plus a recharge of floodplain soil from the river's channel store. Floodplains hold no standing water.
2. **Draw order.** Draws come from surface, then soil, never from rivers (GDD §6.1, CHANGE-049). That means a thirsty consumer drains soil before it touches rivers. If A1 vegetation draws heavily, that could dry soils in a way that looks wrong next to a full river. Revisit once A1's provider exists.
3. ~~**Same-week crossing on short, steep rivers.**~~ **Resolved by GDD §7.2:** `k` follows the slope and is clamped to 0.3–0.85, so a pulse moves about `k/(1−k)` cells per week (5.7 on the steepest reaches, under 1 on plains) and slows in the lowlands. What remains is a tuning check in S-2.6: the GDD's target is a lowland peak 3–10 weeks after the mountain peak, with `RELEASE_MIN`, `RELEASE_SLOPE` and `HILLSLOPE_RELEASE_RATE` as the first knobs.
4. **Where does the glacier melt belong in the pass order?** The GDD lists glacier at pass 8 but says the melt joins the surface and then runs off. Pass 2 does that within one week. If S-2.6 wants the GDD's one-week delay, move the melt after the cascade. Not decided.
5. **Inundated cells and sinks.** A draw on an inundated cell is unmet today. If a consumer should draw from the lake, a basin draw is a small addition. Revisit with A1's provider.

## Status

**Status:** Approved (2026-11-17, user, after the locked-GDD alignment). Interview done with the user the same day: topological cascade, basins inside the cascade, cell-depth units, local ground water with baseflow, injected water sinks (multiplier + draw), floods judged against each river's running mean, and double totals with a tight check. Aligned to the locked GDD on 2026-11-17: slope-based release (0.3–0.85, hillslope 0.9), in-basin directions, tiny-depression filter, freeze hysteresis and basin ice, springs, flood recharge, firn hand-off check, passability order and the protected-cell cap, frozen low-flow weeks. Approval is a separate step.

Plan: [water-pipeline.plan.md](water-pipeline.plan.md) (written 2026-11-17, not started).
