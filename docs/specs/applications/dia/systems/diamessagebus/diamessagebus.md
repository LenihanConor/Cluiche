# System Spec: DiaMessageBus

**Research:** @docs/research/gameplay_msg_bus/summary.md

## Parent Application
@docs/specs/applications/dia/dia.md

**Gameplay Domains:** gameplay, ai, entity

## Purpose

DiaMessageBus is the standard typed message routing layer for gameplay systems and entity components in Cluiche. It sits one layer above DiaMailbox — owning a `Mailbox` instance, registering routers, running the two-pass per-tick flush, and maintaining the frame ledger that feeds editor tooling.

Without DiaMessageBus, gameplay systems couple directly to each other through ad-hoc observer subscriptions that are invisible to tooling and unscalable. DiaMessageBus replaces that pattern with a single, editor-visible routing table: systems declare what they produce and consume, the bus routes between them, and the schema browser surfaces the full connection graph at design time.

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
- Provide `Bus::Post<T>(Address, const T&)` — O(1) enqueue into the Mailbox ring for type T
- Run two-pass flush per sim tick:
  - **Pre-Primary**: drain all flush adapters (physics, input) into the bus
  - **Primary pass**: drain all type queues, resolve subscribers via router, call registered handlers
  - **Reaction pass**: drain reaction-tagged messages enqueued during Primary handlers; handlers may not re-enqueue
- Provide producer registration: `RegisterProducer<T>(StringCRC producerId)` — records which system/component produces type T; feeds the schema browser routing table
- Provide consumer registration: `Subscribe<T>(StringCRC subscriberId, Handler)` — records subscriber and installs dispatch callback; returns `BusSubscriptionHandle` (RAII unsubscribe)
- Maintain a **frame ledger ring buffer**: 60 seconds of per-tick message batches, stripped in Release builds; exposed read-only via `ServiceStream<LedgerSnapshot>` for editor consumption
- Provide `IFlushAdapter` interface — physics adapter, input adapter implement this; bus calls `Flush()` in pre-Primary step
- Register `DiaMessageBusModule` as an `IModule` on SimPU; flush is called from module `Update()`
- Provide `dia.messagingbus.architecture.module.md` YAML module documentation
- Ship as `Dia/DiaMessageBus/DiaMessageBus.vcxproj` static library registered in `Cluiche.sln`

## Non-Responsibilities

- **Message schema definitions.** `DiaGameplayMessages` (or game-equivalent) lives in the application layer — game-specific content, not engine infrastructure.
- **EntityRouter implementation.** EntityRouter is owned by diaentitytemplate and registered with the bus at stage start. DiaMessageBus only holds the registration point.
- **Cross-PU messaging.** SimPU → RenderPU or MainPU communication remains the stream system's job. Flush adapters are the seam, not the bus itself.
- **Thread safety.** Bus is single-threaded (sim thread only), matching DiaMailbox's SD-MBX-007 decision.
- **Message persistence across stage transitions.** The bus and its ledger die with the stage's `MessageBusModule`. The ring buffer is transient.
- **Live inspector UI (CluicheEditor).** The ring buffer and `ServiceStream` are the designed seam; the editor live-replay panel is a future "DiaMessageBus tooling v2" milestone. The in-game `IDebugDomain` overlay (`MessageBusDebugDomain`, opened with `~`) is in scope — see Feature: visual-debugger.
- **EventDispatcher.** `Dia::Core::Events::EventDispatcher` is removed as part of this system (see Feature: eventdispatcher-removal). DiaInput migrates to `InputBusAdapter`.
- **Schema code-gen.** DiaPython-based IDL → C++ message type generation is deferred to tooling v2.

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

        // Record this system/component as a producer of type T. Editor only — no runtime effect.
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

        // --- Ledger (editor / diagnostic) ---

        // Read-only snapshot of the last completed tick's ledger entry.
        // Exposed via ServiceStream<LedgerSnapshot> for cross-PU editor access.
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

### LedgerSnapshot

```cpp
namespace Dia::MessageBus {

    struct LedgerMessageEntry {
        Dia::Core::StringCRC  typeId;       // message type
        Dia::Core::StringCRC  routerId;     // which router resolved it
        uint32_t              count;        // messages of this type this tick
        uint32_t              deliveries;   // total handler calls this tick
        Pass                  pass;         // Primary or Reaction
    };

    static constexpr uint32_t kLedgerCapacity = 3600;  // ticks; ~60s at 60hz

    struct LedgerSnapshot {
        uint64_t                                         tickIndex;
        uint64_t                                         timestampUs;   // wall-clock at flush time
        Dia::Core::Containers::DynamicArrayC<LedgerMessageEntry, 64> entries;
        uint32_t                                         droppedCount;  // overflow drops this tick
    };

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

| Feature | Description | Spec | Status |
|---------|-------------|------|--------|
| core-bus | `Bus` class, `MessageBusModule`, `BroadcastRouter`, two-pass flush, `Post<T>`, `Broadcast<T>`, `Subscribe<T>`, `RegisterProducer<T>` | [core-bus.md](core-bus.md) | Draft |
| entity-router-registration | `RegisterRouter` point for diaentitytemplate's `EntityRouter`; `kEntityRouterId` address constant; integration test with entity-addressed `HitEvent` | [entity-router-registration.md](entity-router-registration.md) | Draft |
| flush-adapters | `IFlushAdapter` interface; `PhysicsBusAdapter` (DiaRigidBody2D); `InputBusAdapter` (DiaInput, replaces EventDispatcher usage) | [flush-adapters.md](flush-adapters.md) | Draft |
| frame-ledger | `LedgerSnapshot`, ring buffer (60 seconds), `ServiceStream<LedgerSnapshot>` export, Release-build strip | [frame-ledger.md](frame-ledger.md) | Draft |
| schema-browser | CluicheEditor panel: graph view, list view, payload inspector, structural duplicate analysis; reads routing table from `MessageBusModule` via `DiaDebugServer`; mockup at `docs/research/gameplay_msg_bus/inspector-mockup.html` | [schema-browser.md](schema-browser.md) | Draft |
| eventdispatcher-removal | Remove `Dia::Core::Events::EventDispatcher`, `EventQueue`, `Delegate` from DiaCore; migrate `DiaInput::InputSourceManager::UpdateModern` and `LegacyEventConverter` to `InputBusAdapter`; delete dead test files | [eventdispatcher-removal.md](eventdispatcher-removal.md) | Draft |
| visual-debugger | `MessageBusDebugDomain` — `IDebugDomain` (not world-space): Schema tab (registered producers → router → subscribers per type), Live tab (current-tick `LedgerSnapshot` counts + deliveries + pass tag), History tab (per-tick totals ring buffer); commands: `selectTab`, `setHistoryWindow`; guarded `#ifdef DIA_DEBUG` | [visual-debugger.md](visual-debugger.md) | Draft |
| messagebus-test-stage | CluicheTest E2E stage: 5 wandering Emitters + 5 wandering Receivers; `NetworkPulseEvent` (broadcast, 2s interval), `DirectPingEvent` (EntityRouter, deterministic round-robin), `PongEvent` (Reaction pass), `BurstEvent` (InputBusAdapter auto-trigger); colour flash overlays + animated connection lines; 5 checkpoints | [messagebus-test-stage spec](../../../../../cluichetest/systems/teststages/messagebus-test-stage.md) | Approved |
| module-and-build | `DiaMessageBus.vcxproj`, `Cluiche.sln` registration, `dia.messagingbus.architecture.module.md` YAML | [module-and-build.md](module-and-build.md) | Draft |

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
- **DiaEditor / DiaDebugServer** — the schema browser reads the routing table via existing WebSocket/debug protocol; DiaMessageBus exposes data, the editor consumes it.

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
- **Schema code-gen (DiaPython IDL).** Deferred to tooling v2.
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
| SD-MBX2-004 | Frame ledger lives game-side (SimPU), 60 seconds, stripped in Release | Captures history before editor is opened; zero cost in shipping builds | Ledger | Accepted | Yes |
| SD-MBX2-005 | DiaGameplayMessages lives in application layer, not engine | Message schemas are game content; engine must not encode game-specific types | Schema | Accepted | Yes |
| SD-MBX2-006 | EventDispatcher removed from DiaCore in same spec | Eliminates a stranded messaging system with STL internals; DiaInput migrates to InputBusAdapter | DiaInput | Accepted | Yes |
| SD-MBX2-007 | Schema browser is design-time only; live inspector deferred | Ring buffer + ServiceStream seam designed in; UI is a follow-on "tooling v2" milestone | Editor | Accepted | Yes |
| SD-MBX2-008 | Single-threaded (sim thread only) | Matches DiaMailbox SD-MBX-007; cross-PU traffic uses DiaStreams | All | Accepted | Yes |

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
| AD-001 | Dia App | Module YAML frontmatter | Create `dia.messagingbus.architecture.module.md` with public API, responsibilities, and dependency declarations. |
| AD-002 | Dia App | No STL in public APIs | Reinforces PD-004. |
| AD-003 | Dia App | Namespace `Dia::<Module>::` | All code in `Dia::MessageBus::` namespace. |

## Status

`Approved` — Ready to build.

**Plan:** [diamessagebus.plan.md](diamessagebus.plan.md) _(created at implementation start)_
