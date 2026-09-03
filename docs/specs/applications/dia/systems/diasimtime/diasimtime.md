# System Spec: DiaSimTime

## Parent Application
@docs/specs/applications/dia/dia.md

**Research:** @docs/research/scalab_sim_time/summary.md

**Gameplay Domains:** simulation, ai, physics, animation

## Purpose

DiaSimTime gives Dia a first-class simulation time architecture: a deterministic game clock (gameTime/gameDt/tick — the clock's progression is deterministic; which systems execute on a given tick is not, see ST-010), a named clock hierarchy, an event scheduler, per-system LOD policy, dormancy management, and a CPU budget — so the engine can simulate large populations without executing everything every frame.

Today the engine has one fidelity knob: ProcessingUnit Hz. Every SimModule runs at SimPU's fixed rate or not at all. `TimeServer` exists in DiaCore but is unwired from the Module pipeline. `AIBudgetScheduler` distributes CPU across AI systems but has no priority tiers, no fairness, and no dormancy. `UtilitySetComponent.eval_period_ticks` is the only per-system frequency reduction — counter-based, silent when SimPU Hz changes.

DiaSimTime replaces this patchwork with a coherent system. The engine gains:

- A `SimTimeContext` passed to every SimPU module each tick — deterministic int64 game time, gameDt, tick counter, timeScale, isPaused — replacing raw `float deltaTime`. The SimPU runs a **fixed-timestep accumulator with capped catch-up** (ST-011): every gameplay/physics/AI `SimModule` sees a constant-size `gameDt` per call, never a raw variable frame delta, and the sim clock never silently drifts behind wall-clock under load
- Typed module base classes (`SimModule` / `RenderModule` / `MainModule`) that seal the framework tick entry and expose the correct per-role context type — `DoUpdate(float)` can no longer be silently overridden as an empty no-op, and each PU role gets its own typed context instead of a raw float. PU *placement* itself remains a runtime check (`TypeRegistry::GetAllowedPUs` + assert in `ProcessingUnit::AddModule`, promoted to fire in Release too — see ST-001), since modules attach to PUs via manifest-driven instanceId strings, not a compile-time type relationship
- A named clock tree (`SimTimeDomain`) so pause, slow-motion, and fast-forward work on any subtree independently
- An event scheduler (`SimTimeScheduler`) that fires events at specific game times — cost proportional to events happening, not entities existing
- A per-system registry (`SimTimeRegistry`) with LOD tier (`SimTimePolicy`) and dormancy state (`SimTimeState`), so each system declares how often it needs to run and whether it can sleep
- An extended CPU budget (`SimTimeBudget`) with priority tiers and deadline-based fairness, absorbing `AIBudgetScheduler`
- A `StreamWriter<SimTimeContext>` (FrameStream) published each tick, unblocking `FrameStreamStore::FetchClosestTo()` for render interpolation

**Dependency chain:**
`DiaSimTime → DiaApplicationFlow (Module lifecycle, ProcessingUnit), DiaCore (TimeServer, TimeAbsolute, TimeRelative, DynamicArrayC, StringCRC), DiaAIBudget (AIBudgetScheduler absorbed), DiaStreams (FrameStream StreamWriter/StreamReader, EventStreamStore)`

## Responsibilities

- Define typed Module base classes `SimModule`, `RenderModule`, `MainModule` in DiaApplicationFlow; extend ProcessingUnit to inject the correct context struct per tick
- Implement a fixed-timestep accumulator on `ProcessingUnit` for the `kSim` PU only (ST-011): banks real elapsed time, drains it in whole `1/GetFrequencyHz()` steps, runs the full SimPU module forward-pass once per step (0, 1, or several per real `Update()` call), and caps catch-up at `maxCatchUpTicksPerFrame` to avoid a spiral of death. Render/Main PUs are unaffected — single pass per `Update()`, as today
- Define `SimTimeContext`, `RenderTimeContext`, `MainTimeContext` context structs
- Extend `Dia::Core::TimeServer` with `Pause()`, `Resume()`, `Step(TimeRelative)`, `AdvanceTo(TimeAbsolute)`; strip the thread-sleep from `Tick()` (sleep belongs to ProcessingUnit's rate-limiter, not the clock)
- Provide `DiaMainTime` — trivial `MainModule` that publishes `MainTimeContext` from wall-clock time
- Provide `DiaRenderTime` — `RenderModule` that reads the sim-time `FrameStream` (`StreamReader<SimTimeContext>`, `FetchLatest()` / `FetchClosestTo()`) each tick and pushes the resulting `RenderTimeContext { frameDt, simTime, renderFrame }` into the owning `ProcessingUnit` (`SetRenderTimeContext`, framework-internal) for other RenderModules to read via `GetRenderTimeContext()` — one-tick-stale by design
- Provide `SimTimeDomain` and `SimTimeDomainRegistry` — named clock tree; any subtree can be paused, scaled, or stepped independently; StringCRC keys
- Provide `SimTimeScheduler` — timer wheel + min-heap overflow; `ScheduleAt`, `ScheduleAfter`, `ScheduleRecurring`, `Cancel`, `Reschedule`; ticks against the world `SimTimeDomain`, not wall clock; fires events into `EventStreamStore` on SimPU
- Provide `ISimTimeBudgetedSystem` — replaces `IAIBudgetedSystem`; adds `GetPriority()` returning `SimTimePriority { kCritical, kHigh, kNormal, kBackground }`
- Provide `SimTimeBudget` — extends `AIBudgetScheduler` with priority tiers and deadline/carry-forward; deferred systems promoted when staleness exceeds tier deadline; absorbs AIBudgetScheduler into DiaSimTime
- Provide `SimTimePolicy` component — declares a system's LOD tier (`SimTimeTier`), max update interval, `canSleep`, and `canAnalyticallyAdvance` (Phase 5 hook)
- Provide `SimTimeState` — `kAwake` / `kSleeping` per registered system; sleeping systems skip all gate checks
- Provide `SimTimeWakeCondition` registration API — `RegisterWakeOnTime(systemId, TimeAbsolute)`, `RegisterWakeOnMessage(systemId, eventType)`, `WakeSystem(systemId)` (manual)
- Provide `SimTimeRegistry` — owns all per-system registrations; drives the per-tick gate sequence: sleep → policy → budget
- Provide `DiaSimTimeModule` — the umbrella `SimModule` on SimPU; assembles SimTimeDomain + SimTimeScheduler + SimTimeBudget + SimTimeRegistry; publishes `SimTimeContext` via a `StreamWriter<SimTimeContext>` (FrameStream, timestamped by gameTime) after each tick
- Provide `SimTimeSaveState` (`ISaveable`) — a single combined save participant persisting world `gameTime`, the `SimTimeScheduler`'s pending queue, and each registered system's `SimTimeState` (kAwake/kSleeping), with internally-sequenced restore order (ST-015); restored scheduler entries get fresh `ScheduleHandle`s, never the pre-save value (ST-016)
- Provide `dia.diasimtime.architecture.module.md` YAML module documentation
- Provide `DiaSimTime.vcxproj` static library registered in `Cluiche.sln`
- Migrate all existing `AIBudgetScheduler` consumers (`DiaUtilityAI`, `DiaHTN`, `DiaPathfinding`, `DiaBehaviourTree`) to `ISimTimeBudgetedSystem`
- Provide `dia.diasimtime.*` DiaMetrics counters for budget utilisation, deferred count, scheduler queue depth, sleeping system count
- Emit `DIA_LOG_WARNING` when a system is deferred beyond its deadline; `DIA_LOG_INFO` on wake transitions

## Non-Responsibilities

- Per-entity fidelity decisions — systems gate their own entity work internally; DiaSimTime gates *systems*
- Spatial / proximity wake conditions — a spatial system fires an event; `RegisterWakeOnMessage` reacts; spatial query lives in DiaEntitySpatial
- `SimTimeAnalytical` — Phase 5 feature spec; hook point (`canAnalyticallyAdvance` in SimTimePolicy, `kSleeping` state) is designed in but not implemented in this system spec
- Simulation islands as a framework concept — a SimModule with a coherent system set IS an island; no registry needed
- Network synchronisation or lockstep — not in scope
- Rendering frame pacing — DiaRenderTime reads sim time; it does not drive vsync or swap chains
- Editor visualisation of simulation fidelity tiers — deferred to a future DiaSimTimeVisualDebugger feature

## Public Interfaces

### SimTimeContext, RenderTimeContext, MainTimeContext

```cpp
namespace Dia::SimTime {

    // Passed to SimModule::DoUpdate() each SimPU tick.
    struct SimTimeContext {
        Core::TimeAbsolute  gameTime;    // canonical game clock — int64 µs, deterministic
        Core::TimeRelative  gameDt;      // this tick's step (timeStep * timeScale)
        uint64_t            tick;        // monotonic tick counter (for replay / debug)
        float               timeScale;   // current scale (1.0 normal, 0.5 slow-mo)
        bool                isPaused;    // true if sim is paused (Step() ticks still fire)
    };

    // Passed to RenderModule::DoUpdate() each RenderPU frame.
    struct RenderTimeContext {
        float               frameDt;      // wall-clock seconds since last render frame
        Core::TimeAbsolute  simTime;      // READ-ONLY snapshot from the sim-time FrameStream
        uint64_t            renderFrame;  // render frame counter (independent of sim tick)
    };

    // Passed to MainModule::DoUpdate() each MainPU tick.
    struct MainTimeContext {
        float  wallClockDt;   // wall-clock seconds since last main tick
    };

} // namespace Dia::SimTime
```

### Typed Module Base Classes (DiaApplicationFlow)

```cpp
namespace Dia::ApplicationFlow {

    // Base for all SimPU modules. The framework tick entry DoUpdate(float) is sealed
    // and FORWARDS to the typed virtual, pulling the context the owning ProcessingUnit
    // cached this Update() (see ProcessingUnit context injection). It is NOT empty —
    // an empty body would mean the module never ticks. This mirrors the existing
    // TestStageModuleBase pattern (seal the framework virtual, expose a typed one).
    // PU placement (Sim vs Render vs Main) is a runtime check (TypeRegistry::GetAllowedPUs
    // + assert in ProcessingUnit::AddModule, promoted to fire in Release — see ST-001),
    // not a compile-time one: modules attach to PUs via manifest instanceId strings.
    class SimModule : public Module {
    public:
        virtual void DoUpdate(const Dia::SimTime::SimTimeContext& ctx) = 0;
    private:
        void DoUpdate(float /*deltaTime*/) final {           // sealed — do not override
            DoUpdate(GetProcessingUnit()->GetSimTimeContext());
        }
    };

    // Base for all RenderPU modules.
    class RenderModule : public Module {
    public:
        virtual void DoUpdate(const Dia::SimTime::RenderTimeContext& ctx) = 0;
    private:
        void DoUpdate(float /*deltaTime*/) final {
            DoUpdate(GetProcessingUnit()->GetRenderTimeContext());
        }
    };

    // Base for all MainPU modules.
    class MainModule : public Module {
    public:
        virtual void DoUpdate(const Dia::SimTime::MainTimeContext& ctx) = 0;
    private:
        void DoUpdate(float /*deltaTime*/) final {
            DoUpdate(GetProcessingUnit()->GetMainTimeContext());
        }
    };

} // namespace Dia::ApplicationFlow

The context is built once per `ProcessingUnit::Update()` — the PU already knows its role
via `GetAffinity()` (`mAffinity`, from the MainPU/SimPU/RenderPU instance ID) — and cached,
so every module on that PU reads the same snapshot through `GetSimTimeContext()` /
`GetRenderTimeContext()` / `GetMainTimeContext()`. The `Module` base stays context-agnostic;
each typed subclass knows which accessor to call.

`SimTimeContext` and `MainTimeContext` are computed directly inside `ProcessingUnit::Update()`
each tick. `RenderTimeContext` is **not** — the PU does not read the sim-time FrameStream
itself. Instead `DiaRenderTime` reads it (`StreamReader<SimTimeContext>::FetchLatest()`) and
pushes the resulting `RenderTimeContext` into the owning PU via a framework-internal
`ProcessingUnit::SetRenderTimeContext()` during its own `DoUpdate`. Sibling `RenderModule`s
read whichever `RenderTimeContext` was pushed last tick — one-tick-stale by design, so there
is no manifest-order dependency between `DiaRenderTime` and other RenderModules.

For `kSim`, "each tick" above means each **fixed step the accumulator drains**, not each real
`Update()` call — see ST-011. `Update(dt)` on a Sim PU banks `dt` into an accumulator and runs
the module forward-pass zero, one, or several times per real call, each time advancing
`SimTimeContext` by exactly one fixed step. Render/Main PUs are unaffected by this — they still
run exactly one pass per `Update()` call, with `dt` used directly (Main) or ignored in favour of
the pushed `RenderTimeContext` (Render).
```

### TimeServer additions (DiaCore)

```cpp
namespace Dia::Core {
    class TimeServer {
    public:
        // Existing API unchanged.
        void Tick();  // no longer sleeps the calling thread

        // New additions:
        void Pause();
        void Resume();
        void Step(TimeRelative step);           // advance exactly one step even if paused
        void AdvanceTo(TimeAbsolute target);    // jump forward to target; no-op if target <= current

        bool IsPaused() const;
    };
}
```

### SimTimeDomain

```cpp
namespace Dia::SimTime {

    class SimTimeDomain {
    public:
        Core::TimeAbsolute  Now() const;
        Core::TimeRelative  Step() const;       // current effective step size

        void  SetScale(float scale);
        void  Pause();
        void  Resume();
        void  Step(Core::TimeRelative step);    // manual single-step advance
        void  AdvanceTo(Core::TimeAbsolute t);

        float               GetScale() const;
        bool                IsPaused() const;
        Core::StringCRC     GetId() const;

        void  Tick();  // world domain: called by the owning ProcessingUnit's fixed-timestep
                        // accumulator (ST-011), once per drained step. Sub-domains: called by
                        // SimTimeDomainRegistry::TickAll(), once per DiaSimTimeModule::DoUpdate.
    };

    class SimTimeDomainRegistry {
    public:
        static constexpr Core::StringCRC kWorldId{"world"};
        static constexpr Core::StringCRC kNoParent{""};

        SimTimeDomain&  Create(Core::StringCRC id, Core::StringCRC parentId = kNoParent);
        SimTimeDomain*  Find(Core::StringCRC id);           // nullptr if not found
        void            Destroy(Core::StringCRC id);
        void            TickAll();                          // advances all NON-world domains in creation
                                                              // order; kWorldId is externally clocked by
                                                              // the owning ProcessingUnit and is a no-op here
    };

} // namespace Dia::SimTime
```

### SimTimeScheduler

```cpp
namespace Dia::SimTime {

    using ScheduleHandle = uint64_t;
    static constexpr ScheduleHandle kInvalidHandle = 0;

    class SimTimeScheduler {
    public:
        // Fire once at absolute game time.
        ScheduleHandle  ScheduleAt(Core::TimeAbsolute time,
                                   Core::StringCRC eventType,
                                   Core::StringCRC targetSystemId);

        // Fire once after delay from current game time.
        ScheduleHandle  ScheduleAfter(Core::TimeRelative delay,
                                      Core::StringCRC eventType,
                                      Core::StringCRC targetSystemId);

        // Fire repeatedly at interval. First fire is at now + interval.
        ScheduleHandle  ScheduleRecurring(Core::TimeRelative interval,
                                          Core::StringCRC eventType,
                                          Core::StringCRC targetSystemId);

        void  Cancel(ScheduleHandle handle);
        void  Reschedule(ScheduleHandle handle, Core::TimeAbsolute newTime);

        // Called by DiaSimTimeModule after clock advances.
        // Fires all events where scheduledTime <= currentTime into EventStreamStore.
        void  Tick(Core::TimeAbsolute currentTime);

        int   GetQueueDepth() const;
    };

} // namespace Dia::SimTime
```

### ISimTimeBudgetedSystem

```cpp
namespace Dia::SimTime {

    enum class SimTimePriority { kCritical, kHigh, kNormal, kBackground };

    // Replaces IAIBudgetedSystem. IAIBudgetedSystem kept as a deprecated alias.
    class ISimTimeBudgetedSystem {
    public:
        virtual ~ISimTimeBudgetedSystem() = default;

        virtual Core::StringCRC   GetSystemId() const = 0;
        virtual SimTimePriority   GetPriority() const { return SimTimePriority::kNormal; }

        // Called with however many ms remain in this priority tier.
        // Must honour the budget. Must handle budgetMs == 0.0f as a no-op.
        virtual void UpdateBudgeted(float budgetMs) = 0;
    };

} // namespace Dia::SimTime
```

### SimTimePolicy, SimTimeState, SimTimeTier

```cpp
namespace Dia::SimTime {

    enum class SimTimeTier {
        kImmediate,  // every SimPU tick (default)
        kHigh,       // ~30 Hz
        kMedium,     // ~10 Hz
        kLow,        // ~2 Hz
        kDormant     // event-driven only; system still registered but never ticked
    };

    struct SimTimePolicy {
        SimTimeTier         tier                = SimTimeTier::kImmediate;
        Core::TimeRelative  maxInterval         = Core::TimeRelative::Zero();  // 0 = tier default
        bool                canSleep            = false;
        bool                canAnalyticallyAdvance = false;  // Phase 5 hook
    };

    enum class SimTimeState { kAwake, kSleeping };

} // namespace Dia::SimTime
```

### DiaSimTimeModule

```cpp
namespace Dia::SimTime {

    class DiaSimTimeModule : public ApplicationFlow::SimModule {
    public:
        static constexpr Core::StringCRC kUniqueId{"DiaSimTime"};
        Core::StringCRC GetUniqueId() const override { return kUniqueId; }

        // Register a system. Call from other SimModules' DoStart() or OnConnectStreams().
        void  Register(ISimTimeBudgetedSystem* system, const SimTimePolicy& policy);
        void  Unregister(ISimTimeBudgetedSystem* system);

        // Dormancy control.
        void          Sleep(Core::StringCRC systemId);
        void          Wake(Core::StringCRC systemId);
        SimTimeState  GetState(Core::StringCRC systemId) const;

        // Wake condition registration.
        void  RegisterWakeOnTime(Core::StringCRC systemId, Core::TimeAbsolute at);
        void  RegisterWakeOnMessage(Core::StringCRC systemId, Core::StringCRC eventType);

        // Sub-service access.
        SimTimeDomainRegistry&  GetDomainRegistry();
        SimTimeScheduler&       GetScheduler();

    protected:
        StartResult DoStart() override;
        void        DoUpdate(const SimTimeContext& ctx) override;  // drives tick sequence
        StopResult  DoStop() override;
    };

} // namespace Dia::SimTime
```

### SimTimeSaveState

```cpp
namespace Dia::SimTime {

    // Single combined save participant — registered once with SaveRegistry from
    // DiaSimTimeModule::DoStart(). Internally sequences its own restore order
    // (world domain -> scheduler queue -> sleep state) so correctness does not
    // depend on SaveRegistry's plain registration-order walk (ST-015).
    class SimTimeSaveState : public Dia::SaveGame::ISaveable {
    public:
        void Serialize(Dia::SaveGame::SaveContext& ctx) const override;
        void Deserialize(Dia::SaveGame::LoadContext& ctx) override;
        int  GetVersion() const override { return 1; }
    };

} // namespace Dia::SimTime
```

`Serialize` writes: world domain `gameTime` (via the new `SaveContext::Write(int64_t)`), the
scheduler's pending entries (`eventType`, `targetSystemId`, absolute fire time — each an
`int64_t`), and each registered system's `SimTimeState` keyed by `StringCRC systemId`.

`Deserialize` restores in this fixed order, inside one call, so no cross-participant ordering
guarantee is needed from `SaveRegistry`:
1. Read the saved `gameTime`, call `mWorldDomain.AdvanceTo(savedGameTime)` to re-anchor the
   clock (a freshly-constructed domain starts at its minimum time, so this is a genuine forward
   jump, not a no-op).
2. Read each scheduler entry and re-insert via `ScheduleAt(savedTime, eventType, targetSystemId)`
   — this issues a **fresh** `ScheduleHandle`, never the pre-save value (ST-016).
3. Read each system's saved `SimTimeState` and re-apply `Sleep()`/`Wake()` for systemIds
   currently registered; log (`DIA_LOG_WARNING`) and skip any systemId not currently registered
   (e.g. content changed between the save and this session) rather than asserting.

### Per-tick gate sequence

The gate loop operates on `ISimTimeBudgetedSystem` instances **registered with `DiaSimTimeModule`** — it does not gate sibling `SimModule`s (the ProcessingUnit ticks those unconditionally via its tick pipeline). This is a direct generalization of how today's `AIBudgetModule::DoUpdate` drives `AIBudgetScheduler`. `ProcessingUnit` must never consult `SimTimeRegistry` — that would invert the DiaApplicationFlow → DiaSimTime dependency.

Each tick `DiaSimTimeModule::DoUpdate()` runs in this order:

```
1. SimTimeDomainRegistry::TickAll()        → advance all non-world clocks (world already
                                              advanced by the owning ProcessingUnit this Update())
2. SimTimeScheduler::Tick(gameTime)         → fire scheduled events into EventStreamStore
3. SimTimeBudget::AllocateTick()            → partition budget by priority tier

4. For each registered system (SimTimeRegistry order):
   a. SimTimeState == kSleeping?        → skip (zero cost)
   b. SimTimePolicy tier due this tick? → skip (LOD throttle)
   c. SimTimeBudget has capacity?       → skip with deadline if exhausted

   Pass: system->UpdateBudgeted(remainingMs)

5. Publish SimTimeContext → StreamWriter<SimTimeContext>::Write(ctx, gameTime)  (FrameStream)
```

### Config JSON (manifest)

```json
{
  "moduleType": "DiaSimTime",
  "budget": {
    "criticalMs":    2.0,
    "highMs":        1.0,
    "normalMs":      0.5,
    "backgroundMs":  0.25
  },
  "tier_hz": {
    "high":   30.0,
    "medium": 10.0,
    "low":     2.0
  }
}
```

There is no separate `worldDomainHz` field — the fixed step is `1.0f / SimPU's own GetFrequencyHz()`, sourced from the SimPU's manifest-configured frequency (already authoritative for PU pacing). A second, independent Hz knob here would risk drifting out of sync with the PU's actual rate.

The fixed-timestep accumulator's catch-up cap (ST-011) is a **`ProcessingUnit`-level** setting, not a `DiaSimTimeModule` one — `ProcessingUnit` doesn't read module config JSON, and the cap applies to the PU's tick loop regardless of which modules happen to be registered on it. It is a new field on the SimPU's manifest declaration, alongside the existing `frequencyHz`/`dedicatedThread`:

```json
{ "instanceId": "SimPU", "frequencyHz": 60.0, "dedicatedThread": true, "maxCatchUpTicksPerFrame": 5 }
```

Defaults to 5 if unspecified. Backlog beyond the cap is dropped, not deferred (`simtime.accumulator.dropped_ticks`).

### Metrics

| Metric key | Type | Description |
|---|---|---|
| `simtime.budget.used_ms` | Gauge | Total CPU consumed by registered systems last tick |
| `simtime.budget.deferred_count` | Counter | Systems skipped due to budget exhaustion |
| `simtime.budget.stale_ms_max` | Gauge | Oldest deferred work (ms since last update) |
| `simtime.scheduler.queue_depth` | Gauge | Scheduled events waiting to fire |
| `simtime.registry.sleeping_count` | Gauge | Systems currently sleeping |
| `simtime.tick` | Counter | Monotonic sim tick counter |
| `simtime.accumulator.dropped_ticks` | Counter | Backlog steps discarded when catch-up hit `maxCatchUpTicksPerFrame` (ST-011) |

## Features

| Feature | Description | Spec | Status |
|---|---|---|---|
| Foundation | TimeServer additions (Pause/Resume/Step/AdvanceTo, sleep removed from Tick); typed base classes SimModule/RenderModule/MainModule; SimTimeContext/RenderTimeContext/MainTimeContext structs; DiaRenderTime module; DiaMainTime module | inline | Done |
| SimTimeDomain | Named clock tree; SimTimeDomain + SimTimeDomainRegistry; pause/scale/step any subtree independently; world clock built-in | inline | Done |
| SimTimeScheduler | Timer wheel + heap overflow; ScheduleAt/ScheduleAfter/ScheduleRecurring/Cancel/Reschedule; fires into EventStreamStore; ticks against world SimTimeDomain | inline | Done |
| DiaSimTime umbrella | DiaSimTimeModule; SimTimeRegistry; SimTimePolicy + SimTimeState + SimTimeTier; SimTimeBudget (absorbs AIBudgetScheduler); ISimTimeBudgetedSystem; per-tick gate sequence; StreamWriter<SimTimeContext> (FrameStream) publish; SimTimeSaveState (save/load) | inline | Done |
| SimTimeAnalytical | ISimTimeAnalytical; AdvanceTo(systemId, TimeAbsolute); LastEvaluated timestamps; query-forces-correctness; continuous/discrete/tick models | _(follow-on feature spec)_ | Planned |

## Dependencies on Other Systems

**Required:**
- **DiaApplicationFlow** — `Module` base class, `ProcessingUnit` (injects context structs), `StartResult`/`StopResult`
- **DiaCore** — `TimeServer`, `TimeAbsolute`, `TimeRelative`, `DynamicArrayC`, `StringCRC`, `DIA_ASSERT`, `DIA_LOG_*`, `SystemClock`
- **DiaStreams** — `FrameStream` `StreamWriter<SimTimeContext>` (publish from SimPU) / `StreamReader<SimTimeContext>` (read by RenderPU); `EventStreamStore` (scheduler event delivery). `EventStreamWriter`/`EventStreamReader::Connect()` currently hardcodes `kDefaultMaxReaders = 8` with no capacity parameter and fails silently past that cap (`RegisterReader()` returns -1, uncontrolled by the caller) — given Q2's fan-out design (each consuming system type gets its own reader), Task 3.3 adds an optional capacity parameter to `Connect()` in DiaStreams itself, used for the `SimTimeSchedulerFire` stream
- **DiaMetrics** — budget, scheduler, and registry metrics via `MetricRegistry`
- **DiaSaveGame** — `ISaveable`, `SaveContext`/`LoadContext`, `SaveRegistry`. `SaveContext`/`LoadContext` gain a `Write(int64_t)`/`Read(int64_t)` overload (Task 4.6) — today only `int32_t`/`float`/`bool`/`const char*` exist, insufficient for `TimeAbsolute`/`TimeRelative`. `DiaSimTime` is the first production consumer of `ISaveable` — no prior art exists to follow

**Absorbed:**
- **DiaAIBudget** — `AIBudgetScheduler` extended and incorporated as `SimTimeBudget`; `IAIBudgetedSystem` kept as deprecated alias for `ISimTimeBudgetedSystem` during migration

**Explicitly excluded:**
- **DiaEntitySpatial** — proximity-based wake conditions delegate to spatial events; no compile-time dep
- **diaentitytemplate** — DiaSimTime gates systems, not entities; entity-level concerns are internal to each system
- **DiaObservation** — metrics via DiaMetrics only; trace integration deferred

**Dependents (migration required):**
- `DiaUtilityAI` — async path currently implements `IAIBudgetedSystem`; migrate to `ISimTimeBudgetedSystem`
- `DiaHTN` — async replan path currently implements `IAIBudgetedSystem`; migrate
- `DiaPathfinding` — `PathfindingSystem::Update(ms)` wrapped as `ISimTimeBudgetedSystem` adapter
- `DiaBehaviourTree` — `BehaviourTreeSystem` implements `IAIBudgetedSystem` today; not yet wired into a production `AIBudgetModule` (only exercised by its own GoogleTests), but migrates alongside the other three since it implements the interface being replaced

## Out of Scope

- Per-entity LOD — systems gate entity work internally; DiaSimTime gates systems only
- Simulation islands as a framework feature — a SimModule with coherent system set IS an island
- `SimTimeAnalytical` — Phase 5 feature spec; hook point (`canAnalyticallyAdvance`, `kSleeping`) is designed in
- Spatial wake conditions — DiaEntitySpatial fires an event; `RegisterWakeOnMessage` reacts
- Network synchronisation, lockstep, or deterministic rollback
- Render frame pacing / vsync
- Editor visualisation of tier assignments and sleep state

## Decisions

| ID | Decision | Rationale | Scope | Status | Binding |
|---|---|---|---|---|---|
| ST-001 | Typed base classes (SimModule/RenderModule/MainModule) seal the tick entry and expose per-role context types; PU placement stays a runtime check, promoted to fire in Release | Cross-thread module placement is a known bug source, but modules attach to PUs via manifest-driven instanceId strings — there is no C++ type relationship for the compiler to check, so placement itself cannot be a compile error. What sealing `DoUpdate(float) final` on all three *does* guarantee at compile time: it can no longer be silently overridden as an empty no-op, and each PU role gets its own typed context (SimTimeContext/RenderTimeContext/MainTimeContext) instead of a raw float. Placement is enforced by the existing `TypeRegistry::GetAllowedPUs` + assert in `ProcessingUnit::AddModule` (`ProcessingUnit.cpp`) — currently `#ifdef DIA_DEBUG` only; promoted to fire in all configs (Task 1.0) so a misplaced module fails loudly in Release too, not just Debug. The assert is also skipped outright whenever the PU itself is `kAny`-affinity (`ProcessingUnit.cpp:86-88`) — a structural gap, not a config gap: any custom-named PU (e.g. CluicheEditor's `EditorPU`) defaults to `kAny` and gets zero enforcement in any config, forever. Task 1.0 closes this too, by validating `kAny` PUs against a module's declared `kAllowedPUs` when one exists, instead of skipping the PU side of the check outright. | All DiaApplicationFlow modules | Accepted | Yes |
| ST-002 | TimeServer extended in place; sleep stripped from Tick() | TimeServer's int64 time model is correct. Adding Pause/Resume/Step/AdvanceTo is small. Stripping the sleep separates concerns — clock advances time, ProcessingUnit controls rate. No new class. | DiaCore/TimeServer | Accepted | Yes |
| ST-003 | AIBudgetScheduler absorbed into SimTimeBudget; IAIBudgetedSystem kept as deprecated alias | AIBudgetScheduler is 80% of SimTimeBudget. Extend rather than rebuild. Alias avoids breaking existing consumers during migration. | DiaAIBudget consumers | Accepted | Yes |
| ST-004 | SimTimeContext carries gameDt as TimeRelative (int64), not float | Deterministic. Callers convert to float when integrating physics or animating. Source is exact and does not accumulate error over long sessions. | SimTimeContext | Accepted | Yes |
| ST-005 | DiaSimTime gates registered systems (ISimTimeBudgetedSystem), not DiaEntity instances and not sibling SimModules | Entity-level fidelity is each system's internal concern. The gate loop runs inside DiaSimTimeModule over its registrations — like AIBudgetModule drives AIBudgetScheduler today. ProcessingUnit does not gate modules; it ticks every SimModule each frame. | SimTimeRegistry | Accepted | Yes |
| ST-006 | SimTimeSleep is SimTimeState + RegisterWakeOnX, not a standalone module | Dormancy is a field (kAwake/kSleeping) per registered system. Wake mechanisms delegate to existing systems (Scheduler, EventStreamStore). Not enough substance for a separate module. | DiaSimTimeModule | Accepted | Yes |
| ST-007 | SimTimeContext published to a FrameStream (StreamWriter, timestamped by gameTime) after every sim tick — not a ServiceStream | SimTimeContext is a per-tick timestamped snapshot, which is exactly what FrameStream models; ServiceStream is a register-once stable handle and is the wrong primitive. The FrameStream ring buffer unblocks FrameStreamStore::FetchClosestTo() for render interpolation. RenderTimeContext.simTime is a read-only snapshot — render cannot advance the sim clock. | DiaRenderTime | Accepted | Yes |
| ST-008 | SimTimeBudget tiers have explicit deadlines; deferred systems are promoted when stale | Registration-order fairness (AIBudgetScheduler today) causes starvation. Deadline-based promotion ensures every system runs within a bounded worst-case latency. | SimTimeBudget | Accepted | Yes |
| ST-009 | SimTimeDomain uses StringCRC keys; world domain always exists as kWorldId | PD-001 compliance. World domain is the default; encounter/entity sub-domains are opt-in. | SimTimeDomainRegistry | Accepted | Yes |
| ST-010 | Budget-driven system gating is wall-clock (`steady_clock`) and therefore non-deterministic; only the clock (gameTime/gameDt/tick) is deterministic | `SimTimeBudget` extends `AIBudgetScheduler`, which measures real CPU time to decide what to skip, defer, or promote each tick. Two runs with identical game-time inputs can execute a different set of systems on a given tick under different load. Replay/debug tooling built on `SimTimeContext.tick` can reproduce clock progression exactly but must not assume identical system-execution sets tick-for-tick. | SimTimeBudget | Accepted | Yes |
| ST-011 | SimPU runs a fixed-timestep accumulator with capped catch-up, owned by `ProcessingUnit` (not `SimTimeDomain`) | Without an accumulator, one `SimTimeDomain::Tick()` per real `Update()` call means the sim clock advances by exactly one fixed step regardless of how much real time actually elapsed — under load, game time silently falls behind wall-clock forever, with no correction. `ProcessingUnit::Update(dt)` banks `dt` into `mSimAccumulatorSec` for `kSim` only; each call drains it in whole `1/GetFrequencyHz()` steps, running the full module forward-pass once per step (0, 1, or several times), so every `SimModule::DoUpdate` always sees a constant-size `gameDt` — never a raw variable frame delta. Catch-up is capped at `maxCatchUpTicksPerFrame` (a new field on the SimPU's own manifest declaration, alongside `frequencyHz`/`dedicatedThread` — not `DiaSimTimeModule`'s config JSON, since `ProcessingUnit` doesn't read module config; default 5); excess backlog is dropped (not deferred) and logged (`DIA_LOG_WARNING`) plus counted (`simtime.accumulator.dropped_ticks`), to avoid a spiral of death. While `IsPaused()`, the accumulator does not accrue — it resets to zero each `Update()` call — so resuming after a long pause does not trigger a catch-up burst; manual `SimTimeDomain::Step()` bypasses the accumulator entirely and is unaffected. The reverse pass (`kStopping` modules) and post-tick bookkeeping (`mPostTickFn`, metrics) run exactly once per real `Update()` call regardless of how many fixed steps drained, so module shutdown sequencing is never gated by the accumulator. `timeScale` continues to affect game-time advanced per step (`gameDt = fixedStep * timeScale`), not step frequency — the accumulator itself always paces against unscaled real time. This lives on `ProcessingUnit` (DiaApplicationFlow), not `SimTimeDomain` (DiaCore), consistent with ST-002's separation: the clock advances time, the PU controls rate/cadence. | ProcessingUnit (kSim) | Accepted | Yes |
| ST-012 | Transient one-shot async work (e.g. UtilityAI/HTN async evaluation requests) does not register with `SimTimeRegistry`/`SimTimeBudget` at all — a separate, lightweight completion path exists instead | `AIBudgetScheduler`'s registration array is sized for steady-state, long-lived systems (`kMaxSystems=16`). Async work items (`UtilityEvalWorkItem`, `HTNPlanWorkItem`) are dynamically created per call and can spike arbitrarily with population size — they need "run to completion within some budget," not tier/sleep/deadline machinery. Registering them would consume a steady-state slot for a one-shot job, turning the existing 16-slot ceiling from an AI-only constraint into a cross-domain one once `SimTimeBudget` also hosts physics/animation/etc. A separate one-shot completion queue keeps steady-state registration bounded at 16 while letting transient work scale with population instead of contending for the same slots. | SimTimeBudget | Accepted | Yes |
| ST-013 | Modules with a deliberate `PUAffinity::kAny` (e.g. CluicheGameBaseline's `ObservationModule`/`ProfilerModule`, and `Dia::ApplicationFlow::ObservationModule` itself — confirmed at `ObservationModule.h:20`, `kAllowedPUs = PUAffinity::kAny`) are exempted from the typed-base migration — they keep deriving from `Module` directly and keep their own `DoUpdate(float)` override | ST-001's typed bases close the "silent empty override" hole for modules that ARE meant to be Sim/Render/Main-specific. That risk doesn't apply to modules that legitimately don't care which PU role they run under — forcing them onto one of the three typed bases would silently foreclose future re-placement (there is no fourth "any" typed base) for no safety benefit. `Module::DoUpdate(float)` remains a valid, intentional override point for this category; the hard cutover (Open Q3) only applies to modules that adopt a typed base. | DiaApplicationFlow | Accepted | Yes |
| ST-014 | `UtilitySetComponent.eval_period_ticks` is left as-is; its interaction with `SimTimePolicy` tiering is documented, not fixed | If a system is tiered down via `SimTimePolicy` (e.g. `kLow`), its per-entity `eval_period_ticks` counter only increments on the ticks the system actually receives — so the counter silently means "every Nth time the system runs," compounding with whatever tier it's on. This is the exact "silent when SimPU Hz changes" failure the Purpose section names as today's problem, recurring one layer down at the entity-component level; DiaSimTime gates systems, not entities (ST-005), so fixing it here is out of scope. Documented so it isn't rediscovered as a live tuning bug during Phase 4 work. | DiaUtilityAI / UtilitySetComponent | Accepted | Yes |
| ST-015 | `SimTimeScheduler`/`SimTimeRegistry` persist through a single combined `ISaveable` participant (`SimTimeSaveState`), not per-sub-component registrations | `SaveRegistry` walks registered participants in plain registration order with no priority/ordering field. Relying on registration order across three separate participants (world domain, scheduler, registry) to always land in the right restore sequence is fragile and would silently break if registration order ever changed. One participant that internally sequences its own restore (domain → scheduler → sleep state) removes that fragility entirely, with no change needed to `SaveRegistry` itself. | SimTimeSaveState | Accepted | Yes |
| ST-016 | `ScheduleHandle` identity is not preserved across a save/load boundary — `Deserialize` reissues fresh handles for restored scheduler entries | `ScheduleHandle` is a generation-based `uint64_t` scoped to one `SimTimeScheduler` instance's lifetime; preserving exact handle values across a save boundary would require persisting and exactly restoring the generation counter and handle-to-slot mapping, for no real benefit — nothing needs a *specific* handle value to survive, only the effect (the event still fires at the right absolute time). Systems must not persist a `ScheduleHandle` in their own save data expecting it to remain valid after a load; if they need to reference "the event I scheduled" across a save boundary, they re-derive it (e.g. by `targetSystemId` + `eventType`) rather than store the handle. | SimTimeScheduler | Accepted | Yes |

## Inherited Binding Decisions

| ID | Source | Decision | Implication for this system |
|---|---|---|---|
| PD-001 | Platform | StringCRC for all IDs | System IDs (`GetSystemId()`), event type IDs, domain names, log channel, metric keys all use StringCRC. |
| PD-002 | Platform | ProcessingUnit/Phase/Module architecture | `DiaSimTimeModule` is a `SimModule` on SimPU. `DiaRenderTime` is a `RenderModule` on RenderPU. `DiaMainTime` is a `MainModule` on MainPU. |
| PD-004 | Platform | No STL containers in public APIs | `SimTimeScheduler` and `SimTimeBudget` use `DiaCore::DynamicArrayC` and DiaCore priority structures internally. No `std::priority_queue`, `std::vector`, or `std::map` in public headers. |
| PD-005 | Platform | x64 Windows only | `DiaSimTime.vcxproj` targets x64 exclusively. |
| PD-006 | Platform | Visual Studio project files are source of truth | `DiaSimTime.vcxproj` and `.vcxproj.filters` created and maintained manually. |
| PD-007 | Platform | C++20 required | Compiled under `/std:c++20`. `DoUpdate(float)` sealed with `final` keyword. |
| PD-008 | Platform | Directory.Build.props owns OutDir/IntDir/toolchain | `DiaSimTime.vcxproj` must NOT override `OutDir`, `IntDir`, `PlatformToolset`, `WindowsTargetPlatformVersion`, or `LanguageStandard`. |
| PD-009 | Platform | Generated output under `Cluiche/out/` | Metric and log file output goes under `Cluiche/out/<AppName>/`. |
| AD-001 | Dia App | Module system with YAML frontmatter | Create `dia.diasimtime.architecture.module.md` with public API, responsibilities, and dependency declarations. |
| AD-002 | Dia App | No STL containers in public APIs | Reinforces PD-004. |
| AD-003 | Dia App | Namespace `Dia::<Module>::` | All types in `Dia::SimTime::` namespace. Typed base classes remain in `Dia::ApplicationFlow::` (they extend Module). |
| AD-004 | Dia App | ProcessingUnit/Phase/Module for application structure | DiaSimTimeModule is a proper SimModule subclass — not a singleton or service locator. |

## Open Design Questions

1. **SimTimeDomain home module** — **RESOLVED (2026-08-14): DiaCore.** The typed base classes (in DiaApplicationFlow) reference `SimTimeContext`, and the DiaSimTime module depends on DiaApplicationFlow — so placing the context structs (or the domain) in the module would create a dependency cycle. Both the context structs and `SimTimeDomain`/`SimTimeDomainRegistry` live in `DiaCore/SimTime` (namespace `Dia::SimTime`), beside `TimeServer`. This also lets `ProcessingUnit` own the world `SimTimeDomain` as its clock. Scheduler/budget/registry/policy/state remain in the DiaSimTime module.

2. **Scheduler event delivery** — **RESOLVED (2026-08-14): EventStreamStore fan-out, no callback path.** `SimTimeScheduler` sends `Event<SimTimeSchedulerFire>` with payload `{ StringCRC eventType; StringCRC targetSystemId; }`; subscribing systems register a reader and filter. `EventStreamStore<T>` fans a copy of each event to all readers (≤8 default) with no built-in target routing, so filtering is the native pattern. A direct callback registry was rejected (pointer-lifetime hazards, coupling). Fan-out coarseness is acceptable at expected scale; a dedicated per-target stream is the fallback if profiling ever demands it.

3. **DoUpdate(float) migration strategy** — **RESOLVED (2026-08-14): hard cutover, single branch.** Coexistence would keep `Module::DoUpdate(float)` overridable and reopen the compile-time hole ST-001 exists to close; and because the typed bases seal `DoUpdate(float) final`, there is no partially-migrated state that still compiles. The migration lands atomically on one branch (TimeServer + context structs → typed bases + PU injection → compiler-driven module/test-double migration → merge), leveraging `TestStageModuleBase` to cover ~22 stages in a single edit.

4. **IAIBudgetedSystem deprecation timeline** — `IAIBudgetedSystem` is kept as a deprecated alias during migration. How long should it live? Retire it once all four consumers (UtilityAI, HTN, Pathfinding adapter, BehaviourTreeSystem) are migrated, or keep it indefinitely for third-party/game code?

5. **Save/load contract for `SimTimeScheduler`/`SimTimeState`** — **RESOLVED (2026-08-21): build it now, in this plan, not deferred.** `DiaSaveGame` has zero production consumers today (no prior art to copy) — `DiaSimTime` is the first. A single combined `SimTimeSaveState` (`ISaveable`) participant persists world `gameTime`, the scheduler's pending queue, and per-system sleep state, with its own internally-sequenced restore order (world domain → scheduler → sleep state), so correctness does not depend on `SaveRegistry`'s plain registration-order walk (ST-015). `ScheduleHandle` identity is not preserved across a save/load boundary — restored entries get fresh handles (ST-016). `SaveContext`/`LoadContext` gain an `int64_t` overload (Task 4.6) to carry `TimeAbsolute`/`TimeRelative` — today they only support `int32_t`/`float`/`bool`/`const char*`. See Task 4.6/4.7 and the `SimTimeSaveState` interface above.

## Status

`Approved`

Implementation tracked in @docs/specs/applications/dia/systems/diasimtime/diasimtime.plan.md.
