# Feature Spec: Physics Logging (DiaSoftBody2D)

## Traceability

| Level | Parent | This Feature |
|-------|--------|--------------|
| Platform | @docs/specs/platform/Cluiche.md | - |
| Application | @docs/specs/applications/dia.md | - |
| System | @docs/specs/systems/dia/diasoftbody2d.md | **physics-logging** |

**Status:** `Done`

---

## Problem Statement

Soft body simulations can quietly degrade before any visible artefact appears: the accumulator may silently drop simulation time when it cannot keep pace, PBD velocity derivation can produce unreasonably large values when the timestep or forces are badly calibrated, and tearable constraints can be removed without any indication to the developer. Without structured in-simulation diagnostics these conditions are difficult to correlate to their root cause.

---

## Solution Overview

Insert `DIA_LOG_WARNING("Physics", ...)` and `DIA_LOG_DEBUG("Physics", ...)` calls into the relevant internal simulation functions inside `DiaSoftBody2D`, all guarded by `#ifndef NDEBUG`. No new public types or functions are introduced; all additions are strictly internal. The `Physics` channel is shared with `DiaRigidBody2D` — a developer filtering on "Physics" sees warnings from both simulation systems together in chronological order.

Three distinct conditions are instrumented:

1. **MaxSubSteps hit** — emitted in `SoftBodyWorld::Update()` when the accumulator loop exits early because the sub-step budget is exhausted.
2. **Particle velocity safety threshold** — emitted in `FinalizeVelocities()` when a particle's derived velocity `(pos - prevPos) / dt` exceeds a hardcoded safety ceiling.
3. **Constraint torn** — emitted in `CheckTearing()` at `DIA_LOG_DEBUG` level when a constraint is removed due to tearing.

All three conditions fire only in debug builds. In Release (`NDEBUG` defined) every call compiles to `((void)0)` via the `DIA_LOG_WARNING` / `DIA_LOG_DEBUG` macros defined in DiaLogger.

---

## Acceptance Criteria

| ID | Criterion | Verification Method |
|----|-----------|---------------------|
| AC1 | `DIA_LOG_WARNING("Physics", ...)` fires in `SoftBodyWorld::Update()` when the accumulator loop exits at `maxSubSteps` | Unit test: provide extreme `deltaTime` forcing maxSubSteps exhaustion; verify warning captured |
| AC2 | MaxSubSteps warning message contains the sub-step count and `deltaTime` values | Unit test: inspect captured log message text |
| AC3 | `DIA_LOG_WARNING("Physics", ...)` fires in `FinalizeVelocities()` when derived particle velocity exceeds `kMaxSafeParticleVelocity` (500.0f m/s) | Unit test: construct scenario with extreme position delta; verify warning captured |
| AC4 | Particle velocity warning message contains the body ID and particle index and derived velocity magnitude | Unit test: inspect captured log message text |
| AC5 | `DIA_LOG_DEBUG("Physics", ...)` fires in `CheckTearing()` when a constraint is torn | Unit test: construct rope with very low `maxStretch`; run steps until tearing; verify debug entry captured |
| AC6 | Tearing debug message contains the body ID, constraint index, and stretch ratio at time of tearing | Unit test: inspect captured log message text |
| AC7 | No warnings fire during a stable rope simulation at rest | Unit test: run 120 steps with a settled rope under gravity; assert zero `kWarning` entries on `Physics` channel |
| AC8 | All `DIA_LOG_WARNING` and `DIA_LOG_DEBUG` calls compile to `((void)0)` in Release | Build test: build Release; verify via preprocessor that NDEBUG guard removes the calls |
| AC9 | Full solution builds clean in Debug and Release | `msbuild Cluiche.sln /p:Configuration=Debug /p:Platform=x64` and Release equivalent |

---

## Public API

No new public types, methods, or headers. All changes are internal to the implementation files. The two internal constants introduced are:

```cpp
// DiaSoftBody2D/SoftBodyWorld.cpp (internal, not in any header)
#ifndef NDEBUG
static constexpr float kMaxSafeParticleVelocity = 500.0f;  // m/s (derived velocity)
#endif
```

This constant is `static constexpr` at file scope inside the `.cpp` translation unit where it is used. It is not part of any header or public namespace.

---

## Implementation Notes

### Insertion Point 1 — MaxSubSteps hit (`SoftBodyWorld::Update()`)

```cpp
// In SoftBodyWorld::Update(), after the accumulator while-loop exits:
#ifndef NDEBUG
if (steps >= mDef.maxSubSteps)
{
    DIA_LOG_WARNING("Physics",
        "SoftBodyWorld: maxSubSteps (%d) reached — simulation time lost. deltaTime=%.4f",
        mDef.maxSubSteps, deltaTime);
}
#endif
```

The guard condition is `steps >= mDef.maxSubSteps` to distinguish an early exit from normal loop completion. The pattern mirrors the identical instrumentation in `DiaRigidBody2D` (`PhysicsWorld::Update()`) deliberately — same condition, same channel, different prefix string.

### Insertion Point 2 — Particle velocity safety threshold (`FinalizeVelocities()`)

PBD derives velocity from position delta each step. `Particle` has no stored velocity field — velocity is always computed on demand (SD-007). Insert the check inline during `FinalizeVelocities()`:

```cpp
#ifndef NDEBUG
{
    // Derive velocity inline — Particle has no velocity field (SD-007)
    const Dia::Maths::Vector2D derivedVelocity = (particle.position - particle.prevPosition) / dt;
    const float speed = derivedVelocity.Magnitude();
    if (speed > kMaxSafeParticleVelocity)  // > not >= : 500 m/s is the ceiling, not inclusive boundary
    {
        DIA_LOG_WARNING("Physics",
            "SoftBody '%s' particle [%d]: derived velocity %.1f exceeds safety threshold",
            body->GetId().AsChar(), particleIndex, speed);
    }
}
#endif
```

Only dynamic particles (those with `invMass > 0`) have their positions integrated; pinned particles (`invMass == 0`) keep `position == prevPosition` and produce zero derived velocity, so no guard on pinned state is required — the magnitude check is sufficient.

`body->GetId()` refers to the owning `Rope` or `Cloth` object's `StringCRC` identifier, retrieved via the loop context.

### Insertion Point 3 — Constraint torn (`CheckTearing()`)

`CheckTearing()` iterates constraints and removes those whose current stretch ratio exceeds `maxStretch`. Immediately before (or at the point of) constraint removal:

```cpp
#ifndef NDEBUG
{
    const float stretchRatio = currentLength / restLength;
    DIA_LOG_DEBUG("Physics",
        "Rope/Cloth '%s': constraint [%d] torn at stretch ratio %.2f",
        body->GetId().AsChar(), constraintIndex, stretchRatio);
}
#endif
```

`constraintIndex` is the zero-based index of the constraint in the body's internal constraint array at the time of removal. `body->GetId()` returns the `StringCRC` of the owning `Rope` or `Cloth`.

This is emitted at `DIA_LOG_DEBUG` (not `DIA_LOG_WARNING`) because tearing is an expected, designed simulation outcome for tearable bodies — it is informational for debugging, not a symptom of instability.

---

## Dependencies

### Required Features
- **soft-body-world** — provides `SoftBodyWorld::Update()` and `maxSubSteps`
- **particle** — provides `FinalizeVelocities()` and particle data (`position`, `prevPosition`, `invMass`)
- **rope** — provides `CheckTearing()` for rope constraints and `Rope::GetId()`
- **cloth** — provides `CheckTearing()` for cloth constraints and `Cloth::GetId()`

### Required Modules
- **DiaLogger** — `DIA_LOG_WARNING`, `DIA_LOG_DEBUG` macros; `Physics` channel

### Dependent Features
None. Physics logging is a terminal feature — nothing depends on it.

---

## Testing Strategy

### Unit Tests (`Cluiche/Tests/GoogleTests/SoftBody2D/TestSoftBodyLogging.cpp`)

All tests capture log entries by registering a test-only `ISink` implementation that stores `LogEntry` objects into a `DynamicArrayC`. Each test clears the sink before the scenario and asserts on the captured entries afterward.

1. **MaxSubSteps warning** — construct a `SoftBodyWorld` with `maxSubSteps = 2`, call `Update(10.0f)` with a single rope; assert one `kWarning` entry on `Physics` channel containing "SoftBodyWorld".
2. **MaxSubSteps message content** — verify captured message contains the sub-step count and a non-zero deltaTime value.
3. **No maxSubSteps warning on normal delta** — call `Update(1.0f / 60.0f)` with a simple rope; assert zero `kWarning` entries on `Physics` channel.
4. **Particle velocity warning** — construct a rope at rest then manually set `prevPosition` far from `position` for one particle before calling `Update(fixedTimestep)`; assert one `kWarning` containing the body ID and particle index.
5. **Particle velocity message content** — verify message contains velocity magnitude above 500.0f.
6. **No velocity warning on normal simulation** — run stable rope under gravity for 60 steps; assert zero velocity warnings.
7. **Tearing debug log** — construct a rope with `maxStretch = 0.01f` (very low); apply gravity for enough steps to trigger tearing; assert at least one `kDebug` entry containing "torn at stretch ratio".
8. **Tearing message content** — verify message contains the body ID, a non-negative constraint index, and stretch ratio > 0.01f.
9. **No tearing log for non-tearable rope** — construct a rope with `maxStretch = 0.0f`; run 120 steps under heavy gravity; assert zero tearing debug entries.
10. **Stable simulation — zero warnings** — run 120 steps on a world with a settled pinned-top cloth; assert zero `kWarning` entries on `Physics` channel.
11. **Shared channel with RigidBody2D** — register sink filtering on "Physics" channel only; drive both `PhysicsWorld` and `SoftBodyWorld` to hit maxSubSteps; assert exactly two `kWarning` entries (one per system) in the shared sink.

---

## Binding Decisions Compliance

| Decision | Source | Summary | Compliance |
|----------|--------|---------|------------|
| PD-004 | Platform | No STL in public APIs | No new public API introduced; internal constants are plain `constexpr float` |
| PD-007 | Platform | C++20 required | `static constexpr` compliant |
| AD-003 | Dia App | Namespace `Dia::<Module>::` | All implementation in `Dia::SoftBody2D::` translation units; no namespace pollution |
| SD-L04 | DiaLogger System | Trace/Debug compile out in Release | `DIA_LOG_WARNING` and `DIA_LOG_DEBUG` calls guarded by `#ifndef NDEBUG`; compile to `((void)0)` in Release |
| SD-008 | System | No STL in public APIs | Reinforced; no STL introduced anywhere |

---

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Implementation Notes (Insertion Point 2) | `particle.velocity` assignment in original pseudocode was incorrect — `Particle` has no `velocity` field (velocity is always derived per SD-007). How should the magnitude be calculated? | Derive inline: `const Dia::Maths::Vector2D derivedVelocity = (particle.position - particle.prevPosition) / dt; const float speed = derivedVelocity.Magnitude();` Pseudocode corrected above. |
| 2 | Implementation Notes (Insertion Points 2 & 3) | The original pseudocode called `body->GetId().GetString()` — `StringCRC` has no `GetString()` method, only `AsChar()`. Will this compile? | No — corrected throughout to `body->GetId().AsChar()`. |
| 3 | Solution Overview | If a particle exceeds `kMaxSafeParticleVelocity` every frame for 60 consecutive frames, will the warning log 60 times? | Yes — no deduplication or throttling is specified. Acceptable for a debug-only path, but implementers should document this behaviour in code comments. |
| 4 | Public API | `kMaxSafeParticleVelocity = 500.0f` m/s is hardcoded with no justification. How was this value chosen? | 500 m/s is ~4× typical maximum elastic collision velocity; exceeding it indicates mistuned forces or timestep. Add a comment in the `.cpp` explaining the rationale. |
| 5 | Dependencies | Must DiaLogger be explicitly initialized before soft body code can log? | No — `Logger::Instance()` is lazily initialized. Safe to call from any module at any time; no mandatory app-level setup required. |
| 6 | Solution Overview | Is the `Physics` channel already established in DiaRigidBody2D, or is this a new convention? | Any module can emit to a named channel — no registration required. DiaSoftBody2D and DiaRigidBody2D both adopt `Physics` by convention. No setup needed. |
| 7 | Testing Strategy | Does DiaLogger expose a public `ISink` that tests can implement to capture log entries? | Yes — `ISink` is a public abstract base in `<DiaLogger/ISink.h>` with `OnLogEntry()`, `SetChannelFilter()`, and registration via `Logger::RegisterSink()`. Test infrastructure is feasible as specified. |
| 8 | Acceptance Criteria (AC7) | Is 120 frames sufficient to guarantee a stable rope simulation produces zero warnings? | Depends on rope configuration. Test spec should document the exact rope definition (stiffness, iterations, pinned top) to eliminate false negatives from oscillating configurations. |
| 9 | Implementation Notes (Insertion Point 1) | Does `steps >= mDef.maxSubSteps` reliably distinguish early exit from normal loop completion? | Safe only if the loop guard is strictly `steps < mDef.maxSubSteps`. The accumulator loop pseudocode in soft-body-world.md confirms this structure. |
| 10 | Implementation Notes (Insertion Point 2) | Should the threshold check use `>` or `>=`? | `>` is correct — 500.0 m/s is a hard ceiling, not an inclusive boundary. Code comment added: `// > not >= : 500 m/s is the ceiling, not inclusive boundary` |

## Status

`Done`
