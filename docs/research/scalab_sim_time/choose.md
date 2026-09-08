# Research: Choice — Scalable Simulation Time

**Date:** 2026-08-14
**Chosen candidate:** DiaSimTime — full system, all candidates in build order

## Rationale

Build the complete scalable simulation time architecture. All five candidates are in scope, delivered in dependency order. The user's stated reason: do it all. The evaluation confirmed no candidate is architecturally wrong or wasteful — SimTimeAnalytical scored lower on near-term return but fits the long-term Sims/RPG ambition of the engine and the hook point (SimTimeState in DiaSimTime) is already designed to receive it.

## What Was Ruled Out

Nothing. All five build items are in scope.

## Build Order

| Phase | Item | Size | Gate |
|---|---|---|---|
| 1 | Foundation — TimeServer additions, typed base classes (SimModule / RenderModule / MainModule), context structs (SimTimeContext / RenderTimeContext / MainTimeContext), DiaRenderTime + DiaMainTime shells | M | prerequisite for all others |
| 2 | SimTimeDomain — named clock tree, world/encounter/entity hierarchy, Pause/scale/step any subtree | M | prerequisite for Scheduler correctness |
| 3 | SimTimeScheduler — timer wheel, fire-at-T, recurring events, ScheduleHandle, ticks against SimTimeDomain | M | ships standalone value immediately |
| 4 | DiaSimTime — umbrella module on SimPU; assembles SimTimeDomain + SimTimeScheduler + SimTimeBudget (extended AIBudgetScheduler) + SimTimeRegistry + SimTimeState + SimTimeWakeCondition; publishes SimTimeContext via ServiceStream | L | capstone; depends on phases 1–3 |
| 5 | SimTimeAnalytical — ISimTimeAnalytical, AdvanceTo(system, t), LastEvaluated stamps, query-forces-correctness, continuous / discrete / tick models | L | v2 phase; plug-in point already designed in DiaSimTime |

## Pre-Spec Commitments

**Naming (locked):**
- Module: `DiaSimTime` (SimPU), `DiaRenderTime` (RenderPU), `DiaMainTime` (MainPU)
- Base classes: `SimModule`, `RenderModule`, `MainModule` (Option B — shorter)
- Context structs: `SimTimeContext`, `RenderTimeContext`, `MainTimeContext`
- Internal services: `SimTimeDomain`, `SimTimeScheduler`, `SimTimeBudget`, `SimTimeRegistry`, `SimTimeAnalytical`
- Per-system data: `SimTimePolicy`, `SimTimeState` (kAwake/kSleeping), `SimTimeWakeCondition`
- Interfaces: `ISimTimeBudgetedSystem` (replaces IAIBudgetedSystem), `ISimTimeAnalytical`

**Architectural decisions (locked):**
- Option B typed base classes: `SimModule` cannot register on RenderProcessingUnit; wrong placement is a compile error
- Sleep stripped from TimeServer.Tick() — PU owns rate-limiting, clock owns only logical advancement
- TimeServer gains: `Pause()`, `Resume()`, `Step()`, `AdvanceTo(TimeAbsolute)`
- SimTimeContext is the only framework change visible to module authors — all gating (policy / sleep / budget) is internal to DiaSimTime
- SimBudget is an extension of AIBudgetScheduler (priority tiers + deadlines), not a rebuild; AIBudgetScheduler absorbed into DiaSimTime
- SimIsland is a design pattern (a SimModule with a coherent system set), not a framework feature
- DiaSimTimeManager → renamed DiaSimTime throughout
- SimTimeAnalytical deferred to Phase 5 but hook point exists: sleeping systems (SimTimeState == kSleeping) is where analytical advancement plugs in

**Scope note:**
- Phase 1–4 is the system spec: `DiaSimTime`
- Phase 5 is a follow-on feature spec: `SimTimeAnalytical` under `DiaSimTime`
- Suggested parent system: DiaApplicationFlow (or a new top-level simulation domain)

## Next Step

Run `/spec-system` with this research as input.
Suggested system name: `DiaSimTime`
Suggested parent application: `Dia`
