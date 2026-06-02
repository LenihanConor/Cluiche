#include "Modules/VisualDebuggerConsoleModule.h"

#ifdef DIA_DEBUG

#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/ProcessingUnit.h>
#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaApplicationFlow/IApplicationControl.h>
#include <DiaGraphics/Frame/DebugFrameData.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Trace/DiaTrace.h>

namespace Cluiche { namespace AppFlow {

const Dia::Core::StringCRC VisualDebuggerConsoleModule::kTypeId("VisualDebuggerConsoleModule");

VisualDebuggerConsoleModule::VisualDebuggerConsoleModule(const Dia::Core::StringCRC& instanceId)
    : Module(instanceId)
{}

Dia::ApplicationFlow::StartResult VisualDebuggerConsoleModule::DoStart()
{
    if (!mLayerManagerStream.IsAvailable())
        return Dia::ApplicationFlow::StartResult::kLoading;
    if (!mDebugUI.Get())
        return Dia::ApplicationFlow::StartResult::kLoading;

    if (!mConsole.IsVisible())
        mConsole.Toggle();

    DIA_LOG_INFO("Debug", "VisualDebuggerConsoleModule started");
    return Dia::ApplicationFlow::StartResult::kReady;
}

void VisualDebuggerConsoleModule::DoUpdate(float /*dt*/)
{
    DIA_TRACE_ZONE("VisualDebuggerConsoleModule.Update", Dia::Observation::Trace::Category::kDiaApplicationFlow);

    if (!mDebugUI.Get() || !mDebugUI.Get()->IsFrameActive())
        return;

    if (!mLayerManagerStream.IsAvailable())
        return;

    Dia::Core::StringCRC currentStage;
    if (auto* app = GetApplication())
        currentStage = app->GetCurrentStage();

    static Dia::Graphics::DebugFrameData sEmptyFrameData;
    mConsole.Render(mLayerManagerStream.Get(), sEmptyFrameData, currentStage);
}

Dia::ApplicationFlow::StopResult VisualDebuggerConsoleModule::DoStop()
{
    DIA_LOG_INFO("Debug", "VisualDebuggerConsoleModule stopped");
    return Dia::ApplicationFlow::StopResult::kDone;
}

void VisualDebuggerConsoleModule::OnConnectStreams(Dia::ApplicationFlow::Application& app)
{
    mLayerManagerStream.Connect(app);
}

} } // namespace Cluiche::AppFlow

namespace { using VisualDebuggerConsoleModule_ = Cluiche::AppFlow::VisualDebuggerConsoleModule; }
DIA_MODULE(VisualDebuggerConsoleModule_);
DIA_DESCRIBE(VisualDebuggerConsoleModule_::kTypeId, "Renders the in-game visual debugger console and command interface.");

#endif // DIA_DEBUG
