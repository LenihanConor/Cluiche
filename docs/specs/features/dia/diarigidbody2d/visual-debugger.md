# Feature Spec: Visual Debugger (DiaRigidBody2D)

## Traceability

| Level | Parent | This Feature |
|-------|--------|--------------|
| Platform | @docs/specs/platform/Cluiche.md | - |
| Application | @docs/specs/applications/dia.md | - |
| System | @docs/specs/systems/dia/diarigidbody2d.md | **visual-debugger** |

**Status:** `Approved`

---

## Problem Statement

Rigid body physics problems — tunnelling, interpenetration, incorrect constraint anchor placement, sleeping bodies blocking expected motion — are almost impossible to diagnose purely from game output. Developers need a way to see the simulation state (shapes, velocities, contacts, constraints, sleep state) overlaid on the game world each frame without modifying the simulation itself or polluting game rendering code with physics internals.

---

## Solution Overview

`DiaRigidBodyVisualDebugger` is a standalone class that reads from a `const PhysicsWorld&` and writes draw primitives into a `Dia::Graphics::FrameData&`. It is completely separate from the simulation: it does not modify any physics state and is not called by `PhysicsWorld`. Game code instantiates it, enables it, and calls `Draw(world, frameData)` once per frame after `PhysicsWorld::Update()` and before rendering.

Body iteration uses `GetPointBodies()` and `GetRigidBodies()` — two separate read accessors on `PhysicsWorld` — to visit both pools. Draw helpers that accept a body use `Body2DBase*` so they work uniformly across types.

The class lives in a **separate `DiaRigidBody2DVisualDebugger` static library project** — not inside `DiaRigidBody2D`. This is the architectural boundary where physics and graphics concerns are permitted to mix. `DiaRigidBody2D.vcxproj` has zero DiaGraphics dependencies. Game code that does not use the visual debugger links only `DiaRigidBody2D` and incurs no graphics overhead.

`PhysicsWorld` requires one new read accessor, `GetLastContacts() const`, to expose the contact list from the most recently completed step. This is a minimal, non-owning, const-correct addition to the physics-world feature.

---

## Acceptance Criteria

| ID | Criterion | Verification Method |
|----|-----------|---------------------|
| AC1 | `SetEnabled(false)` causes `Draw()` to emit zero primitives into `FrameData` | Unit test: construct world with bodies; disable; call Draw; assert FrameData primitive count == 0 |
| AC2 | One AARect body produces exactly 4 line primitives (the four edges) | Unit test: add one AARect body; call Draw; assert 4 `DebugFrameDataLine2D` entries |
| AC3 | One circle body produces exactly one circle primitive | Unit test: add one circle body; call Draw; assert 1 `DebugFrameDataCircle2D` entry |
| AC4 | Dynamic body drawn in white, static in grey, kinematic in cyan | Unit test: add one of each type; inspect colour on each primitive |
| AC5 | Sleeping body drawn in dark blue regardless of body type | Unit test: put dynamic body to sleep; call Draw; assert dark blue colour |
| AC6 | Awake dynamic or kinematic body produces a velocity arrow line; static body does not | Unit test: dynamic body with known velocity; verify line drawn from body position; kinematic body with velocity also produces arrow; static body produces no arrow |
| AC7 | Velocity arrow direction matches normalised velocity, length capped at 10 visual units | Unit test: known velocity vector; measure line end-point |
| AC8 | Contact normal produces a line of 0.2 units from the contact point along the normal | Unit test: arrange overlapping bodies; step world; call Draw; verify contact-normal line present with correct length |
| AC9 | Constraint anchor points connected by a green line | Unit test: add distance constraint with known anchors; call Draw; verify green line between anchor world positions |
| AC10 | Empty world (no bodies, no contacts, no constraints) does not crash | Unit test |
| AC11 | `GetLastContacts()` on `PhysicsWorld` returns the contact list from the completed step | Unit test: step with collision; verify non-empty contact list from accessor |
| AC12 | Full solution builds clean | `msbuild Cluiche.sln /p:Configuration=Debug /p:Platform=x64` |

---

## Public API

### DiaRigidBodyVisualDebugger

File locations (separate project — not inside DiaRigidBody2D):
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

### PhysicsWorld — new read accessor

The following const accessor is added to `PhysicsWorld` as part of this feature. It exposes the contacts generated during the most recently completed simulation step:

```cpp
// Added to PhysicsWorld (physics-world feature)
const Dia::Core::DynamicArrayC<Contact>& GetLastContacts() const;
```

`mLastContacts` is populated at the end of each `StepOnce()` call and cleared at the start of the next one. Its lifetime is bounded by the `PhysicsWorld` instance.

---

## Implementation Notes

### Draw Dispatch

`Draw()` returns immediately if `!mEnabled`. Otherwise it calls four private helpers in order:

1. `DrawBodies()` — shapes and sleep colour
2. `DrawVelocityArrows()` — dynamic awake bodies only
3. `DrawContactNormals()` — from `world.GetLastContacts()`
4. `DrawConstraints()` — from world constraint list

### DrawBodies

Iterates both `world.GetPointBodies()` and `world.GetRigidBodies()`. Both return `const Dia::Core::DynamicArrayC<PointBody2D*>&` and `const Dia::Core::DynamicArrayC<RigidBody2D*>&` respectively. A private helper `DrawBody(const Body2DBase* body, ...)` handles the common draw logic so shape dispatch and colour selection are not duplicated:

- Determine base colour by `BodyType` (via `Body2DBase::GetBodyType()`): dynamic = `Dia::Graphics::RGBA::White()`, static = `Dia::Graphics::RGBA::Grey()`, kinematic = `Dia::Graphics::RGBA::Cyan()`.
- If `body->IsAwake() == false`, override colour to `kSleepColour` (`static constexpr Dia::Graphics::RGBA kSleepColour{0, 0, 80, 255}` — dark blue, defined at file scope in the `.cpp`).
- Shape dispatch (shape pointers retrieved via `GetBroadShape()` / `GetCircleShape()` / `GetPolyShape()` — these exist on both `PointBody2D` and `RigidBody2D`):
  - If body has a `circleShape`: emit one `DebugFrameDataCircle2D(center, radius, colour)`.
  - If body has a `polyShape`: emit N lines along the polygon edges using `DebugFrameDataLine2D`.
  - Otherwise (AARect broad shape only): emit 4 lines from the four corners of the AARect.

`PhysicsWorld` must expose two new const accessors:
```cpp
const Dia::Core::DynamicArrayC<PointBody2D*>& GetPointBodies() const;
const Dia::Core::DynamicArrayC<RigidBody2D*>& GetRigidBodies() const;
```
Both are additive and do not break any existing behaviour.

### DrawVelocityArrows

Iterates both pools. For each **awake dynamic or kinematic** body (checked via `Body2DBase::IsAwake()` and `GetBodyType()` — static bodies are skipped):

```
start  = body->GetTransform()->GetPosition()
dir    = body->GetVelocity().Normalised()
length = min(body->GetVelocity().Magnitude(), 10.0f)
end    = start + dir * length
emit DebugFrameDataLine2D(start, end, Dia::Graphics::RGBA::Yellow())
```

If `body->GetVelocity().Magnitude() < 0.0001f`, skip (no visible arrow for stationary bodies). Both `PointBody2D` and `RigidBody2D` have `GetVelocity()` and `GetTransform()` — no cast required for this helper.

### DrawContactNormals

For each `Contact` in `world.GetLastContacts()`:

```
start = contact.result.contactPoint
end   = start + contact.result.normal * 0.2f
emit DebugFrameDataLine2D(start, end, Dia::Graphics::RGBA::Red())
```

### DrawConstraints

Iterates all active constraints via `PhysicsWorld::GetConstraints() const` (returns `const Dia::Core::DynamicArrayC<IConstraint*>&`). For each constraint:

```
worldAnchorA = constraint->GetWorldAnchorA()
worldAnchorB = constraint->GetWorldAnchorB()
emit DebugFrameDataLine2D(worldAnchorA, worldAnchorB, Dia::Graphics::RGBA::Green())
```

`IConstraint` requires two new `const` accessors: `GetWorldAnchorA()` and `GetWorldAnchorB()`, both returning `Dia::Maths::Vector2D`. These return the constraint anchor points transformed into world space using the attached bodies' transforms.

### Layer Debug (Deferred)

Collision layer IDs are not drawn. Text rendering is not available in the current `FrameData` draw primitive set. This capability is deferred until a text/label primitive is added to `DiaGraphics`. Document in `FrameData` extension backlog.

### DiaGraphics Dependency Scope

`DiaRigidBody2D.vcxproj` has **no DiaGraphics dependency**. All `#include <DiaGraphics/...>` directives are confined to the `DiaRigidBody2DVisualDebugger` project. This is enforced structurally — the debugger is a separate link target, not a subdirectory of `DiaRigidBody2D`.

### Project File Changes

**New project `DiaRigidBody2DVisualDebugger.vcxproj`** (StaticLibrary, Debug\|x64 + Release\|x64):
- Depends on `DiaRigidBody2D` and `DiaGraphics`
- `AdditionalIncludeDirectories`: DiaRigidBody2D + DiaGraphics include paths
- Contains `DiaRigidBodyVisualDebugger.h` and `DiaRigidBodyVisualDebugger.cpp`
- Registered in `Cluiche.sln` under the Library folder with a new GUID

**`DiaRigidBody2D.vcxproj`**: no changes — DiaGraphics is not added here.

**`Cluiche.sln`**: add `DiaRigidBody2DVisualDebugger` project with ProjectDependencies on `DiaRigidBody2D` and `DiaGraphics`.

---

## Dependencies

### Required Features
- **physics-world** — body iteration (`GetPointBodies()`, `GetRigidBodies()`), contact list accessor (`GetLastContacts()`), constraint list accessor (`GetConstraints()`)
- **physics-body** — `Body2DBase*` for colour/sleep dispatch; shape pointers, velocity, body type, transform on both `PointBody2D` and `RigidBody2D`
- **body-sleeping** — `SleepState`, `GetSleepState()`
- **constraints-and-joints** — `IConstraint`, `GetWorldAnchorA()`, `GetWorldAnchorB()`

### Required Modules
- **DiaGraphics** — `FrameData`, `DebugFrameDataLine2D`, `DebugFrameDataCircle2D`, `Dia::Graphics::RGBA`

### Dependent Features
None. This is a terminal visualisation feature.

---

## Testing Strategy

### Unit Tests (`Cluiche/Tests/GoogleTests/RigidBody2D/TestVisualDebugger.cpp`)

Tests use a lightweight stub `FrameData` that records all received draw calls into a `DynamicArrayC`, allowing inspection of primitive type, position, and colour.

1. **Disabled — zero primitives** — create world with one dynamic body; `SetEnabled(false)`; call `Draw`; assert `frameData.GetPrimitiveCount() == 0`.
2. **AARect body — four lines** — single AARect dynamic body; `SetEnabled(true)`; call `Draw`; assert exactly 4 line entries.
3. **Circle body — one circle** — single circle dynamic body; call `Draw`; assert exactly 1 circle entry.
4. **Dynamic colour** — dynamic body; assert line colour == White.
5. **Static colour** — static body; assert line colour == Grey.
6. **Kinematic colour** — kinematic body; assert line colour == Cyan.
7. **Sleeping colour** — dynamic body put to sleep; assert line colour == dark blue `(0, 0, 80, 255)`.
8. **Velocity arrow — dynamic awake** — dynamic body with velocity `(5, 0)`; assert one additional line primitive from body position in positive-X direction.
9. **Velocity arrow — static absent** — static body; assert no velocity-arrow line.
10. **Velocity arrow capped** — body velocity 9999 m/s; assert arrow line length == 10 visual units.
11. **Contact normal line** — step world with two overlapping bodies; call `Draw`; assert one red line of length ~0.2 from contact point.
12. **Constraint line** — add distance constraint; call `Draw`; assert one green line between anchor world positions.
13. **Empty world — no crash** — call `Draw` on an empty world; no assertions needed beyond no exception.
14. **GetLastContacts accessor** — step world with collision between a `PointBody2D` and a `RigidBody2D`; call `GetLastContacts()`; assert non-empty; verify `Contact::GetBodyA()` / `GetBodyB()` return correct `Body2DBase*` pointers.

---

## AI Review Questions

| # | Question | Answer |
|---|----------|--------|
| 1 | Adding DiaGraphics as a dependency of DiaRigidBody2D.vcxproj means every user of the physics library pulls in a DiaGraphics link dependency, even if they never use the visual debugger. Is this acceptable, or should the debugger live in a separate optional project? | Separate project — `DiaRigidBody2DVisualDebugger` is where physics and graphics concerns are mixed; that boundary must not sit inside `DiaRigidBody2D`. `DiaRigidBody2D.vcxproj` has no DiaGraphics dependency. A new `DiaRigidBody2DVisualDebugger.vcxproj` static library depends on both `DiaRigidBody2D` and `DiaGraphics`. |
| 2 | `mLastContacts` is populated at the end of each `StepOnce()` and cleared at the start of the next. If `Update()` runs multiple sub-steps in one frame, only the contacts from the last sub-step are visible to `DrawContactNormals`. Is this acceptable? | Acceptable. The visual debugger shows the world state at the end of the last sub-step, which is the state that produced the final rendered frame. Sub-step contact accumulation is unnecessary complexity. |
| 3 | `GetWorldAnchorA()` and `GetWorldAnchorB()` must be added to `IConstraint`. Should these be pure virtual (all constraints must implement them) or have a default implementation (e.g. returning `Vector2D::Zero()`)? | Pure virtual — every constraint has a meaningful anchor point and the debugger should always be able to draw it. A default returning zero would silently produce incorrect visuals. |
| 4 | Kinematic bodies can have non-zero velocity but the spec only draws velocity arrows for awake dynamic bodies. Should kinematic bodies also get velocity arrows? | Yes — kinematic bodies draw velocity arrows too. They are velocity-driven and the arrow is useful for debugging platform/animated body motion. Static bodies still produce no arrow. |
| 5 | The spec uses `Dia::Graphics::RGBA(0, 0, 80, 255)` for sleeping bodies as a magic literal. Should this be a named constant (`kSleepColour`) inside the debugger `.cpp`, or is a magic literal acceptable here? | Named constant — define `static constexpr Dia::Graphics::RGBA kSleepColour{0, 0, 80, 255}` at file scope in `DiaRigidBodyVisualDebugger.cpp`. All other colours already use named factory methods (`RGBA::White()`, etc.); the sleeping colour should follow the same pattern. |

---

## Binding Decisions Compliance

| Decision | Source | Summary | Compliance |
|----------|--------|---------|------------|
| PD-004 | Platform | No STL in public APIs | ✅ No STL in `DiaRigidBodyVisualDebugger` public interface; `FrameData` and `DynamicArrayC` used throughout |
| PD-005 | Platform | x64 only | ✅ No Win32 configurations; DiaRigidBody2D.vcxproj targets x64 exclusively |
| PD-006 | Platform | Visual Studio project files are source of truth | ✅ New `DiaRigidBody2DVisualDebugger.vcxproj` + `.vcxproj.filters` created; `DiaRigidBody2D.vcxproj` unchanged (no graphics dependency added) |
| PD-007 | Platform | C++20 required | ✅ No incompatible constructs introduced |
| PD-008 | Platform | Directory.Build.props owns OutDir/IntDir/toolchain | ✅ No per-project overrides of OutDir, IntDir, PlatformToolset, or LanguageStandard |
| AD-001 | Dia App | Module system with YAML frontmatter | ✅ `DiaRigidBodyVisualDebugger` lives inside the `DiaRigidBody2D` module; no new module file needed |
| AD-002 | Dia App | No STL in public APIs | ✅ Reinforces PD-004 |
| AD-003 | Dia App | Namespace `Dia::<Module>::` | ✅ Class in `Dia::RigidBody2D::` |
| SD-005 | System | Non-owning Transform pointer | ✅ Debugger reads `body->GetTransform()->GetPosition()` without modifying it; read-only access consistent with non-owning contract |
| SD-006 | System | No STL in public APIs | ✅ Reinforced |
| SD-009 | System | Static + kinematic body types | ✅ All three body types drawn with distinct colours (white/grey/cyan); sleeping override applies regardless of type |
| SD-010 | System | Dual threshold sleeping | ✅ Sleep state read via `Body2DBase::IsAwake()`; sleeping bodies drawn dark blue regardless of type |
