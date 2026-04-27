# Feature Spec: SoftBodyWorld

## Traceability

| Level | Parent | This Feature |
|-------|--------|--------------|
| Platform | @docs/specs/platform/Cluiche.md | - |
| Application | @docs/specs/applications/dia.md | - |
| System | @docs/specs/systems/dia/diasoftbody2d.md | **soft-body-world** |

**Status:** `Approved`

---

## Problem Statement

Soft body objects (ropes, cloth) need a simulation container that drives them at a consistent fixed timestep regardless of the render frame rate. This container must handle body lifetime, gravity, static geometry registration, and the ordered PBD step loop — while remaining decoupled from both DiaApplication scheduling and, optionally, from DiaRigidBody2D when rigid body coupling is not needed.

---

## Solution Overview

`SoftBodyWorld` is the single simulation container for all soft body objects. It is constructed with a `WorldDef` that sets gravity, the fixed timestep, the maximum number of sub-steps per frame, the solver iteration count, and an optional non-owning `Dia::RigidBody2D::PhysicsWorld*` for coupling.

`Update(float deltaTime)` implements an accumulator loop identical in structure to `DiaRigidBody2D::PhysicsWorld::Update()`. Each full tick calls `StepOnce()` in the correct PBD phase order. If the accumulator has less than one full timestep remaining, the world is idle for that frame. If the frame spike would exceed `maxSubSteps` ticks, excess time is discarded (spiral-of-death prevention).

`SoftBodyWorld` owns all `Rope` and `Cloth` objects it creates. Static geometry shapes are registered as non-owning typed pointers; callers are responsible for shape lifetime. When `rigidBodyWorld == nullptr`, all rigid body coupling steps are skipped entirely — the world operates as a pure cosmetic soft body simulation.

**Important:** Callers must invoke `RigidBody2D::PhysicsWorld::Update()` before `SoftBodyWorld::Update()` each frame for correct one-frame coupling behaviour. This requirement is documented in `WorldDef` as a code comment and is not enforced at runtime (no safe detection mechanism exists without coupling the worlds).

---

## Acceptance Criteria

| ID | Criterion | Verification Method |
|----|-----------|---------------------|
| AC1 | Constructing `SoftBodyWorld` with a `WorldDef` stores gravity, fixedTimestep, maxSubSteps, and solverIterations correctly | Unit test |
| AC2 | `Update(dt)` with `dt = fixedTimestep` calls exactly one `StepOnce()` | Unit test: mock/spy step counter |
| AC3 | `Update(dt)` with `dt = 2.5 * fixedTimestep` calls exactly 2 `StepOnce()` calls (floor) | Unit test |
| AC4 | When accumulated time would require more than `maxSubSteps` ticks, exactly `maxSubSteps` ticks are executed and excess time is discarded | Unit test |
| AC5 | `Update(0.0f)` (zero delta) calls no `StepOnce()` | Unit test |
| AC6 | `AddRope()` returns a non-null pointer; `GetBodyCount()` increments | Unit test |
| AC7 | `AddCloth()` returns a non-null pointer; `GetBodyCount()` increments | Unit test |
| AC8 | `RemoveBody()` on a previously added body decrements `GetBodyCount()`; removed body pointer is no longer valid | Unit test |
| AC9 | `SetGravity()` changes gravity; subsequent step applies updated gravity to particles | Unit test |
| AC10 | Stepping an empty world (no bodies) completes without crash or assertion | Unit test |
| AC11 | `AddStaticShape(const AARect*)` / `AddStaticShape(const Circle*)` / `AddStaticShape(const Line*)` register shapes; `RemoveStaticShape(ptr)` deregisters | Unit test |
| AC12 | `rigidBodyWorld == nullptr` — `StepOnce()` skips `ResolveRigidBodyCollision()` entirely; no crash | Unit test |
| AC13 | `StepOnce()` phase order: ApplyExternalForces → ProjectConstraints → ResolveGeometryCollision → ResolveRigidBodyCollision → FinalizeVelocities → CheckTearing | Code review + integration test |
| AC14 | Full solution builds clean | `msbuild Cluiche.sln /p:Configuration=Debug /p:Platform=x64` |

---

## Public API

```cpp
namespace Dia::SoftBody2D {

struct WorldDef {
    Dia::Maths::Vector2D              gravity          = { 0.0f, -9.81f };
    float                             fixedTimestep    = 1.0f / 60.0f;
    int                               maxSubSteps      = 8;
    int                               solverIterations = 10;

    // Non-owning; optional.
    // When non-null, enables particle vs rigid body collision and anchor coupling.
    // IMPORTANT: call RigidBodyWorld::Update() BEFORE SoftBodyWorld::Update() each frame.
    // One frame of coupling lag is expected and acceptable (see rigid-body-coupling feature spec).
    Dia::RigidBody2D::PhysicsWorld*   rigidBodyWorld   = nullptr;
};

// Base class for Rope and Cloth (enables RemoveBody polymorphism)
class SoftBody {
public:
    virtual ~SoftBody() = default;
    virtual const Dia::Core::StringCRC& GetId() const = 0;
};

class SoftBodyWorld {
public:
    explicit SoftBodyWorld(const WorldDef& def);
    ~SoftBodyWorld();

    // --- Body management (world owns returned objects) ---
    Rope*  AddRope(const RopeDef& def);
    Cloth* AddCloth(const ClothDef& def);
    void   RemoveBody(SoftBody* body);
    int    GetBodyCount() const;

    // --- Static geometry collision shapes (non-owning; shapes must outlive world) ---
    void AddStaticShape(const Dia::Geometry2D::AARect* shape);
    void AddStaticShape(const Dia::Geometry2D::Circle* shape);
    void AddStaticShape(const Dia::Geometry2D::Line*   shape);
    // Removes whichever typed list contains this pointer; no-op if not found (DIA_ASSERT in debug)
    void RemoveStaticShape(const void* shapePtr);

    // --- Simulation ---
    // Call once per frame with real elapsed time.
    // Internally steps by fixedTimestep up to maxSubSteps times.
    void Update(float deltaTime);

    // --- World properties ---
    void                        SetGravity(const Dia::Maths::Vector2D& gravity);
    const Dia::Maths::Vector2D& GetGravity() const;

    // --- Body iteration (used by DiaSoftBodyVisualDebugger) ---
    const Dia::Core::DynamicArrayC<SoftBody*>& GetBodies() const;

private:
    // Called once per fixed tick; executes the full PBD phase sequence
    void StepOnce();

    // PBD phase implementations (called from StepOnce in order)
    void ApplyExternalForces();         // gravity → predict new positions
    void ProjectConstraints();          // N iterations of distance/structural/shear/bend constraints
    void ResolveGeometryCollision();    // particle vs registered static shapes
    void ResolveRigidBodyCollision();   // particle vs rigid bodies; back-impulse (skipped if rigidBodyWorld==nullptr)
    void FinalizeVelocities();          // prevPosition = position - velocity*dt (implicit from PBD)
    void CheckTearing();                // remove over-stretched constraints; mark body torn
};

} // namespace Dia::SoftBody2D
```

---

## Implementation Notes

### Accumulator Loop

```
mAccumulator += deltaTime;
int steps = 0;
while (mAccumulator >= mFixedTimestep && steps < mMaxSubSteps) {
    StepOnce();
    mAccumulator -= mFixedTimestep;
    ++steps;
}
```

This is structurally identical to `DiaRigidBody2D::PhysicsWorld::Update()`. The excess accumulator remainder is retained for the next frame, providing a smooth simulation regardless of frame rate variance.

### Body Storage

Bodies are stored as `DynamicArrayC<SoftBody*>` with owning heap allocations. `RemoveBody` performs a linear search by pointer and frees the allocation. A `DIA_ASSERT` fires if the pointer is not found (programming error).

### Static Shape Storage

Three separate `DynamicArrayC` members store typed non-owning pointers:
- `DynamicArrayC<const Dia::Geometry2D::AARect*> mStaticRects`
- `DynamicArrayC<const Dia::Geometry2D::Circle*> mStaticCircles`
- `DynamicArrayC<const Dia::Geometry2D::Line*>   mStaticLines`

`RemoveStaticShape(const void*)` searches all three lists and removes the matching entry. In debug builds, a `DIA_ASSERT` fires if the pointer is not found in any list.

### ApplyExternalForces

For each dynamic particle (`invMass > 0`) in each body:
```
predictedPos = position + velocity * dt + gravity * dt * dt
prevPosition = position
position     = predictedPos
```
Where `velocity = DeriveVelocity(particle, dt)` from the previous step.

### FinalizeVelocities

After constraint projection and collision resolution, `prevPosition` already holds the correct previous position (set during `ApplyExternalForces`). No explicit velocity update is required — `DeriveVelocity` will produce the correct result next step. `FinalizeVelocities` is a no-op phase for bookkeeping clarity; implementations may use it for damping or energy clamping.

### File Layout

```
Dia/DiaSoftBody2D/
├── SoftBodyWorld.h
├── SoftBodyWorld.cpp
├── SoftBody.h           (SoftBody base class)
├── Particle.h
├── Rope.h / Rope.cpp
├── Cloth.h / Cloth.cpp
└── Constraints/
    ├── DistanceConstraint.h/.cpp
    └── ...
```

---

## Dependencies

### Required Features (must exist first)
- **particle** — `Particle` struct; `DeriveVelocity`
- **rope** — `Rope`, `RopeDef`
- **cloth** — `Cloth`, `ClothDef`
- **geometry-collision** — `ResolveGeometryCollision()` implementation
- **rigid-body-coupling** — `ResolveRigidBodyCollision()` implementation

### Required Modules
- **DiaRigidBody2D** — `PhysicsWorld` (non-owning pointer for coupling; may be null)
- **DiaGeometry2D** — `AARect`, `Circle`, `Line`
- **DiaMaths** — `Vector2D`
- **DiaCore** — `DynamicArrayC`, `DIA_ASSERT`

---

## Testing Strategy

### Unit Tests (`Cluiche/Tests/GoogleTests/SoftBody2D/TestSoftBodyWorld.cpp`)

1. **Construct** — `WorldDef` values readable from world after construction
2. **Single step** — `Update(fixedTimestep)` calls exactly one internal step
3. **Fractional step** — `Update(0.5f * fixedTimestep)` calls zero steps; accumulated time retained
4. **Double step** — `Update(2.5f * fixedTimestep)` calls exactly two steps
5. **MaxSubSteps cap** — `Update(100.0f * fixedTimestep)` calls exactly `maxSubSteps` steps
6. **Empty world** — `Update(fixedTimestep)` on world with no bodies: no crash
7. **AddRope** — body count increments; pointer non-null
8. **AddCloth** — body count increments; pointer non-null
9. **RemoveBody** — body count decrements after removal
10. **SetGravity** — particle descends after gravity set; does not descend after SetGravity({0,0})
11. **Static shape lifecycle** — add then remove; no crash on step after removal
12. **Null rigidBodyWorld** — coupling step skipped; simulation completes normally

---

## Binding Decisions Compliance

| Decision | Source | Summary | Compliance |
|----------|--------|---------|------------|
| PD-001 | Platform | StringCRC for entity/component IDs | Body types (`Rope::kUniqueId`, `Cloth::kUniqueId`) use `StringCRC` |
| PD-004 | Platform | No STL containers in public APIs | `DynamicArrayC` for all internal collections; no STL in public interface |
| AD-003 | Dia App | Namespace `Dia::<Module>::` | `Dia::SoftBody2D::` throughout |
| SD-002 | System | SoftBodyWorld is a separate peer world | `SoftBodyWorld` is standalone; `PhysicsWorld*` is optional and injected |
| SD-005 | System | Fixed timestep with accumulator pattern | Accumulator loop with `maxSubSteps` cap |
| SD-008 | System | No STL in public API | No STL in `WorldDef`, `SoftBodyWorld`, or body management API |
