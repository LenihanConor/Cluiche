# Research: Ideate — Scalable Simulation Time

**Input:** docs/research/scalab_sim_time/explore.md

## Naming Decisions (session 2026-08-14)

| Thing | Name | Notes |
|---|---|---|
| SimPU manager module | `DiaSimTime` | umbrella; owns all sim time services |
| RenderPU manager | `DiaRenderTime` | simple; reads ServiceStream |
| MainPU manager | `DiaMainTime` | trivial; may stay inside ProcessingUnit |
| SimPU context struct | `SimTimeContext` | passed into SimModule.DoUpdate |
| RenderPU context struct | `RenderTimeContext` | passed into RenderModule.DoUpdate |
| MainPU context struct | `MainTimeContext` | passed into MainModule.DoUpdate |
| SimPU base class | `SimModule` | Option B chosen — shorter, role describes thread |
| RenderPU base class | `RenderModule` | |
| MainPU base class | `MainModule` | |
| Clock hierarchy | `SimTimeDomain` | internal to DiaSimTime |
| Event scheduler | `SimTimeScheduler` | internal to DiaSimTime |
| CPU budget | `SimTimeBudget` | extends AIBudgetScheduler; internal |
| Per-system registry | `SimTimeRegistry` | internal to DiaSimTime |
| Analytical service | `SimTimeAnalytical` | internal to DiaSimTime |
| Per-system LOD | `SimTimePolicy` | data component on each registered system |
| Per-system dormancy | `SimTimeState` | kAwake / kSleeping; internal to DiaSimTime |
| Wake conditions | `SimTimeWakeCondition` | registered per system with DiaSimTime |
| Budget interface | `ISimTimeBudgetedSystem` | replaces IAIBudgetedSystem |
| Analytical interface | `ISimTimeAnalytical` | implemented by systems wanting lazy eval |

## Candidates After Refinement

**Dropped / absorbed:**
- C5 SimSleep → collapsed into DiaSimTime (SimTimeState + SimTimeWakeCondition)
- C6 SimBudget → absorbed as SimTimeBudget (extend AIBudgetScheduler, not rebuild)
- C7 SimIsland → not a framework concept; a SimModule with a coherent system set IS an island

---

### Candidate 1: DiaSimClock
**Home module/system:** New `DiaSimClock` (or evolution of `DiaCore/Time/TimeServer`)
**Size:** M  *(upgraded from S — see design decision below)*

> **Design decision (session 2026-08-14):** Module signature change uses **Option B — typed base classes per PU role** (`SimModule`, `RenderModule`, `MainModule`), NOT a blanket `DoUpdate(SimTimeContext&)` change.
> Rationale: typed base classes enforce the thread contract at compile time — a `SimModule` cannot be registered on `RenderProcessingUnit`; a `RenderModule` cannot advance sim time. This eliminates cross-thread state access bugs by making the wrong thing uncompilable. Widens C1 from S → M but is a prerequisite for all higher candidates. Option A (ServiceStream pull, no signature change) was ruled out because it leaves the thread-safety problem unsolved by convention rather than type system.
**Description:**
Wire a first-class simulation clock into the Module/PU tick pipeline. Currently `TimeServer` exists in DiaCore as a standalone utility (fixed-step logical clock, time-scale, int64 microseconds) but `ProcessingUnit.Update()` passes raw wall-clock `float deltaTime` to every module. This candidate replaces or augments `DoUpdate(float deltaTime)` with a `SimTimeContext` struct containing: `GameTime currentTime`, `TimeRelative deltaTime`, `uint64_t tick`, `float timeScale`, and `bool isPaused`. The ProcessingUnit owns one `TimeServer` per PU (or receives it from a shared clock service) and injects it each tick.

A SimClock service also unblocks `FrameStreamStore::FetchClosestTo(TimeAbsolute)`, which is currently stubbed because there is no reliable sim-time source to pass to it.

**Primary value:** Every module, physics world, AI system, and animation player shares one canonical timestamped tick instead of each reconstructing time from an accumulated float. Enables save/load correctness, deterministic replay, and accurate interpolation between sim and render.

---

### Candidate 2: DiaTimeDomain
**Home module/system:** New `DiaTimeDomain` (DiaCore or DiaApplicationFlow)
**Size:** M
**Description:**
A hierarchy of named clocks where child clocks derive their current time and scale from a parent. The world clock drives gameplay; a local domain clock can run at a different scale (a slow-motion spell, a fast-forward cutscene, an isolated encounter). Pausing the world clock does not pause the UI clock. Each domain clock exposes: `SetScale(float)`, `Pause()`, `Resume()`, `Step(TimeRelative)`, `AdvanceTo(TimeAbsolute)`, and `Now() → TimeAbsolute`.

Conceptually: `WorldClock → SimDomainClock(faction/encounter) → EntityLocalClock(optional)`. Clock trees are registered with a `TimeDomainRegistry` (StringCRC keys). Modules request a clock by name; they do not hold raw floats.

This is the whitepaper's "Game Time / Simulation Time / Local Time Domains" concept fully realised. It subsumes DiaSimClock (Candidate 1) as its simplest form (a single-domain case).

**Primary value:** Slow motion, fast-forward, paused cutscenes, and isolated encounter time all work correctly without any module knowing about the others. Replay and save/load have a single authority to snapshot.

---

### Candidate 3: DiaScheduler
**Home module/system:** New `DiaScheduler` module on SimPU
**Size:** M
**Description:**
An engine-level event scheduler that replaces per-frame polling for time-driven behaviour. Public API: `Schedule(entityId, atTime, eventType, payload) → ScheduleHandle`, `ScheduleAfter(entityId, delay, ...) → handle`, `ScheduleRecurring(entityId, period, ...) → handle`, `Cancel(handle)`, `Reschedule(handle, newTime)`. Events fire as `EventStream` messages on SimPU at the correct simulation tick.

Implementation: bucketed timer wheel (O(1) insert/fire for near-future events) with a min-heap overflow for far-future events. The scheduler ticks against the sim clock (Candidate 1) not wall-clock time, so it is deterministic and survives pause/resume/fast-forward automatically.

Immediate consumers: AI behaviour triggers ("Bob becomes hungry at 12:00"), shop open/close cycles, crop grow events, HTN divergence checks at scheduled intervals, DiaAIBudget deferral with a guaranteed retry time rather than "maybe next frame."

**Primary value:** Simulation cost becomes proportional to things *happening*, not things *existing*. A world with 10,000 scheduled NPCs has near-zero per-frame cost between events.

---

### Candidate 4: DiaSimPolicy
**Home module/system:** New `DiaSimPolicy` component + `SimPolicyModule` on SimPU
**Size:** M
**Description:**
A per-entity (and per-simulation-island) `SimulationPolicy` component that declares the entity's fidelity requirements: `RequiredFidelity` (Immediate / High / Medium / Low / Dormant / Analytical), `MaxUpdateIntervalMs`, `CanSleep`, `CanAnalyticallyAdvance`, and `WakeConditions`. A `SimPolicyModule` reads these policies and, each tick, gates which entities' systems run.

Fidelity tiers map to Hz bands:
- **Immediate** — every SimPU tick (60 Hz default)
- **High** — 10–30 Hz
- **Medium** — 1–5 Hz
- **Low** — every few seconds
- **Dormant** — event-driven only
- **Analytical** — state advanced on query, no tick

The module integrates with the existing `AIBudgetScheduler` (AI systems respect the entity's policy tier) and extends the concept to physics (suppresses PhysicsWorld step contribution from Low/Dormant bodies beyond their own velocity-sleep) and animation (suppresses AnimClipPlayer update for off-screen entities).

**Primary value:** Individual gameplay systems stop independently inventing distance checks and tick-skipping. The engine owns the fidelity decision; systems opt in by checking policy.

---

### Candidate 5: DiaSimSleep
**Home module/system:** New `DiaSimSleep` module on SimPU
**Size:** M
**Description:**
An engine-standard dormancy system with explicit wake semantics. A `SleepHandle` is acquired for any entity or simulation group; sleeping means zero CPU until a wake condition fires. Wake conditions are composable: `OnScheduledTime(T)`, `OnPlayerEntersRadius(r)`, `OnMessageReceived(typeId)`, `OnPropertyChanged(blackboardSlot)`, `OnQueryForces`. Each condition is a small registered callback evaluated lazily.

The sleep system integrates with `DiaScheduler` (Candidate 3) for time-based wakes, `DiaStreams` `EventStreamStore` for message-based wakes, and `DiaBlackboard` observers for property-based wakes. When a wake trigger fires, the dormant entity's `SleepHandle` is released and `SimPU` resumes normal updates for it next tick.

The whitepaper's "Village 17: sleeping since Day 43, next interesting event: Market opens Day 44 08:00, wake triggers: player enters region" is the direct spec for this candidate.

**Primary value:** Entire chunks of the world — towns, factions, ecosystems — disappear from active CPU cost without disappearing from the world. When something interesting happens, they wake.

---

### Candidate 6: DiaSimBudget
**Home module/system:** Evolution of `DiaAIBudget` → generalised `DiaSimBudget` module on SimPU
**Size:** M
**Description:**
Generalise `AIBudgetScheduler` from AI-only to all simulation domains. A `SimBudgetModule` owns a configurable CPU time budget (e.g., 4 ms/frame) partitioned across priority classes: Critical (unlimited), High (2 ms), Background (1 ms), Catch-up (0.5 ms), Maintenance (0.5 ms). Any `ISimulatedSystem` (implementing the existing `IAIBudgetedSystem` pattern) registers at a priority tier.

When the budget is exhausted for a tier, remaining work is **deferred with a deadline** (not silently dropped as today). A deferred system has a maximum latency before it is promoted to a higher tier. This prevents starvation. The scheduler also emits metrics: `sim.budget.used_ms`, `sim.budget.deferred_count`, `sim.budget.stale_ms_max` (oldest deferred work).

**Primary value:** Frame spikes from large simulation loads become fidelity degradation instead. The engine reduces update frequency rather than blowing the frame budget. AI, physics step counts, and animation updates all participate in the same budget.

---

### Candidate 7: DiaSimIsland
**Home module/system:** New `DiaSimIsland` module on SimPU
**Size:** L
**Description:**
A simulation island is a named group of entities that share a fidelity tier and sleep/wake state. Islands can be spatial (all entities within a region) or semantic (a faction, a household, an economy, an encounter). An island has its own `SimulationPolicy` that overrides member entities' policies downward. When an island sleeps, all members sleep. When it wakes, all members wake.

Islands compose with `DiaSimPolicy` (Candidate 4), `DiaSimSleep` (Candidate 5), and `DiaScheduler` (Candidate 3). The `SimIslandRegistry` (StringCRC keys) maintains the island graph. An entity can belong to multiple islands (e.g., an NPC is in the "Nearby Town" spatial island and the "BlacksmithGuild" semantic island; the more demanding policy wins).

The whitepaper's architecture tree — Player Area at 60 Hz, Nearby Town at 2 Hz, Distant Kingdom event-driven, Economy analytical — is directly expressible as four islands.

**Primary value:** Scalability becomes a data-driven configuration problem rather than a per-system engineering problem. A designer declares island membership; the engine handles the rest.

---

### Candidate 8: DiaAnalyticalSim
**Home module/system:** New `DiaAnalyticalSim` service (DiaCore or DiaApplicationFlow)
**Size:** L
**Description:**
The "analytical advancement" and "query-forces-correctness" concepts from the whitepaper. Systems that support analytical advancement implement `IAnalyticalSystem`: `AdvanceTo(entityId, toTime)` and `GetEvaluatedState(entityId, atTime)`. The engine tracks `LastEvaluated` timestamps per-entity per-system. A query for `bob.GetHunger()` triggers: is state stale? → compute `hunger += rate * elapsed` → return current value.

Supports three model types: **Continuous** (linear/exponential formulas: hunger, crop growth, resource accumulation, relationship decay), **Discrete** (replays a small event log from last checkpoint to query time: factory production steps, travel waypoints), and **Tick** (runs N compressed ticks at reduced fidelity for systems that have no closed form).

The `LastEvaluated` timestamp must be serialised with entity state for save/load correctness. This candidate is the hardest to implement correctly but delivers the most dramatic scaling improvement for large dormant populations.

**Primary value:** A world with 100,000 dormant NPCs has zero per-frame simulation cost. When queried or awakened, each NPC's state is instantly correct for the current game time — years of hunger, crop growth, and faction relationships materialise in microseconds.

---

### Candidate 9: DiaSimManager
**Home module/system:** New `DiaSimManager` module (umbrella system on SimPU)
**Size:** XL
**Description:**
The full architecture from the whitepaper implemented as a single coherent system. Owns and integrates: a `TimeDomainRegistry` (Candidate 2), a `DiaScheduler` (Candidate 3), a `SimIslandRegistry` (Candidate 7), a `SimBudgetModule` (Candidate 6), an `AnalyticalSimService` (Candidate 8), and a `DormancyManager` (Candidate 5). Modules and gameplay systems interact with `DiaSimManager` as a unified service rather than reaching into its components separately.

Provides the complete architecture tree the whitepaper describes:

```
Time Service (TimeDomain)
     │
     ├── Scheduler
     ├── Simulation Clock
     └── Timers
           │
     Simulation Manager (SimIslands + Budget)
           │
     ┌─────┼─────┐
   Active  Reduced  Sleeping
           │
     Simulation Islands
           │
     ┌─────┼─────┐
   Tick   Events  Analytical
```

This candidate is the target end-state, not a starting point. It is most appropriate as a system spec that owns several feature specs aligned to Candidates 1–8 above.

**Primary value:** Gameplay can create 100,000 simulated entities without implicitly creating 100,000 things that execute every frame. The architecture is coherent rather than a patchwork of independent LOD mechanisms per domain.

---

## Coverage Map

The nine candidates span the full range of the whitepaper's five concepts and the design axes from explore.md:

| Whitepaper concept | Candidates |
|---|---|
| First-class simulation clock | C1 (DiaSimClock), C2 (DiaTimeDomain) |
| Scheduling as engine primitive | C3 (DiaScheduler) |
| Simulation fidelity tiers | C4 (DiaSimPolicy), C7 (DiaSimIsland) |
| Catch-up / analytical simulation | C8 (DiaAnalyticalSim) |
| Wake / sleep semantics | C5 (DiaSimSleep) |
| CPU budget | C6 (DiaSimBudget) |
| Full integrated architecture | C9 (DiaSimManager — umbrella) |

**Scope range:** S (DiaSimClock, one week, one module) through XL (DiaSimManager, multi-month, umbrella system). Candidates 1–3 are clearly separable and foundational; Candidates 4–8 are the scalability layer; Candidate 9 is the long-term architecture target.

**Design axes covered:** Clock representation (C1/C2), clock granularity (C2), scheduler data structure (C3), fidelity decision authority (C4), dormancy model (C5), budget enforcement (C6), simulation island scope (C7), analytical model (C8).

**First-consumer coverage:** Physics (C4, C6), animation (C4), AI — all five systems (C3, C4, C5, C6), gameplay (C3, C5, C7, C8), render interpolation (C1), save/load (C1, C8).
