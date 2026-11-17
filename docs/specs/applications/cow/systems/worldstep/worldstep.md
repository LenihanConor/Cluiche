# System Spec: World Step

**Parent:** @docs/specs/applications/cow/cow.md

**Gameplay Domains:** simulation, persistence

**Amended 2026-11-17, A5 Hydrology:** a third stage, `hydrology` (A5, [a5-hydrology.md](../a5-hydrology/a5-hydrology.md)), sits between `weather` and `ecology`. The chain is now A4 → A5 → A1 (A3 inline). Stage ids are `["weather", "hydrology", "ecology"]` in S-3.5. A5 reads A4's working copy inside the same `Run`; A4 reads A5's previous committed state at `Capture`.

## Summary

World Step is the thin coordinator that runs CoW's world simulation as **one deferred step**. It owns the `world` DiaDeferredStep channel, decides when to kick it, orders the systems inside it (A4 Weather, then A5 Hydrology, then A1 Ecology; amended 2026-11-17, A5 Hydrology), commits their results in order, and samples the result into DiaCellHistory.

It is deliberately small. It owns **cadence and ordering** and nothing else: no simulation logic, no world data, no cell layers, no debug surface beyond two status commands. A4, A5 and A1 keep their own modules, parameters, debug domains, commands, layers and save participants, and register with this coordinator as **stages**.

The stage list is declarative, so a testbed composes exactly the layers it needs — weather alone for S-2.5, ecology alone for S-3 Controlled mode (optionally with hydrology), weather plus hydrology for S-2.6 Rivers & Floods, all three for S-3.5 (amended 2026-11-17, A5 Hydrology) — through `.diaapp` config rather than code.

## Problem

DiaDeferredStep makes inter-channel ordering an explicit **non**-responsibility: *"A chain whose stages depend on each other (A4 → A1 → A3; now A4 → A5 → A1 → A3, amended 2026-11-17) runs as one channel, and the consumer orders the stages inside `Run`."* So something on the CoW side must own that channel and that ordering. Nothing does today.

Three further gaps fall out of the same hole:

1. **Who kicks?** A4's and A1's specs each have their module stepping itself off `SimulationTick`. Under one channel, exactly one component may kick, and it must know every stage is ready.
2. **Who samples history?** Both A4's and A1's specs call DiaCellHistory `Sample()` from their own apply. Two samples per world week with different layer sets makes the scrub timeline ambiguous.
3. **Who drains before a save?** DFS-005 requires in-flight steps drained and pending results serialized. Neither A4 nor A1 can do this alone, because neither knows about the other's step.

Putting all of that in A4 would make A1's spec depend on an A4 internal. Putting it in a combined world module would break S-2.5 (A4 alone) and S-3 Controlled mode (ecology alone), which both exist in approved specs.

## Goals

1. **One channel, correctly ordered.** Register the `world` channel with DiaDeferredStep, run every enabled stage inside one `Run` in declared order, and commit in the same order.
2. **Declarative composition.** The enabled stages and their order come from `.diaapp` config, so a stage hosting only weather is a config difference, not a code path.
3. **Correct cadence.** Kick once per game week on `SimulationTick` with `band >= TimeBand::Week`, with a fixed apply horizon `k` in days, and never kick until every enabled stage is ready.
4. **One history timeline.** Signal `HistoryDriverModule` once per applied step, after every stage has committed, so a world week yields exactly one sample at the correct key.
5. **Save-safe.** Drain in-flight steps before E1 serializes, so DFS-005's pending-result rule has a single owner.
6. **A synchronous mode that is a supported configuration**, not test scaffolding: it is the simple path for single-layer testbeds and the determinism oracle for the deferred path.
7. **Observable without being chatty.** Two sim-hosted commands, metrics for stall and slack, health on repeated stalls.

## Non-Goals

1. **Any simulation logic.** Weather, ecology and succession stay in A4, A1 and A3.
2. **World data or the hex grid.** S-1 owns `WorldStructure` and the shared `WorldHexGrid` / `CellGridDesc` (S-1 amendment, 2026-11-13). Stages get the grid from S-1 directly; the coordinator never touches it.
3. **Cell layers, debug domains, panels, inspectors.** Each system registers its own.
4. **Scheduling, the job pool, the worker cap, the stall policy.** DiaDeferredStep owns all of it.
5. **A3 as a stage.** A3 resolves **synchronously inside A1's step 10** via `ISuccessionResolver`, deliberately with no bus lag. The chain is therefore orderable stages only, with A3 never one of them: **three** since A5 joined (`weather`, `hydrology`, `ecology`), two before (amended 2026-11-17, A5 Hydrology).
5b. **History sampling itself** (revised 2026-11-13). CoW-Shared `HistoryDriverModule` owns `Sample()`, label arithmetic, band mapping, reset on bundle load and the world id. The coordinator only tells it when a step applied.
6. **Choosing `k` per band.** See Resolved Design Decisions 1 — one fixed `k`, measured before revisiting.
7. **Weather or ecology notifications.** Each stage publishes its own (`WeatherUpdated`, `CellEcologyChanged` …) from its own `Commit`.

## Architecture

### Placement

**CoW-Shared** (AD-002). A4, A1, S-2.5, S-3 and S-3.5 all consume it. Pure logic in `Cluiche/CoW/Shared/WorldStep/`; the SimModule in `Cluiche/CoW/Modules/`.

```
SimPU
  └─ WorldStepModule (SimModule)              implements Dia::DeferredStep::IDeferredStep
       on SimulationTick (band >= Week) → Kick(world, day, day + k)   [if every stage IsReady]
       Capture()  → each enabled stage captures into its own working copy
       Run(exec)  → stage[0].Run(exec); stage[1].Run(exec);  …  (declared order)
       Apply()    → each stage Commit() in order, each publishes its own message,
                    then signal HistoryDriverModule::OnWorldStepApplied(key)
       pre-save   → DrainAll(), then stages serialize their pending results
       owns: the channel, cadence, k, the stage table, history sampling

  ├─ WeatherModule (A4)  → registers stage "weather"
  ├─ HydrologyModule (A5) → registers stage "hydrology"   (amended 2026-11-17, A5 Hydrology)
  └─ EcologyModule (A1)  → registers stage "ecology"   (A3 resolves inside it)
```

### The stage contract

```cpp
namespace CoW::WorldStep {

    // A world-sim layer that participates in the deferred world step.
    // Capture and Commit run on SimPU; Run runs on worker threads.
    class IWorldStepStage {
    public:
        virtual ~IWorldStepStage() = default;

        virtual bool IsReady() const = 0;                       // initialised and spun up
        virtual void Capture(StepKey key) = 0;             // SimPU: latch inputs into the working copy
        virtual void Run(Dia::DeferredStep::IParallelExecutor&) = 0;   // workers: working copy only
        virtual void Commit() = 0;                              // SimPU: swap, then publish own message

        // DFS-005: a completed but unapplied working copy.
        virtual void SerializePending(Dia::SaveGame::SaveContext&) const = 0;
        virtual void DeserializePending(Dia::SaveGame::LoadContext&) = 0;
    };
}
```

`Capture` receives the kick key (the game day) so a stage can resolve its own band set — A1 uses it to decide which of Season/Year/Decade/Century are due. The coordinator does not interpret bands.

### Ordering is declarative, not registration order

Stage order is semantically critical — ecology reads the weather this step produced — so it must not depend on module start order. The coordinator reads an ordered id list from config and matches registrations against it:

**The module is stage-scoped, not `stages: ["all"]`.** Its stages (`WeatherModule`, `HydrologyModule`, `EcologyModule`) are themselves stage-scoped, and different testbeds need different `order` lists, so each hosting stage declares its own instance with its own config:

```json
{
    "instance_id": "WorldStepModule",
    "type_id": "WorldStepModule",
    "stages": ["S35WorldSimStage"],
    "dependencies": ["DeferredStepModule", "MessageBusHostModule", "SimQueryHostModule"],
    "soft_dependencies": ["SaveLoadModule", "HistoryDriverModule"],
    "config": {
        "order": ["weather", "hydrology", "ecology"],
        "apply_after_days": 7,
        "deferred": true
    }
}
```

S-2.5 declares its own instance with `"order": ["weather"]`, S-2.6 Rivers & Floods with `["weather", "hydrology"]`, S-3 Controlled with `["ecology"]` (optionally `["hydrology", "ecology"]`), S-3.5 with all three (amended 2026-11-17, A5 Hydrology). A single `stages: ["all"]` instance could not serve both, and would also keep the coordinator alive on stages that run no world sim.

Resolution rules, which differ by lifecycle phase — this distinction matters, because stages legitimately come and go:

| Situation | Behaviour |
|---|---|
| A registered stage is **not** in `order` | Skipped, one Warning naming it |
| An id in `order` has not registered **yet** (startup, before every module's `DoStart`) | Normal. No kicks, no error; the coordinator waits |
| An id in `order` has not registered by the first `SimulationTick` after start | **Config error:** Error log, health Failing, no kicks. Catches a mis-composed stage instead of silently running half a world |
| A stage **unregisters** while the coordinator is live (stage exit) | Normal teardown, not an error. Kicks stop; if a step is in flight it is drained first |

### Lifecycle

- **Stage exit / `DoStop`.** The coordinator unregisters the `world` channel, which drains first. The drained step is **abandoned, not committed** — `DrainAll` does not apply by design (Feature 1 AC 26), and a stage that is tearing down has nothing to commit into. So no `Commit` and no history sample run during teardown, and the last published week is the last *applied* one. A stage that unregisters before the coordinator is dropped from the chain and its pending work is discarded.
- **Re-entry.** Nothing persists across a stage visit: the channel, the stage table and the key history are rebuilt from config on the next `DoStart`.
- **Catch-up.** DiaSimTime's accumulator can drain several sim steps in one real frame (ST-011), and DiaDeferredStep applies every due step in that frame. The world sim deliberately **lags** under heavy catch-up rather than running several weeks in one `Run` — the apply key never slips (DFS-003), so catch-up produces more applies, not bigger ones. See Open Design Question 2.

### Cadence

- Kick on `SimulationTick` with **`band >= TimeBand::Week`**. The `>=` is mandatory: `ResolveBand` returns only the *largest* band a day completes, so `== Week` would skip the four weeks a year that are also Season/Year boundaries.
- `kickKey` is the game day; `applyKey` is `day + apply_after_days` (default **7**, one game week).
- **No kick unless every enabled stage reports `IsReady()`.** Until then the coordinator idles and logs once.
- **A kick that finds the channel in flight is retried, not skipped (amended 2026-11-16, Feature 2).** One pending week is remembered and kicked on the next free tick with the *missed* day as the stage key and `applyKey = current day + k`; a second miss while one is pending is an Error and a lost week. A late kick is a reported anomaly (Warning, `late_kicks`, Degraded).

**Slack.** At 30 Hz, `k = 7` days is 7 sim frames (~233 ms) at the Fastest preset and minutes at Slow. Against a combined A4 + A1 Season cost of roughly 26 ms CPU (~13 ms wall on a 2-worker cap) that is more than an order of magnitude of headroom. **A5 adds a third cost (amended 2026-11-17, A5 Hydrology):** `cow.hydro.step_ms` is report-only, with no gate, and is not in the 26 ms figure. Its river routing is serial within a river tree. The slack estimate must be re-measured with A5 in, and a miss stalls SimPU and is reported (DFS-003). The exception is the Century tick — see Resolved Design Decisions 1.

### Synchronous mode

With `"deferred": false` the coordinator runs `Capture` → `Run` → `Commit` inline on SimPU in the same `DoUpdate`, through a `SerialExecutor` or the capped executor. Nothing else changes: the same stages, the same order, the same sampling.

This exists for two reasons, both load-bearing:

- **Single-layer testbeds don't need deferral.** S-2.5 runs weather alone at ~10 ms CPU inside a 33 ms frame.
- **It is the determinism oracle.** A deferred run must be bit-identical to a synchronous run of the same weeks. That is a far stronger check than serial-vs-parallel alone, because it also covers capture isolation and apply ordering.

### History sampling

**The coordinator does not sample. It signals** (revised 2026-11-13, user decision — see Resolved Decision 2).

After every stage has committed, the coordinator raises `OnWorldStepApplied({ applyKey, stepDay, stageCount })` **synchronously inside `Apply`** to registered `IWorldStepListener`s (Feature 3, amended 2026-11-16: a listener interface rather than a direct call, so there is no compile-time dependency on `HistoryDriverModule`). CoW-Shared `HistoryDriverModule` (S-3.5 Feature 1) registers as a listener and owns `Sample()` and everything around it: label arithmetic, band mapping, reset on bundle load, and the world id from terrain `Capture()`. It labels the sample by **`stepDay`**, the day the step represents, not by the apply key (Feature 3).

- **One sampler, one timeline.** A4's and A1's own `Sample()` calls are dropped, and the coordinator never acquires one.
- **The key is correct under deferred execution.** This is the whole reason the trigger moves: `HistoryDriverModule`'s original trigger was a game-clock tick, which under deferred stepping would sample at tick N while the world state it reads is from week N−k — silently mislabelled data. A synchronous signal from `Apply` cannot drift.
- **Stages with no world step are unaffected.** `HistoryDriverModule` keeps its clock trigger there (S-1's own testbed, T-1 fixtures), so nothing that records history today changes.
- If no listener is registered the signal is a no-op — history is a soft dependency.

`history_band`, the `bands` name list and the `uint8_t` band index all stay `HistoryDriverModule`'s concern, not the coordinator's.

### Save and load

**The coordinator writes no save code and E1 needs no amendment** (corrected 2026-11-13 against DiaDeferredStep Feature 3, `save-load.md`). DiaDeferredStep owns the whole pending-result rule:

- It drains automatically on `SaveManager::OnSaveStarted`, which already fires synchronously before any `Serialize`. That resolved its own Open Design Question 1, and the planned "amend E1's save to call `DrainAll`" was **dropped**.
- It owns the single `ISaveable` participant holding every channel's pending result and apply key. There is no `"worldstep"` participant.
- It abandons in-flight steps on `OnLoadStarted`.

So the coordinator's only obligation is to implement `SerializePending` / `DeserializePending` on itself, delegating to each enabled stage in declared order and tagging each blob with the stage id. On load, a pending result restored for a stage that is **not** in the current `order` is discarded with a Warning — a save taken on S-3.5 must load cleanly on S-2.5.

Note `DrainAll` deliberately **does not apply** (DiaDeferredStep Feature 1 AC 26): applying at drain time would change the outcome versus the uninterrupted run. That shapes shutdown — see Lifecycle.

### Observability

Deliberately minimal, because the sim-hosted name budget is tight (58/64 of S-3.5's cap is already consumed by built code). **Two names only:**

| Name | Kind | Effect |
|---|---|---|
| `cow.worldstep.status` | query | enabled stages and order, readiness, mode, `k`, last kick/apply keys, last stall ms, in-flight state |
| `cow.worldstep.step` | command | debug: force one kick now, ignoring the week gate (refused while a step is in flight) |

Both are sim-hosted through `SimQueryHostModule` with mandatory `SimHelpInfo`, category `worldstep`. **A partial registration rolls back** every name registered in that call — the failure mode that leaks names permanently in `WorldGenerationModule` today.

- **Metrics:** `cow.worldstep.step_ms` (total CPU across stages, with `MetricInfo`), `cow.worldstep.slack_days` at completion, `cow.worldstep.stalls`, `cow.worldstep.late_kicks`, `cow.worldstep.lost_weeks`.
- **Health:** Degraded on any stall or late kick in the last 16 applied steps; Failing on a missing configured stage.
- **Logs** (`WST` channel): Info on configure (mode, order, `k`), on first readiness, on drain. Warning on a late kick or an unknown registered stage. Error on a missing configured stage.
- **Trace and profiling:** `DIA_TRACE_ZONE` on Capture, Run and Apply; `DIA_PROFILE_SCOPE` per stage inside each phase, so the Perf Inspector can split weather from ecology. Worker scopes carry the `world` channel as subject through DiaDeferredStep's job attribution.

## Public Interfaces

```cpp
namespace CoW::WorldStep {

    // The key is DiaDeferredStep's, not a CoW alias: the game day index.
    using Dia::DeferredStep::StepKey;
    inline constexpr const char* kChannelId = "world";

    class IWorldStepStage { /* see The stage contract */ };

    // Pure logic: stage table, declared order, readiness gating, phase sequencing.
    // No Dia module dependencies; unit-tested with fake stages.
    class WorldStepChain {                                   // full contract: step-chain.md
    public:
        bool Configure(const WorldStepConfig& config);
        bool RegisterStage(Dia::Core::StringCRC id, IWorldStepStage& stage);
        void UnregisterStage(Dia::Core::StringCRC id);      // removal deferred to the step boundary
        bool AllReady() const;
        bool MissingStages(StageIdList& out) const;
        bool UnknownStages(StageIdList& out) const;
        bool Capture(StepKey key);                          // false if out of sequence
        bool Run(Dia::DeferredStep::IParallelExecutor& executor);
        bool Commit();
        void Abandon();                                     // discard the in-flight step, no Commit
        ChainPhase Phase() const;
        uint32_t EnabledCount() const;
        IWorldStepStage* StageAt(uint32_t index) const;
    };

    struct WorldStepConfig {
        StageIdList order;
        uint32_t    applyAfterDays = 7;
        bool        deferred       = true;
    };
    bool LoadWorldStepConfig(const Json::Value& root, WorldStepConfig& out);

    // SimModule wiring: the channel, the cadence, sampling, the save hook.
    class WorldStepModule : public Dia::ApplicationFlow::SimModule,
                            public Dia::DeferredStep::IDeferredStep {
    public:
        static const Dia::Core::StringCRC kTypeId;

        void RegisterStage(Dia::Core::StringCRC id, IWorldStepStage& stage);   // A4, A1 call this in DoStart
        void UnregisterStage(Dia::Core::StringCRC id);

        // IDeferredStep
        void Capture() override;
        void Run(Dia::DeferredStep::IParallelExecutor&) override;
        void Apply() override;
        void SerializePending(Dia::SaveGame::SaveContext&) const override;
        void DeserializePending(Dia::SaveGame::LoadContext&) override;

        bool         IsDeferred() const;
        StepKey LastAppliedKey() const;
    };
}
```

## Features

| # | Feature | Description | Spec | Status |
|---|---------|-------------|------|--------|
| 1 | Step Chain | `IWorldStepStage`, `WorldStepChain`, `WorldStepConfig` + JSON load, declared-order resolution, skipped and missing stage handling, readiness gating, Capture/Run/Commit sequencing. Pure logic, TDD with fake stages | @docs/specs/applications/cow/systems/worldstep/step-chain.md | Approved |
| 2 | Module & Cadence | `WorldStepModule`: `.diaapp` config, DiaDeferredStep channel registration, `SimulationTick` week gate with `>=`, `k`, kick gating on readiness and in-flight, synchronous mode, the two sim-hosted commands with rollback, metrics, health, logs, trace | @docs/specs/applications/cow/systems/worldstep/module-and-cadence.md | Approved |
| 3 | Save & History Signal | `SerializePending` / `DeserializePending` delegating to stages in declared order, stage-id tagging, discarding a pending result for a stage absent from `order`, and the single synchronous `HistoryDriverModule::OnWorldStepApplied(key)` signal per applied step (no sampling logic here) | @docs/specs/applications/cow/systems/worldstep/save-and-history.md | Approved |

**Build order:** 1 → 2 → 3. Feature 1 needs no Dia code beyond the `IParallelExecutor` declaration and can start as soon as DiaDeferredStep Feature 2 exists.

**Routed elsewhere:** the worker cap, stall policy and scheduler are DiaDeferredStep's. Per-band budgets and the Century measurement are A1's. The stage, panels and perf gates are S-2.5's, S-3's and S-3.5's.

## Binding Decisions

| ID | Decision | How it applies |
|----|----------|----------------|
| PD-002 | ProcessingUnit/Phase/Module architecture | `WorldStepModule` is a SimModule on SimPU. Stages arrive by explicit registration, never a singleton or service locator |
| PD-004 | No STL containers in public APIs | `StageIdList` is a `DynamicArrayC`. `RangeFn` follows DiaThreading's existing `std::function` precedent |
| AD-002 | Four-tier dependency model | CoW-Shared: A4, A1, S-2.5, S-3 and S-3.5 consume it |
| AD-003 | GDD owns design intent | **No GDD artifact exists or is needed.** World Step is an engineering consequence of DiaDeferredStep's ordering non-responsibility, not game design: it adds no mechanic, constant or player-visible behaviour. Nothing here is owed to the DesignDocs owner, and there is no Deviations section for the same reason |
| AD-005 | One stage per testbed | World Step owns no stage. Testbeds host it and compose `order` |
| DFS-002 | `Run` reads only the captured working copy | The coordinator calls `Capture` on every stage before any `Run`, and a test poisons a stage's source after capture |
| DFS-003 | A late step stalls at its apply key; the key never slips | The coordinator reports the stall; it does not slip `k` to avoid it |
| DFS-005 | Saves drain, then store pending results with apply keys | Owned entirely by DiaDeferredStep Feature 3 (automatic drain on `OnSaveStarted`, one `ISaveable` for every channel). The coordinator only implements `SerializePending` / `DeserializePending` and delegates to stages |
| SD-ENT-022 | Component access rule | Stages are registered interfaces, not statics; the coordinator holds borrowed references and drops them on `UnregisterStage` |

## Dependencies

**Requires:**
- **DiaDeferredStep** — system Approved 2026-11-12 (amended 2026-11-13); **all five feature specs Approved 2026-11-13 with plans written, `Not Started`.** Supplies `IDeferredStep`, `IParallelExecutor::ParallelFor(count, minRange, fn)`, `StepKey`, `IStepLauncher` / `LaunchToken`, `DeferredStepScheduler` (`Kick` / `Tick` / `DrainAll`), `DeferredStepModule`, the strict worker cap (`CapGate`, coordinators included) and the stall policy. World Step Feature 1 can start once DeferredStep **Feature 1 Task 1** ships the headers. Worker-thread trace/profile attribution for the `world` channel comes from **Feature 5** (`job-attribution`), not Feature 4.
- **`IStepKeySource`** — DeferredStep Feature 4 takes the current key by injection. CoW supplies the game day; confirm during Feature 2 whether World Step or GameSpeedController is the natural implementer.
- **GameSpeedController** — built. `SimulationTick`, `TimeBand`, `WeekOf`.
- **DiaSimTime** — built, via DiaDeferredStep's tick ordering.
- **E1 `SaveLoadModule`** — built, **unchanged**. No amendment: DiaDeferredStep Feature 3 drains on `SaveManager::OnSaveStarted`, which already fires before any `Serialize`.
- **CoW-Shared `HistoryDriverModule`** (S-3.5 Feature 1, Approved 2026-10-20, not built) — owns `Sample()`, label arithmetic, band mapping, reset on bundle load and the world id. The coordinator signals it and nothing more. Soft: absent, the signal is a no-op. The AC8 trigger amendment (Resolved Decision 2) is written into S-3.5 Feature 1; the driver auto-detects world-step vs clock mode by resolving `WorldStepModule` at init (amended 2026-11-16), so no config key and no double sampling.
- **DiaCellHistory** — built; reached only *through* `HistoryDriverModule`. The coordinator holds no recorder reference.
- **`SimQueryHostModule`** — built. Two commands with `SimHelpInfo`.
- **S-1** — built, and **not a direct dependency**. The coordinator reads no world data, owns no grid and does **not** subscribe to `WorldGenerationComplete`: each stage initialises itself from S-1 and reports through `IsReady()`, which is the coordinator's only readiness signal. S-1 appears here only because the stages need it.
- DiaMessageBus via `MessageBusHostModule`; DiaCore (Json, StringCRC, containers, logging).

**Consumed by (stages):** A4 Weather (`weather`), A5 Hydrology (`hydrology`, amended 2026-11-17; registers its own `IWorldStepStage` and publishes `HydrologyUpdated{week}` from its `Commit`), A1 Ecology (`ecology`, with A3 inline). A4 and A1 need amending to register as stages rather than stepping themselves, and to drop their own `Sample()` calls.

**Required by:** S-2.5 Weather Patterns, S-2.6 Rivers & Floods (amended 2026-11-17), S-3 Ecology, S-3.5 World Sim.

## Test Strategy

**Unit (Feature 1, GoogleTests, fake stages):**
- Config load: a valid order list; an empty list; an unknown id; a duplicate id.
- A registered stage absent from `order` is skipped and warned once.
- A configured stage that never registers reports `MissingStages` and blocks kicks.
- Capture, Run and Commit each visit enabled stages in **declared** order, independent of registration order (register ecology first, assert weather still runs first).
- `AllReady()` is false until every enabled stage is ready.
- Capture isolation: a fake stage poisons its source after `Capture`; `Run` is unaffected.

**Integration (Features 2–3):**
- Exactly one kick per game week; the `>=` gate still kicks on Season/Year/Decade/Century ticks (assert 52 kicks per year, not 48).
- No kick before readiness; no kick while in flight, and the skip is counted and logged.
- A step applies at exactly `kick + k`, even when it finished early.
- **Deferred equals synchronous:** the same stages over 52 weeks in both modes give bit-identical stage state. This is the primary determinism gate.
- A late step stalls, and the stall is reported through log, metric and health.
- Save mid-step then load: the pending result applies at its key and matches the uninterrupted run (DiaDeferredStep owns the drain and the participant; this test covers the coordinator's stage delegation).
- A pending result saved with `["weather","ecology"]` loads on a stage configured `["weather"]`: the ecology blob is discarded with a Warning and the load succeeds. (amended 2026-11-17, A5 Hydrology) The same holds for a saved `hydrology` blob loaded where `hydrology` is not in `order`.
- (amended 2026-11-17, A5 Hydrology) **Three-stage chain:** with fake stages `weather`, `hydrology`, `ecology`, the order is honoured whatever the registration order; `hydrology` sees the working copy `weather` just wrote inside the same `Run`, and `weather` sees `hydrology`'s committed state from the previous step at `Capture`.
- (amended 2026-11-17, A5 Hydrology) **Determinism oracle over three stages:** the same three stages over 104 weeks in deferred and synchronous mode give bit-identical state (A5's own test: deferred vs synchronous over 104 weeks with A4).
- Exactly one `OnWorldStepApplied(key)` signal per applied step, raised after every stage has committed and carrying the apply key; with no `HistoryDriverModule` present it is a no-op and nothing else changes.
- Both commands register with non-empty descriptions, and a forced partial-registration failure leaves **no** names behind.

**Lifecycle (Feature 2):**
- A configured stage that has not registered before the first `SimulationTick` sets health Failing and blocks kicks; registering late during startup does not.
- A stage unregistering while live stops kicks **without** an error, and an in-flight step is drained and committed first.
- `DoStop` with a step in flight drains and abandons it (no commit, no history signal) before the channel is unregistered, matching Lifecycle.
- Stage re-entry (enter, exit, enter) rebuilds the chain from config and kicks again, with no leaked channel registration or stale keys. This is the pattern the CellLayers E2E flakes live in, so it gets an explicit test.
- Under simulated catch-up (several sim steps drained in one real frame) the apply key never slips, and applies are more frequent rather than larger.

**Sanitizer:** ASan/UBSan over the chain tests, including a stage unregistered while a step is in flight.

**Close-out:** gap audit and edge-case pass, scoped `--filter` while iterating, one full `--shards 2` run at the end. No mutation checks.

## Resolved Design Decisions

1. **One `world` channel, kicked weekly, with one fixed `k`** (user decision 2026-11-13). A Century tick runs Season + Year + Decade + Century in a single `Run` and may exceed `k`'s slack, stalling SimPU once per ~100 game-years. That is accepted and reported rather than designed around, because the cost has never been measured.
   - Rejected: **`k` per band.** It does not work on one channel — DiaDeferredStep rejects a kick while a step is in flight, so a Century step with a larger `k` would block the following weekly weather kicks.
   - Rejected: **two channels** (`world.fast` weekly, `world.slow` for heavy bands). It keeps weather flowing, but inter-channel ordering is out of DiaDeferredStep's scope and both channels write ecology state, so the coordinator would own a real read/write hazard. Revisit only if the measurement in Open Design Question 1 justifies it.
   - Rejected: making A1's Century band incremental. A genuine option, but it is an A1 design change and premature before measurement.
2. **`HistoryDriverModule` samples; the coordinator signals it from `Apply`** (user decision 2026-11-13, **revising the same day's earlier decision** that the coordinator would sample).
   - The reversal came from reading S-3.5 Feature 1 properly. `HistoryDriverModule` is already CoW-Shared and already owns far more than a `Sample()` call: label arithmetic, band mapping, reset on bundle load, world id from terrain `Capture()`, every band, and ordering. Moving all of that into a step coordinator would have duplicated a designed, AC-covered module.
   - The objection that killed it first time — that it must *observe* apply rather than know it, costing a frame of lag — was wrong. A **synchronous signal from inside `Apply`** gives it the apply key directly, with no lag and no bus.
   - It also answers a question neither original option did: stages that record history but run **no** world steps (S-1's own testbed, T-1 fixtures) keep `HistoryDriverModule`'s clock trigger and are untouched.
   - What it fixes in S-3.5: AC8 samples *"at the start of the next simulation frame, after all systems handled the tick"*, which assumes **synchronous** same-tick handling. Under deferred stepping that would sample at tick N while reading week N−k state — silently mislabelled history. Only the trigger changes; every other AC stands.
   - Rejected: each stage sampling on its own commit (what A4's and A1's specs said). Two samples per week, different layer sets, ambiguous scrub timeline.
   - Rejected: the coordinator sampling and `HistoryDriverModule` surviving only for non-world-step stages. Two samplers, and a fuzzy boundary where a stage has both.
3. **Synchronous execution is a supported configuration, not test scaffolding** (user decision 2026-11-13). `"deferred": false` gives single-layer testbeds the simple path and makes the deferred-vs-synchronous oracle runnable on a real stage.
4. **Stage order is declarative in `.diaapp`, not registration order.** Ordering is semantically load-bearing and module start order is not a contract. A missing configured stage is a hard error rather than a silent half-world.
5. **S-1 keeps the shared `WorldHexGrid`** (user decision 2026-11-13); the coordinator never touches it. S-1 already builds it from the world dimensions, so only the `DIA_DEBUG`-only encapsulation inside `WorldGenViewHost` has to change — folded into the S-1 post-close amendment set.
6. **A3 is not a stage.** It resolves synchronously inside A1's step 10 through `ISuccessionResolver`, by A1's and A3's existing design, so the chain has three orderable stages (two before A5 joined; amended 2026-11-17, A5 Hydrology).

## Open Design Questions

1. **What does the Century tick actually cost, and does it need the two-channel split?** Decision 1 accepts the stall unmeasured. A1's only perf gate covers the Season band (and Feature 4 measures Season and Year), so Season + Year + Decade + Century combined has never been budgeted — DiaDeferredStep's own spec notes *"the Century frame has no budget at all."* This needs an A1 plan task that measures the combined tick at Large in Release. If it exceeds `k`'s slack by a wide margin, revisit the two-channel option or make A1's Century band incremental.
2. ~~**Is a skipped kick ever acceptable, or always a bug?**~~ **Resolved 2026-11-16 (Feature 2):** a missed week is retried on a following tick with the missed day as the stage key; a second miss is an Error and a lost week. Catch-up is covered by Feature 2 AC 12.
3. ~~**Who implements `IStepKeySource`?**~~ **Resolved 2026-11-16 (Feature 2):** `WorldStepModule`, reading the day from `SimulationTick`. The same-step ordering hazard is Feature 2 Open Q1 and its first plan task.

## Gap-pass amendments (2026-11-16)

Found by the plan-writing pass; each item below **supersedes** any contradicting text above. Numbering is stable.

1. **Interfaces.** `RegisterStage` returns `bool` (false on a duplicate id or a full table). The `WorldStepChain` block is the one in `step-chain.md` (adds `UnknownStages`, `StageIdAt`, `CapturedKey`, `RestoreRan`, `SetObserver`); `LoadWorldStepConfig` takes a `ConfigError&`.
2. **Test Strategy lifecycle line.** "A stage unregistering while live ... in-flight step is drained and committed first" is superseded: the step is drained and **abandoned** (no `Commit`, no history signal), matching Lifecycle.
3. **History signal.** `OnWorldStepApplied({applyKey, stepDay, stageCount})` through `IWorldStepListener` (Feature 3). "Carries the apply key" is superseded.
4. **Build order.** Feature 1 can start once **DiaDeferredStep Feature 1 Task 1** ships the headers (not "Feature 2").
5. **Participant order and key-source timing** are resolved by Feature 3's amendment 5 and Feature 2's amendment 1.

## Status

**Status:** `Approved` (2026-11-13). Interview completed 2026-11-13: module topology, the coordinator's home, band cadence, shared-grid ownership, history sampling and synchronous mode all decided by the user. Arose from the A4 Weather dependency audit (2026-11-13), which found that DiaDeferredStep leaves the `world` channel unowned.

**Amended 2026-11-17 (A5 Hydrology).** A third stage `hydrology` between `weather` and `ecology`; composition examples, the slack note and the determinism oracle cover three stages. Status stays `Approved`. A5 itself is `Draft`, so until it is built `["weather", "ecology"]` remains valid config.

**Not blocked on spec work.** DiaDeferredStep's five feature specs were all Approved 2026-11-13 with plans written (`Not Started`), so the build path is clear: DeferredStep Feature 1 Task 1 ships the headers, after which World Step Feature 1 can start in parallel with the rest of DeferredStep.

**Amendments owed elsewhere on approval:**
- **A4 Weather** — register as stage `weather`; drop its own `Sample()` call (its spec currently places it in `Apply()`).
- **A5 Hydrology** (added 2026-11-17) — registers as stage `hydrology`; publishes `HydrologyUpdated{week}` from `Commit`; owns its own `HydrologyUpdated` message.
- **A1 Ecology** — register as stage `ecology`; drop its own `Sample()`; drop the hand-rolled `mSeasonPending` next-frame deferral and its Open Design Question 1, both of which `Capture`/`Commit` make unnecessary; add the Century-tick measurement task.
- **S-1** — expose the shared `WorldHexGrid` (joins the existing post-close amendment set).
- **E1 Save/Load** — *no amendment needed* (corrected 2026-11-13; DiaDeferredStep Feature 3 drains on `OnSaveStarted`). A4's spec still says "E1's save path calls `DrainAll`" and needs that line removed.
- **S-3.5 World Sim** — its Feature 1 keeps `HistoryDriverModule` and every duty, with **one amendment**: AC8's trigger becomes "when the world step applies" on stages that run one, and stays the game-clock tick on stages that do not. The reason is in Resolved Decision 2: a tick trigger under deferred stepping samples at tick N while reading week N−k state. S-3.5 also declares the `WorldStepModule` instance with all three stages (`weather`, `hydrology`, `ecology`; amended 2026-11-17, A5 Hydrology) and owns the sim-hosted name-cap raise.
- **S-2.6 Rivers & Floods** (new testbed, A4 + A5; added 2026-11-17) — declares its own instance with `"order": ["weather", "hydrology"]`.
- **S-2.5 Weather Patterns** — declares its own `WorldStepModule` instance with `"order": ["weather"]`, and may set `"deferred": false` since weather alone fits in a frame.

**Plan:** [worldstep.plan.md](worldstep.plan.md) (written 2026-11-16, not started).
