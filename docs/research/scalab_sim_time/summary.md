# Research Summary — Scalable Simulation Time

**Session folder:** docs/research/scalab_sim_time/
**Date:** 2026-08-14

## One-Line Answer

Build `DiaSimTime` — a five-phase system that gives Dia a first-class simulation clock, event scheduler, per-system LOD policy, dormancy management, and (in phase 5) analytical advancement, so the engine can simulate 100,000 things without executing 100,000 things every frame.

## Journey

1. **Explored:** Dia already has `TimeServer` (int64 logical clock), `AIBudgetScheduler` (budget distribution), and `Body2DBase` sleep (per-body LOD) but they are isolated, incompatible, and none are wired to the Module/PU tick pipeline. The only existing per-entity frequency reduction is `UtilitySetComponent.eval_period_ticks` — a counter-based tick skip that breaks silently if SimPU Hz changes.
2. **Ideated:** 9 candidates generated spanning S–XL; refined to 5 after discussion collapsed SimSleep into DiaSimTime, absorbed SimBudget into AIBudgetScheduler, and dropped SimIsland as a pattern rather than a framework feature.
3. **Evaluated:** SimTimeScheduler scored highest on standalone value (4.05); Foundation and DiaSimTime tied at 3.95; SimTimeAnalytical scored lowest (2.85) due to correctness risk and large-scale-only payoff.
4. **Chose:** Build all five phases in dependency order. User's stated reason: do it all.

## Chosen Work Item

**Name:** DiaSimTime  
**Home module:** New system — suggested parent application: Dia  
**Suggested spec type:** System (phases 1–4) + one follow-on Feature spec (phase 5 SimTimeAnalytical)  
**Estimated size:** L (phases 1–4 combined) + L (phase 5)

## Build Order

| Phase | Deliverable | Size |
|---|---|---|
| 1 | Foundation: TimeServer additions, typed base classes, context structs, DiaRenderTime + DiaMainTime | M |
| 2 | SimTimeDomain: named clock hierarchy, pause/scale/step any subtree | M |
| 3 | SimTimeScheduler: timer wheel, fire-at-T, recurring, ticks against sim clock | M |
| 4 | DiaSimTime: umbrella module assembling phases 1–3 + SimTimeBudget + SimTimeRegistry + SimTimeState | L |
| 5 | SimTimeAnalytical: AdvanceTo, LastEvaluated, query-forces-correctness, lazy eval | L |

## Key Insights from Exploration

- **TimeServer is already 80% of Foundation.** The int64 microsecond time model is correct. What's missing: `Pause()`, `Resume()`, `Step()`, `AdvanceTo()`, and stripping the thread-sleep out of `Tick()`. No new class needed.
- **AIBudgetScheduler is already 80% of SimTimeBudget.** Add priority tiers and deadline/carry-forward, rename the interface `ISimTimeBudgetedSystem`, absorb into DiaSimTime. Existing UtilityAI and HTN implementations get it for free.
- **Typed base classes (Option B) solve a real bug class.** `SimModule` / `RenderModule` / `MainModule` make cross-thread module placement a compile error. `SimModule` cannot register on `RenderProcessingUnit`. The wrong thing becomes uncompilable rather than just ill-advised.
- **SimIsland is a pattern, not a feature.** A `SimModule` that owns a coherent group of systems and applies consistent `SimTimePolicy` settings to them IS a simulation island. No framework layer needed.
- **SimSleep collapses to three lines.** Dormancy is a `SimTimeState` enum (kAwake/kSleeping) per registered system plus a wake-condition registration API that delegates to Scheduler (time), EventStreamStore (message), and Analytical (query). Not a standalone system.
- **FrameStreamStore.FetchClosestTo() gets unblocked by Foundation.** It has been stubbed since it was written, waiting for a reliable sim-time source. Phase 1 provides it.
- **SimTimeAnalytical is the hardest piece.** Correctness constraints (hidden dependencies, stale reads, save/load of LastEvaluated timestamps) make this non-trivial. The hook point is designed into DiaSimTime (sleeping systems via SimTimeState == kSleeping). Build it as a phase 5 feature spec, not part of the system spec.
- **The unit of work is a system, not an entity.** DiaSimTime gates registered *systems* (SimModules, IAIBudgetedSystems) — not individual DiaEntity instances. Per-entity fidelity decisions are internal to each system.

## Naming (locked)

| Thing | Name |
|---|---|
| SimPU manager | `DiaSimTime` |
| RenderPU manager | `DiaRenderTime` |
| MainPU manager | `DiaMainTime` |
| SimPU base class | `SimModule` |
| RenderPU base class | `RenderModule` |
| MainPU base class | `MainModule` |
| SimPU context struct | `SimTimeContext` |
| RenderPU context struct | `RenderTimeContext` |
| MainPU context struct | `MainTimeContext` |
| Clock hierarchy | `SimTimeDomain` |
| Event scheduler | `SimTimeScheduler` |
| CPU budget | `SimTimeBudget` |
| Per-system registry | `SimTimeRegistry` |
| Analytical service | `SimTimeAnalytical` |
| Per-system LOD | `SimTimePolicy` |
| Per-system dormancy | `SimTimeState` (kAwake / kSleeping) |
| Wake conditions | `SimTimeWakeCondition` |
| Budget interface | `ISimTimeBudgetedSystem` |
| Analytical interface | `ISimTimeAnalytical` |

## Discarded Candidates

| Candidate | Why discarded |
|---|---|
| SimIsland (C7) | A design pattern — a SimModule with a coherent system set IS an island. No framework layer adds value. |
| SimSleep as standalone (C5) | Collapses to SimTimeState field + wake registration API inside DiaSimTime. Not a separate feature. |
| SimBudget as standalone (C6) | AIBudgetScheduler already covers 80% of it. Extend in place and absorb into DiaSimTime. |

## References

- docs/research/scalab_sim_time/explore.md
- docs/research/scalab_sim_time/ideate.md
- docs/research/scalab_sim_time/evaluate.md
- docs/research/scalab_sim_time/choose.md
