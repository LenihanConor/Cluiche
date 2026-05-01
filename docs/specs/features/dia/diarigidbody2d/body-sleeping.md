# Feature Spec: Body Sleeping

## Traceability

| Level | Parent | This Feature |
|-------|--------|--------------|
| Platform | @docs/specs/platform/Cluiche.md | - |
| Application | @docs/specs/applications/dia.md | - |
| System | @docs/specs/systems/dia/diarigidbody2d.md | **body-sleeping** |

**Status:** `Approved`

---

## Problem Statement

In any active physics scene most bodies quickly come to rest and stop moving. Continuing to integrate, broad-phase query, and run collision detection on them wastes CPU every step. Without sleeping, a pile of 50 crates costs the same as 50 actively bouncing bodies indefinitely.

---

## Solution Overview

Each dynamic body tracks a `sleepTimer` (stored in `Body2DBase`). The threshold test differs by type:

- **`PointBody2D`**: only linear speed is checked — no angular state exists.
- **`RigidBody2D`**: both linear and angular speed must be below their thresholds.

When the timer exceeds `sleepTimeThreshold`, the body is marked `kSleeping`. Sleeping bodies are skipped during force integration, broad-phase update, narrow-phase, response, and constraint solving. A sleeping body wakes when:

- An impulse or force is applied to it directly
- A constraint it is attached to is solved (partner body woke it)
- An active body's AABB overlaps it in the broad-phase
- Game code calls `body->Wake()` explicitly

Static and kinematic bodies never sleep.

---

## Acceptance Criteria

| ID | Criterion | Verification Method |
|----|-----------|---------------------|
| AC1 | Dynamic body below both thresholds for `sleepTimeThreshold` seconds becomes `kSleeping` | Unit test |
| AC2 | Sleeping body position does not change each step | Unit test |
| AC3 | `ApplyImpulse()` on sleeping body wakes it | Unit test |
| AC4 | `ApplyForce()` on sleeping body wakes it | Unit test |
| AC5 | `Wake()` called explicitly wakes the body | Unit test |
| AC6 | Body above linear threshold does not sleep even if angular velocity is zero | Unit test |
| AC7 | `RigidBody2D` above angular threshold does not sleep even if linear velocity is zero | Unit test |
| AC7b | `PointBody2D` below linear threshold sleeps regardless of angular threshold (no angular state) | Unit test |
| AC8 | Sleep timer resets when body exceeds threshold | Unit test: body dips below threshold briefly, does not sleep |
| AC9 | Static body never enters sleep state | Unit test |
| AC10 | Kinematic body never enters sleep state | Unit test |
| AC11 | `allowSleeping = false` in `PointBodyDef` / `RigidBodyDef` prevents body from ever sleeping | Unit test |
| AC12 | Active body overlapping sleeping body in broad-phase wakes the sleeping body | Unit test |
| AC13 | Full solution builds clean | `msbuild Cluiche.sln /p:Configuration=Debug /p:Platform=x64` |

---

## Public API

```cpp
namespace Dia::RigidBody2D {

// Added to WorldDef
struct WorldDef {
    // ... existing fields ...
    float sleepLinearThreshold  = 0.01f;   // m/s — below this, linear speed counts toward sleeping
    float sleepAngularThreshold = 0.01f;   // rad/s
    float sleepTimeThreshold    = 0.5f;    // seconds below both thresholds before sleeping
};

// allowSleeping added to both PointBodyDef and RigidBodyDef (see physics-body spec)
// struct PointBodyDef { ...; bool allowSleeping = true; ... };
// struct RigidBodyDef { ...; bool allowSleeping = true; ... };

// Sleep state and timer live on Body2DBase (shared by both types)
enum class SleepState { kAwake, kSleeping };

// Body2DBase (base of both PointBody2D and RigidBody2D) provides:
//   SleepState GetSleepState() const;
//   bool       IsAwake()       const;
//   void       Wake();          // Force wake; resets sleep timer
//   void       Sleep();         // Force sleep immediately

} // namespace Dia::RigidBody2D
```

---

## Implementation Notes

### Sleep Fields on Body2DBase

```cpp
// In Body2DBase (see physics-body spec):
float      mSleepTimer    = 0.0f;
SleepState mSleepState    = SleepState::kAwake;
bool       mAllowSleeping;
```

### Integration Skip

In `IntegrateLinearForces()`, `IntegrateAngularForces()`, `IntegrateLinearVelocities()`, and `IntegrateAngularVelocities()`, skip bodies where `IsAwake() == false`.

### Sleep Timer Update (end of StepOnce, after ClearForceAccumulators)

```cpp
// PointBody2D: linear threshold only (no angular state)
void UpdateSleepTimers(pointBodies, dt, linearThreshold, sleepTimeThreshold) {
    for each dynamic PointBody2D where allowSleeping:
        if linearSpeed < linearThreshold:
            body.mSleepTimer += dt
            if body.mSleepTimer >= sleepTimeThreshold:
                body.mSleepState = kSleeping
                body.mVelocity = Vector2D::Zero()
        else:
            body.mSleepTimer = 0
            body.mSleepState = kAwake
}

// RigidBody2D: dual threshold (linear AND angular)
void UpdateSleepTimers(rigidBodies, dt, linearThreshold, angularThreshold, sleepTimeThreshold) {
    for each dynamic RigidBody2D where allowSleeping:
        if linearSpeed < linearThreshold AND angularSpeed < angularThreshold:
            body.mSleepTimer += dt
            if body.mSleepTimer >= sleepTimeThreshold:
                body.mSleepState = kSleeping
                body.mVelocity = Vector2D::Zero()
                body.mAngularVelocity = 0
        else:
            body.mSleepTimer = 0
            body.mSleepState = kAwake
}
```

Zeroing velocity on sleep prevents micro-movement accumulating if the body is woken and immediately re-sleeps.

### Broad-Phase Wake

In `DetectCollisions()`, when a broad-phase candidate pair contains one awake and one sleeping body: first check `ShouldCollide()` — if the pair is layer-filtered, skip without waking. Only if the pair passes the layer check, call `sleeping->Wake()` before running narrow-phase.

### Constraint Wake

In `SolveConstraints()`, if `PreStep` computes a non-zero corrective impulse for a sleeping body, call `Wake()` on it.

### StepOnce Insertion Point

`UpdateSleepTimers()` runs at the very end of `StepOnce()`, after `ClearForceAccumulators()`.

---

## Dependencies

### Required Features
- **physics-body** — `Body2DBase` holds `SleepState`, `mSleepTimer`, `mAllowSleeping`, `Wake()`, `Sleep()`; `PointBodyDef`/`RigidBodyDef` have `allowSleeping`
- **physics-world** — adds sleep thresholds to `WorldDef`; calls `UpdateSleepTimers()`
- **force-and-integration** — must skip sleeping bodies
- **collision-detection** — must wake sleeping bodies on broad-phase overlap
- **constraints-and-joints** — must wake sleeping bodies when constraint impulse is non-zero

### Required Modules
- **DiaCore** — `DIA_ASSERT`

---

## Testing Strategy

### Unit Tests (`Cluiche/Tests/GoogleTests/RigidBody2D/TestBodySleeping.cpp`)

1. Body at rest — sleeps after `sleepTimeThreshold` steps
2. Body at rest — position does not change while sleeping
3. `ApplyImpulse` wakes body — `IsAwake()` true, velocity changed
4. `ApplyForce` wakes body — `IsAwake()` true
5. `Wake()` explicit — body immediately awake, sleep timer reset
6. Body oscillates near threshold — timer resets, does not sleep prematurely
7. `allowSleeping = false` — body never sleeps regardless of velocity
8. Static body — never reports `kSleeping`
9. Kinematic body — never reports `kSleeping`
10. Active body overlaps sleeping body — sleeping body wakes

---

## AI Review Questions

| # | Question | Answer |
|---|----------|--------|
| 1 | Two sleeping bodies stacked on each other with no active neighbour — do they stay asleep indefinitely? | Yes. Broad-phase wake only fires when an **awake** body's AABB overlaps a **sleeping** body. Two sleeping bodies do not query each other. Gravity is not applied while sleeping so no drift occurs. Game code calls `Wake()` explicitly if needed. |
| 2 | `ApplyImpulse()` is called on a sleeping body mid-step (from a collision callback). Integration already ran and skipped it. Is a 1-step lag on wake acceptable? | Yes. The impulse is recorded and the body is marked `kAwake`; it starts moving visibly on the next step. At 60 Hz this is sub-frame and imperceptible. Standard behaviour in Box2D and Chipmunk. |
| 3 | Body A (awake) wakes Body B (sleeping) via broad-phase overlap. Body B's AABB overlaps Body C (also sleeping). Does Body B cascade-wake Body C in the same step? | No. Body B wakes but was not in the active set when broad-phase queried this step. C wakes next step if B's AABB still overlaps it. One step delay per sleeping body in a chain. Simpler and predictable. |
| 4 | A sleeping body is woken by broad-phase overlap in step N. Does a `kEnter` CollisionEvent fire for the pair in step N? | Yes. Broad-phase wake occurs inside `DetectCollisions()`, before narrow-phase runs. The body is `kAwake` when the Contact is produced, so `EmitCollisionEvents()` fires `kEnter` in the same step. No special-casing needed. |
| 5 | A sleeping body's transform is moved externally by game code (teleporting). Its AABB in the broad-phase is now stale. What happens? | Caller contract: game code **must** call `Wake()` after moving a sleeping body's transform. Sleeping bodies skip broad-phase sync by design. If `Wake()` is not called, collision detection misses the new position until the body naturally wakes. Document as a contract in the API; add `DIA_ASSERT` in debug if helpful. |
| 6 | Should there be a minimum number of awake bodies before sleeping is evaluated (e.g. skip if only 1 body in world)? | No minimum. Sleeping a single body is valid and useful (e.g. a sleeping particle). `UpdateSleepTimers` is trivially cheap for any pool size. No special-casing. |

---

## Binding Decisions Compliance

| Decision | Source | Summary | Compliance |
|----------|--------|---------|------------|
| PD-004 | Platform | No STL in public APIs | ✅ No STL introduced; `SleepState` enum and `float` fields only |
| PD-007 | Platform | C++20 required | ✅ Scoped enum `SleepState`, `constexpr` thresholds — all C++20 compliant |
| AD-003 | Dia App | Namespace | ✅ `Dia::RigidBody2D::` |
| SD-001 | System | Fixed timestep + accumulator | ✅ `UpdateSleepTimers()` called at end of each `StepOnce()` — integrates with fixed step |
| SD-005 | System | Non-owning Transform pointer | ✅ Sleeping bodies skip all integration; Transform is never touched while `kSleeping` |
| SD-006 | System | No STL in public API | ✅ Reinforces PD-004 |
| SD-009 | System | Static + kinematic body types | ✅ Static and kinematic bodies never enter sleep state — explicitly stated and enforced |
| SD-010 | System | Dual threshold (linear + angular) + settle timer | ✅ Implemented here; `PointBody2D` uses linear threshold only (no angular state) |
