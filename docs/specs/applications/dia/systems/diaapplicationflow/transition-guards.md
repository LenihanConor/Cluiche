# Feature Spec: Transition Guards

## Parent System
@docs/specs/applications/dia/systems/diaapplicationflow/diaapplicationflow.md

## Builds On
@docs/specs/applications/dia/systems/diaapplicationflow/stage-system.md
@docs/specs/applications/dia/systems/diaapplicationflow/stage-transitions.md

## Research
@docs/research/e2e_testing/design-decisions.md

## Traceability

| Level | Spec | Decision IDs honored |
|-------|------|---------------------|
| Platform | @docs/specs/platform/Cluiche.md | PD-001, PD-004, PD-007 |
| Application | @docs/specs/applications/dia/dia.md | AD-001, AD-002, AD-003 |
| System | @docs/specs/applications/dia/systems/diaapplicationflow/diaapplicationflow.md | SD-001, SD-004, SD-005, SD-014, SD-015, SD-017 |

## Problem Statement

Today, every queued `TransitionTo` runs as soon as the next `Update()` reaches `ApplyPendingTransition` — there is no mechanism to **delay** a transition based on conditions outside the framework. This blocks several features:

- **Remote orchestration (DiaOrchestrator/DiaRemoteControl).** The orchestrator wants to drive the app stage-by-stage from outside. To do that, the app must hold at the bootstrap stage on launch and never auto-advance until the orchestrator sends `navigate_to`. Today the app advances autonomously the moment manifest auto-advance fires, racing the WebSocket connection.
- **Future loading screens.** A loading screen module wants to delay the transition to gameplay until its assets resolve.
- **Future asset gating.** A module that needs an off-thread resource ready before its successor stage starts.

The common shape is: *"some module has a reason to hold the next stage transition until it says go."* This feature introduces **transition guards** as the first-class mechanism for that pattern — a generic veto-on-transition hook that any module can register, with no game-specific code in the framework.

DiaRemoteControl will be the first consumer; it is **not** the only intended consumer. Loading screens and asset gating are explicit future use cases (called out in the design decisions doc).

## Acceptance Criteria

| # | Criterion | Verification |
|---|-----------|--------------|
| AC1 | A module can register a transition guard with `Application::RegisterTransitionGuard(Module* owner, GuardFn fn)` returning a bool registration result. | Unit test |
| AC2 | A module can deregister all its guards with `Application::UnregisterTransitionGuards(Module* owner)`. Idempotent (calling with no registered guards is a no-op). | Unit test |
| AC3 | When at least one registered guard returns `GuardResult::Hold`, `ApplyPendingTransition` does **not** call `BeginStop` on any outgoing module. The pending transition stays queued (`mHasPendingTransition` remains true) and is re-checked next frame. | Unit test |
| AC4 | When all registered guards return `GuardResult::Allow` (or there are zero guards), `ApplyPendingTransition` proceeds exactly as today: `BeginStop` outgoing modules → drain → start incoming. | Unit test |
| AC5 | Guards are evaluated in registration order. The first `Hold` short-circuits — remaining guards are not called that frame. | Unit test |
| AC6 | Guards apply to the **initial stage entry** that follows `Application::Start()`. If a guard registered before `Start()` returns `Hold`, the app does not enter `initial_stage` until the guard allows. (Concretely: guards register from `DoStart`, which runs once a module's stage is entered — so the very first entry of `initial_stage` cannot be guarded by a module that was inactive at boot. AC6 covers guards registered by **modules in the boot stage** holding the transition **out of** boot. See AI Review Q1.) | Unit test |
| AC7 | Guards hold indefinitely — no framework-level timeout. (Module start/stop timeouts are unaffected; they apply once the transition has been allowed.) | Spec |
| AC8 | Zero guards registered → behaviour is byte-identical to today. No tests regress, no behavioural change in CluicheTest or CluicheEditor. | Build + existing test suite |
| AC9 | A guard's lifetime is tied to its owning module. The framework does not call a guard registered by a destroyed/detached module. (Achieved via `UnregisterTransitionGuards(this)` from the module's destructor — F1 of this feature documents the convention.) | Code review + unit test (deregister before transition) |
| AC10 | `IApplicationInspectable::GetTransitionInfo` is extended with a `heldByGuards: bool` field that reflects "a transition is queued AND at least one guard is currently holding it." Used by debug tooling. | Unit test |
| AC11 | `IsTransitioning()` returns true while a transition is held by guards (consistent with today's semantics that a queued-but-not-yet-applied transition counts as transitioning). | Unit test |
| AC12 | A `LifecycleEvent` of kind `kStageTransitionHeldByGuard` is emitted on the `$lifecycle` stream **the first time** a frame's guard check returns Hold for the current pending transition. Not re-emitted every frame while still held. Re-armed when the transition is allowed and committed (or replaced by a new pending transition). | Unit test on $lifecycle subscriber |
| AC13 | `dia run cluichetest` boots, runs, and shuts down with no behavioural change. `dia run googletest` passes. | Build + test verification gate |

## Design

### Public API

`Dia/DiaApplicationFlow/Application.h` — additions:

```cpp
namespace Dia { namespace ApplicationFlow {

    enum class GuardResult { Allow, Hold };

    // Guard signature is opaque — guards decide from their own state, not from
    // the from/to stage names. (See AI Review Q4 for rationale.)
    using TransitionGuardFn = std::function<GuardResult()>;

    class Application : public IApplicationInspectable
                      , public IApplicationControl
    {
    public:
        // ... existing ...

        // Registers a guard whose lifetime is tied to `owner`. Caller must
        // call UnregisterTransitionGuards(owner) before the module is
        // destroyed (typically from the module's destructor or DoStop).
        // Returns false if the registry is full (kMaxGuards exceeded);
        // asserts in debug.
        bool RegisterTransitionGuard(Module* owner, TransitionGuardFn fn);

        // Removes every guard registered against `owner`. Idempotent.
        void UnregisterTransitionGuards(Module* owner);

    private:
        struct GuardEntry
        {
            Module*           owner;
            TransitionGuardFn fn;
        };

        // Fixed-capacity registry. 8 is generous: today's only consumer
        // (RemoteControlModule) registers exactly one guard; future
        // consumers (loading screens, asset gating) typically register
        // zero or one each. If we ever exceed 8, it's a design smell.
        static constexpr unsigned int kMaxGuards = 8;

        Dia::Core::Containers::DynamicArrayC<GuardEntry, kMaxGuards> mGuards;

        // True between EmitLifecycleEvent(kStageTransitionHeldByGuard) and
        // the transition being allowed/replaced. Prevents per-frame spam.
        bool mPendingHeldByGuardEmitted = false;

        // Returns Allow if every registered guard returns Allow; Hold if any
        // guard returns Hold. Zero guards => Allow.
        GuardResult CheckGuards();
    };

}}
```

### `IApplicationInspectable` change

`Dia/DiaApplicationFlow/IApplicationInspectable.h` — extend `TransitionInfo`:

```cpp
struct TransitionInfo
{
    bool                  inProgress;
    Dia::Core::StringCRC  fromStage;
    Dia::Core::StringCRC  toStage;
    bool                  heldByGuards;   // NEW
};
```

`Application::GetTransitionInfo()` populates `heldByGuards = mHasPendingTransition.load() && !mGuards.IsEmpty() && CheckGuards() == Hold`.
(The implementation can cache the last computed result rather than re-running guards from `GetTransitionInfo` if guards have side effects worth avoiding — see AI Review Q5.)

### `LifecycleEvent` change

`Dia/DiaApplicationFlow/LifecycleEvent.h` — add a kind:

```cpp
enum class LifecycleEventKind
{
    // ... existing ...
    kStageTransitionHeldByGuard,
};
```

The event populates `fromStage = mCurrentStage`, `toStage = mPendingStage`. No new fields needed.

### Runtime change in `Application::ApplyPendingTransition`

`Dia/DiaApplicationFlow/Application.cpp` — at the top of `ApplyPendingTransition`, **before** consuming the pending transition under the mutex, run the guard check:

```cpp
void Application::ApplyPendingTransition()
{
    // Peek at pending transition without consuming, so we can re-evaluate
    // next frame if guards hold.
    Dia::Core::StringCRC pendingStage;
    {
        std::lock_guard<std::mutex> lock(mTransitionMutex);
        if (!mHasPendingTransition.load())
            return;
        pendingStage = mPendingStage;
    }

    // No-op: already in target stage. Consume + clear the pending flag.
    if (mCurrentStage == pendingStage)
    {
        std::lock_guard<std::mutex> lock(mTransitionMutex);
        mPendingStage = Dia::Core::StringCRC();
        mHasPendingTransition.store(false);
        mPendingHeldByGuardEmitted = false;
        return;
    }

    // Guard check.
    if (CheckGuards() == GuardResult::Hold)
    {
        if (!mPendingHeldByGuardEmitted)
        {
            LifecycleEvent ev;
            ev.kind      = LifecycleEventKind::kStageTransitionHeldByGuard;
            ev.fromStage = mCurrentStage;
            ev.toStage   = pendingStage;
            EmitLifecycleEvent(ev);
            mPendingHeldByGuardEmitted = true;
        }
        return; // try again next frame
    }

    // All guards allowed — proceed exactly as today.
    Dia::Core::StringCRC newStage;
    {
        std::lock_guard<std::mutex> lock(mTransitionMutex);
        newStage = mPendingStage;
        mPendingStage = Dia::Core::StringCRC();
        mHasPendingTransition.store(false);
        mPendingHeldByGuardEmitted = false;
    }

    // ... rest of existing function unchanged: log, emit kStageTransitionStarted,
    //     stop outgoing modules, set mDrainingToStage / mTransitionDraining ...
}
```

`TransitionTo()` is updated to clear `mPendingHeldByGuardEmitted` when a *new* pending stage replaces a held one, so the next held-by-guard event for that new transition fires:

```cpp
void Application::TransitionTo(const StringCRC& stageId)
{
    std::lock_guard<std::mutex> lock(mTransitionMutex);
    if (mPendingStage != stageId)               // new target
        mPendingHeldByGuardEmitted = false;
    mPendingStage = stageId;
    mHasPendingTransition.store(true);
}
```

### Threading

Guards run on the main thread (inside `Update` → `ApplyPendingTransition`). Registration / deregistration is **also main-thread-only** — same contract as `RegisterOrFindStreamStore` (called during `OnConnectStreams` for streams; for guards, called from `DoStart` and module destruction, both of which are main-thread). No locks needed inside `mGuards`.

The spec is explicit on this: *guards do not run from background PU threads*. A dedicated-thread PU's modules can register a guard, but the guard function is invoked from the main thread when `ApplyPendingTransition` runs, so the guard logic must be thread-safe with respect to whatever state it reads.

### Rationale for the chosen shape

Captured from the interview:

- **Hold point: before stop phase.** No outgoing module is touched until the transition is committed to. If a guard says Hold, the app stays in the current stage cleanly. Guarding after stop would leave the app in a partially-torn-down state while held — fragile.
- **Pull model: re-poll guards each frame.** The framework asks; guards answer based on their own state. No callback bookkeeping, no per-guard release tracking. Idempotent. RemoteControlModule's guard becomes `[this]() { return mHeld ? Hold : Allow; }`.
- **App-global registration, lifetime tied to module.** Same model as stream registration. One list. No per-stage indexing complexity.
- **Opaque signature.** Guards decide from their own state, not from the from/to names. RemoteControlModule's hold logic is "am I currently holding?" — it doesn't need stage names. If a future use case needs from/to (e.g., "only guard transitions out of Boot"), we promote the signature then. Smaller surface today.
- **Guards apply to initial entry.** Without this, the orchestrator can't hold the bootstrap transition — directly contradicts the design intent. The narrow caveat (a guard can only hold a transition *out of* the stage in which the guarding module was active, since `DoStart` is what runs `RegisterTransitionGuard`) is documented in AC6 and AI Review Q1.
- **No timeout.** Guards are deliberate. Timing them out forces a false answer; module start/stop timeouts already exist for the genuine "module is wedged" case.

### Out of scope

- Per-stage guards (`RegisterGuard(forStage='Boot', fn)`). Not needed for any current consumer; we'd add a stage filter to the registration call when first needed.
- Guard arguments (from/to stage names). See above.
- Async guard release callbacks. Pull model handles every current use case.
- Cross-PU guards (guards owned by modules on a dedicated thread executed *on* that thread). Guards are main-thread-only; the guard fn must be safe to call from the main thread.

## Files Touched

| File | Change |
|------|--------|
| `Dia/DiaApplicationFlow/Application.h` | Add `GuardResult`, `TransitionGuardFn`, `RegisterTransitionGuard`, `UnregisterTransitionGuards`, `GuardEntry`, `mGuards`, `mPendingHeldByGuardEmitted`, `CheckGuards()`. |
| `Dia/DiaApplicationFlow/Application.cpp` | Update `ApplyPendingTransition` (peek-then-check-then-consume), update `TransitionTo` (clear emit-flag on new target), implement `CheckGuards`, `RegisterTransitionGuard`, `UnregisterTransitionGuards`. |
| `Dia/DiaApplicationFlow/IApplicationInspectable.h` | Add `heldByGuards: bool` to `TransitionInfo`. |
| `Dia/DiaApplicationFlow/LifecycleEvent.h` | Add `LifecycleEventKind::kStageTransitionHeldByGuard`. |
| `Cluiche/Tests/GoogleTests/ApplicationFlow/TestTransitionGuards.cpp` | New test file covering AC1–AC12. |
| `Cluiche/Tests/GoogleTests/GoogleTests.vcxproj` | Add `TestTransitionGuards.cpp`. |
| `docs/specs/systems/dia/diaapplicationflow.md` | Add Decisions row SD-020 (Transition guards as veto hook); add Features row "Transition Guards"; update plan reference. |
| `Dia/DiaApplicationFlow/dia.applicationflow.architecture.module.md` | Document the guard API in the module doc (one-line addition). |

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | TDD-RED: write `TestTransitionGuards.cpp` covering AC1–AC12. Run `dia run googletest --filter="TransitionGuards*"` and quote failing output. | All AC1–AC12 fail with "register/unregister/heldByGuards not declared" | Todo | sonnet | TDD red gate per CLAUDE.md |
| 2 | Add `GuardResult` / `TransitionGuardFn` / API method declarations to `Application.h`; add `heldByGuards` to `TransitionInfo`; add `kStageTransitionHeldByGuard` to LifecycleEvent. | Compiles | Todo | haiku | Mechanical |
| 3 | Implement `RegisterTransitionGuard` / `UnregisterTransitionGuards` / `CheckGuards` in `Application.cpp`; add `mGuards`, `mPendingHeldByGuardEmitted` members. | AC1, AC2, AC9 pass | Todo | sonnet | |
| 4 | Update `ApplyPendingTransition` to peek-then-guard-check-then-consume; emit `kStageTransitionHeldByGuard` once per held-pending; update `TransitionTo` to clear the emit flag on new target. | AC3, AC4, AC5, AC6, AC11, AC12 pass | Todo | sonnet | The peek-vs-consume change is the load-bearing diff |
| 5 | Update `GetTransitionInfo` to populate `heldByGuards`. | AC10 pass | Todo | haiku | |
| 6 | Run `dia run googletest`; confirm AC1–AC12 pass and existing tests still pass. Run `dia run cluichetest` for AC8/AC13. Quote output. | AC8, AC13 verification gate | Todo | sonnet | Verification gate |
| 7 | Add SD-020 to `docs/specs/systems/dia/diaapplicationflow.md` Decisions table; add Transition Guards row to Features table. | Doc only | Todo | haiku | |
| 8 | Update `dia.applicationflow.architecture.module.md` (one-line add). Commit. | git status clean | Todo | haiku | |

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-001 | StringCRC for IDs | Guard owners are tracked by `Module*` pointer (lifetime-scoped to the module), not by ID — registration is an owner-keyed operation. The `LifecycleEvent` envelope already carries `fromStage` / `toStage` as `StringCRC`. No new identifier system introduced. |
| PD-004 | No STL containers in public APIs | `mGuards` uses `DynamicArrayC<GuardEntry, kMaxGuards>` — Dia container, not STL. `TransitionGuardFn` is `std::function<GuardResult()>`. **Note:** `std::function` is already used internally by `Module::DoStart` callbacks and by `JobSystem::JobFn` (Dia::Threading); it is not a container. PD-004 is about containers — no violation. |
| PD-005 | x64 only | No platform-specific code. |
| PD-006 | VS project files source of truth | `TestTransitionGuards.cpp` added to `GoogleTests.vcxproj` manually. |
| PD-007 | C++20 required | `std::function`, `enum class`, designated initializers — all C++17 or earlier; nothing pushes the floor. |
| PD-008 | Directory.Build.props owns build settings | No per-project overrides. |
| PD-009 | Generated output under `Cluiche/out/` | No new outputs. |
| AD-001 | Module docs with YAML frontmatter | `dia.applicationflow.architecture.module.md` updated (one-line). |
| AD-002 | No STL in public APIs | See PD-004 above. |
| AD-003 | Namespace `Dia::<Module>::` | All new code in `Dia::ApplicationFlow::`. |
| AD-004 | PU/Stage/Module for app structure | Reinforced — guards are a Module-owned hook into Stage transitions. |
| SD-001 | Config is sole source of truth for structural wiring | Guards are *behavioural* opt-in registered from code, not structural wiring declared in the manifest. They do not affect manifest schema, validation, or stage diff computation. SD-001 covers structural wiring (PUs, Stages, Streams, Modules) — guards are an orthogonal lifecycle hook, like `DoStart` returning `kLoading`. No conflict. |
| SD-004 | TransitionTo is app-wide | Reinforced — a single guard list is checked once per pending transition, not per-PU. |
| SD-005 | Transitions queued, execute next frame | Reinforced — guards run in `ApplyPendingTransition` which already runs at the top of the next frame. Held transitions stay queued; the next frame re-checks. |
| SD-014 | Full validation at load | N/A — guards have no manifest representation, so there is nothing to validate at load. |
| SD-015 | No shared modules across PUs | Guard ownership is `Module*` — one module owns its guards, no cross-PU sharing. |
| SD-017 | Clean break, no backward compatibility | This is a purely additive change (zero guards = today's behaviour, AC8). No backward-compatibility shim needed. |
| SD-019 | Manifest v3 stages with `transitions[]` + `auto_advance` | Compatible — guards run *between* `mHasPendingTransition` becoming true and outgoing modules being stopped, regardless of whether the pending transition came from a `TransitionTo` call, a manifest auto_advance, or anywhere else. The auto-advance code in `TickTransitionDrain` calls `TransitionTo`, which queues the transition; the next frame's `ApplyPendingTransition` runs the guard check before stopping anything. |

## Open Questions

None.

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Initial entry coverage | AC6 says guards apply to initial stage entry. But guards are registered from `DoStart`, and `DoStart` only runs once a module's stage is entered — chicken-and-egg. How does this actually work? | The bootstrap mechanic is: the manifest's `initial_stage` is e.g. `Boot`. Modules with stage membership `Boot` (or `all`) have `DoStart` called as part of the **initial entry** — which is itself a queued `TransitionTo(initial_stage)` from `Application::Start()`. Those modules' `DoStart` runs *during* the initial transition's start phase, after the guard check has allowed entering Boot. From within their `DoStart`, those modules can register a guard that **then holds the transition out of Boot** to whatever comes next. So the orchestrator's flow is: app starts → enters Boot → RemoteControlModule's `DoStart` registers a holding guard → manifest `auto_advance` of Boot fires `TransitionTo(next)` → next frame's `ApplyPendingTransition` checks the guard → Hold → orchestrator connects, sends `navigate_to`, guard flips to Allow → next frame transitions out of Boot. **What's NOT covered:** holding the very first entry into `initial_stage`, before any module's `DoStart` has run. That's a knowingly-accepted limitation; if needed, a future refinement is "register guards from a module constructor" (already legal — modules exist before stage entry), but no consumer needs it today. |
| 2 | Guard fairness | AC5 short-circuits on first Hold. Is this fair to later-registered guards? Could a misbehaving early guard starve later ones? | Fairness isn't relevant. Guards are not allocated work — they're consulted. Short-circuiting is a performance optimization (skip evaluating guards we don't need). The held-by-guard event identifies *that* the transition is held, not *which* guard held it. If diagnostics ever need "which guard held it," we add a guard ID to the event, evaluate all guards (without short-circuit), and report the first. Today's consumers don't need that. |
| 3 | Held-event re-emission | AC12 says emit once per held-pending, then re-arm on commit/replace. What if the orchestrator subscribes *after* the event was already emitted? | The orchestrator subscribes via `$lifecycle` stream tap (existing infrastructure). Stream-tap consumers receive events emitted *after* they subscribe. If the harness connects mid-hold, it sees the pending transition via the next `GetTransitionInfo()` call (`heldByGuards = true`) and proceeds based on that. The event is a notification of state *change* (start of hold); the inspectable interface exposes the steady-state. Two complementary surfaces. |
| 4 | Opaque signature | The interview chose `GuardResult Check()` (opaque). What if a future consumer genuinely needs from/to? | Promote to overload: keep `RegisterTransitionGuard(Module*, std::function<GuardResult()>)` and add `RegisterTransitionGuard(Module*, std::function<GuardResult(const StringCRC&, const StringCRC&)>)`. Or change the type to a variant. This is a strict superset — no migration cost. The decision today is "don't pay for an unneeded surface"; we can pay later when a use case demands it. |
| 5 | `GetTransitionInfo` calling guards | The doc says `heldByGuards` calls `CheckGuards()`. But guards may have side effects via captured state. Is calling them from an inspectable getter safe? | Safe in practice — guards as designed (RemoteControlModule, future loading screen, future asset gating) are pure reads of the module's own state. But the spec should explicitly require **guards must be idempotent and side-effect-free**, since the framework may call them more than once per frame (once from `ApplyPendingTransition`, possibly once from `GetTransitionInfo` if debug tooling polls). Add this to the API contract. **Implementation:** alternatively cache the last guard-check result + frame number on `Application`, and have `GetTransitionInfo` return the cached value rather than re-evaluate. This is a small optimization and avoids relying on guard purity. We'll cache (smaller blast radius). |
| 6 | UnregisterTransitionGuards in destructor | AC9 ties guard lifetime to module via destructor cleanup. What about modules that fail to destruct cleanly (e.g., on shutdown crash)? | A guard whose owning module is mid-destruction (or destructed but not unregistered) will be called by the framework with an invalid `Module*`. The guard fn is a `std::function` — it captures whatever state it captured. The captured `this` would be dangling. **Mitigation:** ApplicationFlow's existing shutdown path calls `BeginStop` on every module, then waits for `kInactive`. The convention this spec adds: **modules MUST call `UnregisterTransitionGuards(this)` in their destructor and/or `DoStop`.** A debug-build assert in `~Module()` ("guards still registered against destroying module") is added as a safety net (`Module::~Module()` already exists). |
| 7 | Capacity 8 | Why 8 and not unlimited? | Fixed-capacity matches the rest of DiaApplicationFlow (`kMaxProcessingUnits = 4`, `kMaxStreams = 64`, `kMaxRollbackAttempts = 3`). 8 is enough for: 1× RemoteControlModule, 1× LoadingScreenModule, 1× AssetGateModule, 5× headroom. If a real consumer needs more we bump it; the limit is a debug-time sanity check, not a hard product cap. Asserts in debug, returns false in release. |
| 8 | Order of operations relative to `mTransitionDraining` | If a transition is currently draining (outgoing modules stopping), can a *new* `TransitionTo` arrive whose guards then hold? What happens? | The new `TransitionTo` overwrites `mPendingStage` (existing semantics — latest wins). The current drain continues to completion; on completion, `TickTransitionDrain` commits the new stage and starts incoming modules for it. Then the next `Update` calls `ApplyPendingTransition` for the *queued new pending* — and now guards run. This is correct: guards never preempt an in-flight transition; they only gate the *next* one. The drain itself isn't a guarded operation. |
| 9 | Held-by-guard during shutdown | What if a guard is holding when `RequestShutdown()` is called? | Shutdown bypasses the pending-transition machinery entirely (`BeginStopAllActive` is called directly from `Update`'s shutdown path). Guards are not consulted during shutdown. The held pending transition is effectively discarded. **Spec addition:** when entering shutdown, clear `mPendingHeldByGuardEmitted` (no event re-emit on next session — though this matters only if an Application instance is reused, which is not a current pattern). |
| 10 | Naming | Why "guard" and not "gate" / "veto" / "barrier"? | "Guard" matches existing C++ vocabulary (`std::lock_guard`, transition guards in state machines) and is unambiguous about its role: it inspects, it can deny, it never causes the action itself. "Gate" implies a one-shot release. "Veto" implies a vote. "Barrier" implies sync between threads. "Guard" is the most precise. |

## Status

`Done` (2026-05-21) — All 13 ACs pass; 5174 tests GREEN; CluicheTest passes.

Plan: [transition-guards.plan.md](transition-guards.plan.md)
