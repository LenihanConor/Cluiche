#include "Modules/TestStages/AIHTNTestStageModule.h"

#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaAutomation/AutomationService.h>
#include <DiaBlackboard/Blackboard.h>
#include <DiaHTN/HTNPlanner.h>
#include <DiaHTN/HTNPlan.h>
#include <DiaHTN/TaskResult.h>
#include <DiaHTN/RuleActionBridge.h>
#include <DiaCore/Json/external/json/json.h>
#include <DiaObservation/Log/DiaLog.h>

namespace CluicheTest {

const Dia::Core::StringCRC AIHTNTestStageModule::kTypeId("AIHTNTestStageModule");

// Domain JSON:
//   TopGoal → Method 1 (health < 50): [Retreat, CallForHelp]
//   TopGoal → Method 2 (always):      [Attack]
static constexpr const char* kDomainJson = R"({
    "tasks": {
        "TopGoal": {
            "type": "compound",
            "methods": [
                {
                    "id": "TopGoal_LowHealth",
                    "precondition": { "slot": "self", "field": "health", "op": "<", "value": 50.0 },
                    "subtasks": ["Retreat", "CallForHelp"]
                },
                {
                    "id": "TopGoal_Default",
                    "subtasks": ["Attack"]
                }
            ]
        },
        "Retreat":     { "type": "primitive", "operator": "Retreat",     "params": [] },
        "CallForHelp": { "type": "primitive", "operator": "CallForHelp", "params": [] },
        "Attack":      { "type": "primitive", "operator": "Attack",      "params": [] }
    }
})";

AIHTNTestStageModule::AIHTNTestStageModule(const Dia::Core::StringCRC& instanceId)
    : TestStageModuleBase(instanceId)
{}

AIHTNTestStageModule::~AIHTNTestStageModule() = default;

Dia::Core::StringCRC AIHTNTestStageModule::GetStageName() const
{
    return Dia::Core::StringCRC("AIHTNTestStage");
}

const Dia::Core::StringCRC* AIHTNTestStageModule::GetCheckpointNames(unsigned int& outCount) const
{
    static const Dia::Core::StringCRC names[] = {
        Dia::Core::StringCRC("ai.htn.plan_built"),
        Dia::Core::StringCRC("ai.htn.plan_complete"),
        Dia::Core::StringCRC("ai.htn.diverged_and_replanned"),
        Dia::Core::StringCRC("ai.htn.rule_bridge_operator_fired"),
        Dia::Core::StringCRC("ai.htn.async_plan_completed"),
        Dia::Core::StringCRC("ai.htn.async_plan_correct"),
    };
    outCount = 6;
    return names;
}

void AIHTNTestStageModule::OnStart(Dia::Automation::AutomationService* service)
{
    SetupAI();
    RegisterCheckpoints(service);
}

void AIHTNTestStageModule::SetupAI()
{
    // --- Blackboard ---
    auto& bb = mBlackboard.GetBlackboard();
    bb.Register<float>(Dia::Core::StringCRC("health")) = 30.0f;

    // --- ConditionRegistry ---
    mConditionRegistry = std::make_unique<Dia::Condition::ConditionRegistry>(&bb);
    mConditionRegistry->RegisterFloat(
        Dia::Core::StringCRC("self"), Dia::Core::StringCRC("health"),
        [](void* d) { return static_cast<Dia::Blackboard::Blackboard*>(d)->Get<float>(Dia::Core::StringCRC("health")); });

    // --- Domain ---
    {
        Json::Value root;
        Json::Reader().parse(kDomainJson, root);
        Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
        mDomain = Dia::HTN::HTNDomain::LoadFromJson(root, errors);
    }

    // --- Action registry (for RuleActionBridge) ---
    mActionRegistry.Register(Dia::Core::StringCRC("CallForHelp"),
        [](void* ctx) {
            auto* self = static_cast<AIHTNTestStageModule*>(ctx);
            ++self->mCallForHelpFireCount;
            self->mRuleBridgeFired = true;
        });

    // --- Operator registry ---
    mOperatorRegistry.Register(Dia::Core::StringCRC("Retreat"),
        [](void* ctx, const Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 8>&)
        -> Dia::HTN::TaskResult {
            auto* self = static_cast<AIHTNTestStageModule*>(ctx);
            ++self->mRetreatFireCount;
            return Dia::HTN::TaskResult::kSucceeded;
        });

    mOperatorRegistry.Register(Dia::Core::StringCRC("Attack"),
        [](void* ctx, const Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 8>&)
        -> Dia::HTN::TaskResult {
            auto* self = static_cast<AIHTNTestStageModule*>(ctx);
            ++self->mAttackFireCount;
            return Dia::HTN::TaskResult::kSucceeded;
        });

    // Bridge CallForHelp rule action as an instant-succeed operator
    Dia::HTN::RegisterRuleActionAsOperator(
        Dia::Core::StringCRC("CallForHelp"),
        mActionRegistry.Find(Dia::Core::StringCRC("CallForHelp")),
        mOperatorRegistry);

    // --- HTNPlannerComponent ---
    mHTNComponent.SetDomain(&mDomain);
    mHTNComponent.SetRegistry(&mOperatorRegistry);
    mHTNComponent.SetRootTask(Dia::Core::StringCRC("TopGoal"));

    mAIReady = true;
}

void AIHTNTestStageModule::RegisterCheckpoints(Dia::Automation::AutomationService* service)
{
    service->RegisterCheckpoint(this, Dia::Core::StringCRC("ai.htn.plan_built"),
        [this]() -> Dia::Automation::CheckpointResult {
            return { mPlanBuilt, mPlanBuilt ? "sync plan built for TopGoal (health=30)" : "pending", 0.f };
        });

    service->RegisterCheckpoint(this, Dia::Core::StringCRC("ai.htn.plan_complete"),
        [this]() -> Dia::Automation::CheckpointResult {
            return { mPlanComplete, mPlanComplete ? "first plan executed to completion" : "pending", 0.f };
        });

    service->RegisterCheckpoint(this, Dia::Core::StringCRC("ai.htn.diverged_and_replanned"),
        [this]() -> Dia::Automation::CheckpointResult {
            return { mDivergedAndReplanned,
                     mDivergedAndReplanned ? "HasDiverged() triggered replan after health=80 mutation" : "pending",
                     0.f };
        });

    service->RegisterCheckpoint(this, Dia::Core::StringCRC("ai.htn.rule_bridge_operator_fired"),
        [this]() -> Dia::Automation::CheckpointResult {
            return { mRuleBridgeFired,
                     mRuleBridgeFired ? "CallForHelp via RuleActionBridge operator ticked" : "pending",
                     0.f };
        });

    service->RegisterCheckpoint(this, Dia::Core::StringCRC("ai.htn.async_plan_completed"),
        [this]() -> Dia::Automation::CheckpointResult {
            return { mAsyncPlanCompleted,
                     mAsyncPlanCompleted ? "ReplanAsync callback arrived after scheduler.Update()" : "pending",
                     0.f };
        });

    service->RegisterCheckpoint(this, Dia::Core::StringCRC("ai.htn.async_plan_correct"),
        [this]() -> Dia::Automation::CheckpointResult {
            return { mAsyncPlanCorrect,
                     mAsyncPlanCorrect ? "async plan has correct operator count for health=80 (Attack only)" : "pending",
                     0.f };
        });
}

bool AIHTNTestStageModule::AllCheckpointsPassed() const
{
    return mPlanBuilt && mPlanComplete && mDivergedAndReplanned
        && mRuleBridgeFired && mAsyncPlanCompleted && mAsyncPlanCorrect;
}

void AIHTNTestStageModule::OnUpdate(float /*deltaTime*/)
{
    if (!mAIReady) return;

    auto& bb = mBlackboard.GetBlackboard();

    switch (mPhase)
    {
    case Phase::kFirstPlan:
    {
        // Build first sync plan (health=30 → Retreat + CallForHelp)
        mHTNComponent.Replan(*mConditionRegistry);
        mPlanBuilt = mHTNComponent.HasActivePlan();
        if (mPlanBuilt)
            mPhase = Phase::kExecuting;
        break;
    }

    case Phase::kExecuting:
    {
        // Tick one operator per frame until plan completes or fails.
        // Tick() returns kSucceeded both when an operator succeeds AND when the plan is
        // already complete (no more tasks). Check the plan directly for completion.
        const auto result = mHTNComponent.Tick(this);
        if (result == Dia::HTN::TaskResult::kFailed)
        {
            ReportFailed();
            break;
        }
        const Dia::HTN::HTNPlan* plan = mHTNComponent.GetActivePlan();
        if (plan && plan->IsComplete())
        {
            mPlanComplete = true;
            mPhase = Phase::kMutating;
        }
        break;
    }

    case Phase::kMutating:
    {
        // Mutate blackboard to trigger divergence on next HasDiverged() check
        bb.Get<float>(Dia::Core::StringCRC("health")) = 80.0f;
        mPhase = Phase::kSecondPlan;
        break;
    }

    case Phase::kSecondPlan:
    {
        // Verify divergence, then replan
        if (mHTNComponent.HasDiverged(*mConditionRegistry))
        {
            mHTNComponent.Replan(*mConditionRegistry);
            mDivergedAndReplanned = true;

            // Execute second plan (health=80 → Attack only)
            // Tick to completion inline since it's a single instant-succeed operator
            Dia::HTN::TaskResult r = Dia::HTN::TaskResult::kRunning;
            int safety = 32;
            while (r == Dia::HTN::TaskResult::kRunning && --safety > 0)
                r = mHTNComponent.Tick(this);

            // Now submit async plan
            mPhase = Phase::kAsyncSubmit;
        }
        break;
    }

    case Phase::kAsyncSubmit:
    {
        if (!mAsyncSubmitted)
        {
            mAsyncSubmitted = true;
            // Use standalone planner so we can receive the callback directly.
            mStandalonePlanner.PlanAsync(
                Dia::Core::StringCRC("TopGoal"),
                mDomain,
                *mConditionRegistry,
                mScheduler,
                &AIHTNTestStageModule::OnStandaloneAsyncPlan,
                this);
            mPhase = Phase::kAsyncWait;
        }
        break;
    }

    case Phase::kAsyncWait:
    {
        // Drain scheduler; callback fires synchronously inside Update()
        mScheduler.Update(10.0f);

        if (mAsyncCallbackFired && !mAsyncPlanCompleted)
        {
            mAsyncPlanCompleted = true;
            // health=80 → Default method → [Attack] — count = 1
            mAsyncPlanCorrect = (mAsyncResultPlan.GetTaskCount() == mAsyncPlanExpectedCount);
            mPhase = Phase::kDone;
        }
        break;
    }

    case Phase::kDone:
        break;
    }

    if (AllCheckpointsPassed() && !IsResolved())
        ReportPassed();
}

/*static*/ void AIHTNTestStageModule::OnStandaloneAsyncPlan(Dia::HTN::HTNPlan plan, void* userData)
{
    auto* self = static_cast<AIHTNTestStageModule*>(userData);
    self->mAsyncResultPlan   = std::move(plan);
    self->mAsyncCallbackFired = true;
}

void AIHTNTestStageModule::OnStop()
{
    mConditionRegistry.reset();
    mAIReady             = false;
    mAsyncSubmitted      = false;
    mPhase               = Phase::kFirstPlan;
    mRetreatFireCount    = 0;
    mAttackFireCount     = 0;
    mCallForHelpFireCount = 0;
    mPlanBuilt           = false;
    mPlanComplete        = false;
    mDivergedAndReplanned = false;
    mRuleBridgeFired     = false;
    mAsyncPlanCompleted  = false;
    mAsyncPlanCorrect    = false;
    mAsyncCallbackFired  = false;
}

} // namespace CluicheTest

namespace { using AIHTNTestStageModule_ = CluicheTest::AIHTNTestStageModule; }
DIA_MODULE(AIHTNTestStageModule_);
DIA_DESCRIBE(AIHTNTestStageModule_::kTypeId, "Integration stage: HTN (sync + diverge/replan + async) + AIBudget + RuleActionBridge");
