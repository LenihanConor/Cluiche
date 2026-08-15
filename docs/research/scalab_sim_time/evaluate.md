# Research: Evaluate — Scalable Simulation Time

**Input:** docs/research/scalab_sim_time/ideate.md

## Scoring Criteria

| Axis | Weight | Description |
|---|---|---|
| Engine Value | 0.25 | Improves Dia module reusability or capability |
| Game Value | 0.20 | Improves CluicheTest as a demo or testbed |
| Implementation Cost | 0.25 | Inverse of effort — 5 = very cheap, 1 = very expensive |
| Risk | 0.15 | Inverse of uncertainty — 5 = well-understood, 1 = highly uncertain |
| Cluiche Fit | 0.15 | Aligns with module structure and PD-001 through PD-007 |

## Scores

| Candidate | Engine (0.25) | Game (0.20) | Cost (0.25) | Risk (0.15) | Fit (0.15) | Total |
|---|---|---|---|---|---|---|
| Foundation (typed bases + context structs) | 5 | 3 | 3 | 4 | 5 | **3.95** |
| SimTimeDomain (named clock hierarchy) | 4 | 4 | 3 | 4 | 4 | **3.75** |
| SimTimeScheduler (timer wheel) | 5 | 5 | 3 | 3 | 4 | **4.05** |
| SimTimeAnalytical (AdvanceTo / lazy eval) | 4 | 3 | 2 | 2 | 3 | **2.85** |
| DiaSimTime (umbrella module) | 5 | 5 | 2 | 3 | 5 | **3.95** |

## Top 3 Candidates

### Rank 1: SimTimeScheduler (score: 4.05)
**Why:** Highest standalone game value of the group. Removes per-frame polling for time-driven behaviour across AI (NPC schedules, HTN retriggers), gameplay (shop open/close, crop timers, quest deadlines), and physics (deferred wake events). Cost becomes proportional to things *happening* rather than things *existing* — the single biggest win from the whitepaper. Timer wheels are well-understood, the implementation risk is low, and it slots directly into the existing EventStreamStore delivery pattern (PD-004 compliant via DiaCore containers). Can ship value independently before DiaSimTime is complete.
**Watch out for:** Correctness when the clock is paused, scaled, or stepped — scheduled events must fire at the right *game* time, not wall-clock time. Requires SimTimeDomain (or at minimum the Foundation TimeServer changes) to be stable first.

### Rank 2: Foundation — typed bases + context structs (score: 3.95, tie)
**Why:** Hard prerequisite for everything else. The TypeServer changes (Pause/Resume/Step/AdvanceTo, sleep stripped) are small. The typed base classes (SimModule / RenderModule / MainModule) enforce the thread contract at compile time — the wrong thing becomes uncompilable rather than just ill-advised, closing a known source of cross-thread state bugs. SimTimeContext unblocks FrameStreamStore.FetchClosestTo() which has been stubbed awaiting a real sim clock. Without this, no other candidate can ship correctly.
**Watch out for:** The module migration is mechanical but wide — every existing SimPU module needs to change its DoUpdate signature. Needs a clean bulk migration pass; doing it incrementally risks a long period of mixed signatures.

### Rank 3: DiaSimTime — umbrella module (score: 3.95, tie)
**Why:** The capstone that makes the architecture coherent. Once built, all AI systems (UtilityAI, HTN, Rules, Blackboard, AIBudget), physics, and animation participate in one budget with fair deferral and deadlines. Per-system SimTimePolicy and SimTimeState mean LOD and dormancy are declared once per system, not reinvented per-frame inside each module's DoUpdate. Directly enables the ArenaTestStage (6 AI systems, 11 tasks) and any future large-population simulation. Scores equal to Foundation because it delivers the most observable value but costs more to build and depends on all the others.
**Watch out for:** Integration risk is real — getting the gate ordering right (sleep → policy → budget), thread-safe ServiceStream publishing, and migrating all existing IAIBudgetedSystem implementations to ISimTimeBudgetedSystem without regressions. Should be the last item built, after Foundation + SimTimeDomain + SimTimeScheduler are stable.

## Recommendation

Build in dependency order, not score order. **SimTimeScheduler** scores highest but cannot run correctly without the **Foundation** clock changes, and both deliver more value once **DiaSimTime** assembles them under a coherent management layer.

The practical build order is:

```
1. Foundation          prerequisite for everything; closes the thread-safety gap
2. SimTimeDomain       named clock tree; enables pause/slow-mo; prerequisite for Scheduler correctness
3. SimTimeScheduler    highest standalone value; ships NPC schedules + AI event triggers immediately
4. DiaSimTime          capstone; assembles 1–3 + SimTimeBudget + SimTimeRegistry + SimTimeState
5. SimTimeAnalytical   v2; only needed at large population scale; highest risk, lowest near-term return
```

**SimTimeAnalytical** is the outlier — it scored 2.85 against 3.75–4.05 for the others. It is architecturally sound but expensive, correctness-sensitive, and only delivers meaningful value at scales Cluiche does not yet reach. It aligns with no PD decision directly (PD-003 was superseded) and introduces the "stored state vs evaluated state" distinction as a new engine concept with no existing analogue. Defer to a later spec once items 1–4 are proven. Note the hook point: `SimTimeState` (kAwake/kSleeping) in DiaSimTime is where analytical advancement would plug in — the architecture is ready for it when the time comes.

The top candidate for spec work is **Foundation**, because it is blocking everything else and its scope is well-defined: TimeServer additions, typed base classes, three context structs, DiaRenderTime and DiaMainTime shells.
