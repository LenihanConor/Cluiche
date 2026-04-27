# System Spec: DiaSoftBody2D

## Parent Application
@docs/specs/applications/dia.md

## Purpose

DiaSoftBody2D is the 2D soft body simulation system for the Dia engine. It provides Position-Based Dynamics (PBD) simulation for deformable objects — ropes, chains, and cloth grids. It supports both purely cosmetic simulation and two-way coupling with DiaRigidBody2D: soft body endpoints can be pinned to rigid bodies, and particle collisions push impulses back into the rigid body world.

DiaSoftBody2D is a peer to DiaRigidBody2D — both depend on DiaGeometry2D but are independent simulation worlds. Game code drives them separately and controls step ordering. DiaSoftBody2D does depend on DiaRigidBody2D for its coupling API (`PhysicsBody::ApplyImpulse()`).

**Dependency chain:**  
`DiaSoftBody2D → DiaRigidBody2D → DiaGeometry2D → DiaMaths → DiaCore`  
`DiaSoftBody2D → DiaGeometry2D` (direct; for static shape collision)

## Responsibilities

- Maintain a `SoftBodyWorld` containing a set of soft body objects (`Rope`, `Cloth`)
- Simulate soft bodies using PBD: predict particle positions, project distance / structural / shear / bend constraints, finalize velocities
- Integrate particle positions with a fixed timestep using an accumulator pattern (consistent with DiaRigidBody2D)
- Apply gravity and external forces to dynamic particles; respect pinned particles (invMass = 0)
- Detect and resolve collisions between particles and caller-registered static DiaGeometry2D shapes (AARect, Circle, Line)
- Detect and resolve collisions between particles and DiaRigidBody2D bodies; apply back-impulses to rigid bodies for two-way coupling
- Support pinning rope/cloth particles to DiaRigidBody2D bodies (particle follows rigid body; rigid body receives coupling force back)
- Support tearable constraints: per-constraint max-stretch threshold that removes the constraint when exceeded
- Provide `Rope` (1D particle chain) and `Cloth` (2D particle grid) body types
- Expose particle positions each frame so DiaGraphics can render deformable objects
- Emit structured debug logs to the `Physics` DiaLogger channel (debug builds only): max-sub-steps hit, particle velocity exceeded safety threshold, torn constraint detected
- Provide a `dia.softbody2d.architecture.module.md` YAML module documentation file
- Provide a `DiaSoftBody2D.vcxproj` static library project registered in `Cluiche.sln`

## Non-Responsibilities

- Rigid body simulation (forces, impulse response, rigid-rigid collision) — DiaRigidBody2D
- 3D soft body simulation — future DiaSoftBody3D
- Rendering or debug drawing of particles/constraints — DiaGraphics
- Application scheduling — game code calls `SoftBodyWorld::Update()`; ProcessingUnit/Phase integration is the caller's concern
- Self-collision (cloth intersecting itself) — deferred; future feature spec
- Soft body vs soft body collision — deferred
- Fluid simulation — out of scope
- Skinned mesh deformation — DiaGraphics concern; SoftBodyWorld exposes particle positions only
- Serialization of soft body state

## Public Interfaces

### SoftParticle

```cpp
namespace Dia::SoftBody2D {
    struct Particle {
        Dia::Maths::Vector2D position;
        Dia::Maths::Vector2D prevPosition;
        float invMass;   // 0 = pinned (static); > 0 = dynamic
        float radius;    // Used for collision detection
    };
}
```

### Rope

```cpp
namespace Dia::SoftBody2D {
    struct RopeDef {
        Dia::Core::StringCRC             id;
        Dia::Maths::Vector2D             startPoint;
        Dia::Maths::Vector2D             endPoint;
        int                              particleCount  = 10;
        float                            mass           = 1.0f;    // Total; distributed evenly
        float                            stiffness      = 1.0f;    // [0,1] constraint stiffness
        float                            particleRadius = 0.1f;
        float                            maxStretch     = 0.0f;    // Tear ratio; 0 = no tearing
        // Optional rigid body pin anchors (non-owning)
        Dia::RigidBody2D::PhysicsBody*   startAnchor    = nullptr;
        Dia::RigidBody2D::PhysicsBody*   endAnchor      = nullptr;
    };

    class Rope {
    public:
        static constexpr Dia::Core::StringCRC kUniqueId{"Rope"};

        int              GetParticleCount() const;
        Particle&        GetParticle(int index);
        const Particle&  GetParticle(int index) const;
        bool             IsTorn() const;
        const Dia::Core::StringCRC& GetId() const;
    };
}
```

### Cloth

```cpp
namespace Dia::SoftBody2D {
    struct ClothDef {
        Dia::Core::StringCRC  id;
        Dia::Maths::Vector2D  origin;               // Top-left corner in world space
        float                 width;
        float                 height;
        int                   resX;                 // Particle columns
        int                   resY;                 // Particle rows
        float                 mass                  = 1.0f;
        float                 structuralStiffness   = 1.0f;
        float                 shearStiffness        = 0.8f;
        float                 bendStiffness         = 0.3f;
        float                 particleRadius        = 0.05f;
        float                 maxStretch            = 0.0f;   // 0 = no tearing
        bool                  pinTopRow             = false;  // Pin entire top row at creation
    };

    class Cloth {
    public:
        static constexpr Dia::Core::StringCRC kUniqueId{"Cloth"};

        int              GetParticleCount() const;     // resX * resY
        Particle&        GetParticle(int x, int y);
        const Particle&  GetParticle(int x, int y) const;
        bool             IsTorn() const;
        void             PinParticle(int x, int y);    // Set invMass = 0
        void             UnpinParticle(int x, int y);  // Restore original invMass
        const Dia::Core::StringCRC& GetId() const;
    };
}
```

### SoftBodyWorld

```cpp
namespace Dia::SoftBody2D {
    struct WorldDef {
        Dia::Maths::Vector2D              gravity          = { 0.0f, -9.81f };
        float                             fixedTimestep    = 1.0f / 60.0f;
        int                               maxSubSteps      = 8;
        int                               solverIterations = 10;
        // Non-owning; optional — required for rigid body collision and coupling.
        // IMPORTANT: call RigidBodyWorld::Update() BEFORE SoftBodyWorld::Update() each frame.
        Dia::RigidBody2D::PhysicsWorld*   rigidBodyWorld   = nullptr;
    };

    class SoftBodyWorld {
    public:
        explicit SoftBodyWorld(const WorldDef& def);

        // Body management
        Rope*  AddRope(const RopeDef& def);
        Cloth* AddCloth(const ClothDef& def);
        void   RemoveBody(SoftBody* body);

        // Static geometry collision shapes (non-owning; shapes must outlive world)
        void AddStaticShape(const Dia::Geometry2D::AARect* shape);
        void AddStaticShape(const Dia::Geometry2D::Circle* shape);
        void AddStaticShape(const Dia::Geometry2D::Line*   shape);
        void RemoveStaticShape(const void* shapePtr);

        // Simulation
        void Update(float deltaTime);

        // World properties
        void                        SetGravity(const Dia::Maths::Vector2D& gravity);
        const Dia::Maths::Vector2D& GetGravity() const;
    };
}
```

### PBD Step Algorithm

```
accumulator += deltaTime
while accumulator >= fixedTimestep and steps < maxSubSteps:
    ApplyExternalForces()          // gravity → predict particle positions
    ProjectConstraints()           // N iterations: distance / structural / shear / bend
    ResolveGeometryCollision()     // particles vs registered static shapes
    ResolveRigidBodyCollision()    // particles vs rigid bodies; apply back-impulses
    FinalizeVelocities()           // velocity = (pos - prevPos) / dt
    CheckTearing()                 // remove over-stretched constraints
    accumulator -= fixedTimestep
```

## Features

| Feature | Description | Spec | Status |
|---------|-------------|------|--------|
| Particle | Core particle data type: position, prevPosition, invMass, radius. Velocity derived from PBD position delta. | [particle.md](../../features/dia/diasoftbody2d/particle.md) | Draft |
| SoftBodyWorld | Simulation container: fixed-timestep accumulator, gravity, PBD step loop, body management. | [soft-body-world.md](../../features/dia/diasoftbody2d/soft-body-world.md) | Draft |
| Rope | 1D particle chain with distance constraints. Tearable. Optional rigid body pin anchors at endpoints. | [rope.md](../../features/dia/diasoftbody2d/rope.md) | Draft |
| Cloth | 2D particle grid with structural, shear, and bend constraints. Tearable. Pin rows/individual particles. | [cloth.md](../../features/dia/diasoftbody2d/cloth.md) | Draft |
| Geometry Collision | Particle vs registered static DiaGeometry2D shapes (AARect, Circle, Line). Positional correction. | [geometry-collision.md](../../features/dia/diasoftbody2d/geometry-collision.md) | Draft |
| Rigid Body Coupling | Two-way: pin particles to PhysicsBody anchors; particle collisions apply back-impulses to rigid bodies. | [rigid-body-coupling.md](../../features/dia/diasoftbody2d/rigid-body-coupling.md) | Draft |
| Physics Logging | Structured `Physics` channel DiaLogger warnings: maxSubSteps hit, particle velocity safety threshold, torn constraints detected. Debug builds only. | [physics-logging.md](../../features/dia/diasoftbody2d/physics-logging.md) | Approved |
| Visual Debugger | `DiaSoftBodyVisualDebugger`: takes `SoftBodyWorld&` + `FrameData&`. Draws particles, constraints (colour-coded by type), torn constraints, pinned particles, rigid body anchor links. On/off toggle. | [visual-debugger.md](../../features/dia/diasoftbody2d/visual-debugger.md) | Approved |

## Dependencies on Other Systems

**Required:**
- **DiaRigidBody2D** — `PhysicsBody::ApplyImpulse()` for two-way coupling; `PhysicsWorld::QueryCircle()` for rigid body particle collision
- **DiaGeometry2D** — Shape types (AARect, Circle, Line), `IntersectionTests` for particle-shape collision
- **DiaMaths** — Vector2D, Angle (via DiaGeometry2D and direct use)
- **DiaCore** — StringCRC, DynamicArrayC, DIA_ASSERT

**Injected at runtime (non-owning):**
- `Dia::RigidBody2D::PhysicsWorld*` — optional; caller provides for rigid body collision/coupling
- Static geometry shapes registered per-world via `AddStaticShape()`

**Dependents:**
- Game code (CluicheTest and future games) — creates SoftBodyWorld, adds ropes/cloth, calls Update()
- Animation system — will consume Rope and Cloth for banners, ragdoll attachments, chains

## Out of Scope

- Self-collision (cloth intersecting itself) — deferred; future feature spec
- Soft body vs soft body collision — deferred
- 3D soft bodies — future DiaSoftBody3D
- Continuous collision detection for fast-moving particles — deferred
- Sleeping / deactivation of idle bodies — deferred optimisation
- Skinned mesh rendering — DiaGraphics concern
- Serialization of soft body configuration or state
- Volume preservation constraints (pressure, balloon) — future feature spec
- Driven/motorised constraints — future feature spec

## Decisions

| ID | Decision | Rationale | Scope | Status | Binding |
|----|----------|-----------|-------|--------|---------|
| SD-001 | PBD solver (Position-Based Dynamics) | Fast, stable, artistically controllable; no stiffness coefficient tuning; handles tearing naturally by removing constraints; XPBD variant makes stiffness timestep-independent | All features | Accepted | Yes |
| SD-002 | SoftBodyWorld is a separate peer world, not embedded in PhysicsWorld | Maintains peer architecture (both depend on DiaGeometry2D); enables standalone cosmetic use without rigid bodies; game code controls step ordering | SoftBodyWorld | Accepted | Yes |
| SD-003 | Two-way rigid body coupling | Ropes/cloth must be able to pull/push rigid bodies; required for connected simulations (cranes, ragdoll attachments, swinging doors) | Rigid Body Coupling | Accepted | Yes |
| SD-004 | Tearing is per-constraint opt-in via `maxStretch` ratio threshold (0 = disabled) | Non-tearable objects pay no extra cost; tearing is declared in the def, not a runtime mode switch | Rope, Cloth | Accepted | Yes |
| SD-005 | Fixed timestep with accumulator pattern | Consistent with DiaRigidBody2D; deterministic; same step-ordering contract for game code | SoftBodyWorld | Accepted | Yes |
| SD-006 | Static geometry registered as typed shape pointers, not via ISpatialStructure | Particle counts are small (< 200 per body); flat O(n×m) iteration over registered shapes is fast enough; avoids injected spatial structure complexity at this scale | Geometry Collision | Accepted | Yes |
| SD-007 | Velocity derived from PBD position delta: `v = (pos - prevPos) / dt` | PBD standard; no separate velocity integration; constraints modify positions directly; velocity naturally emerges without separate accumulation | All features | Accepted | Yes |
| SD-008 | No STL in public API | Consistent with PD-004 / AD-002 | All features | Accepted | Yes |
| SD-009 | Rope first, cloth second | Rope (1D chain) establishes the particle + constraint framework; cloth builds on it; reduces implementation risk | All features | Accepted | Yes |

**Status values:** `Proposed` · `Accepted` · `Rejected` · `Superseded`  
**Binding:** `Yes` = enforced constraint on all features in this system · `No` = guidance only

## Inherited Binding Decisions

| ID | Source | Decision | Implication for this system |
|----|--------|----------|----------------------------|
| PD-001 | Platform | StringCRC for all entity/component IDs | Rope/Cloth identified by `kUniqueId` StringCRC; per-instance `id` in def structs |
| PD-004 | Platform | No STL containers in public APIs | DynamicArrayC for all collection parameters |
| PD-005 | Platform | x64 only | DiaSoftBody2D.vcxproj targets x64 exclusively; no Win32 configurations |
| PD-006 | Platform | Visual Studio project files are source of truth | DiaSoftBody2D.vcxproj and .vcxproj.filters created and manually maintained; all files explicitly listed |
| PD-007 | Platform | C++20 required | All code compiled under `/std:c++20` |
| PD-008 | Platform | Directory.Build.props owns OutDir/IntDir/toolchain | DiaSoftBody2D.vcxproj must NOT override OutDir, IntDir, PlatformToolset, WindowsTargetPlatformVersion, or LanguageStandard |
| AD-001 | Dia App | Module system with YAML frontmatter documentation | Create `dia.softbody2d.architecture.module.md` with public API, responsibilities, and dependency declarations |
| AD-002 | Dia App | No STL containers in public APIs | Reinforces PD-004 |
| AD-003 | Dia App | Namespace `Dia::<Module>::` | All code in `Dia::SoftBody2D::` namespace per SD-001 |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Solver | PBD stiffness is iteration-count dependent — more iterations = stiffer. Should stiffness be exposed as iterations or as a compliance value? | Expose as [0,1] stiffness per constraint. Internally map to XPBD compliance: `α = (1 - stiffness) / (dt²)`. This makes stiffness timestep-independent — consistent across different fixedTimestep values. |
| 2 | Coupling order | Game code must call `RigidBodyWorld::Update()` before `SoftBodyWorld::Update()` for correct two-way coupling. Should this be enforced? | Document the required ordering in WorldDef (code comment) and the rigid-body-coupling feature spec. No runtime assert — there's no safe way to detect ordering violations without coupling the worlds directly. |
| 3 | Tearing topology | When a cloth constraint tears, two particles become disconnected. Does this create orphaned particles? | Orphaned particles fall freely under gravity — this is correct and desirable (torn cloth pieces should fall). No special handling needed; PBD naturally handles it since particles with no constraints simply integrate freely. |
| 4 | Particle count limits | Should RopeDef/ClothDef validate max particle counts at construction? | Yes — DIA_ASSERT in body constructors. Rope: max 200 particles. Cloth: max 64×64 = 4096 particles. Document limits in feature specs. Exceeding limits is a programming error, not a runtime condition. |
| 5 | Coupling impulse lag | Back-impulses are applied after the rigid body step has already run this frame, introducing one frame of lag. Is this acceptable? | Yes — one frame of lag is imperceptible at 60fps and is the standard approach in game engines. Document as a known limitation in the rigid-body-coupling feature spec. |
| 6 | Back-impulse magnitude | How is the back-impulse magnitude from particle collision to rigid body computed? | Derived from positional correction applied to the particle: `impulse = correction / dt * particleMass`. Applied via `PhysicsBody::ApplyImpulse()`. Full derivation documented in rigid-body-coupling feature spec. |
| 7 | Cloth particle pinning | `pinTopRow = true` pins the entire top row at creation. What if game code needs to pin arbitrary particles at runtime? | `Cloth::PinParticle(x, y)` / `UnpinParticle(x, y)` allow runtime pinning. `pinTopRow` is a creation-time convenience shortcut that calls PinParticle for the whole row. |
| 8 | Static shape removal | `RemoveStaticShape(const void*)` uses a void pointer for type erasure. Is this safe? | Yes — caller passes back the exact pointer they registered; removal compares addresses. Passing an unregistered pointer is a no-op (with DIA_ASSERT in debug). Shape type is tracked internally via the typed `AddStaticShape` overloads. |
| 9 | Multiple rigid body worlds | WorldDef accepts one `PhysicsWorld*`. What if game code has multiple rigid body worlds? | Out of scope for v1. Document that SoftBodyWorld couples to at most one PhysicsWorld. Multi-world coupling would require a wrapper or extension in a future feature spec. |
| 10 | Soft-soft collision | Should ropes collide with cloth or other ropes? | Out of scope for v1. Self-collision and soft-soft collision are the most expensive PBD extensions and are not needed for the stated use cases (cosmetic cloth, chains, ragdoll attachments). Add as a future feature spec when needed. |

## Status

`Approved`
