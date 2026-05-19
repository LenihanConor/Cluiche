# System Spec: DiaEntity

**Research:** @docs/research/entity_system/summary.md

## Parent Application
@docs/specs/applications/dia.md

## Purpose

DiaEntity is the gameplay-level entity system for the Dia engine. It is a **layered ECS
where systems own their data**: physics owns rigid bodies, rendering owns draw objects,
animation owns skeletons. Entities are lightweight identifiers; components are typed
adapters that hold (a) per-entity config, (b) the asset references that drive system
loads, and (c) a thin gameplay interface delegating to the owning system via a handle.

DiaEntity replaces the existing `IComponent`/`IComponentObject`/`IComponentFactory`
infrastructure (PD-003 / AD-005), which is barely used and philosophically opposed to the
target architecture. The old infrastructure is removed before DiaEntity is built (Decision
8 from the research). PD-003 and AD-005 will need a follow-up Superseded amendment once
DiaEntity is in place; this spec drives that conversation.

The system is built in vertical layers that stack: foundation → reflection → blueprints →
references → hierarchy → mailbox routing → queries → editor inspection → application
integration. Each layer adds value without reworking the previous; the spec captures the
contract for each layer as a feature.

**Dependency chain:**
`DiaEntity → DiaMailbox → DiaCore (StringCRC, HandlePool<T>, DynamicArrayC, DIA_ASSERT, DIA_LOG_WARNING, jsoncpp wrapper)`

DiaEntity has no dependency on DiaApplicationFlow. Application integration (the EntityModule
adapter that plugs a Realm into the SimPU stage lifecycle) lives in application code, not
in DiaEntity.

## Responsibilities

- Provide **`Realm`** — the entity container that owns entity identity, component storage by type, the per-realm mailbox, the entity router, query caches, the reflection registry binding, and the end-of-frame mutation queue. (Renamed from "World" per Decision 16; reads cleanly next to Stage/Module/PU.)
- Provide **`Entity`** — generational handle (`Handle<Entity>` from DiaCore), 64-bit (32 index + 32 generation), realm-local. Optional debug name (`StringCRC` + debug-build string) per Decision 1.
- Provide **`IComponent` (DiaEntity's, not the old DiaCore one)** — the abstract base for all components. Every concrete component declares config, hooks asset triggers, exposes a typed gameplay interface to its owning system, and may opt into `DoUpdate(dt)` per Decision 6.
- Provide **`DIA_COMPONENT(ClassName, "string-name")`** macro — emits the `StringCRC` type ID and a static `ComponentTypeDesc` (reflection metadata: name, size/alignment, schema version, field list, default constructor, JSON load/save). Mirrors `DIA_MODULE` per Decision 2 / 14.
- Provide **`ComponentRegistry`** — process-global registry of `ComponentTypeDesc` keyed by `StringCRC`. Components register at static init via `DIA_COMPONENT`. Realm consults the registry to look up type metadata at runtime.
- Provide **reflection metadata** per Decision 14 — every component declares its serialisable fields via `FIELD(name)` macros. Reflection drives JSON load/save, editor inspection, and prefab schema migration. Field types are a curated set: primitives (bool/int/float/uint), `StringCRC`, math types (`Vec2`/`Vec3`/`Quat`/`Mat44`), asset handles, entity references.
- Provide **blueprint loading** — `JsonBlueprintLoader` reads a versioned JSON blueprint, instantiates an entity graph in the realm, resolves entity references, and triggers asset loads via component asset-trigger hooks. Per Decision 11. Loader is interface-based so a future `UsdBlueprintLoader` can replace it without DiaEntity API churn.
- Provide **typed entity references** — `EntityRef<TComponent>` resolves at construction-time to a target entity that has the required component. Per Decision 7 (typed reference slots). Validation at resolve time, not at every dereference.
- Provide **component dependencies** — components declare required other components via `REQUIRES(OtherComponent)`. Hard-validated at entity creation; a missing dependency is a fatal load error. Per Decision 4.
- Provide **parent/child hierarchy** — `Parent` and `ChildBuffer` components per Decision 15. Cycle rejection in debug. Destroy-cascade default policy: destroy subtree. Re-parenting routes through the end-of-frame mutation pipeline.
- Provide **the entity router** — implementation of `Dia::Mailbox::IMailboxRouter` for `kEntityRouterId`. Decodes a tagged 64-bit address payload into one of four kinds: Entity (specific entity by handle), All (all entities in realm), ComponentType (all entities with a given component type), Self (sender entity). Resolves to the matching subscriber set.
- Provide **end-of-frame mutation pipeline** — structural changes (entity create/destroy, component add/remove, parent change) collected during the frame, applied as a batch at the realm's `EndOfFrame()` boundary. Per Decision 10. Query caches rebuild after mutation; mailbox messages queued during the frame are delivered next frame.
- Provide **query system** — `Realm::Query<ComponentA, ComponentB...>()` returns an iterator over entities that have all listed components. Caches matching entity sets keyed by signature; invalidates at `EndOfFrame()` based on collected mutations. Dynamic (no archetype storage) and good enough for v1's <1000-entity scale.
- Provide **`IEntityInspectable`** per Decision 3 — polled inspection interface mirroring `IApplicationInspectable`. Editor reads entity list, components per entity, field values, mailbox log, and (with reflection-driven setters) writes individual fields for live tuning. Per Decision 17 tier (a)+(b).
- Provide **debug editing tiers** per Decision 17 — read-only inspection (a) and live field edit (b) are v1; live structural edit (c) is deferred but the mutation pipeline must remain capable of routing editor-originated commands when (c) is built.
- Ship as `Dia/DiaEntity/DiaEntity.vcxproj` static library registered in `Cluiche.sln`.
- Provide `dia.entity.architecture.module.md` YAML module documentation.

## Non-Responsibilities

- **Application lifecycle.** DiaEntity does not depend on DiaApplicationFlow. The `EntityModule` adapter that owns a Realm and plugs into a stage's lifecycle is an application-level concern. Lives in CluicheTest / future games, not in DiaEntity.
- **Stage lifecycle.** Realms are created and destroyed by their owner. Stage-boundary hard cuts are the dominant pattern but not enforced by DiaEntity.
- **Cross-realm anything.** Entity handles are realm-local. There is no cross-realm reference, migration, or shared state. Editor-preview / test-fixture realms are supported by leaving `Realm` constructible as a normal value type, but DiaEntity has no API for talking between realms (Decision 16 — explicit non-decision).
- **System-side data.** Physics bodies, render objects, skeletons, state machines all live in their respective systems. DiaEntity holds handles into those systems via component fields.
- **Multi-thread safety.** Single-threaded per realm. Cross-PU traffic uses DiaApplicationFlow streams or DiaMailbox cross-mailbox patterns (which DiaMailbox does not provide today). Concurrent realm mutation is undefined.
- **Network replication.** No replication traits, no snapshot extraction, no entity ID mapping for multiplayer.
- **Animation graph / physics step / render extraction.** Those run in their owning systems. DiaEntity provides queries and component access.
- **Determinism.** No deterministic execution mode. Default order is component-type-registration-order then entity-index-order; consumers that need stricter ordering own that responsibility.
- **General reflection for the engine.** Reflection lives **inside DiaEntity** and applies to component types only. If a second consumer (asset definitions, editor configs) needs reflection, it gets lifted to a `DiaReflection` module then — not preemptively. Per the research addendum's scope discipline.
- **General relationship graph.** Only parent/child hierarchy. Arbitrary relationship edges between entities are out of scope.
- **Old IComponent / IComponentObject / IComponentFactory.** Those are removed before DiaEntity is built (Decision 8). DiaEntity introduces a new `IComponent` in its own namespace.

## Public Interfaces

### Entity, Realm

```cpp
namespace Dia::Entity {
    // Realm-local generational handle. Equivalent to Handle<EntityTag> from DiaCore.
    using Entity = Dia::Core::Handle<class EntityTag>;

    class Realm {
    public:
        Realm(); // capacity is template-parameterised on a typedef alias; see decisions
        ~Realm();

        Realm(const Realm&) = delete;
        Realm& operator=(const Realm&) = delete;

        // Create a fresh entity with no components attached. Returns Invalid() if full.
        // Structural change — applied immediately for entity slot allocation; component
        // attachments via AddComponent are queued through the end-of-frame mutation pipeline.
        Entity CreateEntity();
        Entity CreateEntity(Dia::Core::StringCRC debugName); // overload with name

        // Queue a destroy. Applied at EndOfFrame.
        void   QueueDestroy(Entity entity);

        bool   IsAlive(Entity entity) const;
        const char* GetDebugName(Entity entity) const; // null in release if no name set

        // Add a component. Constructs the component in its type-pool (HandlePool<T>),
        // wires it to the entity, applies REQUIRES validation. Queued to EndOfFrame.
        // Component config is a JSON object (typically the corresponding subnode of a blueprint).
        template<class TComponent>
        void QueueAddComponent(Entity entity, const Json::Value& config);

        template<class TComponent>
        void QueueRemoveComponent(Entity entity);

        // Synchronous read accessors. Returns nullptr if entity has no such component
        // or is not alive. Pointer is stable until the next EndOfFrame applies a structural
        // change to that entity (component add/remove or destroy).
        template<class TComponent>
        TComponent* GetComponent(Entity entity);

        template<class TComponent>
        const TComponent* GetComponent(Entity entity) const;

        // Returns true if entity has a component of this type.
        template<class TComponent>
        bool HasComponent(Entity entity) const;

        // Per-frame entry points. Applications drive these from their stage update loops.
        void Update(float dt);    // walks all DoUpdate-opted components in registration order
        void EndOfFrame();        // applies queued structural changes; rebuilds query caches; delivers mailbox

        // Query: returns a view that iterates entities with all listed components.
        template<class... TComponents>
        QueryView<TComponents...> Query();

        // Access to the realm-owned mailbox. Senders, subscribers, and routers operate on this.
        Dia::Mailbox::Mailbox&       GetMailbox();
        const Dia::Mailbox::Mailbox& GetMailbox() const;

        // The entity router is auto-registered with the mailbox on Realm construction.
        // Address::routerId for entity-flavoured addresses is kEntityRouterId
        // (declared in EntityAddress.h, see Address Encoding section below).
    };
}
```

### IComponent and DIA_COMPONENT

```cpp
namespace Dia::Entity {
    class IComponent {
    public:
        virtual ~IComponent() = default;

        // Called once after construction and field load, before any DoUpdate call.
        virtual void OnAttach(Realm& realm, Entity self) {}

        // Called once before destruction. The component is still attached and queryable.
        virtual void OnDetach(Realm& realm, Entity self) {}

        // Optional per-frame tick. Components opt in by overriding; the registry sees
        // the override via reflection metadata (a flag on ComponentTypeDesc).
        virtual void DoUpdate(Realm& realm, Entity self, float dt) {}

        // Called when a referenced asset has finished loading. Fires per asset trigger.
        virtual void OnAssetLoaded(Realm& realm, Entity self, Dia::Core::StringCRC assetId) {}

        // Identity (filled in by DIA_COMPONENT macro)
        virtual Dia::Core::StringCRC GetTypeId() const = 0;
    };
}

// In DonkeyFeetComponent.h:
//
// class DonkeyFeetComponent : public Dia::Entity::IComponent {
// public:
//     DIA_COMPONENT(DonkeyFeetComponent, "donkey-feet");
//
//     FIELD(int,         numFeet,    2);              // type, name, default
//     FIELD(StringCRC,   colour,     StringCRC{"black"});
//
//     REQUIRES(TransformComponent);  // hard-validated at attach
//
//     void DoUpdate(Realm&, Entity, float dt) override;
// };
//
// In DonkeyFeetComponent.cpp (or a registration unit):
//   DIA_COMPONENT_REGISTER(DonkeyFeetComponent);
//
// The macro expands to: static type CRC, static ComponentTypeDesc with field array,
// JSON load/save thunks, default constructor entry, and a Register call into
// ComponentRegistry that runs at static init.
```

### Reflection — ComponentTypeDesc

```cpp
namespace Dia::Entity {
    enum class FieldType : uint8_t {
        Bool, Int32, UInt32, Float, StringCRC,
        Vec2, Vec3, Quat, Mat44,
        AssetHandle, EntityRef
    };

    struct FieldDesc {
        const char* name;
        uint16_t    offset;
        FieldType   type;
        // Default value is encoded in the ComponentTypeDesc's defaultConstruct fn.
    };

    using LoadFromJsonFn = void (*)(IComponent* dst, const Json::Value& config);
    using SaveToJsonFn   = void (*)(const IComponent* src, Json::Value& outConfig);
    using DefaultCtorFn  = void (*)(void* placement); // placement-new T{}

    struct ComponentTypeDesc {
        Dia::Core::StringCRC typeId;
        const char*          debugName;
        uint16_t             size;
        uint16_t             alignment;
        uint16_t             schemaVersion;
        uint16_t             flags;            // overridesDoUpdate, etc.

        const FieldDesc*     fields;
        uint16_t             fieldCount;

        DefaultCtorFn        defaultConstruct;
        LoadFromJsonFn       loadFromJson;
        SaveToJsonFn         saveToJson;
    };

    class ComponentRegistry {
    public:
        static ComponentRegistry& Get(); // process-global

        bool Register(const ComponentTypeDesc& desc); // called from DIA_COMPONENT_REGISTER
        const ComponentTypeDesc* Find(Dia::Core::StringCRC typeId) const;

        // Iteration for editor / debug tools.
        uint32_t GetCount() const;
        const ComponentTypeDesc& GetByIndex(uint32_t i) const;
    };
}
```

### Address Encoding & Entity Router

The entity router decodes the opaque 64-bit `Address::payload` from DiaMailbox. Encoding
is a tagged union: high byte selects kind, low 56 bits carry the kind-specific body.

```cpp
namespace Dia::Entity {
    extern const Dia::Core::StringCRC kEntityRouterId; // = StringCRC("dia.entity.router")

    enum class AddressKind : uint8_t {
        Entity        = 1, // body = (Entity::index : 32 | Entity::generation : 24); top 8 bits free
        All           = 2, // body unused
        ComponentType = 3, // body = StringCRC of component type (low 32 bits)
        Self          = 4  // body = sender's Entity packed same way as kind=Entity
    };

    // Payload layout: bits 56..63 = kind, bits 0..55 = body (kind-specific)
    Dia::Mailbox::Address MakeEntityAddress(Entity target);
    Dia::Mailbox::Address MakeAllAddress();
    Dia::Mailbox::Address MakeComponentTypeAddress(Dia::Core::StringCRC componentTypeId);
    Dia::Mailbox::Address MakeSelfAddress(Entity sender);

    AddressKind  GetAddressKind(const Dia::Mailbox::Address&);
    Entity       GetAddressEntity(const Dia::Mailbox::Address&);          // valid when kind==Entity or Self
    Dia::Core::StringCRC GetAddressComponentType(const Dia::Mailbox::Address&); // valid when kind==ComponentType

    class EntityRouter final : public Dia::Mailbox::IMailboxRouter {
    public:
        explicit EntityRouter(Realm& realm);

        Dia::Core::StringCRC GetRouterId() const override { return kEntityRouterId; }

        void Resolve(const Dia::Mailbox::Address& addr,
                     const Dia::Mailbox::SubscriberSet& live,
                     Dia::Mailbox::SubscriberSet& outMatched) override;

    private:
        Realm& mRealm;
    };
}
```

The 24-bit generation in the Entity-kind encoding sacrifices 8 bits of generation room
relative to `Handle<T>`'s native 32-bit generation. This is acceptable — 24 bits of
generation rolls every 16M frees on the same slot, which at 60 Hz is ~3 days of
worst-case continuous churn on a single slot. If a real-world scenario approaches that,
we either widen the address payload (deferred) or use a side-channel for high-churn pools.

### Hierarchy

```cpp
namespace Dia::Entity {
    class ParentComponent : public IComponent {
    public:
        DIA_COMPONENT(ParentComponent, "parent");
        FIELD(EntityRef<class IComponent>, value, Entity::Invalid());
        // EntityRef with IComponent base means "any entity is acceptable as parent"
    };

    class ChildBufferComponent : public IComponent {
    public:
        DIA_COMPONENT(ChildBufferComponent, "child-buffer");
        // children stored in a fixed-cap dynamic array; cap revisited per real use case
        Dia::Core::Containers::DynamicArrayC<Entity, 16> children;
    };

    namespace Hierarchy {
        // Queue a parent change. Applied at EndOfFrame. Cycle check runs in debug.
        void QueueSetParent(Realm& realm, Entity child, Entity parent);

        // Queue subtree destruction. Applied at EndOfFrame.
        void QueueDestroySubtree(Realm& realm, Entity root);
    }
}
```

### Query

```cpp
namespace Dia::Entity {
    template<class... TComponents>
    class QueryView {
    public:
        // Iterator that yields (Entity, TComponents*...) tuples. Pointers are stable for
        // the lifetime of the QueryView (i.e. between EndOfFrame boundaries).
        class Iterator { /* ... */ };

        Iterator begin();
        Iterator end();
        uint32_t Count() const;
    };
}
```

Implementation: query results are cached per (component-type-set) signature inside Realm.
Cache is rebuilt at `EndOfFrame()` by walking the entity-component matrix once. v1 scale
is <1000 entities × <50 component types; the rebuild is cheap. No archetype storage.

### IEntityInspectable

```cpp
namespace Dia::Entity {
    class IEntityInspectable {
    public:
        virtual ~IEntityInspectable() = default;

        // Read-only inspection (Tier a — Decision 17)
        virtual uint32_t GetEntityCount() const = 0;
        virtual void     GetAllEntities(Dia::Core::Containers::DynamicArrayC<Entity, 1024>& out) const = 0;
        virtual void     GetComponentTypeIds(Entity, Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 32>& out) const = 0;

        // Reflected field read
        virtual bool ReadField(Entity, Dia::Core::StringCRC componentTypeId,
                               const char* fieldName, Json::Value& out) const = 0;

        // Mailbox / debug-event log access
        virtual const Dia::Mailbox::Mailbox& GetMailbox() const = 0;

        // Live field edit (Tier b — Decision 17)
        virtual bool WriteField(Entity, Dia::Core::StringCRC componentTypeId,
                                const char* fieldName, const Json::Value& value) = 0;
    };
}
```

`Realm` implements `IEntityInspectable`. Tier (c) live structural edit is intentionally
absent from this v1 surface; when added later, additional `QueueX` methods join here.

## Features

| Feature | Description | Spec | Status |
|---------|-------------|------|--------|
| remove-old-icomponent | Delete `Dia/DiaCore/Architecture/Components/`, migrate `SkeletonComponent` and `StateMachineComponent` consumers, remove related tests. Per Decision 8. Must land before any DiaEntity code is written. | TBD | Planned |
| foundation | `Realm`, `Entity`, `IComponent` base, component type-pools (one `HandlePool<T>` per component type), entity create/destroy lifecycle, basic `GetComponent`/`HasComponent`, end-of-frame structural change application. | TBD | Planned |
| reflection | `DIA_COMPONENT` macro, `FIELD` macro, `ComponentTypeDesc`, `ComponentRegistry`, JSON load/save thunks, schema version field. Per Decision 14. | TBD | Planned |
| blueprint-loader | `JsonBlueprintLoader` interface + concrete impl, versioned JSON schema with separate references block, asset-trigger callback into components. Per Decision 11. | TBD | Planned |
| component-deps-and-refs | `REQUIRES` macro with hard validation at attach, `EntityRef<TComponent>` typed reference slots, two-pass blueprint resolution (instantiate then patch references). Per Decisions 4, 7. | TBD | Planned |
| hierarchy | `ParentComponent`, `ChildBufferComponent`, `Hierarchy::QueueSetParent`/`QueueDestroySubtree`, cycle rejection in debug, destroy-subtree default cascade. Per Decision 15. | TBD | Planned |
| mailbox-router | `EntityRouter`, address encoding (`MakeEntityAddress` etc.), auto-registration of router with realm's mailbox, resolve implementations for all four kinds (Entity/All/ComponentType/Self). | TBD | Planned |
| query-system | `Realm::Query<...>`, signature-keyed query cache, end-of-frame invalidation/rebuild. Per Decision 10. | TBD | Planned |
| editor-inspection | `IEntityInspectable` implementation on Realm, reflection-driven `ReadField`/`WriteField`, mailbox log accessor. Tiers (a) + (b) per Decision 17. | TBD | Planned |
| update-loop | `Realm::Update(dt)` walks DoUpdate-opted components in registration order, then entity-index order. Per Decision 6. | TBD | Planned |
| module-and-build | `DiaEntity.vcxproj`, registration in `Cluiche.sln`, `dia.entity.architecture.module.md` YAML, dependency edge in `dia_modules.py`. | TBD | Planned |

Feature specs to be written after this system spec is approved. The order above is the
intended implementation order; each feature stacks on the previous.

## Dependencies on Other Systems

**Required:**
- **DiaCore** — `Handle<T>`, `HandlePool<T>` (one per component type, plus the entity slot pool itself), `StringCRC`, `DynamicArrayC`, `DIA_ASSERT`, `DIA_LOG_WARNING`, jsoncpp wrapper for blueprint loading
- **DiaMailbox** — `Mailbox` (one per realm), `Address` (entity router decodes payload), `IMailboxRouter` (entity router implements), `SubscriberSet` (entity router fills)
- **DiaMaths** — math types referenced by reflection's curated field type set (`Vec2`/`Vec3`/`Quat`/`Mat44`)

**Explicitly excluded:**
- **DiaApplicationFlow** — Realm has no knowledge of stages, modules, or PUs. Application integration lives outside DiaEntity.
- **DiaSerializer** — Reflection-driven JSON load/save lives inside DiaEntity. Reusing `DiaSerializer::MetadataValue` was considered but rejected: components have typed schemas (rich field types incl. asset handles, entity refs), not free-form metadata bags. DiaSerializer remains the right tool for engine definition files; DiaEntity is the right tool for component schemas.
- **Any specific gameplay system** (DiaRigidBody2D, DiaRig2D, DiaStateMachine, DiaGraphics) — components in those systems consume DiaEntity (their components live in their respective modules, register via `DIA_COMPONENT`, use DiaEntity from below). DiaEntity does not depend on any of them.

**Dependents (once DiaEntity ships):**
- **CluicheTest application code** — `EntityModule` adapter that owns a Realm, plugs into DiaApplicationFlow v2 stage lifecycle (DoStart loads blueprints → returns kLoading until assets resolve → kReady; stage transition destroys realm)
- **CluicheEditor** — implements an editor panel against `IEntityInspectable` for the running realm
- **All future component-bearing systems** — physics components, rendering components, animation components, AI components

## Out of Scope

- **Migration to DiaApplicationFlow's stream system.** Cross-realm or cross-PU entity traffic. SimPU is single-realm in v1.
- **Hot reload of components.** Schema migrations across reload are designed for (schema version field) but live reload of running entity state is deferred.
- **Live structural edit from editor (Decision 17 tier c).** Pipeline must remain capable of routing such commands later; v1 does not expose the API.
- **Determinism.** Entity-index iteration order is stable but no broader guarantees.
- **Garbage collection of orphan handles into systems.** When a component is removed, its `OnDetach` is called and the component is responsible for releasing system-side handles. DiaEntity does not enforce or audit this.
- **Network replication, snapshot extraction, prediction.** Deferred indefinitely.
- **General relationship graph beyond parent/child.** Deferred.
- **Multi-realm communication.** Each realm is a self-contained universe.

## Decisions

| ID | Decision | Rationale | Scope | Status | Binding |
|----|----------|-----------|-------|--------|---------|
| SD-ENT-001 | Container is `Realm`, not `World` | "World" overlaps with Stage in this codebase. `Realm` is short, unambiguous, reads cleanly. Per research Decision 16. | All | Accepted | Yes |
| SD-ENT-002 | Systems own their data; components are typed adapters (config + asset trigger + interface) | Foundational architectural stance. Distinguishes DiaEntity from archetype-chunk ECS designs. Per research Decision 0 / explore.md. | All | Accepted | Yes |
| SD-ENT-003 | Component data is stored in per-type `HandlePool<T>` instances owned by the realm | Reuses HandlePool primitive. Generation tracking gives use-after-free safety. Per-type pools keep memory layout predictable; entity is just a row of handles into pools. | All | Accepted | Yes |
| SD-ENT-004 | Reflection metadata is required for every component type; declared via `DIA_COMPONENT` + `FIELD` macros | Drives JSON load/save and editor inspection without per-type hand-written code. Hard-coded prerequisite for blueprints and inspector. Per research Decision 14 (addendum). | All | Accepted | Yes |
| SD-ENT-005 | Reflection is component-only and lives inside DiaEntity (not a generic `DiaReflection` module) | Scope discipline. Lift to a separate module only if a second consumer emerges. Per research addendum. | Reflection | Accepted | Yes |
| SD-ENT-006 | Blueprint format is versioned JSON with separate references block; loader is interface-based for future replacement | JSON aligns with project preference. Versioning supports schema migration. Interface-based loader keeps a clean path to USD or other formats. Per research Decision 11. | Blueprints | Accepted | Yes |
| SD-ENT-007 | Component dependencies (`REQUIRES`) are hard-validated at attach; missing deps are fatal load errors | Silent dependency failure produces broken entities that crash later in unrelated code. Per research Decision 4. | Foundation | Accepted | Yes |
| SD-ENT-008 | Cross-entity references are typed slots (`EntityRef<TComponent>`) validated at resolve | Type tag on the slot prevents wiring arbitrary entities into reference holes. Validated once at resolve, not at every dereference, for cost. Per research Decision 7. | Foundation | Accepted | Yes |
| SD-ENT-009 | Hierarchy is parent/child only via `Parent` and `ChildBuffer` components; destroy-cascade default is destroy-subtree | Covers attached weapons, particle emitters, scene grouping. General relationships deferred. Per research Decision 15 (addendum). | Hierarchy | Accepted | Yes |
| SD-ENT-010 | Entity router decodes a tagged 64-bit payload (kind in high byte; body kind-specific) | Single-payload encoding keeps DiaMailbox's `uint64` opaque address sufficient for all four kinds. Refines research Decision 9 in light of DiaMailbox's generic shape. | Routing | Accepted | Yes |
| SD-ENT-011 | Address-encoded entity generation is 24-bit (high byte holds kind) | Sacrifices 8 generation bits relative to native `Handle<Entity>`. ~3 days of continuous worst-case churn on one slot. Acceptable for v1. | Routing | Accepted | Yes |
| SD-ENT-012 | Structural changes (create entity, destroy, add/remove component, set parent) are queued and applied at `Realm::EndOfFrame()` | Avoids iterator invalidation and order-dependent bugs during gameplay update. Per research Decision 10. | Mutation | Accepted | Yes |
| SD-ENT-013 | Entity slot allocation is immediate (returns valid `Entity` handle); component attachments are queued | Callers need a stable handle to wire references in code. The slot exists immediately but the entity has no components until EndOfFrame applies the queued attachments. Documented contract. | Mutation | Accepted | Yes |
| SD-ENT-014 | Query results are cached per signature, rebuilt at EndOfFrame when relevant components were added/removed | Dynamic enough for v1 scale. Archetype storage is overkill (research candidate 4 rejected). Per research Decision 10. | Queries | Accepted | Yes |
| SD-ENT-015 | `Realm::Update(dt)` walks DoUpdate-opted components in component-type-registration order, then entity-index order | Predictable, simple, deterministic. No per-component scheduling. Per user choice during system spec interview. Decision 6 from research is fully realised here. | Update | Accepted | Yes |
| SD-ENT-016 | Editor inspection is the v1 surface; live field edit is v1; live structural edit is deferred | Stage-boundary reload IS the primary content-iteration loop. Live structural edit serves runtime probing, not authoring. Per research Decision 17 (addendum). | Editor | Accepted | Yes |
| SD-ENT-017 | Realm is non-copyable, non-movable | Owns inline component pools and outstanding handles tied to its address. Movability would require remapping all handles. | All | Accepted | Yes |
| SD-ENT-018 | Single-threaded per realm | Concurrent realm mutation is not in v1 priorities. Cross-PU is the stream system. | All | Accepted | Yes |
| SD-ENT-019 | Namespace is `Dia::Entity::` | Consistent with `Dia::<Module>::` (AD-003). | All | Accepted | Yes |
| SD-ENT-020 | The old `IComponent` infrastructure is removed before DiaEntity is built | Clean slate. The two existing consumers (SkeletonComponent, StateMachineComponent) are migrated as part of removal — they become DiaEntity components in their own modules. Per research Decision 8. | Migration | Accepted | Yes |
| SD-ENT-021 | PD-003 / AD-005 require a Superseded amendment once DiaEntity ships | Those decisions reference the old `IComponent` model. They cannot remain Accepted as written. Amendment text is out of scope of this system spec but the requirement is captured here. | Platform / App decisions | Accepted | Yes |

**Status values:** `Proposed` · `Accepted` · `Rejected` · `Superseded`
**Binding:** `Yes` = enforced constraint on all features in this system · `No` = guidance only

## Inherited Binding Decisions

| ID | Source | Decision | Implication for this system |
|----|--------|----------|----------------------------|
| PD-001 | Platform | StringCRC for all entity/component IDs | Component type IDs are `StringCRC` (via `DIA_COMPONENT`). Router IDs are `StringCRC`. Entity debug names are `StringCRC` with debug-build string storage. |
| PD-002 | Platform | ProcessingUnit/Phase/Module architecture | DiaEntity does not own a PU/Module. Application code wraps Realm in an `EntityModule` that lives in SimPU. DiaEntity itself has no PU/Phase coupling. |
| PD-003 | Platform | Component-based entities (IComponent/IComponentObject) | **Conflict — Pending Supersede.** This decision references the old IComponent model that DiaEntity replaces. Per SD-ENT-021, PD-003 needs a Superseded amendment. DiaEntity introduces a new `Dia::Entity::IComponent` distinct from the old one, satisfying the spirit of "component-based entities" with a different concrete model. Open question: track in CONFLICT table below. |
| PD-004 | Platform | No STL containers in public APIs | All public APIs use `DynamicArrayC`, `Handle<T>`, `HandlePool<T>`, `StringCRC`. No STL types in any signature. Internal use of `Json::Value` (jsoncpp) is allowed because jsoncpp is the engine's blessed JSON library, not STL. |
| PD-005 | Platform | x64 only | `DiaEntity.vcxproj` targets x64 exclusively. |
| PD-006 | Platform | Visual Studio project files are source of truth | `DiaEntity.vcxproj` and `.vcxproj.filters` created and manually maintained. |
| PD-007 | Platform | C++20 required | Compiled under `/std:c++20`. Uses concepts for `EntityRef<TComponent>` constraints, `if constexpr` for reflection field-type dispatch, `std::span` for read-only views internally (not in public APIs per PD-004). |
| PD-008 | Platform | Directory.Build.props owns OutDir/IntDir/toolchain | `DiaEntity.vcxproj` must NOT override OutDir, IntDir, PlatformToolset, WindowsTargetPlatformVersion, or LanguageStandard. |
| PD-009 | Platform | Generated output under `Cluiche/out/<AppName>/` | DiaEntity does not generate output. The blueprint loader reads JSON from asset paths but does not write generated artefacts. |
| PD-010 | Platform | `.diagame` is the project root file; `.diastage` declares stage metadata | DiaEntity does not directly couple to `.diagame` or `.diastage`. Application's `EntityModule` reads stage manifests and feeds blueprint asset paths to DiaEntity via the loader interface. |
| AD-001 | Dia App | Module system with YAML frontmatter | Create `dia.entity.architecture.module.md` with public API, responsibilities, and dependency declarations. |
| AD-002 | Dia App | No STL containers in public APIs | Reinforces PD-004. |
| AD-003 | Dia App | Namespace `Dia::<Module>::` | All code in `Dia::Entity::` namespace. |
| AD-004 | Dia App | ProcessingUnit/Phase/Module for app structure | DiaEntity does not own a PU. Composition with DiaApplicationFlow is the application's job. |
| AD-005 | Dia App | Component-based entities (IComponent/IComponentObject) | **Conflict — Pending Supersede.** Same as PD-003. AD-005 references the old model; DiaEntity replaces it. Open question below. |

## Conflicts and Open Questions

| ID | Issue | Resolution |
|----|-------|-----------|
| C-1 | PD-003 and AD-005 reference the old `IComponent`/`IComponentObject` model | DiaEntity supersedes both. After DiaEntity ships, both decisions need a Superseded amendment that points to this spec as the new authority. SD-ENT-021 captures this requirement. The amendment is a separate document edit and is not part of this spec's implementation; it is a follow-up housekeeping task. **For the purposes of approving this spec:** acknowledged conflict, design proceeds, amendment tracked. |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Realm capacity | Realm needs a fixed entity capacity (HandlePool requires compile-time capacity). What's the v1 number? | The research locked <1000 entities per stage as the v1 scale. A `kMaxEntitiesPerRealm = 1024` default in `Realm.h` (template-parameterised on `Realm`'s own type so consumers can specialise) is the v1 target. Components type-pools size-default to `kMaxEntitiesPerRealm` as well — sparse pools are cheap because they're not allocated until a component type registers. |
| 2 | Component pools sized per-type | Each component type wants its own HandlePool capacity. How is this sized? | Default to `kMaxEntitiesPerRealm` for all component types (worst case: every entity has every component). Components that are known-rare (e.g. `BossEnemyTagComponent`) can override at registration via an optional second arg to `DIA_COMPONENT_REGISTER`. v1 doesn't optimise this; revisit when memory is actually a concern. |
| 3 | DIA_COMPONENT vs DIA_MODULE | The research says "matches DIA_MODULE" but DIA_MODULE has its own conventions. What's the actual macro signature? | `DIA_COMPONENT(ClassName, "string-name")` declares the type and emits a static type CRC + a static `ComponentTypeDesc` populated by `FIELD(...)` macros within the class body. `DIA_COMPONENT_REGISTER(ClassName)` is a separate macro placed in a `.cpp` file that registers the desc with `ComponentRegistry` at static init. Two macros instead of one to keep header-only declarations clean and avoid ODR violations on the registration call. |
| 4 | FIELD types | The curated field type list (Bool/Int32/UInt32/Float/StringCRC/Vec2/Vec3/Quat/Mat44/AssetHandle/EntityRef) — is this complete? | Sufficient for v1. Missing types that may appear later: enums (handled as Int32 + a debug-time enum-name table), bitfields (also Int32), arrays of fields (deferred — currently a component with an array-typed field hand-rolls JSON load/save). The spec explicitly excludes arbitrary C++ types: if a component needs more, it doesn't get reflection. |
| 5 | EntityRef implementation | `EntityRef<TComponent>` — how is it stored? | `struct EntityRef { Entity entity; }`. The template parameter is purely for type-checking at the API layer (concepts assert `TComponent : IComponent`). Stored as a plain `Entity`. Resolution validates that `realm.HasComponent<TComponent>(ref.entity)` once at attach (or at blueprint resolve); after that, dereferencing is a normal `realm.GetComponent<TComponent>(ref.entity)` call. |
| 6 | Asset trigger flow | "Component config drives asset loads" — what's the actual handshake? | Component declares one or more `ASSET_FIELD(name, AssetTypeId)` fields (subset of FIELD that flags the field as an asset trigger). Blueprint loader enumerates asset fields after JSON load, calls `AssetService::Request(assetId)` for each, and the realm tracks pending loads. Realm reports `kLoading` until all pending complete; `OnAssetLoaded` is called on the component as each finishes. EntityModule's DoStart returns kLoading-then-kReady mirrors this. |
| 7 | Two-pass blueprint resolution | EntityRef references in blueprints can be forward references (entity B referenced before it's declared). How? | Pass 1: instantiate all entities and components with EntityRef fields set to `Entity::Invalid()`. Pass 2: walk the blueprint's references block (Decision 11's "separate references block") and patch each named reference. Pass 3: validate every required EntityRef is non-null; missing references are fatal load errors. |
| 8 | DoUpdate ordering and Mailbox.Drain | When does mailbox delivery happen relative to DoUpdate? | Frame timing per research summary: (1) frame start → process mailbox (drain last frame's queued messages, deliver to subscribers); (2) Realm::Update(dt) → walk DoUpdate components; (3) gameplay update outside Realm → drives queries, sends messages; (4) Realm::EndOfFrame() → apply structural mutations, rebuild query caches, queue mailbox messages for next-frame drain. Mailbox is delivered at frame start, not in EndOfFrame, to give DoUpdate a chance to react to inbound messages. |
| 9 | Self vs Entity address kind | Why a separate Self kind? Sender is always known; couldn't gameplay just use Entity(senderHandle)? | Self is sugar over MakeEntityAddress(realm.GetSender()). It's worth being explicit because `Self` is the most common case (component sends to its own entity) and the encoding lets the router skip the lookup since the sender is part of the message dispatch context. Negligible cost win; readability is the real reason. Could be cut if it adds complexity — flag for review during implementation. |
| 10 | All address kind cost | Resolving "All" means iterating every subscriber for the type. Is this expensive? | At v1 scale (<1000 entities × <10 subscribers per type) it's fine. If a frame has 100 broadcasts, that's 100×10 = 1000 ops. The bigger risk is overflowing `SubscriberSet`'s 64-element capacity (which DiaMailbox sized as a placeholder, see DiaMailbox AI Q14). DiaEntity is the consumer that should resize SubscriberSet to `kMaxEntitiesPerRealm`. |
| 11 | Hierarchy destroy-cascade | If destroying the parent destroys the subtree, what about external EntityRefs into the subtree? | They become invalid (handle generation mismatches). Holders that read them get nullptr from `GetComponent<TComponent>()`. Components that need cleanup-on-target-destroy should subscribe to a `EntityDestroyedMessage` via the mailbox (a message DiaEntity emits during the destroy pass). Open question: should this message be in v1 or deferred? **v1.** It's needed for any non-trivial gameplay. |
| 12 | EntityDestroyedMessage | What's the subscription model — every component that holds a reference subscribes? | Yes. Components opt in (override `OnAttach` to subscribe; override `OnDetach` to unsubscribe). The message carries the destroyed entity's handle. Subscription is filtered server-side by the entity router: only subscribers that registered via "subscribe to destroy events for entity X" get the message. ComponentType-flavoured subscriptions (subscribe to all destroys) are also supported. |
| 13 | Mailbox lifetime per realm | Realm owns one Mailbox. What if a consumer wants their own mailbox for a non-entity concern? | They construct their own. DiaMailbox's caller-owned model means a consumer in CluicheTest can have a stage-scoped Mailbox separate from the entity mailbox. DiaEntity owns one for entity routing; nothing prevents others. |
| 14 | IEntityInspectable WriteField type checking | `WriteField(entity, typeId, fieldName, Json::Value)` — what guards against type mismatches? | Reflection checks: lookup field by name, compare its `FieldType` to the JSON type, return false on mismatch and emit DIA_LOG_WARNING. No exceptions, no asserts (editor errors must not crash the runtime). Editor is expected to render correct widgets per FieldType, but a malformed write is recoverable. |
| 15 | Old IComponent migration | The two existing consumers (SkeletonComponent, StateMachineComponent) — what happens to them during the remove-old-icomponent feature? | They are deleted. DiaRig2D and DiaStateMachine continue to own their respective objects (Skeleton, StateMachine) directly — the system-owned-data principle. New DiaEntity components (`Rig2DComponent`, `StateMachineComponent` in Dia::Entity:: but living in their respective module folders) are created in their original modules later, after DiaEntity ships, as needed. The remove feature deletes; subsequent features in those modules re-add the component layer on top of DiaEntity. |
| 16 | Test consumer for DiaEntity | How is DiaEntity exercised before any real gameplay system depends on it? | A `TestRealmComponent` and a small CluicheTest blueprint exercise the foundation, reflection, blueprint loading, references, hierarchy, queries, mailbox routing, and inspection. DummyStage is the natural host. Once that smoke test works, real component types (Rig2D, StateMachine, RigidBody2D) migrate over. |
| 17 | vcxproj placement | Where does `DiaEntity.vcxproj` live? | `Dia/DiaEntity/DiaEntity.vcxproj` — same pattern as every other Dia module. Added to `Cluiche.sln` under the `Dia` solution folder. |
| 18 | Schema version mismatch | Component reflection has `schemaVersion`. What happens on a mismatch? | Loader checks the version embedded in the blueprint JSON against the registered desc. Match → load fields normally. Mismatch with a registered migration function → run migration. Mismatch with no migration → fatal load error. v1 ships with no migrations; the field exists for forward compatibility. |
| 19 | Reflection field-name lookup cost | `WriteField` looks up by `const char* fieldName` linearly through the field array. OK? | Fine. Field counts per component are <20; linear scan is faster than hash lookup at that size. Editor writes are user-driven, not in any hot path. |
| 20 | Live structural edit pipeline preservation | SD-ENT-016 says "the mutation pipeline must remain capable of routing editor-originated commands when (c) is built." What does that actually mean for the design? | The end-of-frame mutation queue accepts mutations from any source (gameplay code, components, future editor). The queue's API is `QueueX(...)` — public on Realm, callable from anywhere. When (c) is built, the editor calls the same `QueueX` methods through `IEntityInspectable` extensions. No pipeline rework needed. The constraint is: don't add a "gameplay-only mutation path" that bypasses the queue. |
| 21 | Generic entity router test | How do you unit-test `EntityRouter::Resolve` without a real Realm? | Build a tiny TestRealm with two entities, two component types, four subscribers. Send messages with each address kind and assert the correct subscriber set comes out. Tests live in `Cluiche/Tests/GoogleTests/Entity/`. |

## Status

`Approved`
