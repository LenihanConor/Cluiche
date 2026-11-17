# System Spec: A5 Hydrology

**Parent:** @docs/specs/applications/cow/cow.md
**Design source:** GDD `08_SystemsDeepDive/A5_Hydrology/`. **Not written yet.** The brief and the interview answers are in `Cluiche/out/CoW/gdd-foldback-hydrology-weather-2026-11-17.md` (Part 2), sent to the GDD session. This spec cannot be Approved until the GDD session has locked A5 or explicitly accepted these decisions (AD-003).
**Status:** `Draft`

**Gameplay Domains:** simulation, debug

## Summary

A5 is the single owner of **water on the ground** in CoW's world simulation. Each game week it takes the water A4 Weather drops on the land (rain and snowmelt) and works out what that water does: how much soaks into the soil, how much reaches ground water, how much runs downhill along S-1's flow directions into rivers, how far floods travel down a river over the following weeks, how full each lake basin is, where floodplains flood, and what freezes. In a long cold climate phase, snow that never melts becomes glacier ice.

River **courses** never move: they are S-1's, which keeps GDD DECISION-064/065 ("rivers don't dry up, they shrink"). **Flow** is dynamic. A river can run very low, and a low river is a state ("dry riverbed"), not a new course. **Lakes grow and shrink inside their basins**: S-1's drainage stage already finds every basin and its spill height, and A5 fills or drains each basin by water balance. A5 marks the cells under water; A3 decides when a long-flooded cell *becomes* Lake (or an exposed lakebed becomes land).

A5 runs as a World Step stage between A4 and A1, so one deferred step runs **A4 → A5 → A1 (+A3 inline)**.

## Problem

Today water has three owners and none of them is in charge:

| Where | What it does with water |
|---|---|
| S-1 | Rivers, lakes and flow direction are generated once and never change |
| A4 (Feature 2, approved, unbuilt) | A per-cell `groundWater` bucket: rain and melt go in, 20% drains out a week. No water moves sideways, so there are no rivers and no floods downstream |
| A1 (Feature 4, approved, unbuilt) | A second model: step 2 "water flow" and a Decade "water table". It reads rivers as static geography |
| A3 | `DryRiverbed` legacy and Wetland ↔ Grassland transitions assume water changes, but nothing produces those changes |

The GDD promises spring floods that "flow from high elevation to low" (A4 TechnicalSpec §14 invariant 5) and droughts that shrink rivers. Neither the bucket nor the static rivers can deliver that. Climate cycles (A4 Feature 8, a ~4-decade ice-age phase) also need somewhere for lakes to fall and water to freeze.

## Goals

1. **One water model.** A5 owns soil moisture, ground water, runoff, river flow, lake storage, flooding and ice. A4 stops at "water reaching the ground"; A1 and A3 read A5.
2. **Floods travel.** Runoff routes downhill along S-1's flow directions through a chain of river stores, so a melt pulse in the mountains arrives in the lowlands over weeks, not instantly.
3. **Lakes breathe inside their basins.** Each S-1 basin holds a water volume. Its level follows inflow, outflow over the spill point and evaporation. Cells below the level are inundated. Basins that are not lakes at generation can fill in wet periods.
4. **Reachability is never broken.** Floods are temporary. S-1's corridor and crossing cells always stay passable. Ice makes water easier to cross, never harder. A long-run test proves no region is permanently cut off.
5. **Ice and glaciers.** Rivers and lakes freeze in cold weeks. Snow that survives whole years becomes glacier ice, which A3 can turn into cold biomes. When the climate warms, the ice retreats and its melt joins runoff.
6. **Deterministic.** No RNG. The result is bit-identical at any range count and whether the step ran deferred or synchronously.
7. **Observable and tunable.** Cell layers, history, sim-hosted commands with help, presets and regression scenarios, in the same shape as A4. It is tuned in its own testbed, **S-2.6 Rivers & Floods**.

## Non-Goals

- **Precipitation, snow pack and snowmelt.** A4 owns them. A5 receives `waterToGround` and reads `snowPack` and `temperature`.
- **Biome change.** A3 decides when an inundated or glaciated cell changes biome. A5 only exposes how long a cell has been flooded, exposed or iced.
- **River courses and terrain.** S-1 owns them. A5 never moves a river or edits elevation.
- **Weather influences and dragon water effects.** A4's influence API (A4 Feature 7) is the one entry point. A5 applies the ground-water part of a "wetter"/"drier" influence. A1's GDD `water_dragon_emission` moves onto that path (user decision 2026-11-17). There is no dragon content now.
- **Narrative events.** `RiverRunsDry`, `FloodBegan`, `LakeLevelRecord` and `FreezeUp` are debug queries only, until D4 is designed (the same rule as A4).
- **Forecasting.** None (user decision 2026-11-17).
- **Player-facing water visuals.** V-1 reads the snapshot.
- **Shallow-water physics.** No water heights or momentum. Routing is flow accumulation through linear stores.

## Features

| # | Feature | Summary | Spec | Status |
|---|---------|---------|------|--------|
| 1 | Data & Parameters | `HydrologyFields`, `HydrologyTerrain` (incl. basin table), `hydrology_parameters.json` (validated, hot-reload, live override), the `Passability` enum. Pure logic, TDD | TBD | Draft |
| 2 | Water Pipeline | The weekly passes: infiltration and soil, ground water, runoff, river routing (linear-store cascade in topological order), basin water balance and inundation, floodplain flooding, freeze and glacier, clamping and totals. Serial vs parallel bit-identity. Pure logic, TDD with hand-built grids | TBD | Draft |
| 3 | `HydrologyModule` & World Step Stage | Registers as stage `hydrology`; Capture/Run/Commit; reads A4's in-run result; `HydrologyUpdated{week}` + `HydrologySnapshot`; init from S-1, spin-up, `HoldGeneration`/`WorldGenerationFailed` | TBD | Draft |
| 4 | Save/Load | E1 participant; pending working set through DiaDeferredStep (DFS-005); JSON arrays | TBD | Draft |
| 5 | Debug & History | DiaCellLayers base layers, DiaCellHistory groups and events, sim-hosted `cow.hydro.*` commands with help, `cow.hydro.step_ms` | TBD | Draft |
| 6 | Presets & Regression | Scenarios (forced wet year, drought decade, melt pulse, ice-age phase), layer goldens via `CellLayerGolden.h`, the reachability long-run test | TBD | Draft |

**Build order:** 1 → 2 → 6 (pure logic, provable by GoogleTests on hand-built grids) → 3 → 4 → 5. 3 needs `WorldStepModule` and A4 Feature 3. The S-2.6 testbed is its own spec, written once GDD A5 is locked.

## Architecture

### Placement

**CoW-Shared** (AD-002): consumed by A1, A3, A4 (evaporation), V-1, S-2.6, S-3 and S-3.5. Pure logic in `Cluiche/CoW/Shared/Hydrology/`; the SimModule, debug domain and save participant in `Cluiche/CoW/Modules/`.

```
SimPU
  └─ WorldStepModule ("order": ["weather", "hydrology", "ecology"])
  |    one `world` channel: Capture all → Run in order → Commit in order
  |
  └─ HydrologyModule (SimModule)
       on WorldGenerationComplete → HydrologySimulation::Initialise(terrain, basins) → spin-up
       registers as world-step stage "hydrology"; does NOT kick or schedule
       on Commit → publish HydrologyUpdated{week}
       on BiomeTransitionComplete → update cached biome (applies next week)
       owns: HydrologySimulation, HydrologyParameters, save participant, debug domain, cell layers
```

A5 can run without A1 (S-2.6: `["weather", "hydrology"]`). It cannot run without a water source: when A4 is absent, a **fixed-input driver** (a constant or scripted `waterToGround` per week) stands in, so tests and Controlled modes work without weather.

### Coupling with A4 (one week each way, no cycle inside a step)

| Direction | What | When |
|---|---|---|
| A4 → A5 | `waterToGround` (rain + melt), `temperature`, `snowPack`, `glacierInput` | **This week**: A5's `Run` reads A4's finished working copy, because A4 ran first inside the same `Run` |
| A5 → A4 | `surfaceWetness` (A4 §6.2 secondary evaporation), `openWater` fraction (A4 §6.1 primary evaporation, now that lake extent varies), `glacierEligible` flag | **Last week**: A4 `Capture` latches A5's committed state |

This is the one-week lag the World Step model gives for free. It is documented, not hidden: a melt pulse affects evaporation a week after it lands.

**A4 amendment owed (before A4 Features 1 and 2 are built):**
- `groundWater` leaves `WeatherFields` and `waterToGround` is added.
- §10/§11.2 route into `waterToGround`.
- §12 (drain) is deleted.
- §6.1/§6.2 read A5's captured values. With no A5 present, they fall back to S-1's static `waterSource` and zero ground evaporation.
- The `rainfall` output drops its `groundWater·0.1` term.
- **Snow over the 1.0 cap goes to A5 instead of being discarded:** A4 outputs it as `glacierInput` on cells A5 flags as glacier-eligible (decided 2026-11-17). A4 still owns `snowPack`; A5 never writes it.
- **Spin-up is coupled:** A4's spin-up job runs A4 + A5 together (see Initial state).

### Grid and terrain ingest

The grid is the same `Dia::Pathfinding::HexTopology` (flat-top odd-q) that A4 uses, built from S-1's dimensions. A5 copies at `Initialise` and never holds the pointer:

- From `Cell`: `elevation`, `waterSource`, `downstream` (0..5 on river cells), `biome`, `region`, `terrainFlags`.
- From S-1's drainage stage: flow direction for **every** land cell, flow accumulation (an init weight only), the **basin table** (cells, spill height, spill cell, volume), lake membership and **bed elevation** under lakes (`Cell::elevation` is the lake *surface*).
- From S-1 sites: corridor and crossing cells (the reachability rule).

**S-1 consumer amendment owed (CoW-side, no AD-001 sign-off; decided 2026-11-17):** S-1 **persists** raw/bed elevation and the basin table (cells, spill height, spill cell) in `.wld.json`, bumping the world format version. Flow direction is already rebuilt on import. Import reads the basin table back, so imported, `world_file` and F-3 baked worlds give A5 exactly what a freshly generated world does. A drainage-rule change in S-1 never silently changes an old world's basins. Rebuilding on import, and A5 running its own priority-flood, were both rejected. **Version 1 world files** (user decision 2026-11-17) still import, with a lake-only basin table and `basinsFromLakesOnly = true`. On such worlds A5 holds each lake at its surface level and fills no other basin. Rivers, soil, floods and ice work as normal. A re-bake from the seed restores full basins.

### State (structure of arrays)

```cpp
namespace CoW::Hydrology {

    struct HydrologyFields {                    // per cell, N each
        DynamicArrayC<float> surfaceWater;      // [0,1] standing water this week (feeds A4 §6.2)
        DynamicArrayC<float> soilMoisture;      // [0,1] what A1 reads as cell water
        DynamicArrayC<float> groundWater;       // [0,1] slow store; baseflow into rivers
        DynamicArrayC<float> riverStore;        // >= 0 water in transit (river cells, unbounded, normalised by flow scale)
        DynamicArrayC<float> riverFlow;         // >= 0 outflow this week, the river's "size"
        DynamicArrayC<float> ice;               // [0,1] frozen surface water / river / lake ice
        DynamicArrayC<float> glacier;           // [0,1] glacier ice mass
        DynamicArrayC<uint16_t> inundatedWeeks; // consecutive weeks under basin water (A3 reads)
        DynamicArrayC<uint16_t> exposedWeeks;   // consecutive weeks a lake cell has been above water (A3 reads)
        DynamicArrayC<uint16_t> snowYears;      // consecutive years snow pack survived summer (glacier onset)
        DynamicArrayC<uint8_t>  flooded;        // floodplain flood flag this week
    };

    struct BasinState {                          // per S-1 basin, not per cell
        DynamicArrayC<float> volume;            // current water volume
        DynamicArrayC<float> level;             // derived from volume via the basin's hypsometry
    };

    enum class Passability : uint8_t { Dry, Wet, Fordable, Deep, Frozen };
}
```

At Large this is about 60 KB per float field, under 1 MB per working copy.

### Weekly pipeline (Feature 2)

All passes are full-grid unless stated. Each reads a stable copy and gathers in a fixed direction order (the A4 rule), so results are independent of range count.

1. **Inputs:** read `waterToGround`, `temperature`, `snowPack` from A4's working copy (or from the fixed-input driver); apply captured influences (the ground-water part only).
2. **Freeze/thaw:** surface water, river store and basin surface ice up below `freeze_threshold` and thaw above it. Frozen water is held, not routed.
3. **Infiltration:** split `waterToGround` + surface water into soil and surface by soil capacity (biome-weighted). Soil over capacity percolates to ground water.
4. **Evapotranspiration:** soil and surface lose water by temperature. This is A5's own ground-side loss; air moisture stays A4's.
5. **Runoff:** surface water above the retention threshold leaves the cell downstream.
6. **River routing:** a linear-store cascade. Each cell adds its runoff and ground-water baseflow to `riverStore` and releases `release_rate · riverStore` to its downstream cell. Cells are processed in **topological order** (upstream first, precomputed at init from flow direction), so each river tree is serial and different trees run in parallel. A pulse moves roughly one release time per cell per week, so it arrives downstream over several weeks.
7. **Basins:** inflow from routed water and direct precipitation, minus open-water evaporation (temperature × open area). Volume above the spill height leaves through the spill cell into routing next week. Level comes from volume through the basin's sorted bed elevations. Inundated cells are the basin cells with bed below the level; `inundatedWeeks` and `exposedWeeks` update.
8. **Floodplains:** a river cell whose flow exceeds `bankfull · mean flow` sets `flooded` on itself and its low neighbours (elevation within `floodplain_rise`) for that week. The flag clears when the flow drops: floods are temporary by construction.
9. **Glacier:** at the end of each melt season, cells whose snow pack never fell below `glacier_snow_floor` increment `snowYears`. At `glacier_onset_years` the cell becomes **glacier-eligible**, and from then on A4 hands it the snowfall above A4's 1.0 snow-pack cap (`glacierInput`, which A4 discards today). A5 adds it to `glacier` mass. Glacier mass melts into runoff above `glacier_melt_threshold`, so the end of an ice age gives a long melt pulse. `snowYears` resets, and eligibility ends, when the snow clears. A5 never writes A4's `snowPack`, and no water is counted twice.
10. **Clamp and totals:** clamp every field. Serial range-ordered totals (water in soil, ground, rivers, basins, ice) feed the panel and the water-balance test; they never feed back.

**Water balance (a tested invariant):** total water in = water out to sea + evapotranspiration + open-water evaporation + Δstorage, within float tolerance. The ocean is an unbounded sink. Unlike A4's moisture (soft conservation), A5 conserves.

### Initial state and spin-up (decided 2026-11-17)

- **Seed:** S-1 lakes start full at their spill height. Every other basin, the soil, ground water, river stores, ice and glacier start at zero.
- **Coupled spin-up:** A4's spin-up worker job runs A4 and A5 **together**, week by week in World Step order, through `WeatherRunner` + `HydrologyRunner`. It lasts 52 weeks, extended to the longest active climate cycle's settling period when cycles are on (A4 Feature 8 sets it; an ice-age world may need several years). The first live year is therefore already consistent: evaporation feedback, lake levels and baseflow have settled.
- The stage reports not-ready until the job finishes, and a newer world cancels it (A4's rule). SimPU never hitches.
- Spinning A4 up first and A5 from a replay afterwards was rejected: the first coupled year would drift. An analytic seed with a short spin was also rejected.
- When A4 is absent (fixed-input driver), A5 spins up alone on the driver's inputs.

### Reachability (Goal 4)

A5 publishes `Passability` per cell:

| Cell | Passability |
|---|---|
| Inundated basin cell (bed below level) | `Deep`, except corridor/crossing sites and cells with depth ≤ `ford_depth`, which are `Fordable` |
| Flooded floodplain cell | `Wet` (slower, never blocking) |
| Frozen water | `Frozen`, crossable by land |
| Everything else | `Dry` |

**Invariants:**
- Corridor and crossing cells are never `Deep`.
- **Regression R-Reach:** run each preset for 100 years at Small with the ice-age cycle on, re-run S-1's `ReachabilityValidator` with `Deep` cells as water every simulated year, and assert that every region stays reachable.

### Outputs

- `HydrologyUpdated{week}` published from `Commit`.
- `HydrologySnapshot`, a const view of committed state: `SoilMoisture(cell)`, `RiverFlow(cell)`, `Flooded(cell)`, `Ice(cell)`, `Glacier(cell)`, `InundatedWeeks(cell)`, `ExposedWeeks(cell)`, `PassabilityAt(cell)`, `SurfaceWetness(cell)`, `OpenWater(cell)`, plus per-basin `Level(basin)` and `Volume(basin)`.
- **A1 reads:** a seasonal mean of soil moisture (replacing A4's `rainfall` accumulator as cell water, and A1 Feature 4's water flow/table), river flow (the river-overlay water floor scales with it), flooded and ice.
- **A3 reads:** `inundatedWeeks` and `exposedWeeks` (Lake ↔ Wetland/Grassland), low river flow sustained over `dry_riverbed_weeks` (the `DryRiverbed` legacy), and `glacier` (cold-biome rules).

### Debug and observability (Feature 5)

- **DiaCellLayers base layers** `hydro.soil`, `hydro.ground`, `hydro.surface`, `hydro.river_flow`, `hydro.flooded`, `hydro.ice`, `hydro.glacier`, `hydro.passability`, against the shared grid desc. Basin level is shown as the inundation mask.
- **DiaCellHistory:** per-week groups for the water totals and per-region soil means; events for flood start/end, river runs dry, lake level record, freeze-up.
- **Sim-hosted commands** with mandatory help, consolidated as A4 did: `cow.hydro.state`, `cow.hydro.basins`, `cow.hydro.find {kind}` (floods, dry rivers, records), `cow.hydro.set_param`, `cow.hydro.preset`, `cow.hydro.balance`. That is 6 names against the 192 cap.
- **Instrumentation:**
  - `DIA_PROFILE_SCOPE` per pass and `DIA_TRACE_ZONE` on the step.
  - `cow.hydro.step_ms` with `MetricInfo` (S-3.5 R5).
  - A water-balance error gauge.
  - Health goes degraded when the balance error exceeds tolerance.
  - `HYD` log channel.

### Performance

**Report-only (user decision 2026-11-17).** `cow.hydro.step_ms` is measured and reported per size, with no gate. Routing is serial within a river tree, so parallel speed-up is bounded by the largest tree. The step must still finish inside the World Step slack, or DiaDeferredStep stalls and reports it (DFS-003).

## Public Interfaces (sketch)

```cpp
namespace CoW::Hydrology {
    class HydrologySimulation {                    // pure logic, Features 1-2
    public:
        void Initialise(const HydrologyTerrain&, const HydrologyParameters&);
        void Step(const WaterInput& in, Dia::DeferredStep::IParallelExecutor& exec);   // one week
        const HydrologyFields& Fields() const;
        const BasinState&      Basins() const;
        WaterTotals            Totals() const;
    };

    struct WaterInput {                            // A4's working copy or the fixed-input driver
        const float* waterToGround; const float* temperature; const float* snowPack;
        const float* glacierInput;                 // A4 snow above its cap, on glacier-eligible cells only
        WeekOfYear week;
    };

    class HydrologyRunner;                          // synchronous oracle: init + spin-up + N weeks
    class HydrologyStage final : public CoW::WorldStep::IWorldStepStage;   // Feature 3
    class HydrologySnapshot;                        // const view, committed state only
}
```

## Binding Decisions

| ID | Decision | How it applies |
|----|----------|----------------|
| PD-002 | ProcessingUnit/Phase/Module architecture | `HydrologyModule` is a SimModule on SimPU. No singletons; providers come through `ModuleRef` or ServiceStreams |
| PD-004 | No STL containers in public APIs | Fields use `DynamicArrayC`; the snapshot exposes accessors |
| AD-001 | Ask permission before Dia edits | A5 adds no Dia code. It uses built DiaCellLayers, DiaCellHistory, DiaValueSchema, `HexTopology`, and Approved DiaDeferredStep |
| AD-002 | Four-tier model | CoW-Shared |
| AD-003 | GDD owns design intent | GDD A5 does not exist yet; this spec stays `Draft` until the GDD session locks it or accepts these decisions |
| AD-005 | One stage per testbed | A5 owns no stage. S-2.6 Rivers & Floods is its testbed (own spec) |
| DFS-002 | `Run` reads only the captured working copy | A5 reads A4's *working copy* inside the same `Run` (stage order), plus its own capture. Nothing SimPU-owned |
| DFS-003 | A late step stalls SimPU | Report-only budget, but the step must fit the World Step slack |
| DFS-005 | Saves store pending results with apply keys | Feature 4 |
| SD-PERF-7 | Telemetry cost budgets | `cow.hydro.step_ms` is one gauge per applied step; no per-cell telemetry leaves SimPU |

## Decisions (interview 2026-11-17)

1. **Lakes grow and shrink, bounded by S-1 basins.** Water fills a basin up to its spill height and overflows through the outlet. Basins that are not lakes at generation can fill. A5 marks inundation; **A3 flips the biome** after a sustained period.
2. **Routing is flow accumulation through linear stores** along S-1 flow directions, so floods take weeks to arrive. Shallow-water simulation was rejected (cost, stability, bit-identity).
3. **Reachability: floods are temporary, corridors and crossings stay fordable, ice never blocks.** Proven by the R-Reach long-run test.
4. **The ice age reaches glaciers.** Persistent snow becomes glacier ice, which A3 turns into cold biomes; it retreats when the cycle warms.
5. **Own testbed:** S-2.6 Rivers & Floods (A4 + A5 on S-2.5's worlds).
6. **Dragon water effects go through A4's influence API.** A1's direct `water_dragon_emission` is dropped.
7. **Performance is report-only.**
8. **Water is conserved** (unlike A4's moisture), and a balance test enforces it.
9. **S-1 persists basin data in `.wld.json`** (raw/bed elevation + basin table; world format version bump). It was chosen over rebuilding on import and over A5 running its own flood fill.
10. **Spin-up is coupled with A4** in one worker job, lengthened when climate cycles are on.
11. **Glacier water comes from A4's capped-off snow.** On glacier-eligible cells A4 hands its above-cap snowfall to A5 (`glacierInput`) instead of discarding it. A5 never writes `snowPack`. Glacier mass melts into runoff, so the end of an ice age gives a melt pulse.

## Dependencies

- **S-1 World Generation (built):** cells, flow direction, basin table, lakes, rivers, corridor sites, `ReachabilityValidator`. Needs the consumer amendment above for imported worlds.
- **A4 Weather (Approved, unbuilt):** `waterToGround`, temperature, snow pack. Needs the amendment above, applied **before** A4 Features 1 and 2 are built. Influence API and climate cycles are A4 Features 7 and 8 (not yet specced).
- **World Step (Approved, unbuilt):** stage registration and `order`.
- **DiaDeferredStep (Approved, building):** `IParallelExecutor`, pending save.
- **E1 SaveLoad (Done), DiaCellLayers, DiaCellHistory, DiaValueSchema (Done), GameSpeedController (Done).**
- **Required by:** A1 (amend: Feature 4 water flow/table removed; reads A5), A3 (amend: inundation, dry-riverbed and glacier rules), V-1, S-2.6, S-3 (Living mode), S-3.5 (order gains `hydrology`; R1/R2/R5 cover it), F-3 (a hydrology start state and pack).

## Test Strategy

- **Per-pass unit tests on hand-built grids:**
  - a single river chain (pulse arrival time and attenuation)
  - a two-tributary confluence (order-independence)
  - a basin that fills, spills and drains
  - a basin that drops below a cell and exposes it
  - a floodplain that floods and clears
  - freeze holds water and thaw releases it
  - glacier onset after N years and retreat
- **Invariants:**
  - water balance closes every week
  - no negative stores
  - serial vs parallel bit-identity at 1, 2, 7 and 16 ranges
  - deferred vs synchronous bit-identity over 104 weeks with A4
- **Reachability (R-Reach):** 100 years per preset at Small with the ice-age cycle on; no region is permanently unreachable; corridors are never `Deep`.
- **Goldens:** per-layer hashes via `CellLayerGolden.h` for the presets × seeds 1–3 at Small.
- **Fixed-input driver:** A5 runs with no A4 (constant and scripted inputs), so Features 1, 2 and 6 never need weather built.

## Open Design Questions

The three interview-time questions are resolved (Decisions 9–11). Open, to revisit during implementation:

1. **Spin-up length with cycles on.** A multi-year coupled spin-up at Large could take seconds on the worker job. If it does, consider caching a spun-up state in F-3 bundles (a start state) rather than shortening the spin-up.
2. **Topological routing and parallelism.** River trees are serial. If one dominant tree drains most of a continent, the routing pass is effectively single-threaded. Measure it first (performance is report-only); split long trees into chained segments only if the step misses its World Step slack.
3. **Basin overflow chains.** A filling basin can spill into another basin downstream, and that one into a third. Overflow enters routing **next week**, so a chain of k basins takes k weeks to propagate. Check that this matches the GDD's intent for chained lakes; same-week resolution would need an ordered basin pass.

## Status

**Status:** `Draft` (2026-11-17). Interview done with the user the same day, including the three follow-up questions (Decisions 9–11). Waiting on: the GDD session writing and locking A5 (fold-back sent), then approval. No feature specs and no plan.
