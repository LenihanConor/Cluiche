////////////////////////////////////////////////////////////////////////////////
// Filename: Light2DModule.cpp
////////////////////////////////////////////////////////////////////////////////
#include "Modules/Light2DModule.h"

#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaObservation/Log/DiaLog.h>

namespace Cluiche { namespace AppFlow {

const Dia::Core::StringCRC Light2DModule::kTypeId("Light2DModule");

Light2DModule::Light2DModule(const Dia::Core::StringCRC& instanceId)
    : SimModule(instanceId)
{}

Dia::ApplicationFlow::StartResult Light2DModule::DoStart()
{
    DIA_LOG_INFO("Application", "Light2DModule::DoStart");
    return Dia::ApplicationFlow::StartResult::kReady;
}

void Light2DModule::DoUpdate(const Dia::SimTime::SimTimeContext& /*ctx*/)
{
}

Dia::ApplicationFlow::StopResult Light2DModule::DoStop()
{
    DIA_LOG_INFO("Application", "Light2DModule::DoStop");
    return Dia::ApplicationFlow::StopResult::kDone;
}

} } // namespace Cluiche::AppFlow

namespace { using Light2DModule_ = Cluiche::AppFlow::Light2DModule; }
DIA_MODULE(Light2DModule_);
DIA_DESCRIBE(Light2DModule_::kTypeId, "Owns the LightRegistry2D for the SimPU.");
