#include "Modules/MainStateProducerModule.h"
#include "Modules/AutomationModule.h"

#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaAutomation/AutomationService.h>
#include <DiaCore/Time/TimeAbsolute.h>
#include <DiaObservation/Log/DiaLog.h>

namespace Cluiche { namespace AppFlow {

const Dia::Core::StringCRC MainStateProducerModule::kTypeId("MainStateProducerModule");

MainStateProducerModule::MainStateProducerModule(const Dia::Core::StringCRC& instanceId)
    : Module(instanceId)
{}

Dia::ApplicationFlow::StartResult MainStateProducerModule::DoStart()
{
    DIA_LOG_INFO("Application", "MainStateProducerModule started");
    return Dia::ApplicationFlow::StartResult::kReady;
}

void MainStateProducerModule::DoUpdate(float /*dt*/)
{
    MainToRenderFrame frame;

    if (auto* am = AutomationModule::GetStatic())
    {
        if (auto* svc = am->GetService())
            frame.automationStatus.heartbeatEnabled = svc->IsHeartbeatEnabled();
    }

    DoPopulateFrame(frame);

    mLastFrame = frame;
    mFrameOutput.Write(mLastFrame, Dia::Core::TimeAbsolute::Zero());
}

Dia::ApplicationFlow::StopResult MainStateProducerModule::DoStop()
{
    mLastFrame = MainToRenderFrame{};
    mFrameOutput.Write(mLastFrame, Dia::Core::TimeAbsolute::Zero());
    DIA_LOG_INFO("Application", "MainStateProducerModule stopped");
    return Dia::ApplicationFlow::StopResult::kDone;
}

void MainStateProducerModule::OnConnectStreams(Dia::ApplicationFlow::Application& app)
{
    mFrameOutput.Connect(app);
}

} } // namespace Cluiche::AppFlow

namespace { using MainStateProducerModule_ = Cluiche::AppFlow::MainStateProducerModule; }
DIA_MODULE(MainStateProducerModule_);
