////////////////////////////////////////////////////////////////////////////////
// Filename: AutomationModule.cpp
////////////////////////////////////////////////////////////////////////////////
#include "Modules/AutomationModule.h"
#include <DiaAutomation/AutomationService.h>
#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/Streams/ServiceStreamWriter.h>
#include <DiaCore/Core/Assert.h>
#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaObservation/Log/DiaLog.h>

// Full instantiation lives here — header only forward-declares to avoid
// pulling Application.h into every TU that includes AutomationModule.h.
template class Dia::ApplicationFlow::ServiceStreamWriter<Dia::Automation::AutomationService>;

namespace Cluiche { namespace AppFlow {

const Dia::Core::StringCRC AutomationModule::kTypeId("AutomationModule");

AutomationModule::AutomationModule(const Dia::Core::StringCRC& instanceId)
    : Module(instanceId)
    , mAutomationServiceStream(new Dia::ApplicationFlow::ServiceStreamWriter<Dia::Automation::AutomationService>(this, Dia::Core::StringCRC("AutomationService")))
{
}

AutomationModule::~AutomationModule()
{
    delete mAutomationServiceStream;
}

Dia::ApplicationFlow::StartResult AutomationModule::DoStart()
{
    auto* app = dynamic_cast<Dia::ApplicationFlow::Application*>(GetApplication());
    DIA_ASSERT(app != nullptr, "AutomationModule: GetApplication() is not a Dia::ApplicationFlow::Application");

    mService = Dia::Core::UniquePtr<Dia::Automation::AutomationService>(
        new Dia::Automation::AutomationService(*app));

    mService->RegisterCommands();
    mService->EnableNavigationHold();

    mAutomationServiceStream->Register(*mService);

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
    mAutomationServiceStream->Deregister();
    mService.Reset();
    DIA_LOG_INFO("Automation", "AutomationModule stopped");
    return Dia::ApplicationFlow::StopResult::kDone;
}

void AutomationModule::OnConnectStreams(Dia::ApplicationFlow::Application& app)
{
    mAutomationServiceStream->Connect(app);
}

} } // namespace Cluiche::AppFlow

namespace { using AutomationModule_ = Cluiche::AppFlow::AutomationModule; }
DIA_MODULE(AutomationModule_);
DIA_DESCRIBE(AutomationModule_::kTypeId, "Drives automated test sequences and script-based input replay.");
