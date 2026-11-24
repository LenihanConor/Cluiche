#include "Modules/Weather/WeatherModule.h"

#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Trace/DiaTrace.h>
#include <Shared/World/WorldSimNames.h>

namespace CoW {

const Dia::Core::StringCRC WeatherModule::kTypeId("WeatherModule");

WeatherModule::WeatherModule(const Dia::Core::StringCRC& instanceId)
    : SimModule(instanceId)
{
}

WeatherModule::~WeatherModule() {}

Dia::Core::StringCRC WeatherModule::StageId() const
{
    return Dia::Core::StringCRC(World::StageId(World::kWeather).c_str());
}

void WeatherModule::OnConfigure(const char*) {}   // Task 5: spinup_weeks and parameters path

Dia::ApplicationFlow::StartResult WeatherModule::DoStart()
{
    using Dia::ApplicationFlow::StartResult;
    DIA_TRACE_ZONE("WeatherModule::DoStart", Dia::Observation::Trace::Category::kNone);

    WorldStepModule* worldStep = nullptr;
    if (mUseTestHosts)
    {
        worldStep = mTestHosts.worldStep;
    }
    else
    {
        worldStep = mWorldStepRef.Get();
        if (mBusRef.Get() == nullptr || mWorldGenRef.Get() == nullptr)
            return StartResult::kLoading;
    }
    if (worldStep == nullptr)
        return StartResult::kLoading;

    mStage.Start();
    if (!worldStep->RegisterStage(StageId(), mStage))
    {
        DIA_LOG_ERROR(Weather::kLogChannel, "WeatherModule: WorldStepModule refused stage id '%s'",
                      World::StageId(World::kWeather).c_str());
        mStage.Stop();
        return StartResult::kFailed;
    }
    mWorldStep = worldStep;
    mRegistered = true;
    DIA_LOG_INFO(Weather::kLogChannel, "WeatherModule: stage '%s' registered", World::StageId(World::kWeather).c_str());
    return StartResult::kReady;
}

void WeatherModule::DoUpdate(const Dia::SimTime::SimTimeContext&)
{
    mStage.Update();
}

Dia::ApplicationFlow::StopResult WeatherModule::DoStop()
{
    DIA_TRACE_ZONE("WeatherModule::DoStop", Dia::Observation::Trace::Category::kNone);
    if (mRegistered && mWorldStep != nullptr)
        mWorldStep->UnregisterStage(StageId());
    mRegistered = false;
    mWorldStep = nullptr;
    mStage.Stop();
    return Dia::ApplicationFlow::StopResult::kDone;
}

} // namespace CoW

namespace { using WeatherModule_ = CoW::WeatherModule; }
DIA_MODULE(WeatherModule_);
DIA_DESCRIBE(WeatherModule_::kTypeId, "A4 weather: the world-step weather stage host.");
