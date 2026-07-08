#include "Modules/SimNavigationHandlerModule.h"

#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaObservation/Log/DiaLog.h>

namespace Cluiche { namespace AppFlow {

const Dia::Core::StringCRC SimNavigationHandlerModule::kTypeId("SimNavigationHandlerModule");

SimNavigationHandlerModule::SimNavigationHandlerModule(const Dia::Core::StringCRC& instanceId)
    : Module(instanceId)
{}

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
            TransitionTo(target);
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
            TransitionTo(target);
            return;
        }
    }
}

Dia::ApplicationFlow::StopResult SimNavigationHandlerModule::DoStop()
{
    return Dia::ApplicationFlow::StopResult::kDone;
}

void SimNavigationHandlerModule::OnConnectStreams(Dia::ApplicationFlow::Application& app)
{
    mBootNavInput.Connect(app);
    mHUDNavInput.Connect(app);
}

} } // namespace Cluiche::AppFlow

namespace { using SimNavigationHandlerModule_ = Cluiche::AppFlow::SimNavigationHandlerModule; }
DIA_MODULE(SimNavigationHandlerModule_);
DIA_DESCRIBE(SimNavigationHandlerModule_::kTypeId, "Executes navigation requests sent by RenderPU modules on the sim thread.");
