#include "Modules/TestStages/AIDecisionTestStageModule.h"

#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaAutomation/AutomationService.h>
#include <DiaBlackboard/Blackboard.h>
#include <DiaCondition/ConditionExpr.h>
#include <DiaRules/RuleSet.h>
#include <DiaUtilityAI/UtilitySet.h>
#include <DiaCore/Json/external/json/json.h>
#include <DiaObservation/Log/DiaLog.h>

namespace CluicheTest {

const Dia::Core::StringCRC AIDecisionTestStageModule::kTypeId("AIDecisionTestStageModule");

// JSON for the rule set: health < 50 fires CallForHelp
static constexpr const char* kRuleSetJson = R"({
    "rules": [
        {
            "id": "health_low_call_for_help",
            "guard": { "slot": "self", "field": "health", "op": "<", "value": 50.0 },
            "actions": ["CallForHelp"]
        }
    ]
})";

// JSON for the utility set:
//   Attack: prereq enemy.visible == true, linear scorer on enemy.distance (high when near, so invert)
//   Flee:   no prereq, logistic scorer on self.health inverted (high score when health low)
static constexpr const char* kUtilitySetJson = R"({
    "actions": [
        {
            "id": "Attack",
            "prerequisite": { "slot": "enemy", "field": "visible", "op": "==", "value": true },
            "scorers": [
                {
                    "slot": "enemy",
                    "field": "distance",
                    "input_min": 0.0,
                    "input_max": 20.0,
                    "curve": { "shape": "linear", "invert": true }
                }
            ]
        },
        {
            "id": "Flee",
            "scorers": [
                {
                    "slot": "self",
                    "field": "health",
                    "input_min": 0.0,
                    "input_max": 100.0,
                    "curve": { "shape": "logistic", "param": 10.0, "invert": true }
                }
            ]
        }
    ]
})";

AIDecisionTestStageModule::AIDecisionTestStageModule(const Dia::Core::StringCRC& instanceId)
    : TestStageModuleBase(instanceId)
{}

AIDecisionTestStageModule::~AIDecisionTestStageModule() = default;

Dia::Core::StringCRC AIDecisionTestStageModule::GetStageName() const
{
    return Dia::Core::StringCRC("AIDecisionTestStage");
}

const Dia::Core::StringCRC* AIDecisionTestStageModule::GetCheckpointNames(unsigned int& outCount) const
{
    static const Dia::Core::StringCRC names[] = {
        Dia::Core::StringCRC("ai.condition.health_low_passes"),
        Dia::Core::StringCRC("ai.condition.enemy_visible_passes"),
        Dia::Core::StringCRC("ai.rules.call_for_help_fired"),
        Dia::Core::StringCRC("ai.utility.flee_wins"),
        Dia::Core::StringCRC("ai.budget.utility_async_completed"),
    };
    outCount = 5;
    return names;
}

void AIDecisionTestStageModule::OnStart(Dia::Automation::AutomationService* service)
{
    SetupAI();
    RegisterCheckpoints(service);
}

void AIDecisionTestStageModule::SetupAI()
{
    // --- Blackboard ---
    auto& bb = mBlackboard.GetBlackboard();
    bb.Register<float>(Dia::Core::StringCRC("health")) = 30.0f;
    bb.Register<bool>(Dia::Core::StringCRC("visible"))  = true;
    bb.Register<float>(Dia::Core::StringCRC("distance")) = 5.0f;

    // --- ConditionRegistry bridging Blackboard slots ---
    // ConditionRegistry takes a void* data; callbacks receive it back.
    // We store the Blackboard pointer as data.
    mConditionRegistry = std::make_unique<Dia::Condition::ConditionRegistry>(&bb);

    mConditionRegistry->RegisterFloat(
        Dia::Core::StringCRC("self"), Dia::Core::StringCRC("health"),
        [](void* d) { return static_cast<Dia::Blackboard::Blackboard*>(d)->Get<float>(Dia::Core::StringCRC("health")); });

    mConditionRegistry->RegisterBool(
        Dia::Core::StringCRC("enemy"), Dia::Core::StringCRC("visible"),
        [](void* d) { return static_cast<Dia::Blackboard::Blackboard*>(d)->Get<bool>(Dia::Core::StringCRC("visible")); });

    mConditionRegistry->RegisterFloat(
        Dia::Core::StringCRC("enemy"), Dia::Core::StringCRC("distance"),
        [](void* d) { return static_cast<Dia::Blackboard::Blackboard*>(d)->Get<float>(Dia::Core::StringCRC("distance")); });

    // --- Action registry ---
    mActionRegistry.Register(Dia::Core::StringCRC("CallForHelp"),
        [](void* ctx) {
            auto* module = static_cast<AIDecisionTestStageModule*>(ctx);
            module->mRulesCallForHelpFired = true;
        });

    mActionRegistry.Register(Dia::Core::StringCRC("Attack"), [](void*) {});
    mActionRegistry.Register(Dia::Core::StringCRC("Flee"),
        [](void* ctx) {
            auto* module = static_cast<AIDecisionTestStageModule*>(ctx);
            module->mUtilityFleeWins = true;
        });

    // --- RuleSetComponent ---
    {
        Json::Value root;
        Json::Reader().parse(kRuleSetJson, root);
        mRuleSetComponent.SetRuleSet(Dia::Rules::RuleSet::LoadFromJson(root));
        mRuleSetComponent.SetRegistry(&mActionRegistry);
    }

    // --- UtilitySetComponent ---
    {
        Json::Value root;
        Json::Reader().parse(kUtilitySetJson, root);
        mUtilitySetComponent.SetUtilitySet(Dia::UtilityAI::UtilitySet::LoadFromJson(root));
        mUtilitySetComponent.SetRegistry(&mActionRegistry);
    }

    // Load a second UtilitySet for the async path (EvaluateAsync is non-const)
    {
        Json::Value root;
        Json::Reader().parse(kUtilitySetJson, root);
        mAsyncUtilitySet = Dia::UtilityAI::UtilitySet::LoadFromJson(root);
    }

    mAIReady = true;
}

void AIDecisionTestStageModule::RegisterCheckpoints(Dia::Automation::AutomationService* service)
{
    service->RegisterCheckpoint(this, Dia::Core::StringCRC("ai.condition.health_low_passes"),
        [this]() -> Dia::Automation::CheckpointResult {
            return { mConditionHealthLow,
                     mConditionHealthLow ? "ConditionExpr{health<50} passed" : "pending", 0.f };
        });

    service->RegisterCheckpoint(this, Dia::Core::StringCRC("ai.condition.enemy_visible_passes"),
        [this]() -> Dia::Automation::CheckpointResult {
            return { mConditionEnemyVisible,
                     mConditionEnemyVisible ? "ConditionExpr{enemy.visible==true} passed" : "pending", 0.f };
        });

    service->RegisterCheckpoint(this, Dia::Core::StringCRC("ai.rules.call_for_help_fired"),
        [this]() -> Dia::Automation::CheckpointResult {
            return { mRulesCallForHelpFired,
                     mRulesCallForHelpFired ? "RuleSet fired CallForHelp" : "pending", 0.f };
        });

    service->RegisterCheckpoint(this, Dia::Core::StringCRC("ai.utility.flee_wins"),
        [this]() -> Dia::Automation::CheckpointResult {
            return { mUtilityFleeWins,
                     mUtilityFleeWins ? "UtilitySet selected Flee" : "pending", 0.f };
        });

    service->RegisterCheckpoint(this, Dia::Core::StringCRC("ai.budget.utility_async_completed"),
        [this]() -> Dia::Automation::CheckpointResult {
            return { mBudgetAsyncCompleted,
                     mBudgetAsyncCompleted ? "EvaluateAsync callback fired" : "pending", 0.f };
        });
}

/*static*/ void AIDecisionTestStageModule::OnUtilityAsyncResult(
    Dia::UtilityAI::UtilitySelection /*result*/, void* userData)
{
    auto* self = static_cast<AIDecisionTestStageModule*>(userData);
    self->mBudgetAsyncCompleted = true;
}

bool AIDecisionTestStageModule::AllCheckpointsPassed() const
{
    return mConditionHealthLow && mConditionEnemyVisible
        && mRulesCallForHelpFired && mUtilityFleeWins && mBudgetAsyncCompleted;
}


void AIDecisionTestStageModule::OnUpdate(float /*deltaTime*/)
{
    if (!mAIReady) return;

    auto& bb = mBlackboard.GetBlackboard();

    // Mirror live blackboard values for the drawer
    mLiveHealth        = bb.Get<float>(Dia::Core::StringCRC("health"));
    mLiveEnemyVisible  = bb.Get<bool>(Dia::Core::StringCRC("visible"));
    mLiveEnemyDistance = bb.Get<float>(Dia::Core::StringCRC("distance"));

#ifdef DIA_DEBUG
    if (!mDrawer)
    {
        auto* vd = mVisualDebuggerRef.Get();
        if (vd)
        {
            mDrawer = std::make_unique<AIDecisionTestDrawer>(
                mLiveHealth, mLiveEnemyVisible, mLiveEnemyDistance,
                mConditionHealthLow, mConditionEnemyVisible,
                mRulesCallForHelpFired, mUtilityFleeWins, mBudgetAsyncCompleted,
                mAllPassed,
                vd->GetLayerManager());
            vd->GetLayerManager().Register(mDrawer.get(), 20, Dia::Core::StringCRC("AIDecision"));
        }
    }
#endif

    // Frame 1: evaluate conditions, rules, utility (sync), then submit async
    if (GetFrameCount() == 1)
    {
        // Standalone condition expressions
        {
            Json::Value node;
            node["slot"] = "self"; node["field"] = "health"; node["op"] = "<"; node["value"] = 50.0;
            Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
            auto expr = Dia::Condition::ConditionExpr::LoadFromJson(node, errors);
            mConditionHealthLow = expr.Evaluate(*mConditionRegistry);
        }
        {
            Json::Value node;
            node["slot"] = "enemy"; node["field"] = "visible"; node["op"] = "=="; node["value"] = true;
            Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
            auto expr = Dia::Condition::ConditionExpr::LoadFromJson(node, errors);
            mConditionEnemyVisible = expr.Evaluate(*mConditionRegistry);
        }

        // Rules (sync) — fires CallForHelp action which sets mRulesCallForHelpFired
        mRuleSetComponent.Evaluate(*mConditionRegistry, this);

        // Utility (sync) — dispatch winner; Flee action handler sets mUtilityFleeWins
        mUtilitySetComponent.Evaluate(*mConditionRegistry, this, 0.0f);
    }

    // Frame 2: submit async evaluation (mAsyncUtilitySet loaded in SetupAI)
    if (GetFrameCount() == 2 && !mAsyncSubmitted)
    {
        mAsyncSubmitted = true;
        mAsyncUtilitySet.EvaluateAsync(
            *mConditionRegistry, mActionRegistry, this,
            mScheduler, &AIDecisionTestStageModule::OnUtilityAsyncResult, this);
    }

    // Frame 3+: drain scheduler until callback fires
    if (GetFrameCount() >= 3 && !mBudgetAsyncCompleted)
        mScheduler.Update(10.0f);

    if (AllCheckpointsPassed())
    {
        mAllPassed = true;
        if (!IsResolved() && GetFrameCount() >= kMinDisplayFrames)
            ReportPassed();
    }
}

void AIDecisionTestStageModule::OnStop()
{
#ifdef DIA_DEBUG
    if (mDrawer)
    {
        if (auto* vd = mVisualDebuggerRef.Get())
            vd->GetLayerManager().Unregister(mDrawer->GetLayerName());
        mDrawer.reset();
    }
#endif

    mConditionRegistry.reset();
    mAIReady               = false;
    mAsyncSubmitted        = false;
    mAllPassed             = false;
    mLiveHealth            = 0.0f;
    mLiveEnemyVisible      = false;
    mLiveEnemyDistance     = 0.0f;
    mConditionHealthLow    = false;
    mConditionEnemyVisible = false;
    mRulesCallForHelpFired = false;
    mUtilityFleeWins       = false;
    mBudgetAsyncCompleted  = false;
}

} // namespace CluicheTest

namespace { using AIDecisionTestStageModule_ = CluicheTest::AIDecisionTestStageModule; }
DIA_MODULE(AIDecisionTestStageModule_);
DIA_DESCRIBE(AIDecisionTestStageModule_::kTypeId, "Integration stage: Condition + Rules + UtilityAI (sync/async) + AIBudget");
