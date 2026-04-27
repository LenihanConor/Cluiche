# Feature Spec: Collision Response

## Traceability

| Level | Parent | This Feature |
|-------|--------|--------------|
| Platform | @docs/specs/platform/Cluiche.md | - |
| Application | @docs/specs/applications/dia.md | - |
| System | @docs/specs/systems/dia/diarigidbody2d.md | **collision-response** |

**Status:** `Approved`

---

## Problem Statement

When two bodies overlap, their velocities must be corrected so they separate realistically. Without response, bodies pass through each other. The response must account for mass ratios (heavier bodies move less), restitution (bounciness), friction (tangential resistance), and positional correction (prevent sinking).

---

## Solution Overview

Impulse-based collision response. For each contact in the contact list, compute and apply an impulse along the contact normal to separate the bodies. A second tangential impulse applies friction. Baumgarte positional correction bleeds out residual penetration over several frames to prevent sinking without causing jitter.

**Body type dispatch:** Contact pairs may involve any combination of `PointBody2D` and `RigidBody2D`. Angular impulse terms (cross products with contact radius `r`, inertia denominators) only apply when a body is a `RigidBody2D`. For `PointBody2D`, `invInertia = 0` and all angular terms drop out of the impulse formula, leaving pure linear response.

---

## Acceptance Criteria

| ID | Criterion | Verification Method |
|----|-----------|---------------------|
| AC1 | Two equal-mass bodies colliding head-on exchange velocities (restitution=1) | Unit test |
| AC2 | Body hitting static wall bounces back with correct velocity (restitution=1) | Unit test |
| AC3 | Restitution=0: bodies do not bounce; relative velocity at contact = 0 after response | Unit test |
| AC4 | Heavy body barely affected by light body collision (mass ratio) | Unit test: 100:1 mass ratio |
| AC5 | Friction slows tangential sliding velocity | Unit test: body sliding along floor |
| AC6 | Static body not moved by impulse | Unit test |
| AC7 | Kinematic body not moved by impulse; dynamic body receives full impulse | Unit test |
| AC8 | Positional correction reduces penetration depth each step | Unit test: overlapping bodies converge to separation |
| AC9 | Restitution clamped to 0 when relative contact velocity below slop threshold (no micro-bounce) | Unit test: body resting on floor; verify no jitter |
| AC10 | Full solution builds clean | `msbuild Cluiche.sln /p:Configuration=Debug /p:Platform=x64` |

---

## Public API

```cpp
namespace Dia::RigidBody2D {

struct ResponseConfig {
    float baumgarteSlop          = 0.01f;  // Penetration depth below which no correction applied
    float baumgarteFactor        = 0.2f;   // Fraction of penetration corrected per step [0,1]
    float restitutionVelocitySlop = 0.5f;  // Relative velocity below which restitution = 0
};

// Internal to PhysicsWorld::StepOnce
// Contact stores void* body pointers; resolver queries invInertia at dispatch time.
// For PointBody2D contacts invInertia is 0, so angular terms vanish automatically.
void ResolveCollisions(
    const Dia::Core::DynamicArrayC<Contact>& contacts,
    const ResponseConfig&                    config,
    float                                    dt);

} // namespace Dia::RigidBody2D
```

---

## Implementation Notes

### Normal Impulse

```
relativeVelocity = vB - vA  (at contact point)
contactVelocity  = dot(relativeVelocity, normal)

if contactVelocity > 0: bodies already separating — skip

e = min(bodyA.restitution, bodyB.restitution)
if contactVelocity > -restitutionVelocitySlop: e = 0  // resting contact

j = -(1 + e) * contactVelocity
    / (invMassA + invMassB + cross(rA, normal)² * invInertiaA
                           + cross(rB, normal)² * invInertiaB)

bodyA.velocity -= j * invMassA * normal
bodyB.velocity += j * invMassB * normal
// Angular terms only applied for RigidBody2D (invInertia = 0 for PointBody2D → no-op)
bodyA.angularVelocity -= cross(rA, j * normal) * invInertiaA
bodyB.angularVelocity += cross(rB, j * normal) * invInertiaB
```

Where `rA`, `rB` are vectors from body centers to contact point. For `PointBody2D` bodies, `invInertia = 0` so the inertia terms in the denominator and the angular velocity updates both vanish — no special-casing required.

### Friction Impulse

```
tangent = relativeVelocity - dot(relativeVelocity, normal) * normal
tangent.Normalize()

jt = -dot(relativeVelocity, tangent)
     / (invMassA + invMassB + ...)

mu = sqrt(bodyA.friction * bodyB.friction)  // Combined friction coefficient

// Coulomb's law: friction impulse bounded by normal impulse
if abs(jt) < j * mu:
    frictionImpulse = jt * tangent
else:
    frictionImpulse = -j * mu * tangent  // kinetic friction
```

### Baumgarte Positional Correction

```
correction = max(depth - baumgarteSlop, 0) / (invMassA + invMassB)
             * baumgarteFactor * normal

bodyA.transform.position -= invMassA * correction
bodyB.transform.position += invMassB * correction
```

Applied after impulse; corrects position directly, not via velocity (avoids adding energy).

---

## Dependencies

### Required Features
- **collision-detection** — `Contact` list with `ContactResult`
- **physics-body** — mass, inertia, velocity, `ApplyImpulse`

---

## Testing Strategy

### Unit Tests (`Cluiche/Tests/GoogleTests/RigidBody2D/TestCollisionResponse.cpp`)

1. Head-on elastic collision (e=1) — velocity exchange
2. Perfectly inelastic collision (e=0) — bodies move together
3. Body vs static wall (e=0.5) — bounce at half speed
4. 100:1 mass ratio — light body bounces; heavy body barely moves
5. Friction — body sliding along floor decelerates tangentially
6. Resting contact — no micro-bounce after settling (restitution slop)
7. Positional correction — overlapping bodies separate over ~5 steps

---

## Binding Decisions Compliance

| Decision | Source | Summary | Compliance |
|----------|--------|---------|------------|
| PD-004 | Platform | No STL in public APIs | ✅ |
| AD-003 | Dia App | Namespace | ✅ `Dia::RigidBody2D::` |
| SD-003 | System | Impulse-based response | ✅ Implemented here |
