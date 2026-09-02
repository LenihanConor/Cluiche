#include "Modules/MainStateProducerModule.h"

#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaAutomation/AutomationService.h>
#include <DiaCore/Time/TimeAbsolute.h>
#include <DiaObservation/Log/DiaLog.h>

namespace Cluiche { namespace AppFlow {

const Dia::Core::StringCRC MainStateProducerModule::kTypeId("MainStateProducerModule");

MainStateProducerModule::MainStateProducerModule(const Dia::Core::StringCRC& instanceId)
    : MainModule(instanceId)
{}

Dia::ApplicationFlow::StartResult MainStateProducerModule::DoStart()
{
    DIA_LOG_INFO("Application", "MainStateProducerModule started");
    return Dia::ApplicationFlow::StartResult::kReady;
}

void MainStateProducerModule::DoUpdate(const Dia::SimTime::MainTimeContext& /*ctx*/)
{
    MainToRenderFrame frame;

    if (auto* am = mAutomationRef.Get())
    {
        if (auto* svc = am->GetService())
            frame.automationStatus.heartbeatEnabled = svc->IsHeartbeatEnabled();
    }

    if (auto* assets = mAssetServiceRef.Get())
    {
        frame.assetLoadStatus.stageId = assets->GetCurrentAppFlowStage();
        const auto raw = assets->GetStageLoadState(frame.assetLoadStatus.stageId);
        using Raw = AssetServiceModule::StageLoadState;
        using Out = AssetLoadStatus::State;
        switch (raw)
        {
            case Raw::kLoading:  frame.assetLoadStatus.state = Out::kLoading;  break;
            case Raw::kComplete: frame.assetLoadStatus.state = Out::kComplete; break;
            case Raw::kFailed:   frame.assetLoadStatus.state = Out::kFailed;   break;
            default:             frame.assetLoadStatus.state = Out::kIdle;     break;
        }
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
DIA_DESCRIBE(MainStateProducerModule_::kTypeId, "Produces the main application state frame and drives top-level game logic.");
