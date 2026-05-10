# Plan: DiaRigidBody2D

**Spec:** @docs/specs/systems/dia/diarigidbody2d.md  
**Status:** Done  
**Started:** 2026-04-26  
**Last Updated:** 2026-04-26

## Prerequisites

| Prerequisite | Why needed | Status |
|---|---|---|
| DiaGeometry2D (all 6 features) | All shape types, IntersectionTests, Transform, ISpatialStructure — used throughout | Not Started |
| `Dia::Core::Handle<T>` | Used by injected ISpatialStructure implementations | Not Started (tracked in DiaGeometry2D plan Phase 5) |

DiaRigidBody2D cannot begin until DiaGeometry2D Phase 1–4 are complete (project scaffold + shapes + intersection tests + transform). Spatial structure phases (6–8) must be done before PhysicsWorld can accept an injected broad-phase.

## Implementation Phases

### Phase 1 — Project Scaffold
Establish the DiaRigidBody2D project before any feature code lands.

| # | Task | Status | Notes |
|---|------|--------|-------|
| 1.1 | Create `Dia/DiaRigidBody2D/` directory structure (`Bodies/`, `World/`, `Integration/`, `Detection/`, `Response/`, `Events/`, `Queries/`, `Constraints/`, `Docs/`) | Done | |
| 1.2 | Create `DiaRigidBody2D.vcxproj` (StaticLibrary, Debug\|x64 + Release\|x64, depends on DiaGeometry2D + DiaMaths + DiaCore) | Done | GUID: {C1D2E3F4-A5B6-7890-CDEF-012345678902} |
| 1.3 | Create `DiaRigidBody2D.vcxproj.filters` | Done | |
| 1.4 | Register `DiaRigidBody2D` in `Cluiche/Cluiche.sln` under the Library folder | Done | |
| 1.5 | Create `dia.rigidbody2d.architecture.module.md` in `Docs/` | Done | |
| 1.6 | Verify empty project builds clean | Done | Build succeeded — 0 warnings, 0 errors |

### Phase 2 — PointBody2D + RigidBody2D
Core data types; everything else depends on them.

**Spec:** @docs/specs/features/dia/diarigidbody2d/physics-body.md

| # | Task | Status | Notes |
|---|------|--------|-------|
| 2.1 | Implement `IConstraint` interface in `Constraints/IConstraint.h` (uses `RigidBody2D*`) | Done | |
| 2.2 | Implement `BodyType` enum | Done | Also `SleepState` enum in `BodyType.h` |
| 2.3 | Implement `PointBodyDef` + `PointBody2D` — linear properties, force accumulator, shape pointers | Done | |
| 2.4 | Implement `RigidBodyDef` + `RigidBody2D` — adds angular velocity, invInertia, torque, angular damping, constraint list | Done | |
| 2.5 | Add both to `.vcxproj` + `.filters` | Done | |
| 2.6 | Build clean; write `TestPhysicsBody.cpp` (PointBody2D and RigidBody2D sections) | Done | 18/18 tests pass |

### Phase 3 — PhysicsWorld + Force & Integration
World is the container; integration is the first thing it runs. Both needed before collision detection makes sense.

**Specs:** @docs/specs/features/dia/diarigidbody2d/physics-world.md · @docs/specs/features/dia/diarigidbody2d/force-and-integration.md

| # | Task | Status | Notes |
|---|------|--------|-------|
| 3.1 | Implement `IntegrateLinearForces()`, `IntegrateAngularForces()`, `IntegrateLinearVelocities()`, `IntegrateAngularVelocities()`, `ClearForceAccumulators()` free functions | Done | |
| 3.2 | Implement `MomentOfInertia` helpers (Circle, AARect, Triangle, ConvexPolygon) | Done | |
| 3.3 | Implement `BodyPairKey` and `CollisionPairState` structs | Done | Also added `CollisionEvent`, `Contact`, `RaycastHit`, `WorldDef`, `PhysicsWorldCapacities` |
| 3.4 | Implement `PhysicsWorld` — constructor, `AddPointBody`/`AddRigidBody`/`RemovePointBody`/`RemoveRigidBody`, `Update()` accumulator loop, `StepOnce()` shell | Done | Broad-phase uses Handle<void*> parallel arrays |
| 3.5 | Wire `IntegrateForces` and `IntegrateVelocities` into `StepOnce()` | Done | Detection/Response/Constraints stubbed; wired in later phases |
| 3.6 | Add to `.vcxproj` + `.filters` | Done | |
| 3.7 | Build clean; write `TestForceAndIntegration.cpp` and `TestPhysicsWorld.cpp` (accumulator tests only) | Done | 42/42 tests pass |

### Phase 4 — Collision Detection
Depends on Phase 3 (PhysicsWorld, body list) and DiaGeometry2D IntersectionTests.

**Spec:** @docs/specs/features/dia/diarigidbody2d/collision-detection.md

| # | Task | Status | Notes |
|---|------|--------|-------|
| 4.1 | Define `Contact` struct | Done | |
| 4.2 | Implement `DetectCollisions()` — broad-phase query, narrow-phase dispatch, contact list output | Done | Shape dispatch: Circle or ConvexPolygon; fallback to AARect; `ShouldCollide` + visited HashTable for dedup |
| 4.3 | Wire into `StepOnce()` | Done | |
| 4.4 | Build clean; write `TestCollisionDetection.cpp` | Done | 6/6 tests pass; 48/48 total |

### Phase 5 — Collision Response
Depends on Phase 4 (Contact list must exist).

**Spec:** @docs/specs/features/dia/diarigidbody2d/collision-response.md

| # | Task | Status | Notes |
|---|------|--------|-------|
| 5.1 | Define `ResponseConfig` struct | Done | In ResolveCollisions.h; also added to WorldDef |
| 5.2 | Implement normal impulse calculation (mass ratio, restitution, restitution slop) | Done | |
| 5.3 | Implement friction impulse (Coulomb model) | Done | |
| 5.4 | Implement Baumgarte positional correction | Done | |
| 5.5 | Implement `ResolveCollisions()` free function; wire into `StepOnce()` | Done | |
| 5.6 | Build clean; write `TestCollisionResponse.cpp` | Done | 7/7 tests pass; 55/55 total |

### Phase 6 — Collision Events
Depends on Phase 4 (Contact list). Runs after response in StepOnce.

**Spec:** @docs/specs/features/dia/diarigidbody2d/collision-events.md

| # | Task | Status | Notes |
|---|------|--------|-------|
| 6.1 | Define `CollisionEvent`, `CollisionEventType` | Done | |
| 6.2 | Implement `EmitCollisionEvents()` — diff current contacts vs `mActivePairs` HashTable | Done | Enter/Stay/Exit classification; canonical BodyPairKey ordering |
| 6.3 | Wire `ObserverSubject` into `PhysicsWorld`; expose `GetLastCollisionEvents()` | Done | Stores event list; notifies observers with int(EventType) |
| 6.4 | Wire into `StepOnce()` | Done | |
| 6.5 | Build clean; write `TestCollisionEvents.cpp` | Done | 8/8 tests pass; 63/63 total |

### Phase 7 — Spatial Queries
Depends on Phase 3 (PhysicsWorld) and DiaGeometry2D spatial structures.

**Spec:** @docs/specs/features/dia/diarigidbody2d/spatial-queries.md

| # | Task | Status | Notes |
|---|------|--------|-------|
| 7.1 | Define `RaycastHit` struct | Done | Already present as stub from Phase 1 |
| 7.2 | Implement `Raycast()` — broad QueryRay → narrow-phase confirm → nearest hit | Done | Uses `Dia::Geometry2D::Raycast::CastCircle` / `CastAARect`; nearest by distance |
| 7.3 | Implement `QueryRegion()` and `QueryCircle()` — broad query → narrow confirm → dedup | Done | IntersectionTests narrow confirm; returns `Body2DBase*`; broad-phase deduplicates internally |
| 7.4 | Wire into `PhysicsWorld` | Done | Signature changed to `DynamicArrayC<Body2DBase*>` |
| 7.5 | Build clean; write `TestSpatialQueries.cpp` | Done | 7/7 tests pass; 70/70 total |

### Phase 8 — Body Sleeping
Depends on Phase 3 (integration must skip sleeping bodies) and Phase 4 (broad-phase must wake on overlap). Can be added after Phases 3–6 are working.

**Spec:** @docs/specs/features/dia/diarigidbody2d/body-sleeping.md

| # | Task | Status | Notes |
|---|------|--------|-------|
| 8.1 | Add sleep fields to `Body2DBase`: `mSleepTimer`, `mSleepState`, `mAllowSleeping`; add `Wake()`, `Sleep()`, `IsAwake()` | Done | Already present; added `AllowsSleeping()`, `GetSleepTimer()`, `SetSleepTimer()`, `SetSleepState()` accessors |
| 8.2 | Implement `UpdateSleepTimers()` — overloaded for `PointBody2D` (linear only) and `RigidBody2D` (dual threshold) | Done | Zero velocity on sleep transition |
| 8.3 | Insert sleep skip in `IntegrateLinearForces`, `IntegrateAngularForces`, `IntegrateLinearVelocities`, `IntegrateAngularVelocities` | Done | Already wired from prior phases |
| 8.4 | Broad-phase wake: in `DetectCollisions`, wake sleeping body when paired with an awake body | Done | Added after `ShouldCollide` check |
| 8.5 | Wake on impulse/force: add `Wake()` calls to `ApplyImpulse()` / `ApplyForce()` on both body types | Done | `ApplyImpulse` already had it; added `Wake()` to `ApplyForce()` |
| 8.6 | Wire `UpdateSleepTimers()` into `StepOnce()` — runs last, after `ClearForceAccumulators` | Done | |
| 8.7 | Add sleep threshold fields to `WorldDef` | Done | `sleepLinearThreshold`, `sleepAngularThreshold`, `sleepTimeThreshold` |
| 8.8 | Add to `.vcxproj` + `.filters` | Done | `UpdateSleepTimers.h/.cpp` in Integration filter |
| 8.9 | Build clean; write `TestBodySleeping.cpp` | Done | 10/10 tests pass; 80/80 total |

### Phase 9 — Collision Layers & Masks
Depends on Phase 4 (DetectCollisions must apply `ShouldCollide` filter). Self-contained — can be added any time after Phase 4.

**Spec:** @docs/specs/features/dia/diarigidbody2d/collision-layers.md

| # | Task | Status | Notes |
|---|------|--------|-------|
| 9.1 | Add `mLayer`, `mMask` fields to `Body2DBase`; add `GetLayer()`, `GetMask()`, `SetLayer()`, `SetMask()`; add `layer`/`mask` to both Def structs | Done | All present from Phase 2; power-of-two assert in constructors |
| 9.2 | Implement `ShouldCollide(const Body2DBase&, const Body2DBase&)` free function | Done | Bilateral check; already in DetectCollisions.h |
| 9.3 | Insert `ShouldCollide` filter in `DetectCollisions()` after broad-phase, before narrow-phase | Done | Already wired from Phase 4; order: static-static skip → ShouldCollide → broad-phase wake → dedup → narrow |
| 9.4 | Add `Layers::` namespace constants (`kDefault`, `kPlayer`, etc.) | Done | `kNone`, `kDefault`, `kPlayer`, `kEnemy`, `kProjectile`, `kTrigger`, `kAll` in DetectCollisions.h |
| 9.5 | Build clean; write `TestCollisionLayers.cpp` | Done | 9/9 tests pass; 89/89 total |

### Phase 10 — Constraints & Joints
Depends on Phases 3–6 (bodies and collision response must be working; constraint solver runs after response). Sleeping wake-on-constraint (Phase 8) should be in place first.

**Spec:** @docs/specs/features/dia/diarigidbody2d/constraints-and-joints.md

| # | Task | Status | Notes |
|---|------|--------|-------|
| 10.1 | Implement `IConstraint` interface fully (PreStep, ApplyImpulse, InvolvesBody) | Done | Takes `RigidBody2D*` per spec |
| 10.2 | Implement `SolveConstraints()` — iteration loop, warm-starting; wake sleeping rigid bodies on non-zero impulse | Done | ConstraintSolver.h/.cpp: PreStep pass once, N iterations of ApplyImpulse |
| 10.3 | Implement `PinJoint` | Done | World-pin and body-body; per-axis effMass; kBeta=0.2 |
| 10.4 | Implement `DistanceConstraint` | Done | Scalar axis projection; warm-start |
| 10.5 | Implement `SpringConstraint` (soft constraint with compliance) | Done | compliance=1/stiffness; kSoft=kStructural+compliance/dt² |
| 10.6 | Implement `HingeJoint` with optional angle limits | Done | Translation same as PinJoint; angular limit clamp via relAngle |
| 10.7 | Wire `AddConstraint`/`RemoveConstraint` into `PhysicsWorld`; wire `SolveConstraints` into `StepOnce()` | Done | ConstraintSolverConfig in WorldDef; wired after ResolveCollisions |
| 10.8 | Add all to `.vcxproj` + `.filters` | Done | All 10 constraint files in Constraints filter |
| 10.9 | Build clean; write `TestConstraints.cpp` | Done | 8/8 tests pass; 97/97 total |

## Implementation Patterns

### Project file conventions
- Same rules as DiaGeometry2D — do NOT override OutDir/IntDir/toolchain properties
- All physics free functions (`IntegrateForces`, `DetectCollisions`, etc.) are in their own `.h/.cpp` pairs, not member functions of PhysicsWorld — keeps PhysicsWorld lean and functions independently testable

### StepOnce() call order
```
IntegrateLinearForces(mPointBodies, gravity, dt)   // gravity + forces → linear velocity
IntegrateLinearForces(mRigidBodies, gravity, dt)
IntegrateAngularForces(mRigidBodies, dt)           // torque → angular velocity (rigid only)
UpdateBroadPhase()                                 // sync all body AABBs
DetectCollisions()                                 // broad + narrow → Contact list (both pools)
ResolveCollisions()                                // impulse response (angular terms for rigid only)
SolveConstraints(mRigidBodies, dt)                 // SI constraint solver (rigid only)
IntegrateLinearVelocities(mPointBodies, dt)        // linear velocity → position
IntegrateLinearVelocities(mRigidBodies, dt)
IntegrateAngularVelocities(mRigidBodies, dt)       // angular velocity → rotation (rigid only)
EmitCollisionEvents()                              // enter/stay/exit notifications
ClearForceAccumulators(mPointBodies)
ClearForceAccumulators(mRigidBodies)
```

### Namespace
All code: `namespace Dia::RigidBody2D { ... }`

### Test project location
All new test files go in `Cluiche/Tests/GoogleTests/RigidBody2D/` (new subdirectory).

### Body shape dispatch
Each body has at most one narrow-phase shape. Priority: `circleShape` → `polyShape` → `broadShape` (AARect fallback). A small inline dispatch in `DetectCollisions` handles the 3-case if/else — do not over-engineer.

## Feature Spec Status Summary

| Feature | Spec | Status |
|---------|------|--------|
| PhysicsBody | @docs/specs/features/dia/diarigidbody2d/physics-body.md | Approved |
| PhysicsWorld | @docs/specs/features/dia/diarigidbody2d/physics-world.md | Approved |
| Force & Integration | @docs/specs/features/dia/diarigidbody2d/force-and-integration.md | Approved |
| Collision Detection | @docs/specs/features/dia/diarigidbody2d/collision-detection.md | Approved |
| Collision Response | @docs/specs/features/dia/diarigidbody2d/collision-response.md | Approved |
| Collision Events | @docs/specs/features/dia/diarigidbody2d/collision-events.md | Approved |
| Spatial Queries | @docs/specs/features/dia/diarigidbody2d/spatial-queries.md | Approved |
| Constraints & Joints | @docs/specs/features/dia/diarigidbody2d/constraints-and-joints.md | Approved |
| Body Sleeping | @docs/specs/features/dia/diarigidbody2d/body-sleeping.md | Approved |
| Collision Layers & Masks | @docs/specs/features/dia/diarigidbody2d/collision-layers.md | Approved |
| Physics Logging | @docs/specs/features/dia/diarigidbody2d/physics-logging.md | Approved |
| Visual Debugger | @docs/specs/features/dia/diarigidbody2d/visual-debugger.md | Approved |

## Session Notes

### 2026-04-23
- System spec approved; all 8 feature specs written and approved
- Scope is full rigid body + constraints in v1 (pin, distance, spring, hinge)
- Broad-phase injected via ISpatialStructure (caller chooses Grid/Quadtree/BVH)
- Collision events via DiaCore ObserverSubject (no DiaApplicationFlow dependency)
- Constraint solver: sequential impulses with warm-starting
- StepOnce call order documented above — order is load-bearing (constraints after response, velocities last)

### 2026-04-26 (session 2)
- Phase 10 complete — ConstraintSolver (PreStep+N×ApplyImpulse), PinJoint, DistanceConstraint, SpringConstraint (soft/compliance), HingeJoint (+ angle limits); all wired into WorldDef + StepOnce; 8/8 new tests pass; 97/97 total
- DiaRigidBody2D system DONE — all 10 phases complete; all feature specs implemented

### 2026-04-26
- Phase 9 complete — Layers:: constants (kNone/kDefault/kPlayer/kEnemy/kProjectile/kTrigger/kAll); all layer/mask/ShouldCollide already wired from Phase 2/4; 9/9 new tests pass; 89/89 total
- Phase 8 complete — UpdateSleepTimers (PointBody2D/RigidBody2D overloads), broad-phase wake in DetectCollisions, ApplyForce wake, sleep thresholds in WorldDef; 10/10 new tests pass; 80/80 total
- Phase 7 complete — Raycast (nearest hit), QueryRegion, QueryCircle; signature uses `Body2DBase*` output; 7/7 new tests pass; 70/70 total
- Phase 6 complete — EmitCollisionEvents: Enter/Stay/Exit classification; ObserverSubject notifications; 8/8 new tests pass; 63/63 total
- Phase 5 complete — ResolveCollisions: normal impulse, Coulomb friction, Baumgarte correction; ResponseConfig in WorldDef; 7/7 new tests pass; 55/55 total
- Phase 4 complete — DetectCollisions (broad+narrow), ShouldCollide, analytical contact computation (Circle-Circle, AARect-AARect, Circle-AARect, Poly-Poly SAT); 6/6 new tests pass; 48/48 total
- Phase 3 complete — Integration free functions, MomentOfInertia, PhysicsWorld with accumulator loop; 42/42 tests pass
- Phase 2 complete — PointBody2D, RigidBody2D, Body2DBase, IConstraint, BodyType/SleepState implemented
- 18/18 unit tests pass (TestPhysicsBody.cpp)
- GoogleTests vcxproj wired up with DiaRigidBody2D.lib link dependency and ProjectReference
- Phase 1 complete — project scaffold created and builds clean
- GUID assigned: {C1D2E3F4-A5B6-7890-CDEF-012345678902}
- Dependencies wired: DiaGeometry2D + DiaMaths + DiaCore (ProjectReference + ProjectDependencies)
- Registered under Library folder in Cluiche.sln

### 2026-04-24
- Sleeping and Collision Layers/Masks brought into scope (previously deferred)
- Feature specs added; implementation phases TBD (to be inserted before Phase 8 in plan)
- PhysicsBody split into PointBody2D (translation only) and RigidBody2D (translation + rotation + constraints)
- Driven by both memory/cache concern (4 floats lighter per non-rotating body) and API clarity
- PhysicsWorld now manages two pools (mPointBodies, mRigidBodies); angular integration and constraint solver run on rigid pool only
- Collision response angular terms drop out naturally for PointBody2D via invInertia=0 (no special-casing)
- Revised specs: physics-body.md, diarigidbody2d.md, physics-world.md, force-and-integration.md, collision-response.md, constraints-and-joints.md
- Completed two-type propagation to all remaining specs: collision-detection, collision-events, spatial-queries, body-sleeping, collision-layers, visual-debugger, physics-logging
- Contact struct uses typed pointer pairs (no vtable); CollisionEvent/RaycastHit use Body2DBase* (non-virtual base, no vtable); both decided on cache-miss grounds
- Sleeping and layer/mask apply to both body types (PointBody2D uses linear threshold only for sleep)
- Phases 8 (Body Sleeping) and 9 (Collision Layers) added to plan; old Phase 8 (Constraints) renumbered to Phase 10
- body-sleeping.md and collision-layers.md need approval (Steps 3 + 4)
