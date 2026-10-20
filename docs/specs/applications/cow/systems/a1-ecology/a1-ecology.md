# System Spec: A1 Ecology

**Parent:** @docs/specs/applications/cow/cow.md  
**Design source:** `Output Docs/GDD/08_SystemsDeepDive/A1_Ecology/` (DesignDocs repo; `A1_EcologySystem.md`, `A1_TechnicalSpec.md`, `A1_TuningParameters.md`, `A1_AuthoringDebug.md`, `A1_EdgeCaseMatrix.md`, `A1_PrototypeTestbed.md`, `A1_ProductionBacklog.md`, all Locked 2026-10-12)  
**Conflicts resolved here:** [a1-gdd-conflicts.md](a1-gdd-conflicts.md) (C1–C14, interviewed 2026-10-18)

**Gameplay Domains:** persistence, debug

## Summary

A1 is CoW's ecology. Every hex cell carries producer biomass, species populations (juvenile and adult), transients, pressure fields and history. Seasonal, yearly, decadal and century rules run over those values, and booms, crashes, migrations, drought, disease and recovery emerge from them; nothing is spawned on a schedule.

A1 is fed by three systems. S-1 provides the world structure, A4 provides weekly rainfall, temperature and snow, and GameSpeedController provides day ticks. A1 tracks the stress that builds toward biome succession and tells A3 when a threshold is crossed. A3 decides the transition and applies it back through A1's API. A1 also implements A4's `IBiomeSeasonalBaseline`.

This spec builds **Layer 1** of the GDD's three layers: the analytical cell layer, which always runs. Dragon inputs (tick step 9) are defined as an interface only. Population tokens (Layer 2) and local instantiation (Layer 3) are deferred.

**Map sizes** (amended 2026-10-19, product review): Small 66×42 (2,772 cells), Standard 80×52 (4,160 cells), Large 104×64 (6,656 cells, release performance gate for S-1, A4, A1). S-1 now builds and tests all three sizes.

## Goals

1. **Species-driven, data-only roster.** Species, biomes and succession rules load from JSON. Code never names a species or functional group (TechnicalSpec §1a).
2. **Band-gated tick pipeline.** The GDD steps run at the Season, Year, Decade and Century boundaries of GameSpeedController's day ticks, each as a full-grid pass, with no `delta_scale`.
3. **Pure cell logic, testable alone.** The per-cell equations run on one cell with no neighbours, no module and no weather events (GoogleTests, `isolate_cell`). The same code runs grid-wide when fed by A4 (S-3 Ecology, Living mode).
4. **Defined ecology state.** Thriving, Stable, Stressed and Collapsed come from a hysteretic health index (C4).
5. **Succession counters for every A3 path.** A data rule table covers every A3 transition. A1 evaluates it and raises `BiomeSuccessionTriggered` with a `pressure_source` (C7).
6. **Deterministic.** Same world, data, weather and tick count give bit-identical state at any thread count. Randomness comes only from the `A1` stream of `RandomStreamModule`.
7. **Save/load.** Ecology state round-trips through E1, and a reload continues bit-identically.
8. **Observable.** A debug domain with per-cell overlays, a cell inspector, `cow.ecology.*` DiaAPI commands, and profiling, metrics and health.
9. **Performance** (amended 2026-10-19, product review). Season band ≤ 16 ms in Release at Large (6,656 cells) with about 15 species. Small and Standard budgets scaled by cell count: Small (2,772 cells) ≤ 6.7 ms, Standard (4,160 cells) ≤ 10.0 ms. Reported gate.

## Non-Goals

- **Dragon pressure (step 9), hunts, Full Fight and Cultivated Ground.** Interfaces are declared (see Public Interfaces); they are built when S-4 needs them.
- **Population tokens (Layer 2) and local instantiation (Layer 3).** Deferred. S-3 Ecology sees migration through density overlays and flow arrows.
- **Succession transition rules, legacy modifiers and recovery counters.** These belong to A3. A1 provides the counters and the apply API.
- **Weather.** A4 owns rainfall and temperature variation. A1 does not roll `year_quality` (C6).
- **Event display and naming.** These belong to D4. A1 only detects thresholds.
- **World generation.** A1 derives what it needs from S-1's `WorldStructure` and never writes it.
- **Player-facing visuals.** These belong to V-1. A1's overlays are debug-only.
- **Year-quality scale.** The GDD's `YEAR_QUALITY_*` values (GOOD 1.3, AVERAGE 1.0, BAD 0.7) belong to climate-control sliders (S-3's pinned-climate input), not to A1 fields (amended 2026-10-19, product review).

## Architecture

### Placement

**CoW-Shared** (AD-002). A1 is consumed by A3, A4, S-3 Ecology and later S-4, V-1, B1, B2, C3 and D4. Pure logic lives in `Cluiche/CoW/Shared/Ecology/`. The SimModule, debug domain and save participant live in `Cluiche/CoW/Modules/`. Test helpers (hand-built grids, a stub weather input, golden rosters) ship in `Cluiche/CoW/Shared/Ecology/Testing/`.

```
SimPU
  └─ EcologyModule (SimModule)
       on WorldGenerationComplete → EcologySimulation::Initialise(world, data)
       on WeatherUpdated{week}    → AccumulateWeek(A4 snapshot); week closes season → mSeasonPending
       next sim frame with mSeasonPending (or SimulationTick band >= Season when input is the stub)
                                  → StepBands(bands) → publish CellEcologyChanged / EventThresholdCrossed /
                                    BiomeSuccessionTriggered
       implements IBiomeSeasonalBaseline (A4 finds it via ModuleRef)
       owns: EcologySimulation, EcologyData, save participant, debug domain
```

### Grid

Dia's `HexTopology` / `HexPathGrid(width, height)`: flat-top, odd-q offset, cell index `y * width + x`, directions `0..5` (NE, N, NW, SW, S, SE), `kNoCell` past hard edges, no wrap (C10). This is the same definition S-1 and A4 use. The GDD's axial equations are applied through `NeighbourAt`, and nothing in A1 assumes axial coordinates or an off-map world.

### Biomes (C1, C2)

One shared `StringCRC` id set, used by S-1, A1, A3, A4 and `species.json`:

| Group | Ids |
|-------|-----|
| Primary (S-1 produces) | `Grassland`, `TemperateForest`, `Wetland`, `Desert`, `Alpine`, `Tundra`, `Rainforest`, `Volcanic` |
| Succession outputs (A3 produces) | `Scrub`, `BurnedAshland`, `DesertifiedPlain` |
| Water (S-1 produces) | `Ocean`, `Lake` |

Lore names (Verdant Lowlands, Stormridge, Ancient Interiors) are regions or depth zones, not biomes. `Volcanic` (primary, eruption ash) and `BurnedAshland` (fire succession) are different ids. A3 uses `BurnedAshland` consistently (amended 2026-10-19, product review). `biomes.json` gives every id a `BiomeProfile`, including its seasonal temperature for `IBiomeSeasonalBaseline` and a seasonal `rainfall_baseline` that S-3's pinned climate uses (the GDD's original per-biome seasonal patterns, TechnicalSpec §1b). An unknown id returns 0.5 and logs once (A4 EC-19).

### State (structure of arrays)

`EcologyFields` holds one `DynamicArrayC<float>` per scalar field, indexed by cell, so passes stream, saves are arrays and the parallel split matches A4's.

- **Abiotic:** `biome` (palette index), `elevation`, `slope`, `water`, `rainfall_accumulator`, `temperature`, `snow_covered`, `soil_fertility`, `soil_depth`.
- **Producers:** `grass`, `shrub`, `tree`, `aquatic`, `seed_bank`.
- **Consumers:** dense `[species × cell]` arrays, `juvenile` and `adult`, as fractions of `max_density_per_cell` [0, 1] (C14). A species absent from a cell is 0. Functional-group totals are computed on demand from tags.
- **Transients:** `carrion`, `waste`, `ash`, `disease_load`, `fire_intensity`, `drought_stress`.
- **Pressure fields:** `prey_scent`, `predator_scent`, `dragon_scent`, `carrion_scent`, `water_gradient`.
- **Ecology state:** `ecology_state`, `health`, `state_streak` (signed seasons past a band edge), `recovering` flag.
- **Succession:** `succession_pressure[rule × cell]` (C7, added to the GDD schema).
- **History:** `years_since_fire`, `drought_years`, `last_dragon_visit`, `local_extinction_flags`, `years_low_herbivore_inflow` (consecutive years in which herbivore migration in was below `low_inflow_threshold`; read by A3's tundra rule; A3 amendment 4; saved state field tracked per cell, not read from the ledger — amended 2026-10-19), plus the tended fields (declared, unused until step 9).
- **Static geography (derived at init, never changed by the simulation):** `river_overlay`, `corridor_node`, `hearth_site`, `nexus_node`, `world_throne_type`, `route_memory` (C11).

`year_quality` and the deprecated `rainfall` field are not stored (C6).

### Initial state (C11)

`Initialise(world, data)` derives everything A1 needs from S-1. S-1's schema is unchanged.

| Field | Source |
|-------|--------|
| `slope` | Largest elevation difference to any neighbour, normalised |
| `river_overlay` | `waterSource == River` |
| `corridor_node` | `terrainFlags & Pass` |
| `hearth_site`, `nexus_node`, `world_throne_type` | S-1 Feature 9 sites. All false until Feature 9 is built |
| `soil_fertility`, `soil_depth` | `BiomeProfile` defaults, varied ± by seeded noise (DiaMaths, `A1` seed) and reduced by slope |
| `route_memory` | Base value, raised on river and corridor cells (EcologySystem §5.1, §5.3) |
| Producers, populations | `BiomeProfile` starting values. Species are seeded where biome affinity is above zero, at affinity × the biome's starting density |
| Ecology state | `Stable`, streak 0 |

### Tick pipeline (C5)

GameSpeedController publishes one `SimulationTick{day, band}` per day and never skips days, so every boundary is visited exactly once at any speed. A1 runs each step **once per boundary at that band's rate**. The GDD's `delta_scale` ×4/×40/×400 is dropped. The schedule follows EcologySystem §7 (TechnicalSpec §11's pseudo-code disagrees and is superseded):

| Band | Steps |
|------|-------|
| Season | 1 weather (consume accumulator), natural ignition and fire spread, 2 water flow, 3 grass and aquatic, 4 foraging, 8 migration intent, 9 scent (no-op until S-4), ecology state, resolver `OnSeason`, 12 evidence |
| Year | 5 predation and carrion, 6 starvation and disease, 7 reproduction, 8 full move, 9 full (no-op), 11 guardrails, succession rules with `band: Year` + resolver `Resolve` (step 10), resolver `OnYear` |
| Decade | 2 water table, 3 shrubs and trees, 10 succession rules with `band: Decade`, seed drift |
| Century | 10 major succession, 11 legacy recolonization |

The Year, Decade and Century boundaries are also Season boundaries (364 = 4 × 91). Bands run smallest first on the same tick. **Each step is a full-grid pass, step-outer** (every cell finishes step N before any cell starts N+1, EcologySystem §6). Steps that read neighbours read the stable copy taken at the start of the pass and **gather** inflow in direction order 0..5, as A4 does, so the result does not depend on iteration order. Every modified field is clamped at the end of each step (TechnicalSpec §3).

### Weather input (C5, C6)

A1 reads weather through `IEcologyWeatherInput` (per-cell `Rainfall`, `Temperature`, `SnowCover`), not through A4 directly.

- **Living mode and the game:** `WeatherSnapshotInput` adapts A4's `WeatherSnapshot`. On each `WeatherUpdated{week}`, A1 adds `Rainfall` into `rainfall_accumulator` and takes the latest `Temperature` and `SnowCover`.
- **Timing:** the 13th week of a season arrives in the bus's Reaction pass, which can't post. A1 sets `mSeasonPending` and runs the season pipeline on the next sim frame, so week 13 always counts toward its own season.
- **Controlled mode and tests:** `StubWeatherInput` (Testing/) supplies slider or scripted values. Its GOOD/AVERAGE/BAD selector scales the stub's rainfall; this is not an A1 field. With the stub, the module steps on `SimulationTick` with `band >= Season`.

### Ecology state (C4)

Each Season, after step 4, A1 computes a health index `H` in [0, 1] per land cell:

```
H = w_prod * clamp(producer_biomass / biome.producer_target)
  + w_cons * clamp(consumer_total  / biome.consumer_target)
  + w_water * clamp(water / biome.water_optimum)
```

Default bands: Thriving ≥ 0.75, Stable ≥ 0.50, Stressed ≥ 0.25, otherwise Collapsed.

The state moves **one tier at a time**, and only when the band is crossed for long enough:

- down after `state_down_seasons` seasons in a row below the current tier (default 2)
- up after `state_up_seasons` seasons in a row above it (default 4)

So a single bad season never drops a cell, and recovering is slower than degrading (EcologySystem §4). `recovering` is set while a Stressed or Collapsed cell's `H` has risen for `recovering_seasons` in a row. It is a flag, not a fifth state: S-2's "Depleted" is Collapsed, and its "Recovering" is this flag. Weights, bands and streaks are new tuning constants in `ecology_parameters.json`.

### Succession rules (C7, C9)

`succession_rules.json` lists one rule per A3 transition: `{id, from, to, source, direction, band, conditions[], increment, decay}`.

- `direction` is `forward` (degradation and fire) or `recovery` (Scrub→Grassland, BurnedAshland→Grassland, DesertifiedPlain→Scrub).
- Recovery rules use `decay: 0`, so stalled progress is kept.
- A recovery rule's increment is multiplied by the resolver's `GetRecoveryMultiplier(cell)` (dragon acceleration, 1.0 until S-4).
- **Content ownership** (amended 2026-10-19, product review): A3 owns the rule-table contents. A1 ships an initial file translated from A3 TechnicalSpec §4 so A1 can be exercised before A3 exists. A1 evaluates every rule and provides the file schema; A3 authors the conditions and increments.

- `source` is one of `DROUGHT`, `FIRE`, `OVERGRAZE`, `DISEASE`, `DRYING` or `COLD`.
- `conditions` are `{field, op, value}` tests over cell fields and history (for example `drought_years > 3`, `seed_bank < 0.2`).

At each rule's band, for every cell whose biome matches `from`:

- all conditions true: the counter rises by `increment`
- otherwise: it decays by `decay`, never below 0
- counter ≥ 1.0: the rule raises a trigger `{cell, to, source}`

At step 10, A1 hands the band's triggers, in cell-index order, to the `ISuccessionResolver` (A3) if one is registered. A1 also publishes every trigger, accepted or rejected, as `BiomeSuccessionTriggered`, so observers can see them. With no resolver, the triggers are only published.

`GetSuccessionProgress(cell)` (the highest counter) feeds A3's 50% Stressed visual.

A3 applies transitions only through A1's API (C9), synchronously inside step 10. A1 owns every cell field:

- `ApplyBiomeTransition(cell, to, LegacyDelta)` sets the biome, applies the legacy deltas **once** and clamps them, and resets **every** succession counter for the cell, forward and recovery.
- There is no `RemoveLegacy`. Legacy deltas are one-shot, and A1's own processes recover them. A3's legacy marker is for presentation only (A3 amendment 2).

`EcologyModule` publishes the resolver's output (transitions, rejections, recovery stages) at the end of the band, as `BiomeTransitionComplete` and `BiomeRecoveryProgressed`. Every event goes out once, at the end of the band that produced it (A3 amendment 1).

### Natural fire (A3 amendment 3)

Without dragons, nothing would start a fire, so BurnedAshland could never emerge. A1 adds natural ignition:

- **When:** at the Season band, each land cell may ignite with `p = base_ignition × drought_fit(drought_stress) × fuel × temperature_fit`, where fuel is grass + shrub + tree biomass.
- **Determinism:** draws use the `A1` stream in cell-index order.
- **Spread:** fire spreads with `fire_spread_rate`, burns biomass into ash, and sets `fire_intensity`, which A3's fire rules read.
- **New constants:** `base_ignition`, `ignition_drought_fit`, `ignition_temperature_fit`.
- **By hand:** `cow.ecology.ignite` starts a fire.

### Aquatic ecology and coastal feeding (C3)

Ocean and Lake cells run an **aquatic-only** ecology: `aquatic` biomass plus species with `"habitat": "aquatic"` (same `species.json` schema). Aquatic species also live in river and wetland land cells. Land species treat Ocean and Lake as impassable, and aquatic species can't move onto dry land cells.

**Coastal cross-feeding:** a land predator or scavenger whose diet lists an aquatic group can hunt aquatic populations in **adjacent** water cells, scaled by `coastal_reach`. Each side gathers from the stable copy: the water cell computes its losses from neighbouring land predators, and the land cell computes its food gain from neighbouring aquatic prey. This keeps the result deterministic. Water cells compute `H` from aquatic biomass and aquatic consumers only.

Water cells are permanent `water_gradient` sources and answer `IBiomeSeasonalBaseline` like every other biome.

### Guardrails (C10)

There is no off-map world. **Lifeboat sources** replace it:

- map-edge land cells
- refugia cells (slope or tree cover above a threshold, or wetland)

At each Decade boundary, a Collapsed land neighbour of a lifeboat source has `off_map_recolonization_rate` (default 0.02) chance per missing functional group to receive a small founding population of a species with affinity for its biome. The draws use the `A1` stream in cell-index order. A global extinction floor keeps every species above zero somewhere if it has any affinity on the map.

### Events

At the end of each band, A1 publishes:

- **`CellEcologyChanged{cell, ecology_state, biome}`:** once per cell whose ecology state or biome changed.
- **`EventThresholdCrossed{type, anchor_cell, cell_count}`:** for TechnicalSpec §10's connected-cell clusters (drought front, predator wave, disease bloom), found by flood fill over cells past the threshold.
- **Recolonization** (C13): a new type, raised when a lifeboat founding takes. The GDD lists it but never defines it.

The herd-migration token event and the dragon kill field wait for Layer 2 and step 9. Payloads are small POD. The cluster cell lists are read through `GetLastEvents()`.

### Parallel execution and performance

Passes are split into cell ranges through the same `IParallelExecutor` seam as A4: `SerialExecutor` first, then a `JobSystemService` executor with `TrySubmit` and an inline fallback. Each range writes only its own cells, and reductions are serial in range order, so results are bit-identical at any range count.

The season band is gated at ≤ 16 ms in Release at Large (6,656 cells) with about 15 species. Missing it is reported but doesn't fail the build. This replaces S-3's stale 200×200 target.

### Save/load (E1)

`EcologySaveParticipant` writes every non-static `EcologyFields` array as a JSON array, plus the species and biome palettes (CRC lists). Floats are stored as doubles (`%.17g`) so they round-trip bit-exactly. Static geography is re-derived from the world on load. The estimated size is about 35 fields × 6,656 cells, inside the 100 MB buffer.

### Change ledger (why values changed)

*Amended 2026-10-18.* A1 is meant to be understood, not just watched, so every step records **why** values changed, not only the results. Each band, `EcologyLedger` holds per-cell, per-species deltas by cause:

- **Populations:** births, maturation, deaths by starvation, predation (by predator species), disease, Allee and base mortality, migration in and out (by direction), lifeboat founding.
- **Producers:** growth, grazing (by species), wilt, fire.
- **State:** the health index terms, and which succession rule conditions passed or failed.

The ledger covers the last band only, is overwritten each band, is never saved, and doesn't feed back into the simulation. The equations write their terms to it as they compute them, so it can't drift from what actually ran. A build flag can turn it off if the 16 ms gate needs the time. Aggregate calculations (e.g., A3's yearly inflow checks) use saved state fields, not the ledger (amended 2026-10-19, product review).

### Debug, tuning and editing tools

*Amended 2026-10-18, 2026-10-19.* The tools for understanding and tuning ecology belong to A1, as A4's do, so every stage that hosts A1 gets them: the Ecology testbed, and later the dragon testbeds. Every action is a `cow.ecology.*` DiaAPI command, and the panel calls the same commands. The commands and their execution model come first; the panel depends only on them.

`EcologyDebugDomain` (SimPU) provides:

- **Panel:** season, year, cell count, state distribution, functional-group totals, last band profile, data source (file or fallback roster), weather input (A4 or stub), A3 present or not.
- **Overlays (amended 2026-10-19, DiaCellLayers/DiaCellHistory):** ecology overlays become registered DiaCellLayers layers (ecology state, health, biome, each producer, species density or functional-group density, each pressure field, succession progress). Top-3 migration arrows become an overlay through debug draw. Paint can apply to the current selection. Event clusters shown as overlays.
- **Cell inspector (amended 2026-10-19, DiaCellLayers/DiaCellHistory):** single-cell view uses `describeCell` to show every field of the selected cell, including per-species juvenile/adult counts and maturation, plus the **change ledger** for that cell: each value's change last band, broken down by cause.
- **Graphs (amended 2026-10-19, DiaCellLayers/DiaCellHistory):** a rolling time-series panel showing the selected cell's per-species juvenile/adult counts and producers, plus map-wide functional-group totals and the state distribution. Map-wide totals and the state distribution read DiaCellHistory aggregates, so they cover the whole run and are recorded whether or not the panel is open. Per-cell series need that layer to be tracked (`cells.history.track`). Long-run analysis lives in the World Sim Inspector.
- **Tuning:** live sliders over `ecology_parameters.json` and the selected species' and biome's entries, applied at the next band. `cow.ecology.tuning.save` writes changes to the defaults files themselves via the `source` alias (dia-needs #16, F-3 World Assets; amended 2026-10-19). `cow.world.pack.save_as ecology <id>` is the normal tuning path, writing overrides-only ecology packs to `Assets/CoW/Global/Ecology/Packs/<id>.ecology.json`. Hot reload picks up hand edits.
- **Editing:** set a cell's fields or a species' population, and paint species, producers, water or biome over a brush of cells. Painting a biome reinitialises the painted cells from the biome's starting values. Painted/edited cells can be saved as an ecology start state via `cow.world.startstate.save_as` (baked terrain only; F-3 World Assets, amended 2026-10-19), written to `Assets/CoW/Global/Ecology/StartStates/<id>.ecostate.json`. `isolate_cell` turns off migration and propagation into and out of the selected cell, which gives the old single-cell S-2 behaviour.
- **Commands:** `inspect`, `ledger`, `set_field`, `set_species`, `paint`, `isolate_cell`, `set_ecology_state`, `step_band`, `reload_data`, `tuning.set`, `tuning.save`, `find_events`, `ignite`, `apply_transition` (until A3).
- **Instrumentation (amended 2026-10-19, DiaCellLayers/DiaCellHistory):** `DIA_PROFILE_SCOPE` per step, `MetricRegistry` counters (cells per state, events per band, band time), an `IHealthReporter` (Degraded on the fallback roster or a failed data validation). Ecology state changes and crashes are emitted as DiaCellHistory events.

S-3 Ecology hosts these. A1 owns no stage.

### Logging

*Added 2026-10-19, product review.* A1 logs on the `ECO` channel:

- **Info:** load/reload complete (species and biome counts, file paths), tuning write-back (file updated), guardrail-reset events (recolonization, extinction-floor recovery).
- **Warning:** unknown biome or species id falls back to default 0.5 affinity (logged once per id), clamped-to-range values (field name, cell, range), parameter out-of-range uses fallback (parameter name, provided value, fallback).
- **Error:** data-file schema validation fails (file path, JSON keys missing or wrong type), `EcologyWorldInput` and `WorldStructure` dimension mismatch (expected vs actual).

## Public Interfaces

```cpp
namespace CoW::Ecology {

    // Pure logic, no Dia module dependencies. Unit-tested directly.
    class EcologySimulation {
    public:
        bool Initialise(const EcologyWorldInput& world, const EcologyData& data, uint64_t seed);
        void AccumulateWeek(const IEcologyWeatherInput& weather);           // one A4 week
        void StepBands(TimeBand band, IParallelExecutor& executor);         // runs every band <= band, smallest first
        void StepCell(uint32_t cell, TimeBand band);                        // one cell, no neighbour passes (tests)

        // A3 apply API (C9). A1 owns every cell field.
        void ApplyBiomeTransition(uint32_t cell, Dia::Core::StringCRC to, const LegacyDelta& legacy);   // one-shot deltas
        void SetSuccessionResolver(ISuccessionResolver* resolver);          // A3; nullptr = triggers only published
        float GetSuccessionProgress(uint32_t cell) const;

        // Dragon interface (step 9). Declared; queued and ignored until S-4.
        void QueueDragonInput(const DragonEcologyInput& input);

        EcologyView GetView() const;                                        // const accessors for debug, A3, Ecology testbed
        const EcologyLedger& GetLastLedger() const;                         // why values changed last band
        void SetIsolated(uint32_t cell, bool isolated);                     // debug: no migration/propagation in or out
        const EcologyEvents& GetLastEvents() const;
        const EcologyStepProfile& GetLastProfile() const;
        void RestoreState(const EcologyFields& fields);
    };

    class IEcologyWeatherInput {                                            // C5 seam
    public:
        virtual ~IEcologyWeatherInput() = default;
        virtual float Rainfall(uint32_t cell) const = 0;
        virtual float Temperature(uint32_t cell) const = 0;
        virtual bool  SnowCover(uint32_t cell) const = 0;
    };

    class ISuccessionResolver {                                            // A3 seam (A3 amendment 1); declared here so A1 doesn't depend on A3
    public:
        virtual ~ISuccessionResolver() = default;
        virtual void Resolve(const TriggerList& triggers, EcologySimulation& sim, SuccessionOutput& out) = 0;   // step 10
        virtual void OnYear(const EcologyView& view, SuccessionOutput& out) = 0;
        virtual void OnSeason() = 0;
        virtual float GetRecoveryMultiplier(uint32_t cell) const = 0;
    };

    struct EcologyData { /* species roster, biome profiles, succession rules, parameters */ };
    bool LoadEcologyData(const Json::Value& species, const Json::Value& biomes,
                         const Json::Value& rules, const Json::Value& params, EcologyData& out);
    EcologyData FallbackEcologyData();      // 3 land species (deer, elk, wolf) + 1 aquatic (fish), S-2 GDD decision 4

    class EcologyModule : public Dia::ApplicationFlow::SimModule,
                          public CoW::Weather::IBiomeSeasonalBaseline,
                          public IWorldAssetParticipant {      // F-3 World Assets, amended 2026-10-19
    public:
        float GetSeasonalTemperature(Dia::Core::StringCRC biome, CoW::Weather::Season season) const override;
        const EcologySimulation& GetSimulation() const;
        void RegisterSuccessionResolver(ISuccessionResolver* resolver);    // A3's SuccessionModule, via ModuleRef
        // IWorldAssetParticipant (kinds `ecology`, `ecology_start`): ecology pack and start state
        Dia::Core::StringCRC Kind() const override;
        Json::Value Defaults() const override;
        void Apply(const Json::Value& resolved) override;
        Json::Value Capture() const override;
    };
}
```

Messages (`CellEcologyChanged`, `EventThresholdCrossed`, `BiomeSuccessionTriggered`) are hand-written structs following the `GameSpeedMessages.h` pattern, in `Cluiche/CoW/Modules/EcologyMessages.h`.

## Features

| # | Feature | Description | Spec | Status |
|---|---------|-------------|------|--------|
| 1 | Ecology Data | `species.json` (incl. `habitat`, aquatic groups, `coastal_reach`), `biomes.json` (13 ids, `BiomeProfile`, seasonal temperature and `rainfall_baseline`), `succession_rules.json`, `ecology_parameters.json`: load from `Assets/CoW/Global/Ecology/` with envelopes `cow.species`, `cow.biomes`, `cow.ecology_params` (amended 2026-10-19, F-3 World Assets), validate, defaults, fallback roster. Pure logic, TDD | TBD | Draft |
| 2 | Cell State & Initialise | `EcologyFields` SoA, palettes, `EcologyWorldInput` from `WorldStructure`, derived geography (C11), seeded soil, starting populations, `IBiomeSeasonalBaseline` implementation. Pure logic, TDD | TBD | Draft |
| 3 | Cell Equations | Per-cell steps 1, 3–7 and 12: producers, per-species births, maturation, Allee, competition, starvation, predation (age-weighted), disease, clamping; `StepCell`; `EcologyLedger` deltas by cause, written as each term is computed (ledger sums equal the actual change). One test group per equation from TechnicalSpec §4–§5. Pure logic, TDD | TBD | Draft |
| 4 | Grid Passes | Steps 2, 8 and 11 plus pressure-field propagation: water flow and table, migration solver (group and softmax, river and corridor modifiers), coastal cross-feeding, lifeboat guardrails, seed drift, natural ignition and fire spread, `years_low_herbivore_inflow`; step-outer passes, gather, `IParallelExecutor`, bit-identity, 16 ms gate. Pure logic, TDD | TBD | Draft |
| 5 | Ecology State & Succession | Health index plus hysteresis (C4), succession rule evaluation, forward and recovery, with the recovery multiplier (C7), the `ISuccessionResolver` seam at step 10, `BiomeSuccessionTriggered`, one-shot `ApplyBiomeTransition` (C9). Pure logic, TDD | TBD | Draft |
| 6 | Event Detection | Connected-cell clustering for drought front, predator wave, disease bloom; recolonization; `CellEcologyChanged` change set. Pure logic, TDD | TBD | Draft |
| 7 | EcologyModule | SimModule: data load and hot reload, `WorldGenerationComplete` init, `WeatherUpdated` accumulation via `WeatherSnapshotInput`, `mSeasonPending` scheduling, stub-input mode, band gating, publishing, `IBiomeSeasonalBaseline` via `ModuleRef`, `JobSystemService` executor, `cow_main.diaapp` entry | TBD | Draft |
| 8 | Ecology Save/Load | `EcologySaveParticipant`: JSON arrays plus palettes, validate, save/continue/load/continue round-trip | TBD | Draft |
| 9 | Ecology Debug & Tools | `EcologyDebugDomain`: command model first, then panel, inspector with change ledger, CellField overlays, migration arrows, graph panel, live tuning sliders + `tuning.save` write-back, cell edit/paint, `isolate_cell`, profiling, metrics, health. Needs an HTML mockup before build | TBD | Draft |

**Routed elsewhere:** the stage, its Controlled and Living modes, the guided proof scenarios (the old S-2 tests) and the long-run checks (the old S-3 tests) go to **S-3 Ecology** (S-2 and S-3 merged, 2026-10-18). Transition rules, legacy and recovery go to **A3**.

**Build order:** 1 → 2 → 3 → 5. Feature 4 follows 3. Feature 6 follows 4 and 5. Feature 7 follows 6. Features 8 and 9 follow 7 and can run in parallel.

- Features 1–6 need no S-1, A3 or A4 code: their tests use hand-built hex grids and `StubWeatherInput`.
- S-3 Ecology needs Features 1–7 and 9, plus S-1 Features 1 and 8 (its worlds come from S-1).

## Binding Decisions

| ID | Decision | How it applies |
|----|----------|----------------|
| PD-001 | StringCRC for ids | Biome, species, functional group and rule ids are `StringCRC`. Fields store palette indices |
| PD-002 | ProcessingUnit/Phase/Module architecture | `EcologyModule` is a SimModule on SimPU. A4 finds the baseline provider and A3 finds the apply API through `ModuleRef`, never through singletons |
| PD-004 | No STL containers in public APIs | `EcologyFields` uses `DynamicArrayC`. Views expose accessors, not `std::` types |
| AD-001 | Ask permission before Dia/baseline edits | None needed. A1 uses existing DiaMessageBus, DiaSaveGame, `JobSystemService` (Done 2026-10-15), DiaMaths noise (Done 2026-10-15), DiaAPI `CommandRegistry`. CellField (dia-needs #14) is Spec Approved, not built (amended 2026-10-19, product review) |
| AD-002 | Four-tier dependency model | CoW-Shared: A3, A4, S-3 Ecology and later systems consume it |
| AD-003 | GDD owns design intent | GDD A1 owns the mechanics and constants. Deviations are listed below for the GDD owner |
| AD-005 | One stage per testbed | A1 owns no stage. S-3 Ecology hosts it |

## Deviations from GDD A1

These are for the DesignDocs owner to fold back into A1, A3, S-2 and S-3:

1. **One biome id set** (C1, C2): 8 primary + `Scrub`, `BurnedAshland`, `DesertifiedPlain` + `Ocean`, `Lake`. `species.json` affinity uses these ids. The lore names in the TechnicalSpec §1a, S-2 and S-3 examples are regions.
2. **Ocean and Lake run aquatic-only ecology, with coastal cross-feeding** (C3). This is new content: an aquatic roster and `coastal_reach` need authoring.
3. **Ecology state formula is new** (C4): a health index, one tier per move, `state_down_seasons` and `state_up_seasons`. S-2's five states become four plus a `recovering` flag.
4. **No `delta_scale`** (C5). Each band's steps run once per boundary at that band's rate. EcologySystem §7's schedule wins over TechnicalSpec §11's pseudo-code. Steps are step-outer full-grid passes.
5. **The season pipeline runs on the sim frame after week 13 arrives.** A2's ordering guarantee (CH-A1R-06) can't hold on the bus, because the Reaction pass can't post.
6. **`year_quality` is dropped from A1** (C6). Rainfall comes from A4 only. This closes A4's PARK-A4-001 as "not needed". S-2's year-quality selector scales the stub weather input.
7. **`succession_pressure` joins the cell schema, and succession counters come from a data rule table** owned by A3 (C7). Forest, wetland, tundra and rainforest counters are now defined, and `pressure_source` is always set.
8. **A3 runs on A1's bands, synchronously, through `ISuccessionResolver`, and writes A1 only through `ApplyBiomeTransition`** (C9; refined by the A3 spec 2026-10-18). This corrects A3 TechnicalSpec §1's "does not subscribe to SimulationTick". Recovery counters are rules in the same table, and legacy deltas are one-shot.
9. **Hard map edges.** Off-map recolonization becomes edge and refugia lifeboats (C10). The grid is odd-q `HexTopology`, not axial.
10. **Slope, soil, river, corridor and route memory are derived at init** (C11). S-1's schema is unchanged.
11. **The recolonization event is defined** (C13). Herd migration and the dragon kill field wait for Layer 2 and step 9.
12. **S-3 Ecology's guided scenarios** (amended 2026-10-19, product review): the old S-3 Tests 3 and 4 referring to "Barren→Pioneer→Established→Mature→Ancient" stage names become S-3 Ecology's `A3 across biomes` and `Visual tracking` scenarios (see S-3 spec Feature 3 scenario table), which track A3's real transitions (Grassland→Scrub, BurnedAshland→Grassland, etc.) and SEEDING/ESTABLISHING/COMPLETE recovery stages (C8).
13. **Stale references** (C12): the performance target is 16 ms at Large (6,656 cells), not 200×200. Saves are JSON, not blobs. S-2's section references should be §4/§5 (equations), §9 (succession) and §10 (events).
14. **Populations are stored as fractions of `max_density_per_cell`** (C14). S-2's 0–500 sliders are a display scale.
15. **Layer 1 only.** Step 9, Layer 2 and Layer 3 are deferred, with interfaces declared.
16. **A1 owns its tools, and S-2 and S-3 merge into S-3 Ecology** (2026-10-18). Tuning, editing, graphs and the change ledger are part of A1 (A4's pattern). One stage hosts them, with a Controlled mode (pinned weather, for understanding and tuning) and a Living mode (A4 and A3 live, long-run emergence). This changes the GDD's testbed roster.
17. **Natural ignition** (from the A3 spec): seeded lightning fires at the Season band, so fire and BurnedAshland emerge without dragons. This is new GDD content.
18. **`years_low_herbivore_inflow`** (from the A3 spec) stands in for tundra's "migration routes severed".

## Dependencies

**Requires:**
- **S-1 World Generation**, Features 1 (Terrain) and 8 (Biome Classification), for `WorldGenerationComplete` and `GetCurrentWorld()`. Feature 9 (Regions and Sites) is soft: site flags stay false until it exists. A1 Features 1–6 don't need S-1 code.
- GameSpeedController (`SimulationTick`, `TimeBand`, `SeasonOf`). Done.
- `RandomStreamModule` (`A1` stream). Done.
- E1 `SaveLoadModule`. Done.
- `JobSystemService` + `TrySubmit` (dia-needs #12). Done 2026-10-15.
- DiaMaths seeded noise (dia-needs #15). Done.
- Dia CellField renderer (dia-needs #14), for Feature 9's overlays. Spec Approved, not built.
- DiaMessageBus via `MessageBusHostModule`, `VisualDebuggerModule`, DiaAPI `CommandRegistry`, DiaCore (Json, StringCRC, containers, logging).

**Soft dependencies (fallbacks until they exist):**
- **A4.** Without it, `StubWeatherInput` is used and the panel reports the stub as the source.
- **A3.** Without it, triggers are published and counted but no biome changes. A debug `cow.ecology.apply_transition` command can apply one by hand.

**Required by:** A3 (triggers, apply API), A4 (`IBiomeSeasonalBaseline`), S-3 Ecology, and later S-4, V-1, B1, B2, C3 and D4.

## Test Strategy

**Unit (Features 1–6, GoogleTests, hand-built hex grids, `StubWeatherInput`):**
- Data load: valid files, out-of-range values, unknown biome or species references, missing keys → defaults. Fallback roster when the files are absent.
- Initialise: derived slope, river and corridor; seeded soil is deterministic; starting populations follow affinity.
- One test group per equation (TechnicalSpec §4–§5): grass growth, breeding only in season, maturation after N seasons, juvenile 2× predation, Allee below `min_viable`, competition, pack bonus, starvation, functional-group totals.
- Ecology state: a single bad season doesn't drop a tier; down after `state_down_seasons`; up only after the longer streak; one tier per move; the `recovering` flag.
- Succession: counters rise and decay per rule; the trigger fires at 1.0 with the right `to` and `source`; every counter resets after `ApplyBiomeTransition`; legacy deltas apply once and clamp; recovery rules keep progress when their conditions fail, and multiply by the resolver's factor; triggers reach the resolver in cell-index order, and the biome changes before step 12; natural ignition is deterministic and scales with drought and fuel.
- Grid passes: migration conserves population (out = in plus edge loss of 0); pressure fields decay faster than they spread with distance; Ocean and Lake are impassable to land species; coastal feeding is symmetric; lifeboats only seed Collapsed neighbours.
- Events: cluster detection at and below the minimum size.
- Determinism: same inputs give bit-identical fields. `SerialExecutor` vs 1, 3 and 17 ranges, and reversed range order, all give bit-identical fields after 40 years.

**Integration (Features 7–8):**
- The season pipeline runs once per season, after week 13 is accumulated (accumulator = sum of 13 stub weeks).
- No step runs between boundaries. Stub mode steps on `SimulationTick`.
- A4 receives baseline values for all 13 biome ids.
- Save at year 5, continue to 10, load year 5, continue to 10: identical.

**Performance (Feature 4 gate):** one Season and one Year band at Large in Release with the job executor and 15 species, reported against 16 ms, with a per-step profile. Amended 2026-10-19, product review: also report Small and Standard against their scaled budgets (6.7 ms / 10.0 ms).

**Proof (routed to S-3 Ecology):** the GDD's S-2 scenarios (Controlled mode) and S-3's emergence, long-run and A1/A4 response tests (Living mode).

## Open Design Questions

1. **Publishing on the frame after week 13.** The design assumes `EcologyModule` can publish from its own update on the next sim frame after setting `mSeasonPending`. Feature 7 must check this against DiaMessageBus's pass rules. If it can't, the season pipeline runs on the next day's `SimulationTick`, a one-day lag that changes no results.
2. **Starting constants.** The health-index weights and bands, the streak lengths, `coastal_reach` and the aquatic roster are new numbers with no GDD values. S-3 Ecology's Controlled mode tunes them first. If they move a lot, should the GDD lock them, or should they stay as tuning-file values?
3. **Memory at scale.** Dense `[species × cell]` arrays cost 15 species × 2 × 6,656 floats (about 0.8 MB), which is fine. If the roster grows past about 40 species, a sparse per-biome species set may be needed. This is revisited in Feature 4's benchmark.
4. **Same-tick Season+Year ledger loss (resolved 2026-10-19, DiaCellLayers/DiaCellHistory).** When the Season and Year bands run on the same tick (every 364 days), the last-band-only ledger is overwritten by the Year band, so the Season band's deltas are lost. **Resolved:** Recording the ledger's top causes per band as DiaCellHistory events preserves the Season delta information.

## Status

**Status:** `Approved` (2026-10-18). Conflicts C1–C14 resolved in the interview the same day. Amended the same day: change ledger (Feature 3), tools moved into A1 (Feature 9), and S-2/S-3 merged into one Ecology testbed (deviation 16). Amended again on A3's approval (2026-10-18): the `ISuccessionResolver` seam, recovery rules, `RemoveLegacy` removed, natural ignition, and `years_low_herbivore_inflow` (deviations 17–18). No plan yet; create `a1-ecology.plan.md` when implementation starts (Features 1–6 need no S-1, A3 or A4 code).
