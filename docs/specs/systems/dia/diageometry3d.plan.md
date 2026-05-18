# Plan: DiaGeometry3D System

**Spec:** @docs/specs/systems/dia/diageometry3d.md  
**Status:** In Progress  
**Started:** 2026-05-18  
**Last Updated:** 2026-05-18

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Shape Primitives | `dia run googletest --filter="AABB*:OOBB*:Sphere*:Capsule*:Triangle*:Cylinder*:Ray3D*:Plane3D*:Frustum*"` | Not Started | sonnet | See shape-primitives.plan.md |
| 2 | Intersection Tests | `dia run googletest --filter="Intersection*:Contains*:ClosestPoint*"` | Not Started | sonnet | Blocked until Task 1 Done. See intersection-tests.plan.md |
| 3 | Spatial Grid | `dia run googletest --filter="SpatialGrid3D*"` | Not Started | sonnet | Blocked until Tasks 1+2 Done. See spatial-grid.plan.md |

## Session Notes

### 2026-05-18

**Spec decisions summary (full chain: Platform → Dia → DiaGeometry3D):**

Platform binding decisions active here: no STL containers in any public API (PD-004); x64 only (PD-005); Visual Studio `.vcxproj` files are source of truth — all files explicitly listed (PD-006); C++20 (`/std:c++20`) required (PD-007); `Directory.Build.props` owns `OutDir`, `IntDir`, toolchain, language standard — new vcxproj must NOT override these (PD-008).

Dia application binding decisions: every module has a YAML frontmatter `dia.*.architecture.module.md` doc (AD-001); no STL in public APIs (AD-002); namespace is `Dia::<Module>::` (AD-003).

DiaGeometry3D system decisions: namespace `Dia::Geometry3D::` — no `3D` suffix on type names (SD-001/SD-002); `IntersectionTests` is a static class with unified `Test()` overloads (SD-003); spatial structures templated on stored type with `Handle<T>` identity (SD-004); Y-up, right-handed coordinate convention (SD-006); `SpatialGrid3D` has third template param `MaxCells` (default 4096) because 3D worlds need more cells than 2D (SD-007); six spatial query types including `QueryFrustum` for rendering culling (SD-008); Quaternion-primary rotation storage, `GetAxes()` for matrix-style extraction (SD-009); `IntersectionClassify` enum is duplicated in `Dia::Geometry3D::` — not shared from 2D (SD-013); V1 priority pairs are culling-driven (SD-014); test utilities ship in `Dia/DiaGeometry3D/Testing/` (SD-012).

All DiaMaths prerequisites confirmed Done: `Vector3D::Cross`, `Quaternion`, `Matrix44`, `Matrix34`, `Transform3D` all landed in commits ef06640–15d0fc0.
