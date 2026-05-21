# System Spec: DiaMailbox

**Research:** @docs/research/entity_system/summary.md

## Parent Application
@docs/specs/applications/dia.md

## Purpose

DiaMailbox is a generic typed deferred messaging primitive for the Dia engine. Modules use it
to publish typed messages addressed to opaque destinations; subscribers drain the relevant
typed queues at a controlled point in their frame. A pluggable router system resolves
opaque addresses into concrete subscriber sets — DiaMailbox itself knows nothing about
entities, components, physics bodies, or any domain concept.

The immediate motivating consumer is DiaEntity (entity-to-entity, component-to-component,
broadcast messaging), but DiaMailbox is deliberately scoped one layer below entities so any
module-to-module, editor-to-game, or future system-keyed messaging can use the same
primitive. It is the typed-deferred-queue equivalent of HandlePool: a foundation building
block extracted from a known consumer because more consumers will follow.

The research locked four address kinds (Entity / All / ComponentType / Self — Decision 9 in
choose.md). After review, those four kinds are correctly the responsibility of DiaEntity's
**entity router**, not of DiaMailbox itself. DiaMailbox sees `(routerId, payload64)`; the
entity router decodes the payload into the four kinds. This refines D9 — the kinds still
exist, just one layer deeper.

**Dependency chain:**
`DiaMailbox → DiaCore (StringCRC, DynamicArrayC, Handle<T>, HandlePool<T>, DIA_ASSERT, DIA_LOG_WARNING)`

## Responsibilities

- Provide `Mailbox` — the per-instance container that owns typed message queues and registered routers
- Provide typed message registration: `Register<T, kCapacity>()` carves out a fixed-capacity ring buffer for messages of type `T` (compile-time capacity, matches SD-CORE-001)
- Provide typed publish: `Send<T>(Address, const T&)` — copies the message into the type's ring buffer with the supplied opaque address
- Provide typed drain: `Drain<T>(Visitor)` — calls `visitor(Address, const T&)` for every queued message of type `T`, then empties the ring; deterministic FIFO order within type
- Provide subscription registration with caller-managed lifetimes — `Subscribe<T>(SubscriberId)` returns a `SubscriptionHandle`; explicit `Unsubscribe(handle)` removes; mailbox destruction invalidates all outstanding handles
- Provide a router registration system — modules register an `IMailboxRouter` against a `StringCRC` router ID; routers resolve `Address::payload` to subscriber sets when domain consumers ask
- Provide `Address` — opaque value type (`{StringCRC routerId, uint64_t payload}`) that carries enough bits for any router's encoding (entity handles, type CRCs, broadcast tags) without DiaMailbox interpreting them
- Provide overflow policy — drop-oldest with debug warning is default; assert-on-overflow available per type at registration
- Ship as `Dia/DiaMailbox/DiaMailbox.vcxproj` static library registered in `Cluiche.sln`
- Provide `dia.mailbox.architecture.module.md` YAML module documentation

## Non-Responsibilities

- **Knowledge of entities, components, physics bodies, or any domain concept.** Routers live in their respective domain modules (e.g. `EntityRouter` in DiaEntity). DiaMailbox is a transport primitive only.
- **Address payload semantics.** `Address::payload` is a `uint64_t`; what those bits mean is a router's contract.
- **Cross-frame ordering guarantees beyond FIFO-within-type.** No total ordering across types. Cross-type ordering would require a global queue and merge logic out of scope for v1.
- **Thread safety.** Mailbox is single-threaded; concurrent `Send` / `Drain` is undefined. Cross-thread messaging (e.g. SimPU → RenderPU) is the existing stream system's job, not DiaMailbox's.
- **Persistence.** Messages are transient; nothing serialises across mailbox destruction.
- **Backpressure or flow control.** Senders cannot ask "is the queue near full." Overflow policy is per-type at registration.
- **Routing at send time.** `Send` is O(1) — it appends to the type's ring with the address attached. Resolution happens at drain time when a consumer asks "which subscribers match this address?"

## Public Interfaces

### Address

```cpp
namespace Dia::Mailbox {
    // Opaque destination. routerId names which router knows how to interpret payload.
    // payload is router-defined: entity handle bits, type CRC, broadcast tag, etc.
    struct Address {
        Dia::Core::StringCRC routerId;
        uint64_t             payload;

        bool operator==(const Address& rhs) const;
        bool operator!=(const Address& rhs) const;
    };
}
```

### Mailbox

```cpp
namespace Dia::Mailbox {
    // Caller-managed identity for subscribers. Common pattern: subscriber is
    // identified by a Handle<EntityType> reinterpreted to bits, or a StringCRC
    // module name. DiaMailbox treats it as opaque.
    struct SubscriberId {
        uint64_t value;
        bool operator==(const SubscriberId& rhs) const { return value == rhs.value; }
    };

    enum class OverflowPolicy : unsigned char {
        DropOldest, // ring overwrites oldest; emits DIA_LOG_WARNING
        Assert      // DIA_ASSERT on full; Send returns false in release
    };

    class SubscriptionHandle; // opaque; tied to the issuing Mailbox; invalidated on Mailbox destruction

    class Mailbox {
    public:
        Mailbox();
        ~Mailbox(); // destroys all queues, routers, and outstanding subscriptions

        Mailbox(const Mailbox&) = delete;
        Mailbox& operator=(const Mailbox&) = delete;

        // Register a typed queue. Must be called before Send<T> or Drain<T>.
        // Capacity is compile-time per SD-CORE-001 / DiaMailbox decisions.
        // Returns false if already registered for type T.
        template <class T, uint32_t kCapacity>
        bool RegisterType(OverflowPolicy policy = OverflowPolicy::DropOldest);

        // Append a message to type T's queue. addr is opaque; routers decide later.
        // Returns false if the type is unregistered or the queue is full under
        // OverflowPolicy::Assert.
        template <class T>
        bool Send(const Address& addr, const T& message);

        // Drain all queued messages of type T. visitor signature: void(const Address&, const T&).
        // After Drain returns, the queue is empty. FIFO order within the type.
        template <class T, class Visitor>
        void Drain(const Visitor& visitor);

        // Subscribe a SubscriberId to messages of type T. Caller holds the handle
        // and must Unsubscribe (or let the handle's RAII wrapper unsubscribe) before
        // the SubscriberId becomes invalid. Mailbox destruction invalidates all handles.
        template <class T>
        SubscriptionHandle Subscribe(SubscriberId subscriber);

        // Explicit unsubscribe. Idempotent — second call is a no-op.
        void Unsubscribe(SubscriptionHandle handle);

        // --- Router system ---

        // Register a router for a routerId. Mailbox does not own the router pointer
        // (caller must keep it alive). Returns false if routerId already registered.
        bool RegisterRouter(IMailboxRouter* router);

        // Look up a router by ID. Returns nullptr if unregistered.
        IMailboxRouter* GetRouter(Dia::Core::StringCRC routerId);

        // Resolve an address against its router, filling matched subscribers.
        // Returns false if routerId is unregistered. Domain consumers (e.g. DiaEntity's
        // delivery pass) call this; DiaMailbox does not invoke routers automatically.
        template <class T>
        bool Resolve(const Address& addr, SubscriberSet& outMatched);
    };
}
```

### IMailboxRouter

```cpp
namespace Dia::Mailbox {
    // Resolves an address payload to the set of subscribers that should receive a message
    // sent to that address. Routers live in domain modules (EntityRouter in DiaEntity, etc.).
    class IMailboxRouter {
    public:
        virtual ~IMailboxRouter() = default;

        virtual Dia::Core::StringCRC GetRouterId() const = 0;

        // Given an address (the routerId is guaranteed to match this router) and the
        // current set of subscribers for the message type, fill outMatched with those
        // that should receive this message. liveSubscribers and outMatched are caller-
        // provided fixed-capacity arrays.
        virtual void Resolve(const Address& addr,
                             const SubscriberSet& liveSubscribers,
                             SubscriberSet& outMatched) = 0;
    };

    // Caller-provided fixed-capacity subscriber list. Concrete capacity decided per
    // call site by the consumer module. DiaMailbox provides the type alias only.
    using SubscriberSet = Dia::Core::Containers::DynamicArrayC<SubscriberId, /*cap=*/64>;
}
```

The `SubscriberSet` capacity (64) is a placeholder — the concrete type lives in DiaMailbox
but the chosen number gets revisited as soon as a real consumer (DiaEntity) sizes its
working buffers. v1 assumption: <64 subscribers per resolution call.

## Features

| Feature | Description | Spec | Status |
|---------|-------------|------|--------|
| address-and-types | `Address` value type, `SubscriberId`, `SubscriptionHandle`, `SubscriberSet` alias, `OverflowPolicy` enum, public types only | [address-and-types.md](../../features/dia/diamailbox/address-and-types.md) | Approved |
| typed-queue | `Mailbox::RegisterType`, `Send`, `Drain` — per-type ring buffers with compile-time capacity and overflow policy | [typed-queue.md](../../features/dia/diamailbox/typed-queue.md) | Approved |
| subscriptions | `Mailbox::Subscribe` / `Unsubscribe`, `SubscriptionHandle` lifetime tied to issuing Mailbox | [subscriptions.md](../../features/dia/diamailbox/subscriptions.md) | Approved |
| routers | `IMailboxRouter` interface, `RegisterRouter`, `GetRouter`, `Resolve` typed wrapper; `MockRouter` + `MailboxFixture` test utilities | [routers.md](../../features/dia/diamailbox/routers.md) | Approved |
| module-and-build | `DiaMailbox.vcxproj`, registration in `Cluiche.sln`, `dia.mailbox.architecture.module.md` YAML | [module-and-build.md](../../features/dia/diamailbox/module-and-build.md) | Approved |

## Dependencies on Other Systems

**Required:**
- **DiaCore** — `StringCRC` (router IDs), `DynamicArrayC` (ring buffers, subscriber sets), `Handle<T>` and `HandlePool<T>` (subscription handle storage), `DIA_ASSERT`, `DIA_LOG_WARNING`

**Explicitly excluded:**
- **DiaEntity** — DiaMailbox is a foundation layer for DiaEntity, not the other way around. The entity router is owned by DiaEntity.
- **DiaApplicationFlow** — DiaMailbox does not know about stages, modules, or PUs. Consumers that want stage-scoped mailboxes own a `Mailbox` from a stage-scoped module and let it die with the module.
- Any domain system (DiaRigidBody2D, DiaGraphics, DiaRig2D, etc.) — DiaMailbox is a primitive; domain systems may consume it but DiaMailbox depends on none of them.

**Dependents (once DiaMailbox ships):**
- **DiaEntity** — owns the entity router; entity-to-entity messaging routes through DiaMailbox
- **Future cross-module messaging** — editor → game runtime, debug commands, ad-hoc system-to-system events

## Out of Scope

- **Knowing about entities or any domain.** Lives one layer below.
- **Cross-thread messaging.** SimPU → RenderPU goes through the existing stream system in DiaApplicationFlow. Not DiaMailbox's job.
- **Cross-mailbox messaging.** A Mailbox is a self-contained universe. Multi-mailbox topologies (federation, hierarchies) are not in v1.
- **Schema migration for messages.** Messages are transient and live one frame; no on-disk format means no migration concern.
- **Wire / network transport.** A mailbox does not serialise messages to bytes. Networking layers can build on top, but DiaMailbox itself is in-process only.
- **Reflection of message types.** Senders and receivers must already share the C++ type. No introspection or stringly-typed lookup.
- **Cross-frame queueing across stage transitions.** Mailbox dies with its owner; messages don't survive.

## Decisions

| ID | Decision | Rationale | Scope | Status | Binding |
|----|----------|-----------|-------|--------|---------|
| SD-MBX-001 | Address is `(StringCRC routerId, uint64_t payload)` — opaque to DiaMailbox | Decouples DiaMailbox from any domain. Routers in domain modules interpret payload. The four kinds locked in research D9 (Entity/All/ComponentType/Self) live in the entity router's payload encoding, not in DiaMailbox. | All | Accepted | Yes |
| SD-MBX-002 | Per-type ring buffers with compile-time capacity (`Register<T, kCap>()`) | Matches SD-CORE-001 fixed-capacity convention; matches HandlePool's compile-time choice. Bounded memory, deterministic layout. | Typed queues | Accepted | Yes |
| SD-MBX-003 | Delivery is polled — `Drain<T>(visitor)` — not callback-on-send | Caller controls delivery timing; supports the end-of-frame-batch model the entity research locked. Callbacks would couple delivery timing to send timing. | Delivery | Accepted | Yes |
| SD-MBX-004 | Subscription lifetime: caller manages, mailbox lifetime is the upper bound | Generic primitive cannot know caller's scoping rules. Stage-scoped use falls out for free when consumer owns the Mailbox in a stage-scoped module — destroying the module destroys the mailbox destroys the subscriptions. | Subscriptions | Accepted | Yes |
| SD-MBX-005 | Routers register against a `StringCRC` ID, not a type | Multiple routers may resolve "entity-flavoured" addresses (e.g. live-game entity router + editor preview entity router). Type-based registration would force one router per address kind. CRC IDs scale with arbitrary domain growth. | Routing | Accepted | Yes |
| SD-MBX-006 | Default overflow policy is DropOldest with `DIA_LOG_WARNING`; assert is opt-in per type | A robust message bus drops stale data rather than crashing the frame. Critical messages (e.g. lifecycle events that must not be lost) opt into Assert at registration. | Overflow | Accepted | Yes |
| SD-MBX-007 | Single-threaded — no internal locking | Per-PU mailbox is the expected model. Cross-PU messaging is already the stream system's domain. Adding locks is pure cost for the single-threaded usage. | All | Accepted | Yes |
| SD-MBX-008 | Mailbox is non-copyable, non-movable | Owns inline ring storage and outstanding subscription handles tied to its address. Movability would require remapping all outstanding handles — needless for v1. | All | Accepted | Yes |
| SD-MBX-009 | Namespace is `Dia::Mailbox::` | Consistent with `Dia::<Module>::` (AD-003). | All | Accepted | Yes |
| SD-MBX-010 | `Resolve` is called by domain consumers, not internally during `Send` | Send is O(1) append. Routing happens at the consumer's chosen point (e.g. drain time, end-of-frame). This keeps Send free of router cost and keeps routing latency under the consumer's control. | Routing | Accepted | Yes |

**Status values:** `Proposed` · `Accepted` · `Rejected` · `Superseded`
**Binding:** `Yes` = enforced constraint on all features in this system · `No` = guidance only

## Inherited Binding Decisions

| ID | Source | Decision | Implication for this system |
|----|--------|----------|----------------------------|
| PD-001 | Platform | StringCRC for all entity/component IDs | Router IDs are `StringCRC`; subscriber identity is opaque (`SubscriberId`) but DiaEntity will populate it from `Handle<Entity>` bits — DiaMailbox itself uses no raw strings. |
| PD-004 | Platform | No STL containers in public APIs | Ring buffers are `DynamicArrayC` or hand-rolled fixed arrays; subscriber sets are `DynamicArrayC<SubscriberId, kCap>`. No STL types in any signature. |
| PD-005 | Platform | x64 only | `DiaMailbox.vcxproj` targets x64 exclusively. |
| PD-006 | Platform | Visual Studio project files are source of truth | `DiaMailbox.vcxproj` and `.vcxproj.filters` created and manually maintained. |
| PD-007 | Platform | C++20 required | Compiled under `/std:c++20`; uses templates and concepts where appropriate. |
| PD-008 | Platform | Directory.Build.props owns OutDir/IntDir/toolchain | `DiaMailbox.vcxproj` must NOT override OutDir, IntDir, PlatformToolset, WindowsTargetPlatformVersion, or LanguageStandard. |
| AD-001 | Dia App | Module system with YAML frontmatter | Create `dia.mailbox.architecture.module.md` with public API, responsibilities, and dependency declarations. |
| AD-002 | Dia App | No STL containers in public APIs | Reinforces PD-004. |
| AD-003 | Dia App | Namespace `Dia::<Module>::` | All code in `Dia::Mailbox::` namespace. |

PD-002, PD-003, AD-004, AD-005 (ProcessingUnit/component-based architecture decisions) are
not directly applicable: DiaMailbox is a primitive library, not a runtime framework or
component system. It composes into stage-scoped consumers via ownership, not inheritance.

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Address payload size | `uint64_t` payload — enough for any foreseeable router? | Yes. An entity handle is 64 bits (32 index + 32 generation, exactly fits). A `StringCRC` is 32 bits with 32 bits to spare for tag/flag. Future routers (physics body handle, asset handle) are all <= 64 bits. If a router ever needs more, payload can carry a pointer to a router-owned side-channel struct. |
| 2 | Address routerId required | Every address must carry a routerId. What about "default" or "unrouted" messages? | Reserve `StringCRC{}` (zero CRC) as `kNullRouter` — `Send` with a null router still queues the message; consumers calling `Resolve` on a null router get an empty subscriber set, meaning broadcast-to-no-one. Drain still works for use cases where the consumer treats the queue as a typed deferred buffer rather than a routed mailbox. |
| 3 | Subscription handle storage | What does `SubscriptionHandle` actually look like internally? | `SubscriptionHandle = Handle<Subscription>` where `Subscription` is a slot in a `HandlePool<Subscription, kMaxSubs>` owned by the Mailbox. Generation tracking handles double-unsubscribe and "is my subscription still alive" cleanly. This is a concrete reuse of the HandlePool feature — proves the layering. |
| 4 | Drain ordering across types | If two systems publish messages of types A and B in interleaved order, then a consumer drains A then B, ordering across A/B is lost. Acceptable? | Yes for v1. FIFO-within-type covers the common cases (gameplay events of the same kind). Cross-type ordering is a different feature (an event log) and would require a global queue. Defer until a real use case appears. |
| 5 | Router pointer ownership | `RegisterRouter(IMailboxRouter*)` — Mailbox stores raw pointer. Who owns lifetime? | Caller. The router is typically owned by a domain module (e.g. DiaEntity owns its `EntityRouter` as a member). The module unregisters before destruction. DiaMailbox does not own routers — that would couple lifetimes incorrectly (e.g. mailbox might outlive the entity world). |
| 6 | Overflow visibility | DropOldest emits `DIA_LOG_WARNING` per drop. What if a queue is undersized and drops thousands per frame? | Log spam risk is real. v1 logs once per frame per type (a per-type "drops since last frame" counter logged on Drain). If a queue persistently overflows, the type was registered with too-low capacity — fix by raising capacity, not by silencing the warning. |
| 7 | Multiple Mailbox instances | Is one Mailbox per stage the expected model, or one per consumer? | One per consumer-scope is the expected model. DiaEntity's EntityModule owns one. Editor → game cross-mailbox traffic is two separate mailboxes; messages don't auto-flow between them. If we ever want federation, that's a wrapper system. |
| 8 | Resolve template | `Resolve<T>` is templated, but routing is type-agnostic — why? | Resolve<T> uses the type to pick the right subscriber list (subscriptions are per-type). The router itself doesn't see T — it just resolves an address against a given subscriber set. The template is for indexing into the right typed substructure inside Mailbox, not for the router's contract. |
| 9 | Send during Drain | What happens if a Drain visitor calls `Send<T>` for the same type T being drained? | Newly-sent messages are NOT visited in the current Drain pass — they sit in the ring for the next Drain. Documented behaviour. The alternative (drain-until-empty including new arrivals) risks infinite loops if a visitor unconditionally re-sends, and complicates the per-type "drops since drain" accounting. |
| 10 | Router not registered | `Send` to an address whose `routerId` has no registered router. What happens? | Send still succeeds (message goes into the type's ring with the address attached). The unregistered router is discovered at `Resolve` time, where it returns `false` and emits `DIA_LOG_WARNING`. This decouples Send from router lifecycles — registering routers later in the frame after early Sends is OK. |
| 11 | Polling vs. push consumer pattern | Polled drain matches the entity research's end-of-frame model. Are there consumers that genuinely want push? | Editor live-edit might want push (UI updates immediately on game-side state changes). v1: editor does its own end-of-tick Drain and converts to push at the editor boundary. Reconsider only if a real consumer surfaces; adding callbacks later is non-breaking. |
| 12 | Entity Router refinement of Decision 9 | The research's Decision 9 listed Entity/All/ComponentType/Self as DiaMailbox's address kinds. This spec moves them down into the entity router. Is this a contradiction? | No — it's a refinement. The four kinds still exist; they're encoded in the entity router's `payload` interpretation. DiaMailbox stayed generic per the user's "must be generic" requirement; the entity-flavoured addressing semantics belong in DiaEntity. The research summary will be updated to reflect this layering. |
| 13 | vcxproj placement | Where does `DiaMailbox.vcxproj` live? | `Dia/DiaMailbox/DiaMailbox.vcxproj` — same pattern as every other Dia module. Added to `Cluiche.sln` under the `Dia` solution folder. |
| 14 | SubscriberSet capacity | `using SubscriberSet = DynamicArrayC<SubscriberId, 64>` is a placeholder. When does this get sized properly? | Once DiaEntity is specced, the capacity is sized to `kMaxEntities` (or whatever DiaEntity caps a stage at) so a broadcast can fan out to every entity in the worst case. Updating this is a compile-time constant change with no API churn. |

## Status

`Approved` — Implementation in progress.

**Plan:** [diamailbox.plan.md](diamailbox.plan.md)
