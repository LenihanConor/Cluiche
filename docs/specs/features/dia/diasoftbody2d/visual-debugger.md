# Feature Spec: Visual Debugger (DiaSoftBody2D)

## Traceability

| Level | Parent | This Feature |
|-------|--------|--------------|
| Platform | @docs/specs/platform/Cluiche.md | - |
| Application | @docs/specs/applications/dia.md | - |
| System | @docs/specs/systems/dia/diasoftbody2d.md | **visual-debugger** |

**Status:** `Deferred` — moved to separate spec: [DiaSoftBody2DVisualDebugger](../../../systems/dia/diarigidbody2dvisualdebugger.md)

---

## Problem Statement

Soft body simulation problems — particles tunnelling through geometry, pinned particles moving unexpectedly, rope tearing from the wrong end, cloth crumpling at the wrong corner, rigid body anchors not coupling correctly — are nearly invisible in game output. Developers need a zero-intrusion way to see particle positions, constraint connectivity (including colour-coded constraint types), pinning state, and rigid body anchor links each frame without touching simulation code.

---

## Solution Overview

`DiaSoftBodyVisualDebugger` is a standalone class that reads from a `const SoftBodyWorld&` and writes draw primitives into a `Dia::Graphics::FrameData&`. It does not modify any simulation state and is not called by `SoftBodyWorld`. Game code instantiates it, enables it, and calls `Draw(world, frameData)` once per frame after `SoftBodyWorld::Update()` and before rendering.

The class is the **only** place in the `DiaSoftBody2D` library that touches `DiaGraphics`. All other translation units in `DiaSoftBody2D` have no `DiaGraphics` includes. This constraint must be maintained; it is enforced by placing the class in a dedicated subdirectory `DiaSoftBody2D/Debug/`.

The visual pattern intentionally mirrors `DiaRigidBodyVisualDebugger`: same `SetEnabled` / `Draw` interface, same `FrameData` draw primitive calls, same colour convention for shared concepts. Developers familiar with one debugger can immediately read the other.

---

## Acceptance Criteria

| ID | Criterion | Verification Method |
|----|-----------|---------------------|
| AC1 | `SetEnabled(false)` causes `Draw()` to emit zero primitives into `FrameData` | Unit test: construct world with rope; disable; call Draw; assert FrameData primitive count == 0 |
| AC2 | Each particle is drawn as a circle at its position with its radius | Unit test: rope with known particle positions and radii; assert one circle per particle with matching centre and radius |
| AC3 | Dynamic particle drawn in white; pinned particle (`invMass == 0`) drawn in magenta | Unit test: rope with pinned end; assert pinned particle circle is magenta |
| AC4 | Active structural constraints drawn as white lines between constrained particle pairs | Unit test: rope with 3 particles; assert 2 white lines |
| AC5 | Shear constraints drawn in cyan | Unit test: cloth with shear constraints; assert at least one cyan line |
| AC6 | Bend constraints drawn in blue | Unit test: cloth with bend constraints; assert at least one blue line |
| AC7 | Rope constraints drawn in white (same as structural) | Unit test: inspect rope constraint line colour |
| AC8 | Rigid body anchor link drawn in yellow from anchor particle to rigid body world position when anchor is set | Unit test: rope with `startAnchor` set to a known `PhysicsBody` position; assert yellow line from start particle to body position |
| AC9 | No anchor link drawn when `startAnchor` and `endAnchor` are both null | Unit test: rope with no anchors; assert zero yellow lines |
| AC10 | Particle velocity arrow drawn from particle position in direction of `(pos - prevPos)` (green, skips pinned) | Unit test: particle with known `pos` and `prevPos`; assert green line in correct direction |
| AC11 | Empty world (no bodies) does not crash | Unit test |
| AC12 | Full solution builds clean | `msbuild Cluiche.sln /p:Configuration=Debug /p:Platform=x64` |

---

## Public API

### DiaSoftBodyVisualDebugger

File locations:
- `Dia/DiaSoftBody2D/Debug/DiaSoftBodyVisualDebugger.h`
- `Dia/DiaSoftBody2D/Debug/DiaSoftBodyVisualDebugger.cpp`

Both files reside in the `DiaSoftBody2D/Debug/` subdirectory, which is the **sole** location of `DiaGraphics` includes within the `DiaSoftBody2D` library.

```cpp
namespace Dia::SoftBody2D {

class DiaSoftBodyVisualDebugger {
public:
    DiaSoftBodyVisualDebugger();

    void SetEnabled(bool enabled);
    bool IsEnabled() const;

    // Call after SoftBodyWorld::Update(), before rendering.
    // Writes draw primitives into frameData; does not modify world state.
    void Draw(const SoftBodyWorld& world, Dia::Graphics::FrameData& frameData);

private:
    bool mEnabled = false;

    void DrawParticles(const SoftBodyWorld& world, Dia::Graphics::FrameData& frameData);
    void DrawConstraints(const SoftBodyWorld& world, Dia::Graphics::FrameData& frameData);
    void DrawAnchorLinks(const SoftBodyWorld& world, Dia::Graphics::FrameData& frameData);
    void DrawVelocities(const SoftBodyWorld& world, Dia::Graphics::FrameData& frameData);
};

} // namespace Dia::SoftBody2D
```

---

## Implementation Notes

### Draw Dispatch

`Draw()` returns immediately if `!mEnabled`. Otherwise it calls four private helpers in order:

1. `DrawParticles()` — particle circles with pinned/dynamic colour
2. `DrawConstraints()` — constraint lines colour-coded by type
3. `DrawAnchorLinks()` — rigid body anchor lines
4. `DrawVelocities()` — particle velocity direction arrows

### DrawParticles

`SoftBodyWorld` must expose `GetBodies() const` returning a `const Dia::Core::DynamicArrayC<SoftBody*>&` to enable iteration. `SoftBody` is the common base of `Rope` and `Cloth`; both provide `GetParticleCount()` and `GetParticle(int index)`.

For each body, for each particle:

```
colour = (particle.invMass == 0.0f)
             ? Dia::Graphics::RGBA::Magenta()
             : Dia::Graphics::RGBA::White()
emit DebugFrameDataCircle2D(particle.position, particle.radius, colour)
```

### DrawConstraints

Iterates constraints on each body. Each constraint is a pair of particle indices; the body provides the particle positions. Colour by constraint type:

| Constraint type | Colour |
|-----------------|--------|
| Rope distance constraint | `Dia::Graphics::RGBA::White()` |
| Cloth structural | `Dia::Graphics::RGBA::White()` |
| Cloth shear | `Dia::Graphics::RGBA::Cyan()` |
| Cloth bend | `Dia::Graphics::RGBA(0, 0, 255, 255)` (blue) |

Torn constraints are not drawn because they have already been removed from the constraint list by `CheckTearing()`.

```
for each constraint:
    posA = body->GetParticle(constraint.indexA).position
    posB = body->GetParticle(constraint.indexB).position
    emit DebugFrameDataLine2D(posA, posB, colourForType)
```

`Cloth` must expose its constraint list in a way that carries constraint type. The recommended approach is a per-type iterator or flag on a flat constraint struct:

```cpp
// Internal constraint struct (DiaSoftBody2D, not public):
struct SoftConstraint {
    int   indexA;
    int   indexB;
    float restLength;
    float stiffness;
    bool  active;
    enum class Type { kRope, kStructural, kShear, kBend } type;
};
```

The `type` field is set at body construction time and never changes. The debugger reads it without modifying it.

### DrawAnchorLinks

Rigid body anchor links apply only to `Rope` objects that have a non-null `startAnchor` or `endAnchor`.

```
for each Rope in world:
    if rope.startAnchor != nullptr:
        anchorParticlePos = rope.GetParticle(0).position
        rigidBodyPos      = rope.startAnchor->GetTransform()->GetPosition()
        emit DebugFrameDataLine2D(anchorParticlePos, rigidBodyPos,
                                  Dia::Graphics::RGBA::Yellow())
    if rope.endAnchor != nullptr:
        anchorParticlePos = rope.GetParticle(rope.GetParticleCount() - 1).position
        rigidBodyPos      = rope.endAnchor->GetTransform()->GetPosition()
        emit DebugFrameDataLine2D(anchorParticlePos, rigidBodyPos,
                                  Dia::Graphics::RGBA::Yellow())
```

`Rope` must expose `GetStartAnchor() const` and `GetEndAnchor() const` returning `const Dia::RigidBody2D::PhysicsBody*`. These are read-only accessors over existing internal anchor state set from `RopeDef`.

### DrawVelocities

Derived velocity for PBD is `v = (pos - prevPos)`. For each dynamic particle (`invMass > 0`):

```
delta  = particle.position - particle.prevPosition
if delta.Magnitude() < 0.0001f: skip
end    = particle.position + delta   // no scaling — raw one-step displacement
emit DebugFrameDataLine2D(particle.position, end, Dia::Graphics::RGBA::Green())
```

Pinned particles (`invMass == 0`) are always skipped — their delta is always zero.

### SoftBodyWorld — new accessor

`SoftBodyWorld` must expose:

```cpp
const Dia::Core::DynamicArrayC<SoftBody*>& GetBodies() const;
```

This returns all active bodies (both `Rope` and `Cloth` objects). The debugger uses `SoftBody*` and downcasts where needed (via type-safe enum or virtual dispatch), or uses the base particle and constraint iteration interface on `SoftBody`.

### DiaGraphics Dependency Scope

The `#include <DiaGraphics/FrameData.h>` directive appears **only** in:
- `Dia/DiaSoftBody2D/Debug/DiaSoftBodyVisualDebugger.h`
- `Dia/DiaSoftBody2D/Debug/DiaSoftBodyVisualDebugger.cpp`

No other file in `DiaSoftBody2D` may include any `DiaGraphics` header. This mirrors the identical constraint in `DiaRigidBody2D` and is a hard structural rule enforced by code review.

### Project File Changes

`DiaSoftBody2D.vcxproj`:
- Add `DiaSoftBody2D/Debug/DiaSoftBodyVisualDebugger.h` to `<ClInclude>` items.
- Add `DiaSoftBody2D/Debug/DiaSoftBodyVisualDebugger.cpp` to `<ClCompile>` items.
- Add `DiaGraphics` include directory to `AdditionalIncludeDirectories`.
- Add `DiaGraphics.lib` to `AdditionalDependencies` for all configurations.

`DiaSoftBody2D.vcxproj.filters`:
- Place both files under a `Debug\` filter group.

---

## Dependencies

### Required Features
- **soft-body-world** — body iteration (`GetBodies()`)
- **particle** — particle position, prevPosition, invMass, radius
- **rope** — particle and constraint data, anchor accessors (`GetStartAnchor()`, `GetEndAnchor()`)
- **cloth** — particle and constraint data with type information
- **rigid-body-coupling** — anchor pointers on `Rope` (accessed read-only for visualisation)

### Required Modules
- **DiaGraphics** — `FrameData`, `DebugFrameDataLine2D`, `DebugFrameDataCircle2D`, `Dia::Graphics::RGBA`
- **DiaRigidBody2D** — `PhysicsBody::GetTransform()` used for anchor link world position

### Dependent Features
None. This is a terminal visualisation feature.

---

## Testing Strategy

### Unit Tests (`Cluiche/Tests/GoogleTests/SoftBody2D/TestSoftBodyVisualDebugger.cpp`)

Tests use a lightweight stub `FrameData` that records all received draw calls into a `DynamicArrayC`, allowing inspection of primitive type (line vs circle), position, and colour.

1. **Disabled — zero primitives** — create world with one rope; `SetEnabled(false)`; call `Draw`; assert `frameData.GetPrimitiveCount() == 0`.
2. **Particle circles drawn** — rope with 4 particles; call `Draw`; assert exactly 4 circle primitives.
3. **Particle positions match** — verify each circle centre equals the known particle position.
4. **Particle radii match** — verify each circle radius equals the known particle radius.
5. **Dynamic particle colour** — dynamic particle (`invMass > 0`); assert circle colour == White.
6. **Pinned particle colour** — pinned particle (`invMass == 0`); assert circle colour == Magenta.
7. **Rope constraints — count** — rope with 4 particles has 3 constraints; assert 3 white line primitives in constraint group.
8. **Cloth structural constraints** — 2x2 cloth has structural constraints; assert white lines.
9. **Cloth shear constraints** — 2x2 cloth; assert at least one cyan line.
10. **Cloth bend constraints** — cloth with bend constraints; assert at least one blue line.
11. **Anchor link drawn** — rope with `startAnchor` pointing to a known-position `PhysicsBody`; assert one yellow line from first particle position to body position.
12. **No anchor link without anchor** — rope with both anchors null; assert zero yellow lines.
13. **Velocity arrow drawn** — particle with `prevPosition != position`; assert one green line from particle in correct direction.
14. **Velocity arrow skips pinned** — pinned particle with `prevPosition != position` (manually set for test); assert no green line for that particle.
15. **Empty world — no crash** — call `Draw` on a world with no bodies; no assertions beyond no exception.

---

## Binding Decisions Compliance

| Decision | Source | Summary | Compliance |
|----------|--------|---------|------------|
| PD-004 | Platform | No STL in public APIs | No STL in `DiaSoftBodyVisualDebugger` public interface; `FrameData` and `DynamicArrayC` used throughout |
| PD-006 | Platform | Visual Studio project files are source of truth | `DiaSoftBody2D.vcxproj` and `.vcxproj.filters` updated with new files under `Debug\` filter |
| PD-007 | Platform | C++20 required | No incompatible constructs introduced |
| AD-003 | Dia App | Namespace `Dia::<Module>::` | Class in `Dia::SoftBody2D::` |
| SD-008 | System | No STL in public APIs | Reinforced |

---

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | DrawParticles / DrawConstraints | Spec calls `Dia::Graphics::RGBA::Magenta()` etc. — these are static const objects, not methods. | Remove `()` throughout: use `RGBA::Magenta`, `RGBA::White`, `RGBA::Cyan`, `RGBA::Yellow`, `RGBA::Green`. Implementation Notes updated accordingly. |
| 2 | Implementation Notes | `SoftBodyWorld::GetBodies()` is required by this spec but was absent from soft-body-world.md's public API. Gap or intentional? | Genuine gap — soft-body-world.md has been updated to add `const Dia::Core::DynamicArrayC<SoftBody*>& GetBodies() const;` to the public interface. |
| 3 | DrawConstraints | Spec proposes `SoftConstraint` with a `type` field, but rope.md defines `DistanceConstraint` with no type field. Which is canonical? | No conflict. Rope uses `DistanceConstraint` (all rope constraints are the same type). Cloth must expose type info via `SoftConstraint` or a parallel type array. Choice deferred to cloth implementation; this spec requires type info to be accessible per-constraint. |
| 4 | DrawAnchorLinks / DrawConstraints | Downcast from `SoftBody*` to `Rope*`/`Cloth*` described as "via type-safe enum or virtual dispatch" without committing. Which? | Use virtual `GetBodyType()` returning an enum on `SoftBody` base — no RTTI cost in Release. Implementers should add this to `SoftBody` before implementing the debugger. |
| 5 | DrawConstraints | If `Cloth` uses a flat `DistanceConstraint` array with no type field, how does the debugger colour-code structural vs shear vs bend constraints? | Cloth must expose constraint type per index. Recommend `virtual ConstraintType GetConstraintType(int index) const = 0` on `SoftBody` base, or a typed accessor on `Cloth` directly. |
| 6 | Dependencies | DiaGraphics isolation is "enforced by code review only." Is this adequate? | Adequate for this codebase. Known limitation: linker-level enforcement may be added in a future refactor if the codebase grows. |
| 7 | DrawVelocities | Raw displacement arrows could be very large for fast-moving particles. Should there be clamping or scaling? | No clamping — large arrows are intentional (they reveal fast-moving particles). Rendering layer can apply viewport scaling if needed. |
| 8 | Acceptance Criteria (AC11) | Beyond an empty body list, are there other null-pointer risks in Draw? | Anchor null checks are already explicit in DrawAnchorLinks. Main risk is out-of-range `GetParticle()` calls; mitigated by internal bounds-checking on `SoftBody`. |
| 9 | DrawConstraints | Should torn constraints be briefly highlighted (e.g., red flash) to aid tearing debugging? | Out of scope for v1. Developers should enable the `Physics` channel (physics-logging spec) to observe torn-constraint messages in the log. Defer to a future feature spec. |
| 10 | DrawConstraints | Could multi-body constraint sharing cause double-drawing? | Not applicable — soft-soft coupling is explicitly out of scope for v1. Each constraint is owned by exactly one body. |

## Status

`Deferred` — moved to separate spec: DiaSoftBody2DVisualDebugger. DiaGraphics dependency makes it a distinct deliverable.
