# System Spec: DiaMaths

## Parent Application
@docs/specs/applications/dia.md

## Purpose

DiaMaths is the pure linear-algebra and core-mathematics library for the Dia engine. After the DiaGeometry2D migration (system spec `Done`), DiaMaths retains only mathematical primitives that have no geometric semantics: scalars, angles, vectors, matrices, quaternions, transforms, easing/interpolation, trigonometry, and pseudo-random numbers. All shape primitives, intersection tests, and spatial structures live in DiaGeometry2D / DiaGeometry3D.

The system spec exists now because two large pieces of work depend on it:
1. **DiaGeometry3D** (system spec `Approved`) requires `Vector3D::Cross`, `Matrix44`, `Matrix34`, `Quaternion`, `Transform3D`. None of these exist yet.
2. **DiaMaths cleanup** — the DiaGeometry2D migration *copied* `Shape/2D/` and `Shape/Common/` into DiaGeometry2D but did not delete the originals. DiaMaths still ships duplicate shape headers that callers may still resolve to. This must be cleaned up to make DiaMaths the single source of truth for "pure linear algebra only".

**Dependency chain:**  
`DiaGeometry2D, DiaGeometry3D, DiaGraphics, DiaApplicationFlow, ... → DiaMaths → DiaCore`

DiaMaths is at the bottom of the engine stack with no maths-domain dependencies of its own — it depends on DiaCore only for assertions, type macros, and a small number of fundamental utilities.

## Responsibilities

- Own scalar/angle utilities: `FloatMaths`, `CoreMaths`, `Angle`, `Trigonometry`, `Easing`, `Interpolation`, `Random`, `HalfFloat`, `MathsDefines`
- Own vector types: `Vector2D`, `Vector3D`, `Vector4D`, `VectorHalf2D`, plus adapters and `VectorUtils` (vector-level conversions / swizzles)
- Own matrix types: `Matrix22`, `Matrix33`, `Matrix44`, `Matrix34`
- Own rotation type: `Quaternion`
- Own transform types: `Transform2D` (existing), `Transform3D` (new)
- Provide a `DiaMaths.vcxproj` static library project, registered in `Cluiche.sln`
- Maintain the existing `dia.maths.architecture.module.md` parent doc and per-submodule docs (`dia.maths.core`, `dia.maths.vector`, `dia.maths.matrix`, `dia.maths.transform`, `dia.maths.quaternion`)
- Ensure the public API contains no geometry primitives (no shapes, no intersection tests, no spatial structures)

## Non-Responsibilities

- 2D/3D geometric primitives (shapes, lines, rays, planes, frustums) — DiaGeometry2D / DiaGeometry3D
- Intersection tests, contact results, classify enums — DiaGeometry2D / DiaGeometry3D
- Spatial acceleration structures (grid, quadtree, BVH, octree) — DiaGeometry2D / DiaGeometry3D
- Cross-dimension shape conversions — DiaGeometryBridge
- Vector-level dimension conversions (`Vector3DXYFromVector2D` etc.) live here in `VectorUtils` because they are pure-vector operations with no shape semantics — kept in DiaMaths
- Physics simulation, rendering, scene management — out of scope for the maths layer
- Currency/money math, units of measure, big-number arithmetic — out of scope

## Current State (Migration Notes)

After the DiaGeometry2D migration, DiaMaths still contains stale duplicate files that were copied (not moved) into DiaGeometry2D:

- `Dia/DiaMaths/Shape/2D/` — `AARect2D`, `Arc2D`, `Capsule2D`, `Circle2D`, `Ellipse2D`, `IntersectionPoint2D`, `Line2D`, `OORect2D`, `Ray2D`, `Triangle2D` (all `.h` / `.cpp` / `.inl`)
- `Dia/DiaMaths/Shape/Common/` — `IntersectionClassify`, `IntersectionTests` (all `.h` / `.cpp` / `.inl`)
- `Dia/DiaMaths/Shape/dia.maths.shape.architecture.module.md` and child module docs

These belong in DiaGeometry2D and must be deleted from DiaMaths as a feature of this system spec. The `DiaMaths.vcxproj` and `.vcxproj.filters` need the corresponding entries removed, and the module docs (`dia.maths.shape.*`) need to be deleted (per DiaGeometry2D AI Review Q10: "delete them; the code is the source of truth").

## Public Interfaces

### Namespace

All types live under `Dia::Maths::`. Submodule structure mirrors the directory layout but does not introduce sub-namespaces.

### Existing Surface (retained)

```cpp
namespace Dia::Maths {
    // Core scalars
    class Angle { ... };
    class FloatMaths { ... };
    class HalfFloat { ... };
    namespace CoreMaths { ... }
    namespace Trigonometry { ... }
    namespace Easing { ... }
    namespace Interpolation { ... }
    class Random { ... };

    // Vectors
    class Vector2D { ... };
    class Vector3D { ... };  // gains Cross() — see Additions
    class Vector4D { ... };
    class VectorHalf2D { ... };
    namespace VectorUtils { ... }   // dimension conversions, swizzles

    // Matrices (existing)
    class Matrix22 { ... };
    class Matrix33 { ... };

    // Transforms (existing)
    class Transform2D { ... };
}
```

### Additions for 3D

```cpp
namespace Dia::Maths {

    // Vector3D gains Cross() — required by Plane construction, OOBB axes,
    // Frustum plane derivation, Triangle normals, etc.
    class Vector3D {
        // ... existing API ...
        Vector3D Cross(const Vector3D& rhs) const;
    };

    // 4x4 matrix — row-major float m[4][4] storage. Layout matches Matrix22 and Matrix33
    // for cross-dimension code consistency. For OpenGL/glTF upload, callers transpose via
    // GetTransposed() (which returns a column-major 16-float buffer).
    class Matrix44 {
    public:
        Matrix44();
        Matrix44(const Matrix44& other);
        Matrix44(float m00, float m01, float m02, float m03,
                 float m10, float m11, float m12, float m13,
                 float m20, float m21, float m22, float m23,
                 float m30, float m31, float m32, float m33);

        // Factory methods
        static Matrix44 Identity();
        static Matrix44 FromTranslation(const Vector3D& translation);
        static Matrix44 FromRotation(const Quaternion& rotation);
        static Matrix44 FromScale(const Vector3D& scale);
        static Matrix44 FromScale(float uniformScale);
        static Matrix44 FromTRS(const Vector3D& translation, const Quaternion& rotation, const Vector3D& scale);

        // Y-up right-handed builders for the rendering driver
        static Matrix44 Perspective(const Angle& fovY, float aspect, float nearZ, float farZ);
        static Matrix44 Orthographic(float left, float right, float bottom, float top, float nearZ, float farZ);
        static Matrix44 LookAt(const Vector3D& eye, const Vector3D& target, const Vector3D& up);

        // Operators (mirror Matrix33: +, -, *, *=, ==, !=, unary -)
        // Element access — m(row, col), matching Matrix33::operator()
        float& operator()(int row, int col);
        float  operator()(int row, int col) const;

        // Transform
        Vector3D TransformPoint(const Vector3D& point) const;     // includes translation (w=1)
        Vector3D TransformDirection(const Vector3D& dir) const;   // ignores translation (w=0)
        Vector4D TransformVector4(const Vector4D& v) const;

        // Properties
        void     SetIdentity();
        Matrix44 Transpose() const;
        float    Determinant() const;
        Matrix44 Inverse() const;

        // OpenGL/glTF interop — fills a 16-float column-major buffer.
        // Use when uploading to glLoadMatrixf, glUniformMatrix4fv (with GL_FALSE for transpose), etc.
        void GetColumnMajor(float outData[16]) const;

        // Component extraction (assumes affine input — projection matrices undefined behaviour)
        Vector3D    GetTranslation() const;
        Quaternion  GetRotation() const;
        Vector3D    GetScale() const;

        // Direct access — row-major m[row][col], matching Matrix33::m
        float m[4][4];
    };

    // 3x4 affine matrix — cheaper alternative to Matrix44 for transform hierarchies
    // (rotation + translation only, no projection row). Row-major float m[3][4] to match
    // the rest of the matrix family.
    class Matrix34 { ... };

    // Unit quaternion — primary 3D rotation representation per DiaGeometry3D SD-009.
    class Quaternion {
    public:
        Quaternion();                                 // identity (0, 0, 0, 1)
        Quaternion(float x, float y, float z, float w);
        Quaternion(const Quaternion& other);

        // Factory methods
        static Quaternion Identity();
        static Quaternion FromAxisAngle(const Vector3D& axis, const Angle& angle);
        static Quaternion FromEuler(const Angle& yaw, const Angle& pitch, const Angle& roll);  // YXZ Y-up convention
        static Quaternion FromMatrix33(const Matrix33& rotation);
        static Quaternion FromMatrix44(const Matrix44& transform);
        static Quaternion LookRotation(const Vector3D& forward, const Vector3D& up);

        // Operators
        Quaternion& operator=(const Quaternion& other);
        Quaternion  operator*(const Quaternion& rhs) const;     // composition
        Quaternion& operator*=(const Quaternion& rhs);
        bool        operator==(const Quaternion& rhs) const;
        bool        operator!=(const Quaternion& rhs) const;

        // Operations
        Quaternion  Conjugate() const;
        Quaternion  Inverse() const;
        Quaternion& Normalize();
        Quaternion  AsNormal() const;
        float       Dot(const Quaternion& rhs) const;
        float       Magnitude() const;
        bool        IsValid() const;

        // Rotate a vector by this quaternion
        Vector3D    Rotate(const Vector3D& v) const;

        // Conversions
        Matrix33   ToMatrix33() const;
        Matrix44   ToMatrix44() const;     // pure rotation, no translation
        void       ToAxisAngle(Vector3D& outAxis, Angle& outAngle) const;
        void       ToEuler(Angle& outYaw, Angle& outPitch, Angle& outRoll) const;

        // Interpolation
        static Quaternion Slerp(const Quaternion& a, const Quaternion& b, float t);
        static Quaternion Nlerp(const Quaternion& a, const Quaternion& b, float t);

        float x, y, z, w;
    };

    // 3D transform with Vector3D position, Quaternion rotation, Vector3D scale.
    // Mirrors Transform2D's local/world hierarchy semantics (raw-pointer parent, no ownership,
    // no cycles allowed).
    class Transform3D {
    public:
        Transform3D();
        Transform3D(const Vector3D& position, const Quaternion& rotation, const Vector3D& scale);
        Transform3D(const Transform3D& other);
        Transform3D& operator=(const Transform3D& other);

        // Local space
        const Vector3D&   GetLocalPosition() const;
        const Quaternion& GetLocalRotation() const;
        const Vector3D&   GetLocalScale() const;
        void              SetLocalPosition(const Vector3D& position);
        void              SetLocalRotation(const Quaternion& rotation);
        void              SetLocalRotation(const Angle& yaw, const Angle& pitch, const Angle& roll);  // Euler overload
        void              SetLocalScale(const Vector3D& scale);
        void              SetLocalScale(float uniformScale);

        // World space (computed via parent chain)
        Vector3D    GetWorldPosition() const;
        Quaternion  GetWorldRotation() const;
        Vector3D    GetWorldScale() const;
        void        SetWorldPosition(const Vector3D& position);
        void        SetWorldRotation(const Quaternion& rotation);
        void        GetWorldTransform(Vector3D& outPosition, Quaternion& outRotation, Vector3D& outScale) const;

        // Hierarchy
        Transform3D* GetParent() const;
        void         SetParent(Transform3D* parent);
        bool         HasParent() const;

        // Transformations
        void Translate(const Vector3D& delta);
        void TranslateWorld(const Vector3D& delta);
        void Rotate(const Quaternion& delta);
        void Scale(const Vector3D& scale);
        void Scale(float uniformScale);

        // Space conversion
        Vector3D TransformPoint(const Vector3D& localPoint) const;
        Vector3D TransformDirection(const Vector3D& localDirection) const;
        Vector3D InverseTransformPoint(const Vector3D& worldPoint) const;
        Vector3D InverseTransformDirection(const Vector3D& worldDirection) const;

        // Matrix generation
        Matrix44 GetLocalMatrix() const;
        Matrix44 GetWorldMatrix() const;
        Matrix34 GetLocalAffine() const;
        Matrix34 GetWorldAffine() const;

        // Y-up right-handed convenience axes
        Vector3D GetForward() const;   // rotated -Z (Y-up RH)
        Vector3D GetRight() const;     // rotated +X
        Vector3D GetUp() const;        // rotated +Y

        // Look-at
        void LookAt(const Vector3D& target, const Vector3D& up = Vector3D::YAxis());

    private:
        Vector3D     mLocalPosition;
        Quaternion   mLocalRotation;
        Vector3D     mLocalScale;
        Transform3D* mParent;
        void GetParentWorldTransform(Vector3D& outPosition, Quaternion& outRotation, Vector3D& outScale) const;
    };
}
```

## Features

| Feature | Description | Spec | Status |
|---------|-------------|------|--------|
| Vector3D Cross product | Add `Cross(const Vector3D&) const` to Vector3D. Update DiaMaths.vcxproj if needed. | [vector3d-cross.md](../../features/dia/diamaths/vector3d-cross.md) | Draft |
| Matrix44 | 4×4 matrix, column-major float[16], identity/perspective/orthographic/lookAt builders, transpose/inverse/determinant, transform point/direction. Y-up RH. | [matrix44.md](../../features/dia/diamaths/matrix44.md) | Draft |
| Matrix34 | 3×4 affine matrix (rotation+translation, no projection row). Cheaper for transform hierarchies. | [matrix34.md](../../features/dia/diamaths/matrix34.md) | Draft |
| Quaternion | New `DiaMaths/Quaternion/` submodule. Unit quaternion with Slerp/Nlerp, axis-angle and Euler conversions, Matrix33/44 conversions, vector rotation. | [quaternion.md](../../features/dia/diamaths/quaternion.md) | Draft |
| Transform3D | 3D transform with parent-child hierarchy. Vector3D position, Quaternion rotation, Vector3D scale. Both Quaternion and Euler setter overloads. Y-up RH forward/right/up. | [transform3d.md](../../features/dia/diamaths/transform3d.md) | Draft |
| DiaMaths Shape Cleanup | Delete `Dia/DiaMaths/Shape/2D/` and `Dia/DiaMaths/Shape/Common/` and their module docs. Update vcxproj/filters. Verify no consumers still reference the DiaMaths-side headers. | [shape-cleanup.md](../../features/dia/diamaths/shape-cleanup.md) | Draft |

## Dependencies on Other Systems

**Required:**
- **DiaCore** — Assertions (`DIA_ASSERT`), type macros (`DIA_TYPE_DECLARATION`), `StringCRC` for module identity, `Containers/Handle` only if any maths type later needs it (none do at present)

**Dependents:**
- **DiaGeometry2D** — uses `Vector2D`, `Angle`, `Matrix33` for shapes and `Transform`
- **DiaGeometry3D** — uses `Vector3D`, `Matrix44`, `Matrix34`, `Quaternion`, `Transform3D` (this spec is a hard prerequisite for DiaGeometry3D's `Done` status)
- **DiaGraphics** — uses `Vector2D`, `Vector3D`, `Matrix33`, `Matrix44`
- **DiaRigidBody2D**, **DiaSoftBody2D**, **DiaAnimation2D**, **DiaRig2D**, **DiaIK2D** — use 2D types
- All future 3D consumers (DiaRigidBody3D, renderer culling, scene graphs) — use 3D types

## Out of Scope

- Geometry primitives (shapes, intersection tests, spatial structures) — DiaGeometry2D / DiaGeometry3D
- Cross-dimension shape helpers — DiaGeometryBridge
- Currency / units / dimensional analysis
- Big-number / arbitrary-precision arithmetic
- Symbolic algebra
- Auto-differentiation
- SIMD-optimised builds — defer until a profiler shows scalar maths is the bottleneck (not the case in any current benchmark)

## Decisions

| ID | Decision | Rationale | Scope | Status | Binding |
|----|----------|-----------|-------|--------|---------|
| SD-001 | DiaMaths is "pure linear algebra only" — no shapes, no intersection tests, no spatial structures | Single-responsibility split established by DiaGeometry2D migration; DiaMaths is the bottom of the maths/geometry stack | All features | Accepted | Yes |
| SD-002 | All types live under `Dia::Maths::` namespace; no sub-namespaces per submodule | Submodules are organisational (directories, vcxproj filters, module docs) — not a namespace boundary. Mirrors existing `Vector2D`, `Matrix33`, `Transform2D` | All features | Accepted | Yes |
| SD-003 | Quaternion lives in a new `Dia/DiaMaths/Quaternion/` submodule | Sibling to Vector/, Matrix/, Transform/. Quaternion is mathematically distinct from vectors and matrices and warrants its own module surface | Quaternion | Accepted | Yes |
| SD-004 | Transform3D rotation API exposes both `Quaternion` and `Euler` setter overloads; storage is always Quaternion | Quaternion-primary per DiaGeometry3D SD-009 (stable hierarchy composition, no gimbal lock). Euler overloads are convenience for hand-authored code; storage path immediately converts to Quaternion to avoid Euler-as-truth bugs | Transform3D | Accepted | Yes |
| SD-005 | All DiaMaths matrices share the same layout: **row-major `float m[N][N]`** | Cross-dimension consistency. Matrix22 and Matrix33 are already row-major `float m[N][N]` with public `m` access and `operator()(row, col)` (see `Matrix33.h:101`); Matrix44 and Matrix34 mirror this verbatim. Code that ports between dimensions is not surprised. OpenGL/glTF interop is handled by `Matrix44::GetColumnMajor(float[16])` — one transpose at upload time, negligible cost relative to render workloads | Matrix22, Matrix33, Matrix44, Matrix34 | Accepted | Yes |
| SD-006 | Matrix44 element access via `operator()(row, col)` and public `m[4][4]` array | Mirrors `Matrix33::operator()` and `Matrix33::m`. `m(2, 3)` always means row 2, column 3; `m.m[r][c]` allows direct array access for performance-sensitive loops | Matrix44 | Accepted | Yes |
| SD-007 | Y-up, right-handed coordinate convention for all 3D builders (Perspective, LookAt, Orthographic, GetForward) | Aligned with DiaGeometry3D SD-006. `GetForward()` is rotated `-Z` axis; `GetUp()` is rotated `+Y`; `GetRight()` is rotated `+X` | Matrix44, Transform3D, Quaternion | Accepted | Yes |
| SD-008 | Matrix34 is row-major `float m[3][4]` storage, affine-only (no projection row) | Cheaper than Matrix44 for transform hierarchies. Storage matches Matrix22/33/44 row-major convention per SD-005 — promotion to Matrix44 copies rows directly and appends `(0, 0, 0, 1)`. Operations that require a projection row (Perspective, etc.) are not provided | Matrix34 | Accepted | Yes |
| SD-009 | Transform3D parent pointer is raw, non-owning, no cycles permitted | Mirrors Transform2D semantics (`Transform2D.h:114-124`). Caller manages lifetime and acyclicity. Debug build asserts on cycle detection | Transform3D | Accepted | Yes |
| SD-010 | DiaMaths cleanup feature deletes `Shape/2D/`, `Shape/Common/`, `Shape/dia.maths.shape*.architecture.module.md` after verifying no callers reference them | Single-source-of-truth principle. Stale duplicates are worse than none (DiaGeometry2D AI Review Q10) | Shape Cleanup | Accepted | Yes |
| SD-011 | DiaMaths is a static library with no STL containers in public API | Reinforces PD-004 / AD-002. `Vector3D`, `Matrix44`, `Quaternion`, `Transform3D` use only built-in types and DiaMaths types in their interfaces | All features | Accepted | Yes |
| SD-012 | New types follow existing patterns: factory methods (`FromX`), copy ctor, `operator=`, arithmetic operators, `IsValid()`, `Identity()` static | Consistent shape with `Matrix33`, `Transform2D`, `Vector3D`. Reduces cognitive cost when reading new code | Matrix44, Matrix34, Quaternion, Transform3D | Accepted | Yes |
| SD-013 | Test utilities ship under `Dia/DiaMaths/Testing/` (new directory) | Per project memory: test helpers live with the library, not in GoogleTests. Hosts canonical test matrices, quaternions, transforms, epsilon-comparison helpers | All features | Accepted | Yes |

**Status values:** `Proposed` · `Accepted` · `Rejected` · `Superseded`  
**Binding:** `Yes` = enforced constraint on all features in this system · `No` = guidance only

## Inherited Binding Decisions

| ID | Source | Decision | Implication for this system |
|----|--------|----------|----------------------------|
| PD-001 | Platform | StringCRC for all entity/component IDs | Module identified by StringCRC (`kUniqueId`); maths types do not have IDs themselves |
| PD-004 | Platform | No STL containers in public APIs | All public maths APIs use only POD, builtin types, or DiaMaths types — no `std::vector` / `std::array` exposure |
| PD-005 | Platform | x64 only | `DiaMaths.vcxproj` targets x64 exclusively |
| PD-006 | Platform | Visual Studio project files are source of truth | `DiaMaths.vcxproj` and `.vcxproj.filters` updated for every new file; no implicit globbing |
| PD-007 | Platform | C++20 required | All code compiled under `/std:c++20`; can use concepts in templated maths if needed (none currently are) |
| PD-008 | Platform | Directory.Build.props owns OutDir/IntDir/toolchain | `DiaMaths.vcxproj` must NOT override OutDir, IntDir, PlatformToolset, WindowsTargetPlatformVersion, or LanguageStandard |
| AD-001 | Dia App | Module system with YAML frontmatter documentation | `dia.maths.architecture.module.md` (parent), plus per-submodule docs for Vector/, Matrix/, Quaternion/, Transform/. Updated when submodules added/removed |
| AD-002 | Dia App | No STL containers in public APIs | Reinforces PD-004 |
| AD-003 | Dia App | Namespace `Dia::<Module>::` | All code in `Dia::Maths::` namespace per SD-002 |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Storage consistency | An earlier draft made Matrix44 column-major for OpenGL/glTF interop, conflicting with Matrix22/33's row-major layout. Why the change to row-major? | Cross-dimension code consistency is more valuable than zero-copy GL upload. All DiaMaths matrices now share row-major `float m[N][N]` storage with `operator()(row, col)` and public `m` access (SD-005, SD-006, SD-008). OpenGL/glTF consumers call `Matrix44::GetColumnMajor(float[16])` at upload time — one transpose per matrix per frame, negligible against render workloads. Readers and authors no longer have to remember which dimension uses which layout. |
| 2 | Quaternion correctness | Quaternion has subtle correctness traps: non-unit quaternions during composition, hemisphere flips during Slerp, basis convention (xyzw vs wxyz) breaking interop. How is this prevented? | Storage order is `x, y, z, w` (matches GLM, glTF, most engines). `Normalize()` and `IsValid()` guard against drift. `Slerp` flips one input if the dot product is negative (shortest-path interpolation). Tests include round-trips through Matrix33/44 and axis-angle to catch convention bugs. |
| 3 | Euler convention | SD-004 supports Euler overloads. Which Euler convention — XYZ, ZYX, YXZ, intrinsic vs extrinsic? Mismatched conventions are a classic source of bugs. | YXZ intrinsic (yaw around Y, then pitch around X', then roll around Z''). Standard for Y-up game engines. Documented prominently in `Quaternion::FromEuler` and `Transform3D::SetLocalRotation(yaw, pitch, roll)` Doxygen. Tests verify against known reference values. |
| 4 | Cleanup risk | The Shape Cleanup feature deletes files some consumers may still include via `<DiaMaths/Shape/2D/...>`. How is this verified safe? | Pre-deletion grep across the entire repo for `#include <DiaMaths/Shape/` and `Dia::Maths::Circle`/`Dia::Maths::AARect` etc. Any remaining call sites must be migrated to `Dia::Geometry2D::` types first. The cleanup feature spec details this verification step. Does not run until the migration grep returns zero hits. |
| 5 | Module docs | `dia.maths.shape.architecture.module.md` and `dia.maths.shape.x2d.architecture.module.md` and `dia.maths.shape.common.architecture.module.md` exist. Should they be deleted or updated to point to DiaGeometry2D? | Deleted. Architecture docs follow the code; the code is moving out. Cross-referencing two locations is the same trap as keeping the duplicate headers. |
| 6 | Quaternion submodule docs | A new `Dia/DiaMaths/Quaternion/` submodule needs `dia.maths.quaternion.architecture.module.md`. Should the parent `dia.maths.architecture.module.md` be updated to reference it? | Yes. The parent doc lists `dependent_modules` (currently `dia.maths.core`, `dia.maths.matrix`, `dia.maths.shape`, `dia.maths.vector`). Update: drop `dia.maths.shape` (deleted by cleanup feature), add `dia.maths.quaternion`, add `dia.maths.transform` (which already has files but may not have a submodule doc). |
| 7 | Transform2D vs Transform3D | Transform2D is in `DiaMaths/Transform/`. Transform3D will be in the same directory. Should the existing module doc be updated, or split? | One `dia.maths.transform.architecture.module.md` covers both — Transform2D and Transform3D are siblings of identical responsibility (one-dimensional split). Consistent with how `Vector/` holds Vector2D/3D/4D in one submodule. |
| 8 | Matrix34 use cases | What concretely uses Matrix34 vs Matrix44? Without a consumer, this risks being unused code. | Transform3D's `GetLocalAffine()` / `GetWorldAffine()` return Matrix34. Future skeleton/animation systems (DiaRig3D, DiaAnimation3D) typically store bone transforms as 3×4 affines for cache density. Acceptable to ship Matrix34 with only Transform3D as a consumer; the cost is low and it prevents the API gap when those systems land. If by review time no consumer exists, consider deferring to a feature spec. |
| 9 | Quaternion::LookRotation | `LookRotation(forward, up)` is a common cause of NaN bugs when forward and up are parallel. How is the edge case handled? | Asserts `Cross(forward, up).Magnitude() > epsilon` in debug; falls back to identity in release with a `DIA_LOG_WARNING`. Documented in the Quaternion feature spec. |
| 10 | Matrix44 perspective convention | `Matrix44::Perspective` — what depth range does it produce? `[-1, 1]` (OpenGL) or `[0, 1]` (DirectX/Vulkan)? | OpenGL `[-1, 1]` to match SFML/OpenGL — the engine's current renderer pathway (DiaGraphics is SFML-backed). If a renderer eventually targets Vulkan/DirectX, add a separate `PerspectiveZeroToOne` builder rather than overloading the default. Storage layout (now row-major per SD-005) is independent of this depth-range choice. |
| 11 | Vector3D::Cross sign convention | Right-handed coordinates have `X × Y = Z`. Confirm this is what we ship, since left-handed would invert. | Right-handed: `Vector3D::XAxis().Cross(Vector3D::YAxis()) == Vector3D::ZAxis()`. Tests assert this on construction. |
| 12 | Approval gate | Per CLAUDE.md, this system spec cannot be marked Done until all child feature specs are Approved. What's the dependency chain? | Six feature specs must reach Approved: vector3d-cross, matrix44, matrix34, quaternion, transform3d, shape-cleanup. DiaGeometry3D's `Done` status is gated on this system's `Done` status, since DiaGeometry3D consumes types defined here. |
| 13 | DiaGraphics 2D Transform duplication | `DiaGraphics/Misc/Transform.h` is a 2D transform that duplicates `DiaMaths::Transform2D`. Is its cleanup part of this spec? | No — out of scope. That cleanup is tracked at the end of `peppy-weaving-pond.md` as a trailing implementation step. This system spec is about DiaMaths' own surface; consolidating DiaGraphics' duplicate is a DiaGraphics concern. |
| 14 | Existing Transform2D | Transform2D is `Done` and untouched by this spec. But Transform3D's API mirrors it — should we audit Transform2D for any improvements before mirroring? | No major audit. The Transform2D pattern is mature and mirrored verbatim with 3D types substituted (`Vector3D` for `Vector2D`, `Quaternion` for `Angle`, `Matrix44` for `Matrix33`). Any 2D improvements identified during 3D spec work should be filed as a separate Transform2D feature, not bundled. |
| 15 | DiaMaths system spec previously TBD | The Dia application spec listed DiaMaths as TBD (`dia.md:49`). What changes by approving this spec? | Update the systems table in `dia.md` to point to `diamaths.md`. Drops the "TBD" marker. Confirms the dependency chain DiaGeometry3D→DiaMaths is now spec-backed. |

## Status

`Approved` — Steps 1–4 complete (interview, draft, binding decisions, AI review). Awaiting feature spec authorship and implementation. Plan: implementation plan will be authored as `@docs/specs/systems/dia/diamaths.plan.md` once the Vector3D::Cross, Matrix44, Matrix34, Quaternion, Transform3D, and shape-cleanup feature specs are all Approved. Hard prerequisite for DiaGeometry3D's `Done` status.
