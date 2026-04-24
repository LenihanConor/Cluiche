# Plan: DiaGeometry2D

**Spec:** @docs/specs/systems/dia/diageometry2d.md  
**Status:** Not Started  
**Started:** —  
**Last Updated:** 2026-04-23

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
| 1.1 | Create `Dia/DiaGeometry2D/` directory structure (`Shapes/`, `Intersection/`, `Transform/`, `Spatial/`, `Docs/`) | Not Started | |
| 1.2 | Create `DiaGeometry2D.vcxproj` (StaticLibrary, Debug\|x64 + Release\|x64, depends on DiaMaths + DiaCore) | Not Started | Follow DiaMaths.vcxproj as template; no files yet |
| 1.3 | Create `DiaGeometry2D.vcxproj.filters` | Not Started | Filter groups: Shapes, Intersection, Transform, Spatial, Docs |
| 1.4 | Register `DiaGeometry2D` in `Cluiche/Cluiche.sln` under the Library folder | Not Started | New GUID; add ProjectDependencies for DiaCore + DiaMaths |
| 1.5 | Create `dia.geometry2d.architecture.module.md` in `Docs/` | Not Started | YAML frontmatter per AD-001; `module_id: dia.geometry2d`; `parent_module_id: dia.root` |
| 1.6 | Verify empty project builds clean | Not Started | `msbuild DiaGeometry2D.vcxproj /p:Configuration=Debug /p:Platform=x64` |

### Phase 2 — Shape Primitives Migration
Migrate existing types from DiaMaths and add new types. All subsequent features depend on these types existing.

**Spec:** @docs/specs/features/dia/diageometry2d/shape-primitives.md

| # | Task | Status | Notes |
|---|------|--------|-------|
| 2.1 | Grep codebase for `#include <DiaMaths/Shape/` and `Dia::Maths::` shape type usage — catalogue all callers | Not Started | Produces a hit list for 2.7/2.8 |
| 2.2 | Migrate 8 existing shape types from `DiaMaths/Shape/2D/` to `DiaGeometry2D/Shapes/` — rename (strip `2D`), update namespace to `Dia::Geometry2D::`, update include guards | Not Started | Circle, AARect, OORect, Line, Ray, Triangle, Arc, Capsule |
| 2.3 | Migrate `IntersectionPoint2D` → replace with `ContactResult` struct in `Shapes/ContactResult.h` | Not Started | `ContactResult` is the new shared result type |
| 2.4 | Delete `Ellipse2D.*` files (not migrated) | Not Started | Verify no callers first (from 2.1 hit list) |
| 2.5 | Implement new types: `Point`, `Plane`, `ConvexPolygon` (max 16 vertices), `Sector`, `AnnularSector` | Not Started | New files in `Shapes/` |
| 2.6 | Add all Shapes to `DiaGeometry2D.vcxproj` + `.filters`; remove Shape/2D from `DiaMaths.vcxproj` + `.filters` | Not Started | |
| 2.7 | Update DiaGraphics: add DiaGeometry2D ProjectReference; update all `DiaMaths/Shape/` includes | Not Started | From 2.1 hit list |
| 2.8 | Update any other callers (CluicheTest etc.) found in 2.1 | Not Started | |
| 2.9 | Build solution clean; write `TestShapePrimitives.cpp` unit tests | Not Started | `Cluiche/Tests/GoogleTests/Geometry2D/` |

### Phase 3 — Intersection Tests
Depends on Phase 2 (all shape types must exist). Also requires `Handle<T>` prerequisite for Phase 4.

**Spec:** @docs/specs/features/dia/diageometry2d/intersection-tests.md

| # | Task | Status | Notes |
|---|------|--------|-------|
| 3.1 | Migrate `IntersectionClassify` and existing `IntersectionTests` stubs to `DiaGeometry2D/Intersection/`; update namespace | Not Started | |
| 3.2 | Implement GJK algorithm + EPA in `Intersection/GJK.cpp` and `SupportFunctions.h` | Not Started | Support functions for Circle, AARect, OORect, Triangle, Capsule, ConvexPolygon, Point |
| 3.3 | Implement 3 fast-path `Test()` overloads: Circle-Circle, AARect-AARect, Circle-AARect | Not Started | Analytic; do not use GJK |
| 3.4 | Wire all remaining convex-vs-convex `Test()` overloads through GJK dispatch | Not Started | All symmetric overloads covered |
| 3.5 | Implement dedicated tests: Plane, Ray, Line, Arc, Sector, AnnularSector | Not Started | Non-convex; cannot use GJK |
| 3.6 | Implement `Contains()` for all applicable shapes; `ClosestPoint()` for all shapes | Not Started | |
| 3.7 | Implement `Classify()` template wrapper | Not Started | Calls `Test()`, returns `IntersectionClassify` |
| 3.8 | Add all Intersection files to `.vcxproj` + `.filters` | Not Started | |
| 3.9 | Build clean; write `TestIntersectionTests.cpp` | Not Started | Priority: Circle-Circle, Circle-AARect, AARect-AARect first |

### Phase 4 — Transform Migration
Independent of Phase 3; can run in parallel with it. Depends on Phase 2.

**Spec:** @docs/specs/features/dia/diageometry2d/transform.md

| # | Task | Status | Notes |
|---|------|--------|-------|
| 4.1 | Migrate `Transform2D.*` to `DiaGeometry2D/Transform/Transform.h/.cpp/.inl`; rename class; update namespace | Not Started | |
| 4.2 | Remove `Transform/` from `DiaMaths.vcxproj`; add to `DiaGeometry2D.vcxproj` | Not Started | |
| 4.3 | Update all callers of `Dia::Maths::Transform2D` | Not Started | From 2.1 hit list |
| 4.4 | Build clean; write `TestTransform.cpp` | Not Started | |

### Phase 5 — DiaCore: Handle\<T\>
Prerequisite for all spatial structure features. Can be done in parallel with Phase 3/4.

| # | Task | Status | Notes |
|---|------|--------|-------|
| 5.1 | Implement `Dia::Core::Handle<T>` in `Dia/DiaCore/Containers/Handle.h` | Not Started | Index + generation counter; `IsValid()`, `Invalid()` static |
| 5.2 | Add `Handle.h` to `DiaCore.vcxproj` + `.filters` | Not Started | |
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
