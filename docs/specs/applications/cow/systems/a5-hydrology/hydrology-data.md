# Feature Spec: A5 Hydrology Data & Parameters

**Parent:** @docs/specs/applications/cow/systems/a5-hydrology/a5-hydrology.md (Feature 1)
**Status:** Approved (2026-11-17, user, after the locked-GDD alignment)
**Design source:** the locked GDD `C:\GitHub\DesignDocs\COW\Output Docs\GDD\08_SystemsDeepDive\A5_Hydrology\` (locked 2026-11-17: `A5_TechnicalSpec.md` §1, §13 and `A5_TuningParameters.md`; DECISION-129..134, CHANGE-047/048). The GDD owns design intent and constants (AD-003). Amended to match it (aligned to locked GDD 2026-11-17).
**Depends on:** S-1 consumer amendment 8 (basin table and bed elevation in the world record). Unit tests use hand-built terrain, so the amendment is needed only once Feature 3 reads real worlds.
**Amended 2026-11-18 (world-sim review, user decisions):** see "World-sim review amendment" below.

## Summary

The types, the parameters and `Initialise` for A5, as pure logic, following A4 Feature 1's conventions.

- **Field arrays** (`HydrologyFields`, `BasinState`, including each basin's `iceFraction`).
- **Read-only terrain copy** (`HydrologyTerrain`, including the basin table).
- **Parameters:** one descriptor table for the scalars, plus a separate `biome_table` block holding three values (`soil_capacity`, `infil_rate`, `veg_factor`) for each of the 13 biomes. Both are validated, loaded from a versioned file, merged from sparse overrides atomically, and settable by key.
- **`HydrologySimulation::Initialise`**, which copies the terrain, validates it, drops tiny depressions, replaces in-basin flow directions, seeds the starting state and builds the static tables Feature 2's passes need.
- **`ApplyPhaseSeed`**, the phase-scaled seed for the slow stores (S-1 lakes and glaciers), called before spin-up.

Defaults are the **GDD's first-run values** (`A5_TuningParameters.md`, aligned to locked GDD 2026-11-17). S-2.6 tunes them, and the tuned values are folded back to the GDD. A default the GDD does not define stays marked *provisional*.

**Units (user decision, kept).** All water is in **cell-depth units**: the same depth unit as A4's `waterToGround` and the GDD's "water units (wu)". There is no conversion. The GDD's `BASIN_DEPTH_SCALE` turns elevation into this unit (`basin.depth_scale`).

## Goals

1. Every field and parameter Feature 2 names exists, with a type, a range and a default.
2. **One mechanism per kind of value.**
   - **Scalars** go through the descriptor table, which drives load, range checks, defaults, `SetParam` and override merging.
   - **Per-biome values** go through the `biome_table` row descriptors, with the same five operations.
3. **The parameter file** uses a `format` + `version` envelope, rejects unknown keys and merges overrides atomically (A4's rules, and F-3's pack contract).
4. **`Initialise` rejects bad terrain** with a named reason, and otherwise leaves a seeded, ready simulation.
5. **The seed state shortens spin-up** (user decision 2026-11-17).
   - S-1 lakes start full, scaled for the climate-cycle phase the world starts in (`ApplyPhaseSeed`; a no-op at the default warm-point start).
   - Glaciers are seeded for the start phase the same way.
   - Soil and ground water start at `init.soil_fraction` and `init.ground_fraction` of capacity. Defaults are 0.5 and 0.3 (user decision 2026-11-17, kept over the GDD's start-at-0: seeding shortens the coupled spin-up).

## Non-Goals

- **Reading the world from S-1.** Feature 3 assembles `HydrologyTerrainSource` from `WorldGenerationModule` and the amendment 8 record.
- **Commands and live reload.** Hot reload, `cow.hydro.set_param` registration and pack participants are Features 3 and 5. This feature supplies the primitives they call.
- **The weekly passes.** These are Feature 2. This feature builds the static tables, and Feature 2's spec defines what they hold.
- **Save/load.** Feature 4 serialises the fields listed here as saved.

## Design

### Types (`Cluiche/CoW/Shared/Hydrology/HydrologyTypes.h`)

```cpp
namespace CoW::Hydrology {

    struct HydrologyFields {                         // N cells each; "saved" = Feature 4 writes it
        DynamicArrayC<float>    surfaceWater;        // saved
        DynamicArrayC<float>    soilMoisture;        // saved
        DynamicArrayC<float>    groundWater;         // saved
        DynamicArrayC<float>    riverStore;          // saved: the GDD's channel_store, on every land cell (hillslope and river)
        DynamicArrayC<float>    riverFlow;           // saved: readers between kicks need it (Feature 4)
        DynamicArrayC<float>    meanFlow;            // saved, river cells (Feature 2 flood rule)
        DynamicArrayC<float>    ice;                 // saved
        DynamicArrayC<float>    glacier;             // saved
        DynamicArrayC<float>    openWater;           // derived, [0,1] (A4 primary evaporation)
        DynamicArrayC<uint16_t> inundatedWeeks;      // saved
        DynamicArrayC<uint16_t> exposedWeeks;        // saved
        DynamicArrayC<uint16_t> snowYears;           // saved
        DynamicArrayC<uint8_t>  glacierEligible;     // saved
        DynamicArrayC<uint16_t> lowFlowWeeks;        // saved, river cells: consecutive low-flow weeks (A3 DryRiverbed)
        DynamicArrayC<uint16_t> glacierWeeks;        // saved: consecutive weeks with glacier >= glacier.biome_min (A3 glaciation, reads >= 260)
        DynamicArrayC<uint16_t> glacierFreeWeeks;    // saved: consecutive weeks with glacier < glacier.biome_min (A3 glacial retreat, reads >= 260)
        DynamicArrayC<uint8_t>  flooded;             // saved: readers between kicks need it (Feature 4)
        DynamicArrayC<uint8_t>  passability;         // derived, Passability values
    };

    struct BasinState {                              // per basin
        DynamicArrayC<float> volume;                 // saved
        DynamicArrayC<float> iceFraction;            // saved, [0,1]: frozen share of the open surface (Feature 2 pass 1)
        DynamicArrayC<float> level;                  // derived from volume
    };

    struct BasinRecord {                             // from S-1 amendment 8, read-only
        uint32_t firstCell, cellCount;               // into HydrologyTerrain::basinCells
        float    spillHeight;
        uint32_t spillCell;
        uint8_t  isLake;                             // was a lake at generation
    };
    // Initialise drops tiny depressions from the working basin set (see Initialise).
    // BasinState arrays are sized to the real basins that remain.

    struct HydrologyTerrain {                        // copied at Initialise, never refreshed except biome
        uint16_t width, height;
        DynamicArrayC<float>    elevation;           // Cell::elevation (lake surface on lakes)
        DynamicArrayC<float>    bedElevation;        // amendment 8 (== elevation off-lake)
        DynamicArrayC<uint8_t>  flowDirection;       // 0..5, kFlowSink = 6, every cell
        DynamicArrayC<uint8_t>  waterSource;         // WorldGeneration::WaterSource
        DynamicArrayC<uint8_t>  biome;               // WorldGeneration::Biome; updated by A3 through Feature 3; saved by Feature 4 (S-1's cells keep the old biome)
        DynamicArrayC<uint8_t>  isRiver;             // waterSource == River
        DynamicArrayC<uint8_t>  isCrossing;          // protected cells (GDD is_protected): corridor sites + Pass + RiverCrossesBarrier + Hearth, Nexus, world-throne cells
        DynamicArrayC<uint32_t> basinCells;          // concatenated, per BasinRecord
        DynamicArrayC<BasinRecord> basins;
        bool basinsFromLakesOnly = false;            // version 1 world (amendment 8 AC 37): lakes pinned at their surface, lake cells always Deep except protected cells, which stay Fordable (GDD EC-15)
    };

    enum class Passability : uint8_t { Dry, Wet, Fordable, Deep, Frozen };
    enum class RiverState  : uint8_t { Dry, Low, Normal, Flood, Frozen };   // derived by the snapshot (Feature 3), aligned to locked GDD 2026-11-17
}
```

At Large (6,656 cells) the fields are about 210 KB per copy. Two copies (committed + working) are well under 1 MB.

**`isCrossing`** is the GDD's `is_protected`. It marks every cell the world must never block (aligned to locked GDD 2026-11-17):
- corridor-node sites (all kinds, including river crossings)
- cells with `TerrainFlag::Pass`
- cells with `TerrainFlag::RiverCrossesBarrier`
- **fixed-site cells: Hearth Sites, Nexus Nodes and world-throne sites**

Feature 2 never makes these `Deep`: it caps them at `Fordable`. For fixed sites this is a safety net. S-1 already never places a Hearth, Nexus or world-throne cell below a real basin's spill height (GDD Key Design Decision 5; S-1 AC 38 reads `basin.min_cells` and `basin.min_capacity` from this feature's parameters to decide which basins are real).

### Scalar descriptor table (`HydrologyParameters.h`)

`ParamDescriptor { group, key, member, min, max, default, kind }`, the same shape as A4's. **43 rows** (aligned to locked GDD 2026-11-17). Defaults are the GDD's first-run values. The *GDD name* column maps the GDD's `UPPER_SNAKE` constant to the Cluiche dotted key. The hard range is for validation. It is wider than the GDD's tuning range, which is guidance only.

| Key | GDD name | Default | Range | Used by (Feature 2 pass) |
|---|---|---|---|---|
| `freeze.threshold` | `FREEZE_TEMP` | 0.25 | 0.01–1 | 1 (below this, water freezes) |
| `freeze.thaw_temp` | `THAW_TEMP` | 0.32 | 0–1 | 1 (above this, ice melts; hysteresis) |
| `freeze.rate` | `FREEZE_RATE` | 0.5 | 0–1 | 1 |
| `freeze.thaw_rate` | `THAW_RATE` | 0.6 | 0–1 | 1 |
| `freeze.ice_melt_floor` | `ICE_MELT_FLOOR` | 0.01 | 0.0001–1 | 1, 2 |
| `freeze.ice_crossing_min` | `ICE_CROSSING_MIN` | 0.6 | 0–10 | 9 (ice, or basin ice fraction, at which water is `Frozen`; renamed from `ice_crossable`) |
| `soil.frozen_ground_factor` | `FROZEN_GROUND_FACTOR` | 0.1 | 0–1 | 3 |
| `soil.field_capacity` | `FIELD_CAPACITY` | 0.6 | 0–1 | 3 |
| `soil.percolation_rate` | `PERCOLATION_RATE` | 0.1 | 0–1 | 3 |
| `soil.gw_capacity` | `GW_CAPACITY` | 2.0 | 0.01–100 | 3 (springs above it) |
| `evap.et_rate` | `ET_RATE` | 0.15 | 0–1 | 4 |
| `evap.surface_rate` | `SURFACE_EVAP_RATE` | 0.3 | 0–1 | 4 |
| `routing.surface_retention` | `SURFACE_RETENTION` | 0.02 | 0–5 | 5 |
| `routing.baseflow_rate` | `BASEFLOW_RATE` | 0.02 | 0–1 | 5 |
| `routing.release_min` | `RELEASE_MIN` | 0.4 | 0–1 | 6 (was `r_min`) |
| `routing.release_slope` | `RELEASE_SLOPE` | 10.0 | 0–1000 | 6 (was `r_slope`) |
| `routing.release_floor` | `RELEASE_FLOOR` | 0.3 | 0–1 | 6 (clamp floor) |
| `routing.release_ceil` | `RELEASE_CEIL` | 0.85 | 0–1 | 6 (clamp ceiling, below 1; was `r_max`) |
| `routing.hillslope_release` | `HILLSLOPE_RELEASE_RATE` | 0.9 | 0–1 | 6 (non-river cells; was a fixed 1.0) |
| `basin.evap_rate` | `OPEN_WATER_EVAP_RATE` | 0.05 | 0–1 | 6 |
| `basin.depth_scale` | `BASIN_DEPTH_SCALE` | 20.0 | 0.1–1000 | Initialise, 6, 7, 9 (cell-depth per unit of elevation over one cell) |
| `basin.ford_depth` | `FORD_DEPTH` | 0.5 | 0–100 | 9 (cell-depth; it was elevation units) |
| `basin.min_cells` | `MIN_BASIN_CELLS` | 3 | 1–1000 | Initialise, and S-1 AC 38 (whole number) |
| `basin.min_capacity` | `MIN_BASIN_CAPACITY` | 1.0 | 0–1000 | Initialise, and S-1 AC 38 (cell-depth) |
| `flood.mean_weeks` | `MEAN_FLOW_HALF_LIFE_WEEKS` | 104 | 4–1040 | 8 (a **half-life**: `α = 1 − 0.5^(1/mean_weeks)`) |
| `flood.bankfull_factor` | `BANKFULL_FACTOR` | 1.8 | 1–20 | 8 |
| `flood.min_flow` | `MIN_MEAN_FLOW` | 0.001 | 0–10 | 7, 8 |
| `flood.floodplain_rise` | `FLOODPLAIN_RISE` | 0.02 | 0–1 | 8 (elevation units) |
| `flood.recharge_share` | `FLOOD_RECHARGE_SHARE` | 0.3 | 0–1 | 8 (share of the surplus that soaks into floodplain soil) |
| `river.low_flow_fraction` | `LOW_FLOW_FRACTION` | 0.1 | 0–1 | 8 (`lowFlowWeeks`), Feature 3 `RiverState` (was 0.2) |
| `glacier.melt_threshold` | `GLACIER_MELT_TEMP` | 0.4 | 0–1 | 2 |
| `glacier.melt_rate` | `GLACIER_MELT_RATE` | 0.02 | 0–1 | 2 |
| `glacier.snow_floor` | `GLACIER_SNOW_FLOOR` | 0.2 | 0–1 | 10 |
| `glacier.onset_years` | `GLACIER_ONSET_YEARS` | 3 | 1–50 | 10 (stored as float, read as whole years) |
| `glacier.check_week` | `GLACIER_CHECK_WEEK` | 26 | 1–52 | 10 (end of Summer; see Open Design Question 3) |
| `glacier.biome_min` | `GLACIER_BIOME_MIN` | 0.5 | 0–100 | 8 (`glacierWeeks` counts glacier at or above this; `glacierFreeWeeks` counts below it) |
| `wetness.surface_scale` | `SURFACE_WETNESS_SCALE` | 0.1 | 0.001–10 | Feature 3 `SurfaceWetness` |
| `wetness.soil_weight` | `SOIL_WETNESS_WEIGHT` | 0.25 | 0–1 | Feature 3 `SurfaceWetness` |
| `wetness.river_open_water` | `RIVER_OPEN_WATER` | 0.3 | 0–1 | 7 (a river's open-water share at mean flow; A4 §6.1) |
| `init.soil_fraction` | none (Cluiche) | 0.5 | 0–1 | Initialise (user decision; the GDD starts soil at 0) |
| `init.ground_fraction` | none (Cluiche) | 0.3 | 0–1 | Initialise (share of `gw_capacity`; user decision; the GDD starts it at 0) |
| `spinup.lake_seed_cold` | `LAKE_SEED_COLD` | 0.6 | 0–1 | `ApplyPhaseSeed` |
| `spinup.glacier_seed_max` | `GLACIER_SEED_MAX` | 1.0 | 0–100 | `ApplyPhaseSeed` (cell-depth) |

Not in this table, on purpose:
- **`SPINUP_WEEKS` (52)** is the module's `spinup_weeks` config key (Feature 3), not a hydrology parameter.
- **`BALANCE_TOLERANCE`** (GDD 1e-5) is replaced by Feature 2's own test, `1e-4 · max(in, out, 1)` per week plus a cumulative drift bound (user decision, kept). See "GDD differences".
- **`INFLUENCE_GROUND_WATER` (0.05)**. A4 Feature 7 hands the influence in as a depth (Feature 3, "GDD differences").

Cross-row rules, each failing with both keys named:
- `release_floor ≤ release_ceil`
- `freeze.threshold ≤ freeze.thaw_temp`
- `basin.min_cells`, `glacier.onset_years` and `glacier.check_week` are whole numbers

### Biome table (`biome_table` block)

**Three values per biome** (aligned to locked GDD 2026-11-17), keyed by the biome's id string (`BiomeId`). The earlier columns `ground_capacity`, `percolation_rate`, `et_rate` and `retention` are now the global scalars `soil.gw_capacity`, `soil.percolation_rate`, `evap.et_rate` and `routing.surface_retention`. The row descriptors are `{ column, min, max }`, and every column is validated for every biome.

| Column | Range | Meaning |
|---|---|---|
| `soil_capacity` | 0–5 | Most water the soil holds (cell-depth) |
| `infil_rate` | 0–5 | Most water entering soil per week (cell-depth per week; it was a share) |
| `veg_factor` | 0–5 | Multiplier on soil evapotranspiration |

GDD values. The GDD's `Grassland` is `GrasslandSteppe` here, `Wetland` is `WetlandDelta`, `Desert` is `DesertBasin`, `Alpine` is `AlpineMountain`, `Tundra` is `TundraBoreal` and `Volcanic` is `VolcanicAshland`.

| Biome | soil_capacity | infil_rate | veg_factor |
|---|---|---|---|
| Ocean | 0 | 0 | 0 |
| Lake | 0 | 0 | 0 |
| GrasslandSteppe | 0.6 | 0.10 | 1.0 |
| TemperateForest | 0.9 | 0.15 | 1.2 |
| WetlandDelta | 1.0 | 0.05 | 1.1 |
| DesertBasin | 0.3 | 0.12 | 0.3 |
| AlpineMountain | 0.3 | 0.05 | 0.4 |
| TundraBoreal | 0.4 | 0.04 | 0.5 |
| Rainforest | 1.0 | 0.15 | 1.4 |
| VolcanicAshland | 0.3 | 0.20 | 0.3 |
| Scrub | 0.5 | 0.10 | 0.7 |
| BurnedAshland | 0.4 | 0.06 | 0.3 |
| DesertifiedPlain | 0.3 | 0.06 | 0.3 |

Ocean and Lake rows are zero because their cells have no soil budget (GDD: "water cells; no soil budget"). Basin and river water use their own stores. An unknown biome id uses `GrasslandSteppe`'s row and logs once per id (GDD EC-09).

### Parameter file

The envelope is `{"format": "cow.hydrology_params", "version": 1, "hydrology_parameters": { <groups> }, "biome_table": { "<BiomeId>": { <columns> } } }`.

- Defaults live at `Cluiche/Assets/CoW/Global/Hydrology/hydrology_parameters.json` (F-3 layout).
- **Load** validates every key and range:
  - An unknown group, key, biome id or column is an error, not a warning.
  - A missing key keeps its default.
  - A missing biome row keeps its default row.
- **`ApplyOverrides(json, inOut, err)`** applies a sparse object over an existing set, for scalars and biome rows alike, under the same validation. It is atomic: on any failure `inOut` is byte-identical. F-3 hydrology packs are overrides-only and use this.
- **`SetParam(key, value)`** works on a dotted scalar key, or on `biome_table.<BiomeId>.<column>`. It runs the same checks as load.
- **`ToJson`** writes the full set, in descriptor order and biome enum order. Writing the result of a load gives back the same bytes.

### `HydrologySimulation::Initialise`

```cpp
bool Initialise(const HydrologyTerrain& terrain, const HydrologyParameters& params,
                Dia::Core::Containers::String256& err);
void ApplyPhaseSeed(const float* coldness, const uint8_t* snowSurvivesSummer);   // per cell, N each; null coldness = 0; after Initialise, before spin-up (seam fix 2026-11-17)
```

**Validation** (fails with `err` naming the first problem, and leaves the simulation un-initialised):
- array sizes equal `width × height`
- flow directions in 0..6
- every basin cell index in range and in exactly one basin
- spill cell in range and not inside its own basin
- bed elevation ≤ elevation on every cell
- with `basinsFromLakesOnly`, every basin has `isLake`
- **GDD EC-14 (aligned to locked GDD 2026-11-17):** no flow-direction cycle; no spill cell whose elevation is above its basin's spill height; no footprint cell with a river `downstream` that leaves the basin (other than at the spill cell). The topological sort over cells and basin nodes must succeed, or `Initialise` fails with the cycle named

**Tiny depressions are not basins** (GDD CH-A5-03, aligned to locked GDD 2026-11-17). Capacity is `V(spillHeight) = Σ (spillHeight − bed) · basin.depth_scale` over footprint cells with bed below the spill height. A basin with fewer than `basin.min_cells` cells, or with capacity below `basin.min_capacity`, is dropped from the working basin set:
- its cells keep S-1's flow directions, route straight through and are never inundated;
- S-1 lake basins (`isLake`) are never dropped (they are on the map as water);
- the number dropped is logged once at Info and available to the debug state query.

**In-basin flow directions** (GDD §7.2). S-1's directions inside a real basin follow the filled surface toward the spill point, which would carry water straight across a dry basin. For each real basin, `Initialise` replaces them with steepest descent on `bedElevation` toward the basin's lowest cell (the pit). A footprint cell with no lower footprint neighbour is routed through its lowest-bed neighbour that is nearer the pit by a flood fill from the pit, so every footprint cell reaches the pit (the exact tie rule is a plan task; Open Design Question 3). Cells outside every real basin keep S-1's directions. `basinsFromLakesOnly` worlds have no override: lake bed equals surface, so there is nothing to descend.

**Seeding:**
- Fields are allocated and zeroed.
- On land, `soilMoisture = init.soil_fraction · soil_capacity[biome]` and `groundWater = init.ground_fraction · soil.gw_capacity`. Both fractions default to 0.5 and 0.3 (user decision 2026-11-17).
- Each lake basin's `volume` is its capacity (full); non-lake basins start at 0. `iceFraction` starts at 0.
- `meanFlow` starts at 0, and Feature 2's flood rule needs `meanFlow > flood.min_flow`, so nothing floods in the first weeks.
- `level`, `openWater` and `passability` are derived once, so a reader sees a consistent state before the first `Step`.

**`ApplyPhaseSeed(const float* coldness, const uint8_t* snowSurvivesSummer)`** (GDD §13, aligned to locked GDD 2026-11-17; per-cell (seam fix 2026-11-17, user)). Called once, after `Initialise` and before spin-up. `coldness[cell]` is the cycles' normalised coldness at the world's start phase: 0 at the warm point, 1 at the coldest (A4 `WeatherRunner::StartColdness`). Null means 0 everywhere. Feature 3 passes both arrays in. It overwrites only the slow stores:
- every lake basin: `volume = capacity · lerp(1.0, spinup.lake_seed_cold, basinColdness)`, where `basinColdness` is the mean of `coldness` over the basin's cells, summed in ascending cell order;
- every cell flagged in `snowSurvivesSummer` (A4 `WeatherRunner::StartSnowSurvivesSummer`, null = none): `glacier = spinup.glacier_seed_max · coldness[cell]`, `snowYears = glacier.onset_years`, `glacierEligible = 1`;
- it re-derives `level`, `openWater` and `passability`.

At `coldness = 0` (the default start at the warm point) it changes nothing: full lakes, no glaciers. Soil, ground water, channel stores and ice are not seeded.

**Static tables** (contents defined by Feature 2):
- downstream index (with the in-basin override)
- the real-basin set and its capacity curves
- topological order over cells and basin nodes, and its trees
- per-cell release rates

They are built here so `Step` allocates nothing.

**Re-initialise.** Calling `Initialise` again with a new terrain discards everything. It is how Feature 3 handles a new world.

## Public API

Files under `Cluiche/CoW/Shared/Hydrology/`:

| File | Contents |
|---|---|
| `HydrologyTypes.h/.cpp` | Fields, basin state, terrain, `Passability` |
| `HydrologyParameters.h/.cpp` | Struct, descriptor table, biome table, load, overrides, `SetParam`, `ToJson`, defaults |
| `HydrologySimulation.h/.cpp` | Class, `Initialise`, `ApplyPhaseSeed`, accessors `Fields()`, `Basins()`, `Terrain()`, `IsInitialised()` |

## Acceptance Criteria

### Types
1. Each field has N entries after `Initialise`, and each basin array has one entry per basin.
2. The "saved" set listed above is exposed as a constant list Feature 4 can iterate. Derived fields are not in it.

### Parameters
3. The descriptor table has 43 rows with unique dotted keys. Every default lies inside its own range, and the defaults equal the GDD values in the table above (a test lists them, so a drift from the GDD fails).
4. The biome table has 13 rows (one per `Biome`, enum order) × 3 columns. Every default lies inside its column range. The Ocean and Lake rows are zero.
5. **Loading the shipped defaults:**
   - loading the shipped `hydrology_parameters.json` equals `DefaultHydrologyParameters()`;
   - `ToJson(defaults)` equals the shipped file byte for byte.
6. **Load rejects bad files:**
   - Load fails with the offender named for: an unknown group, key, biome id or column; a wrong `format`; or a `version` other than 1.
   - Load also fails, with the key named, for an out-of-range value, `release_floor > release_ceil`, or `freeze.threshold > freeze.thaw_temp`.
7. A file missing keys or biome rows loads, and the missing ones keep their defaults.
8. `ApplyOverrides` with a sparse object (one scalar plus one biome cell) changes only those two values. With one bad entry among good ones, it changes **none** and `inOut` is byte-identical.
9. `SetParam` accepts dotted scalar keys and `biome_table.<BiomeId>.<column>`. It rejects unknown keys and out-of-range values, and leaves the set unchanged.

### Initialise
10. Hand-built terrain with two lakes, one dry basin and a river initialises:
    - lake volumes equal their capacity;
    - the dry basin's volume is 0, and every `iceFraction` is 0;
    - land soil equals `init.soil_fraction · soil_capacity`, and ground water equals `init.ground_fraction · soil.gw_capacity` (at the 0.5 / 0.3 defaults, and with 0 to match the GDD's start);
    - every derived field is consistent (levels match volumes, `openWater` = 1 exactly on inundated cells).
11. Each validation failure listed above is rejected with a message naming it, and the simulation stays un-initialised.
12. With `basinsFromLakesOnly`, a terrain whose basins are all lakes initialises, and one with a non-lake basin is rejected. No basin is dropped by the tiny-depression filter, and lake cells keep their S-1 directions.
13. Re-initialising with a different terrain size gives correctly sized fields and no trace of the old world.
14. `isCrossing` cells are exactly the union of corridor sites, `Pass` cells, `RiverCrossesBarrier` cells and Hearth, Nexus and world-throne site cells on a hand-built terrain.
15. `Initialise` is deterministic: two calls on the same inputs give byte-identical fields and tables.
16. **Tiny depressions:** a depression of 2 cells, and one of 5 cells with capacity 0.5, are not in the working basin set. Their cells keep S-1's directions and are never inundated. A 4-cell depression with capacity 1.5 is kept. A tiny S-1 lake is kept. Changing `basin.min_cells` or `basin.min_capacity` changes the set.
17. **In-basin directions:** on a dry basin whose S-1 directions cross it toward the spill, every footprint cell's downstream path reaches the pit, and each step goes to a lower bed elevation or follows the flood-fill fallback. Cells outside the footprint are unchanged. A `basinsFromLakesOnly` terrain has no override.
18. **Topological order:** every cell comes after all its upstream cells. A basin node comes after every footprint cell and every cell upstream of the basin, and before its spill cell. A terrain with a flow cycle, a spill cell above its spill height, or a river leaving a basin footprint is rejected with the cause named (GDD EC-14).
19. **`ApplyPhaseSeed`:** at `coldness = 0` it changes nothing. At `coldness = 1` lake volume is `capacity · lake_seed_cold`, and glaciers are `glacier_seed_max` with `snowYears = onset_years` and `glacierEligible = 1` on the masked cells only. At 0.5 lake volume is the midpoint. A null mask seeds no glacier. Levels, `openWater` and `passability` are re-derived.

## Files Touched

| File | Change |
|---|---|
| The three file pairs above | New |
| `Cluiche/Assets/CoW/Global/Hydrology/hydrology_parameters.json` | New, the GDD's first-run defaults |
| `Cluiche/Tests/GoogleTests/CoW/Hydrology/TestHydrologyTypes.cpp`, `TestHydrologyParameters.cpp`, `TestHydrologyInitialise.cpp` | New |
| `Cluiche/CoW/Shared/Hydrology/Testing/HydrologyTestGrids.h` | New (shared with Feature 2): hand-built terrains |
| `CoW.vcxproj`, `GoogleTests.vcxproj` (+ filters) | Via `dia docs vcxproj-add` |
| CoW `pipeline.toml` | Deploys `Global/Hydrology/` |

No Dia files, so no AD-001 sign-off.

## Binding Decisions

| ID | Decision | How this feature complies |
|----|----------|---------------------------|
| PD-004 | No STL containers in public APIs | `DynamicArrayC`. Errors use `String256`, keys `String64`. The descriptor tables are returned as `(pointer, count)` pairs (A4 Feature 1's choice) |
| AD-002 | Four-tier model | CoW-Shared, pure logic |
| AD-003 | GDD owns design intent | Defaults, names and the fixed-site rule follow the locked GDD (2026-11-17). Remaining differences are listed under "GDD differences" |

## Test Plan

All GoogleTests, on hand-built terrain only; S-1 isn't needed. Written test-first (RED, then GREEN), in this order:
1. Types
2. Descriptor table and defaults
3. Biome table
4. Load and validation
5. Overrides and `SetParam`
6. `ToJson` round trip
7. Initialise seeding, tiny-depression filter and in-basin directions
8. Initialise validation and topological order
9. `ApplyPhaseSeed`

Scoped filter `*Hydrology*`.

## GDD differences

Only real remaining differences (aligned to locked GDD 2026-11-17):

| Topic | GDD | This spec |
|---|---|---|
| Units | "water units (wu)" | The same depth unit, called cell-depth here (user decision). No conversion |
| Initial soil and ground water | Start at 0 | Seeded at 0.5 and 0.3 of capacity (user decision 2026-11-17) to shorten the coupled spin-up. Setting both to 0 gives the GDD's start |
| Balance tolerance | `BALANCE_TOLERANCE` 1e-5 × stored | Feature 2's `1e-4 · max(in, out, 1)` per week plus a cumulative drift bound (user decision: tight check, double totals) |
| Tiny S-1 lakes | Not stated | An S-1 lake is always a basin, even below the minimum size (Open Design Question 4) |
| `INFLUENCE_GROUND_WATER` | A5 constant, 0.05, times A4's weight | Not an A5 parameter; A4 Feature 7 hands the amount in as a depth |

## Open Design Questions

1. ~~**Biome capacities need re-reading once Feature 2 runs.**~~ Partly **resolved by the GDD**: the biome table is now the GDD's `soil_capacity`, `infiltration_rate` and `veg_factor`. Only S-2.6 shows whether rivers stay alive through a dry summer, so the first tuning pass may still change the numbers.
2. **Are some values per region, not per biome?** Soil depth arguably follows terrain (thin on hills and mountains, deep in valleys) more than biome. If S-2.6 shows biome-only capacities look wrong on steep ground, add a terrain multiplier column keyed by `TerrainType` rather than growing the biome table.
3. **Which week is `glacier.check_week`?** The GDD says "week of the year 26, end of Summer". Feature 2's `WaterInput.week` is 0..51. Plan task 1 fixes the mapping (likely: the GDD's 26 is the 26th week, index 25) and tests it against `kWeeksPerSeason`.
4. **Should a tiny S-1 lake be an A5 basin?** The GDD filter is about priority-flood depressions. A lake that S-1 already placed on the map must hold water, so it is kept. If S-1 never generates lakes below the minimum, the question does not arise.
5. **Exact in-basin tie rule.** "Steepest descent toward the pit" has dead ends on a rough bed. The flood-fill fallback above is this spec's choice. The GDD gives the intent only.

## World-sim review amendment (2026-11-18)

**Each item supersedes any contradicting text in this spec.** Full list: @docs/specs/applications/cow/systems/a5-hydrology/a5-hydrology.md.

| # | Item | Supersedes |
|---|---|---|
| 1 | The sidecar `Global/Hydrology/hydro_parameters.schema.json` (format `cow.hydro_params`) is the single source of kind, range, default and description. The 43 rows' keys, GDD names, defaults and ranges move into it as data. The C++ `ParamDescriptor` table is deleted | Summary "Parameters" bullet; Goals 2-3; Design "Scalar descriptor table" (`ParamDescriptor { group, key, member, min, max, default, kind }`, 43 rows); Files Touched `HydrologyParameters.h/.cpp` row |
| 2 | **No base file.** `hydrology_parameters.json` is not shipped. Runtime value = sidecar default, then packs in order, then `set`. "Save as defaults" is `cow.hydro.params {action: save_defaults}` (dev only) | Design "Parameter file" (envelope, "Defaults live at ..."); Files Touched `hydrology_parameters.json`; AC 5 (shipped file equals `DefaultHydrologyParameters()`, byte-for-byte `ToJson`) |
| 3 | Load is strict through `ParameterFile`: Failing health, no compiled fallback, no `DefaultHydrologyParameters()`. `ApplyOverrides` becomes `ApplyPack` (lenient: unknown key dropped with a Warning, out-of-range clamped with a Warning; atomic; constraints reject the whole pack). `SetParam` becomes `ParameterFile::Set` (strict). `ToJson` becomes `ParameterFile::ToJson(Full/Diff)`. The params hash becomes `Hash()` | Design "Parameter file" Load, `ApplyOverrides`, `SetParam` and `ToJson` bullets; Public API "Parameters"; ACs 6-9 (the "unknown key is an error" rule holds for the sidecar and `Set`, not for packs) |
| 4 | Cross-row rules are sidecar `constraints` (`routing.release_floor <= routing.release_ceil`, `freeze.threshold <= freeze.thaw_temp`), checked by C++, the CLI and T-2 with both keys named. The whole-number rules (`basin.min_cells`, `glacier.onset_years`, `glacier.check_week`) are `int` kinds. A GDD tuning range that differs from the hard range may be sidecar `bands` | Design "Cross-row rules" bullets; AC 6 constraint clause; ODQ 1 wording ("first tuning pass") |
| 5 | `biome_table` becomes **one map per column**, each `keyRef cow.biomes` and complete: `hydro.biome.soil_capacity`, `hydro.biome.infil_rate`, `hydro.biome.veg_factor`. Keys are registry ids; the GDD-to-enum name mapping (`Grassland` = `GrasslandSteppe` ...) is replaced by the registry ids. Ocean and Lake values stay zero. Set key form: `hydro.biome.<column>.<BiomeId>`. **An unknown biome uses the registry `fallback` role**, not `GrasslandSteppe` | Design "Biome table (`biome_table` block)", including the row keys and the unknown-id sentence; AC 4 (13 rows, enum order); AC 9 key form |
| 6 | `ParamBinding<HydrologyParameters>` provides the per-step snapshot; `Resolve` reports no unbound parameter and no unbound binding | Public API "Parameters" (struct as descriptor-driven target) |
| 7 | **Open Design Question 2 (soil by terrain) is resolved:** soil capacity is multiplied by S-1's `terrain.soil_depth_mult[TerrainType]` (provisional; tuned in S-2.6). No terrain column is added to the biome maps | Open Design Question 2 |
| 8 | `BiomeIndex` from the registry replaces the `Biome` enum; arrays are sized by `Count()` | Types and AC 4 mentions of 13 biomes in enum order |
| 9 | Basin and depression searches in `Initialise` (tiny-depression filter, in-basin flood-fill fallback) may use `PriorityFlood` / `ConnectedComponents` | "`HydrologySimulation::Initialise`" helper steps |
| 10 | Replacement ACs for 3-9: the sidecar loads strictly and its defaults equal the GDD values (a test lists them); constraints and `int` kinds reject bad values naming both keys; each biome map is complete or the load fails; a pack with one bad entry among good ones is lenient and atomic as above; `Hash()` changes when any scalar or any map value changes by one ULP | ACs 3-9; Test Plan item 5 ("Overrides and `SetParam`") |

## Status

**Status:** Approved (2026-11-17, user, after the locked-GDD alignment). Interview done with the user the same day:
- the biome table is its own block;
- soil and ground water can be seeded from capacities (default 0, as the GDD).

Aligned to the locked GDD on 2026-11-17: constants, the three-column biome table, protected fixed-site cells, tiny-depression filter, in-basin directions, phase-scaled seed. Approval is a separate step.

Plan: [hydrology-data.plan.md](hydrology-data.plan.md) (written 2026-11-17, not started).

Amended 2026-11-18 (world-sim review decisions).
