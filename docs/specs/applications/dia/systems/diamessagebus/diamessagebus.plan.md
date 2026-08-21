**Spec:** @docs/specs/applications/dia/systems/diamessagebus/diamessagebus.md
**Status:** In Progress

> Declaration-first system. The through-line is the authoring loop (B): design messages in a `.diagamemessages` doc → `dia codegen messages` generates structs + registration wiring → build the system out by filling in handler slots. The last-tick ledger and the in-game visual debugger are in scope. Only the Inspector tier (cross-PU `ServiceStream` export + live CluicheEditor Inspector) is deferred (see bottom).
>
> Each task = one feature spec unless noted. Write + approve the feature spec before starting implementation. Feature specs live alongside this file.
>
> **Phase 8** covers cross-system adoption — eight Approved feature specs living in *other* systems' own directories (DiaEntity, DiaEconomy, DiaOrder, DiaBehaviourTree, DiaEntitySpawner, DiaAICallout, DiaAssetRuntime, DiaAnimation2D), each wiring that system onto the bus this plan builds. They're tracked here because they're all consumers of the same `core-bus` prerequisite and their build order is only sensible relative to it — the owning system's own plan should also get a one-line task added pointing back here when work starts.

## Phase 0: Scaffold

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | [module-and-build](module-and-build.md) — `DiaMessageBus.vcxproj`, `Cluiche.sln` registration, `dia.messagebus.architecture.module.md` YAML module doc | Build passes | Done | haiku | `dia scaffold module` + `dia docs vcxproj-add`; DiaMessageBus.vcxproj scaffolded, registered in Cluiche.sln (framework layer), module YAML authored. dia check sln-sync clean; dia check deps 37/37 OK; build 0 errors/0 warnings Debug|x64. |

## Phase 1: Core bus (runtime substrate)

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 2 | [core-bus](core-bus.md) — `Bus`, `MessageBusModule`, `BroadcastRouter` (new concrete `IMailboxRouter`), `RegisterType<T>`, `RegisterProducer<T>` (lightweight metadata table, no routing effect), `Post<T>`, `Broadcast<T>`, `Subscribe<T>` (Primary pass), `BusSubscriptionHandle`, `IFlushAdapter`; **last-tick ledger** + `GetLastTickLedger()` | `TestDiaMessageBusCore*` | Done | sonnet | Prereq: 1; TDD — prove RED first. DiaMailbox is built; wrap it. Ledger doubles as the test assertion surface.; Bus + MessageBusModule + BroadcastRouter + RegisterType/RegisterProducer/Subscribe/Post/Broadcast + single Primary pass + last-tick ledger implemented. Module base is Dia::ApplicationFlow::Module (spec's IModule wording was stale) via EntitySpawnerModule split pattern. 20/20 new tests green, 53/53 Mailbox regression green. |
| 3 | [core-bus](core-bus.md) (two-pass flush) — `Pass` enum, Reaction pass drain, re-entrancy assert (Debug) / silent drop (Release) per SD-MBX2-001 res. #1 | `TestDiaMessageBusFlush*` | Done | sonnet | Prereq: 2; same feature spec (Q1), split out as its own task — hardest single piece; Reaction sweep + Post re-entrancy guard implemented; DIA_ASSERT firing verified via g_pAssertFunc recorder. FIXED (was flagged as a known limitation, then resolved same session): Reaction delivery was order-dependent on RegisterType order between posting type and target type. Fixed via a pre-sweep per-type queue-count snapshot bounding the Primary drain (Mailbox::Drain<T,Visitor> gained a backward-compatible maxCount param). 29/29 core-bus tests green, 53/53 Mailbox regression green, 43/43 EntitySpawner/DiaEntity spot-check green. |

## Phase 2: Declaration-first codegen loop (the heart — B)

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 4 | [diagamemessages-format](diagamemessages-format.md) — `.diagamemessages` JSON IDL format; `_validate_diagamemessages` + `*.diagamemessages` scan in `cli_validate.py` | `dia validate manifest` rejects invalid files; valid sample parses | Done | sonnet | No runtime prereqs — pure tooling; front-loadable; _validate_diagamemessages added to cli_validate.py; *.diagamemessages added to manifest() scan+dispatch. 182 files scanned, 4 pre-existing unrelated errors unchanged. Invalid/valid sample files verified. |
| 5 | [diagamemessages-format](diagamemessages-format.md) (codegen) — `dia codegen messages`: structs (`kTypeId` + fields) **+ `RegisterMessages(Bus&, Handlers&)` wiring + `Handlers` binding struct**; generated headers committed; per SD-MBX2-009/010 | codegen on sample produces header that compiles + wires against `Bus`; unbound handler asserts | Done | sonnet | Prereq: 4; generated output compiles once 2 is done; dia codegen messages implemented; generates structs+Handlers+RegisterMessages wiring. Fixed include path bug (DiaCore/CRC not DiaCore/String) and threads message pass explicitly into Subscribe() calls. Sample fixture generated+validated; verified by manual inspection against Bus.h (no vcxproj target yet to compile against). 5/5 Python tests green. |

## Phase 3: Editor — offline design surface (B)

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 6 | [schema-browser](schema-browser.md) — CluicheEditor offline panel; scans ALL `.diagamemessages` tree-wide, builds the **union graph**, joins producers↔consumers by `StringCRC`; graph / list / web views, field inspector, structural-duplicate + **orphan** analysis; read-only viewer (v1); mockup at `docs/research/gameplay_msg_bus/inspector-mockup.html` | HTML mockup acceptance gate | Done | opus | Prereq: 4; DiaSchemaBrowser CluicheEditor plugin: read-only union-graph builder over all *.diagamemessages files (5 found, 17 messages), Graph/List/Web views, Payload tab, Schema Analysis (computed dupe scoring + bidirectional orphans), matching approved inspector-mockup.html wired to real data. Placed under Dia/ per DiaPipelineEditor/DiaSceneEditor precedent. 21/21 unit tests green, pipeline build 3/3 stages green. |

## Phase 4: Entity routing (B — entity↔entity / system↔entity design)

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 7 | [entity-router-registration](entity-router-registration.md) — `Bus::RegisterRouter`, `kEntityRouterId` constant, integration test with mock `EntityRouter` and entity-addressed `HitEvent` | `TestDiaMessageBusEntityRouter*` | Done | sonnet | Prereq: 2; RegisterRouter/kEntityRouterId/kBroadcastRouterId already existed from core-bus; added EntityRouterRegistrationTest.cpp with MockEntityRouter. 4/4 new tests green, 29/29 core-bus regression green. |

## Phase 5: Independent cleanup (in scope, unsequenced)

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 8 | [eventdispatcher-removal](eventdispatcher-removal.md) — Remove `EventDispatcher`, `EventQueue`, `Delegate` from DiaCore; migrate `DiaInput::InputSourceManager` + `LegacyEventConverter` to `InputBusAdapter`; delete dead test files | Full compile + GoogleTest green | Done | sonnet | Prereq: 2. Verified blast radius: DiaInput + DiaCore/Architecture/Events + 4 test files (ChatPanelBridge `mEventQueue` is `std::queue`, not coupled); EventDispatcher/EventQueue/LegacyEventConverter removed; UpdateModern removed, Update(EventData&) preserved; 4 dead test files deleted. DEVIATION: Delegate.h NOT deleted -- spec's blast-radius claim was wrong, it's live production code (ActionMap::ActionCallback), deleting it would break DiaInput. Retiring it is a separate future decision. Full suite green (8610 tests). |
| 9 | [flush-adapters](flush-adapters.md) — `PhysicsBusAdapter` in DiaRigidBody2D (drains physics `EventStream`); `InputBusAdapter` in DiaInput | Integration tests | Done | sonnet | Prereq: 2, 5, 8 — needs codegen (5) for the generated `physics_messages.h`/`input_messages.h` the adapters `#include`; runs after eventdispatcher-removal (8); PhysicsBusAdapter: Observer-based bridge over PhysicsWorld's collision ObserverSubject, new PhysicsCollisionEvent (distinct from unsafe raw-pointer CollisionEvent), Broadcast-only (no entity identity available). InputBusAdapter: owns InputSourceManager&+scratch buffer; documented but not wired into KernelModule due to a real Main/Sim PU threading conflict. Neither wired into production (matches Economy/Callout precedent). 12/12 new tests green, 197/197 RigidBody2D + full Input regression green. |

## Phase 6: In-game visual debugger (A — in-process)

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 10 | [frame-ledger](frame-ledger.md) — history **ring buffer** (`kLedgerCapacity` ticks) over `LedgerSnapshot`, Release-build strip; feeds History tab. (`ServiceStream` export stays deferred.) | `TestDiaMessageBusLedger*` | Done | sonnet | Prereq: 2 (last-tick ledger); LedgerHistory ring buffer (3600 ticks) owned by MessageBusModule, DIA_DEBUG-only, visitor-style read API. Real sizeof(LedgerSnapshot)=9496B (not spec's ~1.3KB estimate) required heap-once-at-construction backing instead of inline array to avoid stack overflow. 7/7 new tests green, 29/29 core-bus regression green. |
| 11 | [visual-debugger](visual-debugger.md) — `MessageBusDebugDomain` `IDebugDomain`: Schema / Live / History tabs; reads bus + ledger in-process; `#ifdef DIA_DEBUG` | AC-2..AC-8 (GoogleTest) + `~` panel visible | Done | sonnet | Prereq: 10; wire into test stage; Domain implementation (AC-2..AC-8) done -- minimal Bus.h introspection added (ForEachRegisteredType/ForEachProducerForType/ForEachSubscriberForType, DIA_DEBUG-only accessors, always-compiled bookkeeping). 8/8 new tests green, 59/59 across affected suites green. AC-1 (`~` panel visible) deferred to task 12 -- no stage yet owns both a Domain and a MessageBusModule to wire it into. |

## Phase 7: End-to-end proof (B)

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 12 | [messagebus-test-stage](../../../../../cluichetest/systems/teststages/messagebus-test-stage.md) — CluicheTest E2E stage; proves doc → codegen → build loop; message types authored in `.diagamemessages`, structs+wiring generated; asserts `dropped == 0` via last-tick ledger | All checkpoints pass; determinism second-pass passes | Not Started | sonnet | Prereq: 3, 5, 7; stage spec has its own plan |

## Phase 8: Consumer integration (cross-system bus adoption)

Eight Approved feature specs, each in its owning system's own directory. Split into sub-tasks where a feature has a bus-independent half (can start before core-bus lands) and a bus-dependent half (can't).

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 13 | [entity-router-bus-wiring](../diaentity/entity-router-bus-wiring.md) — register `Domain`'s existing `EntityRouter` on the Bus's mailbox too; add `Domain::GetEntityRouter()`; define `SubscriberId` = entity handle bits convention | `EntityRouterBusWiringTests` — subscribe via `SubscriberId{handle.bits}`, post to `{kEntityRouterId, handle.bits}`, assert delivery | Done | sonnet | Prereq: 2 (core-bus), 7 (entity-router-registration). Composition-root wiring exercised in test fixture (no real stage owns both Domain+Bus yet). FOUND+FIXED: Bus::kEntityRouterId value ("entity") didn't match real EntityRouter::GetRouterId() ("dia.entity.router") -- would have silently never resolved. 11/11 new tests green, 784/784 DiaEntity regression green. |
| 14 | [economy-observer-bus-adapter](../diaeconomy/economy-observer-bus-adapter.md) — redesign `PoolChangedEvent`/`TransactionClampedEvent`/`TransferCompletedEvent` to snapshot values (drop raw `EconomyInstance*`); add `PoolReachedMaximumEvent`/`PoolReachedMinimumEvent`; `EconomyBusAdapter : IEconomyObserver` | `EconomyBusAdapterTests` | Done | sonnet | Prereq: 2. Grep for existing direct `IEconomyObserver` implementers before changing struct signatures (open question in spec); PoolChangedEvent/TransactionClampedEvent/TransferCompletedEvent redesigned to snapshot values via GetInstanceName(); PoolReachedMaximum/MinimumEvent added; EconomyBusAdapter forwards all five to Bus::Broadcast. Two existing IEconomyObserver implementers (EconomyEventsSource, EconomyTestHelpers) updated. 7/7 new tests green, 135/135 full DiaEconomy suite green. |
| 15 | [order-observer-bus-adapter](../diaorder/order-observer-bus-adapter.md) — bridging pattern only; no concrete `OrderQueue<TContext>` instantiation found in codebase at spec time | `OrderBusAdapterTests` | Not Started | sonnet | UNBLOCKED: real `TContext` now exists -- `Dia::Order::OrderQueue<GuardOrderContext>` in `Cluiche/CluicheTest/Modules/TestStages/BehaviourTreeTestStageModule.h`, with concrete `GuardMoveOrder : IOrder<GuardOrderContext>` and an existing direct observer `GuardOrderObserver` to coexist with. `IOrder::GetOrderId()` confirmed (StringCRC). Dispatch after task 16 lands (both may touch BehaviourTreeTestStageModule.h -- avoid file collision). |
| 16 | [behaviourtree-observer-bus-adapter](../diabehaviourtree/behaviourtree-observer-bus-adapter.md) — `BehaviourTreeBusAdapter : IBehaviourTreeEventListener`; posts to `{kEntityRouterId, ownerEntity.bits}`, not Broadcast | `BehaviourTreeBusAdapterTests` | Done | sonnet | Prereq: 2, **13** (entity-router-bus-wiring — addressing doesn't resolve to real subscribers until that lands). Identify the `BehaviourTreeComponent` attach call site to supply the owner entity handle; BehaviourTreeBusAdapter forwards via entity-addressed Post (never Broadcast) using MakeEntityAddress. No real per-entity attach site found (only DebugGalleryTestStageModule.cpp, no entity handle in scope) -- proved via direct tests against a real Domain-owned entity. 6/6 new tests green, 101/101 full suite green. |
| 17 | [spawn-despawn-bus-broadcast](../diaentityspawner/spawn-despawn-bus-broadcast.md) — `Bus::Broadcast<EntitySpawnedEvent>`/`<EntityDespawnedEvent>` added in `EntitySpawnerModule::DoUpdate`, alongside existing `EventStreamWriter` publish | `SpawnDespawnBusBroadcastTests` | Done | sonnet | Prereq: 2. No open questions — smallest task in this phase; EntitySpawnerModule broadcasts EntitySpawnedEvent/EntityDespawnedEvent on Bus alongside existing EventStreamWriter publish; Bus& threaded through constructor; kTypeId hand-added to existing SpawnerTypes.h structs (no codegen -- structs already fixed layout). 7/7 new tests green, 29/29 full suite green. |
| 18a | [callout-observer-bus-adapter](../diaaibroadcast/callout-observer-bus-adapter.md) — `ICalloutObserver`/`CalloutObserverSubject` composed by `CalloutRegistry`; notify on Emit/Claim/Release only (not TTL expiry) | Unit tests on notification firing, no bus dependency | Done | sonnet | No prereq — this half has no bus dependency, can start immediately; 12 new tests + 72/72 full AICallout suite green |
| 18b | [callout-observer-bus-adapter](../diaaibroadcast/callout-observer-bus-adapter.md) (bus half) — `CalloutBusAdapter : ICalloutObserver` forwarding to `Bus::Broadcast` | `CalloutObserverBusAdapterTests` | Done | sonnet | Prereq: 2, 18a; CalloutBusAdapter forwards Emit/Claim/Release to Bus::Broadcast via codegen'd callout_messages.diagamemessages. 17/17 tests green (12+5 new), 77/77 full AICallout suite green. |
| 19a | [asset-runtime-bus-adapter](../diaassetruntime/asset-runtime-bus-adapter.md) (restore) — rebuild `IAssetStateListener`/`RegisterListener`/`UnregisterListener`/dispatch wiring exactly per the existing Approved `event-notification.md` spec (missing from codebase despite plan marking it Done) | Restore original `EventNotificationTest.cpp` — 9 tests | Done | sonnet | No prereq — no bus dependency, can start immediately. Original enum (Registered/Staged/Unloading) no longer exists (commit e17825a4 refactored to handler-driven state machine) — dispatch adapted into current `TryTransition()`; 9 new tests (TestListenerNotification.cpp) + 125/125 full suite green |
| 19b | [asset-runtime-bus-adapter](../diaassetruntime/asset-runtime-bus-adapter.md) (bus half) — `AssetRuntimeBusAdapter : IAssetStateListener` forwarding to `Bus::Broadcast` | `AssetRuntimeBusAdapterTests` | Done | sonnet | Prereq: 2, 19a; AssetRuntimeBusAdapter forwards OnAssetReady/Unloading/LoadFailed to Bus::Broadcast via codegen'd assetruntime_messages.diagamemessages. 6/6 new tests green, 33/33 AssetRuntime*-matching suite green. |
| 20a | [clip-completion-bus-adapter](../diaanimation2d/clip-completion-bus-adapter.md) (detection) — add one-shot/loop completion detection to `AnimClipPlayer::Update`; `IAnimClipObserver`; must distinguish natural finish from explicit `Stop()` | `ClipCompletionBusAdapterTests` — natural-finish-vs-Stop() case required | Done | sonnet | No prereq — no bus dependency, can start immediately; 10 new tests + 108/108 full Animation2D suite green |
| 20b | [clip-completion-bus-adapter](../diaanimation2d/clip-completion-bus-adapter.md) (bus half) — `AnimClipBusAdapter : IAnimClipObserver`; `Broadcast` is primary delivery mode (resolved decision — no `AnimationComponent2D` built for this) | Same test file, bus-delivery cases | Done | sonnet | Prereq: 2, 20a; AnimClipBusAdapter forwards via codegen'd animation2d_messages.diagamemessages; payload carries GetId() StringCRC not a raw pointer. 15/15 tests green, 214/214 broad Animation2D regression green. |

## Phase 8 Test Plan

Exhaustive, per-feature test enumeration derived from each spec's Acceptance Criteria plus the specific edge cases surfaced during design review (raw-pointer/snapshot fixes, entity-handle threading, TTL-expiry exclusion, Stop()-vs-natural-finish, etc.). Task-row `Test` columns above name the test file; this section is the exhaustive breakdown within it.

### Task 13 — entity-router-bus-wiring

| # | Test Name | Type | Description |
|---|-----------|------|-------------|
| 1 | `GetEntityRouter_ReturnsSameInstanceAsPrivateMailboxRouter` | Unit | `Domain::GetEntityRouter()` returns the identical `EntityRouter&` already registered on Domain's private `Mailbox` |
| 2 | `RegisterRouterOnBus_DelegatesCleanlyAlongsideExistingRegistration` | Unit | `Bus::RegisterRouter(&domain.GetEntityRouter())` succeeds without disturbing the router's existing registration on Domain's own mailbox |
| 3 | `EntityAddressedPost_DeliversToSubscribedComponent` | Integration | Subscribe via `SubscriberId{handle.bits}` on the Bus, post `{kEntityRouterId, handle.bits}` from a second system, assert delivery |
| 4 | `EntityAddressedPost_NoSubscribers_SilentNoOp` | Boundary | Posting to an entity with zero Bus-side subscribers produces no assert, no warning, no delivery |
| 5 | `EntityAddressedPost_WrongEntityHandle_NotDelivered` | Unit | A message addressed to entity A is never delivered to a subscriber registered under entity B's `SubscriberId` |
| 6 | `ComponentTypeAddress_StillResolvesViaExistingMechanism` | Regression | `MakeComponentTypeAddress` fan-out still works unchanged after the router is registered on a second mailbox |
| 7 | `SubscriberId_DerivedFromEntityHandleBits_MatchesAddressPayload` | Unit | `SubscriberId{entityHandle.bits()}` used at `Subscribe` matches the payload encoded by `MakeEntityAddress` at `Post` time |
| 8 | `DomainPrivateMailbox_EntityDestroyedMessage_StillDeliveredUnaffected` | Regression | `EntityDestroyedMessage` broadcast on Domain's private mailbox is unchanged after this feature ships |
| 9 | `DomainPrivateMailbox_And_Bus_AreIndependentQueues` | Integration | A message sent via `domain.GetMailbox().Send()` is never visible to a Bus subscriber, and vice versa — no accidental dual-posting |
| 10 | `RegistrationBeforeMessageBusModuleExists_FailsCleanly` | Boundary | Attempting the composition-root registration call before `MessageBusModule` is constructed produces a clear failure, not silent corruption |
| 11 | `RouterRegisteredOnBothMailboxes_FunctionsIndependently` | Integration | The same `EntityRouter` instance resolves correctly when called from Domain's private mailbox and from the Bus's mailbox in the same test run |
| 12 | `StaleEntityHandle_GenerationMismatch_SilentNoOpOnBusPath` | Boundary | A stale/expired entity handle posted via the Bus path resolves to zero matches, matching existing Domain-mailbox behavior |
| 13 | `Determinism_RepeatedRun_IdenticalDeliverySequence` | Determinism | Two identical runs of the same Post/Subscribe sequence produce identical delivery order and counts |

### Task 14 — economy-observer-bus-adapter

| # | Test Name | Type | Description |
|---|-----------|------|-------------|
| 1 | `PoolChangedEvent_ContainsNoRawPointerField` | Unit | Struct definition carries only `StringCRC`/`float` snapshot fields — no `EconomyInstance*`/`&` |
| 2 | `PoolChangedEvent_QueuedThenDelivered_ValuesMatchEmitTimeNotDeliveryTime` | Invariant | Mutate the live `EconomyInstance` between `Post` and the next Primary-pass drain; delivered event still reflects values at emit time (dangling-pointer regression proof) |
| 3 | `TransactionClampedEvent_SnapshotValuesCorrect` | Unit | Requested vs. actual amount fields match the clamp that occurred |
| 4 | `TransferCompletedEvent_SnapshotValuesCorrect` | Unit | From/to resource and amount fields match the completed transfer |
| 5 | `PoolReachedMaximumEvent_NewStruct_FiresOnMaxTransition` | Unit | Value crossing into max fires the new struct-based event, replacing the old non-struct overload |
| 6 | `PoolReachedMinimumEvent_NewStruct_FiresOnMinTransition` | Unit | Value crossing into min fires the new struct-based event |
| 7 | `EconomyBusAdapter_AllFiveCallbacks_ForwardToBroadcast` | Unit | Each of the five `IEconomyObserver` overrides calls `bus.Broadcast<T>()` exactly once with the correct payload |
| 8 | `EconomyBusAdapter_RegisteredAlongsideDirectObserver_BothNotified` | Integration | A direct synchronous `IEconomyObserver` and `EconomyBusAdapter` both fire from the same `Notify()` call — adapter doesn't replace direct use |
| 9 | `DirectSynchronousObserver_StillFiresImmediately_NoBusRequired` | Regression | An `IEconomyObserver` with no bus involvement continues to receive same-tick synchronous notifications exactly as before |
| 10 | `AllFiveEventTypes_DeclaredAndCodegen_KTypeIdPresent` | Build | `.diagamemessages` declarations generate structs with `kTypeId` that compile against `Bus::Broadcast` |
| 11 | `BusSubscriber_ReceivesEconomyEvent_EndToEnd` | Integration | A `Bus::Subscribe<PoolChangedEvent>` handler receives the event after a transaction triggers it |
| 12 | `ModuleOrdering_EconomyBeforeMessageBus_SameTickDelivery` | Integration | When the Economy-owning module updates before `MessageBusModule`, the event is delivered within the same tick |
| 13 | `ModuleOrdering_EconomyAfterMessageBus_OneTickLagStillDelivered` | Integration | When ordered the other way, delivery lags exactly one tick and is not lost |
| 14 | `ExistingIEconomyObserverImplementer_CompilesAgainstNewSignatures` | Build | If any existing direct implementer of `OnPoolReachedMaximum`/`Minimum` is found, it compiles cleanly against the new struct-based signatures |
| 15 | `MultipleTransactionsSameTick_AllEventsQueuedAndDelivered_NoneDropped` | Unit | Several `Earn`/`Spend`/`Transfer` calls within one tick each produce a distinct queued event, none overwritten |

### Task 15 — order-observer-bus-adapter (Blocked — pattern-only; re-validate against the real `TContext` once one exists)

| # | Test Name | Type | Description |
|---|-----------|------|-------------|
| 1 | `OrderBusAdapter_OnOrderStarted_BroadcastsWithOrderIdOnly` | Unit | Against a placeholder concrete `TContext`, verify the broadcast payload carries `OrderId` + concrete fields, never the `IOrder<TContext>&` itself |
| 2 | `OrderBusAdapter_OnOrderFinished_BroadcastsOrderFinishedEvent` | Unit | Finished callback maps to the corresponding value-only event |
| 3 | `OrderBusAdapter_OnOrderCancelled_BroadcastsOrderCancelledEvent` | Unit | Cancelled callback maps to the corresponding value-only event |
| 4 | `OrderBusAdapter_OnQueueEmpty_BroadcastsOrderQueueEmptyEvent` | Unit | Parameterless callback still produces a valid broadcast with whatever queue identity is available |
| 5 | `OrderBusAdapter_NeverForwardsPolymorphicReference` | Invariant | Static/compile-time check — no code path passes `IOrder<TContext>&` into a message struct |
| 6 | `OrderBusAdapter_RegisteredPerConcreteQueueInstance_NotGenericSingleton` | Unit | Each `OrderQueue<ConcreteContext>` gets its own adapter instance; no shared/static adapter |
| 7 | `IOrder_GetOrderId_StableAcrossQueueLifetime` | Unit | Prerequisite check (open Q2) — the concrete `IOrder<TContext>` subtype's order identifier doesn't change across `Start`/`Finish`/`Cancel` |
| 8 | `OnQueueEmpty_PayloadIncludesQueueIdentity_IfEntityAddressable` | Unit | If the queue is entity-scoped, the payload/addressing uses `kEntityRouterId` rather than a bare broadcast (open Q3) |

### Task 16 — behaviourtree-observer-bus-adapter

| # | Test Name | Type | Description |
|---|-----------|------|-------------|
| 1 | `BehaviourTreeBusAdapter_OnNodeEntered_PostsToEntityAddress_NotBroadcast` | Unit | Verify the call is `Post({kEntityRouterId, owner.bits}, ...)`, never `Broadcast` |
| 2 | `BehaviourTreeBusAdapter_OnNodeCompleted_PostsWithCorrectNodeIdAndResult` | Unit | Payload carries the exact `nodeId`/`NodeResult` passed to the callback |
| 3 | `BehaviourTreeBusAdapter_OnTreeCompleted_PostsWithCorrectResult` | Unit | Payload carries the terminal `NodeResult` |
| 4 | `BehaviourTreeBusAdapter_AddressPayload_MatchesOwnerEntityBitsFromConstruction` | Unit | The address bits used at `Post` time equal the entity handle bound at adapter construction |
| 5 | `BehaviourTreeBusAdapter_RegisteredAlongsideDirectListener_BothReceiveEvents` | Integration | A direct `IBehaviourTreeEventListener` (e.g. debug logging) and the bus adapter both fire from the same `Tick()` |
| 6 | `RemovingBusAdapter_DoesNotAffectOtherRegisteredListeners` | Unit | `RemoveEventListener` on the adapter leaves other listeners' subscriptions intact |
| 7 | `NodeEnteredCompletedTreeCompletedEvents_DeclaredAndCodegen_KTypeIdPresent` | Build | All three `.diagamemessages`-declared types compile against `Bus::Post` |
| 8 | `EndToEnd_EntitySubscriber_ReceivesTreeCompletedEvent_ViaEntityRouterBusWiring` | Integration | Full path: `BehaviourTreeComponent` ticks to completion → adapter posts → a Bus subscriber on that entity receives it (requires task 13 shipped) |
| 9 | `TwoEntitiesEachWithOwnComponent_EventsNotCrossDelivered` | Unit | Entity A's tree events never reach a subscriber registered against entity B |
| 10 | `AdapterConstructedWithoutOwnerHandle_FailsClearlyAtConstruction` | Boundary | Unlike Animation2D's optional-owner design, this AC requires the handle at construction — verify a missing handle is a hard, visible failure, not silent |
| 11 | `MultipleNodeEnteredEvents_SingleTick_AllDeliveredInFIFOOrder` | Unit | A tree visiting several nodes in one `Tick()` produces events delivered in visitation order |
| 12 | `OnNodeEntered_FiresPerVisitedNode_MatchesExistingListenerCoverage` | Regression | Bus-delivered node-entry sequence matches what the original `IBehaviourTreeEventListener` test suite already asserts for direct listeners |

### Task 17 — spawn-despawn-bus-broadcast

| # | Test Name | Type | Description |
|---|-----------|------|-------------|
| 1 | `EntitySpawnerModule_Spawn_BroadcastsEntitySpawnedEvent` | Unit | A successful spawn calls `bus.Broadcast<EntitySpawnedEvent>()` with correct entity/blueprint/tag fields |
| 2 | `EntitySpawnerModule_Despawn_BroadcastsEntityDespawnedEvent_WithCorrectReason` | Unit | Each `DespawnReason` (Lifetime, Radius, Cap, Explicit) produces a correctly-tagged broadcast |
| 3 | `EntitySpawnedEvent_StreamWriterConsumer_StillReceivesEvent` | Regression | Existing `EventStreamWriter<EntitySpawnedEvent>` consumers (e.g. RenderPU) are unaffected by the new bus call |
| 4 | `EntityDespawnedEvent_StreamWriterConsumer_StillReceivesEvent` | Regression | Same regression check for despawn |
| 5 | `BusSubscriber_And_StreamConsumer_BothReceiveSameSpawnEvent_Independently` | Integration | Both delivery paths fire from one spawn call, neither blocking or altering the other |
| 6 | `EntitySpawnerImpl_HasNoBusDependency` | Build | Compile/dependency check — `EntitySpawnerImpl` does not `#include` any `DiaMessageBus` header; only `EntitySpawnerModule` does |
| 7 | `DespawnCallback_InternalBridge_StillFiresBeforeBusBroadcast` | Regression | The existing `Impl→Module` C-callback bridge fires exactly as before; the bus call is additive after it |
| 8 | `AllFourDespawnReasons_BroadcastCorrectly_Parametrized` | Unit | Parametrized test sweeping `Lifetime`/`Radius`/`Cap`/`Explicit` through the full despawn path |
| 9 | `EntitySpawnedEvent_EntityDespawnedEvent_DeclaredAndCodegen_KTypeIdPresent` | Build | `.diagamemessages` declarations generate valid, bus-registrable types |

### Task 18 — callout-observer-bus-adapter

| # | Test Name | Type | Description |
|---|-----------|------|-------------|
| 1 | `Emit_AlwaysNotifiesObservers_WithFullCalloutAndHandle` | Unit | Every successful `Emit` fires `OnCalloutEmitted` with the complete `Callout` value and its `CalloutHandle` |
| 2 | `Claim_Success_NotifiesObservers` | Unit | A `Claim` returning `true` fires `OnCalloutClaimed` |
| 3 | `Claim_Failure_AlreadyClaimed_DoesNotNotify` | Boundary | A `Claim` returning `false` (already claimed) fires no notification — only success notifies |
| 4 | `Release_RealRelease_NotifiesObservers` | Unit | A `Release` that actually transitions claimed→unclaimed fires `OnCalloutReleased` |
| 5 | `Release_NoOp_ExpiredHandle_DoesNotNotify` | Boundary | Releasing an expired handle is a silent no-op with zero notifications |
| 6 | `Release_NoOp_AlreadyUnclaimed_DoesNotNotify` | Boundary | Releasing an already-unclaimed callout fires nothing |
| 7 | `Release_NoOp_WrongClaimer_DoesNotNotify` | Boundary | Releasing with the wrong `claimerEntityId` fires nothing |
| 8 | `Update_TTLExpiry_NeverFiresAnyObserverNotification` | Boundary | The single most important exclusion in this spec — `Update(dt)` expiring callouts must never call any `ICalloutObserver` method, at any capacity |
| 9 | `MultipleObservers_Subscribed_AllReceiveSameNotification` | Unit | Two+ subscribed observers each receive the same `Emit`/`Claim`/`Release` event |
| 10 | `Unsubscribe_StopsReceivingFutureNotifications` | Unit | An unsubscribed observer receives nothing from subsequent registry operations |
| 11 | `CalloutBusAdapter_OnCalloutEmitted_BroadcastsFullCalloutPayload` | Unit | Adapter forwards the complete `Callout`+`CalloutHandle` on emit, matching the "no `Query()` round-trip needed" design goal |
| 12 | `CalloutBusAdapter_OnCalloutClaimed_BroadcastsHandleAndClaimerIdOnly` | Unit | Claim/release payloads are the lighter handle+claimerId shape, not the full callout |
| 13 | `CalloutBusAdapter_OnCalloutReleased_BroadcastsHandleAndClaimerIdOnly` | Unit | Same lightweight shape for release |
| 14 | `CalloutEmittedClaimedReleasedEvents_DeclaredAndCodegen_KTypeIdPresent` | Build | All three declared types compile against `Bus::Broadcast` |
| 15 | `ExistingCalloutRegistryConsumer_AICalloutTestStageModule_CompilesUnchanged` | Regression | The Done test-stage module continues to compile and pass without modification |
| 16 | `CalloutRegistry_NoObserversSubscribed_BaseAPIStillWorksNormally` | Boundary | With zero observers registered, `Emit`/`Claim`/`Release`/`Query`/`Update` behave exactly as before this feature |
| 17 | `QueuedCalloutEmittedEvent_HandleRemainsValidUntilBusDrainsIt` | Invariant | The `CalloutHandle` carried in a queued bus message stays valid/dereferenceable through the registry's realistic same-frame lifetime (open Q2) |
| 18 | `BusSubscriber_ReactsToEmit_WithoutCallingQuery` | Integration | End-to-end proof of the core design goal — a subscriber filters and reacts locally off the `Emit` payload alone |
| 19 | `Determinism_EmitClaimReleaseSequence_IdenticalNotificationOrderAcrossRuns` | Determinism | Two identical runs of the same sequence of registry operations produce identical notification order and counts |

### Task 19 — asset-runtime-bus-adapter

| # | Test Name | Type | Description |
|---|-----------|------|-------------|
| 1 | `RegisterListener_SingleListener_ReceivesOnAssetReady` | Unit | Restoring original `event-notification.md` coverage — one listener gets `OnAssetReady` on `Registered→Staged` |
| 2 | `RegisterListener_SingleListener_ReceivesOnAssetUnloading` | Unit | One listener gets `OnAssetUnloading` on transition to `Unloading` |
| 3 | `RegisterListener_MultipleListeners_NotifiedInRegistrationOrder` | Unit | Two+ listeners fire in first-registered-first-notified order |
| 4 | `RegisterListener_DuplicatePointer_RejectedWithWarning` | Boundary | Registering the same listener pointer twice is a no-op with a logged warning |
| 5 | `RegisterListener_CapacityExceeded_LogsWarningAndFails` | Boundary | The 17th listener registration (capacity 16) fails cleanly with a warning, no overflow |
| 6 | `UnregisterListener_DuringDispatch_DeferredSafely` | Boundary | A listener unregistering itself (or another) mid-dispatch is deferred until the dispatch loop completes — no iterator invalidation |
| 7 | `OnAssetReady_ResolvedPathPassedCorrectly` | Unit | The `String512` resolved path delivered matches the manifest-resolved absolute path |
| 8 | `OnAssetReady_FiresOnceOnRegisteredToStaged_NotOnReStage` | Boundary | Re-staging an already-loaded asset does not re-fire `OnAssetReady` |
| 9 | `OnAssetLoadFailed_FiresOnAcknowledgeFailedWhileStaged` | Unit | Matches original spec AC for the failure path |
| 10 | `AssetRuntimeBusAdapter_OnAssetReady_BroadcastsAssetReadyEvent` | Unit | Adapter forwards to `Bus::Broadcast<AssetReadyEvent>` |
| 11 | `AssetRuntimeBusAdapter_OnAssetUnloading_BroadcastsAssetUnloadingEvent` | Unit | Adapter forwards unloading transitions |
| 12 | `AssetRuntimeBusAdapter_OnAssetLoadFailed_BroadcastsAssetLoadFailedEvent` | Unit | Adapter forwards load-failure |
| 13 | `AssetRuntimeBusAdapter_RegisteredAlongsideDirectListener_BothNotified` | Integration | A direct `IAssetStateListener` (e.g. a graphics system) and the bus adapter both fire from one transition |
| 14 | `IAssetLoadCallback_PrivateLoaderPath_UnaffectedByBusAdapter` | Regression | The pre-existing private `OnLoadComplete`/`OnLoadFailed` loader callback is untouched — proves the two mechanisms stay distinct |
| 15 | `AssetReadyUnloadingLoadFailedEvents_DeclaredAndCodegen_KTypeIdPresent` | Build | Three `.diagamemessages` types compile against `Bus::Broadcast` |
| 16 | `AssetRuntimeBusAdapter_NoDiaApplicationFlowDependency` | Build | Dependency check — adapter references only `DiaMessageBus`, never `DiaApplicationFlow` (SD-ARUN-001) |
| 17 | `BusSubscriber_ReceivesAssetReadyEvent_WithoutPollingIsAssetReady` | Integration | End-to-end proof of the design goal |
| 18 | `LateJoiningBusSubscriber_NoReplay_MustSelfServeViaGetStagedAssets` | Boundary | A subscriber registering after `RequestStageLoad` gets no replayed events and must call `GetStagedAssets()` — matches original late-join pattern, now also true for bus subscribers |

### Task 20 — clip-completion-bus-adapter

| # | Test Name | Type | Description |
|---|-----------|------|-------------|
| 1 | `OneShotClip_ReachesNaturalEnd_FiresOnClipFinished` | Unit | Playing a one-shot clip to completion fires `OnClipFinished` exactly once |
| 2 | `OneShotClip_ExplicitStopBeforeEnd_DoesNotFireOnClipFinished` | Boundary | Calling `Stop()` mid-playback must **not** fire `OnClipFinished` — the exact distinction called out as an AC |
| 3 | `OneShotClip_StopCalledOnCompletionFrame_FiresAtMostOnce` | Boundary | Race between natural completion and an external `Stop()` in the same `Update` does not double-fire |
| 4 | `LoopingClip_WrapsOnce_FiresOnClipLooped` | Unit | A looping clip crossing normalized time 1.0 fires `OnClipLooped` once per wrap |
| 5 | `LoopingClip_MultipleWrapsAcrossFrames_FiresOncePerWrap` | Unit | N wraps over N `Update` calls produce exactly N `OnClipLooped` notifications, no double-counting |
| 6 | `LoopingClip_NeverFiresOnClipFinished` | Boundary | Looping mode never fires the one-shot completion event, regardless of how many cycles run |
| 7 | `Subscribe_MultipleObservers_AllReceiveNotification` | Unit | Two+ subscribed `IAnimClipObserver`s both receive the same completion/loop event |
| 8 | `Unsubscribe_StopsReceivingFutureNotifications` | Unit | An unsubscribed observer receives nothing afterward |
| 9 | `AnimClipBusAdapter_NoOwnerSupplied_UsesBroadcast` | Unit | Default-constructed (invalid) owner entity → adapter calls `Broadcast`, per the resolved design decision that this is the primary path, not a fallback |
| 10 | `AnimClipBusAdapter_OwnerSupplied_PostsToEntityRouterAddress` | Unit | A valid owner entity handle → adapter calls `Post({kEntityRouterId, owner.bits}, ...)` |
| 11 | `AnimClipBusAdapter_DefaultConstructedEntity_TreatedAsInvalid` | Boundary | `Entity{}`'s `IsValid()` correctly gates the Broadcast-vs-Post branch |
| 12 | `ClipFinishedLoopedEvents_DeclaredAndCodegen_KTypeIdPresent` | Build | Both `.diagamemessages` types compile against `Bus::Post`/`Bus::Broadcast` |
| 13 | `ClipFinishedEvent_PayloadUsesStableClipId_NotRawPointer` | Invariant | Payload carries a stable clip identifier, not an `AnimClip*` — same class of bug as DiaEconomy's original raw-pointer issue, must not regress here |
| 14 | `PlayWhilePlaying_RestartsFromZero_ResetsCompletionState_NoStaleFinishedFire` | Boundary | Restarting a clip mid-playback doesn't carry over stale completion-detection state into a spurious `OnClipFinished` |
| 15 | `OneShotClip_UpdateAfterAlreadyFinished_DoesNotRefireOnClipFinished` | Boundary | Calling `Update` repeatedly after natural completion does not re-notify |
| 16 | `BusSubscriber_ReceivesClipFinishedEvent_WithoutPollingIsPlaying` | Integration | End-to-end proof of the design goal |
| 17 | `LoopNotification_DefaultExampleWiring_LeavesItUnsubscribed` | Integration | Per the open-question recommendation, verify any example/default wiring doesn't subscribe to `OnClipLooped` out of the box, avoiding unintended high-frequency bus traffic |

---

## Deferred — Inspector tier (needs a running-game connection)

The in-game visual debugger is in scope (Phase 6). Only the cross-PU / editor-connected pieces remain deferred, to return when live out-of-process debugging is prioritized.

| Task | Spec | Notes |
|------|------|-------|
| ledger-servicestream-export | folded into [frame-ledger.md](frame-ledger.md) (marked deferred) | `ServiceStream<LedgerSnapshot>` cross-PU export |
| live-inspector | _(not yet specced)_ | CluicheEditor connected to a running game via `DiaDebugServer` |
