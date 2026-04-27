# Feature Spec: PhysicsBody

## Traceability

| Level | Parent | This Feature |
|-------|--------|--------------|
| Platform | @docs/specs/platform/Cluiche.md | - |
| Application | @docs/specs/applications/dia.md | - |
| System | @docs/specs/systems/dia/diarigidbody2d.md | **physics-body** |

**Status:** `Approved`

---

## Problem Statement

The physics system needs value types that represent simulated objects in the world. Objects vary in complexity: many bodies (particles, debris, static props) only translate and never rotate, while others (crates, ragdoll limbs) need full rotational dynamics and constraint attachment. Carrying angular velocity, torque, inertia, and angular damping on every body wastes memory and pollutes cache lines for the common lightweight case.

---

## Solution Overview

Two distinct types replace the original single `PhysicsBody`:

- **`PointBody2D`** — translational physics only. No angular state, no torque, no constraints. Suitable for particles, debris, projectiles, and any object where rotation is irrelevant or handled externally.
- **`RigidBody2D`** — full 2D rigid body. Extends PointBody2D's data with angular velocity, inverse inertia, torque accumulator, angular damping, and a constraint list. Required for objects that spin, fall realistically, or participate in joints.

Both types are plain C++ classes (not components, not state objects), constructed via their respective `Def` structs, and owned by `PhysicsWorld`. Game code receives a pointer back from `PhysicsWorld::AddPointBody()` / `AddRigidBody()`. Both hold a non-owning pointer to a `Dia::Geometry2D::Transform` which physics updates each step.

Three body types are supported on both classes: `kDynamic` (forces + impulses), `kStatic` (infinite mass, never moves), `kKinematic` (velocity-driven, not affected by impulses).

---

## Acceptance Criteria

| ID | Criterion | Verification Method |
|----|-----------|---------------------|
| AC1 | `PointBody2D` constructed from `PointBodyDef`; all properties match def values | Unit test |
| AC2 | `RigidBody2D` constructed from `RigidBodyDef`; all properties including angular fields match def values | Unit test |
| AC3 | `PointBody2D::ApplyForce()` accumulates; `ClearForces()` zeroes accumulator | Unit test |
| AC4 | `RigidBody2D::ApplyForceAtPoint()` generates correct torque contribution | Unit test: force applied off-center produces non-zero torque |
| AC5 | `ApplyImpulse()` on `kStatic` body (both types) is no-op (zero inverse mass) | Unit test |
| AC6 | `ApplyImpulse()` on `kKinematic` body (both types) is no-op | Unit test |
| AC7 | `GetInverseMass()` returns 0 for `kStatic` (both types) | Unit test |
| AC8 | `PointBody2D` has no angular velocity, torque, inertia, or constraint fields | Compile check: no such members or methods |
| AC9 | `RigidBody2D` holds `DynamicArrayC<IConstraint*>` (non-owning); `AddConstraint`/`RemoveConstraint` work | Unit test |
| AC10 | Transform pointer set in def accessible via `GetTransform()` (both types) | Unit test |
| AC11 | Both types inherit `Body2DBase`; `Body2DBase*` pointer resolves `GetId`, `GetBodyType`, `GetInverseMass`, `IsAwake`, `GetLayer`, `GetMask` without cast | Unit test |
| AC12 | `allowSleeping = false` prevents sleep on both types | Unit test |
| AC13 | `layer` with more than one bit set triggers `DIA_ASSERT` | Debug assert test |
| AC14 | `SetLayer` / `SetMask` take effect immediately (readable same call) | Unit test |
| AC15 | Full solution builds clean | `msbuild Cluiche.sln /p:Configuration=Debug /p:Platform=x64` |

---

## Public API

```cpp
namespace Dia::RigidBody2D {

enum class BodyType  { kDynamic, kStatic, kKinematic };
enum class SleepState { kAwake, kSleeping };

// -------------------------------------------------------------------------
// Body2DBase — non-virtual base; common fields used by shared structs
// (CollisionEvent, RaycastHit, BodyPairKey).  No vtable.
// -------------------------------------------------------------------------
class Body2DBase {
public:
    BodyType                    GetBodyType()    const;
    float                       GetInverseMass() const;  // 0 for static/kinematic
    float                       GetRestitution() const;
    float                       GetFriction()    const;
    const Dia::Core::StringCRC& GetId()          const;

    // --- Sleeping (both body types; PointBody2D uses linear threshold only) ---
    SleepState GetSleepState() const;
    bool       IsAwake()       const;
    void       Wake();
    void       Sleep();

    // --- Collision layers ---
    uint32_t GetLayer() const;
    uint32_t GetMask()  const;
    void     SetLayer(uint32_t layer);
    void     SetMask(uint32_t mask);

protected:
    Dia::Core::StringCRC mId;
    BodyType             mType;
    float                mMass;
    float                mInvMass;
    float                mRestitution;
    float                mFriction;
    SleepState           mSleepState  = SleepState::kAwake;
    float                mSleepTimer  = 0.0f;
    bool                 mAllowSleeping;
    uint32_t             mLayer;
    uint32_t             mMask;
};

// -------------------------------------------------------------------------
// PointBody2D — translational physics only (no rotation)
// -------------------------------------------------------------------------

struct PointBodyDef {
    Dia::Core::StringCRC              id;
    Dia::Geometry2D::Transform*       transform    = nullptr;  // Non-owning
    const Dia::Geometry2D::AARect*    broadShape   = nullptr;  // Non-owning; for broad-phase
    const Dia::Geometry2D::Circle*        circleShape  = nullptr;  // Non-owning; narrow-phase
    const Dia::Geometry2D::ConvexPolygon* polyShape    = nullptr;  // Non-owning; narrow-phase
    BodyType type          = BodyType::kDynamic;
    float    mass          = 1.0f;
    float    restitution   = 0.2f;
    float    friction      = 0.5f;
    float    linearDamping = 0.0f;
    bool     allowSleeping = true;
    uint32_t layer         = 0x1;
    uint32_t mask          = 0xFFFFFFFF;
};

class PointBody2D : public Body2DBase {
public:
    static constexpr Dia::Core::StringCRC kUniqueId{"PointBody2D"};

    explicit PointBody2D(const PointBodyDef& def);

    // --- Forces & impulses ---
    void ApplyForce(const Dia::Maths::Vector2D& force);
    void ApplyImpulse(const Dia::Maths::Vector2D& impulse);
    void ClearForces();

    // --- Velocity state ---
    void SetVelocity(const Dia::Maths::Vector2D& vel);
    const Dia::Maths::Vector2D& GetVelocity() const;

    // --- Transform (non-owning) ---
    Dia::Geometry2D::Transform*       GetTransform();
    const Dia::Geometry2D::Transform* GetTransform() const;

    // --- Collision shapes (non-owning) ---
    const Dia::Geometry2D::AARect*        GetBroadShape()  const;
    const Dia::Geometry2D::Circle*        GetCircleShape() const;
    const Dia::Geometry2D::ConvexPolygon* GetPolyShape()   const;

private:
    Dia::Geometry2D::Transform*           mTransform;
    float                                 mLinearDamping;
    Dia::Maths::Vector2D                  mVelocity;
    Dia::Maths::Vector2D                  mForceAccum;
    const Dia::Geometry2D::AARect*        mBroadShape;
    const Dia::Geometry2D::Circle*        mCircleShape;
    const Dia::Geometry2D::ConvexPolygon* mPolyShape;
};

// -------------------------------------------------------------------------
// RigidBody2D — full rigid body (translation + rotation + constraints)
// -------------------------------------------------------------------------

class IConstraint;  // Forward declaration (defined in constraints feature)

struct RigidBodyDef {
    Dia::Core::StringCRC              id;
    Dia::Geometry2D::Transform*       transform    = nullptr;  // Non-owning
    const Dia::Geometry2D::AARect*    broadShape   = nullptr;  // Non-owning; for broad-phase
    const Dia::Geometry2D::Circle*        circleShape  = nullptr;  // Non-owning; narrow-phase
    const Dia::Geometry2D::ConvexPolygon* polyShape    = nullptr;  // Non-owning; narrow-phase
    BodyType type             = BodyType::kDynamic;
    float    mass             = 1.0f;
    float    restitution      = 0.2f;
    float    friction         = 0.5f;
    float    linearDamping    = 0.0f;
    float    angularDamping   = 0.0f;
    float    momentOfInertia  = 1.0f;  // Rotational inertia (use MomentOfInertia helpers)
    bool     allowSleeping    = true;
    uint32_t layer            = 0x1;
    uint32_t mask             = 0xFFFFFFFF;
};

class RigidBody2D : public Body2DBase {
public:
    static constexpr Dia::Core::StringCRC kUniqueId{"RigidBody2D"};

    explicit RigidBody2D(const RigidBodyDef& def);

    // --- Forces & impulses ---
    void ApplyForce(const Dia::Maths::Vector2D& force);
    void ApplyForceAtPoint(const Dia::Maths::Vector2D& force,
                           const Dia::Maths::Vector2D& worldPoint);
    void ApplyTorque(float torque);
    void ApplyImpulse(const Dia::Maths::Vector2D& impulse);
    void ApplyAngularImpulse(float impulse);
    void ClearForces();

    // --- Velocity state ---
    void SetVelocity(const Dia::Maths::Vector2D& vel);
    void SetAngularVelocity(float omega);
    const Dia::Maths::Vector2D& GetVelocity()        const;
    float                       GetAngularVelocity() const;

    // --- Additional properties ---
    float GetInverseInertia() const;  // 0 for static/kinematic

    // --- Transform (non-owning) ---
    Dia::Geometry2D::Transform*       GetTransform();
    const Dia::Geometry2D::Transform* GetTransform() const;

    // --- Collision shapes (non-owning) ---
    const Dia::Geometry2D::AARect*        GetBroadShape()  const;
    const Dia::Geometry2D::Circle*        GetCircleShape() const;
    const Dia::Geometry2D::ConvexPolygon* GetPolyShape()   const;

    // --- Constraints (non-owning list; managed by PhysicsWorld) ---
    void AddConstraint(IConstraint* constraint);
    void RemoveConstraint(IConstraint* constraint);
    int  GetConstraintCount() const;
    IConstraint* GetConstraint(int index);

private:
    Dia::Geometry2D::Transform*           mTransform;
    float                                 mLinearDamping;
    float                                 mAngularDamping;
    float                                 mInvInertia;
    Dia::Maths::Vector2D                  mVelocity;
    float                                 mAngularVelocity;
    Dia::Maths::Vector2D                  mForceAccum;
    float                                 mTorqueAccum;
    Dia::Core::DynamicArrayC<IConstraint*> mConstraints;  // Non-owning
    const Dia::Geometry2D::AARect*        mBroadShape;
    const Dia::Geometry2D::Circle*        mCircleShape;
    const Dia::Geometry2D::ConvexPolygon* mPolyShape;
};

} // namespace Dia::RigidBody2D
```

---

## Implementation Notes

- `kStatic` bodies (both types): `mInvMass = 0` (`mInvInertia = 0` for RigidBody2D). All force/impulse methods check `mInvMass > 0` before modifying velocity.
- `kKinematic` bodies (both types): `mInvMass = 0` for impulse purposes, but velocity is writable directly. Physics integration applies velocity to position but skips force accumulation.
- `RigidBody2D::ApplyForceAtPoint` torque contribution: `torque += cross(r, force)` where `r = worldPoint - body.GetTransform()->GetWorldPosition()`.
- `momentOfInertia` in `RigidBodyDef` is approximate (game code provides it based on shape/mass). Helper functions in the integration feature compute it analytically for common shapes.
- `PointBody2D` has no `ApplyForceAtPoint`, `ApplyTorque`, `ApplyAngularImpulse` — these don't exist on the type, preventing misuse at compile time.
- `Body2DBase` has no virtual functions — no vtable. It is a plain data base used only so `CollisionEvent`, `RaycastHit`, and `BodyPairKey` can hold a `Body2DBase*` without needing a cast for common property reads (`GetId`, `GetBodyType`, `GetInverseMass`, `GetLayer`, `GetMask`, `IsAwake`).
- **Sleeping on `PointBody2D`**: only the linear threshold applies; angular threshold is ignored (no angular state). `UpdateSleepTimers` checks `linearSpeed < sleepLinearThreshold` only for `PointBody2D`.
- **Layer / mask**: `layer` must have exactly one bit set — enforced by `DIA_ASSERT((layer & (layer - 1)) == 0)` in the constructor. `mask` may have any bits set.

---

## Dependencies

### Required Features (must exist first)
- **DiaGeometry2D / shape-primitives** — `Transform`, `AARect`, `Circle`, `ConvexPolygon`

### Required Modules
- **DiaCore** — `StringCRC`, `DynamicArrayC`, `DIA_ASSERT`
- **DiaMaths** — `Vector2D`

### Dependent Features
- **physics-world** — owns and manages both body types in separate pools
- **force-and-integration** — reads/writes linear fields on both; reads/writes angular fields on `RigidBody2D` only
- **collision-detection** — reads shape pointers from both types
- **collision-response** — calls `ApplyImpulse()` on both; angular impulse terms only apply to `RigidBody2D`
- **constraints-and-joints** — calls `AddConstraint()` / `RemoveConstraint()` on `RigidBody2D` only

---

## Testing Strategy

### Unit Tests (`Cluiche/Tests/GoogleTests/RigidBody2D/TestPhysicsBody.cpp`)

**PointBody2D:**
1. Construct dynamic PointBody2D — all properties match PointBodyDef
2. Construct static PointBody2D — `GetInverseMass() == 0`
3. `ApplyForce` + `ClearForces` — accumulator pattern
4. `ApplyImpulse` on static — velocity unchanged
5. `ApplyImpulse` on dynamic — velocity changes by `impulse * invMass`

**RigidBody2D:**
6. Construct dynamic RigidBody2D — all properties including angular fields match RigidBodyDef
7. Construct static RigidBody2D — `GetInverseMass() == 0`, `GetInverseInertia() == 0`
8. `ApplyForceAtPoint` off-center — torque non-zero
9. `ApplyAngularImpulse` on dynamic — angular velocity changes
10. `AddConstraint` / `RemoveConstraint` — count correct; no crash on remove non-existent

**Body2DBase (both types):**
11. `Body2DBase*` pointing to `PointBody2D` — `GetId`, `GetBodyType`, `GetInverseMass`, `IsAwake`, `GetLayer`, `GetMask` all resolve correctly
12. `allowSleeping = false` in def — `Sleep()` has no effect; body stays `kAwake`
13. `SetLayer(3)` triggers `DIA_ASSERT` (not a power of two)
14. `SetMask(0)` — `GetMask() == 0`

---

## Binding Decisions Compliance

| Decision | Source | Summary | Compliance |
|----------|--------|---------|------------|
| PD-001 | Platform | StringCRC for IDs | ✅ `kUniqueId` + per-instance `mId` on both types |
| PD-004 | Platform | No STL in public APIs | ✅ `DynamicArrayC<IConstraint*>` |
| AD-003 | Dia App | Namespace | ✅ `Dia::RigidBody2D::` |
| SD-005 | System | Non-owning Transform pointer | ✅ Both types |
| SD-006 | System | No STL in public API | ✅ |
| SD-009 | System | Static + kinematic + dynamic body types | ✅ Both types |
