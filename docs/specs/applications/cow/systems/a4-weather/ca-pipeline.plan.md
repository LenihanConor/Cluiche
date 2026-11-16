# Plan: A4 CA Pipeline (A4 Weather Feature 2)

**Spec:** @docs/specs/applications/cow/systems/a4-weather/ca-pipeline.md
**Status:** Not Started
**System plan:** @docs/specs/applications/cow/systems/a4-weather/a4-weather.plan.md (row 2)

**Starts after A4 Feature 1 (`weather-data.plan.md`) tasks 1 to 14 are Done:** it extends Feature 1's
`WeatherSimulation`, `WeatherParameters` (which already carries `cloud_decay` as its 21st constant),
`IBiomeSeasonalBaseline`, `SeasonalTargetTemperature` and the empty `BuildGridTables()` seam. It also
needs DiaDeferredStep Feature 1 **Task 1** Done (`IParallelExecutor`, `SerialExecutor` headers). Task 21
(deferred-vs-synchronous) additionally needs DiaDeferredStep's manual launcher; if that is not built yet,
that one task stays Blocked and the rest of the plan proceeds.

`presets-and-regression.plan.md` needs this plan built (its forcing, region-forcing and every regression
task use `Step`, `AddForcing` and `RegisterCellSet`).

All code is CoW (application) under `Cluiche/CoW/Shared/Weather/`. No Dia edits (AD-001): `IParallelExecutor`
and `SerialExecutor` are consumed, not changed. Where a task could need more than sonnet it says "ask
before escalating" (feedback_confirm_opus).

## Implementation Patterns

### Lifecycle (Task 5)
- `WeatherSimulation` (Feature 1 class) gains `Capture(Season, const IBiomeSeasonalBaseline&)`,
  `Run(Dia::DeferredStep::IParallelExecutor&)`, `Commit()`, `Step(Season, baseline, executor)` (exactly
  `Capture; Run; Commit`), `GetGridTotals()`, `GetLastProfile()`, `SetCommittedWeek(uint64_t)`.
- Two `WeatherFields` instances (committed, working) plus a **latched** struct `CapturedInputs`
  (`WeatherParameters` copy, `Season`, `float baseline[13]` for the captured season, forcing array copy,
  cell-set copy, working biome array copy). `Run` takes **only** these, the immutable terrain and the grid
  tables. The baseline provider is called once, 13 times, inside `Capture` and never in `Run` (DFS-002).
- `Commit` swaps working into committed (swap, not copy), increments the week, decrements
  `weeksRemaining` and drops expired records. Wind overrides carry no timer.
- No `Run` method reads a member that `Capture` did not latch. Reviewing this is a `grep` task in Task 20.

### Grid tables (Task 4)
- Built in Feature 1's empty `BuildGridTables()`: `DynamicArrayC<uint32_t> neighbours` of `N*6` from
  `Dia::Pathfinding::HexTopology::NeighbourAt`, `kNoCell` past the edge; `static constexpr` unit vectors
  for `d = 0..5` at on-screen angle `30 deg + 60 deg * d`, counter-clockwise, y-down; scratch arrays
  `moistureOut`, `heatOut` (float) and `dirIndex` (`uint8_t`) allocated here and reused.
- **`Run` does no heap allocation.** Every per-pass temporary is one of these scratch arrays or a stack
  value.

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
- Orographic: upwind neighbour `neighbours[c*6 + (dirIndex+3)%6]`; `kNoCell` means no lift; zero wind
  speed means no lift (AC 20). Condensate is capped at `moisture`, routed exactly as pass 8, and
  `cloudCover = min(moisture, 1)` after the subtraction.
- Pass 8 and the orographic routing share one `RouteCondensate(cell, amount)` helper (snow below
  `snow_threshold` into `snowPack` capped 1.0, else `groundWater` capped 1.0) so they cannot diverge.
- Finish: clamp every field to its range, then the **serial** totals pass in cell order on the calling
  thread (`WeatherGridTotals`: `double` sums, `rainingCellCount`).
- Profile: per-pass wall time on the calling thread; CPU time summed across ranges via a relaxed
  `fetch_add` on a `uint64_t` of microseconds. Timing never feeds the simulation.

### Forcings (Tasks 17, 18)
- `WeatherForcing.h`: POD `{ ForcingKind kind; uint32_t cell; CellSetHandle cellSet; float value, value2;
  uint16_t weeksRemaining; }`, `ForcingKind { PressureHold, MoistureHold, Rain, WindOverride }`.
- Capacity 64 records, 16 cell sets: fixed `DynamicArrayC` with capacity checks; `AddForcing` returns
  false when full, `RegisterCellSet` returns `kNoCellSet` when 16 exist, for an empty set, or for
  unsorted, duplicated or out-of-range input.
- Hooks, all inside `Run`: A after pass 2 (pressure hold), B after pass 3 (wind override replaces
  `windDirection` and `windSpeed` and **re-snaps `dirIndex`**), C after pass 4 (moisture hold), D inside
  pass 8 (rain adds to `precipitation`, routes by temperature, consumes no moisture).
- Overlap rule: lowest record index wins per cell and per kind. Implement by iterating records in index
  order and skipping a cell already claimed for that kind in a small per-kind bitset allocated at
  `Initialise` (no allocation in `Run`).
- `ClearCellSets()` also drops forcings that reference a set. Re-`Initialise` drops sets and forcings
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
- The no-allocation check (AC 6) needs an allocation counter; see Open items.
- Performance (AC 37) is reported in Release, hard-fails only above 3x budget. Never gated otherwise.

### Verification cadence
- Per task: `dia run googletest --filter="<suite prefixes for the task>" --shards 2`. Never run
  `WeatherPerf*` or the 52-week determinism matrix while iterating a single pass.
- One full `dia run googletest --shards 2` in the verification task. No mutation checks.

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 0 | Prerequisite and Open Q3 investigation, read only: confirm DiaDeferredStep Feature 1 Task 1 headers (`IParallelExecutor`, `SerialExecutor`) exist and whether a `Testing/` reverse or fixed-range executor ships; check the CoW and DiaCore projects' floating-point settings (`/fp:`) in Debug and Release; read `CellLayerGolden::HashLayer` to see whether it hashes exact bits or quantised values; confirm `HexTopology::NeighbourAt` and `kNoCell`; confirm `DIA_PROFILE_SCOPE` usage in CoW | Findings recorded in notes | Not Started | sonnet | TDD-exempt investigation. The `/fp` and `HashLayer` answers decide whether goldens are per-configuration (Task 22). If a Dia header is missing, stop and flag it |
| 1 | Create empty `WeatherPipeline`, `WindDirection` file pairs, `WeatherForcing.h`, `WeatherStepProfile.h`, `Testing/` executors folder, and the seven test files; register with `dia docs vcxproj-add CoW ...` and `dia docs vcxproj-add GoogleTests ...` | Solution builds | Not Started | haiku | Mechanical, TDD-exempt |
| 2 | RED then GREEN: test executors `ReverseRangeExecutor` and `FixedRangeExecutor(n)` (skip if Task 0 found equivalents) | Self-test: ranges cover `[0, N)` exactly once, contiguous, in the stated order; n = 1, 3, 17, N | Not Started | sonnet | Needed by every determinism task. They must implement the same `ParallelFor(count, minRange, fn)` contract as `IParallelExecutor` |
| 3 | RED then GREEN: `SnapWindDirection`, direction unit vectors, `PrevailingDirection(latitude)` with the CH-02 transition lerp | AC 21, 22, 24: non-tie angles nearest of six; east and west ties by column parity; angles `1e-3` either side of a tie snap to the nearer direction | Not Started | sonnet | Pure function. Test the tie window edge: exactly `1e-4` and just beyond |
| 4 | RED then GREEN: grid tables in `BuildGridTables()` (neighbours, unit vectors, scratch arrays) | AC 6 (allocation half, setup side): neighbour table equals `HexTopology::NeighbourAt` for every cell and direction on odd and even columns; `kNoCell` at all edges; scratch sized N | Not Started | sonnet | Re-`Initialise` rebuilds all tables for the new size. Commit |
| 5 | RED then GREEN: lifecycle with no passes: `Capture`, `Run` (empty), `Commit`, `Step`, `SetCommittedWeek`, working and committed fields | AC 1, 3, 4, 5: provider called only inside `Capture` (a provider that asserts off-thread passes); committed unchanged between `Capture` and `Commit` however many `Run` calls; `Commit` swaps, increments week, decrements and drops expired forcings; `Step` equals `Capture; Run; Commit` bit for bit | Not Started | sonnet | Expiry needs `AddForcing`; use the minimal version here and finish the capacity logic in Task 17. The poison test (AC 2) lands in Task 20 once passes exist |
| 6 | RED then GREEN: pass 1 Solar with the weekly reset | AC 7, 8: equator land converges to baseline at `land_heat_capacity`, water more slowly; cloud albedo reduces delta; latitude 1.0 cools by `latitude_cooling_factor` through `SeasonalTargetTemperature`; `precipitation` zero after pass 1; `cloudCover *= (1 - cloud_decay)` before use, unchanged at `cloud_decay` 0 | Not Started | sonnet | Calls Feature 1's `SeasonalTargetTemperature`; a test asserts it is that helper's value, not a re-derivation. `cloud_decay` already exists from Feature 1 |
| 7 | RED then GREEN: pass 2 Pressure | AC 9: temperature 1.0 gives 0.2, 0.0 gives 0.8, 0.5 gives 0.5; clamped to [0, 1] (EC-07) | Not Started | sonnet | Hook A is added in Task 17 |
| 8 | RED then GREEN: pass 3 Wind, writing `windDirection`, `windSpeed` and `dirIndex`; export `PressureGradient(cell, fields, tables)` | AC 10: uniform pressure gives prevailing only; high pressure pushes neighbours away, low pulls in; prevailing by latitude band with continuous lerp across both transitions; speed clamped [0, 1] | Not Started | sonnet | `kNoCell` neighbours contribute nothing but still count in the divide by 6. Commit tasks 6 to 8 |
| 9 | RED then GREEN: pass 4 Evaporation (water and ground water) | AC 11: saturated water cell gains nothing (EC-04); ground water moves into moisture and is never negative | Not Started | sonnet | Hook C is added in Task 17 |
| 10 | RED then GREEN: pass 5a and 5b moisture transport with edge inflow | AC 12, 20, 28 (moisture): one cell downwind per step at the stated rate; total moisture conserved on a closed interior with `edge_inflow` off; off-map downwind neighbour keeps its moisture (EC-16); zero wind transports nothing; inflow summed in direction order then edge term, matching a reference loop on mixed magnitudes | Not Started | sonnet | Gather only, no scatter, no atomics. Name the stable-read test after the invariant |
| 11 | RED then GREEN: pass 6a and 6b heat transport | AC 13, 28 (heat): same shape with `heat_transport_rate`; temperature never negative (CH-07); a 1.0 cell with inflow ends at 1.0 after the finish clamp | Not Started | sonnet | Finish clamp lands in Task 15; use a local clamp assertion until then and tighten the test then |
| 12 | RED then GREEN: boundary behaviour across both transports | AC 25, 26, 27: with `edge_inflow` on, uniform moisture and temperature under steady wind stay uniform on all cells over 52 weeks; off, upwind-edge cells lose moisture (numeric proof); a cell with both neighbours off-map gains and loses nothing | Not Started | sonnet | Commit tasks 9 to 12 |
| 13 | RED then GREEN: pass 7 Orographic with routing | AC 14, 15, 20: windward cell cools, condenses and rains; ring-of-mountains windward faces rain and the interior stays dry (EC-09); leeward rain below windward; downslope wind and off-map upwind never lift; condensate routed to `snowPack` below and `groundWater` above `snow_threshold`; never exceeds `moisture`; zero wind never lifts | Not Started | sonnet | Shares `RouteCondensate` with pass 8, so write the helper here and reuse it in Task 14 |
| 14 | RED then GREEN: pass 8 Precipitation | AC 16: rain and snow split at `snow_threshold`; zero moisture gives zero and never negative (EC-11, CH-09); snow pack caps at 1.0 (CH-04); ground water excess discarded (EC-12); `cloudCover = moisture * 0.8` after subtraction | Not Started | sonnet | Hook D is added in Task 17 |
| 15 | RED then GREEN: pass 9 Snow melt, pass 10 Ground water drain, Finish clamp and the serial totals pass | AC 17, 18, 19, 32 (reference half): no melt below threshold; melt with zero snow is a no-op (EC-13); melt moves snow to ground water and cools; a cell below `melt_threshold` all year keeps snow at 1.0 (EC-06); ground water about 0.33 of start after 5 dry weeks; every field in range after 52 weeks of extreme temperatures (EC-03, EC-20); totals equal a cell-order reference reduction | Not Started | sonnet | The totals pass is serial on the calling thread. Commit tasks 13 to 15 |
| 16 | RED then GREEN: zigzag integration test and the stable-reads proof on the assembled pipeline | AC 23, 31 (first assertion): trade winds move a pulse due east over 12 weeks with at most 1 row of drift, westerlies the same going west; a reverse-order executor gives the same bytes after one step | Not Started | sonnet | If the reverse-order test fails, a pass reads a neighbour field it writes: fix the pass, do not relax the test |
| 17 | RED then GREEN: forcings with hooks A to D, capacity and expiry | AC 34, 35, 36: `PressureHold` survives pass 2 and shapes wind; `MoistureHold` visible to transport; `WindOverride` replaces pass 3 output and `dirIndex`; `Rain` adds exactly `value`, routes by temperature, consumes no moisture; expiry after `weeksRemaining`; `AddForcing` false at 64; application order does not change results | Not Started | sonnet | Wind overrides carry no timer; see Open items on `weeks` for presets. Commit |
| 18 | RED then GREEN: region forcings: `RegisterCellSet`, `ClearCellSets`, `kNoCellSet`, lowest-record-wins overlap | AC 36a: region applies to every cell in the set and no other; empty set rejected; 17th set returns `kNoCellSet`; unsorted, duplicated or out-of-range list rejected; overlap resolves to the lower record index; region and per-cell forms give identical output; `ClearCellSets` drops dependent forcings; re-`Initialise` drops sets beyond the new grid | Not Started | sonnet | Sets are latched at `Capture` and read-only in `Run`. Commit |
| 19 | RED then GREEN: purity | AC 2, 6: after `Capture`, poison the caller's parameters, forcing list, biome edit queue and baseline provider; result equals an unpoisoned run; `Run` performs no heap allocation | Not Started | sonnet | Allocation counter approach is Open item 2. A grep of `Run`-reachable members against `CapturedInputs` is part of this task; record the list in notes |
| 20 | RED then GREEN: determinism matrix | AC 29, 30, 31, 32: same inputs twice are identical after 52 weeks; `SerialExecutor`, 1, 3, 17 and one-range-per-cell executors, and reverse order, give identical fields, totals and week counter at Small and at a hand-built Large terrain | Not Started | sonnet | If any pair differs, stop: it is a real ordering bug, not a tolerance to widen. Run the 52-week Large cases once at the end of the task, not per edit |
| 21 | RED then GREEN: deferred-vs-synchronous adaptor | AC 33: a test-local adaptor driving `Capture`, `Run`, `Commit` through DiaDeferredStep's manual launcher with `Capture` at key N and `Apply` at N+k matches `Step` per week bit for bit over 52 weeks | Not Started | sonnet | **Blocked** until DiaDeferredStep's manual launcher is built. Mark Blocked with `dia docs plan` and continue; Feature 3 re-runs this through the module |
| 22 | Profile and performance: `WeatherStepProfile` (13 passes), per-pass wall and CPU, `WeatherPerfTests` reporting; run in Release | AC 37: per-pass breakdown printed; budgets 10 ms Large, 6.3 Standard, 4.2 Small reported; hard failure only above 3x budget | Not Started | sonnet | A miss is reported in notes, not failed. If missed, fusing local passes is the fallback, not the first design (spec decision). Report numbers; do not tune in this task |
| 23 | Golden registration: create `Tests/GoogleTests/CoW/Golden/weather_layers.golden.json` and the first entry through `CellLayerGolden` | AC 38: all 9 fields for the hand-built terrain after a fixed week count match the golden | Not Started | sonnet | Uses `GoldenStore::Check` and `DIA_UPDATE_GOLDEN=1` for the first write only. Per-configuration if Task 0 found exact-bit hashing with differing `/fp` settings. Commit |
| 24 | Update `a4-weather.md` as the spec's Files Touched requires: Public Interfaces (`Step`, `Capture`/`Run`/`Commit`), the "stable copy" sentence, the "reduced per range" sentence, "20 constants" to 21, GDD deviation list 15 to 19 | Doc diff reviewed against the Ca-pipeline spec list | Not Started | haiku | Doc edit only, not code. Confirm with the user first if the parent spec has changed since 2026-11-16 |
| 25 | Exhaustive test pass: gap audit of AC 1 to 38 (including 36a), then edge cases | Gap list in notes. Edge cases: N = 1 and N smaller than `kMinRange`; width 1 and height 1 grids; a forcing on the last cell; a cell set containing every cell; `Run` twice on one `Capture`; `Commit` without `Capture`; `Capture` twice without `Commit`; week counter overwrite with `SetCommittedWeek`; seasons crossing 52 weeks; zero-speed wind everywhere for 52 weeks | Not Started | sonnet | No mutation check. Add only the tests the audit shows missing |
| 26 | Observation Opportunity Scan over every file touched across logs, traces, profiling, metrics, health | Suggestions in notes | Not Started | haiku | Non-blocking. `DIA_PROFILE_SCOPE` per pass is in scope and should already exist; note `cow.weather.step_ms` metric as Feature 5's |
| 27 | One full verification: `dia run googletest --shards 2`; `dia check deps`; `dia docs precommit`; Release run of `WeatherPerf*` reported | All AC 1 to 38 quoted; pass/fail and failing names only | Not Started | haiku | One full run. AC 33 reported as Blocked if Task 21 still is |
| 28 | Close-out: `dia docs plan` rows to Done with notes, then `dia docs spec-done docs/specs/applications/cow/systems/a4-weather/ca-pipeline.md` | Spec status Done, plan header updated | Not Started | haiku | **Do not run until every row above is Done.** Not to be run when this plan is written |

## Open items

1. **`cloud_decay` ownership.** This spec says it "adds" `cloud_decay` as the 21st constant. Feature 1's spec already includes it in the descriptor table and in AC 3 and 4. This plan treats Feature 1 as the owner and adds no constant here. Confirm.
2. **How AC 6 (no heap allocation in `Run`) is measured.** The spec states the requirement but not the mechanism. Options: a global `operator new` counter active in tests only, or a Dia allocator hook if one exists. A global `operator new` replacement in a test binary may be an engine-level concern; if a Dia hook is needed this is an AD-001 ask-first item.
3. **`weeksRemaining` for wind overrides.** This spec says wind overrides "carry no timer" and persist until `clear_wind_overrides`. Feature 6's `coastal_storm` and `drought` presets use a `WindOverride` with 6 and 8 weeks. Decide whether `weeksRemaining` on a wind override is honoured when non-zero (Feature 6's `weeks: 0` means until cleared). Task 17 implements the spec as written (no timer) unless told otherwise.
4. **Mapping of `angleDegrees` and `speed` to `value` and `value2`** for `WindOverride` is not stated. This plan assumes `value` is the angle in radians and `value2` the speed; confirm.
5. **Feature 6 needs `ClearForcings()`** (clear forcings, keeping or clearing cell sets). This spec lists only `ClearCellSets()` (which also drops forcings). Not added here; Feature 6 adds it.
6. **Golden hash exactness and Debug/Release (spec Open Q3)** is answered by Task 0 but the decision (per-configuration golden or Debug-only) is the user's.
7. **Size of "Small" and "Large" test terrains** for Tasks 20 and 22 is not given here (Large is 6,656 cells per Feature 6). Use the dimensions defined by S-1's size presets; confirm if a hand-built Large terrain is wanted at those dimensions.
8. **Test file names are tentative** in the spec ("until Feature 1 fixes the folder layout"). This plan follows Feature 1's layout.
