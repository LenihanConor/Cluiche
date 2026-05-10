#include "Modules/LoggerModule.h"

#include <DiaLogger/Logger.h>
#include <DiaLogger/ISink.h>
#include <DiaLogger/DebugOutputSink.h>
#include <DiaLogger/StdOutSink.h>
#include <DiaLogger/LogLevel.h>
#include <DiaLogger/DiaLog.h>
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
        Dia::Logger::DebugOutputSink* sink = new Dia::Logger::DebugOutputSink();
        sink->SetLevelThreshold(Dia::Logger::LogLevel::kInfo);
        mOwnedSinks[mOwnedSinkCount++] = sink;
    }
    {
        Dia::Logger::StdOutSink* sink = new Dia::Logger::StdOutSink();
        sink->SetLevelThreshold(Dia::Logger::LogLevel::kInfo);
        mOwnedSinks[mOwnedSinkCount++] = sink;
    }
}

LoggerModule::~LoggerModule()
{
}

Dia::ApplicationFlow::StartResult LoggerModule::DoStart()
{
    Dia::Logger::Logger& logger = Dia::Logger::Logger::Instance();
    logger.RegisterThreadBuffer();

    for (unsigned int i = 0; i < mOwnedSinkCount; ++i)
        logger.RegisterSink(mOwnedSinks[i]);

    DIA_LOG_INFO("Application", "LoggerModule DoStart");
    return Dia::ApplicationFlow::StartResult::kReady;
}

void LoggerModule::DoUpdate(float /*dt*/)
{
    Dia::Logger::Logger::Instance().FlushBuffers();
}

Dia::ApplicationFlow::StopResult LoggerModule::DoStop()
{
    DIA_LOG_INFO("Application", "LoggerModule DoStop");
    Dia::Logger::Logger& logger = Dia::Logger::Logger::Instance();

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
