# Plan: 3D Intersection Tests

**Spec:** @docs/specs/features/dia/diageometry3d/intersection-tests.md  
**Status:** Not Started  
**Started:** —  
**Last Updated:** 2026-05-18

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Create `Dia/DiaGeometry3D/Intersection/IntersectionTests.h` | — | Not Started | sonnet | Full class declaration: all Test() + Contains() + ClosestPoint() overloads |
| 2 | Implement priority pairs | IntersectionAABBFrustumTest, IntersectionSphereFrustumTest, IntersectionTriangleFrustumTest, IntersectionAABBAABBTest, IntersectionAABBSphereTest, IntersectionSphereSphereTest, ray casts | Not Started | sonnet | ACs 6–15: Vs-Frustum tests, AABB-AABB, AABB-Sphere+sym, Sphere-Sphere, Ray vs AABB/Sphere/Triangle/Plane |
| 3 | Implement fast-path pairs | IntersectionAABBOOBBTest, IntersectionOOBBOOBBTest, IntersectionTriangleAABBTest, IntersectionTriangleSphereTest | Not Started | sonnet | ACs 16–19: OOBB SAT (15-axis), Akenine-Möller Triangle-vs-AABB, Triangle-vs-Sphere |
| 4 | Implement Contains + ClosestPoint | ContainsTest, ClosestPointTest | Not Started | sonnet | ACs 21–30: per-shape point queries; OOBB local-space via Quaternion::Inverse; Triangle barycentric all 7 regions |
| 5 | Update `dia.geometry3d.architecture.module.md` | — | Not Started | haiku | Add `IntersectionTests` to public_api.entry_points |
| 6 | Register in `DiaGeometry3D.vcxproj` + `.vcxproj.filters` | Build passes | Not Started | haiku | Add `Intersection/IntersectionTests.h` and `.cpp` under Intersection filter |
| 7 | Write tests | All green | Not Started | sonnet | `Cluiche/Tests/GoogleTests/Geometry3D/TestIntersectionTests.cpp`; register in GoogleTests.vcxproj + filters |
| 8 | `dia run googletest --filter="Intersection*:Contains*:ClosestPoint*"` | All green | Not Started | haiku | Verify and report pass/fail + any failures |

## Session Notes

### 2026-05-18

**Spec decisions summary:**

Inherits all Platform/Dia/DiaGeometry3D binding decisions from shape-primitives (no STL, `Dia::Geometry3D::` namespace, C++20, x64, no vcxproj overrides of Directory.Build.props settings).

IntersectionTests-specific decisions: static class, unified `Test(a, b)` overloads — symmetric pairs (e.g. `Test(AABB, Sphere)` and `Test(Sphere, AABB)`) are separate overloads delegating to a shared implementation; `kAContainsB` ↔ `kBContainsA` swapped for the reversed call. Ray pairs return only `kPenetrating` or `kNoIntersection` (rays cannot contain volumes). `Test(AABB, Frustum)` returns `kBContainsA` when all 8 AABB corners are inside the frustum; `kAContainsB` when all 8 frustum corners are inside the AABB (requires computing frustum corners). Allocation-free — all SAT axes on stack. OOBB SAT uses full 15 axes (no shortcut). Akenine-Möller for Triangle-vs-AABB. Möller–Trumbore for Ray-vs-Triangle; coplanar = `kNoIntersection`. `ClosestPoint(Triangle, point)` handles all 7 barycentric regions. `Contains(OOBB, point)` transforms point to OOBB local space via `Quaternion::Inverse`.

**Blocked until shape-primitives is Done.**
