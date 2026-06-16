# Feature Spec: Transform3D

## Traceability

| Level | Spec | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia/dia.md |
| System | DiaMaths | @docs/specs/applications/dia/systems/diamaths/diamaths.md |
| Feature | Transform3D | (this document) |

## Summary

Add `Dia::Maths::Transform3D` — a 3D transform with position (Vector3D), rotation (Quaternion), and scale (Vector3D), supporting parent-child hierarchy via raw, non-owning pointer. Mirrors `Transform2D` (`Dia/DiaMaths/Transform/Transform2D.h:37-228`) verbatim with 3D types substituted: `Vector3D` for `Vector2D`, `Quaternion` for `Angle`, `Matrix44` for `Matrix33`.

Lives alongside `Transform2D` in `Dia/DiaMaths/Transform/`. Uses both Quaternion and Euler-angle setters (per system SD-004), with Quaternion always being the storage form. Provides Y-up right-handed `GetForward / GetRight / GetUp` axes. World-space setters back-solve through parent inverse (per user direction).

## Problem

DiaMaths has no 3D transform. Every consumer that needs a parented 3D entity (renderer scene graph, physics body, gameplay actor) would otherwise:
- Compose Quaternion + Vector3D + Vector3D inline and hand-roll the parent traversal each time; or
- Re-derive world-space accessors per consumer with subtle differences in convention; or
- Use Transform2D and hack a Z component on top.

Transform3D in DiaMaths is the canonical home — single source of truth for hierarchy semantics, world-space derivation, and matrix generation in 3D.

## Acceptance Criteria

1. **Default constructor produces identity transform at origin** — position `(0,0,0)`, rotation `Quaternion::Identity()`, scale `(1,1,1)`, no parent.
2. **`SetLocalRotation(const Quaternion&)`** stores the quaternion verbatim. **`SetLocalRotation(const Angle& yaw, const Angle& pitch, const Angle& roll)`** converts via `Quaternion::FromEuler` (YXZ intrinsic per system SD-004 and `diamaths.md` AI Review Q3) and stores as quaternion.
3. **Storage is always Quaternion.** Euler-overload setters are sugar; no Euler is ever stored as the source of truth.
4. **Local-space getters (`GetLocalPosition`, `GetLocalRotation`, `GetLocalScale`) are O(1) and return const-ref.**
5. **World-space getters traverse the parent chain.** `GetWorldPosition` returns the position transformed up the parent chain. `GetWorldRotation` returns `parent.GetWorldRotation() * mLocalRotation`. `GetWorldScale` returns the component-wise product up the chain.
6. **`GetWorldTransform(outPos, outRot, outScale)` returns all three at once,** traversing the parent chain only once. Documented in source as the optimised path for callers needing all three.
7. **World-space setters back-solve to local** via parent inverse: `SetWorldPosition(p)` updates `mLocalPosition` so `GetWorldPosition()` returns `p` after the call. Same for `SetWorldRotation` and `SetWorldScale`.
8. **`SetParent(Transform3D*)` is a raw, non-owning pointer.** `nullptr` detaches from parent. **No cycle detection in release;** debug builds may assert (best-effort, see AC 9).
9. **Cycle detection in debug:** `SetParent` walks the proposed ancestor chain looking for `this`; asserts and rejects if found. In release the check is compiled out (perf path).
10. **`Translate(delta)` adds delta in local space** — the delta vector is transformed by local rotation before applying. **`TranslateWorld(delta)`** adds delta directly in world space.
11. **`Rotate(Quaternion delta)`** applies `delta * mLocalRotation` (delta in local space, applied first). Mirrors how Transform2D::Rotate works.
12. **`Scale(Vector3D)` and `Scale(float)` multiply current scale element-wise / uniformly.**
13. **`TransformPoint(localPoint)` applies scale → rotate → translate.** Returns world-space point.
14. **`InverseTransformPoint(worldPoint)` applies translate-inverse → rotate-inverse → scale-inverse.** Returns local-space point. Round-trips with `TransformPoint` within float epsilon for non-zero scale.
15. **`TransformDirection` and `InverseTransformDirection` ignore translation.**
16. **`GetLocalMatrix()` and `GetWorldMatrix()` return Matrix44.** Translation in column 3, rotation+scale in upper-left 3×3.
17. **`GetLocalAffine()` and `GetWorldAffine()` return Matrix34** — the same data without the implicit (0,0,0,1) row.
18. **Y-up RH axes:** `GetForward()` returns `mLocalRotation.Rotate(-Vector3D::ZAxis())` for forward (Y-up RH convention: -Z is forward). `GetRight()` returns rotated `+XAxis`. `GetUp()` returns rotated `+YAxis`.
19. **`LookAt(target, up = YAxis)` sets `mLocalRotation`** so `GetForward()` points at `target` (in world space) and `GetUp()` is as close to `up` as possible. Asserts in debug if `target == GetWorldPosition()` or if the up vector is parallel to the target direction.
20. **No new files outside `Dia/DiaMaths/Transform/`.** Existing `Transform2D` is untouched. `Dia/DiaMaths/Transform/Transform3D.h`, `.inl`, `.cpp` are added.
21. **`DiaMaths.vcxproj` and `.vcxproj.filters`** register the three new files under the existing Transform filter.
22. **One unified module doc:** `Dia/DiaMaths/Transform/dia.maths.transform.architecture.module.md` (create if missing) lists both `Transform2D` and `Transform3D` as public_api entry points (per `diamaths.md` AI Review Q7).

## API Design

```cpp
// Dia/DiaMaths/Transform/Transform3D.h
namespace Dia::Maths {

class Vector3D;
class Quaternion;
class Matrix44;
class Matrix34;
class Angle;

// 3D transform with parent-child hierarchy.
//
// MIRRORS Transform2D semantics:
//   - Local space (relative to parent) and World space (absolute)
//   - Raw non-owning parent pointer; caller manages lifetime
//   - No cycles permitted (cycle = stack overflow at GetWorld* time)
//
// ROTATION: Stored as Quaternion. Euler setters are sugar (YXZ intrinsic).
// AXES: Y-up right-handed. Forward = -Z, Right = +X, Up = +Y.
class Transform3D
{
public:
    Transform3D();                                                       // identity at origin
    Transform3D(const Vector3D& position, const Quaternion& rotation, const Vector3D& scale);
    Transform3D(const Transform3D& other);
    Transform3D& operator=(const Transform3D& other);

    // Local space (relative to parent)
    const Vector3D&   GetLocalPosition() const;
    const Quaternion& GetLocalRotation() const;
    const Vector3D&   GetLocalScale()    const;

    void SetLocalPosition(const Vector3D& position);
    void SetLocalRotation(const Quaternion& rotation);
    void SetLocalRotation(const Angle& yaw, const Angle& pitch, const Angle& roll);  // YXZ intrinsic
    void SetLocalScale(const Vector3D& scale);
    void SetLocalScale(float uniformScale);

    // World space (computed via parent chain)
    Vector3D    GetWorldPosition() const;
    Quaternion  GetWorldRotation() const;
    Vector3D    GetWorldScale()    const;

    void SetWorldPosition(const Vector3D& position);
    void SetWorldRotation(const Quaternion& rotation);
    void SetWorldScale(const Vector3D& scale);

    // Optimised batch getter — traverses parent chain only once
    void GetWorldTransform(Vector3D& outPosition, Quaternion& outRotation, Vector3D& outScale) const;

    // Hierarchy
    Transform3D* GetParent() const;
    void         SetParent(Transform3D* parent);                          // debug: asserts no cycle
    bool         HasParent() const;

    // Transformations
    void Translate(const Vector3D& delta);                                // local space (rotated by local rotation)
    void TranslateWorld(const Vector3D& delta);                           // world space (no rotation)
    void Rotate(const Quaternion& delta);                                 // applies delta * current rotation
    void Scale(const Vector3D& scale);
    void Scale(float uniformScale);

    // Space conversions
    Vector3D TransformPoint(const Vector3D& localPoint) const;
    Vector3D TransformDirection(const Vector3D& localDirection) const;
    Vector3D InverseTransformPoint(const Vector3D& worldPoint) const;
    Vector3D InverseTransformDirection(const Vector3D& worldDirection) const;

    // Matrix generation
    Matrix44 GetLocalMatrix() const;
    Matrix44 GetWorldMatrix() const;
    Matrix34 GetLocalAffine() const;
    Matrix34 GetWorldAffine() const;

    // Y-up RH convenience axes
    Vector3D GetForward() const;     // rotated -Z
    Vector3D GetRight()   const;     // rotated +X
    Vector3D GetUp()      const;     // rotated +Y

    // Look at target in world space
    void LookAt(const Vector3D& target, const Vector3D& up = Vector3D::YAxis());

private:
    Vector3D     mLocalPosition;
    Quaternion   mLocalRotation;
    Vector3D     mLocalScale;
    Transform3D* mParent;

    void GetParentWorldTransform(Vector3D& outPosition, Quaternion& outRotation, Vector3D& outScale) const;
};

}  // namespace Dia::Maths
```

## Tasks

| # | Task | Description |
|---|------|-------------|
| 1 | Create `Dia/DiaMaths/Transform/Transform3D.h` | Class declaration |
| 2 | Create `Dia/DiaMaths/Transform/Transform3D.inl` | Inline trivial methods (constructors, member access, simple setters) |
| 3 | Create `Dia/DiaMaths/Transform/Transform3D.cpp` | Non-trivial: world-space getters / setters with parent traversal, world-to-local back-solving, Translate/Rotate/Scale, Transform/InverseTransform, matrix generation, LookAt, debug cycle detection in SetParent |
| 4 | Create / update `Dia/DiaMaths/Transform/dia.maths.transform.architecture.module.md` | List both Transform2D and Transform3D in public_api.entry_points |
| 5 | Update `Dia/DiaMaths/DiaMaths.vcxproj` and `.vcxproj.filters` | Register all three new files; create Transform filter if not already present |
| 6 | Add tests | `Cluiche/Tests/GoogleTests/Maths/TestTransform3D.cpp` |
| 7 | Run `dia run googletest --filter="Transform3D*"` | All green |

## Test Plan (Task 6)

| Suite | Tests |
|-------|-------|
| `Transform3DDefaultTest` | Default constructor: position zero, rotation identity, scale (1,1,1), no parent |
| `Transform3DLocalSettersTest` | SetLocalPosition / Rotation (Quaternion) / Rotation (Euler) / Scale read back via getters |
| `Transform3DEulerOverloadTest` | SetLocalRotation(yaw, pitch, roll) stores quaternion equivalent to FromEuler; YXZ order verified |
| `Transform3DWorldNoParentTest` | With no parent, world == local |
| `Transform3DWorldWithParentTest` | Parent at (10,0,0), child local (1,0,0) → world (11,0,0); parent rotated 90° around Y, child local (1,0,0) → world rotated to (0,0,-1) within epsilon |
| `Transform3DWorldNestedTest` | A → B → C parent chain; C.GetWorld* honours all three transforms; GetWorldTransform optimised path matches per-getter results |
| `Transform3DSetWorldTest` | SetWorldPosition then GetWorldPosition round-trips under a parent; same for SetWorldRotation and SetWorldScale |
| `Transform3DLookAtTest` | LookAt(target) — GetForward dotted with normalized (target - position) is ~1.0; up parallel to target asserts in debug |
| `Transform3DAxesTest` | GetForward = mLocalRotation.Rotate(-ZAxis); GetRight = Rotate(+XAxis); GetUp = Rotate(+YAxis); 90° around Y rotates Forward to -X within epsilon |
| `Transform3DMatrixTest` | GetLocalMatrix.TransformPoint(local) == TransformPoint(local); GetLocalAffine.ToMatrix44 == GetLocalMatrix |
| `Transform3DTranslateRotateScaleTest` | Translate adds in local-rotated space; TranslateWorld adds verbatim; Rotate composes; Scale multiplies element-wise |
| `Transform3DInverseTransformTest` | TransformPoint then InverseTransformPoint round-trips within epsilon for non-degenerate scale |
| `Transform3DCycleDetectionTest` | A.SetParent(&B); B.SetParent(&A) — second call asserts in debug build (DEATH_TEST or equivalent) |

## Files

| File | Action |
|------|--------|
| `Dia/DiaMaths/Transform/Transform3D.h` | Create |
| `Dia/DiaMaths/Transform/Transform3D.inl` | Create |
| `Dia/DiaMaths/Transform/Transform3D.cpp` | Create |
| `Dia/DiaMaths/Transform/dia.maths.transform.architecture.module.md` | Create or modify — list both Transform2D and Transform3D |
| `Dia/DiaMaths/DiaMaths.vcxproj` | Modify |
| `Dia/DiaMaths/DiaMaths.vcxproj.filters` | Modify |
| `Cluiche/Tests/GoogleTests/Maths/TestTransform3D.cpp` | Create |
| `Cluiche/Tests/GoogleTests/GoogleTests.vcxproj` | Modify |
| `Cluiche/Tests/GoogleTests/GoogleTests.vcxproj.filters` | Modify |

## Dependencies

| Dependency | What this feature uses |
|------------|----------------------|
| DiaMaths — `Vector3D` | Position, scale, axis args |
| DiaMaths — `Quaternion` | Rotation storage, composition, world-rotation traversal |
| DiaMaths — `Matrix44` | GetLocalMatrix, GetWorldMatrix |
| DiaMaths — `Matrix34` | GetLocalAffine, GetWorldAffine |
| DiaMaths — `Angle` | Euler-overload setter signatures |
| DiaCore — `DIA_ASSERT` | Cycle detection, LookAt degenerate guard |

**Order:** Transform3D is the **last** of the DiaMaths features. Vector3D::Cross, Quaternion, Matrix44, Matrix34 must all be implemented first (or in the same batch with forward declarations resolved).

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-001 | StringCRC for IDs | **Not applicable.** Transforms have no IDs (caller manages identity). |
| PD-004 | No STL in public API | **Compliant.** All signatures use POD or DiaMaths types. |
| PD-005 | x64 only | **Compliant.** |
| PD-006 | VS project files source of truth | **Compliant.** |
| PD-007 | C++20 required | **Compliant.** |
| PD-008 | Directory.Build.props owns build settings | **Compliant.** |
| AD-001 | Module YAML | **Compliant.** Transform submodule doc created/updated. |
| AD-002 | No STL public APIs | **Compliant.** |
| AD-003 | Namespace `Dia::<Module>::` | **Compliant.** `Dia::Maths::`. |
| SD-001 | DiaMaths is "pure linear algebra only" | **Compliant.** Transform3D is core 3D maths abstraction. |
| SD-002 | Single namespace | **Compliant.** No sub-namespace. |
| SD-004 | Both Quaternion and Euler setter overloads | **Compliant — directly satisfies.** |
| SD-007 | Y-up RH | **Compliant — directly satisfies.** GetForward = -Z, etc. |
| SD-009 | Raw non-owning parent pointer, no cycles | **Compliant — directly satisfies.** Mirrors Transform2D semantics; debug cycle detection. |
| SD-011 | Static lib, no STL | **Compliant.** |
| SD-012 | New types follow existing patterns | **Compliant.** Mirrors Transform2D verbatim with 3D substitutions. |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Mirror | Does Transform3D really need to mirror Transform2D verbatim? | Yes for the public API. Mirrored APIs let game-code authors who know Transform2D move to Transform3D without learning new patterns. The system spec AI Review Q14 in `diamaths.md` confirmed no audit / improvement of Transform2D before mirroring; if Transform2D needs improvement that's a separate feature. |
| 2 | Euler order | YXZ intrinsic vs ZYX vs XYZ? | YXZ intrinsic per `diamaths.md` AI Review Q3 — confirmed by the user during the feature-batch interview. Yaw around Y, then pitch around X', then roll around Z''. Standard for Y-up FPS-style cameras. |
| 3 | World setters | The user explicitly asked for world-space setters mirroring Transform2D. Why are world setters tricky? | `SetWorldRotation(qWorld)` requires solving for `mLocalRotation` such that `parent.GetWorldRotation() * mLocalRotation == qWorld`. Solution: `mLocalRotation = parent.GetWorldRotation().Inverse() * qWorld`. Same approach for position (subtract parent world translation, rotate by parent world rotation inverse, divide by parent world scale) and scale (component-wise divide). Documented in source. |
| 4 | LookAt up vector | What if `up` is parallel to `(target - position)`? | Asserts in debug; release falls back to identity rotation with a warning log. Mirror of Quaternion::LookRotation's policy. |
| 5 | LookAt target == position | What happens? | Asserts in debug; release returns without modifying rotation. Mirror of Transform2D.h:213's "Note: Does nothing if target is at same position as transform". |
| 6 | Cycle detection cost | Walking the ancestor chain on SetParent is O(depth). Worst case? | A pathological deep hierarchy (1000+ levels) would make every SetParent slow. In practice, scene graphs are shallow (10s of levels). Debug-only; release skips entirely. |
| 7 | Translate vs TranslateWorld semantics | What is the difference exactly? | `Translate(delta)` rotates `delta` by the current local rotation before adding to position — this is "move forward in the direction the transform is facing". `TranslateWorld(delta)` adds delta directly to position regardless of orientation — "move along world axes". Both useful. |
| 8 | Scale traversal | Does GetWorldScale really multiply along the parent chain? | Yes — element-wise. Useful for hierarchical zooming and consistent with Unity / Unreal semantics. Edge case: if any parent scale component is zero, world scale collapses to zero (mathematically correct; documented). |
| 9 | Module doc location | Should Transform3D have its own architecture doc separately? | No — one unified `dia.maths.transform.architecture.module.md` per `diamaths.md` AI Review Q7. Transform2D and Transform3D are siblings of identical responsibility (one-dimensional split). |
| 10 | Approval gate | Anything blocking? | Depends on Quaternion + Matrix44 + Matrix34 being implemented (in same batch). Spec can be Approved on its own merits; implementation order is enforced by the chain. |

## Status

`Approved` — Steps 1–4 complete (interview, draft, binding decisions, AI review). Last DiaMaths 3D-additions feature in the implementation chain.
