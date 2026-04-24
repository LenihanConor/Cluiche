# Plan: DiaGeometry2D

**Spec:** @docs/specs/systems/dia/diageometry2d.md  
**Status:** In Progress  
**Started:** 2026-04-24  
**Last Updated:** 2026-04-24

## Prerequisites

| Prerequisite | Why needed | Status |
|---|---|---|
| `Dia::Core::Handle<T>` in DiaCore | Used by all three spatial structures (SpatialGrid, Quadtree, BVH) for safe object identity | Not Started |

`Handle<T>` is a new type in `Dia/DiaCore/Containers/Handle.h`. It must be implemented and added to `DiaCore.vcxproj` before Phase 3 begins. It is a small self-contained type (index + generation counter) with no dependencies.

## Implementation Phases

### Phase 1 — Project Scaffold
Establish the DiaGeometry2D project before any feature code lands. Nothing else can build against it until this exists.

| # | Task | Status | Notes |
|---|------|--------|-------|
| 1.1 | Create `Dia/DiaGeometry2D/` directory structure (`Shapes/`, `Intersection/`, `Transform/`, `Spatial/`, `Docs/`) | Done | |
| 1.2 | Create `DiaGeometry2D.vcxproj` (StaticLibrary, Debug\|x64 + Release\|x64, depends on DiaMaths + DiaCore) | Done | GUID: {A3C4D5E6-F7B8-9012-CDEF-012345678901} |
| 1.3 | Create `DiaGeometry2D.vcxproj.filters` | Done | Filter groups: Shapes, Intersection, Transform, Spatial, Docs |
| 1.4 | Register `DiaGeometry2D` in `Cluiche/Cluiche.sln` under the Library folder | Done | Added with ProjectDependencies for DiaCore + DiaMaths |
| 1.5 | Create `dia.geometry2d.architecture.module.md` in `Docs/` | Done | |
| 1.6 | Verify empty project builds clean | Not Started | `msbuild DiaGeometry2D.vcxproj /p:Configuration=Debug /p:Platform=x64` |

### Phase 2 — Shape Primitives Migration
Migrate existing types from DiaMaths and add new types. All subsequent features depend on these types existing.

**Spec:** @docs/specs/features/dia/diageometry2d/shape-primitives.md

| # | Task | Status | Notes |
|---|------|--------|-------|
| 2.1 | Grep codebase for `#include <DiaMaths/Shape/` and `Dia::Maths::` shape type usage — catalogue all callers | Done | Callers: DiaGraphics (IRenderTarget.h, SpriteDrawCommand.h), GoogleTests (TestCircle2D.cpp, TestIntersectionClassify.cpp) |
| 2.2 | Migrate 8 existing shape types from `DiaMaths/Shape/2D/` to `DiaGeometry2D/Shapes/` — rename (strip `2D`), update namespace to `Dia::Geometry2D::`, update include guards | Done | Circle, AARect, OORect, Line, Ray, Triangle, Arc, Capsule all created |
| 2.3 | Migrate `IntersectionPoint2D` → replace with `ContactResult` struct in `Shapes/ContactResult.h` | Done | `ContactResult` with point, normal, depth fields |
| 2.4 | Delete `Ellipse2D.*` files (not migrated) | Not Started | No callers found; deletion deferred until confirmed safe |
| 2.5 | Implement new types: `Point`, `Plane`, `ConvexPolygon` (max 16 vertices), `Sector`, `AnnularSector` | Done | All 5 new types created |
| 2.6 | Add all Shapes to `DiaGeometry2D.vcxproj` + `.filters`; remove Shape/2D from `DiaMaths.vcxproj` + `.filters` | Done (Shapes added); Not Started (DiaMaths removal deferred) | DiaMaths removal blocked by DiaGraphics ABI dependency |
| 2.7 | Update DiaGraphics: add DiaGeometry2D ProjectReference; update all `DiaMaths/Shape/` includes | Blocked | IRenderTarget.h returns Maths::AARect2D in its virtual API — updating includes would break binary ABI; deferred until full migration of DiaGraphics public API |
| 2.8 | Update any other callers (CluicheTest etc.) found in 2.1 | Not Started | |
| 2.9 | Build solution clean; write `TestShapePrimitives.cpp` unit tests | Not Started | `Cluiche/Tests/GoogleTests/Geometry2D/` |

### Phase 3 — Intersection Tests
Depends on Phase 2 (all shape types must exist). Also requires `Handle<T>` prerequisite for Phase 4.

**Spec:** @docs/specs/features/dia/diageometry2d/intersection-tests.md

| # | Task | Status | Notes |
|---|------|--------|-------|
| 3.1 | Migrate `IntersectionClassify` and existing `IntersectionTests` stubs to `DiaGeometry2D/Intersection/`; update namespace | Done | IntersectionClassify migrated to Shapes/; IntersectionTests in Intersection/ |
| 3.2 | Implement GJK algorithm + EPA in `Intersection/GJK.cpp` and `SupportFunctions.h` | Not Started | Deferred — existing analytic tests ported; GJK is a future phase |
| 3.3 | Implement 3 fast-path `Test()` overloads: Circle-Circle, AARect-AARect, Circle-AARect | Done | All ported from original IntersectionTests |
| 3.4 | Wire all remaining convex-vs-convex `Test()` overloads through GJK dispatch | Not Started | Deferred with GJK |
| 3.5 | Implement dedicated tests: Plane, Ray, Line, Arc, Sector, AnnularSector | Partial | Line, Arc, Ray ported; Plane/Sector/AnnularSector stubs |
| 3.6 | Implement `Contains()` for all applicable shapes; `ClosestPoint()` for all shapes | Not Started | |
| 3.7 | Implement `Classify()` template wrapper | Not Started | |
| 3.8 | Add all Intersection files to `.vcxproj` + `.filters` | Done | |
| 3.9 | Build clean; write `TestIntersectionTests.cpp` | Not Started | |

### Phase 4 — Transform Migration
Independent of Phase 3; can run in parallel with it. Depends on Phase 2.

**Spec:** @docs/specs/features/dia/diageometry2d/transform.md

| # | Task | Status | Notes |
|---|------|--------|-------|
| 4.1 | Migrate `Transform2D.*` to `DiaGeometry2D/Transform/Transform.h/.cpp/.inl`; rename class; update namespace | Done | Transform class in Dia::Geometry2D namespace |
| 4.2 | Remove `Transform/` from `DiaMaths.vcxproj`; add to `DiaGeometry2D.vcxproj` | Done (added to new); Not Started (remove from DiaMaths deferred) | |
| 4.3 | Update all callers of `Dia::Maths::Transform2D` | Not Started | No callers found in grep — Transform2D was not yet used outside DiaMaths |
| 4.4 | Build clean; write `TestTransform.cpp` | Not Started | |

### Phase 5 — DiaCore: Handle\<T\>
Prerequisite for all spatial structure features. Can be done in parallel with Phase 3/4.

| # | Task | Status | Notes |
|---|------|--------|-------|
| 5.1 | Implement `Dia::Core::Handle<T>` in `Dia/DiaCore/Containers/Handle.h` | Done | Index + generation counter; `IsValid()`, `Invalid()` static |
| 5.2 | Add `Handle.h` to `DiaCore.vcxproj` + `.filters` | Done | |
| 5.3 | Write unit test for handle validity / stale detection | Not Started | |

### Phase 6 — Spatial Grid + ISpatialStructure
Depends on Phases 2, 3, 5.

**Spec:** @docs/specs/features/dia/diageometry2d/spatial-grid.md

| # | Task | Status | Notes |
|---|------|--------|-------|
| 6.1 | Define `ISpatialStructure<T>` in `Spatial/ISpatialStructure.h` | Not Started | Abstract base; 5 query methods + Insert/Remove/Update/Clear/Resolve |
| 6.2 | Implement `SpatialGrid<T>` — slot pool, cell index mapping, Insert/Remove/Update | Not Started | |
| 6.3 | Implement 5 query methods: QueryRegion, QueryCircle, QueryPoint, QueryRay, QueryKNearest | Not Started | Duplicate prevention via visited bitset |
| 6.4 | Add to `.vcxproj` + `.filters` | Not Started | |
| 6.5 | Build clean; write `TestSpatialGrid.cpp` | Not Started | |

### Phase 7 — Quadtree
Depends on Phase 6 (`ISpatialStructure<T>` must exist).

**Spec:** @docs/specs/features/dia/diageometry2d/quadtree.md

| # | Task | Status | Notes |
|---|------|--------|-------|
| 7.1 | Implement `Quadtree<T>` — flat node pool, recursive insert, boundary-object handling | Not Started | |
| 7.2 | Implement 5 query methods; implement `Rebuild()` | Not Started | K-nearest uses priority traversal |
| 7.3 | Add to `.vcxproj` + `.filters` | Not Started | |
| 7.4 | Build clean; write `TestQuadtree.cpp` including ISpatialStructure substitution test | Not Started | |

### Phase 8 — BVH
Depends on Phase 6.

**Spec:** @docs/specs/features/dia/diageometry2d/bvh.md

| # | Task | Status | Notes |
|---|------|--------|-------|
| 8.1 | Implement `BVH<T>` — flat node array, binned SAH build | Not Started | |
| 8.2 | Implement 5 query methods; front-to-back raycast traversal; assert on Insert/Remove/Update | Not Started | |
| 8.3 | Add to `.vcxproj` + `.filters` | Not Started | |
| 8.4 | Build clean; write `TestBVH.cpp` including substitution test vs SpatialGrid | Not Started | |

## Implementation Patterns

### Project file conventions
- All new `.vcxproj` files: StaticLibrary, `Debug|x64` + `Release|x64` only
- Do NOT override `OutDir`, `IntDir`, `PlatformToolset`, `WindowsTargetPlatformVersion`, `LanguageStandard` — owned by `Directory.Build.props`
- Include dirs: `./;./../;%(AdditionalIncludeDirectories)` (local + sibling modules)
- Follow DiaMaths.vcxproj as the reference template

### Namespace
All code: `namespace Dia::Geometry2D { ... }`

### Include guard convention
`DIA_GEOMETRY2D_<TYPENAME>_H` (e.g., `DIA_GEOMETRY2D_CIRCLE_H`)

### Template spatial structures
Header-only or `.inl`-included implementation (templates cannot be in `.cpp`). Place implementation in `SpatialGrid.inl`, `Quadtree.inl`, `BVH.inl` included at the bottom of their respective `.h` files.

### Test project location
All new test files go in `Cluiche/Tests/GoogleTests/Geometry2D/` (new subdirectory).

## Feature Spec Status Summary

| Feature | Spec | Status |
|---------|------|--------|
| Shape Primitives | @docs/specs/features/dia/diageometry2d/shape-primitives.md | Approved |
| Intersection Tests | @docs/specs/features/dia/diageometry2d/intersection-tests.md | Approved |
| Transform | @docs/specs/features/dia/diageometry2d/transform.md | Approved |
| Spatial Grid | @docs/specs/features/dia/diageometry2d/spatial-grid.md | Approved |
| Quadtree | @docs/specs/features/dia/diageometry2d/quadtree.md | Approved |
| BVH | @docs/specs/features/dia/diageometry2d/bvh.md | Approved |

## Session Notes

### 2026-04-23
- System spec approved; all 6 feature specs written and approved
- Shape set expanded from 9 to 13 types; Ellipse dropped; Plane, Point, ConvexPolygon, Sector, AnnularSector added
- Intersection approach changed from N×N to GJK+EPA hybrid with 3 fast paths
- `Handle<T>` identified as DiaCore prerequisite (tracked as Phase 5)
- DiaGraphics will gain DiaGeometry2D dependency — needs include + ProjectReference update
