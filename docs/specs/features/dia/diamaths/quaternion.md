# Feature Spec: Quaternion

## Traceability

| Level | Spec | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaMaths | @docs/specs/systems/dia/diamaths.md |
| Feature | Quaternion | (this document) |

## Summary

Add `Dia::Maths::Quaternion` — a unit quaternion type for 3D rotation representation. Lives in a new `Dia/DiaMaths/Quaternion/` submodule (per system SD-003), sibling to `Vector/`, `Matrix/`, `Transform/`. Storage order is `x, y, z, w` (GLM / glTF convention).

Quaternion is the **primary 3D rotation representation** for the engine (per `diageometry3d.md` SD-009 and `diamaths.md` SD-004). It backs `Transform3D`, `OOBB::orientation`, `Matrix44::FromRotation`, and any future rotation-aware system. Matrix33/44 conversions are provided so consumers preferring matrix form (SAT axis extraction, OpenGL upload) can round-trip.

## Problem

DiaMaths currently has no rotation primitive in 3D. Matrix33 has rotation factories (`FromRotation(Angle)`) but those are 2D. Without Quaternion, every 3D consumer would have to:
- Store rotation as Matrix33 (9 floats, drift-prone, hard to compose stably) or Euler angles (gimbal lock); or
- Reinvent quaternion logic ad-hoc in each module that touches 3D rotation.

A single, tested Quaternion in DiaMaths eliminates that duplication and gives the rest of the engine a stable composition primitive.

## Acceptance Criteria

1. **Storage order is `x, y, z, w`.** Direct member access via `q.x, q.y, q.z, q.w` matches GLM, glTF, Bullet, modern engines.
2. **Identity is `(0, 0, 0, 1)`.** Default constructor produces identity. `Quaternion::Identity()` static returns the same.
3. **Composition follows the Hamilton convention.** `(a * b).Rotate(v)` equals `a.Rotate(b.Rotate(v))` — applying `b` first, then `a`. Tests verify this.
4. **`Rotate(const Vector3D&)` returns a vector rotated by this quaternion.** For `q = FromAxisAngle(YAxis, 90deg)`, `q.Rotate(XAxis())` returns a vector equal to `-ZAxis()` within float epsilon (right-handed Y-up).
5. **`FromAxisAngle(axis, angle)` produces a unit quaternion.** Magnitude is 1.0 within float epsilon; `Rotate` of the axis vector returns the axis unchanged.
6. **`FromEuler(yaw, pitch, roll)` follows YXZ intrinsic order** (yaw around Y, then pitch around X', then roll around Z''). Locked by `diamaths.md` AI Review Q3.
7. **`FromMatrix33(m)` and `ToMatrix33()` round-trip.** For any unit quaternion `q`, `Quaternion::FromMatrix33(q.ToMatrix33())` equals `q` within float epsilon (or its negation — quaternions and `-quaternion` represent the same rotation).
8. **`Slerp(a, b, t)` takes the shortest-path interpolation.** If `a.Dot(b) < 0`, one input is negated to ensure `t=0.5` interpolates through the geodesic midpoint, not the antipode.
9. **`Slerp(a, b, 0)` equals `a`; `Slerp(a, b, 1)` equals `b`** (within float epsilon, after handling the antipode flip).
10. **`Nlerp(a, b, t)` is normalised lerp.** Faster than Slerp; still normalised on output. Used where speed matters more than constant-angular-velocity interpolation.
11. **`Conjugate()` and `Inverse()` for a unit quaternion are equal** within float epsilon. `Inverse()` does not assume unit length and divides by squared magnitude.
12. **`Normalize()` mutates in place; `AsNormal()` returns by value.** Mirrors Vector3D's pattern (`Normalize` / `AsNormal`).
13. **`IsValid()` returns false for NaN, infinite, or zero-magnitude quaternions.**
14. **`LookRotation(forward, up)` asserts in debug if `forward` and `up` are parallel** (cross product near zero); returns Identity in release with a `DIA_LOG_WARNING` to a Maths channel.
15. **`ToAxisAngle(outAxis, outAngle)` produces equivalent rotation to the original.** For non-identity input, `FromAxisAngle(*outAxis, *outAngle)` round-trips. For identity input, `outAxis` is set to `XAxis` and `outAngle` to zero (deterministic; documented).
16. **All public methods are `const` except `Normalize`, `operator=`, `operator*=`.**
17. **No STL containers in any public method signature.**
18. **Storage type is column-of-floats.** `float x, y, z, w` — no fancy SIMD layout this round (per system SD-011 & out-of-scope).
19. **New submodule docs exist:** `Dia/DiaMaths/Quaternion/dia.maths.quaternion.architecture.module.md` lists `Quaternion` as the public API, parent `dia.maths`, dependencies on `dia.maths.vector` and `dia.maths.matrix` (for conversions) and `dia.core`.
20. **`DiaMaths.vcxproj` and `.vcxproj.filters` are updated** to include `Quaternion.h`, `Quaternion.cpp`, `Quaternion.inl`, and the architecture doc, under a new `Quaternion` filter.
21. **Parent module doc updated:** `Dia/DiaMaths/Docs/dia.maths.architecture.module.md` `dependent_modules` list adds `dia.maths.quaternion` (per `diamaths.md` AI Review Q6).

## API Design

```cpp
// Dia/DiaMaths/Quaternion/Quaternion.h
namespace Dia::Maths {

class Vector3D;
class Matrix33;
class Matrix44;
class Angle;

// Unit quaternion for 3D rotation.
//
// STORAGE: x, y, z, w (matches GLM, glTF, Bullet).
// IDENTITY: (0, 0, 0, 1) — zero rotation.
// COMPOSITION: Hamilton convention — (a * b).Rotate(v) == a.Rotate(b.Rotate(v)).
// HANDEDNESS: Right-handed (matches DiaGeometry3D SD-006, DiaMaths SD-007).
class Quaternion
{
public:
    DIA_TYPE_DECLARATION;

    Quaternion();                                                     // identity
    Quaternion(float x, float y, float z, float w);
    Quaternion(const Quaternion& other);

    // Factories
    static Quaternion Identity();
    static Quaternion FromAxisAngle(const Vector3D& axis, const Angle& angle);
    static Quaternion FromEuler(const Angle& yaw, const Angle& pitch, const Angle& roll);  // YXZ intrinsic
    static Quaternion FromMatrix33(const Matrix33& rotation);
    static Quaternion FromMatrix44(const Matrix44& transform);
    static Quaternion LookRotation(const Vector3D& forward, const Vector3D& up);

    // Assignment
    Quaternion& operator=(const Quaternion& other);

    // Composition
    Quaternion  operator*(const Quaternion& rhs) const;     // Hamilton; *this first then rhs
    Quaternion& operator*=(const Quaternion& rhs);

    bool operator==(const Quaternion& rhs) const;
    bool operator!=(const Quaternion& rhs) const;

    // Operations
    Quaternion  Conjugate() const;                          // (-x, -y, -z, w)
    Quaternion  Inverse()   const;                          // Conjugate / SquareMagnitude
    Quaternion& Normalize();                                // mutates
    Quaternion  AsNormal()  const;                          // returns
    float       Dot(const Quaternion& rhs) const;
    float       Magnitude()       const;
    float       SquareMagnitude() const;
    bool        IsValid() const;                            // not NaN/Inf, magnitude > epsilon

    // Rotate a vector by this quaternion. Equivalent to (q * Quaternion(v.x, v.y, v.z, 0) * q.Inverse()).xyz
    // but implemented via the optimised t = 2 * cross(q.xyz, v); v + q.w * t + cross(q.xyz, t) form.
    Vector3D Rotate(const Vector3D& v) const;

    // Conversions
    Matrix33   ToMatrix33() const;
    Matrix44   ToMatrix44() const;                          // pure rotation; translation is zero
    void       ToAxisAngle(Vector3D& outAxis, Angle& outAngle) const;
    void       ToEuler(Angle& outYaw, Angle& outPitch, Angle& outRoll) const;  // YXZ intrinsic

    // Interpolation
    static Quaternion Slerp(const Quaternion& a, const Quaternion& b, float t);  // shortest-path
    static Quaternion Nlerp(const Quaternion& a, const Quaternion& b, float t);

    // Direct access — order x, y, z, w
    float x, y, z, w;
};

}  // namespace Dia::Maths
```

## Tasks

| # | Task | Description |
|---|------|-------------|
| 1 | Create `Dia/DiaMaths/Quaternion/Quaternion.h` | Class declaration per API Design above |
| 2 | Create `Dia/DiaMaths/Quaternion/Quaternion.inl` | Inline trivial methods (constructor, member access, equality, arithmetic operators) |
| 3 | Create `Dia/DiaMaths/Quaternion/Quaternion.cpp` | Implement non-trivial methods (FromAxisAngle, FromEuler, FromMatrix33/44, ToMatrix33/44, ToAxisAngle, ToEuler, Rotate, Slerp, Nlerp, LookRotation, Inverse, Normalize, IsValid) |
| 4 | Create `Dia/DiaMaths/Quaternion/dia.maths.quaternion.architecture.module.md` | YAML frontmatter; lists Quaternion as public API |
| 5 | Update `Dia/DiaMaths/Docs/dia.maths.architecture.module.md` | Add `dia.maths.quaternion` to `dependent_modules` |
| 6 | Update `Dia/DiaMaths/DiaMaths.vcxproj` and `.vcxproj.filters` | Register all new files under a new `Quaternion` filter |
| 7 | Add tests | `Cluiche/Tests/GoogleTests/Maths/TestQuaternion.cpp` covering ACs 1–18; register in GoogleTests vcxproj |
| 8 | Run `dia run googletest --filter="Quaternion*"` | All green |

## Test Plan (Task 7)

| Suite | Tests |
|-------|-------|
| `QuaternionStorageTest` | x/y/z/w order, default constructor is identity, copy / assignment |
| `QuaternionAxisAngleTest` | Round-trip FromAxisAngle ↔ ToAxisAngle for canonical axes; FromAxisAngle is unit-magnitude; FromAxisAngle then Rotate of the axis returns the axis unchanged |
| `QuaternionEulerTest` | FromEuler(0,0,0) is Identity; YXZ order verified by composing equivalent FromAxisAngle quaternions; round-trip FromEuler ↔ ToEuler within epsilon |
| `QuaternionMatrixTest` | FromMatrix33(Matrix33::Identity()) is Identity; round-trip Quaternion ↔ Matrix33; ToMatrix33 then Matrix33::TransformDirection equals Quaternion::Rotate within epsilon |
| `QuaternionRotateTest` | Right-handed: FromAxisAngle(YAxis, 90deg).Rotate(XAxis) == -ZAxis; FromAxisAngle(ZAxis, 180deg).Rotate(XAxis) == -XAxis; rotating Zero returns Zero |
| `QuaternionCompositionTest` | Hamilton convention: (a * b).Rotate(v) == a.Rotate(b.Rotate(v)); Identity * q == q; q * Identity == q; q * q.Inverse() == Identity |
| `QuaternionConjugateInverseTest` | Conjugate of unit quaternion equals Inverse; Inverse of non-unit divides by squared magnitude correctly |
| `QuaternionNormalizeTest` | Normalize mutates in place; AsNormal returns; both produce magnitude == 1 within epsilon |
| `QuaternionIsValidTest` | Identity is valid; NaN inputs invalid; zero magnitude invalid; infinite components invalid |
| `QuaternionSlerpTest` | Slerp(a,b,0) == a; Slerp(a,b,1) == b; Slerp takes shortest path (negate one input when dot < 0); midpoint matches Nlerp direction |
| `QuaternionNlerpTest` | Nlerp output is unit-magnitude; endpoint behaviour matches Slerp |
| `QuaternionLookRotationTest` | Forward/up perpendicular case produces a quaternion whose Rotate(ZAxis or -ZAxis as forward) returns the forward direction; parallel forward/up asserts in debug |

## Files

| File | Action |
|------|--------|
| `Dia/DiaMaths/Quaternion/Quaternion.h` | Create |
| `Dia/DiaMaths/Quaternion/Quaternion.inl` | Create |
| `Dia/DiaMaths/Quaternion/Quaternion.cpp` | Create |
| `Dia/DiaMaths/Quaternion/dia.maths.quaternion.architecture.module.md` | Create |
| `Dia/DiaMaths/Docs/dia.maths.architecture.module.md` | Modify — add `dia.maths.quaternion` to `dependent_modules` |
| `Dia/DiaMaths/DiaMaths.vcxproj` | Modify — register four new files |
| `Dia/DiaMaths/DiaMaths.vcxproj.filters` | Modify — add `Quaternion` filter |
| `Cluiche/Tests/GoogleTests/Maths/TestQuaternion.cpp` | Create |
| `Cluiche/Tests/GoogleTests/GoogleTests.vcxproj` | Modify — register test file |
| `Cluiche/Tests/GoogleTests/GoogleTests.vcxproj.filters` | Modify — register under Maths filter |

## Dependencies

| Dependency | What this feature uses |
|------------|----------------------|
| DiaMaths — `Vector3D` | Axis storage, vector rotation, axis-angle decomposition |
| DiaMaths — `Vector3D::Cross` | LookRotation, FromAxisAngle implementation, vector-rotation optimised path |
| DiaMaths — `Matrix33` | FromMatrix33 / ToMatrix33 conversion |
| DiaMaths — `Matrix44` | FromMatrix44 / ToMatrix44 conversion (forward declaration; cross-feature dependency) |
| DiaMaths — `Angle` | FromAxisAngle, FromEuler, ToAxisAngle, ToEuler signatures |
| DiaCore — `DIA_ASSERT`, `DIA_TYPE_DECLARATION`, `DIA_LOG_WARNING` | Preconditions, type registration, LookRotation parallel-input warning |

**Implementation order note:** This feature depends on Vector3D::Cross (already approved) and Matrix44 (separate feature spec; both submitted in same batch). The Quaternion ↔ Matrix44 conversion methods may forward-declare and be implemented after Matrix44 lands; or both features land together. Implementer chooses but must not ship Quaternion without those conversions tested.

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-001 | StringCRC for IDs | **Not applicable.** Pure maths type. |
| PD-004 | No STL containers in public API | **Compliant.** All signatures use POD or DiaMaths types. |
| PD-005 | x64 only | **Compliant.** No platform-specific code. |
| PD-006 | Visual Studio project files are source of truth | **Compliant.** vcxproj/.filters updated manually. |
| PD-007 | C++20 required | **Compliant.** No C++20-specific features used; compiles under /std:c++20. |
| PD-008 | Directory.Build.props owns build settings | **Compliant.** No vcxproj overrides. |
| AD-001 | Module system with YAML frontmatter | **Compliant.** New `dia.maths.quaternion.architecture.module.md`; parent doc updated. |
| AD-002 | No STL in public APIs | **Compliant.** Same as PD-004. |
| AD-003 | Namespace `Dia::<Module>::` | **Compliant.** All code in `Dia::Maths::`. |
| SD-001 | DiaMaths is "pure linear algebra only" | **Compliant.** Quaternion is pure linear algebra; canonical home. |
| SD-002 | Single namespace `Dia::Maths::`; submodules are organisational | **Compliant.** No `Dia::Maths::Quaternion::` sub-namespace; type lives in `Dia::Maths::`. |
| SD-003 | Quaternion lives in new `Dia/DiaMaths/Quaternion/` submodule | **Compliant — directly satisfies this decision.** |
| SD-005 | All matrices share row-major `m[N][N]` layout | **Not applicable** to Quaternion itself; matrix conversions (`ToMatrix33`, `ToMatrix44`) honour the layout of their target. |
| SD-007 | Y-up, right-handed | **Compliant.** All factories and rotations honour right-handed convention; tests assert it. |
| SD-009 | Transform3D parent pointer is raw, no cycles | **Not applicable.** Transform3D is a separate feature. |
| SD-011 | Static library, no STL in public API | **Compliant.** |
| SD-012 | New types follow existing patterns | **Compliant.** Mirrors Matrix33/Vector3D patterns: factory methods, `Identity()`, `Inverse()`, `Normalize()`/`AsNormal()`, `IsValid()`, public member access. |
| SD-013 | Test utilities under `Dia/DiaMaths/Testing/` | **Compliant by deferral.** Quaternion test fixtures (canonical rotations, epsilon-equal helpers) ship under `Dia/DiaMaths/Testing/` if a second consumer needs them; v1 tests use inline helpers. |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Storage order | Why xyzw and not wxyz? | xyzw matches GLM, glTF, Bullet, Eigen's `Quaternionf` constructor (despite Eigen storing internally as wxyz, its named constructor `Quaternionf(w,x,y,z)` is still the loud signal). Most modern engines and file formats use xyzw. Diverging would force conversions at every interop boundary. |
| 2 | Hamilton vs JPL | Both Hamilton and JPL conventions are in the wild. Which does this Quaternion use? | Hamilton convention. `(a * b).Rotate(v) == a.Rotate(b.Rotate(v))` — left-to-right composition reads "apply b, then a". Standard for graphics; JPL is mostly aerospace. Tests assert this. |
| 3 | Slerp shortest path | The naive Slerp produces the geodesic; what about the dot-flip? | If `a.Dot(b) < 0`, the implementation negates one input before interpolating. This guarantees the shortest path on the 4D hypersphere (q and -q represent the same rotation). Without it, animations occasionally take the long way around 360°. |
| 4 | LookRotation degenerate | `LookRotation(forward, up)` with parallel forward/up has no defined answer. What's the policy? | Asserts in debug; returns Identity + warning log in release. Caller must avoid the situation; library does not synthesise an arbitrary basis. |
| 5 | Identity convention | Is `(0,0,0,1)` definitely identity? | Yes — `w = cos(angle/2)` with angle=0 gives `cos(0) = 1`; the imaginary part is `axis * sin(0) = 0`. Default constructor produces this. |
| 6 | Matrix44 conversion | Matrix44 doesn't exist yet (separate feature spec). Is the Quaternion ↔ Matrix44 conversion a circular dependency? | Forward-declare `Matrix44` in Quaternion.h; implement `FromMatrix44` / `ToMatrix44` in Quaternion.cpp once Matrix44 lands. Both features are batched in the same DiaMaths plan; commit order picks one to land first (likely Quaternion before Matrix44, since Matrix44::FromRotation(Quaternion) is the consumer). |
| 7 | Float comparisons | Quaternion equality is bit-exact. Should it be epsilon-tolerant? | No. `operator==` is exact (matches Vector3D, Matrix33). Tests use a per-test epsilon helper (e.g. `ExpectQuaternionNear(a, b, 1e-5f)`). Exact equality has its uses; epsilon comparison hidden in the operator hides bugs. |
| 8 | Performance | Should Rotate(Vector3D) use the optimised cross-product form or the conjugation form (q * v * q^-1)? | Optimised form: `t = 2 * cross(q.xyz, v); result = v + q.w * t + cross(q.xyz, t)`. Faster, fewer ops, same result. Documented in source. Conjugation form is mathematically clearer but slower; not used. |
| 9 | Submodule docs | What goes in `dia.maths.quaternion.architecture.module.md`? | YAML frontmatter mirroring `dia.maths.vector` and `dia.maths.matrix`: schema=dia.module.v1, module_id=dia.maths.quaternion, parent=dia.maths, layer=platform, public_api lists Quaternion class, dependent_modules lists dia.maths.vector + dia.maths.matrix + dia.core. Parent doc adds `dia.maths.quaternion` to its dependent_modules. |
| 10 | Quaternion::Inverse vs Conjugate | Why offer both? | Conjugate is cheaper (negate xyz). For unit quaternions the two are equal — but the library cannot assume the caller's quaternion is unit (composition can drift). `Inverse` divides by squared magnitude; `Conjugate` does not. Default to `Inverse` if unsure; use `Conjugate` only when you guarantee unit length. |
| 11 | DIA_TYPE_DECLARATION | The system spec mandates this on Matrix33; should Quaternion have it? | Yes. SD-012 says "follow existing patterns" — Matrix33 has `DIA_TYPE_DECLARATION` (Matrix33.h:46). Quaternion does too. |
| 12 | Approval gate | What blocks Approval? | Nothing intrinsic. Vector3D::Cross is approved, Matrix44 is in the same batch. Quaternion can be Approved on its own merits and implemented in any order with Matrix44 (forward declarations bridge the gap). |

## Status

`Approved` — Steps 1–4 complete (interview, draft, binding decisions, AI review).
