#include "ObservationModule.h"

#include <DiaApplicationFlow/IApplicationControl.h>

#include <DiaObservation/Log/Logger.h>
#include <DiaObservation/Log/ISink.h>
#include <DiaObservation/Log/DebugOutputSink.h>
#include <DiaObservation/Log/StdOutSink.h>
#include <DiaObservation/Log/LogLevel.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Config/ObservationConfigLoader.h>
#include <DiaObservation/Profile/PerfSummary.h>

#include <chrono>
#include <cstring>
#include <cstdio>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace Dia { namespace ApplicationFlow {

namespace {
    int64_t NowNs()
    {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    }
}

ObservationModule::ObservationModule(const Dia::Core::StringCRC& instanceId,
                                     const char* appName,
                                     const char* configAssetRelPath)
    : Module(instanceId)
    , mAppName(appName)
    , mConfigAssetRelPath(configAssetRelPath)
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

Dia::ApplicationFlow::StartResult ObservationModule::DoStart()
{
    Dia::Observation::Log::Logger& logger = Dia::Observation::Log::Logger::Instance();
    logger.RegisterThreadBuffer();

    for (unsigned int i = 0; i < mOwnedSinkCount; ++i)
        logger.RegisterSink(mOwnedSinks[i]);

    DIA_LOG_INFO("Application", "ObservationModule DoStart");

    Dia::Observation::SessionConfig config;
    std::memset(&config, 0, sizeof(config));
    strncpy_s(config.appName, mAppName, sizeof(config.appName) - 1);
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
    snprintf(obsConfigPath, sizeof(obsConfigPath), "%s%s", exePath, mConfigAssetRelPath);
    Dia::Observation::ObservationConfigLoader::Load(obsConfigPath, obsConfig);

    ApplyConfigOverrides(obsConfig);
    mPerfEnabled = obsConfig.perfConfig.perfEnabled;
    if (mPerfEnabled && obsConfig.perfConfig.perfExitSeconds > 0)
    {
        mPerfExitTimer.Arm(obsConfig.perfConfig.perfExitSeconds, NowNs());
        DIA_LOG_INFO("Application", "ObservationModule: --perf exit armed for %u s", static_cast<unsigned>(obsConfig.perfConfig.perfExitSeconds));
    }

    if (!mSessionManager.Start(config, obsConfig))
    {
        DIA_LOG_ERROR("Application", "ObservationModule: failed to start session");
    }
    else
    {
        DIA_LOG_INFO("Application", "ObservationModule session: %s", mSessionManager.GetSessionId());
    }

    // Perf Inspector A3c: plain pointer pass of the session's trip reporter to every PU.
    if (GetApplication() != nullptr)
        GetApplication()->SetTripReporter(mSessionManager.GetTripReporter());

    // Perf Inspector A4a: apply trip detection config to all PUs
    if (GetApplication() != nullptr)
    {
        // Convert ObservationConfig::PerfConfig to Dia::Observation::Profile::TripConfig
        Dia::Observation::Profile::TripConfig tripConfig;
        tripConfig.tripFrames    = obsConfig.perfConfig.tripFrames;
        tripConfig.tripFactor    = obsConfig.perfConfig.tripFactor;
        tripConfig.rearmFrames   = obsConfig.perfConfig.rearmFrames;
        tripConfig.minIntervalMs = static_cast<double>(obsConfig.perfConfig.minIntervalMs);
        tripConfig.budgetScale   = obsConfig.perfConfig.budgetScale;

        GetApplication()->SetTripConfig(tripConfig);
    }

    return Dia::ApplicationFlow::StartResult::kReady;
}

void ObservationModule::DoUpdate(float dt)
{
    mSessionManager.Tick(dt);
    mSessionManager.IncrementFrameCount();

    if (mPerfExitTimer.ShouldExit(NowNs()) && GetApplication() != nullptr)
    {
        DIA_LOG_INFO("Application", "ObservationModule: --perf duration elapsed, requesting shutdown");
        GetApplication()->RequestShutdown();
    }
}

Dia::ApplicationFlow::StopResult ObservationModule::DoStop()
{
    DIA_LOG_INFO("Application", "ObservationModule DoStop");

    Dia::Observation::Log::Logger& logger = Dia::Observation::Log::Logger::Instance();
    logger.Stop();

    if (GetApplication() != nullptr)
        GetApplication()->SetTripReporter(nullptr);

    // Perf Inspector A4b: one-shot perf-summary.json, before the session directory is finalised.
    // File I/O happens here on the stopping thread, never on a PU tick.
    if (mPerfEnabled && GetApplication() != nullptr)
    {
        Dia::Observation::Profile::PerfSummary summary;
        GetApplication()->CollectPerfSummary(summary);
        if (Dia::Observation::Profile::WritePerfSummaryIfEnabled(mPerfEnabled, summary, mSessionManager.GetSessionDirectory()))
            DIA_LOG_INFO("Application", "ObservationModule wrote perf-summary.json (%u units)", static_cast<unsigned>(summary.units.size()));
        else
            DIA_LOG_ERROR("Application", "ObservationModule: failed to write perf-summary.json");
    }
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

} } // namespace Dia::ApplicationFlow
