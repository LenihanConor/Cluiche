# System Spec: DiaGeometry2D

## Parent Application
@docs/specs/applications/dia.md

## Purpose

DiaGeometry2D is the 2D geometry system for the Dia engine. It owns all 2D geometric primitives (shapes), intersection and overlap tests, 2D transforms with parent-child hierarchy, and spatial acceleration structures (grid, quadtree, BVH). It is extracted from DiaMaths, which retains only pure linear algebra (vectors, matrices, core math utilities).

The separation exists because geometry — shapes, their spatial relationships, and structures for querying them — is a distinct concern from mathematics. DiaMaths remains a pure math library. DiaGeometry2D depends on DiaMaths and serves as the foundation for DiaRigidBody2D and any system that needs 2D spatial queries (collision detection, frustum culling, raycasting, AI vision).

**Dependency chain:**  
`DiaRigidBody2D → DiaGeometry2D → DiaMaths → DiaCore`

## Responsibilities

- Own all 2D geometric primitive types: Point, Line, Ray, Plane, Circle, AARect, OORect, Triangle, Arc, Capsule, ConvexPolygon, Sector, AnnularSector (13 types; Ellipse dropped)
- Provide full pairwise intersection tests returning `ContactResult` (normal, depth, contact point) via GJK+EPA for convex pairs; fast paths for Circle-Circle, AARect-AARect, Circle-AARect; dedicated implementations for non-convex shapes
- Provide closest-point queries between shapes
- Own 2D transform with parent-child scene hierarchy (local/world space, matrix composition)
- Provide spatial acceleration structures: uniform grid, quadtree, BVH
- Expose a consistent `Dia::Geometry2D::` namespace for all types
- Provide a `DiaGeometry2D.vcxproj` static library project, registered in `Cluiche.sln`
- Provide a `dia.geometry2d.architecture.module.md` YAML module documentation file

## Non-Responsibilities

- Pure math (vectors, matrices, angles, trig, easing) — owned by DiaMaths
- 3D geometry — future DiaGeometry3D module
- Physics simulation (rigid bodies, forces, constraints, collision response) — DiaRigidBody2D
- Rendering or debug drawing of shapes — DiaGraphics
- Scene management or entity hierarchy beyond Transform — DiaScene (Future)
- File I/O or serialization of geometry data (Decision for future)

## Migration from DiaMaths

The following files migrate from `Dia/DiaMaths/` to `Dia/DiaGeometry2D/` and are renamed (2D suffix dropped; namespace provides context):

| DiaMaths source | DiaGeometry2D type | Old namespace | New namespace |
|---|---|---|---|
| `Shape/2D/Circle2D.*` | `Circle` | `Dia::Maths::` | `Dia::Geometry2D::` |
| `Shape/2D/AARect2D.*` | `AARect` | `Dia::Maths::` | `Dia::Geometry2D::` |
| `Shape/2D/OORect2D.*` | `OORect` | `Dia::Maths::` | `Dia::Geometry2D::` |
| `Shape/2D/Line2D.*` | `Line` | `Dia::Maths::` | `Dia::Geometry2D::` |
| `Shape/2D/Ray2D.*` | `Ray` | `Dia::Maths::` | `Dia::Geometry2D::` |
| `Shape/2D/Triangle2D.*` | `Triangle` | `Dia::Maths::` | `Dia::Geometry2D::` |
| `Shape/2D/Arc2D.*` | `Arc` | `Dia::Maths::` | `Dia::Geometry2D::` |
| `Shape/2D/Capsule2D.*` | `Capsule` | `Dia::Maths::` | `Dia::Geometry2D::` |
| `Shape/2D/Ellipse2D.*` | `Ellipse` | `Dia::Maths::` | `Dia::Geometry2D::` |
| `Shape/2D/IntersectionPoint2D.*` | `IntersectionPoint` | `Dia::Maths::` | `Dia::Geometry2D::` |
| `Shape/Common/IntersectionClassify.*` | `IntersectionClassify` | `Dia::Maths::` | `Dia::Geometry2D::` |
| `Shape/Common/IntersectionTests.*` | `IntersectionTests` | `Dia::Maths::` | `Dia::Geometry2D::` |
| `Transform/Transform2D.*` | `Transform` | `Dia::Maths::` | `Dia::Geometry2D::` |

**DiaMaths retains:** `Core/`, `Vector/`, `Matrix/` — pure linear algebra, no geometry.

**Callers updated:** DiaGraphics (gains DiaGeometry2D dependency), any other consumers of DiaMaths shapes.

## Public Interfaces

### Namespace

All types live under `Dia::Geometry2D::`. Future 3D geometry will use `Dia::Geometry3D::`.

### Shape Primitives

```cpp
namespace Dia::Geometry2D {
    // Axis-aligned rectangle (min/max corners)
    class AARect { ... };
    // Object-oriented (rotated) rectangle
    class OORect { ... };
    // Circle (center + radius)
    class Circle { ... };
    // Line segment (two endpoints)
    class Line { ... };
    // Ray (origin + normalized direction, half-infinite)
    class Ray { ... };
    // Triangle (three vertices)
    class Triangle { ... };
    // Arc segment
    class Arc { ... };
    // Capsule (rounded line segment)
    class Capsule { ... };
    // Ellipse
    class Ellipse { ... };
    // Result type for intersection point queries
    class IntersectionPoint { ... };
}
```

### Intersection Tests

```cpp
namespace Dia::Geometry2D {
    enum class IntersectionClassify {
        kNoIntersection,
        kPenetrating,
        kAContainsB,
        kBContainsA
    };

    class IntersectionTests {
    public:
        // Circle vs all shapes
        static IntersectionClassify Test(const Circle& a, const Circle& b);
        static IntersectionClassify Test(const Circle& a, const AARect& b);
        static IntersectionClassify Test(const Circle& a, const OORect& b);
        static IntersectionClassify Test(const Circle& a, const Line& b);
        static IntersectionClassify Test(const Circle& a, const Triangle& b);
        static IntersectionClassify Test(const Circle& a, const Arc& b);
        static IntersectionClassify Test(const Circle& a, const Capsule& b);
        static IntersectionClassify Test(const Circle& a, const Ray& b);

        // AARect vs all shapes
        static IntersectionClassify Test(const AARect& a, const AARect& b);
        static IntersectionClassify Test(const AARect& a, const OORect& b);
        static IntersectionClassify Test(const AARect& a, const Line& b);
        static IntersectionClassify Test(const AARect& a, const Triangle& b);
        static IntersectionClassify Test(const AARect& a, const Arc& b);
        static IntersectionClassify Test(const AARect& a, const Capsule& b);
        // ... (all pairwise combinations)

        // Point containment queries
        static bool Contains(const Circle& shape, const Dia::Maths::Vector2D& point);
        static bool Contains(const AARect& shape, const Dia::Maths::Vector2D& point);
        // ... (all shapes)

        // Closest point queries
        static Dia::Maths::Vector2D ClosestPoint(const Circle& shape, const Dia::Maths::Vector2D& point);
        static Dia::Maths::Vector2D ClosestPoint(const AARect& shape, const Dia::Maths::Vector2D& point);
        // ... (all shapes)
    };
}
```

### Transform

```cpp
namespace Dia::Geometry2D {
    class Transform {
    public:
        // Local space (relative to parent)
        void SetLocalPosition(const Dia::Maths::Vector2D& pos);
        void SetLocalRotation(const Dia::Maths::Angle& angle);
        void SetLocalScale(const Dia::Maths::Vector2D& scale);

        const Dia::Maths::Vector2D& GetLocalPosition() const;
        const Dia::Maths::Angle& GetLocalRotation() const;
        const Dia::Maths::Vector2D& GetLocalScale() const;

        // World space (absolute, computed via parent chain)
        Dia::Maths::Vector2D GetWorldPosition() const;
        Dia::Maths::Angle GetWorldRotation() const;
        Dia::Maths::Vector2D GetWorldScale() const;
        Dia::Maths::Matrix33 GetWorldMatrix() const;

        // Parent-child hierarchy
        void SetParent(Transform* parent);
        Transform* GetParent() const;
    };
}
```

### Spatial Structures (Planned)

```cpp
namespace Dia::Geometry2D {
    // Uniform grid spatial partitioning
    template<typename T>
    class SpatialGrid { ... };

    // Hierarchical spatial partitioning
    template<typename T>
    class Quadtree { ... };

    // Bounding volume hierarchy
    template<typename T>
    class BVH { ... };
}
```

## Features

| Feature | Description | Spec | Status |
|---------|-------------|------|--------|
| 2D Shape Primitives | Migrate Circle, AARect, OORect, Line, Ray, Triangle, Arc, Capsule, Ellipse from DiaMaths. Rename (drop 2D suffix). New namespace `Dia::Geometry2D::`. | [shape-primitives.md](../../features/dia/diageometry2d/shape-primitives.md) | Approved |
| Intersection Tests | Migrate IntersectionClassify + IntersectionTests. Complete all commented-out pairwise tests. Unified static API. | [intersection-tests.md](../../features/dia/diageometry2d/intersection-tests.md) | Approved |
| Transform | Migrate Transform2D → Transform. Parent-child hierarchy, local/world space, matrix composition. | [transform.md](../../features/dia/diageometry2d/transform.md) | Approved |
| Spatial Grid | Uniform grid; defines `ISpatialStructure<T>` and `Handle<T>` prerequisite. Five query types. | [spatial-grid.md](../../features/dia/diageometry2d/spatial-grid.md) | Approved |
| Quadtree | Recursive quadrant partitioning for non-uniform object distributions. Dynamic insert/remove. | [quadtree.md](../../features/dia/diageometry2d/quadtree.md) | Approved |
| BVH | Static bounding volume hierarchy (SAH build). Optimal for fixed geometry. | [bvh.md](../../features/dia/diageometry2d/bvh.md) | Approved |

## Dependencies on Other Systems

**Required:**
- **DiaMaths** — Vector2D, Vector3D, Angle, Matrix33 used by all shape types and Transform

**Required (build):**
- **DiaCore** — Assertions (DIA_ASSERT), StringCRC, DiaCore containers for spatial structures

**Dependents (after migration):**
- **DiaGraphics** — Will add DiaGeometry2D dependency (currently depends on DiaMaths shapes)
- **DiaRigidBody2D** — Will depend on DiaGeometry2D for all primitive types and spatial structures

## Out of Scope

- Physics simulation (forces, rigid bodies, collision response) — DiaRigidBody2D
- 3D geometry — future DiaGeometry3D
- Rendering / debug draw of shapes — DiaGraphics
- Serialization of geometry data
- Convex hull computation — deferred; add if needed by physics
- Navmesh / pathfinding — out of scope for geometry layer

## Decisions

| ID | Decision | Rationale | Scope | Status | Binding |
|----|----------|-----------|-------|--------|---------|
| SD-001 | Namespace `Dia::Geometry2D::` (not `Dia::Maths::`) | Clean separation of geometry from math; `Geometry2D`/`Geometry3D` naming future-proofs a 3D counterpart | All features | Accepted | Yes |
| SD-002 | Drop `2D` suffix from all migrated type names | Namespace captures dimensionality; avoids redundancy (`Dia::Geometry2D::Circle`, not `Circle2D`) | All features | Accepted | Yes |
| SD-003 | IntersectionTests uses unified static `Test()` overloads, not per-shape methods | Consistent call site (`IntersectionTests::Test(a, b)`); avoids asymmetric APIs; easier to extend | Intersection Tests | Accepted | Yes |
| SD-004 | Spatial structures (Grid, Quadtree, BVH) are templated on stored type | Avoids void* or inheritance overhead; caller controls stored data alongside shape | Spatial features | Accepted | Yes |
| SD-005 | DiaGeometry2D is a static library with no STL containers in public API | Consistent with PD-004 / AD-002; DiaCore containers used instead | All features | Accepted | Yes |
| SD-006 | Complete all commented-out intersection test stubs as part of migration | Partial APIs create confusion; the geometry system should be a complete primitive | Intersection Tests | Accepted | Yes |
| SD-007 | Transform supports parent-child hierarchy via raw pointer (no ownership) | Transform is typically owned by a game entity; geometry layer should not manage lifetimes | Transform | Accepted | Yes |

**Status values:** `Proposed` · `Accepted` · `Rejected` · `Superseded`  
**Binding:** `Yes` = enforced constraint on all features in this system · `No` = guidance only

## Inherited Binding Decisions

| ID | Source | Decision | Implication for this system |
|----|--------|----------|----------------------------|
| PD-001 | Platform | StringCRC for all entity/component IDs | Module itself identified by StringCRC (`kUniqueId`); spatial structures use StringCRC for object IDs where applicable |
| PD-004 | Platform | No STL containers in public APIs | Spatial structures must use DiaCore containers (DynamicArrayC, HashTable), not std::vector/std::map |
| PD-005 | Platform | x64 only | DiaGeometry2D.vcxproj targets x64 exclusively; no Win32 configurations |
| PD-006 | Platform | Visual Studio project files are source of truth | DiaGeometry2D.vcxproj and .vcxproj.filters created and manually maintained; all files explicitly listed |
| PD-007 | Platform | C++20 required | All code compiled under `/std:c++20`; template spatial structures may use concepts |
| PD-008 | Platform | Directory.Build.props owns OutDir/IntDir/toolchain | DiaGeometry2D.vcxproj must NOT override OutDir, IntDir, PlatformToolset, WindowsTargetPlatformVersion, or LanguageStandard |
| AD-001 | Dia App | Module system with YAML frontmatter documentation | Create `dia.geometry2d.architecture.module.md` with public API, responsibilities, and dependency declarations |
| AD-002 | Dia App | No STL containers in public APIs | Reinforces PD-004; internal use of STL acceptable where no public exposure |
| AD-003 | Dia App | Namespace `Dia::<Module>::` | All code in `Dia::Geometry2D::` namespace per SD-001 |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Migration | Should DiaMaths retain type aliases for migrated shapes to ease the transition (e.g., `using Circle2D = Dia::Geometry2D::Circle`)? | DiaMaths should not retain aliases — callers should be updated at migration time. Aliases would create long-term confusion about ownership. |
| 2 | Intersection Tests | How should symmetry be handled — is `Test(Circle, AARect)` the same call as `Test(AARect, Circle)`? | Provide both orderings as separate overloads calling a shared implementation. `IntersectionClassify` result is symmetric (swap kAContainsB/kBContainsA). |
| 3 | Spatial Structures | Should spatial structures own their stored objects or hold non-owning pointers? | Non-owning (raw pointers or IDs). Ownership belongs to the caller's entity system. Geometry layer only indexes positions. |
| 4 | Spatial Structures | Should Grid, Quadtree, and BVH share a common interface (ISpatialStructure)? | Yes — a common query interface (`Query(AARect region)`, `Raycast(Ray)`) would allow DiaRigidBody2D to swap implementations. Define this in the Spatial Grid feature spec. |
| 5 | Transform | Is Transform a component (IComponent) or a plain value type? | Plain value type for the geometry layer. If game code needs it as a component, that's DiaApplication's concern — it can wrap Transform in a Module/Component. |
| 6 | DiaGraphics | DiaGraphics gains a new dependency on DiaGeometry2D. Does this break any existing build ordering? | DiaGraphics currently depends on DiaMaths. Replacing that with DiaGeometry2D (which itself depends on DiaMaths) should be transparent to build order — same depth in the DAG. |
| 7 | Naming | `OORect` (Object-Oriented Rectangle) is a confusing abbreviation — should it be renamed? | Consider `OBBox` (Oriented Bounding Box) which is standard game-engine terminology. Defer to feature spec decision; flag as a candidate rename during migration. |
| 8 | Completeness | Are there 2D geometry types that belong here but don't exist yet (e.g., Polygon)? | Convex Polygon is a natural addition but is deferred — it's needed for physics (SAT) more than pure geometry. Add as a Planned feature when DiaRigidBody2D spec is written. |
| 9 | Callers | Which systems currently include DiaMaths shape headers and will need include-path updates? | Known: DiaGraphics. Unknown: any game code in CluicheTest. A grep for `#include <DiaMaths/Shape/` will find all callers before migration begins. |
| 10 | Module doc | Should `dia.maths.shape.*` module docs be deleted or archived when shapes move? | Delete them. The code is the source of truth; stale module docs are worse than none. |

## Status

`Approved` — Plan: @docs/specs/systems/dia/diageometry2d.plan.md
