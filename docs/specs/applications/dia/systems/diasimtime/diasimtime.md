# System Spec: DiaSimTime

## Parent Application
@docs/specs/applications/dia/dia.md

**Research:** @docs/research/scalab_sim_time/summary.md

**Gameplay Domains:** simulation, ai, physics, animation

## Purpose

DiaSimTime gives Dia a first-class simulation time architecture: a deterministic game clock, a named clock hierarchy, an event scheduler, per-system LOD policy, dormancy management, and a CPU budget — so the engine can simulate large populations without executing everything every frame.

Today the engine has one fidelity knob: ProcessingUnit Hz. Every SimModule runs at SimPU's fixed rate or not at all. `TimeServer` exists in DiaCore but is unwired from the Module pipeline. `AIBudgetScheduler` distributes CPU across AI systems but has no priority tiers, no fairness, and no dormancy. `UtilitySetComponent.eval_period_ticks` is the only per-system frequency reduction — counter-based, silent when SimPU Hz changes.

DiaSimTime replaces this patchwork with a coherent system. The engine gains:

- A `SimTimeContext` passed to every SimPU module each tick — deterministic int64 game time, gameDt, tick counter, timeScale, isPaused — replacing raw `float deltaTime`
- Typed module base classes (`SimModule` / `RenderModule` / `MainModule`) that enforce thread placement at compile time — wrong placement is a compile error, not a convention
- A named clock tree (`SimTimeDomain`) so pause, slow-motion, and fast-forward work on any subtree independently
- An event scheduler (`SimTimeScheduler`) that fires events at specific game times — cost proportional to events happening, not entities existing
- A per-system registry (`SimTimeRegistry`) with LOD tier (`SimTimePolicy`) and dormancy state (`SimTimeState`), so each system declares how often it needs to run and whether it can sleep
- An extended CPU budget (`SimTimeBudget`) with priority tiers and deadline-based fairness, absorbing `AIBudgetScheduler`
- A `StreamWriter<SimTimeContext>` (FrameStream) published each tick, unblocking `FrameStreamStore::FetchClosestTo()` for render interpolation

**Dependency chain:**
`DiaSimTime → DiaApplicationFlow (Module lifecycle, ProcessingUnit), DiaCore (TimeServer, TimeAbsolute, TimeRelative, DynamicArrayC, StringCRC), DiaAIBudget (AIBudgetScheduler absorbed), DiaStreams (FrameStream StreamWriter/StreamReader, EventStreamStore)`

## Responsibilities

- Define typed Module base classes `SimModule`, `RenderModule`, `MainModule` in DiaApplicationFlow; extend ProcessingUnit to inject the correct context struct per tick
- Define `SimTimeContext`, `RenderTimeContext`, `MainTimeContext` context structs
- Extend `Dia::Core::TimeServer` with `Pause()`, `Resume()`, `Step(TimeRelative)`, `AdvanceTo(TimeAbsolute)`; strip the thread-sleep from `Tick()` (sleep belongs to ProcessingUnit's rate-limiter, not the clock)
- Provide `DiaMainTime` — trivial `MainModule` that publishes `MainTimeContext` from wall-clock time
- Provide `DiaRenderTime` — `RenderModule` that reads the sim-time `FrameStream` (`StreamReader<SimTimeContext>`, `FetchLatest()` / `FetchClosestTo()`) and publishes `RenderTimeContext { frameDt, simTime, renderFrame }` to all RenderModules
- Provide `SimTimeDomain` and `SimTimeDomainRegistry` — named clock tree; any subtree can be paused, scaled, or stepped independently; StringCRC keys
- Provide `SimTimeScheduler` — timer wheel + min-heap overflow; `ScheduleAt`, `ScheduleAfter`, `ScheduleRecurring`, `Cancel`, `Reschedule`; ticks against the world `SimTimeDomain`, not wall clock; fires events into `EventStreamStore` on SimPU
- Provide `ISimTimeBudgetedSystem` — replaces `IAIBudgetedSystem`; adds `GetPriority()` returning `SimTimePriority { kCritical, kHigh, kNormal, kBackground }`
- Provide `SimTimeBudget` — extends `AIBudgetScheduler` with priority tiers and deadline/carry-forward; deferred systems promoted when staleness exceeds tier deadline; absorbs AIBudgetScheduler into DiaSimTime
- Provide `SimTimePolicy` component — declares a system's LOD tier (`SimTimeTier`), max update interval, `canSleep`, and `canAnalyticallyAdvance` (Phase 5 hook)
- Provide `SimTimeState` — `kAwake` / `kSleeping` per registered system; sleeping systems skip all gate checks
- Provide `SimTimeWakeCondition` registration API — `RegisterWakeOnTime(systemId, TimeAbsolute)`, `RegisterWakeOnMessage(systemId, eventType)`, `WakeSystem(systemId)` (manual)
- Provide `SimTimeRegistry` — owns all per-system registrations; drives the per-tick gate sequence: sleep → policy → budget
- Provide `DiaSimTimeModule` — the umbrella `SimModule` on SimPU; assembles SimTimeDomain + SimTimeScheduler + SimTimeBudget + SimTimeRegistry; publishes `SimTimeContext` via a `StreamWriter<SimTimeContext>` (FrameStream, timestamped by gameTime) after each tick
- Provide `dia.diasimtime.architecture.module.md` YAML module documentation
- Provide `DiaSimTime.vcxproj` static library registered in `Cluiche.sln`
- Migrate all existing `AIBudgetScheduler` consumers (`DiaUtilityAI`, `DiaHTN`, `DiaPathfinding`) to `ISimTimeBudgetedSystem`
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
    // A SimModule cannot be registered on RenderProcessingUnit — compile error.
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

        void  Tick();  // called by DiaSimTimeModule; advances clock one step
    };

    class SimTimeDomainRegistry {
    public:
        static constexpr Core::StringCRC kWorldId{"world"};
        static constexpr Core::StringCRC kNoParent{""};

        SimTimeDomain&  Create(Core::StringCRC id, Core::StringCRC parentId = kNoParent);
        SimTimeDomain*  Find(Core::StringCRC id);           // nullptr if not found
        void            Destroy(Core::StringCRC id);
        void            TickAll();                          // advances all domains in creation order
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

### Per-tick gate sequence

The gate loop operates on `ISimTimeBudgetedSystem` instances **registered with `DiaSimTimeModule`** — it does not gate sibling `SimModule`s (the ProcessingUnit ticks those unconditionally via its tick pipeline). This is a direct generalization of how today's `AIBudgetModule::DoUpdate` drives `AIBudgetScheduler`. `ProcessingUnit` must never consult `SimTimeRegistry` — that would invert the DiaApplicationFlow → DiaSimTime dependency.

Each tick `DiaSimTimeModule::DoUpdate()` runs in this order:

```
1. SimTimeDomainRegistry::TickAll()        → advance all clocks
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
  "worldDomainHz": 60.0,
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

### Metrics

| Metric key | Type | Description |
|---|---|---|
| `simtime.budget.used_ms` | Gauge | Total CPU consumed by registered systems last tick |
| `simtime.budget.deferred_count` | Counter | Systems skipped due to budget exhaustion |
| `simtime.budget.stale_ms_max` | Gauge | Oldest deferred work (ms since last update) |
| `simtime.scheduler.queue_depth` | Gauge | Scheduled events waiting to fire |
| `simtime.registry.sleeping_count` | Gauge | Systems currently sleeping |
| `simtime.tick` | Counter | Monotonic sim tick counter |

## Features

| Feature | Description | Spec | Status |
|---|---|---|---|
| Foundation | TimeServer additions (Pause/Resume/Step/AdvanceTo, sleep removed from Tick); typed base classes SimModule/RenderModule/MainModule; SimTimeContext/RenderTimeContext/MainTimeContext structs; DiaRenderTime module; DiaMainTime module | inline | Approved |
| SimTimeDomain | Named clock tree; SimTimeDomain + SimTimeDomainRegistry; pause/scale/step any subtree independently; world clock built-in | inline | Approved |
| SimTimeScheduler | Timer wheel + heap overflow; ScheduleAt/ScheduleAfter/ScheduleRecurring/Cancel/Reschedule; fires into EventStreamStore; ticks against world SimTimeDomain | inline | Approved |
| DiaSimTime umbrella | DiaSimTimeModule; SimTimeRegistry; SimTimePolicy + SimTimeState + SimTimeTier; SimTimeBudget (absorbs AIBudgetScheduler); ISimTimeBudgetedSystem; per-tick gate sequence; StreamWriter<SimTimeContext> (FrameStream) publish | inline | Approved |
| SimTimeAnalytical | ISimTimeAnalytical; AdvanceTo(systemId, TimeAbsolute); LastEvaluated timestamps; query-forces-correctness; continuous/discrete/tick models | _(follow-on feature spec)_ | Planned |

## Dependencies on Other Systems

**Required:**
- **DiaApplicationFlow** — `Module` base class, `ProcessingUnit` (injects context structs), `StartResult`/`StopResult`
- **DiaCore** — `TimeServer`, `TimeAbsolute`, `TimeRelative`, `DynamicArrayC`, `StringCRC`, `DIA_ASSERT`, `DIA_LOG_*`, `SystemClock`
- **DiaStreams** — `FrameStream` `StreamWriter<SimTimeContext>` (publish from SimPU) / `StreamReader<SimTimeContext>` (read by RenderPU); `EventStreamStore` (scheduler event delivery)
- **DiaMetrics** — budget, scheduler, and registry metrics via `MetricRegistry`

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
| ST-001 | Typed base classes (SimModule/RenderModule/MainModule) enforce PU placement at compile time | Cross-thread module placement is a known bug source. Making the wrong thing uncompilable is better than convention. The framework tick `DoUpdate(float)` is sealed `final` on all three and forwards to the typed `DoUpdate(ctx)`, pulling the context from the owning ProcessingUnit (mirrors the existing TestStageModuleBase seal-and-forward pattern) — it is not an empty body. | All DiaApplicationFlow modules | Accepted | Yes |
| ST-002 | TimeServer extended in place; sleep stripped from Tick() | TimeServer's int64 time model is correct. Adding Pause/Resume/Step/AdvanceTo is small. Stripping the sleep separates concerns — clock advances time, ProcessingUnit controls rate. No new class. | DiaCore/TimeServer | Accepted | Yes |
| ST-003 | AIBudgetScheduler absorbed into SimTimeBudget; IAIBudgetedSystem kept as deprecated alias | AIBudgetScheduler is 80% of SimTimeBudget. Extend rather than rebuild. Alias avoids breaking existing consumers during migration. | DiaAIBudget consumers | Accepted | Yes |
| ST-004 | SimTimeContext carries gameDt as TimeRelative (int64), not float | Deterministic. Callers convert to float when integrating physics or animating. Source is exact and does not accumulate error over long sessions. | SimTimeContext | Accepted | Yes |
| ST-005 | DiaSimTime gates registered systems (ISimTimeBudgetedSystem), not DiaEntity instances and not sibling SimModules | Entity-level fidelity is each system's internal concern. The gate loop runs inside DiaSimTimeModule over its registrations — like AIBudgetModule drives AIBudgetScheduler today. ProcessingUnit does not gate modules; it ticks every SimModule each frame. | SimTimeRegistry | Accepted | Yes |
| ST-006 | SimTimeSleep is SimTimeState + RegisterWakeOnX, not a standalone module | Dormancy is a field (kAwake/kSleeping) per registered system. Wake mechanisms delegate to existing systems (Scheduler, EventStreamStore). Not enough substance for a separate module. | DiaSimTimeModule | Accepted | Yes |
| ST-007 | SimTimeContext published to a FrameStream (StreamWriter, timestamped by gameTime) after every sim tick — not a ServiceStream | SimTimeContext is a per-tick timestamped snapshot, which is exactly what FrameStream models; ServiceStream is a register-once stable handle and is the wrong primitive. The FrameStream ring buffer unblocks FrameStreamStore::FetchClosestTo() for render interpolation. RenderTimeContext.simTime is a read-only snapshot — render cannot advance the sim clock. | DiaRenderTime | Accepted | Yes |
| ST-008 | SimTimeBudget tiers have explicit deadlines; deferred systems are promoted when stale | Registration-order fairness (AIBudgetScheduler today) causes starvation. Deadline-based promotion ensures every system runs within a bounded worst-case latency. | SimTimeBudget | Accepted | Yes |
| ST-009 | SimTimeDomain uses StringCRC keys; world domain always exists as kWorldId | PD-001 compliance. World domain is the default; encounter/entity sub-domains are opt-in. | SimTimeDomainRegistry | Accepted | Yes |

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

4. **IAIBudgetedSystem deprecation timeline** — `IAIBudgetedSystem` is kept as a deprecated alias during migration. How long should it live? Retire it once all three consumers (UtilityAI, HTN, Pathfinding adapter) are migrated, or keep it indefinitely for third-party/game code?

5. **SimTimeAnalytical save/load contract** — Phase 5 requires `LastEvaluated` timestamps to be serialised alongside system state. Does this create a dependency on `DiaSaveGame`, or should each system be responsible for serialising its own timestamp via the existing save system?

## Status

`Approved`

Implementation tracked in @docs/specs/applications/dia/systems/diasimtime/diasimtime.plan.md.
