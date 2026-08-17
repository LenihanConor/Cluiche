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

## Phase 4: Tooling

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 8 | [visual-debugger](visual-debugger.md) — `MessageBusDebugDomain` (IDebugDomain): Schema tab (producers/router/subscribers per type), Live tab (LedgerSnapshot entry list + totals), History tab (per-tick ring buffer; dropped-tick highlight); `OnCommand` tab/window dispatch; guarded `#ifdef DIA_DEBUG` | `TestMessageBusDebugDomain*` GoogleTest suite (AC-2 through AC-8 per feature spec) | Not Started | sonnet | Prereq: 4 (frame-ledger); adds DiaVisualDebugger as debug-only dep |
| 9 | Exhaustive testing — (a) GoogleTest: `MessageBusDebugDomain` `GetJSONState()` shape, command dispatch, mock-ledger stats computation, producer/subscriber schema stability, reaction-pass entries tagged correctly; (b) GoogleTest: full Bus round-trip with mock `EntityRouter` + real BroadcastRouter + Reaction pass re-entrancy assert; (c) HTML mockup for `MessageBusDebugDomain` panel (Schema / Live / History tabs) as editor acceptance gate | All tests green; HTML mockup reviewed | Not Started | sonnet | Prereq: 8; covers both runtime correctness and editor panel visual contract |
| 10 | [messagebus-test-stage](../../../../../cluichetest/systems/teststages/messagebus-test-stage.md) — CluicheTest E2E stage; see feature spec for full task breakdown (7 sub-tasks) | All 5 checkpoints pass; `dropped == 0`; determinism second-pass passes | Not Started | sonnet | Prereq: 8; stage spec has its own plan |
