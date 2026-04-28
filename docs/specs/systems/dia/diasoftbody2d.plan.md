# Plan: DiaSoftBody2D

**Spec:** @docs/specs/systems/dia/diasoftbody2d.md  
**Status:** In Progress  
**Started:** 2026-04-27  
**Last Updated:** 2026-04-27

## Implementation Order Rationale

The dependency chain drives task ordering. Particle is the foundation struct; Rope establishes the constraint pattern (SD-009: rope first, cloth second); Cloth extends it. SoftBodyWorld is the simulation driver — its core PBD loop must exist before geometry collision and rigid body coupling can be wired in. Physics logging is instrumentation inserted into existing SoftBodyWorld methods. Visual debugger is terminal (no dependents) and requires DiaGraphics, so it comes last.

Circular compile-time dependency between SoftBodyWorld and GeometryCollision/RigidBodyCoupling is avoided because both are implemented inside `SoftBodyWorld.cpp` as private methods — no separate translation units.

## Dependencies (External to This System)

DiaSoftBody2D depends on APIs from DiaRigidBody2D that must exist:
- `PhysicsBody::ApplyImpulse()`, `GetTransform()`, `GetInverseMass()`
- `PhysicsWorld::QueryCircle()` (or `QueryRegion()` as fallback broad-phase)
- `DiaGeometry2D::IntersectionTests` circle-vs-shape overloads: `CircleVsAARect`, `CircleVsCircle`, `CircleVsLine` returning `ContactInfo` with `normal` + `penetrationDepth`

**Pre-implementation check:** Verify these APIs exist before starting Task 6 and Task 7. If missing, add them as prerequisite sub-tasks.

## Tasks

| # | Task | Spec | Status | Notes |
|---|------|------|--------|-------|
| 1 | **Project scaffolding** — Create `Dia/DiaSoftBody2D/` directory, `.vcxproj` (static lib, x64, refs DiaCore/DiaMaths/DiaGeometry2D/DiaRigidBody2D), `.vcxproj.filters`, register in `Cluiche.sln` under Library folder, create `dia.softbody2d.architecture.module.md` | System | Done | GUID: `{D2E3F4A5-B6C7-8901-DEFC-112233445566}`. Full solution builds clean. |
| 2 | **Particle + SoftBody base + DistanceConstraint** — `Particle.h` (struct + `DeriveVelocity` free function), `SoftBody.h` (abstract base: virtual `GetId()`, `GetBodyType()` returning enum), `Constraints/DistanceConstraint.h` (internal struct with `Type` enum: kRope/kStructural/kShear/kBend + `active` flag) | [particle.md](../../features/dia/diasoftbody2d/particle.md) | Done | Header-only. Registered in vcxproj + filters. Build clean. |
| 3 | **Rope** — `Rope.h` / `Rope.cpp`: `RopeDef`, construction (evenly spaced particles, distance constraints, invMass distribution, anchor pin override), `GetParticle()`, `IsTorn()`, `GetId()`, anchor accessors, stiffness/maxStretch accessors | [rope.md](../../features/dia/diasoftbody2d/rope.md) | Done | kUniqueId uses `static const` (not constexpr) — StringCRC ctor not constexpr. Anchor type is Body2DBase*. |
| 4 | **Cloth** — `Cloth.h` / `Cloth.cpp`: `ClothDef`, grid construction (structural+shear+bend constraints), `GetParticle(x,y)`, `PinParticle`/`UnpinParticle` (with `mOriginalInvMass` backup), tearing, grid accessors | [cloth.md](../../features/dia/diasoftbody2d/cloth.md) | Done | Uses heap `DynamicArray<T>` (not `DynamicArrayC`) — constraint count up to ~24K for 64x64 grid, too large for static alloc. |
| 5 | **SoftBodyWorld (core PBD loop)** — `SoftBodyWorld.h` / `SoftBodyWorld.cpp`: `WorldDef`, body management (`AddRope`/`AddCloth`/`RemoveBody`/`GetBodies`), static shape registration, `Update()` accumulator, `StepOnce()` calling `ApplyExternalForces` / `ProjectConstraints` / `FinalizeVelocities` / `CheckTearing`. Geometry collision + rigid body coupling are stub no-ops initially. | [soft-body-world.md](../../features/dia/diasoftbody2d/soft-body-world.md) | Done | Anchor overwrites in ApplyExternalForces. XPBD constraint projection. Geometry/RB collision stubs. |
| 6 | **Geometry collision** — Implement `ResolveGeometryCollision()` in `SoftBodyWorld.cpp`: particle-as-circle vs registered AARect/Circle/Line shapes. Positional correction + prevPosition velocity cancellation. | [geometry-collision.md](../../features/dia/diasoftbody2d/geometry-collision.md) | Done | Used ClosestPoint APIs (not IntersectionTests) for contact normal/depth. Static helpers per shape type. Pinned particles still resolved (per spec AC6). |
| 7 | **Rigid body coupling** — Implement anchor position overwrite in `ApplyExternalForces()` + `ResolveRigidBodyCollision()` in `SoftBodyWorld.cpp`: anchor back-impulse + particle-vs-rigid-body collision with back-impulse. No-op when `rigidBodyWorld == nullptr`. | [rigid-body-coupling.md](../../features/dia/diasoftbody2d/rigid-body-coupling.md) | Done | `QueryCircle` confirmed. `const_cast` for `ApplyImpulse`. Anchor bodies excluded from Part B collision to avoid double-impulse. |
| 8 | **Physics logging** — Insert `DIA_LOG_WARNING`/`DIA_LOG_DEBUG` at 3 instrumentation points in `SoftBodyWorld.cpp`: maxSubSteps hit, particle velocity threshold, constraint torn. All `#ifndef NDEBUG`. | [physics-logging.md](../../features/dia/diasoftbody2d/physics-logging.md) | Not Started | No new public API. Internal-only changes. Shared "Physics" channel with DiaRigidBody2D. |
| 9 | **Visual debugger** — `Debug/DiaSoftBodyVisualDebugger.h` / `.cpp`: DrawParticles (white/magenta), DrawConstraints (colour-coded by type), DrawAnchorLinks (yellow), DrawVelocities (green). DiaGraphics includes ONLY in Debug/ subdirectory. | [visual-debugger.md](../../features/dia/diasoftbody2d/visual-debugger.md) | Not Started | Add DiaGraphics include dir + lib dependency to vcxproj. Only file in DiaSoftBody2D that touches DiaGraphics. |
| 10 | **Unit tests** — Create `Cluiche/Tests/GoogleTests/SoftBody2D/` with test files: TestParticle, TestRope, TestCloth, TestSoftBodyWorld, TestGeometryCollision, TestRigidBodyCoupling, TestSoftBodyLogging, TestSoftBodyVisualDebugger. Register in GoogleTests.vcxproj + filters. Add `DiaSoftBody2D.lib` to linker dependencies. | All features | Not Started | One test file per feature spec. Test strategy defined in each feature spec. |
| 11 | **Build verification + vcxproj finalization** — Ensure all source files are listed in vcxproj/filters. Build Debug + Release x64. Run unit tests. Fix any compile/link errors. | All features | Not Started | Final pass. Update vcxproj incrementally during earlier tasks, but do full verification here. |

## File Layout (Target)

```
Dia/DiaSoftBody2D/
  Particle.h
  SoftBody.h
  Rope.h
  Rope.cpp
  Cloth.h
  Cloth.cpp
  SoftBodyWorld.h
  SoftBodyWorld.cpp
  Constraints/
    DistanceConstraint.h
  Debug/
    DiaSoftBodyVisualDebugger.h
    DiaSoftBodyVisualDebugger.cpp
  Docs/
    dia.softbody2d.architecture.module.md
  DiaSoftBody2D.vcxproj
  DiaSoftBody2D.vcxproj.filters

Cluiche/Tests/GoogleTests/SoftBody2D/
  TestParticle.cpp
  TestRope.cpp
  TestCloth.cpp
  TestSoftBodyWorld.cpp
  TestGeometryCollision.cpp
  TestRigidBodyCoupling.cpp
  TestSoftBodyLogging.cpp
  TestSoftBodyVisualDebugger.cpp
```

## Session Notes

### 2026-04-27
- Plan created. All 8 feature specs are Approved. No code exists yet.
- DiaRigidBody2D project structure reviewed as reference for vcxproj patterns.
- Key reference points: GUID pattern, include dirs (`./;./../;`), ProjectReference with `ReferenceOutputAssembly=false`, Library folder nesting in .sln.
- DiaSoftBody2D needs ProjectReferences to DiaCore, DiaMaths, DiaGeometry2D, DiaRigidBody2D (4 deps — one more than DiaRigidBody2D which has 3).
- Visual debugger adds DiaGraphics as a 5th dependency (include dir + lib), but ONLY for the Debug/ subdirectory files.
- Pre-implementation dependency check needed before Tasks 6-7: DiaGeometry2D circle-vs-shape IntersectionTests overloads and DiaRigidBody2D QueryCircle/QueryRegion.
