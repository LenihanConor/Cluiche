# Plan: A4 CA Pipeline (A4 Weather Feature 2)

**Spec:** @docs/specs/applications/cow/systems/a4-weather/ca-pipeline.md
**Status:** Not Started
**System plan:** @docs/specs/applications/cow/systems/a4-weather/a4-weather.plan.md (row 2)

Rebuilt 2026-11-18 against the world-sim review amendment. This is pure logic on hand-built hex terrain, so
it can start **before** the foundation chain finishes, as soon as Feature 1's types and `Initialise` exist.

**Starts after A4 Feature 1 (`weather-data.plan.md`) has its types, sidecar, `ParamBinding` and `Initialise`
tasks Done** (Features 1's baseline and re-`Initialise` tasks may still be open): it extends Feature 1's
`WeatherSimulation`, the snapshotted `WeatherParameters` struct (which already carries `cloud_decay`),
`IBiomeSeasonalBaseline`, `SeasonalTargetTemperature` and the empty `BuildGridTables()` seam. That in turn needs
`parameter-files.plan.md` and `biome-registry.plan.md` Done (the struct is filled by `ParamBinding::Snapshot`;
biomes are `BiomeIndex`). Nothing in this plan needs `grid-search-helpers.plan.md`, `worldsim-kit.plan.md` or
`blackboard-scopes.plan.md`. It also needs DiaDeferredStep Feature 1 **Task 1** Done (`IParallelExecutor`,
`SerialExecutor` headers). Task 21
(deferred-vs-synchronous) additionally needs DiaDeferredStep's manual launcher; if that is not built yet,
that one task stays Blocked and the rest of the plan proceeds.

`presets-and-regression.plan.md` needs this plan built (its forcing, region-forcing and every regression
task use `Step`, `AddForcing` and `RegisterCellSet`).

All code is CoW (application) under `Cluiche/CoW/Shared/Weather/`. No Dia edits (AD-001): `IParallelExecutor`
and `SerialExecutor` are consumed, not changed. Where a task could need more than sonnet it says "ask
before escalating" (feedback_confirm_opus).

## Implementation Patterns

### Lifecycle (Task 5)
- `WeatherSimulation` (Feature 1 class) gains `Capture(const WeatherParameters&, Season, uint32_t weekOfYear, const IBiomeSeasonalBaseline&, const HydrologyInputs&, const InfluenceSnapshot&)`. The `WeatherParameters` argument is the struct already filled by `ParamBinding::Snapshot` at the caller (amendment 1): no pass and no `Capture` code reads `ParameterFile`, and cycle modifiers on scalar parameters are already resolved in it. `LatchClimate(uint64_t evalDay)` (Feature 8),
  `Run(Dia::DeferredStep::IParallelExecutor&)`, `Commit()`, `Step(Season, weekOfYear, baseline, hydro, influences, executor)` (exactly
  `Capture; Run; Commit`), `GetGridTotals()`, `GetLastProfile()`, `SetCommittedWeek(uint64_t)`.
- Two `WeatherFields` instances (committed, working) plus a **latched** struct `CapturedInputs`
  (`WeatherParameters` copy, `Season`, `float baseline[kMaxBiomes]` for the captured season sized by the
  registry's `Count()` (at most 254), the S-1 westerly-band latitudes, forcing array copy, cell-set copy, working
  biome-index array copy). `Run` takes **only** these, the immutable terrain and the grid tables. The baseline
  provider is called once per registry biome, inside `Capture` and never in `Run` (DFS-002).
- `Commit` swaps working into committed (swap, not copy), increments the week, decrements
  `weeksRemaining` and drops expired records, **wind overrides included**: `weeksRemaining == 0` means until
  cleared (never decremented or dropped); a positive value expires after that many weeks (amendment 1).
  `clear` (Feature 5) removes every wind override at once.
- No `Run` method reads a member that `Capture` did not latch. Reviewing this is a `grep` task in Task 20.

### Grid tables (Task 4)
- Built in Feature 1's empty `BuildGridTables()`: `DynamicArrayC<uint32_t> neighbours` of `N*6` from
  `Dia::Pathfinding::HexTopology::NeighbourAt`, `kNoCell` past the edge; `static constexpr` unit vectors
  for `d = 0..5` at on-screen angle `30 deg + 60 deg * d`, counter-clockwise, y-down; scratch arrays
  `moistureOut`, `heatOut`, `anomalyOut` (float; `anomalyOut` added 2026-11-17 (row 6d)) and `dirIndex` (`uint8_t`) allocated here and reused, plus pass 0's per-cell delta scratch (Feature 7).
- **`Run` does no heap allocation.** Every per-pass temporary is one of these scratch arrays or a stack
  value. Every scratch array, grid table and the cell-set storage is sized at `Initialise`. Code rule: no
  `new`, no growing containers and no `std::` allocation in the `Run` call tree (amendment 5).

### Wind direction (Task 3)
- `Cluiche/CoW/Shared/Weather/WindDirection.h/.cpp`: `uint8_t SnapWindDirection(float angleRadians,
  uint32_t column)`, `PrevailingDirection(float latitude)`, the unit-vector table.
- Snap is `d = round((angle - 30 deg) / 60 deg) mod 6`. A tie is any angle within `1e-4` rad of a
  half-step. East tie (0): even column 5, odd column 0. West tie (pi): even column 3, odd column 2.
  Any other tie uses the lower index. Normalise the angle into [0, 2pi) first. `HexTopology::DirectionFromAngle`
  is not used.

### Passes (Tasks 6 to 16)
- `WeatherPipeline.h/.cpp`: one function per pass taking a `PassContext` (working fields, terrain,
  tables, `CapturedInputs`, scratch). Each pass is one `executor.ParallelFor(N, kMinRange = 256, fn)`;
  each wraps itself in `DIA_PROFILE_SCOPE` named for the pass. Transport is two passes (a: outflow, b:
  gather).
- **Stable-reads rule, enforced and tested:** no pass reads from a neighbour a field that the same pass
  writes. Wind reads neighbour `pressure` and writes wind fields; 5a/6a read only local cells; 5b/6b read
  `*Out` and `dirIndex` and write own cell; orographic reads immutable `elevation`.
- Gather (5b/6b): `inflow = sum over d = 0..5 ascending of out[n]` where `n = neighbours[c*6+d]`,
  `n != kNoCell` and `dirIndex[n] == (d+3)%6`, **then** `edgeInflow(c)` added last. Fixed order, so the
  float sum is independent of range count and range order. Heat: `temperature = max(temperature - out, 0)
  + inflow`.
- Westerly band (amendment 3): pass 3 reads the two band latitudes from a `WesterlyBand {lat1, lat2}` latched
  from S-1's parameters at `Capture`, not from a local constant. The cycle wind-latitude shift applies relative
  to them. With S-1's shipped values the output is bit-identical to the old 30/60 degree constants.
- Orographic lift is scaled by a per-cell `orographicFactor` (amendment 4, from S-1's `terrain.orographic_factor`
  map by `TerrainType`), default 1.0 where unset so goldens do not move (see open item 9). Upwind neighbour `neighbours[c*6 + (dirIndex+3)%6]`; `kNoCell` means no lift; zero wind
  speed means no lift (AC 20). Condensate is capped at `moisture`, routed exactly as pass 8, and
  `cloudCover = min(moisture, 1)` after the subtraction.
- Biome-dependent reads use `BiomeIndex` and registry tags or roles only (amendment 2): the pipeline holds no
  biome constant. Fixtures come from the foundation's `BiomeTestRegistry.h`.
- Pass 8 and the orographic routing share one `RouteCondensate(cell, amount)` helper (snow below
  `snow_threshold` into `snowPack` capped 1.0, else `waterToGround`; snow above the cap to `glacierInput` on `glacierEligible` cells; amended 2026-11-17 (A5 Hydrology)) so they cannot diverge.
- Finish: clamp every field to its range, then the **serial** totals pass in cell order on the calling
  thread (`WeatherGridTotals`: `double` sums, `rainingCellCount`).
- Profile: per-pass wall time on the calling thread; CPU time summed across ranges via a relaxed
  `fetch_add` on a `uint64_t` of microseconds. Timing never feeds the simulation.

### Forcings (Tasks 17, 18)
- `WeatherForcing.h`: POD `{ ForcingKind kind; uint32_t cell; CellSetHandle cellSet; float value, value2;
  uint16_t weeksRemaining; }`, `ForcingKind { PressureHold, MoistureHold, Rain, WindOverride }`.
- `WindOverride` mapping (amendment 2): `value` = wind direction in **radians** in [0, 2pi), on-screen
  counter-clockwise, y-down world space (same convention as `windDirection`); `value2` = speed in [0, 1];
  `weeksRemaining` 0 = until cleared. Callers holding degrees convert before building the record.
- Capacity 64 records, 16 cell sets: fixed `DynamicArrayC` with capacity checks; `AddForcing` returns
  false when full, `RegisterCellSet` returns `kNoCellSet` when 16 exist, for an empty set, or for
  unsorted, duplicated or out-of-range input.
- Hooks, all inside `Run`: A after pass 2 (pressure hold), B after pass 3 (wind override replaces
  `windDirection` and `windSpeed` and **re-snaps `dirIndex`**), C after pass 4 (moisture hold), D inside
  pass 8 (rain adds to `precipitation`, routes by temperature, consumes no moisture).
- Overlap rule: lowest record index wins per cell and per kind. Implement by iterating records in index
  order and skipping a cell already claimed for that kind in a small per-kind bitset allocated at
  `Initialise` (no allocation in `Run`).
- `ClearCellSets()` removes the sets **and** every forcing that references one. `ClearForcings()` (removes
  every forcing, keeps cell sets) is part of the class API but is **added by Feature 6**, not here
  (amendment 3). Re-`Initialise` drops sets and forcings
  whose cells fall outside the new grid.

### Testing
- Test executors: `ReverseRangeExecutor` and `FixedRangeExecutor(n)` (n = 1, 3, 17, one range per cell).
  First check whether DiaDeferredStep Feature 1 ships equivalents under `DiaDeferredStep/Testing/`; use
  those if so, else write them test-local (tests ship with libraries, so a CoW-local helper goes under
  `Cluiche/CoW/Shared/Weather/Testing/`, not in GoogleTests).
- Files under `Tests/GoogleTests/CoW/Weather/`: `WindDirectionTests`, `WeatherLifecycleTests`,
  `WeatherPassTests` (one suite per pass group), `WeatherEdgeInflowTests`, `WeatherForcingTests`,
  `WeatherDeterminismTests`, `WeatherPerfTests`. Suite prefixes `WindDirection*`, `WeatherLifecycle*`,
  `WeatherPass*`, `WeatherEdge*`, `WeatherForcing*`, `WeatherDeterminism*`, `WeatherPerf*`.
- Determinism tests compare with `memcmp` after 52 weeks.
- The no-allocation check (AC 6): the test records each array's data pointer and capacity (scratch arrays,
  grid tables, cell-set storage, both field sets) before and after `Run` and asserts they are unchanged.
  No global `operator new` hook is added (amendment 5).
- Goldens (Tasks 0, 23): `weather_layers.golden.json` is **owned by Feature 6** (hand-built
  `MakeCoastRange(66, 42)`); this feature's entries use the key prefix `pipeline/`, and whichever feature
  lands first creates the file. Hashes compare bit for bit, so the build must use `/fp:precise` with no
  fast-math in both Debug and Release (amendment 6).
- Performance (AC 37) is reported as Debug ballpark numbers only; a bad ballpark queues a perf pass, it does not gate. No Release benchmarking (2026-11-17, speed-ups). The Release check moved to worldstep.plan.md Weather-stack close-out.

### Verification cadence
- Per task: narrow `dia run googletest --filter="<suite prefixes for the task>" --shards 2` (always `--shards 2`). Never run
  `WeatherPerf*` or the 52-week determinism matrix while iterating a single pass.
- The feature-wide `--filter="Weather*"` runs only in the verification task (task 27); the full suite runs once in worldstep.plan.md, Weather-stack close-out. No `SLOW_` suites here (longest run is 52 weeks). Sanitizer: on request only (user, 2026-11-17). No mutation checks. (2026-11-17, speed-ups)

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 0 | Prerequisite and Open Q3 investigation, read only: confirm the foundation names (`ParameterFile` plus `ParamBinding<T>::Snapshot` from `parameter-files.plan.md`; `BiomeRegistry`, `BiomeIndex`, tags and `BiomeTestRegistry.h` from `biome-registry.plan.md`; Feature 1's types, `Initialise` and `SeasonalTargetTemperature`); record the **names of S-1's westerly-band latitude parameters and of the `terrain.orographic_factor` map** and whether S-1 exposes them to A4 as a bound struct; confirm DiaDeferredStep Feature 1 Task 1 headers (`IParallelExecutor`, `SerialExecutor`) exist and whether a `Testing/` reverse or fixed-range executor ships; check the CoW and DiaCore projects' floating-point settings (`/fp:`) in Debug and Release; read `CellLayerGolden::HashLayer` to see whether it hashes exact bits or quantised values; confirm `HexTopology::NeighbourAt` and `kNoCell`; confirm `DIA_PROFILE_SCOPE` usage in CoW | Findings recorded in notes | Not Started | sonnet | TDD-exempt investigation. Verify `/fp:precise` with no fast-math in Debug and Release for CoW and DiaCore; if the configurations still differ, store one hash set per configuration (amendment 6, closes Open Q3). The `/fp` and `HashLayer` answers decide the golden layout (Task 23). If a Dia header is missing, stop and flag it |
| 1 | Create empty `WeatherPipeline`, `WindDirection` file pairs, a test helper `Testing/WeatherTestParams.h` (`MakeTestParams()` loads the shipped sidecar through `ParameterFile` and `Snapshot`s it, so tests never hand-fill the struct), `WeatherForcing.h`, `WeatherStepProfile.h`, `Testing/` executors folder, and the seven test files; register with `dia docs vcxproj-add CoW ...` and `dia docs vcxproj-add GoogleTests ...` | Solution builds | Not Started | haiku | Mechanical, TDD-exempt |
| 2 | RED then GREEN: test executors `ReverseRangeExecutor` and `FixedRangeExecutor(n)` (skip if Task 0 found equivalents) | Self-test: ranges cover `[0, N)` exactly once, contiguous, in the stated order; n = 1, 3, 17, N | Not Started | sonnet | Needed by every determinism task. They must implement the same `ParallelFor(count, minRange, fn)` contract as `IParallelExecutor` |
| 3 | RED then GREEN: `SnapWindDirection`, direction unit vectors, `PrevailingDirection(latitude)` with the CH-02 transition lerp | AC 21, 22, 24: non-tie angles nearest of six; east and west ties by column parity; angles `1e-3` either side of a tie snap to the nearer direction | Not Started | sonnet | Pure function. Test the tie window edge: exactly `1e-4` and just beyond |
| 4 | RED then GREEN: grid tables in `BuildGridTables()` (neighbours, unit vectors, scratch arrays) | AC 6 (allocation half, setup side): neighbour table equals `HexTopology::NeighbourAt` for every cell and direction on odd and even columns; `kNoCell` at all edges; scratch sized N | Not Started | sonnet | Re-`Initialise` rebuilds all tables for the new size. Commit |
| 5 | RED then GREEN: lifecycle with no passes: `Capture`, `Run` (empty), `Commit`, `Step`, `SetCommittedWeek`, working and committed fields | AC 1, 3, 4, 5: provider called only inside `Capture` (a provider that asserts off-thread passes); `Capture` takes an already-snapshotted `WeatherParameters` and never touches a `ParameterFile`; committed unchanged between `Capture` and `Commit` however many `Run` calls; `Commit` swaps, increments week, decrements and drops expired forcings; `Step` equals `Capture; Run; Commit` bit for bit | Not Started | sonnet | Expiry needs `AddForcing`; use the minimal version here and finish the capacity logic in Task 17. The poison test (AC 2) lands in Task 20 once passes exist |
| 6 | RED then GREEN: pass 1 Solar with the weekly reset | AC 7, 8: equator land converges to baseline at `land_heat_capacity`, water more slowly; cloud albedo reduces delta; latitude 1.0 cools by `latitude_cooling_factor` through `SeasonalTargetTemperature`; `precipitation` zero after pass 1; `cloudCover *= (1 - cloud_decay)` before use, unchanged at `cloud_decay` 0 | Not Started | sonnet | Calls Feature 1's `SeasonalTargetTemperature`; a test asserts it is that helper's value, not a re-derivation. `cloud_decay` is a Feature 1 sidecar parameter; this task only reads it from the snapshotted struct. The target is `SeasonalTargetTemperature(clamp(baseline + tempOffset[c], 0, 1), ...)` where `tempOffset` is a **per-cell field forcing** (spec-6 resolution 1: cycles stay field forcings, not modifiers) and is 0 when no cycles are latched (Feature 8 lands the non-zero path; this task only adds the zero-default read) |
| 7 | RED then GREEN: pass 2 Pressure (plus `pressureAnomaly`; amended 2026-11-17 (row 6d)) | AC 9, 9a: temperature 1.0 gives 0.2, 0.0 gives 0.8, 0.5 gives 0.5; clamped to [0, 1] (EC-07); `pressure = clamp(0.5 - (t - 0.5)*sens + pressureAnomaly)` (anomaly -0.3 at t 0.5 gives 0.2); an all-zero anomaly equals the old formula bit for bit | Not Started | sonnet | Hook A is added in Task 17. The anomaly field comes from Feature 1 (ten arrays) |
| 8 | RED then GREEN: pass 3 Wind, writing `windDirection`, `windSpeed` and `dirIndex`; export `PressureGradient(cell, fields, tables)`; `PrevailingDirection` takes an effective latitude (`latitude - windShift[c]`, shift 0 with no cycles; amended 2026-11-17 (row 6d)) | AC 10, 10a: uniform pressure gives prevailing only; high pressure pushes neighbours away, low pulls in; prevailing by latitude band with continuous lerp across both transitions, with the band latitudes taken from the latched `WesterlyBand` (a test passes non-default latitudes and the bands move; S-1's shipped values reproduce the old 30/60 output bit for bit); speed clamped [0, 1] | Not Started | sonnet | `kNoCell` neighbours contribute nothing but still count in the divide by 6. Commit tasks 6 to 8 |
| 9 | RED then GREEN: pass 4 Evaporation (primary scaled by A5 `openWater` and the cycle `evapMultiplier` (1 with no cycles; amended 2026-11-17 (row 6d)), secondary from A5 `surfaceWetness`) | AC 11: saturated water cell gains nothing (EC-04); secondary evaporation is never negative; with no A5, `waterSource` primary evaporation and zero secondary | Not Started | sonnet | Hook C is added in Task 17. Task text amended 2026-11-17 (A5 Hydrology) |
| 10 | RED then GREEN: pass 5a and 5b moisture transport with edge inflow; the same gather carries `pressureAnomaly` through `anomalyOut` (signed; amended 2026-11-17 (row 6d)) | AC 12, 12a, 20, 28 (moisture and anomaly): one cell downwind per step at the stated rate; total moisture conserved on a closed interior with `edge_inflow` off; off-map downwind neighbour keeps its moisture (EC-16); zero wind transports nothing; inflow summed in direction order then edge term, matching a reference loop on mixed magnitudes | Not Started | sonnet | Gather only, no scatter, no atomics. Name the stable-read test after the invariant |
| 10a | RED then GREEN (added 2026-11-17, row 6d): pass 5c anomaly decay | AC 12b: each week `pressureAnomaly *= pressure_anomaly_decay` (0.7) and clamps to +/-`pressure_anomaly_max` (0.5); a seeded -0.3 halves in about two weeks with no transport; a world with a zero anomaly stays zero and a run is bit-identical to one built before the anomaly existed; `kPassCount` is 14 (0 and 5c are the new entries; pass 0's body is Feature 7's task) | Not Started | sonnet | Local pass, profile entry `5c`. Range-count and reverse-order determinism are covered by Task 20 |
| 11 | RED then GREEN: pass 6a and 6b heat transport | AC 13, 28 (heat): same shape with `heat_transport_rate`; temperature never negative (CH-07); a 1.0 cell with inflow ends at 1.0 after the finish clamp | Not Started | sonnet | Finish clamp lands in Task 15; use a local clamp assertion until then and tighten the test then |
| 12 | RED then GREEN: boundary behaviour across both transports | AC 25, 26, 27: with `edge_inflow` on, uniform moisture and temperature under steady wind stay uniform on all cells over 52 weeks; off, upwind-edge cells lose moisture (numeric proof); a cell with both neighbours off-map gains and loses nothing | Not Started | sonnet | Commit tasks 9 to 12 |
| 13 | RED then GREEN: pass 7 Orographic with routing | AC 14, 15, 20: with every `orographicFactor` 1.0 the output equals the pre-factor reference bit for bit, and a factor 2.0 on one cell scales its lift; windward cell cools, condenses and rains; ring-of-mountains windward faces rain and the interior stays dry (EC-09); leeward rain below windward; downslope wind and off-map upwind never lift; condensate routed to `snowPack` below and `waterToGround` above `snow_threshold`; never exceeds `moisture`; zero wind never lifts | Not Started | sonnet | Shares `RouteCondensate` with pass 8, so write the helper here and reuse it in Task 14. Adds the optional per-cell `orographicFactor` array (default 1.0, latched at `Initialise` or set by the caller from S-1's `TerrainType` map; open item 9) |
| 14 | RED then GREEN: pass 8 Precipitation | AC 16: rain and snow split at `snow_threshold`; zero moisture gives zero and never negative (EC-11, CH-09); snow pack caps at 1.0 (CH-04); excess snow becomes `glacierInput` on `glacierEligible` cells only and is discarded elsewhere (EC-12 deleted with the store); rain lands in `waterToGround`; `cloudCover = moisture * 0.8` after subtraction | Not Started | sonnet | Hook D is added in Task 17. Task text amended 2026-11-17 (A5 Hydrology) |
| 15 | RED then GREEN: pass 9 Snow melt **and firn hand-off** (GDD §11.3; amended 2026-11-17 (row 6d)), Finish clamp (`glacierInput` [0, 2], `pressureAnomaly` +/-0.5) and the serial totals pass (pass 10 ground-water drain retired) | AC 17, 18, 18a, 19, 32 (reference half): firn moves `max(snowPack - glacierSnowFloor, 0)` into `glacierInput` at `weekOfYear == glacierCheckWeek` on eligible cells only, constants from `HydrologyInputs`, never with no A5; no melt below threshold; melt with zero snow is a no-op (EC-13); melt moves snow to `waterToGround` and cools; a cell below `melt_threshold` all year keeps snow at 1.0 (EC-06); `waterToGround` equals rain plus melt, `glacierInput` only on eligible cells, no-A5 fallback (AC 18); every field in range after 52 weeks of extreme temperatures (EC-03, EC-20); totals equal a cell-order reference reduction | Not Started | sonnet | The totals pass is serial on the calling thread. Commit tasks 13 to 15. Task text amended 2026-11-17 (A5 Hydrology) (drain test moved to A5) |
| 16 | RED then GREEN: zigzag integration test and the stable-reads proof on the assembled pipeline | AC 23, 31 (first assertion): trade winds move a pulse due east over 12 weeks with at most 1 row of drift, westerlies the same going west; a reverse-order executor gives the same bytes after one step | Not Started | sonnet | If the reverse-order test fails, a pass reads a neighbour field it writes: fix the pass, do not relax the test |
| 17 | RED then GREEN: forcings with hooks A to D, capacity and expiry | AC 34, 35, 36: `PressureHold` survives pass 2 and shapes wind; `MoistureHold` visible to transport; `WindOverride` (`value` radians in [0, 2pi), `value2` speed) replaces pass 3 output and `dirIndex`; `Rain` adds exactly `value`, routes by temperature, consumes no moisture; expiry after `weeksRemaining`, wind overrides included (0 never expires, 3 expires after 3 `Commit`s); `AddForcing` false at 64; application order does not change results | Not Started | sonnet | Wind overrides honour `weeksRemaining` (0 = until cleared); amendments 1 and 2. Commit |
| 18 | RED then GREEN: region forcings: `RegisterCellSet`, `ClearCellSets`, `kNoCellSet`, lowest-record-wins overlap | AC 36a: region applies to every cell in the set and no other; empty set rejected; 17th set returns `kNoCellSet`; unsorted, duplicated or out-of-range list rejected; overlap resolves to the lower record index; region and per-cell forms give identical output; `ClearCellSets` drops dependent forcings; re-`Initialise` drops sets beyond the new grid | Not Started | sonnet | Sets are latched at `Capture` and read-only in `Run`. Commit |
| 19 | RED then GREEN: purity | AC 2, 6: after `Capture`, poison the caller's parameters, forcing list, biome edit queue and baseline provider; result equals an unpoisoned run; `Run` performs no heap allocation, shown by every array's data pointer and capacity being unchanged across `Run` | Not Started | sonnet | Pointer/capacity mechanism (amendment 5), no operator-new hook. A grep of `Run`-reachable members against `CapturedInputs` is part of this task; record the list in notes |
| 20 | RED then GREEN: determinism matrix | AC 29, 30, 31, 32: same inputs twice are identical after 52 weeks; `SerialExecutor`, 1, 3, 17 and one-range-per-cell executors, and reverse order, give identical fields, totals and week counter at Small and at a hand-built Large terrain | Not Started | sonnet | If any pair differs, stop: it is a real ordering bug, not a tolerance to widen. Run the 52-week Large cases once at the end of the task, not per edit |
| 21 | RED then GREEN: deferred-vs-synchronous adaptor | AC 33: a test-local adaptor driving `Capture`, `Run`, `Commit` through DiaDeferredStep's manual launcher with `Capture` at key N and `Apply` at N+k matches `Step` per week bit for bit over 52 weeks | Not Started | sonnet | **Blocked** until DiaDeferredStep's manual launcher is built. Mark Blocked with `dia docs plan` and continue; Feature 3 re-runs this through the module |
| 22 | Profile and performance: `WeatherStepProfile` (14 entries: 0, 1, 2, 3, 4, 5a, 5b, 5c, 6a, 6b, 7, 8, 9, finish; amended 2026-11-17 (A5 Hydrology and row 6d), was 12), per-pass wall and CPU, `WeatherPerfTests` reporting; Debug ballpark numbers only (`cow.weather.step_ms` itself is registered by the stage base, not here) | AC 37: per-pass breakdown printed; budgets 10 ms Large, 6.3 Standard, 4.2 Small reported against Debug ballpark; never a hard failure | Not Started | sonnet | A miss is reported in notes, not failed. If missed, fusing local passes is the fallback, not the first design (spec decision). Report numbers; do not tune in this task. Release check moved to worldstep.plan.md Weather-stack close-out (2026-11-17). A bad ballpark queues a perf pass (2026-11-17, speed-ups) |
| 23 | Golden registration: create-or-extend `Tests/GoogleTests/CoW/Golden/weather_layers.golden.json` with `pipeline/` entries through `CellLayerGolden` | AC 38: all 10 fields (amended 2026-11-17, row 6d; was 9) for the hand-built terrain after a fixed week count match the golden | Not Started | sonnet | Uses `GoldenStore::Check` and `DIA_UPDATE_GOLDEN=1` for the first write only. Entries use key prefix `pipeline/`; if the file already exists (Feature 6 owns it) add entries only, else create it. Bit-exact compare; per-configuration hash sets only if Task 0 shows Debug and Release still differ under `/fp:precise`. Commit |
| 24 | Update `a4-weather.md` as the spec's Files Touched requires: Public Interfaces (`Step`, `Capture`/`Run`/`Commit`), the "stable copy" sentence, the "reduced per range" sentence, GDD deviation list 15 to 19. Do not restate any parameter count (the table is a sidecar now) | Doc diff reviewed against the Ca-pipeline spec list | Not Started | haiku | Doc edit only, not code, and the one task that edits a spec: ask the user first, since the parent spec was amended on 2026-11-18 |
| 25 | Exhaustive test pass: gap audit of AC 1 to 38 (including 36a), then edge cases | Gap list in notes. Edge cases: N = 1 and N smaller than `kMinRange`; width 1 and height 1 grids; a forcing on the last cell; a cell set containing every cell; `Run` twice on one `Capture`; `Commit` without `Capture`; `Capture` twice without `Commit`; week counter overwrite with `SetCommittedWeek`; seasons crossing 52 weeks; zero-speed wind everywhere for 52 weeks | Not Started | sonnet | No mutation check. Add only the tests the audit shows missing |
| 26 | Observation Opportunity Scan over every file touched across logs, traces, profiling, metrics, health | Suggestions in notes | Not Started | haiku | Non-blocking. `DIA_PROFILE_SCOPE` per pass is in scope and should already exist; note `cow.weather.step_ms` metric as Feature 5's |
| 27 | Scoped verification: `dia run googletest --filter="Weather*" --shards 2`; `dia check deps`; `dia docs precommit`. No Release run (moved) | All AC 1 to 38 quoted; pass/fail and failing names only | Not Started | haiku | Feature-wide scoped run only; full suite and Release run are in worldstep.plan.md Weather-stack close-out (2026-11-17, speed-ups). AC 33 reported as Blocked if Task 21 still is |
| 28 | Close-out: `dia docs plan` rows to Done with notes, then `dia docs spec-done docs/specs/applications/cow/systems/a4-weather/ca-pipeline.md` | Spec status Done, plan header updated | Not Started | haiku | **Do not run until every row above is Done.** Not to be run when this plan is written |

**AC coverage.** AC 1 to 5: Task 5 (plus Task 19 for AC 2). AC 6: Tasks 4, 19. AC 7, 8: Task 6. AC 9, 9a: Task 7. AC 10, 10a: Task 8. AC 11: Task 9. AC 12, 12a, 20, 28: Task 10. AC 12b: Task 10a. AC 13: Task 11. AC 14, 15: Task 13. AC 16: Task 14. AC 17, 18, 18a, 19, 32 (reference half): Task 15. AC 21, 22, 24: Task 3. AC 23, 31: Task 16. AC 25 to 27: Task 12. AC 29 to 32: Task 20. AC 33: Task 21. AC 34 to 36a: Tasks 17, 18. AC 37: Task 22. AC 38: Task 23. Task count unchanged (30 rows: 0 to 28 plus 10a); the amendment changes the text of Tasks 0, 1, 5, 6, 8, 13, 22 and 24 only. The amendment adds no new AC; the westerly-band and `orographicFactor` checks are folded into the AC 10 and AC 14 tasks.

## Open items

1. **`cloud_decay` ownership.** Resolved by amendment 4 (2026-11-16): Feature 1 owns it; this feature only reads it.
2. **How AC 6 (no heap allocation in `Run`) is measured.** Resolved by amendment 5 (2026-11-16): data pointer and capacity of every array unchanged across `Run`, plus a code rule; no global `operator new` hook.
3. **`weeksRemaining` for wind overrides.** Resolved by amendment 1 (2026-11-16): 0 = until cleared, positive expires; honoured by `Commit`.
4. **Mapping of angle and speed to `value` and `value2`.** Resolved by amendment 2 (2026-11-16): `value` radians in [0, 2pi), `value2` speed in [0, 1].
5. **`ClearForcings()`.** Resolved by amendment 3 (2026-11-16): part of the API, added by Feature 6; `ClearCellSets()` also drops dependent forcings.
6. **Golden hash exactness and Debug/Release (spec Open Q3).** Resolved by amendment 6 (2026-11-16): `/fp:precise`, no fast-math in both configurations; Task 0 verifies and stores per-configuration hashes only if they still differ.
7. **Size of "Small" and "Large" test terrains** for Tasks 20 and 22 is not given here (Large is 6,656 cells per Feature 6). Use the dimensions defined by S-1's size presets; confirm if a hand-built Large terrain is wanted at those dimensions. Still open.
8. **Test file names are tentative** in the spec ("until Feature 1 fixes the folder layout"). This plan follows Feature 1's layout. Still open (low impact).
- **Review 2026-11-16:** Test terrains are hand-built at S-1 dimensions (Small 66x42, Large 104x64); file names follow Feature 1 (decisions 13, 18).
9. **How A4 receives S-1's westerly band and `terrain.orographic_factor`.** The amendment says A4 "reads" them from S-1's parameters but gives no parameter names, no accessor, and `WeatherTerrain` (Feature 1) carries no `TerrainType` or factor array. This plan latches a `WesterlyBand` and an optional per-cell `orographicFactor` (default 1.0) at `Capture`/`Initialise`, supplied by the caller. Whether S-1 exposes a bound struct or per-cell values is a spec gap (Task 0 records what exists).
10. **Cycle effects on parameters versus fields.** Amendment item 1 says "cycle modifiers are already resolved" in the snapshot; the spec-6 resolutions say cycles are per-cell field forcings and modifiers are only for scalar parameters. This plan follows the resolutions: the passes read `tempOffset`, `evapMultiplier` and `windShift` as per-cell fields, and the snapshot contains modifier-resolved scalars only. Amendment item 1's sentence should be re-worded in the spec.
11. **Golden hash layout after the world-sim kit.** `weather_layers.golden.json` keys use layer ids `weather.<snake_case>`; the kit's naming test may reject `pipeline/` prefixes. Confirm when Task 23 starts.
12. **The 14-entry profile, 10 arrays and the 30-row count are unchanged** by the amendment; no stale count needed recounting in this plan other than the baseline array size (13 became the registry's `Count()`).

> **Re-plan required (Draoi amendment, 2026-11-21):** file paths, package split and test locations changed; see the spec's Draoi layer amendment section. Do not build from this plan as written.
