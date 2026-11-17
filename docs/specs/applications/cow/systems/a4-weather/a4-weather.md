# System Spec: A4 Weather

**Parent:** @docs/specs/applications/cow/cow.md  
**Design source:** `Output Docs/GDD/08_SystemsDeepDive/A4_Weather/` (DesignDocs repo; `A4_WeatherSystem.md`, `A4_TechnicalSpec.md`, `A4_TuningParameters.md`, `A4_AuthoringDebug.md`, `A4_EdgeCaseMatrix.md`, `A4_ProductionBacklog.md`, all Locked 2026-10-11)

**Gameplay Domains:** simulation, debug

> **Revised 2026-11-13 (dependency audit).** The simulation design is unchanged. What changed around it: DiaDeferredStep (Approved 2026-11-12) now owns `IParallelExecutor` and makes the weekly step **deferred** rather than in-frame; DiaCellLayers, DiaCellHistory and CellField are all **built**; S-1 is **built** and its `Cell` carries a biome **enum**, not a `StringCRC`; SimPU commands must be **sim-hosted with mandatory help**. Sections that merely re-specified built Dia capability (cell inspector, selection commands, the history ring, the executor implementations, a snapshot store, season arithmetic) have been deleted in favour of pointers. Every claim here was checked against code on 2026-11-13.

> **Amended 2026-11-17 (A5 Hydrology; user decisions).** A new system, A5 Hydrology (`../a5-hydrology/a5-hydrology.md`), owns all water on the ground. A4 now stops at "water reaching the ground":
> - `groundWater` leaves `WeatherFields`. A4 outputs `waterToGround` (rain + snowmelt) instead, and the ground-water drain step (§12) moves to A5.
> - A4 reads A5's previous committed week (`surfaceWetness`, `openWater`, `glacierEligible`), latched at `Capture`. With no A5 present A4 falls back to S-1's static `waterSource`.
> - Snow above the 1.0 cap becomes `glacierInput` on A5-flagged cells.
> - Spin-up runs A4 + A5 together when A5 is present. World Step order is `["weather", "hydrology", "ecology"]`.
> - Three new features are added (7 Weather Influence API, 8 Climate Cycles, 9 Smoothing). Not yet specced.
>
> Features 1–6 keep their specs; each is amended in place and marked "(amended 2026-11-17, A5 Hydrology)".

## Summary

A4 simulates CoW's weather as a weekly **cellular automaton** over the world's hex grid. Each cell carries moisture, temperature, pressure, wind, precipitation, cloud cover, snow pack and a per-week `waterToGround` output. Once per game week every cell updates from local rules: solar heating, pressure, wind, evaporation, transport, orographic lift, precipitation and snow. Water that reaches the ground is handed to A5 Hydrology (amended 2026-11-17, A5 Hydrology). Coastal rain, rain shadows, moving storms, spring floods and droughts emerge from those rules rather than being scripted.

A4 owns the **A1 ↔ A4 contract**. It publishes one `WeatherUpdated{week}` notification per week and exposes a read-only per-cell snapshot that A1 (ecology) and V-1 (tile rendering) read. It defines the `IBiomeSeasonalBaseline` query that A1 implements.

A4 also ships the tools for tuning weather on its own: a debug domain with eight cell layers plus a wind overlay, per-cell detail rows, console commands, presets and live parameter tuning. The **S-2.5 Weather Patterns** testbed hosts them as a stage before S-3 Living Ecology exists.

## Goals

1. **Weekly CA update.** On every `SimulationTick` with `band >= TimeBand::Week`, run the GDD's 10-step pipeline (TechnicalSpec §2) over all cells, then commit atomically.
2. **Deterministic.** No RNG. Same world + same parameters + same tick count gives bit-identical state, whatever the game speed, however the grid is iterated, and whether the step ran synchronously or deferred.
3. **The A1/V-1 contract.** `WeatherUpdated{week}` plus a const `WeatherSnapshot` view carrying every field the GDD's §13 payload lists.
4. **Data-driven tuning.** All 20 constants (19 GDD constants plus `cloud_decay`; the ground-water drain moved to A5, amended 2026-11-17) load from `weather_parameters.json`, are validated, hot-reload, and can be overridden live.
5. **Save/load.** Committed weather state round-trips through E1. A reload continues bit-identically, including a step that was in flight when the save was taken.
6. **Observable.** A debug domain with seven scalar base layers plus a wind overlay (nine saved floats per cell, because wind has two components; amended 2026-11-17, A5 Hydrology: `weather.water_to_ground` replaces `weather.ground_water`, and a tenth transient array `glacierInput` is not saved or layered), per-cell detail rows, console commands, and per-step profiling.
7. **Tunable in isolation.**
   - The five presets and the four regression scenarios (AuthoringDebug §5–§6) ship with A4.
   - The S-2.5 testbed hosts the stage that proves the GDD success criteria (`A4_WeatherSystem.md` §Success Criteria).

## Non-Goals

- **Weather narrative events** (`DroughtFrontDetected`, `StormFormed`, `SpringMeltFlood`). These belong to D4. A4's `find_storms` / `find_droughts` are debug queries only.
- **Player-facing weather visuals.** Rain/snow particles, cloud shading and snow overlay belong to V-1, which reads the snapshot. The A4 overlays are debug-only.
- **Ecology response.** A1 consumes `WeatherUpdated`. A4 never writes A1 state.
- **Terrain data.** Elevation, latitude, water source and biome come from S-1 (see Dependencies). A4 doesn't derive or generate them.
- **Water after it reaches the ground** (added 2026-11-17, A5 Hydrology). Soil moisture, ground water, runoff, rivers, lakes, floods and ice belong to A5. A4 stops at `waterToGround` and `glacierInput`.
- **Dragon climate manipulation content** (GDD "fire dragons → warming") (amended 2026-11-17, A5 Hydrology). Supported through the influence API (Feature 7); no dragon content now (there is a backlog review row).
- **Step scheduling, the job pool and the worker cap.** DiaDeferredStep owns all of it (amended 2026-11-13). A4 supplies `Capture` / `Run` / `Apply` and nothing more.
- **The `world` channel and stage ordering.** `WorldStepModule` owns the channel and sequences A4 → A5 → A1 → A3 (amended 2026-11-13; A5 added 2026-11-17, A5 Hydrology). Order is `["weather", "hydrology", "ecology"]`; S-2.5 alone stays `["weather"]`.
- **Cell selection, the cell inspector and layer query commands.** DiaCellLayers owns these (amended 2026-11-13).
- **History storage, scrubbing and snapshots.** DiaCellHistory owns these (amended 2026-11-13).

## Architecture

### Placement

**CoW-Shared** (AD-002). A4 is consumed by A1, V-1, S-2.5 and S-3, and it is CoW-specific content. Pure logic lives in `Cluiche/CoW/Shared/Weather/`. The SimModule, debug domain and save participant live in `Cluiche/CoW/Modules/`.

```
SimPU
  └─ WorldStepModule (SimModule, CoW-Shared, own system spec)
  |    owns the `world` DiaDeferredStep channel; orders stages inside Run
  |    stage 0 → WeatherModule    stage 1 → HydrologyModule (A5, optional; amended 2026-11-17)
  |    stage 2 → EcologyModule (A1, with A3 inline)
  |
  └─ WeatherModule (SimModule)
       on WorldGenerationComplete → WeatherSimulation::Initialise(terrain) → spin-up N weeks
       registers itself as a world-step stage; does NOT kick or schedule
       on Apply (from WorldStepModule) → Broadcast WeatherUpdated{week}
       on BiomeTransitionComplete → update cached biome for the cell (applies next week)
       owns: WeatherSimulation, WeatherParameters, save participant, debug domain, cell layers
```

A4 runs standalone: a stage may register `WeatherModule` as the only world-step stage, which is exactly S-2.5's case. A4's only coupling to A1 is `IBiomeSeasonalBaseline`, which A4 declares and for which it ships a fallback.

### Grid

The hex grid is Dia's, constructed from S-1's world dimensions (amended 2026-11-13: S-1 owns no topology helper — it constructs `Dia::Pathfinding::HexTopology` ad hoc per algorithm, and there is **no neighbour accessor on `WorldStructure` or `Cell`**; A4 does the same).

- `Dia::Pathfinding::HexTopology topo(width, height)` — rectangular **flat-top odd-q**; odd columns shifted half a cell down; `index = y * width + x`.
- Cells are indexed `0..N-1`, each with up to 6 neighbours in a fixed direction order `d = 0..5`.
- Direction `d` sits at on-screen angle `30° + d × 60°`, counter-clockwise (flat-top hexes have no east or west neighbour; `0` NE, `1` N, `2` NW, `3` SW, `4` S, `5` SE). `NeighbourAt(i, d)` returns `Dia::Pathfinding::kNoCell` (`0xFFFFFFFF`) past the world edge.
- Wind angles snap to the nearest direction with `HexTopology::DirectionFromAngle`, `d = round((angle - 30°) / 60°) mod 6`, where the angle is the on-screen counter-clockwise angle in y-down world space. This is the GDD's CH-06 adapted to flat-top hexes; the GDD's east-based wind angle needs the same 30° shift.

Map sizes come from S-1's `WorldSize` (verified against `WorldSizeDimensions`, 2026-11-13): Small 66 × 42 = 2,772 cells, Standard 80 × 52 = 4,160, Large 104 × 64 = 6,656.

**Shared grid description.** DiaCellLayers registration needs a `Dia::Graphics::CellGridDesc`, and S-1, A4 and A1 must all register against the *same* one. Today that desc belongs to `WorldHexGrid`, owned inside S-1's `DIA_DEBUG`-only `WorldGenViewHost`. Ownership of the shared grid is settled in the `WorldStepModule` spec, not here (added 2026-11-13).

### State (structure of arrays)

```cpp
namespace CoW::Weather {

    // Per-cell dynamic state. One float array per field, N cells each.
    struct WeatherFields {
        DynamicArrayC<float> moisture, temperature, pressure;
        DynamicArrayC<float> windDirection, windSpeed;     // radians [0, 2π), [0, 1]
        DynamicArrayC<float> precipitation, cloudCover, waterToGround, snowPack;
        // Amended 2026-11-17 (A5 Hydrology): groundWater is gone. waterToGround is a per-week
        // output (rain + snowmelt reaching the ground this week), zeroed at the start of each step.
    };

    // Amended 2026-11-17 (A5 Hydrology). Per-week hand-off to A5, held with the working copy.
    // Not part of WeatherFields: not saved, recomputed every step.
    struct WeatherHandoff {
        DynamicArrayC<float> glacierInput;    // snow above the 1.0 cap, on glacierEligible cells only
    };

    // Amended 2026-11-17 (A5 Hydrology). A5's previous committed week, latched in Capture.
    // Absent when no A5 is registered (S-2.5 alone): A4 then uses waterSource and zero ground evaporation.
    struct HydrologyInputs {
        DynamicArrayC<float>   surfaceWetness;   // [0,1] feeds §6.2 secondary evaporation
        DynamicArrayC<float>   openWater;        // [0,1] open-water fraction, feeds §6.1 primary evaporation
        DynamicArrayC<uint8_t> glacierEligible;  // 1 = snow above the cap becomes glacierInput
        bool                   present = false;
    };

    // Read-only inputs, captured at Initialise from S-1's Cell array.
    // Amended 2026-11-13: biome is S-1's enum (1 byte), not a StringCRC (4 bytes).
    struct WeatherTerrain {
        DynamicArrayC<float>   elevation;     // [0, 1], Cell::elevation
        DynamicArrayC<float>   latitude;      // [0, 1] normalised, 0 = equator, 1 = pole; Cell::latitude
        DynamicArrayC<uint8_t> waterSource;   // WorldGeneration::WaterSource; Cell::waterSource
        DynamicArrayC<uint8_t> biome;         // WorldGeneration::Biome; Cell::biome, updated by A3
    };
}
```

At Large (6,656 cells) this is 9 floats (36 B) of dynamic state per cell, about 240 KB per copy, plus 14 B/cell of terrain. Amended 2026-11-17 (A5 Hydrology): the working copy also holds `glacierInput` (4 B) and the latched `HydrologyInputs` (9 B) per cell, about 90 KB more; none of it is saved. The GDD's 64 B/cell estimate (at 200 × 200) assumed an array-of-structs layout.

**Terrain ingest (amended 2026-11-13, verified against S-1 code).** All four inputs are plain fields on `WorldGeneration::Cell`, read through `WorldGenerationModule::GetCurrentWorld()` → `WorldStructure::Cells()`, row-major. A4 copies them once at `Initialise` and never holds the pointer. Notes:

- `GetCurrentWorld()` returns `nullptr` until `GetWorldRevision() >= 1`. Readiness is `IsReady(uint32_t revision)` — there is no zero-arg form.
- `Cell::latitude` is **normalised [0, 1]**, not degrees.
- A4 needs none of S-1's generator layers. This matters: `temperature`, `moisture` and `slope` are generator layers rather than `Cell` fields, are absent from `.wld.json`, and read **zero on any imported or `world_file` world**. A4 computes its own temperature and moisture (see Open Design Question 1).

### Deferred execution (amended 2026-11-13)

The weekly step runs through **DiaDeferredStep**: inputs are captured on SimPU at kick key N, the step runs on worker threads against its own working copy, and the result is applied on SimPU at apply key N+k. SimPU never waits for it except when the result is late. A4 does not schedule, submit, wait or cap — it implements the contract and `WorldStepModule` drives it.

A4's committed/working split maps onto the contract directly, which is why the pipeline design survives unchanged:

| `IDeferredStep` | A4 |
|---|---|
| `Capture()` (SimPU) | copy committed fields → working set; latch season, parameters, pending forcings and biome edits |
| `Run(IParallelExecutor&)` (workers) | the 10 pipeline steps over the working set; reads nothing SimPU-owned |
| `Commit()` (SimPU; the stage's `Commit`, called by `WorldStepModule` in `Apply`) | swap working → committed; set the week to `WeekOf(stepDay)` (Feature 3: the week and season come from the step day, not an internal counter); publish `WeatherUpdated`. **No history sampling** — the coordinator signals `HistoryDriverModule` once after every stage has committed |
| `SerializePending` / `DeserializePending` | a completed-but-unapplied working set plus its apply key (DFS-005) |

- **Readers only ever see committed state** (EC-14). `WeatherSnapshot`, the save participant and the debug layers all read committed.
- **`Run` is a pure function of the captured inputs.** After `Capture` returns, `Run` must not read any SimPU-owned state — this is what makes it lock-free (DFS-002).
- **`Dia::DeferredStep::IParallelExecutor`** supplies the range split: `ParallelFor(count, minRange, fn)`. A4 does not define the interface and does not implement `SerialExecutor` or a JobSystem-backed executor; Dia provides both, along with the worker cap, the `TrySubmit` inline fallback and job attribution.
- **The synchronous path stays** as `WeatherRunner` (see Public Interfaces). It is the determinism oracle: a deferred run must be bit-identical to the synchronous run of the same weeks. It is also the simpler path for S-2.5, where weather alone fits inside a sim frame comfortably.
- **Choosing k** is CoW's decision and lives in the `WorldStepModule` spec. At Fastest a week is 7 frames (~233 ms); at Slow it is minutes.

**Bit-identity.** Each range writes only its own cells, reads only the stable copy the pass started from, and gathers inflow in a fixed direction order. So the result does not depend on the range count or on which range finishes first. Grid totals for the panel and metrics are reduced per range and combined in range order after the pass; they never use atomics and never feed back into the simulation.

**Performance (amended 2026-11-13).** The gate is now **CPU time**, not frame time, because the step is off SimPU's critical path. One `Step()` at Large in Release, summed across ranges: **≤ 10 ms of CPU**, with Small ≤ 4.2 ms and Standard ≤ 6.3 ms (scaled by cell count). Separately, the step must complete within its slack — `k` weeks of wall time at the fastest speed preset — or DiaDeferredStep stalls SimPU and reports it (DFS-003). Missing either is reported but doesn't fail the build. A miss means optimising the hot pass or raising k, not spreading the pipeline across frames.

**Spin-up** (52 weeks) runs through `WeatherRunner` as a cancellable **worker job** on the capped executor (amended 2026-11-16, Feature 3): the stage reports not-ready until it finishes, and a newer world cancels it. SimPU never hitches on regeneration.

**Coupled spin-up (amended 2026-11-17, A5 Hydrology).** When A5 is present, the spin-up job runs A4 and A5 **together**, week by week in World Step order (`WeatherRunner` + `HydrologyRunner`), so the first live year is consistent (evaporation feedback, lake levels and baseflow settled). Length is 52 weeks, extended to the longest active climate cycle's settling period when climate cycles are on (Feature 8 sets it). With no A5, A4 spins up alone as before. See `../a5-hydrology/a5-hydrology.md` "Initial state and spin-up".

### Update pipeline

`WeatherSimulation::Step()` runs the TechnicalSpec §3–§12 steps in GDD order. Each step is a **full-grid pass**.

- **Steps that read neighbours read a stable copy.** These are wind (§5, the pressure gradient), moisture and temperature transport (§7, §8) and orographic lift (§9). Each reads the fields as they stood at the end of the previous step, never values the current step has partly written. Local steps update in place. This is how the GDD's "synchronous update" (§14 invariant 2) is met when steps depend on earlier steps' results.
- **Transport is a gather.** Pass A computes each cell's outflow and downwind direction. Pass B sets each cell's inflow to the sum of the outflows aimed at it, summed in direction order 0..5. The result is independent of iteration order, which is what makes parallel execution deterministic. Outflow toward `kNoCell` is skipped, so the moisture or heat stays in the cell (EC-16, CH-10).
- **Upwind edge inflow.** When a cell's upwind neighbour is `kNoCell`, the cell receives an inflow equal to its own outflow, as if the world past the edge had the same weather (a zero-gradient boundary). Without this, upwind edge cells would lose moisture and heat every week and get nothing back. With steady prevailing winds that leaves fake cold, dry strips along the map edges. The `boundary.edge_inflow` parameter (default `true`) switches it off so S-2.5 can show the GDD-literal behaviour side by side.
- **Precomputed directions.** The unit vector for each hex direction is a constant table. The wind step's pressure gradient (§5.2) uses it instead of computing `cos`/`sin` per neighbour.
- **Water hand-off to A5 (amended 2026-11-17, A5 Hydrology).** Precipitation (§10) and snow melt (§11.2) add into `waterToGround`, not `groundWater`. The ground-water drain step (§12) is deleted from A4; the `ground_water_drain_rate` / `ground_water.drain_rate` parameter moves to A5. Evaporation (§6) reads A5's latched values: `openWater` for primary evaporation (lake extent now varies) and `surfaceWetness` for secondary evaporation (was ground water). With no A5 present, §6.1 uses S-1's static `waterSource` and §6.2 is zero. `waterToGround` is a per-week amount, not a store: it is clamped to [0, 2] (rain up to 1 plus melt up to 1).
- **Clamping.** Precipitation amounts are clamped non-negative (CH-09). Snow pack is capped at 1.0 (CH-04). **Amended 2026-11-17 (A5 Hydrology):** snow above the cap is no longer discarded everywhere. On cells A5 flags `glacierEligible` it is output as `glacierInput`; elsewhere it is still discarded. A4 still owns `snowPack` and A5 never writes it. The EC-12 ground-water cap is deleted with the store. Temperature after transport is clamped non-negative (CH-07). After the last step every field is clamped to its range (EC-20).
- **Season.** A4 owns the named-season enum (there is none in the engine or in CoW today). The week and season come from GameSpeedController's existing helpers (amended 2026-11-13): `WeekOf(day)`, `SeasonOf(day)`, `kWeeksPerSeason = 13`, `kSeasonsPerYear = 4` in `CoW/Shared/GameSpeedController.h`. A4 does **not** re-derive the 13-week arithmetic. There is no `SeasonAdvanced` message.

### Biome seasonal baseline (A4-owned contract, A1 implements)

```cpp
namespace CoW::Weather {
    enum class Season : uint8_t { Spring, Summer, Autumn, Winter };   // A4 owns this; engine has none

    class IBiomeSeasonalBaseline {
    public:
        virtual ~IBiomeSeasonalBaseline() = default;
        // Baseline air temperature [0, 1] for the biome in the season.
        // Must return a value for all 13 biomes: Ocean and Lake included, and the three
        // A3 succession outputs, which A4 receives via BiomeTransitionComplete.
        // An unknown biome returns 0.5 (EC-19). A4 asserts the result is in range.
        virtual float GetSeasonalTemperature(WorldGeneration::Biome biome, Season season) const = 0;
    };

    // Default implementation until A1 exists: a table from weather_parameters.json.
    // Used by S-2.5 and by tests.
    class TableBiomeSeasonalBaseline final : public IBiomeSeasonalBaseline { /* ... */ };
}
```

**Amended 2026-11-13:** keyed on the shared `Biome` enum, not a `StringCRC`. **One biome list** serves S-1, A4, A1 and A3 (user decision 2026-11-13: the systems should feel cohesive, and one list means one palette, one name table and no mapping call at every boundary). S-1's enum is widened from 10 to **13** by appending A3's three succession outputs:

| Subset | Values |
|---|---|
| Generated by S-1 classification (indices 0–9) | `Ocean, Lake, GrasslandSteppe, TemperateForest, WetlandDelta, DesertBasin, AlpineMountain, TundraBoreal, Rainforest, VolcanicAshland` |
| Produced only by A3 succession (indices 10–12) | `Scrub, BurnedAshland, DesertifiedPlain` |

Appending rather than renumbering keeps every serialized `biomeIndex` value and `.wld.json` name valid, and leaves `BiomeClassify` untouched — it simply never emits 10–12, guarded by `kGeneratedBiomeCount = 10` and `IsGeneratedBiome(Biome)`. A4 holds all 13, because `BiomeTransitionComplete` delivers succession biomes S-1 cannot generate. The baseline is a flat `float[13][4]` lookup, and `baseline_table` is keyed by `BiomeId` strings (`Biome.Grassland`, `Biome.Desert`, `Biome.Scrub` … — note these are **not** the enum names) resolved through `BiomeFromId` at load. **This needs the S-1 amendment below before A4 Feature 1 is built.**

`WeatherModule` takes the provider via `ModuleRef` to whichever module implements it (A1's `EcologyModule` once it exists). If none is found it falls back to `TableBiomeSeasonalBaseline` and reports Degraded health.

### Output to A1 and V-1

| Direction | Message | Payload |
|---|---|---|
| Publishes | `WeatherUpdated` | `uint64_t week; Season season;`, sent from `Apply()` |
| Subscribes | `SimulationTick` | gate `band >= TimeBand::Week` |
| Subscribes | `WorldGenerationComplete` | `{seed, retryAttempts, worldRevision}` — initialise and spin up |
| Subscribes | `WorldGenerationFailed` | `{reason, seed, attempts, message}` — do not initialise; report Unhealthy (added 2026-11-13) |
| Subscribes | `BiomeTransitionComplete` | `{cell, newBiome}` from A3, takes effect from the next week. Not subscribed until A3 publishes it |

**Gate correctness (verified 2026-11-13).** `TimeBand` is `{Day, Week, Season, Year, Decade, Century}` ordered smallest to largest, and `ResolveBand(day)` returns the **largest** band the day completes — day 91 reports `Season`, not `Week`. So the gate must be `band >= TimeBand::Week`; `== TimeBand::Week` would silently skip four weeks a year. The `SimulationTick` bus queue holds 16, so A4 must drain promptly.

**Registration order (added 2026-11-13).** `WorldGenerationModule` exposes `HoldGeneration()` / `ReleaseGeneration()` for dependants that must be subscribed before the first world lands. `WeatherModule` calls `HoldGeneration()` in `DoStart` and releases once subscribed.

Per-cell data is read through the snapshot, never sent through the bus (bus payloads are small POD):

```cpp
namespace CoW::Weather {
    enum class PrecipitationType : uint8_t { None, Rain, Snow };

    class WeatherSnapshot {          // const view over committed state; valid until the next Apply()
    public:
        uint32_t GetCellCount() const;
        uint64_t GetWeek() const;

        // A1 fields (GDD §13)
        float Rainfall(uint32_t cell) const;        // precipitation only (amended 2026-11-17, A5 Hydrology: the groundWater·0.1 term is dropped; A1 reads soil moisture from A5)
        float WaterToGround(uint32_t cell) const;   // added 2026-11-17: rain + snowmelt reaching the ground this week (A5 reads the working copy; this is the committed view)
        float Temperature(uint32_t cell) const;
        bool  SnowCover(uint32_t cell) const;       // snowPack > kSnowCoverThreshold (0.1)

        // V-1 fields (GDD §13)
        float CloudCover(uint32_t cell) const;
        float PrecipitationIntensity(uint32_t cell) const;
        PrecipitationType GetPrecipitationType(uint32_t cell) const;   // None if intensity == 0
        float WindDirection(uint32_t cell) const;
        float SnowPack(uint32_t cell) const;

        const WeatherFields& GetFields() const;     // raw access: debug, save, bulk readers
    };
}
```

Same access rule as GameSpeedController: subscribers read the snapshot **in response to `WeatherUpdated`** and do not poll it on other frames. A1 accumulates `Rainfall` over the 13 weeks of each season (A1 §13).

### Initial state and spin-up

On `WorldGenerationComplete`:

1. Read terrain from S-1. **Assert at least one water-source cell** (EC-01, `Cell::waterSource != WaterSource::None`). If none, log an error, report Unhealthy health, and don't run.
2. Seed each cell:
   - temperature: the seasonal baseline for its biome in Spring, with the latitude modifier applied
   - moisture: `initial_moisture_water` on water-source cells, `initial_moisture_land` elsewhere
   - pressure: 0.5
   - all other fields: 0
3. **Spin up.** Run `spinup_weeks` steps silently, off SimPU as a cancellable job (Feature 3), (default 52, set in the module config). No `WeatherUpdated` is sent, and the week counter resets to 0 afterwards. The first published week has settled winds and moisture. Amended 2026-11-17 (A5 Hydrology): with A5 present this is the coupled A4 + A5 spin-up described above, and `waterToGround` starts at 0 like every other output.

Spin-up is deterministic, so a save does not need to know about it. A new game rerunning it gives the same result.

### Parameters

The tuning constants live in `Assets/CoW/Global/Weather/weather_parameters.json`, using the GDD TuningParameters §14 schema with envelope `"format": "cow.weather_params", "version": 1`. The schema is extended with an `init` block holding `initial_moisture_water` and `initial_moisture_land`, a `baseline_table` block for the default `TableBiomeSeasonalBaseline`, and a `boundary` block for `edge_inflow`. The `.diaapp` module config gives the file path and `spinup_weeks`:

```json
{
    "instance_id": "WeatherModule",
    "type_id": "WeatherModule",
    "stages": ["S25WeatherPatternsStage"],
    "dependencies": ["MessageBusHostModule", "WorldStepModule", "WorldGenerationModule", "SimQueryHostModule"],
    "soft_dependencies": ["SaveLoadModule", "CellLayersModule", "CellHistoryModule"],
    "config": {
        "parameters": "Global/Weather/weather_parameters.json",
        "spinup_weeks": 52
    }
}
```

`weather_job_ranges` and `history_weeks` are **removed** (amended 2026-11-13): range count and the worker cap belong to DiaDeferredStep's `.diaapp` config, and history belongs to DiaCellHistory's. `SaveLoadModule` is a soft dependency: when present, weather state is saved; without it, A4 still runs but doesn't participate in saves (this is the S-2.5 case).

- **Validation on load.** Every rate must be in `[0, 1]` and `snow_threshold <= melt_threshold`. A missing or invalid file logs an error, falls back to the compiled defaults (the GDD values), and reports Degraded health. This is the same rule as GameSpeedController.
- **Hot reload.** `cow.weather.reload` re-reads the file between steps. `cow.weather.set_param <dotted.key> <value>` overrides one value until the next reload. Both are latched at `Capture()`, never applied mid-`Run`.
- **Tuned parameters are saved as weather packs** (F-3 World Assets). `cow.world.pack.save_as weather <id>` writes overrides-only to `Assets/CoW/Global/Weather/Packs/<id>.weather.json` via the `source` alias. Parameters in the defaults file are not saved; packs are the normal tuning path.

### History (DiaCellHistory)

A4 has **no history buffer of its own** (amended 2026-11-13; the interim `WeatherHistory` ring and `cow.weather.step_back` are deleted — this was already the plan of record). Recording, scrubbing, snapshots and step-back all come from DiaCellHistory, which is built. A4's obligations:

- **A4 does not sample** (amended 2026-11-13, World Step Approved). CoW-Shared `HistoryDriverModule` owns `Sample()` and is signalled by `WorldStepModule` once after every stage has committed, so a world week produces exactly one sample at the correct apply key with a consistent layer set. A4 only registers its layers and emits its events.
- **Declare tracked layers in the module's `.diaapp` `config`**, not a `.diastage` — the `history` block is read by `CellHistoryModule::OnConfigure` and is capped at **255 characters**, so the list must stay short. S-2.5 declares:
  ```json
  "config": { "history": { "bands": ["Week"], "track": ["weather.precipitation", "weather.temperature", "weather.water_to_ground"], "cellBudgetMB": 16 } }
  ```
- **Give every tracked float layer a schema `min`/`max`**, or tracking is refused with an Error. Values are quantised to Float16 over `[min, max]`.
- **Budget awareness.** The scrub window is `budgetBytes / bytesPerSample`, so every extra tracked layer shortens the window for all of them. Ring cap is 64 layers; budget defaults are 64/64/16 MB.
- **Snapshots reuse E1's store.** `SaveLoadModule::GetSnapshotStore()` already returns a `Dia::CellHistory::ISnapshotStore` backed by `SnapshotStoreAdapter`. A4 implements no store. Take/restore are driven by `SnapshotManager` at a frame boundary.
- **Events.** Droughts and storms are emitted as `CellHistoryRecorder::Emit(HistoryEvent{type, group, cell, payloadJson, text})`.
  - `HistoryEvent` carries **one anchor `cell` and no cell-count field**, so A4's affected-cell count goes in `payloadJson` (compact JSON, ≤ 1 KB).
  - Events stay **pending until the next `Sample()`** commits them. A4 `Emit`s during its `Commit`, and `HistoryDriverModule` samples after every stage has committed, so ordering is satisfied by construction — but it means an event A4 emits is committed by the driver's sample, not by any call of A4's own.
  - Each event type needs a `kind: "event"` ValueSchema entry with its payload field list, or the recorder logs a warning per type in Debug builds.

**Not built, by decision:** DiaCellHistory's in-game panel (snapshot controls, delta banner, group table) was deliberately left out and re-homed to the CluicheEditor Inspector as T-1 Feature 7. A4 Feature 5 therefore has **no in-game history UI to host**; the live card shows the current week only, and scrubbing is reached through `cells.history.*` commands or T-1.

### Save/load (E1)

`WeatherSaveParticipant` (`Dia::SaveGame::ISaveable`, version 1) follows the `GameSpeedSaveParticipant` pattern. It is registered with `SaveLoadModule::RegisterParticipant(StringCRC("weather"), &participant)` — the id is a **registration argument, not a member of `ISaveable`** (amended 2026-11-13), and registration order is serialization order.

The real interface (verified 2026-11-13):

```cpp
void     Serialize  (SaveContext& ctx) const override;
void     Deserialize(LoadContext& ctx)       override;
uint32_t GetVersion () const                 override;    // not `version`
bool     Validate   (LoadContext& ctx) const override;    // optional; default returns true
```

- **Serialize** writes the format `version`, `ready`, `width`/`height`/`cell_count`, the applied `week` (derived from the step day), a 64-bit `params_hash` of the effective parameters, nine committed `WeatherFields` arrays as JSON float arrays and the A4-cached `uint8_t` biome array (amended 2026-11-16, Feature 4). Terrain is not saved; it comes back from S-1's world on load. `ready: false` is a valid no-op record.
- **Pending deferred step (added 2026-11-13, DFS-005).** If a step has completed but not yet been applied when the save is taken, `SerializePending` writes the working set and its apply key alongside. On load it is restored and applied at that key, so a save taken mid-step reloads identically to the uninterrupted run. A4 writes **no** save code for this beyond `SerializePending`/`DeserializePending` on its step: DiaDeferredStep Feature 3 drains automatically on `SaveManager::OnSaveStarted` and owns the single `ISaveable` holding every channel's pending result, so **E1 needs no change** (corrected 2026-11-13; the planned "E1 calls `DrainAll`" amendment was dropped). The coordinator delegates to A4's step in declared order.
- **Validate** checks internal consistency only (lengths, ranges, version), because the live world is still the old one. A dimension mismatch against the loaded world is handled in `Deserialize` by discarding the saved state and spinning up fresh. A `params_hash` mismatch warns and runs under the current tuning (Feature 4).
- **Deserialize** restores everything into committed state.

**Size.** A4's 9 float fields as JSON doubles (`%.17g`, bit-identical round-trip) at ~30 bytes per float: Small ≈ 0.8 MB, Standard ≈ 1.2 MB, Large ≈ 1.9 MB. Combined with A1's ~35 fields/cell the total ecology + weather save is projected at ~10–15 MB at Large, well inside `SaveContext::kBufferSize` (verified 2026-11-13: exactly 100 MB). The 8-slot cap limits total disk use to 800 MB.

E1 saves at the sim-frame boundary and `Apply()` runs on SimPU, so a save can never see a half-updated grid and the GDD's `is_update_in_progress()` flag isn't needed. `SaveLoadModule::CaptureState()` (a dry-run capture with no disk I/O) is available for A4's round-trip tests.

### Debug and observability

`WeatherDebugDomain` (`Dia::VisualDebugger::IDebugDomain`) is registered with `VisualDebuggerModule::RegisterDomain(domain)` in Debug builds, which also registers its drawers with `DebugLayerManager` (amended 2026-11-13: the container is `DiaDebugDomainRegistry`; there is no `ModuleDebugRegistry`, and `DebugLayerManager::BroadcastLayerState` has been deleted). Interface constraints: `GetDescription()` must be ≤ 80 characters and `GetAccentColour()` must return a `DebugGroupAccents` constant, never a literal.

**Panel card:**
- shows the week and season, cell count, grid totals (mean moisture, total snow pack, number of cells raining), parameter source (file or fallback), baseline provider (A1 or table), and the last step's CPU time and slack
- has overlay toggles
- posts commands, never mutates state directly (UI/business separation)

**Cell layers (DiaCellLayers, built).** Registration is `CellLayerRegistry::Register(CellLayerDesc)` → `CellRegisterResult{Ok, Duplicate, CapacityReached, WrongProcessingUnit, NullGrid}`. A4 reaches the registry through a `ServiceStreamReader<CellLayerRegistry>` on `"CellLayerRegistryService"`, resolved with `FindCellLayerRegistry()` in `DoUpdate` or later. **Registration must happen on the owning PU (SimPU) or it is rejected** and flips the module's health to Degraded.

- Seven scalar fields are **base layers** (one at a time), supplied by a **pull callback**: `read(float* dest, uint32_t count)` plus a `version()` change counter that gates re-reads. A4 does *not* implement `IViewValueSource` — that is DiaCellHistory's replay seam.
- **Canonical layer ids:** `weather.moisture`, `weather.pressure`, `weather.temperature`, `weather.precipitation`, `weather.snow_pack`, `weather.cloud_cover`, `weather.water_to_ground`. Shared with S-2.5's scenario loader, which rejects unknown names. **Amended 2026-11-17 (A5 Hydrology):** `weather.ground_water` is removed; ground water is A5's `hydro.ground`. `weather.water_to_ground` takes its place, so the count stays seven. `glacierInput` has no layer.
- Labels, ramps and hotkeys come from DiaValueSchema via `schemaKey = StringCRC("<for>/<key>")`. A4 owns its layers' `ui.hotkey` values and follows the S-2.5 GDD hotkey table (Snow = **K**); a clash logs a warning and the later layer gets none. Float layers need `min`/`max` for both the legend and history tracking.
- **Wind overlay (amended 2026-11-13).** There is **no vector layer kind** — `CellValueKind` is `{Float, Category}` and `read` is `float*`, one scalar per cell. Wind is registered as a `CellOverlayDesc` whose `draw(IDebugDraw&, const void* selection)` callback composes each arrow by hand, because **`IDebugDraw` has no 2D arrow primitive** (`RequestDrawArrow3D` is 3D-only): a line or ray plus an explicit head. One arrow per cell at Large is ~20k primitives per frame, so the overlay **decimates by default** (every Nth cell, exposed as a panel control) and respects `DroppedCount()`.
- **Per-cell detail rows** are registered with `CellLayerRegistry::Details()` (a `CellDetailBuilder` provider, 64 rows max, invoked only when exactly one cell is selected), plus a hover-extra fn for tooltip lines. A4 shows its fields and the derived values: seasonal baseline, prevailing direction, pressure perturbation, upwind elevation. **A4 does not build a cell inspector or any selection commands** — DiaCellLayers owns `cells.select.*`, `cells.query.*` and `cells.inspect`.

Layers and overlays are hosted wherever the domain is registered: S-2.5 first, then S-3.

**Console commands.** All `cow.weather.*` commands are **sim-hosted** via `SimQueryHostModule::RegisterCommand(name, SimHelpInfo, handler)` (amended 2026-11-13). This is not optional: `QueryRegistry` has an owner-thread veto, so a SimPU module cannot register there, and a DiaAPI command's handler runs on the caller's thread, which would race SimPU state. Consequences:

- **A description is mandatory** (non-empty, ≤ 256 chars); registration fails and logs an Error without one. `category` is `"weather"`, `owner` is `"WeatherModule"`, and arguments are declared as `Dia::API::ValueParam`s (≤ 16).
- **Arguments are JSON objects.** No positional forms.
- **Name budget.** A4 registers **7** names (amended 2026-11-16). The cap is 64, enforced in three tables that must all hold (`SimQueryHost::kMaxHandlers`, `SimQueryHostModule::kMaxAnnounced`, `DebugServer::kMaxSimNames`); the 65th registration returns false and logs an Error, with no assert. It is **per-stage in practice** — names are unregistered on stage exit and an outgoing stage is fully down before the incoming one starts — so A4's own stages are safe (S-2.5 is well under). The problem is S-3.5, which hosts every layer at once and projects to **≥99**. A4 does not shrink to fit; the cap is raised. See Open Design Question 3.
- All mutations are queued and latched at the next `Capture()`.
- The commands are a typed command model (`WeatherCommandExecutor`) bound to a `WeatherSimulation&`, so a scenario's setup commands mean the same thing live and headless.

**Seven names (consolidated 2026-11-16, Feature 5; was sixteen).** `cow.weather.force` (storm / high_pressure / rain), `cow.weather.wind` (set / set_region / clear), `cow.weather.params` (reload / set), `cow.weather.state` (export / import), `cow.weather.load_preset`, and the queries `cow.weather.find` (storms / droughts / rain_shadow) and `cow.weather.info` (profile / memory). Each takes a JSON object with an `action` field and queues its mutation to the next `Capture`. Droughts are computed from the DiaCellHistory precipitation ring, not from A4 state. The wind overlay is a 4 × 4 block plan refined where wind varies or pressure is below 0.3, capped at 2,000 arrows. See `weather-debug.md`.

**Dropped (owned elsewhere, 2026-11-13):** `inspect <cell>` → DiaCellLayers `cells.inspect`; `step_back` → DiaCellHistory `cells.history.view` and `cells.snapshot.restore`.

**Regions** for `set_wind_region` and `find_droughts` come from S-1's `RegionTable` (`RegionRecord{id, zone, landmass, isIsland, primaryBiome, purity, cellCount, centreCell, neighbours}`, `kMaxRegions = 64`) and `Cell::region` (`RegionId`, 0 = ocean or unassigned).

Forcings and overrides are deterministic inputs applied at a fixed point in the pipeline (EC-15). They are **not saved**: they are debug tools.

**Instrumentation:**
- traces and profiling: `DIA_TRACE_ZONE` on the step, `DIA_PROFILE_SCOPE` per pipeline step (backs `profile`). Worker-side scopes carry the `world` channel as subject via DiaDeferredStep's job attribution.
- metrics: `cow.weather.step_ms` **registered with `MetricInfo`** (required by S-3.5 R5, added 2026-11-13), plus raining-cell count and mean moisture.
- events: droughts and storms as DiaCellHistory events (see History).
- health (`IHealthReporter`):
  - Degraded: fallback parameters or fallback baseline
  - Unhealthy: no water sources, or world generation failed

**Logging:** channel `WTH`.
- **Info:** parameter file load/reload success (path, source: file or fallback), preset loaded, reset, baseline provider (A1 or table)
- **Warning:** parameter out of range (clamped, key, value), unknown overlay id (scenario loader), unknown preset name, debug-draw primitives dropped
- **Error:** parameter file not found or parse failed (path), no water-source cells (EC-01), baseline provider returned out-of-range temperature, a command failed to register

### Presets and regression scenarios

A4 provides the reusable pieces that a testbed needs. It does not own the stage.

- **Presets.** The five AuthoringDebug §5 presets are `coastal_storm`, `rain_shadow`, `winter_snow`, `spring_melt` and `drought`: whether each is a weather pack (parameter overrides) or a scenario (forced state) is decided in A4 Feature 6 (see F-3 Open Design Question 1).
  - Each preset is a data file: a starting committed state, forcings, and a season, over a fixed preset world. That world is S-2.5's `s25_coast_range` (an S-1 preset + seed).
  - `cow.weather.load_preset <name>` applies one at the next capture boundary.
- **Regression scenarios.** The four AuthoringDebug §6.1–6.4 scenarios are GoogleTests over hand-built hex terrains. Each terrain has a coast, an inland region and a mountain range. The tests use the GDD's success ratios:
  - coastal precipitation > 1.5× inland
  - windward > 2× leeward
  - more than 70% of mountain cells have snow
  - less than 30% of the snow remains after spring

### Testbed host: S-2.5 Weather Patterns

A4 in isolation is proven in **S-2.5 Weather Patterns**, a separate testbed, which registers `WeatherModule` as the only world-step stage. S-2.5 owns the stage, the preset and overlay panels, click-to-inspect, and the GDD's nine tests as numeric checks run headless for the verdict and live as check lines.

S-2.5 uses A4's debug domain, presets and commands as they are. Its spec asked for three A4 changes, applied here on 2026-10-18: the pure `WeatherRunner` driver, the command executor binding to a `WeatherSimulation&`, and `s25_coast_range` as the preset world. **S-2.5 needs amending in turn (2026-11-13):** its weekly tick budget (`s25-weatherpatterns.md`, "the slowest `Step()`") is written as an in-frame figure and must become CPU-ms plus slack, and its step-back controls now come from DiaCellHistory.

## Public Interfaces

```cpp
namespace CoW::Weather {

    // Pure logic, no Dia module dependencies. Unit-tested directly.
    class WeatherSimulation {
    public:
        bool Initialise(const WeatherTerrain& terrain, const WeatherParameters& params,
                        const IBiomeSeasonalBaseline& baseline);        // false if no water source
        void Step(Season season, Dia::DeferredStep::IParallelExecutor& executor);  // one week, synchronous
        void SetParameters(const WeatherParameters& params);            // effective next capture
        void SetBiome(uint32_t cell, WorldGeneration::Biome biome);     // A3; effective next capture
        void AddForcing(const WeatherForcing& forcing);                 // debug; effective next capture
        WeatherSnapshot GetSnapshot() const;
        const WeatherStepProfile& GetLastProfile() const;
        void RestoreCommitted(uint64_t week, const WeatherFields& fields);
    };

    struct WeatherParameters { /* GDD §14 groups + init + baseline_table + boundary */ };
    bool LoadWeatherParameters(const Json::Value& root, WeatherParameters& out);   // false = invalid; out = defaults
    WeatherParameters DefaultWeatherParameters();

    // Pure synchronous driver: initialise + spin-up, then one Step per week with the season
    // taken from the week. Shared by WeatherModule's spin-up, S-2.5's headless runner, and the
    // determinism oracle (a deferred run must match this bit-for-bit).
    class WeatherRunner {
    public:
        bool Start(const WeatherTerrain& terrain, const WeatherParameters& params,
                   const IBiomeSeasonalBaseline& baseline, uint32_t spinupWeeks,
                   Dia::DeferredStep::IParallelExecutor& executor,
                   uint64_t startWeekOffset = 0);   // calendar start (S-2.5 Feature 3, 2026-11-16): preset runs start at the preset's season
        void StepWeek(Dia::DeferredStep::IParallelExecutor& executor);
        WeatherSimulation& GetSimulation();
    };

    // The deferred-step contract. Registered as a stage with WorldStepModule, which owns the channel.
    class WeatherStage final : public CoW::WorldStep::IWorldStepStage {   // Feature 3; was IDeferredStep
    public:
        bool IsReady() const override;
        void Capture(StepKey stepDay) override;                         // SimPU
        void Run(Dia::DeferredStep::IParallelExecutor&) override;        // workers, working copy only
        void Commit() override;                                         // SimPU
        void SerializePending(SaveGame::SaveContext&) const override;
        void DeserializePending(SaveGame::LoadContext&) override;
    };

    // SimModule wiring.
    class WeatherModule : public Dia::ApplicationFlow::SimModule {
    public:
        WeatherSnapshot GetSnapshot() const;        // A1, V-1, debug: read on WeatherUpdated
        const WeatherSimulation& GetSimulation() const;
        // F-3 World Assets: the module owns a `WeatherPackParticipant` (kind `weather`,
        // defaults `cow.weather_params`) and exposes it by reference; it does not inherit the
        // interface. Apply validates and swaps mParams (effective next week); Initialise uses
        // mParams, never the file; IsReady(rev) is true after Initialise AND spin-up for that revision.
        IWorldAssetParticipant& GetWeatherPackParticipant();
    };
}
```

Messages (`WeatherUpdated`, `BiomeTransitionComplete`) are hand-written structs following the `GameSpeedMessages.h` pattern, in `Cluiche/CoW/Modules/WeatherMessages.h`.

## Features

| # | Feature | Description | Spec | Status |
|---|---------|-------------|------|--------|
| 1 | Weather Data & Parameters | `WeatherFields`/`WeatherTerrain` SoA, `WeatherParameters` + JSON load/validate/defaults, `IBiomeSeasonalBaseline` + `TableBiomeSeasonalBaseline` over the 10-biome enum, `Initialise` (terrain ingest, seeding, EC-01 assert). Pure logic, TDD | @docs/specs/applications/cow/systems/a4-weather/weather-data.md | Approved |
| 2 | CA Pipeline | The 10 GDD steps + clamping as full-grid passes, committed/working swap, gather transport, upwind edge inflow, precomputed hex directions, forcings/overrides. One test group per step plus EC-03 to EC-20, serial-vs-parallel bit-identity, CPU-time gate. Consumes `Dia::DeferredStep::IParallelExecutor`. Pure logic, TDD | @docs/specs/applications/cow/systems/a4-weather/ca-pipeline.md | Approved |
| 3 | WeatherModule & Deferred Step | SimModule: `.diaapp` config, parameter file load and hot reload, `WeatherStep` (`Capture`/`Run`/`Apply`), registration as a `WorldStepModule` stage, `SimulationTick` week gate, `HoldGeneration` + `WorldGenerationComplete`/`Failed`, spin-up, baseline provider lookup and fallback, `WeatherUpdated` publish, snapshot access, `BiomeTransitionComplete`, `cow_main.diaapp` entry, deletion of `JobSystemPlaceholderConsumerModule` | @docs/specs/applications/cow/systems/a4-weather/weather-module.md | Approved |
| 4 | Weather Save/Load | `WeatherSaveParticipant`: JSON array serialize/deserialize of committed `WeatherFields` (9 float arrays) + `uint8_t` biome array; `SerializePending`/`DeserializePending` for an unapplied step (DFS-005); size-checking `Validate`; save/continue/load/continue round-trip test including a save taken mid-step | @docs/specs/applications/cow/systems/a4-weather/weather-save.md | Approved |
| 5 | Weather Debug | `WeatherDebugDomain` panel card, 7 base cell layers + the decimated wind overlay, `CellDetailRegistry` rows + hover extras, 7 sim-hosted `cow.weather.*` names (consolidated 2026-11-16) with help metadata, DiaCellHistory events, profiling, metrics, health | @docs/specs/applications/cow/systems/a4-weather/weather-debug.md | Approved |
| 6 | Presets & Regression | The 5 preset data files + `load_preset`, and the 4 AuthoringDebug §6 scenarios as GoogleTests over hand-built hex terrains (GDD ratios), with layer goldens | @docs/specs/applications/cow/systems/a4-weather/presets-and-regression.md | Approved |
| 7 | Weather Influence API | Public, deterministic way for any CoW system to influence weather: storm, warmer, cooler, wetter, drier. Each has a source, a region (cell set or centre + radius), strength, lifetime and falloff, and is saved with the game. Generalises the CA forcing hooks the debug `spawn_storm` uses. A5 applies the ground-water part of wetter/drier. A1's GDD `water_dragon_emission` moves onto this path. Dragon abilities are enabled but there is no content now (backlog review row). Added 2026-11-17 | TBD | Draft |
| 8 | Climate Cycles | Data-driven layered cycles: period, per-field amplitude, optional regional pattern, phase. Deterministic functions of the game day. Targets a ~4-year El Niño-style oscillation and a ~4-decade ice-age cycle. Phases are saved. Presets can switch cycles off or pin a phase. Extends spin-up length. Added 2026-11-17 | TBD | Draft |
| 9 | Smoothing | The CA stays weekly. A presentation layer interpolates across the week for V-1 and gameplay reads, plus an immediate visual-only preview of a new influence (the deferred step applies at N+7). It never feeds back into the CA. No forecasting. Added 2026-11-17 | TBD | Draft |

**Features 1–6 are amended (2026-11-17, A5 Hydrology)** for the ground-water hand-off, the A5 inputs, the snow-cap hand-off and coupled spin-up. Features 7–9 are new and not yet specced; they build after Features 1–3 and 6.

**Routed elsewhere:** the stage, panels, click-to-inspect and success-criteria walkthrough go to the S-2.5 Weather Patterns testbed. The `world` channel, stage ordering and the choice of k go to `WorldStepModule`.

**Build order (revised 2026-11-16):** 1 can start now on hand-built terrain. 6's loader, selectors and fills need only 1, but its forcings and every regression test need 2 built. 2 follows DiaDeferredStep Feature 2 (the executor interface). 5 is largely unblocked — DiaCellLayers, DiaCellHistory and CellField are all built — and needs only Feature 1's fields to read. 3 needs `WorldStepModule` and S-1. 4 needs DiaDeferredStep Feature 3 for the pending-result rule.

- Features 1, 2 and 6 need no S-1 code: their tests build `WeatherTerrain` by hand.
- Features 3–5 need S-1 (built) and the engine dependencies below.

## Binding Decisions

| ID | Decision | How it applies |
|----|----------|----------------|
| PD-002 | ProcessingUnit/Phase/Module architecture | `WeatherModule` is a SimModule on SimPU. No singletons; providers come in through `ModuleRef` or ServiceStreams |
| PD-004 | No STL containers in public APIs | `WeatherFields`/`WeatherTerrain` use `DynamicArrayC`. The snapshot exposes accessors, not `std::` types |
| PD-005 | x64 is the only build target | Saves are JSON text, little-endian floats stored as doubles (jsoncpp `%.17g`) |
| AD-001 | Ask permission before Dia edits | A4 adds no Dia code. The capabilities it uses are all built or separately approved: `JobSystemService` + `TrySubmit` (dia-needs #12, built 2026-10-15), CellField (#14, built), DiaCellLayers, DiaCellHistory, DiaValueSchema (all built), DiaDeferredStep (Approved 2026-11-12, `IParallelExecutor` sign-off given). `WorldStepModule` is CoW-Shared, not Dia |
| AD-002 | Four-tier dependency model | CoW-Shared, because A1, V-1, S-2.5 and S-3 all consume it |
| AD-003 | GDD owns design intent | GDD A4 owns the mechanics and constants. Deviations are listed below for the GDD owner |
| AD-005 | One stage per testbed | A4 is a System and owns no stage. The S-2.5 testbed is the single stage that hosts it |
| DFS-002 | `Run` reads only the captured working copy | `Capture()` latches terrain, parameters, season, forcings and biome edits. Enforced by a test that poisons the source after capture |
| DFS-003 | A late step stalls SimPU at its apply key; the key never slips | A4's CPU budget must leave slack inside k weeks of wall time at the fastest preset |
| DFS-005 | Saves drain, then store pending results with their apply keys | Feature 4 implements `SerializePending`/`DeserializePending` |
| SD-PERF-7 | Telemetry cost budgets | `cow.weather.step_ms` is one gauge per applied step, written on SimPU; no per-cell telemetry leaves SimPU |

## Deviations from GDD A4

These are for the DesignDocs owner to fold back into A4. Numbering is stable; 13 is restated and 14 is new.

1. **`WeatherUpdated` is a notification, not a per-cell payload.** It is one `{week, season}` broadcast. The §13 per-cell fields are read through `WeatherSnapshot` because bus payloads are small POD. The fields and formulas are unchanged.
2. **`SeasonAdvanced` is replaced by `SimulationTick`.** A4 steps on `band >= Week` and reads the season from the day (GameSpeedController deviation 2).
3. **Prevailing winds don't depend on the season.** This follows TechnicalSpec §5.1. ProductionBacklog A4-019's "winter westerlies stronger" isn't in the mechanics, so it isn't implemented.
4. **The steps run as staged passes.** Each step reads the previous step's results. Steps that read neighbours read a stable copy, and transport is a gather. This makes TechnicalSpec §2's "read current, write next" concrete, since later steps depend on earlier ones.
5. **No `is_update_in_progress()`** (EC-14). `Apply()` runs on SimPU and E1 saves at frame boundaries.
6. **The initial state and spin-up are new.** The GDD doesn't define them. A4 adds seeding rules and 52 silent weeks.
7. **`year_quality` forcing (CH-01) is left out.** Closed 2026-10-18: `year_quality` is gone (CHANGE-038), and CHANGE-042 removed the stale term from the A4 TechSpec.
8. **Terrain inputs come from S-1, which is built.** Closed 2026-11-13: S-1 is hex and `Cell` carries `elevation`, `latitude`, `waterSource` and `biome`. The GDD's square `{biome, terrain, region}` description is stale.
9. **The A4 docs' open question 2 ("S-1.5 sandbox?") is closed.** The GDD has since decided on a separate S-2.5 Weather Patterns testbed (2026-10-12). A4's own docs still list the question as open.
10. **History and step-back come from DiaCellHistory, not A4** (restated 2026-11-13). This answers GDD open question 3. Production builds track no layers; S-2.5 tracks three. A4 holds no ring buffer.
11. **Overlays are debug-only.** This answers GDD open question 1 for now. Player-facing weather is V-1's call.
12. **Upwind world edges get inflow.** The GDD's hard boundary (EC-16) only covers outflow at the downwind edge. A4 adds a zero-gradient inflow at the upwind edge so the map edge doesn't create a fake climate strip. It can be switched off with `boundary.edge_inflow`.
13. **The weekly update is deferred, and the budget is CPU time** (restated 2026-11-13). This replaces the GDD's 200 × 200 and 35 ms / 30 fps figure (EC-18, AuthoringDebug §7.1) *and* A4's own earlier "≤ 10 ms inside one sim frame". The step now runs on worker threads and applies k weeks later, so the gate is ≤ 10 ms of CPU at Large (Small ≤ 4.2 ms, Standard ≤ 6.3 ms) plus completion inside k's wall-clock slack. Shrinking the grid stays the last resort.
14. **S-1 already models a static climate, and A4 overrides it** (new 2026-11-13). S-1 derives `temperature`, `airMoisture` and `rainOut` layers and classifies biomes from them. A4 recomputes temperature and moisture dynamically from the same terrain, so the two can disagree — a `DesertBasin` cell can run wet for a season. The GDD should state which is authoritative for player-facing text and for A1's thresholds. See Open Design Question 1.

## Dependencies

**Requires:**
- **S-1 World Generation — built.** A4 reads `elevation`, `latitude`, `waterSource` and `biome` from `Cell` via `WorldGenerationModule::GetCurrentWorld()`, plus `RegionTable` for `set_wind_region` and `find_droughts`, and holds the first generation with `HoldGeneration()`. It subscribes to `WorldGenerationComplete` and `WorldGenerationFailed`.
- **DiaDeferredStep** — system Approved 2026-11-12 (amended 2026-11-13); **all five feature specs Approved 2026-11-13, plans written, `Not Started`.** Supplies `IDeferredStep`, `IParallelExecutor::ParallelFor(count, minRange, fn)`, `StepKey`, the capped executor (strict worker cap, coordinators included), the scheduler and the whole pending-result save rule. **Gates A4 Features 2, 3 and 4** on the build, not on spec work: A4 Feature 2 can start once DeferredStep Feature 1 Task 1 ships the headers. DeferredStep Open Q1 confirms A4's grid (~100 KB as JSON at Large) is cheap enough to serialize as a pending result. Note `ParallelFor` is expected to be called more than once per `Run` — A4's multi-pass pipeline is the normal consumer shape.
- **`WorldStepModule` (CoW-Shared, own system spec, not written).** Owns the `world` channel, orders stages, chooses k, owns the shared `WorldHexGrid`. **Blocks Feature 3.**
- **GameSpeedController — built.** `SimulationTick`, `TimeBand`, `WeekOf`/`SeasonOf`/`kWeeksPerSeason`.
- **E1 `SaveLoadModule` — built** (soft: A4 runs without it; S-2.5 has no SaveLoadModule). Also supplies `GetSnapshotStore()` and `CaptureState()`.
- **DiaCellLayers — built.** Layer and overlay registration, `CellDetailRegistry`, selection, the cell inspector and the `cells.*` command surface. Reached via the `CellLayerRegistryService` stream; registration is SimPU-only.
- **DiaCellHistory — built (engine scope; the in-game panel was not built).** Sampling, tracking, scrubbing, snapshots and events.
- **CellField — built and fully behind DiaCellLayers.** A4's layers reach the screen through layer registration alone; the wind overlay goes through `IDebugDraw`, not CellField. The viewer *host* needs the `KernelCellFieldRegistry` stream, and whoever lands as its first real consumer clears CoW's `KernelModule` provider+consumer stopgap in `cow_main.diaapp`.
- **DiaValueSchema — built.** Labels, ramps, `ui.hotkey`, `min`/`max`, and the `kind: "event"` entries A4's history events need.
- **`SimQueryHostModule` (CluicheGameBaseline) — built.** All `cow.weather.*` commands register here with `SimHelpInfo`.
- **`VisualDebuggerModule` + `DebugLayerManager`.**
- **DiaThreading `JobSystem` / `JobSystemService` / `TrySubmit` — built.** Used only *through* DiaDeferredStep's executor. A4 owns deleting CoW's `JobSystemPlaceholderConsumerModule` (module, `cow_main.diaapp` entry, vcxproj entries) once the real consumer exists. Note there is no `TrySubmit(fn, tag)` overload, so the non-asserting path cannot carry per-range job attribution.
- DiaMessageBus via `MessageBusHostModule`; DiaCore (Json, StringCRC, containers, logging).

**Soft dependencies (fallbacks until they exist):**
- **A5 Hydrology** (Draft; added 2026-11-17). A4 runs without it: primary evaporation uses S-1's static `waterSource`, secondary evaporation is zero, `glacierInput` is zero, and spin-up runs A4 alone. With A5, A4 latches `surfaceWetness`, `openWater` and `glacierEligible` from A5's previous committed week in `Capture`, and spin-up is coupled. A5 reads `waterToGround`, `temperature`, `snowPack` and `glacierInput` from A4's working copy inside the same `Run`.
- A1 implements `IBiomeSeasonalBaseline`. Until then A4 uses the table fallback and reports Degraded.
- A3 publishes `BiomeTransitionComplete`. Until then biomes are fixed after world generation.

**Required by:** A5 Hydrology (soft; added 2026-11-17: reads `waterToGround`, `glacierInput`, temperature and snow pack), A1 (§13 weather integration; amended 2026-11-17: no `groundWater·0.1` in `rainfall`, soil moisture comes from A5), V-1 (weather rendering), S-2.5 (A4-isolated testbed), S-3 (A4 overlays and emergence), D4 later. **A1 inherits this spec's executor change** — its own `IParallelExecutor` seam (`a1-ecology.md`) must move to `Dia::DeferredStep` too.

## Test Strategy

**Unit (Features 1–2, GoogleTests, hand-built hex terrain):**
- Parameter load: valid file, out-of-range values, missing keys → defaults. `baseline_table` resolves all 10 `BiomeId` strings; an unknown id is rejected.
- Initialise: seeding rules; no water source → false (EC-01).
- One test group per pipeline step, using each step's GDD worked expectations:
  - pressure: temp 1.0 → 0.0
  - wind: uniform pressure gives prevailing wind only; a high-pressure cell gives divergence
  - evaporation: saturation self-limits
  - transport: downwind move and edge retention
  - orographic: windward rain and dry leeward
  - precipitation: rain vs snow split
  - melt: thresholds
  - hand-off (amended 2026-11-17, A5 Hydrology; replaces the drainage test, which moves to A5): rain and snowmelt route into `waterToGround`; snow above the cap becomes `glacierInput` on `glacierEligible` cells only and is discarded elsewhere; with no A5 present, primary evaporation uses `waterSource` and secondary evaporation is zero
- Edge cases EC-03 to EC-13, EC-16 and EC-20.
- Upwind edge inflow: on, a uniform field under steady wind stays uniform at the edges. Off, the upwind edge loses moisture each week (the GDD-literal behaviour).
- Determinism:
  - the same inputs give bit-identical fields
  - `SerialExecutor` vs a test executor with 1, 3 and 17 ranges, and with ranges run in reverse order, all give bit-identical fields after 52 weeks
  - **deferred vs synchronous:** a `WeatherStep` driven through DiaDeferredStep's manual executor matches `WeatherRunner` bit-for-bit over 52 weeks
  - **capture isolation:** the test step poisons its terrain source after `Capture()`; `Run` is unaffected

**Layer goldens (added 2026-11-13, "cell data over pixels").** Verification is by cell data and stats, not screenshots. Feature 2 and Feature 6 use `Dia/DiaCellLayers/Testing/CellLayerGolden.h` (`HashLayer`, `GoldenStore::Check`, update with `DIA_UPDATE_GOLDEN=1`) with A4's own golden file under `Tests/GoogleTests/CoW/Golden/weather_layers.golden.json`: all 9 fields over the 5 presets at Small after a fixed week count. Screenshots stay as a per-feature taste check for the user.

**Integration (Features 3–4):**
- No step without a week band; the gate uses `>=` so season/year boundaries still step the week (a test asserts 52 weeks per year, not 48).
- Exactly one `WeatherUpdated` per week, published from `Apply()`. No broadcast during spin-up.
- A step applies at exactly its apply key; a late step stalls and reports.
- `HoldGeneration` keeps the first world until A4 is subscribed; `WorldGenerationFailed` leaves A4 uninitialised and Unhealthy.
- Hot reload takes effect at the next capture only.
- Save at week 26, continue to 52, load week 26, continue to 52: identical (A4-042).
- Save taken with a step in flight: the reload applies the pending result at its key and matches the uninterrupted run.
- Every registered command has a non-empty description, and all 16 register successfully (a failed registration must not leave a partial set behind — see the `WorldGenerationModule` leak in BACKLOG Loose Ends).

**Emergence (Feature 6):** AuthoringDebug §6.1–6.4 over hand-built hex terrains, using the GDD's ratios. Each preset loads and runs 13 weeks without health errors. Checking the patterns by eye in a running world is S-2.5's job.

**Performance (Feature 2 gate):** one `Step()` at Large in Release, CPU time summed across ranges, reported against the 10 ms budget, plus the per-step `profile` breakdown and the wall-clock slack against k.

**Close-out:** a gap audit and edge-case pass per the standing rule, scoped `--filter` runs while iterating and one full `--shards 2` run at the end. No mutation checks.

## Resolved Design Decisions

1. **Upwind world edges use a zero-gradient inflow.** An edge cell whose upwind neighbour is off the map receives an inflow equal to its own outflow.
   - Rejected: the GDD's literal hard boundary, which leaves fake cold, dry strips along upwind edges and probably fails the coastal-rain criterion.
   - Rejected: moving temperature differences instead of raw temperature. That changes §8 throughout the map and doesn't fix moisture.
   - The `boundary.edge_inflow` toggle keeps the GDD-literal behaviour available in S-2.5 for comparison.
2. **The step is deferred through DiaDeferredStep, and A4 keeps a synchronous path as its oracle** (revised 2026-11-13; was "parallel from the start and runs in one frame"). Inputs are captured at kick key N, the pipeline runs on workers, the result applies at N+k. The order-independent gather makes the result bit-identical at any range count, and `WeatherRunner` gives a synchronous reference run to compare against.
   - Rejected: running inside one sim frame. Even spread across the pool, SimPU waits at the end of the frame, so the cost stays on the critical path as a spike — and the coincident week+season+year frames of the full A4→A1→A3 chain cannot fit.
   - Rejected: A4 owning its own `IParallelExecutor`, `SerialExecutor` and JobSystem adapter. These moved into Dia with the AD-001 sign-off of 2026-11-12, so every world system shares one capped executor instead of competing pools.
   - Rejected: spreading the pipeline over the week's 7 day ticks. A save could then land mid-pipeline, which brings back the `is_update_in_progress` problem; DFS-005 solves the save case properly.
   - Rejected: a dedicated world PU (DFS-006).
3. **Saves use JSON text.** At Large, A4's 9 fields as JSON doubles (`%.17g`) are about 1.9 MB; with A1 the total is ~10–15 MB against a 100 MB buffer. Text is simpler than binary blobs, human-readable, and needs no base64 codec. Float precision is exact: jsoncpp writes doubles with `%.17g`, so floats round-trip bit-identically.
   - Rejected: blob API with base64 (dia-needs #11, rejected 2026-10-15 once the buffer was raised).
   - Rejected: quantising fields to fixed point. The simulation would have to run in fixed point too to stay deterministic.
4. **One biome list, carried as an enum, not a `StringCRC`** (new 2026-11-13, user decision). A single 13-value `Biome` enum in `CoW/Shared/WorldGeneration/` serves S-1, A4, A1 and A3: S-1's 10 generated values plus A3's 3 succession outputs, appended. The terrain array is 1 byte per cell instead of 4, the seasonal baseline is a flat `float[13][4]`, and the save needs no CRC palette.
   - Rejected: keeping `StringCRC` for loose coupling. CoW-Shared owns the type; `BiomeId`/`BiomeFromId` bridge to strings where JSON needs them.
   - Rejected: two types (`Biome` 10 for worldgen, a wider `RuntimeBiome` 13 for runtime). It would have given compile-time proof that classification cannot emit a succession biome, but at the cost of two palettes, two name tables and a mapping call at every system boundary — and the `IBiomeSeasonalBaseline` signature would have had to take the wider type anyway, since A4 declares the interface and A1 implements it. Cohesion won. The lost type safety is replaced by one test: after generation, every cell satisfies `IsGeneratedBiome`.
   - Zone validity (`IsBiomeValidInZone`) and forbidden neighbour pairs (`AreBiomesForbidden`) stay **classification** predicates and return `false` for the three succession outputs. Succession may legitimately create adjacencies classification forbids.
5. **A4 registers cell layers and detail rows; it builds no inspector, selection or history UI** (new 2026-11-13). DiaCellLayers and DiaCellHistory own those surfaces and are built.
   - Rejected: A4's own cell inspector and `inspect` command, its `WeatherHistory` ring and `step_back`, and its own `ISnapshotStore`. All duplicate built engine capability.

## Open Design Questions

1. ~~**Does A4 seed from S-1's static climate, or ignore it?**~~ **Resolved 2026-11-16 (Feature 1): A4 is self-contained.** Original question: S-1 computes `temperature`, `airMoisture` and `rainOut` and classified every biome from them; A4 recomputes both dynamically. Seeding from S-1's fields would make week 0 agree with the biome map, but those layers read **zero on imported and `world_file` worlds**, so the seed would silently differ between a generated and a loaded world — and A4 would then need a fallback anyway. Ignoring them keeps A4 self-consistent at the cost of visible disagreement with biome names. Affects Feature 1's seeding rules and GDD deviation 14.
2. ~~**How much does the wind overlay decimate, and is per-cell wind worth it?**~~ **Resolved 2026-11-16 (Feature 5): adaptive 4 × 4 block plan.** Original question: One arrow per cell at Large is ~20k debug-draw primitives per frame against a budget that was raised to 8,192 geometry entries. Every Nth cell is the cheap answer, but a coarse flow-field style overlay (one arrow per 3×3 block, averaged) may read better and cost less. Affects Feature 5 only; decide against the mockup.
3. **The sim-hosted name cap must be raised before S-3.5 is wired** (measured 2026-11-13; not an A4 blocker). The cap is per-stage in practice, so today's worst stage is 45/64 (CluicheTest CellLayersViewer) and CoW's S-1 stage is 38/64. But S-3.5's **built** components alone are 58/64, so it crosses 64 with just its own 10 names — before A4's 16, A1's ≥9, A3's 6 or F-3's ≥11 exist. Projected total **≥99**, over by ≥35. Release is not safe either (~59/64 if the new modules follow S-1's unguarded pattern). Recommended fix: raise all three tables 64 → 192 (there is precedent — commit `6b3a7f80` did 32 → 64; the SimPU table is a `std::vector` so costs nothing, and only `DebugServer::mSimHelp` has real cost at ~7 KB/slot, 450 KB → 1.35 MB Debug heap), plus raise the `SimQueryReply` stream capacity from 48. Needs AD-001 sign-off. Owned by S-3.5, not A4.

4. **Gameplay read API (A4 + A5): derived per-cell flight wind cost, visibility, fire risk, footing — undecided** (raised 2026-11-17). Who owns the derived values, and whether they are computed per cell each week or on demand, is open.
5. **Coupled spin-up length at Large** (added 2026-11-17). With climate cycles on, a multi-year A4 + A5 spin-up could take seconds on the worker job. See A5 Open Design Question 1.

## Gap-pass amendments (2026-11-16)

Found by the plan-writing pass; each item below **supersedes** any contradicting text above. Numbering is stable.

1. The Features table's Feature 1 description reads "13-biome enum" (the shared `Biome` list was widened from 10 to 13); the "10-biome enum" wording is superseded.
2. The parameter count is **21** (the 20 GDD constants plus `solar.cloud_decay`, owned by Feature 1). **Superseded 2026-11-17 (A5 Hydrology):** it is now **20** (19 GDD constants plus `solar.cloud_decay`); the `ground_water` group moves to A5.
3. The sim-hosted name cap is 192 per PU in code (`SimQueryHost::kMaxHandlers`); the earlier 64 is stale.
4. `WeatherRunner::Start` takes a `startWeekOffset` (S-2.5 Feature 3); `WeatherSimulation` gains `SetCommittedWeek`, `ClearForcings`, `Reseed`, `RegisterCellSet`/`ClearCellSets` (Features 2, 3, 6).

## Status

**Status:** `Approved` (2026-10-14). **Revised 2026-11-13** by a dependency audit: the execution model moved to DiaDeferredStep, three "not built" dependencies turned out to be built, duplicated engine surfaces were removed, and the biome type, command hosting and history ownership were corrected. **Revision re-approved 2026-11-16** (user, during the Feature 2 interview).

**Blocked on:** nothing in spec work. DiaDeferredStep's five features are Approved with plans (2026-11-13) and World Step is drafted, so A4 Features 2, 3 and 4 are gated only on those builds — Feature 2 can start as soon as DeferredStep Feature 1 Task 1 ships the headers. Features 1, 5 and 6 are startable now. The one real prerequisite is the **S-1 biome widening** (10 → 13), which gates Feature 1.

**Ripple edits owed elsewhere:** `a1-ecology.md` (its `IParallelExecutor` seam and 16 ms band gate) and `s25-weatherpatterns.md` (its weekly tick budget and step-back controls). Both deliberately left until this spec is re-approved, so they are touched once.

**Amended 2026-11-17 (user decisions: A5 hand-off, Features 7–9).** Status stays `Approved`. Features 7–9 are `Draft` with no spec yet. The A1 and GDD fold-backs for the hand-off go through the GDD session.

**Plan:** [a4-weather.plan.md](a4-weather.plan.md) (written 2026-11-16, not started).
