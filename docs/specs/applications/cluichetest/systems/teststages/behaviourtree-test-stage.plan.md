**Spec:** @docs/specs/applications/cluichetest/systems/teststages/behaviourtree-test-stage.md
**Status:** In Progress

## Implementation Patterns

### Module base class
Inherits `TestStageModuleBase` (not `Module` directly). Override `OnStart(AutomationService*)`, `OnUpdate(float)`, `OnStop()`, `GetStageName()`, `GetBudgetFrames()`, `GetCheckpointNames()`. `PUAffinity::kSim`.

### VisualDebuggerModule wiring
`Dia::ApplicationFlow::ModuleRefV2<Cluiche::AppFlow::VisualDebuggerModule>` (note: V2). Lazy `RegisterDomain()` call in first `OnUpdate` frame when `!mDomainsRegistered`. `UnregisterDomain()` in `OnStop`. All debug code under `#ifdef DIA_DEBUG`.

### BehaviourTree action nodes
Actions are registered `ActionFn` callbacks: `NodeResult(*)(void* ctx, const DynamicArrayC<StringCRC,8>& params)`. Each guard's `BehaviourTreeComponent::SetActionContext(&guard)` so action fns receive a `GuardAgent*` via void*. Register all action IDs in a shared `ActionRegistry`.

### DiaOrder integration
`IOrder<GuardOrderContext>` interface: `GetOrderId()`, `Start(ctx)`, `bool Update(ctx, dt)` (true=done), `Finish(ctx)`, `Cancel(ctx)`. `OrderQueue<GuardOrderContext>::Update(ctx, dt)` drives order lifecycle each frame. Pre-allocate `GuardMoveOrder` instances (no heap alloc in action fns). `OrderQueue::Enqueue(IOrder*)` takes raw pointer — safe for stack/member allocated orders.

### BehaviourTree JSON format (decorator node)
```json
{ "type": "decorator", "decorator": "repeater", "repeat_count": 0, "break_on_failure": false, "child": "node_id" }
```
`repeat_count: 0` = infinite repeat. Parallel: `{ "type": "parallel", "policy": "require_all", "children": [...] }`.

### Checkpoint pattern
`service->RegisterCheckpoint(this, StringCRC("bt.name"), [this]() -> CheckpointResult { return {bool, "msg", 0.f}; });`
`GetCheckpointNames()` returns static array of all checkpoint StringCRCs for this stage.

### Manifest additions (cluiche_main.diaapp)
Stage must be added to: Boot `transitions[]`, new stage entry (with `"transitions": ["Boot"]`), `UIModule.stages[]`, `DebugPanelPageModule.stages[]`, `VisualDebuggerModule.stages[]`, `TestStageHUDModule.stages[]`, and a new SimPU module entry mirroring ArenaTestStageModule pattern.

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | `dia scaffold stage BehaviourTreeTestStage` — produces stub module + manifest files | Stage appears in Boot menu; `dia run cluichetest` compiles | Done | haiku | Scaffold created; cluiche_main.diaapp wired; vcxproj updated |
| 2 | Create `guard_bt.json` — tree definition with Selector/Sequence/Parallel/Repeater/Condition/Action nodes | JSON parses without error | Done | sonnet | All 5 node types; repeater uses `repeat_count: 0` |
| 3 | Implement module header (`BehaviourTreeTestStageModule.h`) + `GuardAgent` struct + DiaOrder types (`GuardOrderContext`, `GuardMoveOrder`) | Compiles; types are self-consistent | Done | sonnet | GuardAgent has waypoints[], chaseTarget, ownMoveOrder*, orderInFlight, targetLostFrames; ModuleRef (not V2) |
| 4 | Implement `BehaviourTreeTestStageModule.cpp` — `OnStart`: load tree asset, init 3 guards, register action fns, set blackboard slots, register checkpoints | Module builds | Done | sonnet | Inline JSON; shared BT asset; ActionFn static callbacks with guard->modulePtr |
| 5 | Implement `OnUpdate`: advance wanderer, update guard blackboard bool slots, tick BTs, `orderQueue.Update(ctx, dt)`, latch checkpoint flags | Guards patrol/alert/chase; checkpoints latch | Done | sonnet | target_visible/has_target bool slots set before BT tick; kTargetLostThreshold=60 frames |
| 6 | Implement `OnStop`: unregister domain, reset BT components, clear order queues, unregister checkpoints | Module stops cleanly | Done | haiku | |
| 7 | Register 4 metrics in `OnStart`; emit updated values in `OnUpdate` | Metrics readable via automation | Done | haiku | cluichetest.bt.{total_chases,patrol_waypoints,decorator_cycles,total_frames} |
| 8 | Wire `BehaviourTreeVisualDebugger` for Guard 0: create in OnStart, lazy RegisterDomain in first OnUpdate, UnregisterDomain in OnStop | Debugger panel shows live BT node traversal for Guard 0 | Done | sonnet | Guard 0 BT component registered as event listener; lazy domain registration |
| 9 | Write pytest scenario `test_bt_e2e.py` + add to `default.json` | Scenario listed in `dia orchestrate --list` | Done | sonnet | Follow arena test pattern |
| 10 | Final verification: `dia run cluichetest`, navigate to BehaviourTreeTestStage; all 5 checkpoints pass | All checkpoints pass; metrics match minimums | Pending | sonnet | Gate task — no code changes |
