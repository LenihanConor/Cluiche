#pragma once
#include "Modules/TestStages/TestStageModuleBase.h"
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaApplicationFlow/ModuleRefV2.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaBlackboard/BlackboardComponent.h>
#include <DiaCondition/ConditionRegistry.h>
#include <DiaRules/RuleActionRegistry.h>
#include <DiaHTN/HTNDomain.h>
#include <DiaHTN/OperatorRegistry.h>
#include <DiaHTN/HTNPlannerComponent.h>
#include <DiaHTN/HTNPlanner.h>
#include <DiaAIBudget/AIBudgetScheduler.h>
#include <memory>

#ifdef DIA_DEBUG
#include "Modules/VisualDebuggerModule.h"
#include "Modules/TestStages/Drawers/AIHTNTestDrawer.h"
#endif

namespace CluicheTest {

class AIHTNTestStageModule : public TestStageModuleBase
{
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kSim;
    static constexpr const char* kDescription = "Integration stage: HTN (sync + diverge/replan + async) + AIBudget + RuleActionBridge";
    static constexpr unsigned int kMinDisplayFrames = 150; // 5 s at 30 Hz

    explicit AIHTNTestStageModule(const Dia::Core::StringCRC& instanceId);
    ~AIHTNTestStageModule() override;

protected:
    Dia::Core::StringCRC GetStageName() const override;
    unsigned int GetBudgetFrames() const override { return 240; }
    const Dia::Core::StringCRC* GetCheckpointNames(unsigned int& outCount) const override;
    void OnStart(Dia::Automation::AutomationService* service) override;
    void OnUpdate(float deltaTime) override;
    void OnStop() override;

private:
    void SetupAI();
    void RegisterCheckpoints(Dia::Automation::AutomationService* service);
    bool AllCheckpointsPassed() const;

    // --- Data ---
    Dia::Blackboard::BlackboardComponent    mBlackboard;
    std::unique_ptr<Dia::Condition::ConditionRegistry> mConditionRegistry;

    Dia::Rules::RuleActionRegistry          mActionRegistry;
    Dia::HTN::HTNDomain                     mDomain;
    Dia::HTN::OperatorRegistry              mOperatorRegistry;
    Dia::HTN::HTNPlannerComponent           mHTNComponent;
    Dia::HTN::HTNPlanner                    mStandalonePlanner; // used for direct async call
    Dia::AIBudget::AIBudgetScheduler        mScheduler;

    // Async result storage (filled by standalone planner callback)
    Dia::HTN::HTNPlan                       mAsyncResultPlan;
    bool                                    mAsyncCallbackFired = false;

    static void OnStandaloneAsyncPlan(Dia::HTN::HTNPlan plan, void* userData);

    // Operator call counters (void* operatorContext = this)
    int mRetreatFireCount     = 0;
    int mAttackFireCount      = 0;
    int mCallForHelpFireCount = 0;

    // Checkpoint flags
    bool mPlanBuilt             = false;
    bool mPlanComplete          = false;
    bool mDivergedAndReplanned  = false;
    bool mRuleBridgeFired       = false;
    bool mAsyncPlanCompleted    = false;
    bool mAsyncPlanCorrect      = false;

    // State machine — use drawer's Phase enum so the drawer can read it directly
#ifdef DIA_DEBUG
    using Phase = AIHTNTestDrawer::Phase;
#else
    enum class Phase { kFirstPlan, kExecuting, kMutating, kSecondPlan, kAsyncSubmit, kAsyncWait, kDone };
#endif
    Phase mPhase               = Phase::kFirstPlan;
    bool  mAIReady             = false;
    bool  mAsyncSubmitted      = false;
    bool  mAllPassed           = false;

    // Live blackboard mirror for the drawer
    float mLiveHealth          = 0.0f;

    // Expected operator count for async plan (health=80 → Attack only, 1 operator)
    int mAsyncPlanExpectedCount = 1;

#ifdef DIA_DEBUG
    Dia::ApplicationFlow::ModuleRef<Cluiche::AppFlow::VisualDebuggerModule> mVisualDebuggerRef{this};
    std::unique_ptr<AIHTNTestDrawer> mDrawer;
#endif
};

} // namespace CluicheTest
