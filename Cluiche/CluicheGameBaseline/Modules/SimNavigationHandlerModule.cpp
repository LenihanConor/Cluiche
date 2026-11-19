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
    : SimModule(instanceId)
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

void SimNavigationHandlerModule::DoUpdate(const Dia::SimTime::SimTimeContext& /*ctx*/)
{
    static constexpr unsigned int kMaxPerFrame = 4;
    Dia::Core::Containers::DynamicArrayC<Dia::ApplicationFlow::Event<RenderToSimNavRequest>, kMaxPerFrame> events;

    mBootNavInput.Consume(events);
    for (unsigned int i = 0; i < events.Size(); ++i)
    {
        const Dia::Core::StringCRC& target = events[i].payload.target;
        const Dia::Core::StringCRC& preset = events[i].payload.preset;
        if (target.Value() != 0)
        {
            DIA_LOG_INFO("Application", "SimNavigationHandlerModule: navigate_to('%s') from BootMenu", target.AsChar());
            Navigate(target, preset);
            return;
        }
    }

    {
        Dia::Core::Containers::DynamicArrayC<Dia::ApplicationFlow::Event<StageLaunchRequest>, kMaxPerFrame> launches;
        mLaunchInput.Consume(launches);
        for (unsigned int i = 0; i < launches.Size(); ++i)
        {
            const StageLaunchRequest& request = launches[i].payload;
            if (request.stage.Value() == 0) { continue; }
            Dia::ApplicationFlow::IApplicationControl* app = GetApplication();
            if (!app) { return; }
            DIA_LOG_INFO("Application", "SimNavigationHandlerModule: launch '%s' from stages.launch", request.stage.AsChar());
            HandleLaunchRequest(*app, mAutomationService->IsAvailable() ? &mAutomationService->Get() : nullptr, request);
            return;
        }
    }

    events.RemoveAll();
    mHUDNavInput.Consume(events);
    for (unsigned int i = 0; i < events.Size(); ++i)
    {
        const Dia::Core::StringCRC& target = events[i].payload.target;
        const Dia::Core::StringCRC& preset = events[i].payload.preset;
        if (target.Value() != 0)
        {
            DIA_LOG_INFO("Application", "SimNavigationHandlerModule: navigate_to('%s') from HUD", target.AsChar());
            Navigate(target, preset);
            return;
        }
    }
}

Dia::ApplicationFlow::StopResult SimNavigationHandlerModule::DoStop()
{
    return Dia::ApplicationFlow::StopResult::kDone;
}

void SimNavigationHandlerModule::RouteNavigation(Dia::ApplicationFlow::IApplicationControl& app,
                                                 Dia::Automation::AutomationService* automation,
                                                 const Dia::Core::StringCRC& target,
                                                 const Dia::Core::StringCRC& preset)
{
    if (preset.Value() != 0)
        DIA_LOG_INFO("Application", "preset launch requested (source=nav) stage='%s' preset='%s'", target.AsChar(), preset.AsChar());

    if (automation && automation->IsHolding())
    {
        automation->ReleaseNavigationHold(target, nullptr, nullptr, preset);
    }
    else if (preset.Value() != 0)
    {
        app.TransitionTo(target, preset);
    }
    else
    {
        app.TransitionTo(target);
    }
}

void SimNavigationHandlerModule::HandleLaunchRequest(Dia::ApplicationFlow::IApplicationControl& app,
                                                    Dia::Automation::AutomationService* automation,
                                                    const StageLaunchRequest& request)
{
    if (request.stage.Value() == 0)
        return;
    RouteNavigation(app, automation, request.stage, request.preset);
}

void SimNavigationHandlerModule::Navigate(const Dia::Core::StringCRC& target, const Dia::Core::StringCRC& preset)
{
    Dia::ApplicationFlow::IApplicationControl* app = GetApplication();
    if (!app)
        return;
    RouteNavigation(*app, mAutomationService->IsAvailable() ? &mAutomationService->Get() : nullptr, target, preset);
}

void SimNavigationHandlerModule::OnConnectStreams(Dia::ApplicationFlow::Application& app)
{
    mBootNavInput.Connect(app);
    mHUDNavInput.Connect(app);
    mLaunchInput.Connect(app);
    mAutomationService->Connect(app);
}

} } // namespace Cluiche::AppFlow

namespace { using SimNavigationHandlerModule_ = Cluiche::AppFlow::SimNavigationHandlerModule; }
DIA_MODULE(SimNavigationHandlerModule_);
DIA_DESCRIBE(SimNavigationHandlerModule_::kTypeId, "Executes navigation requests sent by RenderPU modules on the sim thread.");
