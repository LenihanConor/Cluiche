////////////////////////////////////////////////////////////////////////////////
// Filename: AutomationModule.cpp
////////////////////////////////////////////////////////////////////////////////
#include "Modules/AutomationModule.h"
#include <DiaAutomation/AutomationService.h>
#include <DiaApplicationFlow/Application.h>
#include <DiaCore/Core/Assert.h>
#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaObservation/Log/DiaLog.h>

namespace Cluiche { namespace AppFlow {

const Dia::Core::StringCRC AutomationModule::kTypeId("AutomationModule");

AutomationModule::AutomationModule(const Dia::Core::StringCRC& instanceId)
    : Module(instanceId)
{
}

AutomationModule::~AutomationModule() = default;

Dia::ApplicationFlow::StartResult AutomationModule::DoStart()
{
    auto* app = dynamic_cast<Dia::ApplicationFlow::Application*>(GetApplication());
    DIA_ASSERT(app != nullptr, "AutomationModule: GetApplication() is not a Dia::ApplicationFlow::Application");

    mService = Dia::Core::UniquePtr<Dia::Automation::AutomationService>(
        new Dia::Automation::AutomationService(*app));

    mService->RegisterCommands();
    mService->EnableNavigationHold();

    DIA_LOG_INFO("Automation", "AutomationModule started — hold active, commands registered");
    return Dia::ApplicationFlow::StartResult::kReady;
}

void AutomationModule::DoUpdate(float dt)
{
    if (mService)
        mService->TickHeartbeat(dt);
}

Dia::ApplicationFlow::StopResult AutomationModule::DoStop()
{
    mService.Reset();
    DIA_LOG_INFO("Automation", "AutomationModule stopped");
    return Dia::ApplicationFlow::StopResult::kDone;
}

} } // namespace Cluiche::AppFlow

namespace { using AutomationModule_ = Cluiche::AppFlow::AutomationModule; }
DIA_MODULE(AutomationModule_);
