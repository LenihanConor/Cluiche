#include "Modules/FrontEndModule.h"

#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/ProcessingUnit.h>
#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaInput/EKey.h>
#include <DiaObservation/Log/DiaLog.h>

namespace CoW {

const Dia::Core::StringCRC FrontEndModule::kTypeId("FrontEndModule");

FrontEndModule::FrontEndModule(const Dia::Core::StringCRC& instanceId)
    : SimModule(instanceId)
{}

Dia::ApplicationFlow::StartResult FrontEndModule::DoStart()
{
    using Dia::ApplicationFlow::StartResult;
    using LoadState = Cluiche::AppFlow::AssetLoadStatus::State;

    if (!mWaitLogged)
    {
        DIA_LOG_INFO("CoW", "FrontEndModule: waiting for stage assets");
        mWaitLogged = true;
    }

    if (!mAssetRuntime.IsAvailable() || !mAssetLoadStatus.IsAvailable())
        return StartResult::kLoading;

    const Cluiche::AppFlow::AssetLoadStatus& status = mAssetLoadStatus.Get();
    if (status.stageId != Dia::Core::StringCRC("FrontEnd"))
        return StartResult::kLoading;

    if (status.state == LoadState::kFailed)
    {
        DIA_LOG_ERROR("CoW", "FrontEndModule: stage asset load failed");
        return StartResult::kFailed;
    }
    if (status.state != LoadState::kComplete)
        return StartResult::kLoading;

    mExitRequested = false;
    DIA_LOG_INFO("CoW", "FrontEndModule: started (FrontEnd stage running, %u assets) - press Escape to return to Boot",
                 status.loaded);
    return StartResult::kReady;
}

void FrontEndModule::DoUpdate(const Dia::SimTime::SimTimeContext& /*ctx*/)
{
    // Placeholder: idle. Game logic for the FrontEnd stage goes here.

    if (mExitRequested)
        return;

    if (Cluiche::AppFlow::InputStreamModule* input = mInput.Get())
    {
        if (input->WasKeyPressed(Dia::Input::EKey::Escape))
        {
            DIA_LOG_INFO("CoW", "FrontEndModule: Escape pressed - returning to Boot");
            mExitRequested = true;
            NavigateTo(Dia::Core::StringCRC("Boot"));
        }
    }
}

Dia::ApplicationFlow::StopResult FrontEndModule::DoStop()
{
    DIA_LOG_INFO("CoW", "FrontEndModule: stopped");
    mWaitLogged = false;
    return Dia::ApplicationFlow::StopResult::kDone;
}

void FrontEndModule::NavigateTo(const Dia::Core::StringCRC& target)
{
    // Same rule as SimNavigationHandlerModule: while AutomationModule holds
    // navigation, a plain TransitionTo() would wait on the guard forever.
    if (mAutomation.IsAvailable() && mAutomation.Get().IsHolding())
        mAutomation.Get().ReleaseNavigationHold(target);
    else
        TransitionTo(target);
}

void FrontEndModule::OnConnectStreams(Dia::ApplicationFlow::Application& app)
{
    mAssetLoadStatus.Connect(app);
    mAssetRuntime.Connect(app);
    mAutomation.Connect(app);
}

} // namespace CoW

namespace { using FrontEndModule_ = CoW::FrontEndModule; }
DIA_MODULE(FrontEndModule_);
DIA_DESCRIBE(FrontEndModule_::kTypeId, "Placeholder logic for the FrontEnd stage: logs on start, idles, Escape returns to Boot.");
