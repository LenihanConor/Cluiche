# Plan: 3D Shape Primitives

**Spec:** @docs/specs/features/dia/diageometry3d/shape-primitives.md  
**Status:** In Progress  
**Started:** 2026-05-18  
**Last Updated:** 2026-05-18

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Create directory layout + vcxproj + sln wiring | Build passes | Not Started | sonnet | `Dia/DiaGeometry3D/{Shapes,Intersection,Spatial,Testing,Docs}/`; DiaGeometry3D.vcxproj (static lib); add to Cluiche.sln |
| 2 | `IntersectionClassify.h` | — | Not Started | haiku | Enum only; `Dia/DiaGeometry3D/Shapes/IntersectionClassify.h` |
| 3 | `AABB.h/.cpp/.inl` | TestAABB | Not Started | sonnet | AC 11: min/max, FromCenterExtents, CalculateCenter/Extents/Volume/SurfaceArea, Encapsulate(point/AABB), IsIntersecting/Contains(point) |
| 4 | `OOBB.h/.cpp/.inl` | TestOOBB | Not Started | sonnet | AC 12: center + halfExtents + Quaternion orientation, GetAxes() |
| 5 | `Sphere.h/.cpp/.inl` | TestSphere | Not Started | sonnet | AC 13: center + radius, CalculateVolume/SurfaceArea, Encapsulate(point), IsIntersecting/Contains |
| 6 | `Capsule.h/.cpp/.inl` | TestCapsule | Not Started | sonnet | AC 14: startA + endB + radius, CalculateLength/Axis/AxisDirection, IsIntersecting/Contains |
| 7 | `Triangle.h/.cpp/.inl` | TestTriangle | Not Started | sonnet | AC 15: v0/v1/v2, CalculateNormal (RH cross), CalculateArea, CalculateCentroid |
| 8 | `Cylinder.h/.cpp/.inl` | TestCylinder | Not Started | sonnet | AC 16: startA + endB + radius (capped), CalculateLength/Axis/AxisDirection, IsIntersecting/Contains |
| 9 | `Ray.h/.cpp/.inl` | TestRay3D | Not Started | sonnet | AC 17: origin + unit direction, assert near-zero in debug, GetPointAt(t) |
| 10 | `Plane.h/.cpp/.inl` + `PlaneSide` enum | TestPlane3D | Not Started | sonnet | AC 18: (normal, d) form, DistanceTo signed, ClassifyPoint, FromPointAndNormal, FromThreePoints |
| 11 | `Frustum.h/.cpp/.inl` + `FrustumPlane` enum | TestFrustum | Not Started | sonnet | AC 19: six Planes (inward normals), Gribb–Hartmann FromMatrix44, CalculateAABB (8 corners), IsIntersecting/Contains |
| 12 | `Testing/Geometry3DShapeFactory.h` | Used by tests | Not Started | sonnet | MakeUnitAABB, MakeUnitSphere, MakeAxisAlignedTriangle, MakeIdentityFrustum, MakeXZPlane, MakeYAxisCapsule |
| 13 | `Docs/dia.geometry3d.architecture.module.md` | — | Not Started | haiku | YAML frontmatter; lists all 9 shapes + IntersectionClassify; dependent_modules: dia.maths, dia.core |
| 14 | Register all shape files in vcxproj + filters | Build passes | Not Started | haiku | 9 × 3 files (h/cpp/inl) + IntersectionClassify.h + Testing + Docs under Shapes/Testing/Docs filters |
| 15 | Write tests (9 test files) | All green | Not Started | sonnet | One file per shape under `Cluiche/Tests/GoogleTests/Geometry3D/`; register in GoogleTests.vcxproj + filters + add DiaGeometry3D.lib dependency |
| 16 | `dia run googletest --filter="AABB*:OOBB*:Sphere*:Capsule*:Triangle*:Cylinder*:Ray3D*:Plane3D*:Frustum*"` | All green | Not Started | haiku | Verify and report pass/fail + any failures |

## Session Notes

### 2026-05-18

**Spec decisions summary:**

Platform/Dia constraints: no STL in public API; `Directory.Build.props` owns toolchain settings (do NOT set PlatformToolset, LanguageStandard, OutDir, IntDir in the new vcxproj); all files explicitly listed in vcxproj; C++20; x64 only; namespace `Dia::Geometry3D::`.

Shape-primitives decisions: `IntersectionClassify` lives in `Dia/DiaGeometry3D/Shapes/IntersectionClassify.h` and is **not** shared with DiaGeometry2D (SD-013). All type names drop the `3D` suffix — namespace provides dimensionality. OOBB orientation is `Quaternion` stored; `GetAxes()` returns three `Vector3D` values (not `Matrix33`). `Frustum` normals point **inward** (point inside = +normal side of every plane). `Frustum::CalculateAABB()` is required (used by SpatialGrid3D QueryFrustum). `Frustum::FromMatrix44` uses Gribb–Hartmann extraction. Triangle `CalculateNormal` uses `(v1-v0).Cross(v2-v0).AsNormal()` — right-handed. Default `AABB()` = zero-volume at origin. `Encapsulate` provided only on AABB and Sphere. `Ray` constructor asserts non-zero direction in debug.

DiaMaths prerequisites confirmed Done: `Vector3D::Cross`, `Quaternion`, `Matrix44`, `Matrix34`, `Transform3D`.

Test utilities (`Geometry3DShapeFactory.h`) live in `Dia/DiaGeometry3D/Testing/` per SD-012.

Mirror the DiaGeometry2D vcxproj as a template for the new static library project — but do NOT copy build settings that Directory.Build.props owns.
