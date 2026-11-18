# Feature Spec: A5 Hydrology Data & Parameters

**Parent:** @docs/specs/applications/cow/systems/a5-hydrology/a5-hydrology.md (Feature 1)
**Status:** Draft (2026-11-17)
**Design source:** the A5 brief in `Cluiche/out/CoW/gdd-foldback-hydrology-weather-2026-11-17.md`. GDD `A5_Hydrology/` is not locked yet (the GDD session `cow-68` has it), so this spec cannot be Approved before then (AD-003).
**Depends on:** S-1 consumer amendment 8 (basin table and bed elevation in the world record). Unit tests use hand-built terrain, so the amendment is needed only once Feature 3 reads real worlds.

## Summary

The types, the parameters and `Initialise` for A5, as pure logic, following A4 Feature 1's conventions.

- **Field arrays** (`HydrologyFields`, `BasinState`).
- **Read-only terrain copy** (`HydrologyTerrain`, including the basin table).
- **Parameters:** one descriptor table for the scalars, plus a separate `biome_table` block holding six values for each of the 13 biomes. Both are validated, loaded from a versioned file, merged from sparse overrides atomically, and settable by key.
- **`HydrologySimulation::Initialise`**, which copies the terrain, validates it, seeds the starting state and builds the static tables Feature 2's passes need.

All defaults are **provisional** (user decision 2026-11-17): GDD A5 has no constants yet. S-2.6 tunes them, and the tuned values are folded back to the GDD as A5's constants.

## Goals

1. Every field and parameter Feature 2 names exists, with a type, a range and a default.
2. **One mechanism per kind of value.**
   - **Scalars** go through the descriptor table, which drives load, range checks, defaults, `SetParam` and override merging.
   - **Per-biome values** go through the `biome_table` row descriptors, with the same five operations.
3. **The parameter file** uses a `format` + `version` envelope, rejects unknown keys and merges overrides atomically (A4's rules, and F-3's pack contract).
4. **`Initialise` rejects bad terrain** with a named reason, and otherwise leaves a seeded, ready simulation.
5. **The seed state shortens spin-up** (user decision 2026-11-17).
   - S-1 lakes start full.
   - Soil and ground water start at a fraction of each biome's capacity.

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
        DynamicArrayC<float>    riverStore;          // saved (non-river cells hold 0)
        DynamicArrayC<float>    riverFlow;           // saved: readers between kicks need it (Feature 4)
        DynamicArrayC<float>    meanFlow;            // saved, river cells (Feature 2 flood rule)
        DynamicArrayC<float>    ice;                 // saved
        DynamicArrayC<float>    glacier;             // saved
        DynamicArrayC<float>    seasonMinSnow;       // saved (glacier counters)
        DynamicArrayC<float>    openWater;           // derived, [0,1] (A4 primary evaporation)
        DynamicArrayC<uint16_t> inundatedWeeks;      // saved
        DynamicArrayC<uint16_t> exposedWeeks;        // saved
        DynamicArrayC<uint16_t> snowYears;           // saved
        DynamicArrayC<uint8_t>  glacierEligible;     // saved
        DynamicArrayC<uint16_t> lowFlowWeeks;        // saved, river cells: consecutive low-flow weeks (A3 DryRiverbed)
        DynamicArrayC<uint16_t> glacierWeeks;        // saved: consecutive weeks with glacier mass > 0 (A3 glaciation)
        DynamicArrayC<uint8_t>  flooded;             // saved: readers between kicks need it (Feature 4)
        DynamicArrayC<uint8_t>  passability;         // derived, Passability values
    };

    struct BasinState {                              // per basin
        DynamicArrayC<float> volume;                 // saved
        DynamicArrayC<float> level;                  // derived from volume
    };

    struct BasinRecord {                             // from S-1 amendment 8, read-only
        uint32_t firstCell, cellCount;               // into HydrologyTerrain::basinCells
        float    spillHeight;
        uint32_t spillCell;
        uint8_t  isLake;                             // was a lake at generation
    };

    struct HydrologyTerrain {                        // copied at Initialise, never refreshed except biome
        uint16_t width, height;
        DynamicArrayC<float>    elevation;           // Cell::elevation (lake surface on lakes)
        DynamicArrayC<float>    bedElevation;        // amendment 8 (== elevation off-lake)
        DynamicArrayC<uint8_t>  flowDirection;       // 0..5, kFlowSink = 6, every cell
        DynamicArrayC<uint8_t>  waterSource;         // WorldGeneration::WaterSource
        DynamicArrayC<uint8_t>  biome;               // WorldGeneration::Biome; updated by A3 through Feature 3; saved by Feature 4 (S-1's cells keep the old biome)
        DynamicArrayC<uint8_t>  isRiver;             // waterSource == River
        DynamicArrayC<uint8_t>  isCrossing;          // corridor sites + Pass flag + RiverCrossesBarrier
        DynamicArrayC<uint32_t> basinCells;          // concatenated, per BasinRecord
        DynamicArrayC<BasinRecord> basins;
        bool basinsFromLakesOnly = false;            // version 1 world (amendment 8 AC 37)
    };

    enum class Passability : uint8_t { Dry, Wet, Fordable, Deep, Frozen };
}
```

At Large (6,656 cells) the fields are about 210 KB per copy. Two copies (committed + working) are well under 1 MB.

**`isCrossing`** marks every cell that S-1 treats as a crossing:
- corridor-node sites (all kinds, including river crossings)
- cells with `TerrainFlag::Pass`
- cells with `TerrainFlag::RiverCrossesBarrier`

Feature 2 never makes these `Deep`.

### Scalar descriptor table (`HydrologyParameters.h`)

`ParamDescriptor { group, key, member, min, max, default, kind }`, the same shape as A4's. **23 rows**, every default provisional (three added 2026-11-17 after the Feature 3–4 specs):

| Key | Default | Range | Used by (Feature 2 pass) |
|---|---|---|---|
| `freeze.threshold` | 0.25 | 0–1 | 1 (temperature below which water freezes) |
| `freeze.rate` | 0.5 | 0–1 | 1 |
| `freeze.thaw_rate` | 0.4 | 0–1 | 1 |
| `freeze.ice_crossable` | 0.3 | 0–10 | 9 |
| `glacier.melt_threshold` | 0.55 | 0–1 | 2 |
| `glacier.melt_rate` | 0.05 | 0–1 | 2 |
| `glacier.snow_floor` | 0.2 | 0–1 | 10 |
| `glacier.onset_years` | 3 | 1–50 | 10 (stored as float, read as whole years) |
| `routing.r_min` | 0.3 | 0–1 | 6 |
| `routing.r_slope` | 4.0 | 0–100 | 6 |
| `routing.r_max` | 0.85 | 0–1 | 6 (kept below 1: Feature 2 Open Question 3) |
| `routing.baseflow_rate` | 0.05 | 0–1 | 5 |
| `basin.evap_rate` | 0.08 | 0–1 | 6 |
| `basin.ford_depth` | 0.02 | 0–1 | 9 (in elevation units) |
| `flood.mean_weeks` | 104 | 4–1040 | 8 |
| `flood.bankfull_factor` | 2.5 | 1–20 | 8 |
| `flood.min_flow` | 0.01 | 0–10 | 8 |
| `flood.floodplain_rise` | 0.01 | 0–1 | 8 (in elevation units) |
| `init.soil_fraction` | 0.5 | 0–1 | Initialise |
| `init.ground_fraction` | 0.3 | 0–1 | Initialise |
| `wetness.surface_scale` | 0.1 | 0–10 | Feature 3 `SurfaceWetness` (GDD value) |
| `wetness.soil_weight` | 0.25 | 0–1 | Feature 3 `SurfaceWetness` (GDD SOIL_WETNESS_WEIGHT) |
| `river.low_flow_fraction` | 0.2 | 0–1 | Feature 2 pass 8 (`lowFlowWeeks`: flow below this share of `meanFlow`) |

The cross-row rule is `r_min ≤ r_max`. A load or override that breaks it fails with both keys named.

### Biome table (`biome_table` block)

Six values per biome, keyed by the biome's id string (`BiomeId`). The row descriptors are `{ column, min, max }`, and every column is validated for every biome.

| Column | Range | Meaning |
|---|---|---|
| `soil_capacity` | 0–5 | Most water the soil holds (cell-depth) |
| `ground_capacity` | 0–20 | Most water the ground store holds |
| `infil_rate` | 0–1 | Share of available water entering soil per week |
| `percolation_rate` | 0–1 | Share of soil water moving to ground per week |
| `et_rate` | 0–1 | Evapotranspiration at temperature 1.0, ×`etMultiplier` |
| `retention` | 0–5 | Surface water a cell holds before it runs off |

Provisional defaults:

| Biome | soil | ground | infil | perc | et | retention |
|---|---|---|---|---|---|---|
| Ocean | 0 | 0 | 0 | 0 | 0 | 0 |
| Lake | 0 | 0 | 0 | 0 | 0 | 0 |
| GrasslandSteppe | 0.6 | 2.0 | 0.5 | 0.10 | 0.35 | 0.05 |
| TemperateForest | 1.0 | 3.0 | 0.7 | 0.12 | 0.40 | 0.10 |
| WetlandDelta | 1.2 | 4.0 | 0.4 | 0.05 | 0.45 | 0.40 |
| DesertBasin | 0.2 | 1.0 | 0.3 | 0.15 | 0.60 | 0.02 |
| AlpineMountain | 0.2 | 0.5 | 0.2 | 0.20 | 0.15 | 0.02 |
| TundraBoreal | 0.5 | 1.0 | 0.3 | 0.05 | 0.20 | 0.10 |
| Rainforest | 1.2 | 3.0 | 0.8 | 0.12 | 0.55 | 0.10 |
| VolcanicAshland | 0.4 | 2.5 | 0.6 | 0.25 | 0.30 | 0.02 |
| Scrub | 0.4 | 1.5 | 0.4 | 0.10 | 0.40 | 0.03 |
| BurnedAshland | 0.3 | 2.0 | 0.3 | 0.10 | 0.30 | 0.02 |
| DesertifiedPlain | 0.15 | 0.8 | 0.2 | 0.10 | 0.55 | 0.01 |

Ocean and Lake rows are all zero because their cells are not land stores. Basin and river water use their own stores.

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
```

**Validation** (fails with `err` naming the first problem, and leaves the simulation un-initialised):
- array sizes equal `width × height`
- flow directions in 0..6
- every basin cell index in range and in exactly one basin
- spill cell in range and not inside its own basin
- bed elevation ≤ elevation on every cell
- with `basinsFromLakesOnly`, every basin has `isLake`

**Seeding:**
- Fields are allocated and zeroed.
- On land, `soilMoisture = init.soil_fraction · soil_capacity[biome]` and `groundWater = init.ground_fraction · ground_capacity[biome]`.
- Each lake basin's `volume` is its spill volume (full); non-lake basins start at 0.
- `meanFlow` starts at 0, and Feature 2's flood rule needs `meanFlow > min_flow`, so nothing floods in the first weeks.
- `level`, `openWater` and `passability` are derived once, so a reader sees a consistent state before the first `Step`.

**Static tables** (contents defined by Feature 2):
- downstream index
- basin contraction
- topological trees
- per-cell release rates
- basin capacity curves

They are built here so `Step` allocates nothing.

**Re-initialise.** Calling `Initialise` again with a new terrain discards everything. It is how Feature 3 handles a new world.

## Public API

Files under `Cluiche/CoW/Shared/Hydrology/`:

| File | Contents |
|---|---|
| `HydrologyTypes.h/.cpp` | Fields, basin state, terrain, `Passability` |
| `HydrologyParameters.h/.cpp` | Struct, descriptor table, biome table, load, overrides, `SetParam`, `ToJson`, defaults |
| `HydrologySimulation.h/.cpp` | Class, `Initialise`, accessors `Fields()`, `Basins()`, `Terrain()`, `IsInitialised()` |

## Acceptance Criteria

### Types
1. Each field has N entries after `Initialise`, and each basin array has one entry per basin.
2. The "saved" set listed above is exposed as a constant list Feature 4 can iterate. Derived fields are not in it.

### Parameters
3. The descriptor table has 23 rows with unique dotted keys. Every default lies inside its own range.
4. The biome table has 13 rows (one per `Biome`, enum order) × 6 columns. Every default lies inside its column range. The Ocean and Lake rows are zero.
5. **Loading the shipped defaults:**
   - loading the shipped `hydrology_parameters.json` equals `DefaultHydrologyParameters()`;
   - `ToJson(defaults)` equals the shipped file byte for byte.
6. **Load rejects bad files:**
   - Load fails with the offender named for: an unknown group, key, biome id or column; a wrong `format`; or a `version` other than 1.
   - Load also fails, with the key named, for an out-of-range value or `r_min > r_max`.
7. A file missing keys or biome rows loads, and the missing ones keep their defaults.
8. `ApplyOverrides` with a sparse object (one scalar plus one biome cell) changes only those two values. With one bad entry among good ones, it changes **none** and `inOut` is byte-identical.
9. `SetParam` accepts dotted scalar keys and `biome_table.<BiomeId>.<column>`. It rejects unknown keys and out-of-range values, and leaves the set unchanged.

### Initialise
10. Hand-built terrain with two lakes, one dry basin and a river initialises:
    - lake volumes equal their spill volume;
    - the dry basin's volume is 0;
    - land soil and ground water equal their fractions × capacity;
    - every derived field is consistent (levels match volumes, `openWater` = 1 exactly on inundated cells).
11. Each validation failure listed above is rejected with a message naming it, and the simulation stays un-initialised.
12. With `basinsFromLakesOnly`, a terrain whose basins are all lakes initialises, and one with a non-lake basin is rejected.
13. Re-initialising with a different terrain size gives correctly sized fields and no trace of the old world.
14. `isCrossing` cells are exactly the union of corridor sites, `Pass` cells and `RiverCrossesBarrier` cells on a hand-built terrain.
15. `Initialise` is deterministic: two calls on the same inputs give byte-identical fields and tables.

## Files Touched

| File | Change |
|---|---|
| The three file pairs above | New |
| `Cluiche/Assets/CoW/Global/Hydrology/hydrology_parameters.json` | New, the provisional defaults |
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
| AD-003 | GDD owns design intent | Draft until GDD A5 is locked. Every default is provisional and listed for fold-back |

## Test Plan

All GoogleTests, on hand-built terrain only; S-1 isn't needed. Written test-first (RED, then GREEN), in this order:
1. Types
2. Descriptor table and defaults
3. Biome table
4. Load and validation
5. Overrides and `SetParam`
6. `ToJson` round trip
7. Initialise seeding
8. Initialise validation

Scoped filter `*Hydrology*`.

## Open Design Questions

1. **Biome capacities need re-reading once Feature 2 runs.** The provisional table guesses relative values: forest holds more than steppe, and desert loses most to evapotranspiration. Only S-2.6 shows whether rivers stay alive through a dry summer with these numbers. Expect the first tuning pass to change most of the table.
2. **Are some values per region, not per biome?** Soil depth arguably follows terrain (thin on hills and mountains, deep in valleys) more than biome. If S-2.6 shows biome-only capacities look wrong on steep ground, add a terrain multiplier column keyed by `TerrainType` rather than growing the biome table.

## Status

**Status:** Draft (2026-11-17). Interview done with the user the same day:
- the biome table is its own block;
- defaults are provisional and tuned in S-2.6;
- soil and ground water are seeded from biome capacities.

Approval waits on GDD A5's lock.
