# Feature Spec: Projection Helpers (3D → 2D)

## Traceability

| Level | Spec | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaGeometryBridge | @docs/specs/systems/dia/diageometrybridge.md |
| Feature | Projection Helpers (3D → 2D) | (this document) |

## Summary

Add the **driver-only v1** subset of `Project()` free-function overloads in the `Dia::GeometryBridge::` namespace: project a 3D shape onto an axis-aligned 2D plane (`Axis2D::kXY`, `kXZ`, `kYZ`) by dropping one axis. V1 covers `AABB`, `Sphere`, `Frustum`, `Vector3D` — exactly what the 3D rendering culling driver needs (top-down minimap projection, screen-space scissor bounds, world-position to 2D map projection).

Triangle / Ray / Plane / OOBB / Capsule / Cylinder projections are **deferred to follow-on feature specs** per system AI Review Q11 and the user direction during the feature batch interview.

## Problem

The renderer-culling driver projects 3D scene bounds onto a 2D plane (typically XZ for a Y-up top-down minimap or screen-space scissor): "given this AABB / Sphere / Frustum, what 2D rectangle does it cover on the XZ plane?" Without a shared bridge, every consumer reimplements the projection inline — risk of subtle axis errors and copy-pasted bugs. Driver-only scope ships the subset that real consumers will hit immediately, not speculative coverage.

## Acceptance Criteria

### Scope (V1 — driver-only)

1. The `Axis2D` enum is defined in `Dia/DiaGeometryBridge/Project/Project.h` (or shared header `Dia/DiaGeometryBridge/Axis2D.h`): `enum class Axis2D { kXY, kXZ, kYZ };`. Mirrors system spec API design.
2. `Project(const Dia::Geometry3D::AABB&, Axis2D)` returns `Dia::Geometry2D::AARect` covering the 3D AABB projected onto the chosen plane.
3. `Project(const Dia::Geometry3D::Sphere&, Axis2D)` returns `Dia::Geometry2D::Circle` with the sphere's center projected and same radius.
4. `Project(const Dia::Geometry3D::Frustum&, Axis2D)` returns `Dia::Geometry2D::AARect` — the conservative AABB bound of the projected frustum corners (per system AI Review Q4). Computes the 8 frustum corner vertices, projects each, returns the bounding 2D rectangle.
5. `Project(const Dia::Maths::Vector3D&, Axis2D)` returns `Dia::Maths::Vector2D` — thin wrapper over `DiaMaths::VectorUtils` for call-site convenience (system AI Review Q15).

### Out of v1 scope (explicitly deferred per follow-on feature specs)

6. `Project(Triangle, Axis2D)`, `Project(Ray, Axis2D)`, `Project(Plane, Axis2D)`, `Project(OOBB, Axis2D)`, `Project(Capsule, Axis2D)`, `Project(Cylinder, Axis2D)`, `Project(Frustum, Axis2D)` returning exact ConvexPolygon — all deferred. Document in spec; do not implement.

### Required behaviour

7. **`Axis2D` is a mandatory parameter — no default** (per system SD-004).
8. **All free functions live in `Dia::GeometryBridge::` namespace** — no static class wrapper, no method-on-shape (per system SD-003).
9. **No STL** in any public method signature.
10. **AABB projection** drops the axis specified: e.g. `kXZ` projection of AABB((0,1,2)→(10,11,12)) returns AARect((0,2)→(10,12)).
11. **Sphere projection** drops the axis from the center: e.g. `kXZ` projection of Sphere(center=(5,5,5), r=3) returns Circle(center=(5,5), r=3). Radius unchanged.
12. **Frustum projection (conservative AABB)** computes 8 corner vertices via the frustum's planes (intersections of three planes), projects each, returns the bounding 2D rectangle. Edge case: degenerate frustum (zero-volume) returns an empty AARect with `min == max`.
13. **Vector3D projection** delegates to `DiaMaths::VectorUtils::Vector2DFromVector3DXY/XZ/ZY` — thin inline wrapper.

### Project file integration

14. New `Dia/DiaGeometryBridge/DiaGeometryBridge.vcxproj` static library project created. Lists the project header(s) and source files. References DiaGeometry2D, DiaGeometry3D, DiaMaths, DiaCore.
15. `.vcxproj.filters` organises files under `Project` filter (and later `Lift` filter for the second feature).
16. `Dia/DiaGeometryBridge/Docs/dia.geometrybridge.architecture.module.md` exists with YAML frontmatter listing public_api as `Project` (and `Lift` from the sibling feature spec). Lists `dia.geometry2d`, `dia.geometry3d`, `dia.maths`, `dia.core` in `dependent_modules`.
17. `Cluiche/Cluiche.sln` adds `DiaGeometryBridge.vcxproj` and the project's dependencies.

### Test utilities

18. `Dia/DiaGeometryBridge/Testing/BridgeTestFactory.h` contains canonical 3D inputs (`MakeUnitFrustum`, `MakeUnitAABB3D`, `MakeOriginSphere`) and projection-aware helpers (`ExpectAARectEqual` with epsilon).

## API Design

```cpp
// Dia/DiaGeometryBridge/Axis2D.h  (or inline in Project.h if small)
namespace Dia::GeometryBridge {
    enum class Axis2D {
        kXY,   // drop Z (front view)
        kXZ,   // drop Y (top-down for Y-up worlds — the common case)
        kYZ,   // drop X (side view)
    };
}

// Dia/DiaGeometryBridge/Project/Project.h
#include "DiaGeometry2D/Shapes/AARect.h"
#include "DiaGeometry2D/Shapes/Circle.h"
#include "DiaGeometry3D/Shapes/AABB.h"
#include "DiaGeometry3D/Shapes/Sphere.h"
#include "DiaGeometry3D/Shapes/Frustum.h"
#include "DiaMaths/Vector/Vector2D.h"
#include "DiaMaths/Vector/Vector3D.h"
#include "DiaGeometryBridge/Axis2D.h"

namespace Dia::GeometryBridge {

// 3D AABB → 2D AARect; drops the axis specified.
Dia::Geometry2D::AARect Project(const Dia::Geometry3D::AABB& aabb, Axis2D plane);

// 3D Sphere → 2D Circle; center projected, radius unchanged.
Dia::Geometry2D::Circle Project(const Dia::Geometry3D::Sphere& sphere, Axis2D plane);

// 3D Frustum → conservative 2D AARect bound. Computes the 8 frustum corner
// vertices and returns the bounding 2D rectangle of their projections.
Dia::Geometry2D::AARect Project(const Dia::Geometry3D::Frustum& frustum, Axis2D plane);

// 3D point → 2D point. Delegates to DiaMaths::VectorUtils.
Dia::Maths::Vector2D Project(const Dia::Maths::Vector3D& point, Axis2D plane);

// Deferred (NOT in v1): Triangle, Ray, Plane, OOBB, Capsule, Cylinder, Frustum→ConvexPolygon

}  // namespace Dia::GeometryBridge
```

## Tasks

| # | Task | Description |
|---|------|-------------|
| 1 | Create `Dia/DiaGeometryBridge/` directory layout | Project/, Lift/ (for sibling feature), Testing/, Docs/ |
| 2 | Create `Axis2D.h` | Enum only |
| 3 | Create `Project/Project.h` | Function declarations per API Design above |
| 4 | Create `Project/Project.cpp` | Implementation. AABB / Sphere / Vector3D projections are 5–10 lines each. Frustum projection: extract 8 corners (intersections of three planes), project each, bound to AARect. |
| 5 | Create `Testing/BridgeTestFactory.h` | Canonical fixtures |
| 6 | Create `Docs/dia.geometrybridge.architecture.module.md` | YAML frontmatter |
| 7 | Create `DiaGeometryBridge.vcxproj` and `.vcxproj.filters` | Static library |
| 8 | Add `DiaGeometryBridge.vcxproj` to `Cluiche/Cluiche.sln` | Wire dependencies on DiaGeometry2D, DiaGeometry3D, DiaMaths, DiaCore |
| 9 | Add tests | `Cluiche/Tests/GoogleTests/GeometryBridge/TestProject.cpp` |
| 10 | Run `dia run googletest --filter="ProjectAABB*:ProjectSphere*:ProjectFrustum*:ProjectVector*"` | All green |

## Test Plan (Task 9)

| Suite | Tests |
|-------|-------|
| `ProjectAABBTest` | Axis-aligned AABB(0,1,2)→(10,11,12): kXY drops Z → AARect((0,1)→(10,11)); kXZ drops Y → AARect((0,2)→(10,12)); kYZ drops X → AARect((1,2)→(11,12)) |
| `ProjectSphereTest` | Sphere(center=(5,5,5), r=3): kXZ → Circle((5,5), 3); radius preserved across all three planes |
| `ProjectFrustumTest` | Identity-projection-frustum (cube as 6 inward planes from -1 to +1): kXZ → AARect((-1,-1)→(1,1)); Y-up RH perspective frustum: projection on kXZ produces AARect bounding the projected far-plane corners |
| `ProjectVectorTest` | Vector3D(5,7,11): kXY → Vector2D(5,7); kXZ → (5,11); kYZ → (7,11). Round-trips with VectorUtils convention |
| `ProjectFrustumDegenerateTest` | Zero-volume frustum (all 6 planes coincident): returns AARect with min == max (caller can detect via `CalculateRadius() == 0`) |

## Files

| File | Action |
|------|--------|
| `Dia/DiaGeometryBridge/Axis2D.h` | Create |
| `Dia/DiaGeometryBridge/Project/Project.h` | Create |
| `Dia/DiaGeometryBridge/Project/Project.cpp` | Create |
| `Dia/DiaGeometryBridge/Testing/BridgeTestFactory.h` | Create |
| `Dia/DiaGeometryBridge/Docs/dia.geometrybridge.architecture.module.md` | Create |
| `Dia/DiaGeometryBridge/DiaGeometryBridge.vcxproj` | Create |
| `Dia/DiaGeometryBridge/DiaGeometryBridge.vcxproj.filters` | Create |
| `Cluiche/Cluiche.sln` | Modify — register DiaGeometryBridge project |
| `Cluiche/Tests/GoogleTests/GeometryBridge/TestProject.cpp` | Create |
| `Cluiche/Tests/GoogleTests/GoogleTests.vcxproj` and `.filters` | Modify — add GeometryBridge filter and link DiaGeometryBridge |

## Dependencies

| Dependency | What this feature uses |
|------------|----------------------|
| DiaGeometry2D — `AARect`, `Circle` | Output types |
| DiaGeometry3D — `AABB`, `Sphere`, `Frustum`, `Plane` | Input types; Frustum corner extraction via plane-plane-plane intersection |
| DiaMaths — `Vector2D`, `Vector3D`, `VectorUtils` | Vector projection |
| DiaCore — `DIA_ASSERT` | (Minimal — projection has few preconditions) |

**Order:** Implements after both `DiaGeometry3D` system spec features (`shape-primitives`, `intersection-tests`) and DiaGeometry2D's existing shapes (already `Done`). Bridge is the last system in the implementation chain.

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-001 | StringCRC for IDs | **Not applicable.** |
| PD-004 | No STL in public API | **Compliant.** All signatures use POD or DiaGeometry / DiaMaths types. |
| PD-005 | x64 only | **Compliant.** |
| PD-006 | VS project files source of truth | **Compliant.** New vcxproj/.filters created manually. |
| PD-007 | C++20 required | **Compliant.** |
| PD-008 | Directory.Build.props owns build settings | **Compliant.** |
| AD-001 | Module YAML | **Compliant.** New `dia.geometrybridge.architecture.module.md`. |
| AD-002 | No STL public APIs | **Compliant.** |
| AD-003 | Namespace `Dia::<Module>::` | **Compliant.** All in `Dia::GeometryBridge::`. |
| SD-001 | `Dia::GeometryBridge::` namespace | **Compliant — directly satisfies.** |
| SD-002 | DiaGeometryBridge is the only Dia module that depends on both DiaGeometry2D and DiaGeometry3D | **Compliant — directly satisfies.** |
| SD-003 | Helpers are free functions, not static-class methods or shape methods | **Compliant — directly satisfies.** |
| SD-004 | `Axis2D` enum is the projection-plane selector; mandatory parameter | **Compliant — directly satisfies.** |
| SD-006 | Y-up RH | **Compliant.** Frustum corner extraction respects the inward-normal RH convention. |
| SD-007 | Static library, no STL in public API | **Compliant.** |
| SD-008 | Degenerate cases (zero-volume frustum) return degenerate result | **Compliant.** AC 12 codifies this. |
| SD-010 | Bridge does not own any shape types | **Compliant.** Pure transformational; uses types owned by DiaGeometry2D/3D and DiaMaths. |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | V1 scope | Why drop Triangle / Ray / Plane / OOBB / Capsule / Cylinder from v1? | System spec AI Review Q11 + user direction in feature interview: ship the driver subset (rendering culling needs AABB, Sphere, Frustum, Vector3D). Other shapes have no immediate consumer; YAGNI says wait for one. Each deferred shape becomes a small follow-on feature spec when needed. |
| 2 | Frustum corner extraction | Plane-plane-plane intersection has corner-case algebra (parallel planes). What's the policy? | The 6 planes of a non-degenerate frustum form 8 valid corners (each corner is the intersection of three adjacent planes — e.g. near + left + top). The implementation enumerates the 8 known plane-triples; degenerate frustums (any plane parallel) return AARect with min == max. Document in source. |
| 3 | Frustum exact polygon | When does the deferred Frustum→ConvexPolygon ship? | After DiaGeometry2D adds `ConvexPolygon` (currently planned in DiaGeometry2D system spec, not implemented). Bridge feature spec at that time. The conservative AABB variant in v1 is sufficient for minimap-style top-down culling. |
| 4 | Axis2D default | Why no default kXZ for Y-up worlds? | System SD-004 forbids defaults. Convenience wrappers per consumer (`auto MyProject = [](auto x){ return Project(x, kXZ); };`) avoid repetition without a library-wide assumption. |
| 5 | Vector3D projection delegation | `Project(Vector3D, Axis2D)` is a thin VectorUtils wrapper. Worth providing? | Yes (system AI Review Q15). Call-site uniformity matters when projecting multiple shapes in the same code path. The wrapper is one line; cost is essentially zero. |
| 6 | Sphere projection radius | Projecting a 3D sphere onto a plane gives a circle with the same radius. Is that always correct? | Yes for axis-aligned dimension drops. The "shadow" of a sphere on any axis-aligned plane is a circle of the sphere's radius. Documented; tested. |
| 7 | AARect coordinate convention | DiaGeometry2D::AARect uses (BottomLeft, TopRight) per `AARect.h:18`. After kXZ projection, what does "BottomLeft" mean for an XZ-plane projection? | The "lower-X, lower-Z" corner. AARect treats its two extents semantically as min/max regardless of physical orientation. Tests verify the projection picks the smaller value for both axes. |
| 8 | Test fixtures | What's in `BridgeTestFactory.h`? | `MakeUnitAABB3D()`, `MakeOriginSphere(r)`, `MakeUnitFrustum()` (cube as 6 inward planes), `MakeYUpPerspectiveFrustum(fov, aspect, near, far)`, `ExpectAARectEqual(a, b, eps)`, `ExpectCircleEqual(a, b, eps)`. Keeps each test concise. |
| 9 | Approval gate | What blocks Approval? | Hard prerequisite: DiaGeometry3D shape-primitives feature implemented (provides AABB, Sphere, Frustum). Spec can be Approved on its own merits. |
| 10 | Sibling feature dependency | The Lift Helpers feature spec (sibling) creates the same `DiaGeometryBridge.vcxproj`. Are tasks 1, 6, 7, 8 duplicated? | Yes — the project layout, vcxproj, sln registration are shared between Project and Lift features. Whichever feature lands first creates them; the second feature only adds its files. Document in implementation order: Project first creates the module skeleton, Lift extends it. |

## Status

`Approved` — Steps 1–4 complete (interview, draft, binding decisions, AI review). First DiaGeometryBridge feature; creates the module skeleton consumed by the Lift feature.
