#include "Modules/VisualDebuggerConsoleModule.h"

#ifdef DIA_DEBUG

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
    if (!VisualDebuggerModule::GetStaticLayerManager())
        return Dia::ApplicationFlow::StartResult::kLoading;
    if (!mDebugUI.Get())
        return Dia::ApplicationFlow::StartResult::kLoading;

    mConsole.Toggle();

    DIA_LOG_INFO("Debug", "VisualDebuggerConsoleModule started");
    return Dia::ApplicationFlow::StartResult::kReady;
}

void VisualDebuggerConsoleModule::DoUpdate(float /*dt*/)
{
    DIA_TRACE_ZONE("VisualDebuggerConsoleModule.Update", Dia::Observation::Trace::Category::kDiaApplicationFlow);

    if (!mDebugUI.Get() || !mDebugUI.Get()->IsFrameActive())
        return;

    auto* mgr = VisualDebuggerModule::GetStaticLayerManager();
    if (!mgr)
        return;

    Dia::Core::StringCRC currentStage;
    if (auto* app = GetApplication())
        currentStage = app->GetCurrentStage();

    static Dia::Graphics::DebugFrameData sEmptyFrameData;
    mConsole.Render(*mgr, sEmptyFrameData, currentStage);
}

Dia::ApplicationFlow::StopResult VisualDebuggerConsoleModule::DoStop()
{
    DIA_LOG_INFO("Debug", "VisualDebuggerConsoleModule stopped");
    return Dia::ApplicationFlow::StopResult::kDone;
}

} } // namespace Cluiche::AppFlow

namespace { using VisualDebuggerConsoleModule_ = Cluiche::AppFlow::VisualDebuggerConsoleModule; }
DIA_MODULE(VisualDebuggerConsoleModule_);

#endif // DIA_DEBUG
