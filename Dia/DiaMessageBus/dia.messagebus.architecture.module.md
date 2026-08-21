---
schema: dia.module.v1
module_id: dia.messagebus
name: DiaMessageBus
owner_team: TBD
layer: framework
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

non_responsibilities:
  - Message schema definitions (application layer -- DiaGameplayMessages or equivalent)
  - EntityRouter implementation (owned by diaentitytemplate)
  - Cross-PU messaging (DiaStreams handles that boundary)
  - Thread safety (single-threaded sim thread only, per SD-MBX2-008)
  - Ledger history ring buffer and ServiceStream cross-PU export (deferred live-tooling follow-on, SD-MBX2-004)
  - EventDispatcher (removed in eventdispatcher-removal feature, SD-MBX2-006)

dependent_modules: []

public_api:
  headers:
    - Dia/DiaMessageBus/Bus.h
    - Dia/DiaMessageBus/MessageBusModule.h
    - Dia/DiaMessageBus/IFlushAdapter.h
    - Dia/DiaMessageBus/LedgerSnapshot.h
    - Dia/DiaMessageBus/BusSubscriptionHandle.h
  namespaces:
    - Dia::MessageBus
  entry_points:
    - Bus
    - MessageBusModule
    - IFlushAdapter
    - LedgerSnapshot
    - BusSubscriptionHandle

dependencies:
  required:
    - dia.mailbox
    - dia.core
    - dia.application.flow
    - dia.streams
    - dia.observation
  optional: []
  forbidden:
    - dia.entity.template
    - dia.gameplay.messages
---
