# Feature Spec: Physics Logging (DiaRigidBody2D)

## Traceability

| Level | Parent | This Feature |
|-------|--------|--------------|
| Platform | @docs/specs/platform/Cluiche.md | - |
| Application | @docs/specs/applications/dia.md | - |
| System | @docs/specs/systems/dia/diarigidbody2d.md | **physics-logging** |

**Status:** `Approved`

---

## Problem Statement

Physics simulations can diverge or stall in ways that produce no immediate visual symptom — a body quietly tunnels, a constraint drifts for many frames, or the accumulator silently discards simulation time because it cannot keep up. Without in-simulation diagnostics, these problems are hard to reproduce and slow to diagnose. Developers need low-friction early warnings fired at the exact moment a dangerous condition is reached, visible in the debug output without requiring a separate profiling pass.

---

## Solution Overview

Insert `DIA_LOG_WARNING("Physics", ...)` and `DIA_LOG_DEBUG("Physics", ...)` calls into the relevant internal simulation functions, all guarded by `#ifndef NDEBUG`. No new public types or functions are introduced; all additions are strictly internal. The `Physics` channel is already registered in the DiaLogger channel registry and is filtered by any sink configured to watch that channel.

Four distinct conditions are instrumented:

1. **MaxSubSteps hit** — emitted in `PhysicsWorld::Update()` when the accumulator loop exits early because the sub-step budget is exhausted.
2. **Body velocity safety threshold** — emitted in `IntegrateLinearVelocities()` after integration when a body's velocity magnitude exceeds a hardcoded safety ceiling. Applies to both `PointBody2D` and `RigidBody2D` pools.
3. **Constraint drift** — emitted in `SolveConstraints()` after all solver iterations when any constraint's positional error remains above a hardcoded drift threshold.
4. **Sleep state change** — emitted in the sleep-state transition path (both directions: sleeping and waking) at `DIA_LOG_DEBUG` level (compiled out in Release alongside `DIA_LOG_WARNING`).

All four conditions fire only in debug builds. In Release (`NDEBUG` defined) every call compiles to `((void)0)` via the `DIA_LOG_WARNING` / `DIA_LOG_DEBUG` macros defined in DiaLogger.

---

## Acceptance Criteria

| ID | Criterion | Verification Method |
|----|-----------|---------------------|
| AC1 | `DIA_LOG_WARNING("Physics", ...)` fires in `PhysicsWorld::Update()` when accumulator loop exits at `maxSubSteps` | Unit test: provide extreme `deltaTime` that forces maxSubSteps exhaustion; verify warning captured |
| AC2 | Warning message contains the `maxSubSteps` count and `deltaTime` values | Unit test: inspect captured log message text |
| AC3 | `DIA_LOG_WARNING("Physics", ...)` fires in `IntegrateVelocities()` when body velocity magnitude exceeds `kMaxSafeVelocity` (1000.0f m/s) | Unit test: apply massive force to a body; verify warning captured |
| AC4 | Velocity warning message contains the body ID and velocity magnitude | Unit test: inspect captured log message text |
| AC5 | `DIA_LOG_WARNING("Physics", ...)` fires in `SolveConstraints()` when any constraint's positional error exceeds `kMaxConstraintDrift` (5.0f units) after all iterations | Unit test: construct a degenerate constraint setup; verify warning captured |
| AC6 | Constraint drift warning message contains the drift value and iteration count | Unit test: inspect captured log message text |
| AC7 | `DIA_LOG_DEBUG("Physics", ...)` fires when a body transitions to `kSleeping` | Unit test: let a body reach the sleep threshold; verify debug entry captured |
| AC8 | `DIA_LOG_DEBUG("Physics", ...)` fires when a sleeping body wakes; message contains wake reason | Unit test: wake body via impulse, force, broadphase, explicit, and constraint; verify reason string in each message |
| AC9 | No warnings fire during a stable single-body simulation at rest (after sleep) | Unit test: run 60 steps with a static scene; assert zero warning-level Physics log entries |
| AC10 | All `DIA_LOG_WARNING` and `DIA_LOG_DEBUG` calls compile to `((void)0)` in Release | Build test: build Release; verify via preprocessor that NDEBUG guard removes the calls |
| AC11 | Full solution builds clean in Debug and Release | `msbuild Cluiche.sln /p:Configuration=Debug /p:Platform=x64` and Release equivalent |

---

## Public API

No new public types, methods, or headers. All changes are internal to the implementation files. The two internal constants introduced are:

```cpp
// DiaRigidBody2D/PhysicsWorld.cpp (internal, not in any header)
#ifndef NDEBUG
static constexpr float kMaxSafeVelocity    = 1000.0f;  // m/s
static constexpr float kMaxConstraintDrift =    5.0f;  // world units
#endif
```

These constants are `static constexpr` at file scope inside the `.cpp` translation units where they are used. They are not part of any header or public namespace.

---

## Implementation Notes

### Insertion Point 1 — MaxSubSteps hit (`PhysicsWorld::Update()`)

```cpp
// In PhysicsWorld::Update(), after the accumulator while-loop exits:
#ifndef NDEBUG
if (steps >= mDef.maxSubSteps)
{
    DIA_LOG_WARNING("Physics",
        "PhysicsWorld: maxSubSteps (%d) reached — simulation time lost. deltaTime=%.4f",
        mDef.maxSubSteps, deltaTime);
}
#endif
```

The guard condition is `steps >= mDef.maxSubSteps` to distinguish an early exit from normal loop completion.

### Insertion Point 2 — Velocity safety threshold (`IntegrateLinearVelocities()`)

```cpp
// In IntegrateLinearVelocities(), immediately after updating body->mVelocity:
// Runs for both PointBody2D and RigidBody2D pools; Body2DBase::GetId() used for message.
#ifndef NDEBUG
const float speed = body->mVelocity.Magnitude();
if (speed > kMaxSafeVelocity)
{
    DIA_LOG_WARNING("Physics",
        "PhysicsBody '%s': velocity magnitude %.1f exceeds safety threshold — simulation may be diverging",
        body->GetId().GetString(), speed);
}
#endif
```

Only dynamic bodies are integrated; static and kinematic bodies are already skipped by the integration loop, so no additional type guard is needed here.

### Insertion Point 3 — Constraint drift (`SolveConstraints()`)

```cpp
// In SolveConstraints(), after the final solver iteration loop:
#ifndef NDEBUG
for each constraint in mConstraints:
{
    const float error = constraint->GetPositionalError();
    if (error > kMaxConstraintDrift)
    {
        DIA_LOG_WARNING("Physics",
            "IConstraint: positional drift %.3f exceeds threshold after %d iterations",
            error, mDef.solverIterations);
    }
}
#endif
```

`IConstraint::GetPositionalError()` returns the magnitude of the remaining positional separation error after the solver pass. This method is added to `IConstraint` as a non-pure virtual `const` accessor with a default implementation returning `0.0f`. Constraint implementations that cache positional error override it; those that don't will simply never trigger the drift warning. The cached value is initialised to `0.0f` so constraints that have never been solved do not fire false warnings.

### Insertion Point 4 — Sleep state change (`UpdateSleepTimers()` and `Wake()`)

Sleep-to-awake transition (in `Body2DBase::Wake(reason)` — the internal overload with `WakeReason` lives entirely in `PhysicsBody.cpp`, never in a header; public `Wake()` with no parameter calls through with `kExplicit`):

```cpp
// Internal wake-reason enum (PhysicsBody.cpp, not in header):
#ifndef NDEBUG
enum class WakeReason { kImpulse, kForce, kBroadphase, kExplicit, kConstraint };

static const char* WakeReasonToString(WakeReason r)
{
    switch (r) {
        case WakeReason::kImpulse:     return "impulse";
        case WakeReason::kForce:       return "force";
        case WakeReason::kBroadphase:  return "broadphase";
        case WakeReason::kExplicit:    return "explicit";
        case WakeReason::kConstraint:  return "constraint";
    }
    return "unknown";
}
#endif
```

Awake-to-sleeping transition (in `UpdateSleepTimers()`, when `mSleepState` is set to `kSleeping`):

```cpp
#ifndef NDEBUG
DIA_LOG_DEBUG("Physics",
    "PhysicsBody '%s': sleep state -> kSleeping",
    body->GetId().GetString());
#endif
```

Sleeping-to-awake transition (at the top of `PhysicsBody::Wake()`, before resetting state):

```cpp
#ifndef NDEBUG
DIA_LOG_DEBUG("Physics",
    "PhysicsBody '%s': woke (reason: %s)",
    GetId().GetString(), WakeReasonToString(reason));
#endif
```

The `WakeReason` parameter on `Wake()` is an internal-only overload used only within the physics library. The public `Wake()` signature (no parameter) remains unchanged; it calls the internal overload with `WakeReason::kExplicit`.

---

## Dependencies

### Required Features
- **physics-world** — provides `PhysicsWorld::Update()` and `maxSubSteps`
- **force-and-integration** — provides `IntegrateLinearVelocities()` (both pools)
- **constraints-and-joints** — provides `SolveConstraints()` and `IConstraint::GetPositionalError()`
- **body-sleeping** — provides `UpdateSleepTimers()` and `Body2DBase::Wake()`

### Required Modules
- **DiaLogger** — `DIA_LOG_WARNING`, `DIA_LOG_DEBUG` macros; `Physics` channel

### Dependent Features
None. Physics logging is a terminal feature — nothing depends on it.

---

## Testing Strategy

### Unit Tests (`Cluiche/Tests/GoogleTests/RigidBody2D/TestPhysicsLogging.cpp`)

All tests capture log entries by registering a test-only `ISink` implementation that stores `LogEntry` objects into a `DynamicArrayC`. Each test clears the sink before the scenario and asserts on the captured entries afterward.

1. **MaxSubSteps warning** — construct a world with `maxSubSteps = 2`, call `Update(10.0f)` (huge delta); assert one `kWarning` entry on `Physics` channel containing "maxSubSteps".
2. **MaxSubSteps message content** — verify captured message contains the sub-step count and a non-zero deltaTime value.
3. **No maxSubSteps warning on normal delta** — call `Update(1.0f / 60.0f)` with a single body; assert zero `kWarning` entries on `Physics` channel.
4. **Velocity threshold warning** — add a dynamic body, apply a force large enough to exceed 1000 m/s in one step, call `Update(fixedTimestep)`; assert one `kWarning` entry containing the body ID.
5. **Velocity threshold message content** — verify message contains velocity magnitude above 1000.0f.
6. **No velocity warning on normal velocity** — run stable simulation; assert zero velocity threshold warnings.
7. **Constraint drift warning** — construct a distance constraint with anchor points separated far beyond `kMaxConstraintDrift`; run one step; assert at least one `kWarning` containing "positional drift".
8. **Constraint drift message content** — verify message contains drift value and iteration count.
9. **Sleep debug log** — let a body come to rest; run enough steps to cross the sleep threshold; assert one `kDebug` entry containing "kSleeping".
10. **Wake debug log — impulse** — wake the sleeping body via `ApplyImpulse`; assert `kDebug` entry containing "reason: impulse".
11. **Wake debug log — force** — wake via `ApplyForce`; assert "reason: force".
12. **Wake debug log — broadphase** — trigger broadphase wake; assert "reason: broadphase".
13. **Wake debug log — explicit** — call `Wake()` directly; assert "reason: explicit".
14. **Wake debug log — constraint** — attach a constraint to sleeping body, solve; assert "reason: constraint".
15. **Stable scene — zero warnings** — run 120 steps on a world with one sleeping static body; assert zero `kWarning` entries on `Physics` channel.

---

## AI Review Questions

| # | Question | Answer |
|---|----------|--------|
| 1 | `IConstraint::GetPositionalError()` is described as returning the cached error from the last `SolveVelocity()` call. If a constraint has never been solved (e.g. added mid-step), what does it return? | Returns `0.0f` — safe default; no false drift warning fires on the first step before the solver has run. Constraint implementations initialise the cached error to `0.0f`. |
| 2 | The velocity safety threshold (`kMaxSafeVelocity = 1000.0f`) and constraint drift threshold (`kMaxConstraintDrift = 5.0f`) are hardcoded constants. Should these be configurable via `WorldDef`, or are compile-time constants acceptable? | Compile-time constants are acceptable. These are diagnostic safety nets, not tunable simulation parameters. Putting them in `WorldDef` would pollute the public API with debug-only concerns. |
| 3 | The `WakeReason` enum and `WakeReasonToString` are `#ifndef NDEBUG` guarded. The public `Wake()` signature is unchanged. Does the internal overload `Wake(WakeReason)` appear anywhere in a header, or is it entirely hidden in the `.cpp`? | Entirely in the `.cpp`. `WakeReason`, `WakeReasonToString`, and the internal `Wake(WakeReason)` overload are all file-scope inside `PhysicsBody.cpp`. The public `Wake()` header signature is unchanged. |
| 4 | AC9 requires "zero warnings during a stable single-body simulation at rest (after sleep)". A sleeping body's sleep-state transition fires a `DIA_LOG_DEBUG` entry. Does AC9 count debug entries, or only warning-level entries? | `kWarning` only. AC9 asserts zero `kWarning`-level `Physics` channel entries; `kDebug` sleep-state transitions are expected and do not fail the test. |
| 5 | The spec says `GetPositionalError()` must be added to `IConstraint` as a `const` accessor. `IConstraint` is a pure virtual interface — does adding a new pure virtual method break any existing constraint implementations, or is this the first implementation pass? | Default implementation returning `0.0f` — not pure virtual. Future constraint implementations that don't cache positional error won't be forced to implement it, and the drift check will simply never fire for them. |

---

## Binding Decisions Compliance

| Decision | Source | Summary | Compliance |
|----------|--------|---------|------------|
| PD-004 | Platform | No STL in public APIs | ✅ No new public API introduced; internal constants are plain `constexpr float` |
| PD-007 | Platform | C++20 required | ✅ `static constexpr` and scoped enums are C++20-compliant |
| AD-002 | Dia App | No STL in public APIs | ✅ Reinforces PD-004; no STL anywhere |
| AD-003 | Dia App | Namespace `Dia::<Module>::` | ✅ All implementation in `Dia::RigidBody2D::` translation units; no namespace pollution |
| SD-001 | System | Fixed timestep + accumulator | ✅ All four instrumentation points sit inside `StepOnce()` sub-functions — fire at the correct point in the fixed step |
| SD-006 | System | No STL in public APIs | ✅ Reinforced; no STL introduced anywhere |
| SD-007 | System | Sequential impulse constraint solver | ✅ Constraint drift check runs after the SI solver's final iteration — directly monitors the solver's residual error |
| SD-010 | System | Dual threshold sleeping with settle timer | ✅ Sleep/wake debug logs instrument both directions of the `kSleeping` ↔ `kAwake` transition |
| SD-L04 | DiaLogger System | Trace/Debug compile out in Release | ✅ `DIA_LOG_WARNING` and `DIA_LOG_DEBUG` calls guarded by `#ifndef NDEBUG`; compile to `((void)0)` in Release |
