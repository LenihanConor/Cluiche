#include "Modules/SimNavigationHandlerModule.h"

#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaAutomation/AutomationService.h>
#include <DiaStreams/ServiceStreamReader.h>
#include <DiaObservation/Log/DiaLog.h>

// Full instantiation lives here — header only forward-declares.
template class Dia::ApplicationFlow::ServiceStreamReader<Dia::Automation::AutomationService>;

namespace Cluiche { namespace AppFlow {

const Dia::Core::StringCRC SimNavigationHandlerModule::kTypeId("SimNavigationHandlerModule");

SimNavigationHandlerModule::SimNavigationHandlerModule(const Dia::Core::StringCRC& instanceId)
    : Module(instanceId)
    , mAutomationService(new Dia::ApplicationFlow::ServiceStreamReader<Dia::Automation::AutomationService>(this, Dia::Core::StringCRC("AutomationService")))
{}

SimNavigationHandlerModule::~SimNavigationHandlerModule()
{
    delete mAutomationService;
}

Dia::ApplicationFlow::StartResult SimNavigationHandlerModule::DoStart()
{
    return Dia::ApplicationFlow::StartResult::kReady;
}

void SimNavigationHandlerModule::DoUpdate(float /*dt*/)
{
    static constexpr unsigned int kMaxPerFrame = 4;
    Dia::Core::Containers::DynamicArrayC<Dia::ApplicationFlow::Event<RenderToSimNavRequest>, kMaxPerFrame> events;

    mBootNavInput.Consume(events);
    for (unsigned int i = 0; i < events.Size(); ++i)
    {
        const Dia::Core::StringCRC& target = events[i].payload.target;
        if (target.Value() != 0)
        {
            DIA_LOG_INFO("Application", "SimNavigationHandlerModule: navigate_to('%s') from BootMenu", target.AsChar());
            Navigate(target);
            return;
        }
    }

    events.RemoveAll();
    mHUDNavInput.Consume(events);
    for (unsigned int i = 0; i < events.Size(); ++i)
    {
        const Dia::Core::StringCRC& target = events[i].payload.target;
        if (target.Value() != 0)
        {
            DIA_LOG_INFO("Application", "SimNavigationHandlerModule: navigate_to('%s') from HUD", target.AsChar());
            Navigate(target);
            return;
        }
    }
}

Dia::ApplicationFlow::StopResult SimNavigationHandlerModule::DoStop()
{
    return Dia::ApplicationFlow::StopResult::kDone;
}

void SimNavigationHandlerModule::Navigate(const Dia::Core::StringCRC& target)
{
    if (mAutomationService->IsAvailable() && mAutomationService->Get().IsHolding())
    {
        mAutomationService->Get().ReleaseNavigationHold(target);
    }
    else
    {
        TransitionTo(target);
    }
}

void SimNavigationHandlerModule::OnConnectStreams(Dia::ApplicationFlow::Application& app)
{
    mBootNavInput.Connect(app);
    mHUDNavInput.Connect(app);
    mAutomationService->Connect(app);
}

} } // namespace Cluiche::AppFlow

namespace { using SimNavigationHandlerModule_ = Cluiche::AppFlow::SimNavigationHandlerModule; }
DIA_MODULE(SimNavigationHandlerModule_);
DIA_DESCRIBE(SimNavigationHandlerModule_::kTypeId, "Executes navigation requests sent by RenderPU modules on the sim thread.");
