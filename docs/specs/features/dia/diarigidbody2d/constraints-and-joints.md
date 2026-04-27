# Feature Spec: Constraints & Joints

## Traceability

| Level | Parent | This Feature |
|-------|--------|--------------|
| Platform | @docs/specs/platform/Cluiche.md | - |
| Application | @docs/specs/applications/dia.md | - |
| System | @docs/specs/systems/dia/diarigidbody2d.md | **constraints-and-joints** |

**Status:** `Approved`

---

## Problem Statement

Rigid body simulation alone cannot represent connected bodies — hinges, ropes, springs, pinned joints. These are required for the animation pass: ragdolls, chains, doors, pendulums, vehicle suspensions. Constraints must be enforced every step after collision response, without destabilising the simulation.

---

## Solution Overview

A sequential impulse (SI) constraint solver. Each constraint type derives from `IConstraint` and implements `PreStep()` (pre-compute Jacobians and effective mass) and `ApplyImpulse()` (apply corrective velocity impulse). `PhysicsWorld` calls `SolveConstraints()` which runs multiple solver iterations per step (default: 10). More iterations = more accurate but more expensive.

Constraints operate on `RigidBody2D` only. Angular DOF (rotation, inertia) is required for all constraint types in v1. `PointBody2D` cannot be attached to constraints.

Four constraint types in v1: `PinJoint`, `DistanceConstraint`, `SpringConstraint`, `HingeJoint`.

---

## Acceptance Criteria

| ID | Criterion | Verification Method |
|----|-----------|---------------------|
| AC1 | `PinJoint` holds two bodies at a fixed world point — no drift under gravity over 60 steps | Unit test |
| AC2 | `DistanceConstraint` maintains target distance between two anchor points | Unit test: measure distance each step over 60 steps |
| AC3 | `SpringConstraint` oscillates bodies around rest length (Hooke's law) | Unit test: energy-preserving oscillation |
| AC4 | `HingeJoint` allows rotation but prevents relative translation | Unit test: bodies rotate freely; anchor points don't drift |
| AC5 | Constraints on static body work (one-body constraint) | Unit test: pendulum pinned to static body |
| AC6 | Removing a constraint via `PhysicsWorld::RemoveConstraint` stops it being solved | Unit test |
| AC7 | Solver iterations configurable; higher iterations reduce drift | Unit test: compare 1 vs 10 iterations |
| AC8 | Constraints don't destabilise simulation at default iteration count | Unit test: 2-body chain under gravity, 60 steps, no explosion |
| AC9 | Full solution builds clean | `msbuild Cluiche.sln /p:Configuration=Debug /p:Platform=x64` |

---

## Public API

```cpp
namespace Dia::RigidBody2D {

class IConstraint {
public:
    virtual ~IConstraint() = default;
    virtual void PreStep(float dt)      = 0;  // Compute Jacobian, effective mass, bias
    virtual void ApplyImpulse()         = 0;  // Apply corrective velocity impulse
    virtual bool InvolvesBody(const RigidBody2D* body) const = 0;
};

// --- Pin Joint ---
// Holds bodyA's anchor point coincident with bodyB's anchor point (or world point if bodyB is null)
class PinJoint : public IConstraint {
public:
    PinJoint(RigidBody2D* bodyA, const Dia::Maths::Vector2D& anchorA,
             RigidBody2D* bodyB, const Dia::Maths::Vector2D& anchorB);
    // bodyB = nullptr pins bodyA to a world-space point
};

// --- Distance Constraint ---
// Maintains a fixed distance between two anchor points
class DistanceConstraint : public IConstraint {
public:
    DistanceConstraint(RigidBody2D* bodyA, const Dia::Maths::Vector2D& anchorA,
                       RigidBody2D* bodyB, const Dia::Maths::Vector2D& anchorB,
                       float targetDistance);
    void SetTargetDistance(float distance);
    float GetTargetDistance() const;
};

// --- Spring Constraint ---
// Spring force between two anchor points: F = -k * (len - rest) - damping * relVel
class SpringConstraint : public IConstraint {
public:
    SpringConstraint(RigidBody2D* bodyA, const Dia::Maths::Vector2D& anchorA,
                     RigidBody2D* bodyB, const Dia::Maths::Vector2D& anchorB,
                     float restLength, float stiffness, float damping);
    void SetStiffness(float k);
    void SetDamping(float d);
    void SetRestLength(float len);
};

// --- Hinge Joint ---
// Allows relative rotation only; prevents relative translation of anchor points
class HingeJoint : public IConstraint {
public:
    HingeJoint(RigidBody2D* bodyA, const Dia::Maths::Vector2D& anchorA,
               RigidBody2D* bodyB, const Dia::Maths::Vector2D& anchorB);
    // Optional angle limits
    void SetAngleLimits(const Dia::Maths::Angle& min, const Dia::Maths::Angle& max);
    void ClearAngleLimits();
};

// --- Solver (internal to PhysicsWorld::StepOnce; operates on RigidBody2D pool only) ---
struct ConstraintSolverConfig {
    int iterations = 10;  // Solver passes per step
};

void SolveConstraints(
    Dia::Core::DynamicArrayC<IConstraint*>& constraints,
    const ConstraintSolverConfig&           config,
    float                                   dt);

} // namespace Dia::RigidBody2D
```

---

## Implementation Notes

### Sequential Impulse Overview

```
PreStep phase (once per step):
  for each constraint:
    Compute Jacobian J
    Compute effective mass: M_eff = (J * M_inv * J^T)^-1
    Compute position bias (Baumgarte): bias = beta/dt * positionError
    Warm-start: apply cached accumulated impulse from previous step

Solve phase (N iterations):
  for each constraint:
    Compute relative velocity along constraint axis
    Compute lambda = -(J * v + bias) / M_eff
    Clamp lambda (constraint limits)
    Apply impulse: v += M_inv * J^T * lambda
    Accumulate lambda for next frame warm-starting
```

### Warm Starting

Cache the accumulated impulse from the previous step and re-apply it at the start of PreStep. Greatly improves convergence speed — effectively "remembers" what it took to satisfy the constraint last frame.

### Spring as Soft Constraint

Springs are implemented as soft constraints with a compliance term rather than hard position constraints. This avoids stiffness at high spring constants:
```
M_eff_soft = M_eff + compliance/dt²
```
Where `compliance = 1/stiffness`.

### File Layout

```
Dia/DiaRigidBody2D/Constraints/
├── IConstraint.h
├── PinJoint.h/.cpp
├── DistanceConstraint.h/.cpp
├── SpringConstraint.h/.cpp
├── HingeJoint.h/.cpp
└── ConstraintSolver.h/.cpp
```

---

## Dependencies

### Required Features
- **physics-body** — `RigidBody2D`, force/velocity/angular accessors; `IConstraint*` list on `RigidBody2D`
- **physics-world** — owns constraints; calls `SolveConstraints`
- **force-and-integration** — constraints solved after force integration, before velocity integration

### Required Modules
- **DiaMaths** — `Vector2D`, `Angle`, `Matrix22` (for Jacobians)
- **DiaCore** — `DynamicArrayC`, `DIA_ASSERT`

---

## Testing Strategy

### Unit Tests (`Cluiche/Tests/GoogleTests/RigidBody2D/TestConstraints.cpp`)

1. **PinJoint**: Two bodies; pin joint at midpoint; simulate 60 steps under gravity; anchor distance < 0.01 units
2. **DistanceConstraint**: Two bodies; simulate 60 steps; distance stays within 1% of target
3. **SpringConstraint**: Single body attached to static via spring; energy-conserving oscillation (no damping case)
4. **HingeJoint**: Two bodies; relative translation < 0.01; rotation unrestricted
5. **HingeJoint with limits**: Angle stays within specified min/max
6. **Chain**: 5 bodies connected by distance constraints; simulate 60 steps; no explosion
7. **Remove constraint**: Remove mid-simulation; constraint no longer enforced
8. **Iteration count**: 1 vs 10 iterations — 10 shows measurably less drift on pin joint

---

## Binding Decisions Compliance

| Decision | Source | Summary | Compliance |
|----------|--------|---------|------------|
| PD-004 | Platform | No STL in public APIs | ✅ |
| AD-003 | Dia App | Namespace | ✅ `Dia::RigidBody2D::` |
| SD-007 | System | Sequential impulse solver | ✅ Implemented here |
