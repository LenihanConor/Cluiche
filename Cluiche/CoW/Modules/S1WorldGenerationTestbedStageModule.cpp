#include "Modules/S1WorldGenerationTestbedStageModule.h"

#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/ProcessingUnit.h>
#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaInput/EKey.h>
#include <DiaObservation/Log/DiaLog.h>

namespace CoW {

const Dia::Core::StringCRC S1WorldGenerationTestbedStageModule::kTypeId("S1WorldGenerationTestbedStageModule");

S1WorldGenerationTestbedStageModule::S1WorldGenerationTestbedStageModule(const Dia::Core::StringCRC& instanceId)
    : SimModule(instanceId)
{}

Dia::ApplicationFlow::StartResult S1WorldGenerationTestbedStageModule::DoStart()
{
    using Dia::ApplicationFlow::StartResult;
    using LoadState = Cluiche::AppFlow::AssetLoadStatus::State;

    if (!mWaitLogged)
    {
        DIA_LOG_INFO("CoW", "S1WorldGenerationTestbedStageModule: waiting for stage assets");
        mWaitLogged = true;
    }

    if (!mAssetRuntime.IsAvailable() || !mAssetLoadStatus.IsAvailable())
        return StartResult::kLoading;

    const Cluiche::AppFlow::AssetLoadStatus& status = mAssetLoadStatus.Get();
    if (status.stageId != Dia::Core::StringCRC("S1WorldGenerationTestbedStage"))
        return StartResult::kLoading;

    if (status.state == LoadState::kFailed)
    {
        DIA_LOG_ERROR("CoW", "S1WorldGenerationTestbedStageModule: stage asset load failed");
        return StartResult::kFailed;
    }
    if (status.state != LoadState::kComplete)
        return StartResult::kLoading;

    mExitRequested = false;
    DIA_LOG_INFO("CoW", "S1WorldGenerationTestbedStageModule: started (S1WorldGenerationTestbedStage stage running, %u assets) - press Escape to return to Boot",
                 status.loaded);
    return StartResult::kReady;
}

void S1WorldGenerationTestbedStageModule::DoUpdate(const Dia::SimTime::SimTimeContext& /*ctx*/)
{
    // Placeholder: idle. Game logic for the S1WorldGenerationTestbedStage stage goes here.

    if (mExitRequested)
        return;

    if (Cluiche::AppFlow::InputStreamModule* input = mInput.Get())
    {
        if (input->WasKeyPressed(Dia::Input::EKey::Escape))
        {
            DIA_LOG_INFO("CoW", "S1WorldGenerationTestbedStageModule: Escape pressed - returning to Boot");
            mExitRequested = true;
            NavigateTo(Dia::Core::StringCRC("Boot"));
        }
    }
}

Dia::ApplicationFlow::StopResult S1WorldGenerationTestbedStageModule::DoStop()
{
    DIA_LOG_INFO("CoW", "S1WorldGenerationTestbedStageModule: stopped");
    mWaitLogged = false;
    return Dia::ApplicationFlow::StopResult::kDone;
}

void S1WorldGenerationTestbedStageModule::NavigateTo(const Dia::Core::StringCRC& target)
{
    // Same rule as SimNavigationHandlerModule: while AutomationModule holds
    // navigation, a plain TransitionTo() would wait on the guard forever.
    if (mAutomation.IsAvailable() && mAutomation.Get().IsHolding())
        mAutomation.Get().ReleaseNavigationHold(target);
    else
        TransitionTo(target);
}

void S1WorldGenerationTestbedStageModule::OnConnectStreams(Dia::ApplicationFlow::Application& app)
{
    mAssetLoadStatus.Connect(app);
    mAssetRuntime.Connect(app);
    mAutomation.Connect(app);
}

} // namespace CoW

namespace { using S1WorldGenerationTestbedStageModule_ = CoW::S1WorldGenerationTestbedStageModule; }
DIA_MODULE(S1WorldGenerationTestbedStageModule_);
DIA_DESCRIBE(S1WorldGenerationTestbedStageModule_::kTypeId, "Placeholder logic for the S1WorldGenerationTestbedStage stage: logs on start, idles, Escape returns to Boot.");
