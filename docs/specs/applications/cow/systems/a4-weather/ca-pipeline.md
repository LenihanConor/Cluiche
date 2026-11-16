# Feature Spec: A4 CA Pipeline

**Parent:** @docs/specs/applications/cow/systems/a4-weather/a4-weather.md (Feature 2)
**Status:** Approved (2026-11-16)
**Design source:** `DesignDocs/COW/Output Docs/GDD/08_SystemsDeepDive/A4_Weather/A4_TechnicalSpec.md` §2–§12, `A4_EdgeCaseMatrix.md`
**Depends on:** A4 Feature 1 (`WeatherFields`, `WeatherTerrain`, `WeatherParameters`, `IBiomeSeasonalBaseline`, `Initialise`); DiaDeferredStep Feature 1 Task 1 (`IParallelExecutor`, `SerialExecutor` headers)

## Summary

The weekly step of the weather cellular automaton, as pure logic. It runs the GDD's 10 pipeline steps over every cell of the hex grid as full-grid passes on a working copy of the weather state. It is deterministic and bit-identical at any `ParallelFor` range count and in any range order. It does not know about threads, the scheduler, the bus or the save system.

The feature adds three things to `WeatherSimulation` (Feature 1 creates the class):

- a `Capture` / `Run` / `Commit` split, which is the shape DiaDeferredStep's `IDeferredStep` needs (Feature 3 adapts it)
- `Step`, the synchronous composition of the three
- the forcing and wind-override inputs the debug commands use (Feature 5 only issues them)

## Goals

1. The 10 GDD steps run in GDD order, one `ParallelFor` pass per step (transport is two passes), over a working set. Readers see only committed state.
2. **Bit-identical results** for the same inputs and tick count at any range count (1, 3, 17, one range per cell) and with ranges executed in reverse order. No RNG, no atomics in anything that feeds the simulation, no dependence on iteration order.
3. Transport is a **gather** with a defined upwind-edge inflow (`boundary.edge_inflow`, default on).
4. `Run` is a pure function of what `Capture` latched. It reads no SimPU-owned state (DFS-002).
5. One step at Large costs ≤ 10 ms CPU summed across ranges, with Small ≤ 4.2 ms and Standard ≤ 6.3 ms. It is reported and has a hard ceiling (see AC 37).

## Non-Goals

- `WeatherStep : IDeferredStep`, scheduling, apply keys, `WeatherUpdated` publication: **Feature 3**.
- Serialising committed or pending state: **Feature 4**.
- The `cow.weather.*` commands, debug layers and panel: **Feature 5**. This feature supplies the forcing records they issue and the grid totals they display.
- Presets and the four §6 scenarios: **Feature 6**. They use this pipeline unchanged.
- Parameter file loading and validation: **Feature 1** (this feature adds one constant, `cloud_decay`, to the struct).
- Initial seeding and spin-up orchestration: Features 1 and 3. Spin-up is just 52 calls to `Step`.

## Design

### Lifecycle on `WeatherSimulation`

```
Capture(season, baselineProvider)   SimPU      copy committed → working; latch season, parameters,
                                               forcings, biome edits, and the 13-entry seasonal
                                               baseline table (provider is called here and never in Run)
Run(executor)                       workers    the pipeline over the working set only
Commit()                            SimPU      swap working → committed; week++; decrement forcing timers
Step(season, baseline, executor)               Capture; Run; Commit — the synchronous path (WeatherRunner)
```

The system spec's Public Interfaces lists only `Step(Season, IParallelExecutor&)`. It needs the three methods, and `Step` needs the baseline provider, because a virtual call into A1's provider from a worker would break DFS-002. This is a spec-sync amendment (see Files Touched).

`Run` reads: the working fields, the working biome array, the immutable terrain (elevation, latitude, water source: owned by `WeatherSimulation`, never mutated after `Initialise`), the latched parameters, season, baseline table, forcings and the precomputed neighbour table. Nothing else.

### Grid tables (built once at `Initialise`)

- `uint32_t neighbours[N * 6]`, from `Dia::Pathfinding::HexTopology::NeighbourAt`, `kNoCell` past the edge. Passes index this table; they never call the topology.
- `static constexpr` unit vectors for directions `d = 0..5`, at on-screen angle `30° + 60°·d`, counter-clockwise, y-down world space. This is the angle convention for every wind angle in the system, including the `wind_direction` V-1 reads.
- Scratch arrays, allocated at `Initialise` and reused: `moistureOut`, `heatOut` (float), `dirIndex` (`uint8_t`, the snapped downwind direction). **`Run` does no heap allocation.**

### Stable reads, by construction

The system spec says neighbour-reading steps read "a stable copy". The rule this feature enforces, and tests, is:

> **No pass reads a field from a neighbour that the same pass writes.**

SoA lets every neighbour-reading pass satisfy this without copying whole fields:

| Pass | Reads from neighbours | Writes | Why it is safe |
|---|---|---|---|
| 3 Wind | `pressure` | `windDirection`, `windSpeed`, `dirIndex` | pressure is not written in pass 3 |
| 5a / 6a Outflow | nothing | `moistureOut` / `heatOut` | local |
| 5b / 6b Gather | `*Out`, `dirIndex` | own `moisture` / `temperature` | the `*Out` scratch isn't written in pass b |
| 7 Orographic | `elevation` (immutable terrain) | own `temperature`, `moisture`, … | elevation never changes |

This replaces the system spec's "stable copy" wording, which implied snapshotting fields. It gives the same guarantee for two scratch arrays instead of nine copies. A reverse-order test (AC 31) is what proves the rule holds.

### The pipeline

Each row is one pass (`DIA_PROFILE_SCOPE` per pass; backs the `profile` command). "Local" passes read and write only their own cell.

| Pass | GDD | Behaviour (parameter names as in A4_TuningParameters §14) |
|---|---|---|
| 1 Solar | §3 | **Weekly reset first:** `precipitation = 0`; `cloudCover *= (1 − cloud_decay)`. Then `seasonTemp = baseline[biome[c]]`, latitude modifier, cloud albedo, heat capacity by water source, relax `temperature` toward `seasonTemp`, clamp [0,1]. Local |
| 2 Pressure | §4 | `pressure = clamp(0.5 − (temperature − 0.5)·pressure_sensitivity)`. Local. **Forcing hook A:** pressure holds |
| 3 Wind | §5 | Prevailing direction by latitude band (with CH-02 transition lerp) + pressure gradient over the 6 neighbours (precomputed unit vectors; `kNoCell` neighbours contribute nothing but still count in the ÷6). Writes continuous `windDirection`, clamped `windSpeed`, and the snapped `dirIndex`. **Forcing hook B:** wind overrides replace the result for their cells, and `dirIndex` is re-snapped from the override |
| 4 Evaporation | §6 | Water-source evaporation (self-limiting at saturation) and ground-water evaporation. Local. **Forcing hook C:** moisture holds |
| 5a/5b Moisture transport | §7 | 5a: `moistureOut[c] = moisture·windSpeed·moisture_transport_rate`, 0 if the downwind neighbour is `kNoCell` (EC-16: it stays in the cell). 5b: gather (below) |
| 6a/6b Heat transport | §8 | Same shape on `temperature` with `heat_transport_rate`. 6b: `temperature = max(temperature − out, 0) + inflow` (CH-07), so the clamp is order-free |
| 7 Orographic | §9 | Upwind neighbour = `neighbours[c·6 + (dirIndex+3) % 6]`. If it is `kNoCell`: **no lift** (nothing to compare against). If `elevation[up] < elevation[c]`: cool the cell, and if `temperature < condensation_temp` condense `moisture·gain·orographic_precip_rate` (capped at `moisture`). The condensate is **routed as in pass 8**, and `cloudCover = min(moisture, 1)` is taken after the subtraction |
| 8 Precipitation | §10 | As GDD, with `max(…, 0)` (CH-09). Snow below `snow_threshold` into `snowPack` (capped 1.0, CH-04), else into `groundWater`. `cloudCover = moisture·0.8` after the subtraction. **Forcing hook D:** one-week rain adds to `precipitation` and routes the same way; it does not consume moisture |
| 9 Snow melt | §11 | As GDD; melt adds to `groundWater` and cools the cell by `melt·melt_cooling` (clamped ≥ 0) |
| 10 Ground water | §12 | Drain `groundWater·ground_water_drain_rate`, floor 0 |
| Finish | §14, EC-20 | Clamp every field to its range (snow and ground water cap 1.0). Then the **serial totals pass** |

Evaporation (§6.2) belongs to pass 4. Pass 10 is only the drain.

### Wind snap and the prevailing-direction tie

Wind angles snap to one of six hex directions: `d = round((angle − 30°) / 60°) mod 6`. Both prevailing directions land **exactly on a tie** (trade winds 0° between NE 30° and SE 330°; westerlies π between NW 150° and SW 210°), and float noise in `atan2` would otherwise choose a side arbitrarily. Rule (user decision 2026-11-16, column-parity zigzag):

- Any angle within **1e-4 rad** of a tie is a tie.
- A tie is broken by the cell's column parity so that air zigzags and makes no net north or south progress.

| Tie | Even column | Odd column |
|---|---|---|
| East (0°) | SE, `d = 5` | NE, `d = 0` |
| West (π) | SW, `d = 3` | NW, `d = 2` |

Any other tie (rare: a pressure perturbation landing exactly on 90° or 270° multiples) uses the lower index. Non-tie angles snap normally (CH-06). The snap is a pure function `SnapWindDirection(angle, column)` in A4's own header; `HexTopology::DirectionFromAngle` is not used for ties and Dia is not edited (AD-001).

### Transport gather

For each cell `c`, in pass b:

```
inflow(c) = Σ over d = 0..5, in ascending order, of  moistureOut[n]   where n = neighbours[c·6 + d],
                                                      n != kNoCell and dirIndex[n] == (d + 3) % 6
          + edgeInflow(c)                              (added last)

edgeInflow(c) = moistureOut[c]  if boundary.edge_inflow
                                and neighbours[c·6 + (dirIndex[c] + 3) % 6] == kNoCell
              = 0               otherwise
```

- `moistureOut[c]` is already 0 when the downwind neighbour is off-map, so a cell on both an upwind and a downwind edge gains nothing and loses nothing from its own outflow.
- A cell with an upwind edge loses its outflow downwind and regains the same amount, so the net from its own outflow is zero (zero-gradient boundary, system spec Resolved Decision 1).
- Heat transport is identical on `heatOut`.
- The summation order is fixed, so the float result depends on neither range count nor range order.

### Forcings and overrides

`WeatherForcing` (header here, issued by Feature 5) is a small POD record: kind, cell, value(s), `weeksRemaining`.

- Kinds: `PressureHold`, `MoistureHold`, `Rain` (one week), `WindOverride`.
- Forcings are **latched at `Capture`** and applied at fixed points inside `Run`: hooks A–D in the table above (EC-15). A hold writes its target value after the pass that owns the field, so the next pass sees it. This is why a held low pressure survives the recomputation in pass 2.
- `Commit` decrements `weeksRemaining` and drops expired records. Forcings are not saved.
- Capacity: 64 active forcings, enforced with an error return from `AddForcing` rather than an unbounded vector.
- Wind overrides persist until `clear_wind_overrides` (Feature 5), so they carry no timer.

### New constant

`cloud_decay` (default **0.5**, range [0,1]) joins the §14 `solar` group. This is the **21st** constant: the GDD defines 20, and the system spec's "all 20 constants" becomes 21. The default is a starting point for tuning, not a design value; Feature 6's scenarios decide whether it holds.

### Grid totals

`WeatherGridTotals { double meanMoisture; double totalSnowPack; uint32_t rainingCellCount; }` is computed in a **serial** pass in cell order at the end of `Run`, on the calling thread (user decision 2026-11-16). It is about 7k adds at Large. The result is bit-identical at any range count and is committed with the fields. It never feeds back into the simulation. This replaces the system spec's "reduced per range and combined in range order" sentence, which would make the sums depend on the range count.

### Profile

`WeatherStepProfile` holds, per pass: wall time on the calling thread, and CPU time summed across ranges (a relaxed `fetch_add` on a `uint64_t` of per-range elapsed microseconds). Timing is never read by the simulation, so the atomics don't affect determinism. The profile covers the last step only.

### Pass granularity

`ParallelFor(N, minRange, fn)` with `minRange` = 256 cells (tunable constant, starting point). Ranges are contiguous index spans; each range writes only cells inside its span. Fusing adjacent local passes is a **fallback** if the CPU gate is missed, not a starting design (user decision 2026-11-16).

## Public API (additions to `CoW/Shared/Weather/`)

```cpp
namespace CoW::Weather {

    enum class ForcingKind : uint8_t { PressureHold, MoistureHold, Rain, WindOverride };
    struct WeatherForcing { ForcingKind kind; uint32_t cell; float value; float value2; uint16_t weeksRemaining; };

    struct WeatherGridTotals { double meanMoisture = 0; double totalSnowPack = 0; uint32_t rainingCellCount = 0; };

    struct WeatherStepProfile {
        static constexpr uint32_t kPassCount = 13;     // 1,2,3,4,5a,5b,6a,6b,7,8,9,10,finish
        uint64_t wallMicros[kPassCount];
        uint64_t cpuMicros[kPassCount];
        uint64_t totalCpuMicros;
        uint32_t lastRangeCount;
    };

    // Pure helper, unit-tested directly.
    uint8_t SnapWindDirection(float angleRadians, uint32_t column);

    class WeatherSimulation {                           // additions to Feature 1's class
    public:
        void Capture(Season season, const IBiomeSeasonalBaseline& baseline);   // SimPU
        void Run(Dia::DeferredStep::IParallelExecutor& executor);               // any thread; working set only
        void Commit();                                                          // SimPU
        void Step(Season season, const IBiomeSeasonalBaseline& baseline,
                  Dia::DeferredStep::IParallelExecutor& executor);              // Capture + Run + Commit
        bool AddForcing(const WeatherForcing& forcing);                         // false when full
        const WeatherGridTotals& GetGridTotals() const;
        const WeatherStepProfile& GetLastProfile() const;
    };
}
```

`Step` replaces the system spec's two-argument form.

## Acceptance Criteria

### Lifecycle and purity

1. `Capture` copies committed → working and latches the season, parameters, forcings, biome edits and the 13-entry baseline table. The baseline provider is called only inside `Capture`: a provider that asserts when called off the capturing thread passes.
2. `Run` after `Capture` does not read anything the caller can mutate. A test poisons (overwrites) the caller's parameters, forcing list, biome edit queue and baseline provider after `Capture`; the result equals a run with no poisoning.
3. Committed state does not change between `Capture` and `Commit`, however many times `Run` is invoked on a different working set.
4. `Commit` swaps working → committed, increments the week, decrements forcing timers and drops expired forcings.
5. `Step` equals `Capture; Run; Commit` bit-for-bit.
6. `Run` performs no heap allocation. Scratch is allocated at `Initialise`.

### Per-step behaviour (one test group per pass, using the GDD worked expectations)

7. **Solar:** an equator land cell converges toward its seasonal baseline at `land_heat_capacity`; a water cell converges more slowly. Cloud albedo reduces the delta. Latitude 1.0 cools by `latitude_cooling_factor`.
8. **Weekly reset:** `precipitation` is 0 at the end of pass 1 whatever it was before. `cloudCover` is multiplied by `(1 − cloud_decay)` before pass 1 uses it; with `cloud_decay = 0` it is unchanged.
9. **Pressure:** temperature 1.0 → pressure 0.2; 0.0 → 0.8; 0.5 → 0.5 (at default sensitivity). The result is clamped to [0,1] (EC-07).
10. **Wind:** uniform pressure gives the prevailing wind only. A high-pressure cell pushes neighbours' wind away from it, and a low-pressure cell pulls it in. The prevailing direction matches the latitude band, with a continuous lerp across the two transition zones (CH-02). Speed is clamped to [0,1].
11. **Evaporation:** water cells add `temperature·ocean_evap_rate·(1 − moisture)`, so a saturated cell gains nothing (EC-04). Ground water moves into moisture and is not negative afterward.
12. **Moisture transport:** moisture moves one cell downwind per step at the stated rate. Total moisture is conserved by the transport passes (excluding edge-inflow and forcing cells; test with `edge_inflow` off on a closed interior). A cell whose downwind neighbour is off-map keeps its moisture (EC-16).
13. **Heat transport:** same, with `heat_transport_rate`. Temperature never goes negative (CH-07), and a 1.0 cell receiving inflow ends at 1.0 after the finish clamp.
14. **Orographic:** with wind blowing upslope, the windward cell cools, condenses and rains (EC-09: a ring of mountains rains on the windward faces and leaves the interior dry). The leeward cell receives less rain than the windward cell. Downslope wind never triggers lift. A cell whose upwind neighbour is off-map never triggers lift.
15. **Orographic routing:** condensate below `snow_threshold` goes to `snowPack` and above goes to `groundWater`, with the same caps as pass 8. A cell never condenses more than its `moisture` (user decision 2026-11-16).
16. **Precipitation:** rain and snow split at `snow_threshold`. Zero moisture produces no precipitation and no negative amount (EC-11, CH-09). Snow pack caps at 1.0 and the excess is discarded (EC-12 for ground water).
17. **Melt:** no melt below `melt_threshold`. Melt with zero snow pack is a no-op (EC-13). Melt moves snow to ground water and cools the cell. A cell that stays below `melt_threshold` all year keeps its snow capped at 1.0 (EC-06).
18. **Ground water:** drain is 20%/week; ground water is about 0.33 of its start after 5 dry weeks; never negative.
19. **Finish:** every field is clamped to its range after the last pass (EC-20). Temperature extremes 0.0 and 1.0 for 52 consecutive weeks stay in range (EC-03).
20. **Zero wind:** at wind speed 0, nothing is transported and orographic lift does not fire on stale directions (EC-05).

### Wind snap

21. `SnapWindDirection` maps non-tie angles to the nearest of the six directions (angle = 30° + 60°·d).
22. East ties (0° ± 1e-4 rad) return SE (5) on even columns and NE (0) on odd columns. West ties (π ± 1e-4) return SW (3) on even and NW (2) on odd.
23. With uniform pressure, uniform terrain and trade winds, a moisture pulse moves due east at the end of 12 weeks with no net row drift (±1 row from the zigzag), and the same for westerlies going west.
24. Angles one `1e-3` rad either side of a tie snap to the nearer direction, not the parity direction.

### Transport edges

25. With `edge_inflow` on, a uniform moisture and temperature field under steady wind stays uniform across **all** cells, edges included, over 52 weeks (zero-gradient boundary).
26. With `edge_inflow` off, upwind-edge cells lose moisture each week (GDD-literal behaviour), proven numerically.
27. A cell with both its upwind and downwind neighbours off-map gains and loses nothing from its own outflow.
28. Inflow is summed in direction order 0..5, then the edge term: a test with mixed-magnitude inflows (different orders give different float sums) shows the result matches a reference loop in that order.

### Determinism

29. The same inputs give bit-identical fields after 52 weeks, run twice.
30. `SerialExecutor` and a test executor with 1, 3 and 17 ranges, and one range per cell, give bit-identical fields, grid totals and week counter after 52 weeks, at Small, and at Large with a hand-built terrain.
31. A test executor that runs the same ranges in **reverse order** gives the same bytes. This is what proves "no pass reads a neighbour field it writes".
32. Grid totals are bit-identical at any range count (serial pass), and equal a reference reduction in cell order.
33. **Deferred vs synchronous:** a test-local adaptor that drives `Capture` / `Run` / `Commit` through DiaDeferredStep's manual launcher, with `Capture` at key N and `Apply` at N+k, matches `Step`-per-week bit-for-bit over 52 weeks. The real `WeatherStep` arrives in Feature 3 and re-runs this test through the module.
### Forcings

34. `PressureHold` survives the pressure recomputation in pass 2 and shapes the wind in pass 3. `MoistureHold` is visible to the transport passes. `WindOverride` replaces pass 3's output for its cells and changes `dirIndex`.
35. `Rain` adds exactly `value` to `precipitation` for one week, routes to snow or ground water by temperature, and consumes no moisture.
36. Forcings expire after `weeksRemaining`. `AddForcing` returns false at 64 active forcings and nothing is dropped silently. The same forcings applied in any order give identical results.

### Performance

37. One `Step()` at Large in Release is reported with its per-pass wall and CPU breakdown. Budgets: **≤ 10 ms CPU** at Large, ≤ 6.3 ms Standard, ≤ 4.2 ms Small. A miss is reported and noted in the plan but does not fail the build. A **hard failure above 3× budget** catches accidental O(N²) behaviour or per-cell allocation (user decision 2026-11-16).

### Layer goldens

38. All 9 fields for the hand-built terrain after a fixed week count are checked against `Tests/GoogleTests/CoW/Golden/weather_layers.golden.json` through `CellLayerGolden`. The five-preset goldens belong to Feature 6; this feature registers the golden file and the first entry. See Open Design Question 3.

## Files Touched

New (names tentative until Feature 1 fixes the folder layout):

- `Cluiche/CoW/Shared/Weather/WeatherPipeline.h/.cpp`: the passes
- `Cluiche/CoW/Shared/Weather/WindDirection.h/.cpp`: `SnapWindDirection`, direction tables, prevailing-direction function
- `Cluiche/CoW/Shared/Weather/WeatherForcing.h`: forcing record
- `Cluiche/CoW/Shared/Weather/WeatherStepProfile.h`
- `Tests/GoogleTests/CoW/Weather/` — one test file per pass group, `WindDirectionTests`, `WeatherDeterminismTests`, `WeatherEdgeInflowTests`, `WeatherForcingTests`, `WeatherPerfTests`
- `Tests/GoogleTests/CoW/Golden/weather_layers.golden.json`
- A test-local `ReverseRangeExecutor` and `FixedRangeExecutor(n)`. If DiaDeferredStep Feature 1 ships equivalents under `DiaDeferredStep/Testing/`, use those instead (tests ship with libraries).

Modified:

- `Cluiche/CoW/Shared/Weather/WeatherSimulation.h/.cpp` (Feature 1): `Capture` / `Run` / `Commit` / `Step` / `AddForcing`
- `WeatherParameters` (Feature 1): adds `cloud_decay`
- `a4-weather.md`, to be updated when this spec is approved:
  - Public Interfaces (`Step` signature, `Capture`/`Run`/`Commit`)
  - the "stable copy" sentence in Update pipeline
  - the "reduced per range" totals sentence
  - "20 constants" → 21
  - a new GDD deviation list (orographic routing, weekly precipitation reset and cloud decay, ties, serial totals)

No Dia files are added or edited (AD-001). `IParallelExecutor` and `SerialExecutor` come from DiaDeferredStep Feature 1.

## Binding Decisions

| ID | Decision | How this feature complies |
|---|---|---|
| DFS-002 | `Run` reads only the captured working copy | `Capture` latches everything including the baseline table, so the provider is never called on a worker. AC 2 poisons the sources. |
| PD-004 | No STL containers in public APIs | Fields, scratch and neighbour table are `DynamicArrayC`. `WeatherForcing` is POD. Nothing `std::` crosses the public interface. |
| PD-005 | x64 only | Bit-identity is guaranteed per binary on x64. See Open Design Question 3 for cross-configuration limits. |
| AD-001 | Ask permission before Dia edits | None. The `ParallelFor` and `SerialExecutor` signatures come from the already-approved DiaDeferredStep Feature 1. |
| AD-003 | GDD owns design intent | Five deviations from the GDD pipeline are listed below for the GDD owner; none changes a constant's meaning except the new `cloud_decay`. |

## GDD Deviations Introduced (for the system spec's list, #15–#19)

15. **Orographic precipitation is routed like §10** (snow or ground water by temperature). The GDD adds it to `precipitation` only.
16. **`precipitation` is reset each week** and **`cloud_cover` decays** by a new `cloud_decay` constant. The GDD never clears either.
17. **Wind snap ties break by column parity.** Both prevailing directions fall exactly on a hex tie.
18. **Grid totals are a serial reduction**, so they are range-count independent.
19. **Stable reads by construction**, via scratch arrays, instead of copying the neighbour-read fields.

## Test Plan

- Pure logic, GoogleTests over hand-built hex terrains (no S-1 code). TDD per `.claude/skills/tdd.md`: RED for each pass group before the pass is written.
- Determinism tests run at 52 weeks, fields compared with `memcmp`.
- Performance and golden entries run as part of the final system task; scoped `--filter` while iterating, one full `--shards 2` run at the close-out, no mutation checks.
- The Observation Opportunity Scan at Verify covers `DIA_PROFILE_SCOPE` per pass (in scope here) and notes `cow.weather.step_ms` metric registration (Feature 5).

## Open Design Questions

1. **Is `cloud_decay` = 0.5 a sensible default, and does the GDD's precipitation-gating still hold?** With decay, a cell that rained last week has half its clouds this week, which raises its solar heating relative to the GDD's persistent-cloud behaviour. Higher heating means less precipitation in wet regions. Feature 6's coastal-rain and rain-shadow ratios are the test; if they fail, `cloud_decay` is the first knob to turn before touching GDD constants.
2. **Are forcing hook points A–D in the right places?** Pressure after pass 2, wind after pass 3, moisture after pass 4, rain after pass 8 are the earliest points where each held value is visible to every later pass. A `spawn_storm` that holds pressure low *and* moisture high touches two hooks. If Feature 5's scenarios need a storm to *start* in a cell rather than hold it, a one-shot "set value" forcing kind will be needed. It would be cheap to add here.
3. **Bit-identity is per binary. Do the layer goldens hold across Debug and Release?** Range count and order do not change the instruction sequence per cell, so AC 30/31 hold in every configuration. But `sinf`/`cosf`/`atan2f` and any compiler contraction or vectorisation can differ between Debug and Release, so a golden hash over exact float bits may fail in the other configuration. Task 1 of the plan should check the CoW and DiaCore projects' floating-point settings (bgfx builds `Fast`; the CoW and DiaCore projects were not checked here) and whether `CellLayerGolden::HashLayer` hashes exact bits or quantised values. If exact, the golden is Debug-only or per-config.

## Status

**Status:** `Approved` (2026-11-16).
