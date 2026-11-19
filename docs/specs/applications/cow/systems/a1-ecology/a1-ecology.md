# System Spec: A1 Ecology

**Parent:** @docs/specs/applications/cow/cow.md  
**Testbed:** required  
**Design source:** `Output Docs/GDD/08_SystemsDeepDive/A1_Ecology/` (DesignDocs repo; `A1_EcologySystem.md`, `A1_TechnicalSpec.md`, `A1_TuningParameters.md`, `A1_AuthoringDebug.md`, `A1_EdgeCaseMatrix.md`, `A1_PrototypeTestbed.md`, `A1_ProductionBacklog.md`, all Locked 2026-10-12)  
**Conflicts resolved here:** [a1-gdd-conflicts.md](a1-gdd-conflicts.md) (C1–C14, interviewed 2026-10-18)
**Amended 2026-11-17, A5 Hydrology (aligned to locked GDD 2026-11-17, CHANGE-048; GDD A5 locked under DECISION-129..134, CHANGE-047/048):** water on the ground now belongs to A5 ([a5-hydrology.md](../a5-hydrology/a5-hydrology.md)). Feature 4 loses the water flow pass and the Decade water table, but keeps a thin step 2 that builds `water_gradient` from A5's snapshot. A1 reads cell water, river flow, flooding and ice from `HydrologySnapshot`. The `water_dragon_emission` direct write is dropped. A5 is a soft dependency: without it A1 falls back to A4 rainfall or the pinned climate.
**Amended 2026-11-18, ecology expansion (interviewed and re-approved 2026-11-18):** the food web becomes a **data-driven flow graph** (every pool, producer, consumer, detritus and decomposer is a data node, diets are weighted edges, and the equations no longer name roles). A **nutrient cycle** is added: A1 owns the nutrient pools and moves them downstream using S-1's river network and A5's flow and flood data. **Seasonal presence** is added: seasonal visitors arrive from the map edges and fish runs come up from river mouths, both drawn from off-map reservoirs with year-to-year feedback. Population tokens (Layer 2) move to their own spec, [a1-populationtokens.md](../a1-populationtokens/a1-populationtokens.md). Breadth ideas not taken now are listed under Future Extensions. New sections: Ecology flow graph, Nutrient cycle, Seasonal presence. New Features 10 and 11; Features 1 and 3 rewritten. GDD folded back and A1 re-locked 2026-11-18 (CHANGE-051, DECISION-136..139; one-way predation dropped, EdgeCaseMatrix §13.6 and A1-70 now allow cycles).
**Amended 2026-11-18 (world-sim review, user decisions):** see "World-sim review amendment" below.

**Gameplay Domains:** persistence, debug

## Summary

A1 is CoW's ecology. Every hex cell carries producer biomass, species populations (juvenile and adult), transients, pressure fields and history. Seasonal, yearly, decadal and century rules run over those values, and booms, crashes, migrations, drought, disease and recovery emerge from them; nothing is spawned on a schedule.

A1 is fed by four systems. S-1 provides the world structure, A4 provides weekly rainfall, temperature and snow, A5 Hydrology provides soil moisture, river flow, floods and ice (amended 2026-11-17, A5 Hydrology), and GameSpeedController provides day ticks. A1 tracks the stress that builds toward biome succession and tells A3 when a threshold is crossed. A3 decides the transition and applies it back through A1's API. A1 also implements A4's `IBiomeSeasonalBaseline`.

*Amended 2026-11-18.* What lives in a cell, and who eats whom, is a data-driven **flow graph**: nutrient pools feed producers, producers feed consumers, deaths and waste become detritus, and decomposers return detritus to the pools. Nutrients also move downstream along rivers and settle where floods spread them. The year has a rhythm that comes from data: **seasonal visitors** arrive from the map edges in spring, graze and breed, and leave in autumn, and **fish runs** come up rivers from their mouths, spawn and die, which carries an upstream nutrient pulse and gives river predators a seasonal feast. Both are drawn from off-map reservoirs, so a bad year on the map shrinks the next year's arrivals.

This spec builds **Layer 1** of the GDD's three layers: the analytical cell layer, which always runs. Dragon inputs (tick step 9) are defined as an interface only. Population tokens (Layer 2) are specced separately in [a1-populationtokens.md](../a1-populationtokens/a1-populationtokens.md) (amended 2026-11-18). Local instantiation (Layer 3) is deferred.

**Map sizes** (amended 2026-10-19, product review): Small 66×42 (2,772 cells), Standard 80×52 (4,160 cells), Large 104×64 (6,656 cells, release performance gate for S-1, A4, A1). S-1 now builds and tests all three sizes.

## Goals

1. **Species-driven, data-only roster and food web** (amended 2026-11-18). Species, producers, nutrient pools, detritus, biomes and succession rules load from JSON, and so does who eats whom (the flow graph). Code never names a species, functional group, producer, pool or trophic role (TechnicalSpec §1a, extended). A more complex web later is a data change, not a code change.
2. **Band-gated tick pipeline.** The GDD steps run at the Season, Year, Decade and Century boundaries of GameSpeedController's day ticks, each as a full-grid pass, with no `delta_scale`.
3. **Pure cell logic, testable alone.** The per-cell equations run on one cell with no neighbours, no module and no weather events (GoogleTests, `isolate_cell`). The same code runs grid-wide when fed by A4 (S-3 Ecology, Living mode).
4. **Defined ecology state.** Thriving, Stable, Stressed and Collapsed come from a hysteretic health index (C4).
5. **Succession counters for every A3 path.** A data rule table covers every A3 transition. A1 evaluates it and raises `BiomeSuccessionTriggered` with a `pressure_source` (C7).
6. **Deterministic.** Same world, data, weather and tick count give bit-identical state at any thread count. Randomness comes only from the `A1` stream of `RandomStreamModule`.
7. **Save/load.** Ecology state round-trips through E1, and a reload continues bit-identically.
8. **Observable.** A debug domain with per-cell DiaCellLayers layers, per-cell detail rows, sim-hosted `cow.ecology.*` commands, and profiling, metrics and health.
9. **Performance** (amended 2026-10-19, product review). Season band ≤ 16 ms in Release at Large (6,656 cells) with about 15 species. Small and Standard budgets scaled by cell count: Small (2,772 cells) ≤ 6.7 ms, Standard (4,160 cells) ≤ 10.0 ms. Reported gate. *Amended 2026-11-18:* the gate covers the shipped default graph (about 24 nodes: 4 producers, ~15 consumers, 2 detritus, 1 nutrient pool, plus 2 seasonal species), the nutrient pass and the seasonal-presence pass.
10. **Closed nutrient books** (added 2026-11-18). Nutrient mass changes only through declared sources and sinks (bedrock weathering, ash, run die-off brought in from off-map, river-mouth outflow, leaching). A conservation test checks the books every band.
11. **A year with a rhythm** (added 2026-11-18). Seasonal visitors and fish runs change cell values over the year from data alone, and off-map reservoirs carry the effect of one year into the next.

## Non-Goals

- **Dragon pressure (step 9), hunts, Full Fight and Cultivated Ground.** Interfaces are declared (see Public Interfaces); they are built when S-4 needs them.
- **Population tokens (Layer 2) and local instantiation (Layer 3).** Layer 2 is its own system spec, [a1-populationtokens.md](../a1-populationtokens/a1-populationtokens.md) (amended 2026-11-18), built on top of this one. Layer 3 is deferred. Until Layer 2 exists, S-3 Ecology sees migration through density overlays and flow arrows.
- **The breadth extensions under Future Extensions** (pathogens as nodes, pollinators, plant structure, regional adaptation, invasives). The flow graph is designed so most of them become data, but none of them is built here (added 2026-11-18).
- **An off-map world.** The only off-map state is one reservoir per seasonal species (Seasonal presence). Lifeboats and hard edges (C10) are unchanged (added 2026-11-18).
- **Succession transition rules, legacy modifiers and recovery counters.** These belong to A3. A1 provides the counters and the apply API.
- **Weather.** A4 owns rainfall and temperature variation. A1 does not roll `year_quality` (C6).
- **Water on the ground** (amended 2026-11-17, A5 Hydrology)**.** Soil moisture, ground water, runoff, river flow, lakes, floods and ice belong to A5. A1 no longer moves water or runs a water table (it keeps only a thin step 2 that builds the `water_gradient` pressure field from A5's values, aligned to locked GDD 2026-11-17, CHANGE-048), and no longer writes water directly from dragons (the GDD `water_dragon_emission`). Dragon water effects go through A4's weather influence API (A4 Feature 7); there is no dragon content now.
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
       holds generation (HoldGeneration) in DoStart until subscribed  (added 2026-11-13)
       on WorldGenerationComplete → EcologySimulation::Initialise(world, data)
       on WorldGenerationFailed   → do not initialise; report Unhealthy  (added 2026-11-13)
       registers as World Step stage "ecology"; does NOT kick or schedule (amended 2026-11-13)
       World Step order is ["weather", "hydro", "ecology"]: A1 runs last and reads A5's committed state
       (amended 2026-11-17, A5 Hydrology)
       Capture(key) → latch weather accumulator + due bands from the key
       Run(exec)    → StepBands on the working copy
       Commit()     → swap, then publish CellEcologyChanged / EventThresholdCrossed /
                      BiomeSuccessionTriggered. No history Sample() — HistoryDriverModule
                      samples, signalled by the coordinator after every stage commits
       implements IBiomeSeasonalBaseline (A4 finds it via ModuleRef)
       owns: EcologySimulation, EcologyData, save participant, debug domain
```

### Grid

Dia's `HexTopology` / `HexPathGrid(width, height)`: flat-top, odd-q offset, cell index `y * width + x`, directions `0..5` (NE, N, NW, SW, S, SE), `kNoCell` past hard edges, no wrap (C10). This is the same definition S-1 and A4 use. The GDD's axial equations are applied through `NeighbourAt`, and nothing in A1 assumes axial coordinates or an off-map world.

### Biomes (C1, C2)

> **Superseded 2026-11-18 (user, world-sim review):** the enum is replaced by the S-1 data registry `Global/World/biomes.json` with runtime `BiomeIndex`, roles and tags (@docs/specs/applications/cow/systems/s1-worldgeneration/biome-registry.md). The reasons below (one list, one palette, one name table) still hold, and the list now lives in data. A1's text is aligned in the world-sim amendments (spec 6); until then, read "enum value" as "registry index" and "enum order" as "registry row order".

**One shared 13-value `Biome` enum** (amended 2026-11-13, user decision), declared in `CoW/Shared/WorldGeneration/TerrainEnums.h` and used by S-1, A1, A3, A4 and `species.json`. It is an enum, not a `StringCRC` set: one list means one palette, one name table and no mapping at any system boundary. S-1's enum is widened 10 → 13 by appending A3's outputs — see @docs/specs/applications/cow/systems/s1-worldgeneration/consumer-amendments.md amendment 1, which **gates A1 Feature 2**.

| Group | Enum values | `BiomeId` strings |
|-------|-------------|-------------------|
| Primary (S-1 generates, indices 0–9 with Ocean/Lake) | `GrasslandSteppe`, `TemperateForest`, `WetlandDelta`, `DesertBasin`, `AlpineMountain`, `TundraBoreal`, `Rainforest`, `VolcanicAshland` | `Biome.Grassland`, `Biome.TemperateForest`, `Biome.Wetland`, `Biome.Desert`, `Biome.Alpine`, `Biome.Tundra`, `Biome.Rainforest`, `Biome.Volcanic` |
| Succession outputs (A3 produces, indices 10–12) | `Scrub`, `BurnedAshland`, `DesertifiedPlain` | `Biome.Scrub`, `Biome.BurnedAshland`, `Biome.DesertifiedPlain` |
| Water (S-1 generates) | `Ocean`, `Lake` | `Biome.Ocean`, `Biome.Lake` |

**Note the `BiomeId` strings carry a `Biome.` prefix and are not the enum names** — `species.json` affinity keys and `biomes.json` ids use the string form and resolve through `BiomeFromId` at load. `IsGeneratedBiome(b)` distinguishes the ten classification may place from the three only succession produces; S-1's `IsBiomeValidInZone` and `AreBiomesForbidden` are **classification** predicates and return `false` for the three, so A1 must not use them to validate a live cell.

Lore names (Verdant Lowlands, Stormridge, Ancient Interiors) are regions or depth zones, not biomes. `Volcanic` (primary, eruption ash) and `BurnedAshland` (fire succession) are different ids. A3 uses `BurnedAshland` consistently (amended 2026-10-19, product review). `biomes.json` gives every id a `BiomeProfile`, including its seasonal temperature for `IBiomeSeasonalBaseline` and a seasonal `rainfall_baseline` that S-3's pinned climate uses (the GDD's original per-biome seasonal patterns, TechnicalSpec §1b). An unknown id returns 0.5 and logs once (A4 EC-19).

### State (structure of arrays)

`EcologyFields` holds one `DynamicArrayC<float>` per scalar field, indexed by cell, so passes stream, saves are arrays and the parallel split matches A4's.

- **Abiotic** (amended 2026-11-17, A5 Hydrology: `water` is now fed by A5's seasonal mean `soilMoisture`, and `rainfall_accumulator` is only the fallback input when A5 is absent; A1 no longer simulates ground water): `biome` (the `Biome` enum stored directly as `uint8_t`; the palette-index indirection is dropped — amended 2026-11-13, one shared enum), `elevation`, `slope`, `water`, `rainfall_accumulator`, `temperature`, `snow_covered`, `soil_depth` (`soil_fertility` became the `nutrient.soil` pool, amended 2026-11-18). **New saved fields (aligned to locked GDD 2026-11-17, CHANGE-048, A1 TechnicalSpec 2 and 13.4):** `soil_wetness_accumulator` (sum of weekly A5 soil wetness, reset each Season), `river_flow_ratio_accumulator` (sum of weekly river-flow ratios, river cells only, reset each Season) and `inundated` (bool, copied from A5 each week). All three go in A1's save participant. A saved mid-Season state restores both accumulators.
- **Flow-graph nodes (amended 2026-11-18).** Each node kind gets dense `[node × cell]` arrays, indexed by the compiled node index, not by a named field:
  - **Producers:** one biomass array per producer node. The shipped graph has `producer.grass`, `producer.shrub`, `producer.tree` and `producer.aquatic`, so the old fields exist as data. `seed_bank` stays one per-cell field shared by land producers.
  - **Consumers:** `juvenile` and `adult` per consumer node (species), as fractions of `max_density_per_cell` [0, 1] (C14). A species absent from a cell is 0. Functional-group totals are computed on demand from tags.
  - **Detritus:** one array per detritus node. The shipped graph has `detritus.carrion` and `detritus.waste` (dung and plant litter), which replace the old `carrion` and `waste` transients.
  - **Nutrient pools:** one array per pool node. The shipped graph has one pool, `nutrient.soil`, which replaces the static `soil_fertility` field (see Nutrient cycle).
- **Transients:** `ash`, `disease_load`, `fire_intensity`, `drought_stress`.
- **Seasonal accumulators (added 2026-11-18, saved):** `flood_weeks_accumulator` (weeks this Season with A5 `Flooded` or `Inundated`, reset each Season).
- **Off-map reservoirs (added 2026-11-18, saved, not per cell):** for each seasonal species, a short cohort ring `reservoir[species][cohort]` of off-map adults and maturing young (see Seasonal presence).
- **Pressure fields:** `prey_scent`, `predator_scent`, `dragon_scent`, `carrion_scent`, `water_gradient`.
- **Ecology state:** `ecology_state`, `health`, `state_streak` (signed seasons past a band edge), `recovering` flag.
- **Succession:** `succession_pressure[rule × cell]` (C7, added to the GDD schema).
- **History:** `years_since_fire`, `drought_years`, `last_dragon_visit`, `local_extinction_flags`, `years_low_herbivore_inflow` (consecutive years in which herbivore migration in was below `low_inflow_threshold`; read by A3's tundra rule; A3 amendment 4; saved state field tracked per cell, not read from the ledger — amended 2026-10-19), plus the tended fields (declared, unused until step 9).
- **Static geography (derived at init, never changed by the simulation):** `river_overlay` (the static course; live flow comes from A5), `corridor_node`, `hearth_site`, `nexus_node`, `world_throne_type`, `route_memory` (C11). *Added 2026-11-18:* `downstream` (copied from S-1's `Cell::downstream`, 0..5 on river cells, `kNoCell` elsewhere), `river_mouth` (a river cell whose downstream neighbour is Ocean or Lake, with which one), `upstream_order` (river cells sorted mouth-first for the run and transport passes) and `edge_distance` (hex steps to the nearest map-edge land cell, for visitor arrival and departure).

`year_quality` and the deprecated `rainfall` field are not stored (C6).

### Initial state (C11)

`Initialise(world, data)` derives everything A1 needs from S-1. S-1's schema is unchanged.

| Field | Source |
|-------|--------|
| `slope` | Largest elevation difference to any neighbour, normalised |
| `river_overlay` | `waterSource == River` (the course only; it stays static. The "water floor" a river gives its cells now scales with A5's live `riverFlow` — amended 2026-11-17, A5 Hydrology) |
| `corridor_node` | `terrainFlags & Pass` |
| `hearth_site`, `nexus_node`, `world_throne_type` | S-1's `SiteRegistry` / `SiteSet`. **Populated from the start** — amended 2026-11-13: S-1 Feature 9 (Regions and Sites) is built, so the old "all false until Feature 9" caveat is void |
| `nutrient.<pool>` (was `soil_fertility`), `soil_depth` | `BiomeProfile` defaults (one starting value per pool), varied ± by seeded noise (DiaMaths, `A1` seed) and reduced by slope |
| `downstream`, `river_mouth`, `upstream_order`, `edge_distance` (added 2026-11-18) | S-1 `Cell::downstream` and biome; `edge_distance` by breadth-first search from map-edge land cells |
| Off-map reservoirs (added 2026-11-18) | Each seasonal species' `reservoir.initial` from `species.json` |
| `route_memory` | Base value, raised on river and corridor cells (EcologySystem §5.1, §5.3) |
| Producers, populations | `BiomeProfile` starting values. Species are seeded where biome affinity is above zero, at affinity × the biome's starting density |
| Ecology state | `Stable`, streak 0 |

### Tick pipeline (C5)

GameSpeedController publishes one `SimulationTick{day, band}` per day and never skips days, so every boundary is visited exactly once at any speed. A1 runs each step **once per boundary at that band's rate**. The GDD's `delta_scale` ×4/×40/×400 is dropped. The schedule follows EcologySystem §7 (TechnicalSpec §11's pseudo-code disagrees and is superseded):

| Band | Steps |
|------|-------|
| Season | 1 weather (consume accumulator), natural ignition and fire spread, 2 thin `water_gradient` build from A5's snapshot (soil moisture, river flow, inundation); A1 no longer moves water (amended 2026-11-17, aligned to locked GDD 2026-11-17, CHANGE-048), 3 grass and aquatic, 4 foraging, 8 migration intent, 9 scent (no-op until S-4), ecology state, resolver `OnSeason`, 12 evidence |
| Season, added 2026-11-18 | **S1 seasonal presence** after step 4 (visitor arrival, departure and stragglers; run advance, spawning and die-off; see Seasonal presence), **N1 nutrients** after step 4 and S1 (decomposition, uptake, leaching, river transport, flood deposition; see Nutrient cycle) |
| Year | 5 predation and carrion, 6 starvation and disease, 7 reproduction, 8 full move, 9 full (no-op), 11 guardrails, succession rules with `band: Year` + resolver `Resolve` (step 10), resolver `OnYear`; *added 2026-11-18:* **S2 reservoir year** (off-map survival, breeding and cohort ageing for each seasonal species) |
| Decade | ~~2 water table~~ (deleted, A5 owns ground water; amended 2026-11-17), 3 shrubs and trees, 10 succession rules with `band: Decade`, seed drift |
| Century | 10 major succession, 11 legacy recolonization |

The Year, Decade and Century boundaries are also Season boundaries (364 = 4 × 91). Bands run smallest first on the same tick. **Each step is a full-grid pass, step-outer** (every cell finishes step N before any cell starts N+1, EcologySystem §6). Steps that read neighbours read the stable copy taken at the start of the pass and **gather** inflow in direction order 0..5, as A4 does, so the result does not depend on iteration order. Every modified field is clamped at the end of each step (TechnicalSpec §3).

*Amended 2026-11-18.* Steps 3–7 no longer have hard-coded roles. "Grass and aquatic" (step 3) means *every producer node whose `band` is Season*; "shrubs and trees" (Decade) means *every producer node whose `band` is Decade*. Steps 4–7 run over consumer nodes and their diet edges in the compiled evaluation order. Detritus decay (carrion rotting and waste breaking down) moves into N1, so carrion is still made in step 5 and eaten by scavengers through ordinary diet edges.

### Ecology flow graph (added 2026-11-18)

The flow graph is the food web as data. It is loaded from `flow_graph.json` (envelope `cow.flow_graph`) next to `species.json`, which still holds the per-species parameters. Node ids are `StringCRC`.

**Node kinds**

| Kind | What it is | Per-cell state | Shipped nodes |
|---|---|---|---|
| `pool` | An abiotic nutrient store | amount | `nutrient.soil` |
| `producer` | Plant biomass that grows from water, temperature, light and a pool | biomass | `producer.grass`, `producer.shrub`, `producer.tree`, `producer.aquatic` |
| `consumer` | A species (from `species.json`, joined by id) | juvenile, adult | the roster |
| `detritus` | Dead matter waiting to break down | amount | `detritus.carrion`, `detritus.waste` |

Decomposers are not a kind of their own. A decomposer is a consumer whose diet edges point at detritus nodes, or, in the simple case, a detritus node's own `decay` edge to a pool (the shipped graph uses the decay edge; a data author can add worm or fungus consumers later).

**Edge kinds**

| Edge | From → to | Parameters |
|---|---|---|
| `eats` | consumer → producer, consumer or detritus | `preference` (relative weight in the diet), `efficiency` (fraction turned into the eater's growth), `life_stage` (`adult`, `juvenile` or both), `reach` (`cell` or `coastal`, which replaces the old `coastal_reach` diet rule) |
| `uptake` | producer → pool | `rate`, `half_saturation` (Michaelis-Menten) |
| `returns` | producer or consumer → detritus | which detritus a death or waste goes to, `fraction` |
| `decay` | detritus → pool | `rate` per Season, scaled by water and temperature fit |

Functional-group tags stay on consumers, and the debug layers, A3 conditions and DiaCellHistory groups keep using them.

**Compile at load.** `CompileFlowGraph` turns the JSON into flat, index-based tables: node arrays by kind, edges in compressed-sparse-row form (per eater, its food list; per food, its eater list) and an evaluation order by derived trophic level. The per-cell equations read only these tables, so the generic graph costs no lookups in the hot loop. Compilation is deterministic: the same file gives the same indices and order.

**Validation** (load fails, falls back to the shipped graph and reports Degraded):

- every edge names an existing node of an allowed kind
- every consumer has at least one `eats` edge, and every producer has an `uptake` edge
- every node that can die has a `returns` edge, and every detritus has a `decay` edge (so the nutrient books close)
- `eats` cycles between consumers are allowed (omnivory, cannibalism); trophic level is computed with cycles broken at the lowest-preference edge, and the order is reported in the panel
- warnings (not failures): a node nothing eats, a pool nothing takes up, a consumer whose only food never shares a biome with it

**Equations stay the GDD's.** Producer growth, foraging, predation (age-weighted), starvation, carrion and scavenging are the GDD's TechnicalSpec §4–§5 equations, rewritten over edges: "herbivores graze grass" becomes "for each `eats` edge from a consumer to a producer". The shipped `flow_graph.json` encodes today's roles exactly, and a **role-equivalence golden** proves the generic code with the shipped graph gives the same fields as the GDD's role-based equations on the reference grid (Feature 3).

**What other systems see.** A3's rule conditions name fields by id, and producer, detritus and pool fields resolve by node id (`producer.grass`, `nutrient.soil`). `GetView()` exposes values by node index plus an id lookup. The A3 rule file's `grass` or `soil_fertility` references are rewritten to node ids (A3 amendment owed, deviation 20).

### Weather input (C5, C6)

A1 reads weather through `IEcologyWeatherInput` (per-cell `Rainfall`, `Temperature`, `SnowCover`), not through A4 directly.

- **Living mode and the game:** `WeatherSnapshotInput` adapts A4's `WeatherSnapshot`. On each `WeatherUpdated{week}`, A1 adds `Rainfall` into `rainfall_accumulator` and takes the latest `Temperature` and `SnowCover`.
- **Timing (amended 2026-11-13, World Step Approved):** the `mSeasonPending` next-frame deferral is **deleted**, along with deviation 5 and Open Design Question 1. It existed because the 13th week of a season arrives in the bus's Reaction pass, which cannot post. Under World Step, `Capture` and `Commit` are explicit SimPU points outside the bus passes: A1 accumulates on `WeatherUpdated`, and the coordinator's `Capture(key)` tells it which bands are due from the game day. Week 13 counts toward its own season by construction.
- **Controlled mode and tests:** `StubWeatherInput` (Testing/) supplies slider or scripted values. Its GOOD/AVERAGE/BAD selector scales the stub's rainfall; this is not an A1 field. With the stub, the module steps on `SimulationTick` with `band >= Season`.

### Hydrology input (amended 2026-11-17, A5 Hydrology)

A1 reads water through `IEcologyHydrologyInput`, a seam like `IEcologyWeatherInput`, so A1 still runs without A5.

| Method | Meaning |
|---|---|
| `SoilMoisture(cell)` | Seasonal mean of A5's `soilMoisture`. **This is A1's cell `water`**, replacing the A4 `rainfall` accumulator as the water input |
| `RiverFlow(cell)` | Live flow. Scales the river-overlay water floor on `river_overlay` cells |
| `Flooded(cell)` | Floodplain flood this week |
| `Ice(cell)` | Frozen surface water |
| `Inundated(cell)` | Cell is under basin water this week. A1 treats an inundated cell as **aquatic habitat** (aquatic species can live there before A3 turns it into Lake). It also suppresses grass growth and caps land-species capacity at the GDD's `INUNDATED_LAND_CAPACITY` |
| `InundatedWeeks`, `ExposedWeeks`, `LowFlowWeeks`, `GlacierWeeks` (cell) | A5's consecutive-week counters (`inundatedWeeks`, `exposedWeeks`, `lowFlowWeeks`, `glacierWeeks`). A1 exposes them to A3's rule conditions as `inundated_weeks`, `exposed_weeks`, `low_flow_weeks`, `glacier_weeks`, and the new `glacier_free_weeks` (A5 `glacierFreeWeeks`, counts while glacier mass is below `GLACIER_BIOME_MIN`; used by glacial retreat). Also `is_protected` for the inundation rule A1 copies, never writes them |
| `PassabilityAt(cell)` | `Dry/Wet/Fordable/Deep/Frozen`. Migration treats `Deep` like impassable water for land species and `Frozen` as land; `Wet` is slower, never blocking |

- **With A5 (`HydrologySnapshotInput`):** on `HydrologyUpdated{week}` A1 adds `SoilMoisture` into a seasonal mean. A4's `rainfall` no longer includes ground water, so there is no double count.
- **Without A5 (S-3 Controlled mode, tests):** the fallback is today's behaviour: the old A4 `rainfall_accumulator` (or the pinned climate's rainfall) feeds `water`, with no inundation. The `river_overlay` floor stays at its static value. `StubHydrologyInput` (Testing/) supplies scripted values.
- A5 runs before A1 in the World Step order, so A1's `Capture` latches A5's committed state of the same step.
- **Mapping (aligned to locked GDD 2026-11-17, CHANGE-048):** `water` is the mean of the 13 weekly soil wetness values per Season. The river-overlay floor is `RIVER_FLOOR_MIN + (RIVER_FLOOR_MAX - RIVER_FLOOR_MIN) x ratio`, where `ratio` is the **Season mean of the weekly `min(1, river_flow / max(mean_flow, MIN_MEAN_FLOW))`** (`river_flow_ratio_accumulator / 13`). `MIN_MEAN_FLOW` is read from A5. `cell.water = max(cell.water, floor)` on river cells, and 1.0 on inundated cells. The floor never reaches 0 (DECISION-065). `RIVER_FLOOR_MIN/MAX` are A1 tuning values, still TBD.
- **Step 2 (thin):** at the Season band `water_gradient` sources are set from A5's soil wetness, the river floor and inundation. Wetland edges follow inundation. Spread and decay of the field are unchanged. Without A5 the sources fall back to the static `water_source` flags (water cells and the river overlay), as today.
- **Step 2 source formula (Cluiche-specified; the GDD gives this in words only, A1 TechnicalSpec 13.4).** For each cell, after step 1 has set `water`:
  ```
  source(cell) = 1.0                                  if cell.inundated or the cell is an Ocean/Lake (water) cell
               = clamp(cell.water, 0, 1)              otherwise
  water_gradient(cell) = max(water_gradient_after_spread_and_decay(cell), source(cell))
  ```
  `cell.water` already holds the Season mean of soil wetness, raised to the river floor on river cells (`max(mean wetness, floor)`), so one clamp covers "soil wetness and the river floor". `max` means a source never lowers a value the existing spread already gave, and the field's spread and decay code is untouched. If the first playtests show dry cells with high wetness swamping the gradient, a source scale is the first tuning knob (TBD).
- **Frozen rivers** read as `Frozen` (A5 `river_state`), not dry. A1 never turns a frozen river into a low-water cell.

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

- `source` is one of `DROUGHT`, `FIRE`, `OVERGRAZE`, `DISEASE`, `DRYING`, `COLD` or `INUNDATION` (new, aligned to locked GDD 2026-11-17, CHANGE-048).
- `to` may be `none` for a **marker-only rule** (the `DRY_RIVERBED` marker). A1 still raises the trigger when the counter reaches 1.0. A3 sets the marker and does not call `ApplyBiomeTransition`, so A1's other counters on the cell are not reset (GDD A1 TechnicalSpec 15.3).
- **Rule count is now 17** (12 stress and recovery rules plus five hydrology rules). The five hydrology rules are in A3's table and are evaluated at the Decade band; A1 evaluates them like any rule. Sources: inundation uses `INUNDATION`; exposed lakebed and dry riverbed use `DRYING`; glaciation and glacial retreat use `COLD`. They read A5 counters directly with `increment: 1.0`, `decay: 1.0`, so they fire at the first Decade check where the condition holds.
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

Ocean and Lake cells run an **aquatic-only** ecology: `aquatic` biomass plus species with `"habitat": "aquatic"` (same `species.json` schema). Aquatic species also live in river and wetland land cells. **Inundated land cells (A5) count as aquatic habitat too** (aligned to locked GDD 2026-11-17, CHANGE-048), so a flooded valley gets fish before A3 turns it into Lake. When A3 does flip it to Lake, land populations leave through the migration solver and any that can't move are lost. Land species treat Ocean and Lake as impassable, and aquatic species can't move onto dry land cells.

**Coastal cross-feeding:** a land predator or scavenger whose diet lists an aquatic group can hunt aquatic populations in **adjacent** water cells, scaled by `coastal_reach`. Each side gathers from the stable copy: the water cell computes its losses from neighbouring land predators, and the land cell computes its food gain from neighbouring aquatic prey. This keeps the result deterministic. Water cells compute `H` from aquatic biomass and aquatic consumers only.

Water cells are permanent `water_gradient` sources and answer `IBiomeSeasonalBaseline` like every other biome.

*Amended 2026-11-18:* "whose diet lists an aquatic group" is now an `eats` edge with `reach: coastal`. The `coastal_reach` parameter scales those edges. In river and wetland land cells a land predator eats aquatic species through ordinary `reach: cell` edges, which is how bears eat a fish run.

### Nutrient cycle (added 2026-11-18)

A1 owns the nutrients. A5 stays water-only and is read, never extended. Nutrient pools are flow-graph nodes, so a later graph can add more pools (for example a separate nitrogen and phosphorus) without code changes. The shipped graph has one pool, `nutrient.soil`.

**N1, every Season, in this order (step-outer, gather from the stable copy):**

| Sub-step | Effect | Source of rates |
|---|---|---|
| Decay | each detritus node moves `decay.rate × water_fit × temperature_fit` of itself into its pool | `decay` edges |
| Ash | ash left by fire releases `ash_nutrient_fraction` into `nutrient.soil`, then clears | `ecology_parameters.json` |
| Weathering | each land cell gains `BiomeProfile.weathering` (bedrock input, small) | `biomes.json` |
| Uptake | producers took their uptake during step 3 (Michaelis-Menten on the pool); N1 books it | `uptake` edges |
| Leaching | a fraction of each pool, `leach_rate × water`, is lost from land cells; wetter soil leaches more | `ecology_parameters.json` |
| River transport | on river cells, a fraction `transport_rate × flow_ratio` of the pool moves to the `downstream` cell. Cells are processed in reverse `upstream_order` with gather, so the result is order-independent. At a `river_mouth` the moved amount leaves the map (a sink to Ocean) or enters the Lake cell | `ecology_parameters.json`; `flow_ratio` is the Season mean already in `river_flow_ratio_accumulator` |
| Flood deposition | cells with `flood_weeks_accumulator > 0` receive silt: `flood_silt_rate × flood_weeks / 13` of the upstream river cell's pool, taken from that river cell | `ecology_parameters.json` |

**Without A5:** `flow_ratio` is 1.0 on the static river course and there is no flooding, so transport runs at its base rate and nothing is deposited.

**Books.** Every sub-step writes its amount to the ledger under its cause (`nutrient.decay`, `nutrient.uptake`, `nutrient.leach`, `nutrient.transport_in/out`, `nutrient.flood_silt`, `nutrient.weathering`, `nutrient.run_import`). The conservation test checks, for every band, `Δ total = weathering + ash + run import − leaching − mouth outflow` to float tolerance.

**What it changes.** The health index and the growth equations read `nutrient.soil` where they read `soil_fertility` before, so overgrazed or burned land changes fertility over time instead of keeping a fixed value. Floodplains and deltas get richer, and steep wet slopes get poorer. A3 rules can read `nutrient.soil` in conditions (desertification, for example).

### Seasonal presence (added 2026-11-18)

Any species in `species.json` may carry a `seasonal` block. Seasonal species are ordinary consumer nodes with ordinary diet edges. What's special is where they come from and when they leave.

```json
"seasonal": {
  "kind": "visitor",                         // "visitor" (map edge) or "run" (river mouth)
  "arrive": { "season": "Spring" },          // or { "temperature_above": 8.0 }, checked each Season
  "depart": { "season": "Autumn" },          // or { "temperature_below": 4.0 }
  "entry":  { "edges": ["N", "S"] },         // visitor: which map edges; run: { "water": "Ocean" | "Lake" | "any" }
  "reservoir": { "initial": 0.6, "cap": 1.0, "offmap_survival": 0.85, "offmap_growth": 1.1, "cohort_years": 2 },
  "stragglers": "die",                       // "die" or "stay" (visitors only)
  "spawn": { "conditions": [ { "field": "elevation", "op": ">", "value": 0.3 } ], "die_after": true }  // runs only
}
```

**Off-map reservoirs.** Each seasonal species has one reservoir, a cohort ring of `cohort_years + 1` scalars (in units of map-wide population mass): adults ready to come, plus young maturing off-map. It is the only off-map state in A1 (an extension to C10, deviation 22).

- **S2, every Year:** the ready cohort is multiplied by `offmap_survival`, young move up one cohort, and young that reach the front join the ready cohort. `offmap_growth` (default 1.0) is for species that also breed somewhere off the map; a species that breeds only on the map keeps 1.0. The ready cohort is clamped to `cap`.
- **Feedback:** what returns to the reservoir is what *left the map healthy*: visitors that reached an edge before departure, and run young that drifted out of a river mouth. A drought year, a dragon-hunted valley or a dammed river shows up as smaller arrivals one or more years later.

**Visitors (S1).**

- **Arrival:** on the arrival Season, the ready cohort enters map-edge land cells on the listed edges. It is split across those cells by the species' biome affinity × producer food available, with a seeded jitter from the `A1` stream in cell-index order. The cohort empties.
- **While present:** visitors are normal consumers. They forage, breed (young are born on the map), are hunted and die like residents, and they make detritus and waste that feed the nutrient cycle.
- **Migration bias:** during the arrival Season the migration solver adds an inland pull (`−∇edge_distance`); from one Season before departure it adds a homeward push (`+∇edge_distance` toward the listed edges). These are extra terms in step 8's softmax, scaled by `seasonal_bias` (a parameter).
- **Departure:** on the departure Season, adults and juveniles in map-edge cells on the listed edges leave into the reservoir (young into the youngest cohort). What is still inland is handled by `stragglers`: `die` turns it into carrion (a late autumn feast for scavengers), and `stay` keeps it as a resident population for the year.

**Fish runs (S1).**

- **Arrival:** the ready cohort enters at `river_mouth` cells matching `entry.water`. It is split across mouths by the Season mean flow ratio of the mouth cell, then moves up the river tree in `upstream_order`. At each fork the run splits by the branches' flow ratios. A run moves up to `run_reach` cells per Season (a species parameter).
- **Blocked:** a cell whose A5 passability is `Frozen` or whose `low_flow_weeks` passes `run_low_flow_block` stops the run there. Without A5, nothing blocks.
- **Spawning:** run mass that reaches cells meeting `spawn.conditions` spawns there. Young are born as the species' juveniles in that cell and drift downstream at `run_reach` cells per Season. Young reaching a mouth join the reservoir's youngest cohort.
- **Die-off:** with `die_after: true`, spawned adults die in the spawning cells and become `detritus.carrion`. That is the nutrient import from off-map (`nutrient.run_import` in the books): mass from the ocean is left upstream. Adults stopped short (blocked or out of reach) die where they stop.
- **Feast:** land predators and scavengers with `eats` edges to the run species feed in the river cells it passes through, so bears and later dragons get a seasonal feast along the rivers.

**Granularity.** A1 steps once per Season, so presence changes four times a year: arrival, a full Season of presence, departure. Runs move up to `run_reach` cells per Season pass. See Open Design Question 5.

**Events.** `SeasonalArrival{species, mass, entry_cells}`, `SeasonalDeparture{species, mass_out, stragglers}`, `RunSpawned{species, cells, mass}` and `RunBlocked{species, cell, mass}`, published at the end of the band and emitted to DiaCellHistory (`kind: event` schema entries).

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
- **Seasonal presence events** (added 2026-11-18): `SeasonalArrival`, `SeasonalDeparture`, `RunSpawned`, `RunBlocked` (see Seasonal presence).

The herd-migration token event waits for Layer 2 ([a1-populationtokens.md](../a1-populationtokens/a1-populationtokens.md)) and the dragon kill field for step 9. Payloads are small POD. The cluster cell lists are read through `GetLastEvents()`.

### Parallel execution and performance

Passes are split into cell ranges through **`Dia::DeferredStep::IParallelExecutor`** (amended 2026-11-13: the interface, `SerialExecutor` and the JobSystem-backed capped executor all moved into Dia with the AD-001 sign-off of 2026-11-12; A1 and A4 no longer define or implement them). The signature is `ParallelFor(count, minRange, fn)`, and calling it several times in one `Run` — once per pass — is the expected consumer shape. Each range writes only its own cells, and reductions are serial in range order, so results are bit-identical at any range count. The worker cap is strict and includes the coordinator, which runs ranges itself rather than idling.

The season band is gated at ≤ 16 ms in Release at Large (6,656 cells) with about 15 species. Missing it is reported but doesn't fail the build. This replaces S-3's stale 200×200 target.

### Save/load (E1)

`EcologySaveParticipant` writes every non-static `EcologyFields` array as a JSON array, plus the **species** palette (a CRC list). The **biome palette is gone** (amended 2026-11-13): `biome` is the shared enum stored as a byte, so it serializes as a plain `uint8_t` array. Floats are stored as doubles (`%.17g`) so they round-trip bit-exactly. Static geography is re-derived from the world on load. *Amended 2026-11-18:* node arrays are saved with a **node palette** (CRC list in compiled order), so a reload under an edited graph maps by id and fails validation on a missing node; the seasonal reservoirs and `flood_weeks_accumulator` are saved. The estimated size is about 40 fields × 6,656 cells, inside the 100 MB buffer (`SaveContext::kBufferSize`, verified 2026-11-13).

The real `ISaveable` interface (verified 2026-11-13) is `Serialize(SaveContext&) const`, `Deserialize(LoadContext&)`, **`GetVersion()`** (not `version`), and an optional `Validate(LoadContext&) const` that defaults to `true` and runs on *every* participant before *any* `Deserialize`, so a load is all-or-nothing. The participant **id is a registration argument**, not a member: `SaveLoadModule::RegisterParticipant(StringCRC("ecology"), &participant)`, and registration order is serialization order.

**Pending deferred step.** A1's step may be complete but unapplied when a save is taken, so it implements `SerializePending` / `DeserializePending` on its World Step stage (DFS-005). A1 writes no other save code for this: DiaDeferredStep drains automatically on `SaveManager::OnSaveStarted` and owns the single `ISaveable` holding every channel's pending result, so **E1 needs no change**.

### Change ledger (why values changed)

*Amended 2026-10-18.* A1 is meant to be understood, not just watched, so every step records **why** values changed, not only the results. Each band, `EcologyLedger` holds per-cell, per-species deltas by cause:

- **Populations:** births, maturation, deaths by starvation, predation (by predator species), disease, Allee and base mortality, migration in and out (by direction), lifeboat founding.
- **Producers:** growth, grazing (by species), wilt, fire.
- **Nutrients and seasonal presence (added 2026-11-18):** the N1 causes (Nutrient cycle) and arrivals, departures, stragglers, run spawning and die-off by species.
- **State:** the health index terms, and which succession rule conditions passed or failed.

The ledger covers the last band only, is overwritten each band, is never saved, and doesn't feed back into the simulation. The equations write their terms to it as they compute them, so it can't drift from what actually ran. A build flag can turn it off if the 16 ms gate needs the time. Aggregate calculations (e.g., A3's yearly inflow checks) use saved state fields, not the ledger (amended 2026-10-19, product review).

### Debug, tuning and editing tools

*Amended 2026-10-18, 2026-10-19, 2026-11-13.* The tools for understanding and tuning ecology belong to A1, as A4's do, so every stage that hosts A1 gets them: the Ecology testbed, and later the dragon testbeds. Every action is a `cow.ecology.*` command and the panel calls the same commands; the commands and their execution model come first, and the panel depends only on them.

**Commands are sim-hosted, not DiaAPI** (amended 2026-11-13). `QueryRegistry` has an owner-thread veto, so a SimPU module cannot register there at all, and a DiaAPI handler runs on the caller's thread, which would race Sim-owned ecology state. A1 registers through `SimQueryHostModule::RegisterCommand(name, SimHelpInfo, handler)`:

- **A description is mandatory** (non-empty, ≤ 256 chars) or registration fails with an Error. `category` is `"ecology"`, `owner` is `"EcologyModule"`, arguments are declared as `Dia::API::ValueParam`s (≤ 16).
- **Arguments are JSON objects.** No positional forms.
- **A partial registration must roll back** every name registered in that call — the failure mode that permanently leaks names in `WorldGenerationModule` today.
- **Name budget.** A1 registers **16** names (14, plus `flow_graph` and `seasonal`, amended 2026-11-18; the cap is now 192, so the count below is history). The cap is 64 per SimPU, enforced per-stage in practice, and S-3.5's *built* components already occupy 58/64 — so S-3.5 needs the cap raised to 192 before it is wired (tracked in BACKLOG; owned by S-3.5, not A1). A1's own stage, S-3, is well under.

`EcologyDebugDomain` (SimPU) provides:

- **Panel:** season, year, cell count, state distribution, functional-group totals, last band profile, data source (file or fallback roster), weather input (A4 or stub), A3 present or not. *Added 2026-11-18:* flow-graph summary (node counts by kind, trophic order, validation warnings, shipped or file graph), map-wide nutrient total with this band's books (sources, sinks, residual), and per seasonal species its reservoir cohorts, present/away state and next arrival estimate.
- **Layers and overlays (amended 2026-10-19; corrected 2026-11-13).** Ecology fields become registered DiaCellLayers layers (ecology state, health, biome, each producer, species or functional-group density, each pressure field, succession progress) — see the layer table below. Registration is `CellLayerRegistry::Register(CellLayerDesc)` with a **pull** `read(float*, uint32_t)` plus a `version()` change counter, reached through a `ServiceStreamReader<CellLayerRegistry>` on `"CellLayerRegistryService"` and resolved in `DoUpdate` or later. **Registration must happen on the owning PU (SimPU)** or it is rejected with `WrongProcessingUnit` and flips health to Degraded. A1 does **not** implement `IViewValueSource` — that is DiaCellHistory's replay seam.
  - **Migration arrows are a draw-callback overlay, not a layer.** `CellValueKind` is `{Float, Category}` only — there is no vector kind — and `IDebugDraw` has **no 2D arrow primitive** (`RequestDrawArrow3D` is 3D-only). Each arrow is composed from a line or ray plus an explicit head, so the top-3 overlay **decimates by default** (every Nth cell, a panel control) and respects `DroppedCount()`. At Large, three arrows per cell is ~60k primitives, far past the 8,192 geometry budget.
  - Paint applies to the current selection, which comes from DiaCellLayers' `cells.select.*`; A1 reads the selection, it does not implement one. Event clusters are a second draw-callback overlay.
- **Per-cell detail rows (amended 2026-11-13; A1 does not build a cell inspector).** DiaCellLayers owns the inspector (`InspectorState` / `InspectorModel`, `cells.inspect`, stats, histogram, pin-delta). A1 registers a **detail provider** via `CellLayerRegistry::Details().Register(grid, name, builder)` — a `CellDetailBuilder` emitting sections and rows, 64 rows maximum, invoked only when exactly one cell is selected — plus a hover-extra fn for tooltip lines. The rows show every field of the selected cell, including per-species juvenile/adult counts and maturation, plus the **change ledger** for that cell: each value's change last band, broken down by cause. The 64-row cap matters for a large roster: with 15 species the per-species rows alone approach it, so the provider groups species and expands the selected group.
- **Graphs (amended 2026-10-19; corrected 2026-11-13):** map-wide functional-group totals and the state distribution read DiaCellHistory aggregates, so they cover the whole run and are recorded whether or not any panel is open; per-cell series need that layer tracked (`cells.history.track`). **But DiaCellHistory's in-game panel was deliberately not built** (Feature 3 task 3.10b and Feature 4 task 4.10 were dropped at its close on 2026-11-04 and re-homed to the CluicheEditor Inspector as the proposed T-1 Feature 7). So A1 has **no in-game host for a rolling time-series panel**: the live card shows current-band values, and time series are reached through `cells.history.*` or T-1. Long-run analysis lives in the World Sim Inspector by design, not as a fallback.
- **Tuning:** live sliders over `ecology_parameters.json` and the selected species' and biome's entries, applied at the next band. `cow.ecology.tuning.save` writes changes to the defaults files themselves via the `source` alias (dia-needs #16, F-3 World Assets; amended 2026-10-19). `cow.world.pack.save_as ecology <id>` is the normal tuning path, writing overrides-only ecology packs to `Assets/CoW/Global/Ecology/Packs/<id>.ecology.json`. Hot reload picks up hand edits.
- **Editing:** set a cell's fields or a species' population, and paint species, producers, water or biome over a brush of cells. Painting a biome reinitialises the painted cells from the biome's starting values. Painted/edited cells can be saved as an ecology start state via `cow.world.startstate.save_as` (baked terrain only; F-3 World Assets, amended 2026-10-19), written to `Assets/CoW/Global/Ecology/StartStates/<id>.ecostate.json`. `isolate_cell` turns off migration and propagation into and out of the selected cell, which gives the old single-cell S-2 behaviour.
- **Commands (16):** `ledger`, `set_field`, `set_species`, `paint`, `isolate_cell`, `set_ecology_state`, `step_band`, `reload_data` (full name `cow.ecology.reload_data`: re-reads species, biomes, ecology parameters and succession rules from disk; used by T-2 "Reload in game"), `tuning.set`, `tuning.save`, `find_events`, `ignite`, `apply_transition` (until A3), `cause` (the T-1 feed below). *Added 2026-11-18:* `flow_graph` (read-only: the compiled nodes, edges and trophic order as JSON; T-2 and T-1 can draw it) and `seasonal` (read the reservoirs, or set a species' ready cohort or force an arrival or departure this band, for testing feedback). **Dropped 2026-11-13:** `inspect` — DiaCellLayers owns `cells.inspect` and the whole `cells.select.*` / `cells.query.*` surface; A1 must not re-spec selection or inspection.
- **Instrumentation (amended 2026-10-19; extended 2026-11-13):** `DIA_PROFILE_SCOPE` per step and `DIA_TRACE_ZONE` per band; `MetricRegistry` counters (cells per state, events per band) plus **`cow.ecology.band_ms` registered with `MetricInfo`** — required by S-3.5 R5 and not optional; an `IHealthReporter` (Degraded on the fallback roster, a failed data validation, or rejected layer registrations). Ecology state changes and crashes are emitted as DiaCellHistory events — event types need `kind: "event"` DiaValueSchema entries with their payload field lists, or the recorder warns per type. **A1 does not call `Sample()`** (see the module diagram): CoW-Shared `HistoryDriverModule` owns sampling and is signalled by `WorldStepModule` once per applied step, and A1's emitted events are committed by that sample.
- **Cause events for T-1 (amended 2026-10-19, T-1 World Sim Inspector):** each band, for each region and each layer tracked in DiaCellHistory, A1 emits its top 3 ledger causes as `cow.ecology.cause {group, layer, cause, delta, cells}` events. **Amended 2026-10-20 (T-1 Why Panel):** `cells` is the top 8 contributing cells by absolute delta, `[{q, r, delta}]`, taken from the ledger terms for that cause (their deltas never exceed the event's `delta`); it lets T-1's "Show cells" work for past bands and from files, since the ledger itself keeps only the last band. The event type is described in DiaValueSchema (`kind: event`). The counts are A1 parameters: `cause_events_per_region` (default 3) and `cause_cells_per_event` (default 8), so they can change as data without a spec amendment. This feeds T-1's why panel.

- **Layer ids** (amended 2026-10-19, consistency pass): A1 registers these DiaCellLayers layers. Schema keys are `cow.ecology.cell/<field>`.

  | Layer id | Kind | Source | Notes |
  |---|---|---|---|
  | `ecology.state` | Category | `ecology_state` | Palette from the state enum |
  | `ecology.health` | Float | `health` | |
  | `ecology.producers_total` | Float | `grass + shrub + tree + aquatic` (derived) | Default tracked layer |
  | `ecology.producer.<id>` | Float | one per producer node (amended 2026-11-18; the shipped graph gives `grass/shrub/tree/aquatic`) | |
  | `ecology.seed_bank`, `ecology.water` | Float | fields of the same name | |
  | `ecology.soil_fertility` | Float | alias of the first pool, `nutrient.soil` (kept 2026-11-18 so S-3 and T-1 references hold) | |
  | `ecology.nutrient.<pool>` | Float | one per pool node (added 2026-11-18) | |
  | `ecology.detritus.<id>` | Float | one per detritus node (added 2026-11-18; replaces `ecology.transient.carrion` and `waste`) | |
  | Overlay `ecology.runs` | — | run fronts and spawning cells along rivers, draw callback (added 2026-11-18) | |
  | `ecology.group.<tag>` | Float | functional-group density (sum of tagged species, adult + juvenile) | e.g. `ecology.group.herbivore` (default tracked) |
  | `ecology.species.<id>` | Float | one species' density | Registered per species in the roster |
  | `ecology.pressure.<field>` | Float | pressure fields (`prey_scent` …) | |
  | `ecology.transient.<field>` | Float | `ash`, `fire_intensity`, `drought_stress` … | |
  | `ecology.succession_progress` | Float | highest forward `succession_pressure` | Shared with A3's `GetSuccessionProgress` |
  | Overlay `ecology.migration` | — | top-3 migration arrows | |

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
        void StepBands(TimeBand band, Dia::DeferredStep::IParallelExecutor& executor);   // every band <= band, smallest first
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

    struct EcologyData { /* species roster, biome profiles, succession rules, parameters, compiled flow graph */ };
    bool LoadEcologyData(const Json::Value& species, const Json::Value& biomes, const Json::Value& flowGraph,
                         const Json::Value& rules, const Json::Value& params, EcologyData& out);
    EcologyData FallbackEcologyData();      // 3 land species (deer, elk, wolf) + 1 aquatic (fish), S-2 GDD decision 4, plus the shipped graph

    // Flow graph (added 2026-11-18). Index-based, built once at load; the hot loop reads only these tables.
    struct CompiledFlowGraph {
        uint32_t NodeCount(NodeKind kind) const;
        NodeIndex Find(Dia::Core::StringCRC id) const;                       // kNoNode if absent
        EdgeSpan  FoodOf(NodeIndex eater) const;                             // eats edges, CSR
        EdgeSpan  EatersOf(NodeIndex food) const;
        const NodeIndex* EvaluationOrder() const;                            // by derived trophic level
    };
    bool CompileFlowGraph(const Json::Value& flowGraph, const SpeciesRoster& species,
                          CompiledFlowGraph& out, FlowGraphReport& report);  // validation errors and warnings

    // Seasonal presence (added 2026-11-18), on EcologySimulation:
    //   const SeasonalReservoir& GetReservoir(NodeIndex species) const;
    //   void SetReservoirReady(NodeIndex species, float mass);             // debug, `cow.ecology.seasonal`

    class EcologyModule : public Dia::ApplicationFlow::SimModule,
                          public CoW::Weather::IBiomeSeasonalBaseline {
    public:
        float GetSeasonalTemperature(WorldGeneration::Biome biome, CoW::Weather::Season season) const override;
        const EcologySimulation& GetSimulation() const;
        void RegisterSuccessionResolver(ISuccessionResolver* resolver);    // A3's SuccessionModule, via ModuleRef
        // F-3 World Assets (amended 2026-10-20, Feature 6): two adapters, one per kind, exposed by reference.
        //   EcologyPackParticipant       (kind `ecology`, defaults `cow.ecology_params`; species and biome tuning stays tuning.save)
        //   EcologyStartStateParticipant (kind `ecology_start`; Apply uses the same edit path as set_field and paint)
        // The edit set is cumulative: every cell field that differs from the freshly initialised state, including cells set by a
        // loaded start state; Capture() returns it sparse and sorted; a re-Initialise clears it. Initialise uses live parameters.
        IWorldAssetParticipant& GetEcologyPackParticipant();
        IWorldAssetParticipant& GetEcologyStartStateParticipant();
    };
}
```

Messages (`CellEcologyChanged`, `EventThresholdCrossed`, `BiomeSuccessionTriggered`, and from 2026-11-18 `SeasonalArrival`, `SeasonalDeparture`, `RunSpawned`, `RunBlocked`) are hand-written structs following the `GameSpeedMessages.h` pattern, in `Cluiche/CoW/Modules/EcologyMessages.h`.

## Features

| # | Feature | Description | Spec | Status |
|---|---------|-------------|------|--------|
| 1 | Ecology Data & Flow Graph | `species.json` (incl. `habitat`, aquatic groups, `coastal_reach`, the optional `seasonal` block), `biomes.json` (13 ids, `BiomeProfile`, seasonal temperature, `rainfall_baseline`, starting pool values, `weathering`), **`flow_graph.json` (envelope `cow.flow_graph`): node and edge schema, validation, `CompileFlowGraph` to CSR tables and trophic order, the shipped graph encoding today's roles** (amended 2026-11-18), `succession_rules.json`, `ecology_parameters.json`: load from `Assets/CoW/Global/Ecology/` with envelopes `cow.species`, `cow.biomes`, `cow.ecology_params` (amended 2026-10-19, F-3 World Assets), validate, defaults, fallback roster and graph. Pure logic, TDD | TBD | Draft |
| 2 | Cell State & Initialise | `EcologyFields` SoA, palettes, `EcologyWorldInput` from `WorldStructure`, derived geography (C11), seeded soil, starting populations, `IBiomeSeasonalBaseline` implementation. Pure logic, TDD | TBD | Draft |
| 3 | Cell Equations | Per-cell steps 1, 3–7 and 12 **over the compiled flow graph** (amended 2026-11-18): producer growth with pool uptake, per-species births, maturation, Allee, competition, starvation, predation (age-weighted) and scavenging along `eats` edges, deaths and waste along `returns` edges, disease, clamping; `StepCell`; `EcologyLedger` deltas by cause, written as each term is computed (ledger sums equal the actual change). One test group per equation from TechnicalSpec §4–§5, plus the **role-equivalence golden** (shipped graph = GDD role equations) and graphs the GDD never had (omnivore, a scavenger that also hunts, a two-pool producer). Pure logic, TDD | TBD | Draft |
| 4 | Grid Passes | Steps 8 and 11 plus pressure-field propagation (**water flow and the water table removed, A5 owns them; a thin step 2 builds `water_gradient` from A5's snapshot — amended 2026-11-17, aligned to locked GDD 2026-11-17, CHANGE-048**): migration solver (group and softmax, river and corridor modifiers), coastal cross-feeding, lifeboat guardrails, seed drift, natural ignition and fire spread, `years_low_herbivore_inflow`; step-outer passes, gather, `Dia::DeferredStep::IParallelExecutor`, bit-identity, 16 ms Season gate **plus the combined Season+Year+Decade+Century measurement** (Open Design Question 1). Pure logic, TDD | TBD | Draft |
| 5 | Ecology State & Succession | Health index plus hysteresis (C4), succession rule evaluation, forward and recovery, with the recovery multiplier (C7), the `ISuccessionResolver` seam at step 10, `BiomeSuccessionTriggered`, one-shot `ApplyBiomeTransition` (C9). Pure logic, TDD | TBD | Draft |
| 6 | Event Detection | Connected-cell clustering for drought front, predator wave, disease bloom; recolonization; `CellEcologyChanged` change set. Pure logic, TDD | TBD | Draft |
| 7 | EcologyModule | SimModule: data load and hot reload, `HydrologyUpdated` accumulation via `HydrologySnapshotInput` with the no-A5 fallback (amended 2026-11-17), `WorldGenerationComplete` init, `WeatherUpdated` accumulation via `WeatherSnapshotInput`, **registration as World Step stage `ecology` with `Capture`/`Run`/`Commit` and `SerializePending`/`DeserializePending`** (amended 2026-11-13; replaces `mSeasonPending` scheduling and the self-driven band gate), due-band resolution from the capture key, stub-input mode, publishing, `IBiomeSeasonalBaseline` via `ModuleRef`, `cow_main.diaapp` entry | TBD | Draft |
| 8 | Ecology Save/Load | `EcologySaveParticipant`: JSON arrays plus palettes, validate, save/continue/load/continue round-trip | TBD | Draft |
| 9 | Ecology Debug & Tools | `EcologyDebugDomain`: command model first (16 sim-hosted `cow.ecology.*` with mandatory help and rollback), then panel card, `CellDetailRegistry` rows with the change ledger + hover extras, DiaCellLayers layers, the decimated migration-arrow draw overlay, live tuning sliders + `tuning.save` write-back, cell edit/paint over the `cells.select.*` selection, `isolate_cell`, DiaCellHistory events, profiling, `cow.ecology.band_ms` with `MetricInfo`, health. **No cell inspector and no time-series panel** — DiaCellLayers owns the first, and DiaCellHistory's in-game panel was never built. Needs an HTML mockup before build | TBD | Draft |
| 10 | Nutrient Cycle | N1 (added 2026-11-18): detritus decay, ash release, weathering, uptake books, leaching, river transport along `downstream` in `upstream_order` with gather, river-mouth outflow, flood deposition from `flood_weeks_accumulator`; derived `downstream`/`river_mouth`/`upstream_order` at init; no-A5 fallback; nutrient ledger causes; conservation test every band; `ecology.nutrient.*` and `ecology.detritus.*` layers, save fields and panel books when Features 7–9 exist. Pure logic, TDD | TBD | Draft |
| 11 | Seasonal Presence | S1 and S2 (added 2026-11-18): the `seasonal` block, off-map cohort reservoirs, visitor arrival/bias/departure/stragglers, `edge_distance`, fish-run entry at mouths, flow-weighted forks, `run_reach`, blocking by ice and low flow, spawning, die-off to carrion (`nutrient.run_import`), young drifting out to the reservoir, the four events, `cow.ecology.seasonal`, `ecology.runs` overlay, reservoir save. Ships one visitor and one run species in the default roster. Pure logic first, TDD | TBD | Draft |

**Routed elsewhere:** the stage, its Controlled and Living modes, the guided proof scenarios (the old S-2 tests) and the long-run checks (the old S-3 tests) go to **S-3 Ecology** (S-2 and S-3 merged, 2026-10-18). Transition rules, legacy and recovery go to **A3**.

**Build order:** 1 → 2 → 3 → 5. Feature 4 follows 3. Feature 6 follows 4 and 5. Feature 7 follows 6. Features 8 and 9 follow 7 and can run in parallel. *Amended 2026-11-18:* Feature 10 follows 3 and 4 (it needs uptake and the gather pattern); Feature 11 follows 4 and 10 (migration bias, and die-off feeds the books). Both are pure logic and can land before or after 7; if after, their module, save and debug wiring are tasks in their own plans. Layer 2 ([a1-populationtokens.md](../a1-populationtokens/a1-populationtokens.md)) follows Features 1–11 and S-3 Controlled-mode tuning.

- Features 1–6 need no S-1, A3 or A4 code: their tests use hand-built hex grids and `StubWeatherInput`.
- S-3 Ecology needs Features 1–7 and 9, plus S-1 Features 1 and 8 (its worlds come from S-1).

## Binding Decisions

| ID | Decision | How it applies |
|----|----------|----------------|
| PD-001 | StringCRC for ids | Biome, species, functional group and rule ids are `StringCRC`. Fields store palette indices |
| PD-002 | ProcessingUnit/Phase/Module architecture | `EcologyModule` is a SimModule on SimPU. A4 finds the baseline provider and A3 finds the apply API through `ModuleRef`, never through singletons |
| PD-004 | No STL containers in public APIs | `EcologyFields` uses `DynamicArrayC`. Views expose accessors, not `std::` types |
| AD-001 | Ask permission before Dia/baseline edits | **None needed, and nothing A1 depends on is unbuilt in Dia** (re-verified 2026-11-13). A1 uses existing DiaMessageBus, DiaSaveGame, DiaMaths noise, DiaCellLayers, DiaCellHistory, DiaValueSchema, CellField (all built) and DiaDeferredStep (Approved, separately specced). `IParallelExecutor` moved into Dia under the sign-off of 2026-11-12, so A1 no longer proposes any shared-layer change |
| AD-002 | Four-tier dependency model | CoW-Shared: A3, A4, S-3 Ecology and later systems consume it |
| AD-003 | GDD owns design intent | GDD A1 owns the mechanics and constants. Deviations are listed below for the GDD owner |
| AD-005 | One stage per testbed | A1 owns no stage. S-3 Ecology hosts it |

## Deviations from GDD A1

These are for the DesignDocs owner to fold back into A1, A3, S-2 and S-3:

1. **One biome id set** (C1, C2): 8 primary + `Scrub`, `BurnedAshland`, `DesertifiedPlain` + `Ocean`, `Lake`. `species.json` affinity uses these ids. The lore names in the TechnicalSpec §1a, S-2 and S-3 examples are regions.
2. **Ocean and Lake run aquatic-only ecology, with coastal cross-feeding** (C3). This is new content: an aquatic roster and `coastal_reach` need authoring.
3. **Ecology state formula is new** (C4): a health index, one tier per move, `state_down_seasons` and `state_up_seasons`. S-2's five states become four plus a `recovering` flag.
4. **No `delta_scale`** (C5). Each band's steps run once per boundary at that band's rate. EcologySystem §7's schedule wins over TechnicalSpec §11's pseudo-code. Steps are step-outer full-grid passes.
5. ~~**The season pipeline runs on the sim frame after week 13 arrives.**~~ **Withdrawn 2026-11-13:** World Step's `Capture`/`Commit` are explicit SimPU points outside the bus passes, so A2's ordering guarantee (CH-A1R-06) holds after all and there is no deviation to fold back. Nothing is owed to the GDD here.
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
19. **Water is A5's** (amended 2026-11-17, A5 Hydrology). GDD step 2 "Water flow" and the Decade "water table drift" leave A1. The Decade water table stays deleted. Step 2 survives as a thin `water_gradient` build from A5 (aligned to locked GDD 2026-11-17, CHANGE-048). A1 reads soil moisture, river flow, flooding, inundation and ice from A5, and treats inundated cells as aquatic habitat. The GDD `water_dragon_emission` direct write is dropped; dragon water effects go through A4's influence API. Folded back: GDD A1 and A5 locked 2026-11-17 (CHANGE-048). The thin step 2 source formula is Cluiche-specified.
20. **The food web is a data flow graph** (2026-11-18). TechnicalSpec §1a's rule that code never names a species extends to producers, pools, detritus and roles. The fixed producer fields (`grass`, `shrub`, `tree`, `aquatic`) and transients (`carrion`, `waste`) become shipped nodes, and diets become weighted `eats` edges. The GDD's equations are unchanged in meaning (the role-equivalence golden proves it). **A3 amendment owed:** rule conditions that read `grass`, `soil_fertility` etc. use node ids.
21. **Nutrient cycle** (2026-11-18, new GDD content). `soil_fertility` stops being static: it is the `nutrient.soil` pool, fed by decay, ash, weathering and run die-off, drained by uptake, leaching and river-mouth outflow, moved downstream along rivers and spread by floods. A1 owns it and only reads A5, so **no A5 change**.
22. **Seasonal presence and off-map reservoirs** (2026-11-18, new GDD content; extends C10). Seasonal visitors enter from listed map edges, fish runs enter at river mouths, and each draws on an off-map cohort reservoir whose next arrivals depend on what left the map healthy. This is the only off-map state; lifeboats and hard edges are unchanged.
23. **Layer 2 is its own system spec, and herds can be persistent** (2026-11-18). See [a1-populationtokens.md](../a1-populationtokens/a1-populationtokens.md): the GDD's transient fronts (form on outflow, dissolve back into cells) are kept, and persistent herd tokens that own a flagged species' population are new GDD content.

Deviations 20–23 **folded back 2026-11-18**: GDD A1 re-locked under CHANGE-051 (DECISION-136 flow graph, 137 nutrients, 138 seasonal presence, 139 herd tokens). The GDD also dropped "Predation is one-way" (TechnicalSpec §1a) and reworded EdgeCaseMatrix §13.6 and backlog A1-70 (CH-A1R-02) to allow cycles, matching this spec. A3's GDD uses `nutrient.soil`.

## Dependencies

**Requires:**
*Statuses re-verified against code 2026-11-13; every "not built" below was stale.*

- **S-1 World Generation — fully built** (system `Done` 2026-11-13), including Feature 9 Regions and Sites (`SiteRegistry`, `SiteSet`, `RegionTable`), so `hearth_site`, `nexus_node` and `world_throne_type` **can be populated at init** — the "all false until Feature 9" caveat in Initial state is obsolete. A1 reads the world through `GetCurrentWorld()` (null until `GetWorldRevision() >= 1`) and `IsReady(revision)`; note `IsReady` takes a revision and there is no zero-arg form. A1 Features 1–6 still need no S-1 code.
- **S-1 `Cell::downstream`** (built) — the river network for nutrient transport and fish runs (added 2026-11-18). A1 reads it at init and needs no S-1 change.
- **S-1 Consumer Amendments, amendment 1** (@docs/specs/applications/cow/systems/s1-worldgeneration/consumer-amendments.md, Draft) — the shared `Biome` enum widened 10 → 13. **Gates A1 Feature 2.**
- **World Step** (@docs/specs/applications/cow/systems/worldstep/worldstep.md, Approved 2026-11-13) — A1 registers as stage `ecology`; the coordinator owns the cadence, the channel and history sampling. **Gates Feature 7.**
- **DiaDeferredStep** — system Approved 2026-11-12, all five feature specs Approved 2026-11-13 with plans. Supplies `IParallelExecutor::ParallelFor(count, minRange, fn)`, `SerialExecutor` and the capped executor. **Gates Feature 4's executor work**; A1 implements none of them.
- GameSpeedController (`SimulationTick`, `TimeBand`, `WeekOf`/`SeasonOf`, `kWeeksPerSeason`). Built. The band gate must use `>=`: `ResolveBand` returns only the largest band a day completes.
- `RandomStreamModule` (`A1` stream), E1 `SaveLoadModule`, DiaMaths seeded noise — all built.
- **DiaCellLayers — built.** Layer and overlay registration, `CellDetailRegistry`, selection and the `cells.*` surface. SimPU-only registration.
- **DiaCellHistory — built (engine scope).** Aggregates, tracking, events. **Its in-game panel was not built** — see Graphs.
- **DiaValueSchema — built.** Labels, ramps, `min`/`max`, `ui.hotkey`, and the `kind: "event"` entries A1's events need.
- **CellField (dia-needs #14) — built, and fully behind DiaCellLayers.** It never blocked A1's overlays: layers reach the screen through layer registration, and the migration-arrow overlay goes through `IDebugDraw`. A1 does not touch CellField.
- **`SimQueryHostModule`** — built. All `cow.ecology.*` commands register here with `SimHelpInfo`.
- `JobSystemService` + `TrySubmit` — built, but consumed only *through* DiaDeferredStep's executor. A1 holds no `ServiceStreamReader<JobSystem>` of its own.
- DiaMessageBus via `MessageBusHostModule`, `VisualDebuggerModule`, DiaCore (Json, StringCRC, containers, logging).

**Soft dependencies (fallbacks until they exist):**
- **A5 Hydrology** (amended 2026-11-17; GDD locked 2026-11-17, CHANGE-048; Cluiche spec Draft, unbuilt, @docs/specs/applications/cow/systems/a5-hydrology/a5-hydrology.md). Without it, A1 uses A4 rainfall or the pinned climate as its water input, and the river-overlay floor stays static. With it, A5 is stage `hydro` ahead of `ecology` in the World Step order. *Added 2026-11-18:* Features 10 and 11 also read `RiverFlow`, `Flooded`, `Inundated`, `PassabilityAt` and `LowFlowWeeks`, all already in the A1 hydrology seam; without A5, transport runs at base rate, nothing floods and nothing blocks a run.
- **A4.** Without it, `StubWeatherInput` is used and the panel reports the stub as the source.
- **A3.** Without it, triggers are published and counted but no biome changes. A debug `cow.ecology.apply_transition` command can apply one by hand.

**Requires nothing more from A4 for water:** A4's `rainfall` output no longer carries ground water (A5 amendment owed to A4).

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
- Flow graph (added 2026-11-18): each validation rule rejects or warns as listed; compilation is deterministic (same file, same indices, order and CSR); cycles get a stable trophic order; the role-equivalence golden; an edited graph with an omnivore runs and stays bounded.
- Nutrients (added 2026-11-18): conservation every band to tolerance; transport is order-independent (gather) and moves mass only along `downstream`; mouth outflow leaves the map; floods enrich only flooded cells; leaching rises with water; no-A5 fallback.
- Seasonal presence (added 2026-11-18): arrival empties the ready cohort into the listed edges only; departure returns edge populations to the reservoir; `die` stragglers become carrion, `stay` stragglers remain; a run splits at forks by flow, stops at `run_reach`, at ice and at low flow; die-off moves mass into carrion and books `run_import`; a bad map year gives smaller arrivals after `cohort_years`; a save mid-presence and mid-run reloads bit-identically.
- Determinism: same inputs give bit-identical fields. `SerialExecutor` vs 1, 3 and 17 ranges, and reversed range order, all give bit-identical fields after 40 years. **Added 2026-11-13:** a deferred run through World Step matches a synchronous run of the same bands bit-for-bit, and a capture-isolation test poisons the weather/world source after `Capture` to prove `Run` never reads Sim-owned state (DFS-002).

**Layer goldens (added 2026-11-13, "cell data over pixels").** Verification is by cell data and stats, not screenshots. Features 3–5 use `Dia/DiaCellLayers/Testing/CellLayerGolden.h` (`HashLayer`, `GoldenStore::Check`, update with `DIA_UPDATE_GOLDEN=1`) against `Tests/GoogleTests/CoW/Golden/ecology_layers.golden.json`: the default tracked layers plus `ecology.state` and `ecology.health`, over a fixed hand-built grid and roster after a fixed band count. Screenshots stay as a per-feature taste check for the user.

**Integration (Features 7–8):**
- The season pipeline runs once per season, after week 13 is accumulated (accumulator = sum of 13 stub weeks).
- No step runs between boundaries. Stub mode steps on `SimulationTick`.
- A4 receives baseline values for all 13 biome ids.
- (amended 2026-11-17, A5 Hydrology) With a stub A5, `water` follows the seasonal mean of `SoilMoisture`; with no A5 it follows the A4 rainfall fallback; the river-overlay floor rises and falls with `RiverFlow`.
- Save at year 5, continue to 10, load year 5, continue to 10: identical.

**Performance (Feature 4 gate):** one Season and one Year band at Large in Release with the job executor and 15 species, reported against 16 ms, with a per-step profile. Amended 2026-10-19, product review: also report Small and Standard against their scaled budgets (6.7 ms / 10.0 ms).

**Proof (routed to S-3 Ecology):** the GDD's S-2 scenarios (Controlled mode) and S-3's emergence, long-run and A1/A4 response tests (Living mode).

## Open Design Questions

1. **What does the Century tick cost?** (opened 2026-11-13, World Step Resolved Decision 1.) Bands run smallest-first on the same tick and `ResolveBand` returns only the largest, so the worst case is **Season + Year + Decade + Century in one `Run`** — whole-grid major succession plus legacy recolonization on top of everything else. A1's only gate covers Season (Feature 4 measures Season and Year), so this combination has never been budgeted; DiaDeferredStep's spec notes "the Century frame has no budget at all". World Step accepts the resulting SimPU stall once per ~100 game-years as reported-but-unmeasured. **Feature 4 owes a plan task measuring the combined tick at Large in Release.** If it badly exceeds `k`'s slack, the options are splitting the world channel in two or making the Century band incremental — the latter is an A1 design change.
   *(The former Open Question 1, "publishing on the frame after week 13", is resolved — see deviation 5.)*
2. **Starting constants.** The health-index weights and bands, the streak lengths, `coastal_reach` and the aquatic roster are new numbers with no GDD values. S-3 Ecology's Controlled mode tunes them first. If they move a lot, should the GDD lock them, or should they stay as tuning-file values?
3. **Memory at scale.** Dense `[species × cell]` arrays cost 15 species × 2 × 6,656 floats (about 0.8 MB), which is fine. If the roster grows past about 40 species, a sparse per-biome species set may be needed. This is revisited in Feature 4's benchmark.
4. **Same-tick Season+Year ledger loss (resolved 2026-10-19, DiaCellLayers/DiaCellHistory).** When the Season and Year bands run on the same tick (every 364 days), the last-band-only ledger is overwritten by the Year band, so the Season band's deltas are lost. **Resolved:** Recording the ledger's top causes per band as DiaCellHistory events preserves the Season delta information.
5. **Is one step per Season fine-grained enough for runs and visitors?** (opened 2026-11-18.) Presence changes four times a year, and a run moves `run_reach` cells per Season pass, so a run on a long river can take several Seasons to arrive. If S-3 shows it reads as jumps rather than a wave, the option is a light weekly presence pass on `WeatherUpdated` (runs advance and spawn weekly, everything else stays seasonal). That costs a weekly A1 hook World Step doesn't have today.
6. **What does the generic graph cost?** (opened 2026-11-18.) The CSR tables keep the hot loop index-only, but a richer web multiplies edges per cell. Feature 3 measures the shipped graph against the role-based reference; Feature 4's gate covers it at Large. If a large web misses 16 ms, the options are a per-biome active node set (Open Q3) or skipping zero-density food in the edge loop.
7. **How is a dynamic food web kept stable?** (opened 2026-11-18.) Data authors can now make webs the GDD never tuned: an apex omnivore, or two predators eating each other. Validation catches broken books, not boom-bust collapse. The fallback is S-3 Controlled mode plus the guardrails (lifeboats, extinction floor); a "web health" check in the panel (any node trending to zero over N years) may be worth adding during tuning.

## Future Extensions (breadth, not built here)

*Added 2026-11-18.* Ideas raised with the ecology expansion and deliberately not taken now. Most are data on the flow graph; the rest need new state.

| Idea | Fits the graph? | Shape |
|---|---|---|
| Pathogens as nodes | Mostly data | A `pathogen` node kind with `infects` edges to species and reservoirs in detritus or water; spreads with herd tokens (Layer 2). Replaces the per-species `disease_load` |
| Pollinators and insects | Data | Seasonal consumer nodes plus a `pollinates` edge that boosts a producer's growth; food for birds |
| Plant structure | New state | Grass, shrub and canopy as layered producers with shading, plus seed dispersal by animals and wind; changes A3's inputs, so A3 is part of it |
| Regional adaptation | New state | Per-region trait drift (cold or drought tolerance) per species; echoes CoW's dragon breeding theme |
| Invasive species | Data + events | Introductions that outcompete natives, possibly carried by dragons or herds |

Each becomes a spec amendment or its own spec when picked up. Their names are also in BACKLOG.
## World-sim review amendment (2026-11-18)

User decisions from @docs/research/worldsim_data_driven/summary.md (§0, §0.1). Each item supersedes any contradicting text elsewhere in this spec (Architecture, Features, Binding Decisions, Dependencies, Status). Bodies are not rewritten. The Biome note at "Biomes (C1, C2)" already carries its own "Superseded 2026-11-18" block; items 1 and 9 extend it. No plan exists; write it at `/implement` against this amendment.

1. **Parameters are a DiaAttribute `ParameterFile` (D1, D5, D6, D7).** Tuning constants and per-biome profile values (supersedes Data & Flow Graph, Nutrient cycle "Source of rates", Debug "Tuning", Public Interfaces, Feature 1):
   - Sidecar `Global/Ecology/ecology_parameters.schema.json` (format `cow.ecology_params`) owns each value's kind, range and default. Cross-field rules are sidecar `constraints: [{a, op, b}]`. **There is no base `ecology_parameters.json`.**
   - Strict load: a fault fails loudly (health Failing), no compiled fallback. Shipped data is gated by `dia validate schema`. Packs `Global/Ecology/Packs/<id>.ecology.json` are lenient and atomic (`ApplyOverrides`). The `EcologyParams` struct is filled by a `Bind`/`Snapshot` once per step.
   - `BiomeProfile` is dropped. Each per-biome column is **one map in the sidecar**: `ecology.biome.<column>`, `keyRef: cow.biomes`, complete. Columns: `rainfall_baseline`, pool starts, `soil_depth`, `weathering`, producer starts, `density`, `producer_target`, `consumer_target`, `water_optimum`. Seasonal temperature per biome stays A1-owned (A1 is the `IBiomeSeasonalBaseline` provider), as maps per season.
   - **`Global/Ecology/biomes.json` (`cow.biomes` in A1) is REMOVED.** Every A1 reference to `biomes.json` (Biomes, Initial state, Nutrient cycle "Weathering", Feature 1, Save/load, pack participant) now means the S-1 BiomeRegistry `Global/World/biomes.json` (`cow.biomes`, owned by S-1, @docs/specs/applications/cow/systems/s1-worldgeneration/biome-registry.md) for identity, roles and tags, plus the sidecar maps above for A1's values. Feature 1 ships no biome file.
   - **Records** (authors add entries): `species.json`, `flow_graph.json`, `succession_rules.json` (A3 owns the content) and token records load through DiaData `JsonDefinitionLoader`, validated by their sidecars.
2. **Stage is a `WorldStepStageBase` (World-Sim Kit, @docs/specs/applications/cow/systems/worldstep/worldsim-kit.md).** Supersedes the Placement `Capture/Run/Commit` sketch and "report Unhealthy".
   - `EcologyModule`'s stage derives `WorldStepStageBase`. A1 gains what it lacked: revision trio, `MutationQueue(128)` for tuning, paint and biome changes, the discard rule, visit generation and `IsReady` semantics.
   - Hooks: `LatchInputs` latches the weather accumulator and the due bands; `RunStep` steps the bands; `PublishCommit` publishes the ecology update.
   - Health is kOK / kDegraded / kFailing ("Unhealthy" becomes Failing). Stage id is `ecology`; World Step order is `["weather", "hydro", "ecology"]` (inline-renamed).
   - The base registers `cow.ecology.step_ms` and `cow.ecology.discarded_steps`. `cow.ecology.band_ms` may stay as an extra metric.
   - **To confirm at A1 re-plan: kit Open Question 1.** `WorldStepStageBase<TSim, TEntry, TLatch>` must fit A1's several bands per kick with an accumulator (`TLatch` carries the due bands). Split the base only if it does not fit.
3. **Save (A8) uses the kit codec (supersedes Save/load (E1) where it conflicts).** Kit codec helpers; **no in-record version** (`GetVersion` + `RegisterMigration`); participant id `cow.ecology` (was `ecology`); `params_hash = ParameterFile::Hash()`; validate ranges come from the layer sidecar; shared restore and pending blob (the DFS-005 pending-step text stays). Format `cow.ecology_state`.
4. **Commands (D) (supersedes Debug and tools "Tuning" and "Commands (16)", Feature 9).**
   - **One `cow.ecology.params {action: set|get|list|reload|save_defaults}`.** It replaces `cow.ecology.tuning.set`, `tuning.save` (now `save_defaults`, dev-only, writes the sidecar `default` fields in place) and the params half of `reload_data`.
   - Record reload is a separate **`cow.ecology.records {action: reload}`** (final name to confirm at re-plan). It reloads `species.json`, **`flow_graph.json` (it was missing from `reload_data`)** and `succession_rules.json`.
   - `reload_data` is removed. Registered through `SimCommandSet`. `cow.world.pack.save_as ecology <id>` stays the normal tuning path. The "16" command count in Feature 9 is stale until re-plan recounts.
5. **Naming (E, §0.1).** Layer ids stay `ecology.<snake>`. **Schema keys become `cow.ecology.layers/<field>`** (were `cow.ecology.cell/<field>`; supersedes the "Layer ids" bullet). Param keys are `cow.ecology.params/<key>`. Formats: `cow.ecology_params`, `cow.ecology_state`. Events are `Ecology<Thing>`.
6. **Events (F, A7).** Every ecology and succession event (`CellEcologyChanged`, `EventThresholdCrossed`, recolonization, `SeasonalArrival`/`SeasonalDeparture`, `RunSpawned`, `RunBlocked`, `BiomeSuccessionTriggered`, cause events, cluster events) is declared in `cow_worldsim.diagamemessages` and generated with `dia codegen messages`. **Ecology and cluster event types are a data catalogue**; new kinds are data. The fixed cluster types in Events (drought front, predator wave, disease bloom) are superseded: they become shipped catalogue entries, not code.
7. **Data I/O (G, A9).** Every file and sidecar is catalogued, paths resolve through `PathStore`, and `FileWatcher` reloads in dev builds (records hot reload; the biome registry does not).
8. **Conditions (H, A5a, A5b).** Seasonal `spawn.conditions` (Seasonal presence) and succession rule `conditions[]` (Succession rules, Feature 1 validation) are **DiaCondition `ConditionExpr`** (AND/OR/NOT) over the shared `WorldCellFields` per-cell context, with a cell cursor. Leaves are `{op, slot, field, value}`; fields are `slot.field` (`ecology.producer.grass`, `hydro.inundated_weeks`, `terrain.elevation`, `biome.tag.land`). The flat `{field, op, value}` AND-list is superseded. Load validation uses the new `ConditionExpr::Validate(const IConditionContext&)` overload. A1 contributes its committed fields to `WorldCellFields` under slot `ecology`. Paint and preset selection may use `CellSelectorSet`.
9. **Biomes and tags (I, A2a, A2b).** `BiomeIndex` comes from the registry (the enum is deleted; supersedes "Biomes (C1, C2)" including `BiomeFromId`, `IsGeneratedBiome`, the "13" count and the `Biome.` string prefix). **Tags replace biome lists:** "any land biome" becomes `biome.tag.land`; fire fuel (Natural fire "grass + shrub + tree") becomes producer nodes tagged `fuel` in the flow graph (biomass sum). **Ignition is never gated by the biome `fuel` tag.** Biome `fuel` means "has a fire-succession rule" (GDD-confirmed 2026-11-18; see biome-registry.plan.md "Confirmed biome tag assignment"). "Can this cell burn" is `biome.tag.land` plus biomass; named sums such as `producers_total` (layer `ecology.producers_total`) become **tag sums over flow-graph nodes**. An unknown biome falls back to the registry's `fallback` role (logged once), replacing "returns 0.5".
10. **Nutrients (J).** Per-pool behaviour is data: `leach_rate`, `transport_rate`, `flood_silt_rate` and weathering per nutrient pool (maps keyed by pool id, or per-pool record fields). The ash target pool is data (was hard-wired to `nutrient.soil`). A new nutrient is genuinely data-only. Supersedes the N1 table's "Source of rates", "Ash" and "Weathering" rows.
11. **Curves (K, A12).** Fit terms (water fit, temperature fit, health terms where a curve applies) are DiaUtilityAI `ResponseCurve` JSON (linear, quadratic, exponential, logistic, sine, step) authored in the params sidecar or records. Curves only, not `UtilitySet`.
12. **Searches (L, D4).** `edge_distance` and other hex searches use the DiaPathfinding grid search helpers (`MultiSourceSteps` and siblings, @docs/specs/applications/dia/systems/diapathfinding/grid-search-helpers.md), not a hand-written breadth-first search (supersedes the Initial state table's "by breadth-first search").
13. **World summaries (M, A13).** Ecology summary values go to the World Step-owned world blackboard via `Blackboard::Set` (@docs/specs/applications/dia/systems/diablackboard/blackboard-scopes.md). Gameplay reads them through `ModuleRef` / `Lookup`.

**Open at re-plan (not resolved here):** (a) record-load failure behaviour: Flow graph "Validation" says a bad graph falls back to the shipped graph and reports Degraded, while params load strict with no fallback; the decisions fix params only. (b) Kit Open Question 1 (item 2). (c) Final record-reload command name (item 4).



## Spec-6 resolutions (2026-11-18, user)

These settle open points raised while amending. Each item supersedes contradicting text above, including the world-sim review amendment.

1. **Record files fail loudly (R1).** `species.json`, `flow_graph.json`, `succession_rules.json` and token records load strictly. Any fault puts the ecology module in health Failing (naming the file, record and field), and it refuses to run. The flow-graph text "falls back to the shipped graph, Degraded" is **superseded**. Shipped data is gated by `dia validate schema`.
2. **Records reload** is the kit's `cow.ecology.records {action: reload|list}`. It includes `flow_graph.json`. This confirms the name marked "to confirm" in the amendment above.
3. **Saves** store biomes through the kit's biome palette (World-Sim Kit, spec-6 resolution 1).
4. **A3 glaciation** is two rules (18 rules in total). Protected cells use `terrain.is_protected`.

## Status

**Status:** `Approved` (amended and re-approved 2026-11-18, ecology expansion: flow graph, nutrient cycle, seasonal presence, Layer 2 split out). Previously `Approved` (2026-10-18), **revised and re-approved 2026-11-13 (dependency audit).** The ecology design is unchanged; what moved around it: the shared `Biome` enum (13, not a `StringCRC` set, and the biome palette index is dropped); registration as a World Step stage, which **deleted** `mSeasonPending`, deviation 5 and the former Open Question 1; `IParallelExecutor` moved to Dia; commands became sim-hosted with mandatory help and a 14-name budget; DiaCellLayers, DiaCellHistory, DiaValueSchema and CellField are all **built** (every "not built" status was stale) and CellField never blocked the overlays; S-1 Feature 9 is built so site flags populate at init; the cell inspector and selection are DiaCellLayers' and were deleted here; the time-series panel has no in-game host; `cow.ecology.band_ms` with `MetricInfo` and layer goldens were added; the `ISaveable` signature was corrected and pending-step serialization added. New Open Question 1 is the unmeasured Century tick.

**Amended 2026-11-17 (A5 Hydrology).** Water moves to A5: Feature 4 loses water flow and the water table, A1 gains a hydrology input with a no-A5 fallback, World Step order is `["weather", "hydro", "ecology"]`, `water_dragon_emission` is dropped. Status stays `Approved`; Features 4 and 7 text changed, and the unbuilt A5 spec is `Draft`. **Aligned to locked GDD 2026-11-17 (CHANGE-048):** thin step 2 `water_gradient` restored, inundated cells count as aquatic habitat, A5 counters exposed to A3. GDD A1 TechnicalSpec 13.4 is now written; it supplies the accumulators and the river-floor mean ratio, and the step 2 formula is Cluiche's. The rule schema gains `INUNDATION` and `to: none`; the shipped rule file has 17 rules.

Conflicts C1–C14 resolved in the interview on 2026-10-18. Amended the same day: change ledger (Feature 3), tools moved into A1 (Feature 9), and S-2/S-3 merged into one Ecology testbed (deviation 16). Amended again on A3's approval (2026-10-18): the `ISuccessionResolver` seam, recovery rules, `RemoveLegacy` removed, natural ignition, and `years_low_herbivore_inflow` (deviations 17–18). No plan yet; create `a1-ecology.plan.md` when implementation starts (Features 1–6 need no S-1, A3 or A4 code).

Amended 2026-11-18 (world-sim review decisions).
