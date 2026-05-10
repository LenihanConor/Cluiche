# Feature Spec: Transform

## Traceability

| Level | Parent | This Feature |
|-------|--------|--------------|
| Platform | @docs/specs/platform/Cluiche.md | - |
| Application | @docs/specs/applications/dia.md | - |
| System | @docs/specs/systems/dia/diageometry2d.md | **transform** |

**Status:** `Approved`

---

## Problem Statement

`Transform2D` currently lives in `Dia/DiaMaths/Transform/` under `Dia::Maths::`. It provides parent-child scene hierarchy, local/world space, and matrix composition — none of which are pure math concerns. It belongs in the geometry layer. The file needs to move to DiaGeometry2D, the class renamed to `Transform`, and the namespace updated to `Dia::Geometry2D::`.

---

## Solution Overview

Migrate `Transform2D.h/.cpp/.inl` to `Dia/DiaGeometry2D/Transform/Transform.h/.cpp/.inl`. Rename the class from `Transform2D` to `Transform`. Update the namespace. Remove from `DiaMaths.vcxproj`; add to `DiaGeometry2D.vcxproj`. Update all callers.

`Transform` is a plain value type — it is **not** a component (`IComponent`) and carries no DiaApplicationFlow dependency. Game code that wants a transform component wraps it in a Module or Component at the application layer.

### Key Design Points

1. **Plain value type** — no inheritance from `IComponent`, `StateObject`, or any engine framework class
2. **Non-owning parent pointer** — `Transform* mParent` is a raw pointer; caller manages lifetimes
3. **Lazy world matrix** — `GetWorldMatrix()` traverses the parent chain; no cached invalidation in v1 (add caching if profiling shows it's needed)
4. **Scale propagation** — world scale is multiplicative through the parent chain
5. **No circular parents** — `SetParent()` asserts that the new parent is not a descendant of this transform (cycle detection)

---

## Acceptance Criteria

| ID | Criterion | Verification Method |
|----|-----------|---------------------|
| AC1 | `Transform` class exists at `Dia/DiaGeometry2D/Transform/Transform.h` | File system check |
| AC2 | Class is in `Dia::Geometry2D::` namespace | Grep |
| AC3 | `Transform2D` no longer exists in `Dia/DiaMaths/Transform/` | File system check |
| AC4 | `GetLocalPosition/Rotation/Scale` return correct values after `Set*` calls | Unit test |
| AC5 | `GetWorldPosition()` returns sum of all parent positions (no rotation) | Unit test: two-level parent chain |
| AC6 | `GetWorldMatrix()` correctly composes parent-child chain | Unit test: verify matrix multiplication order |
| AC7 | `SetParent(nullptr)` detaches — world space equals local space | Unit test |
| AC8 | `SetParent()` with a descendant asserts (cycle prevention) | Unit test: assert fires |
| AC9 | Full solution builds clean | `msbuild Cluiche.sln /p:Configuration=Debug /p:Platform=x64` |
| AC10 | No `Dia::Maths::Transform2D` references remain in codebase | Grep |

---

## Public API

```cpp
namespace Dia::Geometry2D {

class Transform {
public:
    Transform();

    // --- Local space (relative to parent, or world if no parent) ---
    void SetLocalPosition(const Dia::Maths::Vector2D& position);
    void SetLocalRotation(const Dia::Maths::Angle& rotation);
    void SetLocalScale(const Dia::Maths::Vector2D& scale);

    const Dia::Maths::Vector2D& GetLocalPosition() const;
    const Dia::Maths::Angle&    GetLocalRotation()  const;
    const Dia::Maths::Vector2D& GetLocalScale()     const;

    // Local transform as a matrix
    Dia::Maths::Matrix33 GetLocalMatrix() const;

    // --- World space (absolute, traverses parent chain) ---
    Dia::Maths::Vector2D GetWorldPosition() const;
    Dia::Maths::Angle    GetWorldRotation()  const;
    Dia::Maths::Vector2D GetWorldScale()     const;
    Dia::Maths::Matrix33 GetWorldMatrix()    const;

    // --- Parent-child hierarchy ---
    void        SetParent(Transform* parent);  // nullptr to detach
    Transform*  GetParent() const;
    bool        HasParent() const;

private:
    Dia::Maths::Vector2D mLocalPosition;
    Dia::Maths::Angle    mLocalRotation;
    Dia::Maths::Vector2D mLocalScale;
    Transform*           mParent;  // Non-owning
};

} // namespace Dia::Geometry2D
```

---

## Implementation Notes

### World Matrix Composition

```cpp
Dia::Maths::Matrix33 Transform::GetWorldMatrix() const {
    Dia::Maths::Matrix33 local = GetLocalMatrix();
    if (mParent != nullptr) {
        return mParent->GetWorldMatrix() * local;
    }
    return local;
}
```

Matrix multiplication order: parent * child (parent applied first, consistent with column-vector convention in DiaMaths Matrix33).

### Local Matrix Construction

```cpp
Dia::Maths::Matrix33 Transform::GetLocalMatrix() const {
    // Translation * Rotation * Scale
    Dia::Maths::Matrix33 t = Matrix33::Translation(mLocalPosition);
    Dia::Maths::Matrix33 r = Matrix33::Rotation(mLocalRotation);
    Dia::Maths::Matrix33 s = Matrix33::Scale(mLocalScale);
    return t * r * s;
}
```

### Cycle Detection in SetParent

```cpp
void Transform::SetParent(Transform* parent) {
    if (parent != nullptr) {
        // Walk up parent's chain — assert if we find ourselves
        Transform* check = parent;
        while (check != nullptr) {
            DIA_ASSERT(check != this, "Transform cycle detected");
            check = check->mParent;
        }
    }
    mParent = parent;
}
```

### File Layout

```
Dia/DiaGeometry2D/Transform/
├── Transform.h
├── Transform.cpp
└── Transform.inl
```

---

## Dependencies

### Required Modules
- **DiaMaths** — `Vector2D`, `Angle`, `Matrix33`
- **DiaCore** — `DIA_ASSERT`

### Dependent Features
- **DiaRigidBody2D / physics-body** — `PhysicsBody` holds a non-owning `Transform*`
- Any game entity system that needs 2D scene hierarchy

---

## Testing Strategy

### Unit Tests (`Cluiche/Tests/GoogleTests/Geometry2D/TestTransform.cpp`)

1. **Default state** — position (0,0), rotation 0, scale (1,1), no parent
2. **Local setters/getters** — round-trip for position, rotation, scale
3. **No parent** — world == local for all properties
4. **Single parent** — child world position = parent local + child local (no rotation)
5. **Two-level chain** — grandparent → parent → child; world position sums all three
6. **Rotation propagation** — parent rotation affects child world position
7. **Scale propagation** — parent scale multiplies child world scale
8. **Detach parent** — `SetParent(nullptr)`; world == local after detach
9. **Cycle detection** — `child.SetParent(&child)` fires DIA_ASSERT; parent.SetParent(child) where child's parent is parent fires DIA_ASSERT

---

## Binding Decisions Compliance

| Decision | Source | Summary | Compliance |
|----------|--------|---------|------------|
| PD-004 | Platform | No STL containers in public APIs | ✅ No STL in public interface |
| PD-007 | Platform | C++20 required | ✅ No language features added; compiles under C++20 |
| AD-003 | Dia App | Namespace `Dia::<Module>::` | ✅ `Dia::Geometry2D::` |
| SD-007 | System | Transform parent pointer is non-owning (raw pointer) | ✅ `Transform* mParent` — caller manages lifetime |

---

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Performance | `GetWorldMatrix()` traverses the parent chain every call. Is this acceptable? | Yes for v1. Add a dirty-flag cache (invalidate on any `Set*` or parent change) as a follow-up if profiling shows it's a hotspot. Don't optimise speculatively. |
| 2 | Scale | Should non-uniform scale be supported (Vector2D scale vs float scale)? | Yes — `Vector2D` scale is already in the design. Non-uniform scale is needed for sprite stretching. |
| 3 | Rotation | Should rotation be stored as `Angle`, `float` radians, or `Matrix22`? | `Dia::Maths::Angle` — consistent with DiaMaths convention; wraps float with unit clarity. |
| 4 | Detach behaviour | When `SetParent(nullptr)`, should world position be preserved (convert to local) or just zero the parent? | Zero the parent pointer only — local values unchanged. Preserving world position requires computing the inverse of the old parent's world matrix; that's a convenience helper, not a core behaviour. |
| 5 | Copying | Should `Transform` be copyable? | Yes — it's a plain value type. Copy copies local values and parent pointer (shallow). Semantics are clear: the copy has the same parent but is a different node. |
