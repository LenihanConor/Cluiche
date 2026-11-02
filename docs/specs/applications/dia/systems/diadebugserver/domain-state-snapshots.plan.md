**Spec:** @docs/specs/applications/dia/systems/diadebugserver/domain-state-snapshots.md
**Status:** In Progress

## Implementation Patterns

- Precedent: `Dia/DiaCellLayers/ViewerStateSnapshot.{h,cpp}` (publisher armed by demand, FrameStream slot with `simFrame`, `ViewerStateHealth`, metrics). Copy its shape; do not invent a new one.
- Producer: `VisualDebuggerModule::DoUpdate` (Sim). Consumer: `DebugPanelPageModule` (Main). Today Main reads a `ServiceStreamReader<DiaDebugDomainRegistry>` (`mDomainRegistry`) and calls `BuildDomainStatesRoot` / `QueryDomainState` on it.
- Pure logic (packer, budget, whole-domain truncation) lives in `Dia/DiaVisualDebugger/Console/` so it is unit-testable without streams; the module only wires streams.
- Watch for `ViewerStateAdapter` (CellLayers), which is registered as a domain on the Main side; Task 1 must state how it interacts with the new snapshot.
- Scoped runs while iterating: `dia run googletest --filter="<Suite>*" --shards 2`. One full run only at Finalize.
- Escalation rule: any new header in Dia (DiaVisualDebugger) is covered by the AD-001 sign-off in the spec header.

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Audit the 14 domains (thread assumptions, payload size), measure total payload, fix the snapshot type and budget; write findings into this plan | Measured sizes recorded in Notes | Done | sonnet | Audited + measured: drawers-only domain 333 B; MessageBus 6 KB at 60-tick window but 207 KB at 3600 (history always emitted); typical worst ~75 KB. Recommend 128 KB budget + cap MB history. See Task 1 Findings |
| 2 | `DomainStateSnapshot`, packer (whole-domain budget, truncated flag), pure unit tests (ACs 3, 6) | RED-GREEN: packer unit tests, golden JSON | Pending | sonnet | |
| 3 | `VisualDebuggerModule` producer, interest handling, metrics and health (ACs 1, 5, 7, 10, 11) | RED-GREEN: thread-id domain test, interest window test | Pending | opus | Confirm with user before dispatching on opus |
| 4 | `DebugPanelPageModule` and `QueryDomainState` read the snapshot; remove the registry reader (ACs 2, 4) | Existing `TestDomainStateQuery` cases against the snapshot form | Pending | sonnet | |
| 5 | Remove the `UtilityAIDebugDomain` atomic and fix the `MessageBusDebugDomain` threading comment (ACs 8, 9) | Toggle via command stream test | Pending | haiku | |
| 6 | Stress test and `dia check sanitizer` (AC 12) | Sanitizer run | Pending | sonnet | |
| 7 | Exhaustive test pass (gap audit, edge tests, mutation check) | Mutation check | Pending | sonnet | |
| 8 | Docs: module docs, registry, close the backlog row | `dia check deps`, `dia docs registry` | Pending | haiku | |

## Task 1 Findings

Audit covers all 38 `IDebugDomain` implementations found (not 14): baseline registers Coord2D/3D, Entity, MessageBus, Attribute, Physics2D (RigidBody2D), GameSpeed, F2TestRunner; DebugGallery registers ~25 more. All registrants are Sim modules (`F2TestRunnerModule`, `GameSpeedControllerModule` are `SimModule`), so moving `GetJSONState` to SimPU is the natural thread.

### Per-domain thread audit

- Drawer-only domains (Geometry2D, AssetRuntime, RigidBody2D, SoftBody2D, Lighting3D, IK2D, Rig2D, Animation2D, Scene2D, Mesh3D, Coord2D, Coord3D, Entity): read `DebugLayerManager::IsLayerEnabled` (Sim-owned) plus Sim-owned state. No mutation. Safe on Sim.
- Atomic-toggle domains (AIBudget, Callout, Blackboard, EntitySpatial, FlowField, HTN, Mailbox, Pathfinding, Rules, ScalarField, StateMachine, Steering, UtilityAI, Attribute, BehaviourTree is a plain bool): read-only, safe. SURPRISE: 14 domains carry a `std::atomic<bool>` only because Main reads them; spec AC 8/13 only name UtilityAI. Decide in Task 5 whether to sweep all (cheap, mechanical) or leave.
- Blackboard: `VisitSlots`, bounded by `kMaxSlots = 32`; Sim-owned. Safe.
- UtilityAI: `GetLastFrameScores`, bounded 32 actions. Safe.
- AIBudget: reads scheduler result; per-system list only when drawer enabled. Safe.
- MessageBus: reads `GetLastTickLedger()` / `LedgerHistory` / registered-type tables (std::vector copies inside). No mutation; `mActiveTab` and `mHistoryWindowTicks` are mutated by `OnCommand` on Sim. Safe on Sim (and unsafe on Main today).
- Attribute: MUTATES inside `GetJSONState` (rebinds observer via `Subscribe/Unsubscribe`, `mDirty`, `mFrameCounter`, `mCachedState`, `mRebuildCount`) and reads the Entity domain. Real race on Main today; correct on Sim. Behaviour change: `kConditionalPollIntervalFrames` counts `GetJSONState` calls, so it becomes "Sim frames with interest" rather than Main frames.
- GameSpeed / F2TestRunner (CoW): read controller / runner state, Sim modules. Safe.
- CellLayerViewerDomain: `GetJSONState` mutates (`mHotkeys.Rebuild`) but is Main-owned and called only by the viewer publisher on Main, not via the registry. Out of scope.
- No domain uses UI-thread-only data, window handles or statics.

### ViewerStateAdapter interaction

The adapter is NOT registered on the Main side: `CellLayersStateRelayModule` (Sim) builds it and registers it in the Sim registry, so the new packer simply calls it on Sim like any domain. Effects: (1) `RequestState()` demand now fires only while panel interest is active (good: chains demand); (2) the viewer's nested JSON (up to `ViewerStateSlot::kCapacity` = 32 KB) counts against the snapshot budget and is parsed on Sim instead of Main; (3) data now crosses twice (Main viewer -> Sim adapter -> snapshot -> Main panel), two frames of latency; (4) fix the stale "Main thread only" comment on `mText` in `ViewerStateSnapshot.h`. No code change needed otherwise.

### Measured payloads (compact, `Json::FastWriter`, via scratch gtest, since deleted)

| Domain | Compact bytes |
|---|---|
| Drawer-only (Coord2D, incl. `_id/_displayName/_group/_description/_accent`) | 333 |
| Blackboard, 32 slots (max), drawer enabled | 2,900 |
| MessageBus, 12 types, 3600-tick history, window 60 | 5,991 (schema 1,173, live 1,133, history 3,421) |
| MessageBus, same, window 3600 | 206,663 (history 204,091) |
| UtilityAI (<=32 actions), AIBudget (<=32 systems), Attribute (one entity) | estimated from code: 2-3 KB, 2 KB, 5-15 KB (not measured, needs live entity) |

Per-message-type MessageBus cost is about 100 B schema + 95 B live, so 50 types is about 10 KB. Estimated typical worst case (DebugGallery: ~25 drawer domains at 8 KB + MessageBus 12 KB + Blackboard 3 KB + UtilityAI 3 KB + AIBudget 2 KB + Attribute 15 KB + CellLayers viewer up to 32 KB) is about 75 KB.

### Recommendations

- `kMaxSnapshotBytes`: 128 KB (about 1.7x worst-case headroom). 192 KB also works but has no data behind it.
- FrameStream slot: `FrameStreamStore<T>` keeps `kRingCapacity = 8` slots of `T` and `Write` copies the whole `T` (not `jsonBytes`). 128 KB gives about 1 MB resident and a roughly 128 KB memcpy per published frame; acceptable only because production is interest-gated. The producer MUST heap-allocate the scratch `DomainStateSnapshot` (as `CellLayersStateRelayModule` does with `mSlot`), never put it on the stack. Main should cache the parse by `simFrame` and skip re-parsing unchanged frames.
- Open design question 1 (per-domain topics): NOT needed, provided MessageBus history is bounded. MessageBus `setHistoryWindow` allows up to 3600 ticks (about 204 KB alone) and `GetJSONState` emits Schema, Live and History tabs regardless of `activeTab`. Required decision for Task 2/3 (needs user sign-off, MessageBus owner): either emit only the active tab, or cap the emitted history to a few hundred ticks (600 ticks about 34 KB). Otherwise one domain starves the rest and the whole-domain truncation drops everything after it.

### Surprises that change later tasks

1. `PanelController::Process` -> `PaletteIndex::Build(const DiaDebugDomainRegistry&)` also does `VisitAll` on Main (`DebugPanelPageModule.cpp:94`). Task 4 must rebuild the palette from the parsed snapshot (it needs only `_displayName/_description/_group/_id`, all in the envelope), not just `BuildDomainStatesRoot` and `QueryDomainState`.
2. The packer should run `BuildDomainState` (envelope included) on Sim so the snapshot is the final per-domain panel JSON; Main then only wraps in `simFrame`.
3. 14 atomic-toggle domains (see above) versus AC 8/13 scope.
4. Domain count is 38, not 14; AC 3 golden test should use a representative subset.
