# Feature Spec: A5 HydrologyModule & World Step Stage

**Parent:** @docs/specs/applications/cow/systems/a5-hydrology/a5-hydrology.md (Feature 3)
**Status:** Approved (2026-11-17, user, after the locked-GDD alignment)
**Design source:** the locked GDD `C:\GitHub\DesignDocs\COW\Output Docs\GDD\08_SystemsDeepDive\A5_Hydrology\` (locked 2026-11-17: `A5_TechnicalSpec.md` §13, §14, §16, `A5_EdgeCaseMatrix.md` EC-10, EC-14, EC-15, EC-23; DECISION-129..134, CHANGE-047/048). The GDD owns design intent and constants (AD-003). Aligned to it (aligned to locked GDD 2026-11-17); the differences that remain are listed under "GDD differences".
**Depends on:**
- A5 Features 1 and 2 (`HydrologySimulation`, `HydrologyParameters`, `HydrologyTerrain`, `WaterInput`, `FixedWaterInput`).
- World Step Features 1 and 2 (`IWorldStepStage`, `WorldStepModule::RegisterStage`).
- DiaDeferredStep Features 1, 2 and 4 (`IParallelExecutor`, the capped executor).
- S-1 (built): `WorldGenerationModule::GetCurrentWorld`, `IsReady(revision)`, `HoldGeneration`/`ReleaseGeneration`, `WorldGenerationComplete`/`WorldGenerationFailed`.
- S-1 consumer amendment 8 (basin table, bed elevation) for real worlds. Unit tests use hand-built sources.

**Coordinates with (amendments owed, not Dia):**
- A4 Feature 3 (`WeatherModule` implements `IWaterInputSource`, calls the spin-up host).
- A4 Feature 7 (the influence record).

**Consumed by:** A5 Features 4–6, A1 (`HydrologyUpdated`, `HydrologySnapshot`, `RegisterSinkProvider`), A3, V-1, S-2.6.

## Summary

`HydrologyModule` is the SimModule that turns the pure A5 simulation into a live World Step stage.

- It loads parameters, ingests S-1's terrain on `WorldGenerationComplete`, spins the world up **off SimPU**, and registers a `hydrology` stage with `WorldStepModule`.
- After that it only does what the coordinator asks: `Capture(stepDay)`, `Run`, `Commit`.
- It owns no cadence, never kicks, and never subscribes to `SimulationTick`.
- On commit it publishes `HydrologyUpdated{week}`.

The module also owns the seams that keep A5 free of its neighbours:
- **Water in.** An `IWaterInputSource` (A4's `WeatherModule`) gives A5 this week's `waterToGround`, `temperature`, `snowPack` and `glacierInput`, read from A4's working copy inside the same `Run`. With no source, a `.diaapp`-configured `FixedWaterInput` stands in.
- **Water out.** Up to 8 registered `IWaterSinkProvider`s fill an ET multiplier and an absolute draw per cell (A1 vegetation later, anything else now).
- **Influences.** The ground-water part of A4's wetter/drier influences becomes `extraWater` (wetter) and a draw (drier).
- **Back to A4.** The module implements A4's `IHydrologyInputsProvider` (`surfaceWetness`, `openWater`, `glacierEligible` from A5's last committed week).

The module has **no save or history code** (Feature 4 and the coordinator), and no commands, layers or metrics registration (Feature 5).

## Goals

1. `HydrologyModule` is a `WorldStepModule` stage with id `hydrology`. It never kicks itself and never subscribes to `SimulationTick`.
2. The week of every step comes from the **step day**: `week = WeekOf(stepDay)`, week-of-year `= week % 52`. There is no second week counter.
3. World regeneration never stalls SimPU and never lets an old world's step touch a new world.
4. Every mutation (parameters, biome edits, sinks, influences, the input source) is latched at `Capture`, never applied while a step is in flight.
5. `Run` reads only the captured working set and A4's working buffers for this step (DFS-002).
6. With A4 present, A5 spins up **coupled** with A4 in A4's worker job. Without A4, A5 spins up alone on the fixed driver.
7. Wiring is by interfaces through `ModuleRef`. No singletons, no service locators, no ServiceStreams (everything here is SimPU).
8. A missing or bad parameter file, a bad provider output or a missing input source degrades loudly, and the module keeps running where it safely can.
9. `IsReady(revision)` is the single truth for "this world is initialised and spun up".

## Non-Goals

- Cadence, `k`, the week gate, stalling, kick retry (World Step).
- The weekly passes (Feature 2). This feature calls `Step` and adds nothing to the physics.
- `HydrologySaveParticipant` and the pending-result blob (Feature 4).
- Layers, history groups, commands, `cow.hydro.step_ms` registration, the water-balance gauge and the health reporter's balance rule (Feature 5). This feature only **times** the step.
- Presets and regression scenarios (Feature 6). They will add entry types to the mutation queue.
- Defining influence records. A4 Feature 7 owns them; this feature reads one summed array through a small seam.
- The A1 sink provider itself. A1 registers it when A1's vegetation draw is specced.
- Publishing `WorldGenerationFailed`. S-1 owns it; A5 only reacts.

## Design

### Placement and manifest

`Cluiche/CoW/Modules/HydrologyModule.h/.cpp`, `HydrologyStage.h/.cpp`, `HydrologyMessages.h` (hand-written POD, like `GameSpeedMessages.h`). Shared, pure and unit-testable without any module: `Cluiche/CoW/Shared/Hydrology/` (`HydrologyCoupling.h`, `HydrologyTerrainBuilder.h/.cpp`, `HydrologyRunner.h/.cpp`, `HydrologySpinUp.h/.cpp`, `HydrologySnapshot.h/.cpp`).

**Stage-scoped**, like `WeatherModule`: each hosting stage declares its own instance, listed **after** `WorldStepModule` (and after `WeatherModule` where present).

```json
{ "instance_id": "HydrologyModule", "type_id": "HydrologyModule",
  "stages": ["S26RiversFloodsStage"],
  "dependencies": ["MessageBusHostModule", "WorldStepModule", "WorldGenerationModule", "SimQueryHostModule"],
  "soft_dependencies": ["WeatherModule", "SaveLoadModule", "CellLayersModule", "CellHistoryModule"],
  "config": { "parameters": "Global/Hydrology/hydrology_parameters.json", "spinup_weeks": 52, "start_coldness": 0.0,
              "fixed_input": { "mode": "constant", "water_to_ground": 0.05, "temperature": 0.5 } } }
```

(The stage name is illustrative; S-2.6 and S-3.5 own their manifests.)

- `spinup_weeks` is validated `0..520` as A4's is; 0 means no spin-up (tests). The GDD's `SPINUP_WEEKS` is 52.
- `start_coldness` (0..1, default 0) is the start phase's coldness for the **alone** path only. With A4, A4 supplies it (see Spin-up).
- `fixed_input` is optional. `mode` is `constant` (two numbers) or `script` (two equal-length weekly arrays, played in a loop). Values are validated against `FixedWaterInput`'s ranges, and a bad block is a config error naming the key.
- A `WeatherModule` found through the soft dependency is wired in `DoStart` (next section). If one is not present, A5 runs on the fixed driver.

### Wiring by interfaces (`HydrologyCoupling.h`, CoW-Shared)

A5-owned interfaces. They contain only PODs and float pointers, so `Shared/Hydrology/` includes nothing from `Shared/Weather/`.

```cpp
namespace CoW::Hydrology {

    // Pointers into the SOURCE'S working buffers for the step captured on this call.
    struct WaterSourceView {
        const float* waterToGround = nullptr;
        const float* temperature   = nullptr;
        const float* snowPack      = nullptr;
        const float* glacierInput  = nullptr;     // may be null = zero
        uint32_t     cellCount     = 0;
    };

    class IWaterInputSource {                     // WeatherModule implements it
    public:
        virtual ~IWaterInputSource() = default;
        // SimPU, inside the source's own Capture or later in the same Capture phase. False when
        // the source has no captured step (not ready, or its step is discarded).
        virtual bool GetWorkingOutput(WaterSourceView& out) const = 0;
        // Signed ground-water depth per cell, summed over the influences active for the captured step
        // (positive wetter, negative drier). False = none active. The record layout is A4 Feature 7's.
        virtual bool CopyGroundWaterInfluence(Dia::Core::DynamicArrayC<float>& signedDepth) const = 0;
        virtual bool IsReadyForRevision(uint32_t worldRevision) const = 0;
    };

    struct WaterSinkBuffers {                     // pre-filled neutral: etMultiplier 1.0, draw 0.0
        float*   etMultiplier;
        float*   draw;
        uint32_t cellCount;
    };
    class IWaterSinkProvider {
    public:
        virtual ~IWaterSinkProvider() = default;
        // SimPU, inside Capture only. Multiply into etMultiplier, add into draw. Never keep the pointers.
        virtual void AddSinks(uint64_t stepDay, const WaterSinkBuffers& buffers) = 0;
    };
}
```

A4 owns `IHydrologyInputsProvider` and `HydrologyInputs` (weather-module.md). `HydrologyModule` implements the provider; its one method returns copies of `surfaceWetness`, `openWater` and `glacierEligible` from the **committed** state.

**What `HydrologyModule` does in `DoStart`:**
- Registers stage id `hydrology` with `WorldStepModule`.
- If a `WeatherModule` resolves, calls `weather.SetHydrologyProvider(this, this)` (this module is both the `IHydrologyInputsProvider` and the `IHydrologySpinUpHost`; A4 F3 signature, aligned 2026-11-17) and `SetWaterSource(&weather)` on itself. If not, neither is set.
- Calls `HoldGeneration()`, released once its subscriptions are in place, so the first world is never missed.

`DoStop` undoes all of it (see Lifecycle).

### Water source and the fixed driver

`SetWaterSource(IWaterInputSource*)` is public, so a test or a testbed can set a fake. Null clears it.

- **Source set:** `Capture` calls `GetWorkingOutput`. The returned pointers are A4's working buffers for the step now captured. They stay valid until A4's `Commit` for two reasons:
  - A4 allocates its working buffers once, at promotion, and does not reallocate them between its `Capture` and its `Commit` (an A4 Feature 3 requirement).
  - A4's `Capture` runs before A5's (declared order), so the buffers exist and belong to the same step.
- **Reading rule.** A5 reads those pointers **only in `Run`**, and A4's `Run` has finished by then because the coordinator runs stages in order. A5 never reads them in `Commit` (A4 has already swapped by then, because `weather` commits first).
- **No source:** the module uses `FixedWaterInput` built from `fixed_input`. `ForWeek(WeekOf(stepDay))` gives the pointers; `snowPack` and `glacierInput` are zero arrays.
- **Source set and `fixed_input` configured:** the source wins and one Info is logged.
- **Neither:** the module is Unhealthy ("no water input"), reports `IsReady() == false`, and logs an Error. It does **not** invent a default rain.
- **A bad view** (`GetWorkingOutput` returns false, a null required pointer, or `cellCount != N`): the step is marked invalid, `Run` is a no-op, `Commit` discards it (below), health is Degraded, one Error is logged.

### Sink providers

`RegisterSinkProvider(Dia::Core::StringCRC id, IWaterSinkProvider*)` returns false for a null provider, a duplicate id, or when 8 are registered. `UnregisterSinkProvider(id)` is immediate; providers are only called inside `Capture` on SimPU, so removal cannot race a `Run`.

At `Capture`:
1. If at least one provider is registered, `etMultiplier` is filled with 1.0 and `draw` with 0.0.
2. Providers are called in **ascending `StringCRC` id**, not registration time. Multiplication and addition run in that fixed order, so the floats are the same however the modules started.
3. After the last provider, each value is checked. A non-finite or negative value is replaced by the neutral value (1.0 or 0.0), logged as an Error once per provider id, and health goes Degraded. No upper bound is applied: any larger limit would be invented, and Feature 2 clips draws to what is there.
4. With **no** provider, the arrays are not allocated and `WaterSinks` holds nulls (neutral, Feature 2 AC 3).

The module adds nothing to the sink arrays except the influence draw below, so a provider cannot see another provider's contribution.

### Influences

If the source offers a ground-water influence array, `Capture` copies it once. Then, per cell:
- `signed > 0` goes into `extraWater` (wetter);
- `signed < 0` adds `-signed` to the `draw` array (drier), after all providers.

A4 hands the array already weighted (strength × falloff × the ground-water factor, summed over active influences in A4's deterministic order) and in cell-depth units. A5 holds no influence constant. If no influence is active, `extraWater` stays null and nothing is added to `draw`.

### Module state

```cpp
HydrologyParameters  mParams;                 // loaded; latched into the simulation at Capture
HydrologySimulation* mActive;                 // the sim the stage steps; null until the first world is ready
HydrologySimulation* mPending;                // a spun-up sim for the newest revision, waiting for a Capture boundary
uint32_t             mTargetRevision;
uint32_t             mActiveRevision, mPendingRevision, mCaptureRevision;
uint64_t             mCapturedDay;
bool                 mStepValid;              // false: no input this step, Commit will discard
MutationQueue        mQueue;                  // parameter sets, overrides, biome edits, reload (bounded, 128)
SinkTable            mProviders;              // fixed capacity 8, sorted by id
IWaterInputSource*   mSource;                 // non-owning
FixedWaterInput      mFixed;
```

Two copies of the mutable state (committed + working) live **inside** `HydrologySimulation` (see below), so `mActive` is one object.

### `HydrologySimulation` additions (amends Features 1 and 2)

Feature 2's `Step(in, exec)` stays as it is. This feature splits it, additively:

```cpp
void   Capture(const HydrologyLatch&);   // working := committed; apply the latch (parameters, biome edits)
void   Run(const WaterInput&, Dia::DeferredStep::IParallelExecutor&);   // Step on the working set only
void   Commit();                         // swap working <-> committed
// Step(in, exec) == Capture(neutral latch) + Run + Commit; Feature 2's tests are unchanged.
void   SetCommittedWeek(uint64_t week);  uint64_t CommittedWeek() const;
const HydrologyFields& Fields() const;   // committed
const HydrologyFields& WorkingFields() const;
```

- `HydrologyLatch` holds a parameter copy and the pending biome edits (cell, biome). The biome array is part of the working set, so a biome edit never changes what a reader sees before `Commit`.
- Commit is a swap of arrays, not a copy.

### World ingest

On `WorldGenerationComplete{worldRevision}`:

1. `mTargetRevision = worldRevision`. `IsReady()` is false at once, and any running A5-only spin-up is cancelled (it checks the revision between weeks).
2. `HydrologyTerrainSource` is read from `GetCurrentWorld()`: dimensions, `Cell::elevation`, bed elevation, full-grid flow direction, `waterSource`, `biome`, terrain flags, corridor-site cells, **Hearth, Nexus and world-throne site cells**, and the amendment 8 basin table. The module never holds the world pointer.
3. The pure `BuildHydrologyTerrain(source, terrain, err)` makes `HydrologyTerrain`, including `isCrossing` (corridor sites ∪ `Pass` ∪ `RiverCrossesBarrier` ∪ fixed-site cells) and `basinCells`. A **version-1** world has no basin table. Then the builder makes a lake-only table from lake membership, sets `basinsFromLakesOnly = true`, and logs one Info.
4. `HydrologySimulation::Initialise(terrain, params, err)` runs on SimPU. A failure logs an Error with the named reason, reports Unhealthy, and starts nothing. A5 does **not** publish `WorldGenerationFailed`; that is S-1's. (The GDD's EC-14 sends an inconsistent world down `WorldGenerationFailed`. See "GDD differences".)
5. Spin-up starts (next section).

`WorldGenerationFailed{reason, …}`: Unhealthy with the reason, `IsReady()` false, any running A5-only spin-up cancelled. The last good world is **not** kept active, as in A4.

Ingest is idempotent per revision: a second call for the same revision does nothing. A4's handler may call `BeginSpinUp(revision)` **before** A5's own handler has run, and it must work either way.

### Spin-up (decided 2026-11-17)

Two paths, chosen by whether an `IWaterInputSource` is set.

**Coupled (source set).** A4's `WeatherModule` owns the worker job (A4 Feature 3). A5 supplies the pieces through `IHydrologySpinUpHost`, which `HydrologyModule` implements:

```cpp
class HydrologySpinUpTicket;                  // self-contained: owns an initialised sim and a parameter copy
struct SpinUpStart {                          // from A4 Feature 8 (seam fix 2026-11-17, user): both per cell, N entries,
                                              // valid for the call only
    const float*   coldness = nullptr;            // WeatherRunner::StartColdness(cell), [0,1]; null = all 0 (warm start)
    const uint8_t* snowSurvivesSummer = nullptr;  // WeatherRunner::StartSnowSurvivesSummer(cell); null = no glacier seed
};
class IHydrologySpinUpHost {
public:
    // SimPU. A4 calls it when it launches its job. Ingests the world for `revision` if A5 has not yet.
    // Applies the phase-scaled seed (Feature 1 ApplyPhaseSeed) from `start` before returning the ticket.
    // Null = A5 is not joining (init failed, or A5 already holds restored state for this revision).
    virtual HydrologySpinUpTicket* BeginSpinUp(uint32_t revision, const SpinUpStart& start) = 0;
    // SimPU. The job finished and its revision is still current. Takes the ticket's sim as mPending.
    virtual void CompleteSpinUp(HydrologySpinUpTicket* ticket, uint32_t revision) = 0;
    // A stale or cancelled job just destroys the ticket (it references no module). Any thread.
    static void Destroy(HydrologySpinUpTicket* ticket);
};
```

- The ticket's `StepWeek(weekIndex, const WaterSourceView&, exec)` is `HydrologyRunner`'s week. It also implements A4's `IHydrologyInputsProvider`, so the A4 job feeds A4's spin-up weeks from A5's spin-up state exactly as a live `Capture` does.
- The A4 job calls `weatherRunner.StepWeek`, then `ticket->StepWeek` with A4's working view, in World Step order.
- Length is `SPINUP_WEEKS`: **52 weeks**, with climate cycles **held at the world's start phase** for the whole spin-up (the GDD's frozen-phase spin-up, A4 Feature 8). There is no longer spin-up "extended when cycles are on" (aligned to locked GDD 2026-11-17). A5 sees only the resulting inputs.
- **Phase-scaled seed (GDD §13).** One year cannot build lakes or glaciers, so slow stores start seeded for the start phase: S-1 lakes `capacity · lerp(1, spinup.lake_seed_cold, basinColdness)`, where `basinColdness` is the mean of `coldness[cell]` over the basin's cells (in ascending cell order). Glaciers are `spinup.glacier_seed_max · coldness[cell]` on the cells A4 flags in `snowSurvivesSummer`, with `snowYears = onset_years`. The default start is the warm point (`coldness` 0 everywhere), so the seed is a no-op: full lakes, no glaciers. A preset can pin another start phase. A5 applies it in `BeginSpinUp` through Feature 1's `ApplyPhaseSeed`. Both arrays, and the mask rule, are A4 Feature 8's `(seam fix 2026-11-17, user)`.
- On completion A4 calls `CompleteSpinUp`. A5 sets `mPending` and `mPendingRevision` at the same `DoUpdate` poll where A4 sets its own pending sim, so the coordinator's next `Capture` promotes **both** together.
- Spin-up is silent: no `HydrologyUpdated`, no history signal.
- A4 cancels by destroying the ticket. A5 holds nothing of it.
- If A4 never calls `BeginSpinUp` (see the "A4 restored" case in Feature 4), A5 notices: it polls `source->IsReadyForRevision(target)` in `DoUpdate`. When A4 is ready for the target revision, A5 has no pending sim, and `BeginSpinUp` was never called for it, A5 concludes A4 skipped the coupled job. It then **seeds only** (Initialise's state, no spin-up) and goes Degraded with a Warning naming the reason. Open Design Question 2.

**Alone (no source).** A5 seeds with a uniform coldness array filled from `start_coldness` and no glacier mask (lakes only) (seam fix 2026-11-17, user), then launches its own job with `JobSystemService::TrySubmit(fn, tag)` and the `world` tag. The job runs `HydrologyRunner::RunFixed(sim, driver, spinup_weeks, executor, cancel)` on the DeferredStep capped executor. Its result goes into a SimPU-owned `SpinUpSlot` (`Idle/Running/Done/Cancelled`, the revision, the sim pointer). `DoUpdate` polls it and sets `mPending` on `Done` for the target revision. This is A4's gap-pass amendment 2: no bus message, no callback onto SimPU. With no job system (headless tests), a **synchronous launcher** seam runs the same code inline. The spun-up state is bit-identical either way, and equals `HydrologyRunner` run synchronously.

For either path, a job whose revision is stale discards its sim. While `IsReady()` is false, `AllReady()` is false, so the coordinator kicks nothing and the executor is not shared concurrently.

### The stage (`HydrologyStage : IWorldStepStage`)

| Method | Behaviour |
|---|---|
| `IsReady()` | `(mPending && mPendingRevision == mTargetRevision) \|\| (mActive && mActiveRevision == mTargetRevision)`, no Unhealthy cause, and a water input exists (source or fixed driver). False during spin-up and after `WorldGenerationFailed` |
| `Capture(stepDay)` | 1. **Promote** `mPending` to `mActive` if present (the chain is Idle; the old sim is destroyed). 2. Drain `mQueue` in arrival order into a `HydrologyLatch`. 3. `mCaptureRevision = mActiveRevision`, `mCapturedDay = stepDay`. 4. Get the input view (source or driver) and set `mStepValid`. 5. Fill sinks, then influences. 6. `sim.Capture(latch)` |
| `Run(exec)` | If `!mStepValid`, return. Otherwise build `WaterInput{…, week = WeekOf(mCapturedDay) % 52}` and call `sim.Run(input, exec)`. Time it. Reads nothing SimPU-owned (DFS-002) |
| `Commit()` | If `!mStepValid` or `mCaptureRevision != mTargetRevision`: **discard** (no `sim.Commit`, no message, one Info log, counted; Degraded for an invalid view). Otherwise `sim.Commit()`, `SetCommittedWeek(WeekOf(mCapturedDay))`, record the step time, publish `HydrologyUpdated{week}` |
| `SerializePending` / `DeserializePending` / `ValidatePending` | Feature 4 |

Week arithmetic uses `WeekOf` and the real `kWeeksPerSeason`/`kSeasonsPerYear` constants from `GameSpeedController.h`, never a literal 52. A retried step publishes the week of the **represented** day. `SetCommittedWeek` is used so the snapshot's `GetWeek()` is `WeekOf(stepDay)`, as in A4.

**Order inside a step.** With `["weather", "hydrology", "ecology"]` the coordinator runs Capture(weather), Capture(hydrology), …, Run(weather), Run(hydrology), …, Commit(weather), Commit(hydrology), …. So A5's `Run` sees A4's finished working copy of this week, and A4's `Capture` of the **next** step sees A5's committed state from this one. That is the one-week lag in the parent spec.

### Biome edits and parameters

All public mutators latch through `mQueue` and are safe to call on SimPU at any time.

```cpp
bool QueueParameterSet(const char* dottedKey, float value, ParamError& err);      // key may be biome_table.<BiomeId>.<column>
bool QueueParameterOverrides(const Json::Value& overrides, ParamError& err);      // atomic; packs and reload merge
bool QueueReload();                                                               // re-read the file at the next Capture
bool QueueBiomeChange(uint32_t cell, WorldGeneration::Biome biome);
```

- Each validates against Feature 1's rules on a copy and returns false immediately on bad input, queueing nothing. A full queue (128 entries, the same as A4) returns false, logs one Warning and degrades health.
- `BiomeTransitionComplete` (A3's payload is A3's to define) is subscribed when A3 publishes it, and calls `QueueBiomeChange`. The new biome applies from the **next** `Capture`, so it takes effect next week.
- Parameters are read from the file **once at init**. They change only through the queue. A failed load falls back to `DefaultHydrologyParameters()`, logs an Error with the file and dotted key, and goes Degraded.
- A failed queued reload keeps the previous parameters and logs.
- Feature 5 registers `cow.hydro.*` over these methods. Feature 6 adds preset and state-replace entries.

### Snapshot (`HydrologySnapshot`, CoW-Shared)

A const view over the **committed** state. It is valid until the next `Commit` or promotion, and subscribers read it in response to `HydrologyUpdated` only. Before the first world is ready it is empty (`GetCellCount() == 0`).

| Accessor | Meaning |
|---|---|
| `GetWeek()`, `GetCellCount()`, `GetBasinCount()` | The last published week; sizes |
| `SoilMoisture(cell)` | Soil wetness `[0,1]`: `soilMoisture / soil_capacity[biome]`, 0 where capacity is 0, 1 on inundated cells. What A1 reads as cell water |
| `SoilStore(cell)` | The raw soil store in cell-depth units |
| `RiverFlow(cell)`, `MeanFlow(cell)` | `riverFlow`, `meanFlow` |
| `RiverState(cell)` | `RiverState` (GDD §14.2): `Frozen` if `ice ≥ freeze.ice_crossing_min`; otherwise `Dry` below `river.low_flow_fraction · meanFlow`, `Low` below `0.7 · meanFlow` (the GDD's fixed 0.7), `Normal` up to bankfull (`flood.bankfull_factor · max(meanFlow, flood.min_flow)`), `Flood` above. Non-river cells report `Normal`. For V-1 (river width by flow) and A1 |
| `Flooded(cell)`, `Ice(cell)`, `Glacier(cell)` | Flags and stores |
| `BasinIceFraction(basin)` | The basin's frozen surface share |
| `LowFlowWeeks(cell)`, `GlacierWeeks(cell)`, `GlacierFreeWeeks(cell)` | A3's other counters (dry riverbed, glaciation, glacial retreat) |
| `InundatedWeeks(cell)`, `ExposedWeeks(cell)` | A3's counters |
| `PassabilityAt(cell)` | `Passability` |
| `SurfaceWetness(cell)`, `OpenWater(cell)` | A4's inputs: `OpenWater` is the committed field; `SurfaceWetness` is defined below |
| `GlacierEligible(cell)` | The committed flag |
| `Level(basin)`, `Volume(basin)` | Per basin |

**`SurfaceWetness(cell)`** = `clamp01(surfaceWater / wetness.surface_scale + wetness.soil_weight · SoilMoisture(cell))`, the GDD's formula. Feature 1 holds the constants (`wetness.surface_scale` 0.1, `wetness.soil_weight` 0.25, CH-A5-04: low weight so A4's land evaporation stays near its S-2.5 tuning). `OpenWater(cell)` includes the river term (Feature 2 pass 7).

**What the readers do with it (the GDD's ripples, noted here, their specs are their owners'):**
- **A1** keeps `water_gradient` and builds it from A5 in a thin step 2. It treats inundated cells as aquatic habitat.
- **A3** reads the counters at the Decade band. First-run thresholds: Lake after 104 inundated weeks, Lake to Wetland after 52 exposed weeks, `DRY_RIVERBED` after 26 low-flow weeks, glaciation and retreat after 260 weeks. Glaciated cells become Alpine (`elevation ≥ 0.7`) or Tundra, with no new biome, and A3 saves `pre_glacial_biome`. Ocean and Lake are never glaciated.
- **V-1** shows lake extent, floods, ice and river width by flow. Soil and ground water are debug-only.

### Messages

`HydrologyMessages.h`: `HydrologyUpdated { uint64_t week; }`. Published from `Commit`, after the swap, once per applied step. A discarded step publishes nothing. `BiomeTransitionComplete` is A3's.

### Lifecycle

`DoStop`:
- unregister the stage from `WorldStepModule`;
- `weather.SetHydrologyProvider(nullptr, nullptr)` if the weather module is still there;
- clear the water source and the sink table;
- cancel and join the own spin-up job, discard its result;
- destroy both sims and clear `mQueue`;
- release `HoldGeneration` if still held.

A ticket held by a running A4 job is A4's to destroy, and `CompleteSpinUp` is never called after `DoStop` (A4 stops with its host pointer cleared). Re-entry rebuilds from config on the next `DoStart`. A stale job result from an earlier visit is gated by the revision **and** a per-visit generation id.

### Observability (this feature's share)

- **Health (`IHealthReporter`):** Degraded on fallback parameters, a bad provider output, an invalid or discarded step, a full mutation queue, an unspun seed-only start. Unhealthy on a failed `Initialise`, no water input, or `WorldGenerationFailed`.
- **Logs, channel `HYD`:**
  - Info: parameter source, world revision, spin-up start and finish with duration, a discarded step, the source/driver choice, a version-1 world.
  - Warning: a full queue, a dropped stale spin-up.
  - Error: parameter file failure, a bad terrain, a bad provider value, no input.
- **Trace:** `DIA_TRACE_ZONE` around `Capture`, `Commit`, ingest and the spin-up job.
- **Timing:** `Run` records its wall time. `HydrologyStage::LastStepMs()` hands it to Feature 5, which registers `cow.hydro.step_ms` (SD-PERF-7). No gauge is registered here.

## Public API

```cpp
namespace CoW::Hydrology {

    class HydrologyModule : public Dia::ApplicationFlow::SimModule,
                            public CoW::Weather::IHydrologyInputsProvider,
                            public IHydrologySpinUpHost {
    public:
        static const Dia::Core::StringCRC kTypeId;

        bool RegisterSinkProvider(Dia::Core::StringCRC id, IWaterSinkProvider* provider);   // capacity 8
        void UnregisterSinkProvider(Dia::Core::StringCRC id);
        void SetWaterSource(IWaterInputSource* source);                                      // null clears
        void SetFixedInput(const FixedWaterInput& driver);                                   // tests, testbeds

        bool QueueParameterSet(const char* key, float value, ParamError& err);
        bool QueueParameterOverrides(const Json::Value& overrides, ParamError& err);
        bool QueueReload();
        bool QueueBiomeChange(uint32_t cell, WorldGeneration::Biome biome);

        bool IsReady(uint32_t worldRevision) const;
        HydrologySnapshot GetSnapshot() const;

        // IHydrologyInputsProvider (A4-defined; committed state, SimPU, called inside A4's Capture only)
        // IHydrologySpinUpHost
    };

    class HydrologyRunner {                       // pure; the synchronous oracle and the spin-up body
    public:
        explicit HydrologyRunner(HydrologySimulation& sim);
        bool StepWeek(uint32_t weekIndex, const WaterInput&, Dia::DeferredStep::IParallelExecutor&);
        bool RunFixed(const FixedWaterInput&, uint32_t weeks, Dia::DeferredStep::IParallelExecutor&,
                      const CancelFn& cancelled);   // false if cancelled
    };

    bool BuildHydrologyTerrain(const HydrologyTerrainSource&, HydrologyTerrain& out,
                               Dia::Core::Containers::String256& err);       // pure

    class HydrologyStage final : public CoW::WorldStep::IWorldStepStage { /* table above */ };
    struct HydrologyUpdated { uint64_t week; };
}
```

## Acceptance Criteria

### Module and manifest
1. `dia validate manifest` accepts the instance, requires `WorldGenerationModule` and `WorldStepModule`, and rejects a manifest listing `HydrologyModule` before `WorldStepModule`.
2. `spinup_weeks` outside `[0, 520]` or non-integer is a config error naming the key. A bad `fixed_input` block (unknown `mode`, unequal script arrays, an out-of-range value) is a config error naming the key.
3. The module registers stage id `hydrology` in `DoStart` and unregisters in `DoStop`. It registers no timer and no `SimulationTick` subscription.
4. With a `WeatherModule` present, `DoStart` calls `SetHydrologyProvider(this, this)` and sets the water source. The provider also returns `glacierCheckWeek` and `glacierSnowFloor` from A5's parameters (`GLACIER_CHECK_WEEK` 26, `GLACIER_SNOW_FLOOR` 0.2), so A4's firn hand-off uses A5's constants. With none, neither is set and the module is not Degraded.

### Ingest and terrain
5. `BuildHydrologyTerrain` on a hand-built source produces a valid `HydrologyTerrain`. `isCrossing` is exactly corridor sites ∪ `Pass` ∪ `RiverCrossesBarrier` ∪ Hearth, Nexus and world-throne site cells.
6. A version-1 source (no basin table) gives a lake-only table with `basinsFromLakesOnly = true` and one Info log. Initialisation succeeds. Through the stepped simulation, its lake cells are `Deep` and its protected cells (corridor, crossing and fixed-site cells) are `Fordable`.
7. A source that Feature 1's `Initialise` rejects leaves `IsReady()` false, logs an Error with the reason, reports Unhealthy and starts no job. The module publishes no `WorldGenerationFailed`.
8. `WorldGenerationComplete` makes `IsReady(rev)` false until the spin-up for `rev` completes, then true for `rev` and false for `rev + 1`.
9. A second call to ingest for the same revision (from `BeginSpinUp` and from the event handler, in either order) does nothing the second time.

### Spin-up
10. **Alone:** the module spins up on the fixed driver for `spinup_weeks` and the result is bit-identical to `HydrologyRunner::RunFixed` run synchronously. Spin-up publishes no `HydrologyUpdated`.
11. With a **synchronous launcher** the module behaves identically, and the job completes before `WorldGenerationComplete` returns.
12. A second `WorldGenerationComplete` during a running spin-up cancels it. Its result is discarded and never becomes active. Only the newest revision becomes ready.
13. `WorldGenerationFailed` makes the module Unhealthy and `IsReady()` false until the next success, and cancels a running own job.
13a. **Phase seed, alone:** with `start_coldness` 0 the spun-up state equals a run with no seed. With `start_coldness` 1 the lakes start at `capacity · lake_seed_cold` and no glacier is seeded. `start_coldness` outside 0..1 is a config error naming the key.
13b. **Spin-up length:** spin-up lasts `spinup_weeks` (52 by default) whatever the cycle state. No test or code path lengthens it.
14. **Coupled:** with a source set, the module launches no job of its own. `BeginSpinUp(rev, start)` returns a ticket; after the (fake) A4 job steps it for N weeks and calls `CompleteSpinUp`, the module's pending state equals a synchronous coupled run (fake A4 runner + `HydrologyRunner`), bit-identical for both simulations.
15. A cancelled coupled job (the ticket destroyed) leaves no state in the module and no pending sim.
16. A4's fake handler and A5's handler can run in either order for the same `WorldGenerationComplete`; the result is the same.
17. `BeginSpinUp(rev, start)` returns null if `Initialise` failed for `rev`.
17a. **Phase seed, coupled:** `BeginSpinUp` with a `coldness` array of 1 and a mask applies Feature 1's seed. A non-uniform array seeds each basin from its cells' mean coldness and each masked cell from its own value (seam fix 2026-11-17, user) before the first spun-up week (lakes scaled, masked cells glaciated and eligible). With `coldness` 0 the ticket's state equals a fresh `Initialise`.
18. A source that reports ready for the target revision while `BeginSpinUp` was never called makes A5 take the seed-only path: state equals a fresh `Initialise`, health Degraded, one Warning, `IsReady()` true.
19. `HoldGeneration()` in `DoStart` guarantees the first `WorldGenerationComplete` is received.

### The stage
20. `Capture(stepDay)` derives the week as `WeekOf(stepDay)` and week-of-year as `WeekOf(stepDay) % 52` from the real constants. A retried step (a represented day earlier than the kick day) publishes the week of the **represented** day.
21. `Commit` publishes exactly one `HydrologyUpdated` per applied step, after `sim.Commit`, with the committed week already set. A discarded step publishes nothing.
22. A step captured under revision `r` that commits after `WorldGenerationComplete(r+1)` is discarded: no commit, no message, a counter and an Info log. The new world's state is untouched.
23. Promotion happens only inside `Capture`. A finished spin-up while a step is in flight does not change the sim that `Run` uses (proved with a fake executor whose `ParallelFor` observes the sim pointer before and after).
24. After `Capture`, mutating the module's source state (parameters, queue, provider results, the fixed driver, A4's committed state) does not change `Run`'s result (the DFS-002 poison test at module level).
25. `Run` never calls a provider, the source's `CopyGroundWaterInfluence`, or `GetWorkingOutput` (providers that fail the test when called off SimPU).
26. In a three-stage chain with fake `weather` and `ecology` stages, A5's `Run` sees the working copy that `weather` wrote in the same `Run`, and `weather`'s next `Capture` sees A5's committed state from the previous step.
27. A5 never reads the source pointers in `Commit`: a fake source that swaps its buffers at its own `Commit` (which runs first) does not change A5's committed result.

### Inputs: source and driver
28. With a source, `WaterInput` pointers equal the source's working pointers for that step, and `week` is set.
29. With no source and a `constant` driver, every week gets the same `waterToGround` and `temperature`, and `snowPack`/`glacierInput` are zero.
30. With a `script` driver, week `w` reads entry `w mod length`.
31. With neither source nor driver, the module is Unhealthy, `IsReady()` is false, and an Error is logged. Setting a driver afterwards (`SetFixedInput`) makes it ready at the next revision check.
32. A source and a configured driver together use the source and log one Info.
33. A bad view (false return, null required pointer, wrong `cellCount`) makes the step invalid. `Run` does nothing, `Commit` discards, health is Degraded, one Error is logged, and the next step works.

### Sinks and influences
34. With no provider, `WaterSinks` holds null arrays and the result equals Feature 2's neutral case.
35. Providers are called in ascending `StringCRC` id order whatever the registration order. Two providers registered in either order give bit-identical `etMultiplier` and `draw`.
36. `etMultiplier` is the product and `draw` is the sum of the providers' contributions. Feature 2 AC 26 holds end to end through the module.
37. A non-finite or negative provider output is replaced by neutral, logged once per provider id, and Degraded. Other providers and other cells are unaffected.
38. `RegisterSinkProvider` returns false for null, a duplicate id, and a ninth provider. `UnregisterSinkProvider` removes at once; the next `Capture` does not call it.
39. A positive ground-water influence becomes `extraWater`; a negative one becomes an added `draw`. Water balance closes for both (`drawOut`/`drawUnmet` account for the drier case).
40. With no active influence, `extraWater` is null and `draw` is unchanged.

### Coupling to A4
41. `IHydrologyInputsProvider` returns copies of `surfaceWetness`, `openWater` and `glacierEligible` from **committed** state. Poisoning the provider's source after A4's `Capture` does not change A4's `Run`. It is called only on the capturing thread.
42. `SurfaceWetness` follows the formula above, clamped to `[0,1]`, and equals the provider's copy.
43. A step committed by A5 changes the provider's output only after `Commit`, never during `Run`.

### Snapshot and readiness
44. `GetSnapshot().GetCellCount() == 0` before the first world is ready, and equals the world's cell count afterwards. `GetWeek()` equals the last published week.
45. A reader inside a `HydrologyUpdated` handler sees the committed state of that step. A reader between `Capture` and `Commit` still sees the previous committed state, **including the previous biome**.
46. `SoilMoisture` is `[0,1]`, is 0 on a zero-capacity biome and is 1 on an inundated cell. `SoilStore` is the raw store.
46a. **`RiverState`:** `Frozen` when `ice ≥ ice_crossing_min`; `Dry` below 0.1 of the mean; `Low` below 0.7; `Normal` up to bankfull; `Flood` above. `Frozen` wins over `Dry`, so a frozen river is never reported as dry. A river with `meanFlow` below `min_flow` is judged against `min_flow`.

### Parameters, queue, biome edits
47. A missing file, a bad value or an unknown key falls back to defaults, logs an Error naming the dotted key, reports Degraded, and the module still steps.
48. `QueueParameterSet` with a valid value takes effect at the next `Capture` and not before. An invalid value returns false at once with the error and queues nothing. A `biome_table.<BiomeId>.<column>` key works.
49. `QueueParameterOverrides` is atomic. A queued reload re-reads the file at the next `Capture`; a failing reload keeps the previous parameters and logs.
50. Queued mutations drain in arrival order inside `Capture`, before `sim.Capture`. A 129th entry returns false with one Warning and degrades health.
51. A biome edit queued during a step takes effect at the **next** `Capture` and changes the next step's `soil_capacity`/`infil_rate`/`veg_factor` choice. The step in flight is unchanged.
52. The module never re-reads the parameter file after init except through a queued reload.

### Timing and determinism
53. `Run` records wall time, and `LastStepMs()` returns it after `Commit`. A discarded step does not update it.
54. **Deferred equals synchronous:** `"deferred": true` and `"deferred": false` give bit-identical committed fields for A5 (and for fake A4) after 104 weeks.
55. Bit-identical at 1, 2, 7 and 16 ranges through the module with the real capped executor.

### Lifecycle
56. Enter, exit, enter: nothing from the first visit survives (no stage registration, no pending sim, no queued mutation, no provider, no spin-up result, no source). A job that finishes after exit is discarded.
57. `DoStop` during a running own job cancels and joins it without a crash.

## Files Touched

| File | Change |
|---|---|
| `Cluiche/CoW/Modules/HydrologyModule.h/.cpp`, `HydrologyStage.h/.cpp`, `HydrologyMessages.h` | New |
| `Cluiche/CoW/Shared/Hydrology/HydrologyCoupling.h` | New: `WaterSourceView`, `IWaterInputSource`, `IWaterSinkProvider`, `WaterSinkBuffers`, `IHydrologySpinUpHost` |
| `Cluiche/CoW/Shared/Hydrology/HydrologyTerrainBuilder.h/.cpp`, `HydrologyRunner.h/.cpp`, `HydrologySpinUp.h/.cpp`, `HydrologySnapshot.h/.cpp` | New (pure) |
| `Cluiche/CoW/Shared/Hydrology/HydrologySimulation.h/.cpp` | Edit: `Capture`/`Run`/`Commit`, `SetCommittedWeek`, working accessors (Features 1–2 class) |
| `Cluiche/CoW/Shared/Hydrology/HydrologyParameters.h/.cpp`, `hydrology_parameters.json` | Edit: two `wetness.*` rows (Feature 1 amendment) |
| `Cluiche/CoW/Shared/Hydrology/Testing/` | New: fake source, fake sink provider, fake world source (test utilities ship with the library) |
| `Cluiche/Tests/GoogleTests/CoW/Hydrology/{TestHydrologyModule,TestHydrologyStage,TestHydrologySpinUp,TestHydrologySinks,TestHydrologyTerrainBuilder}.cpp` | New |
| `Cluiche/Assets/CoW/Stages/*/misc/ApplicationFlow/*.diaapp` | The hosting stages declare their own instance (S-2.6, S-3.5 own theirs) |
| `CoW.vcxproj`, `GoogleTests.vcxproj` (+ filters) | Via `dia docs vcxproj-add` |

**Spec edits owed on approval (none are Dia):**
- A4 Feature 3: `WeatherModule` implements `IWaterInputSource`; its job calls `IHydrologySpinUpHost`; its working buffers are allocated once per promotion; it exposes `IsReadyForRevision`.
- A4 Feature 7: the influence record offers the summed signed ground-water array.
- A5 Features 1 and 2: the `Capture`/`Run`/`Commit` split and the two `wetness.*` rows.
- The parent spec's Features table row 3.

**No Dia files are added or edited, so no AD-001 sign-off.** `JobSystemService::TrySubmit`, the capped executor and `WorldStepModule::RegisterStage` are used as Approved. If the plan finds the capped executor needs a change, stop and flag it (AD-001).

## Binding Decisions

| ID | Decision | How this feature complies |
|----|----------|---------------------------|
| PD-002 | ProcessingUnit/Phase/Module architecture; no singletons; PU boundaries are streams | SimModule on SimPU. Collaborators come by `ModuleRef` and pushed-in interfaces. Nothing crosses a PU; `Run` touches only captured copies |
| PD-004 | No STL containers in public APIs | `DynamicArrayC`, raw pointer views, `String256` errors. The provider table and queue are fixed-capacity Dia arrays |
| AD-001 | Ask permission before Dia edits | None needed (see Files Touched) |
| AD-002 | Four-tier model | Pure logic in CoW-Shared; module, stage and messages in CoW `Modules/` |
| AD-003 | GDD owns design intent | Follows the locked GDD (2026-11-17). Remaining differences are listed below |
| DFS-002 | `Run` reads only the captured working copy | `Capture` latches everything. Promotion and mutation drain happen only inside `Capture`. A5 reads A4's working buffers, never SimPU-owned state; ACs 23–25, 27 |
| DFS-003 | A late step stalls; the key never slips | The stage never re-times anything. A discarded step is a loss, counted and logged, never a retime |
| SD-PERF-7 | Telemetry cost budgets | Only SimPU writes health and logs. One step-time value per applied step. No per-cell telemetry |
| World Step Resolved Decision 4 | Order is declarative | The module registers by id `hydrology` and knows nothing of other stages' order; it reaches A4 only through the injected source |
| DFS-005 | Saves store pending results | Feature 4 |

## GDD differences

Only real remaining differences (aligned to locked GDD 2026-11-17). The user's 2026-11-17 decisions win where they differ.

| Topic | GDD | This spec |
|---|---|---|
| Spin-up with no A4 | Not stated | A5 spins up alone on the fixed driver, in its own job, seeded with `start_coldness` and no glacier mask |
| Wetter influence | Added to `surface_water` with `INFLUENCE_GROUND_WATER` (0.05) times A4's weight | Added as `extraWater`. A5 holds no influence constant; A4 Feature 7 hands the amount in as depth. If A4 hands a weight instead, the 0.05 moves into A5 (Open Design Question 4) |
| Drier influence | Taken from `soil` only, clipped to soil, counted as `influence_out` | A draw, taken from surface, then soil, then channel store (Feature 2 pass 4), counted as `drawOut` (user decision: injected sinks) |
| Fixed-input driver | Supplies W, T and G | Supplies `waterToGround` and `temperature`. `snowPack` and `glacierInput` are zero |
| Sink providers | None (A1 reads soil wetness only) | Up to 8 `IWaterSinkProvider`s fill an ET multiplier and a draw (user decision) |
| Init failure | EC-14: the world goes down `WorldGenerationFailed` | A5 goes Unhealthy and not ready. It does not publish S-1's message |
| Load entry point | `GameLoaded` and `restore_state()` | An E1 participant (Feature 4) |
| Terrain read | `WLD.get_hydrology_terrain()` | `HydrologyTerrainSource` read from `WorldGenerationModule` |

No longer differences, now aligned: spin-up length and the phase-scaled seed (GDD §13), version-1 worlds (EC-15: a lake-only table, lakes `Deep` except protected cells), and the wetness constants (all in Feature 1).

## Test Plan

TDD, RED then GREEN. GoogleTests on hand-built terrain, a fake `WorldStepModule` that records registrations and a driver that calls `Capture`/`Run`/`Commit`, a fake world-event source, a fake `IWaterInputSource` (with poisonable buffers), fake sink providers, a fake message bus, and the synchronous launcher. One integration test uses the real capped executor with 2 workers.

TDD order:
1. `BuildHydrologyTerrain`
2. `HydrologySimulation` `Capture`/`Run`/`Commit` split (`Step` equals the composition)
3. `HydrologyRunner` and the pure spin-up
4. `HydrologySnapshot` and `SurfaceWetness`
5. Sink table, sink latching and influence translation
6. Source and driver latching
7. Stage week derivation and revision guards
8. Queue and parameter latching
9. Alone spin-up through the module
10. Coupled spin-up with the fake A4
11. Lifecycle

Scoped `--filter=*Hydrology*` while iterating; one full `--shards 2` run at the end. No mutation checks. The sanitizer runs only if asked. Gap audit and edge pass is the last plan task. **Observation scan** at verify (expected finds: a trace zone around ingest, a health reason per Degraded cause).

## Open Design Questions

1. **Ingest and `Initialise` run on SimPU.** The static tables (topological trees, capacity curves) are cheap at Small, but at Large with many basins they may cost milliseconds on the event handler. Measure it in the first S-2.6 run. If it shows, move the ingest into the spin-up job and keep only the world copy on SimPU.
2. **A4 restored from a save, A5 not (or the reverse).** A4's coupled job is skipped when A4 restores, so A5 has no partner and only seeds (Degraded). That is honest but leaves the first year drifting. Alternatives: refuse the load, or run A5 alone for 52 weeks on A4's committed temperature and a mean rain. Not chosen because a mean rain is an invented input. The reverse (A5 restored, A4 absent from the save) is handled: `BeginSpinUp` returns null and A4 spins alone.
3. ~~**Does a river have open water for A4's primary evaporation?**~~ **Resolved by GDD §14.2:** `openWater` of a river cell is `RIVER_OPEN_WATER · min(1, flow / max(mean, MIN_MEAN_FLOW))`. Feature 2 pass 7 sets it, and Feature 1 holds `wetness.river_open_water` (0.3).
4. **Who applies `INFLUENCE_GROUND_WATER`?** The GDD has A4 return a weight and A5 multiply it by 0.05. This spec has A4 Feature 7 hand A5 a depth. A4 Feature 7 is not written. Settle it there, and move the constant into Feature 1 if A5 should own it.

## Status

**Status:** Approved (2026-11-17, user, after the locked-GDD alignment). Decisions encoded from the user session the same day:
- `HydrologyModule` is a CoW-Shared-backed SimModule and the `hydrology` World Step stage. It never kicks.
- Wiring is by interfaces through `ModuleRef` (no singletons, no ServiceStreams): sink providers (capacity 8, ascending id), `SetWaterSource`, A4's `IHydrologyInputsProvider`.
- A5's `Run` reads A4's working copy from the same `Run` (order weather → hydrology → ecology).
- A fixed-input driver covers the no-A4 case.
- Spin-up is coupled with A4 in A4's job (52 weeks, cycles held at the start phase, slow stores seeded for that phase). A5 spins up alone without A4. Not-ready until done; a newer world cancels.
- Synchronous mode is the determinism oracle. Step timing is Feature 3's, the metric is Feature 5's.

**Needs the user's attention before approval:**
- A4 Feature 3 owes `IWaterInputSource`, the spin-up host calls (now with `SpinUpStart`: per-cell coldness and a per-cell snow-survival mask, both from A4 Feature 8; seam fix 2026-11-17) and `IsReadyForRevision`. A4 owes `ClimateCycles::Coldness`, `WeatherRunner::StartColdness`, the firn hand-off (A4 TechSpec §11.3) and the mask rule.
- Open Design Questions 1, 2 and 4.

Aligned to the locked GDD on 2026-11-17. Approval is a separate step.

Plan: [hydrology-module.plan.md](hydrology-module.plan.md) (written 2026-11-17, not started).
