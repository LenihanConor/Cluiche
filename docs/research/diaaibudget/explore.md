# Research: Explore — DiaAIBudget (Frame-Budget AI Scheduler)

**Session date:** 2026-07-28
**Folder:** docs/research/diaaibudget/

---

## Problem Space Overview

In a game with many AI agents — enemies, NPCs, units — every agent needs to evaluate decisions, update state machines, query pathfinding, and react to world changes. Doing all of this every frame for every agent quickly consumes more CPU than the sim thread's frame budget allows. The naive approach (tick all agents unconditionally) breaks down at ~50–100 entities in a Debug build on a 60Hz sim.

The solution is a **frame-budget scheduler**: an infrastructure module that owns the AI work queue, knows how much time is left in the frame, runs as many evaluations as it can, and **defers the rest** to the next tick. This is analogous to time-sliced I/O, or the way a game's streaming system drains a queue until a bandwidth cap is hit.

For Dia, this belongs on the Sim processing unit as a single `Module` subclass. It sits above domain systems (DiaStateMachine, DiaOrder, DiaPathfinding) and below them at the architectural level — it does not own those systems; it *drives* them within a time envelope. Because `DiaPathfinding::PathfindingSystem<TGraph>::Update(float budgetMs)` already implements this exact pattern, DiaAIBudget generalises it to all AI work.

The scheduler also serves as the natural integration point for future `DiaCondition`, `DiaRules`, and `DiaUtilityAI` evaluations — all of which are per-entity, per-tick, and best run under budget control rather than unbounded per-frame loops.

---

## Existing Approaches (Industry)

**Time-sliced update loops:**
- Drain a work queue until a wall-clock budget (e.g. 1ms) is exhausted; remainder carries over.
- Used in `PathfindingSystem::Update(budgetMs)` — already the Dia precedent.
- Risk: starvation — low-priority items can wait indefinitely if high-priority items saturate the budget.

**Priority-tiered queues:**
- Work items are enqueued with a priority class (Critical / Normal / Background).
- Critical items run first and always complete; Normal items run until budget exhausted; Background items run only if budget remains.
- Used by Unreal Engine's AI task graph — Critical = must-fire-this-frame, Normal = can lag one frame, Background = pathfinding cache refreshes.

**Round-robin with staleness tracking:**
- All agents share equal slices; an agent's "time since last update" is tracked and older agents are prioritised.
- Prevents starvation but does not respect urgency. Good when agents are homogeneous.

**LOD-based update frequencies:**
- Agents far from the player/camera update less often (every N frames instead of every frame).
- Not a scheduler per se — it's a static frequency cap baked into the update rate. Less flexible.
- Can combine with budget scheduler: LOD reduces enqueue frequency, budget controls execution time.

**Work-stealing / job queues:**
- Used in AAA engines with job systems (e.g. Naughty Dog's fiber system). Overkill for Dia today.
- DiaAIBudget should be single-threaded (all AI runs on SimPU by rule [[feedback_simpu_game_logic]]).

**Coroutine-based AI:**
- C++20 coroutines allow an AI evaluation to `co_await` across frames. Elegant but complex.
- The scheduler equivalent is: the coroutine is a "work item" in the queue; resuming it is the work unit. Possible future direction, not the right first design.

---

## Design Axes

| Axis | Options | Notes |
|------|---------|-------|
| Work unit granularity | Per-entity tick vs per-system batch vs per-task | Per-entity is most flexible; per-system is simplest |
| Budget source | Fixed config (e.g. 1ms) vs dynamic (% of remaining frame) | Fixed is predictable; dynamic adapts to frame pressure |
| Priority model | 2-tier (critical/normal) vs 3-tier (+background) vs score-based | 3-tier covers observed needs cleanly |
| Starvation prevention | None vs aging (priority boost after N frames) vs guaranteed-once-per-N | Aging is the industry norm |
| Registration model | Push (systems enqueue items) vs pull (scheduler polls systems) | Push is decoupled; pull needs systems to be queryable |
| Budget unit | Milliseconds vs microseconds vs frame count | ms is readable; µs is the right unit for tight budgets |
| Observability | None vs DiaMetrics counters vs full per-entity trace | Metrics minimum; traces optional |
| Config source | Hardcoded vs `OnConfigure(json)` from manifest | Must be JSON-configurable per `Module::OnConfigure` |

---

## Known Tradeoffs

- **Fixed budget vs dynamic budget**: Fixed is simpler and more deterministic; dynamic responds to variable frame load but can destabilise AI tick rates.
- **Per-entity vs per-system granularity**: Per-entity is more precise but has more overhead (one `steady_clock::now()` call per entity). Per-system is cheaper but coarser (can't interrupt mid-system).
- **Critical tier always completes**: Guarantees important reactions (player attack lands) but means an adversarial system could fill Critical to starve Normal. Must enforce a hard cap on Critical items per frame.
- **Round-robin vs priority within a tier**: Pure FIFO within Normal is unfair if some entities are registered before others. A staleness-based tie-break (longest-waiting first) is fairer.
- **PathfindingSystem integration**: DiaPathfinding already does its own budget in `PathfindingSystem::Update(budgetMs)`. DiaAIBudget could either (a) call `PathfindingSystem::Update()` as a single delegated work item, or (b) let PathfindingSystem continue to self-manage. Option (b) is simpler initially; option (a) gives unified budget accounting.

---

## Known Pitfalls (C++ / game engine context)

- `steady_clock::now()` is not free — calling it per work-unit is fine; calling it per sub-step inside an evaluation is too expensive. Time the work units, not the internals.
- Fixed-capacity queues must not silently drop items — overflow should assert in Debug and degrade gracefully in Release (re-enqueue to next frame rather than drop).
- Items that stay in the queue across frames hold pointers to entities — must guard against dangling pointers if an entity is destroyed while its work item is queued. Use entity version IDs or weak handles.
- Priority inversion: a Normal item that blocks a Critical item (because it holds a lock the Critical path needs) cannot happen if all AI is single-threaded on SimPU. This is one reason the SimPU-only rule matters.
- Temporal aliasing: if the same entity is enqueued multiple times per frame (e.g. both a state machine update and a sensor update), the budget counts them separately. Need to clarify whether "one evaluation per entity per frame" is a constraint.

---

## Cluiche-Specific Opportunities

### Relevant Existing Modules

| Module | Relevance |
|--------|-----------|
| DiaPathfinding | `PathfindingSystem<TGraph>::Update(float budgetMs)` — the canonical budget-drain pattern to generalise. Uses `std::chrono::steady_clock`. |
| DiaApplicationFlow (Module) | `DoUpdate(float deltaTime)`, `OnConfigure(const char* configJson)` — DiaAIBudget is a Module subclass. Config (budgetMs, policy) arrives via `OnConfigure`. |
| DiaStateMachine | `FlatStateMachine<TContext>::Update(dt)` — the primary per-entity work item type. Budget scheduler calls this per entity. |
| DiaOrder | `OrderQueue<TContext>::Update(ctx, dt)` — second work item type. Per-entity action execution. |
| DiaBlackboard | `Blackboard` / `BlackboardComponent` — AI state bus. Work items read/write blackboard slots. Scheduler does not own the blackboard. |
| DiaObservation | `DIA_TRACE_ZONE`, `MetricRegistry` — register budget utilisation gauges (`budget_used_us`, `items_deferred`, `items_run`). |
| DiaAutomation | `AutomationService::RegisterCheckpoint` — CI checkpoints like `"ai.budget.all_evaluated_within_2_frames"`. |
| DiaCore/Timer | `TimerSystem` — background-tier items could be refreshed on a timer rather than every frame. |

### Platform Decision Constraints

| Decision | Implication for DiaAIBudget |
|----------|-----------------------------|
| PD-001 StringCRC | Work item type IDs (e.g. `kStateUpdate`, `kOrderUpdate`, `kConditionEval`) are `StringCRC` constants. |
| PD-002 PU/Phase/Module | DiaAIBudget is a `Module` on SimPU. Called once per `DoUpdate`. No thread concerns (SimPU is single-threaded). |
| PD-004 No STL in public APIs | Work item queue uses `Dia::Core::DynamicArrayC` or a fixed-capacity ring buffer from DiaCore. Not `std::queue` or `std::priority_queue`. |
| PD-005 x64 Windows only | Can use `std::chrono::steady_clock` confidently — no cross-platform timer concerns. |
| PD-007 C++20 | Can use concepts to constrain `IWorkItem<TContext>` callback signatures. Can use `std::span` for item arrays. |

### Precedent: PathfindingSystem Budget Loop

The DiaPathfinding budget loop (reproduced for reference):
```cpp
using Clock = std::chrono::steady_clock;
const auto startTime = Clock::now();
while (!mQueue.empty()) {
    const auto elapsed = std::chrono::duration<float, std::milli>(Clock::now() - startTime);
    if (elapsed.count() >= budgetMs) break;
    // ... process one request
}
```
DiaAIBudget generalises this: instead of one queue of path requests, it manages three priority queues (Critical, Normal, Background) whose items are heterogeneous callbacks.

---

## Open Questions for Ideation

- **Registration API**: Should systems register a callback (`void(float dt)`) per entity-tick, or should they register a batch callback (`void(Span<EntityId>, float dt)`) for all their entities at once? Per-entity is flexible; per-batch is more cache-friendly.
- **Work item lifetime**: Are items re-enqueued automatically every frame (standing subscriptions), or do systems manually re-enqueue each tick? Standing subscriptions are simpler for callers but require the scheduler to handle deregistration.
- **PathfindingSystem relationship**: Should DiaAIBudget absorb PathfindingSystem's internal budget loop, or treat PathfindingSystem as an opaque time-limited delegate? The latter avoids a circular dependency (PathfindingSystem doesn't need to know about DiaAIBudget).
- **Budget split across tiers**: Is it better to give each tier a fixed share (e.g. Critical 30%, Normal 50%, Background 20%) or to run tiers sequentially (Critical fills first, Normal takes what's left, Background uses remainder)? Sequential is simpler but Critical can starve Normal.
- **DiaVisualDebugger integration**: What should the debug overlay show? Per-entity last-evaluation-age? Per-tier utilisation bar? Both?
- **Starvation threshold**: How many frames can a Normal item be deferred before it's automatically promoted to Critical? Is this configurable per item type?
- **Size and capacity**: What is the maximum number of registered work items? 256? 1024? Fixed-capacity is required for PD-004. What happens at overflow?
