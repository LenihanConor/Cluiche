---
schema: dia.module.v1
module_id: dia.messagebus
name: DiaMessageBus
owner_team: TBD
layer: foundation/services
status: stub
maturity: dev

path: Dia/DiaMessageBus
language: cpp
parent_module_id: null

summary: >
  Typed message routing layer for gameplay systems and entity components on the sim thread.

intent: >
  Owns one Dia::Mailbox::Mailbox instance per MessageBusModule, registers BroadcastRouter
  and an EntityRouter registration point, and runs a two-pass per-tick flush
  (pre-Primary flush adapters -> Primary pass -> Reaction pass).
  The primary build surface is the generated RegisterMessages wiring produced by
  dia codegen messages from a .diagamemessages IDL document.

responsibilities:
  - Own one Dia::Mailbox::Mailbox instance per MessageBusModule (sim-thread-scoped)
  - Register BroadcastRouter and provide the EntityRouter registration point
  - Run two-pass flush per sim tick (pre-Primary, Primary, Reaction)
  - Provide Bus::Post<T>, Bus::Broadcast<T>, Bus::Subscribe<T>, Bus::RegisterType<T, kCapacity> API
  - Expose last-tick LedgerSnapshot via GetLastTickLedger() (double-buffered)
  - Implement IModule on SimPU (MessageBusModule)
  - Provide IFlushAdapter interface for physics and input adapters
  - Retain the last kLedgerCapacity (3600) completed-tick LedgerSnapshots in a
    Debug-only ring buffer (LedgerHistory), owned by MessageBusModule and
    pushed to from DoUpdate() right after Bus::Update() returns. Compiles out
    of Release entirely (#ifdef DIA_DEBUG). Backing storage is a single
    fixed-capacity heap allocation made once at LedgerHistory construction
    (no heap activity on the Push/ForEachSnapshot hot path). Measured
    sizeof(LedgerSnapshot) = 9496 bytes, so the 3600-slot ring is
    ~34.19 MB (~32.6 MiB) of Debug-only static-lifetime storage.
  - Provide MessageBusDebugDomain, a Debug-only IDebugDomain (in-game visual
    debugger, `~` panel) exposing Schema (routing graph: registered types,
    producers, observed routers, subscribers), Live (last-tick ledger), and
    History (LedgerHistory ring buffer) tabs. Entirely #ifdef DIA_DEBUG;
    zero footprint in Release. Backed by three small Debug-only Bus
    introspection accessors (ForEachRegisteredType/ForEachProducerForType/
    ForEachSubscriberForType) added for this tab's use only.

non_responsibilities:
  - Message schema definitions (application layer -- DiaGameplayMessages or equivalent)
  - EntityRouter implementation (owned by diaentitytemplate)
  - Cross-PU messaging (DiaStreams handles that boundary)
  - Thread safety (single-threaded sim thread only, per SD-MBX2-008)
  - ServiceStream<LedgerSnapshot> cross-PU export outlet for LedgerHistory (deferred live-tooling follow-on, SD-MBX2-004)
  - EventDispatcher (removed in eventdispatcher-removal feature, SD-MBX2-006)

dependent_modules: []

public_api:
  headers:
    - Dia/DiaMessageBus/Bus.h
    - Dia/DiaMessageBus/MessageBusModule.h
    - Dia/DiaMessageBus/IFlushAdapter.h
    - Dia/DiaMessageBus/LedgerSnapshot.h
    - Dia/DiaMessageBus/LedgerHistory.h
    - Dia/DiaMessageBus/BusSubscriptionHandle.h
    - Dia/DiaMessageBus/MessageBusDebugDomain.h
  namespaces:
    - Dia::MessageBus
  entry_points:
    - Bus
    - MessageBusModule
    - IFlushAdapter
    - LedgerSnapshot
    - LedgerHistory
    - BusSubscriptionHandle
    - MessageBusDebugDomain

dependencies:
  required:
    - dia.mailbox
    - dia.core
    - dia.application.flow
    - dia.streams
    - dia.observation
  optional:
    - dia.visual.debugger # MessageBusDebugDomain only; #ifdef DIA_DEBUG, absent from Release
  forbidden:
    - dia.entity.template
    - dia.gameplay.messages
---
