# System Spec: DiaGeometry3D

## Parent Application
@docs/specs/applications/dia.md

## Purpose

DiaGeometry3D is the 3D geometry system for the Dia engine. It owns all 3D geometric primitives (shapes), pairwise intersection tests, and spatial acceleration structures (uniform grid; future octree, BVH3D). It is the 3D sibling of DiaGeometry2D and follows the same architectural pattern: shape primitives, intersection tests, and an `ISpatialStructure3D<T>` interface implemented by `SpatialGrid3D`. Visual debugging is a separate system (DiaGeometry3DVisualDebugger). Cross-dimensional helpers are a separate system (DiaGeometryBridge).

The primary driver for shipping DiaGeometry3D first (ahead of DiaRigidBody3D or other 3D consumers) is **3D rendering culling** — `Frustum` vs `AABB` queries against scene objects. This shapes the priority order of the query API and the choice of primitives included in the first round.

**Coordinate convention:** Y-up, right-handed.

**Dependency chain:**  
`DiaRigidBody3D (future) → DiaGeometry3D → DiaMaths → DiaCore`

DiaGeometry3D **does not** own the underlying linear algebra. `Vector3D::Cross`, `Matrix44`, `Matrix34`, `Quaternion`, and `Transform3D` are pure linear algebra and live in DiaMaths. They are tracked under a separate DiaMaths system spec (TBD per @docs/specs/applications/dia.md:49) — DiaGeometry3D consumes them.

## Responsibilities

- Own all 3D geometric primitive types: `AABB`, `OOBB`, `Sphere`, `Capsule`, `Triangle`, `Cylinder`, `Ray`, `Plane`, `Frustum` (9 types)
- Provide pairwise intersection tests via `IntersectionTests` static API; SAT and fast paths for the priority pairs (AABB-vs-AABB, AABB-vs-Sphere, AABB-vs-Frustum, Sphere-vs-Frustum, Triangle-vs-AABB, Ray vs AABB/Sphere/Triangle/Plane); full GJK+EPA deferred
- Provide point-containment and closest-point queries between shapes
- Provide `ISpatialStructure3D<T>` interface and `SpatialGrid3D<T, MaxObjects, MaxCells>` implementation with six query types (Region, Sphere, Point, Ray, **Frustum**, KNearest)
- Expose a consistent `Dia::Geometry3D::` namespace for all types
- Provide a `DiaGeometry3D.vcxproj` static library project, registered in `Cluiche.sln`
- Provide a `dia.geometry3d.architecture.module.md` YAML module documentation file
- Provide a `Testing/` subdirectory inside the module for shared test fixtures (per project memory: test utilities ship with the library they test, not in GoogleTests)

## Non-Responsibilities

- Pure math (vectors, matrices, quaternions, angles, trig) — DiaMaths
- 2D geometry — DiaGeometry2D
- 2D↔3D conversions and projections — DiaGeometryBridge (separate system spec)
- Visual debug rendering of 3D shapes / grid — DiaGeometry3DVisualDebugger (separate system spec)
- Physics simulation (rigid bodies, forces, collision response) — future DiaRigidBody3D
- Rendering or general debug drawing — DiaGraphics / DiaVisualDebugger
- Scene management beyond what intersection/spatial queries need — future DiaScene
- Serialization of geometry data (mirror `JsonSpatialGridSerializer` pattern when needed; out of this round)
- Convex hull / mesh decomposition — out of scope for the geometry layer

## Public Interfaces

### Namespace

All types live under `Dia::Geometry3D::`. The dimension is captured by the namespace, mirroring `Dia::Geometry2D::`.

### Shape Primitives

```cpp
namespace Dia::Geometry3D {
    // Axis-aligned bounding box (Vector3D min/max corners)
    class AABB { ... };
    // Oriented bounding box (center + half-extents + Quaternion orientation)
    class OOBB { ... };
    // Sphere (center + radius)
    class Sphere { ... };
    // Capsule (two endpoints + radius)
    class Capsule { ... };
    // Triangle (three Vector3D vertices)
    class Triangle { ... };
    // Cylinder (two endpoints on the axis + radius)
    class Cylinder { ... };
    // Ray (origin + normalized direction, half-infinite)
    class Ray { ... };
    // Plane (normal + d, plane equation form)
    class Plane { ... };
    // View frustum (six Plane: near/far/left/right/top/bottom)
    class Frustum { ... };
}
```

### Intersection Tests

```cpp
namespace Dia::Geometry3D {
    enum class IntersectionClassify {
        kNoIntersection,
        kPenetrating,
        kAContainsB,
        kBContainsA
    };

    class IntersectionTests {
    public:
        // Priority pairs (3D rendering culling):
        static IntersectionClassify Test(const AABB& a, const Frustum& b);
        static IntersectionClassify Test(const Sphere& a, const Frustum& b);
        static IntersectionClassify Test(const Triangle& a, const Frustum& b);

        // Bounding-volume pairs:
        static IntersectionClassify Test(const AABB& a, const AABB& b);
        static IntersectionClassify Test(const AABB& a, const Sphere& b);
        static IntersectionClassify Test(const Sphere& a, const Sphere& b);
        static IntersectionClassify Test(const AABB& a, const OOBB& b);
        static IntersectionClassify Test(const OOBB& a, const OOBB& b);     // SAT

        // Triangle-vs-volume (mesh culling):
        static IntersectionClassify Test(const Triangle& a, const AABB& b);
        static IntersectionClassify Test(const Triangle& a, const Sphere& b);

        // Ray casts:
        static IntersectionClassify Test(const Ray& a, const AABB& b);
        static IntersectionClassify Test(const Ray& a, const Sphere& b);
        static IntersectionClassify Test(const Ray& a, const Triangle& b);
        static IntersectionClassify Test(const Ray& a, const Plane& b);

        // Capsule, Cylinder pairs: SAT or fast paths added as needed by consumers.

        // Point containment:
        static bool Contains(const AABB& shape,    const Dia::Maths::Vector3D& point);
        static bool Contains(const Sphere& shape,  const Dia::Maths::Vector3D& point);
        static bool Contains(const Frustum& shape, const Dia::Maths::Vector3D& point);
        static bool Contains(const OOBB& shape,    const Dia::Maths::Vector3D& point);
        // ... (all shapes)

        // Closest point:
        static Dia::Maths::Vector3D ClosestPoint(const AABB& shape,   const Dia::Maths::Vector3D& point);
        static Dia::Maths::Vector3D ClosestPoint(const Sphere& shape, const Dia::Maths::Vector3D& point);
        // ... (all shapes)
    };
}
```

### Spatial Structures

```cpp
namespace Dia::Geometry3D {

    static constexpr unsigned int kMaxQueryResults = 1024;

    template<typename T>
    class ISpatialStructure3D
    {
    public:
        virtual ~ISpatialStructure3D() = default;

        virtual Dia::Core::Handle<T> Insert(const T& object, const AABB& bounds) = 0;
        virtual void Remove(Dia::Core::Handle<T> handle) = 0;
        virtual void Update(Dia::Core::Handle<T> handle, const AABB& newBounds) = 0;
        virtual void Clear() = 0;

        virtual void QueryRegion  (const AABB& region,                    Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<T>, kMaxQueryResults>& out) const = 0;
        virtual void QuerySphere  (const Sphere& sphere,                  Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<T>, kMaxQueryResults>& out) const = 0;
        virtual void QueryPoint   (const Dia::Maths::Vector3D& point,     Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<T>, kMaxQueryResults>& out) const = 0;
        virtual void QueryRay     (const Ray& ray,                        Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<T>, kMaxQueryResults>& out) const = 0;
        virtual void QueryFrustum (const Frustum& frustum,                Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<T>, kMaxQueryResults>& out) const = 0;
        virtual void QueryKNearest(const Dia::Maths::Vector3D& point, int k, Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<T>, kMaxQueryResults>& out) const = 0;

        virtual const T* Resolve(Dia::Core::Handle<T> handle) const = 0;
    };

    // Uniform 3D grid spatial partitioning
    template<typename T, unsigned int MaxObjects = 2048, unsigned int MaxCells = 4096>
    class SpatialGrid3D : public ISpatialStructure3D<T>
    {
    public:
        struct Def
        {
            AABB  worldBounds;
            float cellSize;
        };
        explicit SpatialGrid3D(const Def& def);
        // ... ISpatialStructure3D overrides + debug accessors mirroring SpatialGrid<T, MaxObjects>
    };
}
```

`SpatialGrid3D` mirrors the 2D `SpatialGrid` mechanics (slot pool with generation counters, LIFO free list, dense `mCells[MaxCells]` array of `DynamicArrayC<uint32_t, kMaxObjectsPerCell>`, visited bitset for query deduplication). The third template parameter `MaxCells` exists because 4096 cells is only 16×16×16 in 3D — denser worlds need a larger cap, set per-instance.

## Features

| Feature | Description | Spec | Status |
|---------|-------------|------|--------|
| Shape Primitives | AABB, OOBB, Sphere, Capsule, Triangle, Cylinder, Ray, Plane, Frustum. Construction, accessors, basic geometry queries. | [shape-primitives.md](../../features/dia/diageometry3d/shape-primitives.md) | Draft |
| Intersection Tests | Pairwise tests returning `IntersectionClassify`. SAT + fast paths for priority pairs (frustum-vs-AABB/Sphere/Triangle, ray casts, AABB-vs-AABB, etc.). | [intersection-tests.md](../../features/dia/diageometry3d/intersection-tests.md) | Draft |
| Spatial Grid | `ISpatialStructure3D<T>` interface and `SpatialGrid3D<T, MaxObjects, MaxCells>` uniform grid with six query types including `QueryFrustum`. | [spatial-grid.md](../../features/dia/diageometry3d/spatial-grid.md) | Draft |

Future feature specs (not authored this round): Octree, BVH3D, ConvexPolyhedron, Closest-point queries (full matrix), Serializers.

## Dependencies on Other Systems

**Required:**
- **DiaMaths** — `Vector3D` (with `Cross()`), `Matrix44`, `Matrix34`, `Quaternion`, `Transform3D`. These are *prerequisites* tracked under the future DiaMaths system spec; DiaGeometry3D cannot ship until they exist.
- **DiaCore** — Assertions (`DIA_ASSERT`), `StringCRC`, containers (`DynamicArrayC`, `Handle`)

**Dependents (planned):**
- **DiaGeometry3DVisualDebugger** — read-only consumer of `SpatialGrid3D` accessors and shape types
- **DiaGeometryBridge** — converts between 2D and 3D shape types
- **DiaRigidBody3D (future)** — primitive types and spatial structures
- **Renderer/culling consumer (future)** — `Frustum`, `QueryFrustum`

## Out of Scope

- Pure linear algebra additions (Vector3D::Cross, Matrix44, Matrix34, Quaternion, Transform3D) — owned by DiaMaths
- 2D↔3D conversions / projections — DiaGeometryBridge
- Visual debug rendering — DiaGeometry3DVisualDebugger
- Octree, BVH3D, kd-tree — future feature specs (same `ISpatialStructure3D<T>` interface)
- Full GJK+EPA in 3D — defer to a later intersection-tests feature spec
- Convex hull / mesh decomposition / signed distance fields
- 3D physics broadphase — DiaRigidBody3D (future application)
- Serializers — mirror `JsonSpatialGridSerializer` once stable; out of this round

## Decisions

| ID | Decision | Rationale | Scope | Status | Binding |
|----|----------|-----------|-------|--------|---------|
| SD-001 | Namespace `Dia::Geometry3D::` (not `Dia::Maths::`) | Mirrors DiaGeometry2D SD-001; clean separation of geometry from math | All features | Accepted | Yes |
| SD-002 | Drop `3D` suffix from all shape type names | Mirrors DiaGeometry2D SD-002; namespace captures dimensionality (`Dia::Geometry3D::AABB`, not `AABB3D`); cross-dimension code reads `Geometry2D::AARect` vs `Geometry3D::AABB` clearly | All features | Accepted | Yes |
| SD-003 | `IntersectionTests` uses unified static `Test()` overloads | Mirrors DiaGeometry2D SD-003; consistent call site `IntersectionTests::Test(a, b)` | Intersection Tests | Accepted | Yes |
| SD-004 | Spatial structures templated on stored type with `Handle<T>` identity | Mirrors DiaGeometry2D SD-004; avoids void* / inheritance overhead; `Handle<T>` (slot+generation) gives stale-handle detection | Spatial features | Accepted | Yes |
| SD-005 | DiaGeometry3D is a static library with no STL containers in public API | Reinforces PD-004 / AD-002; uses DiaCore containers | All features | Accepted | Yes |
| SD-006 | Y-up, right-handed coordinate convention | Standard for most game engines (Unity, Unreal, glTF); affects Frustum construction and Plane defaults | All features | Accepted | Yes |
| SD-007 | `SpatialGrid3D` adds `MaxCells` as a third template parameter (default 4096) | 4096 cells is 16×16×16 in 3D — too small for many worlds. Per-instance template arg keeps density tunable; mirrors but extends `SpatialGrid<T, MaxObjects>` 2D shape | Spatial Grid | Accepted | Yes |
| SD-008 | `ISpatialStructure3D<T>` query set: Region, Sphere, Point, Ray, Frustum, KNearest | Six queries (vs 2D's five). `QueryFrustum` is added for 3D rendering culling — the primary driver. `QuerySphere` replaces 2D's `QueryCircle` | Spatial features | Accepted | Yes |
| SD-009 | Rotation representation: Quaternion primary, Matrix33 conversions provided | Quaternion gives stable hierarchy composition, no gimbal lock, compact storage. Matrix33 conversions support OOBB orientation and any code preferring matrix form. Both forms must round-trip exactly for axis-aligned cases | OOBB, Transform3D consumers | Accepted | Yes |
| SD-010 | Visual debugger lives in a sibling system (`DiaGeometry3DVisualDebugger`), not as a feature here. **Deferred to backlog this round.** | Mirrors `DiaRigidBody2D` / `DiaRigidBody2DVisualDebugger` precedent. Keeps pure-geometry separate from rendering concerns. Deferred because DiaGraphics has no 3D debug primitives — `DebugPrimitive` tagged union is 2D-only. Tracked in `docs/BACKLOG.md` as `DiaGeometry3DVisualDebugger system`; gated on a separate `DiaGraphics 3D debug primitives` feature | All features | Accepted | Yes |
| SD-011 | Cross-dimensional helpers (2D↔3D shape conversions) live in a sibling system (`DiaGeometryBridge`), not here | The bridge module must depend on both DiaGeometry2D and DiaGeometry3D — folding it inside either creates an awkward unidirectional dependency | All features | Accepted | Yes |
| SD-012 | Test utilities ship under `Dia/DiaGeometry3D/Testing/` | Per project memory: test helpers live with the library, not in GoogleTests. Mirrors the 2D module structure | All features | Accepted | Yes |
| SD-013 | `IntersectionClassify` enum is duplicated in `Dia::Geometry3D::` rather than reused from `Dia::Geometry2D::` | Avoids cross-module include just for an enum; the values are identical but ownership is per-dimension. Bridge module is responsible for any conversions if needed | Intersection Tests | Accepted | Yes |
| SD-014 | First-round priority pairs are culling-driven: AABB/Sphere/Triangle vs Frustum, ray casts, AABB-vs-AABB | The driver for this round is rendering culling. Other pairs (Capsule, Cylinder, OOBB-vs-OOBB SAT) ship as fast paths if implementation cost is low; full GJK+EPA matrix deferred | Intersection Tests | Accepted | Yes |

**Status values:** `Proposed` · `Accepted` · `Rejected` · `Superseded`  
**Binding:** `Yes` = enforced constraint on all features in this system · `No` = guidance only

## Inherited Binding Decisions

| ID | Source | Decision | Implication for this system |
|----|--------|----------|----------------------------|
| PD-001 | Platform | StringCRC for all entity/component IDs | Module identified by StringCRC (`kUniqueId`); spatial structures use `Handle<T>` for object identity (compatible with StringCRC where consumers want it) |
| PD-004 | Platform | No STL containers in public APIs | Spatial structures use `DynamicArrayC`, `Handle<T>` from DiaCore; no `std::vector` / `std::map` in public interfaces |
| PD-005 | Platform | x64 only | `DiaGeometry3D.vcxproj` targets x64 exclusively |
| PD-006 | Platform | Visual Studio project files are source of truth | `DiaGeometry3D.vcxproj` and `.vcxproj.filters` created and manually maintained; all files explicitly listed |
| PD-007 | Platform | C++20 required | All code compiled under `/std:c++20`; template spatial structures may use concepts |
| PD-008 | Platform | Directory.Build.props owns OutDir/IntDir/toolchain | `DiaGeometry3D.vcxproj` must NOT override OutDir, IntDir, PlatformToolset, WindowsTargetPlatformVersion, or LanguageStandard |
| AD-001 | Dia App | Module system with YAML frontmatter documentation | Create `dia.geometry3d.architecture.module.md` with public API, responsibilities, and dependency declarations |
| AD-002 | Dia App | No STL containers in public APIs | Reinforces PD-004; internal STL acceptable where no public exposure |
| AD-003 | Dia App | Namespace `Dia::<Module>::` | All code in `Dia::Geometry3D::` namespace per SD-001 |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Naming | SD-002 drops the 3D suffix to mirror SD-002 in DiaGeometry2D. But `AABB` (3D) versus `AARect` (2D) breaks the mirror — why isn't 2D called `AABB2D` or 3D called `AABox`? | Inherited inconsistency. The 2D module already chose `AARect` (rectangle) — natural for 2D. The 3D world calls these "axis-aligned bounding boxes" universally; `AABB` is the standard term. Cross-module readers are not expected to confuse `Dia::Geometry2D::AARect` with `Dia::Geometry3D::AABB`. Document in the shape-primitives feature spec; do not rename either. |
| 2 | Naming | Same question for `OOBB` (3D) vs `OORect` (2D). Should we rename `OOBB` to `OOBox` for symmetry, or rename `OORect` to `OOBB` (2D) to standardise? | Keep both as-is. `OOBB` is the universally-understood 3D term ("oriented bounding box"); `OORect` is the existing 2D name. Cross-module renaming is out of scope for this spec. The 2D module's AI Review Q7 already noted `OORect` is a candidate rename — that's a separate decision. |
| 3 | Naming | `Frustum` has no 2D equivalent. Should it instead live in DiaGeometry3D as `ViewFrustum` to be more specific? | No. `Frustum` is the standard mathematical name; "view" is implied by the rendering-culling driver but not enforced (a frustum primitive can model any 6-plane convex region — light volumes, sound cones, etc.). |
| 4 | Maths dependency | DiaGeometry3D requires DiaMaths additions (Vector3D::Cross, Matrix44, Matrix34, Quaternion, Transform3D) that don't exist yet. How is this sequenced? | A separate DiaMaths system spec is authored before any DiaGeometry3D feature is implemented. The maths-3d-additions feature lives under that DiaMaths system spec (TBD per dia.md:49). DiaGeometry3D feature plans must list the required DiaMaths types as prerequisites and gate on their completion. |
| 5 | Quaternion | SD-009 chose Quaternion-primary with Matrix33 conversions. Where does `Quaternion` live — DiaMaths or DiaGeometry3D? | DiaMaths. Quaternion is pure linear algebra (rotation representation), no geometry. It belongs alongside Vector3D and Matrix44, not alongside shapes. Captured under the future DiaMaths system spec. |
| 6 | Spatial cap | SD-007 makes `MaxCells` a template param to handle dense worlds. Does this require parallel changes to the 2D `SpatialGrid` for symmetry? | No — explicit non-goal of this spec. The 2D `SpatialGrid` is `Done`. Adding a third template param to it is a separate refactor under DiaGeometry2D, only justified if a 2D consumer hits the 4096 cap. Track as backlog if it ever does. |
| 7 | Frustum query | `QueryFrustum` is the priority for rendering culling. Should the spatial grid traverse cells using a frustum-vs-AABB cell test, or fall back to a coarse AABB-region query and per-object refinement? | The feature spec for `spatial-grid.md` decides. Recommended approach: compute the frustum's AABB bound, iterate cells within that bound, frustum-test each cell's AABB to skip wholly-outside cells, then per-object frustum-test inside surviving cells. Revisit if profiling shows the per-cell test dominates. |
| 8 | OOBB rotation storage | OOBB can store its orientation as `Quaternion` or `Matrix33`. Which does the public type expose? | `Quaternion` for storage (compact, stable, follows SD-009). The shape provides `GetOrientationMatrix() -> Matrix33` for SAT and other algorithms that need axes. |
| 9 | Cylinder | Cylinder is uncommon in spatial-query libraries. What's the use case? | Logged as "in-scope but optional fast paths" per SD-014. Realistic use: trigger volumes, capsule-without-rounded-ends, mechanical/CAD-style shapes. If no consumer needs it within the first round, the feature spec can mark it deferred. Decide during shape-primitives feature spec interview. |
| 10 | Triangle | Triangle pairs (Triangle-vs-AABB, Triangle-vs-Frustum) are listed in priority. Why include triangle in a culling-focused first round? | Mesh culling — once the renderer culls at the bounds level (AABB-vs-Frustum), the next refinement is per-triangle culling for large meshes. Listing it here keeps mesh-aware spatial structures unblocked even though we ship grid first. |
| 11 | Visual debugger | SD-010 splits the visual debugger into a sibling system. But `SpatialGrid3D` exposes debug accessors (GetCellCountX/Y/Z, GetWorldBounds). Does that violate the split? | No. Read-only accessors are part of the geometry public API for any consumer (debugger, inspector, asset cooker, tests). The debugger module *consumes* them; it does not pollute the geometry module. Mirror `SpatialGrid` in 2D, which exposes the same accessors per its `.h:62-65`. |
| 12 | Bridge module | If `DiaGeometryBridge` is sibling, can DiaGeometry3D reference 2D types in any of its public API? | No. DiaGeometry3D depends only on DiaMaths and DiaCore. Any 2D-vs-3D conversion or interop lives in DiaGeometryBridge. This is enforced by the dependency declaration in `dia.geometry3d.architecture.module.md`. |
| 13 | Test utilities | SD-012 puts test helpers in `Dia/DiaGeometry3D/Testing/`. What concretely lives there? | Shape factories (parameterised builders for canonical AABB / Sphere / Frustum / etc. test cases), grid builders (`SpatialGrid3DBuilder` analogous to 2D's `PhysicsWorldBuilder`), assertion helpers (`ExpectAABBEqual` with epsilon, etc.). Detailed in per-feature plans. |
| 14 | Future-proofing | If we later want a `Frustum`-like primitive that supports more than 6 planes (light volumes with capping planes, CSG-style), should `Frustum` be templated on plane count? | Decide in the shape-primitives feature spec. Default position: keep `Frustum` as a concrete 6-plane type; introduce a separate `ConvexHullPolyhedron` type if/when N-plane convex regions are needed. Templating `Frustum<N>` would over-generalise the priority use case (rendering culling). |
| 15 | Approval gate | Per CLAUDE.md, this system spec cannot be marked Done until all child feature specs are Approved. What feature specs must exist before that gate? | Three minimum: `shape-primitives.md`, `intersection-tests.md`, `spatial-grid.md`. Each must complete all 5 spec steps. The DiaMaths prerequisites (Vector3D::Cross, Matrix44, Matrix34, Quaternion, Transform3D) live under a separate DiaMaths system spec — DiaGeometry3D's `Done` status depends on that being authored and its 3D maths feature being Approved as well. |

## Status

`Approved` — Steps 1–4 complete (interview, draft, binding decisions, AI review). Awaiting feature spec authorship and implementation. Plan: implementation plan will be authored as `@docs/specs/systems/dia/diageometry3d.plan.md` once all child feature specs are Approved.
