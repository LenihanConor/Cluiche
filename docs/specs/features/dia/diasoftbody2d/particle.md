# Feature Spec: Particle

## Traceability

| Level | Parent | This Feature |
|-------|--------|--------------|
| Platform | @docs/specs/platform/Cluiche.md | - |
| Application | @docs/specs/applications/dia.md | - |
| System | @docs/specs/systems/dia/diasoftbody2d.md | **particle** |

**Status:** `Approved`

---

## Problem Statement

Position-Based Dynamics requires a fundamental data type that represents a point mass in the simulation. This type must store the current and previous positions needed to derive velocity implicitly (PBD standard), an inverse mass that allows pinned particles to be expressed naturally as invMass = 0, and a radius for use in collision detection. No external velocity storage is needed — velocity is always derived from the position delta each step.

---

## Solution Overview

`Particle` is a plain struct in the `Dia::SoftBody2D::` namespace. It carries four fields: `position`, `prevPosition`, `invMass`, and `radius`. It has no methods — all simulation logic operates on arrays of particles externally (in `Rope`, `Cloth`, and `SoftBodyWorld`). Velocity is a derived quantity only: `v = (position - prevPosition) / dt`. This is the PBD convention and means the solver never accumulates separate velocity state.

A particle with `invMass == 0.0f` is considered pinned (static). The constraint projection and force integration code must check this and skip such particles. The `radius` field is used exclusively by the geometry and rigid body collision resolution steps — it is not involved in constraint projection.

---

## Acceptance Criteria

| ID | Criterion | Verification Method |
|----|-----------|---------------------|
| AC1 | `Particle` is a plain struct with fields `position`, `prevPosition`, `invMass`, `radius` of the correct types | Compile check + unit test |
| AC2 | A particle constructed with `invMass = 0.0f` is considered pinned; applying a simulated force displacement to it (via the same delta logic used in `SoftBodyWorld`) produces zero position change | Unit test |
| AC3 | Derived velocity formula `v = (position - prevPosition) / dt` yields the correct vector for a known displacement over a known timestep | Unit test |
| AC4 | A particle constructed with `invMass > 0.0f` and a non-zero displacement between `position` and `prevPosition` produces a non-zero derived velocity | Unit test |
| AC5 | `Particle` is in the `Dia::SoftBody2D::` namespace | Compile check |
| AC6 | No STL types appear in the struct definition | Code review |
| AC7 | Full solution builds clean | `msbuild Cluiche.sln /p:Configuration=Debug /p:Platform=x64` |

---

## Public API

```cpp
namespace Dia::SoftBody2D {

struct Particle {
    Dia::Maths::Vector2D position;      // Current world-space position
    Dia::Maths::Vector2D prevPosition;  // Position at end of previous step (used for velocity derivation)
    float                invMass;       // Inverse mass: 0.0f = pinned (static); > 0.0f = dynamic
    float                radius;        // Collision radius used by geometry/rigid-body collision steps
};

// Derived velocity helper (free function; not a method on Particle)
// Caller must pass a non-zero dt.
inline Dia::Maths::Vector2D DeriveVelocity(const Particle& p, float dt)
{
    return (p.position - p.prevPosition) * (1.0f / dt);
}

} // namespace Dia::SoftBody2D
```

---

## Implementation Notes

- `Particle` must remain a plain struct (no virtual functions, no constructors beyond aggregate init). It is stored in `DynamicArrayC<Particle>` inside `Rope` and `Cloth`.
- The `DeriveVelocity` helper is provided as a free function to make the formula explicit and testable without duplicating the formula across `SoftBodyWorld`, `Rope`, and `Cloth`.
- `invMass = 0.0f` is the single canonical representation of a pinned particle. No separate boolean is needed. All force integration and constraint projection code guards with `if (p.invMass > 0.0f)`.
- `prevPosition` is set equal to `position` when a particle is first created (zero initial velocity). Pinned particles keep `prevPosition == position` at all times so their derived velocity is always zero — but this is enforced by skipping their update, not by clamping.
- Maximum particle count per body: Rope max 200 particles; Cloth max 64×64 = 4096 particles. These limits are enforced by `DIA_ASSERT` in the body constructors, not here.

---

## Dependencies

### Required Modules
- **DiaMaths** — `Vector2D` (position and velocity)
- **DiaCore** — `DIA_ASSERT` (used in bodies that store particles)

### Dependent Features
- **rope** — stores `DynamicArrayC<Particle>`; reads/writes all four fields
- **cloth** — stores `DynamicArrayC<Particle>`; reads/writes all four fields
- **soft-body-world** — drives `ApplyExternalForces` and `FinalizeVelocities` over particle arrays
- **geometry-collision** — reads `position`, `prevPosition`, `radius`; writes `position`, `prevPosition` for correction
- **rigid-body-coupling** — reads `invMass`, `position`, `prevPosition`, `radius`; writes `position` for correction

---

## Testing Strategy

### Unit Tests (`Cluiche/Tests/GoogleTests/SoftBody2D/TestParticle.cpp`)

1. **Default construction** — struct fields can be set and read back without corruption
2. **Pinned particle** — `invMass = 0.0f`; simulate one integration step using the same delta logic as `SoftBodyWorld::ApplyExternalForces`; position unchanged
3. **Velocity derivation** — set `prevPosition = {0,0}`, `position = {1, 2}`, `dt = 0.5f`; `DeriveVelocity` returns `{2, 4}`
4. **Dynamic particle velocity** — `invMass = 1.0f`, non-zero displacement; `DeriveVelocity` returns non-zero vector
5. **Zero initial velocity** — newly created particle with `prevPosition == position`; `DeriveVelocity` returns `{0, 0}`

---

## Binding Decisions Compliance

| Decision | Source | Summary | Compliance |
|----------|--------|---------|------------|
| PD-001 | Platform | StringCRC for entity/component IDs | Not applicable to `Particle` struct; per-body IDs (Rope/Cloth) handled in their respective features |
| PD-004 | Platform | No STL containers in public APIs | `Particle` uses only `Dia::Maths::Vector2D` and `float`; no STL |
| AD-003 | Dia App | Namespace `Dia::<Module>::` | `Dia::SoftBody2D::` throughout |
| SD-007 | System | Velocity derived from position delta: `v = (pos - prevPos) / dt` | `DeriveVelocity` free function implements this exactly; no stored velocity field |
| SD-008 | System | No STL in public API | No STL in struct or free function |
