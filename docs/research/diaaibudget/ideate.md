# Research: Ideate — DiaAIBudget (Frame-Budget AI Scheduler)

**Input:** docs/research/diaaibudget/explore.md

---

## Candidates

---

### Candidate 1: Flat Round-Robin Scheduler

**Home module/system:** New module `Dia/DiaAIBudget/`
**Size:** S (≤1 week)
**Description:** The simplest viable design. All registered work items sit in a single flat array. Each frame, the scheduler resumes from where it left off last tick (a "cursor" index), runs items sequentially until the wall-clock budget is exhausted, and stops. Next tick it picks up at the cursor. No priority — every item gets equal CPU share over time.

Registration is a standing subscription: systems call `Register(StringCRC typeId, WorkFn)` once at start; items stay in the queue permanently until `Unregister(id)` is called. The work function is a `void(float dt)` callback. Overflow is a compile-time fixed capacity (e.g. 512 items) with a Debug assert.

**Primary value:** Dead-simple to implement and debug; zero starvation risk; sufficient for a single entity type at small counts (~50 agents).

---

### Candidate 2: Three-Tier Priority Scheduler (recommended baseline)

**Home module/system:** New module `Dia/DiaAIBudget/`
**Size:** S (≤1 week)
**Description:** Three fixed-capacity arrays, one per tier: `Critical` (always runs to completion this frame — hard cap of 16 items to prevent abuse), `Normal` (runs round-robin within budget remainder), and `Background` (runs only if budget has headroom after Normal). Each item is a standing subscription with a `StringCRC` type tag and a `void(float dt)` callback.

Execution order each tick: drain Critical entirely → drain Normal until budget hit (round-robin cursor) → drain Background until budget hit. Starvation prevention on Normal: any item not run for `kMaxDeferFrames` (configurable, default 3) is temporarily promoted to Critical tier for one tick.

The budget is read from `OnConfigure(json)` under key `"budgetUs"` (microseconds, integer). Metrics registered at startup: `ai.budget.used_us` (gauge), `ai.budget.items_run` (counter), `ai.budget.items_deferred` (counter), `ai.budget.critical_count` (gauge).

**Primary value:** Urgent reactions always fire; routine AI spreads fairly; long-horizon planning doesn't starve; all configurable without recompile.

---

### Candidate 3: Batch-Per-System Scheduler

**Home module/system:** New module `Dia/DiaAIBudget/`
**Size:** S (≤1 week)
**Description:** Instead of per-entity callbacks, systems register a *batch* callback: `void(Span<EntityId> entities, float dt)`. The scheduler owns no entity lists — it calls the system's batch function with "all the entities you should process this tick," derived by time-slicing the system's own registered entity list.

Each system registers with `RegisterBatch(StringCRC systemId, Priority, EntityList*, BatchFn)`. The scheduler partitions each system's entity list across N frames based on the budget, calling the batch function with the appropriate slice each tick. Cache-friendlier than per-entity callbacks because a system processes its own data in sequence.

`PathfindingSystem` fits naturally: it already has a queue; the scheduler calls `Update(sliceBudgetMs)` on it as one batch item.

**Primary value:** Better cache behaviour for large agent counts; natural fit for systems that already have per-system update loops.

---

### Candidate 4: Score-Weighted Scheduler

**Home module/system:** New module `Dia/DiaAIBudget/`
**Size:** M (1–3 weeks)
**Description:** Work items are not assigned a fixed tier — they carry a dynamic floating-point priority score, recomputed each frame by a user-supplied `ScoreFn`. The scheduler each tick sorts items by score (descending) and runs the top N until the budget is exhausted. The score function can consider: distance to player (near = urgent), time since last update (old = urgent), entity threat level (high threat = urgent).

Requires a DiaCore-compatible sort (no `std::sort` in the hot path). Score recomputation itself must be cheap or budgeted separately. Expressive but adds complexity: debugging "why did entity 37 not get updated" requires inspecting its score, which changes every frame.

This is closest to what Unreal's `AIPerceptionComponent` does internally — each perception component has a configurable stimuli relevance score that influences update frequency.

**Primary value:** Update frequency naturally tracks game-relevant urgency; entities near combat get more CPU than idle units in the fog.

---

### Candidate 5: Delegating Thin Shell (Minimal Coordinator)

**Home module/system:** New module `Dia/DiaAIBudget/` (very small)
**Size:** S (≤1 week)
**Description:** DiaAIBudget is not a general work queue at all — it is a thin `Module` that holds the single frame budget value and time-slices calls to a fixed, known list of AI system delegates. Each AI system that wants budget-capped execution implements `IAIBudgetedSystem` with one method: `UpdateBudgeted(float budgetMs)`. The scheduler calls them in registration order, passing a shrinking budget slice.

Systems self-manage their internal queues (as PathfindingSystem already does). DiaAIBudget provides only: the shared budget configuration, the call ordering, the per-system elapsed-time measurement, and the metrics. No registration of individual work items — each system is a single registered delegate.

Simplest possible API. The tradeoff is that priority is implicit in registration order — the first registered system gets the most budget. No per-entity starvation tracking.

**Primary value:** Minimal API surface, zero risk of pointer lifetime bugs (no per-entity handles), immediate compatibility with PathfindingSystem's existing `Update(ms)` pattern.

---

### Candidate 6: Component-Driven Scheduler

**Home module/system:** New module `Dia/DiaAIBudget/` + `AIBudgetComponent` (per-entity component)
**Size:** M (1–3 weeks)
**Description:** Each entity that needs AI ticking carries an `AIBudgetComponent` (an `IComponent`) that holds: priority tier, work function reference, last-update frame index, and staleness counter. The scheduler module at `DoUpdate` queries the entity registry for all `AIBudgetComponent` instances, sorts by priority + staleness, and runs them in order until budget exhausted.

Adding an entity to the scheduler is as simple as attaching the component. Removing it is component removal. No explicit Register/Unregister calls — the component IS the registration. Fits perfectly with the Dia component system (PD-003 / IComponent pattern).

Requires a component query API (e.g. `EntityRegistry::Query<AIBudgetComponent>()`) — which either exists or needs to be added. If the entity system doesn't support efficient component queries today, this design is blocked.

**Primary value:** AI scheduling becomes part of the entity composition model — the same mental model as all other Dia entity behaviour.

---

### Candidate 7: Frame-Sliced LOD Scheduler

**Home module/system:** New module `Dia/DiaAIBudget/`
**Size:** S (≤1 week)
**Description:** A hybrid of round-robin and LOD frequency. Each registered item has a `frequencyHz` (how many times per second it should ideally run) alongside its priority. The scheduler computes a "next due frame" for each item and only enqueues it when that frame arrives. Items overdue by more than one period are promoted.

For 60Hz sim: a `frequencyHz=60` item runs every tick; `frequencyHz=10` runs every 6 ticks; `frequencyHz=1` runs once a second. The budget pool only needs to cover items *currently due*, not all registered items. This significantly reduces the burst problem: if 200 entities all tick at 10Hz, only ~33 are due per frame.

Configuration: global budget + per-item frequency. Frequency is a hint not a guarantee — budget can further defer overdue items, but the system tracks and reports when items are running at less than their requested frequency.

**Primary value:** Most entities update infrequently by design, reserving budget for entities that genuinely need per-frame responsiveness (combatants).

---

### Candidate 8: Two-Phase Evaluate-Then-Commit Scheduler

**Home module/system:** New module `Dia/DiaAIBudget/`
**Size:** M (1–3 weeks)
**Description:** Separates AI work into two phases per frame: **Evaluate** (read world state, compute decision — read-only, parallelisable in principle) and **Commit** (write commands, update blackboard, issue orders — write, single-threaded). The scheduler runs Evaluate for as many entities as budget allows, then runs Commit for all entities whose Evaluate completed.

Entities that didn't get an Evaluate this frame reuse their last decision (last commit remains active). This means "no update" results in "continue current behaviour" rather than "freeze" — a much better degradation mode.

Requires work items to split into two callbacks: `EvaluateFn` and `CommitFn`. Adds API complexity. The "continue current behaviour" guarantee requires that all AI-driven actions are idempotent when re-committed (e.g. "move toward target" is safe to re-issue; "fire once" is not — needs one-shot guards).

**Primary value:** AI degrades gracefully under load — agents that didn't get updated simply continue their last decision rather than stopping dead.

---

## Coverage Map

The 8 candidates span the key design axes identified in explore.md:

| Candidate | Priority model | Work unit | Starvation handling | Complexity | Size |
|-----------|---------------|-----------|---------------------|------------|------|
| C1: Round-Robin | None (equal) | Per-entity callback | None (equal share) | Minimal | S |
| C2: Three-Tier | Fixed tiers | Per-entity callback | Aging → promotion | Low | S |
| C3: Batch-Per-System | Tier + system order | Per-system batch | Tier-based | Low | S |
| C4: Score-Weighted | Dynamic score | Per-entity callback | Score includes staleness | Medium | M |
| C5: Delegating Shell | Registration order | Per-system delegate | None | Minimal | S |
| C6: Component-Driven | Tier + staleness | Per-entity component | Staleness counter | Medium | M |
| C7: Frame-Sliced LOD | Frequency + priority | Per-entity callback | Frequency drift tracking | Low | S |
| C8: Two-Phase | Tier | Evaluate/Commit pair | Last-decision fallback | High | M |

**Scope coverage:**
- Simplest buildable: C1 and C5 (~2 days each)
- Best balance of power and simplicity: C2 and C3
- Most future-proof / extensible: C4, C6, C7
- Best degradation model: C8
- PathfindingSystem compatibility: C3 and C5 are natural fits; others require wrapping PathfindingSystem as a single Normal-tier item
