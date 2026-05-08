# Feature Spec: Skeleton Component

## Traceability

| Level | Parent | This Feature |
|-------|--------|--------------|
| Platform | @docs/specs/platform/Cluiche.md | - |
| Application | @docs/specs/applications/dia.md | - |
| System | @docs/specs/systems/dia/diarig2d.md | **skeleton-component** |

**Status:** `Approved`

---

## Problem Statement

Game entities need skeletons attached via the engine's component system so that DiaIK2D, animation, rendering, and game code can discover and operate on an entity's skeleton through the standard IComponentObject interface. Without a component wrapper, skeleton management is ad-hoc and invisible to the component system.

---

## Solution Overview

Provide `SkeletonComponent` — an IComponent that owns a Skeleton and its current Pose. Register a `DynamicComponentFactory<SkeletonComponent>` with the ComponentFactoryRegistry.

### SkeletonComponent

```cpp
namespace Dia::Rig2D {
    class SkeletonComponent : public Dia::Core::IComponent {
    public:
        static constexpr Dia::Core::StringCRC kUniqueId{"SkeletonComponent"};

        explicit SkeletonComponent(const SkeletonDef& def);

        const Dia::Core::StringCRC& GetUniqueId() const override;

        const Skeleton& GetSkeleton() const;
        Pose&           GetCurrentPose();
        const Pose&     GetCurrentPose() const;

        void ResetToBindPose();
    };
}
```

### Ownership Model

- SkeletonComponent **owns** its Skeleton and Pose instances (system spec AI Q10).
- Construction takes a SkeletonDef; the component constructs its internal Skeleton and initializes the Pose to bind pose.
- Shared/instanced skeletons are a future optimization — premature for v1.

### Multi-Skeleton Entities

An entity can have multiple SkeletonComponents with different StringCRC IDs (e.g., `"body_skeleton"`, `"face_skeleton"`). The component system supports this natively. No special multi-skeleton API needed (system spec AI Q17).

### Factory Registration

```cpp
// During engine initialization
Dia::Core::ComponentFactoryRegistry::Instance().Register(
    SkeletonComponent::kUniqueId,
    new Dia::Core::DynamicComponentFactory<SkeletonComponent>()
);
```

### Files

| File | Purpose |
|------|---------|
| `Dia/DiaRig2D/SkeletonComponent.h` | SkeletonComponent class declaration |
| `Dia/DiaRig2D/SkeletonComponent.cpp` | Implementation + factory registration helper |

---

## Acceptance Criteria

| # | Criterion | Verification |
|---|-----------|--------------|
| 1 | `SkeletonComponent` inherits from IComponent | Code review |
| 2 | `GetUniqueId()` returns `kUniqueId` StringCRC | Unit test |
| 3 | Constructor takes SkeletonDef, constructs Skeleton internally, initializes Pose to bind pose | Unit test: construct component, verify GetSkeleton().GetBoneCount() and GetCurrentPose() matches |
| 4 | `GetSkeleton()` returns const reference to internal Skeleton | Unit test |
| 5 | `GetCurrentPose()` returns mutable reference for IK/animation to modify | Unit test: modify pose via reference, verify change persists |
| 6 | `ResetToBindPose()` resets current pose to skeleton's bind pose | Unit test: modify pose, reset, verify matches bind pose |
| 7 | `DynamicComponentFactory<SkeletonComponent>` can be registered and used to create components | Unit test: register factory, create component via registry |
| 8 | Multiple SkeletonComponents with different IDs can coexist on one entity | Unit test (if IComponentObject test infrastructure exists) |
| 9 | No STL in public API | Code review |
| 10 | All code in `Dia::Rig2D::` namespace | Code review |

---

## Tasks

| # | Task | Depends On | Notes |
|---|------|------------|-------|
| 1 | Implement `SkeletonComponent.h` / `SkeletonComponent.cpp` | Flat Skeleton, Pose & Pose Blending features | |
| 2 | Unit tests for SkeletonComponent: construction, accessors, bind pose reset, factory registration | 1 | |

---

## Binding Decisions Compliance

| ID | Source | Decision | Compliance |
|----|--------|----------|------------|
| PD-001 | Platform | StringCRC for all entity/component IDs | Compliant — kUniqueId is StringCRC |
| PD-003 | Platform | Component-based entities (IComponent/IComponentObject) | Compliant — SkeletonComponent implements IComponent |
| PD-004 | Platform | No STL in public APIs | Compliant |
| AD-003 | Dia App | Namespace Dia::\<Module\>:: | Compliant — Dia::Rig2D:: |
| AD-005 | Dia App | Component-based entities | Compliant — registered with ComponentFactoryRegistry via DynamicComponentFactory |

---

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Ownership | Should SkeletonComponent take a Skeleton (move/copy) instead of SkeletonDef? | SkeletonDef is better — the component owns the construction process and can validate internally. Passing a pre-built Skeleton would require a move constructor on Skeleton, which adds complexity for no clear benefit. |
| 2 | Factory | DynamicComponentFactory allocates on heap. Should SkeletonComponent use StaticPooledComponentFactory instead? | DynamicComponentFactory for v1. Skeleton counts per scene are low (tens, not thousands). Pooling is a future optimization if profiling shows allocation pressure. |
| 3 | Lifecycle | When is ResetToBindPose called? | Game code calls it explicitly (e.g., on respawn, on animation reset). It's not called automatically by any system. |

---

## Open Questions

None — all resolved above.
