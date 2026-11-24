# Feature Spec: A4 CA Pipeline

**Parent:** @docs/specs/applications/cow/systems/a4-weather/a4-weather.md (Feature 2)
**Status:** Approved (2026-11-16); region-forcing amendment re-approved 2026-11-16 with Feature 6. Amended 2026-11-17 (user decisions: A5 hand-off, Features 7–9): ground-water drain removed, `waterToGround` and `glacierInput` outputs, A5 inputs latched at `Capture`. Pass numbers are unchanged; pass 10 is retired.
**Amended 2026-11-17 (aligned to locked GDD, CHANGE-047; row 6d):** new pass 0 (Influence), `pressureAnomaly` in pass 2 with transport in 5a/5b and decay in new pass 5c, cycle offsets in passes 1, 3 and 4, firn hand-off in pass 9, `Capture` takes `weekOfYear` and an `InfluenceSnapshot`, `kPassCount` 14.
**Design source:** `DesignDocs/COW/Output Docs/GDD/08_SystemsDeepDive/A4_Weather/A4_TechnicalSpec.md` §2–§12 incl. §4.1 and §11.3 (locked 2026-11-17, CHANGE-047), `A4_EdgeCaseMatrix.md`
**Depends on:** A4 Feature 1 (`WeatherFields`, `WeatherTerrain`, `WeatherParameters`, `IBiomeSeasonalBaseline`, `Initialise`); DiaDeferredStep Feature 1 Task 1 (`IParallelExecutor`, `SerialExecutor` headers)

**Amended 2026-11-18 (world-sim review, user decisions):** see "World-sim review amendment" below.

## Summary

The weekly step of the weather cellular automaton, as pure logic. It runs the GDD's pipeline steps over every cell of the hex grid as full-grid passes on a working copy of the weather state. The GDD's §2 steps 0–11 become passes 0–9 plus a finish: the §12 ground-water drain is deleted and moves to A5 Hydrology (amended 2026-11-17, A5 Hydrology); GDD step 0 (forcing) is pass 0 plus the `Capture`-latched cycle offsets; GDD steps 10–11 (hand water to A5, output to A1) are outputs and `Commit`, not passes. With the carried `pressureAnomaly` the profile has **14 entries** (amended 2026-11-17, row 6d). It is deterministic and bit-identical at any `ParallelFor` range count and in any range order. It does not know about threads, the scheduler, the bus or the save system.

The feature adds three things to `WeatherSimulation` (Feature 1 creates the class):

- a `Capture` / `Run` / `Commit` split, which is the shape DiaDeferredStep's `IDeferredStep` needs (Feature 3 adapts it)
- `Step`, the synchronous composition of the three
- the forcing and wind-override inputs the debug commands use (Feature 5 only issues them)

## Goals

1. The GDD steps run in GDD order (passes 0–9 and a finish; §12 drain removed (amended 2026-11-17, A5 Hydrology)), one `ParallelFor` pass per step (transport is two passes, and the anomaly decay is its own pass 5c), over a working set. Readers see only committed state.
2. **Bit-identical results** for the same inputs and tick count at any range count (1, 3, 17, one range per cell) and with ranges executed in reverse order. No RNG, no atomics in anything that feeds the simulation, no dependence on iteration order.
3. Transport is a **gather** with a defined upwind-edge inflow (`boundary.edge_inflow`, default on).
4. `Run` is a pure function of what `Capture` latched. It reads no SimPU-owned state (DFS-002).
5. One step at Large costs ≤ 10 ms CPU summed across ranges, with Small ≤ 4.2 ms and Standard ≤ 6.3 ms. It is reported and has a hard ceiling (see AC 37).

## Non-Goals

- `WeatherStep : IDeferredStep`, scheduling, apply keys, `WeatherUpdated` publication: **Feature 3**.
- Serialising committed or pending state: **Feature 4**.
- The `cow.weather.*` commands, debug layers and panel: **Feature 5**. This feature supplies the forcing records they issue and the grid totals they display.
- Presets and the four §6 scenarios: **Feature 6**. They use this pipeline unchanged.
- Parameter file loading and validation: **Feature 1** (this feature adds one constant, `cloud_decay`, to the struct).
- Initial seeding and spin-up orchestration: Features 1 and 3. Spin-up is just 52 calls to `Step`.

## Design

### Lifecycle on `WeatherSimulation`

```
Capture(season, weekOfYear, baselineProvider, hydro, influences)
                                    SimPU      copy committed → working; latch season, week of year, parameters,
                                               forcings, biome edits, and the 13-entry seasonal
                                               baseline table (provider is called here and never in Run);
                                               latch A5's HydrologyInputs (surfaceWetness, openWater,
                                               glacierEligible, glacierCheckWeek, glacierSnowFloor; amended 2026-11-17, A5 Hydrology and row 6d)
                                               and the InfluenceSnapshot (Feature 7; empty = pass 0 skipped)
LatchClimate(evalDay)               SimPU      Feature 8: evaluates the cycles for the step day into the working ClimateOffsets
Run(executor)                       workers    the pipeline over the working set only
Commit()                            SimPU      swap working → committed; week++; decrement forcing timers
                                               (Feature 9 turns this swap into a three-buffer rotation and records commitDay)
Step(season, baseline, executor)               Capture; Run; Commit — the synchronous path (WeatherRunner)
```

The system spec's Public Interfaces lists only `Step(Season, IParallelExecutor&)`. It needs the three methods, and `Step` needs the baseline provider, because a virtual call into A1's provider from a worker would break DFS-002. This is a spec-sync amendment (see Files Touched).

`Run` reads: the working fields, the working biome array, the immutable terrain (elevation, latitude, water source: owned by `WeatherSimulation`, never mutated after `Initialise`), the latched parameters, season, week of year, baseline table, forcings, the latched `HydrologyInputs` (amended 2026-11-17, A5 Hydrology), the latched `InfluenceSnapshot` and `ClimateOffsets` (amended 2026-11-17, row 6d) and the precomputed neighbour table. Nothing else.

### Grid tables (built once at `Initialise`)

- `uint32_t neighbours[N * 6]`, from `Dia::Geometry2D::HexTopology::NeighbourAt`, `kNoCell` past the edge. Passes index this table; they never call the topology.
- `static constexpr` unit vectors for directions `d = 0..5`, at on-screen angle `30° + 60°·d`, counter-clockwise, y-down world space. This is the angle convention for every wind angle in the system, including the `wind_direction` V-1 reads.
- Scratch arrays, allocated at `Initialise` and reused: `moistureOut`, `heatOut`, `anomalyOut` (float; `anomalyOut` added 2026-11-17, row 6d), `dirIndex` (`uint8_t`, the snapped downwind direction), and pass 0's per-cell influence deltas (temperature, moisture; Feature 7). **`Run` does no heap allocation.**

### Stable reads, by construction

The system spec says neighbour-reading steps read "a stable copy". The rule this feature enforces, and tests, is:

> **No pass reads a field from a neighbour that the same pass writes.**

SoA lets every neighbour-reading pass satisfy this without copying whole fields:

| Pass | Reads from neighbours | Writes | Why it is safe |
|---|---|---|---|
| 3 Wind | `pressure` | `windDirection`, `windSpeed`, `dirIndex` | pressure is not written in pass 3 |
| 5a / 6a Outflow | nothing | `moistureOut` + `anomalyOut` / `heatOut` | local |
| 5b / 6b Gather | `*Out`, `dirIndex` | own `moisture` + `pressureAnomaly` / `temperature` | the `*Out` scratch isn't written in pass b |
| 5c Anomaly decay | nothing | own `pressureAnomaly` | local |
| 7 Orographic | `elevation` (immutable terrain) | own `temperature`, `moisture`, … | elevation never changes |

This replaces the system spec's "stable copy" wording, which implied snapshotting fields. It gives the same guarantee for two scratch arrays instead of nine copies. A reverse-order test (AC 31) is what proves the rule holds.

### The pipeline

Each row is one pass (`DIA_PROFILE_SCOPE` per pass; backs the `profile` command). "Local" passes read and write only their own cell.

| Pass | GDD | Behaviour (parameter names as in A4_TuningParameters §14) |
|---|---|---|
| 0 Influence | §2 step 0, §18 | (amended 2026-11-17, row 6d) Skipped when the latched `InfluenceSnapshot` is empty. Otherwise per cell, in ascending influence id order (Feature 7): resets `influenceWater`, builds `dPressure`, `dTemperature`, `dMoisture` and `waterWeight`, caps them (`max_delta_*`, `cap_water`), adds `dPressure` into `pressureAnomaly` (clamped ±`pressure_anomaly_max`) and stores `dTemperature`, `dMoisture` and `influenceWater` for the points below. The cycle offsets (Feature 8) are not computed here: `LatchClimate` fills them at `Capture` |
| 1 Solar | §3 | **Weekly reset first:** `precipitation = 0`; `waterToGround = 0`; `glacierInput = 0` (amended 2026-11-17, A5 Hydrology); `cloudCover *= (1 − cloud_decay)`. Then `seasonTemp = clamp(baseline[biome[c]] + tempOffset[c])` (the climate-cycle offset, §19; 0 with no cycles), latitude modifier, cloud albedo, heat capacity by water source, relax `temperature` toward `seasonTemp`, clamp [0,1]. Local. **Influence point:** `dTemperature` is added right after this pass (Feature 7), clamped [0,1] |
| 2 Pressure | §4 | `pressure = clamp(0.5 − (temperature − 0.5)·pressure_sensitivity + pressureAnomaly, 0, 1)` (aligned to locked GDD §4, CHANGE-047; the anomaly is 0 on a world that never had a storm, so the result is then the pre-anomaly formula). Local. **Forcing hook A:** pressure holds (a hold overrides the anomaly's effect on its cell) |
| 3 Wind | §5 | Prevailing direction by latitude band (with CH-02 transition lerp, evaluated at the cell's effective latitude `clamp(latitude − windShift[c], 0, 1)`: the GDD's "shift added to every band edge", §19; shift 0 with no cycles) + pressure gradient over the 6 neighbours (precomputed unit vectors; `kNoCell` neighbours contribute nothing but still count in the ÷6). Writes continuous `windDirection`, clamped `windSpeed`, and the snapped `dirIndex`. **Forcing hook B:** wind overrides replace the result for their cells, and `dirIndex` is re-snapped from the override |
| 4 Evaporation | §6 | Primary evaporation (§6.1, self-limiting at saturation) scaled by A5's `openWater` fraction and by the cycle multiplier `evapMultiplier[c]` (the GDD's `(1 + cycle_offsets.evaporation)`; 1 with no cycles), and secondary evaporation (§6.2) from A5's `surfaceWetness` (was ground water). With no A5 present, §6.1 uses S-1's static `waterSource` and §6.2 is zero (amended 2026-11-17, A5 Hydrology). Local. **Influence point:** `dMoisture` is added right after this pass, clamped [0,1]. **Forcing hook C:** moisture holds (after the influence delta, so a hold wins) |
| 5a/5b Moisture transport | §7, §4.1 | 5a: `moistureOut[c] = moisture·windSpeed·moisture_transport_rate`, also when the downwind neighbour is `kNoCell` (EC-16, changed 2026-11-24 DECISION-141: the outflow leaves the map, the cell loses it and nothing is delivered; amended 2026-11-24, EC-16 changed); **and, with the same fraction, `anomalyOut[c] = pressureAnomaly·windSpeed·moisture_transport_rate`** (signed; the GDD's "same gather as §7 moisture"). 5b: gather (below), for moisture and for `pressureAnomaly` |
| 5c Anomaly decay | §4.1 | `pressureAnomaly = clamp(pressureAnomaly × pressure_anomaly_decay, −pressure_anomaly_max, +pressure_anomaly_max)`. Local. New 2026-11-17 (row 6d) |
| 6a/6b Heat transport | §8 | Same shape on `temperature` with `heat_transport_rate`. 6b: `temperature = max(temperature − out, 0) + inflow` (CH-07), so the clamp is order-free |
| 7 Orographic | §9 | Upwind neighbour = `neighbours[c·6 + (dirIndex+3) % 6]`. If it is `kNoCell`: **no lift** (nothing to compare against). If `elevation[up] < elevation[c]`: cool the cell, and if `temperature < condensation_temp` condense `moisture·gain·orographic_precip_rate` (capped at `moisture`). The condensate is **routed as in pass 8**, and `cloudCover = min(moisture, 1)` is taken after the subtraction |
| 8 Precipitation | §10 | As GDD, with `max(…, 0)` (CH-09). Snow below `snow_threshold` into `snowPack` (capped 1.0, CH-04), else into `waterToGround` (amended 2026-11-17, A5 Hydrology). Snow above the cap goes to `glacierInput` on `glacierEligible` cells and is discarded elsewhere. `cloudCover = moisture·0.8` after the subtraction. **Forcing hook D:** one-week rain adds to `precipitation` and routes the same way; it does not consume moisture |
| 9 Snow melt and firn | §11.2, §11.3 | As GDD; melt adds to `waterToGround` (amended 2026-11-17, A5 Hydrology) and cools the cell by `melt·melt_cooling` (clamped ≥ 0). **Firn hand-off (amended 2026-11-17, aligned to locked GDD §11.3):** when `weekOfYear == hydro.glacierCheckWeek` (A5's `GLACIER_CHECK_WEEK`, 26) and the cell is `glacierEligible`, `firn = max(snowPack − hydro.glacierSnowFloor, 0)` (A5's `GLACIER_SNOW_FLOOR`, 0.2) is subtracted from `snowPack` and added to `glacierInput`. It runs after the melt of the same week, never fires with no A5 (`hydro.present == false`), and is the other half of `glacierInput` beside the above-cap snow of pass 8. A4 still owns and writes `snowPack`; A5 never does |
| ~~10 Ground water~~ | §12 | **Deleted (amended 2026-11-17, A5 Hydrology).** The drain and its `ground_water_drain_rate` parameter move to A5. The pass number is retired, not reused |
| Finish | §14, EC-20 | Clamp every field to its range (snow caps 1.0; `waterToGround` clamps to [0, 2] (rain up to 1 plus melt up to 1); `glacierInput` to [0, 2] (above-cap snow up to 1 plus firn up to 0.8; was [0, 1] before the firn; amended 2026-11-17, A5 Hydrology and row 6d); `pressureAnomaly` to ±0.5). Then the **serial totals pass** |

Evaporation (§6.2) belongs to pass 4. Pass 10 is retired. Pass 0 and pass 5c are new (2026-11-17, row 6d). The profile entries are: 0, 1, 2, 3, 4, 5a, 5b, 5c, 6a, 6b, 7, 8, 9, finish = **14**.

### Wind snap and the prevailing-direction tie

Wind angles snap to one of six hex directions: `d = round((angle − 30°) / 60°) mod 6`. Both prevailing directions land **exactly on a tie** (trade winds 0° between NE 30° and SE 330°; westerlies π between NW 150° and SW 210°), and float noise in `atan2` would otherwise choose a side arbitrarily. Rule (user decision 2026-11-16, column-parity zigzag):

- Any angle within **1e-4 rad** of a tie is a tie.
- A tie is broken by the cell's column parity so that air zigzags and makes no net north or south progress.

| Tie | Even column | Odd column |
|---|---|---|
| East (0°) | SE, `d = 5` | NE, `d = 0` |
| West (π) | SW, `d = 3` | NW, `d = 2` |

Any other tie (rare: a pressure perturbation landing exactly on 90° or 270° multiples) uses the lower index. Non-tie angles snap normally (CH-06). The snap is a pure function `SnapWindDirection(angle, column)` in A4's own header; `HexTopology::DirectionFromAngle` is not used for ties and Dia is not edited (AD-001).

### Transport gather

For each cell `c`, in pass b:

```
inflow(c) = Σ over d = 0..5, in ascending order, of  moistureOut[n]   where n = neighbours[c·6 + d],
                                                      n != kNoCell and dirIndex[n] == (d + 3) % 6
          + edgeInflow(c)                              (added last)

edgeInflow(c) = moistureOut[c]  if boundary.edge_inflow
                                and neighbours[c·6 + (dirIndex[c] + 3) % 6] == kNoCell
              = 0               otherwise
```

- `moistureOut[c]` is the normal fraction even when the downwind neighbour is off-map: that outflow leaves the map and is delivered to no one (amended 2026-11-24, EC-16 changed). A cell on both an upwind and a downwind edge loses its outflow and regains it through the edge term when `edge_inflow` is on; with it off the cell loses it.
- A cell with an upwind edge loses its outflow downwind and regains the same amount, so the net from its own outflow is zero (zero-gradient boundary, system spec Resolved Decision 1).
- Heat transport is identical on `heatOut`.
- The summation order is fixed, so the float result depends on neither range count nor range order.

### Forcings and overrides

`WeatherForcing` (header here, issued by Feature 5) is a small POD record: kind, cell, value(s), `weeksRemaining`.

- Kinds: `PressureHold`, `MoistureHold`, `Rain` (one week), `WindOverride`.
- Forcings are **latched at `Capture`** and applied at fixed points inside `Run`: hooks A–D in the table above (EC-15). A hold writes its target value after the pass that owns the field, so the next pass sees it. This is why a held low pressure survives the recomputation in pass 2.
- `Commit` decrements `weeksRemaining` and drops expired records. Forcings are not saved.
- Capacity: 64 active forcings, enforced with an error return from `AddForcing` rather than an unbounded vector.
- **Region forcings (amended 2026-11-16, A4 Feature 6).** A forcing may name a **cell set** instead of a single cell. `RegisterCellSet` takes a sorted, de-duplicated list of cell indices (out-of-range or unsorted input is rejected), stores it in `WeatherSimulation`, and returns a handle. Up to 16 sets exist at once, each at most N cells. One record then covers a region, so the 64-record cap bounds records, not cells. Sets are latched at `Capture` with the forcings and are read-only in `Run`. Where a cell is covered by several records of the same kind, the **lowest record index** wins (deterministic, order-free of ranges). Cell sets are not saved and a set that references a cell beyond the grid after re-`Initialise` is dropped with its forcings.
- Wind overrides persist until `clear_wind_overrides` (Feature 5), so they carry no timer.

### New constant

`cloud_decay` (default **0.5**, range [0,1]) joins the §14 `solar` group. This is the **21st** constant: the GDD defines 20, and the system spec's "all 20 constants" becomes 21. The default is a starting point for tuning, not a design value; Feature 6's scenarios decide whether it holds.

### Grid totals

`WeatherGridTotals { double meanMoisture; double totalSnowPack; uint32_t rainingCellCount; }` is computed in a **serial** pass in cell order at the end of `Run`, on the calling thread (user decision 2026-11-16). It is about 7k adds at Large. The result is bit-identical at any range count and is committed with the fields. It never feeds back into the simulation. This replaces the system spec's "reduced per range and combined in range order" sentence, which would make the sums depend on the range count.

### Profile

`WeatherStepProfile` holds, per pass: wall time on the calling thread, and CPU time summed across ranges (a relaxed `fetch_add` on a `uint64_t` of per-range elapsed microseconds). Timing is never read by the simulation, so the atomics don't affect determinism. The profile covers the last step only.

### Pass granularity

`ParallelFor(N, minRange, fn)` with `minRange` = 256 cells (tunable constant, starting point). Ranges are contiguous index spans; each range writes only cells inside its span. Fusing adjacent local passes is a **fallback** if the CPU gate is missed, not a starting design (user decision 2026-11-16).

## Public API (additions to `CoW/Shared/Weather/`)

```cpp
namespace CoW::Weather {

    enum class ForcingKind : uint8_t { PressureHold, MoistureHold, Rain, WindOverride };
    // Amended 2026-11-16 (Feature 6): a forcing targets one cell OR a registered cell set.
    using CellSetHandle = uint16_t;  constexpr CellSetHandle kNoCellSet = 0xFFFF;
    struct WeatherForcing { ForcingKind kind; uint32_t cell; CellSetHandle cellSet = kNoCellSet;
                            float value; float value2; uint16_t weeksRemaining; };

    struct WeatherGridTotals { double meanMoisture = 0; double totalSnowPack = 0; uint32_t rainingCellCount = 0; };

    struct WeatherStepProfile {
        static constexpr uint32_t kPassCount = 14;     // 0,1,2,3,4,5a,5b,5c,6a,6b,7,8,9,finish (10 retired 2026-11-17, A5 Hydrology; 0 and 5c added 2026-11-17, row 6d; was 12)
        uint64_t wallMicros[kPassCount];
        uint64_t cpuMicros[kPassCount];
        uint64_t totalCpuMicros;
        uint32_t lastRangeCount;
    };

    // Pure helper, unit-tested directly.
    uint8_t SnapWindDirection(float angleRadians, uint32_t column);

    class WeatherSimulation {                           // additions to Feature 1's class
    public:
        void Capture(Season season, uint32_t weekOfYear, const IBiomeSeasonalBaseline& baseline,
                     const HydrologyInputs& hydro,                              // SimPU; hydro.present == false when no A5 (amended 2026-11-17, A5 Hydrology)
                     const InfluenceSnapshot& influences);                      // Feature 7; an empty snapshot skips pass 0 (amended 2026-11-17, row 6d)
        void LatchClimate(uint64_t evalDay);                                    // Feature 8; call after Capture. No cycles = nothing latched
        void Run(Dia::DeferredStep::IParallelExecutor& executor);               // any thread; working set only
        void Commit();                                                          // SimPU
        void Step(Season season, uint32_t weekOfYear, const IBiomeSeasonalBaseline& baseline,
                  const HydrologyInputs& hydro, const InfluenceSnapshot& influences,
                  Dia::DeferredStep::IParallelExecutor& executor);              // Capture + Run + Commit (Feature 8's runner also calls LatchClimate)
        const WeatherHandoff& GetHandoff() const;                               // glacierInput and influenceWater of the last step (A5 reads the working copy; amended 2026-11-17, A5 Hydrology and row 6d)
        const WeatherFields& GetWorkingFields() const;                          // A5's IWaterInputSource view: valid from Capture to Commit, buffers allocated once at promotion (Feature 3)
        bool AddForcing(const WeatherForcing& forcing);                         // false when full
        CellSetHandle RegisterCellSet(const DynamicArrayC<uint32_t>& sortedCells);   // false-equivalent: kNoCellSet when 16 sets exist
        void ClearCellSets();                                                   // also drops forcings that reference them
        const WeatherGridTotals& GetGridTotals() const;
        // Exported pure helpers for Feature 5 (2026-11-16, no behaviour change): PrevailingDirection(latitude),
        // PressureGradient(cell, fields, tables), SnapWindDirection (above).
        void SetCommittedWeek(uint64_t week);                                   // Feature 3: the module overwrites the counter with WeekOf(stepDay) after Commit
        const WeatherStepProfile& GetLastProfile() const;
    };
}
```

`Step` replaces the system spec's two-argument form.

## Acceptance Criteria

### Lifecycle and purity

1. `Capture` copies committed → working and latches the season, parameters, forcings, biome edits and the 13-entry baseline table. The baseline provider is called only inside `Capture`: a provider that asserts when called off the capturing thread passes.
2. `Run` after `Capture` does not read anything the caller can mutate. A test poisons (overwrites) the caller's parameters, forcing list, biome edit queue and baseline provider after `Capture`; the result equals a run with no poisoning.
3. Committed state does not change between `Capture` and `Commit`, however many times `Run` is invoked on a different working set.
4. `Commit` swaps working → committed, increments the week, decrements forcing timers and drops expired forcings.
5. `Step` equals `Capture; Run; Commit` bit-for-bit.
6. `Run` performs no heap allocation. Scratch is allocated at `Initialise`.

### Per-step behaviour (one test group per pass, using the GDD worked expectations)

7. **Solar:** an equator land cell converges toward its seasonal baseline at `land_heat_capacity`; a water cell converges more slowly. Cloud albedo reduces the delta. Latitude 1.0 cools by `latitude_cooling_factor`.
8. **Weekly reset:** `precipitation` is 0 at the end of pass 1 whatever it was before. `cloudCover` is multiplied by `(1 − cloud_decay)` before pass 1 uses it; with `cloud_decay = 0` it is unchanged.
9. **Pressure:** temperature 1.0 → pressure 0.2; 0.0 → 0.8; 0.5 → 0.5 (at default sensitivity). The result is clamped to [0,1] (EC-07).
9a. (amended 2026-11-17, row 6d) **Pressure anomaly:** with `pressureAnomaly` −0.3 on a cell at temperature 0.5, pass 2 gives pressure 0.2; the sum is clamped to [0, 1]; with every anomaly 0 the output equals the pre-anomaly formula bit for bit.
9b. **Pass 0:** with an empty snapshot pass 0 is skipped and fields, totals and profile are bit-identical to a run without Feature 7 (Feature 7 AC 24). With influences, `dPressure` lands in `pressureAnomaly` before pass 2, `dTemperature` after pass 1 and `dMoisture` after pass 4 (Feature 7 ACs 12, 18).
10. **Wind:** uniform pressure gives the prevailing wind only. A high-pressure cell pushes neighbours' wind away from it, and a low-pressure cell pulls it in. The prevailing direction matches the latitude band, with a continuous lerp across the two transition zones (CH-02). Speed is clamped to [0,1].
10a. (amended 2026-11-17, row 6d) **Wind latitude shift:** with a per-cell `windShift` of +0.1 the band boundaries move poleward by 0.1, including the transition lerp; with shift 0 (no cycles) directions equal the unshifted function exactly (Feature 8 AC 24).
11. **Evaporation:** water cells add `temperature·ocean_evap_rate·evapMultiplier·(1 − moisture)` (the multiplier is 1 with no cycles; Feature 8 AC 23), so a saturated cell gains nothing (EC-04). Secondary evaporation draws on `surfaceWetness`, never moves more than the cell holds and is not negative afterward. Primary evaporation scales with `openWater`; with `hydro.present == false` it uses `waterSource` and secondary evaporation is 0 (amended 2026-11-17, A5 Hydrology).
12. **Moisture transport:** moisture moves one cell downwind per step at the stated rate. Total moisture is conserved by the transport passes (excluding edge-inflow and forcing cells; test with `edge_inflow` off on a closed interior). A cell whose downwind neighbour is off-map loses its outflow share, which leaves the map (EC-16, changed 2026-11-24; with `edge_inflow` on the edge term replaces it, so a uniform field stays uniform at every cell). (amended 2026-11-24, EC-16 changed).
12a. (amended 2026-11-17, row 6d) **Anomaly transport:** on uniform terrain under steady wind, a `pressureAnomaly` pulse moves one cell downwind per step at `moisture_transport_rate` (the same gather, direction order and edge rule as moisture, signed values); an off-map downwind neighbour loses its anomaly share, which leaves the map; with `edge_inflow` on a uniform anomaly stays uniform at every cell (before decay). (amended 2026-11-24, EC-16 changed).
12b. **Anomaly decay (pass 5c):** each week the anomaly is multiplied by exactly `pressure_anomaly_decay` and clamped to ±`pressure_anomaly_max`; a 0.7 decay halves a seeded −0.3 low in about two weeks with no transport; a world whose anomaly is 0 everywhere stays 0.
13. **Heat transport:** same, with `heat_transport_rate`. Temperature never goes negative (CH-07), and a 1.0 cell receiving inflow ends at 1.0 after the finish clamp.
14. **Orographic:** with wind blowing upslope, the windward cell cools, condenses and rains (EC-09: a ring of mountains rains on the windward faces and leaves the interior dry). The leeward cell receives less rain than the windward cell. Downslope wind never triggers lift. A cell whose upwind neighbour is off-map never triggers lift.
15. **Orographic routing:** condensate below `snow_threshold` goes to `snowPack` and above goes to `waterToGround`, with the same cap and glacier rules as pass 8 (amended 2026-11-17, A5 Hydrology). A cell never condenses more than its `moisture` (user decision 2026-11-16).
16. **Precipitation:** rain and snow split at `snow_threshold`. Zero moisture produces no precipitation and no negative amount (EC-11, CH-09). Snow pack caps at 1.0. The excess becomes `glacierInput` on `glacierEligible` cells and is discarded elsewhere (amended 2026-11-17, A5 Hydrology; EC-12 is deleted with the ground-water store). Rain lands in `waterToGround`, never above its input.
17. **Melt:** no melt below `melt_threshold`. Melt with zero snow pack is a no-op (EC-13). Melt moves snow to `waterToGround` and cools the cell (amended 2026-11-17, A5 Hydrology). A cell that stays below `melt_threshold` all year keeps its snow capped at 1.0 (EC-06).
18a. (amended 2026-11-17, aligned to locked GDD §11.3) **Firn:** at `weekOfYear == glacierCheckWeek` (26) on a `glacierEligible` cell, `snowPack` above `glacierSnowFloor` (0.2) moves into `glacierInput` and `snowPack` is lowered by the same amount (a cell at 0.9 gives 0.7 firn and ends at 0.2; a cell at 0.15 gives none); in any other week, on a non-eligible cell, and with `hydro.present == false`, nothing moves. The constants come from `HydrologyInputs`, not from A4's parameters. The firn amount is added to the week's above-cap `glacierInput`, and the sum stays within [0, 2]. The week of year is the one latched at `Capture`.
18. **Hand-off (replaces the ground-water drain test, which moves to A5; amended 2026-11-17, A5 Hydrology):** `waterToGround` equals rain plus snowmelt each week and is 0 on a dry, melt-free cell. `glacierInput` equals the snow above the cap on `glacierEligible` cells and is 0 on every other cell. With `hydro.present == false`, `glacierInput` is 0 everywhere, primary evaporation matches the `waterSource` behaviour and secondary evaporation is 0. A cell's `glacierEligible` flag is the previous committed A5 week's, latched at `Capture` and unchanged by a mid-run poisoning.
19. **Finish:** every field is clamped to its range after the last pass (EC-20). Temperature extremes 0.0 and 1.0 for 52 consecutive weeks stay in range (EC-03).
20. **Zero wind:** at wind speed 0, nothing is transported and orographic lift does not fire on stale directions (EC-05).

### Wind snap

21. `SnapWindDirection` maps non-tie angles to the nearest of the six directions (angle = 30° + 60°·d).
22. East ties (0° ± 1e-4 rad) return SE (5) on even columns and NE (0) on odd columns. West ties (π ± 1e-4) return SW (3) on even and NW (2) on odd.
23. With uniform pressure, uniform terrain and trade winds, a moisture pulse moves due east at the end of 12 weeks with no net row drift (±1 row from the zigzag), and the same for westerlies going west.
24. Angles one `1e-3` rad either side of a tie snap to the nearer direction, not the parity direction.

### Transport edges

25. With `edge_inflow` on, a uniform moisture and temperature field under steady wind stays uniform across **all** cells, edges included, over 52 weeks (zero-gradient boundary).
26. With `edge_inflow` off, upwind-edge cells lose moisture each week (GDD-literal behaviour), proven numerically.
27. A cell with both its upwind and downwind neighbours off-map gains and loses nothing from its own outflow.
28. Inflow is summed in direction order 0..5, then the edge term: a test with mixed-magnitude inflows (different orders give different float sums) shows the result matches a reference loop in that order.

### Determinism

29. The same inputs give bit-identical fields after 52 weeks, run twice.
30. `SerialExecutor` and a test executor with 1, 3 and 17 ranges, and one range per cell, give bit-identical fields, grid totals and week counter after 52 weeks, at Small, and at Large with a hand-built terrain.
31. A test executor that runs the same ranges in **reverse order** gives the same bytes. This is what proves "no pass reads a neighbour field it writes".
32. Grid totals are bit-identical at any range count (serial pass), and equal a reference reduction in cell order.
33. **Deferred vs synchronous:** a test-local adaptor that drives `Capture` / `Run` / `Commit` through DiaDeferredStep's manual launcher, with `Capture` at key N and `Apply` at N+k, matches `Step`-per-week bit-for-bit over 52 weeks. The real `WeatherStep` arrives in Feature 3 and re-runs this test through the module.
### Forcings

34. `PressureHold` survives the pressure recomputation in pass 2 and shapes the wind in pass 3. `MoistureHold` is visible to the transport passes. `WindOverride` replaces pass 3's output for its cells and changes `dirIndex`.
35. `Rain` adds exactly `value` to `precipitation` for one week, routes to snow or `waterToGround` by temperature, and consumes no moisture (amended 2026-11-17, A5 Hydrology).
36. Forcings expire after `weeksRemaining`. `AddForcing` returns false at 64 active forcings and nothing is dropped silently. The same forcings applied in any order give identical results.
36a. A region forcing (`PressureHold` or `WindOverride` over a registered cell set) applies to every cell in the set and to no other. A set with 0 cells is rejected. A 17th set returns `kNoCellSet`. Two records of the same kind overlapping on a cell resolve to the lower record index. Region and per-cell forms give identical output for the same cells.

### Performance

37. One `Step()` at Large in Release is reported with its per-pass wall and CPU breakdown. Budgets: **≤ 10 ms CPU** at Large, ≤ 6.3 ms Standard, ≤ 4.2 ms Small. A miss is reported and noted in the plan but does not fail the build. A **hard failure above 3× budget** catches accidental O(N²) behaviour or per-cell allocation (user decision 2026-11-16).

### Layer goldens

38. All 10 fields (nine plus `pressureAnomaly`; amended 2026-11-17, row 6d) for the hand-built terrain after a fixed week count are checked against `Tests/GoogleTests/CoW/Golden/weather_layers.golden.json` through `CellLayerGolden`. The five-preset goldens belong to Feature 6; this feature registers the golden file and the first entry. See Open Design Question 3.

## Files Touched

New (names tentative until Feature 1 fixes the folder layout):

- `Cluiche/CoW/Shared/Weather/WeatherPipeline.h/.cpp`: the passes
- `Cluiche/CoW/Shared/Weather/WindDirection.h/.cpp`: `SnapWindDirection`, direction tables, prevailing-direction function
- `Cluiche/CoW/Shared/Weather/WeatherForcing.h`: forcing record
- `Cluiche/CoW/Shared/Weather/WeatherStepProfile.h`
- `Tests/GoogleTests/CoW/Weather/` — one test file per pass group, `WindDirectionTests`, `WeatherDeterminismTests`, `WeatherEdgeInflowTests`, `WeatherForcingTests`, `WeatherPerfTests`
- `Tests/GoogleTests/CoW/Golden/weather_layers.golden.json`
- A test-local `ReverseRangeExecutor` and `FixedRangeExecutor(n)`. If DiaDeferredStep Feature 1 ships equivalents under `DiaDeferredStep/Testing/`, use those instead (tests ship with libraries).

Modified:

- `Cluiche/CoW/Shared/Weather/WeatherSimulation.h/.cpp` (Feature 1): `Capture` / `Run` / `Commit` / `Step` / `AddForcing`
- `WeatherParameters` (Feature 1): adds `cloud_decay`
- `a4-weather.md`, to be updated when this spec is approved:
  - Public Interfaces (`Step` signature, `Capture`/`Run`/`Commit`)
  - the "stable copy" sentence in Update pipeline
  - the "reduced per range" totals sentence
  - "20 constants" → 21
  - a new GDD deviation list (orographic routing, weekly precipitation reset and cloud decay, ties, serial totals)

No Dia files are added or edited (AD-001). `IParallelExecutor` and `SerialExecutor` come from DiaDeferredStep Feature 1.

## Binding Decisions

| ID | Decision | How this feature complies |
|---|---|---|
| DFS-002 | `Run` reads only the captured working copy | `Capture` latches everything including the baseline table, so the provider is never called on a worker. AC 2 poisons the sources. |
| PD-004 | No STL containers in public APIs | Fields, scratch and neighbour table are `DynamicArrayC`. `WeatherForcing` is POD. Nothing `std::` crosses the public interface. |
| PD-005 | x64 only | Bit-identity is guaranteed per binary on x64. See Open Design Question 3 for cross-configuration limits. |
| AD-001 | Ask permission before Dia edits | None. The `ParallelFor` and `SerialExecutor` signatures come from the already-approved DiaDeferredStep Feature 1. |
| AD-003 | GDD owns design intent | Five deviations from the GDD pipeline are listed below for the GDD owner; none changes a constant's meaning except the new `cloud_decay`. |

## GDD Deviations Introduced (for the system spec's list, #15–#19)

15. **Orographic precipitation is routed like §10** (snow or `waterToGround` by temperature). The GDD adds it to `precipitation` only.
16. **`precipitation` is reset each week** and **`cloud_cover` decays** by a new `cloud_decay` constant. The GDD never clears either.
17. **Wind snap ties break by column parity.** Both prevailing directions fall exactly on a hex tie.
18. **Grid totals are a serial reduction**, so they are range-count independent.
19. **Stable reads by construction**, via scratch arrays, instead of copying the neighbour-read fields.

## Test Plan

- Pure logic, GoogleTests over hand-built hex terrains (no S-1 code). TDD per `.claude/skills/tdd.md`: RED for each pass group before the pass is written.
- Determinism tests run at 52 weeks, fields compared with `memcmp`.
- Performance and golden entries run as part of the final system task; scoped `--filter` while iterating, one full `--shards 2` run at the close-out, no mutation checks.
- The Observation Opportunity Scan at Verify covers `DIA_PROFILE_SCOPE` per pass (in scope here) and notes `cow.weather.step_ms` metric registration (Feature 5).

## Open Design Questions

1. **Is `cloud_decay` = 0.5 a sensible default, and does the GDD's precipitation-gating still hold?** With decay, a cell that rained last week has half its clouds this week, which raises its solar heating relative to the GDD's persistent-cloud behaviour. Higher heating means less precipitation in wet regions. Feature 6's coastal-rain and rain-shadow ratios are the test; if they fail, `cloud_decay` is the first knob to turn before touching GDD constants.
2. ~~**Are forcing hook points A–D in the right places?**~~ **Resolved 2026-11-17 (GDD §4.1 and §18, locked): a storm is an impulse that seeds the carried `pressureAnomaly` (Feature 7), so no one-shot "set value" forcing kind is needed.** Pressure after pass 2, wind after pass 3, moisture after pass 4, rain after pass 8 stay the hook points for the debug holds.
3. **Bit-identity is per binary. Do the layer goldens hold across Debug and Release?** Range count and order do not change the instruction sequence per cell, so AC 30/31 hold in every configuration. But `sinf`/`cosf`/`atan2f` and any compiler contraction or vectorisation can differ between Debug and Release, so a golden hash over exact float bits may fail in the other configuration. Task 1 of the plan should check the CoW and DiaCore projects' floating-point settings (bgfx builds `Fast`; the CoW and DiaCore projects were not checked here) and whether `CellLayerGolden::HashLayer` hashes exact bits or quantised values. If exact, the golden is Debug-only or per-config.

## Gap-pass amendments (2026-11-16)

Found by the plan-writing pass; each item below **supersedes** any contradicting text above. Numbering is stable.

1. **Wind-override timers.** A `WindOverride` carries `weeksRemaining`: **0 means until cleared**, any positive value expires after that many weeks (`Commit` decrements it). The earlier "wind overrides carry no timer" line is superseded. `clear` (Feature 5) still removes every wind override at once. Acceptance criterion 36 (forcings expire) therefore covers wind overrides when `weeksRemaining > 0`.
2. **`WindOverride` field mapping.** `value` is the wind direction in **radians** in [0, 2π), on-screen counter-clockwise, y-down world space (the same convention as `windDirection`); `value2` is the speed in [0, 1]. Callers that take degrees convert before building the record.
3. **`ClearForcings()`** is added to `WeatherSimulation` by **Feature 6** (not delivered by this feature): it removes every forcing and keeps registered cell sets. `ClearCellSets()` removes the sets **and** every forcing that references one.
4. **`cloud_decay`** is added to the parameter table by **Feature 1** (its criteria 3 and 4). This feature only reads it. The "adds one constant" wording and the "21st constant" sentence are superseded; the count of 21 stands.
5. **Criterion 6 (no heap allocation in `Run`).** Mechanism: every scratch array, grid table and the cell-set storage is sized at `Initialise`; the test records each array's data pointer and capacity before and after `Run` and asserts they are unchanged, and a code rule forbids `new`, growing containers and `std::` allocation in the `Run` call tree. No global `operator new` hook is added (an engine concern).
6. **Goldens.** `Tests/GoogleTests/CoW/Golden/weather_layers.golden.json` is **owned by Feature 6** (hand-built `MakeCoastRange(66, 42)`). This feature's golden entries use the key prefix `pipeline/`, and whichever feature lands first creates the file. Hashes are compared bit for bit, so Debug and Release must agree: the build must use `/fp:precise` with no fast-math in both configurations. Plan Task 0 verifies this and, if the two configurations still differ, stores one hash set per configuration (closes Open Question 3).

7. (amended 2026-11-17, A5 Hydrology) **The §12 ground-water drain is not in A4.** `groundWater` becomes the per-week `waterToGround` output and the store moves to A5. Snow above the cap becomes `glacierInput` on A5-flagged cells. §6.1/§6.2 read A5's latched values.
8. (amended 2026-11-17, aligned to locked GDD, CHANGE-047; row 6d) **Pass list and the anomaly.** Pass 0 (Influence) and pass 5c (anomaly decay) are new; `pressureAnomaly` is a saved field; passes 5a/5b carry it with the moisture gather; the pressure pass adds it. `kPassCount` is 14. The Feature 5 `info profile` count and the Open Design Question 2 "one-shot set value" (superseded by the carried storm anomaly) follow this. `Capture` takes `weekOfYear` and the `InfluenceSnapshot`; Feature 8's `LatchClimate` follows `Capture`. The firn hand-off (GDD §11.3) is in pass 9.

## World-sim review amendment (2026-11-18)

User decisions from the world-sim review. Each item **supersedes** any contradicting text in this spec. Sources: [summary](../../../../../research/worldsim_data_driven/summary.md) §0, §0.1; [parameter-files](../../../dia/systems/diaattribute/parameter-files.md); [biome-registry](../s1-worldgeneration/biome-registry.md); [worldsim-kit](../worldstep/worldsim-kit.md); [grid-search-helpers](../../../dia/systems/diapathfinding/grid-search-helpers.md); [blackboard-scopes](../../../dia/systems/diablackboard/blackboard-scopes.md).

1. **Parameters (system item 1).** `Capture` takes the `WeatherParameters` struct produced by `ParamBinding::Snapshot`, once per step; cycle modifiers are already resolved in it (item 15). No pass reads `ParameterFile` directly.
2. **Biomes (item 13).** Passes that read `biome` use `BiomeIndex` and registry tags/roles, never enum values.
3. **Wind (item 16).** Pass 3 (wind) reads the westerly band latitudes from S-1's parameters, not a local constant. The wind-latitude cycle shift is applied relative to them.
4. **Orographic pass (item 17).** Pass 4 may scale lift by `terrain.orographic_factor` for the cell's `TerrainType`. Provisional; defaults to 1.0 where unset, so goldens do not move until it is tuned.
5. **Profile and metrics.** `cow.weather.step_ms` is registered by the stage base (item 5); the pass profile entries are unchanged.

## Status

**Status:** `Done` (2026-11-24; built in DraoiHexWorldWeather; AC 9b waits for Feature 7 pass 0, AC 33 moved to Feature 3). Was Approved (2026-11-16). Amended 2026-11-17 (user decisions: A5 hand-off, Features 7–9). Amended 2026-11-17 (aligned to locked GDD, CHANGE-047; row 6d).

**Plan:** [ca-pipeline.plan.md](ca-pipeline.plan.md) (re-planned and built 2026-11-24, Done).

**Amended 2026-11-18 (world-sim review).** Status stays `Approved`. Amended 2026-11-18 (world-sim review decisions; user-approved designs in the specs referenced below).


## Draoi layer amendment (2026-11-21)

Applies the World-Sim Kit's "Draoi layer amendment" (@docs/specs/applications/cow/systems/worldstep/worldsim-kit.md) and the system spec's matching section. Supersedes contradicting text above, including the `CoW/Shared/Weather/` paths in Public API and Files Touched. **This whole feature moves to package `DraoiHexWorldWeather`; the design is unchanged and the status stays `Approved`.**

- **Pure sim in Draoi:** `WeatherPipeline`, `WindDirection`, `WeatherForcing`, `WeatherStepProfile`, and `WeatherSimulation::Step` / `Capture` / `Run` / `Commit` / `AddForcing`. All passes, EC-03 to EC-20, the gather transport and the layer goldens are unchanged. No SimModule, bus, save or debug-domain wiring; no SimPU or step-chain knowledge.
- **Contract reworded (thread-neutral):** "`Run` touches only the working copy; the caller must not overlap `Run` with `Capture` or `Commit`." Wherever Lifecycle and the purity ACs name the SimPU, a worker or a step chain, read "the caller". State is not passed or returned by value.
- **A5 inputs** latched at `Capture` become the A4-owned `HydrologyInputs { surfaceWetness, openWater, glacierEligible }` (per-cell arrays, defaults from `DraoiHexWorld`: S-1 static water sources, zero ground evaporation, no glacier; filled by Hydrology's `FillHydrologyInputs` at `Capture`); no per-cell callbacks inside `Run`.
- **Executor:** takes a parallel-for (`IParallelExecutor` / `SerialExecutor` from DiaThreading), not DiaDeferredStep. The test-local `ReverseRangeExecutor` / `FixedRangeExecutor` come from `DiaThreading/Testing/` if shipped, else live in `DraoiHexWorldWeather/Testing/`.
- **Files Touched:** sources under `DraoiHexWorldWeather/`; tests keep running in the GoogleTests project (paths tentative until the package folder exists); the golden file location is decided at re-plan.
- **Stays in CoW:** the preset loader and `cow.weather.*` command wiring that call `AddForcing` (Features 5, 6).
- **Open question:** none new.