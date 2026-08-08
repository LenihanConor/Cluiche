#include "Modules/TestStages/ArenaTestStageModule.h"
#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaAutomation/AutomationService.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaEntity/ComponentPool.h>
#include <DiaEntitySpatial/SpatialComponent.h>
#include <DiaCore/Json/external/json/json.h>

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
    mEnemies.RemoveAll();

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

    DIA_LOG_INFO("CluicheTest", "ArenaTestStageModule::OnStart");
}

// ---------------------------------------------------------------------------
// OnUpdate — stub; fully implemented in Tasks 5-8
// ---------------------------------------------------------------------------
void ArenaTestStageModule::OnUpdate(float /*deltaTime*/)
{
    // TODO: implement in Tasks 5-8
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

    // Clear enemy pool.
    mEnemies.RemoveAll();

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

void ArenaTestStageModule::SpawnWave(Dia::Core::StringCRC /*tag*/, int /*count*/)
{
    // TODO: implement in Task 4 (EnemyAgent init)
}

void ArenaTestStageModule::OnFireEvent(Dia::Core::StringCRC /*eventId*/)
{
    // TODO: implement in Task 6
    // wave1_done/wave2_done/wave3_done -> set blackboard flags + increment mWavesCompleted
    // arena_victory -> set mVictory
}

void ArenaTestStageModule::OnChangeObjectiveState(Dia::Core::StringCRC /*objectiveId*/,
                                                  Dia::Core::StringCRC /*state*/)
{
    // TODO: implement in Task 6
}

void ArenaTestStageModule::UpdateEnemyAI(float /*deltaTime*/)
{
    // TODO: implement in Task 5
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
