#pragma once
#include "Modules/TestStages/TestStageModuleBase.h"
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaBlackboard/BlackboardComponent.h>
#include <DiaCondition/ConditionRegistry.h>
#include <DiaRules/RuleActionRegistry.h>
#include <DiaRules/RuleSetComponent.h>
#include <DiaUtilityAI/UtilitySet.h>
#include <DiaUtilityAI/UtilitySetComponent.h>
#include <DiaAIBudget/AIBudgetScheduler.h>
#include <memory>

namespace CluicheTest {

class AIDecisionTestStageModule : public TestStageModuleBase
{
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kSim;
    static constexpr const char* kDescription = "Integration stage: Condition + Rules + UtilityAI (sync/async) + AIBudget";

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
    Dia::AIBudget::AIBudgetScheduler        mScheduler;

    // Checkpoint flags
    bool mConditionHealthLow       = false;
    bool mConditionEnemyVisible    = false;
    bool mRulesCallForHelpFired    = false;
    bool mUtilityFleeWins          = false;
    bool mBudgetAsyncCompleted     = false;

    // Separate UtilitySet instance owned for the async evaluation path
    Dia::UtilityAI::UtilitySet              mAsyncUtilitySet;

    // State
    bool mAIReady                  = false;
    bool mAsyncSubmitted           = false;
};

} // namespace CluicheTest
