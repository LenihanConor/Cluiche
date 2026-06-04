# Feature Spec: foundation

**System:** diaentitytemplate
**App:** Dia
**Status:** Draft

## Summary

Establish the core runtime primitives of diaentitytemplate: `Domain` (entity container), `Entity` (generational handle), `IComponent` abstract base, per-type component pools, entity create/destroy lifecycle, component attachment/detachment, and the end-of-frame structural mutation pipeline.

## Traceability

| Level | Spec |
|---|---|
| Platform | [platform.md](../../../../platform/PLATFORM.md) |
| Application | [dia.md](../../../applications/dia.md) |
| System | [diaentitytemplate.md](../../systems/dia/diaentitytemplate.md) |

## Goals

- Ship `Domain`, `Entity`, and `IComponent` as a buildable static lib (`diaentitytemplate.vcxproj`)
- Provide safe entity creation with generational handles (use-after-free detected via generation mismatch)
- Provide queued component attachment/detachment applied atomically at `EndOfFrame`
- Provide synchronous read access (`GetComponent`, `HasComponent`) that is stable between `EndOfFrame` boundaries
- Call `OnAttach` / `OnDetach` lifecycle hooks at the correct moments

## Acceptance Criteria

- `Domain::CreateEntity` returns a valid generational `Entity` handle; subsequent `IsAlive` returns true
- `Domain::IsAlive` returns false after `QueueDestroy` + `EndOfFrame`
- `QueueAddComponent<T>` queues the attachment; `GetComponent<T>` returns nullptr until `EndOfFrame` applies it
- `QueueRemoveComponent<T>` queues removal; component is still accessible until `EndOfFrame`
- `EndOfFrame` applies all queued mutations in order: creates → attaches → detaches → destroys
- `OnAttach` is called once per component after it becomes live (post-EndOfFrame)
- `OnDetach` is called once per component before its slot is freed
- `QueueDestroy` calls `OnDetach` on every attached component and frees all per-type pool slots
- Build passes as a static lib with no STL types in any public header
- `kMaxEntitiesPerDomain = 1024`; `kMaxMutationsPerFrame = 256`

## Data Model

### Entity

```cpp
namespace Dia::Entity {
    using Entity = Dia::Core::Handle<class EntityTag>; // 32-bit index + 32-bit generation
}
```

### Domain

```cpp
namespace Dia::Entity {
    class Domain {
    public:
        Domain();
        ~Domain();

        Domain(const Domain&) = delete;
        Domain& operator=(const Domain&) = delete;

        Entity CreateEntity();
        Entity CreateEntity(const char* debugName); // stores StringCRC + raw string (debug builds only)

        void QueueDestroy(Entity entity);

        bool IsAlive(Entity entity) const;
        const char* GetDebugName(Entity entity) const; // null in release builds

        template<class TComponent>
        void QueueAddComponent(Entity entity, const Json::Value& config);

        template<class TComponent>
        void QueueRemoveComponent(Entity entity);

        template<class TComponent>
        TComponent* GetComponent(Entity entity);

        template<class TComponent>
        const TComponent* GetComponent(Entity entity) const;

        template<class TComponent>
        bool HasComponent(Entity entity) const;

        void EndOfFrame(); // applies all queued mutations; calls lifecycle hooks
    };
}
```

### IComponent

```cpp
namespace Dia::Entity {
    class IComponent {
    public:
        virtual ~IComponent() = default;

        virtual void OnAttach(Domain& domain, Entity self) {}
        virtual void OnDetach(Domain& domain, Entity self) {}
        virtual void DoUpdate(Domain& domain, Entity self, float dt) {}
        virtual void OnAssetLoaded(Domain& domain, Entity self, Dia::Core::StringCRC assetId) {}

        virtual Dia::Core::StringCRC GetTypeId() const = 0;
    };
}
```

### Mutation Queue

```cpp
namespace Dia::Entity {
    enum class MutationKind : uint8_t { AddComponent, RemoveComponent, DestroyEntity };

    struct MutationOp {
        MutationKind kind;
        Entity       entity;
        // component type CRC for Add/Remove; unused for Destroy
        Dia::Core::StringCRC componentTypeId;
        // config snapshot for AddComponent (shallow Json::Value copy)
        Json::Value  config;
    };

    // Internal to Domain — not public API
    // DynamicArrayC<MutationOp, kMaxMutationsPerFrame>
}
```

### Capacity Constants

```cpp
namespace Dia::Entity {
    inline constexpr uint32_t kMaxEntitiesPerDomain  = 1024;
    inline constexpr uint32_t kMaxMutationsPerFrame  = 256;
}
```

## Files Touched

| File | Change |
|---|---|
| `Dia/diaentitytemplate/diaentitytemplate.vcxproj` | New — static lib project |
| `Dia/diaentitytemplate/diaentitytemplate.vcxproj.filters` | New |
| `Dia/diaentitytemplate/Entity.h` | New — `Entity` type alias + capacity constants |
| `Dia/diaentitytemplate/IComponent.h` | New — `IComponent` abstract base |
| `Dia/diaentitytemplate/Domain.h` | New — `Domain` class declaration |
| `Dia/diaentitytemplate/Domain.cpp` | New — `Domain` implementation |
| `Dia/diaentitytemplate/Domain.inl` | New — template method definitions |
| `Dia/diaentitytemplate/MutationOp.h` | New — `MutationOp` internal struct |
| `dia.entity.architecture.module.md` | New — YAML module doc |
| `Cluiche/Cluiche.sln` | Add `diaentitytemplate` under `Dia` solution folder |
| `Tests/GoogleTests/Entity/DomainTests.cpp` | New — foundation unit tests |

## Binding Decisions Compliance

| Decision | Summary | Compliance |
|---|---|---|
| PD-001 | StringCRC for all IDs | `Entity` debug name stored as `StringCRC` internally. `GetTypeId()` returns `StringCRC`. No raw string IDs in any API. Compliant. |
| PD-002 | PU/Phase/Module architecture | `Domain` has no PU/Phase coupling. Application code wraps it in an `EntityModule`. Compliant. |
| PD-003 | Component-based entities (old IComponent) | Pending Supersede (SD-ENT-021). This feature introduces `Dia::Entity::IComponent` which replaces the old `Dia::Core::IComponent`. Conflict acknowledged; amendment tracked. Compliant in spirit. |
| PD-004 | No STL in public APIs | All public signatures use `Handle<T>`, `HandlePool<T>`, `StringCRC`, `DynamicArrayC`. `Json::Value` (jsoncpp) is the engine's blessed JSON type. No `std::vector`, `std::string`, etc. Compliant. |
| PD-005 | x64 only | `diaentitytemplate.vcxproj` targets x64 exclusively. Compliant. |
| PD-006 | VS project files are source of truth | `diaentitytemplate.vcxproj` + `.vcxproj.filters` created and maintained manually. Compliant. |
| PD-007 | C++20 required | Compiled under `/std:c++20`. Uses concepts for `TComponent : IComponent` constraints, `if constexpr` in template dispatch. Compliant. |
| PD-008 | Directory.Build.props owns toolchain | `diaentitytemplate.vcxproj` does not override OutDir, IntDir, PlatformToolset, WindowsTargetPlatformVersion, or LanguageStandard. Compliant. |
| PD-009 | Generated output under `Cluiche/out/<AppName>/` | diaentitytemplate generates no output artefacts. Compliant. |
| PD-010 | `.diagame` / `.diastage` as root files | Domain has no coupling to stage files. Application's EntityModule reads stage manifests and feeds blueprints in. Compliant. |
| AD-001 | Module YAML frontmatter | `dia.entity.architecture.module.md` created with public API, responsibilities, dependency declarations. Compliant. |
| AD-002 | No STL in public APIs | Reinforces PD-004. Compliant. |
| AD-003 | Namespace `Dia::<Module>::` | All code in `Dia::Entity::` namespace. Compliant. |
| AD-004 | PU/Phase/Module for app structure | Domain does not own a PU. Compliant. |
| AD-005 | Component-based entities (old model) | Pending Supersede. Same as PD-003. Compliant in spirit. |
| SD-ENT-001 | Container is `Domain` | This feature implements the `Domain` class. Compliant. |
| SD-ENT-002 | Systems own data; components are typed adapters | `IComponent` is an abstract adapter base. No system data stored in Domain. Compliant. |
| SD-ENT-003 | Per-type `HandlePool<T>` instances owned by the domain | Each component type gets its own `HandlePool<TComponent, kMaxEntitiesPerDomain>` allocated on first use. Compliant. |
| SD-ENT-004 | Reflection metadata required for every component | Foundation declares `IComponent` and `GetTypeId()`. Full reflection (`DIA_COMPONENT` + `FIELD`) is the `reflection` feature — deferred but the base is laid here. Compliant for this feature scope. |
| SD-ENT-007 | `REQUIRES` hard-validated at attach | `REQUIRES` is part of the `reflection` feature. Foundation provides the attach hook point. Compliant. |
| SD-ENT-012 | Structural changes queued, applied at EndOfFrame | `QueueAddComponent`, `QueueRemoveComponent`, `QueueDestroy` all queue to `DynamicArrayC<MutationOp, kMaxMutationsPerFrame>`. Applied in `EndOfFrame`. Compliant. |
| SD-ENT-013 | Entity slot allocation immediate; component attachments queued | `CreateEntity` allocates the slot immediately and returns a valid handle. `QueueAddComponent` queues the attachment. Compliant. |
| SD-ENT-017 | Domain is non-copyable, non-movable | Copy constructor and copy assignment deleted. Compliant. |
| SD-ENT-018 | Single-threaded per domain | No synchronisation primitives. Concurrent mutation is undefined. Compliant. |
| SD-ENT-019 | Namespace `Dia::Entity::` | Compliant. |
| SD-ENT-020 | Old IComponent removed before diaentitytemplate built | Done (2026-05-20). Compliant. |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Mutation ordering | `EndOfFrame` applies mutations in order: creates → attaches → detaches → destroys. Is this the right sequence? Should destroys run before detaches? | Creates first so newly created entities can receive component attachments in the same frame. Destroys last so detach hooks can still query living components on other entities. Order: creates → attaches → detaches → destroys. |
| 2 | Component pool allocation | Per-type `HandlePool<TComponent, kMaxEntitiesPerDomain>` — allocated on first `QueueAddComponent` for that type. Where is the pool map stored inside Domain? | `DynamicArrayC` of type-erased pool entries keyed by component type CRC. Each entry holds a `StringCRC` typeId + a pointer to a base `IComponentPool` (virtual `Destroy(index)`, `GetRaw(index)`). Concrete `ComponentPool<T>` inherits it. Max distinct component types per domain needs a cap — propose `kMaxComponentTypesPerDomain = 64`. |
| 3 | `GetComponent` before EndOfFrame | If `QueueAddComponent` is called and `GetComponent` is called in the same frame, it returns nullptr. Is this the right contract, or should there be a `GetComponentPending` path? | Null is correct. The queued-mutation contract (SD-ENT-012/013) is the canonical behaviour. Callers that need same-frame access should create the entity and add components in a prior frame, or use blueprints (which batch the whole entity load). No `GetComponentPending` in v1. |
| 4 | Debug name storage | `CreateEntity(const char* debugName)` — in debug builds, store as `StringCRC` + raw `const char*`. Does the Domain own the string memory, or does the caller? | Domain owns it — copies the string into an internal fixed-size debug name buffer (e.g. `char[64]` per entity slot). No heap allocation. In release builds the buffer is `#ifdef`'d out entirely. |
| 5 | `kMaxMutationsPerFrame` overflow | If `kMaxMutationsPerFrame = 256` is exceeded, what happens? | `DIA_ASSERT` in debug. In release, additional mutations beyond the cap are silently dropped and a `DIA_LOG_WARNING` is emitted. This mirrors the DiaMailbox overflow policy. |
| 6 | `EndOfFrame` called zero times | If `EndOfFrame` is never called (e.g. a test that just creates entities), does anything break? | Mutations stay in the queue. `IsAlive` returns true for immediately-created entity slots (CreateEntity is immediate). `GetComponent` returns nullptr for all queued attachments. Safe — no corruption. Tests that skip EndOfFrame are valid for testing the queued state itself. |
| 7 | `DoUpdate` in foundation | `IComponent::DoUpdate` is declared here but `Domain::Update(dt)` is the `update-loop` feature. Is it correct to declare `DoUpdate` in the base now? | Yes — the virtual is on `IComponent`, and foundation needs to define the full base class. `Domain::Update(dt)` just isn't wired yet. Concrete components can override `DoUpdate` from day one; it just won't be called until the `update-loop` feature ships. |
| 8 | `OnAssetLoaded` in foundation | Same question — declared in `IComponent` but the asset trigger flow is the `component-deps-and-refs` feature. | Same answer — declare the virtual now, implement the call-site later. No cost to having an empty virtual in the base. |

## Open Questions

None.

## Status

`Approved`
