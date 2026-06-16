# System Spec: DiaGeometryBridge

## Parent Application
@docs/specs/applications/dia/dia.md

## Purpose

DiaGeometryBridge is the cross-dimension helper system for the Dia engine. It provides shape-level conversions between `Dia::Geometry2D::` and `Dia::Geometry3D::` types: **projection** (3D→2D, drop an axis) and **lifting** (2D→3D, add an axis with a caller-supplied extent). It is the only Dia module that depends on both DiaGeometry2D and DiaGeometry3D.

The system exists because vector-level dimension conversions already live in `DiaMaths/Vector/VectorUtils.h` (`Vector3DXYFromVector2D` and friends), but no equivalent exists for *shapes*. Without a bridge, callers reinvent ad-hoc conversions wherever 2D and 3D meet — top-down minimap projection, HUD bounding rectangles for 3D objects, lifting 2D triggers into 3D worlds, etc.

DiaGeometryBridge is deliberately small and stateless. It holds no scene data, owns no shape types, runs no spatial queries. It only converts between shape representations.

**Dependency chain:**  
`DiaGeometryBridge → { DiaGeometry2D, DiaGeometry3D } → DiaMaths → DiaCore`

**Coordinate convention:** inherited from DiaGeometry3D — Y-up, right-handed.

## Responsibilities

- Provide free-function conversions in `Dia::GeometryBridge::` namespace
- **Project (3D→2D):** AABB→AARect, Sphere→Circle, Triangle→Triangle, Frustum→ConvexPolygon (or `AARect` bound), Ray→Ray, Plane→Line (where the plane intersects the projection plane)
- **Lift (2D→3D):** AARect→AABB, Circle→Sphere (planar disc) or AABB (extruded billboard), Triangle→Triangle, Ray→Ray
- Define an `Axis2D` enum for projection-plane selection (`XY`, `XZ`, `YZ`)
- Provide a `DiaGeometryBridge.vcxproj` static library project, registered in `Cluiche.sln`
- Provide a `dia.geometrybridge.architecture.module.md` YAML module documentation file
- Provide a `Testing/` subdirectory inside the module for shared test fixtures

## Non-Responsibilities

- Owning shape types — those live in DiaGeometry2D / DiaGeometry3D
- Owning vector-level conversions — those live in `DiaMaths/Vector/VectorUtils.h`
- Replicating intersection logic — bridge does not do "does this 3D AABB intersect that 2D AARect"; caller projects then calls 2D intersection tests
- Spatial structures, scene data, transforms — not the bridge's concern
- Rendering, debug drawing, serialization — out of scope for the geometry layer
- Cross-dimension transforms (e.g. lifting a 2D Transform into a 3D Transform) — Transform2D and Transform3D are distinct types under DiaMaths; if a use case emerges, a separate bridge feature is added

## Public Interfaces

### Namespace

All helpers live in `Dia::GeometryBridge::` namespace as free functions (per SD-003). No static class wrapper, no methods on shape types.

### Axis Enum

```cpp
namespace Dia::GeometryBridge {
    enum class Axis2D {
        kXY,   // drop Z (front view)
        kXZ,   // drop Y (top-down for Y-up worlds — the common case)
        kYZ,   // drop X (side view)
    };
}
```

### Projection (3D → 2D)

All projection helpers require an explicit `Axis2D` argument. There is no default — a library cannot guess whether the caller wants top-down, front, or side view.

```cpp
namespace Dia::GeometryBridge {

    // AABB → AARect: take the 3D box's extent on the surviving two axes.
    Dia::Geometry2D::AARect    Project(const Dia::Geometry3D::AABB& aabb, Axis2D plane);

    // Sphere → Circle: same center (projected), same radius.
    Dia::Geometry2D::Circle    Project(const Dia::Geometry3D::Sphere& sphere, Axis2D plane);

    // Triangle → Triangle: project each vertex.
    Dia::Geometry2D::Triangle  Project(const Dia::Geometry3D::Triangle& tri, Axis2D plane);

    // Frustum → AARect bound: cheap conservative 2D bound for top-down minimap-style culling.
    // Use ProjectToPolygon for an exact convex hull.
    Dia::Geometry2D::AARect    Project(const Dia::Geometry3D::Frustum& frustum, Axis2D plane);

    // Frustum → ConvexPolygon: exact convex hull of the projected frustum corners.
    // Note: ConvexPolygon does not exist yet in DiaGeometry2D (listed in its system spec
    // as planned). Until then, this overload is deferred via SD-004.
    // Dia::Geometry2D::ConvexPolygon Project(const Dia::Geometry3D::Frustum& frustum, Axis2D plane);

    // Ray → Ray: project origin and direction; renormalises direction.
    // If the projected direction has near-zero length (ray points along the dropped axis),
    // returns a zero-length Ray and asserts in debug — caller must check.
    Dia::Geometry2D::Ray       Project(const Dia::Geometry3D::Ray& ray, Axis2D plane);

    // Plane → Line: line of intersection between the 3D plane and the projection plane.
    // If the planes are parallel, returns a degenerate Line and asserts in debug.
    Dia::Geometry2D::Line      Project(const Dia::Geometry3D::Plane& plane3, Axis2D plane);

    // Vector3D → Vector2D convenience (delegates to DiaMaths::VectorUtils).
    Dia::Maths::Vector2D       Project(const Dia::Maths::Vector3D& point, Axis2D plane);
}
```

### Lifting (2D → 3D)

All lift helpers require the caller to supply the extent on the missing axis. There is no default thickness — callers know whether they want zero-volume planar geometry, billboard volumes, or extruded boxes.

```cpp
namespace Dia::GeometryBridge {

    // AARect → AABB: extrude the 2D rectangle along the missing axis.
    // missingAxisMin and missingAxisMax define the extent on the dropped axis.
    Dia::Geometry3D::AABB     Lift(const Dia::Geometry2D::AARect& rect,
                                   Axis2D plane,
                                   float missingAxisMin, float missingAxisMax);

    // Circle → Sphere (planar disc, infinitesimal thickness on missing axis).
    // missingAxisValue places the sphere center on the dropped axis.
    Dia::Geometry3D::Sphere   LiftToSphere(const Dia::Geometry2D::Circle& circle,
                                           Axis2D plane,
                                           float missingAxisValue);

    // Circle → AABB (extruded billboard volume).
    // Useful when feeding 2D circles into a SpatialGrid3D as bounds.
    Dia::Geometry3D::AABB     LiftToAABB(const Dia::Geometry2D::Circle& circle,
                                         Axis2D plane,
                                         float missingAxisMin, float missingAxisMax);

    // Triangle → Triangle: lift each vertex.
    Dia::Geometry3D::Triangle Lift(const Dia::Geometry2D::Triangle& tri,
                                   Axis2D plane,
                                   float missingAxisValue);

    // Ray → Ray: lift origin to the supplied missing-axis value; direction stays planar
    // (no missing-axis component).
    Dia::Geometry3D::Ray      Lift(const Dia::Geometry2D::Ray& ray,
                                   Axis2D plane,
                                   float missingAxisValue);

    // Vector2D → Vector3D convenience (delegates to DiaMaths::VectorUtils).
    Dia::Maths::Vector3D      Lift(const Dia::Maths::Vector2D& point,
                                   Axis2D plane,
                                   float missingAxisValue);
}
```

## Features

| Feature | Description | Spec | Status |
|---------|-------------|------|--------|
| Projection Helpers (3D→2D) | `Project()` overloads for AABB, Sphere, Triangle, Frustum, Ray, Plane, Vector3D. Mandatory `Axis2D` parameter. | [projection-helpers.md](projection-helpers.md) | Draft |
| Lift Helpers (2D→3D) | `Lift()`, `LiftToSphere()`, `LiftToAABB()` overloads for AARect, Circle, Triangle, Ray, Vector2D. Caller-supplied missing-axis extent. | [lift-helpers.md](lift-helpers.md) | Draft |

Future features (not authored this round): Frustum→ConvexPolygon (gated on DiaGeometry2D adding ConvexPolygon), OOBB→OORect projection, Capsule projections, lift-with-Transform3D-applied helpers.

## Dependencies on Other Systems

**Required:**
- **DiaGeometry2D** — consumes `AARect`, `Circle`, `Triangle`, `Ray`, `Line`, `IntersectionClassify` (read-only consumer)
- **DiaGeometry3D** — consumes `AABB`, `Sphere`, `Triangle`, `Ray`, `Plane`, `Frustum` (read-only consumer)
- **DiaMaths** — `Vector2D`, `Vector3D`, `VectorUtils` for vector-level conversions
- **DiaCore** — Assertions (`DIA_ASSERT`)

**Dependents (planned):**
- Renderer minimap / 2D HUD systems projecting 3D scene bounds for top-down display
- 2D editor tooling that authors gameplay regions and lifts them into 3D worlds
- Future cross-dimensional debug visualisation

This is the **only** Dia system that depends on both DiaGeometry2D and DiaGeometry3D simultaneously. DiaGeometry2D and DiaGeometry3D themselves remain dimension-pure.

## Out of Scope

- Cross-dimension Transform conversions — separate concern (Transform2D ↔ Transform3D); add as a feature if a consumer needs it
- Lift / project helpers that bake a Transform3D into the conversion (e.g. project under a camera matrix) — that is renderer-side concern, not geometry-bridge
- Frustum → exact 2D ConvexPolygon — gated on DiaGeometry2D's ConvexPolygon type existing (currently planned, not implemented)
- 4D / homogeneous-coordinate helpers — `DiaMaths::VectorUtils` already covers point-vs-direction Vector4D conversions
- OOBB / Capsule / Cylinder projection — defer to future features once a real consumer needs them
- Performance-tuned batch operations (project an array of 1000 AABBs in one call) — caller can loop; add a batch overload if profiling demands it
- Round-trip exactness guarantees beyond axis-aligned cases — projecting an OOBB then lifting gives an AABB (axis-aligned), not the original OOBB

## Decisions

| ID | Decision | Rationale | Scope | Status | Binding |
|----|----------|-----------|-------|--------|---------|
| SD-001 | Namespace `Dia::GeometryBridge::` | Sibling to `Dia::Geometry2D::` and `Dia::Geometry3D::`; signals dimension-bridging role; concise enough to import per-call-site | All features | Accepted | Yes |
| SD-002 | DiaGeometryBridge is the only Dia module that depends on both DiaGeometry2D and DiaGeometry3D | Keeps both shape modules dimension-pure. Forbids upstream modules from acquiring transitive cross-dimension dependencies through DiaGeometry2D or DiaGeometry3D | All features | Accepted | Yes |
| SD-003 | Helpers are free functions, not static-class methods or shape methods | Lightweight; matches `DiaMaths::VectorUtils` pattern for the cross-dimension role; avoids polluting shape types in DiaGeometry2D/3D with bridge methods (which would force them to know about each other) | All features | Accepted | Yes |
| SD-004 | `Axis2D` enum is the projection-plane selector; mandatory parameter (no default) | A library cannot guess whether top-down, front, or side view is wanted. `kXZ` is the common case for Y-up worlds but explicitness is more important than convenience for an API consumed by many distinct use cases | All projection helpers | Accepted | Yes |
| SD-005 | Lift helpers require the caller to supply the missing-axis extent (no default) | Common-case extents differ wildly: zero for planar HUD markers, full world height for triggers, billboard radius for sprite volumes. Default would be wrong for most. Forces explicit choice | All lift helpers | Accepted | Yes |
| SD-006 | Y-up, right-handed coordinate convention | Inherited from DiaGeometry3D SD-006. Bridge does not reinterpret axes | All features | Accepted | Yes |
| SD-007 | DiaGeometryBridge is a static library with no STL containers in public API | Reinforces PD-004 / AD-002 | All features | Accepted | Yes |
| SD-008 | Projection of degenerate cases (ray along dropped axis, plane parallel to projection plane) returns a degenerate result and asserts in debug | Caller must check for degenerate output. Library does not throw; library does not silently return identity | All projection helpers | Accepted | Yes |
| SD-009 | `Circle` lift offers two flavors: `LiftToSphere` (planar disc) and `LiftToAABB` (extruded billboard volume) | Two distinct use cases. Naming the variants explicitly avoids ambiguity from a single overloaded `Lift(Circle)` that picks one arbitrarily | Lift Helpers | Accepted | Yes |
| SD-010 | DiaGeometryBridge does not own any shape types — every type referenced in its public API is owned elsewhere (DiaGeometry2D, DiaGeometry3D, or DiaMaths) | Single-source-of-truth for shapes; bridge is purely transformational | All features | Accepted | Yes |
| SD-011 | Test utilities ship under `Dia/DiaGeometryBridge/Testing/` | Per project memory: test helpers live with the library, not in GoogleTests. Hosts canonical projection round-trip helpers, axis-aligned fixture builders | All features | Accepted | Yes |

**Status values:** `Proposed` · `Accepted` · `Rejected` · `Superseded`  
**Binding:** `Yes` = enforced constraint on all features in this system · `No` = guidance only

## Inherited Binding Decisions

| ID | Source | Decision | Implication for this system |
|----|--------|----------|----------------------------|
| PD-001 | Platform | StringCRC for all entity/component IDs | Module identified by StringCRC (`kUniqueId`); bridge has no entities of its own |
| PD-004 | Platform | No STL containers in public APIs | All public bridge APIs use only POD, builtin types, or geometry/maths types — no `std::vector` exposure |
| PD-005 | Platform | x64 only | `DiaGeometryBridge.vcxproj` targets x64 exclusively |
| PD-006 | Platform | Visual Studio project files are source of truth | `DiaGeometryBridge.vcxproj` and `.vcxproj.filters` updated for every new file |
| PD-007 | Platform | C++20 required | All code compiled under `/std:c++20` |
| PD-008 | Platform | Directory.Build.props owns OutDir/IntDir/toolchain | `DiaGeometryBridge.vcxproj` must NOT override OutDir, IntDir, PlatformToolset, WindowsTargetPlatformVersion, or LanguageStandard |
| AD-001 | Dia App | Module system with YAML frontmatter documentation | Create `dia.geometrybridge.architecture.module.md` with public API, responsibilities, and dependency declarations |
| AD-002 | Dia App | No STL containers in public APIs | Reinforces PD-004 |
| AD-003 | Dia App | Namespace `Dia::<Module>::` | All code in `Dia::GeometryBridge::` namespace per SD-001 |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Naming | Should the namespace be `Dia::GeometryBridge::` or shorter `Dia::GeoBridge::`? | `Dia::GeometryBridge::` for full-word consistency with `Dia::Geometry2D::` and `Dia::Geometry3D::`. The codebase prefers full words (per `update-config` skill notes about other modules in this codebase). Per-call-site `using namespace Dia::GeometryBridge;` shortens at the consumer side. |
| 2 | API style | SD-003 chose free functions over static class. But `Dia::Geometry2D::IntersectionTests` (system spec SD-003) is a static class. Why diverge? | IntersectionTests groups many overloads operating on heterogeneous shape pairs and benefits from a class-name discriminator (`IntersectionTests::Test(a, b)`). Bridge helpers split cleanly into Project-vs-Lift, with the verb itself acting as the discriminator. A static-class `Bridge::Project()` adds a token without disambiguating. Free functions match `DiaMaths::VectorUtils` precedent for cross-dimension utilities. |
| 3 | Default plane | SD-004 forbids a default `Axis2D`. But the most common case (top-down minimap for Y-up worlds) is `kXZ`. Won't callers find this annoying? | Yes, slightly. Tradeoff accepted: a default is a silent assumption that breaks for the second-most-common case. Editor tooling that authors 2D top-down content can wrap with `auto MyProject = [](auto x) { return Project(x, Axis2D::kXZ); };` once and avoid repetition. |
| 4 | Frustum projection | `Project(Frustum, Axis2D)` is listed but `Frustum→ConvexPolygon` is deferred. What does the AARect overload return for a frustum that is not axis-aligned? | The conservative AABB bound of the projected frustum corners. Cheap, useful for minimap culling. Documented in the projection-helpers feature spec; tests cover the conservative-bound semantics. The exact polygon variant is deferred per SD-004 of DiaGeometry2D's planned `ConvexPolygon` type. |
| 5 | Lift / extrude | When `LiftToAABB(Circle, kXZ, yMin, yMax)` is called with `yMax < yMin`, what happens? | Asserts in debug; in release, swaps the values. AABB invariants require `min ≤ max` per axis. Documented in feature spec. |
| 6 | OOBB | OOBB is in DiaGeometry3D but no `Project(OOBB)` is listed. Is this intentional? | Yes — deferred. Projecting an OOBB into 2D yields either an AARect (loose, axis-aligned bound) or an OORect (tight, oriented). Both are useful but require an OORect orientation policy that the editor / minimap consumer hasn't specified yet. Add when a real use case appears. |
| 7 | Round-trip | Is `Lift(Project(aabb, kXZ), kXZ, originalY.min, originalY.max) == aabb`? | Yes — for axis-aligned AABBs, projection followed by lift with the recovered missing-axis extent is exact. Tests include this round-trip on canonical inputs. Out-of-scope: round-trip exactness for OOBB (impossible without orientation), Sphere↔Circle (depends on which lift variant), Frustum (lossy by definition). |
| 8 | Dependency exclusivity | SD-002 makes DiaGeometryBridge "the only" cross-dimension module. What if a future module legitimately needs both? | The existence of one bridge does not preclude another. If e.g. DiaPhysicsBridge later needs to bridge 2D physics and 3D physics, that's a separate module with its own scope. SD-002's intent is to keep DiaGeometry2D and DiaGeometry3D themselves dimension-pure — not to forbid all future cross-dimension work. |
| 9 | Approval gate | Per CLAUDE.md, this system spec cannot be marked Done until child feature specs are Approved. What's the minimum? | Two feature specs: `projection-helpers.md` and `lift-helpers.md`. Each completes all 5 spec steps. The system additionally requires DiaGeometry3D to be `Done` (it cannot bridge to types that don't exist) — DiaGeometry3D's status is the upstream blocker. |
| 10 | Implementation order | Should DiaGeometryBridge ship before, alongside, or after the DiaGeometry3DVisualDebugger? | After. The visual debugger has no cross-dimension dependency; bridge has both. Implementation sequencing in `peppy-weaving-pond.md` already captures this: bridge runs in parallel with steps 2–5 of the 3D module work, but cannot start before DiaGeometry3D shapes exist. |
| 11 | Minimum viable shipping set | Do all listed conversions need to ship in v1, or is a subset acceptable? | Subset acceptable. The driver (3D rendering culling) primarily needs `Project(AABB)`, `Project(Sphere)`, `Project(Frustum)`. Projection-helpers feature spec can scope v1 to those three plus `Project(Vector3D)`; remaining conversions land per consumer demand. Lift-helpers feature spec can scope similarly to AARect→AABB and Vector2D→Vector3D. |
| 12 | Cylindrical lifts | Should `LiftToCylinder(Circle, axis, length)` exist? | Deferred. Cylinder is in DiaGeometry3D for completeness but has no current consumer. If a use case appears (extrude a 2D footprint into a 3D physics volume), add as a feature spec. |
| 13 | Project under a Transform3D | Many real renderer use cases want `Project(aabb, viewMatrix * projMatrix)` — projection through a camera. Why isn't that here? | Out of scope per "Out of Scope" section. That is renderer-side concern (perspective division, clip-space mapping). DiaGeometryBridge handles axis-aligned dimension drops only. A future feature could add transform-baked variants if the renderer module needs them; current driver (top-down spatial-grid minimap) does not. |
| 14 | Test fixtures | What canonical fixtures live in `Dia/DiaGeometryBridge/Testing/`? | Axis-aligned shape builders (canonical AABB, Sphere, AARect, Circle for round-trip tests), epsilon-comparison helpers (`ExpectAARectEqual`, `ExpectAABBEqual`), and a parametric `Axis2D` test fixture so tests run all three planes. Detailed in per-feature plans. |
| 15 | DiaMaths::VectorUtils overlap | The Project / Lift Vector2D / Vector3D helpers duplicate functionality already in `DiaMaths::VectorUtils`. Why expose them on DiaGeometryBridge too? | Convenience and call-site uniformity. A caller projecting an AABB and a Vector3D in the same code block reads better with `Project(aabb, kXZ); Project(point, kXZ);` than mixing namespaces. The bridge versions are thin inline wrappers around `VectorUtils` — zero implementation duplication. |

## Status

`Approved` — Steps 1–4 complete (interview, draft, binding decisions, AI review). Awaiting feature spec authorship and implementation. Plan: implementation plan will be authored as `@docs/specs/systems/dia/diageometrybridge.plan.md` once the projection-helpers and lift-helpers feature specs are both Approved. Hard prerequisite: DiaGeometry3D `Done` (cannot bridge to types that do not exist).
