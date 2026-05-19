#include "Modules/DebugServerHostModule.h"

#include <DiaApplicationFlow/IApplicationControl.h>
#include <DiaApplicationFlow/IApplicationInspectable.h>
#include <DiaApplicationFlow/Streams/IStreamStore.h>
#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaCore/Json/external/json/json.h>
#include <DiaObservation/Log/LogLevel.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Metric/MetricRegistry.h>

#include <chrono>

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

    const char* gameName = config.isMember("game_name") && config["game_name"].isString()
        ? config["game_name"].asCString() : "";
    const char* gameBuild = config.isMember("game_build") && config["game_build"].isString()
        ? config["game_build"].asCString() : "";
    mServer.SetGameInfo(gameName, gameBuild);

    if (config.isMember("diagame_path") && config["diagame_path"].isString())
        mServer.SetDiagamePath(config["diagame_path"].asCString());

    if (config.isMember("log_level") && config["log_level"].isString())
        mServer.SetLogSinkLevel(Dia::Observation::Log::LogLevelFromString(config["log_level"].asCString()));
}

Dia::ApplicationFlow::StartResult DebugServerHostModule::DoStart()
{
    mServer.SetStateProvider(this);
    mServer.Start();

    // Start observation bridge — epoch offset computed from system/steady clock delta
    auto sysNow = std::chrono::system_clock::now();
    auto steadyNow = std::chrono::steady_clock::now();
    int64_t sysNanos = std::chrono::duration_cast<std::chrono::nanoseconds>(
        sysNow.time_since_epoch()).count();
    int64_t steadyNanos = static_cast<int64_t>(steadyNow.time_since_epoch().count());
    mServer.StartObservationBridge("", sysNanos - steadyNanos);

    // Register debug server metrics with the global MetricRegistry.
    {
        auto& reg = Dia::Observation::Metric::MetricRegistry::Instance();
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

Dia::ApplicationFlow::StopResult DebugServerHostModule::DoStop()
{
    mServer.Stop();

    // Null metric pointers — MetricRegistry owns the objects.
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

Dia::ApplicationFlow::IStreamStore* DebugServerHostModule::FindStream(
    const Dia::Core::StringCRC& id)
{
    auto* ctrl = GetApplication();
    auto* app = dynamic_cast<Dia::ApplicationFlow::IApplicationInspectable*>(ctrl);
    if (!app) return nullptr;
    return app->FindStream(id);
}

} } // namespace Cluiche::AppFlow

namespace { using DebugServerHostModule_ = Cluiche::AppFlow::DebugServerHostModule; }
DIA_MODULE(DebugServerHostModule_);
