# System Spec: A4 Weather

**Parent:** @docs/specs/applications/cow/cow.md  
**Design source:** `Output Docs/GDD/08_SystemsDeepDive/A4_Weather/` (DesignDocs repo; `A4_WeatherSystem.md`, `A4_TechnicalSpec.md`, `A4_TuningParameters.md`, `A4_AuthoringDebug.md`, `A4_EdgeCaseMatrix.md`, `A4_ProductionBacklog.md`, all Locked 2026-10-11)

**Gameplay Domains:** simulation, debug

## Summary

A4 simulates CoW's weather as a weekly **cellular automaton** over the world's hex grid. Each cell carries moisture, temperature, pressure, wind, precipitation, cloud cover, ground water and snow pack. Once per game week every cell updates from local rules: solar heating, pressure, wind, evaporation, transport, orographic lift, precipitation, snow and drainage. Coastal rain, rain shadows, moving storms, spring floods and droughts emerge from those rules rather than being scripted.

A4 owns the **A1 ↔ A4 contract**. It publishes one `WeatherUpdated{week}` notification per week and exposes a read-only per-cell snapshot that A1 (ecology) and V-1 (tile rendering) read. It defines the `IBiomeSeasonalBaseline` query that A1 implements.

A4 also ships the tools for tuning weather on its own: a debug domain with eight overlays, a cell inspector, console commands, presets, optional history for step-back, and live parameter tuning. The **S-2.5 Weather Patterns** testbed hosts them as a stage before S-3 Living Ecology exists.

## Goals

1. **Weekly CA update.** On every `SimulationTick` with `band >= TimeBand::Week`, run the GDD's 10-step pipeline (TechnicalSpec §2) over all cells, then commit atomically.
2. **Deterministic.** No RNG. Same world + same parameters + same tick count gives bit-identical state, whatever the game speed and however the grid is iterated.
3. **The A1/V-1 contract.** `WeatherUpdated{week}` plus a const `WeatherSnapshot` view carrying every field the GDD's §13 payload lists.
4. **Data-driven tuning.** All 20 constants load from `weather_parameters.json`, are validated, hot-reload, and can be overridden live.
5. **Save/load.** Committed weather state round-trips through E1. A reload continues bit-identically.
6. **Observable.** A debug domain with seven scalar base layers plus a wind vector overlay (nine floats per cell, because wind has two components) (amended 2026-10-19, consistency pass), a cell inspector, console commands, and per-step profiling.
7. **Tunable in isolation.**
   - The five presets and the four regression scenarios (AuthoringDebug §5–§6) ship with A4.
   - The S-2.5 testbed hosts the stage that proves the GDD success criteria (`A4_WeatherSystem.md` §Success Criteria).

## Non-Goals

- **Weather narrative events** (`DroughtFrontDetected`, `StormFormed`, `SpringMeltFlood`). These belong to D4. A4's `find_storms` / `find_droughts` are debug queries only.
- **Player-facing weather visuals.** Rain/snow particles, cloud shading and snow overlay belong to V-1, which reads the snapshot. The A4 overlays are debug-only.
- **Ecology response.** A1 consumes `WeatherUpdated`. A4 never writes A1 state.
- **Terrain data.** Elevation, latitude and water_source come from S-1 (see Dependencies). A4 doesn't derive or generate them.
- **Dragon climate manipulation** (GDD "fire dragons → warming"). This can be added later as an external forcing input. The forcing mechanism used by `spawn_storm` is the hook for it.
- **Spreading the update over several frames.** Each weekly update runs inside one sim frame, in parallel (see Parallel execution).

## Architecture

### Placement

**CoW-Shared** (AD-002). A4 is consumed by A1, V-1, S-2.5 and S-3, and it is CoW-specific content. Pure logic lives in `Cluiche/CoW/Shared/Weather/`. The SimModule, debug domain and save participant live in `Cluiche/CoW/Modules/`. Two shared-layer changes are proposed (see Dependencies).

```
SimPU
  └─ WeatherModule (SimModule)
       on WorldGenerationComplete → WeatherSimulation::Initialise(terrain, biomes) → spin-up N weeks
       on SimulationTick (band >= Week) → WeatherSimulation::Step(season) → Broadcast WeatherUpdated{week}
       on BiomeTransitionComplete → update cached biome for the cell (applies next week)
       owns: WeatherSimulation, WeatherParameters, WeatherHistory (optional ring), save participant, debug domain
```

### Grid

The hex grid comes from the amended S-1 (see Dependencies). A4 uses S-1's hex topology helper and does not define its own. A4 needs these properties from it:

- Cells are indexed `0..N-1`, and each cell has up to 6 neighbours in a fixed direction order `d = 0..5`.
- Direction `d` sits at on-screen angle `30° + d × 60°`, counter-clockwise (flat-top hexes have no east or west neighbour; `0` NE, `1` N, `2` NW, `3` SW, `4` S, `5` SE). `NeighbourAt(i, d)` returns `kNoCell` past the world edge. The grid is Dia's `HexTopology` / `HexPathGrid(width, height)`.
- Wind angles snap to the nearest direction with `HexTopology::DirectionFromAngle`, `d = round((angle - 30°) / 60°) mod 6`, where the angle is the on-screen counter-clockwise angle in y-down world space. This is the GDD's CH-06 adapted to flat-top hexes; the GDD's east-based wind angle needs the same 30° shift.

### State (structure of arrays)

```cpp
namespace CoW::Weather {

    // Per-cell dynamic state. One float array per field, N cells each.
    struct WeatherFields {
        DynamicArrayC<float> moisture, temperature, pressure;
        DynamicArrayC<float> windDirection, windSpeed;     // radians [0, 2π), [0, 1]
        DynamicArrayC<float> precipitation, cloudCover, groundWater, snowPack;
    };

    // Read-only inputs, captured at Initialise.
    struct WeatherTerrain {
        DynamicArrayC<float>                elevation;     // [0, 1], from S-1
        DynamicArrayC<float>                latitude;      // 0 = equator, 1 = pole, from S-1
        DynamicArrayC<uint8_t>              waterSource;   // ocean/lake/river, from S-1
        DynamicArrayC<Dia::Core::StringCRC> biome;         // from S-1, updated by A3
    };
}
```

At Large (104 × 64 = 6,656 cells) this is 9 floats (36 B) of dynamic state per cell, or about 240 KB per copy. The GDD's 64 B/cell estimate (at 200 × 200) assumed an array-of-structs layout.

### Update pipeline

`WeatherSimulation::Step()` runs the TechnicalSpec §3–§12 steps in GDD order. Each step is a **full-grid pass**.

- **Committed vs working.** The pipeline copies the committed fields into a working set, runs every step on the working set, and then swaps working and committed. Readers (the snapshot, saves, the debug domain) only ever see committed state (EC-14).
- **Steps that read neighbours read a stable copy.** These are wind (§5, the pressure gradient), moisture and temperature transport (§7, §8) and orographic lift (§9). Each reads the fields as they stood at the end of the previous step, never values the current step has partly written. Local steps update in place. This is how the GDD's "synchronous update" (§14 invariant 2) is met when steps depend on earlier steps' results.
- **Transport is a gather.** Pass A computes each cell's outflow and downwind direction. Pass B sets each cell's inflow to the sum of the outflows aimed at it, summed in direction order 0..5. The result is independent of iteration order, which is what makes parallel execution deterministic. Outflow toward `kNoCell` is skipped, so the moisture or heat stays in the cell (EC-16, CH-10).
- **Upwind edge inflow.** When a cell's upwind neighbour is `kNoCell`, the cell receives an inflow equal to its own outflow, as if the world past the edge had the same weather (a zero-gradient boundary). Without this, upwind edge cells would lose moisture and heat every week and get nothing back. With steady prevailing winds that leaves fake cold, dry strips along the map edges. The `boundary.edge_inflow` parameter (default `true`) switches it off so S-2.5 can show the GDD-literal behaviour side by side.
- **Precomputed directions.** The unit vector for each hex direction is a constant table. The wind step's pressure gradient (§5.2) uses it instead of computing `cos`/`sin` per neighbour.
- **Clamping.** Precipitation amounts are clamped non-negative (CH-09). Snow pack and ground water are capped at 1.0 (CH-04, EC-12). Temperature after transport is clamped non-negative (CH-07). After the last step every field is clamped to its range (EC-20).
- **Season.** Determined from the week (13 weeks per season): weeks 1–13 Spring, 14–26 Summer, 27–39 Autumn, 40–52 Winter. Read from GameSpeedController's calendar via the `SimulationTick` that triggered the step. It is not a separate `SeasonAdvanced` message.

### Parallel execution

Every pass splits the grid into contiguous cell ranges and runs them at the same time. The pass ends when every range has finished, and the next pass starts after that.

```cpp
namespace CoW::Weather {
    // Runs fn(begin, end) over [0, count) split into ranges, and returns once all ranges are done.
    class IParallelExecutor {
    public:
        virtual ~IParallelExecutor() = default;
        virtual void For(uint32_t count, const std::function<void(uint32_t begin, uint32_t end)>& fn) = 0;   // matches JobSystem's JobFn
    };

    class SerialExecutor final : public IParallelExecutor { /* one range; tests and fallback */ };
}
```

- **Bit-identical at any thread count.** Each range writes only its own cells. It reads only the stable copy the pass started from, and gathers inflow in a fixed order. So the result doesn't depend on how many ranges there are or which one finishes first.
- **Reductions are serial.** Grid totals for the debug panel and metrics are worked out per range and then combined in range order, after the pass. They never use atomics. They don't feed back into the simulation.
- **Job system.** `WeatherModule` wraps a `Dia::Threading::JobSystem` in a `JobSystemExecutor`, which submits one job per range and waits on every handle. Dia's `JobSystem` has no ParallelFor on purpose, so the adapter does the range split. The range count is `weather_job_ranges` in the module config (default: worker count × 4). If no job system is available, the module falls back to `SerialExecutor` and reports Degraded health.
  - **Where the `JobSystem` comes from** (dia-needs #12, approved 2026-10-14):
    - `JobSystemModule` (MainPU) publishes the app's pool as a `JobSystemService` `ServiceStream`. This is the same pattern as `KernelTextureHandler`: MainPU registers it and another PU reads it. Cross-PU `ModuleRef` lookups aren't provided by design (`DiaApplicationFlow/Module.h`).
    - `WeatherModule` holds a `ServiceStreamReader<Dia::Threading::JobSystem>`.
  - **Shutdown safety.** `Application::BeginStopAllActive` stops MainPU modules first while SimPU is still running frames, so the pool can shut down during a `Step()`.
    - `JobSystemModule::DoStop` calls `Deregister()` before `Shutdown()`.
    - The executor checks `IsAvailable()` before each pass.
    - The executor submits with Dia's new `JobSystem::TrySubmit`, which returns an invalid handle after `Shutdown` instead of asserting. Any range whose `TrySubmit` fails runs inline on the SimPU thread. Each cell's result depends only on its inputs, so the output is unchanged.
- **Performance gate.** A Feature 2 GoogleTest benchmarks one `Step()` at Large (104 × 64 = 6,656 cells) in Release with the job executor and reports the time. The budget is **≤ 10 ms** (amended 2026-10-19, product review). Missing it is reported but doesn't fail the build. A miss means optimising the hot pass, not spreading the update over several frames. Supporting budgets per map size: Small (66 × 42 = 2,772 cells) ≤ 4.2 ms, Standard (80 × 52 = 4,160 cells) ≤ 6.3 ms, Large (104 × 64 = 6,656 cells) ≤ 10 ms (scaled by cell count).
- **Spin-up** (52 weeks) runs through the same executor and is accepted as a one-time cost at New Game.

### Biome seasonal baseline (A4-owned contract, A1 implements)

```cpp
namespace CoW::Weather {
    enum class Season : uint8_t { Spring, Summer, Autumn, Winter };

    class IBiomeSeasonalBaseline {
    public:
        virtual ~IBiomeSeasonalBaseline() = default;
        // Baseline air temperature [0, 1] for the biome in the season.
        // Must return a value for every biome S-1 can produce, water biomes included.
        // An unknown biome returns 0.5 (EC-19). A4 asserts the result is in range.
        virtual float GetSeasonalTemperature(Dia::Core::StringCRC biome, Season season) const = 0;
    };

    // Default implementation until A1 exists: a 4-season table from weather_parameters.json,
    // the same for every biome. Used by S-2.5 and by tests.
    class TableBiomeSeasonalBaseline final : public IBiomeSeasonalBaseline { /* ... */ };
}
```

`WeatherModule` takes the provider via `ModuleRef` to whichever module implements it (A1's `EcologyModule` once it exists). If none is found it falls back to `TableBiomeSeasonalBaseline` and reports Degraded health.

### Output to A1 and V-1

| Direction | Message | Payload |
|---|---|---|
| Publishes | `WeatherUpdated` | `uint64_t week; Season season;`, sent once after commit |
| Subscribes | `SimulationTick` | gate `band >= TimeBand::Week` |
| Subscribes | `WorldGenerationComplete` | initialise and spin up |
| Subscribes | `BiomeTransitionComplete` | `{cell, newBiome}` from A3, takes effect from the next week. Not subscribed until A3 publishes it |

Per-cell data is read through the snapshot, never sent through the bus (bus payloads are small POD):

```cpp
namespace CoW::Weather {
    enum class PrecipitationType : uint8_t { None, Rain, Snow };

    class WeatherSnapshot {          // const view over committed state; valid until the next Step()
    public:
        uint32_t GetCellCount() const;
        uint64_t GetWeek() const;

        // A1 fields (GDD §13)
        float Rainfall(uint32_t cell) const;        // precipitation + groundWater × kGroundWaterRainfallFactor (0.1)
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

1. Read terrain from S-1. **Assert at least one water-source cell** (EC-01). If none, log an error, report Unhealthy health, and don't run.
2. Seed each cell:
   - temperature: the seasonal baseline for its biome in Spring, with the latitude modifier applied
   - moisture: `initial_moisture_water` on water-source cells, `initial_moisture_land` elsewhere
   - pressure: 0.5
   - all other fields: 0
3. **Spin up.** Run `spinup_weeks` steps silently (default 52, set in the module config). No `WeatherUpdated` is sent, and the week counter resets to 0 afterwards. The first published week has settled winds and moisture.

Spin-up is deterministic, so a save does not need to know about it. A new game rerunning it gives the same result.

### Parameters

The tuning constants live in `Assets/CoW/Global/Weather/weather_parameters.json` (amended 2026-10-19, F-3 World Assets), using the GDD TuningParameters §14 schema with envelope `"format": "cow.weather_params", "version": 1`. The schema is extended with an `init` block holding `initial_moisture_water` and `initial_moisture_land`, a `baseline_table` block for the default `TableBiomeSeasonalBaseline`, and a `boundary` block for `edge_inflow` (see Update pipeline). The `.diaapp` module config gives the file path, `spinup_weeks` and `history_weeks`:

```json
{
    "instance_id": "WeatherModule",
    "type_id": "WeatherModule",
    "stages": ["all"],
    "dependencies": ["MessageBusHostModule"],
    "soft_dependencies": ["SaveLoadModule"],
    "config": { "parameters": "Global/Weather/weather_parameters.json", "spinup_weeks": 52, "history_weeks": 0, "weather_job_ranges": 0 }
}
```

`weather_job_ranges: 0` means "worker count × 4" (amended 2026-10-19, product review). `SaveLoadModule` is a soft dependency: when present, weather state is saved; without it, A4 still runs but doesn't participate in saves (this is the S-2.5 case).

- **Validation on load.** Every rate must be in `[0, 1]` and `snow_threshold <= melt_threshold`. A missing or invalid file logs an error, falls back to the compiled defaults (the GDD values), and reports Degraded health. This is the same rule as GameSpeedController.
- **Hot reload (amended 2026-10-19, product review).** `cow.weather.reload` re-reads the file between steps (GDD calls this `reload_parameters`; A4 uses `cow.weather.reload`). `cow.weather.set_param <dotted.key> <value>` overrides one value until the next reload. Both take effect at the next `Step()`, never in the middle of one.
- **Tuned parameters are saved as weather packs** (amended 2026-10-19, F-3 World Assets). `cow.world.pack.save_as weather <id>` writes overrides-only to `Assets/CoW/Global/Weather/Packs/<id>.weather.json` via the `source` alias. Parameters in the defaults file are not saved; packs are the normal tuning path.

### History (rewind)

> **Amended 2026-10-19 (DiaCellLayers/DiaCellHistory):** `WeatherHistory` and step-back are replaced by DiaCellHistory's scrub and snapshot restore. S-2.5 declares default tracked layers in its `.diastage` config (`"history": {"track": ["weather.precipitation", "weather.temperature", "weather.ground_water"]}`). The `history_weeks` config option remains for backwards compatibility until DiaCellHistory is built; S-2.5 declares it as 13 (about 3 MB at Large).

`WeatherHistory` is an optional ring buffer of committed `WeatherFields`, `history_weeks` deep. It is 0 (off) in `cow_main.diaapp` and 13 in the S-2.5 stage, which is about 3 MB at Large (6,656 cells; amended 2026-10-19, product review: 13 weeks × 9 fields × 6,656 cells × 4 bytes/float ≈ 3.1 MB).

- `cow.weather.step_back` restores the previous committed state and decrements the week. *(Replaced by `cells.history.view` and `cells.snapshot.restore` when DiaCellHistory is built.)*
- Step-back is a **debug operation for A4-only stages**. It doesn't rewind GameSpeedController's day counter or any other system, so it is only offered where A4 runs alone (S-2.5). The command refuses to run when `history_weeks` is 0.

### Save/load (E1)

`WeatherSaveParticipant` (`ISaveable`, id `"weather"`, version 1) follows the `GameSpeedSaveParticipant` pattern.

- **Serialize** writes:
  - the week and the cell count
  - each committed `WeatherFields` array as a JSON float array (9 arrays, one per field)
  - the cached biome per cell, since A3 may have changed biomes since world generation. It is written as a palette (a list of the distinct biome CRCs) plus an array of `uint8_t` palette indices.

  Terrain is not saved; it comes back from S-1's world on load.
- **Validate** rejects the load if the saved cell count doesn't match the world's, or if any field array has the wrong length.
- **Deserialize** restores everything into committed state and clears history.

**Size (amended 2026-10-19, product review).** At the three map sizes, A4's 9 float fields stored as JSON doubles (`%.17g` format, bit-identical round-trip) at ~30 bytes per float in formatted JSON:
- **Small** (66 × 42 = 2,772 cells): 9 × 2,772 = 24,948 floats ≈ **0.8 MB**
- **Standard** (80 × 52 = 4,160 cells): 9 × 4,160 = 37,440 floats ≈ **1.2 MB**
- **Large** (104 × 64 = 6,656 cells): 9 × 6,656 = 59,904 floats ≈ **1.9 MB**

Combined with A1's estimated ~35 fields/cell, the total ecology + weather save is projected at **~10-15 MB** at Large, well within the raised 100 MB buffer limit (`SaveContext::kBufferSize`, updated from 10 MB). The 8-slot cap limits total disk usage to 800 MB. Text JSON is simpler than binary blobs, human-readable for debugging, and requires no base64 codec (amended 2026-10-19, product review: dia-needs #11 blob API was rejected 2026-10-15 after buffer was raised to 100 MB; A4 uses JSON text).

E1 saves at the sim-frame boundary, and `Step()` runs entirely inside one sim frame. So a save can never see a half-updated grid, and the GDD's `is_update_in_progress()` flag isn't needed.

### Debug and observability

`WeatherDebugDomain` (`IDebugDomain`) is registered through `VisualDebuggerModule` in Debug builds.

**Panel card:**
- shows the week and season, cell count, grid totals (mean moisture, total snow pack, number of cells raining), parameter source (file or fallback) and baseline provider (A1 or table)
- has overlay toggles
- posts commands, never mutates state directly (UI/business separation)

**World drawers (9 overlays, AuthoringDebug §1; amended 2026-10-19, product review; amended 2026-10-19, DiaCellLayers/DiaCellHistory):**
- Eight scalar fields become **base layers** (one at a time): moisture, pressure, temperature, precipitation, snow_pack, cloud_cover, ground_water. Each is registered with DiaCellLayers with labels and ramps from DiaValueSchema.
- Wind becomes an **overlay**: drawn as a per-cell arrow coloured by how far the wind deviates from the prevailing direction.
- Base layers are shown one at a time (radio); wind overlay can be on with any base layer.

**Canonical layer IDs (amended 2026-10-19, product review; amended 2026-10-19, DiaCellLayers/DiaCellHistory):** `weather.moisture`, `weather.pressure`, `weather.temperature`, `weather.precipitation`, `weather.snow_pack`, `weather.cloud_cover`, `weather.ground_water`, `weather.wind` (overlay). Shared with S-2.5's scenario loader, which rejects unknown names.

Drawers are hosted wherever the domain is registered: S-2.5 first, then S-3.

**Cell inspector:**
- shows all fields plus the derived values: seasonal baseline, prevailing direction, pressure perturbation, upwind elevation
- the domain feeds it to the panel for the selected cell

**Console commands** (`cow.weather.*`, `CommandRegistry`). All mutations are queued and applied at the next `Step()` boundary. The commands are a typed command model (`WeatherCommandExecutor`) bound to a `WeatherSimulation&`: `WeatherModule` binds one to its own simulation, and S-2.5's headless runner binds another to its own, so a scenario's setup commands mean the same thing live and headless:

| Command | Effect |
|---|---|
| `inspect <cell>` | Print the cell's state |
| `spawn_storm <cell> <intensity> <weeks>` | Forcing: hold pressure low and moisture high for N weeks |
| `spawn_high_pressure <cell> <intensity> <weeks>` | Forcing: hold pressure high and moisture low for N weeks |
| `force_rain <cell> <amount>` | One-week precipitation forcing |
| `set_wind <cell> <deg> <speed>` / `set_wind_region <region> <deg> <speed>` / `clear_wind_overrides` | Wind overrides (replace §5 output for those cells) |
| `reload` / `set_param <key> <value>` | Parameters (see Parameters above) |
| `export_state <file>` / `import_state <file>` | JSON snapshot of committed state and week |
| `find_storms` / `find_droughts <weeks>` | Pattern queries (pressure < 0.3 / consecutive dry weeks per S-1 region) |
| `check_rain_shadow <cell>` | Compare windward and leeward precipitation |
| `profile` / `report_memory` | Per-step timing for the last step / buffer sizes |
| `load_preset <name>` | Apply a preset (state + forcings + season) |
| `step_back` | History rewind (only when `history_weeks` > 0) |

**Hotkeys** (amended 2026-10-20, GDD fold-back): A4 owns its layers' hotkeys in its fields schema (`ui.hotkey`). They follow the S-2.5 GDD hotkey table (Snow = **K**). The S-3 GDD variant (S for Snow, no Ground Water) is replaced by the shared picker.

Forcings and overrides are deterministic inputs applied at a fixed point in the pipeline (EC-15). They are **not saved**: they are debug tools.

**Instrumentation:**
- traces and profiling: `DIA_TRACE_ZONE` on `Step()`, `DIA_PROFILE_SCOPE` per pipeline step (backs `profile`)
- metrics: step duration, raining-cell count, mean moisture
- **events (amended 2026-10-19, DiaCellLayers/DiaCellHistory):** droughts and storms (from `find_storms` / `find_droughts`) are emitted as DiaCellHistory events with type, anchor cell and cell count.
- health (`IHealthReporter`):
  - Degraded: fallback parameters or fallback baseline
  - Unhealthy: no water sources

**Logging (amended 2026-10-19, product review):** channel `WTH` (3-4 letter CoW weather channel, following S-1's `WLD` pattern).
- **Info:** parameter file load/reload success (path, source: file or fallback), preset loaded, reset, baseline provider (A1 or table)
- **Warning:** parameter out of range (clamped, key, value), unknown overlay id (scenario loader), unknown preset name
- **Error:** parameter file not found or parse failed (path), no water-source cells (EC-01), baseline provider returned out-of-range temperature

### Presets and regression scenarios

A4 provides the reusable pieces that a testbed needs. It does not own the stage.

- **Presets.** The five AuthoringDebug §5 presets are `coastal_storm`, `rain_shadow`, `winter_snow`, `spring_melt` and `drought` (amended 2026-10-19, F-3 World Assets): whether each is a weather pack (parameter overrides) or a scenario (forced state) is decided in A4 Feature 6 (see F-3 Open Design Question 1).
  - Each preset is a data file: a starting committed state, forcings, and a season, over a fixed preset world. That world is S-2.5's `s25_coast_range` (an S-1 preset + seed), amended 2026-10-18.
  - `cow.weather.load_preset <name>` applies one at the next step boundary.
- **Regression scenarios.** The four AuthoringDebug §6.1–6.4 scenarios are GoogleTests over hand-built hex terrains. Each terrain has a coast, an inland region and a mountain range. The tests use the GDD's success ratios:
  - coastal precipitation > 1.5× inland
  - windward > 2× leeward
  - more than 70% of mountain cells have snow
  - less than 30% of the snow remains after spring

### Testbed host: S-2.5 Weather Patterns

A4 in isolation is proven in **S-2.5 Weather Patterns**, a separate testbed. The GDD decided this on 2026-10-12, and S-2.5 is a prerequisite for S-3. S-2.5 owns:
- the stage, with A4 alone over curated S-1 worlds, `history_weeks: 13` and the table baseline
- the preset and overlay panels
- click-to-inspect
- step-back
- the GDD's nine tests as numeric checks, run headless for the verdict and live as check lines, plus a walkthrough backed by those checks

S-2.5 uses A4's debug domain, presets and commands as they are. Its spec (@docs/specs/applications/cow/systems/s25-weatherpatterns/s25-weatherpatterns.md, Approved 2026-10-18) asked for three A4 changes, applied here on 2026-10-18:
- **`WeatherRunner`**: a pure initialise → spin-up → step-N-weeks driver (see Public Interfaces) that `WeatherModule` uses, so S-2.5's headless runner can't drift from the module.
- **The command executor binds to a `WeatherSimulation&`**, not to `WeatherModule` (see Console commands).
- **The preset world is S-2.5's `s25_coast_range`** (S-1 preset + seed; see Presets and regression scenarios).

## Public Interfaces

```cpp
namespace CoW::Weather {

    // Pure logic, no Dia module dependencies. Unit-tested directly.
    class WeatherSimulation {
    public:
        bool Initialise(const WeatherTerrain& terrain, const WeatherParameters& params,
                        const IBiomeSeasonalBaseline& baseline);     // false if no water source
        void Step(Season season, IParallelExecutor& executor);        // one week, commits at the end
        void SetParameters(const WeatherParameters& params);         // effective next Step
        void SetBiome(uint32_t cell, Dia::Core::StringCRC biome);     // A3; effective next Step
        void AddForcing(const WeatherForcing& forcing);               // debug; effective next Step
        WeatherSnapshot GetSnapshot() const;
        const WeatherStepProfile& GetLastProfile() const;
        void RestoreCommitted(uint64_t week, const WeatherFields& fields);
    };

    struct WeatherParameters { /* GDD §14 groups + init + baseline_table */ };
    bool LoadWeatherParameters(const Json::Value& root, WeatherParameters& out);   // false = invalid; out = defaults
    WeatherParameters DefaultWeatherParameters();

    // Pure driver shared by WeatherModule and S-2.5's headless runner (amended 2026-10-18).
    // Initialise + spin-up, then one Step per week with the season taken from the week (13 weeks per season).
    class WeatherRunner {
    public:
        bool Start(const WeatherTerrain& terrain, const WeatherParameters& params,
                   const IBiomeSeasonalBaseline& baseline, uint32_t spinupWeeks, IParallelExecutor& executor);
        void StepWeek(IParallelExecutor& executor);
        WeatherSimulation& GetSimulation();
    };

    // SimModule wiring.
    class WeatherModule : public Dia::ApplicationFlow::SimModule {
    public:
        WeatherSnapshot GetSnapshot() const;        // A1, V-1, debug: read on WeatherUpdated
        const WeatherSimulation& GetSimulation() const;
        // F-3 World Assets (amended 2026-10-20, Feature 6): the module owns a `WeatherPackParticipant`
        // (kind `weather`, defaults `cow.weather_params`) and exposes it by reference; it does not inherit the interface.
        // Apply validates and swaps mParams (effective next week); Initialise uses mParams, never the file;
        // IsReady(rev) is true after Initialise AND spin-up for that revision.
        IWorldAssetParticipant& GetWeatherPackParticipant();
    };
}
```

Messages (`WeatherUpdated`, `BiomeTransitionComplete`) are hand-written structs following the `GameSpeedMessages.h` pattern, in `Cluiche/CoW/Modules/WeatherMessages.h`.

## Features

| # | Feature | Description | Spec | Status |
|---|---------|-------------|------|--------|
| 1 | Weather Data & Parameters | `WeatherFields`/`WeatherTerrain` SoA, `WeatherParameters` + JSON load/validate/defaults, `IBiomeSeasonalBaseline` + `TableBiomeSeasonalBaseline`, `Initialise` (terrain ingest, seeding, EC-01 assert). Pure logic, TDD | TBD | Draft |
| 2 | CA Pipeline | The 10 GDD steps + clamping as full-grid passes, committed/working swap, gather transport, upwind edge inflow, precomputed hex directions, forcings/overrides, `IParallelExecutor` + `SerialExecutor`. One test group per step plus EC-03 to EC-20, serial-vs-parallel bit-identity, 10 ms performance gate. Pure logic, TDD | TBD | Draft |
| 3 | WeatherModule | SimModule: `.diaapp` config, parameter file load and hot reload, `JobSystemExecutor` over the `JobSystemService` stream with `TrySubmit` and an inline fallback (serial until dia-needs #12 is built), `SimulationTick` week gate, `WorldGenerationComplete` init + spin-up, baseline provider lookup and fallback, `WeatherUpdated` publish, snapshot access, `BiomeTransitionComplete`, history ring, `cow_main.diaapp` entry | TBD | Draft |
| 4 | Weather Save/Load | `WeatherSaveParticipant`: JSON array serialize/deserialize of committed `WeatherFields` (9 float arrays) + biome palette (CRC list + uint8 indices); size-checking validate; save/continue/load/continue round-trip test | TBD | Draft |
| 5 | Weather Debug | `WeatherDebugDomain` panel card + inspector, 8 overlay world drawers + wind field, `cow.weather.*` console commands, profiling, metrics, health | TBD | Draft |
| 6 | Presets & Regression | The 5 preset data files + `load_preset`, and the 4 AuthoringDebug §6 scenarios as GoogleTests over hand-built hex terrains (GDD ratios) | TBD | Draft |

**Routed elsewhere:** the stage, panels, click-to-inspect and success-criteria walkthrough go to the S-2.5 Weather Patterns testbed.

**Build order:** 1 → 2 → 3, then 4, 5 and 6 can run in parallel (6's `load_preset` command sits in 5's command set).

- Features 1 and 2 need no S-1 code: their tests build `WeatherTerrain` by hand. The same goes for Feature 6's regression tests.
- Features 3–6 need the amended S-1 to be built.

## Binding Decisions

| ID | Decision | How it applies |
|----|----------|----------------|
| PD-002 | ProcessingUnit/Phase/Module architecture | `WeatherModule` is a SimModule on SimPU. No singletons; providers come in through `ModuleRef` |
| PD-004 | No STL containers in public APIs | `WeatherFields`/`WeatherTerrain` use `DynamicArrayC`. The snapshot exposes accessors, not `std::` types |
| PD-005 | x64 is the only build target | Saves are JSON text, little-endian floats stored as doubles (jsoncpp `%.17g`) |
| AD-001 | Ask permission before Dia edits | Two Dia capabilities were approved: (1) `JobSystemService` + `JobSystem::TrySubmit` (dia-needs #12, approved 2026-10-14, not built), and (2) CellField renderer (dia-needs #14, approved 2026-10-15, not built). Each is built from its own feature spec in the Dia tree, not from A4. Everything else uses existing DiaMessageBus, DiaSaveGame text JSON (buffer raised to 100 MB; dia-needs #11 blob API was rejected 2026-10-15), DiaThreading `JobSystem`, DiaVisualDebugger and DiaAPI `CommandRegistry` |
| AD-002 | Four-tier dependency model | CoW-Shared, because A1, V-1, S-2.5 and S-3 all consume it |
| AD-003 | GDD owns design intent | GDD A4 owns the mechanics and constants. Deviations are listed below for the GDD owner |
| AD-005 | One stage per testbed | A4 is a System and owns no stage. The S-2.5 testbed is the single stage that hosts it, and chooses presets from its panel |

## Deviations from GDD A4

These are for the DesignDocs owner to fold back into A4:

1. **`WeatherUpdated` is a notification, not a per-cell payload.** It is one `{week, season}` broadcast. The §13 per-cell fields are read through `WeatherSnapshot` because bus payloads are small POD. The fields and formulas are unchanged.
2. **`SeasonAdvanced` is replaced by `SimulationTick`.** A4 steps on `band >= Week` and reads the season from the day (GameSpeedController deviation 2).
3. **Prevailing winds don't depend on the season.** This follows TechnicalSpec §5.1. ProductionBacklog A4-019's "winter westerlies stronger" isn't in the mechanics, so it isn't implemented.
4. **The steps run as staged passes.** Each step reads the previous step's results. Steps that read neighbours read a stable copy, and transport is a gather. This makes TechnicalSpec §2's "read current, write next" concrete, since later steps depend on earlier ones.
5. **No `is_update_in_progress()`** (EC-14). The update runs inside one sim frame and E1 saves at frame boundaries.
6. **The initial state and spin-up are new.** The GDD doesn't define them. A4 adds seeding rules and 52 silent weeks.
7. **`year_quality` forcing (CH-01) is left out.** Closed 2026-10-18: `year_quality` is gone (CHANGE-038), and CHANGE-042 removed the stale term from the A4 TechSpec.
8. **Terrain inputs depend on an S-1 amendment.** S-1 must be hex and provide elevation, latitude and water_source per cell. Today its `Cell` is `{biome, terrain, region}` on a square grid.
9. **The A4 docs' open question 2 ("S-1.5 sandbox?") is closed.** The GDD has since decided on a separate S-2.5 Weather Patterns testbed (2026-10-12). A4's own docs still list the question as open.
10. **History is 13 weeks, and only in A4-only stages (S-2.5).** It answers GDD open question 3. Production builds keep no history.
11. **Overlays are debug-only.** This answers GDD open question 1 for now. Player-facing weather is V-1's call.
12. **Upwind world edges get inflow.** The GDD's hard boundary (EC-16) only covers outflow at the downwind edge. A4 adds a zero-gradient inflow at the upwind edge so the map edge doesn't create a fake climate strip. It can be switched off with `boundary.edge_inflow`.
13. **The performance target is ≤ 10 ms per week at Large with a parallel update (amended 2026-10-19, product review).** This replaces the GDD's 200 × 200 and 35 ms / 30 fps figure (EC-18, AuthoringDebug §7.1). Supporting budgets per map size: Small ≤ 4.2 ms, Standard ≤ 6.3 ms, Large ≤ 10 ms (scaled by cell count). Shrinking the grid stays the last resort.

## Dependencies

**Requires:**
- **S-1 World Generation, amended.** It must provide:
  - a hex grid with a 6-neighbour topology helper (CoW-Shared)
  - per-cell `elevation`, `latitude` and `water_source`
  - read access to the current world (module accessor), since `WorldGenerationComplete` stays a small notification
  - regions for `set_wind_region` and `find_droughts`

  **A4 Features 3–6 are blocked until S-1 is amended and built.**
- **`JobSystemService` ServiceStream + `JobSystem::TrySubmit` (dia-needs #12, approved 2026-10-14, not built).** This is a CluicheGameBaseline change plus one Dia method. Feature 3 uses `SerialExecutor` until it lands. Features 1–2 don't need it.
- GameSpeedController (`SimulationTick`, season from week), Done.
- DiaMessageBus via `MessageBusHostModule`.
- E1 `SaveLoadModule`, Done (soft: A4 runs without it; S-2.5 has no SaveLoadModule).
- `VisualDebuggerModule`, plus `DebugLayerManager` for drawers.
- **Dia CellField renderer (dia-needs #14, Approved 2026-10-15, not built; amended 2026-10-19, product review).** Per-cell colour layer for the nine weather overlays. Blocks Feature 5's drawers. S-2.5's overlays reach CellField through A4's debug domain.
- **DiaCellLayers (Approved 2026-10-19, not built; amended 2026-10-19, DiaCellLayers/DiaCellHistory).** Layer registration for A4's eight base layers and wind overlay. Blocks Feature 5's drawers.
- **DiaCellHistory (Approved 2026-10-19, not built; amended 2026-10-19, DiaCellLayers/DiaCellHistory).** Recording, scrubbing, snapshots and event emission for droughts and storms. Feature 5 uses `WeatherHistory` until DiaCellHistory is built.
- DiaAPI `CommandRegistry`.
- DiaCore (Json, StringCRC, containers, logging).

**Soft dependencies (fallbacks until they exist):**
- A1 implements `IBiomeSeasonalBaseline`. Until then A4 uses the table fallback.
- A3 publishes `BiomeTransitionComplete`. Until then biomes are fixed after world generation.

**Required by:** A1 (§13 weather integration), V-1 (weather rendering), S-2.5 (A4-isolated testbed), S-3 (A4 overlays and emergence), D4 later.

## Test Strategy

**Unit (Features 1–2, GoogleTests, hand-built hex terrain):**
- Parameter load: valid file, out-of-range values, missing keys → defaults.
- Initialise: seeding rules; no water source → false (EC-01).
- One test group per pipeline step, using each step's GDD worked expectations:
  - pressure: temp 1.0 → 0.0
  - wind: uniform pressure gives prevailing wind only; a high-pressure cell gives divergence
  - evaporation: saturation self-limits
  - transport: downwind move and edge retention
  - orographic: windward rain and dry leeward
  - precipitation: rain vs snow split
  - melt: thresholds
  - drainage: about 0.33 after 5 weeks
- Edge cases EC-03 to EC-13, EC-16 and EC-20.
- Upwind edge inflow: on, a uniform field under steady wind stays uniform at the edges. Off, the upwind edge loses moisture each week (the GDD-literal behaviour).
- Determinism:
  - the same inputs give bit-identical fields
  - `SerialExecutor` vs a test executor with 1, 3 and 17 ranges, and with ranges run in reverse order, all give bit-identical fields after 52 weeks

**Integration (Features 3–4):**
- No step without a week band. Exactly one `WeatherUpdated` per week. No broadcast during spin-up.
- Hot reload takes effect at the next step only.
- Save at week 26, continue to 52, load week 26, continue to 52: identical (A4-042).

**Emergence (Feature 6):** AuthoringDebug §6.1–6.4 over hand-built hex terrains, using the GDD's ratios. Each preset loads and runs 13 weeks without health errors. Checking the patterns by eye in a running world is S-2.5's job.

**Performance (Feature 2 gate):** one `Step()` at Large (104 × 64) in Release with the job executor, reported against the 10 ms budget, plus the per-step `profile` breakdown.

## Resolved Design Decisions

1. **Upwind world edges use a zero-gradient inflow.** An edge cell whose upwind neighbour is off the map receives an inflow equal to its own outflow.
   - Rejected: the GDD's literal hard boundary, which leaves fake cold, dry strips along upwind edges and probably fails the coastal-rain criterion.
   - Rejected: moving temperature differences instead of raw temperature. That changes §8 throughout the map and doesn't fix moisture.
   - The `boundary.edge_inflow` toggle keeps the GDD-literal behaviour available in S-2.5 for comparison.
2. **The update is parallel from the start and runs in one frame.** Passes are split into cell ranges through `IParallelExecutor`. The order-independent gather makes the result bit-identical at any range count. The budget is ≤ 10 ms at Large (104 × 64) in Release (amended 2026-10-19, product review; was 200×200), enforced as a reported gate in Feature 2.
   - Rejected: spreading the update over the week's 7 day ticks. A save could then land mid-pipeline, which brings back the `is_update_in_progress` problem.
   - Rejected: a single-threaded update first, with parallelism added only if the budget is missed. It's cheaper to build the parallel structure while the pipeline is being written than to retrofit it.
3. **Saves use JSON text** (dia-needs #11 blob API was rejected 2026-10-15 after the buffer was raised to 100 MB; amended 2026-10-19, product review). At Large (6,656 cells), A4's 9 fields as JSON doubles (`%.17g`) are about 1.9 MB. Combined with A1's ~35 fields/cell, the total ecology + weather save is projected at ~10-15 MB at Large, well within the 100 MB buffer limit. Text JSON is simpler than binary blobs, human-readable for debugging, and requires no base64 codec. Float precision is exact: jsoncpp writes doubles with `%.17g`, so floats round-trip bit-identically.
   - Rejected: blob API with base64. It would make weather smaller than text JSON, but the 100 MB buffer makes that unnecessary.
   - Rejected: quantising fields to fixed point. The simulation would have to run in fixed point too to stay deterministic.

4. **SimPU gets the job system from a `JobSystemService` `ServiceStream` and submits with `TrySubmit`.** See Parallel execution (dia-needs #12, approved 2026-10-14, not built).
   - Rejected: a private pool inside A4. It would mean two pools competing for cores, and every later SimPU system would repeat it.
   - Rejected: the stream alone without `TrySubmit`. Quitting during a weekly update would hit the `Submit`-after-`Shutdown` assert in Debug builds.
   - Rejected: changing the app's PU stop order. That affects every application.

## Open Design Questions

None remain at system level. Feature-level questions go in each feature spec.

## Status

**Status:** `Approved` (2026-10-14). dia-needs #11 (blob API) was rejected 2026-10-15, so Feature 4 uses JSON text saves and isn't blocked. dia-needs #12 (`JobSystemService` + `TrySubmit`) is Approved but not built; the parallel path of Feature 3 waits on it and runs serially until then (amended 2026-10-19, consistency pass).
