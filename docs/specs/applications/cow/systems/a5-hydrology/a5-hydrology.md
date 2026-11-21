# System Spec: A5 Hydrology

**Parent:** @docs/specs/applications/cow/cow.md
**Design source:** the locked GDD `C:\GitHub\DesignDocs\COW\Output Docs\GDD\08_SystemsDeepDive\A5_Hydrology\` (locked 2026-11-17; DECISION-129..134, CHANGE-047/048, PARK-029, PARK-030). The GDD owns design intent and constants (AD-003). The interview answers are in `Cluiche/out/CoW/gdd-foldback-hydrology-weather-2026-11-17.md` (Part 2). Aligned to the locked GDD (aligned to locked GDD 2026-11-17); the differences that remain are listed under "GDD differences".
**Status:** `Approved` (2026-11-17, user; GDD A5 locked). Plan: @docs/specs/applications/cow/systems/a5-hydrology/a5-hydrology.plan.md
**Amended 2026-11-18 (world-sim review, user decisions):** see "World-sim review amendment" below.

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
| 1 | Data & Parameters | `HydrologyFields`, `HydrologyTerrain` (incl. basin table, `isCrossing` = protected cells), 43 scalar descriptors (GDD names and values) + a 13-biome × 3-column `biome_table`, versioned file, atomic overrides, `Initialise` (validation, tiny-depression filter, in-basin directions, full lakes, static tables) and `ApplyPhaseSeed`. Pure logic, TDD | @docs/specs/applications/cow/systems/a5-hydrology/hydrology-data.md | Approved |
| 2 | Water Pipeline | The weekly passes: inputs, freeze (cells and basin ice), glacier, infiltration and springs, ET and injected water sinks, runoff and baseflow, a topological cascade with basins inside it (slope-based release, same-week crossing and chained lakes), basin levels and inundation, floods with soil recharge against each river's running mean, counters, passability, glacier eligibility, double totals. Pure logic, TDD with hand-built grids | @docs/specs/applications/cow/systems/a5-hydrology/water-pipeline.md | Approved |
| 3 | `HydrologyModule` & World Step Stage | Registers as stage `hydro`; Capture/Run/Commit; reads A4's in-run result; `HydrologyUpdated{week}` + `HydrologySnapshot` (incl. `RiverState`); init from S-1, frozen-phase spin-up with a phase-scaled seed, `HoldGeneration`/`WorldGenerationFailed` | @docs/specs/applications/cow/systems/a5-hydrology/hydrology-module.md | Approved |
| 4 | Save/Load | E1 participant; pending working set through DiaDeferredStep (DFS-005); JSON arrays | @docs/specs/applications/cow/systems/a5-hydrology/hydrology-save.md | Approved |
| 5 | Debug & History | DiaCellLayers base layers, DiaCellHistory groups and events, sim-hosted `cow.hydro.*` commands with help, `cow.hydro.step_ms` | @docs/specs/applications/cow/systems/a5-hydrology/hydrology-debug.md | Approved |
| 6 | Presets & Regression | Scenarios (forced wet year, drought decade, melt pulse, ice-age phase), layer goldens via `CellLayerGolden.h`, the reachability long-run test | @docs/specs/applications/cow/systems/a5-hydrology/hydrology-presets-and-regression.md | Approved |

**Build order:** 1 → 2 → 6 (pure logic, provable by GoogleTests on hand-built grids) → 3 → 4 → 5. 3 needs `WorldStepModule` and A4 Feature 3. The S-2.6 testbed is its own spec (GDD A5 is now locked, so it can be written).

## Architecture

### Placement

**CoW-Shared** (AD-002): consumed by A1, A3, A4 (evaporation), V-1, S-2.6, S-3 and S-3.5. Pure logic in `Cluiche/CoW/Shared/Hydrology/`; the SimModule, debug domain and save participant in `Cluiche/CoW/Modules/`.

```
SimPU
  └─ WorldStepModule ("order": ["weather", "hydro", "ecology"])
  |    one `world` channel: Capture all → Run in order → Commit in order
  |
  └─ HydrologyModule (SimModule)
       on WorldGenerationComplete → HydrologySimulation::Initialise(terrain, basins) → spin-up
       registers as world-step stage "hydro"; does NOT kick or schedule
       on Commit → publish HydrologyUpdated{week}
       on BiomeTransitionComplete → update cached biome (applies next week)
       owns: HydrologySimulation, HydrologyParameters, save participant, debug domain, cell layers
```

A5 can run without A1 (S-2.6: `["weather", "hydro"]`). It cannot run without a water source: when A4 is absent, a **fixed-input driver** (a constant or scripted `waterToGround` per week) stands in, so tests and Controlled modes work without weather.

### Coupling with A4 (one week each way, no cycle inside a step)

| Direction | What | When |
|---|---|---|
| A4 → A5 | `waterToGround` (rain + melt), `temperature`, `snowPack`, `glacierInput` (above-cap snowfall **plus the yearly firn hand-off**) | **This week**: A5's `Run` reads A4's finished working copy, because A4 ran first inside the same `Run` |
| A5 → A4 | `surfaceWetness` (A4 §6.2 secondary evaporation), `openWater` fraction (A4 §6.1 primary evaporation, now that lake extent varies), `glacierEligible` flag | **Last week**: A4 `Capture` latches A5's committed state |

This is the one-week lag the World Step model gives for free. It is documented, not hidden: a melt pulse affects evaporation a week after it lands.

**A4 amendment owed (before A4 Features 1 and 2 are built):**
- `groundWater` leaves `WeatherFields` and `waterToGround` is added.
- §10/§11.2 route into `waterToGround`.
- §12 (drain) is deleted.
- §6.1/§6.2 read A5's captured values. With no A5 present, they fall back to S-1's static `waterSource` and zero ground evaporation.
- The `rainfall` output drops its `groundWater·0.1` term.
- **Glacier ice has two inputs, both handed over by A4** (decided 2026-11-17; the second added by the GDD, CH-A5-01, A4 TechSpec §11.3). On cells A5 flags as glacier-eligible, A4 outputs as `glacierInput`: (1) snowfall above its 1.0 cap, which it used to discard, and (2) the **firn hand-off**: once a year at `GLACIER_CHECK_WEEK` (week 26, end of Summer), the snow above `GLACIER_SNOW_FLOOR` (0.2) moves out of A4's `snowPack` into `glacierInput`. A4 lowers its own `snowPack`. A4 still owns `snowPack`; A5 never writes it.
- **Spin-up is coupled:** A4's spin-up job runs A4 + A5 together (see Initial state). A4 also exposes the start phase's coldness (`ClimateCycles::Coldness`, `WeatherRunner::StartColdness`) for the seed.

### Grid and terrain ingest

The grid is the same `Dia::Pathfinding::HexTopology` (flat-top odd-q) that A4 uses, built from S-1's dimensions. A5 copies at `Initialise` and never holds the pointer:

- From `Cell`: `elevation`, `waterSource`, `downstream` (0..5 on river cells), `biome`, `region`, `terrainFlags`.
- From S-1's drainage stage: flow direction for **every** land cell, flow accumulation (an init weight only), the **basin table** (cells, spill height, spill cell, volume), lake membership and **bed elevation** under lakes (`Cell::elevation` is the lake *surface*).
- From S-1 sites: corridor and crossing cells, and the **fixed-site cells (Hearth, Nexus, world-throne)**. Together they are the protected cells (`isCrossing`, the GDD's `is_protected`).
- S-1 never places a Hearth, Nexus or world-throne cell below a **real** basin's spill height (GDD Key Design Decision 5). "Real" uses A5's `basin.min_cells` (3) and `basin.min_capacity` (1.0): tiny priority-flood depressions are not basins and do not constrain placement (S-1 AC 38 reads the two values from A5's parameters). A5 also caps protected cells at `Fordable` as a safety net.

**S-1 consumer amendment owed (CoW-side, no AD-001 sign-off; decided 2026-11-17):** S-1 **persists** raw/bed elevation and the basin table (cells, spill height, spill cell) in `.wld.json`, bumping the world format version. Flow direction is already rebuilt on import. Import reads the basin table back, so imported, `world_file` and F-3 baked worlds give A5 exactly what a freshly generated world does. A drainage-rule change in S-1 never silently changes an old world's basins. Rebuilding on import, and A5 running its own priority-flood, were both rejected. **Version 1 world files** (user decision 2026-11-17, matched by GDD EC-15) still import, with a lake-only basin table and `basinsFromLakesOnly = true`. On such worlds A5 holds each lake at its surface level and fills no other basin. Lake cells are always `Deep`, **except protected cells (corridor, crossing and fixed-site cells), which stay `Fordable`**, so "protected cells are never Deep" still holds. Rivers, soil, floods and ice work as normal. A re-bake from the seed restores full basins.

### State (structure of arrays)

```cpp
namespace CoW::Hydrology {

    // Water is in cell-depth units (the GDD's "water units"). Full field list: Feature 1.
    struct HydrologyFields {                    // per cell, N each
        DynamicArrayC<float> surfaceWater;      // cell-depth >= 0 standing water; runs off above surface_retention (feeds A4 §6.2)
        DynamicArrayC<float> soilMoisture;      // cell-depth, [0, soil_capacity(biome)]; A1 reads soilMoisture / capacity
        DynamicArrayC<float> groundWater;       // cell-depth, [0, gw_capacity]; baseflow into the channel; springs above the cap
        DynamicArrayC<float> riverStore;        // >= 0, the GDD's channel_store: water in transit on every land cell
        DynamicArrayC<float> riverFlow;         // >= 0 released downstream this week (river cells), the river's "size"
        DynamicArrayC<float> meanFlow;          // river cells: running mean (half-life), the flood and low-flow baseline
        DynamicArrayC<float> ice;               // cell-depth >= 0, frozen surface and channel water (held, never routed)
        DynamicArrayC<float> glacier;           // cell-depth >= 0, glacier ice mass
        DynamicArrayC<uint16_t> inundatedWeeks; // consecutive weeks under basin water (A3 reads)
        DynamicArrayC<uint16_t> exposedWeeks;   // consecutive weeks a Lake cell is above water (A3 reads)
        DynamicArrayC<uint16_t> lowFlowWeeks;   // river cells, not frozen (A3 DryRiverbed)
        DynamicArrayC<uint16_t> glacierWeeks;   // glacier >= glacier.biome_min (A3 glaciation)
        DynamicArrayC<uint16_t> glacierFreeWeeks; // glacier < glacier.biome_min (A3 glacial retreat)
        DynamicArrayC<uint16_t> snowYears;      // consecutive years snow survived summer (glacier onset)
        DynamicArrayC<uint8_t>  flooded;        // floodplain flood flag this week
        // ... glacierEligible, openWater, passability: Feature 1
    };

    struct BasinState {                          // per real basin, not per cell
        DynamicArrayC<float> volume;            // current water volume (cell-depth)
        DynamicArrayC<float> iceFraction;       // frozen share of the open surface
        DynamicArrayC<float> level;             // derived from volume via the basin's hypsometry
    };

    enum class Passability : uint8_t { Dry, Wet, Fordable, Deep, Frozen };
    enum class RiverState  : uint8_t { Dry, Low, Normal, Flood, Frozen };   // snapshot read
}
```

At Large this is about 60 KB per float field, under 1 MB per working copy.

### Weekly pipeline (Feature 2)

Detail and acceptance criteria are in @docs/specs/applications/cow/systems/a5-hydrology/water-pipeline.md. This is the summary, aligned to the feature spec and the locked GDD (2026-11-17). Every pass reads a stable copy. Only the cascade has an order.

0. **Inputs:** `waterToGround` + wetter influence onto the surface of land cells (onto the basin for inundated cells); `glacierInput` onto glacier mass.
1. **Freeze/thaw:** below `FREEZE_TEMP` (0.25) a temperature-scaled share of surface and channel water becomes ice. Above `THAW_TEMP` (0.32) ice melts back to surface water. Between them nothing happens (hysteresis). A basin's `iceFraction` rises and falls the same way. Frozen water is held, not routed.
2. **Glacier:** melt above `GLACIER_MELT_TEMP` (0.4) joins the surface and routes this week.
3. **Infiltration:** soil takes at most the biome's `infil_rate` per week (a tenth on frozen ground). Soil above field capacity percolates to ground water. Ground water above `GW_CAPACITY` comes back up as springs.
4. **ET and sinks:** soil ET = `ET_RATE · T · veg_factor · multiplier · soil`. Surface evaporation is separate. Injected draws come from surface, then soil, then channel store.
5. **Runoff and baseflow:** surface above `SURFACE_RETENTION` and a share of ground water move into the cell's channel store.
6. **Cascade:** one topological order over cells **and basins**. `out = k · (store + inflow)`. Rivers use `k = clamp(RELEASE_MIN + RELEASE_SLOPE · drop, 0.3, 0.85)`, hillslopes use 0.9. A pulse's centre of mass moves about `k / (1 − k)` cells per week. A basin takes its inflow, evaporates, and spills in the same pass, so chains of lakes fill through in one week. Inside a real basin, flow follows steepest descent on the bed to the pit.
7. **Basins:** level and inundation from volume. Newly inundated cells move their land stores into the basin. `inundatedWeeks`, `exposedWeeks` and `openWater` update.
8. **Floodplains and counters:** a river above `BANKFULL_FACTOR` (1.8) × its own running mean floods itself and its low, dry neighbours, and recharges their soil from the river's channel store. Counters `lowFlowWeeks` (frozen weeks neither count nor reset), `glacierWeeks` and `glacierFreeWeeks` update.
9. **Passability:** `Frozen` first, then `Deep` (never on protected cells, which cap at `Fordable`), `Fordable`, `Wet`, `Dry`.
10. **Glacier eligibility:** once a year at `GLACIER_CHECK_WEEK` (26), `snowYears` counts a summer survived when the snow pack is at or above `GLACIER_SNOW_FLOOR` (0.2). Eligible at `GLACIER_ONSET_YEARS` (3).
11. **Totals:** clamp (never drop water), then sum every store and flow in double, serial per range. They never feed back.

**Water balance (a tested invariant):** total water in = water out to sea + evapotranspiration + open-water evaporation + injected draws + Δstorage, within tolerance. The ocean is an unbounded sink. Unlike A4's moisture (soft conservation), A5 conserves. **Inundated cells** (the stable copy's flags) skip passes 3, 4, 5 and 8: their water goes straight into the basin and A1 treats them as aquatic habitat (CH-A5-06).

### Initial state and spin-up (decided 2026-11-17, aligned to locked GDD 2026-11-17)

- **Frozen-phase spin-up:** A4's spin-up worker job runs A4 and A5 **together**, week by week in World Step order, through `WeatherRunner` + `HydrologyRunner`, for `SPINUP_WEEKS` = **52**. Climate cycles are **held at the world's start phase** for the whole spin-up, so the first live year is consistent with that phase and no long multi-year spin-up is needed. There is no longer a spin-up "extended when cycles are on".
- **Phase-scaled seed.** One year cannot build slow stores, so they start seeded for the start phase. `cold(phase)` is the ice-age cycle's normalised coldness at the start: 0 at the warm point, 1 at the coldest. A4 exposes it per cell (`WeatherRunner::StartColdness`), with a per-cell `StartSnowSurvivesSummer` mask for the glacier seed. Lakes use their basin's mean coldness (seam fix 2026-11-17).
  - S-1 lakes: `volume = capacity · lerp(1.0, LAKE_SEED_COLD (0.6), cold(phase))`.
  - Glaciers, on cells whose start-phase temperature keeps snow through the summer: `glacier = GLACIER_SEED_MAX (1.0) · cold(phase)`, with `snowYears` at the onset count (eligible).
  - Every other basin, and every soil, ground-water, channel and ice store, starts at 0. (The earlier "soil and ground water start at a fraction of capacity" knob stays in Feature 1 with a default of 0.)
- **The default start is the warm point**, so the seed is a no-op: full lakes, no glaciers. A preset can pin another start phase.
- The stage reports not-ready until the job finishes, and a newer world cancels it (A4's rule). SimPU never hitches.
- Spinning A4 up first and A5 from a replay afterwards was rejected: the first coupled year would drift. An analytic seed with a short spin was also rejected.
- When A4 is absent (fixed-input driver), A5 spins up alone on the driver's inputs.

### Reachability (Goal 4)

A5 publishes `Passability` per cell:

| Cell | Passability |
|---|---|
| Frozen water: inundated with basin `iceFraction` ≥ `ICE_CROSSING_MIN` (0.6), or a river cell with `ice` ≥ it | `Frozen`, crossable by land (checked first) |
| Inundated basin cell, depth > `FORD_DEPTH` (0.5), not protected | `Deep` |
| Inundated basin cell, any other case, **including protected cells (corridor, crossing, Hearth, Nexus, world-throne)** | `Fordable` |
| Flooded floodplain cell | `Wet` (slower, never blocking) |
| Everything else | `Dry` |

**Invariants:**
- Protected cells (corridor, crossing, Hearth, Nexus and world-throne cells) are never `Deep`. On a `basinsFromLakesOnly` world lake cells are always `Deep` except protected cells.
- For now only R-Reach reads passability. B2 and pathfinding read it when S-4 Dragon is designed (PARK-029). How gameplay reads A5 water is undecided (PARK-030).
- **Regression R-Reach:** run each preset for 100 years at Small with the ice-age cycle on, re-run S-1's `ReachabilityValidator` with `Deep` cells as water every simulated year, and assert that every region stays reachable.

### Outputs

- `HydrologyUpdated{week}` published from `Commit`.
- `HydrologySnapshot`, a const view of committed state: `SoilMoisture(cell)`, `RiverFlow(cell)`, `Flooded(cell)`, `Ice(cell)`, `Glacier(cell)`, `InundatedWeeks(cell)`, `ExposedWeeks(cell)`, `PassabilityAt(cell)`, `SurfaceWetness(cell)`, `OpenWater(cell)`, plus per-basin `Level(basin)` and `Volume(basin)`.
- `RiverState(cell)` (Dry / Low / Normal / Flood / Frozen) for V-1 and A1.
- **A1 reads:** a seasonal mean of soil moisture (replacing A4's `rainfall` accumulator as cell water, and A1 Feature 4's water flow/table), river flow (the river-overlay water floor scales with it), flooded and ice. A1 keeps `water_gradient` and builds it from A5 in a thin step 2 (A1's spec, noted here). A1 treats inundated cells as aquatic habitat.
- **A3 reads** the counters at its Decade check, so only a state that is sustained at the time of the check flips a biome (A3's first-run thresholds, GDD §14.4):
  - `inundatedWeeks` ≥ 104: any non-protected land biome becomes Lake. Lake with `exposedWeeks` ≥ 52 becomes Wetland.
  - `lowFlowWeeks` ≥ 26 on a river cell: the `DRY_RIVERBED` legacy marker, no biome change.
  - `glacierWeeks` ≥ 260 (5 years): Alpine if `elevation ≥ 0.7`, else Tundra. **No new biome.** Ocean and Lake are excluded. A3 saves `pre_glacial_biome`.
  - `glacierFreeWeeks` ≥ 260 on a glaciated cell: glacial retreat back to the recorded biome with a `BARE_ROOTS` marker.
- **V-1 shows** lake extent, floods, ice and river width by flow. Soil and ground water are debug-only (Constitution §7).

### Debug and observability (Feature 5)

- **DiaCellLayers base layers** `hydro.soil`, `hydro.ground`, `hydro.surface`, `hydro.river_flow`, `hydro.flooded`, `hydro.ice`, `hydro.glacier`, `hydro.passability`, against the shared grid desc. Basin level is shown as the inundation mask. (The GDD also lists `hydro.river_state`, `hydro.basins` and `hydro.flow_dir`; those are not shipped, see Feature 5.)
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
| AD-003 | GDD owns design intent | GDD A5 is locked (2026-11-17). This spec follows it. The few remaining differences are under "GDD differences" |
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
10. **Spin-up is coupled with A4** in one worker job: 52 weeks, climate cycles held at the start phase, slow stores seeded for that phase (GDD §13).
11. **Glacier water comes from A4.** On glacier-eligible cells A4 hands A5 its above-cap snowfall and, once a year at the end of summer, the snow that survived above the floor (the firn hand-off, GDD CH-A5-01) as `glacierInput`. A4 lowers its own `snowPack`; A5 never writes it. Glacier mass melts into runoff, so the end of an ice age gives a melt pulse.
12. **Spill chains fill through in the same week:** basins are nodes in the topological cascade (user decision, matched by the GDD).
13. **Injected water sinks** (ET multiplier + draw, `IWaterSinkProvider`) keep A5 independent of A1. Dragon water goes through A4's influence API (user decisions).
14. **Floods are judged against each river's running mean** (`meanFlow`), with soil recharge from the channel store (GDD §9).

## Dependencies

- **S-1 World Generation (built):** cells, flow direction, basin table, lakes, rivers, corridor sites, `ReachabilityValidator`. Needs the consumer amendment above for imported worlds.
- **A4 Weather (Approved, unbuilt):** `waterToGround`, temperature, snow pack. Needs the amendment above, applied **before** A4 Features 1 and 2 are built. Influence API and climate cycles are A4 Features 7 and 8 (not yet specced).
- **World Step (Approved, unbuilt):** stage registration and `order`.
- **DiaDeferredStep (Approved, building):** `IParallelExecutor`, pending save.
- **E1 SaveLoad (Done), DiaCellLayers, DiaCellHistory, DiaValueSchema (Done), GameSpeedController (Done).**
- **Required by:** A1 (amend: Feature 4 water flow/table removed; reads A5; keeps `water_gradient`, built from A5 in a thin step 2; treats inundated cells as aquatic habitat), A3 (amend: inundation, dry-riverbed and glacier rules), V-1, S-2.6, S-3 (Living mode), S-3.5 (order gains `hydro`; R1/R2/R5 cover it), F-3 (a hydrology start state and pack).

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

## GDD differences

Only real remaining differences (aligned to locked GDD 2026-11-17). The user's decisions win. Per-feature differences are in each feature spec.

| Topic | GDD | This spec |
|---|---|---|
| Units | water units (wu) | Same unit, called cell-depth (user decision) |
| Balance tolerance | `BALANCE_TOLERANCE` 1e-5 × stored | `1e-4 · max(in, out, 1)` per week plus a cumulative drift bound (user decision) |
| Water sinks | None; "drier" influence takes from soil | `IWaterSinkProvider` ET multiplier and draw; "drier" is a draw (user decision) |
| Glacier melt timing | Pass 8, after routing | Pass 2, so it routes the same week (Feature 2 Open Design Question 4) |
| Initial soil and ground water | 0 | Knobs exist, default 0 |
| Init failure | `WorldGenerationFailed` | A5 goes Unhealthy; it does not publish S-1's message |

## Open Design Questions

The three interview-time questions are resolved (Decisions 9–11). Open, to revisit during implementation:

1. ~~**Spin-up length with cycles on.**~~ **Resolved by GDD §13:** 52 weeks with cycles held at the start phase and a phase-scaled seed. No multi-year spin-up. Caching a spun-up state in F-3 bundles remains an option if 52 weeks at Large is slow on the worker job.
2. **Topological routing and parallelism.** River trees are serial. If one dominant tree drains most of a continent, the routing pass is effectively single-threaded. Measure it first (performance is report-only; the GDD lists the same question); split long trees into chained segments only if the step misses its World Step slack.
3. ~~**Basin overflow chains.**~~ **Resolved 2026-11-17 (Feature 2 interview):** basins are nodes inside the topological cascade, so chained lakes resolve in the same week. The GDD agrees.

## World-sim review amendment (2026-11-18)

Source: @docs/research/worldsim_data_driven/summary.md §0 and §0.1; kit: @docs/specs/applications/cow/systems/worldstep/worldsim-kit.md; parameters: @docs/specs/applications/dia/systems/diaattribute/parameter-files.md; biomes: @docs/specs/applications/cow/systems/s1-worldgeneration/biome-registry.md; search helpers: @docs/specs/applications/dia/systems/diapathfinding/grid-search-helpers.md.

**Each item below supersedes any contradicting text in this spec and its features.** The feature specs list what they supersede, section by section.

1. **Parameters are schema-first.** Replaced: `hydrology_parameters.json`, the 43-row C++ `ParamDescriptor` table, the `biome_table` block's row descriptors, `LoadHydrologyParameters` / `ApplyOverrides` / `SetParam` / `ToJson` / `DefaultHydrologyParameters`, and the params hash.
   - The sidecar `Global/Hydrology/hydro_parameters.schema.json` (format `cow.hydro_params`) is the single source of each parameter's kind, range, default and description. The 43 GDD keys, names, values and ranges move into it unchanged.
   - Cross-row rules become sidecar `constraints`: `routing.release_floor <= routing.release_ceil` and `freeze.threshold <= freeze.thaw_temp`. The whole-number rules (`basin.min_cells`, `glacier.onset_years`, `glacier.check_week`) are `int` kinds.
   - "Hard range vs GDD tuning range": the hard range is the sidecar `min`/`max`; the GDD tuning range may be recorded as sidecar `bands`.
   - **Loading is strict** through `Dia::Attribute::ParameterFile`. A fault fails loudly: Failing health, no compiled fallback. **Packs** are `Global/Hydrology/Packs/<id>.hydro.json`, applied with `ApplyPack` (lenient, atomic). `ParamBinding<HydrologyParameters>` gives the per-step snapshot. `Hash()` replaces the params hash.
2. **The biome table becomes three maps**, each keyed `keyRef cow.biomes` and complete (`data.incomplete-map` in the CLI, a load failure in C++):

   | Map | Column it replaces |
   |---|---|
   | `hydro.biome.soil_capacity` | `soil_capacity` |
   | `hydro.biome.infil_rate` | `infil_rate` |
   | `hydro.biome.veg_factor` | `veg_factor` |

   The unknown-biome fallback is the registry's `fallback` role (`ResolveOrFallback`), not a hard-coded `GrasslandSteppe` row.
3. **Stage.** `HydrologyStage` derives `WorldStepStageBase`. A5's differences map to hooks: `ExtraReady` (a water input exists); `LatchInputs` returns false for an invalid input view (replaces `mStepValid`); `InitialiseSim` fails with a cause code; `ApplyEntry` handles A5's mutation kinds; `PublishCommit` publishes `HydrologyUpdated`; `OnStartExtra` / `OnStopExtra` wire `SetHydrologyProvider` and `SetWaterSource`. The coupled spin-up and `IHydrologySpinUpHost` stay system code on top of `SpinUpSlot`.
   - Health is DiaObservation `kOK` / `kDegraded` / `kFailing`; "Unhealthy" becomes Failing, with a cause code.
   - **The stage id is `hydro`.** The World Step order is `["weather", "hydro", "ecology"]` (S-2.6: `["weather", "hydro"]`).
   - `IWorldStepStage` gains `ValidatePending`. The base registers `cow.hydro.step_ms` and `cow.hydro.discarded_steps`.
4. **Save** uses the kit codec helpers. No in-record `"version"`: `GetVersion` (`kFormatVersion` stays) plus `RegisterMigration`. Participant id `cow.hydro`. `params_hash = ParameterFile::Hash()`, which now covers the biome maps. `Validate` ranges come from the layer sidecar entry (`cow.hydro.layers/<field>`); a field with no layer gives its range in a hydro state sidecar. Restore uses the shared sequence, the event-order fallback and the pending blob. A5-only extras stay: `basin_count`, `world_hash`, the mixed A4/A5 table.
5. **Commands.** One `cow.hydro.params {action: set|get|list|reload|save_defaults}` with the kit's reply shapes. This gives A5 a `reload` it lacked. The map key form is `hydro.biome.<column>.<BiomeId>` (was `biome_table.<BiomeId>.<column>`). `cow.hydro.set_param` is gone. Registration goes through `SimCommandSet`. The six-name count is unchanged (`state`, `basins`, `find`, `params`, `preset`, `balance`).
6. **Naming.** Layer ids `hydro.<snake_case>`. Schema keys `cow.hydro.layers/<field>` (was `hydro/<key>`). Formats `cow.hydro_params` and `cow.hydro_state`. Save id `cow.hydro`.
7. **Events.** `HydrologyUpdated` and the other A5 messages are declared in `cow_worldsim.diagamemessages` and generated with `dia codegen messages`.
8. **Data I/O.** Every A5 file and sidecar is catalogued, paths resolve through `PathStore`, and `FileWatcher` reloads the sidecar and packs in dev builds (the same path as `params {action: reload}`).
9. **Presets and selectors** use `CellSelectorSet`: DiaCondition `ConditionExpr` over `WorldCellFields` plus `derived` fields. A5 contributes its committed fields to `WorldCellFields` under slot `hydro`, including `hydro.basin_kind`. The clause mapping is in @docs/specs/applications/cow/systems/a5-hydrology/hydrology-presets-and-regression.md.
10. **Biomes.** `BiomeIndex` comes from the registry and the `Biome` enum is deleted. A5's cached per-cell biome array is `BiomeIndex`; the biome count is the registry's `Count()`, not 13.
11. **Terrain profile; Open Design Question 2 (soil by terrain, in Feature 1) is resolved.** S-1's param maps keyed by `TerrainType`, `terrain.soil_depth_mult` and `terrain.runoff` (both provisional), are available. A5 multiplies soil capacity by `terrain.soil_depth_mult`, with the values tuned in S-2.6.
12. **Search helpers.** Basin and depression searches may use DiaPathfinding's `PriorityFlood` and `ConnectedComponents` instead of A5-local copies.
13. **F-3 and T-2.** Hydrology is an F-3 pack kind (`hydro`) with a `HydrologyPackParticipant`, and gets a T-2 form route for `cow.hydro_params`. This fixes this spec's reference to "F-3 hydrology packs", which F-3 did not support. See @docs/specs/applications/cow/systems/f3-worldassets/f3-worldassets.md and @docs/specs/applications/cow/systems/t2-worlddataeditor/t2-worlddataeditor.md.

| Superseded text in this spec | By item |
|---|---|
| Features row 1 ("43 scalar descriptors ... `biome_table`, versioned file, atomic overrides") | 1, 2 |
| Features row 3 ("Registers as stage `hydrology`"); Architecture stage tree (`"order"`, `registers as world-step stage "hydrology"`); the S-2.6 order | 3 |
| Features row 5 and Architecture "Sim-hosted commands" (`cow.hydro.set_param`) | 5 |
| Architecture "Instrumentation" (`cow.hydro.step_ms` registered by A5) | 3 |
| Public Interfaces `HydrologyStage : IWorldStepStage` | 3 |
| GDD differences "Init failure ... A5 goes Unhealthy" | 3 |


## Spec-6 resolutions (2026-11-18, user)

These settle open points raised while amending. Each item supersedes contradicting text above, including the world-sim review amendment.

1. **Layer ids are renamed to field names:** `hydro.soil` -> `hydro.soil_moisture`, `hydro.ground` -> `hydro.ground_water`, `hydro.surface` -> `hydro.surface_water` (and the same `<sys>.<field>` rule for the rest). The schema keys follow (`cow.hydro.layers/<field>`).
2. **Presets** use format `cow.hydro_preset`, at `Global/Hydrology/Presets/<id>.hydro_preset.json` (it was `cow.hydrology_preset` / `.hydropreset.json`).
3. **Saves** store biomes through the kit's biome palette.
4. **Selector clauses:** `crossing` -> `terrain.is_protected`. `basin` -> `hydro.basin_kind` (`none`/`lake`/`dry`; `any` = `!= none`).
5. **Packs** have no separate format id: a hydro pack is a sparse `cow.hydro_params` document under `Packs/`, carrying `basedOn` (Parameter Files resolution).
6. **`terrain.runoff`** is not consumed until a later amendment.

## Draoi layer amendment (2026-11-21)

The platform gained the Draoi layer (PD-011 to PD-014, @docs/specs/applications/draoi/draoi.md). This amendment follows the World-Sim Kit's "Draoi layer amendment" (@docs/specs/applications/cow/systems/worldstep/worldsim-kit.md). Design decisions and the status line are unchanged. Nothing was built, so no code moves. It supersedes contradicting text above, including file paths.

**Pure sim, package `DraoiHexWorldHydrology`.** `HydrologySimulation` and everything under `Shared/Hydrology/` that is not wiring: the routing cascade, basin lakes, floods, glaciers, ground water, soil and freeze, `HydrologyTerrainBuilder`, spin-up, `HydrologyParameters` (type and loader), `WaterTotals` and `Passability`. It may depend on `DraoiHexWorld` and `DraoiHexWorldWeather` only (never Ecology, never the terrain generator). It is scheduler-agnostic: a synchronous `Step(...)` plus a thread-neutral `Capture` / `Run` / `Commit` ("`Run` touches only the working copy; the caller must not overlap `Run` with `Capture` or `Commit`"), no `SimModule`, bus, save or debug-domain wiring, and no mention of SimPU or a step chain. `Step` takes `IParallelExecutor` from DiaThreading (kit amendment item 7), not a DiaDeferredStep type. Tests and helpers ship in `DraoiHexWorldHydrology/Testing/` (`HydrologyTestGrids.h`, fake inputs).

**Stays in CoW (the adapter).** `HydrologyModule` and `HydrologyStage` (derived from the CoW `WorldStepStageBase`), `HydrologyUpdated{week}` and the other bus notifications (declared in `cow_worldsim.diagamemessages`, published from `PublishCommit`), the save participant `cow.hydro` and the restore half of the codec, the `hydro` debug domain and history layers, the `cow.hydro.*` commands and `cow.hydro_params` pack handling, the F-3 pack participant, and the content data under `Global/Hydrology/` (`hydrology_parameters.json`, presets). `HydrologyCoupling.h` is deleted as an interface file: the structs below replace it.

**Coupling becomes plain data, one way (kit item 4).** "Placement" and "Coupling with A4" read as follows:
- **A4 to A5 (forward, this week)** needs no interface. Hydrology includes Weather directly and reads `waterToGround`, `temperature`, `snowPack` and `glacierInput` from Weather types. The CoW adapter fills the `WaterInput` arrays in bulk at `Capture` from A4's working copy; no per-cell call crosses the package edge inside `Run`. `IWaterInputSource` is dropped.
- **A5 to A4 (feedback, last week)** is `HydrologyInputs { surfaceWetness, openWater, glacierEligible }`, a struct of per-cell arrays **owned by `DraoiHexWorldWeather`**, defaulted from `DraoiHexWorld` (S-1 static water sources, zero ground evaporation, no glacier). `DraoiHexWorldHydrology` supplies `FillHydrologyInputs(const HydrologySimulation&, HydrologyInputs&)`; the adapter calls it at A4's `Capture`, from committed state, so the one-week lag is preserved. `IHydrologyInputsProvider` is dropped.
- **A1 to A5 (feedback, sinks)** is `WaterSinkInputs { etMultiplier, draw }`, owned by `DraoiHexWorldHydrology`, neutral by default (1.0 and 0.0). `DraoiHexWorldEcology` supplies the fill function. `IWaterSinkProvider` and the 8-provider registry become CoW adapter code that multiplies and sums each provider into the one array before `Capture`.
- The **ground-water influence** array (A4 Feature 7) is an optional defaulted `groundWaterInfluence` array on `WaterInput`, filled by the adapter.
- **Spin-up.** `IHydrologySpinUpHost` and the coupled A4+A5 spin-up driver are CoW adapter code on top of `SpinUpSlot`; each sim exposes its own single-sim spin-up as a plain function.

**New open design questions.** (1) The A5-to-A4 `HydrologyInputs` struct is placed in Weather per kit item 4; the A4 amendment must use the same name and fields. (2) The coupled spin-up (A4 + A5 stepped together) fits neither package alone. Recommended: the coupled driver stays in CoW and each package exposes only its own single-sim spin-up; revisit if a second game wants the coupled form (a Hydrology function taking a Weather sim is allowed by the dependency direction).

**Plan.** Re-plan required (see the marker in the plan file).

## Status

**Status:** Approved (2026-11-17, user, after the locked-GDD alignment). Interview done with the user the same day, including the three follow-up questions (Decisions 9–11). Aligned to the locked GDD A5 on 2026-11-17. Six feature specs exist (all `Draft`). Approval is a separate step.

Amended 2026-11-18 (world-sim review decisions).
