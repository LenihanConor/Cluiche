# Feature Spec: BehaviourTree Test Stage

## Parent System
@docs/specs/applications/cluichetest/systems/teststages/teststages.md

## Problem Statement

DiaBehaviourTree has comprehensive unit test coverage but has never been exercised in a live SimPU scenario with multiple independent agent instances, real DiaOrder dispatch, and a visual debugger panel showing live node traversal. This stage validates the full runtime loop: shared tree asset, per-guard blackboard instances, priority switching between Patrol/Alert/Chase branches, and all five node types (Selector, Sequence, Parallel, Decorator, Condition/Action) active in one run.

## Template Answers (T1–T10)

| # | Question | Answer |
|---|----------|--------|
| T1 | Engine features exercised | DiaBehaviourTree (Selector, Sequence, Parallel, Decorator:Repeat, Condition nodes, Action nodes); DiaBehaviourTreeVisualDebugger (IDebugDomain panel, live lastTickNodes, node result badges); DiaBlackboard (per-guard blackboard); DiaOrder (MoveToWaypoint + MoveToTarget actions dispatch) |
| T2 | Scene setup in DoStart | 3 guard agents at fixed starting positions, each with a 2-waypoint patrol route; 1 wanderer agent on a deterministic scripted path crossing each guard's patrol zone in sequence; shared `BehaviourTreeAsset` loaded from `guard_bt.json`; each guard owns an independent `BehaviourTreeComponent` + `Blackboard`; Guard 0's component attached to a `BehaviourTreeVisualDebugger` registered with `DiaDebugDomainRegistry` |
| T3 | Checkpoints and success conditions | `bt.patrol_started` — all 3 guards tick BT at least once in Patrol branch (Repeat decorator entered). `bt.chase_triggered` — at least 1 guard transitions to Chase after wanderer enters sight radius. `bt.target_lost` — a guard that was chasing clears `has_target` and returns to Patrol. `bt.parallel_fired` — Chase Parallel node's both child actions (MoveToTarget + SetAlertEffect) fire in the same tick. `bt.multi_state_divergence` — at least 2 guards are in different BT states (one Patrolling, one Chasing) at the same frame |
| T4 | Metrics emitted | `cluichetest.bt.total_chases` (int, total number of times any guard entered Chase), `cluichetest.bt.total_patrol_waypoints` (int, total waypoint completions across all guards), `cluichetest.bt.decorator_cycles` (int, total Repeat decorator cycles), `cluichetest.bt.total_frames` (int, for determinism assertion) |
| T5 | Processing Unit | SimPU — game/AI logic runs on sim thread per platform rule (AD-001 CT) |
| T6 | Assets needed | `guard_bt.json` — shared tree definition (hand-authored JSON); no binary assets |
| T7 | Gap vs unit tests | Unit tests call BT API directly with synthetic inputs; they cannot verify: real-time branch switching as blackboard state changes across frames, multiple independent BT instances sharing one asset, DiaOrder dispatch from Action nodes, or the visual debugger's live node traversal in a running stage |
| T8 | Determinism constraints | Fixed-timestep SimPU (30Hz). Wanderer path is a hardcoded array of Vec2 positions advanced one step per frame — no randomness. Guard movement is deterministic (fixed speed + linear interpolation). BT ticks in fixed guard order each frame. |
| T9 | Expected frame budget | ~360 frames (12s at 30Hz): wanderer completes 3 zone crossings (~4s each), each triggering a guard chase + lose-target cycle. Budget: 600 frames (20s) |
| T10 | Dependencies on other modules | AutomationModule (checkpoints), BehaviourTreeComponent, BehaviourTreeAsset, IBehaviourTreeEventListener, BehaviourTreeVisualDebugger, DiaDebugDomainRegistry, DiaBlackboard, DiaOrder, VisualDebuggerModule (debug panel wiring) |

## Acceptance Criteria

### Shared ACs (inherited from Infrastructure spec)

This stage satisfies AC-S1 through AC-S9 as defined in the [Infrastructure spec](test-stage-infrastructure.md#a--shared-acs-every-test-stage-must-satisfy-these).

### Stage-Specific ACs

| # | Criterion | Verification |
|---|-----------|--------------|
| AC-BT1 | 3 guard agents share a single `BehaviourTreeAsset` but each owns an independent `BehaviourTreeComponent` and `Blackboard` | Three distinct component instances; mutating one guard's blackboard does not affect others |
| AC-BT2 | Root Selector switches branches based on `has_target` and `target_visible` Condition nodes reading per-guard blackboard slots | Debugger panel shows Selector child index switching across ticks |
| AC-BT3 | A guard enters Chase when the wanderer enters its sight radius (≤ 4 units); `has_target` slot flips true; `bt.chase_triggered` checkpoint passes | checkpoint + per-guard state label shows "CHASING" |
| AC-BT4 | A guard returns to Patrol when the wanderer exits range (≥ 6 units); `has_target` clears; `bt.target_lost` checkpoint passes | `bt.target_lost` passes; state label reverts to "PATROLLING" |
| AC-BT5 | Patrol branch uses a `Decorator:Repeat` node that loops the `MoveToWaypoint` action; `bt.patrol_started` checkpoint passes after at least one Repeat cycle per guard | `cluichetest.bt.decorator_cycles` metric ≥ 3 (one per guard) |
| AC-BT6 | Chase branch uses a `Parallel` node that fires both `MoveToTargetAction` (dispatches MoveOrder) and `SetAlertEffectAction` in the same tick; `bt.parallel_fired` checkpoint passes | lastTickNodes in debugger panel contains Parallel node with two child results in same tick |
| AC-BT7 | `MoveToWaypointAction` and `MoveToTargetAction` dispatch via `DiaOrder`; orders are processed by the stage's movement executor each frame | Guards visibly move; DiaOrder dispatch confirmed in action node implementation |
| AC-BT8 | Each guard displays a state label overlaid above their world rect: "PATROLLING", "ALERT", or "CHASING" — updated every frame from the `state_label` blackboard slot | Visually readable on screen during any manual run |
| AC-BT9 | Guard 0's `BehaviourTreeComponent` is attached to a `BehaviourTreeVisualDebugger` registered with `DiaDebugDomainRegistry`; the panel shows live `lastTickNodes` with result badges for Guard 0's tree ticks | Panel visible on screen; node result badges update in real time |
| AC-BT10 | At least 2 guards are in different BT states simultaneously at some frame; `bt.multi_state_divergence` checkpoint passes | Wanderer path is designed so Guard 1 enters Chase while Guard 2 is still Patrolling |

## Design

### Scene Layout

```
World bounds: (−12, −12) → (12, 12)

Guard 0:  start (−8, 0),  patrol route: (−10, −3) ↔ (−5, 3)   — sight radius 4.0
Guard 1:  start (0, 7),   patrol route: (−3,  4) ↔ (3, 10)    — sight radius 4.0
Guard 2:  start (8, 0),   patrol route: (5, −3) ↔ (10, 3)     — sight radius 4.0

Wanderer: scripted path — visits each guard zone in sequence:
  Frame   0–90:  (−9, 0) → (−4, 0)   (crosses Guard 0 zone)
  Frame 120–210: (−3, 7) → (4, 7)    (crosses Guard 1 zone)
  Frame 240–330: (5, 0)  → (10, 0)   (crosses Guard 2 zone)

Visual (ImGui drawList):
  Guard PATROLLING: blue filled rect (0.8 units)
  Guard ALERT:      yellow filled rect — briefly shown before Chase
  Guard CHASING:    red filled rect
  Wanderer:         white filled rect (0.6 units)
  Sight radius:     translucent circle per guard (radius = 4 units)
  State label:      text overlay above each guard: "PATROLLING" / "ALERT" / "CHASING"
  Right sidebar:    per-guard BT state, current active node, last result
```

### Behaviour Tree Definition (`guard_bt.json`)

```
Root: Selector
  ├── Sequence [Chase Branch]
  │     ├── Condition: CheckHasTarget   (reads has_target == true)
  │     └── Parallel (RequireAll)
  │           ├── Action: MoveToTargetAction    [DiaOrder dispatch]
  │           └── Action: SetAlertEffectAction  [sets state_label = "CHASING"]
  ├── Sequence [Alert Branch]
  │     ├── Condition: CheckTargetVisible  (reads target_visible bool slot)
  │     └── Action: SetHasTargetAction     [sets has_target = true, state_label = "ALERT"]
  └── Decorator:Repeat [Patrol Branch]
        └── Action: MoveToWaypointAction  [DiaOrder dispatch; advances waypoint index on arrival]
```

### Guard Agent Structure

```cpp
struct GuardAgent
{
    Dia::Maths::Vec2 position;
    Dia::Maths::Vec2 patrolWaypoints[2];
    int              currentWaypoint = 0;

    Dia::BehaviourTree::BehaviourTreeComponent btComponent;
    Dia::Blackboard::Blackboard                blackboard;
    // Blackboard slots: has_target (bool), target_visible (bool),
    //                   state_label (StringCRC), current_waypoint_index (int)

    // DiaOrder: each guard owns an OrderQueue; action nodes enqueue; DoUpdate drains
    Dia::Order::OrderQueue<GuardOrderContext> orderQueue;

    bool pendingMove = false;
    Dia::Maths::Vec2 moveTarget{0.0f, 0.0f};
};
```

### Per-Guard Blackboard Slots

Condition nodes in `DiaBehaviourTree` read only **bool** slots (`TryGet<bool>`). Float/int values cannot be condition keys. All condition-driving state must be pre-computed as bools by the module before each BT tick.

| Slot | Type | Set by |
|------|------|--------|
| `has_target` | bool | `SetHasTargetAction` on acquire; module clears when range > 6.0 |
| `target_visible` | bool | Module each frame: `distance(guard.pos, wanderer.pos) ≤ 4.0` |
| `state_label` | StringCRC | Action nodes (values: `"PATROLLING"`, `"ALERT"`, `"CHASING"`) |
| `current_waypoint_index` | int | `MoveToWaypointAction` on waypoint arrival |

`target_distance` is tracked internally by the module (float) but is not a blackboard slot — condition nodes only see the derived `target_visible` bool.

### DiaOrder Integration

The real DiaOrder observer interface is `IOrderQueueObserver<TContext>` (not `IOrderReceiver`). `MoveOrder` does not exist in the engine — it is a new application-level type defined in this stage. No DiaEntity is required; `OrderQueue` is fully generic on `TContext`.

Each guard owns an `OrderQueue<GuardOrderContext>`. Action nodes call `guard.orderQueue.Enqueue(GuardMoveOrder{destination})`. DoUpdate drains the queue each frame after all BT ticks.

```cpp
// Application-level types defined in BehaviourTreeTestStageModule.h
struct GuardOrderContext
{
    GuardAgent* agent = nullptr;
};

class GuardMoveOrder : public Dia::Order::IOrder<GuardOrderContext>
{
public:
    static constexpr Dia::Core::StringCRC kTypeId{"GuardMoveOrder"};
    explicit GuardMoveOrder(Dia::Maths::Vec2 dest) : mDestination(dest) {}
    Dia::Core::StringCRC GetTypeId() const override { return kTypeId; }
    Dia::Maths::Vec2 GetDestination() const { return mDestination; }
private:
    Dia::Maths::Vec2 mDestination;
};

// Observer that drains orders into GuardAgent movement state
class GuardOrderObserver : public Dia::Order::IOrderQueueObserver<GuardOrderContext>
{
public:
    explicit GuardOrderObserver(GuardAgent& g) : mGuard(g) {}
    void OnOrderStarted(const Dia::Order::IOrder<GuardOrderContext>& order) override
    {
        if (order.GetTypeId() == GuardMoveOrder::kTypeId)
        {
            mGuard.pendingMove = true;
            mGuard.moveTarget  = static_cast<const GuardMoveOrder&>(order).GetDestination();
        }
    }
private:
    GuardAgent& mGuard;
};
```

`GuardOrderObserver` is registered via `guard.orderQueue.AddObserver(observer)`. Movement applied after all BT ticks: `position += normalize(target - position) * 2.0f * dt`.

### DoUpdate Frame Loop

```
Each frame:
  1. Advance wanderer position (step along scripted path)
  2. For each guard:
     a. Compute target_distance = distance(guard.pos, wanderer.pos)
     b. Update blackboard bool slots: target_visible = (target_distance ≤ 4.0);
        if has_target && target_distance > 6.0: clear has_target (target lost)
     c. Tick BehaviourTreeComponent
     d. Process pending MoveOrder (move guard position)
  3. Check and latch checkpoint conditions
  4. Emit metrics snapshot
  5. DrawDebugImGui (rects, labels, sight circles, sidebar)
  6. Increment mFrameCount
```

### Module Structure

```cpp
// Cluiche/CluicheTest/Modules/TestStages/BehaviourTreeTestStageModule.h
namespace CluicheTest {

class BehaviourTreeTestStageModule : public Dia::ApplicationFlow::Module
{
public:
    static constexpr Dia::Core::StringCRC kTypeId{"BehaviourTreeTestStageModule"};
    explicit BehaviourTreeTestStageModule(const Dia::Core::StringCRC& instanceId);

protected:
    StartResult DoStart() override;
    void        DoUpdate(float deltaTime) override;
    StopResult  DoStop() override;

private:
    void SetupGuards();
    void RegisterCheckpoints(Dia::Automation::AutomationService& automation);
    void RegisterMetrics();
    void UpdateGuardBlackboards();
    void TickBehaviourTrees(float dt);
    void ApplyMovement(float dt);
    void DrawDebugImGui();

    Dia::ApplicationFlow::ModuleRef<AutomationModule>                  mAutomation{this};
    Dia::ApplicationFlow::ModuleRef<Cluiche::AppFlow::VisualDebuggerModule> mVisualDebugger{this};

    Dia::BehaviourTree::BehaviourTreeAsset mSharedTreeAsset;

    static constexpr int kGuardCount = 3;
    GuardAgent           mGuards[kGuardCount];
    GuardOrderObserver   mOrderObservers[kGuardCount];

    // Visual debugger for Guard 0 — registered with VisualDebuggerModule on first DoUpdate
    std::unique_ptr<Dia::BehaviourTree::BehaviourTreeVisualDebugger> mBTDebugger;
    bool mDomainsRegistered = false;

    Dia::Maths::Vec2 mWandererPosition{-9.0f, 0.0f};
    int              mWandererPathIndex = 0;

    // Checkpoint latches
    bool mPatrolStarted       = false;
    bool mChaseTriggered      = false;
    bool mTargetLost          = false;
    bool mParallelFired       = false;
    bool mMultiStateDivergence = false;

    // Metrics
    int  mTotalChases          = 0;
    int  mTotalPatrolWaypoints = 0;
    int  mDecoratorCycles      = 0;
    unsigned int mFrameCount   = 0;
};

} // namespace CluicheTest
DIA_MODULE(BehaviourTreeTestStageModule);
```

### Checkpoint Registration

```cpp
automation->RegisterCheckpoint(this, StringCRC("bt.patrol_started"),
    [this]() -> CheckpointResult {
        return { mPatrolStarted, mPatrolStarted ? "all guards patrolling" : "not yet", 0.0f };
    });

automation->RegisterCheckpoint(this, StringCRC("bt.chase_triggered"),
    [this]() -> CheckpointResult {
        return { mChaseTriggered, mChaseTriggered ? "chase active" : "not yet", 0.0f };
    });

automation->RegisterCheckpoint(this, StringCRC("bt.target_lost"),
    [this]() -> CheckpointResult {
        return { mTargetLost, mTargetLost ? "guard returned to patrol" : "not yet", 0.0f };
    });

automation->RegisterCheckpoint(this, StringCRC("bt.parallel_fired"),
    [this]() -> CheckpointResult {
        return { mParallelFired, mParallelFired ? "parallel both actions fired" : "not yet", 0.0f };
    });

automation->RegisterCheckpoint(this, StringCRC("bt.multi_state_divergence"),
    [this]() -> CheckpointResult {
        return { mMultiStateDivergence, mMultiStateDivergence
            ? "guards in different states" : "not yet", 0.0f };
    });
```

### Pytest Scenario

```python
# Tools/orchestrator/scenarios/cluichetest/behaviourtree/test_bt_e2e.py

def test_behaviour_tree_guard_patrol_chase(dia_client):
    dia_client.navigate_to("BehaviourTreeTestStage")

    r = dia_client.poll_checkpoint("bt.patrol_started", timeout_s=5.0)
    assert r["passed"], f"Guards never started patrolling: {r['message']}"

    r = dia_client.poll_checkpoint("bt.chase_triggered", timeout_s=10.0)
    assert r["passed"], f"No guard entered chase: {r['message']}"

    r = dia_client.poll_checkpoint("bt.parallel_fired", timeout_s=12.0)
    assert r["passed"], f"Parallel node did not fire both actions: {r['message']}"

    r = dia_client.poll_checkpoint("bt.multi_state_divergence", timeout_s=15.0)
    assert r["passed"], f"Guards never diverged to different states: {r['message']}"

    r = dia_client.poll_checkpoint("bt.target_lost", timeout_s=18.0)
    assert r["passed"], f"Guard never lost target and returned to patrol: {r['message']}"

    metrics = dia_client.query_metrics("cluichetest.bt.*")
    assert metrics["cluichetest.bt.total_chases"] >= 1
    assert metrics["cluichetest.bt.total_patrol_waypoints"] >= 3
    assert metrics["cluichetest.bt.decorator_cycles"] >= 3

    # Determinism: second pass must match frame count
    first_frames = metrics["cluichetest.bt.total_frames"]
    dia_client.navigate_to("Boot")
    dia_client.navigate_to("BehaviourTreeTestStage")
    dia_client.poll_checkpoint("bt.target_lost", timeout_s=25.0)
    metrics2 = dia_client.query_metrics("cluichetest.bt.*")
    assert metrics2["cluichetest.bt.total_frames"] == first_frames, \
        f"Non-deterministic: {first_frames} vs {metrics2['cluichetest.bt.total_frames']}"

    dia_client.navigate_to("Boot")
```

## Files Touched

| File | Change |
|------|--------|
| `Cluiche/CluicheTest/Modules/TestStages/BehaviourTreeTestStageModule.h` | New — module header + GuardAgent + GuardOrderReceiver |
| `Cluiche/CluicheTest/Modules/TestStages/BehaviourTreeTestStageModule.cpp` | New — full implementation |
| `Cluiche/CluicheTest/CluicheTest.vcxproj` | Add ClCompile + ClInclude entries |
| `Cluiche/CluicheTest/CluicheTest.vcxproj.filters` | Add filter entries |
| `Cluiche/Assets/CluicheTest/Stages/BehaviourTreeTestStage/behaviourtree_test_stage.diastage` | New — stage declaration |
| `Cluiche/Assets/CluicheTest/Stages/BehaviourTreeTestStage/misc/ApplicationFlow/behaviourtree_test_stage.diaapp` | New — stage app manifest |
| `Cluiche/Assets/CluicheTest/Stages/BehaviourTreeTestStage/guard_bt.json` | New — shared tree definition |
| `Cluiche/Assets/CluicheTest/Global/misc/ApplicationFlow/cluiche_main.diaapp` | Add BehaviourTreeTestStage + Boot↔stage transitions |
| `Cluiche/Assets/CluicheTest/cluichetest.diagame` | Add import for behaviourtree_test_stage.diastage |
| `Cluiche/Assets/CluicheTest/assets.catalogue.json` | Add stage + manifest catalogue entries |
| `Tools/orchestrator/scenarios/cluichetest/behaviourtree/test_bt_e2e.py` | New — pytest scenario |
| `Tools/orchestrator/plans/cluichetest/default.json` | Add scenario to plan |

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | `dia scaffold stage BehaviourTreeTestStage` — produces 9 touch points | Stage appears in Boot menu | Todo | haiku | Exact scaffold command |
| 2 | Create `guard_bt.json` — tree definition: Root Selector → Chase Sequence (CheckHasTarget + Parallel[MoveToTarget + SetAlertEffect]) → Alert Sequence (CheckTargetVisible + SetHasTarget) → Repeat[MoveToWaypoint] | JSON parses without error; asset loads in DoStart | Todo | sonnet | All 5 node types present: Selector, Sequence, Parallel, Decorator:Repeat, Condition, Action |
| 3 | Implement `GuardAgent` + `GuardOrderReceiver` structs; implement `DoStart`: load tree asset, init 3 guards with BehaviourTreeComponent + Blackboard + patrol routes, init wanderer, wire BehaviourTreeVisualDebugger for Guard 0, register checkpoints | Module reaches kReady; BT debugger registered with DiaDebugDomainRegistry | Todo | sonnet | Confirm IOrderReceiver registration API before this task |
| 4 | Implement Condition nodes: `CheckHasTargetCondition`, `CheckTargetVisibleCondition` (reads target_distance ≤ sight_radius from blackboard) | Unit test: returns correct result for blackboard values above/below threshold | Todo | sonnet | |
| 5 | Implement Action nodes: `MoveToWaypointAction` (dispatch MoveOrder → advance waypoint index on arrival), `MoveToTargetAction` (dispatch MoveOrder toward wanderer), `SetHasTargetAction`, `SetAlertEffectAction` (set state_label blackboard slot) | Unit test: dispatched orders processed correctly; blackboard slots updated | Todo | sonnet | DiaOrder dispatch pattern confirmed in Task 3 |
| 6 | Implement `DoUpdate`: advance wanderer, update guard blackboards, clear has_target on range loss, tick all BTs, apply movement from orders, latch checkpoint flags, draw ImGui (rects + labels + sight circles + sidebar) | State labels update on screen; guards visibly chase and return | Todo | sonnet | `ImGui::GetBackgroundDrawList()` for world rects; sidebar via ImGui window |
| 7 | Register 4 metrics in DoStart; emit in DoUpdate | Metric query returns correct final counts | Todo | haiku | |
| 8 | Implement `DoStop`: clear BT components, reset blackboards, reset all counters and flags, unregister BT debugger | Module stops cleanly; second run produces identical results | Todo | haiku | |
| 9 | Write pytest scenario `test_bt_e2e.py` + add to `default.json` | `dia orchestrate --list` shows scenario; full scenario passes | Todo | sonnet | Includes determinism second-pass assertion |
| 10 | Verify: `dia run cluichetest`, navigate to BehaviourTreeTestStage; all 5 checkpoints pass; debugger panel shows live node traversal | All checkpoints pass; metrics match expected minimums | Todo | sonnet | Final E2E gate |

## Dependencies

- **DiaBehaviourTree** — Done ✓ (all 13 tasks complete)
- **DiaBehaviourTreeVisualDebugger** — must be Approved and implemented before Task 3
- **DiaBlackboard** — Done ✓
- **DiaOrder** — confirm `IOrderReceiver` registration API and `MoveOrder` shape before Task 3
- **Test Stage Infrastructure** — Tasks 1–6 must be complete (Boot menu, checkpoint infrastructure, VisualDebuggerModule wiring)
- **VisualDebuggerModule** — confirm whether BehaviourTreeTestStage can reuse the existing `VisualDebuggerModule` pattern from `DebugGalleryTestStage`, or whether additional wiring is needed to show the debug panel alongside this stage

## Resolved Design Questions

1. **DiaOrder receiver registration** ✅ — `IOrderReceiver` does not exist in the engine. The real interface is `IOrderQueueObserver<TContext>` with `queue.AddObserver(observer)`. No DiaEntity required — `OrderQueue` is generic. `MoveOrder` also does not exist; `GuardMoveOrder : IOrder<GuardOrderContext>` is defined as an application-level type in this stage. See DiaOrder Integration section.

2. **Condition node pattern** ✅ — Condition nodes in `DiaBehaviourTree` read **only bool slots** via `TryGet<bool>(blackboardKey)`. Float values cannot be condition keys. The module pre-computes `target_visible` (bool) from `target_distance` (float) before each BT tick, writing it to the blackboard. This is idiomatic — blackboard as pre-computed shared memory. No service indirection needed.

3. **Debug panel wiring** ✅ — `VisualDebuggerModule` (in `Cluiche::AppFlow::` namespace, header `CluicheGameBaseline/Modules/VisualDebuggerModule.h`) is referenced via `ModuleRef`. Domains are registered lazily on the first `DoUpdate` call (same pattern as `DebugGalleryTestStageModule`). The panel is **not** auto-shown — it requires user interaction or an explicit `DebugPanelSetVisibilityEvent`. The stage fires this event in `DoStart` to auto-open the panel when the stage loads.

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-001 | StringCRC for IDs | `kTypeId`, checkpoint names, blackboard slot names, node IDs, order type IDs all StringCRC |
| PD-004 | No STL in public APIs | Fixed-size arrays for guards and waypoints; `std::function` in checkpoint callbacks only |
| PD-006 | VS project files source of truth | Task 1 scaffold + explicit vcxproj/filters additions |
| PD-007 | C++20 required | `constexpr StringCRC`, standard features |
| PD-010 | .diastage for stages | Stage declared in `.diastage`; assets under stage directory |
| AD-001 (CT) | Three PUs | Module lives on SimPU (game/AI logic on sim thread) |
| AD-004 (CT) | Test levels included | This is a test level |
| AD-005 (CT) | App is testbed not product | Stage exists purely for BT/DiaOrder/debugger validation |
| SD-TS-001 | One manifest stage per feature | One stage: BehaviourTreeTestStage |
| SD-TS-002 | Checkpoints in DoStart, auto-clear on stop | RegisterCheckpoints called in DoStart after asset loads |
| SD-TS-003 | Metrics for threshold assertions | 4 metrics registered; pytest asserts minimums |
| SD-TS-004 | All stages return to Boot | `transitions: ["Boot"]` in manifest |
| SD-TS-005 | Individual stages are feature specs | This spec IS the feature spec |

## Status

**Status:** Done
