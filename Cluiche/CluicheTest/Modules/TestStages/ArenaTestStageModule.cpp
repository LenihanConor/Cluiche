#include "Modules/TestStages/ArenaTestStageModule.h"
#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaAutomation/AutomationService.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaEntity/ComponentPool.h>
#include <DiaEntitySpatial/SpatialComponent.h>
#include <DiaCore/Json/external/json/json.h>
#include <DiaStateMachine/StateMachineBuilder.h>

// ---------------------------------------------------------------------------
// Inline JSON assets
//
// arena_trigger_script.json — all 4 trigger types.
//
// Implementation notes vs spec text:
//  - temporal: uses "interval_s" key (spec text says "elapsed_seconds" which
//    is incorrect — TriggerScriptModule.cpp reads "interval_s").
//  - one_shot: uses "one_shot" key (spec says "once" — impl uses "one_shot").
//  - action params: must be nested under "params" key
//    (TriggerScriptModule.cpp reads actionNode["params"]).
//  - state trigger conditions: must be ConditionExpr JSON objects with
//    {op, slot, field, value} — not inline strings.
//    All global blackboard fields use slot="global".
// ---------------------------------------------------------------------------
static constexpr const char* kTriggerScriptJson = R"(
{
  "triggers": [
    {
      "id": "wave1_start",
      "type": "temporal",
      "interval_s": 3.0,
      "one_shot": true,
      "actions": [
        {
          "type": "SpawnEntities",
          "params": { "tag": "enemy_wave1", "count": 5 }
        }
      ]
    },
    {
      "id": "wave1_cleared",
      "type": "count",
      "entity_tag": "enemy_wave1",
      "threshold": 5,
      "one_shot": true,
      "actions": [
        {
          "type": "ChangeObjectiveState",
          "params": { "objective_id": "survive_wave1", "state": "Complete" }
        },
        {
          "type": "FireEvent",
          "params": { "event_id": "wave1_done" }
        }
      ]
    },
    {
      "id": "wave2_start",
      "type": "state",
      "condition": { "op": "==", "slot": "global", "field": "wave1_cleared", "value": 1 },
      "one_shot": true,
      "actions": [
        {
          "type": "SpawnEntities",
          "params": { "tag": "enemy_wave2", "count": 8 }
        }
      ]
    },
    {
      "id": "wave2_cleared",
      "type": "count",
      "entity_tag": "enemy_wave2",
      "threshold": 8,
      "one_shot": true,
      "actions": [
        {
          "type": "ChangeObjectiveState",
          "params": { "objective_id": "survive_wave2", "state": "Complete" }
        },
        {
          "type": "FireEvent",
          "params": { "event_id": "wave2_done" }
        }
      ]
    },
    {
      "id": "wave3_start",
      "type": "state",
      "condition": { "op": "==", "slot": "global", "field": "wave2_cleared", "value": 1 },
      "one_shot": true,
      "actions": [
        {
          "type": "SpawnEntities",
          "params": { "tag": "enemy_wave3", "count": 10 }
        }
      ]
    },
    {
      "id": "wave3_cleared",
      "type": "count",
      "entity_tag": "enemy_wave3",
      "threshold": 10,
      "one_shot": true,
      "actions": [
        {
          "type": "ChangeObjectiveState",
          "params": { "objective_id": "survive_wave3", "state": "Complete" }
        },
        {
          "type": "FireEvent",
          "params": { "event_id": "arena_victory" }
        }
      ]
    },
    {
      "id": "powerup_zone",
      "type": "spatial",
      "region": { "min": [-1.0, -1.0], "max": [1.0, 1.0] },
      "entity_tag": "player",
      "one_shot": true,
      "actions": [
        {
          "type": "GiveResources",
          "params": { "resource": "shield", "amount": 1 }
        },
        {
          "type": "ChangeObjectiveState",
          "params": { "objective_id": "collect_powerup", "state": "Complete" }
        }
      ]
    }
  ]
}
)";

// ---------------------------------------------------------------------------
// arena_objectives.json
//
// Completion conditions are ConditionExpr JSON objects.
// slot="global", field=the blackboard slot name.
// Classification strings: "primary" / "optional" (lowercase — matches
// ObjectiveSet.cpp which compares against "secondary" and "optional").
// ---------------------------------------------------------------------------
static constexpr const char* kObjectivesJson = R"(
{
  "objectives": [
    {
      "id": "survive_wave1",
      "classification": "primary",
      "completion": { "op": "==", "slot": "global", "field": "wave1_cleared", "value": 1 },
      "prerequisites": []
    },
    {
      "id": "survive_wave2",
      "classification": "primary",
      "completion": { "op": "==", "slot": "global", "field": "wave2_cleared", "value": 1 },
      "prerequisites": ["survive_wave1"]
    },
    {
      "id": "survive_wave3",
      "classification": "primary",
      "completion": { "op": "==", "slot": "global", "field": "wave3_cleared", "value": 1 },
      "prerequisites": ["survive_wave2"]
    },
    {
      "id": "collect_powerup",
      "classification": "optional",
      "completion": { "op": "==", "slot": "global", "field": "powerup_collected", "value": 1 },
      "prerequisites": []
    }
  ]
}
)";

// ---------------------------------------------------------------------------
// Inline JSON assets for per-enemy UtilitySet and RuleSet
// ---------------------------------------------------------------------------
static constexpr const char* kEnemyUtilitySetJson = R"(
{
  "actions": [
    {
      "id": "AttackPlayer",
      "scorers": [
        {
          "slot": "self", "field": "health",
          "input_min": 0.0, "input_max": 1.0,
          "curve": { "shape": "linear", "invert": false }
        },
        {
          "slot": "self", "field": "player_distance",
          "input_min": 0.0, "input_max": 6.0,
          "curve": { "shape": "linear", "invert": true }
        }
      ]
    },
    {
      "id": "FleePlayer",
      "scorers": [
        {
          "slot": "self", "field": "health",
          "input_min": 0.0, "input_max": 1.0,
          "curve": { "shape": "linear", "invert": true }
        },
        {
          "slot": "self", "field": "player_distance",
          "input_min": 0.0, "input_max": 6.0,
          "curve": { "shape": "linear", "invert": false }
        }
      ]
    }
  ]
}
)";

static constexpr const char* kEnemyRuleSetJson = R"(
{
  "rules": [
    {
      "id": "DespCharge",
      "guard": { "slot": "self", "field": "health", "op": "<", "value": 0.2 },
      "actions": ["DespCharge"]
    }
  ]
}
)";

// ---------------------------------------------------------------------------
// Static FSM callbacks — cast void* to EnemyAgent* to access blackboard.
// These must be free functions (no captures) to satisfy void(*)(void*).
// ---------------------------------------------------------------------------
static void EnemyFsm_OnEnterIdle(void* ctx)
{
    auto* e = static_cast<CluicheTest::EnemyAgent*>(ctx);
    e->blackboard.Get<int>(Dia::Core::StringCRC("fsm_state")) = 0;
}

static void EnemyFsm_OnEnterPursue(void* ctx)
{
    auto* e = static_cast<CluicheTest::EnemyAgent*>(ctx);
    e->blackboard.Get<int>(Dia::Core::StringCRC("fsm_state")) = 1;
}

static void EnemyFsm_OnEnterAttack(void* ctx)
{
    auto* e = static_cast<CluicheTest::EnemyAgent*>(ctx);
    e->blackboard.Get<int>(Dia::Core::StringCRC("fsm_state")) = 2;
}

static void EnemyFsm_OnEnterDead(void* ctx)
{
    auto* e = static_cast<CluicheTest::EnemyAgent*>(ctx);
    e->blackboard.Get<int>(Dia::Core::StringCRC("fsm_state")) = 3;
}

// Guard: Idle → Pursue: player_distance < 6.0
static bool EnemyGuard_IdleToPursue(const void* ctx)
{
    const auto* e = static_cast<const CluicheTest::EnemyAgent*>(ctx);
    return e->blackboard.Get<float>(Dia::Core::StringCRC("player_distance")) < 6.0f;
}

// Guard: Pursue → Attack: player_distance < 1.0
static bool EnemyGuard_PursueToAttack(const void* ctx)
{
    const auto* e = static_cast<const CluicheTest::EnemyAgent*>(ctx);
    return e->blackboard.Get<float>(Dia::Core::StringCRC("player_distance")) < 1.0f;
}

// Guard: Attack → Dead: health <= 0.0
static bool EnemyGuard_AttackToDead(const void* ctx)
{
    const auto* e = static_cast<const CluicheTest::EnemyAgent*>(ctx);
    return e->health <= 0.0f;
}

// Guard: Attack → Pursue: player_distance >= 1.0
static bool EnemyGuard_AttackToPursue(const void* ctx)
{
    const auto* e = static_cast<const CluicheTest::EnemyAgent*>(ctx);
    return e->blackboard.Get<float>(Dia::Core::StringCRC("player_distance")) >= 1.0f;
}

// ---------------------------------------------------------------------------
// Static DespCharge action callback.
// actionContext is EnemyAgent* (passed as void* to RuleSet::Evaluate).
// ---------------------------------------------------------------------------
static void EnemyAction_DespCharge(void* actionContext)
{
    auto* e = static_cast<CluicheTest::EnemyAgent*>(actionContext);
    if (!e->desperateChargeFired && e->modulePtr)
    {
        e->desperateChargeFired = true;
        e->modulePtr->NotifyDespChargeFired();
        DIA_LOG_INFO("CluicheTest", "Arena: DespCharge fired for enemy");
    }
}

// ---------------------------------------------------------------------------
// Static ConditionRegistry accessors for per-enemy blackboard.
// void* data is EnemyAgent*.
// ---------------------------------------------------------------------------
static float EnemyCondition_Health(void* d)
{
    return static_cast<CluicheTest::EnemyAgent*>(d)->blackboard.Get<float>(
        Dia::Core::StringCRC("health"));
}

static float EnemyCondition_PlayerDistance(void* d)
{
    return static_cast<CluicheTest::EnemyAgent*>(d)->blackboard.Get<float>(
        Dia::Core::StringCRC("player_distance"));
}

namespace CluicheTest {

const Dia::Core::StringCRC ArenaTestStageModule::kTypeId("ArenaTestStageModule");

// ---------------------------------------------------------------------------
// Constructor — wire action handler callbacks.
// SpawnWave / OnFireEvent / OnChangeObjectiveState are stubs until Tasks 5-6;
// the wiring is done here so TriggerScriptModule can dispatch without crashing.
// ---------------------------------------------------------------------------
ArenaTestStageModule::ArenaTestStageModule(const Dia::Core::StringCRC& instanceId)
    : TestStageModuleBase(instanceId)
{
    mSpawnHandler.SetCallback([this](const Dia::TriggerScript::ActionContext& ctx) {
        Dia::Core::StringCRC tag(ctx.params["tag"].asString().c_str());
        int count = ctx.params.isMember("count") ? ctx.params["count"].asInt() : 0;
        SpawnWave(tag, count);
    });

    mFireEventHandler.SetCallback([this](const Dia::TriggerScript::ActionContext& ctx) {
        Dia::Core::StringCRC eventId(ctx.params["event_id"].asString().c_str());
        OnFireEvent(eventId);
    });

    mChangeObjectiveHandler.SetCallback([this](const Dia::TriggerScript::ActionContext& ctx) {
        Dia::Core::StringCRC objectiveId(ctx.params["objective_id"].asString().c_str());
        Dia::Core::StringCRC state(ctx.params["state"].asString().c_str());
        OnChangeObjectiveState(objectiveId, state);
    });

    mGiveResourcesHandler.SetCallback([this](const Dia::TriggerScript::ActionContext& ctx) {
        (void)ctx;
        mPowerupCollected = true;
        mGlobalBlackboard.Get<int>(Dia::Core::StringCRC("powerup_collected")) = 1;
    });
}

ArenaTestStageModule::~ArenaTestStageModule() = default;

// ---------------------------------------------------------------------------
// GetStageName / GetCheckpointNames
// ---------------------------------------------------------------------------
Dia::Core::StringCRC ArenaTestStageModule::GetStageName() const
{
    return Dia::Core::StringCRC("ArenaTestStage");
}

const Dia::Core::StringCRC* ArenaTestStageModule::GetCheckpointNames(unsigned int& outCount) const
{
    static const Dia::Core::StringCRC names[] = {
        Dia::Core::StringCRC("arena.wave1_complete"),
        Dia::Core::StringCRC("arena.wave2_complete"),
        Dia::Core::StringCRC("arena.wave3_complete"),
        Dia::Core::StringCRC("arena.victory"),
        Dia::Core::StringCRC("arena.powerup_collected"),
        Dia::Core::StringCRC("arena.desperate_charge_triggered"),
    };
    outCount = 6;
    return names;
}

// ---------------------------------------------------------------------------
// LoadTriggerScript
//
// 1. Register action handlers with mTriggerActionRegistry.
// 2. Build mGlobalConditionRegistry (bridges global Blackboard -> IConditionContext).
//    All int slots are exposed as float (ConditionRegistry only has float accessors).
//    slot="global", field=each blackboard slot name.
// 3. Create EntitySpatialModule for the 20x20 arena (required for spatial triggers).
// 4. Register SpatialComponent pool and create the player entity.
// 5. Wire dependencies and call TriggerScriptModule::LoadFromJson.
// ---------------------------------------------------------------------------
void ArenaTestStageModule::LoadTriggerScript()
{
    // --- 1. Register action handlers ---
    mTriggerActionRegistry.Register(Dia::Core::StringCRC("SpawnEntities"),       &mSpawnHandler);
    mTriggerActionRegistry.Register(Dia::Core::StringCRC("FireEvent"),            &mFireEventHandler);
    mTriggerActionRegistry.Register(Dia::Core::StringCRC("ChangeObjectiveState"), &mChangeObjectiveHandler);
    mTriggerActionRegistry.Register(Dia::Core::StringCRC("GiveResources"),        &mGiveResourcesHandler);

    mTriggerScript.SetActionRegistry(&mTriggerActionRegistry);

    // --- 2. Build global condition registry ---
    mGlobalConditionRegistry = std::make_unique<Dia::Condition::ConditionRegistry>(this);

    const Dia::Core::StringCRC slotGlobal("global");

    mGlobalConditionRegistry->RegisterFloat(slotGlobal, Dia::Core::StringCRC("wave_num"),
        [](void* d) -> float {
            return static_cast<float>(
                static_cast<ArenaTestStageModule*>(d)->mGlobalBlackboard.Get<int>(
                    Dia::Core::StringCRC("wave_num")));
        });

    mGlobalConditionRegistry->RegisterFloat(slotGlobal, Dia::Core::StringCRC("total_kills"),
        [](void* d) -> float {
            return static_cast<float>(
                static_cast<ArenaTestStageModule*>(d)->mGlobalBlackboard.Get<int>(
                    Dia::Core::StringCRC("total_kills")));
        });

    mGlobalConditionRegistry->RegisterFloat(slotGlobal, Dia::Core::StringCRC("wave1_cleared"),
        [](void* d) -> float {
            return static_cast<float>(
                static_cast<ArenaTestStageModule*>(d)->mGlobalBlackboard.Get<int>(
                    Dia::Core::StringCRC("wave1_cleared")));
        });

    mGlobalConditionRegistry->RegisterFloat(slotGlobal, Dia::Core::StringCRC("wave2_cleared"),
        [](void* d) -> float {
            return static_cast<float>(
                static_cast<ArenaTestStageModule*>(d)->mGlobalBlackboard.Get<int>(
                    Dia::Core::StringCRC("wave2_cleared")));
        });

    mGlobalConditionRegistry->RegisterFloat(slotGlobal, Dia::Core::StringCRC("wave3_cleared"),
        [](void* d) -> float {
            return static_cast<float>(
                static_cast<ArenaTestStageModule*>(d)->mGlobalBlackboard.Get<int>(
                    Dia::Core::StringCRC("wave3_cleared")));
        });

    mGlobalConditionRegistry->RegisterFloat(slotGlobal, Dia::Core::StringCRC("powerup_collected"),
        [](void* d) -> float {
            return static_cast<float>(
                static_cast<ArenaTestStageModule*>(d)->mGlobalBlackboard.Get<int>(
                    Dia::Core::StringCRC("powerup_collected")));
        });

    mTriggerScript.SetConditionContext(mGlobalConditionRegistry.get());

    // --- 3. Create EntitySpatialModule ---
    // Arena bounds: (-10,-10) to (10,10), cell size 2.0 -> 10x10 cells.
    Dia::EntitySpatial::EntitySpatialIndex::SquareDef sqDef;
    sqDef.worldBounds = Dia::Geometry2D::AARect(
        Dia::Maths::Vector2D(-10.f, -10.f),
        Dia::Maths::Vector2D( 10.f,  10.f));
    sqDef.cellSize = 2.0f;

    mEntitySpatialModule = std::make_unique<Dia::EntitySpatial::EntitySpatialModule>(
        mSpatialDomain, sqDef);

    // --- 4. Register SpatialComponent pool and add player entity ---
    mSpatialDomain.RegisterPool(
        new Dia::Entity::ComponentPool<Dia::EntitySpatial::SpatialComponent>(
            Dia::EntitySpatial::SpatialComponent::kTypeId));

    mPlayerEntity = mSpatialDomain.CreateEntity();

    // SpatialComponent config: position and radius serialised as JSON.
    // Vector2D is serialised as {"x": ..., "y": ...}
    Json::Value spatialCfg;
    {
        Json::Value posNode;
        posNode["x"] = mPlayerPosition.x;
        posNode["y"] = mPlayerPosition.y;
        spatialCfg["position"]  = posNode;
        spatialCfg["radius"]    = 0.4f;
        spatialCfg["layerMask"] = 0;
    }
    mSpatialDomain.QueueAddComponent<Dia::EntitySpatial::SpatialComponent>(
        mPlayerEntity, spatialCfg);
    mSpatialDomain.EndOfFrame();

    mTriggerScript.SetSpatialModule(mEntitySpatialModule.get());

    // --- 5. Load TriggerScript from inline JSON ---
    Json::Value root;
    Json::Reader reader;
    if (!reader.parse(kTriggerScriptJson, root))
    {
        DIA_LOG_ERROR("CluicheTest",
            "ArenaTestStageModule: failed to parse kTriggerScriptJson");
        return;
    }

    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    bool ok = mTriggerScript.LoadFromJson(root, *mGlobalConditionRegistry, errors);
    if (!ok)
    {
        for (unsigned int i = 0; i < errors.Size(); ++i)
            DIA_LOG_ERROR("CluicheTest",
                "ArenaTestStageModule TriggerScript error: %s", errors[i]);
    }
    else
    {
        DIA_LOG_INFO("CluicheTest",
            "ArenaTestStageModule: TriggerScript loaded — %d triggers",
            mTriggerScript.GetTriggerCount());
    }
}

// ---------------------------------------------------------------------------
// LoadObjectives
// ---------------------------------------------------------------------------
void ArenaTestStageModule::LoadObjectives()
{
    Json::Value root;
    Json::Reader reader;
    if (!reader.parse(kObjectivesJson, root))
    {
        DIA_LOG_ERROR("CluicheTest",
            "ArenaTestStageModule: failed to parse kObjectivesJson");
        return;
    }

    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    mObjectives = Dia::Objective::ObjectiveSet::LoadFromJson(root, errors);

    for (unsigned int i = 0; i < errors.Size(); ++i)
        DIA_LOG_ERROR("CluicheTest",
            "ArenaTestStageModule Objectives error: %s", errors[i]);

    // Validate completion expressions against the condition registry.
    Dia::Core::Containers::DynamicArrayC<const char*, 32> valErrors;
    if (!mObjectives.Validate(*mGlobalConditionRegistry, valErrors))
    {
        for (unsigned int i = 0; i < valErrors.Size(); ++i)
            DIA_LOG_ERROR("CluicheTest",
                "ArenaTestStageModule Objectives validation: %s", valErrors[i]);
    }
    else
    {
        DIA_LOG_INFO("CluicheTest",
            "ArenaTestStageModule: Objectives loaded and validated");
    }
}

// ---------------------------------------------------------------------------
// RegisterCheckpoints
//
// Called from OnStart. Uses GetAutomationService() from TestStageModuleBase
// (service pointer is already live at this point in the lifecycle).
// ---------------------------------------------------------------------------
void ArenaTestStageModule::RegisterCheckpoints()
{
    Dia::Automation::AutomationService* service = GetAutomationService();
    if (!service)
    {
        DIA_LOG_WARNING("CluicheTest",
            "ArenaTestStageModule::RegisterCheckpoints — AutomationService unavailable");
        return;
    }

    service->RegisterCheckpoint(this, Dia::Core::StringCRC("arena.wave1_complete"),
        [this]() -> Dia::Automation::CheckpointResult {
            bool ok = (mWavesCompleted >= 1);
            return { ok, ok ? "wave 1 cleared" : "wave 1 in progress", 0.f };
        });

    service->RegisterCheckpoint(this, Dia::Core::StringCRC("arena.wave2_complete"),
        [this]() -> Dia::Automation::CheckpointResult {
            bool ok = (mWavesCompleted >= 2);
            return { ok, ok ? "wave 2 cleared" : "wave 2 in progress", 0.f };
        });

    service->RegisterCheckpoint(this, Dia::Core::StringCRC("arena.wave3_complete"),
        [this]() -> Dia::Automation::CheckpointResult {
            bool ok = (mWavesCompleted >= 3);
            return { ok, ok ? "wave 3 cleared" : "wave 3 in progress", 0.f };
        });

    service->RegisterCheckpoint(this, Dia::Core::StringCRC("arena.victory"),
        [this]() -> Dia::Automation::CheckpointResult {
            bool ok = mObjectives.AllPrimaryComplete();
            return { ok, ok ? "all primary objectives complete" : "in progress", 0.f };
        });

    service->RegisterCheckpoint(this, Dia::Core::StringCRC("arena.powerup_collected"),
        [this]() -> Dia::Automation::CheckpointResult {
            return { mPowerupCollected,
                     mPowerupCollected ? "powerup taken" : "not yet",
                     0.f };
        });

    service->RegisterCheckpoint(this, Dia::Core::StringCRC("arena.desperate_charge_triggered"),
        [this]() -> Dia::Automation::CheckpointResult {
            return { mDespChargeTriggered,
                     mDespChargeTriggered ? "desperate charge fired" : "none yet",
                     0.f };
        });
}

// ---------------------------------------------------------------------------
// RegisterMetrics
// ---------------------------------------------------------------------------
void ArenaTestStageModule::RegisterMetrics()
{
    auto& reg = Dia::Observation::Metric::MetricRegistry::Instance();
    if (!mMetricTotalKills)
        mMetricTotalKills = reg.RegisterGauge(Dia::Core::StringCRC("cluichetest.arena.total_kills"));
    if (!mMetricWavesCompleted)
        mMetricWavesCompleted = reg.RegisterGauge(Dia::Core::StringCRC("cluichetest.arena.waves_completed"));
    if (!mMetricPowerupCollected)
        mMetricPowerupCollected = reg.RegisterGauge(Dia::Core::StringCRC("cluichetest.arena.powerup_collected"));
    if (!mMetricDespChargesFired)
        mMetricDespChargesFired = reg.RegisterGauge(Dia::Core::StringCRC("cluichetest.arena.desperate_charges_fired"));
    if (!mMetricTotalFrames)
        mMetricTotalFrames = reg.RegisterGauge(Dia::Core::StringCRC("cluichetest.arena.total_frames"));
}

// ---------------------------------------------------------------------------
// OnStart
// ---------------------------------------------------------------------------
void ArenaTestStageModule::OnStart(Dia::Automation::AutomationService* /*service*/)
{
    // Reset all state for re-entry.
    mPlayerPosition      = Dia::Maths::Vector2D(0.f, -3.f);
    mTotalKills          = 0;
    mWavesCompleted      = 0;
    mDespChargesFired    = 0;
    mDespChargeTriggered = false;
    mPowerupCollected    = false;
    mVictory             = false;
    mAllPassed           = false;
    mWave1ClearDelay     = -1.f;
    mWave2ClearDelay     = -1.f;
    mEnemyCount          = 0;
    mFrameCount          = 0;

    // Register global blackboard slots.
    // Register<T> returns T& so we can set the initial value inline.
    mGlobalBlackboard.Register<int>(Dia::Core::StringCRC("wave_num"))          = 0;
    mGlobalBlackboard.Register<int>(Dia::Core::StringCRC("total_kills"))       = 0;
    mGlobalBlackboard.Register<int>(Dia::Core::StringCRC("wave1_cleared"))     = 0;
    mGlobalBlackboard.Register<int>(Dia::Core::StringCRC("wave2_cleared"))     = 0;
    mGlobalBlackboard.Register<int>(Dia::Core::StringCRC("wave3_cleared"))     = 0;
    mGlobalBlackboard.Register<int>(Dia::Core::StringCRC("powerup_collected")) = 0;

    LoadTriggerScript();
    LoadObjectives();
    RegisterCheckpoints();
    RegisterMetrics();

    DIA_LOG_INFO("CluicheTest", "ArenaTestStageModule::OnStart");
}

// ---------------------------------------------------------------------------
// OnUpdate
// ---------------------------------------------------------------------------
void ArenaTestStageModule::OnUpdate(float deltaTime)
{
    mTriggerScript.Tick(deltaTime);

    if (mGlobalConditionRegistry)
        mObjectives.Evaluate(*mGlobalConditionRegistry);

    UpdateEnemyAI(deltaTime);

    // Advance player northward from (0,-3) until y >= 0.
    static const Dia::Core::StringCRC kPlayerTag("player");
    if (mPlayerPosition.y < 0.f)
    {
        mPlayerPosition.y += 1.0f * deltaTime;
        if (mPlayerPosition.y > 0.f)
            mPlayerPosition.y = 0.f;

        // Update spatial component so the trigger system sees the new position.
        Dia::EntitySpatial::SpatialComponent* sc =
            mSpatialDomain.GetComponent<Dia::EntitySpatial::SpatialComponent>(mPlayerEntity);
        if (sc)
        {
            sc->position = mPlayerPosition;
            sc->MarkDirty();
        }
    }

    mSpatialDomain.EndOfFrame();

    if (mMetricTotalKills)        mMetricTotalKills->Set(static_cast<double>(mTotalKills));
    if (mMetricWavesCompleted)    mMetricWavesCompleted->Set(static_cast<double>(mWavesCompleted));
    if (mMetricPowerupCollected)  mMetricPowerupCollected->Set(mPowerupCollected ? 1.0 : 0.0);
    if (mMetricDespChargesFired)  mMetricDespChargesFired->Set(static_cast<double>(mDespChargesFired));
    if (mMetricTotalFrames)       mMetricTotalFrames->Set(static_cast<double>(mFrameCount + 1));

    ++mFrameCount;
}

// ---------------------------------------------------------------------------
// OnStop
// ---------------------------------------------------------------------------
void ArenaTestStageModule::OnStop()
{
    // TriggerScriptModule is non-copyable and has no explicit move — cannot
    // reset via assignment. Instead we clear the trigger list via LoadFromJson
    // with an empty document, and null the dependency pointers.
    {
        Json::Value emptyRoot;
        emptyRoot["triggers"] = Json::Value(Json::arrayValue);
        Dia::Core::Containers::DynamicArrayC<const char*, 32> dummy;
        // Build a throwaway registry for validation (no slots needed for empty list).
        Dia::Condition::ConditionRegistry emptyReg(nullptr);
        mTriggerScript.LoadFromJson(emptyRoot, emptyReg, dummy);
        mTriggerScript.SetActionRegistry(nullptr);
        mTriggerScript.SetConditionContext(nullptr);
        mTriggerScript.SetSpatialModule(nullptr);
    }

    // Reset objectives.
    mObjectives = Dia::Objective::ObjectiveSet();

    // Reset condition registry (rebuilt in next OnStart).
    mGlobalConditionRegistry.reset();

    // Reset spatial module.
    mEntitySpatialModule.reset();

    // Unregister global blackboard slots.
    mGlobalBlackboard.Unregister(Dia::Core::StringCRC("wave_num"));
    mGlobalBlackboard.Unregister(Dia::Core::StringCRC("total_kills"));
    mGlobalBlackboard.Unregister(Dia::Core::StringCRC("wave1_cleared"));
    mGlobalBlackboard.Unregister(Dia::Core::StringCRC("wave2_cleared"));
    mGlobalBlackboard.Unregister(Dia::Core::StringCRC("wave3_cleared"));
    mGlobalBlackboard.Unregister(Dia::Core::StringCRC("powerup_collected"));

    // Clear enemy pool — reset per-enemy state for the active slots.
    for (unsigned int i = 0; i < mEnemyCount; ++i)
    {
        EnemyAgent& e = mEnemies[i];
        e.fsm.reset();
        e.conditionRegistry.reset();
        e.blackboard.Unregister(Dia::Core::StringCRC("health"));
        e.blackboard.Unregister(Dia::Core::StringCRC("player_distance"));
        e.blackboard.Unregister(Dia::Core::StringCRC("fsm_state"));
        e.blackboard.Unregister(Dia::Core::StringCRC("chosen_action"));
        e.health              = 1.0f;
        e.alive               = true;
        e.desperateChargeFired = false;
        e.modulePtr           = nullptr;
    }
    mEnemyCount = 0;

    // Reset counters and flags.
    mTotalKills          = 0;
    mWavesCompleted      = 0;
    mDespChargesFired    = 0;
    mDespChargeTriggered = false;
    mPowerupCollected    = false;
    mVictory             = false;
    mAllPassed           = false;
    mWave1ClearDelay     = -1.f;
    mWave2ClearDelay     = -1.f;

    DIA_LOG_INFO("CluicheTest", "ArenaTestStageModule::OnStop");
}

// ---------------------------------------------------------------------------
// Private helpers — stubs for Tasks 4-8
// ---------------------------------------------------------------------------

void ArenaTestStageModule::SpawnWave(Dia::Core::StringCRC tag, int count)
{
    DIA_LOG_INFO("CluicheTest", "Arena: SpawnWave count=%d", count);

    // Determine base spawn position for this wave tag.
    const Dia::Core::StringCRC kWave1Tag("enemy_wave1");
    const Dia::Core::StringCRC kWave2Tag("enemy_wave2");
    const Dia::Core::StringCRC kWave3Tag("enemy_wave3");

    float baseX = -8.f;
    float baseY =  8.f;
    if (tag == kWave3Tag)
    {
        baseX = 8.f;
        baseY = 8.f;
    }

    // Per-enemy UtilitySet and RuleSet are loaded from the same JSON per spawn.
    for (int i = 0; i < count; ++i)
    {
        // Access the pre-constructed slot directly; unique_ptr members cannot be copy-assigned.
        DIA_ASSERT(mEnemyCount < kMaxEnemies, "Enemy pool full");
        EnemyAgent& enemy = mEnemies[mEnemyCount++];

        // --- Position ---
        enemy.position = Dia::Maths::Vector2D(
            baseX + static_cast<float>(i) * 0.5f,
            baseY + static_cast<float>(i) * 0.3f);
        enemy.health  = 1.0f;
        enemy.waveTag = tag;
        enemy.alive   = true;
        enemy.desperateChargeFired = false;
        enemy.modulePtr = this;

        // --- Blackboard ---
        enemy.blackboard.Register<float>(Dia::Core::StringCRC("health"))          = 1.0f;
        enemy.blackboard.Register<float>(Dia::Core::StringCRC("player_distance")) = 99.f;
        enemy.blackboard.Register<int>  (Dia::Core::StringCRC("fsm_state"))       = 0;
        enemy.blackboard.Register<Dia::Core::StringCRC>(Dia::Core::StringCRC("chosen_action")) =
            Dia::Core::StringCRC("none");

        // --- ConditionRegistry ---
        // void* data = &enemy; accessors cast it back to EnemyAgent*.
        enemy.conditionRegistry = std::make_unique<Dia::Condition::ConditionRegistry>(&enemy);

        const Dia::Core::StringCRC slotSelf("self");
        enemy.conditionRegistry->RegisterFloat(
            slotSelf, Dia::Core::StringCRC("health"), EnemyCondition_Health);
        enemy.conditionRegistry->RegisterFloat(
            slotSelf, Dia::Core::StringCRC("player_distance"), EnemyCondition_PlayerDistance);

        // --- UtilitySet ---
        {
            Json::Value root;
            Json::Reader reader;
            reader.parse(kEnemyUtilitySetJson, root);
            enemy.utilitySet = Dia::UtilityAI::UtilitySet::LoadFromJson(root);
        }

        // --- RuleSet ---
        {
            Json::Value root;
            Json::Reader reader;
            reader.parse(kEnemyRuleSetJson, root);
            enemy.ruleSet = Dia::Rules::RuleSet::LoadFromJson(root);
        }

        // --- RuleActionRegistry ---
        // Static callback; actionContext passed at Evaluate() time will be &enemy.
        enemy.ruleActionRegistry.Register(Dia::Core::StringCRC("DespCharge"), EnemyAction_DespCharge);

        // --- FlatStateMachine ---
        // Transitions use trigger IDs that match string "check" — the FSM is driven by
        // explicit Fire() calls in UpdateEnemyAI (Task 6). Guards evaluated on Fire().
        //
        // Trigger naming convention: "check_<transition>" — unique per arc so multiple
        // transitions from the same source can be fired selectively.
        Dia::StateMachine::StateMachineDefinition def =
            Dia::StateMachine::StateMachineBuilder()
                .State(Dia::Core::StringCRC("Idle"))
                    .OnEnter(EnemyFsm_OnEnterIdle)
                    .Transition(Dia::Core::StringCRC("Pursue"), Dia::Core::StringCRC("check_idle"))
                        .Guard(EnemyGuard_IdleToPursue)
                .State(Dia::Core::StringCRC("Pursue"))
                    .OnEnter(EnemyFsm_OnEnterPursue)
                    .Transition(Dia::Core::StringCRC("Attack"), Dia::Core::StringCRC("check_pursue"))
                        .Guard(EnemyGuard_PursueToAttack)
                .State(Dia::Core::StringCRC("Attack"))
                    .OnEnter(EnemyFsm_OnEnterAttack)
                    .Transition(Dia::Core::StringCRC("Dead"),   Dia::Core::StringCRC("check_attack"))
                        .Guard(EnemyGuard_AttackToDead)
                    .Transition(Dia::Core::StringCRC("Pursue"), Dia::Core::StringCRC("check_attack_retreat"))
                        .Guard(EnemyGuard_AttackToPursue)
                .State(Dia::Core::StringCRC("Dead"))
                    .OnEnter(EnemyFsm_OnEnterDead)
                .InitialState(Dia::Core::StringCRC("Idle"))
                .Build();

        enemy.fsm = std::make_unique<Dia::StateMachine::FlatStateMachine<EnemyAgent>>(
            Dia::Core::StringCRC("ArenaEnemy"),
            static_cast<Dia::StateMachine::StateMachineDefinition&&>(def),
            enemy);
    }

    // Update global blackboard wave_num.
    mGlobalBlackboard.Get<int>(Dia::Core::StringCRC("wave_num")) = mWavesCompleted + 1;
}

void ArenaTestStageModule::OnFireEvent(Dia::Core::StringCRC eventId)
{
    static const Dia::Core::StringCRC kWave1Done("wave1_done");
    static const Dia::Core::StringCRC kWave2Done("wave2_done");
    static const Dia::Core::StringCRC kWave3Done("wave3_done");
    static const Dia::Core::StringCRC kArenaVictory("arena_victory");

    if (eventId == kWave1Done)
    {
        mGlobalBlackboard.Get<int>(Dia::Core::StringCRC("wave1_cleared")) = 1;
        ++mWavesCompleted;
    }
    else if (eventId == kWave2Done)
    {
        mGlobalBlackboard.Get<int>(Dia::Core::StringCRC("wave2_cleared")) = 1;
        ++mWavesCompleted;
    }
    else if (eventId == kWave3Done)
    {
        mGlobalBlackboard.Get<int>(Dia::Core::StringCRC("wave3_cleared")) = 1;
        ++mWavesCompleted;
    }
    else if (eventId == kArenaVictory)
    {
        mVictory = true;
    }
    // "desperate_charge" action is handled by EnemyAction_DespCharge -> NotifyDespChargeFired()
}

void ArenaTestStageModule::OnChangeObjectiveState(Dia::Core::StringCRC /*objectiveId*/,
                                                  Dia::Core::StringCRC /*state*/)
{
    // ObjectiveSet has no direct SetState mutation — state is driven by condition
    // evaluation on the blackboard each frame via mObjectives.Evaluate().
}

void ArenaTestStageModule::UpdateEnemyAI(float deltaTime)
{
    static const Dia::Core::StringCRC kIdle  ("Idle");
    static const Dia::Core::StringCRC kPursue("Pursue");
    static const Dia::Core::StringCRC kAttack("Attack");

    static const Dia::Core::StringCRC kTrigCheckIdle          ("check_idle");
    static const Dia::Core::StringCRC kTrigCheckPursue        ("check_pursue");
    static const Dia::Core::StringCRC kTrigCheckAttack        ("check_attack");
    static const Dia::Core::StringCRC kTrigCheckAttackRetreat ("check_attack_retreat");

    static const Dia::Core::StringCRC kSlotHealth        ("health");
    static const Dia::Core::StringCRC kSlotPlayerDist    ("player_distance");
    static const Dia::Core::StringCRC kSlotChosenAction  ("chosen_action");

    for (unsigned int i = 0; i < mEnemyCount; ++i)
    {
        EnemyAgent& enemy = mEnemies[i];
        if (!enemy.alive)
            continue;

        // a. Distance to player
        Dia::Maths::Vector2D diff = enemy.position - mPlayerPosition;
        float playerDist = diff.Magnitude();

        // b. Update blackboard
        enemy.blackboard.Get<float>(kSlotHealth)     = enemy.health;
        enemy.blackboard.Get<float>(kSlotPlayerDist) = playerDist;

        // c. Fire FSM trigger based on current state
        Dia::Core::StringCRC curState = enemy.fsm->GetCurrentStateId();
        if (curState == kIdle)
        {
            enemy.fsm->Fire(kTrigCheckIdle);
        }
        else if (curState == kPursue)
        {
            enemy.fsm->Fire(kTrigCheckPursue);
        }
        else if (curState == kAttack)
        {
            enemy.fsm->Fire(kTrigCheckAttack);
            enemy.fsm->Fire(kTrigCheckAttackRetreat);
        }

        // d. Drive FSM update (OnUpdate callbacks, advances time)
        enemy.fsm->Update(deltaTime);

        // e. Utility evaluation — score-only to avoid unintended dispatches
        if (enemy.conditionRegistry)
        {
            Dia::UtilityAI::UtilitySelection sel =
                enemy.utilitySet.SelectWinner(*enemy.conditionRegistry);
            enemy.blackboard.Get<Dia::Core::StringCRC>(kSlotChosenAction) = sel.actionId;
        }

        // f. Rule evaluation — DespCharge fires if health < 0.2
        if (enemy.conditionRegistry)
        {
            enemy.ruleSet.Evaluate(*enemy.conditionRegistry,
                                   enemy.ruleActionRegistry,
                                   static_cast<void*>(&enemy));
        }

        // g. Move toward player if alive
        Dia::Maths::Vector2D dir = mPlayerPosition - enemy.position;
        if (dir.Magnitude() > 0.001f)
        {
            dir.NormalizeSafe();
            enemy.position += dir * (1.5f * deltaTime);
        }

        // h. Deal damage to player while in Attack state
        Dia::Core::StringCRC stateAfterFsm = enemy.fsm->GetCurrentStateId();
        if (stateAfterFsm == kAttack)
            enemy.health -= 0.15f * deltaTime;

        // i. Kill enemy when health depleted
        if (enemy.health <= 0.f && enemy.alive)
        {
            enemy.alive  = false;
            enemy.health = 0.f;
            ++mTotalKills;
            mGlobalBlackboard.Get<int>(Dia::Core::StringCRC("total_kills")) = mTotalKills;
            mTriggerScript.IncrementCount(enemy.waveTag, 1);
            enemy.blackboard.Get<float>(kSlotHealth) = 0.f;
        }
    }
}

bool ArenaTestStageModule::AllCheckpointsPassed() const
{
    return mWavesCompleted >= 3
        && mObjectives.AllPrimaryComplete()
        && mPowerupCollected
        && mDespChargeTriggered;
}

} // namespace CluicheTest

namespace { using ArenaTestStageModule_ = CluicheTest::ArenaTestStageModule; }
DIA_MODULE(ArenaTestStageModule_);
DIA_DESCRIBE(ArenaTestStageModule_::kTypeId, "Multi-system AI/progression E2E: TriggerScript + Objective + Blackboard + StateMachine + UtilityAI + Rules");
