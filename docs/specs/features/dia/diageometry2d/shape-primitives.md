# Feature Spec: Shape Primitives

## Traceability

| Level | Parent | This Feature |
|-------|--------|--------------|
| Platform | @docs/specs/platform/Cluiche.md | - |
| Application | @docs/specs/applications/dia.md | - |
| System | @docs/specs/systems/dia/diageometry2d.md | **shape-primitives** |

**Status:** `Approved`

---

## Problem Statement

2D geometric primitive types currently live in `Dia/DiaMaths/Shape/2D/` under the `Dia::Maths::` namespace with a `2D` suffix on every name. DiaMaths should be a pure linear algebra library. The shapes need to move to DiaGeometry2D with a clean namespace (`Dia::Geometry2D::`) and names that drop the redundant `2D` suffix. The set also needs expanding: Ellipse is dropped (unused stub), an infinite-line type (`Plane`) is added, and four new types are added (Point, ConvexPolygon, Sector, AnnularSector).

---

## Solution Overview

Migrate 8 existing shape types from `Dia/DiaMaths/Shape/2D/` to `Dia/DiaGeometry2D/Shapes/`. Drop `Ellipse2D` entirely. Rename each type (drop `2D` suffix). Add 4 new types: `Point`, `Plane`, `ConvexPolygon`, `Sector`, `AnnularSector`. Update namespace from `Dia::Maths::` to `Dia::Geometry2D::`. Remove migrated files from `DiaMaths.vcxproj`; add all types to `DiaGeometry2D.vcxproj`. Update all callers.

### Final Shape Set (13 types)

| Type | Description | Source |
|------|-------------|--------|
| `Point` | Single position | New |
| `Line` | Line segment (two endpoints) | Migrated (`Line2D`) |
| `Ray` | Origin + normalized direction, half-infinite | Migrated (`Ray2D`) |
| `Plane` | Infinite line: normal (Vector2D) + distance from origin (float), i.e. `n·x = d` | New |
| `Circle` | Center + radius | Migrated (`Circle2D`) |
| `AARect` | Axis-aligned rectangle (min/max corners) | Migrated (`AARect2D`) |
| `OORect` | Oriented rectangle (center, half-extents, angle) | Migrated (`OORect2D`) |
| `Triangle` | Three vertices | Migrated (`Triangle2D`) |
| `Arc` | Center, radius, start/end angle (boundary arc, not filled) | Migrated (`Arc2D`) |
| `Capsule` | Line segment + radius (rounded ends) | Migrated (`Capsule2D`) |
| `ConvexPolygon` | Ordered convex vertex list (max 16 vertices) | New |
| `Sector` | Filled wedge: center, radius, start/end angle | New |
| `AnnularSector` | Filled wedge donut: center, inner/outer radius, start/end angle | New |

**Dropped:** `Ellipse2D` — unused stub, not migrated.

### Key Design Points

1. **Namespace** — `Dia::Geometry2D::Circle`, consistent across all types
2. **No aliases in DiaMaths** — callers updated at migration time; no backward-compat shims
3. **`Plane` naming** — in 2D, "plane" means an infinite dividing line; represented as `normal·x = d` (standard half-space form used for portals, mirror planes, BSP)
4. **`ConvexPolygon` max vertices** — capped at 16 compile-time to avoid heap allocation; sufficient for all physics collision shapes
5. **`OORect` kept** — name is established internally; rename deferred to a later naming review
6. **`Arc` vs `Sector`** — `Arc` is the boundary curve only (1D); `Sector` is the filled pie slice (2D area). `AnnularSector` is the filled donut wedge (ring + angle bounds).
7. **DiaGraphics updated** — gains `DiaGeometry2D` project dependency; `Shape/2D/` includes updated to `DiaGeometry2D/Shapes/`

---

## Acceptance Criteria

| ID | Criterion | Verification Method |
|----|-----------|---------------------|
| AC1 | All 13 shape types exist in `Dia/DiaGeometry2D/Shapes/` | File system check |
| AC2 | All types are in `Dia::Geometry2D::` namespace | Grep for `namespace Dia::Geometry2D` |
| AC3 | No shape types remain in `Dia/DiaMaths/Shape/2D/` | File system; `DiaMaths.vcxproj` has no Shape/2D entries |
| AC4 | `Ellipse2D` files are deleted (not migrated) | File system check |
| AC5 | `DiaGeometry2D.vcxproj` lists all 13 shape headers, sources, and inlines | Project file review |
| AC6 | DiaGraphics compiles with updated includes | `msbuild DiaGraphics.vcxproj /p:Configuration=Debug /p:Platform=x64` |
| AC7 | Full solution builds clean | `msbuild Cluiche.sln /p:Configuration=Debug /p:Platform=x64` |
| AC8 | No `Dia::Maths::` references to shape types remain | Grep for `Dia::Maths::\(Circle\|AARect\|OORect\|Line2D\|Ray2D\|Triangle\|Arc\|Capsule\|Ellipse\)` |
| AC9 | Unit tests pass | `UnitTests.exe` |
| AC10 | `ConvexPolygon` enforces max 16 vertices via `DIA_ASSERT` | Unit test: add 17th vertex fires assert |

---

## Public API

```cpp
namespace Dia::Geometry2D {

// --- Degenerate / linear shapes ---

struct Point {
    Dia::Maths::Vector2D position;
};

class Line {
public:
    Line();
    Line(const Dia::Maths::Vector2D& start, const Dia::Maths::Vector2D& end);
    const Dia::Maths::Vector2D& GetStart() const;
    const Dia::Maths::Vector2D& GetEnd()   const;
    Dia::Maths::Vector2D GetDirection()    const;  // normalized
    float GetLength()                      const;
    Dia::Maths::Vector2D ClosestPointOnShapeTo(const Dia::Maths::Vector2D& point) const;
};

class Ray {
public:
    Ray();
    Ray(const Dia::Maths::Vector2D& origin, const Dia::Maths::Vector2D& direction);
    const Dia::Maths::Vector2D& GetOrigin()    const;
    const Dia::Maths::Vector2D& GetDirection() const;  // normalized
    Dia::Maths::Vector2D PointAt(float t)      const;
    Dia::Maths::Vector2D ClosestPointOnShapeTo(const Dia::Maths::Vector2D& point) const;
};

class Plane {
public:
    Plane();
    Plane(const Dia::Maths::Vector2D& normal, float distance);
    static Plane FromPoints(const Dia::Maths::Vector2D& a, const Dia::Maths::Vector2D& b);

    const Dia::Maths::Vector2D& GetNormal()  const;
    float                       GetDistance() const;

    float SignedDistance(const Dia::Maths::Vector2D& point) const;  // +ve = front, -ve = back
    Dia::Maths::Vector2D ClosestPointOnShapeTo(const Dia::Maths::Vector2D& point) const;

private:
    Dia::Maths::Vector2D mNormal;   // unit normal
    float                mDistance; // n·x = d
};

// --- Convex area shapes (GJK-compatible) ---

class Circle {
public:
    Circle();
    Circle(const Dia::Maths::Vector2D& center, float radius);
    const Dia::Maths::Vector2D& GetCenter() const;
    float                       GetRadius() const;
    void SetCenter(const Dia::Maths::Vector2D& center);
    void SetRadius(float radius);
    Dia::Maths::Vector2D ClosestPointOnShapeTo(const Dia::Maths::Vector2D& point) const;
};

class AARect {
public:
    AARect();
    AARect(const Dia::Maths::Vector2D& min, const Dia::Maths::Vector2D& max);
    const Dia::Maths::Vector2D& GetMin()        const;
    const Dia::Maths::Vector2D& GetMax()        const;
    Dia::Maths::Vector2D        GetCenter()     const;
    Dia::Maths::Vector2D        GetHalfExtents() const;
    Dia::Maths::Vector2D ClosestPointOnShapeTo(const Dia::Maths::Vector2D& point) const;
};

class OORect { /* center, half-extents, orientation angle */ };
class Triangle { /* three vertices, CCW winding */ };
class Capsule  { /* line segment + radius */ };

class ConvexPolygon {
public:
    static constexpr int kMaxVertices = 16;

    ConvexPolygon();
    void AddVertex(const Dia::Maths::Vector2D& vertex);  // DIA_ASSERT if > kMaxVertices
    int  GetVertexCount() const;
    const Dia::Maths::Vector2D& GetVertex(int index) const;
    Dia::Maths::Vector2D ClosestPointOnShapeTo(const Dia::Maths::Vector2D& point) const;

private:
    Dia::Maths::Vector2D mVertices[kMaxVertices];
    int                  mVertexCount;
};

// --- Angular / radial area shapes ---

class Sector {
public:
    Sector();
    Sector(const Dia::Maths::Vector2D& center, float radius,
           const Dia::Maths::Angle& startAngle, const Dia::Maths::Angle& endAngle);
    const Dia::Maths::Vector2D& GetCenter()     const;
    float                       GetRadius()     const;
    const Dia::Maths::Angle&    GetStartAngle() const;
    const Dia::Maths::Angle&    GetEndAngle()   const;
};

class AnnularSector {
    // "Wedge donut" — filled region between inner and outer radius, within angle bounds
public:
    AnnularSector();
    AnnularSector(const Dia::Maths::Vector2D& center,
                  float innerRadius, float outerRadius,
                  const Dia::Maths::Angle& startAngle, const Dia::Maths::Angle& endAngle);
    const Dia::Maths::Vector2D& GetCenter()      const;
    float                       GetInnerRadius() const;
    float                       GetOuterRadius() const;
    const Dia::Maths::Angle&    GetStartAngle()  const;
    const Dia::Maths::Angle&    GetEndAngle()    const;
};

// --- Intersection result (used by IntersectionTests) ---

struct ContactResult {
    bool         hasContact     = false;
    Dia::Maths::Vector2D normal = {};      // Points from B toward A
    float        depth          = 0.0f;    // Penetration depth (0 = touching)
    Dia::Maths::Vector2D point  = {};      // Contact point in world space
};

} // namespace Dia::Geometry2D
```

> **Note:** `ContactResult` is defined here (alongside shapes) since it's a shared return type used by both `IntersectionTests` and DiaRigidBody2D. `IntersectionClassify` is kept as a lightweight classify-only enum for callers that don't need depth/normal.

---

## Implementation Notes

### File Migration Map

| Source (DiaMaths) | Destination (DiaGeometry2D) |
|---|---|
| `Shape/2D/Circle2D.h/.cpp/.inl` | `Shapes/Circle.h/.cpp/.inl` |
| `Shape/2D/AARect2D.h/.cpp/.inl` | `Shapes/AARect.h/.cpp/.inl` |
| `Shape/2D/OORect2D.h/.cpp/.inl` | `Shapes/OORect.h/.cpp/.inl` |
| `Shape/2D/Line2D.h/.cpp/.inl` | `Shapes/Line.h/.cpp/.inl` |
| `Shape/2D/Ray2D.h/.cpp/.inl` | `Shapes/Ray.h/.cpp/.inl` |
| `Shape/2D/Triangle2D.h/.cpp/.inl` | `Shapes/Triangle.h/.cpp/.inl` |
| `Shape/2D/Arc2D.h/.cpp/.inl` | `Shapes/Arc.h/.cpp/.inl` |
| `Shape/2D/Capsule2D.h/.cpp/.inl` | `Shapes/Capsule.h/.cpp/.inl` |
| `Shape/2D/Ellipse2D.*` | **Deleted** |
| `Shape/2D/IntersectionPoint2D.*` | **Replaced** by `ContactResult` in `Shapes/ContactResult.h` |
| *(new)* | `Shapes/Point.h` |
| *(new)* | `Shapes/Plane.h/.cpp/.inl` |
| *(new)* | `Shapes/ConvexPolygon.h/.cpp/.inl` |
| *(new)* | `Shapes/Sector.h/.cpp/.inl` |
| *(new)* | `Shapes/AnnularSector.h/.cpp/.inl` |

### Steps

1. Create `Dia/DiaGeometry2D/Shapes/` directory
2. Copy and rename migrated files; strip `2D` suffix; update `#pragma once` guards to `DIA_GEOMETRY2D_<TYPENAME>_H`
3. Replace `namespace Dia::Maths` with `namespace Dia::Geometry2D`
4. Update cross-includes within shape files to new paths
5. Write new types: `Point`, `Plane`, `ConvexPolygon`, `Sector`, `AnnularSector`
6. Remove Shape/2D entries from `DiaMaths.vcxproj` / `.filters`; delete `Ellipse2D` files
7. Add all Shapes entries to `DiaGeometry2D.vcxproj` / `.filters`
8. Update DiaGraphics includes; add DiaGeometry2D ProjectReference to `DiaGraphics.vcxproj`
9. Grep remaining codebase for old type names and update

---

## Dependencies

### Required Modules
- **DiaMaths** — `Vector2D`, `Angle` used by all shapes
- **DiaCore** — `DIA_ASSERT`

### Dependent Features
- **intersection-tests** — must be complete first before tests can be written
- **transform** — independent, same migration batch

---

## Testing Strategy

### Build Verification
- `msbuild Cluiche.sln /p:Configuration=Debug /p:Platform=x64` — zero errors

### Unit Tests (`Cluiche/Tests/GoogleTests/Geometry2D/TestShapePrimitives.cpp`)
- Construct each type with default and parameterised constructors; verify getters
- `ClosestPointOnShapeTo()` — point on boundary, inside (where applicable), outside
- `Plane::SignedDistance()` — positive on front, negative on back, zero on plane
- `Plane::FromPoints()` — verify normal is perpendicular to segment AB
- `ConvexPolygon` max vertex assert — add 17th vertex fires `DIA_ASSERT`
- `AnnularSector` — verify inner < outer radius assert

---

## Binding Decisions Compliance

| Decision | Source | Summary | Compliance |
|----------|--------|---------|------------|
| PD-004 | Platform | No STL containers in public APIs | ✅ `ConvexPolygon` uses fixed-size array; no std::vector |
| PD-005 | Platform | x64 only | ✅ |
| PD-006 | Platform | VS project files are source of truth | ✅ All files explicitly listed in .vcxproj |
| PD-007 | Platform | C++20 required | ✅ |
| PD-008 | Platform | Directory.Build.props owns OutDir/IntDir | ✅ |
| AD-002 | Dia App | No STL in public APIs | ✅ |
| AD-003 | Dia App | Namespace `Dia::<Module>::` | ✅ `Dia::Geometry2D::` |
| SD-001 | System | Namespace `Dia::Geometry2D::` | ✅ |
| SD-002 | System | Drop `2D` suffix | ✅ |

---

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | ConvexPolygon | Is 16 the right max vertex count? | Sufficient for all game collision shapes (Box2D caps at 8; Chipmunk at 16). Can be raised via a constant change if needed. |
| 2 | Plane naming | Is `Plane` confusing in a 2D context (sounds 3D)? | Acceptable — "plane" in 2D is a standard term for an infinite dividing line (half-plane). Alternative `HalfSpace` is more precise but less familiar. Keep `Plane`. |
| 3 | Arc vs Sector | `Arc` (boundary only) and `Sector` (filled) are easily confused. Should they be more clearly differentiated? | Names are correct. Document prominently in the header: `Arc` = boundary curve, `Sector` = filled region. |
| 4 | AnnularSector | Should `AnnularSector` assert `innerRadius < outerRadius`? | Yes — `DIA_ASSERT(innerRadius < outerRadius)` in constructor. |
| 5 | Ellipse removal | Any existing code that uses `Ellipse2D` will break. How is this handled? | Grep for `Ellipse2D` callers before migration. If none found, safe to delete. If callers exist, discuss replacement strategy before proceeding. |

---

## Files Affected

### New / migrated files (DiaGeometry2D)
- `Dia/DiaGeometry2D/Shapes/Point.h`
- `Dia/DiaGeometry2D/Shapes/Line.h/.cpp/.inl`
- `Dia/DiaGeometry2D/Shapes/Ray.h/.cpp/.inl`
- `Dia/DiaGeometry2D/Shapes/Plane.h/.cpp/.inl`
- `Dia/DiaGeometry2D/Shapes/Circle.h/.cpp/.inl`
- `Dia/DiaGeometry2D/Shapes/AARect.h/.cpp/.inl`
- `Dia/DiaGeometry2D/Shapes/OORect.h/.cpp/.inl`
- `Dia/DiaGeometry2D/Shapes/Triangle.h/.cpp/.inl`
- `Dia/DiaGeometry2D/Shapes/Arc.h/.cpp/.inl`
- `Dia/DiaGeometry2D/Shapes/Capsule.h/.cpp/.inl`
- `Dia/DiaGeometry2D/Shapes/ConvexPolygon.h/.cpp/.inl`
- `Dia/DiaGeometry2D/Shapes/Sector.h/.cpp/.inl`
- `Dia/DiaGeometry2D/Shapes/AnnularSector.h/.cpp/.inl`
- `Dia/DiaGeometry2D/Shapes/ContactResult.h`

### Deleted files (DiaMaths)
- `Dia/DiaMaths/Shape/2D/` — entire directory removed

### Modified
- `Dia/DiaMaths/DiaMaths.vcxproj` + `.filters` — remove Shape/2D entries
- `Dia/DiaGeometry2D/DiaGeometry2D.vcxproj` + `.filters` — add all Shapes entries
- `Dia/DiaGraphics/DiaGraphics.vcxproj` — add DiaGeometry2D ProjectReference
- Any DiaGraphics / CluicheTest files including `DiaMaths/Shape/` headers

### Tests
- `Cluiche/Tests/GoogleTests/Geometry2D/TestShapePrimitives.cpp` (new)
