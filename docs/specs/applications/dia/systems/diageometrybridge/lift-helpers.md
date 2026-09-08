# Feature Spec: Lift Helpers (2D → 3D)

## Traceability

| Level | Spec | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia/dia.md |
| System | DiaGeometryBridge | @docs/specs/applications/dia/systems/diageometrybridge/diageometrybridge.md |
| Feature | Lift Helpers (2D → 3D) | (this document) |

## Summary

Add the **driver-only v1** subset of `Lift()` free-function overloads in the `Dia::GeometryBridge::` namespace: lift a 2D shape into 3D by adding extent on the missing axis (caller-supplied — no defaults per system SD-005). V1 covers `AARect → AABB` and `Vector2D → Vector3D` — the minimum needed for a renderer / editor that authors 2D regions and lifts them into a 3D world (e.g. trigger volumes, ground markers, path corridors).

`Circle → Sphere` / `Circle → AABB`, `Triangle`, `Ray` lifts are **deferred to follow-on feature specs** per system AI Review Q11 and user direction during the feature batch interview.

## Problem

Editor tooling and minimap consumers frequently author content in 2D and need to project it into the 3D world: a 2D rectangle drawn on a top-down minimap becomes an AABB extruded along the Y axis (Y-up worlds); a 2D point becomes a 3D point with a chosen Y. Without `Lift()`, every consumer reinvents this with axis-permutation bugs and inconsistent conventions for the missing-axis extent.

## Acceptance Criteria

### Scope (V1 — driver-only)

1. **`Lift(AARect, Axis2D, missingAxisMin, missingAxisMax)`** returns `Dia::Geometry3D::AABB`. Extrudes the 2D rectangle along the dropped axis between `missingAxisMin` and `missingAxisMax`.
2. **`Lift(Vector2D, Axis2D, missingAxisValue)`** returns `Dia::Maths::Vector3D` — thin wrapper over `DiaMaths::VectorUtils::Vector3DXYFromVector2D` etc.

### Out of v1 scope (explicitly deferred)

3. `LiftToSphere(Circle, ...)`, `LiftToAABB(Circle, ...)`, `Lift(Triangle, ...)`, `Lift(Ray, ...)` — all deferred. Document in spec; do not implement. Each follow-on feature spec adds one variant per consumer demand.

### Required behaviour

4. **`Axis2D` is mandatory.** Same enum as projection-helpers; defined in `Dia/DiaGeometryBridge/Axis2D.h` (created by sibling feature; this feature consumes it).
5. **Caller supplies the missing-axis extent.** No default per system SD-005. `missingAxisMin` and `missingAxisMax` for AARect→AABB; single `missingAxisValue` for Vector2D→Vector3D.
6. **All free functions live in `Dia::GeometryBridge::` namespace.**
7. **No STL** in any public method signature.
8. **AARect lift** semantics: `Lift(AARect((0,0)→(10,10)), Axis2D::kXZ, missingAxisMin=0, missingAxisMax=5)` returns `AABB((0,0,0)→(10,5,10))`. The 2D rectangle's first axis maps to 3D X, its second axis to 3D Z (or whatever survives Axis2D's drop), and the missing axis covers `[missingAxisMin, missingAxisMax]`.
9. **AARect lift assert:** if `missingAxisMin > missingAxisMax`, debug builds assert; release silently swaps them (per system AI Review Q5). The resulting AABB always satisfies `min ≤ max` per axis.
10. **Vector2D lift** semantics: `Lift(Vector2D(5,7), Axis2D::kXZ, missingAxisValue=2.5f)` returns `Vector3D(5, 2.5f, 7)`.
11. **Coordinate mapping (canonical):**
    - `kXY` → 2D (x,y) maps to 3D (x, y, missingAxis); missing axis = Z.
    - `kXZ` → 2D (x,y) maps to 3D (x, missingAxis, y); missing axis = Y. (Top-down for Y-up worlds.)
    - `kYZ` → 2D (x,y) maps to 3D (missingAxis, x, y); missing axis = X.
    
    Documented prominently in source. Tests verify exact mapping for all three planes.

### Project file integration

12. Lift functions live in `Dia/DiaGeometryBridge/Lift/Lift.h` and `Lift.cpp`. Added to the existing `DiaGeometryBridge.vcxproj` (created by the sibling Project feature) under a new `Lift` filter in the `.vcxproj.filters` file.
13. `dia.geometrybridge.architecture.module.md` updated to list `Lift` alongside `Project` in `public_api.entry_points`. The doc is created by whichever feature lands first; the second feature appends.

## API Design

```cpp
// Dia/DiaGeometryBridge/Lift/Lift.h
#include "DiaGeometry2D/Shapes/AARect.h"
#include "DiaGeometry3D/Shapes/AABB.h"
#include "DiaMaths/Vector/Vector2D.h"
#include "DiaMaths/Vector/Vector3D.h"
#include "DiaGeometryBridge/Axis2D.h"

namespace Dia::GeometryBridge {

// 2D AARect → 3D AABB; extrudes along the missing axis between min and max.
//
// Coordinate mapping:
//   kXY → (x, y, missingAxis)         (missing axis = Z)
//   kXZ → (x, missingAxis, y)         (missing axis = Y; top-down for Y-up)
//   kYZ → (missingAxis, x, y)         (missing axis = X)
//
// missingAxisMin and missingAxisMax define the extent on the dropped axis.
// Asserts in debug if missingAxisMin > missingAxisMax; release swaps them.
Dia::Geometry3D::AABB Lift(const Dia::Geometry2D::AARect& rect,
                           Axis2D plane,
                           float  missingAxisMin,
                           float  missingAxisMax);

// 2D point → 3D point. Thin wrapper over DiaMaths::VectorUtils.
Dia::Maths::Vector3D Lift(const Dia::Maths::Vector2D& point,
                          Axis2D plane,
                          float  missingAxisValue);

// Deferred (NOT in v1): LiftToSphere(Circle, ...), LiftToAABB(Circle, ...),
//                       Lift(Triangle, ...), Lift(Ray, ...)

}  // namespace Dia::GeometryBridge
```

## Tasks

| # | Task | Description |
|---|------|-------------|
| 1 | Create `Dia/DiaGeometryBridge/Lift/Lift.h` | Function declarations |
| 2 | Create `Dia/DiaGeometryBridge/Lift/Lift.cpp` | Implementation. AARect lift: per-axis combinatorics for kXY/kXZ/kYZ; Vector2D lift delegates to VectorUtils. |
| 3 | Update `Dia/DiaGeometryBridge/Docs/dia.geometrybridge.architecture.module.md` | Add `Lift` to public_api.entry_points (the doc is created by sibling Project feature; this feature appends) |
| 4 | Update `Dia/DiaGeometryBridge/DiaGeometryBridge.vcxproj` and `.vcxproj.filters` | Register Lift files; add Lift filter |
| 5 | Add tests | `Cluiche/Tests/GoogleTests/GeometryBridge/TestLift.cpp` |
| 6 | Run `dia run googletest --filter="LiftAARect*:LiftVector*"` | All green |

## Test Plan (Task 5)

| Suite | Tests |
|-------|-------|
| `LiftAARectTest` | AARect((0,0)→(10,10)), kXZ, missingAxisMin=0, missingAxisMax=5 → AABB((0,0,0)→(10,5,10)); same input on kXY → AABB((0,0,0)→(10,10,5)); same on kYZ → AABB((0,0,0)→(5,10,10)) |
| `LiftAARectAssertTest` | missingAxisMin > missingAxisMax: debug asserts (DEATH_TEST or equivalent); release silently swaps (resulting AABB has correct min ≤ max) |
| `LiftVectorTest` | Vector2D(5,7) with each Axis2D variant produces the documented Vector3D mapping; round-trips with VectorUtils::Vector3DXYFromVector2D etc. |
| `LiftRoundTripTest` | Project(Lift(rect, kXZ, ymin, ymax), kXZ) == rect (exact). Verified for all three planes; documents the round-trip property called out in system AI Review Q7. |

## Files

| File | Action |
|------|--------|
| `Dia/DiaGeometryBridge/Lift/Lift.h` | Create |
| `Dia/DiaGeometryBridge/Lift/Lift.cpp` | Create |
| `Dia/DiaGeometryBridge/Docs/dia.geometrybridge.architecture.module.md` | Modify — append `Lift` to public_api (file created by Project feature) |
| `Dia/DiaGeometryBridge/DiaGeometryBridge.vcxproj` and `.vcxproj.filters` | Modify — add Lift files and filter |
| `Cluiche/Tests/GoogleTests/GeometryBridge/TestLift.cpp` | Create |
| `Cluiche/Tests/GoogleTests/GoogleTests.vcxproj` and `.filters` | Modify |

## Dependencies

| Dependency | What this feature uses |
|------------|----------------------|
| DiaGeometry2D — `AARect` | Input type |
| DiaGeometry3D — `AABB` | Output type |
| DiaMaths — `Vector2D`, `Vector3D`, `VectorUtils` | Vector lifting |
| DiaGeometryBridge — `Axis2D` enum (created by Project feature) | Plane selector |
| DiaCore — `DIA_ASSERT` | missingAxisMin > max guard |

**Order:** Implements after the sibling Projection Helpers feature (which creates `DiaGeometryBridge.vcxproj` + `Axis2D.h` + `dia.geometrybridge.architecture.module.md`). Cannot land first.

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-001 | StringCRC for IDs | **Not applicable.** |
| PD-004 | No STL in public API | **Compliant.** |
| PD-005 | x64 only | **Compliant.** |
| PD-006 | VS project files source of truth | **Compliant.** |
| PD-007 | C++20 required | **Compliant.** |
| PD-008 | Directory.Build.props owns build settings | **Compliant.** |
| AD-001 | Module YAML | **Compliant.** Existing doc updated. |
| AD-002 | No STL public APIs | **Compliant.** |
| AD-003 | Namespace `Dia::<Module>::` | **Compliant.** `Dia::GeometryBridge::`. |
| SD-001 | `Dia::GeometryBridge::` namespace | **Compliant.** |
| SD-003 | Helpers are free functions | **Compliant — directly satisfies.** |
| SD-005 | Lift helpers require caller-supplied missing-axis extent (no default) | **Compliant — directly satisfies.** |
| SD-006 | Y-up RH | **Compliant.** kXZ is the top-down convention for Y-up; documented. |
| SD-007 | Static library, no STL in public API | **Compliant.** |
| SD-008 | Degenerate cases (min > max) | **Compliant.** AC 9 codifies behaviour. |
| SD-010 | Bridge does not own shape types | **Compliant.** |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | V1 scope | Why drop Circle / Triangle / Ray lifts from v1? | System AI Review Q11 + user direction in feature batch interview: ship the driver subset (renderer / editor needs AARect → AABB and Vector2D → Vector3D for top-down regions and points). Circle has two distinct lift forms (`LiftToSphere` planar, `LiftToAABB` extruded) — both are useful, but neither has an immediate consumer. Add when one appears. |
| 2 | Coordinate mapping | The kXZ mapping puts 2D's "y" into 3D's "z". Is that confusing? | Acknowledged. The 2D AARect doesn't have a Z — it's just (x,y) labels for the rectangle's two axes. After projection, those labels mean "first surviving axis" and "second surviving axis". For kXZ, the first surviving 3D axis is X, the second is Z. So 2D's `y` becomes 3D's `z`. Documented prominently in API and tested across all three planes. |
| 3 | Mandatory extent | Why force the caller to supply missing-axis extent always? | System SD-005 — defaults are wrong for the common case. Editor authoring HUD markers wants extent zero on the missing axis (planar geometry). Trigger volumes want full world height. Billboard geometry wants the camera-distance radius. No single default fits; mandatory parameter avoids the trap. |
| 4 | Round-trip exactness | `Lift(Project(aabb, kXZ), kXZ, originalY.min, originalY.max) == aabb`. Does this always hold? | Yes for axis-aligned AABBs (system AI Review Q7). Verified by `LiftRoundTripTest`. The round-trip property is a useful invariant for editor undo/redo and serialisation testing. |
| 5 | Project / Lift symmetry | Should the API be `Lift(AARect, Axis2D, AABBExtent)` where AABBExtent is a struct holding {min, max}? | No — two floats are simpler and match the system spec API. A struct adds one layer of indirection for no semantic gain. |
| 6 | Vector2D lift cost | `Lift(Vector2D, ...)` is a thin VectorUtils wrapper. Worth providing? | Yes — system AI Review Q15: call-site uniformity matters. Inline wrapper, zero cost. |
| 7 | Module skeleton ownership | Both Project and Lift features touch the same vcxproj and architecture doc. Conflict? | No — sibling features extending one module. Whichever lands first creates the skeleton (Project per recommended implementation order); the second feature appends. AC 12–13 spell this out. |
| 8 | LiftToSphere vs LiftToAABB | Why do those have explicit naming, but the AARect → AABB lift does not need a disambiguator? | AARect → AABB has only one sensible 3D output: an AABB extruded along the missing axis. Circle → 3D has two distinct meanings (planar disc vs billboard volume) and the disambiguator is required. Documented in system AI Review Q9 / SD-009 for when those land. |
| 9 | Test fixtures | What's added to `BridgeTestFactory.h` for Lift testing? | Reuses Project's test factory. Adds: `MakeUnitAARect()`, `ExpectAABBEqual(a, b, eps)`, `ExpectVector3DEqual(a, b, eps)`. |
| 10 | Approval gate | What blocks Approval? | Sibling Projection Helpers feature implementation creates the vcxproj and module skeleton; this feature adds files to it. Spec can be Approved independently. |

## Status

`Approved` — Steps 1–4 complete (interview, draft, binding decisions, AI review). Last DiaGeometryBridge feature; gates the system spec's `Done` status alongside the Projection Helpers feature.
