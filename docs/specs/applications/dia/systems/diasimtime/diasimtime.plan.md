# Implementation Plan: DiaSimTime

**Spec:** @docs/specs/applications/dia/systems/diasimtime/diasimtime.md
**Status:** In Progress — **Phase 1 (Foundation) Done**, **Phase 2 (SimTimeDomain) Done**, **Phase 3 (SimTimeScheduler) Done** (2026-09-02). Phase 1: Tasks 1.0-1.8 + C.1 (1386 tests + `cluichetest` clean); hard-cutover window (1.3-1.6) landed on a short-lived branch and fast-forward-merged cleanly. Phase 2: `SimTimeDomainRegistry` (2.1) bound to the PU's world domain (2.2). Phase 3: `SimTimeScheduler` (3.1, bucketed timer wheel + min-heap overflow) wired to a real `EventStreamStore<SimTimeSchedulerFire>` (3.2); `EventStreamStore` reader-cap bug fixed (3.3). Phase 4 (DiaSimTime umbrella) is Not Started — next entry points are 4.1/4.3 (independent) → 4.2/4.4. Three blocking open questions were resolved before implementation began; see "Spec Review Findings" below for the full design trail.

> This plan covers the four buildable phases (Foundation → SimTimeDomain → SimTimeScheduler → DiaSimTime umbrella). Phase 5 (SimTimeAnalytical) is a separate follow-on feature spec and is out of scope here.

---

## Spec Review Findings (applied to the spec)

Grounded against the real framework (`Module.h`, `ProcessingUnit.{h,cpp}`, `DiaStreams/*`). Three corrections were found and have been **applied to `diasimtime.md`** — recorded here for the design trail:

1. **The sealed `DoUpdate(float) final {}` in ST-001 was misleading — fixed.** `DoUpdate` is pure-virtual and is called from `Module::FrameTick` (only in `kActive` state, `Module.cpp:248`). An empty sealed body means the module never ticks. The correct pattern (already used by `TestStageModuleBase`) is: seal `DoUpdate(float)` `final` and **forward** to the new typed virtual, pulling the context from the owning PU. The spec's base-class listing and ST-001 now show the forward. See Pattern A.

2. **`ServiceStream<SimTimeContext>` was the wrong primitive — fixed.** `ServiceStream` is a register-once stable handle (`Register`/`Get`). `SimTimeContext` is a per-tick timestamped snapshot — that is exactly what **`FrameStream`** (`StreamWriter<T>::Write(data, timestamp)` / `StreamReader<T>::FetchLatest()` / `FetchClosestTo(t)`) is for. All spec references now use `StreamWriter`/`StreamReader<SimTimeContext>`; ST-007 records the rationale. This also naturally unblocks `FetchClosestTo`. See Pattern D.

3. **Gating is at the registered-system level, not the module level — clarified.** The per-tick gate sequence (sleep → policy → budget) applies to `ISimTimeBudgetedSystem` instances registered *inside* `DiaSimTimeModule` — it does **not** gate sibling `SimModule`s (the PU ticks those unconditionally via `FrameTick`). This is a direct generalization of how `AIBudgetModule::DoUpdate` drives `AIBudgetScheduler` today. The gate-sequence section and ST-005 now state this explicitly, so no one makes `ProcessingUnit` consult the registry (which would invert the DiaApplicationFlow → DiaSimTime dependency). See Pattern C.

A skeptical review pass (2026-08-21) against the real framework (`Module.h`, `ProcessingUnit.{h,cpp}`) found six further issues, resolved as follows and applied to `diasimtime.md`:

4. **ST-001's "compile error" claim was unsupported — fixed.** Modules attach to PUs via manifest-driven `instanceId` strings (`ProcessingUnit::AddModule` takes a type-erased `UniquePtr<Module>`); there is no C++ type relationship for the compiler to check. The real placement check is `TypeRegistry::GetAllowedPUs` + `DIA_ASSERT`, and it's `#ifdef DIA_DEBUG`-only (`ProcessingUnit.cpp:83-101`) — Release builds have no enforcement today. ST-001 is reworded to claim what the typed bases actually deliver (sealed no-op-proof tick entry, typed per-role context), and the Debug-only assert is promoted to fire in Release (new Task 1.0).
5. **World-clock double-tick — fixed.** Pattern B ticks the world `SimTimeDomain` directly on the PU; the gate sequence's step 1 (`SimTimeDomainRegistry::TickAll()`) runs moments later inside the same forward pass. `TickAll()`'s contract is now explicit: it advances only non-world domains; `kWorldId` is externally clocked by the owning ProcessingUnit and is a no-op inside `TickAll()`. See Pattern C.
6. **RenderTimeContext ownership was contradictory — fixed.** Pattern B computed `RenderTimeContext` directly on the PU from an unexplained `mLatestSimTimeSnapshot`; Pattern D said `DiaRenderTime` owns the FrameStream read. Resolved: `DiaRenderTime` is the sole reader/producer — it calls a new framework-internal `ProcessingUnit::SetRenderTimeContext()` after `FetchLatest()`. Sibling RenderModules read whatever was pushed last tick — one-tick-stale by design, no manifest-order dependency. See Pattern B/D, Task 1.7.
7. **Task 1.4 ⟷ 2.1 circular dependency — fixed.** 1.4 needed `SimTimeDomain` from 2.1, but the dependency diagram gated 2.1 behind all of Phase 1. The bare `SimTimeDomain` class moves into 1.4's own scope; Task 2.1 is now `SimTimeDomainRegistry` only, wrapping the domain 1.4 already created. This also lets 2.1/2.2 run parallel to 1.5*/1.6/1.7/1.8 instead of serially after Phase 1 — see the updated dependency diagram.
8. **Budget gating vs. the "deterministic clock" pitch — documented, not fixed (not fixable without a much bigger feature).** `SimTimeBudget` extends `steady_clock`-based `AIBudgetScheduler`, so which systems get skipped/deferred each tick depends on real CPU time, not game time. New decision ST-010 states this boundary explicitly; the Purpose section's determinism claim is narrowed to the clock (gameTime/gameDt/tick), not system-execution ordering.
9. **Hard-cutover blast radius — accepted as-is (owner confirmed the scope is intentional); safety net added.** Given finding 4, a bad affinity call during the cutover would previously fail silently in Debug and not at all in Release. New Task 1.0 (Release-mode assert) and an explicit merge gate (below) are the mitigation — no reduction in cutover scope.
10. **No fixed-timestep accumulator — fixed (owner explicitly wants all gameplay running against this clock).** Grounded against `TimeServer.cpp:106`: today `Tick()` advances by exactly one fixed step per call and self-paces with a blocking `sleep_for`. ST-002 removes that sleep; without a replacement, one `ProcessingUnit::Update()` call would still equal exactly one fixed step regardless of real elapsed time — under load, game time silently drifts behind wall-clock with no correction. New decision ST-011: `ProcessingUnit` owns a fixed-timestep accumulator for `kSim` only, draining banked real time in whole `1/GetFrequencyHz()` steps and running the SimPU module forward-pass once per drained step (0, 1, or several per real call), capped at `maxCatchUpTicksPerFrame` to avoid a spiral of death. See Pattern B (rewritten) and Task 1.4 (expanded scope).

A full blast-radius sweep (2026-08-21c, six parallel investigations grounded in `Module.cpp`, `DiaStreams`, `DiaAIBudget`, `DiaSaveGame`, and a fresh repo-wide census) found eleven further items, resolved as follows:

11. **`kAny`-PU placement enforcement is structurally absent, not just Debug-gated — fixed.** `ProcessingUnit::AddModule`'s check skips entirely whenever the *PU itself* is `kAny`-affinity (`ProcessingUnit.cpp:86-88`), and any custom-named PU (e.g. CluicheEditor's `EditorPU`) defaults to `kAny`. Task 1.0's scope now also validates `kAny` PUs against a module's declared `kAllowedPUs`, closing this for good, not just for the three canonical PU names.
12. **`AIBudgetScheduler`'s 16-slot cap becomes a cross-domain cliff under absorption — fixed via ST-012.** `UtilityEvalWorkItem`/`HTNPlanWorkItem` are one-shot async objects dynamically registered per call, not steady-state systems; the real ceiling was "concurrent in-flight evaluations," which scales with population. New decision ST-012: transient one-shot work gets a separate lightweight completion path, never a `SimTimeRegistry` slot. Applied to Task 4.5.
13. **Deliberately-`kAny` modules (`ObservationModule`, `ProfilerModule`) have no typed-base home and would silently lose future re-placement flexibility — fixed via ST-013.** They're exempted from the typed-base migration entirely; they keep their existing direct `Module::DoUpdate(float)` override. Removed from 1.5b's migration set.
14. **`UtilitySetComponent.eval_period_ticks` reproduces the exact "silent when Hz changes" bug DiaSimTime claims to fix, one layer down — documented via ST-014, not fixed (would require a UtilitySetComponent redesign, out of this plan's scope).**
15. **`EventStreamStore` readers hard-capped at 8 with silent failure past that — fixed via new Task 3.3** (DiaStreams gets an optional `Connect()` capacity parameter; used for `SimTimeSchedulerFire`).
16. **Migration bucket counts were never derived live and drift, plus a real double-bucketing bug — fixed.** Fresh census: Dia-engine is 7 files not 6, CluicheEditor is 8 not 7, CluicheTest is 7 not the implied 8. `EntityModule.h`/`Physics2DModule.h` were counted in 1.5b's "affinity already declared" bucket but declare no `kAllowedPUs` at all — moved to 1.5a. CluicheEditor's 8 modules have no genuine Sim/Render/Main ambiguity (single-PU tool loop) — pre-decided all `MainModule`, pulled out of 1.5a's owner-confirmation gate into 1.5c.
17. **`TimeServerModule` is a live, undocumented global 30Hz throttle for the whole SimPU; the original 1.1→1.7 sequencing left a real behavior-change window — fixed.** Its removal is pulled forward into Task 1.1 itself, landing in the same change as the sleep removal it depends on.
18. **Double fixed-step accumulator confirmed in `Physics2DModule`/`PhysicsWorld` and `DiaSoftBody2D`/`SoftBodyWorld` — resolved conservatively.** Kept as-is (no physics-feel/tuning change) on migration; their own catch-up-drop counters get wired into the `simtime.accumulator.dropped_ticks` metric convention so a physics-level drop isn't invisible next to the PU-level one. Applied to Task 1.5a's notes.
19. **`BehaviourTreeSystem` implements `IAIBudgetedSystem` and was missing from the migration list — fixed**, added to Task 4.5.
20. **Task 1.8 was under-scoped — fixed.** `FrameStreamStore` is a 2-slot double buffer today, not a ring buffer; `FetchClosestTo` interpolation needs a storage rewrite first, not a stub fill-in. Re-scoped and model bumped sonnet→opus.
21. **`TestStageModuleBase`'s migration is a full override rewrite, not a rename — clarified in Task 1.5c's description** (it already has its own sealed `DoUpdate(float) final`, which is illegal to re-seal via `SimModule`; needs deleting and replacing with `DoUpdate(const SimTimeContext&)`). Still contained to one file — the "22 leaf stages unaffected" conclusion holds.
22. **SimPU module startup polling is now gated behind the accumulator** (a `kStarting` module isn't polled on a real `Update()` call that drains 0 steps) — low severity (bounded by one fixed step), added as a Task 1.4 test case rather than a design change.
23. **`Cluiche::AppFlow::RenderModule` naming collision with the new `Dia::ApplicationFlow::RenderModule` typed base — flagged in Task 1.5b's notes**, rename recommended (e.g. `SceneRenderModule`) before migrating that file.

Two items from this sweep needed the module owner's call, not an engineering default — both now resolved by the owner (2026-08-21):
- **Save/load contract for `SimTimeScheduler`/`SimTimeState` (Open Q5) — owner decided: build it now.** Grounded investigation found this was a real architectural gap, not a small addition hiding behind "deferred to Phase 5": `DiaSaveGame` had zero production consumers (no prior art to copy), `SaveContext`/`LoadContext` had no `int64_t` overload, `SaveRegistry` had no ordering primitive, and `ScheduleHandle` identity across a save boundary was undefined. Rather than deferring with the risk stated (the initial recommendation), the owner chose to build minimal save/load support into this plan now — see ST-015/ST-016 and Tasks 4.6/4.7.
- **The genuinely-ambiguous engine-lib affinity assignments (Task 1.5a)** — narrower now than originally scoped (CluicheEditor and deliberately-`kAny` modules removed from this gate), but still a per-module data call only the owner can make. Owner acknowledged; confirmation still pending before 1.5a dispatches.

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

### Pattern B — ProcessingUnit owns the clock, the accumulator, and builds the context (Phase 1)

`ProcessingUnit` already knows its role (`mAffinity`, set from instance ID, `ProcessingUnit.cpp:31-35`). Per Q1, the SimPU owns the **world `SimTimeDomain`** (from DiaCore) as its clock — not a bare `TimeServer` — so Phase 1's clock and Phase 2's world domain are the same object.

**Fixed-timestep accumulator (ST-011).** Today's real `ProcessingUnit::Update(float dt)` (`ProcessingUnit.cpp:157`) runs the module forward pass exactly once per call, with `dt` = measured real elapsed time since the last call (from the outer `operator()` loop's best-effort `sleep_for` pacer, `ProcessingUnit.cpp:227-269` — which is inherently imprecise). For `kSim`, that single pass is extracted into a helper and the forward pass is now looped 0–N times per real call, draining a real-time accumulator in fixed steps, so every `SimModule::DoUpdate` always sees a constant-size `gameDt` regardless of the outer loop's jitter:

```cpp
void ProcessingUnit::Update(float dt) {
    // Task 34/35 timing measurement (unchanged) wraps this whole function.

    switch (mAffinity) {
        case PUAffinity::kSim:
        {
            if (mWorldDomain.IsPaused()) {
                mSimAccumulatorSec = 0.0f;   // don't bank time while paused — resuming
            } else {                        // must not trigger a catch-up burst
                mSimAccumulatorSec += dt;
            }

            const float fixedStepSec = 1.0f / GetFrequencyHz();  // SimPU's own manifest Hz —
                                                                  // no separate worldDomainHz knob
            unsigned int ticksThisFrame = 0;
            while (mSimAccumulatorSec >= fixedStepSec &&
                   ticksThisFrame < mMaxCatchUpTicksPerFrame)
            {
                mWorldDomain.Tick();             // advances by fixedStep * timeScale
                mSimTimeContext = { mWorldDomain.Now(), mWorldDomain.Step(),
                                     mWorldDomain.GetTick(), mWorldDomain.GetScale(),
                                     mWorldDomain.IsPaused() };
                RunForwardPass(fixedStepSec);    // extracted: the existing kStarting/kActive
                                                  // FrameTick loop over mModules[]
                mSimAccumulatorSec -= fixedStepSec;
                ++ticksThisFrame;
            }
            if (mSimAccumulatorSec >= fixedStepSec) {   // still behind after the cap: drop it
                DIA_LOG_WARNING("pu", "pu.sim_catchup_dropped id=%s backlog_sec=%.3f",
                                 mInstanceId.AsChar(), mSimAccumulatorSec);
                mMetricDroppedTicks->Increment();        // simtime.accumulator.dropped_ticks
                mSimAccumulatorSec = 0.0f;
            }
            break;   // note: forward pass already ran above, 0..N times — do not run it again below
        }
        case PUAffinity::kRender:
            // mRenderTimeContext is NOT computed here (Issue 3, fixed). DiaRenderTime
            // pushes it via SetRenderTimeContext() during its own DoUpdate, in the
            // single forward pass below — sibling RenderModules read whatever was
            // pushed last tick.
            RunForwardPass(dt);
            break;
        case PUAffinity::kMain:
        default:
            mMainTimeContext = { dt };
            RunForwardPass(dt);
            break;
    }

    RunReversePass(dt);   // kStopping modules: always exactly once per real Update() call,
                           // never gated by the sim accumulator — shutdown sequencing must
                           // not stall just because no fixed step drained this call.

    // ... existing mPostTickFn / Task 34 timing / Task 35 over-budget warning: unchanged,
    // run exactly once per real Update() call regardless of ticksThisFrame ...
}

// Framework-internal — not part of the public Module API. Called by DiaRenderTime
// after it reads FetchLatest() from the sim-time FrameStream (Pattern D, Task 1.7).
void ProcessingUnit::SetRenderTimeContext(const Dia::SimTime::RenderTimeContext& ctx) {
    mRenderTimeContext = ctx;
}
```

`RunForwardPass(float deltaTime)` / `RunReversePass(float deltaTime)` are the existing two loops in `ProcessingUnit::Update` (`ProcessingUnit.cpp:166-185`), extracted unchanged into private helpers so `kSim` can call the forward one multiple times per real call while every other affinity still calls it exactly once. Passing `fixedStepSec` (not the real per-call `dt`) into each sub-tick's `RunForwardPass` is correct: `Module::FrameTick`'s lifecycle-timeout bookkeeping (`startTimeoutMs`/`stopTimeoutMs`) sees the sum of `fixedStepSec` across however many sub-ticks ran this call, which equals the real elapsed time actually consumed from the accumulator.

`timeScale` continues to affect **game time advanced per step** (`gameDt = fixedStep * timeScale`), not step frequency — the accumulator always paces against unscaled real time, unchanged from `TimeServer`'s existing model. Manual `SimTimeDomain::Step()` (editor/debug single-step) bypasses the accumulator entirely — it's an explicit, on-demand call, not part of this loop.

Accessors `GetSimTimeContext() / GetRenderTimeContext() / GetMainTimeContext()` return the cached value. **Clock ownership = the SimPU owns the world `SimTimeDomain`.** The bare `SimTimeDomain` class (Now/Step/SetScale/Pause/Resume/Step/AdvanceTo/GetScale/IsPaused/GetId/Tick) is built here in Task 1.4, not in Phase 2 (Issue 4) — Phase 2's `SimTimeDomainRegistry` (2.1) wraps that same instance afterward (no separate reconciliation); Phase 4's `DiaSimTimeModule` layers scheduler/budget/registry on top and drives pause/scale by reaching the PU's world domain via `GetProcessingUnit()`. (This supersedes the standalone `TimeServerModule` in CluicheGameBaseline — mark it for removal in Task 1.7.)

### Pattern C — System gating generalizes AIBudgetModule (Phase 4)

`DiaSimTimeModule::DoUpdate(ctx)` runs the gate loop over registered `ISimTimeBudgetedSystem*` — exactly how `AIBudgetModule::DoUpdate` calls `AIBudgetScheduler::Update` today, plus two gates in front:

```cpp
void DiaSimTimeModule::DoUpdate(const SimTimeContext& ctx) {
    mDomainRegistry.TickAll();              // advances non-world sub-domains only — world
                                             // already ticked by the PU this Update() (Issue 2)
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
- Consumer (`DiaRenderTime` on RenderPU): `StreamReader<SimTimeContext> mSimTimeReader{this, "SimTime"};` — `FetchLatest()` for the current snapshot (`FetchClosestTo(t)` once ring-buffer interpolation lands), then pushes the resulting `RenderTimeContext` into the owning PU via `SetRenderTimeContext()` (Issue 3) — sibling RenderModules read it one tick stale, by design.

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
| 1.0 | Promote the PU-placement check in `ProcessingUnit::AddModule` (`TypeRegistry::GetAllowedPUs` + `DIA_ASSERT`) from `#ifdef DIA_DEBUG`-only to firing in all configs, including Release. **Also close the `kAny`-PU gap** (finding 11): the check is skipped outright whenever `mAffinity == PUAffinity::kAny` (`ProcessingUnit.cpp:86-88`) — structural, not Debug/Release, since any custom-named PU (e.g. CluicheEditor's `EditorPU`) defaults to `kAny` and gets zero enforcement in any config, forever. Validate `kAny` PUs against a module's declared `kAllowedPUs` too, when one exists, instead of skipping the PU side outright. | `TestPUPlacementAssert`: mismatched `kAllowedPUs` fails in a Release-config test run, not just Debug; a module with a declared `kAllowedPUs` placed on a custom-named `kAny`-affinity PU it's not allowed on also fails | Done | sonnet | Independent of 1.1–1.3; land before opening the cutover branch (Issue 6/9) — it's the safety net for the whole migration; Fixed dead RELEASE_DIA_ASSERT (undefined theConsole/REALBREAKPOINT), routed through g_pAssertFunc; closed kAny-PU gap. Tests pass (18+46). Commit bdd5254f. |
| 1.1 | Extend `Core::TimeServer`: add `Pause()`/`Resume()`/`IsPaused()`/`Step(TimeRelative)`/`AdvanceTo(TimeAbsolute)`; **strip the `sleep_for` from `Tick()`** (rate-limiting stays in ProcessingUnit). **Remove `TimeServerModule` (CluicheGameBaseline) in this same task** (finding 17), not in 1.7 — it's the only production caller relying on `Tick()`'s current blocking behavior, and is today an undocumented global 30Hz throttle for the whole SimPU (every sibling SimModule is silently capped by it). Leaving its removal for a later task creates a real window where the SimPU jumps to full Hz and `DummyLevelModule`'s FPS readout starts reporting garbage. Drop the dead dependency from its two callers (`InputStreamModule`, `DummyLevelModule`). | `TimeServerTests` (DiaCore): pause freezes time, resume restores prior scale, Step advances one step while paused, AdvanceTo jumps forward and is a no-op backward. `TimeServerModule` and its two call sites no longer reference it; baseline app still compiles/launches (temporarily un-rate-limited SimPU is acceptable until 1.4 lands real pacing) | Done | sonnet | Pulls `TimeServerModule` removal forward from 1.7 to close the sequencing gap; TimeServer extended (Pause/Resume/IsPaused/Step/AdvanceTo), sleep stripped from Tick(); TimeServerModule removed + 2 manifests + vcxproj + registeredtypes.diaschema fixed up. googletest TimeServer* (28 tests) + cluichetest launch both pass. Commit d9b16bb0. |
| 1.2 | Add context structs `SimTimeContext` / `RenderTimeContext` / `MainTimeContext` (header-only) in **DiaCore/SimTime** (Q1 — must sit ≤ DiaApplicationFlow to avoid a cycle). Namespace `Dia::SimTime`. | compile-only + `SimTimeContextTests` field defaults | Done | sonnet | `gameDt` is `TimeRelative` (int64), not float (ST-004); Implemented + own test passes standalone. Part of hard-cutover window (diasimtime/typed-modules-cutover branch) — not marked Done until 1.4 lands and the whole window batch-verifies together (typed bases in 1.3 can't link until then).; Batch-verified with 1.3/1.4: 42 targeted tests + 527 ApplicationFlow tests + cluichetest clean, all pass. Commits 5aa2e066, 45d82dc0 (vcxproj). |
| 1.3 | Add typed base classes `SimModule` / `RenderModule` / `MainModule` in DiaApplicationFlow (Pattern A). Seal `DoUpdate(float) final`, forward to typed virtual via PU accessor. | `TestPUTypedModules` (GoogleTests/ApplicationFlow): a `SimModule` receives populated `SimTimeContext`; `RenderModule` cannot be constructed against sim time | Done | opus | Highest-judgment framework task; mirrors `TestStageModuleBase` precedent; Implemented (code-complete, self-reviewed). Cannot build/link standalone by design — DoUpdate(float) forwards to ProcessingUnit::GetSimTimeContext()/GetRenderTimeContext()/GetMainTimeContext(), added in Task 1.4. Batch-verifies once 1.4 lands.; Batch-verified with 1.2/1.4: TestPUTypedModules links and passes now that 1.4 landed. Commits d2f2e1fb, 45d82dc0 (vcxproj). |
| 1.4 | `ProcessingUnit`: implement the bare `SimTimeDomain` class (Now/Step/SetScale/Pause/Resume/Step/AdvanceTo/GetScale/IsPaused/GetId/Tick — moved here from 2.1, Issue 4/7) and own one as the world clock for `kSim` (Q1 — not a bare `TimeServer`). Extract the existing forward/reverse pass loops into `RunForwardPass`/`RunReversePass` helpers (Pattern B). Add the **fixed-timestep accumulator** for `kSim` (ST-011): bank real `dt` into `mSimAccumulatorSec`, drain in whole `1/GetFrequencyHz()` steps, run `RunForwardPass` once per step capped at `mMaxCatchUpTicksPerFrame` (new `ProcessingUnit` constructor param, sourced from the SimPU's own manifest declaration alongside `frequencyHz`/`dedicatedThread` — default 5; NOT `DiaSimTimeModule` config, since `ProcessingUnit` doesn't read module config JSON), reset the accumulator to zero while `IsPaused()`. `RunReversePass` + post-tick bookkeeping stay outside the loop, exactly once per real `Update()` call. Build+cache Main context directly in `Update()`; add framework-internal `SetRenderTimeContext()` (Render context is pushed by 1.7's `DiaRenderTime`, not computed here — Issue 3/6); expose `GetSimTimeContext()/GetRenderTimeContext()/GetMainTimeContext()`. New metric `simtime.accumulator.dropped_ticks` + `DIA_LOG_WARNING` on catch-up drop. | `TestProcessingUnitContext`: Sim PU advances game time each drained step, Main PU exposes wall-clock dt, RenderPU's cached `RenderTimeContext` unchanged until `SetRenderTimeContext()`. `TestSimAccumulator`: a single real `Update(dt)` call with `dt` = 3× the fixed step runs the forward pass exactly 3 times, each with the fixed-size `gameDt`; a `dt` smaller than one fixed step runs the forward pass 0 times and banks the remainder; backlog beyond `mMaxCatchUpTicksPerFrame` is dropped, logged, and counted, not deferred; pausing then a long real `dt` gap does not trigger a catch-up burst on the next call; `kStopping` modules still tick via `RunReversePass` even when 0 fixed steps drained; a `kStarting` SimPU module still receives `DoStart()` polling within one fixed step's worth of real time even when a given real `Update()` call drains 0 steps (finding 22 — startup is bounded, not stalled indefinitely) | Done | opus | Depends on 1.1, 1.2, 1.3 only — no longer depends on 2.1 (Issue 4/7 resolved the circular dependency). Highest-risk task in the plan: restructures `ProcessingUnit::Update`'s control flow, not just its content; Split into two commits: SimTimeDomain (DiaCore, composes TimeServer, own tests pass standalone) landed; ProcessingUnit accumulator wiring (RunForwardPass/RunReversePass extraction, context accessors, manifest maxCatchUpTicksPerFrame field, dropped-ticks metric) dispatched — this is what finally links Tasks 1.2/1.3/1.4 together for batch verification.; Batch-verified: 42 targeted tests + 527-test ApplicationFlow sweep + cluichetest clean, zero sim_catchup_dropped, zero regressions. Commits 76ee1e11 (SimTimeDomain), 463c7924 (ProcessingUnit wiring). |
| 1.5a | Migrate the **genuinely PU-ambiguous** production modules to typed bases, using the **grounded recommendation below** (owner confirms or overrides — no longer a blank data call): `EntityModule`→**SimPU** (owns entity storage/lifecycle; matches current `dummy_stage.diaapp` placement), `Physics2DModule`→**SimPU** (steps `PhysicsWorld`; matches current `cluiche_main.diaapp` placement), `MessageBusModule`→**SimPU** (own spec `diamessagebus.md` already states this as binding PD-002), `EntitySpawnerModule`→**SimPU** (header states sim-thread intent), `TriggerScriptModule`→**SimPU** (header: "Module on SimPU"), `SensorModule`→**SimPU** (header: "Module on SimPU"), `AIBudgetModule`→**SimPU** (header: "Place on SimPU"), `MetricsCollectorModule`→**MainPU** (medium-high confidence, not an explicit statement — wall-clock FPS/uptime/memory telemetry, same shape as `ObservationModule`/`ProfilerModule` which already sit on MainPU; worth a second look). **`Dia::ApplicationFlow::ObservationModule` (the engine one) drops out of this set entirely** — already declares `kAllowedPUs = kAny` in source (`ObservationModule.h:20`), covered by ST-013, not assigned here. Excludes CluicheEditor's modules — pre-decided all `MainModule`, moved to 1.5c (finding 16). `Physics2DModule`/`PhysicsWorld` and the SoftBody2D driver already run their own hardcoded-Hz internal fixed-step accumulators (30Hz/4-substeps, 60Hz/8-substeps) — kept as-is on migration, no physics-feel change; wire their catch-up-drop counters into `simtime.accumulator.dropped_ticks` or an equivalent metric (finding 18). | existing suites compile + pass | Done | sonnet | 7 of 8 recommendations are high-confidence (explicit doc/spec statements or matching live placement); only `MetricsCollectorModule`→MainPU is workload-fit reasoning, not a stated fact — owner should double check that one specifically; All 8 migrated per owner-confirmed assignments. Found+fixed 9 additional test-only DoUpdate wrapper files not in original task scope. 463 tests + cluichetest pass. Commit e2217778. |
| 1.5b | Migrate the CluicheGameBaseline modules with affinity already declared via `kAllowedPUs` to typed bases (re-count from a fresh grep before dispatch, adjusted for the two carve-outs below). Excludes `EntityModule.h`/`Physics2DModule.h` — neither actually declares `kAllowedPUs` despite living in this folder; moved to 1.5a (finding 16). Excludes `ObservationModule.h`/`ProfilerModule.h` — both deliberately declare `kAllowedPUs = kAny`; exempted from the typed-base migration entirely per ST-013, keep their existing direct `Module::DoUpdate(float)` override, unmigrated (finding 13). Note the `Cluiche::AppFlow::RenderModule` naming collision with the new `Dia::ApplicationFlow::RenderModule` typed base (finding 23) — rename the Cluiche class (e.g. `SceneRenderModule`) before migrating it; the self-referential unqualified name is a landmine under `using namespace Dia::ApplicationFlow`. | baseline app launches; smoke stage runs | Done | sonnet | Parallelizable with 1.5c after 1.3/1.4 land. `EntityModule`/`Physics2DModule` migrate under 1.5a instead; `ObservationModule`/`ProfilerModule` don't migrate at all; 24 modules migrated (8 Main/13 Sim/3 Render) + RenderModule->SceneRenderModule rename. 60 tests + cluichetest pass. Commit e2ca1d03. |
| 1.5c | Migrate CluicheEditor's modules (re-count from a fresh grep before dispatch — miscounted as 7, actually 8 including `ChatPanelBridge.h`; verify that one isn't a false-positive same-named method before including it) — all pre-decided `MainModule`, no owner confirmation needed (moved out of 1.5a, finding 16). Migrate CluicheTest's 7 standalone modules + rewrite `TestStageModuleBase`'s override: it already has its own sealed `DoUpdate(float) final`, which is illegal to re-seal via `SimModule` — delete it and replace with `DoUpdate(const SimTimeContext&)` converting `gameDt`→float internally (finding 21, a full rewrite, not a rename — still contained to one file). ~22 leaf stages inherit unchanged — they only ever override `OnUpdate(float)`. | `dia run cluichetest`; editor launches | Done | sonnet | `TestStageModuleBase` is the leverage point — one edit covers 22 stages; 7 CluicheEditor (MainModule) + 6 CluicheTest standalone + TestStageModuleBase (SimModule, final-reseal nuance handled). 22 leaf test stages inherit unchanged. 41 tests + cluichetest + cluicheeditor build all pass. Commit acaf7f3b. |
| 1.6 | Migrate ~25 GoogleTest module doubles to typed bases (`void DoUpdate(float) override {}` → typed). | `dia run googletest` clean | Done | haiku | Mechanical; do in same commit window as 1.3 to avoid long red period (Open Q3); 27 doubles across 15 files migrated to SimModule; TestPUAffinity.cpp's 4 metadata-test doubles deliberately excluded. 975 tests pass. Commit b2e01693. Closes the hard-cutover window. |
| 1.7 | Add `DiaMainTime` (`MainModule`) + `DiaRenderTime` (`RenderModule`): Render reads `StreamReader<SimTimeContext>::FetchLatest()`, builds `RenderTimeContext`, and pushes it via `ProcessingUnit::SetRenderTimeContext()` (Pattern D, Issue 3/6) — one-tick-stale by design, no manifest-order dependency. | `DiaRenderTimeTests`: sibling RenderModules read the `RenderTimeContext` pushed on the *previous* tick, not a live PU computation | Done | sonnet | Depends on 1.4. `TimeServerModule` removal already done in 1.1 (finding 17) — not this task's job anymore; DiaMainTime (MainModule) + DiaRenderTime (RenderModule) implemented in Dia/DiaSimTime. DiaRenderTime reads StreamReader<SimTimeContext>::FetchLatest() from the 'SimTime' FrameStream, tolerates no publisher (nullptr) gracefully, builds RenderTimeContext, pushes via ProcessingUnit::SetRenderTimeContext(). frameDt hardcoded 0.0f with TODO -- DiaRenderTime has no wall-clock source of its own; real gap, tracked, not fixed here. Found+fixed a real blocking bug in FrameStreamStore<T>::Slot (DiaStreams/FrameStreamStore.h): default member init 'T data{}' required T default-constructible, but SimTimeContext's TimeAbsolute/TimeRelative members are factory-only (private default ctor) -- changed Slot::data to std::optional<T> (no default-ctor requirement), verified no regression across 124 stream tests. New tests: Cluiche/Tests/GoogleTests/DiaSimTime/TestDiaRenderTime.cpp (3 tests: no-publisher default-stays, publish-then-read round trip, DiaMainTime start/update/stop). All pass; ApplicationFlow*/SimTime* regression filter (15 tests) and cluichetest both clean.; Commits 28cbc3ff, ccb5dfb9, 787ef701. Verified independently: 141 targeted tests + cluichetest clean. |
| 1.8 | Replace `FrameStreamStore`'s current 2-slot double buffer with a real multi-sample ring buffer, then implement `FetchClosestTo` interpolation on top — it's not a stubbed method sitting on existing ring-buffer storage; today it just calls `FetchLatest()`, and the storage itself needs to change first (finding 20). | `FrameStreamStoreTests`: ring buffer holds N samples; closest-to returns the bracketing pair for a given timestamp and interpolates correctly; existing `FetchLatest()` callers see no behavior change | Done | opus | Unblocked by real sim time; nice-to-have — can defer if timeboxed. Bigger than originally scoped (storage rewrite, not a stub fill-in) — model bumped sonnet→opus; 8-slot ring buffer replaces 2-slot double buffer; FetchClosestTo implemented as nearest-sample selection (O(8) scan) -- const T* signature can't blend values. Zero API/behavior change for FetchLatest(). 135 tests + cluichetest clean. Commit 0e55eb7b. Closes Phase 1 (Foundation). |

### Phase 2 — SimTimeDomain

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 2.1 | `SimTimeDomainRegistry` only in **DiaCore/SimTime** (Q1) — the `SimTimeDomain` class itself now lands in 1.4 (Issue 4/7). Named tree, child scale = parent scale × local scale, pause/resume/step/advance per subtree, StringCRC keys, `kWorldId` always present. Registry roots at the SimPU's world domain created in 1.4 (Pattern B); `TickAll()` advances all *non-world* domains — `kWorldId` is externally clocked by the PU and is a no-op inside `TickAll()` (Issue 2/5). | `SimTimeDomainTests`: child inherits parent pause; independent scale composes; `TickAll()` advances non-world domains and does NOT re-advance `kWorldId` | Done | opus | Registry binds to an externally-owned `SimTimeDomain&` (ctor injection) for `kWorldId` — never copies it, so it's structurally impossible for `TickAll()` to double-tick the world (no separate reconciliation, per Issue 4/5). Sub-domains stored as `std::optional<SimTimeDomain>` entries (DynamicArrayC<Entry,32>, tombstoned on Destroy) since SimTimeDomain isn't default-constructible — same pattern as Task 1.7's FrameStreamStore::Slot fix. `kWorldId`/`kNoParent` are `static const StringCRC` (not literal `constexpr` as the spec's illustrative snippet shows — `CRC::Calc` is a runtime table lookup, not constexpr; follows the existing `StringCRC::kZero` precedent). Scale composition re-applies ancestor-product×local before each `Tick()` then restores local, preserving `TimeServer`'s one-tick `SetScale` latency. 22 tests (11 existing SimTimeDomain + 11 new Registry) pass. Commit b2673624. |
| 2.2 | Wire `SimTimeDomainRegistry` to the PU's world domain and expose it through `DiaSimTimeModule`. **Reduced to wiring** by Q1 — the PU clock already *is* the world domain, so there is no ownership reconciliation. | `TestSimClockOwnership`: pausing `world` domain freezes `SimTimeContext.gameTime`; `DiaSimTimeModule::DoUpdate`'s `TickAll()` call does not double-advance `kWorldId` (Issue 2/5 regression guard) | Done | sonnet | Downgraded opus→sonnet: Q1 dissolved the design seam; Wired: ProcessingUnit now owns mDomainRegistry(mWorldDomain), declared after mWorldDomain; exposed via GetDomainRegistry(). No TickAll() call site added (deferred to Task 4.4/DiaSimTimeModule, which does not exist yet). New test TestProcessingUnitDomainRegistry.cpp (3 cases) added directly against ProcessingUnit since DiaSimTimeModule::DoUpdate doesn't exist yet; the module-specific double-tick regression guard from the original Test column gets re-added in Task 4.4. |

### Phase 3 — SimTimeScheduler

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 3.1 | `SimTimeScheduler` data structure: bucketed timer wheel + min-heap overflow, `DynamicArrayC`-backed, `ScheduleHandle` generation/validation. | `SimTimeSchedulerTests`: insert/fire ordering, cancel, reschedule, handle reuse safety | Done | opus | Built the full scheduling engine (ScheduleAt/After/Recurring/Cancel/Reschedule/GetQueueDepth) minus EventStreamStore wiring, deliberately scoped out to Task 3.2 — zero DiaStreams/DiaApplicationFlow coupling. 64x10ms wheel (640ms horizon) + 256-cap min-heap overflow; generation-packed uint64 handles (low=index, high=generation); epoch-based lazy deletion (separate from generation) makes Cancel/Reschedule/re-arm structure-agnostic — stale wheel/heap Refs self-heal on next visit. Tick() has a templated out-param overload reporting fired payloads (test-visibility hook; real firing destination lands in 3.2). Recurring re-arm advances from previous scheduled time, not currentTime, to avoid drift. 16 tests pass. Commit 2d09a565. |
| 3.2 | Scheduler API + `Tick(currentTime)` firing into `EventStreamStore<SimTimeSchedulerFire>` (Q2 — payload `{ StringCRC eventType; StringCRC targetSystemId; }`, consumers filter; fan-out, no callbacks); `ScheduleAt/After/Recurring`; recurring re-arm; pause/scale-correct (ticks against `world` domain, not wall clock). | `SimTimeSchedulerTests`: events fire at game time not wall time; recurring re-arms; paused clock defers firing; consumer filters by targetSystemId | Done | sonnet | `ScheduleAt/After/Recurring`/Cancel/Reschedule/re-arm were already fully built in 3.1 — this task's real scope was narrower than it reads: added `Connect(IStreamConnector&, maxReaders=16)` wiring a real `EventStreamWriter<SimTimeSchedulerFire>` member (owner=nullptr, safe per EventStreamWriter's `$framework` sentinel fallback), and `TickInternal`'s fire loop now `Send()`s each fired payload through it alongside the existing out-param report. Pause/scale correctness was already inherent (`Tick(currentTime)` never reads wall-clock; whatever the caller passes governs firing) — documented, not changed. `Connect()` isn't called from a real `Module::OnConnectStreams()` yet since `DiaSimTimeModule` doesn't exist until Task 4.4 — same deferred-wiring pattern as Task 2.2. 4 new tests (real fan-out delivery, multi-reader fan-out, safe no-op when disconnected, wall-clock independence) + all 16 existing = 20 pass. Commit b0327125. Closes Phase 3. |
| 3.3 | Add an optional capacity parameter to `EventStreamWriter<T>::Connect()`/`EventStreamReader<T>::Connect()` (DiaStreams) — today hardcoded to `kDefaultMaxReaders=8` with silent failure on overflow (`RegisterReader()` returns -1, uncontrolled by the caller). Use a larger capacity for the `SimTimeSchedulerFire` stream; add a `DIA_LOG_WARNING` on connect-time overflow instead of silently returning a disconnected reader (finding 15). | Test: a reader connecting past a store's configured capacity gets a logged warning, not a silent no-op; a 9th distinct reader against a capacity-16 store connects successfully | Done | sonnet | Real bug found and fixed: `maxReaders` ctor param existed but `ReaderBuffer mReaders[kDefaultMaxReaders]` was a compile-time-fixed 8-slot array regardless of that param's value — requesting >8 readers was a silent no-op. Heap-allocated `mReaders` sized to `mMaxReaders` (ctor/dtor); `RegisterReader()`/dtor loop bounds fixed; `DIA_LOG_WARNING` added on connect-time overflow; optional `maxReaders` threaded through both `Connect()` overloads. 3 new tests (`EventStreamCapacityTest`) + all 133 DiaStreams-related tests pass. Commit f2d96d55. |

### Phase 4 — DiaSimTime umbrella

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 4.1 | `ISimTimeBudgetedSystem` + `SimTimePriority`; add `IAIBudgetedSystem` as deprecated `using`-alias. | compile + `ISimTimeBudgetedSystemTests` default priority | Not Started | sonnet | Preserve AB-005 (`UpdateBudgeted(0)` is a valid no-op) |
| 4.2 | `SimTimeBudget`: extend `AIBudgetScheduler` with 4 priority tiers + deadline/carry-forward promotion (ST-008). Metrics `simtime.budget.*`. Add the **one-shot completion path** (ST-012, finding 12): a separate, lightweight queue for transient async work (`UtilityEvalWorkItem`, `HTNPlanWorkItem`-style) that runs to completion within a budget slice but never occupies a `kMaxSystems=16` steady-state registry slot. | `SimTimeBudgetTests`: low-priority system starved then promoted at deadline; `used_ms`/`deferred_count`/`stale_ms_max` accurate. `SimTimeOneShotTests`: N concurrent one-shot work items complete without consuming a steady-state slot; the 16-slot steady-state cap is unaffected by one-shot volume | Not Started | opus | Extend not rebuild (Pattern E). One-shot path is new surface area, not present in `AIBudgetScheduler` today |
| 4.3 | `SimTimePolicy` / `SimTimeState` / `SimTimeTier` + `SimTimeRegistry`: per-system registration, `DueThisTick` tier→Hz mapping, sleep/wake, wake-condition registration (time→scheduler, message→EventStream). | `SimTimeRegistryTests`: tier throttles call rate; sleeping system skipped; `RegisterWakeOnTime` wakes at T | Not Started | opus | Gate sequence sleep→policy→budget |
| 4.4 | `DiaSimTimeModule` (`SimModule`): assemble domain registry + scheduler + budget + registry; per-tick gate loop (Pattern C); publish `StreamWriter<SimTimeContext>`; register all `simtime.*` metrics; `OnConfigure` parses config JSON (budget tiers, tier_hz — no `worldDomainHz`; the accumulator's `maxCatchUpTicksPerFrame` is a `ProcessingUnit`/manifest field set in 1.4, not module config). | `DiaSimTimeModuleTests`: full tick sequence; registered system gated correctly; SimTimeContext published | Not Started | opus | Capstone; depends on 4.1–4.3, Phase 2, Phase 3 |
| 4.5 | Migrate `DiaUtilityAI`, `DiaHTN` async paths, `DiaPathfinding` adapter, **and `DiaBehaviourTree::BehaviourTreeSystem`** (a 4th `IAIBudgetedSystem` implementer found in the blast-radius sweep — not yet wired into a production `AIBudgetModule`, only exercised by its own GoogleTests, but implements the interface being migrated; finding 19) from `IAIBudgetedSystem` to `ISimTimeBudgetedSystem`; assign priorities. Register `UtilityEvalWorkItem`/`HTNPlanWorkItem` async one-shot evaluations through the new lightweight completion path (ST-012) instead of `SimTimeRegistry` — they're transient, not steady-state registrations. | those systems' existing suites pass; register with `DiaSimTimeModule`; `BehaviourTreeSystemTests` pass under the new interface | Not Started | sonnet | Alias makes this low-risk; verify no behaviour change |
| 4.6 | `DiaSaveGame`: add `SaveContext::Write(int64_t)` / `LoadContext::Read(int64_t)` overloads, mirroring the existing `int32_t`/`float`/`bool`/`const char*` pattern (`SaveContext.h`/`LoadContext.h`). Needed to carry `TimeAbsolute`/`TimeRelative` (`AsLongLongInMicroseconds()`) through save/load. | `SaveContextTests`: an `int64_t` round-trips exactly through `Write`/`Read`, including values beyond `int32_t` range | Not Started | sonnet | Cross-cutting `DiaSaveGame` fix, small and mechanical, motivated entirely by 4.7. Independent — can land anytime before 4.7 |
| 4.7 | `SimTimeSaveState` (`ISaveable`) in DiaSimTime: `Serialize` writes world `gameTime` (via 4.6's overload), the scheduler's pending entries (`eventType`/`targetSystemId`/fire time), and each registered system's `SimTimeState`. `Deserialize` restores in fixed internal order — world domain re-anchored via `AdvanceTo(savedGameTime)` first, then scheduler entries re-inserted via `ScheduleAt` (fresh `ScheduleHandle`s, ST-016), then sleep state re-applied via `Sleep()`/`Wake()` for currently-registered systemIds (log + skip unregistered ones). Register with `SaveRegistry` from `DiaSimTimeModule::DoStart()`. | `SimTimeSaveStateTests`: save then load reproduces identical `gameTime`; a pending scheduler entry fires at the same absolute game time post-load under a fresh handle; a sleeping system remains asleep across save/load; a saved entry for an unregistered systemId logs a warning and is skipped, not asserted | Not Started | opus | Depends on 4.4 (DiaSimTimeModule, scheduler, registry all built) and 4.6 (int64 overload). Single-participant design (ST-015) avoids any `SaveRegistry` ordering dependency |

### Cross-cutting / closeout

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| C.1 | Create `DiaSimTime.vcxproj` + filters via `dia docs vcxproj-add`; register in `Cluiche.sln` (`dia check sln-sync`); write `dia.diasimtime.architecture.module.md`; `dia docs registry`. | `dia check deps`; `dia check sln-sync` clean | Done | sonnet | Do early enough that new files have a home (after 1.2); DiaSimTime.vcxproj+filters authored (DiaCore/DiaApplicationFlow/DiaStreams/DiaObservation refs); registered in Cluiche.sln (ProjectDependencies + config mappings); Docs/dia.diasimtime.architecture.module.md written (Docs/ schema, mirrors DiaSensor). dia check sln-sync had a real bug -- computed a reassignment for a never-nested project but never wrote a new NestedProjects line, looping forever reporting the same MOVE; fixed in sln_sync.py (append path for old_folder=='<none>'), now correctly nests DiaSimTime under 1.2-Application. dia docs registry run; DiaSimTime doesn't appear (Docs/ schema isn't scanned by the registry generator, same as DiaSensor -- pre-existing, not a regression).; Commits 28cbc3ff (FrameStreamStore fix — landed as part of 1.7's discovery), ccb5dfb9 (sln-sync fix), 787ef701 (feature). Verified independently: dia check deps clean, sln well-formed, tests pass. |
| C.2 | `dia check deps` + `dia check spec-sync` green; observation-opportunity scan on touched files; update feature statuses Draft→Done. | both checks pass | Not Started | sonnet | Verify gate |
| C.3 | `dia docs spec-done diasimtime.md` (system spec Done only after buildable features complete; SimTimeAnalytical remains Planned). | — | Not Started | haiku | Final |

---

## Dependency Order & Parallelism

```
1.0 (independent — land before opening the cutover branch)

1.1 ─┐
1.2 ─┼─► 1.3 ─► 1.4 ─┬─► 1.5a/1.5b/1.5c (parallel) ─► 1.6
     │               ├─► 1.7 ─► 1.8
     │               └─► 2.1 ─► 2.2 ─► 3.1 ─► 3.2
     C.1 (after 1.2)                                  │
                                                       │
                    4.1 ─► 4.2 ─┐                      │
                    4.3 ────────┼─► 4.4 ◄───────(needs 2.2 + 3.2)
                                │
                                └─► 4.5 (after 4.4)
                                          └─► C.2 ─► C.3
```

- **Pre-cutover prep (before opening the branch):** Task 1.0 (Release-mode + `kAny`-PU placement assert) lands first and independently. Task 1.5a's affinity assignments — now narrowed to the genuinely-ambiguous engine-lib set only (CluicheEditor and deliberately-`kAny` modules removed from this gate, finding 16/13) — must be confirmed by the module owner *before* the branch opens, not discovered mid-cutover while the tree is already red (Issue 6/9).
- **Hard cutover window:** 1.3 + 1.4 + 1.5* + 1.6 should land together on one short-lived branch. `DoUpdate` is pure-virtual, so a partial migration will not compile (Open Q3). `TestStageModuleBase` absorbs 22 stages in one edit, which shrinks the window considerably. Blast radius is accepted as-is — no reduction in cutover scope.
- **Merge gate:** the cutover branch does not merge to `Development` until `dia run googletest`, `dia run cluichetest`, and CluicheEditor launch all pass clean *on that branch* — not just individually as tasks complete.
- **Parallelizable:** 1.5a/b/c after 1.3+1.4; 2.1/2.2 after 1.4 (no longer gated behind all of Phase 1 — Issue 4/7); 4.1/4.2 vs 4.3 before 4.4; Task 3.3 (DiaStreams reader-cap fix) is independent of everything else — land whenever, but before 4.5 if many system types are expected to want scheduler events; Task 4.6 (DiaSaveGame int64 overload) is likewise independent — land anytime before 4.7; Task 4.7 (`SimTimeSaveState`) depends on 4.4 + 4.6 and closes out Phase 4 alongside 4.5.

## Open Questions

### Resolved (2026-08-14)

1. **SimTimeDomain / context-struct home → DiaCore.** *Decisive:* the typed base classes (DiaApplicationFlow) name `SimTimeContext`, and the DiaSimTime module depends on DiaApplicationFlow — so the structs cannot live in the module without a cycle. Both the context structs and `SimTimeDomain`/`SimTimeDomainRegistry` go in **DiaCore/SimTime** (namespace `Dia::SimTime`), beside `TimeServer`. Bonus: the SimPU owns the world `SimTimeDomain` as its clock, dissolving the old Task 2.2 reconciliation seam. Scheduler/budget/registry/policy/state stay in the module. *(Affects 1.2, 1.4, 2.1, 2.2.)*
2. **Scheduler event routing → EventStreamStore fan-out, no callbacks.** Scheduler `Send`s `Event<SimTimeSchedulerFire>` with payload `{ StringCRC eventType; StringCRC targetSystemId; }`; subscribers filter. Grounded: `EventStreamStore<T>` fans a copy to all ≤8 readers with no built-in target routing — filtering is the native pattern. Callbacks rejected (pointer-lifetime + coupling). Coarseness (every reader copies every event) is acceptable at scale; revisit with a dedicated per-target stream only if measured. *(Affects 3.2.)*
3. **DoUpdate migration → hard cutover, single branch, compiler-driven.** Coexistence would keep `Module::DoUpdate(float)` overridable and reopen the exact hole ST-001 closes; and since the typed bases seal `final`, no "half-migrated compiles" state exists anyway. Branch order: 1.1+1.2 → 1.3+1.4 → chase errors across 1.5a/b/c+1.6 → merge atomically. Run when the tree is quiet. *(Affects 1.3–1.6.)*
5. **Save/load contract for `SimTimeScheduler`/`SimTimeState` — RESOLVED (2026-08-21): build it now, in this plan.** Owner decision: in scope, not deferred. A single combined `SimTimeSaveState` (`ISaveable`) participant persists world `gameTime`, the scheduler's pending queue, and per-system sleep state with an internally-sequenced restore order (ST-015); `ScheduleHandle`s are not preserved across save/load (ST-016); `DiaSaveGame` gains an `int64_t` overload (Task 4.6). *(New Tasks 4.6, 4.7.)*

### Still open (do not block dispatch)

4. **IAIBudgetedSystem alias retirement timeline** (affects 4.5 cleanup — now 4 consumers, not 3: DiaUtilityAI/DiaHTN/DiaPathfinding/DiaBehaviourTree, finding 19) — retire once all four are migrated, or keep for game/third-party code?
6. **`SimTimeAnalytical` save/load contract** (Phase 5 — genuinely out of scope here, distinct from item 5's Phase 1-4 scheduler/dormancy state) — dependency on `DiaSaveGame` vs each system serialising its own `LastEvaluated`.

### Pre-dispatch checklist item (not a design question)

- **Task 1.5a's affinity assignments are grounded and recommended (2026-08-21), not blank** — 8 modules, 7 high-confidence (SimPU, matching explicit doc/spec statements or live placement), 1 medium-confidence (`MetricsCollectorModule`→MainPU, workload-fit reasoning). Owner confirms or overrides before dispatch; the engine's own `ObservationModule` and CluicheEditor's 8 modules no longer block this task (findings 13/16). **CONFIRMED (2026-09-02): owner accepted the recommendation as-is, including `MetricsCollectorModule`→MainPU.** Task 1.5a is unblocked for dispatch when its turn comes.
- **Task 1.0 (Release-mode + `kAny`-PU placement assert) should land before the cutover branch opens** (Issue 6/9, finding 11) — it's the safety net for the whole hard-cutover window, and it's independent of everything else in Phase 1.
