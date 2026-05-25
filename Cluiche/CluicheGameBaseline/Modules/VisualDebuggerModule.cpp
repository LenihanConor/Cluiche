#include "Modules/VisualDebuggerModule.h"

#ifdef DIA_DEBUG

#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaGraphics/Frame/FrameData.h>
#include <DiaObservation/Trace/DiaTrace.h>

namespace Cluiche { namespace AppFlow {

const Dia::Core::StringCRC VisualDebuggerModule::kTypeId("VisualDebuggerModule");

VisualDebuggerModule::VisualDebuggerModule(const Dia::Core::StringCRC& instanceId)
    : Module(instanceId)
{}

Dia::ApplicationFlow::StartResult VisualDebuggerModule::DoStart()
{
    mLayerManager.RegisterDiaAPICommands();
    return Dia::ApplicationFlow::StartResult::kReady;
}

void VisualDebuggerModule::DoUpdate(float /*dt*/)
{
    DIA_TRACE_ZONE("VisualDebuggerModule.Update", Dia::Observation::Trace::Category::kDiaApplicationFlow);
    // DebugLayerManager::Draw is called by the domain modules that own the
    // FrameData (e.g. RenderModule). This update is intentionally a no-op;
    // the module's purpose is to own mLayerManager's lifetime and expose it.
}

Dia::ApplicationFlow::StopResult VisualDebuggerModule::DoStop()
{
    return Dia::ApplicationFlow::StopResult::kDone;
}

} } // namespace Cluiche::AppFlow

namespace { using VisualDebuggerModule_ = Cluiche::AppFlow::VisualDebuggerModule; }
DIA_MODULE(VisualDebuggerModule_);

#endif // DIA_DEBUG
