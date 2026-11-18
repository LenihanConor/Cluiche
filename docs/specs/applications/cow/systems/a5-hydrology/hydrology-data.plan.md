# Plan: A5 Hydrology Data & Parameters (A5 Hydrology Feature 1)

**Spec:** @docs/specs/applications/cow/systems/a5-hydrology/hydrology-data.md
**Status:** Not Started
**System plan:** @docs/specs/applications/cow/systems/a5-hydrology/a5-hydrology.plan.md (row 1)

Can start now: it is pure logic on hand-built hex terrain, with no module, bus or thread. It needs the S-1
13-biome enum and `WorldGeneration::BiomeFromId`, `HexTopology`, and nothing from Dia beyond `DynamicArrayC`
and `Json::Value`. S-1 consumer amendment 8 (basin table, bed elevation) is **not** needed: every test here
uses hand-built terrain. **No plan depends on this one being started; Feature 2 needs it partly Done.**

`water-pipeline.plan.md` (Feature 2) starts after this plan's tasks 1 to 17 are Done: types, parameters
(Tasks 2 to 8), the test grids (Task 10) and `Initialise` (Tasks 11 to 16). `ApplyPhaseSeed` (Task 17) and
the shipped JSON can finish in parallel with Feature 2. Features 3 to 6 consume the types, `ParamDescriptor`
pairs, `ApplyOverrides`, `SetParam`, `ToJson`, `RederiveCommitted()` and the saved-field list.

All code is CoW (application) under `Cluiche/CoW/Shared/Hydrology/`. No Dia edits (AD-001). Where a task
could need more than sonnet it says "ask before escalating" (feedback_confirm_opus).

## Decision: who builds the static tables (binding on both plans)

The spec says Feature 1 builds tables whose contents Feature 2 defines. Feature 1's own ACs 16 to 18 need
the downstream index, the in-basin directions and the topological order (the cycle check is EC-14), so those
cannot wait. Split:

| Piece | Built by | Where |
|---|---|---|
| Working basin set (tiny-depression filter), basin capacity `V(spillHeight)` | **Feature 1** | `Initialise`, `BasinCurve` (see below) |
| In-basin flow directions (steepest descent on bed + flood-fill fallback) | **Feature 1** | `Initialise` |
| Downstream index (with the in-basin override) | **Feature 1** | `Initialise` |
| Topological order over cells and basin nodes (Kahn, ties by ascending index) and the EC-14 failures | **Feature 1** | `Initialise` |
| Per-cell release rate `k`, trees grouped by terminal, largest-tree size | **Feature 2** | the empty `BuildRoutingTables()` seam |
| Cascade, passes, totals | **Feature 2** | `Step` |

Feature 1 leaves a private, **empty** `BuildRoutingTables()` called last in `Initialise` (the A4 `BuildGridTables`
pattern). Feature 2 fills it. The same boundary is written in Feature 2's plan.

`BasinCurve.h/.cpp` (capacity curve, level to volume and back) is listed under Feature 2's Files Touched,
but `Initialise` needs `capacity` (for the filter and for full lakes) and `level` (derived once). **This plan
creates `BasinCurve`**; Feature 2 consumes it and adds its own edge tests. See Open item 1.

## Implementation Patterns

### Types (Task 2)
- `HydrologyTypes.h/.cpp`, namespace `CoW::Hydrology`. `HydrologyFields`, `BasinState`, `BasinRecord`,
  `HydrologyTerrain`, `enum class Passability : uint8_t { Dry, Wet, Fordable, Deep, Frozen }`,
  `enum class RiverState : uint8_t { Dry, Low, Normal, Flood, Frozen }`, exactly as in the spec.
- `void HydrologyFields::Resize(uint32_t n)` zero-fills and fully replaces every array (assign, do not just
  resize, so a smaller second call leaves no stale values). `void BasinState::Resize(uint32_t basinCount)` likewise.
- `bool HydrologyTerrain::IsConsistent() const`: false if any per-cell array is not `width * height` long,
  if `width * height == 0`, or if a `BasinRecord` range runs past `basinCells`.
- **Saved-field list** (AC 2): a `static constexpr` table of `{ const char* name; offset-to-member }` entries
  for the per-cell arrays the struct marks "saved", plus `kSavedBasinFields` for `volume`, `iceFraction`. It is
  a constant list Feature 4 iterates; derived fields (`openWater`, `passability`, `level`) are not in it.
  Member pointers are used with a small type-erased accessor, so the table holds mixed element types
  (`float`, `uint16_t`, `uint8_t`) without `std::variant`. See Open item 2 (the count).
- No `std::` type in a public signature (PD-004).

### Parameters (Tasks 3 to 8)
- `HydrologyParameters.h/.cpp`. `HydrologyParameters` is a POD of `float` members for the 43 scalars plus
  `BiomeRow biome[13]` (`float soilCapacity, infilRate, vegFactor`).
- `enum class ParamKind { Float, Int }`. `ParamDescriptor { const char* group; const char* key; ParamKind kind;
  float HydrologyParameters::* member; float minValue, maxValue, defaultValue; }`. `Int` rows (`basin.min_cells`,
  `glacier.onset_years`, `glacier.check_week`) must be whole numbers. One `static const` table.
  `GetParamDescriptors(uint32_t& count)` returns `(const ParamDescriptor*, count)` (A4's choice, PD-004).
- `BiomeColumnDescriptor { const char* column; float BiomeRow::* member; float minValue, maxValue; }` with 3
  rows. `GetBiomeColumnDescriptors()` the same pointer + count shape. Default rows are a compiled
  `kDefaultBiomeRows[13]` in enum order.
- `FindParam(dottedKey)` plain compare over 43 rows. Biome keys are `biome_table.<BiomeId>.<column>`, resolved
  with `WorldGeneration::BiomeFromId` (`BiomeId` strings, not enum names).
- `ParamError { String64 key; String256 reason; }`, same as A4.
- **Cross-row rules** run on the **result** of a load or override, after the per-key checks, each naming both keys:
  `release_floor <= release_ceil`, `freeze.threshold <= freeze.thaw_temp`. Whole-number rows are checked per
  key (type step).
- Validation order in one pass: envelope (`format` `cow.hydrology_params`, `version` 1), unknown group, unknown
  key, type (number), finite, range, whole-number rows, `biome_table` (unknown biome id, unknown column, range),
  then the cross-row rules. First fault names its dotted key. On any fault `out = DefaultHydrologyParameters()`.
- `ApplyOverrides(json, inOut, err)` is the primitive: build into a local copy, validate, copy back only on
  success (atomic; `inOut` byte-identical on failure). `LoadHydrologyParameters` is the envelope check plus
  `ApplyOverrides` over defaults. `SetParam(key, value)` is a one-key `ApplyOverrides` over the same code path.
- `ToJson(const HydrologyParameters&)` writes the envelope, groups in descriptor order, the `biome_table` in
  enum order, with fixed float formatting (shortest round-trip, same helper as A4's `ToJson` if it ships one).
  Writing a loaded file gives the same bytes.
- Defaults are the GDD first-run values in the spec table. The test copies the 43 defaults and the 13 x 3
  biome values into the test file as literals (not read from the table) so a drift fails.
- Shipped file: `Cluiche/Assets/CoW/Global/Hydrology/hydrology_parameters.json` (F-3 layout). CoW `pipeline.toml`
  gets a `Global/Hydrology/` deploy entry (Task 8).

### Test grids (Task 10)
- `Cluiche/CoW/Shared/Hydrology/Testing/HydrologyTestGrids.h` (shipped with the library, shared with Feature 2).
  Pure builders returning `HydrologyTerrain`: `MakeTwoLakesDryBasinRiver()` (AC 10), `MakeTinyDepressions()`
  (2-cell, 5-cell with capacity 0.5, 4-cell with capacity 1.5, tiny S-1 lake; AC 16), `MakeDryBasinCrossedBySpillDirections()`
  (AC 17), `MakeLakesOnlyWorld()` and a variant with a non-lake basin (AC 12), `MakeFlowCycle()`,
  `MakeSpillCellAboveSpillHeight()`, `MakeRiverLeavingFootprint()` (AC 18).
- `MarkCrossings(HydrologyTerrain&, corridorSiteCells, passCells, riverCrossesBarrierCells, fixedSiteCells)`:
  the helper AC 14 tests. It is test-side; the production builder is Feature 3's `BuildHydrologyTerrain`
  (covered by presets-and-regression AC 30).
- Terrain is built on the real hex layout (`HexTopology::NeighbourAt`) so odd and even columns are exercised.
  Grids stay small (under 12 x 8) unless a test needs more.

### Initialise (Tasks 11 to 16)
- `HydrologySimulation.h/.cpp`: class shell, owned `HydrologyTerrain` copy, working basin set, committed
  `HydrologyFields` + `BasinState`, `bool mInitialised`. `bool Initialise(const HydrologyTerrain&, const
  HydrologyParameters&, String256& err)`. Everything is built into a local temporary and swapped in on
  success, so a failed call leaves the simulation un-initialised (AC 11, 13).
- Accessors: `Fields()`, `Basins()`, `Terrain()`, `IsInitialised()`, `WorkingBasins()` (the working basin set
  after the filter), `DroppedBasinCount()` (debug state query).
- **`BasinCurve.h/.cpp`:** `BasinCurve::Build(terrain, basin, depthScale)` sorts the basin's cells by bed
  elevation and keeps a cumulative volume array. `float Capacity()` = `V(spillHeight)` = `sum over cells with
  bed below spillHeight of (spillHeight - bed) * depthScale`. `float LevelForVolume(float)` is a binary search
  plus linear interpolation; `float VolumeForLevel(float)` is its inverse. All arrays sized at Build; no
  allocation afterwards.
- **Derivation** (`RederiveCommitted()`, public): `level` from `volume`, `openWater` (Ocean 1; inundated cell
  `1 - iceFraction`; river `wetness.river_open_water * min(1, riverFlow / max(meanFlow, flood.min_flow))`; else 0),
  `passability` by the first-match rule (Frozen, Deep unless `isCrossing`, Fordable, Wet, Dry; lake-only worlds
  as the spec). Written as free functions in `HydrologyDerive.h/.cpp` so Feature 2 passes 7 and 9 and Feature 4's
  load call the same code. `Initialise` ends with one call.
- **Validation order:** sizes, flow directions 0..6, basin cell indices (in range and in exactly one basin),
  spill cell (in range, not in its own basin), `bedElevation <= elevation`, lake-only has only lake basins,
  then (after the filter and direction override) the topological sort. First problem is named in `err`.
- **Tiny-depression filter:** capacity `V(spillHeight)`; drop a basin when `cellCount < basin.min_cells` or
  `capacity < basin.min_capacity`, unless `isLake`. Dropped basins' cells are removed from the working set
  (they keep S-1's directions). Count logged once at Info (`DIA_LOG_INFO`).
- **In-basin directions:** per real basin, flood-fill from the pit (lowest bed cell, ties by lowest index)
  over footprint cells to get a hop distance `dist[c]`. For each footprint cell: pick the neighbour **inside
  the footprint** with the lowest bed elevation strictly below this cell's; if none, pick the footprint
  neighbour with the smallest `dist` (ties by lowest direction index). The pit itself keeps a direction toward
  the basin node (mapped to "pit"). Cells outside every real basin are untouched. `basinsFromLakesOnly`
  worlds skip the whole step. This fixes the tie rule (Open Design Question 5): steepest bed descent,
  flood-fill distance as the fallback, lowest direction index last.
- **Downstream index:** `DynamicArrayC<uint32_t> downstream` of N, `kNoCell` for terminals (ocean, `kFlowSink`,
  basin pit gives the basin node). Basin nodes are numbered `N + basinIndex`.
- **Topological order:** Kahn's algorithm over cells and basin nodes, ready set a min-heap by index so ties go
  to the ascending index. The edges: cell to downstream, every footprint cell and every cell routing into the
  basin to the basin node, basin node to its spill cell. If fewer than all nodes come out, the remainder is a
  cycle: `err` names the first cell index in it. Separate checks name "spill cell above spill height" and
  "river leaves footprint" first. Output `DynamicArrayC<uint32_t> topoOrder` (positions), plus `topoRank` for the tests.
- **Seeding:** allocate and zero; on land `soilMoisture = init.soil_fraction * soil_capacity[biome]`,
  `groundWater = init.ground_fraction * soil.gw_capacity`; lake basin `volume = capacity`, others 0;
  `iceFraction` 0. Unknown biome id uses the `GrasslandSteppe` row and logs once per id (GDD EC-09). Inundated
  cells (bed below the lake level) hold no land stores, so a lake cell's soil is 0.
- `BuildRoutingTables()`: private, **empty** seam, called last. Feature 2 fills it.
- **Determinism:** no hash-order iteration, no pointer-keyed containers; Task 16 compares with `memcmp`.

### ApplyPhaseSeed (Task 17)
- `void ApplyPhaseSeed(float coldness, const uint8_t* snowSurvivesSummer)`; `coldness` clamped to [0, 1].
  Lake basin `volume = capacity * lerp(1.0f, spinup.lake_seed_cold, coldness)`. Masked cells:
  `glacier = spinup.glacier_seed_max * coldness`, `snowYears = onset_years` (rounded to whole), `glacierEligible = 1`.
  Null mask seeds none. Ends with `RederiveCommitted()`. At `coldness == 0` the state is bit-identical to a
  fresh `Initialise` (tested with `memcmp`). No-op before `Initialise` (asserts in Debug).

### Open Design Question 3 (`glacier.check_week` mapping)
- Task 0 reads `kWeeksPerSeason` and `WeekOfYear` and records whether the GDD's "26" is index 25 (0..51 weeks,
  end of Summer). Task 3 adds the decision as a comment on the descriptor row and as a test constant. The
  descriptor range stays 1..52. Feature 2's glacier-counter task compares `week + 1 == glacier.check_week`
  (or whatever Task 0 finds) and cites this note.

### Testing
- `Tests/GoogleTests/CoW/Hydrology/TestHydrologyTypes.cpp`, `TestHydrologyParameters.cpp`,
  `TestHydrologyInitialise.cpp`. Suite prefixes `HydrologyTypes*`, `HydrologyParameters*`, `HydrologyInitialise*`
  (all match `*Hydrology*`). Phase-seed tests live in `TestHydrologyInitialise.cpp` under `HydrologyInitialisePhaseSeed*`.
- Byte-identity checks use `memcmp` over the arrays, not `==` on floats.
- Table-driven: one loop over `GetParamDescriptors()` makes the below-min and above-max cases for every row, and a
  second loop does the same for the biome columns.

### Verification cadence
- Per task: `dia run googletest --filter="<suite prefixes for the task>" --shards 2` (report pass/fail only).
- Scoped verification task near the end: `*Hydrology*`. **No full-suite run in this feature** (the one full
  run is the A5 system close-out). No mutation checks. No sanitizer.
- One commit per task (code, tests and the plan row in the same commit).

## Tasks

`dia docs vcxproj-add` must be run from PowerShell, or with literal paths: a bash loop over it has a known bug.
Update rows with `dia docs plan docs/specs/applications/cow/systems/a5-hydrology/hydrology-data.plan.md <#> --status <S> --notes "..."`.

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 0 | Prerequisite check, read only: S-1 13-biome enum, `WorldGeneration::BiomeFromId` and the 13 `BiomeId` strings; `WaterSource` enum; `TerrainFlag::Pass` / `RiverCrossesBarrier`; `HexTopology::NeighbourAt` and `kNoCell`; `kFlowSink` value (6); `String64` / `String256` names; `Json::Value` use from `Cluiche/CoW`; `CellLayerGolden.h` location (Feature 2 uses it); `/fp:` settings of CoW in Debug and Release; `WeekOfYear` and `kWeeksPerSeason` for `glacier.check_week` (closes Open Design Question 3); whether A4's `ParamError`, `ToJson` float formatter and `ParamDescriptor` shapes ship and can be mirrored; DiaDeferredStep Feature 1 Task 1 headers (`IParallelExecutor`, `SerialExecutor`, commit c83a94ed) exist (Feature 2 needs them, not this plan); `hydrology-save.md` saved-field count | Findings recorded in notes | Not Started | haiku | TDD-exempt investigation. If a Dia header or a String type is missing, stop and flag it (AD-001). Record the check-week mapping and the saved-field count here |
| 1 | Create empty file pairs `HydrologyTypes`, `HydrologyParameters`, `HydrologySimulation`, `HydrologyDerive`, `BasinCurve` under `Cluiche/CoW/Shared/Hydrology/`, `Testing/HydrologyTestGrids.h`, the three test files under `Tests/GoogleTests/CoW/Hydrology/`, the `Assets/CoW/Global/Hydrology/` folder, and the CoW `pipeline.toml` deploy line; register each with `dia docs vcxproj-add CoW ...` and `dia docs vcxproj-add GoogleTests ...` | Solution builds with the empty files | Not Started | haiku | Mechanical, TDD-exempt. Run from PowerShell or with literal paths. Idempotent; later tasks edit in place. Commit |
| 2 | RED then GREEN: `HydrologyFields::Resize`, `BasinState::Resize`, `HydrologyTerrain::IsConsistent`, enums, saved-field list constants | AC 1 (array sizes), 2: every field N entries and zero-filled; each basin array one per basin; a smaller second `Resize` leaves no stale values; `IsConsistent` false for each wrong-length array in turn, for `width * height == 0` and for a basin range past `basinCells`; the saved list holds exactly the members marked saved and no derived field | Not Started | sonnet | Prove RED with quoted output. The saved list is checked against the spec's list written out as literals in the test. Commit |
| 3 | RED then GREEN: `ParamKind`, `HydrologyParameters` struct, 43-row scalar descriptor table, `GetParamDescriptors`, `FindParam`, `DefaultHydrologyParameters` | AC 3: 43 rows, unique dotted keys, every default inside its own range, defaults equal the GDD values (all 43 copied into the test as literals), `Int` rows whole, cross-row defaults valid (`release_floor 0.3 <= release_ceil 0.85`, `threshold 0.25 <= thaw_temp 0.32`) | Not Started | sonnet | Do not invent ranges: take them from the spec table. Record the `glacier.check_week` mapping from Task 0 as a comment on the row. Commit |
| 4 | RED then GREEN: biome column descriptors, 13 x 3 default rows, `BiomeFromId` lookup for `biome_table.<BiomeId>.<column>` | AC 4: 13 rows in enum order, 3 columns, defaults match the spec table (literals in the test), defaults inside the column ranges, Ocean and Lake rows all zero, an unknown biome id or column rejected by the lookup | Not Started | sonnet | Mirrors Task 3 for biome rows. Unknown biome id **at runtime** (EC-09 fallback) is Task 12, not here. Commit |
| 5 | RED then GREEN: `LoadHydrologyParameters` valid paths and the envelope | AC 5 (load half), 7: full file; empty `hydrology_parameters`; missing group; missing key keeps default; missing biome row keeps its default row; a partial biome row keeps the other columns; min and max boundaries valid | Not Started | sonnet | Valid path only, so Task 6 drives the failures RED. The shipped-file comparison needs the JSON, so it lands in Task 8 |
| 6 | RED then GREEN: `LoadHydrologyParameters` failure paths, table-driven | AC 6: below and above range generated from the table for every scalar row and every biome column; `Int` rows with a fraction; unknown group, key, biome id and column; wrong `format`; `version` other than 1; string where a number is expected; NaN and Inf (built programmatically); `release_floor > release_ceil` and `freeze.threshold > freeze.thaw_temp` each naming both keys. Each returns false, sets `out` to defaults and names the first offending dotted key | Not Started | sonnet | One loop per table, no per-constant tests. Commit tasks 3 to 6 as one logical group |
| 7 | RED then GREEN: `ApplyOverrides` and `SetParam` (scalars and `biome_table.<BiomeId>.<column>`) | AC 8, 9: a sparse object (one scalar plus one biome cell) changes only those two values (`memcmp` of the rest); one bad entry among good ones changes **none** and `inOut` is byte-identical; `SetParam` accepts dotted scalar and biome keys, rejects unknown keys and out-of-range values and leaves the set unchanged; a cross-row break through an override (`release_floor` above `release_ceil`) is rejected and changes nothing | Not Started | sonnet | Atomicity is the point: apply to a copy, then assign. `SetParam` shares the code path, not a copy of it. Commit |
| 8 | RED then GREEN: `ToJson` plus author `Assets/CoW/Global/Hydrology/hydrology_parameters.json` (envelope, 43 values, `biome_table` 13 x 3) and the CoW `pipeline.toml` deploy entry | AC 5: loading the shipped file equals `DefaultHydrologyParameters()`; `ToJson(defaults)` equals the shipped file byte for byte; `ToJson(Load(x))` equals `x` for an overrides-merged set; descriptor order and biome enum order | Not Started | sonnet | The file is **generated from `ToJson`** once, then committed, so the byte test cannot drift. Register via the asset path, not by hand. Commit |
| 9 | RED then GREEN: `HydrologySimulation` shell and `Initialise` structural validation (sizes, flow directions 0..6, basin cell indices in range and in exactly one basin, spill cell in range and not in its own basin, `bedElevation <= elevation`, lake-only has only lake basins) | AC 11 (structural half): each failure is rejected with a message naming it (`err` contains the cause), `IsInitialised()` false, no throw, no assert in a test build; a failed `Initialise` after a successful one leaves the earlier world intact | Not Started | sonnet | Build into a temporary and swap on success. Needs `HydrologyTestGrids.h` bad-terrain variants; write the minimal ones here and extend in Task 10. Commit |
| 10 | RED then GREEN: `HydrologyTestGrids.h` builders and `MarkCrossings` | AC 14: `isCrossing` is exactly the union of corridor-site cells, `Pass` cells, `RiverCrossesBarrier` cells and Hearth, Nexus and world-throne cells on a hand-built terrain, and no other cell; each builder returns a terrain that `IsConsistent()` | Not Started | sonnet | Builders are test helpers shipped under `Testing/`, not in GoogleTests. Each builder has a short comment naming the AC it serves. Commit |
| 11 | RED then GREEN: `BasinCurve` and `RederiveCommitted()` (`HydrologyDerive`) | AC 10 (derivation half), 1 (basin arrays): capacity equals a hand sum of `(spillHeight - bed) * depth_scale`; `LevelForVolume(VolumeForLevel(x))` round trips; level 0 volume gives the lowest bed; volume at capacity gives `spillHeight`; levels match volumes; `openWater` is 1 exactly on inundated cells and Ocean, `1 - iceFraction` on ice, the river term is 0 at `riverFlow` 0; `passability` first-match order on a hand terrain (Deep beyond `ford_depth`, `isCrossing` caps at Fordable) | Not Started | sonnet | Pure functions, no allocation after `Build`. Feature 2 reuses these for passes 7 and 9 and Feature 4 for load. If `Initialise` seeding is needed to test it, use directly-built `HydrologyFields`. Commit |
| 12 | RED then GREEN: `Initialise` seeding | AC 1, 10, 13 (size half): hand-built terrain with two lakes, one dry basin and a river initialises; lake `volume` equals capacity; dry basin 0; every `iceFraction` 0; land soil `init.soil_fraction * soil_capacity[biome]` and ground `init.ground_fraction * soil.gw_capacity` at the 0.5 / 0.3 defaults **and** with both set to 0 (the GDD start); lake cells hold no land stores; `meanFlow` 0; derived fields consistent; unknown biome id uses the `GrasslandSteppe` row and logs once per id (EC-09) | Not Started | sonnet | Known answers computed by hand in the test, not by calling the production helper. Empty `BuildRoutingTables()` seam called last. Commit |
| 13 | RED then GREEN: tiny-depression filter | AC 16: a 2-cell depression and a 5-cell depression of capacity 0.5 are not in `WorkingBasins()`, keep S-1's directions and are never inundated; a 4-cell depression of capacity 1.5 is kept; a tiny S-1 lake is kept; changing `basin.min_cells` or `basin.min_capacity` changes the set; `DroppedBasinCount()` matches; `BasinState` arrays are sized to the surviving basins | Not Started | sonnet | One Info log with the count (`DIA_LOG_INFO`), once per `Initialise`. Commit tasks 11 to 13 |
| 14 | RED then GREEN: in-basin flow directions | AC 17: on a dry basin whose S-1 directions cross it toward the spill, every footprint cell's downstream path reaches the pit; each step goes to a lower bed elevation or follows the flood-fill fallback; cells outside the footprint are unchanged; a rough bed with a local pit exercises the fallback; ties resolve by lowest direction index; a `basinsFromLakesOnly` terrain has no override | Not Started | sonnet | Fixes the tie rule (Open Design Question 5) as written in Implementation Patterns; record any change in notes and in the spec's Open Design Questions via the user. If the rule proves insufficient, ask before escalating |
| 15 | RED then GREEN: downstream index, topological order and EC-14 failures | AC 18, 11 (EC-14 half): every cell comes after all its upstream cells; a basin node comes after every footprint cell and every cell upstream of the basin and before its spill cell; ties by ascending index (two equal orders on two inputs); a flow cycle, a spill cell above its spill height, and a river leaving a footprint each fail with the cause named in `err` and leave the simulation un-initialised | Not Started | sonnet | Kahn with a min-heap ready set. The cycle message names the first cell index in the cycle. Do not give the cascade (Feature 2) any behaviour here. Commit |
| 16 | RED then GREEN: lake-only worlds, re-initialise and determinism | AC 12, 13, 15: a `basinsFromLakesOnly` terrain of lakes initialises, one with a non-lake basin is rejected, no basin is dropped by the filter and lake cells keep their S-1 directions; re-`Initialise` with a different terrain size equals a fresh simulation on that terrain (`memcmp` of every field, table and terrain copy); the same inputs twice give byte-identical fields and tables; the caller's terrain is not retained (mutate after the call, state unchanged) | Not Started | sonnet | Commit tasks 14 to 16 as one logical group |
| 17 | RED then GREEN: `ApplyPhaseSeed` | AC 19: `coldness` 0 changes nothing (`memcmp` against a fresh `Initialise`); `coldness` 1 gives lake volume `capacity * lake_seed_cold` and, on masked cells only, `glacier = glacier_seed_max`, `snowYears = onset_years`, `glacierEligible = 1`; 0.5 gives the midpoint; a null mask seeds no glacier; `level`, `openWater` and `passability` are re-derived; `coldness` outside [0, 1] is clamped; non-lake basins unchanged | Not Started | sonnet | Soil, ground water, channel stores and ice are untouched (test asserts it). Commit |
| 18 | Exhaustive test pass: gap audit of AC 1 to 19 against the written tests, then edge cases the spec implies but does not number | Gap list in notes. Edge cases: 1 x 1 terrain; a terrain with every cell a river; zero basins; a basin of one cell; a basin spanning the map edge; overrides naming a biome and a scalar of the same name; a `biome_table` that is not an object; a `Json::Value` integer where a float is expected; `Initialise` after a failed `Initialise`; `basin.min_cells` 1 and `min_capacity` 0 (nothing dropped); `ApplyPhaseSeed` before `Initialise`; the largest `Initialise` at Large size (6,656 cells) for a timing note (report only) | Not Started | sonnet | No mutation check (project rule 2026-11-12). Add only the tests the audit shows missing. The Large timing is a note for A5 Open Question 1, not a gate |
| 19 | Observation Opportunity Scan over every file touched, across logs, traces, profiling, metrics, health | Suggestions in notes | Not Started | haiku | Non-blocking. The only expected code instrumentation is the tiny-depression Info log and the EC-09 once-per-id log; note a `cow.hydro.basins_dropped` metric and an `Initialise` trace zone as Feature 3 suggestions |
| 20 | Scoped verification: `dia run googletest --filter="*Hydrology*" --shards 2`; `dia check deps`; `dia docs precommit` | All AC 1 to 19 quoted; pass/fail and failing names only | Not Started | haiku | **No full-suite run here**; the single full `dia run googletest --shards 2` is the A5 system close-out |
| 21 | Close-out: `dia docs plan` rows to Done with notes, then `dia docs spec-done docs/specs/applications/cow/systems/a5-hydrology/hydrology-data.md` | Spec status Done, plan header updated | Not Started | haiku | **Do not run until every row above is Done.** Not to be run when this plan is written |

## Open items

1. **`BasinCurve` moves into this feature.** Feature 2's Files Touched lists `BasinCurve.h/.cpp` as new, but `Initialise` cannot derive `level`, `capacity` or full lakes without it. This plan builds it (Task 11) and Feature 2 consumes it. Same for `HydrologyDerive.h/.cpp` (not named in either spec; needed so Feature 4's "same code as Initialise's final derivation" is one function). Both are CoW-internal; confirm with the user and add them to Feature 1's Public API table when this plan is approved.
2. **Count of saved per-cell fields.** The struct marks 16 arrays "saved" (including `riverFlow` and `flooded`); `hydrology-save.md` says the constant list has 15 per-cell entries, and also saves `biome` (a terrain field). Task 0 reads both and Task 2's literal list follows whichever is right; the mismatch is raised with the user, not guessed.
3. **Table split with Feature 2.** The downstream index, the in-basin override and the topological order are built here (ACs 16 to 18 require them); release rates, trees and the largest-tree size are Feature 2's, in the empty `BuildRoutingTables()` seam. Written the same way in `water-pipeline.plan.md`.
4. **`MarkCrossings` is test-side.** AC 14 tests an `isCrossing` builder, but production assembly is Feature 3's `BuildHydrologyTerrain` (presets-and-regression AC 30 tests it on a generated world). Confirm that is the intended split.
5. **`glacier.check_week` mapping** is closed by Task 0 (Open Design Question 3) and handed to Feature 2.
6. **In-basin tie rule** is fixed in Implementation Patterns (Open Design Question 5). Revisit only if Task 14 finds a counter-example.
7. **Unknown-biome fallback logging** (EC-09) needs a once-per-id set. Use a small fixed `uint8_t seen[256]` inside the simulation so no allocation; no global state.
