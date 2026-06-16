# Feature Spec: Vector3D::Cross

## Traceability

| Level | Spec | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia/dia.md |
| System | DiaMaths | @docs/specs/applications/dia/systems/diamaths/diamaths.md |
| Feature | Vector3D::Cross | (this document) |

## Summary

Add the cross product method to `Dia::Maths::Vector3D`. The class currently has `Dot`, `ProjectOn`, distance helpers, and arithmetic operators, but no `Cross` — a foundational 3D-only operation required by every downstream consumer of 3D maths (Plane normals, OOBB axes, Frustum derivation, Triangle normals, view-matrix construction, Quaternion-to-matrix conversion).

This is the smallest feature in the DiaMaths system spec but a hard prerequisite for everything else: Matrix44, Quaternion, Transform3D, AABB intersection helpers, and DiaGeometry3D shape tests all consume `Cross`.

## Problem

`Dia::Maths::Vector3D` (`Dia/DiaMaths/Vector/Vector3D.h:7-76`) ships with `Dot` (line 66) but no `Cross`. The omission has been inconsequential in 2D-only code, but every 3D feature in the queued work — Quaternion construction from axis-angle, Matrix44::LookAt, OOBB orientation axes, Plane construction from triangle vertices, Frustum-from-camera derivation — requires `Cross`. Adding it here in DiaMaths is the single-source-of-truth point: every 3D consumer downstream uses the same definition.

## Acceptance Criteria

1. `Vector3D Vector3D::Cross(const Vector3D& rhs) const` is declared in `Dia/DiaMaths/Vector/Vector3D.h` alongside `Dot` and the other vector-vector methods.
2. Implementation is in `Dia/DiaMaths/Vector/Vector3D.cpp`, using the standard right-handed formula:
   - `result.x = y * rhs.z - z * rhs.y`
   - `result.y = z * rhs.x - x * rhs.z`
   - `result.z = x * rhs.y - y * rhs.x`
3. **Right-handed convention is asserted by test:** `Vector3D::XAxis().Cross(Vector3D::YAxis())` returns a vector equal to `Vector3D::ZAxis()` within float epsilon (1e-5f). Same for the cyclic permutations: `Y×Z = X` and `Z×X = Y`.
4. **Anti-commutativity is asserted by test:** for non-parallel `a` and `b`, `a.Cross(b)` equals `-b.Cross(a)` within float epsilon.
5. **Self-cross returns zero:** `v.Cross(v)` returns `Vector3D::Zero()` for any non-NaN input vector.
6. **Cross with parallel vector returns zero:** `v.Cross(v * 2.0f)` returns `Vector3D::Zero()` (within float epsilon) for any non-NaN, non-zero `v`.
7. **Orthogonality:** for any non-parallel `a`, `b`, `a.Cross(b).Dot(a)` and `a.Cross(b).Dot(b)` are both within float epsilon of zero.
8. The method is `const` and does not modify `*this`.
9. No new files are created — only `Vector3D.h` and `Vector3D.cpp` are edited. `DiaMaths.vcxproj` and `.vcxproj.filters` are unchanged.
10. Existing Vector3D tests continue to pass without modification.

## API Design

```cpp
// In Dia/DiaMaths/Vector/Vector3D.h, inside the existing class declaration,
// alongside Dot() at line 66:

class Vector3D
{
public:
    // ... existing API ...

    float    Dot(const Vector3D& rhs) const;

    // Right-handed cross product. result is perpendicular to both *this and rhs;
    // its direction follows the right-hand rule (XAxis.Cross(YAxis) == ZAxis).
    // Returns Zero for parallel inputs.
    Vector3D Cross(const Vector3D& rhs) const;

    // ... rest of existing API ...
};
```

## Tasks

| # | Task | Description |
|---|------|-------------|
| 1 | Edit `Vector3D.h` | Add `Cross` declaration alongside existing `Dot` |
| 2 | Edit `Vector3D.cpp` | Implement `Cross` using the standard right-handed formula |
| 3 | Add Cross tests to GoogleTests | New test cases in the Vector3D test file (or new file if none exists). Cover ACs 3–7. |
| 4 | Run `dia run googletest --filter="Vector3D*"` | All green; new tests pass; existing tests still pass |

## Test Plan (Task 3)

| Suite | Tests |
|-------|-------|
| `Vector3DCrossTest` | Right-handed convention (X×Y=Z, Y×Z=X, Z×X=Y); anti-commutativity; self-cross is zero; parallel-cross is zero; orthogonality (result perpendicular to both inputs); known reference vector (e.g. (1,2,3).Cross(4,5,6) == (-3,6,-3)); does not modify *this (const correctness verified by const-ref test) |

## Files

| File | Action |
|------|--------|
| `Dia/DiaMaths/Vector/Vector3D.h` | Modify — add `Cross` declaration |
| `Dia/DiaMaths/Vector/Vector3D.cpp` | Modify — add `Cross` implementation |
| `Cluiche/Tests/GoogleTests/Maths/TestVector3D.cpp` | Create or modify — add Vector3DCrossTest suite. Locate existing Vector3D test file first; create only if none exists. |
| `Cluiche/Tests/GoogleTests/GoogleTests.vcxproj` | Modify only if a new test file is created |
| `Cluiche/Tests/GoogleTests/GoogleTests.vcxproj.filters` | Modify only if a new test file is created |

## Dependencies

| Dependency | What this feature uses |
|------------|----------------------|
| DiaMaths — `Vector3D` | The type being extended |

No new module dependencies.

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-001 | StringCRC for all entity/component IDs | **Not applicable.** Pure maths method, no IDs. |
| PD-004 | No STL containers in public APIs | **Compliant.** Method takes `const Vector3D&` and returns `Vector3D`. No STL. |
| PD-005 | x64 only | **Compliant.** Pure C++; no platform-specific code. |
| PD-006 | Visual Studio project files are source of truth | **Compliant.** No vcxproj changes required (existing files edited). |
| PD-007 | C++20 required | **Compliant.** No language features beyond what is already mandated. |
| PD-008 | Directory.Build.props owns build settings | **Compliant.** No `.vcxproj` edits required for this feature. |
| AD-001 | Module system with YAML frontmatter | **Compliant.** `dia.maths.vector.architecture.module.md` already lists `Vector3D` as public API; adding a method does not require changing the public-API surface listed there (`Vector3D` is the entry point, methods are implementation detail). |
| AD-002 | No STL in public APIs | **Compliant.** Same as PD-004. |
| AD-003 | Namespace `Dia::<Module>::` | **Compliant.** Method lives inside `Dia::Maths::Vector3D`. |
| SD-001 | DiaMaths is "pure linear algebra only" | **Compliant.** Cross product is core linear algebra; ideal fit. |
| SD-002 | All types live under `Dia::Maths::` | **Compliant.** Method on existing class in `Dia::Maths::`. |
| SD-011 | Static library, no STL in public API | **Compliant.** Same as PD-004. |
| SD-012 | New types follow existing patterns | **Compliant.** Method-style API mirrors existing `Dot`, `DistanceTo`, `ProjectOn` on the same class. |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | API style | Why a method (`v.Cross(rhs)`) rather than a free function (`Cross(a, b)`) or a binary operator (e.g. `operator^`)? | The Vector3D class already exposes `Dot`, `ProjectOn`, `DistanceTo`, `SquareDistanceTo`, etc. as methods. Adding `Cross` as a method preserves API consistency. A free function would introduce a second style for no benefit. `operator^` is the GLSL convention but unfamiliar in C++ codebases — the symbol does not visually suggest "cross product" to readers. |
| 2 | Sign convention | The cross-product formula has two "valid" definitions depending on handedness. Why hard-assert right-handed (X×Y=Z) in tests? | Right-handed is the convention chosen by `diageometry3d.md` SD-006 and `diamaths.md` SD-007 for all 3D work. Silently flipping the sign causes downstream bugs that are hard to track (a Frustum's near plane points the wrong way; an OOBB has inverted axes). A test that fails immediately if anyone writes the formula with the wrong sign is the cheapest possible safeguard. |
| 3 | Numerical stability | Should the implementation handle near-parallel inputs specially (e.g. via Kahan summation, or returning a precomputed zero)? | No. The standard formula is numerically stable for typical game/render inputs. Producing a near-zero vector for near-parallel inputs is the correct behaviour — callers that need to detect parallelism check the magnitude of the result. Adding special-case handling would be a premature optimisation with no documented consumer pain. |
| 4 | Existing tests | Is there already a `TestVector3D.cpp` in GoogleTests? | Verify during implementation. If yes, append the `Vector3DCrossTest` suite; if no, create the file and register it in `GoogleTests.vcxproj`. Either way, no other tests are touched. |
| 5 | Doxygen | Should the header doc-comment include the right-hand-rule diagram? | Brief one-line mention suffices: "`XAxis.Cross(YAxis) == ZAxis`" is the most precise documentation possible. Diagrams in headers tend to drift; the test is the durable contract. |
| 6 | Free function symmetry | The cross product is naturally symmetric in its inputs (`a × b` and `b × a` are equally first-class). Doesn't a method privilege one input arbitrarily? | Yes, slightly. But the Vector3D class already does this for `Dot`, `DistanceTo`, etc. — methods on a vector-vector binary operation. Consistency with the existing pattern wins over abstract symmetry. |
| 7 | Operator overloading | Should `operator*(const Vector3D&)` be repurposed for cross-product (matching some old game-engine conventions)? | No. The existing `operator*(const Vector3D&)` is component-wise multiplication (Vector3D.h:35) and is consumed by existing code. Repurposing it would be a breaking change; the cost vastly outweighs syntactic convenience. |
| 8 | Approval gate | Per CLAUDE.md, this feature must complete all 5 spec steps before Approval. Is there anything blocking? | No prerequisites. Vector3D already exists. This feature unblocks the rest of the DiaMaths 3D additions (Quaternion, Matrix44, Transform3D) so should be implemented first in the chain. |

## Status

`Approved` — Steps 1–4 complete (interview, draft, binding decisions, AI review). Awaiting implementation. First feature in the DiaMaths 3D-additions implementation order.
