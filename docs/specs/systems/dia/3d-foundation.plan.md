# Plan: 3D Foundation (DiaMaths 3D + DiaGeometry3D + DiaGeometryBridge)

This is a multi-system implementation plan covering 11 feature specs across three system specs. It is not a single-system plan — the dependency chain (DiaMaths → DiaGeometry3D → DiaGeometryBridge) makes one tracking surface clearer than three.

## Scope

| System | Spec | Status | Features |
|--------|------|--------|----------|
| DiaMaths | @docs/specs/systems/dia/diamaths.md | Approved | 6 |
| DiaGeometry3D | @docs/specs/systems/dia/diageometry3d.md | Approved | 3 |
| DiaGeometryBridge | @docs/specs/systems/dia/diageometrybridge.md | Approved | 2 |

Reference scoping plan: `C:\Users\clenihan\.claude\plans\peppy-weaving-pond.md` (the original implementation-shaped plan; superseded by this spec-tracked plan for execution).

## Spec Decisions Summary (Platform → App → System)

**Platform (Cluiche.md):** StringCRC IDs, no STL in public APIs (PD-004), x64 only, MSBuild, C++20, `Directory.Build.props` owns OutDir/IntDir.

**Application (dia.md):** Module YAML docs (AD-001), no STL in public APIs (AD-002), `Dia::<Module>::` namespace (AD-003).

**System — DiaMaths:**
- SD-001: Pure linear algebra only (no shapes, no intersection tests).
- SD-002: Single `Dia::Maths::` namespace, no sub-namespaces per submodule.
- SD-003: Quaternion lives in new `Dia/DiaMaths/Quaternion/` submodule.
- SD-004: Transform3D exposes both Quaternion and Euler setter overloads; storage is always Quaternion.
- SD-005: All matrices share row-major `float m[N][N]` (Matrix22/33/44/34 consistent).
- SD-006: Matrix element access via `operator()(row, col)` and public `m[N][N]`.
- SD-007: Y-up, right-handed.
- SD-009: Transform3D parent pointer raw, non-owning, no cycles (debug asserts).
- SD-010: Cleanup deletes `Shape/2D/`, `Shape/Common/`, and module docs.

**System — DiaGeometry3D:**
- SD-001: `Dia::Geometry3D::` namespace.
- SD-002: Drop `3D` suffix from type names (AABB, OOBB, Sphere, Capsule, Triangle, Cylinder, Ray, Plane, Frustum).
- SD-003: Unified static `IntersectionTests::Test()` overloads.
- SD-004: Spatial structures templated on stored type with `Handle<T>` identity.
- SD-006: Y-up RH (matches DiaMaths).
- SD-007: `SpatialGrid3D` adds `MaxCells` as third template param (default 4096).
- SD-008: Six spatial queries: Region, Sphere, Point, Ray, **Frustum**, KNearest.
- SD-009: Quaternion-primary rotation, Matrix33 conversions for OOBB.
- SD-013: `IntersectionClassify` duplicated, not shared with 2D.
- SD-014: First-round priority pairs are culling-driven.

**System — DiaGeometryBridge:**
- SD-001: `Dia::GeometryBridge::` namespace.
- SD-002: Only Dia module that depends on both DiaGeometry2D and DiaGeometry3D.
- SD-003: Free functions, not static-class methods or shape methods.
- SD-004: `Axis2D` enum mandatory parameter — no default plane.
- SD-005: Lift helpers require caller-supplied missing-axis extent.
- SD-010: Bridge owns no shape types.

**Driver:** 3D rendering culling — Frustum-vs-AABB is the priority query.
**Deferred:** Visual debugger (DiaGraphics has no 3D debug primitives) and DiaGraphics 2D-Transform consolidation — both on `docs/BACKLOG.md`.

## Implementation Patterns

### Phase 1 — DiaMaths 3D additions

**File layout pattern.** Mirror `Matrix33.h/.cpp/.inl` and `Transform2D.h/.cpp/.inl` exactly:
- `.h`: class declaration, `DIA_TYPE_DECLARATION` macro, public `m` array (matrices), inline-eligible methods declared.
- `.inl`: trivial methods (constructors, member access, equality, simple arithmetic).
- `.cpp`: non-trivial methods (factories, projection builders, Inverse, Determinant, Slerp, etc.).
- `.inl` included at bottom of `.h` after the class declaration.

**Naming pattern.** `From<X>` static factories (Matrix33 already uses `FromTranslation`, `FromRotation`, `FromScale`, `FromTRS`; Quaternion adds `FromAxisAngle`, `FromEuler`, `FromMatrix33`, `FromMatrix44`, `LookRotation`). Consistent across the matrix family.

**Validation pattern.** `IsValid()` returns false for NaN/Infinite/zero-magnitude inputs (mirrors Vector3D). `Identity()` static + default constructor produces identity. `Inverse()` returns identity + asserts in debug for singular inputs.

**Submodule docs.** Each new submodule (`Dia/DiaMaths/Quaternion/`) gets its own architecture YAML (`dia.maths.quaternion.architecture.module.md`). Parent doc (`Dia/DiaMaths/Docs/dia.maths.architecture.module.md`) updates `dependent_modules` list — adds `dia.maths.quaternion`, removes `dia.maths.shape` (after cleanup feature).

**Tests.** Live in `Cluiche/Tests/GoogleTests/Maths/Test<Type>.cpp`. Include shape-method coverage + epsilon-equal helpers. Per system SD-013, test fixtures shared across module live in `Dia/DiaMaths/Testing/` (created when a second consumer needs them; v1 uses inline helpers).

### Phase 2 — DiaGeometry3D

**Module skeleton pattern.** Mirror `Dia/DiaGeometry2D/`:
```
Dia/DiaGeometry3D/
├── Shapes/                  # 9 shape types + IntersectionClassify enum
├── Intersection/            # IntersectionTests static class
├── Spatial/                 # ISpatialStructure3D + SpatialGrid3D
├── Testing/                 # Geometry3DShapeFactory, epsilon helpers
├── Docs/dia.geometry3d.architecture.module.md
├── DiaGeometry3D.vcxproj
└── DiaGeometry3D.vcxproj.filters
```

**Shape pattern.** Each shape is a value type: copy-constructible, copy-assignable, equality-comparable, default constructor produces deterministic-but-degenerate state. Shape methods include `IsIntersecting(Vector3D point)` and `Contains(Vector3D point)` for closed-volume shapes (per Q1 in shape-primitives feature spec — mirrors AARect2D pattern). All shape-vs-shape intersection lives in the central `IntersectionTests::Test()`.

**Frustum convention.** Inward-pointing plane normals — a point inside the frustum is on the +normal side of every plane. `Frustum::FromMatrix44` uses Gribb–Hartmann extraction. `Frustum::CalculateAABB()` returns the bounding AABB of the 8 corner vertices (used by SpatialGrid3D::QueryFrustum prefilter).

**SpatialGrid3D pattern.** Mirror 2D `SpatialGrid<T, MaxObjects>` mechanics verbatim with 3D substitutions:
- Slot pool with generation counters.
- LIFO free list (`uint32_t mFreeList[MaxObjects]`).
- Dense `mCells[MaxCells]` array of `DynamicArrayC<uint32_t, kMaxObjectsPerCell=64>`.
- Visited bitset for query deduplication.
- Cell index: `cellIdx = cz * (cellCountX * cellCountY) + cy * cellCountX + cx`.
- Constructor asserts `cellCountX * cellCountY * cellCountZ <= MaxCells`.

**QueryFrustum (the priority).** Three-level culling per system Q7: AABB-of-frustum prefilter → per-cell frustum test → per-object frustum test.

**QueryRay.** Amanatides–Woo 3D DDA traversal (standard technique for voxel grids).

**QueryKNearest.** Expanding-ring traversal with early termination when current shell's minimum distance exceeds the kth-best slot's distance.

### Phase 3 — DiaGeometryBridge

**Module skeleton pattern.** Free-function helpers only — no class wrappers, no methods on shape types. Two filter directories:
```
Dia/DiaGeometryBridge/
├── Project/Project.h, Project.cpp     # 3D → 2D
├── Lift/Lift.h, Lift.cpp              # 2D → 3D
├── Axis2D.h                           # shared enum
├── Testing/BridgeTestFactory.h
├── Docs/dia.geometrybridge.architecture.module.md
├── DiaGeometryBridge.vcxproj
└── DiaGeometryBridge.vcxproj.filters
```

**Driver-only v1.** Per system spec Q11 + user direction:
- Project: AABB → AARect, Sphere → Circle, Frustum → AARect (conservative bound), Vector3D → Vector2D.
- Lift: AARect → AABB, Vector2D → Vector3D.
- Triangle/Ray/Plane/OOBB/Capsule/Cylinder projections + Circle lifts deferred.

**Coordinate mapping.** Documented prominently in source (and tested for all three planes):
- `kXY` → 2D (x,y) maps to 3D (x, y, missingAxis); missing axis = Z.
- `kXZ` → 2D (x,y) maps to 3D (x, missingAxis, y); missing axis = Y.
- `kYZ` → 2D (x,y) maps to 3D (missingAxis, x, y); missing axis = X.

**Module skeleton ownership.** Whichever feature lands first (Projection Helpers per recommended order) creates the vcxproj, sln entry, and architecture doc. The second feature (Lift Helpers) appends to them.

## Tasks

Tasks are ordered by dependency. Each task is a feature spec implementation; reference the spec for full ACs, test plan, and binding-decisions compliance.

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | **Vector3D::Cross** — `Vector3D::Cross(rhs)` method, right-handed (X×Y=Z) | `Vector3D*` filter | Done | sonnet | @docs/specs/features/dia/diamaths/vector3d-cross.md. Smallest feature — single method on existing class. ~30 lines. |
| 2 | **Quaternion** — new `Dia/DiaMaths/Quaternion/` submodule; xyzw storage; Hamilton convention; Slerp/Nlerp; FromAxisAngle/FromEuler(YXZ)/FromMatrix33/44; LookRotation | `Quaternion*` filter | Done | sonnet | @docs/specs/features/dia/diamaths/quaternion.md. 38/38 tests pass. `FromMatrix44`/`ToMatrix44` deferred — implement when Matrix44 lands (task 3). |
| 3 | **Matrix44** — row-major `float m[4][4]`; Identity/FromTranslation/FromRotation/FromScale/FromTRS; Perspective(Angle, …)/Orthographic/LookAt (Y-up RH, OpenGL [-1,1] depth); Inverse/Determinant/Transpose; GetColumnMajor for GL/glTF upload | `Matrix44*` filter | Done | sonnet | @docs/specs/features/dia/diamaths/matrix44.md. Largest single file (Inverse alone ~80 lines). Mirror Matrix33 patterns. Includes Quaternion::FromMatrix44/ToMatrix44. |
| 4 | **Matrix34** — row-major `float m[3][4]` affine, no projection row; FromMatrix44/ToMatrix44; affine-specific Inverse | `Matrix34*` filter | Pending | sonnet | @docs/specs/features/dia/diamaths/matrix34.md. Depends on Matrix44. |
| 5 | **Transform3D** — Vector3D position + Quaternion rotation + Vector3D scale; raw non-owning parent pointer; world-space getters AND setters; Euler-overload setters (YXZ); GetForward/Right/Up (Y-up RH); LookAt; debug cycle detection | `Transform3D*` filter | Pending | sonnet | @docs/specs/features/dia/diamaths/transform3d.md. Mirrors Transform2D verbatim with 3D types. Last DiaMaths 3D-additions feature in dependency order. |
| 6 | **DiaMaths Shape Cleanup** — pre-deletion grep verification (zero hits); delete `Shape/2D/`, `Shape/Common/`, parent module docs; update `DiaMaths.vcxproj` and parent `dia.maths.architecture.module.md` `dependent_modules` | `dia pipeline --target googletest` Debug+Release green | Pending | haiku | @docs/specs/features/dia/diamaths/shape-cleanup.md. Mechanical deletion + verification. Recommended last in DiaMaths batch (after 3D features ship clean) so the cleanup is the only variable. |
| 7 | **DiaGeometry3D Shape Primitives** — 9 shapes (AABB, OOBB, Sphere, Capsule, Triangle, Cylinder, Ray, Plane, Frustum) + IntersectionClassify enum; Frustum::FromMatrix44 (Gribb–Hartmann); Frustum::CalculateAABB; new `DiaGeometry3D.vcxproj`; `dia.geometry3d.architecture.module.md` | One test file per shape; `AABB*:OOBB*:Sphere*:Capsule*:Triangle*:Cylinder*:Ray3D*:Plane3D*:Frustum*` all green | Pending | sonnet | @docs/specs/features/dia/diageometry3d/shape-primitives.md. Largest task — 9 shapes × 3 files each + module skeleton + 9 test files. Cylinder confirmed in v1 per user direction. |
| 8 | **DiaGeometry3D Intersection Tests** — `IntersectionTests::Test()` priority pairs (AABB/Sphere/Triangle vs Frustum, ray casts, AABB-vs-AABB, Sphere-vs-Sphere, AABB-vs-Sphere); fast paths (OOBB SAT, Triangle-vs-AABB Akenine-Möller, Triangle-vs-Sphere); Contains + ClosestPoint matrix | `IntersectionTests*` all green | Pending | sonnet | @docs/specs/features/dia/diageometry3d/intersection-tests.md. SAT + analytic ray casts + Möller–Trumbore. Complex but bounded. |
| 9 | **DiaGeometry3D SpatialGrid3D** — `ISpatialStructure3D<T>` interface + `SpatialGrid3D<T, MaxObjects=2048, MaxCells=4096>`; six queries including QueryFrustum (AABB prefilter + per-cell + per-object); Amanatides–Woo DDA for QueryRay; expanding-ring KNearest | `SpatialGrid3D*` all green; `QueryFrustum` test asserts correct subset for known eye/target | Pending | opus | @docs/specs/features/dia/diageometry3d/spatial-grid.md. Most complex feature — template heavy, three Query algorithms, mirrors 2D verbatim with 3D substitutions. opus for the QueryFrustum strategy and edge-case handling. |
| 10 | **DiaGeometryBridge Projection Helpers** — driver-only v1: `Project(AABB, Axis2D)`, `Project(Sphere, Axis2D)`, `Project(Frustum, Axis2D)` (conservative AABB), `Project(Vector3D, Axis2D)`; new `DiaGeometryBridge.vcxproj`; `Axis2D.h`; `dia.geometrybridge.architecture.module.md` | `ProjectAABB*:ProjectSphere*:ProjectFrustum*:ProjectVector*` all green | Pending | haiku | @docs/specs/features/dia/diageometrybridge/projection-helpers.md. Mostly thin wrappers. Frustum corner extraction is the only non-trivial part. Creates the bridge module skeleton consumed by task 11. |
| 11 | **DiaGeometryBridge Lift Helpers** — driver-only v1: `Lift(AARect, Axis2D, missingAxisMin, missingAxisMax)`, `Lift(Vector2D, Axis2D, missingAxisValue)`; appends to `DiaGeometryBridge.vcxproj` and module doc | `LiftAARect*:LiftVector*:LiftRoundTrip*` all green | Pending | haiku | @docs/specs/features/dia/diageometrybridge/lift-helpers.md. Smallest task. Coordinate-mapping table from feature spec is the canonical reference. |

## Verification (per CLAUDE.md verify skill)

Each task's verification gate:
1. **Per-feature test filter green** in Debug build (`dia run googletest --filter="<filter>"`).
2. **`dia pipeline --target googletest` Debug + Release** clean — no new warnings.
3. **Spec compliance check** — re-read the feature spec's ACs before marking task Done; verify each AC has corresponding test coverage or quoted code that satisfies it.
4. **Commit per task** before moving to the next.

After Phase 1 (tasks 1–6) complete:
- Update `diamaths.md` Status to `Done`.
- Update `Dia/DiaMaths/Docs/dia.maths.architecture.module.md` to drop `dia.maths.shape` and add `dia.maths.quaternion`.

After Phase 2 (tasks 7–9) complete:
- Update `diageometry3d.md` Status to `Done`.

After Phase 3 (tasks 10–11) complete:
- Update `diageometrybridge.md` Status to `Done`.

## Risks / Gotchas

- **Quaternion ↔ Matrix44 forward-declare loop.** Resolve by implementing Quaternion's `FromMatrix44`/`ToMatrix44` in `Quaternion.cpp` after Matrix44 lands. Quaternion.h forward-declares Matrix44; conversions land in same commit window.
- **Matrix44 row-major + GL upload.** Easy to write `glUniformMatrix4fv(loc, 1, GL_FALSE, m.GetData())` and get transposed matrices on the GPU. The API exposes `GetColumnMajor(float[16])` instead of a raw pointer to force the call site to acknowledge the transpose. Tests verify the column-major output matches the canonical GL identity buffer.
- **OOBB orientation storage.** Quaternion per SD-009. Tests must include rotated OOBBs (not just axis-aligned) to exercise the `GetAxes()` + SAT path.
- **Frustum normal direction.** Inward-pointing. A point inside the frustum is on +normal side of every plane. Easy to flip during Gribb–Hartmann implementation; tests assert with a known view-projection.
- **SpatialGrid3D MaxCells assertion.** `cellCountX * cellCountY * cellCountZ <= MaxCells` — the constructor asserts. Test the boundary: 16×16×16 = 4096 (default cap, just fits); 17×17×17 = 4913 (must reject under default, accept under MaxCells=8192).
- **SpatialGrid3D QueryFrustum cell traversal.** AABB-of-frustum prefilter narrows the cells; the per-cell frustum test must use the cell's AABB (not its center) to avoid false negatives. Test specifically: a cell straddling a frustum plane should be visited.
- **Pre-cleanup grep.** Task 6 (shape cleanup) requires zero hits on `#include <DiaMaths/Shape/` and `Dia::Maths::Circle/AARect/...` symbols. If hits exist, the task is BLOCKED until call sites are migrated. Do this grep early (alongside task 1 ideally) so a long migration is not surprise work mid-rollout.
- **DiaGraphics 2D Transform duplication.** Out of scope here. Tracked as trailing implementation detail in `peppy-weaving-pond.md`. Do not bundle.

## Session Notes

(Append session-by-session notes here as tasks are dispatched and completed. Each entry should reference the task number and any decisions, blockers, or deviations.)

### 2026-05-17 — Task 3 (Matrix44) Done

Matrix44 + Quaternion::FromMatrix44/ToMatrix44 landed in single commit. Files created: `Dia/DiaMaths/Matrix/{Matrix44.h,.cpp,.inl}`, `Cluiche/Tests/GoogleTests/Maths/TestMatrix44.cpp`. Row-major `float m[4][4]`, all factory methods (Identity/FromTranslation/FromRotation/FromScale/FromTRS/Perspective/Orthographic/LookAt), full 4×4 cofactor Inverse, Transpose, Determinant, GetColumnMajor, Transform{Point,Direction,Vector4}, component extraction (GetTranslation/GetRotation/GetScale). Quaternion conversions: `FromMatrix44` extracts upper-left 3×3 and delegates to existing FromMatrix33; `ToMatrix44` produces 4×4 with rotation + (0,0,0,1) bottom row. Y-up RH, OpenGL [-1,1] depth per spec. vcxproj/filters updated. Module arch doc updated (entry_points += Matrix33, Matrix44). **Tests:** 39/39 Matrix44 tests green, 42/42 Quaternion tests green (38 previous + 4 new Matrix44 conversion tests). Debug pipeline clean. All 23 spec ACs satisfied.

**Deviation from plan note:** Task 2 originally deferred Matrix44 conversions; Task 3 implemented them in same window as planned.

### 2026-05-17 — Task 2 (Quaternion) Done

Quaternion submodule landed: `Dia/DiaMaths/Quaternion/{Quaternion.h,.cpp,.inl,arch doc}` + `Cluiche/Tests/GoogleTests/Maths/TestQuaternion.cpp`. xyzw storage, Hamilton convention, YXZ Euler, Slerp/Nlerp, FromAxisAngle/FromEuler/FromMatrix33/LookRotation. 38/38 tests pass under Debug. **Deviation:** `FromMatrix44`/`ToMatrix44` not implemented — Matrix44 lands in task 3, conversions will be added there to avoid a circular header dependency (Quaternion.h forward-declares only Matrix33 today). vcxproj/filters and parent module doc (`dependent_modules += dia.maths.quaternion`) updated.

**Build status note:** GoogleTests Release config has 91 unresolved-external link errors pre-existing on `Development` (DebugLayerManager / FixedDrawRegistry / winmm symbols) — confirmed by stash test. Logged to `docs/BACKLOG.md` Loose Ends. Debug pipeline clean. Continuing on Debug-only verification until Release breakage is repaired.

### 2026-05-17 — Plan authored

All 11 feature specs Approved. All 5 spec steps complete for each (interview → draft → binding decisions → AI review → Approved). Three system specs Approved with feature breakdowns wired up. Ready to dispatch task 1 (Vector3D::Cross).

Backlog items captured during scoping (in `docs/BACKLOG.md`):
- DiaGeometry3DVisualDebugger system — gated on prerequisites.
- DiaGraphics 3D debug primitives — feature under DiaGraphics, hard prerequisite for the visual debugger.

Cross-cutting decisions confirmed during the feature batch interview:
- All matrix layouts row-major (locked by user pushback during DiaMaths spec authorship).
- Quaternion storage xyzw (GLM/glTF convention).
- Matrix44 Perspective takes `Angle` (type-safe over float radians).
- Transform3D world-space setters mirror Transform2D.
- Euler convention YXZ intrinsic.
- Shape methods include `IsIntersecting(point)` and `Contains(point)` per shape (mirrors AARect2D).
- Cylinder in v1.
- SpatialGrid3D::QueryFrustum uses three-level culling.
- DiaGeometryBridge v1 is driver-only (AABB / Sphere / Frustum / Vector3D for Project; AARect / Vector2D for Lift).
