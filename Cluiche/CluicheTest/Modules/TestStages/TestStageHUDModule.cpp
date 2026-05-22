#include "Modules/TestStages/TestStageHUDModule.h"

#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaAutomation/AutomationService.h>

namespace CluicheTest {

const Dia::Core::StringCRC TestStageHUDModule::kTypeId("TestStageHUDModule");

TestStageHUDModule::TestStageHUDModule(const Dia::Core::StringCRC& instanceId)
    : Module(instanceId)
{}

Dia::ApplicationFlow::StartResult TestStageHUDModule::DoStart()
{
    auto* automationModule = mAutomation.Get();
    if (!automationModule || !automationModule->GetService())
        return Dia::ApplicationFlow::StartResult::kLoading;

    return Dia::ApplicationFlow::StartResult::kReady;
}

void TestStageHUDModule::DoUpdate(float)
{
    // ImGui rendering disabled: HUD module runs on MainPU but ImGui frame
    // lifecycle (NewFrame/Render) is managed on RenderPU. Cross-thread ImGui
    // calls are unsafe. Phase 2 will add a render-thread submission path.
}

Dia::ApplicationFlow::StopResult TestStageHUDModule::DoStop()
{
    return Dia::ApplicationFlow::StopResult::kDone;
}

} // namespace CluicheTest

namespace { using TestStageHUDModule_ = CluicheTest::TestStageHUDModule; }
DIA_MODULE(TestStageHUDModule_);
