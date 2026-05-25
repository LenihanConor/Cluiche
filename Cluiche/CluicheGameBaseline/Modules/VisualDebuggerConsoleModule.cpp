#include "Modules/VisualDebuggerConsoleModule.h"

#ifdef DIA_DEBUG

#include <DiaApplicationFlow/ProcessingUnit.h>
#include <DiaApplicationFlow/RegistrationMacrosV2.h>
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
    if (!mVisualDebugger.Get())
        return Dia::ApplicationFlow::StartResult::kLoading;
    if (!mDebugUI.Get())
        return Dia::ApplicationFlow::StartResult::kLoading;

    DIA_LOG_INFO("Debug", "VisualDebuggerConsoleModule started");
    return Dia::ApplicationFlow::StartResult::kReady;
}

void VisualDebuggerConsoleModule::DoUpdate(float /*dt*/)
{
    DIA_TRACE_ZONE("VisualDebuggerConsoleModule.Update", Dia::Observation::Trace::Category::kDiaApplicationFlow);

    if (!mDebugUI.Get() || !mDebugUI.Get()->IsFrameActive())
        return;

    auto* vdbg = mVisualDebugger.Get();
    if (!vdbg)
        return;

    // Use a default DebugFrameData for stats — primitives are tracked by the
    // render path, not accessible here without coupling to RenderModule.
    static Dia::Graphics::DebugFrameData sEmptyFrameData;
    mConsole.Render(vdbg->GetLayerManager(), sEmptyFrameData);
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
