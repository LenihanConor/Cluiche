# Feature Spec: Force & Integration

## Traceability

| Level | Parent | This Feature |
|-------|--------|--------------|
| Platform | @docs/specs/platform/Cluiche.md | - |
| Application | @docs/specs/applications/dia.md | - |
| System | @docs/specs/systems/dia/diarigidbody2d.md | **force-and-integration** |

**Status:** `Approved`

---

## Problem Statement

Physics bodies need to move. Each simulation step must apply accumulated forces and gravity to produce new velocities, then apply velocities to update positions and rotations via the body's Transform. The integration scheme must be stable, deterministic, and correct for the fixed-timestep model.

---

## Solution Overview

Semi-implicit Euler integration (also called symplectic Euler): velocity updated from forces first, then position updated from the new velocity. More stable than explicit Euler for oscillatory systems (springs, pendulums). Two sub-phases per step, each split by body type:

1. **IntegrateLinearForces** — apply gravity + force accumulator → update linear velocity; apply linear damping. Accepts both `PointBody2D` and `RigidBody2D` pools.
2. **IntegrateAngularForces** — apply torque accumulator → update angular velocity; apply angular damping. Accepts `RigidBody2D` pool only.
3. **IntegrateLinearVelocities** — apply linear velocity → update Transform position. Accepts both pools.
4. **IntegrateAngularVelocities** — apply angular velocity → update Transform rotation. Accepts `RigidBody2D` pool only.

This feature also provides analytic moment-of-inertia helpers for common shapes.

---

## Acceptance Criteria

| ID | Criterion | Verification Method |
|----|-----------|---------------------|
| AC1 | Static body position unchanged after integration | Unit test |
| AC2 | Kinematic body: velocity applied to position; forces ignored | Unit test |
| AC3 | Dynamic body under gravity: velocity increases by `gravity * dt` per step | Unit test |
| AC4 | Dynamic body with applied force: velocity increases by `force/mass * dt` | Unit test |
| AC5 | Linear damping: velocity decays by `(1 - damping * dt)` per step (both body types) | Unit test |
| AC6 | Angular damping: angular velocity decays correctly (`RigidBody2D` only) | Unit test |
| AC7 | Position updated from velocity after force integration (semi-implicit order; both body types) | Unit test: verify position update uses post-force velocity |
| AC8 | Applied torque on `RigidBody2D` produces angular velocity change | Unit test |
| AC8b | `PointBody2D` has no angular state — no function exists to apply torque | Compile check |
| AC9 | `MomentOfInertia::ForCircle(mass, radius)` returns `0.5 * mass * r²` | Unit test |
| AC10 | `MomentOfInertia::ForAARect(mass, w, h)` returns `mass * (w² + h²) / 12` | Unit test |

---

## Public API

```cpp
namespace Dia::RigidBody2D {

// --- Integration (internal to PhysicsWorld::StepOnce; not called directly by game code) ---
// Free functions; each overloaded for PointBody2D and RigidBody2D pools

// Apply gravity + force accumulator → linear velocity; apply linear damping
void IntegrateLinearForces(Dia::Core::DynamicArrayC<PointBody2D*>& bodies,
                           const Dia::Maths::Vector2D& gravity, float dt);
void IntegrateLinearForces(Dia::Core::DynamicArrayC<RigidBody2D*>& bodies,
                           const Dia::Maths::Vector2D& gravity, float dt);

// Apply torque accumulator → angular velocity; apply angular damping (RigidBody2D only)
void IntegrateAngularForces(Dia::Core::DynamicArrayC<RigidBody2D*>& bodies, float dt);

// Apply linear velocity → Transform position
void IntegrateLinearVelocities(Dia::Core::DynamicArrayC<PointBody2D*>& bodies, float dt);
void IntegrateLinearVelocities(Dia::Core::DynamicArrayC<RigidBody2D*>& bodies, float dt);

// Apply angular velocity → Transform rotation (RigidBody2D only)
void IntegrateAngularVelocities(Dia::Core::DynamicArrayC<RigidBody2D*>& bodies, float dt);

// Zero per-body force accumulators
void ClearForceAccumulators(Dia::Core::DynamicArrayC<PointBody2D*>& bodies);
void ClearForceAccumulators(Dia::Core::DynamicArrayC<RigidBody2D*>& bodies);

// --- Moment of inertia helpers (public utility) ---

struct MomentOfInertia {
    static float ForCircle      (float mass, float radius);
    static float ForAARect      (float mass, float width, float height);
    static float ForTriangle    (float mass, float base, float height);
    static float ForConvexPolygon(float mass,
                                  const Dia::Geometry2D::ConvexPolygon& poly);
};

} // namespace Dia::RigidBody2D
```

---

## Implementation Notes

### Semi-Implicit Euler

```cpp
void IntegrateLinearForces(bodies, gravity, dt) {
    for each dynamic body:
        acceleration = gravity + body.mForceAccum * body.mInvMass
        body.mVelocity += acceleration * dt
        body.mVelocity *= (1.0f - body.mLinearDamping * dt)
}

void IntegrateAngularForces(rigidBodies, dt) {  // RigidBody2D only
    for each dynamic rigid body:
        angularAccel = body.mTorqueAccum * body.mInvInertia
        body.mAngularVelocity += angularAccel * dt
        body.mAngularVelocity *= (1.0f - body.mAngularDamping * dt)
}

void IntegrateLinearVelocities(bodies, dt) {
    for each dynamic and kinematic body:
        pos = body.GetTransform()->GetLocalPosition()
        body.GetTransform()->SetLocalPosition(pos + body.mVelocity * dt)
}

void IntegrateAngularVelocities(rigidBodies, dt) {  // RigidBody2D only
    for each dynamic and kinematic rigid body:
        rot = body.GetTransform()->GetLocalRotation()
        body.GetTransform()->SetLocalRotation(rot + body.mAngularVelocity * dt)
}
```

**Order matters:** force integration runs before velocity integration — position uses the post-force velocity (semi-implicit). Angular integration follows the same order: `IntegrateAngularForces` then `IntegrateAngularVelocities`.

### Damping

Damping is applied as a velocity multiplier rather than a force to avoid instability at large `dt`:
```
velocity *= pow(1 - damping, dt)   // exact but expensive
velocity *= (1 - damping * dt)     // approximation; fine for small damping values
```
Use the approximation; document the limitation (inaccurate for damping > 0.5).

---

## Dependencies

### Required Features
- **physics-body** — `PointBody2D` (force accumulator, Transform) and `RigidBody2D` (adds torque accumulator, angular velocity, inertia)
- **DiaGeometry2D / transform** — `Transform::SetLocalPosition`, `SetLocalRotation`

### Required Modules
- **DiaMaths** — `Vector2D`, `Angle`
- **DiaCore** — `DynamicArrayC`

---

## Testing Strategy

### Unit Tests (`Cluiche/Tests/GoogleTests/RigidBody2D/TestForceAndIntegration.cpp`)

**PointBody2D:**
1. Static PointBody2D — zero velocity, position unchanged
2. Dynamic PointBody2D free-fall for 1 step — velocity = gravity * dt; position = velocity * dt
3. Force on PointBody2D — velocity and position correct after 1 step
4. Linear damping on PointBody2D — velocity after 1 step = v₀ * (1 - damping * dt)
5. Kinematic PointBody2D — velocity applies to position; gravity has no effect

**RigidBody2D:**
6. Torque on RigidBody2D — angular velocity = torque * invInertia * dt after 1 step
7. Angular damping on RigidBody2D — angular velocity decays correctly
8. RigidBody2D rotation integrates position+rotation independently

**Shared:**
9. Moment of inertia helpers — analytic values verified against known formulae

---

## Binding Decisions Compliance

| Decision | Source | Summary | Compliance |
|----------|--------|---------|------------|
| PD-004 | Platform | No STL in public APIs | ✅ |
| AD-003 | Dia App | Namespace | ✅ `Dia::RigidBody2D::` |
| SD-008 | System | Semi-implicit Euler | ✅ Implemented here |
