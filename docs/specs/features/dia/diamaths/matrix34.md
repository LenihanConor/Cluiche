# Feature Spec: Matrix34

## Traceability

| Level | Spec | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaMaths | @docs/specs/systems/dia/diamaths.md |
| Feature | Matrix34 | (this document) |

## Summary

Add `Dia::Maths::Matrix34` — a 3×4 affine matrix (rotation + translation, no projection row). Storage is row-major `float m[3][4]` per system SD-008. Cheaper than Matrix44 for transform hierarchies (12 floats vs 16) with no functional loss for affine work.

Matrix34 is the cache-friendly form for skeletal animation, scene-graph world matrices, and any system that batches affine transforms. The 4th row of a Matrix44 (always `(0,0,0,1)` for affine inputs) is implicit. Conversion to Matrix44 appends that row; conversion from Matrix44 drops it (asserting it is the identity row in debug).

## Problem

Game-engine consumers that batch many world matrices (skeletal poses, instanced render data) pay a 25% memory tax with Matrix44 — every fourth row is `(0,0,0,1)` for affine inputs. Matrix34 is the standard remedy. Without it, callers either accept the waste or invent ad-hoc 12-float layouts.

## Acceptance Criteria

1. **Storage is row-major `float m[3][4]`** — matches Matrix22/Matrix33/Matrix44 layout convention.
2. **`operator()(int row, int col)` provides bounds-asserted access** with `0 ≤ row < 3` and `0 ≤ col < 4`.
3. **`Identity()` produces the affine identity** — diagonal of upper-left 3×3 is 1, rest of the matrix is 0.
4. **`FromTranslation(Vector3D)` produces a translation matrix** with `m[0][3] = t.x`, `m[1][3] = t.y`, `m[2][3] = t.z`, upper-left 3×3 is identity.
5. **`FromRotation(Quaternion)` produces a rotation-only matrix** with translation column `(0, 0, 0)`.
6. **`FromScale(Vector3D)` and `FromScale(float)` produce scale-only matrices.**
7. **`FromTRS(translation, rotation, scale)` composes Translation × Rotation × Scale.**
8. **`ToMatrix44()` produces the equivalent 4×4** by appending row `(0, 0, 0, 1)`.
9. **`FromMatrix44(const Matrix44&)` extracts a Matrix34** by copying rows 0–2 and asserting in debug that row 3 equals `(0, 0, 0, 1)` within float epsilon. In release, row 3 is silently ignored.
10. **`operator*(Matrix34)` is matrix multiplication that respects the implicit (0,0,0,1) bottom row.** Result is also a Matrix34.
11. **`TransformPoint(Vector3D)` treats input as `(x,y,z,1)`** with translation applied; returns Vector3D.
12. **`TransformDirection(Vector3D)` treats input as `(x,y,z,0)`** without translation; returns Vector3D.
13. **`Inverse()` returns the inverse of an invertible affine matrix.** Uses the affine-specific formula (separate inverse of upper-left 3×3, then negated translation). Faster than full 4×4 inverse. For singular inputs returns Identity and asserts in debug.
14. **`GetTranslation()` returns the right column.** `GetRotation()` returns the rotation part as Quaternion (after extracting scale). `GetScale()` returns scale magnitudes.
15. **`DIA_TYPE_DECLARATION` is present.**
16. **`DiaMaths.vcxproj` and `.vcxproj.filters`** register Matrix34.h, Matrix34.inl, Matrix34.cpp under the existing Matrix filter.
17. **`dia.maths.matrix.architecture.module.md`** updated to list `Matrix34` in `public_api.entry_points`.

## API Design

```cpp
// Dia/DiaMaths/Matrix/Matrix34.h
namespace Dia::Maths {

class Vector3D;
class Quaternion;
class Matrix44;

// 3x4 affine matrix — rotation + translation, no projection row.
//
// STORAGE: Row-major float m[3][4] — matches Matrix22/33/44.
//   The implicit 4th row is (0, 0, 0, 1).
//
// USE: Cheaper than Matrix44 for transform hierarchies. Skeletal poses,
// scene-graph world matrices, and anywhere "no projection ever" is a
// load-bearing invariant.
class Matrix34
{
public:
    DIA_TYPE_DECLARATION;

    // Construction
    Matrix34();                                                          // identity
    Matrix34(const Matrix34& other);
    Matrix34(float m00, float m01, float m02, float m03,
             float m10, float m11, float m12, float m13,
             float m20, float m21, float m22, float m23);

    // Factories
    static Matrix34 Identity();
    static Matrix34 FromTranslation(const Vector3D& translation);
    static Matrix34 FromRotation(const Quaternion& rotation);
    static Matrix34 FromScale(const Vector3D& scale);
    static Matrix34 FromScale(float uniformScale);
    static Matrix34 FromTRS(const Vector3D& translation, const Quaternion& rotation, const Vector3D& scale);
    static Matrix34 FromMatrix44(const Matrix44& transform);             // asserts row 3 == (0,0,0,1)

    // Promotion
    Matrix44 ToMatrix44() const;                                          // appends (0,0,0,1) row

    // Assignment
    Matrix34& operator=(const Matrix34& other);

    // Arithmetic
    Matrix34& operator+=(const Matrix34& other);
    Matrix34& operator-=(const Matrix34& other);
    Matrix34& operator*=(const Matrix34& other);
    Matrix34& operator*=(float scalar);

    Matrix34 operator-() const;
    Matrix34 operator+(const Matrix34& other) const;
    Matrix34 operator-(const Matrix34& other) const;
    Matrix34 operator*(const Matrix34& other) const;                     // implicit (0,0,0,1) bottom row
    Matrix34 operator*(float scalar) const;

    bool operator==(const Matrix34& other) const;
    bool operator!=(const Matrix34& other) const;

    // Element access — m(row, col)
    float& operator()(int row, int col);
    float  operator()(int row, int col) const;

    // Transform
    Vector3D TransformPoint(const Vector3D& point) const;
    Vector3D TransformDirection(const Vector3D& dir) const;

    // Properties
    void     SetIdentity();
    Matrix34 Inverse() const;                                             // affine-specific (faster than 4x4)
    float    Determinant() const;                                         // determinant of upper-left 3x3

    // Component extraction
    Vector3D    GetTranslation() const;
    Quaternion  GetRotation() const;
    Vector3D    GetScale() const;

    // Direct access — row-major m[row][col]
    float m[3][4];
};

}  // namespace Dia::Maths
```

## Tasks

| # | Task | Description |
|---|------|-------------|
| 1 | Create `Dia/DiaMaths/Matrix/Matrix34.h` | Class declaration |
| 2 | Create `Dia/DiaMaths/Matrix/Matrix34.inl` | Inline trivial methods |
| 3 | Create `Dia/DiaMaths/Matrix/Matrix34.cpp` | Non-trivial methods (factories, FromMatrix44 with assert, Inverse via affine formula, multiplication respecting implicit row, component extraction) |
| 4 | Update `Dia/DiaMaths/Matrix/dia.maths.matrix.architecture.module.md` | Add `Matrix34` to public_api.entry_points |
| 5 | Update `Dia/DiaMaths/DiaMaths.vcxproj` and `.vcxproj.filters` | Register all three new files |
| 6 | Add tests | `Cluiche/Tests/GoogleTests/Maths/TestMatrix34.cpp` |
| 7 | Run `dia run googletest --filter="Matrix34*"` | All green |

## Test Plan (Task 6)

| Suite | Tests |
|-------|-------|
| `Matrix34StorageTest` | m[row][col] access; identity layout; copy / assignment |
| `Matrix34FactoryTest` | FromTranslation / FromRotation / FromScale / FromTRS produce expected layouts |
| `Matrix34Matrix44ConvTest` | ToMatrix44 appends (0,0,0,1); FromMatrix44 of an affine Matrix44 round-trips; FromMatrix44 of a non-affine input asserts in debug |
| `Matrix34MultiplyTest` | (A*B).TransformPoint == A.TransformPoint(B.TransformPoint); identity is left/right identity; multiplication respects implicit (0,0,0,1) bottom row |
| `Matrix34TransformTest` | TransformPoint applies translation; TransformDirection ignores translation |
| `Matrix34InverseTest` | A * A.Inverse() == Identity; FromTranslation(t).Inverse() == FromTranslation(-t); singular returns Identity + assert |
| `Matrix34ExtractionTest` | GetTranslation == right column; GetRotation/GetScale round-trip on TRS input |

## Files

| File | Action |
|------|--------|
| `Dia/DiaMaths/Matrix/Matrix34.h` | Create |
| `Dia/DiaMaths/Matrix/Matrix34.inl` | Create |
| `Dia/DiaMaths/Matrix/Matrix34.cpp` | Create |
| `Dia/DiaMaths/Matrix/dia.maths.matrix.architecture.module.md` | Modify — add Matrix34 |
| `Dia/DiaMaths/DiaMaths.vcxproj` | Modify |
| `Dia/DiaMaths/DiaMaths.vcxproj.filters` | Modify |
| `Cluiche/Tests/GoogleTests/Maths/TestMatrix34.cpp` | Create |
| `Cluiche/Tests/GoogleTests/GoogleTests.vcxproj` | Modify |
| `Cluiche/Tests/GoogleTests/GoogleTests.vcxproj.filters` | Modify |

## Dependencies

| Dependency | What this feature uses |
|------------|----------------------|
| DiaMaths — `Vector3D` | Translation, scale, transform args |
| DiaMaths — `Quaternion` | FromRotation, GetRotation |
| DiaMaths — `Matrix44` | FromMatrix44, ToMatrix44 conversion |
| DiaCore — `DIA_ASSERT`, `DIA_TYPE_DECLARATION` | Preconditions, type registration |

**Order:** Implement after Matrix44 lands. Matrix34 requires `FromMatrix44` and `ToMatrix44` which depend on Matrix44 existing.

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-001 | StringCRC for IDs | **Not applicable.** |
| PD-004 | No STL in public API | **Compliant.** |
| PD-005 | x64 only | **Compliant.** |
| PD-006 | VS project files source of truth | **Compliant.** |
| PD-007 | C++20 required | **Compliant.** |
| PD-008 | Directory.Build.props owns build settings | **Compliant.** |
| AD-001 | Module YAML | **Compliant.** Matrix submodule doc updated. |
| AD-002 | No STL public APIs | **Compliant.** |
| AD-003 | Namespace | **Compliant.** `Dia::Maths::`. |
| SD-001 | DiaMaths is "pure linear algebra only" | **Compliant.** |
| SD-002 | Single namespace | **Compliant.** |
| SD-005 | Row-major `float m[N][N]` | **Compliant — directly satisfies.** `m[3][4]` row-major. |
| SD-006 | `operator()(row, col)` and public `m` | **Compliant.** |
| SD-007 | Y-up RH | **Compliant.** |
| SD-008 | Matrix34 row-major affine, no projection row | **Compliant — directly satisfies.** |
| SD-011 | Static lib, no STL | **Compliant.** |
| SD-012 | New types follow existing patterns | **Compliant.** Mirrors Matrix44/Matrix33: factories, Identity, Inverse, public m, operator() access, DIA_TYPE_DECLARATION. |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Use case | The system spec AI Review Q8 raised whether Matrix34 has a real consumer. Does it? | Yes — Transform3D's `GetLocalAffine()` / `GetWorldAffine()` (separate feature spec). Future skeletal animation or instanced render systems will benefit further. Acceptable to ship with one consumer. |
| 2 | Inverse cost | Why a custom affine Inverse instead of going through Matrix44? | The affine formula (invert upper-left 3×3, then negate translation rotated by the inverse) is roughly 2× faster than the general 4×4 inverse and avoids edge cases the general routine handles for projections. For transform hierarchies that invert frequently (parenting, world-to-local), the win is worth the implementation. |
| 3 | FromMatrix44 strictness | Should FromMatrix44 silently accept any 4×4 input, or assert on non-affine row 3? | Assert in debug; silently ignore in release. Strict enough to catch programmer error during dev; permissive in shipping builds where a near-(0,0,0,1) row from numerical drift shouldn't crash. Threshold: `fabs(row3 - (0,0,0,1)) < 1e-5f`. |
| 4 | Multiplication semantics | `Matrix34 * Matrix34` — does the implicit (0,0,0,1) row matter? | Yes. The math is the same as Matrix44 multiplication where `A.row3 = B.row3 = (0,0,0,1)` — the result's bottom row is also (0,0,0,1) and gets implicitly dropped. The implementation skips that row explicitly. |
| 5 | Storage byte size | sizeof(Matrix34) — 48 bytes vs Matrix44's 64 bytes? | Yes. Saves 16 bytes per matrix. For 1000 bones × 60fps × per-frame upload, that's ~1MB saved per second of bandwidth. |
| 6 | Operator+/-(Matrix34) | What does adding two affine matrices mean? | Mathematically, adding two affine matrices does NOT produce an affine matrix in general (the bottom rows would sum to (0,0,0,2)). But Matrix34 has no bottom row, so element-wise addition just works. The result, treated as an affine, still has implicit (0,0,0,1) — but it is no longer the affine sum of the two transforms; it is element-wise addition of the upper portion. Provide the operator for parity with Matrix33/44 but document that affine semantics aren't preserved. |
| 7 | Transform2D parallel | Should there be a Matrix24 (3-row × 3-col affine 2D)? | No — Matrix33 already does that role; in 2D the implicit row is the third row, so the saving (8 bytes) is not large enough to justify a new type. |
| 8 | Approval gate | Anything blocking? | Implements after Matrix44 due to FromMatrix44/ToMatrix44 conversions. Same batch. |

## Status

`Approved` — Steps 1–4 complete (interview, draft, binding decisions, AI review).
