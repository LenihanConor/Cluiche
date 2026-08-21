# System Spec: DiaMessageBus

**Research:** @docs/research/gameplay_msg_bus/summary.md

## Parent Application
@docs/specs/applications/dia/dia.md

**Gameplay Domains:** gameplay, ai, entity

## Purpose

DiaMessageBus is the standard typed message routing layer for gameplay systems and entity components in Cluiche. It sits one layer above DiaMailbox — owning a `Mailbox` instance, registering routers, and running the two-pass per-tick flush.

**This is a declaration-first system.** The primary workflow is design-time: a developer defines message types and their producer/consumer connections in a `.diagamemessages` document, then `dia codegen messages` generates the C++ structs *and* the registration wiring from that document. The developer builds the system out from there by filling in the generated handler slots. The document is the single source of truth for the connection graph — the runtime wiring is generated from it, so what the offline schema browser shows is, by construction, what the game does.

Without DiaMessageBus, gameplay systems couple directly to each other through ad-hoc observer subscriptions that are invisible to tooling, hand-wired, and unscalable. DiaMessageBus replaces that pattern with a document-driven routing table: you design the graph in a doc, generate the wiring, and the bus routes between systems at runtime.

The bus supports four communication patterns via `Address` choice alone:
- **System → System**: `Address{ kBroadcastRouterId, 0 }` — BroadcastRouter fans out to all subscribers
- **Entity → Entity**: `Address{ kEntityRouterId, handle.bits }` — EntityRouter delivers to target entity's components
- **System → Entity**: `Address{ kEntityRouterId, handle.bits }` — same EntityRouter, sender identity irrelevant
- **Entity → System**: `Address{ kBroadcastRouterId, 0 }` — same BroadcastRouter, sender identity irrelevant

Sender identity is never part of routing. Only the `Address` determines receivers.

**Dependency chain:**
`DiaMessageBus → DiaMailbox → DiaCore`
`DiaMessageBus → DiaStreams (ServiceStream for ledger export to editor)`
`DiaMessageBus → DiaObservation (DIA_LOG_*, DIA_TRACE_ZONE, DIA_METRIC_INCREMENT)`

## Responsibilities

- Own one `Dia::Mailbox::Mailbox` instance per `MessageBusModule` (sim-thread-scoped)
- Register `BroadcastRouter` (routerId: `"broadcast"`) — resolves all type-subscribed systems and components
- Provide registration point for `EntityRouter` (owned by diaentitytemplate, registered at stage start)
- Provide type registration: `RegisterType<T, kCapacity>(OverflowPolicy)` — registers T's typed queue with the owned Mailbox (compile-time capacity, matching `Mailbox::RegisterType`); required before Post/Subscribe for type T. Generated `RegisterMessages()` calls this per message type with `kCapacity` as a template argument.
- Provide `Bus::Post<T>(Address, const T&)` — O(1) enqueue into the Mailbox ring for type T
- Run two-pass flush per sim tick:
  - **Pre-Primary**: drain all flush adapters (physics, input) into the bus
  - **Primary pass**: drain all type queues, resolve subscribers via router, call registered handlers
  - **Reaction pass**: drain reaction-tagged messages enqueued during Primary handlers; handlers may not re-enqueue
- Provide producer registration: `RegisterProducer<T>(StringCRC producerId)` — records which system/component produces type T. Emitted by codegen from the doc's `producers` array.
- Provide consumer registration: `Subscribe<T>(StringCRC subscriberId, Handler)` — records subscriber and installs dispatch callback; returns `BusSubscriptionHandle` (RAII unsubscribe). Emitted by codegen from the doc's `consumers` array, bound to a developer-supplied handler slot.
- Provide `IFlushAdapter` interface — physics adapter, input adapter implement this; bus calls `Flush()` in pre-Primary step
- Maintain a **last-tick ledger**: during each flush, tally per-type `count`/`deliveries`/`routerId`/`pass` and `droppedCount`; expose the completed tick via `GetLastTickLedger()` (double-buffered). This is the debugging surface for the build-it-out loop and the assertion surface for core-bus tests.
- Register `DiaMessageBusModule` as an `IModule` on SimPU; flush is called from module `Update()`
- Support the `dia codegen messages` toolchain: the bus API surface (`RegisterType`, `RegisterProducer`, `Subscribe`) is the target that generated `RegisterMessages(Bus&, Handlers&)` functions call — see Feature: diagamemessages-format
- Provide `dia.messagebus.architecture.module.md` YAML module documentation
- Ship as `Dia/DiaMessageBus/DiaMessageBus.vcxproj` static library registered in `Cluiche.sln`

### Deferred to "Live Tooling" follow-on (not in this system's scope)

The following observe a *running* game and belong to the Inspector / Visual Debugger tiers. They are deliberately deferred — this system delivers the declaration-first authoring loop (B) first, plus the last-tick ledger. They return when live debugging (A) is prioritized:

- **Ledger history ring buffer** (`kLedgerCapacity`-tick ring over `LedgerSnapshot`) + **`ServiceStream<LedgerSnapshot>` cross-PU export** — the last-tick tally is in scope; only the history and the editor-facing export are deferred.
- **`MessageBusDebugDomain`** (`IDebugDomain`) — in-game overlay showing live schema / flow / history.
- **Live Inspector panel** (CluicheEditor connected to a running game via `DiaDebugServer`).

## Non-Responsibilities

- **Message schema definitions.** `DiaGameplayMessages` (or game-equivalent) lives in the application layer — game-specific content, not engine infrastructure.
- **EntityRouter implementation.** EntityRouter is owned by diaentitytemplate and registered with the bus at stage start. DiaMessageBus only holds the registration point.
- **Cross-PU messaging.** SimPU → RenderPU or MainPU communication remains the stream system's job. Flush adapters are the seam, not the bus itself.
- **Thread safety.** Bus is single-threaded (sim thread only), matching DiaMailbox's SD-MBX-007 decision.
- **Message persistence across stage transitions.** The bus dies with the stage's `MessageBusModule`.
- **Live-observation tooling (A).** The ledger *history ring buffer*, `ServiceStream` export, `MessageBusDebugDomain` in-game overlay, and live CluicheEditor Inspector are deferred to a "Live Tooling" follow-on. (The *last-tick* ledger is in scope — see Responsibilities.) See the Deferred subsection. The three tool tiers are distinct: Editor (offline, in scope), Inspector (live via DiaDebugServer, deferred), Visual Debugger (in-game overlay, deferred).
- **EventDispatcher.** `Dia::Core::Events::EventDispatcher` is removed as part of this system (see Feature: eventdispatcher-removal). DiaInput migrates to `InputBusAdapter`.

## Public Interfaces

### MessageBusModule

```cpp
namespace Dia::MessageBus {

    // RAII handle returned by Subscribe. Unsubscribes on destruction.
    class BusSubscriptionHandle;

    // Implement and register to have Flush() called in pre-Primary step each tick.
    class IFlushAdapter {
    public:
        virtual ~IFlushAdapter() = default;
        virtual void Flush(Bus& bus) = 0;  // drain internal events into bus via Post<T>
    };

    class Bus {
    public:
        // --- Registration (call during module OnStart / OnConnectStreams) ---
        // Typically invoked by the generated RegisterMessages(Bus&, Handlers&) function.

        // Register T's typed queue with the owned Mailbox. Required before Post/Subscribe<T>.
        // kCapacity = ring size — COMPILE-TIME, matching Mailbox::RegisterType<T, kCapacity>
        // (Mailbox allocates kCapacity*stride at registration). policy = overflow behaviour.
        // Both come from the message's .diagamemessages entry; codegen emits kCapacity as a
        // template argument. Returns false if already registered or registry full.
        template <class T, uint32_t kCapacity>
        bool RegisterType(Dia::Mailbox::OverflowPolicy policy);

        // Record this system/component as a producer of type T. Graph metadata — no routing effect.
        template <class T>
        void RegisterProducer(Dia::Core::StringCRC producerId);

        // Subscribe to messages of type T. handler called during flush passes.
        // pass controls which pass the handler runs in (Primary or Reaction).
        template <class T>
        BusSubscriptionHandle Subscribe(Dia::Core::StringCRC subscriberId,
                                        std::function<void(const T&)> handler,
                                        Pass pass = Pass::Primary);

        // Register a flush adapter (e.g. PhysicsBusAdapter, InputBusAdapter).
        // Adapters are flushed in registration order before the Primary pass.
        void RegisterFlushAdapter(IFlushAdapter* adapter);  // caller keeps alive

        // Register a router (e.g. EntityRouter from diaentitytemplate).
        // Delegates to the owned Mailbox::RegisterRouter.
        void RegisterRouter(Dia::Mailbox::IMailboxRouter* router);

        // --- Posting (call from anywhere on sim thread during Update) ---

        // Enqueue a message. addr determines which router resolves delivery.
        // Returns false only if the type is unregistered (debug assert in dev).
        template <class T>
        bool Post(const Dia::Mailbox::Address& addr, const T& message);

        // Convenience: broadcast to all subscribers of type T.
        template <class T>
        bool Broadcast(const T& message);

        // --- Ledger (diagnostic; in scope) ---

        // Read-only snapshot of the LAST COMPLETED tick's message flow.
        // In scope: last-tick tally only. The history ring buffer and the
        // ServiceStream<LedgerSnapshot> cross-PU export are deferred (A).
        const LedgerSnapshot& GetLastTickLedger() const;

        // --- Router address constants ---
        static constexpr Dia::Core::StringCRC kBroadcastRouterId{ "broadcast" };
        static constexpr Dia::Core::StringCRC kEntityRouterId{ "entity" };
    };

    // Pass tag for Subscribe — controls which flush pass the handler runs in.
    enum class Pass : uint8_t { Primary, Reaction };

}
```

### MessageBusModule

```cpp
namespace Dia::MessageBus {

    // IModule on SimPU. Owns the Bus instance and drives the two-pass flush.
    // Stages create this module; diaentitytemplate's EntityModule registers its router here.
    class MessageBusModule : public Dia::Application::IModule {
    public:
        Bus& GetBus();
        const Bus& GetBus() const;

        // IModule overrides
        void OnStart() override;
        void Update(float dt) override;   // runs pre-Primary, Primary, Reaction
        void OnStop() override;
    };

}
```

### LedgerSnapshot _(last-tick: in scope; history ring buffer + cross-PU export: deferred)_

The **last-tick tally is in this system's scope.** `GetLastTickLedger()` returns a single `LedgerSnapshot` describing what flowed during the just-completed tick — filled during the flush loop, double-buffered (build current, expose previous). It pays off immediately: while building a system out from its `.diagamemessages` doc, the doc says what *should* connect and the ledger says what *did* flow; and core-bus tests can assert on delivery counts through it instead of mocking internals.

Deferred to the Live Tooling follow-on (A): the **history ring buffer** (`kLedgerCapacity` ticks), the **`ServiceStream<LedgerSnapshot>` cross-PU export** to the editor, and the `MessageBusDebugDomain` overlay that consume it.

```cpp
namespace Dia::MessageBus {

    struct LedgerMessageEntry {              // in scope
        Dia::Core::StringCRC  typeId;       // message type (kTypeId — display/graph, not routing)
        Dia::Core::StringCRC  routerId;     // which router resolved it
        uint32_t              count;        // messages of this type this tick
        uint32_t              deliveries;   // total handler calls this tick
        Pass                  pass;         // Primary or Reaction
    };

    struct LedgerSnapshot {                   // in scope — one per completed tick
        uint64_t                                         tickIndex;
        uint64_t                                         timestampUs;   // wall-clock at flush time
        Dia::Core::Containers::DynamicArrayC<LedgerMessageEntry, 64> entries;
        uint32_t                                         droppedCount;  // overflow drops this tick
    };

    // Deferred (A): history ring buffer over LedgerSnapshot + ServiceStream export.
    static constexpr uint32_t kLedgerCapacity = 3600;  // ticks; ~60s at 60hz — ring buffer only
}
```

### IFlushAdapter (example: PhysicsBusAdapter)

```cpp
// Lives in DiaRigidBody2D — reads physics EventStream, posts to bus.
class PhysicsBusAdapter : public Dia::MessageBus::IFlushAdapter {
public:
    void Flush(Dia::MessageBus::Bus& bus) override {
        // drain physics EventStream into bus
        mCollisionStream.Consume(mReaderIdx, mScratch);
        for (auto& ev : mScratch) {
            bus.Post<CollisionEvent>(
                { Bus::kEntityRouterId, ev.bodyA.bits() },
                { ev.bodyB, ev.impulse, ev.normal });
        }
    }
};
```

## Features

### In scope — declaration-first authoring loop (B)

| Feature | Description | Spec | Status |
|---------|-------------|------|--------|
| module-and-build | `DiaMessageBus.vcxproj`, `Cluiche.sln` registration, `dia.messagebus.architecture.module.md` YAML | [module-and-build.md](module-and-build.md) | Draft |
| core-bus | `Bus` class, `MessageBusModule`, `BroadcastRouter`, two-pass flush, `RegisterType<T>`, `Post<T>`, `Broadcast<T>`, `Subscribe<T>`, `RegisterProducer<T>` | [core-bus.md](core-bus.md) | Draft |
| diagamemessages-format | `.diagamemessages` JSON IDL — source of truth for message types; `dia codegen messages` generates C++ structs **+ registration wiring** (`RegisterType`/`RegisterProducer`/`Subscribe`) + `Handlers` binding struct; `dia validate manifest` support; lives alongside C++ source, not deployed with game | [diagamemessages-format.md](diagamemessages-format.md) | Draft |
| schema-browser | CluicheEditor offline panel: graph view, list view, payload/field inspector, structural duplicate analysis; reads `.diagamemessages` file from disk — no live game connection; mockup at `docs/research/gameplay_msg_bus/inspector-mockup.html` | [schema-browser.md](schema-browser.md) | Draft |
| entity-router-registration | `RegisterRouter` point for diaentitytemplate's `EntityRouter`; `kEntityRouterId` address constant; integration test with entity-addressed `HitEvent` | [entity-router-registration.md](entity-router-registration.md) | Draft |
| frame-ledger | Ledger **history ring buffer** (`kLedgerCapacity` ticks) over `LedgerSnapshot` + Release-build strip; feeds the visual debugger's History tab. (Last-tick tally is in core-bus; `ServiceStream` export stays deferred.) | [frame-ledger.md](frame-ledger.md) | Draft |
| visual-debugger | `MessageBusDebugDomain` — in-game `IDebugDomain` overlay: Schema / Live / History tabs; reads the bus + ledger in-process; guarded `#ifdef DIA_DEBUG` | [visual-debugger.md](visual-debugger.md) | Draft |
| messagebus-test-stage | CluicheTest E2E stage proving the doc → codegen → build loop end to end | [messagebus-test-stage spec](../../../../../cluichetest/systems/teststages/messagebus-test-stage.md) | Approved |

### Independent cleanup (in scope, unsequenced)

| Feature | Description | Spec | Status |
|---------|-------------|------|--------|
| eventdispatcher-removal | Remove `Dia::Core::Events::EventDispatcher`, `EventQueue`, `Delegate` from DiaCore; migrate `DiaInput::InputSourceManager::UpdateModern` and `LegacyEventConverter` to `InputBusAdapter`; delete dead test files | [eventdispatcher-removal.md](eventdispatcher-removal.md) | Draft |
| flush-adapters | `IFlushAdapter` interface; `PhysicsBusAdapter` (DiaRigidBody2D); `InputBusAdapter` (DiaInput) — brings physics/input events into the bus | [flush-adapters.md](flush-adapters.md) | Draft |

### Deferred — Inspector tier (needs a running-game connection)

Only the cross-PU / editor-connected pieces remain deferred. The in-game visual debugger is now in scope (above).

| Feature | Description | Spec | Status |
|---------|-------------|------|--------|
| ledger-servicestream-export | `ServiceStream<LedgerSnapshot>` cross-PU export so an out-of-process editor can read the ledger | _(folded into frame-ledger spec, marked deferred)_ | Deferred |
| live-inspector | CluicheEditor panel connected to a running game via `DiaDebugServer`; live flow over the designed graph | _(not yet specced)_ | Deferred |

## Dependencies on Other Systems

**Required:**
- **DiaMailbox** — owns the `Mailbox` instance; `Address`, `IMailboxRouter`, `SubscriptionHandle` types
- **DiaCore** — `StringCRC` (router IDs, producer/subscriber IDs), `DynamicArrayC` (ledger entries, handler tables), `DIA_ASSERT`, `DIA_LOG_WARNING`
- **DiaApplicationFlow** — `IModule` base class; `MessageBusModule` registered on SimPU; `ServiceStream` for ledger export
- **DiaStreams** — `ServiceStreamStore<LedgerSnapshot>` for editor-side ledger reads
- **DiaObservation** — `DIA_LOG_*` for unhandled messages / drops; `DIA_TRACE_ZONE` for flush pass instrumentation; `DIA_METRIC_INCREMENT` for `dia.msgbus.{posted,delivered,dropped}` counters

**Explicitly excluded:**
- **diaentitytemplate** — EntityRouter is owned by diaentitytemplate and registered with the bus at stage start. DiaMessageBus depends on none of it.
- **DiaGameplayMessages** (or game equivalent) — message schema definitions live in the application layer; DiaMessageBus has no knowledge of concrete message types.
- **DiaEditor / DiaDebugServer** — the schema browser reads `.diagamemessages` files from disk offline; no live connection to the bus is required. A future inspector panel would use DiaDebugServer, but that is out of scope.

**Dependents (once DiaMessageBus ships):**
- **diaentitytemplate** — registers its `EntityRouter` with the bus at stage start
- **DiaRigidBody2D** — provides `PhysicsBusAdapter` (owned by physics module, registered with bus)
- **DiaInput** — provides `InputBusAdapter` (replaces EventDispatcher usage)
- **Game application layer** — defines `DiaGameplayMessages` schema; gameplay systems (DamageSystem, HealthSystem, etc.) call `Bus::Post` and `Bus::Subscribe`

## Out of Scope

- **Message schema definitions.** Game-specific content; not an engine responsibility.
- **EntityRouter implementation.** Lives in diaentitytemplate.
- **Cross-PU / cross-thread messaging.** Handled by DiaStreams. Flush adapters bridge the boundary.
- **Live inspector UI.** Deferred to tooling v2. The ring buffer + `ServiceStream` seam is designed in.
- **Schema code-gen (DiaPython IDL).** Pulled into scope as `diagamemessages-format` feature — `dia codegen messages` generates C++ structs from `.diagamemessages` IDL files.
- **EventDispatcher.** Being removed, not replaced with a new version.

## Open Design Questions

All resolved before approval.

| # | Question | Resolution |
|---|----------|------------|
| 1 | Reaction pass re-entrancy — assert, drop, or queue to next Primary? | Assert in Debug with clear message; drop silently in Release. Silently queueing hides a design mistake. |
| 2 | Ledger ring buffer — sized in seconds or ticks? | Fixed compile-time `kLedgerCapacity = 3600` ticks. `LedgerSnapshot` carries `timestampUs` so the editor computes real wall-clock age correctly regardless of tick rate. No dynamic allocation. |
| 3 | DiaMailbox 16-router cap — sufficient? | Yes. Current plan uses 2 (BroadcastRouter + EntityRouter). Plausible future routers (TeamRouter, LayerRouter, ComponentTypeRouter) reach ~5 total. 16-pointer table costs 128 bytes per instance — no reason to reduce. Revisit if router count grows past 8. |

## Decisions

| ID | Decision | Rationale | Scope | Status | Binding |
|----|----------|-----------|-------|--------|---------|
| SD-MBX2-001 | Bus is sole subscriber — systems never subscribe to each other directly | Makes the routing table the single source of truth for the editor graph; eliminates invisible dependencies | All | Accepted | Yes |
| SD-MBX2-002 | Two named passes: Primary + Reaction; reaction handlers may not re-enqueue | Prevents infinite loops without a counter; enforces good message design discipline; self-documenting in the editor | Flush | Accepted | Yes |
| SD-MBX2-003 | Sender identity is irrelevant — only Address determines receivers | Decouples producers from routing semantics; enables system→entity and entity→system without special cases | All | Accepted | Yes |
| SD-MBX2-004 | In scope: last-tick ledger, the ledger history ring buffer, and the in-game `MessageBusDebugDomain` overlay. Deferred: the `ServiceStream<LedgerSnapshot>` cross-PU export and the live CluicheEditor Inspector. | The in-game visual debugger reads the ledger directly in-process, so it and the ring buffer it needs are in scope. The cross-PU export and the CluicheEditor Inspector require a running-game *connection* (Inspector tier) and are the genuinely separable, later work. | Ledger / Debugger | Accepted | Yes |
| SD-MBX2-005 | DiaGameplayMessages lives in application layer, not engine | Message schemas are game content; engine must not encode game-specific types | Schema | Accepted | Yes |
| SD-MBX2-006 | EventDispatcher removed from DiaCore in same spec | Eliminates a stranded messaging system with STL internals; DiaInput migrates to InputBusAdapter | DiaInput | Accepted | Yes |
| SD-MBX2-007 | Schema browser is offline/design-time; reads `.diagamemessages` file, not a live game connection | Editor tools work offline from static files. Inspector (live data via DiaDebugServer) is a separate tier and deferred. | Editor | Accepted | Yes |
| SD-MBX2-008 | Single-threaded (sim thread only) | Matches DiaMailbox SD-MBX-007; cross-PU traffic uses DiaStreams | All | Accepted | Yes |
| SD-MBX2-009 | **Declaration-first: `.diagamemessages` is the source of truth; codegen emits structs AND registration wiring** (`RegisterType`/`RegisterProducer`/`Subscribe`) | Kills connectivity drift by construction — the runtime graph is generated from the doc, so the offline schema browser is guaranteed to match the game. This is the system's central design principle. | All | Accepted | Yes |
| SD-MBX2-010 | Generated `Subscribe` calls bind to a developer-supplied `Handlers` struct of `std::function` slots; unbound slots assert at registration | Handlers must be instance methods (lambda captures `this`) to respect the no-singletons / no-service-locator rule. `std::function` matches the existing `Subscribe` signature. Assert-on-unbound fails loud, so "build it out from there" can't silently leave a hole. | Codegen | Accepted | Yes |
| SD-MBX2-011 | Three tool tiers are distinct and must not be conflated: Editor (offline, in scope), Inspector (live via DiaDebugServer, deferred), Visual Debugger (in-game overlay, deferred) | An editor works from files on disk; an inspector/debugger needs a running game. Collapsing them produced the original mislabeling of the schema browser as an inspector. | Tooling | Accepted | Yes |
| SD-MBX2-012 | Bus routes by `Mailbox::TypeKey<T>()` (`__FUNCSIG__` CRC), not by `T::kTypeId`; `kTypeId` is graph/display metadata only | The underlying Mailbox already keys typed queues by compiler signature. `kTypeId` (from the doc's `id`) exists so the ledger and editor can show human-readable type names; it is not the routing key. | All | Accepted | Yes |

**Status values:** `Proposed` · `Accepted` · `Rejected` · `Superseded`
**Binding:** `Yes` = enforced constraint on all features · `No` = guidance only

## Inherited Binding Decisions

| ID | Source | Decision | Implication |
|----|--------|----------|-------------|
| PD-001 | Platform | StringCRC for all IDs | Router IDs (`"broadcast"`, `"entity"`), producer IDs, and subscriber IDs are all `StringCRC`. No raw strings in public API. |
| PD-002 | Platform | ProcessingUnit/Phase/Module architecture | `MessageBusModule` is an `IModule` on SimPU. The two-pass flush runs in `Update()`. |
| PD-004 | Platform | No STL containers in public APIs | `LedgerSnapshot` uses `DynamicArrayC`. Handler table uses DiaCore containers. `std::function` for handlers is acceptable internally (not in public headers). |
| PD-005 | Platform | x64 only | `DiaMessageBus.vcxproj` targets x64 exclusively. |
| PD-007 | Platform | C++20 required | Compiled under `/std:c++20`; `concept MessageType` may enforce `kTypeId` presence on schema types. |
| PD-008 | Platform | Directory.Build.props owns OutDir/IntDir | `DiaMessageBus.vcxproj` must NOT override OutDir, IntDir, PlatformToolset, WindowsTargetPlatformVersion, or LanguageStandard. |
| AD-001 | Dia App | Module YAML frontmatter | Create `dia.messagebus.architecture.module.md` with public API, responsibilities, and dependency declarations. |
| AD-002 | Dia App | No STL in public APIs | Reinforces PD-004. |
| AD-003 | Dia App | Namespace `Dia::<Module>::` | All code in `Dia::MessageBus::` namespace. |

## Status

`Approved` — Ready to build.

**Plan:** [diamessagebus.plan.md](diamessagebus.plan.md) _(created at implementation start)_
