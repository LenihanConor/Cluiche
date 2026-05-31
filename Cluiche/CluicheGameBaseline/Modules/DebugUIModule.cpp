#include "Modules/DebugUIModule.h"
#include "Modules/KernelModule.h"

#include <DiaImGui/DiaImGuiManager.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Health/HealthRegistry.h>
#include <DiaObservation/Trace/DiaTrace.h>
#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaApplicationFlow/ProcessingUnit.h>

namespace Cluiche { namespace AppFlow {

const Dia::Core::StringCRC DebugUIModule::kTypeId("DebugUIModule");

DebugUIModule::DebugUIModule(const Dia::Core::StringCRC& instanceId)
    : Module(instanceId)
{}

Dia::ApplicationFlow::StartResult DebugUIModule::DoStart()
{
    DIA_TRACE_ZONE("DebugUIModule.Start", Dia::Observation::Trace::Category::kDiaApplicationFlow);

    if (Dia::ImGui::GetManager().GetBackend() == nullptr)
        return Dia::ApplicationFlow::StartResult::kLoading;

    if (!KernelModule::IsRenderContextActive())
        return Dia::ApplicationFlow::StartResult::kLoading;

    DIA_LOG_INFO("Application", "DebugUIModule DoStart entry");

    Dia::Observation::Health::HealthRegistry::Instance().Register(&mHealth);
    mHealth.SetOK();

    mFrameActive = false;

    DIA_LOG_INFO("Application", "DebugUIModule DoStart exit");
    return Dia::ApplicationFlow::StartResult::kReady;
}

void DebugUIModule::DoUpdate(float dt)
{
    Dia::ImGui::NewFrame(dt);
    mFrameActive = true;
}

Dia::ApplicationFlow::StopResult DebugUIModule::DoStop()
{
    DIA_LOG_INFO("Application", "DebugUIModule DoStop entry");
    Dia::Observation::Health::HealthRegistry::Instance().Unregister(&mHealth);
    mFrameActive = false;
    DIA_LOG_INFO("Application", "DebugUIModule DoStop exit");
    return Dia::ApplicationFlow::StopResult::kDone;
}

} } // namespace Cluiche::AppFlow

namespace { using DebugUIModule_ = Cluiche::AppFlow::DebugUIModule; }
DIA_MODULE(DebugUIModule_);
