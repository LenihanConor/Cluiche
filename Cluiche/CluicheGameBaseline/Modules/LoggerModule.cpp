#include "Modules/LoggerModule.h"

#include <DiaObservation/Log/Logger.h>
#include <DiaObservation/Log/ISink.h>
#include <DiaObservation/Log/DebugOutputSink.h>
#include <DiaObservation/Log/StdOutSink.h>
#include <DiaObservation/Log/LogLevel.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaApplicationFlow/RegistrationMacrosV2.h>

#include <cstring>

namespace Cluiche { namespace AppFlow {

LoggerModule::LoggerModule(const Dia::Core::StringCRC& instanceId)
    : Module(instanceId)
{
    std::memset(mOwnedSinks, 0, sizeof(mOwnedSinks));

    // Register both sinks by default:
    //   - DebugOutputSink -> Visual Studio Output window (OutputDebugStringA)
    //   - StdOutSink      -> stdout (visible in a bash/cmd terminal)
    // Config-driven sink selection can be added later once v2 module config
    // parsing is wired up.
    {
        Dia::Observation::Log::DebugOutputSink* sink = new Dia::Observation::Log::DebugOutputSink();
        sink->SetLevelThreshold(Dia::Observation::Log::LogLevel::kInfo);
        mOwnedSinks[mOwnedSinkCount++] = sink;
    }
    {
        Dia::Observation::Log::StdOutSink* sink = new Dia::Observation::Log::StdOutSink();
        sink->SetLevelThreshold(Dia::Observation::Log::LogLevel::kInfo);
        mOwnedSinks[mOwnedSinkCount++] = sink;
    }
}

LoggerModule::~LoggerModule()
{
}

Dia::ApplicationFlow::StartResult LoggerModule::DoStart()
{
    Dia::Observation::Log::Logger& logger = Dia::Observation::Log::Logger::Instance();
    logger.RegisterThreadBuffer();

    for (unsigned int i = 0; i < mOwnedSinkCount; ++i)
        logger.RegisterSink(mOwnedSinks[i]);

    DIA_LOG_INFO("Application", "LoggerModule DoStart");
    return Dia::ApplicationFlow::StartResult::kReady;
}

void LoggerModule::DoUpdate(float /*dt*/)
{
    // Drain is async — no work needed here.
}

Dia::ApplicationFlow::StopResult LoggerModule::DoStop()
{
    DIA_LOG_INFO("Application", "LoggerModule DoStop");
    Dia::Observation::Log::Logger& logger = Dia::Observation::Log::Logger::Instance();

    // Stop() must be called before UnregisterSink to avoid use-after-free
    // while the drain thread is still running.
    logger.Stop();

    for (unsigned int i = 0; i < mOwnedSinkCount; ++i)
        logger.UnregisterSink(mOwnedSinks[i]);

    logger.UnregisterThreadBuffer();

    for (unsigned int i = 0; i < mOwnedSinkCount; ++i)
    {
        delete mOwnedSinks[i];
        mOwnedSinks[i] = nullptr;
    }
    mOwnedSinkCount = 0;

    return Dia::ApplicationFlow::StopResult::kDone;
}

const Dia::Core::StringCRC LoggerModule::kTypeId("LoggerModule");

} } // namespace Cluiche::AppFlow

namespace { using LoggerModule_ = Cluiche::AppFlow::LoggerModule; }
DIA_MODULE(LoggerModule_);
