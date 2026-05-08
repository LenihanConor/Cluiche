# Feature Spec: Rope

## Traceability

| Level | Parent | This Feature |
|-------|--------|--------------|
| Platform | @docs/specs/platform/Cluiche.md | - |
| Application | @docs/specs/applications/dia.md | - |
| System | @docs/specs/systems/dia/diasoftbody2d.md | **rope** |

**Status:** `Done`

---

## Problem Statement

The simplest deformable object is a 1D chain of particles connected by distance constraints. This is the foundation of the particle + constraint framework — ropes establish the patterns that cloth builds on. Ropes need to support both free-hanging chains and chains pinned to fixed world points or to rigid bodies. They also need optional tearing: when a constraint is stretched beyond a threshold, it is removed permanently, splitting the rope.

---

## Solution Overview

`Rope` is a 1D particle chain constructed from `RopeDef`. At creation, `particleCount` particles are evenly spaced along the line from `startPoint` to `endPoint`. Each adjacent pair of particles is connected by a distance constraint with rest length equal to the initial inter-particle spacing. The total mass is distributed evenly: each particle gets `invMass = particleCount / mass` (or 0 if the endpoint is pinned via `startAnchor`/`endAnchor` pinning logic).

Tearing is opt-in via `maxStretch`. When `maxStretch > 0`, each distance constraint is evaluated in `CheckTearing()`: if the current distance between the two particles exceeds `restLength * (1 + maxStretch)`, that constraint is removed permanently. Once any constraint is removed, `IsTorn()` returns true for the lifetime of the rope.

Rigid body anchors (`startAnchor`, `endAnchor`) are optional non-owning pointers. If set, the corresponding endpoint particle's position is overwritten each step with the anchor body's world position before constraint projection. The coupling impulse back to the rigid body is handled by the **rigid-body-coupling** feature, not here.

---

## Acceptance Criteria

| ID | Criterion | Verification Method |
|----|-----------|---------------------|
| AC1 | `GetParticleCount()` returns the value from `RopeDef::particleCount` | Unit test |
| AC2 | Particles are evenly spaced from `startPoint` to `endPoint` at construction | Unit test: measure inter-particle distances |
| AC3 | First and last particles are at `startPoint` and `endPoint` respectively | Unit test |
| AC4 | Total mass is distributed evenly; `invMass` for each dynamic particle equals `particleCount / mass` | Unit test |
| AC5 | With both endpoints free, gravity causes the rope to sag after several simulation steps | Integration test via `SoftBodyWorld::Update` |
| AC6 | Pinning particle[0] (`invMass = 0.0f`) keeps it stationary under gravity | Integration test |
| AC7 | When `maxStretch = 0.0f`, no constraints are ever removed regardless of stretch | Unit test: stretch a constraint beyond any ratio; `IsTorn()` remains false |
| AC8 | When `maxStretch > 0.0f` and a constraint is stretched beyond `restLength * (1 + maxStretch)`, that constraint is removed in `CheckTearing()` | Unit test |
| AC9 | After any constraint removal, `IsTorn()` returns true | Unit test |
| AC10 | `IsTorn()` never reverts to false once set | Unit test |
| AC11 | `GetId()` returns the `StringCRC` id from `RopeDef` | Unit test |
| AC12 | `RopeDef` with `particleCount > 200` triggers `DIA_ASSERT` in debug | Unit test (debug configuration) |
| AC13 | Full solution builds clean | `msbuild Cluiche.sln /p:Configuration=Debug /p:Platform=x64` |

---

## Public API

```cpp
namespace Dia::SoftBody2D {

struct RopeDef {
    Dia::Core::StringCRC             id;
    Dia::Maths::Vector2D             startPoint;
    Dia::Maths::Vector2D             endPoint;
    int                              particleCount  = 10;    // Max 200 (DIA_ASSERT in debug)
    float                            mass           = 1.0f;  // Total mass; distributed evenly
    float                            stiffness      = 1.0f;  // [0, 1] constraint stiffness
    float                            particleRadius = 0.1f;
    float                            maxStretch     = 0.0f;  // Tear ratio (0 = no tearing)
    // Optional rigid body pin anchors (non-owning; anchors must outlive rope)
    Dia::RigidBody2D::PhysicsBody*   startAnchor    = nullptr;
    Dia::RigidBody2D::PhysicsBody*   endAnchor      = nullptr;
};

class Rope : public SoftBody {
public:
    static constexpr Dia::Core::StringCRC kUniqueId{"Rope"};

    explicit Rope(const RopeDef& def);

    // Particle access
    int              GetParticleCount() const;
    Particle&        GetParticle(int index);
    const Particle&  GetParticle(int index) const;

    // State
    bool IsTorn() const;
    const Dia::Core::StringCRC& GetId() const override;

    // Anchor access (non-owning; may be null)
    Dia::RigidBody2D::PhysicsBody* GetStartAnchor() const;
    Dia::RigidBody2D::PhysicsBody* GetEndAnchor()   const;

    // Stiffness (readable by constraint projection)
    float GetStiffness() const;
    float GetMaxStretch() const;

private:
    Dia::Core::StringCRC                         mId;
    Dia::Core::DynamicArrayC<Particle>           mParticles;
    Dia::Core::DynamicArrayC<DistanceConstraint> mConstraints;  // Adjacent pairs
    bool                                         mIsTorn;
    float                                        mStiffness;
    float                                        mMaxStretch;
    Dia::RigidBody2D::PhysicsBody*               mStartAnchor;
    Dia::RigidBody2D::PhysicsBody*               mEndAnchor;
};

} // namespace Dia::SoftBody2D
```

---

## Implementation Notes

### Construction

```
DIA_ASSERT(def.particleCount >= 2 && def.particleCount <= 200);
float totalLength = length(def.endPoint - def.startPoint);
float segmentLength = totalLength / (def.particleCount - 1);
float perParticleInvMass = (def.mass > 0.0f) ? def.particleCount / def.mass : 0.0f;

for (int i = 0; i < def.particleCount; ++i) {
    float t = (def.particleCount > 1) ? float(i) / float(def.particleCount - 1) : 0.0f;
    Particle p;
    p.position = p.prevPosition = Lerp(def.startPoint, def.endPoint, t);
    p.invMass  = perParticleInvMass;
    p.radius   = def.particleRadius;
    mParticles.PushBack(p);
}
// Create constraints between adjacent pairs
for (int i = 0; i < def.particleCount - 1; ++i) {
    mConstraints.PushBack(DistanceConstraint{ i, i + 1, segmentLength, def.stiffness });
}
mIsTorn = false;
```

If `startAnchor` is set, particle[0]'s `invMass` is overridden to 0 at construction (the anchor drives its position). Same logic applies to the last particle when `endAnchor` is set.

### Distance Constraint (internal struct)

```cpp
struct DistanceConstraint {
    int   indexA;
    int   indexB;
    float restLength;
    float stiffness;   // XPBD compliance alpha = (1 - stiffness) / (dt * dt)
    bool  active;      // false = torn; skipped by projection
};
```

### Constraint Projection (XPBD)

For each active constraint, per solver iteration:
```
delta = particles[b].position - particles[a].position
dist  = length(delta)
if dist == 0: skip
correction = (dist - restLength) / dist
alpha = (1.0f - stiffness) / (dt * dt)
wA = particles[a].invMass;  wB = particles[b].invMass
denom = wA + wB + alpha
if denom == 0: skip
lambda = correction / denom
particles[a].position += wA * lambda * delta / dist
particles[b].position -= wB * lambda * delta / dist
```

Pinned particles (`invMass == 0`) contribute zero displacement — the formula naturally handles this.

### Tearing (CheckTearing)

```
for each active constraint c:
    dist = length(particles[c.indexB].position - particles[c.indexA].position)
    if c.restLength > 0 && maxStretch > 0 && dist > c.restLength * (1 + maxStretch):
        c.active = false
        mIsTorn = true
```

Removed constraints are never re-added. `mIsTorn` is set permanently once any constraint is deactivated.

### Anchor Pinning (driven by SoftBodyWorld before ProjectConstraints)

If `mStartAnchor != nullptr`:
```
mParticles[0].position = mStartAnchor->GetTransform()->GetWorldPosition()
```
If `mEndAnchor != nullptr`:
```
mParticles[back].position = mEndAnchor->GetTransform()->GetWorldPosition()
```
These overwrites happen in `SoftBodyWorld::ApplyExternalForces()` after gravity prediction, ensuring the anchor particle is at the correct world position before constraints are projected. The coupling impulse is applied in `ResolveRigidBodyCollision()`.

### File Layout

```
Dia/DiaSoftBody2D/
├── Rope.h
├── Rope.cpp
└── Constraints/
    └── DistanceConstraint.h
```

---

## Dependencies

### Required Features (must exist first)
- **particle** — `Particle` struct

### Required Modules
- **DiaRigidBody2D** — `PhysicsBody::GetTransform()` for anchor pinning
- **DiaMaths** — `Vector2D`, `length()`, `Lerp()`
- **DiaCore** — `StringCRC`, `DynamicArrayC`, `DIA_ASSERT`

### Dependent Features
- **soft-body-world** — creates and drives `Rope` objects; calls `ProjectConstraints`, `CheckTearing`, anchor overwrites
- **rigid-body-coupling** — reads `GetStartAnchor()`, `GetEndAnchor()` to apply back-impulses

---

## Testing Strategy

### Unit Tests (`Cluiche/Tests/GoogleTests/SoftBody2D/TestRope.cpp`)

1. **Particle count** — `GetParticleCount()` matches `RopeDef::particleCount`
2. **Evenly spaced** — inter-particle distances all equal `totalLength / (count - 1)` at construction
3. **Start/end positions** — `GetParticle(0).position == startPoint`, `GetParticle(last).position == endPoint`
4. **InvMass distribution** — `invMass == particleCount / mass` for all dynamic particles
5. **No tearing when disabled** — `maxStretch = 0`; stretch particles manually; `IsTorn()` remains false
6. **Tearing triggers** — `maxStretch = 0.2`; manually displace particle beyond threshold; call `CheckTearing()`; `IsTorn()` == true
7. **IsTorn permanent** — torn rope never reverts
8. **Pinned endpoint** — `invMass = 0`; `ApplyExternalForces` delta: position unchanged
9. **Gravity sag** — integration test: both endpoints free; 60 steps at gravity; midpoint descends
10. **Assert on excess particles** — `particleCount = 201` triggers DIA_ASSERT in debug

---

## Binding Decisions Compliance

| Decision | Source | Summary | Compliance |
|----------|--------|---------|------------|
| PD-001 | Platform | StringCRC for entity/component IDs | `kUniqueId` static constant + per-instance `mId` from `RopeDef::id` |
| PD-004 | Platform | No STL containers in public APIs | `DynamicArrayC<Particle>`, `DynamicArrayC<DistanceConstraint>` |
| AD-003 | Dia App | Namespace `Dia::<Module>::` | `Dia::SoftBody2D::` throughout |
| SD-001 | System | PBD solver | XPBD distance constraint projection implemented here |
| SD-004 | System | Tearing via `maxStretch` ratio (0 = disabled) | `maxStretch = 0.0f` skips tearing; per-constraint `active` flag tracks removal |
| SD-008 | System | No STL in public API | No STL in `RopeDef`, `Rope`, or internal `DistanceConstraint` |
| SD-009 | System | Rope first, cloth second | This feature establishes the particle + distance-constraint pattern that cloth extends |

## Status

`Done`
