#include "Modules/VisualDebuggerModule.h"

#ifdef DIA_DEBUG

#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaCore/Time/TimeAbsolute.h>
#include <DiaObservation/Trace/DiaTrace.h>

namespace Cluiche { namespace AppFlow {

Dia::Debug::DebugLayerManager* VisualDebuggerModule::sLayerManager = nullptr;

const Dia::Core::StringCRC VisualDebuggerModule::kTypeId("VisualDebuggerModule");

VisualDebuggerModule::VisualDebuggerModule(const Dia::Core::StringCRC& instanceId)
    : Module(instanceId)
{}

Dia::ApplicationFlow::StartResult VisualDebuggerModule::DoStart()
{
    sLayerManager = &mLayerManager;
    mLayerManager.SetDebugScale(50.0f);
    mLayerManager.RegisterDiaAPICommands();
    return Dia::ApplicationFlow::StartResult::kReady;
}

void VisualDebuggerModule::DoUpdate(float /*dt*/)
{
    DIA_TRACE_ZONE("VisualDebuggerModule.Update", Dia::Observation::Trace::Category::kDiaApplicationFlow);

    auto* app = GetApplication();
    if (app)
    {
        Dia::Core::StringCRC currentStage = app->GetCurrentStage();
        if (currentStage != mLastKnownStage)
        {
            if (!(mLastKnownStage == Dia::Core::StringCRC()))
                mLayerManager.SetStageActive(mLastKnownStage, false);
            mLayerManager.SetStageActive(currentStage, true);
            mLastKnownStage = currentStage;
        }
    }

    mFrame.Clear();
    mLayerManager.Draw(mFrame);
    mRenderOutput.Write(mFrame, Dia::Core::TimeAbsolute::Zero());
}

Dia::ApplicationFlow::StopResult VisualDebuggerModule::DoStop()
{
    mLastKnownStage = Dia::Core::StringCRC();
    mFrame.Clear();
    mRenderOutput.Write(mFrame, Dia::Core::TimeAbsolute::Zero());
    return Dia::ApplicationFlow::StopResult::kDone;
}

void VisualDebuggerModule::OnConnectStreams(Dia::ApplicationFlow::Application& app)
{
    mRenderOutput.Connect(app);
}

} } // namespace Cluiche::AppFlow

namespace { using VisualDebuggerModule_ = Cluiche::AppFlow::VisualDebuggerModule; }
DIA_MODULE(VisualDebuggerModule_);

#endif // DIA_DEBUG
