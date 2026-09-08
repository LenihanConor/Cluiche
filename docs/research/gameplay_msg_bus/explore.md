# Research: Explore — Gameplay Message Bus

**Session date:** 2026-08-10
**Folder:** docs/research/gameplay_msg_bus/

## Problem Space Overview

Gameplay systems in Cluiche (damage, health, AI, loot, achievements, UI feedback, etc.) currently have no standardised way to communicate. Without a controlled communication layer, systems either couple directly to each other or rely on ad-hoc observer subscriptions that are invisible to tooling. As the number of gameplay systems grows — especially across the 3.0-Gameplay solution folder — this becomes a debugging and maintainability problem.

The goal is **DiaMessageBus**: a runtime layer that routes typed, named messages between gameplay systems and entity components, with no sender-to-receiver coupling, and with enough structure that an editor can visualise what is connected to what and flag duplicate or redundant message types.

The design is substantially settled through pre-research discussion. Key decisions made before Explore are documented in the Decisions section below.

## Existing Infrastructure Audit

### DiaMailbox (foundation — reuse)

`Dia/DiaMailbox/` is a generic typed deferred messaging primitive. It provides:
- Typed ring buffers (`Register<T, kCap>()`, `Send<T>()`, `Drain<T>(visitor)`)
- Opaque address routing via pluggable `IMailboxRouter` — `Address = { StringCRC routerId, uint64_t payload }`
- Caller-managed subscriptions with generation-tracked handles
- Single-threaded, polled-drain model (SD-MBX-003 deliberately excluded callbacks)

DiaMailbox explicitly left out: callback dispatch, flush passes, sim loop wiring, producer registration. These are DiaMessageBus's job. DiaMailbox is the transport; DiaMessageBus is the bus. Same relationship as diaentitytemplate's EntityModule to DiaMailbox.

Status: Done, 49/49 tests GREEN.

### Dia::Core::Events::EventDispatcher (redundant — remove)

`Dia/DiaCore/Architecture/Events/EventDispatcher.h` is a complete typed in-process bus with optional queue. Currently used in exactly one place: `DiaInput::InputSourceManager::UpdateModern()` and `LegacyEventConverter`. Internally uses `std::unordered_map` (violates PD-004). No wiring into ApplicationFlow's module lifecycle.

**Decision: remove EventDispatcher.** DiaInput will migrate to posting into DiaMessageBus via a flush adapter (same pattern as physics). EventDispatcher was solving the same problem as DiaMessageBus with STL internals and no flush model — it is redundant once DiaMessageBus exists.

### Dia::Core::Observer / domain IXxxObserver interfaces (ad-hoc — replace over time)

Raw `Observer`/`ObserverSubject<T>` with `int message` discriminant. Domain interfaces (`IEconomyObserver`, `IOrderQueueObserver`, etc.) are independent synchronous vtable designs. These are the ad-hoc system-to-system coupling DiaMessageBus replaces. Not removed immediately — migrated as each system adopts DiaMessageBus.

### DiaStreams (cross-PU — not replaced)

`EventStreamStore<T>`, `FrameStreamStore<T>`, `ServiceStreamStore<T>` remain the sole cross-PU (cross-thread) communication mechanism. DiaMessageBus is same-PU (SimPU). The seam between them is flush adapters (see below).

## Settled Design Decisions

| # | Decision |
|---|---------|
| 1 | Observer mechanism: bus is the sole subscriber to any system. No system-to-system direct subscriptions. |
| 2 | Physics and animation excluded from the bus as participants. Flush adapters buffer internal events and drain into the bus at a fixed pre-Primary sync point each tick. |
| 3 | Shared schema module `DiaGameplayMessages` owns all message type definitions. Systems couple to the schema, never to each other. Named structs over anonymous typed ports — better debuggability and tooling. |
| 4 | Batched delivery — post to queue, flush once per sim tick. Deterministic, free frame ledger. |
| 5 | Two named passes per tick: Primary then Reaction. Reaction-pass messages cannot re-queue. Hard depth limit prevents infinite loops. |
| 6 | DiaMessageBus builds on DiaMailbox — owns one Mailbox instance, adds callback dispatch, two-pass flush, producer registration, sim loop wiring. |
| 7 | Multiple routers in one bus. `BroadcastRouter` (routerId: "broadcast") for system-to-system fan-out. `EntityRouter` (routerId: "entity") for addressed entity-component delivery. Same Mailbox instance, same flush, same ledger. |
| 8 | All four communication patterns work via Address choice alone: system→system (broadcast), entity→entity (entity router), system→entity (entity router), entity→system (broadcast). Sender identity is irrelevant — only Address determines receivers. |
| 9 | DiaInput crosses a thread boundary (MainPU → SimPU) so it uses EventStream → flush adapter into DiaMessageBus, not direct bus participation. |
| 10 | Module name: DiaMessageBus. Not DiaGameplayMessageBus (too narrow — entity routing is engine infrastructure) and not DiaBus (too vague). |

## Existing Approaches (Industry)

- **Event queue / message bus** — Unreal Delegates + EventGraph, Unity UnityEvents. Decouples producer from consumer at one-tick latency.
- **Observer / signal-slot** — Qt signals/slots, Boost.Signals2. Direct subscription, can produce invisible dependency graphs.
- **ECS command buffers** — Unity DOTS, Bevy events. Data-oriented; messages are plain structs in typed buffers. Consumer polls.
- **Blackboard** — shared data store, polling model. Already present as DiaBlackboard — complementary, not overlapping.

## Design Axes

| Axis | Settled |
|------|---------|
| Delivery timing | Batched, two named passes (Primary + Reaction) |
| Schema ownership | Shared `DiaGameplayMessages` module |
| Subscription model | Bus-mediated observer; systems never subscribe to each other |
| Cross-thread support | Sim-thread-only; cross-thread via existing stream system + flush adapters |
| Physics/animation | Flush adapters at fixed pre-Primary sync point |
| Reaction depth | Two named passes; reaction messages cannot re-queue |
| Runtime introspection | Frame ledger (free — copy of batch before clear); live inspector deferred |
| Message identity | Named structs in shared schema module |
| Transport primitive | DiaMailbox |
| Routing | BroadcastRouter + EntityRouter in same bus instance |

## Architecture Sketch

```
DiaMessageBus (SimModule on SimPU)
├── Mailbox instance
├── BroadcastRouter  (routerId: "broadcast") — fan-out to all subscribed systems
├── EntityRouter     (routerId: "entity")    — addressed delivery to entity components
│
├── Pre-Primary flush adapters
│   ├── PhysicsBusAdapter   ← drains physics EventStream → Bus.Send<CollisionEvent> etc.
│   └── InputBusAdapter     ← drains input EventStream   → Bus.Send<InputEvent> etc.
│
├── Primary pass flush      ← gameplay systems process this tick's messages
└── Reaction pass flush     ← responses to primary; cannot re-queue
```

## Known Tradeoffs

- Batched delivery: one-tick latency per pass. Two passes means a reaction resolves same-frame as its cause. Acceptable for gameplay systems; invisible to player.
- Shared schema module: all systems compile-depend on `DiaGameplayMessages`. Adding a message type requires touching this module. Intentional — it is the single source of truth the editor reads.
- Reaction messages cannot re-queue: enforces a hard depth limit. Deep chains are a design smell, not a runtime problem to solve.
- Frame ledger: cheap for low-volume gameplay messages. Needs usage guideline — not for per-entity per-tick data.
- Two routers in one Mailbox: BroadcastRouter resolution is O(subscribers). EntityRouter resolution is O(components on entity). Both are small sets in practice.

## Known Pitfalls (C++ / game engine context)

- **Subscription lifetime** — system unregistered while bus holds handler pointer. Needs explicit deregister or RAII handle.
- **Message ordering within a pass** — if two systems both produce `DamageEvent` in the same Primary pass, consumer order is undefined within the type. Document as accepted.
- **Circular reaction chains** — `A → DamageEvent → B → CounterAttackEvent → A`. Two-pass cap prevents infinite loops; editor loop detection catches at design time.
- **Schema bloat** — `DamageEvent` / `SpellDamageEvent` / `PoisonDamageEvent` are all "damage with tags." Tag discipline + duplicate detection tooling is the mitigation.
- **Re-entrancy in reaction pass** — reaction-pass handlers must not call `Bus.Send` outside the flush. Clear contract required.
- **STL containers in public API** — violates PD-004. DiaMailbox is already clean; DiaMessageBus public API must use DiaCore containers.

## Cluiche-Specific Opportunities

### Relevant Existing Modules

| Module | Relevance |
|--------|-----------|
| DiaMailbox | Transport primitive — DiaMessageBus owns one Mailbox instance |
| DiaCore | StringCRC for message type IDs and router IDs (PD-001); DynamicArrayC for handler tables |
| DiaApplicationFlow | ProcessingUnit/Phase/Module — bus flush is a Module on SimPU |
| DiaBlackboard | Complementary: state (blackboard) vs events (bus). Should not overlap. |
| DiaObservation | DIA_LOG_*, DIA_TRACE_ZONE for flush points; frame ledger feeds editor |
| DiaEditor | Target consumer of static connection graph and frame ledger |
| diaentitytemplate | EntityRouter lives here; registers with DiaMessageBus at stage start |

### Platform Decision Constraints

| Decision | Implication |
|----------|-------------|
| PD-001 StringCRC | Message type IDs and router IDs are StringCRC. `DamageEvent::kTypeId` is a StringCRC constant. |
| PD-002 ProcessingUnit/Phase/Module | Bus flush belongs in a Module on SimPU. Two-pass flush = two sequential drain calls in the module's Update. |
| PD-004 No STL in public APIs | Handler tables and routing use DiaCore containers. DiaMailbox already clean. |
| PD-007 C++20 | Concepts can enforce message schema constraints (e.g. `MessageType` concept requiring `kTypeId` and being trivially copyable). |

## Open Questions for Ideation

- Should `DiaGameplayMessages` be a standalone module or a folder inside the application layer? It's game-specific content — may not belong in Dia.
- What is the editor surface — static connection graph only, or does the frame ledger need a live viewer?
- How does a system register as a producer vs. consumer? Explicit registration, or inferred from which types it sends vs. subscribes?
- Does the frame ledger persist across frames (ring buffer for replay), or single-frame?
- What happens to unhandled messages — no registered consumer? Silent drop, warn-log, or assert?
- When does EntityRouter register with the bus — stage start, or lazily?
