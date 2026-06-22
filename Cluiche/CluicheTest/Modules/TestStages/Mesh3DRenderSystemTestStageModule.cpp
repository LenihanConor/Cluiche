#include "Modules/TestStages/Mesh3DRenderSystemTestStageModule.h"

#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaAutomation/AutomationService.h>
#include <DiaObservation/Log/DiaLog.h>

namespace CluicheTest {

const Dia::Core::StringCRC Mesh3DRenderSystemTestStageModule::kTypeId("Mesh3DRenderSystemTestStageModule");

Mesh3DRenderSystemTestStageModule::Mesh3DRenderSystemTestStageModule(const Dia::Core::StringCRC& instanceId)
    : TestStageModuleBase(instanceId)
{}

Dia::Core::StringCRC Mesh3DRenderSystemTestStageModule::GetStageName() const
{
    return Dia::Core::StringCRC("Mesh3DRenderSystemTestStage");
}

const Dia::Core::StringCRC* Mesh3DRenderSystemTestStageModule::GetCheckpointNames(unsigned int& outCount) const
{
    static const Dia::Core::StringCRC names[] = {
        Dia::Core::StringCRC("test.mesh3drendersystem.passed")
    };
    outCount = 1;
    return names;
}

void Mesh3DRenderSystemTestStageModule::OnStart(Dia::Automation::AutomationService* service)
{
    // TODO: set up scene

    service->RegisterCheckpoint(this, Dia::Core::StringCRC("test.mesh3drendersystem.passed"),
        [this]() -> Dia::Automation::CheckpointResult {
            return { false, "pending", 0.0f };
        });
}

void Mesh3DRenderSystemTestStageModule::OnUpdate(float /*deltaTime*/)
{
    // TODO: check pass condition, call ReportPassed() when satisfied
}

} // namespace CluicheTest

namespace { using Mesh3DRenderSystemTestStageModule_ = CluicheTest::Mesh3DRenderSystemTestStageModule; }
DIA_MODULE(Mesh3DRenderSystemTestStageModule_);
