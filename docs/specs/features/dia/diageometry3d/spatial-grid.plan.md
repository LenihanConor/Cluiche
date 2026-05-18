# Plan: SpatialGrid3D

**Spec:** @docs/specs/features/dia/diageometry3d/spatial-grid.md  
**Status:** Done  
**Started:** 2026-05-18  
**Last Updated:** 2026-05-18

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Create `Dia/DiaGeometry3D/Spatial/ISpatialStructure3D.h` | — | Done | sonnet | Interface per ACs 1–4: kMaxQueryResults=1024, 6 virtual queries + Insert/Remove/Update/Clear/Resolve |
| 2 | Create `Dia/DiaGeometry3D/Spatial/SpatialGrid3D.h` | — | Done | sonnet | Class declaration per ACs 5–14 + API design: template<T, MaxObjects=2048, MaxCells=4096>; Def struct; private: Slot, mSlots, mFreeList, mCells, mCellCountX/Y/Z, mCellSize, mWorldBounds; helper methods |
| 3 | Implement core: Insert/Remove/Update/Clear/Resolve + QueryRegion/Sphere/Point | SpatialGrid3DInsertTest, SpatialGrid3DStaleHandleTest, SpatialGrid3DMultiCellTest, SpatialGrid3DUpdateTest, SpatialGrid3DQueryRegionTest, SpatialGrid3DQuerySphereTest, SpatialGrid3DQueryPointTest | Done | sonnet | `SpatialGrid3D.inl`: slot pool, free list (LIFO, highest-first init), cell index math, Insert bumps generation (skip 0 on wraparound), Remove leaves generation unchanged, visited bitset dedup |
| 4 | Implement `QueryRay` (3D DDA) | SpatialGrid3DQueryRayTest | Done | sonnet | Amanatides–Woo cell traversal: tMax/tDelta per axis, stepX/Y/Z, exit when outside grid |
| 5 | Implement `QueryFrustum` (priority) | SpatialGrid3DQueryFrustumTest | Done | sonnet | `Frustum::CalculateAABB()` prefilter → iterate cells in AABB range → per-cell frustum-vs-cell-AABB test (skip wholly-outside) → per-object `IntersectionTests::Test(AABB, Frustum)` |
| 6 | Implement `QueryKNearest` (expanding-ring) | SpatialGrid3DKNearestTest | Done | sonnet | Concentric cell shells from point's cell; sort by squared distance to slot AABB center; stop expanding when shell min-distance > kth result distance |
| 7 | Debug accessors + Clear | SpatialGrid3DClearTest, SpatialGrid3DBasicTest | Done | haiku | GetCellCountX/Y/Z, GetCellSize, GetWorldBounds, GetCellCount, GetObjectCount |
| 8 | Update `dia.geometry3d.architecture.module.md` | — | Done | haiku | Added ISpatialStructure3D + SpatialGrid3D to public_api.entry_points |
| 9 | Register in `DiaGeometry3D.vcxproj` + `.vcxproj.filters` | Build passes | Done | haiku | Spatial/ISpatialStructure3D.h, SpatialGrid3D.h, SpatialGrid3D.inl registered under Spatial filter |
| 10 | Write tests | All green | Done | sonnet | `Cluiche/Tests/GoogleTests/Geometry3D/TestSpatialGrid3D.cpp`; registered in GoogleTests.vcxproj + filters; fixed stack overflow by heap-allocating all Grid instances |
| 11 | `dia run googletest --filter="SpatialGrid3D*"` | All green | Done | haiku | 28/28 passed (18ms) |

## Session Notes

### 2026-05-18

**Spec decisions summary:**

Inherits all Platform/Dia/DiaGeometry3D binding decisions (no STL, `Dia::Geometry3D::` namespace, C++20, x64, no vcxproj overrides).

SpatialGrid3D-specific decisions: third template param `MaxCells` (default 4096) because 4096 = 16³ is too small for many 3D worlds — tuned per-instance (SD-007). Six query types including `QueryFrustum` as priority (SD-008). `kMaxObjectsPerCell = 64` (mirrors 2D). Cell index math: `cz * (cellCountX * cellCountY) + cy * cellCountX + cx`. Constructor asserts `cellCountX * cellCountY * cellCountZ <= MaxCells`. Free list initialised highest-index-first so first Insert returns slot 0. Generation 0 = "never used" sentinel; bump skips 0 on wraparound. `Resolve()` returns nullptr if generation mismatches. Query dedup via stack-allocated visited bitset of size MaxObjects. **Not thread-safe** — document in source. `QueryFrustum` is three-level: Frustum AABB bound → per-cell frustum-vs-AABB (skip outside cells) → per-object test. `QueryFrustum` depends on `Frustum::CalculateAABB()` from shape-primitives (confirmed added in that spec). `QueryRay` uses Amanatides–Woo 3D DDA. `QueryKNearest` uses expanding-ring with early termination.

**Blocked until shape-primitives AND intersection-tests are both Done.**
