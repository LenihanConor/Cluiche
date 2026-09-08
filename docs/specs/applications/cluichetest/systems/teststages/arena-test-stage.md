# Feature Spec: Arena Test Stage

## Parent System
@docs/specs/applications/cluichetest/systems/teststages/teststages.md

## Traceability

| Level | Spec | Decision IDs honored |
|-------|------|---------------------|
| Platform | @docs/specs/platform/Cluiche.md | PD-001, PD-004, PD-006, PD-007, PD-010 |
| Application | @docs/specs/applications/cluichetest/cluichetest.md | AD-001, AD-004, AD-005 |
| System | @docs/specs/applications/cluichetest/systems/teststages/teststages.md | SD-TS-001, SD-TS-002, SD-TS-003, SD-TS-004, SD-TS-005 |

## Problem Statement

Six gameplay systems — DiaTriggerScript, DiaObjective, DiaBlackboard, DiaStateMachine, DiaUtilityAI, and DiaRules — each have strong unit test coverage in isolation, but have never been exercised together under real SimPU frame timing in a coordinated scenario. This stage validates the full AI/progression stack end-to-end: trigger events driving objective state changes, enemies running a full AI loop (FSM → UtilityAI decision → Rules reactive overrides) per frame, and a blackboard bridging all of them. It is also the specific E2E exercise for all four DiaTriggerScript trigger types (Spatial, Temporal, State, Count) that unit tests cannot reach because they require real entity positions, elapsed time, and kill-count accumulation across frames.

## Template Answers (T1–T10)

| # | Question | Answer |
|---|----------|--------|
| T1 | Engine features exercised | DiaTriggerScript (Spatial, Temporal, State, Count triggers + ChangeObjectiveState/FireEvent/SpawnEntities/GiveResources actions); DiaObjective (3 Primary + 1 Optional, prerequisite chain, AllPrimaryComplete victory); DiaBlackboard (global arena blackboard + per-enemy blackboards as condition context); DiaStateMachine (per-enemy FlatStateMachine: Idle→Pursue→Attack→Dead); DiaUtilityAI (per-enemy UtilitySet scoring attack vs flee each frame); DiaRules (per-enemy RuleSet: desperate-charge rule fires when health < 0.2) |
| T2 | Scene setup in DoStart | 1 player agent at origin; power-up zone AABB at (−1,−1)→(1,1); global blackboard with slots: wave_num, total_kills, wave1_cleared, wave2_cleared, wave3_cleared. Enemies are spawned progressively by trigger actions (not in DoStart). TriggerScriptModule loaded from `arena_trigger_script.json`, ObjectiveSet loaded from `arena_objectives.json`. |
| T3 | Checkpoints and success conditions | `arena.wave1_complete` — Count trigger for wave1 fires (5 kills). `arena.wave2_complete` — Count trigger for wave2 fires (8 kills). `arena.wave3_complete` — Count trigger for wave3 fires (10 kills). `arena.victory` — ObjectiveSet::AllPrimaryComplete() true. `arena.powerup_collected` — Spatial trigger fires (player enters power-up zone). `arena.desperate_charge_triggered` — at least 1 DiaRules desperate-charge action fires across all enemies. |
| T4 | Metrics emitted | `cluichetest.arena.total_kills` (int, final=23), `cluichetest.arena.waves_completed` (int, final=3), `cluichetest.arena.powerup_collected` (int 0/1), `cluichetest.arena.desperate_charges_fired` (int ≥1), `cluichetest.arena.total_frames` (int, for determinism assertion) |
| T5 | Processing Unit | SimPU — game/AI logic runs on sim thread per platform rule |
| T6 | Assets needed | `arena_trigger_script.json` (trigger definitions), `arena_objectives.json` (objective definitions) — both hand-authored JSON, no binary assets |
| T7 | Gap vs unit tests | Unit tests for each system call APIs directly with synthetic inputs and never exercise cross-system feedback loops: trigger actions mutating objective state, objective state read back through ConditionExpr on the blackboard, FSM state driving UtilityAI evaluation, Rules firing mid-combat. No unit test exercises all 4 trigger types accumulating over real elapsed frames. |
| T8 | Determinism constraints | Fixed-timestep SimPU (30Hz). All enemy positions advance by deterministic rules each frame (no randomness). Enemy spawn order and kill order are deterministic (FIFO by wave). Trigger evaluation order matches JSON array order. |
| T9 | Expected frame budget | ~540 frames (18s at 30Hz): Wave 1 spawns at t=3s (90f), ~5s to kill 5 enemies; 2s gap; Wave 2 ~6s for 8 kills; 2s gap; Wave 3 ~7s for 10 kills. Budget: 900 frames (30s) with headroom. |
| T10 | Dependencies on other modules | AutomationModule (checkpoints), TimeServer (elapsed time for temporal triggers), TriggerScriptModule, ObjectiveSetComponent (or standalone ObjectiveSet), DiaBlackboard, DiaStateMachine, DiaUtilityAI, DiaRules. No DiaRenderModule dependency — visuals are ImGui only. |

## Acceptance Criteria

### Shared ACs (inherited from Infrastructure spec)

This stage satisfies AC-S1 through AC-S9 as defined in the [Infrastructure spec](test-stage-infrastructure.md#a--shared-acs-every-test-stage-must-satisfy-these).

### Stage-Specific ACs

| # | Criterion | Verification |
|---|-----------|--------------|
| AC-A1 | All four DiaTriggerScript trigger types fire at least once during a run: Temporal (wave spawns), Count (wave clears), State (inter-wave gap condition), Spatial (power-up zone) | Checkpoint + event stream confirm each trigger type fired |
| AC-A2 | 3 Primary objectives activate and complete in prerequisite order: Wave1 before Wave2, Wave2 before Wave3 | ObjectiveSet observer callbacks recorded; Wave2 cannot complete before Wave1 |
| AC-A3 | Optional objective "Collect Power-Up" completes when player agent crosses power-up zone AABB | arena.powerup_collected checkpoint passes |
| AC-A4 | AllPrimaryComplete() returns true exactly once, setting arena.victory checkpoint | Checkpoint latches; second evaluation doesn't re-trigger |
| AC-A5 | Each enemy runs FlatStateMachine with all 4 states reached by at least 1 enemy per run (Idle, Pursue, Attack, Dead) | Blackboard slot `fsm_state` transitions logged per enemy |
| AC-A6 | UtilityAI evaluates every live enemy each frame; winning action recorded in per-enemy blackboard slot `chosen_action` | Frame log shows alternating attack/flee outcomes across enemy lifetimes |
| AC-A7 | DiaRules desperate-charge fires for at least 1 enemy (health drops below 0.2 threshold before kill) | arena.desperate_charge_triggered checkpoint passes |
| AC-A8 | Global blackboard slots `wave1_cleared`, `wave2_cleared`, `wave3_cleared` flip true in the correct wave order | State trigger condition evaluations trace through correctly |
| AC-A9 | Stage completes within 900 frames (30s at 30Hz) | Orchestrator timeout |
| AC-A10 | Repeated run (Boot→Arena→Boot→Arena) produces identical `total_kills` and `waves_completed` metrics. `total_frames` is diagnostic only — TriggerScript/EnemyAI/Objectives run through DiaSimTimeModule's wall-clock budget gate (see DiaSimTime Budget Integration below), which DiaSimTime's own spec (ST-010) documents as non-deterministic tick-to-tick, so exact frame-count equality is not required | Determinism — AC-S7 |

## Design

### Arena Layout

```
Arena bounds: (−10, −10) → (10, 10)
Player: starts at origin (0, 0) — moves toward power-up zone linearly over first 90 frames
Power-up zone AABB: (−1, −1) → (1, 1)
Enemy spawn point (Wave 1/2): (−8, 8) — top-left corner
Enemy spawn point (Wave 3): (8, 8)  — top-right corner

Visual (ImGui drawList):
  Player:        green filled rect (0.8 units)
  Enemies alive: red filled rect (0.6 units) — darker shade when health < 0.2 (DespCharge)
  Enemies dead:  grey rect (1 frame linger)
  Power-up zone: cyan translucent rect
  Sidebar:       objective states + last trigger fired + wave counter
```

### Enemy Agent Structure

Each enemy is a lightweight `EnemyAgent` struct — no DiaEntity required. All AI systems operate as standalone objects (not entity-component wrappers):

```cpp
struct EnemyAgent
{
    Dia::Maths::Vec2 position;
    float health = 1.0f;
    Dia::Core::StringCRC waveTag;     // "enemy_wave1", "enemy_wave2", "enemy_wave3"

    Dia::StateMachine::FlatStateMachine fsm;
    Dia::Blackboard::Blackboard blackboard;   // slots: health, player_distance, fsm_state, chosen_action
    Dia::UtilityAI::UtilitySet utilityAi;
    Dia::Rules::RuleSet ruleSet;

    bool alive = true;
    bool desperateChargeFired = false;
};
```

### Trigger Script (`arena_trigger_script.json`)

```json
{
  "triggers": [
    {
      "id": "wave1_start",
      "type": "temporal",
      "elapsed_seconds": 3.0,
      "once": true,
      "actions": [{ "type": "SpawnEntities", "tag": "enemy_wave1", "count": 5 }]
    },
    {
      "id": "wave1_cleared",
      "type": "count",
      "entity_tag": "enemy_wave1",
      "threshold": 5,
      "once": true,
      "actions": [
        { "type": "ChangeObjectiveState", "objective_id": "survive_wave1", "state": "Complete" },
        { "type": "FireEvent", "event_id": "wave1_done" }
      ]
    },
    {
      "id": "wave2_start",
      "type": "state",
      "condition": "wave1_cleared == 1",
      "delay_seconds": 2.0,
      "once": true,
      "actions": [{ "type": "SpawnEntities", "tag": "enemy_wave2", "count": 8 }]
    },
    {
      "id": "wave2_cleared",
      "type": "count",
      "entity_tag": "enemy_wave2",
      "threshold": 8,
      "once": true,
      "actions": [
        { "type": "ChangeObjectiveState", "objective_id": "survive_wave2", "state": "Complete" },
        { "type": "FireEvent", "event_id": "wave2_done" }
      ]
    },
    {
      "id": "wave3_start",
      "type": "state",
      "condition": "wave2_cleared == 1",
      "delay_seconds": 2.0,
      "once": true,
      "actions": [{ "type": "SpawnEntities", "tag": "enemy_wave3", "count": 10 }]
    },
    {
      "id": "wave3_cleared",
      "type": "count",
      "entity_tag": "enemy_wave3",
      "threshold": 10,
      "once": true,
      "actions": [
        { "type": "ChangeObjectiveState", "objective_id": "survive_wave3", "state": "Complete" },
        { "type": "FireEvent", "event_id": "arena_victory" }
      ]
    },
    {
      "id": "powerup_zone",
      "type": "spatial",
      "region": { "min": [-1.0, -1.0], "max": [1.0, 1.0] },
      "entity_tag": "player",
      "once": true,
      "actions": [
        { "type": "GiveResources", "resource": "shield", "amount": 1 },
        { "type": "ChangeObjectiveState", "objective_id": "collect_powerup", "state": "Complete" }
      ]
    }
  ]
}
```

### Objectives (`arena_objectives.json`)

```json
{
  "objectives": [
    {
      "id": "survive_wave1",
      "classification": "Primary",
      "completion": "wave1_cleared == 1",
      "prerequisites": []
    },
    {
      "id": "survive_wave2",
      "classification": "Primary",
      "completion": "wave2_cleared == 1",
      "prerequisites": ["survive_wave1"]
    },
    {
      "id": "survive_wave3",
      "classification": "Primary",
      "completion": "wave3_cleared == 1",
      "prerequisites": ["survive_wave2"]
    },
    {
      "id": "collect_powerup",
      "classification": "Optional",
      "completion": "powerup_collected == 1",
      "prerequisites": []
    }
  ]
}
```

### Per-Enemy State Machine

```
States: Idle → Pursue → Attack → Dead

Transitions:
  Idle    → Pursue  : player_distance < 6.0 (blackboard)
  Pursue  → Attack  : player_distance < 1.0
  Attack  → Dead    : health <= 0.0
  Attack  → Pursue  : player_distance >= 1.0 (player moves away — not expected but valid)

Per-frame in Attack state: deal 0.1 damage to player (not tracked), receive 0.15 damage
```

### Per-Enemy UtilityAI

Two actions evaluated each frame while alive:

| Action | Score Formula |
|--------|--------------|
| `AttackPlayer` | `health * (1.0 - clamp(player_distance / 6.0, 0, 1))` — high when healthy and close |
| `FleePlayer` | `(1.0 - health) * clamp(player_distance / 6.0, 0, 1)` — high when wounded and far |

Winning action name written to blackboard slot `chosen_action` each frame.

### Per-Enemy Rules

One rule per enemy:

```
Rule "DespCharge":
  condition:  health < 0.2
  action:     FireEvent("desperate_charge")  → module increments mDespChargesFired, sets mDespChargeTriggered=true
  once:       true per enemy (latch flag on EnemyAgent)
```

### Blackboard Architecture

**Global blackboard** (one, owned by module):

| Slot | Type | Set by |
|------|------|--------|
| `wave_num` | int | Module on wave spawn |
| `total_kills` | int | Module on enemy death |
| `wave1_cleared` | int (0/1) | TriggerScript FireEvent handler |
| `wave2_cleared` | int (0/1) | TriggerScript FireEvent handler |
| `wave3_cleared` | int (0/1) | TriggerScript FireEvent handler |
| `powerup_collected` | int (0/1) | TriggerScript SpawnEntities/GiveResources handler |

**Per-enemy blackboard** (one per EnemyAgent):

| Slot | Type | Set by |
|------|------|--------|
| `health` | float | Module each frame |
| `player_distance` | float | Module each frame |
| `fsm_state` | int | FSM state-entry callbacks |
| `chosen_action` | StringCRC | UtilityAI evaluation each frame |

The global blackboard is passed as the `IConditionContext` to both TriggerScriptModule and ObjectiveSet. Per-enemy blackboards serve as condition contexts for per-enemy RuleSets.

### DiaSimTime Budget Integration

`DiaSimTimeModule` (umbrella SimPU module, declared before `ArenaTestStageModule` in `arena_test_stage.diaapp` — Arena depends on it) drives TriggerScript/EnemyAI/Objectives instead of `OnUpdate` calling them directly. Three `ISimTimeBudgetedSystem` wrappers are registered in `OnStart` and unregistered in `OnStop`:

| System | Priority tier |
|---|---|
| `Arena.TriggerScript` (wraps `mTriggerScript.Tick`) | kHigh |
| `Arena.EnemyAI` (wraps `UpdateEnemyAI`) | kNormal |
| `Arena.Objectives` (wraps `mObjectives.Evaluate`) | kBackground |

This gives real multi-system contention for `DiaSimTimeModule`'s per-tier CPU budget with production code, rather than only the synthetic stand-ins DiaSimTime's own unit tests use. Consequence: these three calls are wall-clock gated (ST-010) and can be deferred/promoted a tick late under load — see AC-A10.

### Spatial Trigger: SpatialProvider Stub

TriggerScriptModule::SetSpatialModule requires an ISpatialProvider. The module implements a thin inline `ArenaEntitySpatialProvider : ISpatialProvider` that returns the player's current `Vec2` position for the `"player"` entity tag. This avoids a DiaEntitySpatial dependency — the stage owns the player position directly.

### SpawnEntities Custom Action Handler

The stage registers a custom action handler with TriggerScriptModule's TriggerActionRegistry:

```cpp
registry.Register(StringCRC("SpawnEntities"), [this](const ActionContext& ctx) {
    StringCRC tag = ctx.GetParam<StringCRC>("tag");
    int count    = ctx.GetParam<int>("count");
    SpawnWave(tag, count);
});
```

`SpawnWave` creates `count` EnemyAgent instances, initialises their FSM/Blackboard/UtilitySet/RuleSet, and appends them to `mEnemies`. The built-in `ChangeObjectiveState`, `FireEvent`, and `GiveResources` actions use their default handlers.

### DoUpdate Frame Loop

```
Each frame:
  1. Advance TriggerScriptModule::Tick(dt)
  2. Evaluate ObjectiveSet::Evaluate(globalBlackboard)
  3. For each live enemy:
     a. Update blackboard slots (health, player_distance)
     b. FSM::Update() → may transition state
     c. UtilitySet::Evaluate(blackboard) → write chosen_action to blackboard
     d. RuleSet::Evaluate(blackboard) → fire DespCharge if triggered
     e. Move toward player (position += normalize(player - pos) * speed * dt)
     f. If in Attack state: apply damage, decrement health
     g. If health <= 0: transition to Dead, call IncrementCount(waveTag, 1)
  4. Advance player toward power-up zone (deterministic linear path)
  5. Emit metrics snapshot
  6. Increment mFrameCount
```

### Module Structure

```cpp
// Cluiche/CluicheTest/Modules/TestStages/ArenaTestStageModule.h
namespace CluicheTest {

class ArenaTestStageModule : public Dia::ApplicationFlow::Module
{
public:
    static constexpr Dia::Core::StringCRC kTypeId{"ArenaTestStageModule"};
    explicit ArenaTestStageModule(const Dia::Core::StringCRC& instanceId);

protected:
    StartResult DoStart() override;
    void DoUpdate(float deltaTime) override;
    StopResult DoStop() override;

private:
    void LoadTriggerScript();
    void LoadObjectives();
    void RegisterCheckpoints(Dia::Automation::AutomationService& automation);
    void RegisterMetrics();
    void SpawnWave(Dia::Core::StringCRC tag, int count);
    void UpdateEnemyAI(EnemyAgent& enemy, float dt);
    void DrawDebugImGui();

    Dia::ApplicationFlow::ModuleRef<AutomationModule> mAutomation{this};

    Dia::TriggerScript::TriggerScriptModule mTriggerScript;
    Dia::Objective::ObjectiveSet mObjectives;
    Dia::Blackboard::Blackboard mGlobalBlackboard;

    Dia::Maths::Vec2 mPlayerPosition{0.0f, 0.0f};
    Dia::Core::DynamicArrayC<EnemyAgent, 32> mEnemies;

    int mTotalKills = 0;
    int mWavesCompleted = 0;
    int mDespChargesFired = 0;
    bool mDespChargeTriggered = false;
    bool mPowerupCollected = false;
    bool mVictory = false;
    unsigned int mFrameCount = 0;
};

} // namespace CluicheTest
DIA_MODULE(ArenaTestStageModule);
```

### Checkpoint Registration

```cpp
automation->RegisterCheckpoint(this, StringCRC("arena.wave1_complete"),
    [this]() -> CheckpointResult {
        bool ok = mWavesCompleted >= 1;
        return { ok, ok ? "wave 1 cleared" : "wave 1 in progress", 0.0f };
    });
// wave2, wave3 similarly

automation->RegisterCheckpoint(this, StringCRC("arena.victory"),
    [this]() -> CheckpointResult {
        bool ok = mObjectives.AllPrimaryComplete();
        return { ok, ok ? "all primary objectives complete" : "in progress", 0.0f };
    });

automation->RegisterCheckpoint(this, StringCRC("arena.powerup_collected"),
    [this]() -> CheckpointResult {
        return { mPowerupCollected, mPowerupCollected ? "powerup taken" : "not yet", 0.0f };
    });

automation->RegisterCheckpoint(this, StringCRC("arena.desperate_charge_triggered"),
    [this]() -> CheckpointResult {
        return { mDespChargeTriggered, mDespChargeTriggered
            ? std::to_string(mDespChargesFired) + " fired" : "none yet", 0.0f };
    });
```

### Pytest Scenario

```python
# Tools/orchestrator/scenarios/cluichetest/arena/test_arena_e2e.py

def test_arena_three_wave_victory(dia_client):
    dia_client.navigate_to("ArenaTestStage")

    # Wave progression (sequential — each wave clears before next starts)
    r = dia_client.poll_checkpoint("arena.wave1_complete", timeout_s=12.0)
    assert r["passed"], f"Wave 1 did not clear: {r['message']}"

    r = dia_client.poll_checkpoint("arena.wave2_complete", timeout_s=15.0)
    assert r["passed"], f"Wave 2 did not clear: {r['message']}"

    r = dia_client.poll_checkpoint("arena.wave3_complete", timeout_s=18.0)
    assert r["passed"], f"Wave 3 did not clear: {r['message']}"

    # Objective system
    r = dia_client.poll_checkpoint("arena.victory", timeout_s=2.0)
    assert r["passed"], f"Victory condition not met: {r['message']}"

    # Optional + reactive systems
    r = dia_client.poll_checkpoint("arena.powerup_collected", timeout_s=5.0)
    assert r["passed"], f"Spatial trigger / power-up not collected: {r['message']}"

    r = dia_client.poll_checkpoint("arena.desperate_charge_triggered", timeout_s=20.0)
    assert r["passed"], f"DiaRules desperate-charge never fired: {r['message']}"

    # Metric assertions
    metrics = dia_client.query_metrics("cluichetest.arena.*")
    assert metrics["cluichetest.arena.total_kills"] == 23
    assert metrics["cluichetest.arena.waves_completed"] == 3
    assert metrics["cluichetest.arena.powerup_collected"] == 1
    assert metrics["cluichetest.arena.desperate_charges_fired"] >= 1

    # Determinism: second pass must match frame count
    first_frames = metrics["cluichetest.arena.total_frames"]
    dia_client.navigate_to("Boot")
    dia_client.navigate_to("ArenaTestStage")
    dia_client.poll_checkpoint("arena.victory", timeout_s=35.0)
    metrics2 = dia_client.query_metrics("cluichetest.arena.*")
    assert metrics2["cluichetest.arena.total_frames"] == first_frames, \
        f"Non-deterministic frame count: {first_frames} vs {metrics2['cluichetest.arena.total_frames']}"

    dia_client.navigate_to("Boot")
```

## Files Touched

| File | Change |
|------|--------|
| `Cluiche/CluicheTest/Modules/TestStages/ArenaTestStageModule.h` | New — module header + EnemyAgent struct |
| `Cluiche/CluicheTest/Modules/TestStages/ArenaTestStageModule.cpp` | New — full implementation |
| `Cluiche/CluicheTest/CluicheTest.vcxproj` | Add ClCompile + ClInclude entries |
| `Cluiche/CluicheTest/CluicheTest.vcxproj.filters` | Add filter entries |
| `Cluiche/Assets/CluicheTest/Stages/ArenaTestStage/arena_test_stage.diastage` | New — stage declaration |
| `Cluiche/Assets/CluicheTest/Stages/ArenaTestStage/misc/ApplicationFlow/arena_test_stage.diaapp` | New — stage app manifest |
| `Cluiche/Assets/CluicheTest/Stages/ArenaTestStage/arena_trigger_script.json` | New — trigger definitions (7 triggers, all 4 types) |
| `Cluiche/Assets/CluicheTest/Stages/ArenaTestStage/arena_objectives.json` | New — objective definitions (3 Primary + 1 Optional) |
| `Cluiche/Assets/CluicheTest/Global/misc/ApplicationFlow/cluiche_main.diaapp` | Add ArenaTestStage entry + Boot↔stage transitions |
| `Cluiche/Assets/CluicheTest/cluichetest.diagame` | Add import for arena_test_stage.diastage |
| `Cluiche/Assets/CluicheTest/assets.catalogue.json` | Add stage + manifest catalogue entries |
| `Tools/orchestrator/scenarios/cluichetest/arena/test_arena_e2e.py` | New — pytest scenario |
| `Tools/orchestrator/plans/cluichetest/default.json` | Add scenario to plan |

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | `dia scaffold stage ArenaTestStage` — produces 9 touch points | Stage appears in Boot menu | Todo | haiku | Exact command; fills stub module + manifests |
| 2 | Create `arena_trigger_script.json` + `arena_objectives.json` asset files | JSON parses without error | Todo | sonnet | 7 triggers (all 4 types), 4 objectives, 2-s inter-wave delay via State trigger |
| 3 | Implement `ArenaTestStageModule::DoStart`: load assets, init global blackboard, init player, wire TriggerScriptModule (SpawnEntities handler, SpatialProvider stub, ArenaEntitySpatialProvider), register checkpoints | Module reaches kReady; TriggerScript validation passes | Todo | sonnet | SpatialProvider stub returns player Vec2 for tag "player" |
| 4 | Implement `EnemyAgent` + `SpawnWave`: construct FlatStateMachine (4 states + transitions), per-enemy Blackboard, UtilitySet (2 actions + score curves), RuleSet (DespCharge rule) | Wave 1 spawns 5 fully-initialised agents | Todo | sonnet | All AI objects owned by EnemyAgent; no heap alloc beyond DynamicArrayC |
| 5 | Implement `ArenaTestStageModule::DoUpdate`: Tick trigger script, Evaluate objectives, per-enemy AI loop (blackboard update → FSM → UtilityAI → Rules → movement → combat → death → IncrementCount), player linear walk into power-up zone | All 3 waves clear in sequence; victory fires | Todo | sonnet | Enemy movement speed: 1.5 units/s; attack damage: 0.15/s; fixed-step, deterministic |
| 6 | Implement FireEvent handler in module: maps `wave1_done/wave2_done/wave3_done` → set global blackboard slots + increment mWavesCompleted; maps `arena_victory` → set mVictory; maps `desperate_charge` → increment mDespChargesFired | wave1_cleared slot == 1 after wave 1 clears; State trigger fires wave 2 spawn | Todo | sonnet | FireEvent is the bridge between TriggerScript and ObjectiveSet condition context |
| 7 | Register 5 metrics in DoStart via AutomationModule; emit updated values in DoUpdate | Metric query from orchestrator returns correct final counts | Todo | haiku | total_kills, waves_completed, powerup_collected, desperate_charges_fired, total_frames |
| 8 | Implement `DrawDebugImGui`: ImGui drawList colored rects (player=green, alive enemies=red, dead=grey, powerup zone=cyan); sidebar with objective states + last trigger fired + wave counter | Visual output renders on screen during manual run | Todo | sonnet | `ImGui::GetBackgroundDrawList()` for world rects; sidebar via ImGui window |
| 9 | Implement `DoStop`: clear TriggerScriptModule, reset objectives, clear enemy array, reset all counters and flags | Module stops cleanly; second run produces identical results (AC-A10) | Todo | haiku | |
| 10 | Write pytest scenario `test_arena_e2e.py` + add to `default.json` | `dia orchestrate --list` shows arena scenario; full scenario passes | Todo | sonnet | Includes determinism second-pass assertion |
| 11 | Verify: `dia run googletest --filter="ArenaTestStage*"` passes (if unit tests written); `dia run cluichetest` navigates to ArenaTestStage and all 6 checkpoints pass | All checkpoints pass, metrics match expected values | Todo | sonnet | Final E2E gate |

## Dependencies

- **All 6 AI/progression systems** must be Done (confirmed: DiaTriggerScript ✓, DiaObjective ✓, DiaBlackboard ✓, DiaStateMachine ✓, DiaUtilityAI ✓, DiaRules ✓)
- **Test Stage Infrastructure** Tasks 1–6 must be complete (Boot menu manifest-driven, checkpoint infrastructure)
- **`IConditionContext` interface** — the global Blackboard must be usable as a condition context for both TriggerScriptModule and ObjectiveSet. Verify at implementation start that `Dia::Blackboard::Blackboard` implements `IConditionContext` (or that a thin adapter exists)
- **`ITriggerActionHandler` / `TriggerActionRegistry`** — confirm custom action registration API before Task 3
- **`ISpatialProvider` interface** — confirm the exact interface required by TriggerScriptModule spatial triggers before writing `ArenaEntitySpatialProvider`

## Open Design Questions

1. **Blackboard as IConditionContext** — `ConditionExpr` evaluation in both TriggerScript and ObjectiveSet reads from an `IConditionContext`. Does `Dia::Blackboard::Blackboard` already implement that interface, or is a thin adapter class needed? Impacts Task 3 and 4 wiring.

2. **Spatial trigger entity tracking** — TriggerScriptModule's spatial trigger checks entity positions via `ISpatialProvider`. The arena uses struct-based agents (not DiaEntity), so a stub provider returning the player `Vec2` is proposed. Confirm this matches the `ISpatialProvider` contract (returns single-entity position by tag, not a full query), or adjust approach.

3. **Inter-wave delay mechanism** — The State trigger for wave2_start fires when `wave1_cleared == 1`, with a `delay_seconds: 2.0` field. Confirm DiaTriggerScript State trigger supports a `delay_seconds` field; if not, implement the delay in the module's FireEvent handler using a countdown float instead.

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-001 | StringCRC for IDs | kTypeId, checkpoint names, trigger IDs, objective IDs, blackboard slot names, action type keys all StringCRC. |
| PD-004 | No STL in public APIs | Module interface uses Dia containers (DynamicArrayC for enemy array). std::function in checkpoint callbacks only. |
| PD-006 | VS project files source of truth | Task 1 scaffold + Task 1 explicit vcxproj/filters additions. |
| PD-007 | C++20 required | constexpr StringCRC, standard features. |
| PD-010 | .diastage for stages | Stage declared in `.diastage`; assets under stage directory. |
| AD-001 (CT) | Three PUs | Module lives on SimPU (game/AI logic on sim thread). |
| AD-004 (CT) | Test levels included | This is a test level. |
| AD-005 (CT) | App is testbed not product | Stage exists purely for multi-system validation. |
| SD-TS-001 | One manifest stage per feature | One stage: ArenaTestStage. |
| SD-TS-002 | Checkpoints in DoStart, auto-clear on stop | RegisterCheckpoints called in DoStart after assets loaded. |
| SD-TS-003 | Metrics for threshold assertions | 5 metrics registered; pytest asserts exact final values. |
| SD-TS-004 | All stages return to Boot | `transitions: ["Boot"]` in manifest. |
| SD-TS-005 | Individual stages are feature specs | This spec IS the feature spec. |

## Status

**Status:** Done
**Plan:** @docs/specs/applications/cluichetest/systems/teststages/arena-test-stage.plan.md
