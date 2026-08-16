#include "Modules/TestStages/BehaviourTreeTestStageModule.h"

#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaAutomation/AutomationService.h>
#include <DiaObservation/Log/DiaLog.h>

namespace CluicheTest {

const Dia::Core::StringCRC BehaviourTreeTestStageModule::kTypeId("BehaviourTreeTestStageModule");

BehaviourTreeTestStageModule::BehaviourTreeTestStageModule(const Dia::Core::StringCRC& instanceId)
    : TestStageModuleBase(instanceId)
{}

Dia::Core::StringCRC BehaviourTreeTestStageModule::GetStageName() const
{
    return Dia::Core::StringCRC("BehaviourTreeTestStage");
}

const Dia::Core::StringCRC* BehaviourTreeTestStageModule::GetCheckpointNames(unsigned int& outCount) const
{
    static const Dia::Core::StringCRC names[] = {
        Dia::Core::StringCRC("bt.patrol_started"),
        Dia::Core::StringCRC("bt.chase_triggered"),
        Dia::Core::StringCRC("bt.target_lost"),
        Dia::Core::StringCRC("bt.parallel_fired"),
        Dia::Core::StringCRC("bt.multi_state_divergence"),
    };
    outCount = 5;
    return names;
}

void BehaviourTreeTestStageModule::OnStart(Dia::Automation::AutomationService* service)
{
    (void)service;
}

void BehaviourTreeTestStageModule::OnUpdate(float /*deltaTime*/)
{
}

void BehaviourTreeTestStageModule::OnStop()
{
}

} // namespace CluicheTest

namespace { using BehaviourTreeTestStageModule_ = CluicheTest::BehaviourTreeTestStageModule; }
DIA_MODULE(BehaviourTreeTestStageModule_);
