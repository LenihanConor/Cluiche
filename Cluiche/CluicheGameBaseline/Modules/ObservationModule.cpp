#include "Modules/ObservationModule.h"

#include <DiaObservation/Log/Logger.h>
#include <DiaObservation/Log/ISink.h>
#include <DiaObservation/Log/DebugOutputSink.h>
#include <DiaObservation/Log/StdOutSink.h>
#include <DiaObservation/Log/LogLevel.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Config/ObservationConfigLoader.h>
#include <DiaObservation/Config/ObservationConfigCli.h>
#include <DiaApplicationFlow/RegistrationMacrosV2.h>

#include <cstring>
#include <cstdio>
#include <cstdlib>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace Cluiche { namespace AppFlow {

ObservationModule::ObservationModule(const Dia::Core::StringCRC& instanceId)
    : Module(instanceId)
{
    std::memset(mOwnedSinks, 0, sizeof(mOwnedSinks));

    // Under the VS debugger, stdout is also captured to the Output window, so registering
    // both DebugOutputSink and StdOutSink would double every line. Use one or the other.
    if (IsDebuggerPresent())
    {
        Dia::Observation::Log::DebugOutputSink* sink = new Dia::Observation::Log::DebugOutputSink();
        sink->SetLevelThreshold(Dia::Observation::Log::LogLevel::kInfo);
        mOwnedSinks[mOwnedSinkCount++] = sink;
    }
    else
    {
        Dia::Observation::Log::StdOutSink* sink = new Dia::Observation::Log::StdOutSink();
        sink->SetLevelThreshold(Dia::Observation::Log::LogLevel::kInfo);
        mOwnedSinks[mOwnedSinkCount++] = sink;
    }
}

ObservationModule::~ObservationModule()
{
}

Dia::ApplicationFlow::StartResult ObservationModule::DoStart()
{
    Dia::Observation::Log::Logger& logger = Dia::Observation::Log::Logger::Instance();
    logger.RegisterThreadBuffer();

    for (unsigned int i = 0; i < mOwnedSinkCount; ++i)
        logger.RegisterSink(mOwnedSinks[i]);

    DIA_LOG_INFO("Application", "ObservationModule DoStart");

    // Start session manager
    Dia::Observation::SessionConfig config;
    std::memset(&config, 0, sizeof(config));
    strncpy_s(config.appName, "CluicheTest", sizeof(config.appName) - 1);
    strncpy_s(config.buildVersion, "0.1.0", sizeof(config.buildVersion) - 1);
#ifdef DEBUG
    strncpy_s(config.buildConfig, "Debug", sizeof(config.buildConfig) - 1);
#else
    strncpy_s(config.buildConfig, "Release", sizeof(config.buildConfig) - 1);
#endif

    char exePath[512] = {};
    GetModuleFileNameA(nullptr, exePath, sizeof(exePath) - 1);
    char* lastSlash = strrchr(exePath, '\\');
    if (!lastSlash) lastSlash = strrchr(exePath, '/');
    if (lastSlash) *(lastSlash + 1) = '\0';
    snprintf(config.outRootDir, sizeof(config.outRootDir), "%s../../../../out/%s",
        exePath, config.appName);

    Dia::Observation::ObservationConfig obsConfig;
    char obsConfigPath[512] = {};
    snprintf(obsConfigPath, sizeof(obsConfigPath), "%s../../../../Assets/CluicheTest/cluichetest.diaobservation", exePath);
    Dia::Observation::ObservationConfigLoader::Load(obsConfigPath, obsConfig);

    Dia::Observation::ObservationConfigCli::ApplyOverrides(__argc, (const char* const*)__argv, obsConfig);

    if (!mSessionManager.Start(config, obsConfig))
    {
        DIA_LOG_ERROR("Application", "ObservationModule: failed to start session");
    }
    else
    {
        DIA_LOG_INFO("Application", "ObservationModule session: %s", mSessionManager.GetSessionId());
    }

    return Dia::ApplicationFlow::StartResult::kReady;
}

void ObservationModule::DoUpdate(float dt)
{
    mSessionManager.Tick(dt);
    mSessionManager.IncrementFrameCount();
}

Dia::ApplicationFlow::StopResult ObservationModule::DoStop()
{
    DIA_LOG_INFO("Application", "ObservationModule DoStop");

    Dia::Observation::Log::Logger& logger = Dia::Observation::Log::Logger::Instance();
    logger.Stop();

    mSessionManager.Stop();

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

const Dia::Core::StringCRC ObservationModule::kTypeId("ObservationModule");

} } // namespace Cluiche::AppFlow

namespace { using ObservationModule_ = Cluiche::AppFlow::ObservationModule; }
DIA_MODULE(ObservationModule_);
DIA_DESCRIBE(ObservationModule_::kTypeId, "Initializes the observation system: logging sinks, metrics, traces, and health reporters.");
