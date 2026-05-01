# Feature Spec: PhysicsWorld

## Traceability

| Level | Parent | This Feature |
|-------|--------|--------------|
| Platform | @docs/specs/platform/Cluiche.md | - |
| Application | @docs/specs/applications/dia.md | - |
| System | @docs/specs/systems/dia/diarigidbody2d.md | **physics-world** |

**Status:** `Approved`

---

## Problem Statement

The physics system needs a top-level simulation container that owns all physics bodies, drives the fixed-timestep step loop, manages the injected broad-phase structure, and coordinates the sub-systems (integration, collision detection, collision response, constraint solver, event emission).

---

## Solution Overview

`PhysicsWorld` is the entry point for all physics simulation. It is constructed with a `WorldDef` specifying gravity, fixed timestep, and a non-owning pointer to a `Dia::Geometry2D::ISpatialStructure<void*>` for broad-phase queries. Game code calls `Update(deltaTime)` each frame; PhysicsWorld uses an accumulator to run as many fixed steps as needed.

PhysicsWorld maintains **two separate body pools**: `mPointBodies` (`DynamicArrayC<PointBody2D*>`) and `mRigidBodies` (`DynamicArrayC<RigidBody2D*>`). Both pools participate in broad-phase sync, collision detection/response, event emission, force clearing, and sleep updates. Only `mRigidBodies` is passed to angular integration and the constraint solver. Collision event subscriptions are via `GetCollisionEvents()` which returns a `Dia::Core::ObserverSubject`.

PhysicsWorld owns all `PointBody2D`, `RigidBody2D`, and `IConstraint` objects.

---

## Acceptance Criteria

| ID | Criterion | Verification Method |
|----|-----------|---------------------|
| AC1 | `PhysicsWorld` constructed from `WorldDef`; gravity and timestep accessible | Unit test |
| AC2 | `AddPointBody()` and `AddRigidBody()` each return a non-null pointer; body retrievable by ID | Unit test |
| AC3 | `RemovePointBody()` / `RemoveRigidBody()` — body no longer in pool; pointer is invalid after removal | Unit test |
| AC4 | `Update(dt)` with dt < fixedTimestep — no step taken | Unit test: verify body position unchanged |
| AC5 | `Update(dt)` with dt = fixedTimestep — exactly one step taken | Unit test |
| AC6 | `Update(dt)` with dt = 3× fixedTimestep — exactly 3 steps taken (up to maxSubSteps) | Unit test |
| AC7 | `Update(dt)` with large spike — capped at maxSubSteps; no spiral of death | Unit test: dt = 10s, maxSubSteps=8, verify exactly 8 steps |
| AC8 | `SetGravity()` takes effect from next `Update()` call | Unit test |
| AC9 | `GetCollisionEvents()` returns an `ObserverSubject` that can be subscribed to | Compile + subscribe test |
| AC10 | Broad-phase structure updated on `AddBody`, `RemoveBody`, `Update` | Unit test: add body, query broad-phase directly |
| AC11 | Full solution builds clean | `msbuild Cluiche.sln /p:Configuration=Debug /p:Platform=x64` |

---

## Public API

```cpp
namespace Dia::RigidBody2D {

struct WorldDef {
    Dia::Maths::Vector2D gravity        = { 0.0f, -9.81f };
    float                fixedTimestep  = 1.0f / 60.0f;
    int                  maxSubSteps    = 8;
    // Non-owning — caller manages lifetime; must outlive PhysicsWorld
    // Stores void* keys for both PointBody2D* and RigidBody2D*
    Dia::Geometry2D::ISpatialStructure<void*>* broadPhase = nullptr;
};

class PhysicsWorld {
public:
    explicit PhysicsWorld(const WorldDef& def);
    ~PhysicsWorld();

    // --- Body management ---
    PointBody2D* AddPointBody(const PointBodyDef& def);
    RigidBody2D* AddRigidBody(const RigidBodyDef& def);
    void         RemovePointBody(PointBody2D* body);
    void         RemoveRigidBody(RigidBody2D* body);
    int          GetPointBodyCount() const;
    int          GetRigidBodyCount() const;

    // --- Constraint management ---
    IConstraint* AddConstraint(IConstraint* constraint);  // Takes ownership
    void         RemoveConstraint(IConstraint* constraint);

    // --- Simulation ---
    void Update(float deltaTime);

    // --- World properties ---
    void                        SetGravity(const Dia::Maths::Vector2D& gravity);
    const Dia::Maths::Vector2D& GetGravity() const;

    // --- Read accessors (for visual debugger and game code) ---
    const Dia::Core::DynamicArrayC<PointBody2D*>& GetPointBodies() const;
    const Dia::Core::DynamicArrayC<RigidBody2D*>& GetRigidBodies() const;
    const Dia::Core::DynamicArrayC<IConstraint*>&  GetConstraints() const;
    const Dia::Core::DynamicArrayC<Contact>&       GetLastContacts() const;

    // --- Collision events ---
    Dia::Core::ObserverSubject<CollisionEvent>& GetCollisionEvents();

    // --- Spatial queries (delegates to broad-phase + narrow-phase) ---
    // Results are void* — callers cast to PointBody2D* or RigidBody2D* as appropriate
    bool Raycast    (const Dia::Geometry2D::Ray&    ray,    RaycastHit& outHit)                              const;
    void QueryRegion(const Dia::Geometry2D::AARect& region, Dia::Core::DynamicArrayC<void*>& outBodies)      const;
    void QueryCircle(const Dia::Geometry2D::Circle& circle, Dia::Core::DynamicArrayC<void*>& outBodies)      const;

private:
    void StepOnce();  // Single fixed-timestep step

    WorldDef                                   mDef;
    float                                      mAccumulator;
    Dia::Core::DynamicArrayC<PointBody2D*>     mPointBodies;  // Owned; translational only
    Dia::Core::DynamicArrayC<RigidBody2D*>     mRigidBodies;  // Owned; full rotation + constraints
    Dia::Core::DynamicArrayC<IConstraint*>     mConstraints;  // Owned
    Dia::Core::ObserverSubject<CollisionEvent> mCollisionEvents;
    Dia::Core::HashTable<BodyPairKey, CollisionPairState> mActivePairs;
};

} // namespace Dia::RigidBody2D
```

---

## Implementation Notes

### Step Loop

```cpp
void PhysicsWorld::Update(float deltaTime) {
    mAccumulator += deltaTime;
    int steps = 0;
    while (mAccumulator >= mDef.fixedTimestep && steps < mDef.maxSubSteps) {
        StepOnce();
        mAccumulator -= mDef.fixedTimestep;
        ++steps;
    }
}

void PhysicsWorld::StepOnce() {
    IntegrateLinearForces(mPointBodies, mGravity, dt);   // gravity + forces → linear velocity
    IntegrateLinearForces(mRigidBodies, mGravity, dt);   // same for rigid bodies
    IntegrateAngularForces(mRigidBodies, dt);            // torque → angular velocity (rigid only)
    UpdateBroadPhase();                                  // Sync all body AABBs into spatial structure
    DetectCollisions();                                  // Broad + narrow → contact list (both pools)
    ResolveCollisions();                                 // Impulse response (angular terms for rigid only)
    SolveConstraints(mRigidBodies, dt);                  // SI constraint solver (rigid only)
    IntegrateLinearVelocities(mPointBodies, dt);         // linear velocity → position
    IntegrateLinearVelocities(mRigidBodies, dt);         // same for rigid bodies
    IntegrateAngularVelocities(mRigidBodies, dt);        // angular velocity → rotation (rigid only)
    EmitCollisionEvents();                               // Enter/stay/exit observer notifications
    ClearForceAccumulators(mPointBodies);                // Zero linear force/torque accumulators
    ClearForceAccumulators(mRigidBodies);
}
```

### Body Ownership

`PhysicsWorld` owns `PointBody2D` and `RigidBody2D` instances in separate pools. `AddPointBody()` / `AddRigidBody()` heap-allocate (via DiaCore allocator) and return a raw pointer for game code convenience. `RemovePointBody()` / `RemoveRigidBody()` free. The returned pointer is only valid while the body is in the world.

### Broad-Phase Sync

Each step, `UpdateBroadPhase()` iterates both `mPointBodies` and `mRigidBodies`, calling `broadPhase->Update(static_cast<void*>(body), ...)` for each dynamic/kinematic body. Static bodies only update on add/remove. The `void*` key is cast back to the appropriate type after a query by checking which pool the pointer belongs to.

---

## Dependencies

### Required Features
- **physics-body** — `PointBody2D`, `PointBodyDef`, `RigidBody2D`, `RigidBodyDef`, `IConstraint`
- **DiaGeometry2D / spatial-grid** — `ISpatialStructure<T>`

### Required Modules
- **DiaCore** — `DynamicArrayC`, `HashTable`, `ObserverSubject`, `StringCRC`

### Dependent Features
- **force-and-integration** — called from `StepOnce()`
- **collision-detection** — called from `StepOnce()`
- **collision-response** — called from `StepOnce()`
- **collision-events** — called from `StepOnce()`
- **constraints-and-joints** — called from `StepOnce()`
- **spatial-queries** — delegates to PhysicsWorld query methods

---

## Testing Strategy

### Unit Tests (`Cluiche/Tests/GoogleTests/RigidBody2D/TestPhysicsWorld.cpp`)

1. Construct world — gravity and timestep correct
2. `AddPointBody` / `RemovePointBody` — count correct; pointer valid/invalid
3. `AddRigidBody` / `RemoveRigidBody` — count correct; pointer valid/invalid
4. `Update` accumulator — step counts verified (see AC4–AC7)
5. Gravity change — `SetGravity` takes effect next step
6. Broad-phase sync — add body; verify `broadPhase->QueryRegion` returns it

---

## Binding Decisions Compliance

| Decision | Source | Summary | Compliance |
|----------|--------|---------|------------|
| PD-004 | Platform | No STL in public APIs | ✅ `DynamicArrayC` throughout |
| AD-003 | Dia App | Namespace | ✅ `Dia::RigidBody2D::` |
| SD-001 | System | Fixed timestep + accumulator | ✅ Implemented here |
| SD-002 | System | Injected broad-phase | ✅ `WorldDef::broadPhase` |
| SD-004 | System | Observer collision events | ✅ `ObserverSubject<CollisionEvent>` |
| SD-006 | System | No STL in public API | ✅ |
