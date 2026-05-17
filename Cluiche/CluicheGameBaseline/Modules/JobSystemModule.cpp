#include "Modules/JobSystemModule.h"

#include <DiaLogger/DiaLog.h>
#include <DiaCore/Threading/JobSystem.h>

namespace Cluiche { namespace AppFlow {

const Dia::Core::StringCRC JobSystemModule::kTypeId("JobSystemModule");

JobSystemModule::JobSystemModule(const Dia::Core::StringCRC& instanceId)
    : Module(instanceId)
{
}

Dia::ApplicationFlow::StartResult JobSystemModule::DoStart()
{
    DIA_LOG_INFO("Application", "JobSystemModule DoStart entry");
    Dia::Core::JobSystem::Initialize(0);
    DIA_LOG_INFO("Application", "JobSystemModule DoStart ready");
    return Dia::ApplicationFlow::StartResult::kReady;
}

void JobSystemModule::DoUpdate(float /*dt*/)
{
}

Dia::ApplicationFlow::StopResult JobSystemModule::DoStop()
{
    DIA_LOG_INFO("Application", "JobSystemModule DoStop entry");
    Dia::Core::JobSystem::Shutdown();
    return Dia::ApplicationFlow::StopResult::kDone;
}

} } // namespace Cluiche::AppFlow

#include <DiaApplicationFlow/RegistrationMacrosV2.h>
namespace { using JobSystemModule_ = Cluiche::AppFlow::JobSystemModule; }
DIA_MODULE(JobSystemModule_);
