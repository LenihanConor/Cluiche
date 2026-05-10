#include "Modules/TimeServerModule.h"

#include <DiaLogger/DiaLog.h>
#include <DiaCore/Time/TimeAbsolute.h>
#include <DiaCore/Time/TimeRelative.h>

namespace Cluiche { namespace AppFlow {

const Dia::Core::StringCRC TimeServerModule::kTypeId("TimeServerModule");

TimeServerModule::TimeServerModule(const Dia::Core::StringCRC& instanceId)
    : Module(instanceId)
    , mTimeServer(30.0f, Dia::Core::TimeAbsolute::Zero())
{
}

Dia::ApplicationFlow::StartResult TimeServerModule::DoStart()
{
    DIA_LOG_INFO("Application", "TimeServerModule::DoStart entry");
    mTimeServer.Reset();
    DIA_LOG_INFO("Application", "TimeServerModule::DoStart exit");
    return Dia::ApplicationFlow::StartResult::kReady;
}

void TimeServerModule::DoUpdate(float /*dt*/)
{
    mTimeServer.Tick();
}

Dia::ApplicationFlow::StopResult TimeServerModule::DoStop()
{
    DIA_LOG_INFO("Application", "TimeServerModule::DoStop entry");
    return Dia::ApplicationFlow::StopResult::kDone;
}

float TimeServerModule::GetDeltaTime() const
{
    return mTimeServer.GetStep().AsFloatInSeconds();
}

float TimeServerModule::GetTotalTime() const
{
    return mTimeServer.GetTime().AsFloatInSeconds();
}

unsigned int TimeServerModule::GetFrameCount() const
{
    return static_cast<unsigned int>(mTimeServer.GetTick());
}

} } // namespace Cluiche::AppFlow

#include <DiaApplicationFlow/RegistrationMacrosV2.h>
namespace { using TimeServerModule_ = Cluiche::AppFlow::TimeServerModule; }
DIA_MODULE(TimeServerModule_);
