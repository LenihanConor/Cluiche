# System Spec: DiaRigidBody2D

## Parent Application
@docs/specs/applications/dia.md

## Purpose

DiaRigidBody2D is the 2D rigid body physics simulation system for the Dia engine. It provides a fixed-timestep deterministic simulation loop covering velocity integration, force and torque accumulation, collision detection (broad-phase via injected spatial structure + narrow-phase via DiaGeometry2D), and collision response (impulse-based resolution). It emits collision events via an Observer-based event system.

DiaRigidBody2D is a simulation layer — it does not own rendering, entity management, or application scheduling. Those concerns belong to DiaGraphics, DiaApplicationFlow, and game code respectively.

**Two body types:** `PointBody2D` (translational physics only — no angular state, no constraints) and `RigidBody2D` (full rigid body — translation + rotation + constraint attachment). `PhysicsWorld` manages separate pools for each and routes them through appropriate integration steps.

**Includes constraints in v1:** Pin joint, distance constraint, spring, and hinge — required for animation pass. Constraints operate on `RigidBody2D` only.

**Dependency chain:**  
`DiaRigidBody2D → DiaGeometry2D → DiaMaths → DiaCore`

## Responsibilities

- Maintain a `PhysicsWorld` containing separate pools for `PointBody2D` (translational only) and `RigidBody2D` (full rotation + constraints)
- Integrate velocity and position each step using a fixed timestep (semi-implicit Euler); run angular integration only on `RigidBody2D` pool
- Accumulate and apply forces to both body types; accumulate and apply torques to `RigidBody2D` only; clear accumulators after each step
- Detect collisions: broad-phase via caller-injected `ISpatialStructure`, narrow-phase via `Dia::Geometry2D::IntersectionTests`; applies to both body types
- Resolve collisions via impulse-based response; angular impulse terms (cross products with contact radius) apply to `RigidBody2D` only; support restitution and friction per body
- Emit collision enter / stay / exit events via an Observer subject on `PhysicsWorld`; applies to both body types
- Support static bodies (infinite mass, immovable) and dynamic bodies; support kinematic bodies (velocity-driven, not force-driven; not affected by collision response)
- Provide a constraint solver (sequential impulses) supporting pin joints, distance constraints, springs, and hinges; constraints attach only to `RigidBody2D`
- Manage constraint attachment between rigid bodies; enforce constraints each step after collision resolution
- Provide a deterministic fixed-timestep step function; accumulator pattern for variable frame rates
- Support body sleeping: automatic deactivation of idle bodies below configurable velocity and angular velocity thresholds; wake bodies on impulse, constraint activation, or explicit call; opt-out per body via `allowSleeping = false`
- Support collision layers and masks: per-body bitmask pair filtering applied in narrow-phase; bilateral check (both bodies must accept each other's layer); default collides-with-all
- Emit structured debug logs to the `Physics` DiaLogger channel (debug builds only): max-sub-steps hit, body velocity exceeded safety threshold, constraint drift exceeded threshold, sleep state changes
- Provide a `dia.rigidbody2d.architecture.module.md` YAML module documentation file
- Provide a `DiaRigidBody2D.vcxproj` static library project registered in `Cluiche.sln`

## Non-Responsibilities

- Rendering or debug drawing of physics bodies — DiaGraphics
- Entity/component ownership — game code owns entities; PhysicsBody is a value type
- Application scheduling — game code calls `PhysicsWorld::Step()`; integration into ProcessingUnit/Phase is the caller's concern
- Soft body / cloth simulation — out of scope
- 3D physics — future DiaPhysics3D
- Spatial structures — owned by DiaGeometry2D; injected into PhysicsWorld by caller
- Scripting integration — DiaPython's concern

## Public Interfaces

### PointBody2D and RigidBody2D

```cpp
namespace Dia::RigidBody2D {
    enum class BodyType {
        kDynamic,    // Affected by forces and collision response
        kStatic,     // Infinite mass; never moves
        kKinematic   // Velocity-driven; not affected by impulses
    };

    // --- PointBody2D: translational physics only ---
    struct PointBodyDef {
        Dia::Geometry2D::Transform* transform = nullptr;  // Non-owning; owned by caller
        BodyType type = BodyType::kDynamic;
        float mass = 1.0f;
        float restitution = 0.2f;
        float friction = 0.5f;
        float linearDamping = 0.0f;
        const Dia::Geometry2D::AARect* broadShape = nullptr;  // Broad-phase
    };

    class PointBody2D {
    public:
        static constexpr Dia::Core::StringCRC kUniqueId{"PointBody2D"};
        explicit PointBody2D(const PointBodyDef& def);
        void ApplyForce(const Dia::Maths::Vector2D& force);
        void ApplyImpulse(const Dia::Maths::Vector2D& impulse);
        void ClearForces();
        void SetVelocity(const Dia::Maths::Vector2D& vel);
        const Dia::Maths::Vector2D& GetVelocity() const;
        BodyType GetBodyType() const;
        float GetMass() const;
        float GetInverseMass() const;
        float GetRestitution() const;
        float GetFriction() const;
        const Dia::Core::StringCRC& GetId() const;
    };

    // --- RigidBody2D: full rigid body (rotation + constraints) ---
    class IConstraint;
    struct RigidBodyDef {
        Dia::Geometry2D::Transform* transform = nullptr;  // Non-owning; owned by caller
        BodyType type = BodyType::kDynamic;
        float mass = 1.0f;
        float restitution = 0.2f;
        float friction = 0.5f;
        float linearDamping = 0.0f;
        float angularDamping = 0.0f;
        float momentOfInertia = 1.0f;
        const Dia::Geometry2D::AARect* broadShape = nullptr;  // Broad-phase
    };

    class RigidBody2D {
    public:
        static constexpr Dia::Core::StringCRC kUniqueId{"RigidBody2D"};
        explicit RigidBody2D(const RigidBodyDef& def);
        void ApplyForce(const Dia::Maths::Vector2D& force);
        void ApplyForceAtPoint(const Dia::Maths::Vector2D& force,
                               const Dia::Maths::Vector2D& worldPoint);
        void ApplyTorque(float torque);
        void ApplyImpulse(const Dia::Maths::Vector2D& impulse);
        void ApplyAngularImpulse(float impulse);
        void ClearForces();
        void SetVelocity(const Dia::Maths::Vector2D& vel);
        void SetAngularVelocity(float omega);
        const Dia::Maths::Vector2D& GetVelocity() const;
        float GetAngularVelocity() const;
        BodyType GetBodyType() const;
        float GetMass() const;
        float GetInverseMass() const;
        float GetInverseInertia() const;
        float GetRestitution() const;
        float GetFriction() const;
        void AddConstraint(IConstraint* constraint);
        void RemoveConstraint(IConstraint* constraint);
        const Dia::Core::StringCRC& GetId() const;
    };
}
```

### PhysicsWorld

```cpp
namespace Dia::RigidBody2D {
    struct WorldDef {
        Dia::Maths::Vector2D gravity = { 0.0f, -9.81f };
        float fixedTimestep = 1.0f / 60.0f;
        int maxSubSteps = 8;

        // Broad-phase spatial structure (non-owning; caller manages lifetime)
        // Used for both PointBody2D and RigidBody2D; stores body AABBs keyed by pointer
        Dia::Geometry2D::ISpatialStructure<void*>* broadPhase = nullptr;
    };

    class PhysicsWorld {
    public:
        explicit PhysicsWorld(const WorldDef& def);

        // Body management
        PointBody2D* AddPointBody(const PointBodyDef& def);
        RigidBody2D* AddRigidBody(const RigidBodyDef& def);
        void RemovePointBody(PointBody2D* body);
        void RemoveRigidBody(RigidBody2D* body);

        // Simulation
        // Call each frame with the real elapsed time; internally steps by fixedTimestep
        void Update(float deltaTime);

        // Gravity
        void SetGravity(const Dia::Maths::Vector2D& gravity);
        const Dia::Maths::Vector2D& GetGravity() const;

        // Collision event observer
        // Subscribe to receive CollisionEvent notifications
        Dia::Core::ObserverSubject<CollisionEvent>& GetCollisionEvents();

        // Queries (delegates to broad-phase + narrow-phase)
        bool Raycast(const Dia::Geometry2D::Ray& ray,
                     RaycastHit& outHit) const;
        void QueryRegion(const Dia::Geometry2D::AARect& region,
                         Dia::Core::DynamicArrayC<Body2DBase*>& outBodies) const;
        void QueryCircle(const Dia::Geometry2D::Circle& circle,
                         Dia::Core::DynamicArrayC<Body2DBase*>& outBodies) const;
    };
}
```

### Collision Events

```cpp
namespace Dia::RigidBody2D {
    enum class CollisionEventType {
        kEnter,  // First frame two bodies overlap
        kStay,   // Continued overlap
        kExit    // Overlap ended
    };

    struct ContactPoint {
        Dia::Maths::Vector2D position;
        Dia::Maths::Vector2D normal;   // Points from B toward A
        float penetrationDepth;
    };

    struct CollisionEvent {
        CollisionEventType type;
        PhysicsBody* bodyA;
        PhysicsBody* bodyB;
        ContactPoint contact;
    };

    // Usage: subscribe via PhysicsWorld::GetCollisionEvents()
    // Dia::Core::ObserverSubject<CollisionEvent> uses DiaCore Observer pattern
}
```

### ISpatialStructure (in DiaGeometry2D)

```cpp
namespace Dia::Geometry2D {
    // Defined in DiaGeometry2D; DiaRigidBody2D depends on this interface
    // PhysicsWorld uses ISpatialStructure<void*> to store both PointBody2D* and RigidBody2D*
    template<typename T>
    class ISpatialStructure {
    public:
        virtual ~ISpatialStructure() = default;

        virtual void Insert(T object, const AARect& bounds) = 0;
        virtual void Remove(T object) = 0;
        virtual void Update(T object, const AARect& newBounds) = 0;
        virtual void Query(const AARect& region,
                           Dia::Core::DynamicArrayC<T>& outResults) const = 0;
        virtual void Clear() = 0;
    };
}
```

### Fixed Timestep / Accumulator Pattern

```
accum += deltaTime
while accum >= fixedTimestep and steps < maxSubSteps:
    IntegrateForces()
    DetectCollisions()       // broad + narrow phase
    ResolveCollisions()      // impulse-based
    IntegrateVelocities()
    EmitCollisionEvents()
    accum -= fixedTimestep
```

## Features

| Feature | Description | Spec | Status |
|---------|-------------|------|--------|
| PhysicsBody | Two body types: `PointBody2D` (translation only) and `RigidBody2D` (translation + rotation + constraints). Dynamic / static / kinematic modes. | [physics-body.md](../../features/dia/diarigidbody2d/physics-body.md) | Approved |
| PhysicsWorld | Simulation container: fixed-timestep accumulator, gravity, body management, step loop. | [physics-world.md](../../features/dia/diarigidbody2d/physics-world.md) | Approved |
| Force & Integration | Force/torque accumulation, semi-implicit Euler integration of velocity and position. Linear and angular damping. | [force-and-integration.md](../../features/dia/diarigidbody2d/force-and-integration.md) | Approved |
| Collision Detection | Broad-phase via injected ISpatialStructure; narrow-phase via Dia::Geometry2D::IntersectionTests. Contact point and normal generation. | [collision-detection.md](../../features/dia/diarigidbody2d/collision-detection.md) | Approved |
| Collision Response | Impulse-based resolution with restitution and friction. Positional correction for penetration. | [collision-response.md](../../features/dia/diarigidbody2d/collision-response.md) | Approved |
| Collision Events | Observer-based enter/stay/exit events emitted from PhysicsWorld after each step. | [collision-events.md](../../features/dia/diarigidbody2d/collision-events.md) | Approved |
| Spatial Queries | Raycast and region query delegating to injected broad-phase structure. | [spatial-queries.md](../../features/dia/diarigidbody2d/spatial-queries.md) | Approved |
| Constraints & Joints | Pin joint, distance constraint, spring, hinge. Sequential impulse solver. Required for animation pass. | [constraints-and-joints.md](../../features/dia/diarigidbody2d/constraints-and-joints.md) | Approved |
| Body Sleeping | Automatic deactivation of idle bodies below velocity/angular-velocity thresholds. Wake on impulse, constraint, or explicit call. Per-body opt-out. | [body-sleeping.md](../../features/dia/diarigidbody2d/body-sleeping.md) | Approved |
| Collision Layers & Masks | Per-body layer + mask bitmask pair. Bilateral filtering in narrow-phase. Default collides-with-all. | [collision-layers.md](../../features/dia/diarigidbody2d/collision-layers.md) | Approved |
| Physics Logging | Structured `Physics` channel DiaLogger warnings: maxSubSteps hit, velocity safety threshold exceeded, constraint drift, sleep state changes. Debug builds only. | [physics-logging.md](../../features/dia/diarigidbody2d/physics-logging.md) | Approved |
| ~~Visual Debugger~~ | Promoted to its own system: [DiaRigidBody2DVisualDebugger](diarigidbody2dvisualdebugger.md). Separate `.vcxproj` with DiaGraphics dependency — not a feature of this system. | — | Moved |

## Dependencies on Other Systems

**Required:**
- **DiaGeometry2D** — All shape primitives, IntersectionTests, Transform, ISpatialStructure interface
- **DiaMaths** — Vector2D, Angle, Matrix33 (via DiaGeometry2D)
- **DiaCore** — StringCRC, DynamicArrayC, Observer/ObserverSubject, DIA_ASSERT

**Injected at runtime (non-owning):**
- `ISpatialStructure<void*>` — caller provides Grid, Quadtree, or BVH from DiaGeometry2D; used for both PointBody2D and RigidBody2D AABBs

**Explicitly excluded:**
- **DiaApplicationFlow** — no dependency; scheduling and module integration are the caller's concern

**Dependents:**
- Game code (CluicheTest and future games) — creates PhysicsWorld, adds bodies, calls Update()
- Future animation system — will consume the Constraints & Joints feature

## Out of Scope

- Constraint / joint system beyond pin, distance, spring, hinge — e.g., motors, pulleys (future feature spec)
- Soft body, cloth, fluid simulation
- 3D physics — future DiaPhysics3D
- Continuous collision detection (tunnelling prevention) — deferred; add if fast-moving objects become a problem
- Constraint / joint system beyond pin, distance, spring, hinge — e.g., motors, pulleys (future feature spec)
- Network synchronisation / rollback — out of scope for physics layer

## Decisions

| ID | Decision | Rationale | Scope | Status | Binding |
|----|----------|-----------|-------|--------|---------|
| SD-001 | Fixed timestep with accumulator pattern | Deterministic simulation; decoupled from render frame rate; standard game physics practice | PhysicsWorld | Accepted | Yes |
| SD-002 | Broad-phase injected via `ISpatialStructure<PhysicsBody*>` | Caller chooses Grid vs Quadtree based on scene type (dense dynamic vs sparse static); enables per-scene optimisation without changing physics code | Collision Detection | Accepted | Yes |
| SD-003 | Impulse-based collision resolution | Simple, stable, well-understood; sufficient for rigid body games without constraints | Collision Response | Accepted | Yes |
| SD-004 | Collision events via `ObserverSubject<CollisionEvent>` (DiaCore Observer pattern) | Decoupled from DiaApplicationFlow; any system can subscribe without a MessageBus dependency; consistent with platform observer pattern | Collision Events | Accepted | Yes |
| SD-005 | PhysicsBody holds non-owning pointer to `Dia::Geometry2D::Transform` | Transform is owned by the game entity; physics updates position via the transform pointer each step | PhysicsBody | Accepted | Yes |
| SD-006 | No STL containers in public API | Consistent with PD-004 / AD-002; DiaCore containers (DynamicArrayC) used for all output parameters | All features | Accepted | Yes |
| SD-007 | Constraint solver uses sequential impulses (SI) | SI is iterative, stable, and well-suited to game physics; simpler than full LCP; standard choice for 2D rigid body engines (Box2D, Chipmunk) | Constraints | Accepted | Yes |
| SD-008 | Semi-implicit Euler integration for v1 | Simple, stable for most game scenarios, deterministic; can be upgraded to Verlet or RK4 in a feature spec if stability issues arise | Force & Integration | Accepted | Yes |
| SD-009 | Static and kinematic body types alongside dynamic | Static bodies (infinite mass) needed for terrain/walls; kinematic needed for platforms and animated objects that push but aren't pushed | PhysicsBody | Accepted | Yes |
| SD-010 | Sleeping uses dual thresholds (linear + angular velocity) with a settle timer | Single-threshold is unreliable for spinning-but-stationary bodies; settle timer prevents premature sleep from brief low-velocity frames | Body Sleeping | Accepted | Yes |
| SD-011 | Collision filtering uses layer + mask bitmask pair; bilateral check required | Bilateral check (A's mask accepts B's layer AND B's mask accepts A's layer) enables asymmetric filtering (e.g. projectile ignores allies but allies don't ignore projectile) | Collision Layers | Accepted | Yes |

**Status values:** `Proposed` · `Accepted` · `Rejected` · `Superseded`  
**Binding:** `Yes` = enforced constraint on all features in this system · `No` = guidance only

## Inherited Binding Decisions

| ID | Source | Decision | Implication for this system |
|----|--------|----------|----------------------------|
| PD-001 | Platform | StringCRC for all entity/component IDs | PhysicsBody identified by StringCRC; `kUniqueId` constant required |
| PD-004 | Platform | No STL containers in public APIs | Query output uses `DynamicArrayC<PhysicsBody*>`, not `std::vector` |
| PD-005 | Platform | x64 only | DiaRigidBody2D.vcxproj targets x64 exclusively; no Win32 configurations |
| PD-006 | Platform | Visual Studio project files are source of truth | DiaRigidBody2D.vcxproj and .vcxproj.filters created and manually maintained |
| PD-007 | Platform | C++20 required | All code compiled under `/std:c++20` |
| PD-008 | Platform | Directory.Build.props owns OutDir/IntDir/toolchain | DiaRigidBody2D.vcxproj must NOT override OutDir, IntDir, PlatformToolset, WindowsTargetPlatformVersion, or LanguageStandard |
| AD-001 | Dia App | Module system with YAML frontmatter documentation | Create `dia.rigidbody2d.architecture.module.md` with public API, responsibilities, and dependency declarations |
| AD-002 | Dia App | No STL containers in public APIs | Reinforces PD-004 |
| AD-003 | Dia App | Namespace `Dia::<Module>::` | All code in `Dia::RigidBody2D::` namespace |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Integration | Semi-implicit Euler is stable for most cases but can explode at large timesteps. Should maxSubSteps be capped as a safety valve? | Yes — maxSubSteps in WorldDef (default 8) prevents spiral of death if deltaTime spikes. Time is simply lost rather than simulated. |
| 2 | Broad-phase | ISpatialStructure is templated — how is the interface defined in C++? A pure virtual base with template parameter requires careful design to avoid object-slicing. | Use a non-template abstract base with type-erased or concrete `PhysicsBody*` specialisation. The Spatial Grid feature spec in DiaGeometry2D must define this interface. |
| 3 | Collision Events | Enter/stay/exit tracking requires remembering which pairs were colliding last frame. Where does this state live? | In PhysicsWorld — a `HashTable<BodyPairKey, bool>` tracking active collision pairs, compared each step to produce enter/exit events. |
| 4 | Collision Response | Impulse resolution can cause jitter when bodies rest on each other (micro-bouncing). How is this handled? | Restitution bias: clamp restitution to 0 when relative velocity at contact is below a threshold (slop). Positional correction with Baumgarte stabilisation. Document in Collision Response feature spec. |
| 5 | Determinism | Floating-point determinism requires consistent operation ordering. Is per-platform determinism (same result on same machine) sufficient, or cross-platform? | Per-platform only (single Windows x64 target for now). Cross-platform determinism would require fixed-point math — out of scope. |
| 6 | Transform ownership | PhysicsBody holds a raw pointer to `Dia::Geometry2D::Transform`. What happens if the transform is destroyed while the body is still in the world? | Caller is responsible for removing the body before destroying the transform. DIA_ASSERT in PhysicsBody destructor if body is still in a world. |
| 7 | Constraints | Bodies need a list of attached constraints for the sequential impulse solver. How is this stored? | PhysicsBody holds a `DynamicArrayC<IConstraint*>` (non-owning). PhysicsWorld owns constraint objects. Constraints are solved after collision response each step. |
| 8 | Kinematic bodies | Kinematic bodies move via velocity but don't receive impulses. Should they still emit collision events when they hit dynamic bodies? | Yes — kinematic bodies push dynamic bodies and should trigger collision events on both. The dynamic body resolves the impulse; the kinematic body's velocity is unchanged. |
| 9 | Scope | Should DiaRigidBody2D provide a debug draw interface (draw collision shapes, velocities, contact points)? | No — debug drawing belongs in DiaGraphics. Physics exposes data; a debug visualiser layer reads it. |
| 10 | Spatial Queries | Should `Raycast` return the first hit only, or all hits? | First hit for v1 (most common game use case). Add `RaycastAll` as a separate method in the feature spec if needed. |

## Status

`Done` — Implementation complete. Plan: @docs/specs/systems/dia/diarigidbody2d.plan.md
