# System Spec: DiaAIBudget

## Parent Application
@docs/specs/applications/dia/dia.md

**Gameplay Domains:** ai

## Purpose

DiaAIBudget is the frame-budget scheduler for all AI work on the SimPU. It ensures that AI systems collectively consume at most a configured microsecond budget per tick, preventing AI from crowding out physics, order execution, and other simulation work when entity counts grow.

The system provides a single `IAIBudgetedSystem` interface. Any AI system that wants time-limited execution registers itself; the scheduler calls each in registration order, passing a shrinking time slice. Systems self-manage their internal work queues — the scheduler provides only the shared budget envelope, call ordering, per-system elapsed measurement, and utilisation metrics.

This design generalises the pattern already established by `DiaPathfinding::PathfindingSystem<TGraph>::Update(float budgetMs)` — every AI system gets its own `UpdateBudgeted(ms)` call with however many milliseconds remain after earlier systems ran.

**Dependency chain:**
`DiaAIBudget → DiaApplicationFlow (Module lifecycle), DiaMetrics (utilisation counters), DiaCore (containers, StringCRC)`

**Planned follow-on:** A `DiaAIBudgetTiers` feature spec will add Critical / Normal / Background priority queues with staleness-based promotion. The shell design here is intentionally forward-compatible — registration order is the implicit priority today; tiers formalise it later without breaking the `IAIBudgetedSystem` interface.

## Responsibilities

- Define `IAIBudgetedSystem` — interface with `GetSystemId() → StringCRC` and `UpdateBudgeted(float budgetMs)` (wall-clock milliseconds remaining this tick)
- Provide `AIBudgetScheduler` — owns a fixed-capacity array (max `kMaxSystems = 16`) of registered `IAIBudgetedSystem*` pointers; each tick calls them in registration order, measuring elapsed time via `std::chrono::steady_clock` and passing the shrinking remainder to each; stops calling further systems once budget is exhausted
- Provide `AIBudgetModule` — `Dia::ApplicationFlow::Module` subclass; reads `budgetUs` (integer microseconds) from `OnConfigure(json)`; constructs and owns an `AIBudgetScheduler`; calls `AIBudgetScheduler::Update(budgetMs)` from `DoUpdate(float deltaTime)`
- Register DiaMetrics counters at startup: `ai.budget.used_us` (Gauge), `ai.budget.systems_run` (Counter), `ai.budget.systems_deferred` (Counter)
- Assert in Debug if `Register()` is called beyond `kMaxSystems` capacity; in Release, log and return false
- Assert in Debug if any registered `IAIBudgetedSystem*` is null
- Provide `dia.aibudget.architecture.module.md` YAML module documentation
- Provide `DiaAIBudget.vcxproj` static library registered in `Cluiche.sln` under `domain/gameplay/ai`

## Non-Responsibilities

- Priority tiers (Critical / Normal / Background) — planned as `DiaAIBudgetTiers` feature spec
- Per-entity work item tracking — systems manage their own entity queues internally (e.g. `PathfindingSystem` does this already)
- Starvation prevention / aging — deferred to the tiers feature
- Deciding *which* entities a system updates — the scheduler only decides *whether* a system gets any time this tick and *how much*
- Blackboard reads or writes — the scheduler is data-agnostic; AI systems read blackboards themselves
- Driving DiaStateMachine, DiaOrder, or DiaPathfinding directly — those systems implement `IAIBudgetedSystem` via thin adapters in game/test code
- Thread safety — `AIBudgetScheduler::Update()` is single-threaded on SimPU; no locking

## Public Interfaces

### IAIBudgetedSystem

```cpp
namespace Dia::AIBudget {

    // Implement this on any AI system that wants frame-budget scheduling.
    // Register the instance with AIBudgetScheduler::Register().
    class IAIBudgetedSystem {
    public:
        virtual ~IAIBudgetedSystem() = default;

        // Unique identifier for this system (used in metrics and logging).
        virtual Dia::Core::StringCRC GetSystemId() const = 0;

        // Process AI work for up to budgetMs wall-clock milliseconds.
        // Implementations MUST honour the budget — stop processing and
        // defer remaining work when elapsed time exceeds budgetMs.
        // budgetMs may be 0.0f if the budget is already exhausted;
        // implementations must handle this gracefully (no-op).
        virtual void UpdateBudgeted(float budgetMs) = 0;
    };

} // namespace Dia::AIBudget
```

### AIBudgetScheduler

```cpp
namespace Dia::AIBudget {

    class AIBudgetScheduler {
    public:
        static constexpr int kMaxSystems = 16;

        AIBudgetScheduler() = default;

        // Register a system. Returns true on success, false if at capacity.
        // Systems are called in registration order each tick.
        // Caller retains ownership; pointer must remain valid until Unregister().
        bool Register(IAIBudgetedSystem* system);

        // Unregister a previously registered system (no-op if not found).
        void Unregister(IAIBudgetedSystem* system);

        // Call once per SimPU tick from AIBudgetModule::DoUpdate().
        // Calls registered systems in order, passing shrinking time slice.
        // totalBudgetMs: total wall-clock milliseconds allocated this frame.
        void Update(float totalBudgetMs);

        int GetRegisteredCount() const;

    private:
        Dia::Core::DynamicArrayC<IAIBudgetedSystem*> mSystems;
    };

} // namespace Dia::AIBudget
```

### AIBudgetModule

```cpp
namespace Dia::AIBudget {

    // Place on SimPU. Configure via manifest JSON:
    // { "budgetUs": 1000 }   — 1000 microseconds (1ms) per tick
    class AIBudgetModule : public Dia::ApplicationFlow::Module {
    public:
        static constexpr Dia::Core::StringCRC kUniqueId{"AIBudgetModule"};

        Dia::Core::StringCRC GetUniqueId() const override { return kUniqueId; }

        // Access the scheduler to register/unregister AI systems.
        // Call from other modules' OnConnectStreams() or DoStart().
        AIBudgetScheduler& GetScheduler();
        const AIBudgetScheduler& GetScheduler() const;

    protected:
        void OnConfigure(const char* configJson) override;  // reads "budgetUs"
        StartResult DoStart() override;                      // registers metrics
        void DoUpdate(float deltaTime) override;             // calls scheduler
        StopResult DoStop() override;                        // unregisters metrics
    };

} // namespace Dia::AIBudget
```

### Config JSON (manifest)

```json
{
  "moduleType": "AIBudgetModule",
  "budgetUs": 1000
}
```

`budgetUs` is integer microseconds. Default if absent: `1000` (1ms). Converted to milliseconds internally before passing to `AIBudgetScheduler::Update()`.

### Metrics

Registered at `DoStart()` via `Dia::Metrics::MetricRegistry`:

| Metric key | Type | Description |
|------------|------|-------------|
| `ai.budget.used_us` | Gauge | Microseconds actually consumed by AI systems last tick |
| `ai.budget.systems_run` | Counter | Number of systems that received a non-zero budget slice this tick |
| `ai.budget.systems_deferred` | Counter | Number of systems skipped because budget was exhausted |

### Log Channel

```cpp
namespace Dia::AIBudget {
    static constexpr Dia::Core::StringCRC kLogChannel{"AIBudget"};
    // DIA_LOG_INFO on Register/Unregister.
    // DIA_LOG_WARNING when budget is exhausted before all systems run.
    // DIA_LOG_ERROR when Register() called beyond kMaxSystems capacity.
}
```

### Typical Integration (game/test code)

```cpp
// In a game Module's DoStart() or OnConnectStreams():
auto& scheduler = mAIBudgetModule.GetScheduler();
scheduler.Register(&mPathfindingAdapter);   // wraps PathfindingSystem::Update(ms)
scheduler.Register(&mStateMachineTickSystem);

// PathfindingSystem adapter (lives in game code, not DiaAIBudget):
class PathfindingBudgetAdapter : public Dia::AIBudget::IAIBudgetedSystem {
    Dia::Core::StringCRC GetSystemId() const override {
        return Dia::Core::StringCRC{"PathfindingSystem"};
    }
    void UpdateBudgeted(float budgetMs) override {
        mPathfindingSystem.Update(budgetMs);  // already honours budget internally
    }
    Dia::Pathfinding::PathfindingSystem<SquarePathGrid>& mPathfindingSystem;
};
```

## Features

| Feature | Description | Spec | Status |
|---------|-------------|------|--------|
| IAIBudgetedSystem | Interface for budget-aware AI systems: `GetSystemId()` + `UpdateBudgeted(float budgetMs)` | inline | Approved |
| AIBudgetScheduler | Fixed-capacity (16) ordered delegate array; per-tick proportional time-slice dispatch via `steady_clock` | inline | Approved |
| AIBudgetModule | `Module` subclass on SimPU; `OnConfigure` reads `budgetUs`; calls scheduler from `DoUpdate` | inline | Approved |
| Budget Metrics | `ai.budget.used_us`, `ai.budget.systems_run`, `ai.budget.systems_deferred` via DiaMetrics | inline | Approved |
| DiaAIBudgetTiers | Critical / Normal / Background priority queues with staleness aging | _(future feature spec)_ | Planned |

## Dependencies on Other Systems

**Required:**
- **DiaApplicationFlow** — `Module` base class (`DoStart`, `DoUpdate`, `DoStop`, `OnConfigure`, `StartResult`, `StopResult`)
- **DiaMetrics** — `MetricRegistry`, `Gauge`, `Counter` for utilisation reporting
- **DiaCore** — `DynamicArrayC` (system registry), `StringCRC` (system IDs, log channel), `DIA_ASSERT`, `DIA_LOG_*`

**Explicitly excluded:**
- **DiaBlackboard** — scheduler is data-agnostic; AI systems read blackboards themselves
- **DiaStateMachine** — DiaAIBudget does not drive FSMs directly; game code wraps them as `IAIBudgetedSystem` adapters
- **DiaOrder** — same as DiaStateMachine; game code wraps `OrderQueue::Update` in an adapter
- **DiaPathfinding** — `PathfindingSystem::Update(ms)` already honours a budget; game code wraps it as an adapter. No compile-time dependency needed.
- **DiaObservation** — metrics go through DiaMetrics only; full trace/log integration deferred to DiaAIBudgetTiers

**Dependents (future):**
- `DiaCondition` — condition evaluator registers as an `IAIBudgetedSystem` when evaluation workload grows
- `DiaRules` — rule set evaluator registers as an `IAIBudgetedSystem`
- `DiaUtilityAI` — async utility evaluation path registers as `IAIBudgetedSystem`; spec notes `AIBudgetScheduler` as hard dependency

## Out of Scope

- Priority tiers (Critical / Normal / Background) — `DiaAIBudgetTiers` feature spec
- Per-entity granularity — systems manage their own entity queues
- Starvation prevention / priority aging — tiers feature
- Dynamic budget (fraction of remaining frame time) — fixed `budgetUs` only in v1
- Cross-PU scheduling — all AI is on SimPU; no multi-PU coordination
- Visual debugger overlay — deferred; `DiaVisualDebugger` extension when tiers land

## Decisions

| ID | Decision | Rationale | Scope | Status | Binding |
|----|----------|-----------|-------|--------|---------|
| AB-001 | Budget unit is microseconds (integer) in config, converted to ms for chrono | Microseconds are the right precision for a 1–2ms AI budget; float ms in config is error-prone (0.001 vs 1.0 confusion). Integer µs in JSON is unambiguous. Converted once at `OnConfigure`. | AIBudgetModule | Accepted | Yes |
| AB-002 | `kMaxSystems = 16`, fixed-capacity `DynamicArrayC`, no STL | PD-004 forbids STL in public APIs. 16 AI systems is generous for any foreseeable game; capacity overflow is a design error caught in Debug. | AIBudgetScheduler | Accepted | Yes |
| AB-003 | Registration order determines call order and implicit priority | Explicit priority tiers deferred to `DiaAIBudgetTiers`. Registration order is simple, debuggable, and sufficient for the shell: register high-priority systems first. | AIBudgetScheduler | Accepted | Yes |
| AB-004 | Systems self-manage internal queues; scheduler dispatches time, not entities | PathfindingSystem already proves this pattern. Avoids per-entity pointer lifetimes in the scheduler (no dangling handle risk). Keeps `IAIBudgetedSystem` interface minimal. | IAIBudgetedSystem | Accepted | Yes |
| AB-005 | `UpdateBudgeted(0.0f)` is a valid call; systems must handle it as a no-op | Budget can be fully consumed by the first system. Subsequent systems receive 0.0f. Requiring a guard inside every implementation is worse than requiring the contract once in the interface doc. | IAIBudgetedSystem | Accepted | Yes |
| AB-006 | Adapters for PathfindingSystem, StateMachine ticking live in game code, not DiaAIBudget | Prevents circular or upward dependencies (DiaAIBudget → DiaPathfinding would be a dependency inversion). Game code wraps what it owns. | Module boundaries | Accepted | Yes |
| AB-007 | Budget is wall-clock (`steady_clock`), not sim-time `deltaTime` | AI work takes real CPU time regardless of sim speed. Frame-rate independence requires measuring actual elapsed clock, not simulated seconds. Matches PathfindingSystem precedent. | AIBudgetScheduler | Accepted | Yes |

**Status values:** `Proposed` · `Accepted` · `Rejected` · `Superseded`
**Binding:** `Yes` = enforced constraint on all features in this system · `No` = guidance only

## Inherited Binding Decisions

| ID | Source | Decision | Implication for this system |
|----|--------|----------|-----------------------------|
| PD-001 | Platform | StringCRC for all entity/component IDs | `IAIBudgetedSystem::GetSystemId()` returns `StringCRC`. Log channel is `StringCRC{"AIBudget"}`. Metric keys are string literals resolved to `StringCRC` at registration. |
| PD-002 | Platform | ProcessingUnit/Phase/Module architecture | `AIBudgetModule` is a `Module` subclass, placed on SimPU. `DoUpdate(deltaTime)` is the tick entry point. |
| PD-004 | Platform | No STL containers in public APIs | `AIBudgetScheduler` uses `Dia::Core::DynamicArrayC<IAIBudgetedSystem*>`. No `std::vector`, `std::queue`, or `std::array` in public headers. |
| PD-005 | Platform | x64 Windows only | `DiaAIBudget.vcxproj` targets x64 exclusively. `std::chrono::steady_clock` is reliable on Windows x64. |
| PD-006 | Platform | Visual Studio project files are source of truth | `DiaAIBudget.vcxproj` and `.vcxproj.filters` created and maintained manually. |
| PD-007 | Platform | C++20 required | Compiled under `/std:c++20`. Interface default destructor syntax, `= default` constructors. |
| PD-008 | Platform | Directory.Build.props owns OutDir/IntDir/toolchain | `DiaAIBudget.vcxproj` must NOT override `OutDir`, `IntDir`, `PlatformToolset`, `WindowsTargetPlatformVersion`, or `LanguageStandard`. |
| PD-009 | Platform | Generated output under `Cluiche/out/` | Any log or metric file output goes under `Cluiche/out/<AppName>/`. |
| AD-001 | Dia App | Module system with YAML frontmatter | Create `dia.aibudget.architecture.module.md` with public API, responsibilities, and dependency declarations. |
| AD-002 | Dia App | No STL containers in public APIs | Reinforces PD-004. Internal chrono calls are STL and permitted (not public API). |
| AD-003 | Dia App | Namespace `Dia::<Module>::` | All types in `Dia::AIBudget::` namespace. |
| AD-004 | Dia App | ProcessingUnit/Phase/Module for application structure | `AIBudgetModule` must be a proper `Module` subclass — not a standalone singleton or service. |

## Open Design Questions

1. **`GetScheduler()` access model** — `AIBudgetModule::GetScheduler()` returns a reference, which means the caller must hold a pointer to `AIBudgetModule`. Is a `ServiceStream<AIBudgetScheduler*>` cleaner for cross-module access, or is direct module-to-module reference acceptable on the same PU? (Low urgency — the shell needs an answer before implementation.)

## Status

`Done` — implemented in commit 28269359. Plan: @docs/specs/applications/dia/systems/diaaibudget/diaaibudget.plan.md
