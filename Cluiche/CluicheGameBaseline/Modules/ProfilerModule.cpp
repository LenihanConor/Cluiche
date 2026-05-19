#include "Modules/ProfilerModule.h"

#include <DiaObservation/Profile/Profiler.h>
#include <DiaApplicationFlow/RegistrationMacrosV2.h>

namespace Cluiche { namespace AppFlow {

ProfilerModule::ProfilerModule(const Dia::Core::StringCRC& instanceId)
    : Module(instanceId)
{
}

Dia::ApplicationFlow::StartResult ProfilerModule::DoStart()
{
    // Profiler is started by SessionManager; nothing to do here.
    return Dia::ApplicationFlow::StartResult::kReady;
}

void ProfilerModule::DoUpdate(float /*dt*/)
{
    Dia::Observation::Profile::Profiler::Instance().BeginFrame();
    Dia::Observation::Profile::Profiler::Instance().EndFrame();
}

Dia::ApplicationFlow::StopResult ProfilerModule::DoStop()
{
    // Profiler is stopped by SessionManager; nothing to do here.
    return Dia::ApplicationFlow::StopResult::kDone;
}

const Dia::Core::StringCRC ProfilerModule::kTypeId("ProfilerModule");

} } // namespace Cluiche::AppFlow

namespace { using ProfilerModule_ = Cluiche::AppFlow::ProfilerModule; }
DIA_MODULE(ProfilerModule_);
