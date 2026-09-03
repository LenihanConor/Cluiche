#pragma once
#include "Modules/TestStages/TestStageModuleBase.h"
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaApplicationFlow/ModuleRefV2.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaBlackboard/BlackboardComponent.h>
#include <DiaCondition/ConditionRegistry.h>
#include <DiaRules/RuleActionRegistry.h>
#include <DiaRules/RuleSetComponent.h>
#include <DiaUtilityAI/UtilitySet.h>
#include <DiaUtilityAI/UtilitySetComponent.h>
#include <DiaSimTime/SimTimeBudget.h>
#include <memory>

#ifdef DIA_DEBUG
#include "Modules/VisualDebuggerModule.h"
#include "Modules/TestStages/Drawers/AIDecisionTestDrawer.h"
#endif

namespace CluicheTest {

class AIDecisionTestStageModule : public TestStageModuleBase
{
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kSim;
    static constexpr const char* kDescription = "Integration stage: Condition + Rules + UtilityAI (sync/async) + DiaSimTime";
    static constexpr unsigned int kMinDisplayFrames = 150; // 5 s at 30 Hz

    explicit AIDecisionTestStageModule(const Dia::Core::StringCRC& instanceId);
    ~AIDecisionTestStageModule() override;

protected:
    Dia::Core::StringCRC GetStageName() const override;
    unsigned int GetBudgetFrames() const override { return 120; }
    const Dia::Core::StringCRC* GetCheckpointNames(unsigned int& outCount) const override;
    void OnStart(Dia::Automation::AutomationService* service) override;
    void OnUpdate(float deltaTime) override;
    void OnStop() override;

private:
    void SetupAI();
    void RegisterCheckpoints(Dia::Automation::AutomationService* service);
    bool AllCheckpointsPassed() const;

    static void OnUtilityAsyncResult(Dia::UtilityAI::UtilitySelection result, void* userData);

    // --- Data ---
    Dia::Blackboard::BlackboardComponent    mBlackboard;
    std::unique_ptr<Dia::Condition::ConditionRegistry> mConditionRegistry;

    Dia::Rules::RuleActionRegistry          mActionRegistry;
    Dia::Rules::RuleSetComponent            mRuleSetComponent;
    Dia::UtilityAI::UtilitySetComponent     mUtilitySetComponent;
    Dia::SimTime::SimTimeBudget              mBudget;

    // Checkpoint flags
    bool mConditionHealthLow       = false;
    bool mConditionEnemyVisible    = false;
    bool mRulesCallForHelpFired    = false;
    bool mUtilityFleeWins          = false;
    bool mBudgetAsyncCompleted     = false;

    // Separate UtilitySet instance owned for the async evaluation path
    Dia::UtilityAI::UtilitySet              mAsyncUtilitySet;

    // Live blackboard mirrors for the drawer (updated each frame from the blackboard)
    float mLiveHealth        = 0.0f;
    bool  mLiveEnemyVisible  = false;
    float mLiveEnemyDistance = 0.0f;

    // State
    bool mAIReady                  = false;
    bool mAsyncSubmitted           = false;
    bool mAllPassed                = false;

#ifdef DIA_DEBUG
    Dia::ApplicationFlow::ModuleRef<Cluiche::AppFlow::VisualDebuggerModule> mVisualDebuggerRef{this};
    std::unique_ptr<AIDecisionTestDrawer> mDrawer;
#endif
};

} // namespace CluicheTest
