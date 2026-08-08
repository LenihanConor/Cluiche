#include "Modules/TestStages/ArenaTestStageModule.h"
#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaAutomation/AutomationService.h>
#include <DiaObservation/Log/DiaLog.h>

namespace CluicheTest {

const Dia::Core::StringCRC ArenaTestStageModule::kTypeId("ArenaTestStageModule");

ArenaTestStageModule::ArenaTestStageModule(const Dia::Core::StringCRC& instanceId)
    : TestStageModuleBase(instanceId)
{}

ArenaTestStageModule::~ArenaTestStageModule() = default;

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

void ArenaTestStageModule::OnStart(Dia::Automation::AutomationService* service)
{
    // TODO: implement in Task 4
    service->RegisterCheckpoint(this, Dia::Core::StringCRC("arena.wave1_complete"),
        [this]() -> Dia::Automation::CheckpointResult { return { false, "not yet implemented", 0.f }; });
    service->RegisterCheckpoint(this, Dia::Core::StringCRC("arena.wave2_complete"),
        [this]() -> Dia::Automation::CheckpointResult { return { false, "not yet implemented", 0.f }; });
    service->RegisterCheckpoint(this, Dia::Core::StringCRC("arena.wave3_complete"),
        [this]() -> Dia::Automation::CheckpointResult { return { false, "not yet implemented", 0.f }; });
    service->RegisterCheckpoint(this, Dia::Core::StringCRC("arena.victory"),
        [this]() -> Dia::Automation::CheckpointResult { return { false, "not yet implemented", 0.f }; });
    service->RegisterCheckpoint(this, Dia::Core::StringCRC("arena.powerup_collected"),
        [this]() -> Dia::Automation::CheckpointResult { return { false, "not yet implemented", 0.f }; });
    service->RegisterCheckpoint(this, Dia::Core::StringCRC("arena.desperate_charge_triggered"),
        [this]() -> Dia::Automation::CheckpointResult { return { false, "not yet implemented", 0.f }; });
    DIA_LOG_INFO("CluicheTest", "ArenaTestStageModule::OnStart (stub)");
}

void ArenaTestStageModule::OnUpdate(float /*deltaTime*/)
{
    // TODO: implement in Tasks 5-8
}

void ArenaTestStageModule::OnStop()
{
    DIA_LOG_INFO("CluicheTest", "ArenaTestStageModule::OnStop (stub)");
}

} // namespace CluicheTest

namespace { using ArenaTestStageModule_ = CluicheTest::ArenaTestStageModule; }
DIA_MODULE(ArenaTestStageModule_);
DIA_DESCRIBE(ArenaTestStageModule_::kTypeId, "Multi-system AI/progression E2E: TriggerScript + Objective + Blackboard + StateMachine + UtilityAI + Rules");
