# Feature Spec: Rigid Body Coupling

## Traceability

| Level | Parent | This Feature |
|-------|--------|--------------|
| Platform | @docs/specs/platform/Cluiche.md | - |
| Application | @docs/specs/applications/dia.md | - |
| System | @docs/specs/systems/dia/diasoftbody2d.md | **rigid-body-coupling** |

**Status:** `Done`

---

## Problem Statement

Soft bodies that interact with the rigid body world in one direction only are cosmetic. For connected simulations — cranes with hanging ropes, ragdoll attachments, cloth draped over a rigid body — the interaction must be two-way: rigid bodies must drive soft body endpoint positions, and soft bodies must push back on rigid bodies with appropriate impulses. This requires two distinct coupling mechanisms: anchor pinning (endpoint follows rigid body) and particle-vs-rigid-body collision (particle bounces off or wraps around a moving rigid body shape).

---

## Solution Overview

Rigid body coupling is split into two parts, both implemented in `SoftBodyWorld::ResolveRigidBodyCollision()`. When `WorldDef::rigidBodyWorld == nullptr`, this method is a no-op — the world operates in cosmetic-only mode.

**Part A — Anchor Pinning:**
If a `Rope` has `startAnchor` or `endAnchor` set, the corresponding endpoint particle's position is overwritten each step with the anchor rigid body's world position before constraint projection (this overwrite occurs in `ApplyExternalForces`). After `FinalizeVelocities`, the impulse the rope is exerting on the anchor is derived from the particle's displacement and applied back to the rigid body via `ApplyImpulse()`. Pinned particles (`invMass = 0`) do not apply back-impulses.

**Part B — Particle vs Rigid Body Collision:**
After geometry collision, each dynamic particle is tested against rigid bodies near it. `PhysicsWorld::QueryCircle()` retrieves candidate bodies overlapping the particle's circle. For each candidate, `IntersectionTests` checks for penetration. If penetrating, the particle is pushed out (positional correction), and a back-impulse proportional to the correction is applied to the rigid body via `ApplyImpulse()`. Pinned particles (`invMass = 0`) are skipped entirely.

**Known limitation:** The rigid body step runs before the soft body step this frame (caller's responsibility, documented in `WorldDef`). This introduces one frame of coupling lag between the two worlds, which is imperceptible at 60fps and is the standard approach in game engines.

---

## Acceptance Criteria

| ID | Criterion | Verification Method |
|----|-----------|---------------------|
| AC1 | When `startAnchor` is set on a `Rope`, particle[0]'s position matches the anchor body's world position each step after `ApplyExternalForces` | Unit test |
| AC2 | When `endAnchor` is set on a `Rope`, the last particle's position matches the anchor body's world position | Unit test |
| AC3 | After the anchor overwrite and constraint projection, a back-impulse is computed and applied to the anchor body via `ApplyImpulse()` | Unit test: verify `ApplyImpulse` called with non-zero impulse when rope is under tension |
| AC4 | A pinned particle (`invMass = 0`) at an anchor endpoint does not apply a back-impulse | Unit test: verify `ApplyImpulse` not called (or called with zero impulse) |
| AC5 | A dynamic particle overlapping a rigid body shape is pushed out of the shape; its position after resolution is outside the shape | Unit test |
| AC6 | When a particle is pushed out of a rigid body shape, `ApplyImpulse()` is called on that rigid body with a non-zero impulse directed away from the particle | Unit test |
| AC7 | A particle with `invMass = 0` overlapping a rigid body shape: position is NOT corrected (pinned particles are skipped in Part B) and no back-impulse is applied | Unit test |
| AC8 | When `rigidBodyWorld == nullptr`, `ResolveRigidBodyCollision()` is a no-op; no crash; simulation runs normally | Unit test |
| AC9 | Coupling is skipped entirely when `rigidBodyWorld == nullptr` — neither anchor overwrite back-impulse nor particle-vs-body collision is processed | Unit test |
| AC10 | Full solution builds clean | `msbuild Cluiche.sln /p:Configuration=Debug /p:Platform=x64` |

---

## Public API

This feature adds no new public types. It is implemented as part of `SoftBodyWorld`'s internal step sequence. The coupling is enabled by the presence of `WorldDef::rigidBodyWorld` (non-null):

```cpp
// In WorldDef (see soft-body-world feature spec):
Dia::RigidBody2D::PhysicsWorld* rigidBodyWorld = nullptr;
// Non-owning; optional.
// When non-null, enables anchor coupling and particle-vs-rigid-body collision.
// IMPORTANT: call RigidBodyWorld::Update() BEFORE SoftBodyWorld::Update() each frame.
// One frame of coupling lag is expected and documented.
```

The feature uses these DiaRigidBody2D APIs (must exist):
```cpp
// PhysicsBody (from diarigidbody2d physics-body feature):
void ApplyImpulse(const Dia::Maths::Vector2D& impulse);
Dia::Geometry2D::Transform* GetTransform();
float GetInverseMass() const;

// PhysicsWorld (from diarigidbody2d physics-world feature):
// Query rigid bodies overlapping a circle region:
void QueryCircle(const Dia::Geometry2D::Circle& circle,
                 Dia::Core::DynamicArrayC<Dia::RigidBody2D::PhysicsBody*>& outCandidates) const;
```

If `QueryCircle` does not exist on `PhysicsWorld`, it must be added as part of this feature's implementation work (or a `QueryRegion` with the circle's bounding box is used as a conservative approximation with a narrow-phase circle test).

---

## Implementation Notes

### Part A — Anchor Pinning: Position Overwrite

Occurs in `SoftBodyWorld::ApplyExternalForces()` after gravity prediction:

```cpp
for (Rope* rope : mRopes) {
    if (rope->GetStartAnchor() != nullptr) {
        Particle& p = rope->GetParticle(0);
        p.position = rope->GetStartAnchor()->GetTransform()->GetWorldPosition();
        // prevPosition unchanged — PBD will derive a velocity for this particle,
        // but it will be overwritten again next frame (tracked via anchor)
    }
    if (rope->GetEndAnchor() != nullptr) {
        Particle& p = rope->GetParticle(rope->GetParticleCount() - 1);
        p.position = rope->GetEndAnchor()->GetTransform()->GetWorldPosition();
    }
}
```

Note: Cloth does not support anchors in v1. Anchors are a Rope-only feature.

### Part A — Anchor Pinning: Back-Impulse

Occurs in `SoftBodyWorld::ResolveRigidBodyCollision()` after `FinalizeVelocities`:

```cpp
// Back-impulse from anchor rope endpoint
for (Rope* rope : mRopes) {
    auto applyAnchorImpulse = [&](Particle& p, Dia::RigidBody2D::PhysicsBody* anchor) {
        if (anchor == nullptr) return;
        if (p.invMass == 0.0f) return;  // pinned — no back-impulse
        float particleMass = (p.invMass > 0.0f) ? 1.0f / p.invMass : 0.0f;
        Dia::Maths::Vector2D displacement = p.position - p.prevPosition;
        Dia::Maths::Vector2D impulse = displacement * (particleMass / mFixedTimestep);
        anchor->ApplyImpulse(-impulse);  // Newton's 3rd law
    };
    applyAnchorImpulse(rope->GetParticle(0), rope->GetStartAnchor());
    applyAnchorImpulse(rope->GetParticle(rope->GetParticleCount() - 1), rope->GetEndAnchor());
}
```

### Part B — Particle vs Rigid Body Collision

```cpp
Dia::Core::DynamicArrayC<Dia::RigidBody2D::PhysicsBody*> candidates;
for (each body in mBodies) {
    for (each particle in body) {
        if (particle.invMass == 0.0f) continue;  // skip pinned

        Dia::Geometry2D::Circle particleCircle{ particle.position, particle.radius };
        candidates.Clear();
        mRigidBodyWorld->QueryCircle(particleCircle, candidates);

        for (PhysicsBody* rb : candidates) {
            ContactInfo contact;
            if (IntersectionTests::CircleVsBody(particleCircle, *rb, contact)) {
                // Positional correction: push particle out
                particle.position += contact.normal * contact.penetrationDepth;

                // Velocity correction: zero velocity into rigid body
                Dia::Maths::Vector2D vel = DeriveVelocity(particle, mFixedTimestep);
                float normalVel = Dot(vel, contact.normal);
                if (normalVel < 0.0f) {
                    particle.prevPosition -= contact.normal * (normalVel * mFixedTimestep);
                }

                // Back-impulse on rigid body
                float particleMass = 1.0f / particle.invMass;
                Dia::Maths::Vector2D correction = contact.normal * contact.penetrationDepth;
                Dia::Maths::Vector2D backImpulse = correction * (particleMass / mFixedTimestep);
                rb->ApplyImpulse(backImpulse);
            }
        }
    }
}
```

### Coupling Lag

`RigidBody2D::PhysicsWorld::Update()` runs first. Its body transforms are at frame-N positions when `SoftBodyWorld::Update()` begins. Anchor overwrites and particle-vs-body tests use frame-N rigid body positions. The rigid body receives back-impulses which affect its velocity for frame-N+1. This one-frame lag is acceptable at 60fps and is documented as a known limitation.

### `QueryCircle` Availability

`PhysicsWorld` currently exposes `QueryRegion(AARect, ...)`. If `QueryCircle` is not yet implemented, use `QueryRegion` with the particle circle's bounding box as a conservative broad phase, followed by a narrow-phase circle-vs-body test. Document this workaround in implementation notes if used.

### File Layout

```
Dia/DiaSoftBody2D/
└── SoftBodyWorld.cpp  (ResolveRigidBodyCollision, anchor overwrite in ApplyExternalForces)
```

---

## Dependencies

### Required Features (must exist first)
- **particle** — `Particle` struct, `DeriveVelocity`
- **rope** — `GetStartAnchor()`, `GetEndAnchor()`
- **soft-body-world** — calls this feature's logic in `ApplyExternalForces` and `ResolveRigidBodyCollision`
- **geometry-collision** — runs before this step; positions already geometry-corrected

### Required Modules / Features (DiaRigidBody2D)
- **DiaRigidBody2D / physics-body** — `ApplyImpulse()`, `GetTransform()`, `GetInverseMass()`
- **DiaRigidBody2D / physics-world** — `QueryCircle()` (or `QueryRegion()` as fallback)
- **DiaGeometry2D** — `Circle`, `IntersectionTests::CircleVsBody()`

### Required Modules (DiaCore)
- **DiaCore** — `DynamicArrayC`, `DIA_ASSERT`
- **DiaMaths** — `Vector2D`, `Dot()`

---

## Testing Strategy

### Unit Tests (`Cluiche/Tests/GoogleTests/SoftBody2D/TestRigidBodyCoupling.cpp`)

1. **Anchor position tracking** — static rigid body at known position; rope startAnchor set; after one step, `GetParticle(0).position == anchor world position`
2. **Anchor back-impulse applied** — rope under tension with anchor; verify `ApplyImpulse` called on anchor body with non-zero impulse
3. **Anchor back-impulse direction** — impulse opposes the rope's pull direction (Newton's 3rd law)
4. **Pinned endpoint — no back-impulse** — particle[0] has `invMass=0`; `ApplyImpulse` called with zero or not called
5. **Particle vs rigid body push-out** — dynamic particle inside a static rigid body circle; after coupling step, particle is outside
6. **Back-impulse on rigid body** — rigid body receives non-zero impulse after particle collision
7. **Pinned particle vs rigid body** — `invMass=0` particle overlapping rigid body; no correction, no impulse
8. **Null rigidBodyWorld** — coupling step skipped entirely; no crash; positions unchanged
9. **QueryCircle returns no candidates** — no crash; particle position unchanged

---

## Binding Decisions Compliance

| Decision | Source | Summary | Compliance |
|----------|--------|---------|------------|
| PD-004 | Platform | No STL containers in public APIs | `DynamicArrayC` for candidate body query results |
| AD-003 | Dia App | Namespace `Dia::<Module>::` | `Dia::SoftBody2D::` throughout |
| SD-003 | System | Two-way rigid body coupling | Full two-way coupling implemented: anchor position drive (rigid → soft) and back-impulse (soft → rigid); particle-vs-body collision with back-impulse |
| SD-007 | System | Velocity derived from position delta | Back-impulse derived from `(pos - prevPos) / dt * mass`; `prevPosition` adjusted for velocity cancellation |
| SD-008 | System | No STL in public API | No STL in any coupling path |

## Status

`Done`
