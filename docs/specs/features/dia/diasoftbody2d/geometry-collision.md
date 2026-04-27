# Feature Spec: Geometry Collision

## Traceability

| Level | Parent | This Feature |
|-------|--------|--------------|
| Platform | @docs/specs/platform/Cluiche.md | - |
| Application | @docs/specs/applications/dia.md | - |
| System | @docs/specs/systems/dia/diasoftbody2d.md | **geometry-collision** |

**Status:** `Approved`

---

## Problem Statement

Soft body particles fall through floors, walls, and obstacles unless their positions are corrected when they penetrate static geometry. Particles have a radius, so the correct collision primitive is a circle. The resolution must push the particle out of the shape and prevent it from sinking through surfaces in subsequent steps by adjusting the previous-position to cancel the velocity component into the surface.

---

## Solution Overview

`ResolveGeometryCollision()` iterates every particle in every body against every registered static shape. For each (particle, shape) pair, it uses `Dia::Geometry2D::IntersectionTests` with the particle modelled as a circle of radius `particle.radius` centred at `particle.position`. If the circle penetrates the shape, the particle's `position` is moved out along the contact normal by the penetration depth, and `prevPosition` is adjusted to zero the velocity component directed into the surface (preventing re-penetration next step).

Three shape types are supported: `AARect`, `Circle`, and `Line`. Each is stored in a separate `DynamicArrayC` of non-owning typed pointers. No broad-phase or spatial structure is used — SD-006 documents that flat iteration is acceptable for particle counts below 200 per body.

---

## Acceptance Criteria

| ID | Criterion | Verification Method |
|----|-----------|---------------------|
| AC1 | A particle falling through a static `AARect` floor is pushed above the floor surface; its `position.y` is >= floor top edge after resolution | Unit test |
| AC2 | A particle resting on a static `AARect` floor (penetrating slightly) stays at rest after resolution; it does not drift through the floor over 60 steps | Integration test |
| AC3 | A particle penetrating a static `Circle` shape is pushed out along the correct radial normal | Unit test |
| AC4 | A particle penetrating a static `Line` shape is pushed to the correct side | Unit test |
| AC5 | After `RemoveStaticShape(ptr)`, that shape no longer causes collision response | Unit test |
| AC6 | A pinned particle (`invMass = 0`) is still resolved by geometry collision (pinning only prevents force integration; geometry should still push it out to avoid tunnelling into walls) | Unit test |
| AC7 | A particle that does not overlap any registered shape has its position unchanged | Unit test |
| AC8 | An empty shape list causes no crash; all particles pass through unaffected | Unit test |
| AC9 | Resolution of multiple shapes in the same step does not corrupt particle state | Unit test: two shapes on either side of a particle |
| AC10 | Full solution builds clean | `msbuild Cluiche.sln /p:Configuration=Debug /p:Platform=x64` |

---

## Public API

The geometry collision feature is implemented as part of `SoftBodyWorld` (specifically `SoftBodyWorld::ResolveGeometryCollision()`). The static shape registration API is on `SoftBodyWorld`:

```cpp
// In SoftBodyWorld (see soft-body-world feature spec for full class definition):
void AddStaticShape(const Dia::Geometry2D::AARect* shape);
void AddStaticShape(const Dia::Geometry2D::Circle* shape);
void AddStaticShape(const Dia::Geometry2D::Line*   shape);
void RemoveStaticShape(const void* shapePtr);
```

No additional public types are required for this feature. All resolution logic is internal to `SoftBodyWorld`.

---

## Implementation Notes

### Internal Storage

```cpp
// Inside SoftBodyWorld private members:
Dia::Core::DynamicArrayC<const Dia::Geometry2D::AARect*>  mStaticRects;
Dia::Core::DynamicArrayC<const Dia::Geometry2D::Circle*>  mStaticCircles;
Dia::Core::DynamicArrayC<const Dia::Geometry2D::Line*>    mStaticLines;
```

### Resolution Algorithm

For each body, for each particle:

```
// Model particle as a circle
Dia::Geometry2D::Circle particleCircle;
particleCircle.centre = particle.position;
particleCircle.radius = particle.radius;

// Check vs each AARect
for (auto* rect : mStaticRects) {
    ContactInfo contact;
    if (IntersectionTests::CircleVsAARect(particleCircle, *rect, contact)) {
        particle.position  += contact.normal * contact.penetrationDepth;
        // Zero velocity into surface:
        // Adjust prevPosition so derived velocity has zero component along normal
        Dia::Maths::Vector2D vel = DeriveVelocity(particle, dt);
        float normalVel = Dot(vel, contact.normal);
        if (normalVel < 0.0f) {  // moving into surface
            particle.prevPosition -= contact.normal * (normalVel * dt);
        }
    }
}
// Repeat for mStaticCircles and mStaticLines with appropriate IntersectionTests overload
```

The velocity-cancellation step works because PBD derives velocity from the position delta. Setting `prevPosition` such that the normal component of the implied velocity is zero prevents the particle from re-penetrating next step.

### Multiple Shape Overlap

When a particle overlaps more than one shape in the same step, each shape is resolved sequentially. This may cause slight over-correction but is stable for small particle counts. No special multi-contact resolution is needed for v1.

### Pinned Particles

Pinned particles (`invMass == 0`) are still geometrically resolved. Geometry collision is a position correction, not a force — the `invMass` guard only applies to force integration and constraint projection. A pinned particle embedded in a wall should still be pushed out.

### IntersectionTests API Expected

This feature depends on `Dia::Geometry2D::IntersectionTests` providing:
- `CircleVsAARect(circle, rect, outContact)` → bool
- `CircleVsCircle(circle, circle, outContact)` → bool
- `CircleVsLine(circle, line, outContact)` → bool

Where `outContact` contains `normal` (Vector2D, pointing away from shape) and `penetrationDepth` (float). If these overloads do not exist in DiaGeometry2D, they must be added as part of this feature's implementation work.

### File Layout

Geometry collision lives entirely inside `SoftBodyWorld.cpp`:
```
Dia/DiaSoftBody2D/
└── SoftBodyWorld.cpp  (ResolveGeometryCollision implementation)
```

---

## Dependencies

### Required Features (must exist first)
- **particle** — `Particle` struct, `DeriveVelocity`
- **soft-body-world** — provides the static shape registration API and calls this step

### Required Modules
- **DiaGeometry2D** — `AARect`, `Circle`, `Line`, `IntersectionTests` (circle vs each shape type)
- **DiaMaths** — `Vector2D`, `Dot()`
- **DiaCore** — `DynamicArrayC`, `DIA_ASSERT`

### Dependent Features
- **rigid-body-coupling** — runs after geometry collision in the same step

---

## Testing Strategy

### Unit Tests (`Cluiche/Tests/GoogleTests/SoftBody2D/TestGeometryCollision.cpp`)

1. **Particle vs AARect floor** — particle at y=-0.05 (floor at y=0, thickness=1); after resolution, `position.y >= 0`
2. **Particle at rest on floor** — 60 steps with gravity; particle does not sink below floor
3. **Particle vs Circle** — particle inside static circle; pushed to outside; normal is radially outward
4. **Particle vs Line** — particle on wrong side of line; pushed to correct side
5. **Shape removal stops collision** — register shape; verify collision; remove shape; verify no collision on next step
6. **Pinned particle resolved** — `invMass=0` particle overlapping wall; position corrected
7. **No overlap — no change** — particle well clear of all shapes; position unchanged after step
8. **Empty shape list** — no shapes registered; particle passes through freely; no crash
9. **Multiple shapes** — two shapes around a particle; both corrections applied; no state corruption

---

## Binding Decisions Compliance

| Decision | Source | Summary | Compliance |
|----------|--------|---------|------------|
| PD-004 | Platform | No STL containers in public APIs | `DynamicArrayC` for all internal shape lists |
| AD-003 | Dia App | Namespace `Dia::<Module>::` | `Dia::SoftBody2D::` throughout |
| SD-006 | System | No spatial structure for static shapes; flat iteration | Three typed `DynamicArrayC` lists; O(particles × shapes) flat iteration |
| SD-007 | System | Velocity derived from position delta | `prevPosition` adjusted to cancel velocity into surface; no separate velocity field |
| SD-008 | System | No STL in public API | No STL in shape registration API or internal storage |
