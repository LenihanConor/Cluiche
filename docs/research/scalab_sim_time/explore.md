# Research: Explore — Scalable Simulation Time

**Session date:** 2026-08-14
**Folder:** docs/research/scalab_sim_time/

## Problem Space Overview

Game engines traditionally advance the world by calling `Update(float deltaTime)` on every active object every frame. At small entity counts this is fine. As entity counts grow — agents with AI, dynamic physics bodies, crops, economies, populations — the cost of "update everything always" scales linearly and eventually breaks the frame budget. The problem is not that individual updates are expensive; it is that the engine has no concept of *which* things deserve computation *right now* and which can wait hours, seconds, or forever.

The deeper issue is that update frequency, simulation fidelity, and simulated time are three separate concepts that are currently collapsed into one: "how many times does this run per wall-clock second?" A system that can only tune Hz at the ProcessingUnit level (the only current knob in Dia) cannot differentiate an NPC standing next to the player from one sleeping on another continent. Both run at SimPU's frequency or not at all.

The whitepaper frames the solution as making **simulation fidelity independent of simulation time**. This has two distinct parts: (1) a first-class simulation clock that can be paused, scaled, stepped, and queried independently of frame rendering, and (2) a simulation manager that decides how much of the world actually executes per tick, using LOD tiers, event scheduling, sleep/wake semantics, and analytical catch-up for dormant regions. Together these shift the engine's contract from "execute everything" to "execute what matters, advance everything else correctly."

The scope for Dia is broad: physics bodies already have a sleep mechanism, AIBudgetScheduler already distributes a time budget across AI systems, and UtilitySetComponent already has a tick-skip counter. But these mechanisms are isolated, incompatible, and none of them are anchored to a shared simulation clock. The opportunity is to unify and generalise them under a coherent simulation time architecture.

## Existing Approaches

Industry patterns for scalable simulation:

- **Fixed-step accumulator with sub-stepping** — physics world drains wall-clock dt into fixed steps. Dia already does this (RigidBody2D, SoftBody2D). Does not address fidelity tiers across entities.
- **Velocity-threshold body sleep** — bodies below motion threshold stop integrating. Dia already does this (Body2DBase). Per-body, automatic, threshold-driven. Limited to physics.
- **Timer-wheel / bucketed event scheduler** — instead of polling every frame, systems register "fire at T" events sorted into time buckets. O(1) insert, near-O(1) dispatch. Common in MMO servers and city-builder engines.
- **Interest management / LOD tiers** — spatial or importance partitioning that assigns entities to update frequency classes. Used in Sims-family, Dwarf Fortress, Rimworld, Crusader Kings.
- **Analytical advancement / lazy evaluation** — dormant state is not stepped frame-by-frame; instead, a closed-form or approximate formula computes the state at any queried time. Common in city-builder economy systems, stardew-style crop growth.
- **Simulation islands** — groups of entities that interact, simulated together or dormant together. Physics engines use this for contact graph components. Extend to gameplay domains (factions, ecosystems, households).
- **ECS archetypal chunks** — entities of the same component composition share memory; update loops can skip entire archetypes. Unity DOTS, EnTT, Flecs. Not yet in Dia.
- **Discrete event simulation (DES)** — world advances by dispatching events from a priority queue ordered by scheduled time. Between events, nothing runs. Used in network simulators, supply-chain simulation, some RTS economies.
- **Hierarchical time scales** — parent clocks that govern child clocks; pausing a world clock can pause all its children (UI, narration) independently of wall clock. Godot TimeScale, Unity Time.timeScale.
- **Budget-based simulation** — frame allocated a CPU budget; high-priority work runs first, lower-priority work is deferred or reduced-fidelity when budget is exhausted. Dia has a prototype of this in AIBudgetScheduler.

## Design Axes

| Axis | Options | Notes |
|------|---------|-------|
| Clock architecture | Single game clock vs hierarchy of named clocks | Hierarchy allows UI/narration/replay to be independent |
| Clock representation | Float accumulation vs integer ticks | Integer is deterministic, no drift; floats accumulate error over long sessions |
| Simulation granularity | Per-PU (current) → Per-Module → Per-entity → Per-component | Finer granularity = more power, more bookkeeping |
| LOD decision authority | Entity decides its own rate vs engine assigns tier | Engine-assigned is more coherent; entity-decided is simpler to implement |
| LOD criteria | Distance-based vs importance-based vs budget-based | Distance is the most common; importance handles non-spatial cases (economy, quests) |
| Dormancy model | Hard skip (no update) vs soft skip (throttled Hz) vs analytical advance | Analytical advance is most powerful but requires invertible state functions |
| Scheduler data structure | Per-frame poll vs timer-wheel vs priority queue (heap) vs bucketed queue | Timer-wheel is O(1) for uniform durations; heap is O(log N) general |
| Wake trigger model | Passive (query forces eval) vs active (wake event dispatched) vs hybrid | Hybrid: events wake sleeping entities; queries force correct evaluation |
| Budget enforcement | Best-effort (skip low priority) vs hard budget (reduce fidelity) vs none | Hard budget prevents frame spikes; best-effort is simpler |
| Analytical model | None vs continuous formula vs discrete event replay vs checkpoint+replay | Formulas cover linear/exponential growth; DES covers complex discrete logic |
| Thread placement | All on SimPU vs scheduler on its own PU vs distributed | SimPU is natural home; separate PU for scheduler adds latency but frees main/render |
| Simulation island scope | Spatial only vs semantic (faction, household, encounter) vs mixed | Mixed allows economy-wide islands to be dormant independently of spatial concerns |

## Known Tradeoffs

- **Analytical advance vs interactive accuracy** — analytically advanced state is an approximation. If the formula is wrong or the state has hidden dependencies (player actions that were not recorded), the result is wrong. Systems using this must be explicitly designed for lazy evaluation.
- **Integer clocks vs float dt** — existing Dia modules all receive `float deltaTime`. Changing the signature of `DoUpdate` is a broad breaking change. Migration path must be careful.
- **Finer LOD granularity vs scheduler overhead** — per-entity scheduling requires maintaining a data structure over potentially thousands of entries. The scheduler itself must be cheap enough that its overhead does not exceed the savings.
- **Determinism vs performance** — deterministic replay and save/load require that simulation state is fully serialisable and that clock advancement is exact. Float accumulation breaks both. This is a design constraint that must be decided early.
- **Simulation islands vs cross-island interactions** — an NPC in a dormant island receiving a message from an active island requires the dormant island to either wake or to handle the message analytically. Cross-island interaction is the hardest problem in this space.
- **Physics sleep vs simulation sleep** — physics bodies already have their own sleep system. A higher-level simulation sleep must either compose with or supersede the physics sleep. Duplication is a maintenance risk.
- **Event-driven vs tick-driven AI** — the current AI stack (Blackboard, Conditions, Rules, UtilityAI, HTN) is tick-driven and caller-driven. Migrating to event-driven scheduling requires adding wake triggers to these systems, which is non-trivial.

## Known Pitfalls (C++ / game engine context)

- **Float time drift** — accumulating `float deltaTime` over long sessions produces measurable error. A 10-hour play session at 60 Hz drifts by ~seconds. Use `int64_t` microseconds (Dia's `TimeAbstract` already does this) for the canonical simulation clock.
- **Inconsistent time sources** — Dia currently has `SystemClock` (wall clock), `TimeServer` (logical fixed-step), `TimeThreadLimiter` (rate limiter), and raw `float deltaTime` in modules. These are all disconnected. A new sim time system must not add a fifth disconnected source.
- **Stale-read hazards** — lazy evaluation means a query can silently return an out-of-date value if the caller forgets to trigger evaluation. This is the biggest correctness hazard in the analytical advance pattern.
- **Wake storms** — if many sleeping entities share a wake condition (e.g., "day changes"), they all wake simultaneously and spike the frame budget. Wake events need to be staggered or budget-gated.
- **Scheduler callback re-entrancy** — a scheduled callback that reschedules itself (recurring events) must be careful about re-entrancy if the scheduler is called from within a callback.
- **Cross-thread scheduling** — if the scheduler is on SimPU and a render or main thread query forces an evaluation, there is a thread-safety hazard. The lazy-eval path must be thread-safe or restricted to SimPU.
- **Save/load correctness** — dormant entities must serialise their `LastEvaluated` timestamp alongside their state. If a save is loaded without restoring this timestamp, analytical advancement will produce wrong results.
- **AI system `eval_period_ticks`** — the current tick-skip in UtilitySetComponent is counter-based, not time-based. If SimPU Hz changes, the effective update rate changes silently. Time-based thresholds are more robust.

## Cluiche-Specific Opportunities

### Relevant Existing Modules

| Module | Relevance |
|--------|-----------|
| `DiaCore/Time/TimeServer` | Fixed-step logical clock with time-scale and tick counter. **Direct building block** for a sim clock — not yet wired to the Module/PU tick pipeline. |
| `DiaCore/Time/TimeAbstract` / `TimeAbsolute` / `TimeRelative` | Strong-typed `int64_t` microsecond time values. The right foundation for a deterministic sim clock. |
| `DiaCore/Timer/TimerExpiry` | Per-entity countdown timer with configurable duration. Basis for per-entity sleep/wake timers and event scheduling. |
| `DiaApplicationFlow/ProcessingUnit` | Owns the `frequencyHz` knob; drives all module updates; emits `pu.over_budget` warning. The insertion point for sim-time injection. |
| `DiaApplicationFlow/Module` | `DoUpdate(float deltaTime)` is the interface to replace/augment with simulation time context. |
| `DiaAIBudget/AIBudgetScheduler` | Per-frame CPU budget distribution across AI systems. The most developed existing fidelity-tier mechanism. Deferred systems skip entirely — needs fairness and carry-forward. |
| `DiaAIBudget/IAIBudgetedSystem` | Interface for budget-aware AI. Could generalise to `ISimulatedSystem` for non-AI domains. |
| `DiaUtilityAI/UtilitySetComponent` + `eval_period_ticks` | Per-entity tick-skip counter — the only per-entity LOD today. Time-based version of this is a v2 of this feature. |
| `DiaHTN/HTNPlannerComponent` | `HasDiverged(ctx)` and `ReplanAsync` are designed for caller-controlled tick frequency. Natural first consumer of LOD tiers. |
| `DiaRigidBody2D/Bodies/Body2DBase` | Per-body `SleepState` + velocity threshold + sleep timer. Most complete existing LOD. Must compose cleanly with a higher-level simulation sleep. |
| `DiaStreams/EventStreamStore` | Frame-batching flush model explicitly supports producers/consumers at different rates. Natural delivery mechanism for scheduled events across PU boundaries. |
| `DiaStreams/FrameStreamStore` | `FetchClosestTo(TimeAbsolute)` is currently stubbed. A working sim clock would enable this to interpolate correctly for render. |

### Platform Decision Constraints

| Decision | Implication for this topic |
|----------|---------------------------|
| PD-001 StringCRC | Simulation island IDs, scheduler event type IDs, and wake trigger names must use StringCRC, not raw strings. |
| PD-002 ProcessingUnit/Phase/Module | A simulation manager must be a Module on SimPU. The scheduler and clock should be engine-level services accessed via ServiceStream, not singletons. |
| PD-004 No STL in public APIs | Scheduler's public API (register event, cancel event, reschedule) must use DiaCore containers. Priority queue or timer wheel implementation uses DiaCore containers internally. |
| PD-006 VS project files are source of truth | Any new Dia modules (DiaSimClock, DiaScheduler, DiaSimManager) require vcxproj entries via `dia docs vcxproj-add`. |
| PD-007 C++20 required | Concepts can enforce the `ISimulationTarget` interface (HasDiverged, AdvanceTo) cleanly. `std::chrono` is available for high-res wall-clock fallback. |
| PD-008 Directory.Build.props owns OutDir | No build setting changes needed — new modules are libraries following the existing pattern. |

## Open Questions for Ideation

- Should the simulation clock be a new dedicated module (`DiaSimClock`) or an evolution of the existing `TimeServer` in DiaCore?
- Should simulation fidelity tiers be managed by a single engine-level `SimulationManager` module, or should each domain (AI, physics, animation) manage its own LOD independently?
- What is the minimum viable version? A first-class sim clock + scheduler alone would unlock a large class of improvements even without full LOD tiers.
- How should the existing `AIBudgetScheduler` relate to a general simulation budget? Subsume it, compose with it, or leave it parallel?
- Is the analytical advancement pattern worth designing for in v1, given its correctness constraints, or should it be a clearly-deferred v2?
- How should simulation islands be scoped? Pure spatial, semantic (faction/household/ecosystem), or user-defined? And who owns island membership?
- What is the save/load contract for a dormant entity with `LastEvaluated` state? This must be decided before the first implementation ships.
- Should `FrameStreamStore.FetchClosestTo` be unblocked as part of this work, since a working sim clock would enable correct interpolation?
- How does the sleep/wake system interact with the existing RigidBody2D body sleep? Override, compose, or unify?
- Should the scheduler live on SimPU as a Module, or should it be a DiaCore utility (no thread affinity) that callers drive explicitly?
