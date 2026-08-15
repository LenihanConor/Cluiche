# Implementation Plan: DiaSimTime

**Spec:** @docs/specs/applications/dia/systems/diasimtime/diasimtime.md
**Status:** Ready — spec `Approved`; no tasks started. The three blocking open questions are resolved (see below); dispatch is unblocked.

> This plan covers the four buildable phases (Foundation → SimTimeDomain → SimTimeScheduler → DiaSimTime umbrella). Phase 5 (SimTimeAnalytical) is a separate follow-on feature spec and is out of scope here.

---

## Spec Review Findings (applied to the spec)

Grounded against the real framework (`Module.h`, `ProcessingUnit.{h,cpp}`, `DiaStreams/*`). Three corrections were found and have been **applied to `diasimtime.md`** — recorded here for the design trail:

1. **The sealed `DoUpdate(float) final {}` in ST-001 was misleading — fixed.** `DoUpdate` is pure-virtual and is called from `Module::FrameTick` (only in `kActive` state, `Module.cpp:248`). An empty sealed body means the module never ticks. The correct pattern (already used by `TestStageModuleBase`) is: seal `DoUpdate(float)` `final` and **forward** to the new typed virtual, pulling the context from the owning PU. The spec's base-class listing and ST-001 now show the forward. See Pattern A.

2. **`ServiceStream<SimTimeContext>` was the wrong primitive — fixed.** `ServiceStream` is a register-once stable handle (`Register`/`Get`). `SimTimeContext` is a per-tick timestamped snapshot — that is exactly what **`FrameStream`** (`StreamWriter<T>::Write(data, timestamp)` / `StreamReader<T>::FetchLatest()` / `FetchClosestTo(t)`) is for. All spec references now use `StreamWriter`/`StreamReader<SimTimeContext>`; ST-007 records the rationale. This also naturally unblocks `FetchClosestTo`. See Pattern D.

3. **Gating is at the registered-system level, not the module level — clarified.** The per-tick gate sequence (sleep → policy → budget) applies to `ISimTimeBudgetedSystem` instances registered *inside* `DiaSimTimeModule` — it does **not** gate sibling `SimModule`s (the PU ticks those unconditionally via `FrameTick`). This is a direct generalization of how `AIBudgetModule::DoUpdate` drives `AIBudgetScheduler` today. The gate-sequence section and ST-005 now state this explicitly, so no one makes `ProcessingUnit` consult the registry (which would invert the DiaApplicationFlow → DiaSimTime dependency). See Pattern C.

---

## Implementation Patterns

### Pattern A — Typed base class context injection (Phase 1)

The proven precedent is `TestStageModuleBase` (`Cluiche/CluicheTest/.../TestStageModuleBase.h:24`): it seals `DoUpdate(float) final` and exposes `virtual void OnUpdate(float) = 0`. The three typed bases follow the same shape, but forward a **context struct pulled from the owning PU**:

```cpp
// Dia/DiaApplicationFlow/SimModule.h
class SimModule : public Module {
public:
    explicit SimModule(const Core::StringCRC& id) : Module(id) {}
protected:
    virtual void DoUpdate(const Dia::SimTime::SimTimeContext& ctx) = 0;
private:
    // Sealed: framework tick entry. Pulls the context the PU cached this Update()
    // and forwards. NOT empty.
    void DoUpdate(float /*dt*/) final {
        DoUpdate(GetProcessingUnit()->GetSimTimeContext());
    }
};
```

`RenderModule` → `GetRenderTimeContext()`, `MainModule` → `GetMainTimeContext()`. No change to `FrameTick`'s signature; no context pointer on the `Module` base. The base stays context-agnostic; each typed subclass knows which accessor to call.

### Pattern B — ProcessingUnit owns the clock and builds the context (Phase 1)

`ProcessingUnit` already knows its role (`mAffinity`, set from instance ID, `ProcessingUnit.cpp:31-35`). Extend `Update(float dt)` to build and cache the correct context **before** the forward/reverse passes. Per Q1, the SimPU owns the **world `SimTimeDomain`** (from DiaCore) as its clock — not a bare `TimeServer` — so Phase 1's clock and Phase 2's world domain are the same object:

```cpp
void ProcessingUnit::Update(float dt) {
    switch (mAffinity) {
        case PUAffinity::kSim:
            mWorldDomain.Tick();                    // world SimTimeDomain owned by the PU
            mSimTimeContext = { mWorldDomain.Now(), mWorldDomain.Step(),
                                mWorldDomain.GetTick(), mWorldDomain.GetScale(),
                                mWorldDomain.IsPaused() };
            break;
        case PUAffinity::kRender:
            mRenderTimeContext = { dt, mLatestSimTimeSnapshot, ++mRenderFrame };
            break;
        case PUAffinity::kMain:
        default:
            mMainTimeContext = { dt };
            break;
    }
    // ... existing forward pass (FrameTick) / reverse pass unchanged ...
}
```

Accessors `GetSimTimeContext() / GetRenderTimeContext() / GetMainTimeContext()` return the cached value. **Clock ownership = the SimPU owns the world `SimTimeDomain`.** Phase 2's `SimTimeDomainRegistry` roots at that same world domain (no separate reconciliation); Phase 4's `DiaSimTimeModule` layers scheduler/budget/registry on top and drives pause/scale by reaching the PU's world domain via `GetProcessingUnit()`. (This supersedes the standalone `TimeServerModule` in CluicheGameBaseline — mark it for removal in Task 1.7.)

### Pattern C — System gating generalizes AIBudgetModule (Phase 4)

`DiaSimTimeModule::DoUpdate(ctx)` runs the gate loop over registered `ISimTimeBudgetedSystem*` — exactly how `AIBudgetModule::DoUpdate` calls `AIBudgetScheduler::Update` today, plus two gates in front:

```cpp
void DiaSimTimeModule::DoUpdate(const SimTimeContext& ctx) {
    mDomainRegistry.TickAll();              // (clock already ticked by PU; sub-domains here)
    mScheduler.Tick(ctx.gameTime);          // fire due events into EventStreamStore
    mBudget.AllocateTick(ctx);              // partition ms by priority tier
    for (auto& entry : mRegistry) {         // registered systems, not sibling modules
        if (entry.state == SimTimeState::kSleeping)  continue;   // sleep gate
        if (!entry.policy.DueThisTick(ctx))          continue;   // LOD gate
        mBudget.RunIfCapacity(entry);                            // budget gate + deadline promote
    }
    mSimTimeWriter.Write(ctx, ctx.gameTime);  // FrameStream publish (Pattern D)
}
```

### Pattern D — Sim→Render time via FrameStream (Phase 1 + 4)

Use the `FrameStream` family (`Dia/DiaStreams/`), namespace `Dia::ApplicationFlow`:
- Producer (`DiaSimTimeModule` on SimPU): `StreamWriter<SimTimeContext> mSimTimeWriter{this, "SimTime"};` — `Connect()` in `OnConnectStreams()`, `Write(ctx, ctx.gameTime)` each tick.
- Consumer (`DiaRenderTime` on RenderPU): `StreamReader<SimTimeContext> mSimTimeReader{this, "SimTime"};` — `FetchLatest()` for the current snapshot, `FetchClosestTo(t)` once ring-buffer interpolation lands.

### Pattern E — Extend, don't rebuild, AIBudgetScheduler (Phase 4)

`SimTimeBudget` wraps/extends the existing `Dia::AIBudget::AIBudgetScheduler` (`kMaxSystems=16`, `DynamicArrayC`, `steady_clock`). Keep `IAIBudgetedSystem` as a `using`-alias of `ISimTimeBudgetedSystem` during migration (AB-007's wall-clock budget rule carries over unchanged). Priority tiers replace registration-order priority (AB-003); deadline promotion is the new fairness mechanism (ST-008).

### Conventions (all phases)

- Namespace `Dia::SimTime::` for all new types; typed base classes stay in `Dia::ApplicationFlow::`.
- No STL in public headers (PD-004): `DynamicArrayC`, DiaCore priority structures; `std::chrono` allowed internally only.
- StringCRC for system IDs, domain names, event types, log channel, metric keys (PD-001).
- Test helpers live in `Dia/DiaSimTime/Testing/`, not GoogleTests (project convention).
- Use `dia docs vcxproj-add`, `dia docs registry`, `dia scaffold module` for mechanical edits.
- TDD: RED (quoted failing test) → GREEN → REFACTOR for every new public API.

---

## Tasks

### Phase 1 — Foundation

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1.1 | Extend `Core::TimeServer`: add `Pause()`/`Resume()`/`IsPaused()`/`Step(TimeRelative)`/`AdvanceTo(TimeAbsolute)`; **strip the `sleep_for` from `Tick()`** (rate-limiting stays in ProcessingUnit). | `TimeServerTests` (DiaCore): pause freezes time, resume restores prior scale, Step advances one step while paused, AdvanceTo jumps forward and is a no-op backward | Not Started | sonnet | Breaking change to `Tick()` semantics — audit existing `TimeServer` callers (incl. `TimeServerModule`) |
| 1.2 | Add context structs `SimTimeContext` / `RenderTimeContext` / `MainTimeContext` (header-only) in **DiaCore/SimTime** (Q1 — must sit ≤ DiaApplicationFlow to avoid a cycle). Namespace `Dia::SimTime`. | compile-only + `SimTimeContextTests` field defaults | Not Started | sonnet | `gameDt` is `TimeRelative` (int64), not float (ST-004) |
| 1.3 | Add typed base classes `SimModule` / `RenderModule` / `MainModule` in DiaApplicationFlow (Pattern A). Seal `DoUpdate(float) final`, forward to typed virtual via PU accessor. | `TestPUTypedModules` (GoogleTests/ApplicationFlow): a `SimModule` receives populated `SimTimeContext`; `RenderModule` cannot be constructed against sim time | Not Started | opus | Highest-judgment framework task; mirrors `TestStageModuleBase` precedent |
| 1.4 | `ProcessingUnit`: own the world `SimTimeDomain` for `kSim` (Q1 — not a bare `TimeServer`), build+cache per-affinity context in `Update()`, expose `GetSimTimeContext()/GetRenderTimeContext()/GetMainTimeContext()` (Pattern B). | `TestProcessingUnitContext`: Sim PU advances game time each Update; Main PU exposes wall-clock dt | Not Started | opus | Depends on 1.1, 1.2, 1.3. Uses `SimTimeDomain` from 2.1 — or land a minimal world-domain-only slice of 2.1 first |
| 1.5a | Migrate 6 Dia-engine + affinity-undeclared production modules to typed bases. **Assign SimPU/RenderPU/MainPU for the 7 currently-`kAny` modules** (EntityModule, Physics2DModule, editor modules, engine libs) — this is a per-module data call needing owner confirmation (Q3 sub-decision). | existing suites compile + pass | Not Started | sonnet | Blocked on owner confirming the 7 affinity assignments before dispatch |
| 1.5b | Migrate 28 CluicheGameBaseline modules to typed bases (affinity already declared via `kAllowedPUs`). | baseline app launches; smoke stage runs | Not Started | sonnet | Parallelizable with 1.5c after 1.3/1.4 land |
| 1.5c | Migrate 7 CluicheEditor + 7 CluicheTest standalone modules + `TestStageModuleBase` (retype its `OnUpdate` path). ~22 leaf stages inherit — no per-stage DoUpdate change. | `dia run cluichetest`; editor launches | Not Started | sonnet | `TestStageModuleBase` is the leverage point — one edit covers 22 stages |
| 1.6 | Migrate ~25 GoogleTest module doubles to typed bases (`void DoUpdate(float) override {}` → typed). | `dia run googletest` clean | Not Started | haiku | Mechanical; do in same commit window as 1.3 to avoid long red period (Open Q3) |
| 1.7 | Add `DiaMainTime` (`MainModule`) + `DiaRenderTime` (`RenderModule`): Render reads `StreamReader<SimTimeContext>`, publishes `RenderTimeContext` (Pattern D). Remove redundant `TimeServerModule`. | `DiaRenderTimeTests`: render frame carries latest sim snapshot | Not Started | sonnet | Depends on 1.4 |
| 1.8 | Implement `FrameStreamStore::FetchClosestTo` ring-buffer interpolation (currently stubbed → returns latest). | `FrameStreamStoreTests`: closest-to returns bracketing sample for a given timestamp | Not Started | sonnet | Unblocked by real sim time; nice-to-have — can defer if timeboxed |

### Phase 2 — SimTimeDomain

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 2.1 | `SimTimeDomain` + `SimTimeDomainRegistry` in **DiaCore/SimTime** (Q1): named tree, child scale = parent scale × local scale, pause/resume/step/advance per subtree, StringCRC keys, `kWorldId` always present. Registry roots at the SimPU's world domain (Pattern B). | `SimTimeDomainTests`: child inherits parent pause; independent scale composes; TickAll advances all | Not Started | opus | Hierarchy math + lifecycle. A minimal world-domain-only slice may need to land inside the Phase-1 cutover (see 1.4) |
| 2.2 | Wire `SimTimeDomainRegistry` to the PU's world domain and expose it through `DiaSimTimeModule`. **Reduced to wiring** by Q1 — the PU clock already *is* the world domain, so there is no ownership reconciliation. | `TestSimClockOwnership`: pausing `world` domain freezes `SimTimeContext.gameTime` | Not Started | sonnet | Downgraded opus→sonnet: Q1 dissolved the design seam |

### Phase 3 — SimTimeScheduler

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 3.1 | `SimTimeScheduler` data structure: bucketed timer wheel + min-heap overflow, `DynamicArrayC`-backed, `ScheduleHandle` generation/validation. | `SimTimeSchedulerTests`: insert/fire ordering, cancel, reschedule, handle reuse safety | Not Started | opus | Core data structure; no STL (PD-004) |
| 3.2 | Scheduler API + `Tick(currentTime)` firing into `EventStreamStore<SimTimeSchedulerFire>` (Q2 — payload `{ StringCRC eventType; StringCRC targetSystemId; }`, consumers filter; fan-out, no callbacks); `ScheduleAt/After/Recurring`; recurring re-arm; pause/scale-correct (ticks against `world` domain, not wall clock). | `SimTimeSchedulerTests`: events fire at game time not wall time; recurring re-arms; paused clock defers firing; consumer filters by targetSystemId | Not Started | sonnet | Depends on 2.1, 3.1. Q2 resolved: fan-out + filter |

### Phase 4 — DiaSimTime umbrella

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 4.1 | `ISimTimeBudgetedSystem` + `SimTimePriority`; add `IAIBudgetedSystem` as deprecated `using`-alias. | compile + `ISimTimeBudgetedSystemTests` default priority | Not Started | sonnet | Preserve AB-005 (`UpdateBudgeted(0)` is a valid no-op) |
| 4.2 | `SimTimeBudget`: extend `AIBudgetScheduler` with 4 priority tiers + deadline/carry-forward promotion (ST-008). Metrics `simtime.budget.*`. | `SimTimeBudgetTests`: low-priority system starved then promoted at deadline; `used_ms`/`deferred_count`/`stale_ms_max` accurate | Not Started | opus | Extend not rebuild (Pattern E) |
| 4.3 | `SimTimePolicy` / `SimTimeState` / `SimTimeTier` + `SimTimeRegistry`: per-system registration, `DueThisTick` tier→Hz mapping, sleep/wake, wake-condition registration (time→scheduler, message→EventStream). | `SimTimeRegistryTests`: tier throttles call rate; sleeping system skipped; `RegisterWakeOnTime` wakes at T | Not Started | opus | Gate sequence sleep→policy→budget |
| 4.4 | `DiaSimTimeModule` (`SimModule`): assemble domain registry + scheduler + budget + registry; per-tick gate loop (Pattern C); publish `StreamWriter<SimTimeContext>`; register all `simtime.*` metrics; `OnConfigure` parses config JSON (worldDomainHz, budget tiers, tier_hz). | `DiaSimTimeModuleTests`: full tick sequence; registered system gated correctly; SimTimeContext published | Not Started | opus | Capstone; depends on 4.1–4.3, Phase 2, Phase 3 |
| 4.5 | Migrate `DiaUtilityAI`, `DiaHTN` async paths + `DiaPathfinding` adapter from `IAIBudgetedSystem` to `ISimTimeBudgetedSystem`; assign priorities. | those systems' existing suites pass; register with `DiaSimTimeModule` | Not Started | sonnet | Alias makes this low-risk; verify no behaviour change |

### Cross-cutting / closeout

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| C.1 | Create `DiaSimTime.vcxproj` + filters via `dia docs vcxproj-add`; register in `Cluiche.sln` (`dia check sln-sync`); write `dia.diasimtime.architecture.module.md`; `dia docs registry`. | `dia check deps`; `dia check sln-sync` clean | Not Started | haiku | Do early enough that new files have a home (after 1.2) |
| C.2 | `dia check deps` + `dia check spec-sync` green; observation-opportunity scan on touched files; update feature statuses Draft→Done. | both checks pass | Not Started | sonnet | Verify gate |
| C.3 | `dia docs spec-done diasimtime.md` (system spec Done only after buildable features complete; SimTimeAnalytical remains Planned). | — | Not Started | haiku | Final |

---

## Dependency Order & Parallelism

```
1.1 ─┐
1.2 ─┼─► 1.3 ─► 1.4 ─┬─► 1.5a/1.5b/1.5c (parallel) ─► 1.6
     │               └─► 1.7 ─► 1.8
     C.1 (after 1.2)
                         Phase 1 done ─► 2.1 ─► 2.2 ─► 3.1 ─► 3.2
                                                              │
                    4.1 ─► 4.2 ─┐                             │
                    4.3 ────────┼─► 4.4 ◄──────────────(needs 2.2 + 3.2)
                                │
                                └─► 4.5 (after 4.4)
                                          └─► C.2 ─► C.3
```

- **Hard cutover window:** 1.3 + 1.4 + 1.5* + 1.6 should land together (or behind a short-lived branch). `DoUpdate` is pure-virtual, so a partial migration will not compile (Open Q3). `TestStageModuleBase` absorbs 22 stages in one edit, which shrinks the window considerably.
- **Parallelizable:** 1.5a/b/c after 1.3+1.4; 4.1/4.2 vs 4.3 before 4.4.

## Open Questions

### Resolved (2026-08-14)

1. **SimTimeDomain / context-struct home → DiaCore.** *Decisive:* the typed base classes (DiaApplicationFlow) name `SimTimeContext`, and the DiaSimTime module depends on DiaApplicationFlow — so the structs cannot live in the module without a cycle. Both the context structs and `SimTimeDomain`/`SimTimeDomainRegistry` go in **DiaCore/SimTime** (namespace `Dia::SimTime`), beside `TimeServer`. Bonus: the SimPU owns the world `SimTimeDomain` as its clock, dissolving the old Task 2.2 reconciliation seam. Scheduler/budget/registry/policy/state stay in the module. *(Affects 1.2, 1.4, 2.1, 2.2.)*
2. **Scheduler event routing → EventStreamStore fan-out, no callbacks.** Scheduler `Send`s `Event<SimTimeSchedulerFire>` with payload `{ StringCRC eventType; StringCRC targetSystemId; }`; subscribers filter. Grounded: `EventStreamStore<T>` fans a copy to all ≤8 readers with no built-in target routing — filtering is the native pattern. Callbacks rejected (pointer-lifetime + coupling). Coarseness (every reader copies every event) is acceptable at scale; revisit with a dedicated per-target stream only if measured. *(Affects 3.2.)*
3. **DoUpdate migration → hard cutover, single branch, compiler-driven.** Coexistence would keep `Module::DoUpdate(float)` overridable and reopen the exact hole ST-001 closes; and since the typed bases seal `final`, no "half-migrated compiles" state exists anyway. Branch order: 1.1+1.2 → 1.3+1.4 → chase errors across 1.5a/b/c+1.6 → merge atomically. Run when the tree is quiet. *(Affects 1.3–1.6.)*

### Still open (do not block dispatch)

4. **IAIBudgetedSystem alias retirement timeline** (affects 4.5 cleanup) — retire once the three consumers are migrated, or keep for game/third-party code?
5. **SimTimeAnalytical save/load contract** (Phase 5 — not this plan) — dependency on `DiaSaveGame` vs each system serialising its own `LastEvaluated`.

### Pre-dispatch checklist item (not a design question)

- **Task 1.5a needs the 7 affinity assignments confirmed** by the module owner (EntityModule, Physics2DModule, editor modules, engine libs → Sim/Render/Main) before that task is dispatched.
