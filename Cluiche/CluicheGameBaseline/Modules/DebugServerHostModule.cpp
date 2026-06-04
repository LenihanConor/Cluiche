#include "Modules/DebugServerHostModule.h"

#include <DiaApplicationFlow/IApplicationControl.h>
#include <DiaApplicationFlow/IApplicationInspectable.h>
#include <DiaApplicationFlow/Streams/IStreamStore.h>
#include <DiaApplicationFlow/Streams/StreamTypeRegistry.h>
#include <DiaApplicationFlow/LifecycleEvent.h>
#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaCore/Json/external/json/json.h>
#include <DiaObservation/Log/LogLevel.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include <DiaObservation/Metric/Gauge.h>

#include <chrono>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>
#pragma comment(lib, "psapi.lib")
#endif

namespace Cluiche { namespace AppFlow {

const Dia::Core::StringCRC DebugServerHostModule::kTypeId("DebugServerHostModule");

DebugServerHostModule::DebugServerHostModule(const Dia::Core::StringCRC& instanceId)
    : Dia::ApplicationFlow::Module(instanceId)
{
}

DebugServerHostModule::~DebugServerHostModule() = default;

//---------------------------------------------------------------------------
// v2 lifecycle
//---------------------------------------------------------------------------

void DebugServerHostModule::OnConfigure(const char* configJson)
{
    if (configJson == nullptr || configJson[0] == '\0')
        return;

    Json::Value config;
    Json::Reader reader;
    if (!reader.parse(configJson, config))
    {
        DIA_LOG_WARNING("DebugServer", "DebugServerHostModule: OnConfigure — JSON parse failed");
        return;
    }

    if (config.isMember("port") && config["port"].isInt())
        mServer.SetPort(static_cast<uint16_t>(config["port"].asInt()));
    if (config.isMember("auto_start") && config["auto_start"].isBool())
        mServer.EnableAutoStart(config["auto_start"].asBool());
    if (config.isMember("diagame_path") && config["diagame_path"].isString())
        mServer.SetDiagamePath(config["diagame_path"].asCString());
    if (config.isMember("log_level") && config["log_level"].isString())
        mServer.SetLogSinkLevel(Dia::Observation::Log::LogLevelFromString(config["log_level"].asCString()));
}

Dia::ApplicationFlow::StartResult DebugServerHostModule::DoStart()
{
    // Read game identity from .diagame config
    if (const Json::Value* diagame = GetApplication()->GetDiagameConfig())
    {
        const char* gameName  = diagame->isMember("name")  && (*diagame)["name"].isString()
            ? (*diagame)["name"].asCString()  : "";
        const char* gameBuild = diagame->isMember("build") && (*diagame)["build"].isString()
            ? (*diagame)["build"].asCString() : "";
        mServer.SetGameInfo(gameName, gameBuild);
    }

    mServer.SetStateProvider(this);
    mServer.Start();

    // Attach $lifecycle tap so stage transitions are forwarded to connected
    // clients as push events.  The tap lives in the host (which owns
    // DiaApplicationFlow) so DiaDebugServer itself has no dependency on it.
    {
        auto* ctrl = GetApplication();
        auto* app = dynamic_cast<Dia::ApplicationFlow::IApplicationInspectable*>(ctrl);
        if (app)
        {
            auto* lifecycleStore = app->FindStream(Dia::Core::StringCRC("$lifecycle"));
            if (lifecycleStore)
            {
                auto handle = lifecycleStore->AttachTap(
                    [this](const void* bytes, unsigned int size,
                           const Dia::Core::StringCRC& /*streamId*/)
                    {
                        if (!bytes || size < sizeof(Dia::ApplicationFlow::LifecycleEvent)) return;
                        const auto* evt = static_cast<const Dia::ApplicationFlow::LifecycleEvent*>(bytes);
                        if (evt->kind != Dia::ApplicationFlow::LifecycleEventKind::kStageTransitionCommitted) return;
                        mServer.BroadcastStageTransition(evt->fromStage, evt->toStage);
                    });
                mLifecycleTapId = handle.id;
                if (mLifecycleTapId == 0)
                {
                    DIA_LOG_WARNING("DebugServer", "DebugServerHostModule::DoStart - $lifecycle tap attach failed");
                }
            }
            else
            {
                DIA_LOG_WARNING("DebugServer", "DebugServerHostModule::DoStart - $lifecycle stream not found, stage transitions degraded");
            }
        }
    }

    // Start observation bridge — epoch offset computed from system/steady clock delta
    auto sysNow = std::chrono::system_clock::now();
    auto steadyNow = std::chrono::steady_clock::now();
    int64_t sysNanos = std::chrono::duration_cast<std::chrono::nanoseconds>(
        sysNow.time_since_epoch()).count();
    int64_t steadyNanos = static_cast<int64_t>(steadyNow.time_since_epoch().count());
    mServer.StartObservationBridge("", sysNanos - steadyNanos);

    // Register metrics with the global MetricRegistry.
    {
        auto& reg = Dia::Observation::Metric::MetricRegistry::Instance();
        mMetricFps           = reg.RegisterGauge(Dia::Core::StringCRC("dia.fps"));
        mMetricFrameTimeMs   = reg.RegisterGauge(Dia::Core::StringCRC("dia.frame_time_ms"));
        mMetricMemoryBytes   = reg.RegisterGauge(Dia::Core::StringCRC("dia.memory_bytes"));
        mMetricUptimeSecs    = reg.RegisterGauge(Dia::Core::StringCRC("dia.uptime_s"));
        mMetricConnections   = reg.RegisterGauge(Dia::Core::StringCRC("dia.debugserver.connections"));
        mMetricSubscriptions = reg.RegisterGauge(Dia::Core::StringCRC("dia.debugserver.subscriptions"));
        mMetricMessagesSent  = reg.RegisterCounter(Dia::Core::StringCRC("dia.debugserver.messages_sent"));
        static const float kTickBuckets[] = { 0.0f, 1.0f, 2.0f, 5.0f, 10.0f, 20.0f };
        mMetricTickMs = reg.RegisterHistogram(
            Dia::Core::StringCRC("dia.debugserver.tick_ms"), kTickBuckets, 6);
    }

    return Dia::ApplicationFlow::StartResult::kReady;
}

void DebugServerHostModule::DoUpdate(float deltaTime)
{
    // Rolling FPS over kFpsWindowSec.
    if (deltaTime > 0.0f)
    {
        mFpsAccMs += deltaTime * 1000.0f;
        mFpsFrames++;
        mUptimeSecs += static_cast<double>(deltaTime);

        float windowSec = mFpsAccMs / 1000.0f;
        if (windowSec >= kFpsWindowSec)
        {
            float fps         = static_cast<float>(mFpsFrames) / windowSec;
            float frameTimeMs = mFpsAccMs / static_cast<float>(mFpsFrames);
            if (mMetricFps)         mMetricFps->Set(static_cast<double>(fps));
            if (mMetricFrameTimeMs) mMetricFrameTimeMs->Set(static_cast<double>(frameTimeMs));
            mFpsAccMs  = 0.0f;
            mFpsFrames = 0;
        }

        if (mMetricUptimeSecs) mMetricUptimeSecs->Set(mUptimeSecs);
    }

    QueryMemory();
    mServer.Tick(deltaTime);

    // Update debug server metrics from current ServerStats.
    const auto& stats = mServer.GetStats();
    if (mMetricConnections)
        mMetricConnections->Set(static_cast<double>(stats.connectionCount));
    if (mMetricSubscriptions)
        mMetricSubscriptions->Set(static_cast<double>(stats.subscriptionCount));
    if (mMetricTickMs)
        mMetricTickMs->Observe(static_cast<double>(stats.debugServerOverheadMs));
    if (mMetricMessagesSent && stats.messagesSentTotal > mPrevMessagesSent)
    {
        mMetricMessagesSent->Inc(static_cast<uint64_t>(stats.messagesSentTotal - mPrevMessagesSent));
        mPrevMessagesSent = stats.messagesSentTotal;
    }
}

void DebugServerHostModule::QueryMemory()
{
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
    {
        if (mMetricMemoryBytes)
            mMetricMemoryBytes->Set(static_cast<double>(pmc.WorkingSetSize));
    }
#endif
}

Dia::ApplicationFlow::StopResult DebugServerHostModule::DoStop()
{
    // Detach the $lifecycle tap before stopping the server.
    if (mLifecycleTapId != 0)
    {
        auto* ctrl = GetApplication();
        auto* app = dynamic_cast<Dia::ApplicationFlow::IApplicationInspectable*>(ctrl);
        if (app)
        {
            auto* lifecycleStore = app->FindStream(Dia::Core::StringCRC("$lifecycle"));
            if (lifecycleStore)
                lifecycleStore->DetachTap(Dia::ApplicationFlow::TapHandle{mLifecycleTapId});
        }
        mLifecycleTapId = 0;
    }

    mServer.Stop();

    // Null metric pointers — MetricRegistry owns the objects.
    mMetricFps           = nullptr;
    mMetricFrameTimeMs   = nullptr;
    mMetricMemoryBytes   = nullptr;
    mMetricUptimeSecs    = nullptr;
    mMetricConnections   = nullptr;
    mMetricSubscriptions = nullptr;
    mMetricMessagesSent  = nullptr;
    mMetricTickMs        = nullptr;

    return Dia::ApplicationFlow::StopResult::kDone;
}

//---------------------------------------------------------------------------
// IDebugStateProvider — adapt v2 IApplicationInspectable
//---------------------------------------------------------------------------

namespace {
    // Module::GetApplication() returns IApplicationControl*.  Application
    // implements both IApplicationControl and IApplicationInspectable, so
    // we dynamic_cast across.  Returns null if the module isn't attached
    // yet or the Application disappears.
    const Dia::ApplicationFlow::IApplicationInspectable* Inspect(
        const Dia::ApplicationFlow::Module& m)
    {
        auto* ctrl = const_cast<Dia::ApplicationFlow::Module&>(m).GetApplication();
        return dynamic_cast<const Dia::ApplicationFlow::IApplicationInspectable*>(ctrl);
    }

    const char* ModuleStateName(Dia::ApplicationFlow::ModuleState s)
    {
        using Dia::ApplicationFlow::ModuleState;
        switch (s)
        {
            case ModuleState::kInactive: return "inactive";
            case ModuleState::kStarting: return "starting";
            case ModuleState::kActive:   return "active";
            case ModuleState::kStopping: return "stopping";
            case ModuleState::kFailed:   return "failed";
        }
        return "unknown";
    }
}

Dia::Core::StringCRC DebugServerHostModule::GetCurrentStage() const
{
    if (const auto* app = Inspect(*this))
        return app->GetCurrentStage();
    return Dia::Core::StringCRC();
}

bool DebugServerHostModule::IsTransitioning() const
{
    if (const auto* app = Inspect(*this))
        return app->IsTransitioning();
    return false;
}

bool DebugServerHostModule::IsShuttingDown() const
{
    if (const auto* app = Inspect(*this))
        return app->IsShuttingDown();
    return false;
}

void DebugServerHostModule::GetProcessingUnitIds(
    Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 4>& out) const
{
    if (const auto* app = Inspect(*this))
        app->GetProcessingUnits(out);
}

void DebugServerHostModule::GetModulesInPU(
    const Dia::Core::StringCRC& puId,
    Dia::Core::Containers::DynamicArrayC<Dia::DebugServer::DebugModuleInfo, 64>& out) const
{
    const auto* app = Inspect(*this);
    if (!app) return;

    Dia::Core::Containers::DynamicArrayC<Dia::ApplicationFlow::ModuleStateInfo, 64> native;
    app->GetActiveModules(puId, native);

    for (unsigned int i = 0; i < native.Size() && !out.IsFull(); ++i)
    {
        Dia::DebugServer::DebugModuleInfo info;
        info.instanceId = native[i].instanceId;
        info.typeId     = native[i].typeId;
        info.state      = ModuleStateName(native[i].state);
        out.Add(info);
    }
}

Dia::DebugServer::IStreamTapTarget* DebugServerHostModule::FindStream(
    const Dia::Core::StringCRC& id)
{
    auto* ctrl = GetApplication();
    auto* app = dynamic_cast<Dia::ApplicationFlow::IApplicationInspectable*>(ctrl);
    if (!app) return nullptr;
    return app->FindStream(id);
}

Json::Value DebugServerHostModule::SerializeStreamPayload(
    const Dia::Core::StringCRC& dataType,
    const void* bytes,
    size_t size)
{
    return Dia::ApplicationFlow::StreamTypeRegistry::SerializeToJson(dataType, bytes, size);
}

} } // namespace Cluiche::AppFlow

namespace { using DebugServerHostModule_ = Cluiche::AppFlow::DebugServerHostModule; }
DIA_MODULE(DebugServerHostModule_);
DIA_DESCRIBE(DebugServerHostModule_::kTypeId, "Hosts the WebSocket debug server that editor and inspector tools connect to.");
