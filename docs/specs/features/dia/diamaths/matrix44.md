# Feature Spec: Matrix44

## Traceability

| Level | Spec | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaMaths | @docs/specs/systems/dia/diamaths.md |
| Feature | Matrix44 | (this document) |

## Summary

Add `Dia::Maths::Matrix44` — a 4×4 matrix for 3D transformations (rotation, scale, translation, projection). Storage is **row-major `float m[4][4]`** to match the existing `Matrix22` and `Matrix33` (per system SD-005). Element access via `operator()(row, col)` and direct array `m[row][col]`. OpenGL/glTF interop is handled by `GetColumnMajor(float[16])` — one transpose at upload, negligible cost.

Matrix44 is the canonical home for view, projection, world, and TRS matrices in 3D. It backs `Transform3D::GetWorldMatrix`, perspective/orthographic projection for renderer consumers, `LookAt` view matrices, and Quaternion ↔ Matrix conversions.

## Problem

DiaMaths has Matrix22 and Matrix33 but no 4×4. Every 3D consumer would otherwise:
- Use Matrix33 with translation as a separate Vector3D (composability lost — can't multiply matrices to bake translations); or
- Use raw `float[16]` arrays inline (zero type safety, no operator overloading, hand-rolled inverse); or
- Pull in GLM as a dependency (large, External pollution).

A consistent Matrix44 in DiaMaths matches the Matrix33 surface and gives the rest of the engine a typed 4×4.

## Acceptance Criteria

1. **Storage is row-major `float m[4][4]`.** `m[row][col]` matches Matrix22/Matrix33 layout exactly.
2. **`operator()(int row, int col)` provides bounds-asserted access.** Both const and non-const overloads.
3. **`Identity()` returns the 4×4 identity.** Default constructor produces identity.
4. **`FromTranslation(Vector3D)` produces a translation matrix** with `m[0][3] = t.x`, `m[1][3] = t.y`, `m[2][3] = t.z`, rest identity.
5. **`FromRotation(Quaternion)` delegates to `Quaternion::ToMatrix44`** (or implements directly with the same result; tests assert equivalence).
6. **`FromScale(Vector3D)` produces a non-uniform scale matrix.** `FromScale(float)` is uniform.
7. **`FromTRS(translation, rotation, scale)` composes Translation × Rotation × Scale** in that order. Result, applied to a point, scales then rotates then translates.
8. **`Perspective(Angle fovY, float aspect, float nearZ, float farZ)` produces a Y-up right-handed perspective matrix targeting OpenGL `[-1, 1]` clip-space depth** (per `diamaths.md` AI Review Q10). `fovY` is `DiaMaths::Angle` for type safety.
9. **`Orthographic(left, right, bottom, top, nearZ, farZ)` produces an orthographic projection** in the same Y-up RH OpenGL convention.
10. **`LookAt(eye, target, up)` produces a Y-up RH view matrix** that transforms world-space points into camera-space.
11. **`operator*(Matrix44)` is matrix multiplication** following the convention `(A * B).TransformPoint(v) == A.TransformPoint(B.TransformPoint(v))`.
12. **`TransformPoint(Vector3D)` treats input as `(x,y,z,1)`** — translation applied. Returns Vector3D after homogeneous-divide if the result `w` is non-1 (e.g. after a perspective matrix).
13. **`TransformDirection(Vector3D)` treats input as `(x,y,z,0)`** — translation NOT applied. Returns Vector3D.
14. **`TransformVector4(Vector4D)` treats input as a 4D homogeneous vector;** no homogeneous-divide.
15. **`Transpose()` returns the transposed matrix without mutating `*this`.**
16. **`Determinant()` returns the 4×4 determinant.**
17. **`Inverse()` returns the inverse for invertible inputs.** For singular inputs (`Determinant() < epsilon`), returns Identity and asserts in debug.
18. **`GetColumnMajor(float outData[16])` fills a 16-float buffer in column-major order** — `outData[col * 4 + row] = m[row][col]`. For uploading to OpenGL `glUniformMatrix4fv` with `transpose = GL_FALSE` and to glTF JSON arrays.
19. **`GetTranslation()` returns the right column.** `GetRotation()` returns the rotation part as a Quaternion (after extracting scale). `GetScale()` returns the scale magnitudes of the upper-left 3×3 columns. **Behaviour for non-affine input (perspective, projection) is undefined** and documented.
20. **All public methods are `const` except `SetIdentity`, `operator=`, `operator+=`, `operator-=`, `operator*=`.**
21. **`DIA_TYPE_DECLARATION` is present** (matches Matrix33.h:46).
22. **`DiaMaths.vcxproj` and `.vcxproj.filters`** register Matrix44.h, Matrix44.inl, Matrix44.cpp under the existing Matrix filter.
23. **`dia.maths.matrix.architecture.module.md`** updated to list `Matrix44` in `public_api.entry_points`.

## API Design

```cpp
// Dia/DiaMaths/Matrix/Matrix44.h
namespace Dia::Maths {

class Vector3D;
class Vector4D;
class Quaternion;
class Angle;

// 4x4 matrix for 3D transformations.
//
// STORAGE: Row-major float m[4][4] — matches Matrix22 and Matrix33.
//   m[row][col] gives element at (row, col).
//   To upload to OpenGL/glTF use GetColumnMajor(float[16]) which transposes once.
//
// HANDEDNESS: Right-handed (per DiaMaths SD-007).
// PROJECTION DEPTH: OpenGL [-1, 1] (per diamaths.md AI Review Q10).
class Matrix44
{
public:
    DIA_TYPE_DECLARATION;

    // Construction
    Matrix44();                                                           // identity
    Matrix44(const Matrix44& other);
    Matrix44(float m00, float m01, float m02, float m03,
             float m10, float m11, float m12, float m13,
             float m20, float m21, float m22, float m23,
             float m30, float m31, float m32, float m33);

    // Factories
    static Matrix44 Identity();
    static Matrix44 FromTranslation(const Vector3D& translation);
    static Matrix44 FromRotation(const Quaternion& rotation);
    static Matrix44 FromScale(const Vector3D& scale);
    static Matrix44 FromScale(float uniformScale);
    static Matrix44 FromTRS(const Vector3D& translation, const Quaternion& rotation, const Vector3D& scale);

    // Y-up right-handed projection / view builders. fovY uses Angle for type safety.
    static Matrix44 Perspective(const Angle& fovY, float aspect, float nearZ, float farZ);
    static Matrix44 Orthographic(float left, float right, float bottom, float top, float nearZ, float farZ);
    static Matrix44 LookAt(const Vector3D& eye, const Vector3D& target, const Vector3D& up);

    // Assignment
    Matrix44& operator=(const Matrix44& other);

    // Arithmetic
    Matrix44& operator+=(const Matrix44& other);
    Matrix44& operator-=(const Matrix44& other);
    Matrix44& operator*=(const Matrix44& other);
    Matrix44& operator*=(float scalar);

    Matrix44 operator-() const;
    Matrix44 operator+(const Matrix44& other) const;
    Matrix44 operator-(const Matrix44& other) const;
    Matrix44 operator*(const Matrix44& other) const;
    Matrix44 operator*(float scalar) const;

    bool operator==(const Matrix44& other) const;
    bool operator!=(const Matrix44& other) const;

    // Element access — m(row, col); bounds-asserted in debug.
    float& operator()(int row, int col);
    float  operator()(int row, int col) const;

    // Transform
    Vector3D TransformPoint(const Vector3D& point) const;       // homogeneous (w=1) with divide
    Vector3D TransformDirection(const Vector3D& dir) const;     // homogeneous (w=0)
    Vector4D TransformVector4(const Vector4D& v) const;          // pure 4D, no divide

    // Properties
    void     SetIdentity();
    Matrix44 Transpose() const;
    float    Determinant() const;
    Matrix44 Inverse() const;

    // OpenGL / glTF interop — fills 16-float column-major buffer.
    void GetColumnMajor(float outData[16]) const;

    // Component extraction (assumes affine input — undefined for projection matrices)
    Vector3D    GetTranslation() const;
    Quaternion  GetRotation() const;
    Vector3D    GetScale() const;

    // Direct access — row-major m[row][col].
    float m[4][4];
};

}  // namespace Dia::Maths
```

## Tasks

| # | Task | Description |
|---|------|-------------|
| 1 | Create `Dia/DiaMaths/Matrix/Matrix44.h` | Class declaration |
| 2 | Create `Dia/DiaMaths/Matrix/Matrix44.inl` | Inline trivial methods (constructors, element access, equality, basic arithmetic) |
| 3 | Create `Dia/DiaMaths/Matrix/Matrix44.cpp` | Non-trivial methods (factories, projection builders, Inverse, Determinant, Transform*, GetColumnMajor, component extraction) |
| 4 | Update `Dia/DiaMaths/Matrix/dia.maths.matrix.architecture.module.md` | Add `Matrix44` to public_api.entry_points |
| 5 | Update `Dia/DiaMaths/DiaMaths.vcxproj` and `.vcxproj.filters` | Register all three new files under Matrix filter |
| 6 | Add tests | `Cluiche/Tests/GoogleTests/Maths/TestMatrix44.cpp` |
| 7 | Run `dia run googletest --filter="Matrix44*"` | All green |

## Test Plan (Task 6)

| Suite | Tests |
|-------|-------|
| `Matrix44StorageTest` | m[row][col] access matches operator()(row,col); identity diagonal == 1, off-diagonal == 0; copy / assignment |
| `Matrix44FromTranslationTest` | m(0,3)=tx, m(1,3)=ty, m(2,3)=tz; TransformPoint applies translation; TransformDirection ignores it |
| `Matrix44FromRotationTest` | FromRotation(Quaternion::Identity()) is Identity; FromRotation(q).TransformDirection equals q.Rotate within epsilon |
| `Matrix44FromScaleTest` | Diagonal entries; uniform overload; TransformPoint scales correctly |
| `Matrix44FromTRSTest` | Composition order: scale then rotate then translate; round-trip GetTranslation / GetRotation / GetScale on an affine TRS |
| `Matrix44PerspectiveTest` | Symmetric frustum; fovY in degrees and radians via Angle; near/far depth maps to [-1, 1]; aspect ratio honoured |
| `Matrix44OrthographicTest` | Cube → NDC mapping; depth range [-1, 1] |
| `Matrix44LookAtTest` | Eye at origin looking down -Z with up=+Y produces identity rotation in view; world point at (0,0,-1) maps to (0,0,-distance) in view |
| `Matrix44MultiplyTest` | (A*B).TransformPoint == A.TransformPoint(B.TransformPoint); identity * m == m; matrix * identity == m |
| `Matrix44TransformTest` | TransformPoint with translation; TransformDirection ignores translation; TransformVector4 no-divide; perspective matrix divides correctly |
| `Matrix44InverseTest` | A * A.Inverse() == Identity within epsilon; singular matrix returns Identity and asserts in debug |
| `Matrix44DeterminantTest` | Identity == 1; scale matrix == sx*sy*sz; singular matrix == 0 |
| `Matrix44TransposeTest` | Transpose then Transpose == original; Transpose of Identity is Identity |
| `Matrix44GetColumnMajorTest` | outData[col*4 + row] == m[row][col]; identity produces canonical column-major identity buffer |

## Files

| File | Action |
|------|--------|
| `Dia/DiaMaths/Matrix/Matrix44.h` | Create |
| `Dia/DiaMaths/Matrix/Matrix44.inl` | Create |
| `Dia/DiaMaths/Matrix/Matrix44.cpp` | Create |
| `Dia/DiaMaths/Matrix/dia.maths.matrix.architecture.module.md` | Modify — add Matrix44 |
| `Dia/DiaMaths/DiaMaths.vcxproj` | Modify |
| `Dia/DiaMaths/DiaMaths.vcxproj.filters` | Modify |
| `Cluiche/Tests/GoogleTests/Maths/TestMatrix44.cpp` | Create |
| `Cluiche/Tests/GoogleTests/GoogleTests.vcxproj` | Modify |
| `Cluiche/Tests/GoogleTests/GoogleTests.vcxproj.filters` | Modify |

## Dependencies

| Dependency | What this feature uses |
|------------|----------------------|
| DiaMaths — `Vector3D` | TransformPoint/Direction inputs/outputs; FromTranslation/Scale args; component extraction |
| DiaMaths — `Vector3D::Cross` | LookAt basis construction (forward × up = right) |
| DiaMaths — `Vector4D` | TransformVector4 input/output |
| DiaMaths — `Quaternion` | FromRotation, GetRotation; cross-feature dependency in same batch |
| DiaMaths — `Angle` | Perspective fovY type-safety |
| DiaCore — `DIA_ASSERT`, `DIA_TYPE_DECLARATION` | Preconditions and type registration |

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-001 | StringCRC for IDs | **Not applicable.** |
| PD-004 | No STL containers in public API | **Compliant.** All public methods use POD or DiaMaths types. |
| PD-005 | x64 only | **Compliant.** |
| PD-006 | Visual Studio project files are source of truth | **Compliant.** vcxproj/.filters updated manually. |
| PD-007 | C++20 required | **Compliant.** No C++20-specific features required. |
| PD-008 | Directory.Build.props owns build settings | **Compliant.** No vcxproj overrides. |
| AD-001 | Module system with YAML frontmatter | **Compliant.** Matrix submodule doc updated. |
| AD-002 | No STL in public APIs | **Compliant.** Same as PD-004. |
| AD-003 | Namespace `Dia::<Module>::` | **Compliant.** All code in `Dia::Maths::`. |
| SD-001 | DiaMaths is "pure linear algebra only" | **Compliant.** Matrix44 is pure linear algebra. |
| SD-002 | Single namespace `Dia::Maths::` | **Compliant.** No sub-namespace. |
| SD-005 | Row-major `float m[N][N]` for all matrices | **Compliant — directly satisfies this decision.** Matrix44 uses `float m[4][4]` row-major; OpenGL upload via `GetColumnMajor`. |
| SD-006 | `operator()(row, col)` and public `m[N][N]` | **Compliant — directly satisfies this decision.** |
| SD-007 | Y-up, right-handed convention | **Compliant.** Perspective, LookAt, Orthographic all Y-up RH. |
| SD-011 | Static library, no STL in public API | **Compliant.** |
| SD-012 | New types follow existing patterns | **Compliant.** Mirrors Matrix33: factory methods, identity, transpose/inverse/determinant, public m, operator() access, DIA_TYPE_DECLARATION. |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Storage layout | Why row-major when most graphics APIs prefer column-major? | Cross-dimension consistency with Matrix22/Matrix33 (system SD-005). The cost is one transpose per matrix per upload — negligible in render workloads, far cheaper than authors and readers having to track which dimension uses which layout. `GetColumnMajor` provides the GL-friendly form on demand. |
| 2 | Constructor argument order | The 16-float constructor takes m00..m33 in row-by-row source order. Storage matches. Is there ambiguity? | No — both source order and storage are row-major, so the constructor reads naturally: row 0 first, then row 1, etc. Tests assert this. |
| 3 | Perspective convention | Why OpenGL [-1, 1] clip-depth and not Vulkan/DirectX [0, 1]? | Default matches the engine's current SFML/OpenGL render path. If a renderer eventually targets Vulkan or DirectX, add `PerspectiveZeroToOne` rather than overloading the default. Documented in `diamaths.md` AI Review Q10. |
| 4 | LookAt up-vector | What if `up` is parallel to `(target - eye)`? | Asserts in debug; release returns identity with a `DIA_LOG_WARNING`. Same policy as `Quaternion::LookRotation`. Caller must avoid the situation. |
| 5 | Inverse for projection matrices | `Inverse()` for a perspective matrix is mathematically defined but `GetTranslation/GetRotation/GetScale` after it is meaningless. Is that flagged? | Comments in API design. The component-extraction methods document "assumes affine input — undefined for projection matrices". Tests do not call them on projection inputs. |
| 6 | Multiplication order | Why `(A * B).TransformPoint(v) == A.TransformPoint(B.TransformPoint(v))` and not the reverse? | Matches Matrix33 (Matrix33.h does the same — multiplication is row-vector-on-right with column-major convention or column-vector-on-right with row-major; we use the latter). Composing TRS as `Translation * Rotation * Scale` reads scale-then-rotate-then-translate when applied to a point. |
| 7 | TransformPoint with non-1 w | Does TransformPoint always divide? | Yes — TransformPoint always treats input as (x,y,z,1) and divides the result by `w` if `w != 1`. This is the right behaviour for perspective matrices. For pure affine matrices `w` is always 1 and the divide is a no-op. |
| 8 | Float epsilon for Inverse | What's "singular"? | `fabs(Determinant()) < 1e-6f`. Documented. Callers needing tighter or looser thresholds compute Determinant() themselves and decide. |
| 9 | GetColumnMajor signature | Why `void GetColumnMajor(float[16])` instead of returning a struct or a pointer? | The caller usually has a buffer ready (e.g. a `float[16]` on the stack right before `glUniformMatrix4fv`). Filling caller-owned memory avoids any allocation question and matches the SD-011 no-STL stance. |
| 10 | Symbol bloat | Matrix44.cpp will be large (Inverse alone is ~80 lines). Should it be split? | No. Matrix33.cpp is similar size with similar structure. Keep parity. |
| 11 | Approval gate | Quaternion is a cross-feature dependency. What's the order? | Both Quaternion and Matrix44 are batched. Implementer order: Quaternion first (Matrix44::FromRotation depends on Quaternion::ToMatrix44 logic), then Matrix44. Each commits independently with tests green. |

## Status

`Approved` — Steps 1–4 complete (interview, draft, binding decisions, AI review).
