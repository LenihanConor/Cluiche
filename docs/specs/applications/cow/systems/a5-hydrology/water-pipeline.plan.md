# Plan: A5 Water Pipeline (A5 Hydrology Feature 2)

**Spec:** @docs/specs/applications/cow/systems/a5-hydrology/water-pipeline.md
**Status:** Not Started
**System plan:** @docs/specs/applications/cow/systems/a5-hydrology/a5-hydrology.plan.md (row 2)

**Starts after A5 Feature 1 (`hydrology-data.plan.md`) tasks 1 to 16 are Done:** it extends Feature 1's
`HydrologySimulation`, `HydrologyFields`, `HydrologyTerrain`, `HydrologyParameters` (43 scalars + the 13 x 3
biome table), `BasinCurve`, `RederiveCommitted()` and the empty `BuildRoutingTables()` seam, and it reuses
`Testing/HydrologyTestGrids.h`. `ApplyPhaseSeed` (Feature 1 Task 17) and the shipped JSON are not needed. It
also needs DiaDeferredStep Feature 1 **Task 1** Done (`IParallelExecutor`, `SerialExecutor`; commit c83a94ed).
Tasks 20 and 21 (generated-world checks and the golden) additionally need S-1 consumer amendment 8 (basin table,
bed elevation) and a way to build a `HydrologyTerrain` from a generated world; until then they stay Not Started
with "Blocked until" notes and the rest of the plan proceeds.

Later plans that need this one built: `hydrology-module` (calls `Step`), `hydrology-save` (reads the totals and
fields), `hydrology-debug` (calls the release-rate, basin-curve and passability helpers), and
`hydrology-presets-and-regression` (every scenario uses `Step`, `FixedWaterInput`, `RebaselineStorage`).

All code is CoW (application) under `Cluiche/CoW/Shared/Hydrology/`. No Dia edits (AD-001): `IParallelExecutor`
and `SerialExecutor` are consumed, not changed. Where a task could need more than sonnet it says "ask before
escalating" (feedback_confirm_opus).

## Decision: who builds the static tables (binding on both plans)

Feature 1 already built (it needs them for its own ACs 16 to 18): the working basin set and capacities, the
in-basin flow directions, the **downstream index** and the **topological order** (Kahn, ties by ascending index,
EC-14 failures). **This plan fills the empty `BuildRoutingTables()` seam** with the rest: per-cell release rate
`k`, trees grouped by terminal, the largest-tree node count. It does not rebuild the downstream index or the
order; it reads them. If Feature 1's tables lack something the cascade needs (for example the per-node tree id),
the field is added to Feature 1's table struct in a **named Task 5 step** and Feature 1's tests rerun. `BasinCurve`
and the final derivation (`RederiveCommitted()`) come from Feature 1; this plan adds edge tests, not a second
implementation.

## Implementation Patterns

### Public types (Task 3)
- `WaterInput.h` (header only): `WaterSinks { const float* etMultiplier; const float* draw; }`, `WaterInput`
  (`waterToGround`, `temperature`, `snowPack`, `glacierInput`, `extraWater`, `sinks`, `uint8_t week`) and
  `WaterTotals` (double `in, seaOut, etOut, drawOut, drawUnmet, basinEvapOut, fixedLakeMakeup, storage, error`;
  `uint32_t floodedCells, deepCells, glacierCells, largestTreeNodes`), exactly as the spec. Null `extraWater`
  or null sink arrays mean zero and one. The pipeline reads them through one small inline accessor each
  (`ExtraAt(in, c)`, `EtMulAt(in, c)`, `DrawAt(in, c)`) so a null is handled in one place.
- `FixedWaterInput`: `SetConstant(waterToGround, temperature)` and `SetScript(weeklyWater, weeklyTemp)`
  (two equal-length `DynamicArrayC<float>`, played in a loop). `WaterInput ForWeek(uint32_t week) const` returns
  pointers into **member** arrays sized at `SetConstant` / first use (N cells filled uniformly); `snowPack` and
  `glacierInput` point to zero arrays. Ranges: water >= 0, temperature in [0, 1], equal script lengths, non-empty.
  No `std::` types in the signature (PD-004).

### Lifecycle (Task 4)
- `HydrologySimulation` (Feature 1 class) gains `void Step(const WaterInput&, Dia::DeferredStep::IParallelExecutor&)`,
  `const WaterTotals& LastTotals() const`, `void RebaselineStorage()`. `Step` before `Initialise` asserts in
  Debug and is a no-op in Release (AC 2).
- Two field sets: committed (`mFields`, `mBasins`) and working. `Step` copies committed into working, runs the
  passes on working, then swaps (swap, not copy). "Inundated" in any pass always reads the **committed** flag
  (last week's), kept in a stable `uint8_t` array. Totals are produced by the same call and never feed back.
- `Step` reads only its `WaterInput`, the committed fields, the parameters and the immutable terrain and
  tables (DFS-002). A grep of the `Step` call tree for members not latched at `Initialise` is part of Task 19.
- **No heap allocation inside `Step`:** every scratch array (per-cell and per-basin inflow, per-range
  double accumulators, flood-flag scratch) is sized at `Initialise` / `BuildRoutingTables()`. Code rule: no `new`,
  no growing containers, no `std::` allocation in the `Step` call tree. The test records every array's data pointer
  and capacity before and after `Step` and asserts they are unchanged (A4's mechanism; no global `operator new`
  hook). The cascade's internal trees may use STL **privately at build time only**, not in `Step`.
- Pass order inside `Step`, one function per pass in `HydrologyPipeline.cpp`:
  `P0 Inputs`, `P1 Freeze`, `P2 Glacier`, `P3 Infiltration`, `P4 ET and sinks`, `P5 Runoff and baseflow`,
  `P6 Cascade` (in `HydrologyRouting.cpp`), `P7 Basin level`, `P8 Floodplain and counters`, `P9 Passability`,
  `P10 Glacier counters`, `P11 Clamp and totals`. Each local pass is one `executor.ParallelFor(N, kMinRange = 256, fn)`
  writing only its own cells; each wraps itself in `DIA_PROFILE_SCOPE` named for the pass.

### Routing tables and basin curve (Tasks 5 to 7)
- `HydrologyRouting.h/.cpp`: `BuildRoutingTables(...)`, `kNodeCount = N + basinCount`. Release rate:
  `k[c] = isRiver ? clamp(release_min + release_slope * drop, release_floor, release_ceil) : hillslope_release`,
  `drop = max(0, elevation[c] - elevation[downstream[c]])`, 0 at a terminal. A free function
  `float ReleaseRate(const HydrologyTerrain&, const HydrologyParameters&, uint32_t cell, uint32_t downstream)`
  is exported (Feature 5 calls it; no re-derivation there).
- Trees: connected components of the routing graph, one per terminal (sea, `kFlowSink`, or the map edge),
  stored as a CSR (`treeFirst[]`, `treeNodes[]`) in topological order within the tree. `largestTreeNodes`
  is the size of the biggest tree, reported in the totals.
- Cascade (P6): `ParallelFor` over **trees** (grain 1 tree minimum, `kMinRange` of nodes per range balanced by
  prefix sums, fixed at build time). Each tree is serial in its stored order. Per cell node (not inundated):
  `total = riverStore + inflow; out = k * total; riverStore = total - out; riverFlow = isRiver ? out : 0;
  route(out, downstream)`. `route` adds to `inflow[target]`, except a target that is an **inundated cell or a
  basin pit**, which adds to that basin's `inflow`. Per basin node: `volume += inflow + directInput`;
  `basinEvapOut += basin.evap_rate * meanT * openCells` (`openCells = inundatedCount * (1 - iceFraction)`, clipped
  to volume); `overflow = max(0, volume - capacity)` routed to the spill cell in the same pass;
  `volume = min(volume, capacity)`; `inflow = 0`. Terminals: ocean to `seaOut`; a `kFlowSink` land terminal keeps its
  water as `surfaceWater`.
- A node's inflow is summed in topological order (the order is fixed and the same at any range count).
  Every inflow array is zeroed at the start of the pass by the owning range.
- `lake-only` worlds (`basinsFromLakesOnly`): each lake's `volume` stays pinned at capacity, inflow passes
  straight through to the spill cell, and an evaporation shortfall is added to `fixedLakeMakeup`.

### Local passes (Tasks 8 to 13, 16)
- **P0:** non-inundated land cell `surfaceWater += waterToGround + max(extraWater, 0)`; inundated cell adds the same
  to that basin's direct input (a per-basin double-free `float` array written by the tree owner at P6, so P0
  writes a per-cell staging array and the cascade gathers it). Negative `extraWater` is a **sink** applied through
  the draw rule, not an input (it is "drier" per the spec).
- **P1:** cell freeze/thaw per the formula with hysteresis; basin ice fraction from the mean `T` over the basin's
  inundated cells (gather by basin range, serial inside a basin). Basin ice never changes `volume`.
- **P2:** `glacier += glacierInput`; melt above `glacier.melt_threshold` goes to `surfaceWater` of the cell (or to
  the basin's direct input on an inundated cell) so P6 routes it this week.
- **P3:** infiltration with `frozen_ground_factor`, percolation above `field_capacity * cap`, springs return
  the ground-water excess to `surfaceWater`. Inundated cells skip.
- **P4:** soil ET, surface evaporation, then `sinks.draw` from surface, then soil, never from `riverStore`; the unmet
  remainder goes to `drawUnmet`; each amount adds to its own total in **double** per range. An inundated cell's draw
  is unmet. `etMultiplier` applies to soil ET only.
- **P5:** `runoff = max(0, surfaceWater - routing.surface_retention)` and `baseflow = baseflow_rate * groundWater`
  move into the cell's own `riverStore` on every non-inundated land cell.
- **P7:** per basin: `level = LevelForVolume(volume)` (Feature 1), inundation from `bed < level`, newly inundated
  cells move `surfaceWater`, `soilMoisture`, `groundWater` and `riverStore` into the volume (their `ice` stays),
  newly exposed cells start empty, then `inundatedWeeks` / `exposedWeeks` (`exposedWeeks` counts only where the
  cached biome is Lake and the cell is not inundated), then `openWater` via Feature 1's derivation.
- **P8:** `meanFlow += alpha * (riverFlow - meanFlow)` with `alpha = 1 - 0.5^(1/flood.mean_weeks)` (computed once at
  `Initialise`, not per cell); flag gather in direction order into `flooded`; recharge as a **serial** loop in
  cell-id order over rivers in flood only (a pre-built list of river cells in id order). `lowFlowWeeks`
  (not frozen: `ice < ice_crossing_min`; a frozen week neither counts nor resets), `glacierWeeks`,
  `glacierFreeWeeks`. Inundated cells skip the flood part.
- **P9:** the first-match passability rule from Feature 1's derivation function; this pass calls it, it does not
  copy it. Writes `passability`; counts `floodedCells`, `deepCells`, `glacierCells` into the totals.
- **P10:** only on the check week (the mapping recorded in Feature 1 Task 0, GDD "26" being week index 25 unless
  Task 0 found otherwise): `snowYears` increments where `snowPack >= glacier.snow_floor` (equal counts), else 0;
  `glacierEligible = snowYears >= onset_years`. Existing glacier mass stays and keeps melting.
- **P11:** clamp every field non-negative with the excess pushed into the next store (never dropped), clamp
  `iceFraction` to [0, 1], then the totals pass: per-range `double` sums, combined in range order on the calling
  thread, into `WaterTotals`. `error = in - out - (storage - storagePrev)`; `storagePrev` is held across steps and
  reset by `RebaselineStorage()`.

### Determinism
- Local passes write only their own cells. The cascade ranges own whole trees. Basins live inside trees. The flood
  flags are a gather; the recharge is serial in cell-id order. Totals reduce per range and combine in range order.
- Result is bit-identical at 1, 2, 7 and 16 ranges and with `SerialExecutor`, and with a reverse-order executor.

### Testing
- Test executors: use DiaDeferredStep's `Testing/` ones if they ship (and A4's Weather `Testing/` ones if built),
  else write `ReverseRangeExecutor` and `FixedRangeExecutor(n)` under `Cluiche/CoW/Shared/Hydrology/Testing/`
  (tests ship with libraries, not in GoogleTests).
- Test grids in `Testing/HydrologyTestGrids.h` (created by Feature 1; this plan adds): `MakeChain(n, drop)`
  (40-cell river chain, uniform and stepped drops), `MakeConfluence()`, `MakeBasinChain3()`, `MakeFloodplain()`,
  `MakeSteepAndFlatChains()`, `MakeSmallAndLargeRiver()`. Each carries a comment naming the ACs it serves.
- Files under `Tests/GoogleTests/CoW/Hydrology/`: `TestHydrologyPipelineRouting.cpp`, `TestHydrologyPipelineCascade.cpp`,
  `TestHydrologyPipelineLocal.cpp`, `TestHydrologyPipelineBasin.cpp`, `TestHydrologyPipelineFlood.cpp`,
  `TestHydrologyPipelinePassability.cpp`, `TestHydrologyPipelineConservation.cpp`, `TestHydrologyPipelineDeterminism.cpp`,
  `TestHydrologyPipelineGolden.cpp`, `TestWaterInput.cpp`. Suite prefixes `HydrologyPipelineRouting*`, `HydrologyPipelineCascade*`,
  `HydrologyPipelineLocal*`, `HydrologyPipelineBasin*`, `HydrologyPipelineFlood*`, `HydrologyPipelinePassability*`,
  `HydrologyPipelineConservation*`, `HydrologyPipelineDeterminism*`, `HydrologyPipelineGolden*`, `HydrologyWaterInput*`.
- Determinism tests compare with `memcmp` after the stated week count.
- Known answers (conservation, travel time, release) are computed by hand or by a small independent loop in the
  test, not by the production helper.
- Goldens: `hydrology_layers.golden.json`, keys prefixed `pipeline/`; this feature creates the file, Feature 6
  adds `scenario/`. Bit-exact compare; the build uses `/fp:precise` (recorded in Task 0).
- Performance is report-only (parent decision 7); no benchmarking rig.

### Verification cadence
- Per task: `dia run googletest --filter="<suite prefixes for the task>" --shards 2`. Never run the 520-week or
  100-year cases while iterating one pass; they run once at the end of their own task.
- Scoped verification near the end (`*Hydrology*`). **No full-suite run here** (it is the A5 system close-out).
  No mutation checks. No sanitizer.
- One commit per task (code, tests and the plan row together).

## Tasks

`dia docs vcxproj-add` must be run from PowerShell, or with literal paths: a bash loop over it has a known bug.
Update rows with `dia docs plan docs/specs/applications/cow/systems/a5-hydrology/water-pipeline.plan.md <#> --status <S> --notes "..."`.

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 0 | Prerequisite check, read only: Feature 1 tasks 1 to 16 Done; Feature 1's table struct (downstream, topological order, working basins, `BasinCurve`, `RederiveCommitted`) and whether it carries a per-node tree id; DiaDeferredStep Feature 1 Task 1 headers (`IParallelExecutor`, `SerialExecutor`, commit c83a94ed) and whether a `Testing/` reverse or fixed-range executor ships (also A4's Weather `Testing/`); `CellLayerGolden.h` and `GoldenStore::Check` (hash exact bits or quantised?); `/fp:` settings of CoW in Debug and Release; `DIA_PROFILE_SCOPE` usage in CoW; the `glacier.check_week` mapping recorded in Feature 1 Task 0; S-1 amendment 8 and any `BuildHydrologyTerrain` status (decides whether Tasks 20 and 21 can start) | Findings recorded in notes | Not Started | haiku | TDD-exempt investigation. If a Dia header is missing, stop and flag it. If Feature 1's tables lack the tree id, note it for Task 5 |
| 1 | Create empty file pairs `HydrologyPipeline`, `HydrologyRouting` under `Cluiche/CoW/Shared/Hydrology/`, `WaterInput.h`, `Testing/` additions, the ten test files above, and the folder `Tests/GoogleTests/CoW/Golden/` entry; register with `dia docs vcxproj-add CoW ...` and `dia docs vcxproj-add GoogleTests ...` | Solution builds with the empty files | Not Started | haiku | Mechanical, TDD-exempt. `BasinCurve` and `HydrologySimulation` already exist from Feature 1. Run from PowerShell or with literal paths. Commit |
| 2 | RED then GREEN: test executors `ReverseRangeExecutor` and `FixedRangeExecutor(n)` (skip if Task 0 found equivalents) | Self-test: ranges cover `[0, N)` exactly once, contiguous, in the stated order; n = 1, 2, 7, 16, one range per tree or cell | Not Started | sonnet | Must implement the same `ParallelFor(count, minRange, fn)` contract as `IParallelExecutor`. Needed by every determinism task. Commit |
| 3 | RED then GREEN: `WaterSinks`, `WaterInput`, `WaterTotals`, `FixedWaterInput` | AC 3 (accessor half), supports 22/23: null `extraWater` or sink arrays read as 0 and 1; `SetConstant` gives every cell the same pair; `SetScript` plays weekly arrays in a loop (week `L` equals week 0); unequal or empty scripts and out-of-range values rejected; `snowPack` and `glacierInput` are zeros | Not Started | sonnet | Header-only types plus `FixedWaterInput` in a small `.cpp` if needed. The null-neutral accessors are tested directly. Commit |
| 4 | RED then GREEN: `Step`, `LastTotals`, `RebaselineStorage` lifecycle with empty passes (committed and working sets, swap, `storagePrev`, pass-order skeleton) | AC 1, 2, 3 (behaviour half): `Step` before `Initialise` is a no-op in Release and asserts in Debug; committed fields unchanged if the passes are empty; `Step` with null `extraWater`/sinks equals `Step` with explicit zeros and ones (bit for bit, once real passes exist this is re-run in Task 12); poisoning arrays **not** passed in `WaterInput` changes nothing; `RebaselineStorage` makes the next `error` exclude an edit made between steps | Not Started | sonnet | The poison test (AC 1) is completed in Task 19 once passes exist. No allocation check yet |
| 5 | RED then GREEN: `BuildRoutingTables()` (fill Feature 1's seam): per-cell release rate, trees, largest-tree size; export `ReleaseRate(...)` | AC 10a (first half), 11 (tree membership): river `k` equals `clamp(release_min + release_slope * drop, release_floor, release_ceil)` on stepped chains (below floor, in range, above ceil), terminal `drop` 0, non-river `k` equals `hillslope_release`; every node is in exactly one tree and in topological order inside it; two independent chains give two trees; `largestTreeNodes` equals the biggest | Not Started | sonnet | Reads Feature 1's downstream index and order, does not rebuild them. If Task 0 found no tree id, add it to Feature 1's table struct here and rerun Feature 1's `HydrologyInitialise*` tests. Commit |
| 6 | RED then GREEN: `BasinCurve` edge tests only (the code is Feature 1's) | AC 13 (curve half): level for volume is monotone and interpolates between sorted bed steps; equal beds; basin of one cell; volume above capacity clamps at `spillHeight`; capacity equals the hand sum with `basin.depth_scale` | Not Started | sonnet | If a Feature 1 defect shows, fix it in `BasinCurve` here with a note and rerun `HydrologyInitialise*`. Commit tasks 5 and 6 together |
| 7 | RED then GREEN: pass 6 cascade, cell nodes only: single chains, confluence, steepness (stores set directly in the test) | AC 10, 10a (travel time), 11, 12: a one-week pulse at the head of a 40-cell chain reaches the mouth in the same week; the peak is attenuated by the product of the `k` values and the tail decays; the pulse reaches `seaOut` in full to tolerance as weeks grow; the centre of mass advances about `k / (1 - k)` cells per week at `k` = 0.5 and 0.85 (tolerance stated in the test); the result equals an independent `out = k * (store + inflow)` hand loop; two tributaries give the same result in either listed order; larger drops give a higher same-week fraction at the mouth | Not Started | sonnet | Cascade only: P0, P5 are not yet wired. Ocean terminal gives `seaOut`; `kFlowSink` terminal keeps the water as `surfaceWater` (AC 17 is Task 8). Commit |
| 8 | RED then GREEN: pass 6 cascade with basin nodes: fill, spill, dry basin, chained basins, tiny depression, land terminal | AC 13, 13a, 13c, 14, 17: inflow fills a basin and raises its level along the curve; above capacity the overflow leaves through the spill cell the same week and `volume = capacity`; water landing on a dry basin's footprint follows the in-basin directions to the pit and does not cross to the spill; a tiny depression is never inundated and routes at S-1 directions; three basins in series, all at spill, pass a pulse to the sea in one week; a `kFlowSink` land terminal keeps its water as surface water | Not Started | sonnet | Uses `MakeBasinChain3` and Feature 1's dry-basin grid. Basin evaporation goes to `basinEvapOut`, tested in Task 14 with the level. Commit tasks 7 and 8 |
| 9 | RED then GREEN: pass 0 inputs, pass 1 freeze/thaw (cells and basin ice) and pass 2 glacier | AC 4, 4a, 5: below `freeze.threshold` the share `freeze.rate * (threshold - T) / threshold` of `surfaceWater` and `riverStore` moves to `ice` and is not routed; above `thaw_temp` ice melts by the formula and a thin last layer melts in finite time (`ice_melt_floor`); between the two nothing changes; total water unchanged; basin `iceFraction` rises at `freeze.rate`, falls at `thaw_rate`, stays in [0, 1], falls with no inundated cells, and never changes `volume`; `glacierInput` adds to `glacier`; no melt at or below `melt_threshold`; melt follows `melt_rate * (T - threshold) * (glacier + ice_melt_floor)`, never exceeds the mass, goes to surface water and routes the same week | Not Started | sonnet | Inputs: `surfaceWater += waterToGround + max(extraWater, 0)`; an inundated cell's amount goes to the basin direct input. Hysteresis tested with temperatures just inside each bound. Commit |
| 10 | RED then GREEN: pass 3 infiltration, percolation and springs | AC 6: soil takes at most `infil_rate` per week and fills to `soil_capacity` and no further; frozen ground (`T < freeze.threshold`) takes `frozen_ground_factor` of that; soil above `field_capacity * cap` percolates at `percolation_rate`; ground water over `soil.gw_capacity` returns as surface water and ground water is set to the capacity; the total over the pass is unchanged; an inundated cell is untouched (inundated half of AC 13b, completed in Task 14) | Not Started | sonnet | One expected value per biome row on a one-cell grid, hand-computed. Commit |
| 11 | RED then GREEN: pass 4 ET and sinks | AC 7, 8, 26, 3 (sink half): zero ET at `T` 0; soil ET scales with `veg_factor` (forest above desert at equal soil); `etMultiplier` 2 removes exactly twice the neutral ET unless clipped; surface evaporation ignores the multiplier; a draw comes from surface then soil and never from `riverStore`; a draw above what is there empties both and reports the rest in `drawUnmet`; a draw on an inundated cell is unmet; two providers' draws summed equal one provider with the sum and two multipliers equal their product (one test per claim) | Not Started | sonnet | Amounts go to `etOut` and `drawOut` as doubles. Commit |
| 12 | RED then GREEN: pass 5 runoff and baseflow, then wire P0 to P6 into `Step` in spec order | AC 9, 3 (end to end): zero runoff below `routing.surface_retention`; runoff and baseflow land in the cell's own `riverStore` on hillslope and river cells alike; baseflow keeps a river cell flowing many weeks after rain stops; `Step` with null arrays equals explicit neutral arrays (re-run of Task 4's test); chain and basin tests from Tasks 7 and 8 still pass through `Step` | Not Started | sonnet | First task where `Step` runs the real passes in order. The pass order is fixed by a comment table in `HydrologyPipeline.cpp`. Commit tasks 9 to 12 as one logical group |
| 13 | RED then GREEN: pass 7 basin level, inundation, open water, dry-down, lake-only worlds | AC 13b, 15, 16, 21c: an inundated cell's soil, infiltration, ET, runoff and flood flag do not change and its rain goes into the volume; a newly inundated cell moves its land stores into the volume and the balance still closes; a newly exposed cell starts empty; with no inflow, evaporation lowers the level, cells above it become exposed, `exposedWeeks` counts up (only where the cached biome is Lake) and a refill resets it and counts `inundatedWeeks`; `basinsFromLakesOnly`: a lake never changes level, inflow passes through, an evaporation shortfall shows as `fixedLakeMakeup`; river `openWater` equals `wetness.river_open_water * min(1, riverFlow / max(meanFlow, flood.min_flow))`, an inundated cell's is `1 - iceFraction`, Ocean 1 | Not Started | sonnet | Calls Feature 1's `RederiveCommitted` pieces for `openWater` and level, then adds the counters. Commit |
| 14 | RED then GREEN: pass 8 floodplain flags, recharge and counters | AC 18, 18a, 19, 21a, 21b: a steady-flow river never floods; a pulse above `bankfull_factor * max(meanFlow, flood.min_flow)` floods the river cell and its non-inundated neighbours at most `floodplain_rise` above it; the flag clears the week after the flow drops; a higher or inundated neighbour never floods; recharge is `min(recharge_share * surplus, riverStore)` split equally with each cell limited by its soil room, `riverStore` drops by what was given and the balance closes; two overlapping plains give the same result at any range count; a small stream and a large river each given a pulse proportional to its own mean both flood; a frozen week (`ice >= ice_crossing_min`) neither counts nor resets `lowFlowWeeks`, a non-frozen low week increments and a recovered flow resets; `glacierWeeks` and `glacierFreeWeeks` are mutually exclusive each week with exactly one 0, and a cell exactly at `biome_min` counts as glaciated | Not Started | sonnet | The recharge loop is serial, cell-id order, over a pre-built river list. `MakeFloodplain` and `MakeSmallAndLargeRiver` grids. Commit |
| 15 | RED then GREEN: pass 9 passability | AC 20: first match wins; inundated with `iceFraction >= ice_crossing_min`, or a river cell with `ice >= ice_crossing_min`, is `Frozen` and beats `Deep`; deeper than `basin.ford_depth` is `Deep`, a protected cell (`isCrossing`) at the same depth is `Fordable`; shallower inundation is `Fordable`; a flooded cell `Wet`; anything else `Dry`; ice on a dry land cell does not make it `Frozen`; lake-only worlds: lake cells `Deep` except protected cells `Fordable`, and `Frozen` beats the lake-only `Deep` | Not Started | sonnet | One truth-table test over the five rules in order. Calls Feature 1's function; no copy of the rule. Commit |
| 16 | RED then GREEN: pass 10 glacier counters | AC 21: `snowYears` and `glacierEligible` change only on the check week (mapping from Feature 1 Task 0); a `snowPack` exactly at `glacier.snow_floor` counts; below resets to 0; eligibility turns on at `glacier.onset_years` and off when the snow clears; existing glacier mass stays and keeps melting; no other week changes the counters | Not Started | sonnet | Run 52 weeks with a script that hits the check week once per year. Commit tasks 13 to 16 |
| 17 | RED then GREEN: pass 11 clamp and totals, and the first balance tests | AC 24, 22 (hand-built half, 52 weeks): no field is ever negative after any week of extreme inputs and clamping never changes the balance (excess goes into the next store); totals equal a cell-order reference reduction (double); `in`, `out`, `storage`, `error` as in the spec; `iceFraction` stays in [0, 1]; `|error| <= 1e-4 * max(in, out, 1)` each week on the chain, basin-chain and floodplain grids | Not Started | sonnet | A breach never adds or removes water to "correct" it (GDD EC-12). Ice and glacier count as stored water; `iceFraction` does not. Commit |
| 18 | RED then GREEN: long conservation runs on hand-built grids | AC 22 (520 weeks on each hand-built grid), 23 (hand-built Small-size grid): scripted input with a 4-year and a 40-year cycle over 100 simulated years; per-week tolerance every week; cumulative `|error|` under `1e-3 *` cumulative throughput | Not Started | sonnet | Run the long cases once at the end of the task, not per edit. If a breach shows, find the pass that loses or creates water before touching any tolerance. Commit |
| 19 | RED then GREEN: purity and determinism matrix | AC 1 (full), 25, 11 (order independence at scale): poisoning source arrays after the call changes nothing; no allocation in `Step` (data pointer and capacity of every scratch array, table and both field sets unchanged); `SerialExecutor`, 1, 2, 7, 16 ranges, one range per tree, and reverse order give bit-identical fields and totals over 104 weeks on the chain, basin-chain, floodplain and a combined grid; same inputs twice are identical | Not Started | sonnet | If any pair differs, stop: it is a real ordering bug, not a tolerance. Record the grep of `Step`-reachable members against what `Initialise` latched in notes. Commit |
| 20 | RED then GREEN: generated-world checks | AC 22 (104 weeks on a generated Small world), 23 (Small world, 100 years) | Not Started | sonnet | **Blocked until S-1 amendment 8 and a generated-world to `HydrologyTerrain` assembly exist** (Feature 3's `BuildHydrologyTerrain`, or a test-local one per Task 0). Mark with `dia docs plan` and continue. The hand-built halves are Tasks 17 and 18 |
| 21 | Golden registration: create `Tests/GoogleTests/CoW/Golden/hydrology_layers.golden.json` with `pipeline/` entries through `CellLayerGolden` | AC 27: layer hashes for `FixedWaterInput` on S-1 presets x seeds 1 to 3 at Small after 52 weeks, bit exact | Not Started | sonnet | **Blocked until the same generated-world assembly as Task 20.** Uses `GoldenStore::Check` and `DIA_UPDATE_GOLDEN=1` for the first write only. Per-configuration hash sets only if Task 0 shows Debug and Release still differ under `/fp:precise`. Commit |
| 22 | Profile and report: `DIA_PROFILE_SCOPE` per pass (`P0` to `P11`), `largestTreeNodes` in totals, a report-only timing print on a Large-size hand-built grid | Per-pass breakdown printed; parent decision 7 (report only, never fails) | Not Started | sonnet | Report numbers; do not tune. Check the scopes exist and are named for the pass. Commit |
| 23 | Exhaustive test pass: gap audit of AC 1 to 27 (including the lettered ones) against the written tests, then edge cases | Gap list in notes. Edge cases: N = 1 and N below `kMinRange`; a map with no rivers; a map with no basins; a river cell that is also a basin pit; a basin whose spill cell is another basin's footprint; a basin of one cell; zero `waterToGround` for 52 weeks; `T` exactly 0 and exactly 1; `T` exactly at `freeze.threshold`, `thaw_temp`, `glacier.melt_threshold`; `freeze.threshold == thaw_temp`; `release_floor == release_ceil`; `Step` twice with no change in input; `RebaselineStorage` right after `Initialise`; a huge `extraWater`; negative `extraWater` as a sink | Not Started | sonnet | No mutation check (project rule 2026-11-12). Add only the tests the audit shows missing |
| 24 | Observation Opportunity Scan over every file touched across logs, traces, profiling, metrics, health | Suggestions in notes | Not Started | haiku | Non-blocking. `DIA_PROFILE_SCOPE` per pass is in scope (Task 22). Note `cow.hydro.balance_error`, `cow.hydro.largest_tree_nodes` and a health breach on the balance as Feature 5's |
| 25 | Scoped verification: `dia run googletest --filter="*Hydrology*" --shards 2`; `dia check deps`; `dia docs precommit` | All AC quoted; pass/fail and failing names only; ACs 22/23 (generated) and 27 reported as Blocked if Tasks 20 and 21 still are | Not Started | haiku | **No full-suite run here**; the single full `dia run googletest --shards 2` is the A5 system close-out |
| 26 | Close-out: `dia docs plan` rows to Done with notes, then `dia docs spec-done docs/specs/applications/cow/systems/a5-hydrology/water-pipeline.md` | Spec status Done, plan header updated | Not Started | haiku | **Do not run until every row above is Done** (Tasks 20 and 21 included, or explicitly deferred by the user). Not to be run when this plan is written |

## Open items

1. **Table split with Feature 1.** The downstream index and the topological order are Feature 1's (its ACs 16 to 18 need them); this plan fills only the release-rate and tree part of `BuildRoutingTables()`. Same text in `hydrology-data.plan.md`. Confirm with the user.
2. **`BasinCurve` and the derivation code** are built in Feature 1 (it needs capacity and level to seed lakes). The spec lists `BasinCurve` under this feature's Files Touched; this plan only adds edge tests (Task 6).
3. **Generated-world ACs.** AC 22 (generated Small), AC 23 and AC 27 need a generated world turned into a `HydrologyTerrain` with amendment 8's basin table. Tasks 20 and 21 are Blocked until that exists. Decide whether to build a test-local assembly or wait for Feature 3.
4. **Negative `extraWater`.** The spec says "drier" goes through sinks, but the input table makes `extraWater` signed. This plan treats the negative part as a draw (Task 9 notes and Task 23 edge case). Confirm that is the intent (it is not stated for the draw order).
5. **Inundated-cell direct input.** P0 stages per-cell water for inundated cells and the cascade gathers it into the basin's direct input; Pass 6 text says the basin node adds "direct input on inundated cells". If the spec meant a different staging, adjust in Task 9.
6. **Glacier check-week index** comes from Feature 1 Task 0 (Open Design Question 3 in the data spec).
7. **Cascade range grain.** Ranges own whole trees; the grain constant is chosen in Task 5 and tuned only by report, never by a test. A single huge tree runs serial; `largestTreeNodes` shows it.
8. **Same-week glacier melt** (spec Open Design Question 4) is built as spec (pass 2). If S-2.6 wants the GDD's delay, move it after P6; not decided here.
