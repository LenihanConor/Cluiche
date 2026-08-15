**Spec:** @docs/specs/applications/dia/systems/diamessagebus/diamessagebus.md
**Status:** In Progress

> Each task = one feature spec. Write + approve the feature spec before starting implementation. Feature specs live alongside this file.

## Phase 0: Scaffold

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | [module-and-build](module-and-build.md) — `DiaMessageBus.vcxproj`, `Cluiche.sln` registration, `dia.messagingbus.architecture.module.md` YAML module doc | Build passes | Not Started | haiku | `dia scaffold module` + `dia docs vcxproj-add` |

## Phase 1: Core

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 2 | [core-bus](core-bus.md) — `Bus`, `MessageBusModule`, `BroadcastRouter`, `IFlushAdapter` interface, two-pass flush; `Post<T>`, `Broadcast<T>`, `Subscribe<T>`, `RegisterProducer<T>`, `BusSubscriptionHandle`, `Pass` enum | `TestDiaMessageBusCore*` | Not Started | sonnet | Prereq: 1; TDD — prove RED first |

## Phase 2: Extensions

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 3 | [entity-router-registration](entity-router-registration.md) — `Bus::RegisterRouter`, `kEntityRouterId` constant, integration test with mock `EntityRouter` and entity-addressed `HitEvent` | `TestDiaMessageBusEntityRouter*` | Not Started | sonnet | Prereq: 2 |
| 4 | [frame-ledger](frame-ledger.md) — `LedgerSnapshot`, `kLedgerCapacity = 3600` ring buffer on `MessageBusModule`, `ServiceStream<LedgerSnapshot>` export, `DIA_RELEASE` strip | `TestDiaMessageBusLedger*` | Not Started | sonnet | Prereq: 2; parallel with 3 |
| 5 | [eventdispatcher-removal](eventdispatcher-removal.md) — Remove `EventDispatcher`, `EventQueue`, `Delegate` from DiaCore; migrate `DiaInput::InputSourceManager` + `LegacyEventConverter` to `InputBusAdapter` | Full compile + GoogleTest green | Not Started | sonnet | Prereq: 2; grep for callsites before removing |
| 6 | [flush-adapters](flush-adapters.md) — `PhysicsBusAdapter` in DiaRigidBody2D (drains physics `EventStream`); `InputBusAdapter` in DiaInput (replaces EventDispatcher) | Integration tests | Not Started | sonnet | Prereq: 2, 5 |

## Phase 3: Editor

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 7 | [schema-browser](schema-browser.md) — CluicheEditor panel; graph view, list view, payload inspector, structural-duplicate analysis; reads routing table from `MessageBusModule` via `DiaDebugServer`; reference mockup at `docs/research/gameplay_msg_bus/inspector-mockup.html` | HTML mockup acceptance gate | Not Started | opus | Prereq: 3, 4, 6 |
