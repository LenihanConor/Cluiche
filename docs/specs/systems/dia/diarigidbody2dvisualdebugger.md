# System Spec: DiaRigidBody2DVisualDebugger

## Parent Application
@docs/specs/applications/dia.md

## Purpose

DiaRigidBody2DVisualDebugger is a read-only debug visualization system for rigid body physics state. It reads from a `const PhysicsWorld&` and writes draw primitives into a `Dia::Graphics::FrameData&` — collision shapes, velocity arrows, contact normals, constraint lines, and sleep state colour overlays.

It exists as a **separate static library project** (`DiaRigidBody2DVisualDebugger.vcxproj`) because it bridges two systems that are deliberately kept apart: DiaRigidBody2D (pure simulation, no graphics dependency) and DiaGraphics (rendering primitives). This boundary is the architectural reason for promoting it from a feature to its own system — `DiaRigidBody2D.vcxproj` must never link DiaGraphics, and this system is where that mixing is permitted.

Game code that does not need debug drawing links only `DiaRigidBody2D` and incurs no graphics overhead.

**Dependency chain:**
`DiaRigidBody2DVisualDebugger → DiaRigidBody2D → DiaGeometry2D → DiaMaths → DiaCore`
`DiaRigidBody2DVisualDebugger → DiaGraphics`

## Responsibilities

- Draw collision shapes (AARect edges, circles, polygon edges) for all bodies in a PhysicsWorld
- Colour bodies by type: dynamic = white, static = grey, kinematic = cyan
- Override colour to dark blue for sleeping bodies regardless of type
- Draw velocity arrows for awake dynamic and kinematic bodies (capped at 10 visual units)
- Draw contact normal lines from the most recent simulation step
- Draw constraint anchor-to-anchor lines
- Provide an enable/disable toggle that gates all draw output
- Live in its own `.vcxproj` so DiaRigidBody2D has zero DiaGraphics dependency

## Non-Responsibilities

- Modifying physics simulation state — strictly read-only
- Rendering — writes primitives to FrameData; actual rendering is DiaGraphics' concern
- Text/label rendering — deferred until DiaGraphics adds a text primitive
- Collision layer ID visualization — deferred (requires text)
- Application scheduling — game code calls `Draw()` at the appropriate point

## Public Interfaces

### DiaRigidBodyVisualDebugger

File locations:
- `Dia/DiaRigidBody2DVisualDebugger/DiaRigidBodyVisualDebugger.h`
- `Dia/DiaRigidBody2DVisualDebugger/DiaRigidBodyVisualDebugger.cpp`
- `Dia/DiaRigidBody2DVisualDebugger/DiaRigidBody2DVisualDebugger.vcxproj`
- `Dia/DiaRigidBody2DVisualDebugger/DiaRigidBody2DVisualDebugger.vcxproj.filters`

```cpp
namespace Dia::RigidBody2D {

class DiaRigidBodyVisualDebugger {
public:
    DiaRigidBodyVisualDebugger();

    void SetEnabled(bool enabled);
    bool IsEnabled() const;

    // Call after PhysicsWorld::Update(), before rendering.
    // Writes draw primitives into frameData; does not modify world state.
    void Draw(const PhysicsWorld& world, Dia::Graphics::FrameData& frameData);

private:
    bool mEnabled = false;

    void DrawBodies(const PhysicsWorld& world, Dia::Graphics::FrameData& frameData);
    void DrawVelocityArrows(const PhysicsWorld& world, Dia::Graphics::FrameData& frameData);
    void DrawContactNormals(const PhysicsWorld& world, Dia::Graphics::FrameData& frameData);
    void DrawConstraints(const PhysicsWorld& world, Dia::Graphics::FrameData& frameData);
};

} // namespace Dia::RigidBody2D
```

### PhysicsWorld Read Accessors (already exist in DiaRigidBody2D)

This system reads from the following `const` accessors on `PhysicsWorld`, all of which already exist:

```cpp
const Dia::Core::Containers::DynamicArrayC<PointBody2D*, kMaxPointBodies>& GetPointBodies() const;
const Dia::Core::Containers::DynamicArrayC<RigidBody2D*, kMaxRigidBodies>& GetRigidBodies() const;
const Dia::Core::Containers::DynamicArrayC<IConstraint*, kMaxConstraints>& GetConstraints() const;
const Dia::Core::Containers::DynamicArrayC<Contact, kMaxContacts>& GetLastContacts() const;
```

### Body Iteration via Body2DBase

`Body2DBase` (`Dia/DiaRigidBody2D/Bodies/Body2DBase.h`) is the abstract base class for both `PointBody2D` and `RigidBody2D`. It provides all methods the debugger needs: `GetBodyType()`, `IsAwake()`, `GetTransform()`, `GetVelocity()`, `GetCircleShape()`, `GetPolyShape()`, and `ShapeKind`. Draw helpers accept `const Body2DBase*` to avoid duplicated logic.

### IConstraint — Required Addition

`IConstraint` currently provides `GetBodyA()` and `GetBodyB()` but does **not** have `GetWorldAnchorA()` / `GetWorldAnchorB()`. These pure virtual methods must be added to `IConstraint` and implemented by all constraint types (pin, distance, spring, hinge) so the debugger can draw constraint lines between anchor world positions.

## Features

| Feature | Description | Spec | Status |
|---------|-------------|------|--------|
| Core Debug Rendering | Shape drawing, body type colours, sleep colour, velocity arrows, contact normals, constraint lines, enable/disable toggle | TBD | Not Started |

## Dependencies on Other Systems

**Required:**
- **DiaRigidBody2D** — `PhysicsWorld` (read accessors already exist), `Body2DBase`, `PointBody2D`, `RigidBody2D`, `IConstraint` (needs `GetWorldAnchorA/B` addition), `Contact`, `BodyType`, `SleepState`, `ShapeKind`
- **DiaGraphics** — `FrameData`, `DebugFrameDataLine2D`, `DebugFrameDataCircle2D`, `Dia::Graphics::RGBA`
- **DiaGeometry2D** (transitive via DiaRigidBody2D) — `Transform`, shape primitives for position/dimension reads

**Explicitly excluded:**
- **DiaApplication** — no dependency; scheduling is the caller's concern
- **DiaLogger** — debug drawing is visual, not logged

**Dependents:**
- Game code (CluicheTest and future games) — optionally links this library for physics debug overlays

## Out of Scope

- Collision layer ID text labels (requires text primitive in DiaGraphics)
- Performance overlay / stats display
- Soft body debug drawing (belongs to a future DiaSoftBody2DVisualDebugger)
- Interactive debug (picking bodies, modifying state) — this is read-only

## Decisions

| ID | Decision | Rationale | Scope | Status | Binding |
|----|----------|-----------|-------|--------|---------|
| SD-VD-001 | Separate static library project, not a subdirectory of DiaRigidBody2D | DiaRigidBody2D must have zero DiaGraphics dependency; this is the only place physics + graphics concerns mix | Project structure | Accepted | Yes |
| SD-VD-002 | Named colour constants, not magic literals | All body-type colours use `RGBA::White()` etc.; sleep colour is `static constexpr kSleepColour{0, 0, 80, 255}` | Implementation | Accepted | No |
| SD-VD-003 | Velocity arrow length capped at 10 visual units | Prevents absurdly long arrows from dominating the screen; normalised direction preserved | Draw logic | Accepted | No |
| SD-VD-004 | `GetWorldAnchorA()` / `GetWorldAnchorB()` on IConstraint are pure virtual | Every constraint has meaningful anchors; a default returning zero would produce silently incorrect visuals | DiaRigidBody2D IConstraint | Accepted | Yes |

**Status values:** `Proposed` . `Accepted` . `Rejected` . `Superseded`
**Binding:** `Yes` = enforced constraint on all features in this system . `No` = guidance only

## Inherited Binding Decisions

| ID | Source | Decision | Implication for this system |
|----|--------|----------|----------------------------|
| PD-004 | Platform | No STL containers in public APIs | `Draw()` uses `FrameData&` and `DynamicArrayC`; no STL in public interface |
| PD-005 | Platform | x64 only | DiaRigidBody2DVisualDebugger.vcxproj targets x64 exclusively |
| PD-006 | Platform | Visual Studio project files are source of truth | New `.vcxproj` + `.vcxproj.filters` created and manually maintained |
| PD-007 | Platform | C++20 required | All code compiled under `/std:c++20` |
| PD-008 | Platform | `Directory.Build.props` owns OutDir/IntDir/toolchain | No per-project overrides of OutDir, IntDir, PlatformToolset, or LanguageStandard |
| PD-009 | Platform | Generated output under `Cluiche/out/<AppName>/` | N/A — this system produces no generated output files |
| AD-001 | Dia App | Module system with YAML frontmatter | Create `dia.rigidbody2dvisualdebugger.architecture.module.md` |
| AD-002 | Dia App | No STL containers in public APIs | Reinforces PD-004 |
| AD-003 | Dia App | Namespace `Dia::<Module>::` | All code in `Dia::RigidBody2D::` namespace (shares namespace with physics system since it's a companion) |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Body Iteration | `Body2DBase` is referenced in the draw design — does it exist? | Resolved: `Body2DBase` exists at `Dia/DiaRigidBody2D/Bodies/Body2DBase.h`. Both `PointBody2D` and `RigidBody2D` inherit from it. All needed methods (`GetBodyType`, `IsAwake`, `GetTransform`, `GetVelocity`, `GetCircleShape`, `GetPolyShape`, `ShapeKind`) are on the base class. |
| 2 | Namespace | The debugger uses `Dia::RigidBody2D::` namespace despite being a separate project. Should it use its own namespace (e.g. `Dia::RigidBody2DVisualDebugger::`) to reflect the project boundary? | Keep `Dia::RigidBody2D::` — it's a companion system to the physics library and shares the same domain. A separate namespace would add friction for no benefit. |
| 3 | PhysicsWorld Accessors | Does this system require new accessors on PhysicsWorld? | Resolved: All 4 accessors (`GetPointBodies`, `GetRigidBodies`, `GetConstraints`, `GetLastContacts`) already exist on `PhysicsWorld`. No changes needed. |
| 4 | Extensibility | If future debug overlays are needed (e.g. broad-phase grid, AABB bounds, collision pair highlights), should Draw() be designed with a flags/options parameter now, or is the current on/off toggle sufficient for v1? | On/off toggle is sufficient for v1. Flags can be added later without breaking the existing API (overload or default parameter). |
| 5 | Testing | Tests need a stub FrameData to inspect draw calls. Should this test infrastructure live in this system's test directory, or be shared with DiaSoftBody2DVisualDebugger's future tests? | Local to this system's test directory. Only one consumer exists; extract to shared if/when DiaSoftBody2DVisualDebugger needs it. |
| 6 | IConstraint | `GetWorldAnchorA()` / `GetWorldAnchorB()` don't exist on `IConstraint` yet. Adding pure virtuals is a breaking change to DiaRigidBody2D's constraint interface — all 4 constraint types must implement them. Should this be tracked as a prerequisite task in this system's plan, or as a change request on the DiaRigidBody2D constraints-and-joints feature spec? | Change request on the constraints-and-joints feature spec. The addition belongs to DiaRigidBody2D's public API, not this system. This system declares it as a dependency prerequisite. |

## Status

`Approved`
