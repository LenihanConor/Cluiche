#include "Modules/DebugServerHostModule.h"

#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/IApplicationControl.h>
#include <DiaApplicationFlow/IApplicationInspectable.h>
#include <DiaStreams/IStreamStore.h>
#include <DiaStreams/StreamTypeRegistry.h>
#include <DiaStreams/Event.h>
#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaCore/Json/external/json/json.h>
#include <DiaObservation/Log/LogLevel.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include <DiaObservation/Metric/Gauge.h>

#include "Modules/InspectorSources/ModuleStateSource.h"
#include "Modules/InspectorSources/StreamStateSource.h"
#include "Modules/InspectorSources/TimingAggregateSource.h"
#include "Modules/InspectorSources/LifecycleEventSource.h"
#include "Modules/InspectorSources/BlackboardInspectorSource.h"
#include "Types/SimRegistrationMessage.h"

#include <chrono>
#include <cstring>
#include <vector>
#include <memory>
#include <string>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>
#pragma comment(lib, "psapi.lib")
#endif

namespace Cluiche { namespace AppFlow {

const Dia::Core::StringCRC DebugServerHostModule::kTypeId("DebugServerHostModule");

DebugServerHostModule::DebugServerHostModule(const Dia::Core::StringCRC& instanceId)
    : Dia::ApplicationFlow::MainModule(instanceId)
{
}

DebugServerHostModule::~DebugServerHostModule() = default;

//---------------------------------------------------------------------------
// v2 lifecycle
//---------------------------------------------------------------------------

void DebugServerHostModule::OnConnectStreams(Dia::ApplicationFlow::Application& app)
{
    DIA_LOG_INFO("Application", "DebugServerHostModule::OnConnectStreams entry");
    mServerService.Connect(app);
    mDebugPushReader.Connect(app);
    mSimRequests.Connect(app);
    mSimReplies.Connect(app);
    DIA_LOG_INFO("Application", "DebugServerHostModule::OnConnectStreams reader_connected=%d",
        mDebugPushReader.IsConnected() ? 1 : 0);
}

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
    {
        const char* rawPath = config["diagame_path"].asCString();
        char absPath[512] = {};
        if (GetFullPathNameA(rawPath, sizeof(absPath), absPath, nullptr) > 0)
        {
            DIA_LOG_INFO("DebugServer", "DebugServerHostModule: diagame_path raw='%s' resolved='%s'", rawPath, absPath);
            mServer.SetDiagamePath(absPath);
        }
        else
        {
            DIA_LOG_WARNING("DebugServer", "DebugServerHostModule: GetFullPathNameA failed for '%s', using raw path", rawPath);
            mServer.SetDiagamePath(rawPath);
        }
    }
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
    mServer.SetSimRequestSink(this);
    mServer.Start();
    mServerPtr = &mServer;
    mServerService.Register(mServerPtr);

    // Create and activate inspector data sources.
    {
        auto* ctrl = GetApplication();
        auto* app = dynamic_cast<Dia::ApplicationFlow::IApplicationInspectable*>(ctrl);

        mSources[0] = std::make_unique<ModuleStateSource>(app);
        mSources[1] = std::make_unique<StreamStateSource>(app);
        mSources[2] = std::make_unique<TimingAggregateSource>(app);
        mSources[3] = std::make_unique<LifecycleEventSource>(app, &mUptimeSecs);
        mSources[4] = std::make_unique<BlackboardInspectorSource>(mBlackboardRegistry);

        for (auto& src : mSources)
            src->Activate(&mServer);
    }

    // ObservationBridge: now subscription-gated and batched — safe to enable.
    // Only sends to clients subscribed to observation.log / .trace / .metric / .health.
    // Logs batched ≤10/200ms, metrics latest-wins/500ms, traces ≤5/200ms, health immediate.
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

void DebugServerHostModule::DoUpdate(const Dia::SimTime::MainTimeContext& ctx)
{
    const float deltaTime = ctx.wallClockDt;
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
    DrainSimReplies();
    TickAnnounce(SteadyNowMs());
    mServer.Tick(deltaTime);

    // Tick all inspector data sources.
    {
        const auto& stats = mServer.GetStats();
        int connCount = static_cast<int>(mServer.GetConnectionCount());
        int subCount  = static_cast<int>(stats.subscriptionCount);
        for (auto& src : mSources)
            if (src) src->Tick(deltaTime, connCount, subCount);
    }

    // Drain all SimPU→MainPU debug push events and forward to subscribers.
    {
        Dia::Core::Containers::DynamicArrayC<Dia::ApplicationFlow::Event<DebugServerPushEvent>, 32> pushEvents;
        mDebugPushReader.Consume(pushEvents);
        if (pushEvents.Size() > 0)
        {
            DIA_LOG_INFO("DebugServer", "DebugServerHostModule: drained %u push events, subscribers=%u",
                pushEvents.Size(), mServer.GetStats().subscriptionCount);
        }
        for (unsigned int i = 0; i < pushEvents.Size(); ++i)
        {
            const DebugServerPushEvent& evt = pushEvents[i].payload;
            mServer.NotifySubscribers(evt.dataType, evt.payload);
        }
    }

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

bool DebugServerHostModule::PostRequest(const Dia::DebugServer::SimRequestEvent& request)
{
    if (!mSimRequests.IsConnected())
        return false;
    return Dia::ApplicationFlow::IsSuccess(mSimRequests.Send(request));
}

void DebugServerHostModule::DrainSimReplies()
{
    if (!mSimReplies.IsConnected())
        return;

    // Events are ~16 KB each: small batches, bounded work per frame.
    static constexpr unsigned int kBatch      = 2;
    static constexpr unsigned int kMaxBatches = 16;

    Dia::Core::Containers::DynamicArrayC<Dia::ApplicationFlow::Event<Dia::DebugServer::SimReplyEvent>, kBatch> batch;
    for (unsigned int b = 0; b < kMaxBatches && mSimReplies.HasPending(); ++b)
    {
        batch.RemoveAll();
        mSimReplies.Consume(batch);
        for (unsigned int i = 0; i < batch.Size(); ++i)
        {
            HandleSimReply(batch[i].payload, SteadyNowMs());
        }
    }
}

uint64_t DebugServerHostModule::SteadyNowMs()
{
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}

void DebugServerHostModule::RequestResync()
{
    Dia::DebugServer::SimRequestEvent* req = new Dia::DebugServer::SimRequestEvent();
    std::unique_ptr<Dia::DebugServer::SimRequestEvent> owner(req);   // ~16 KB: keep off the stack
    req->requestId = kSimRegistrationRequestId;   // control request
    req->nameCrc   = 0;
    req->kind      = static_cast<uint32_t>(Dia::DebugServer::SimKind::Resync);
    req->json[0]   = '\0';
    if (!PostRequest(*req))
    {
        mAnnounce.NotifyRequestFailed();
        DIA_LOG_WARNING("DebugServer", "DebugServerHostModule: resync request could not be posted, will retry");
    }
}

void DebugServerHostModule::InstallStagedSet()
{
    const uint32_t count = mAnnounce.StagedCount();
    std::vector<Dia::DebugServer::SimHostedName> names(count);
    for (uint32_t i = 0; i < count; ++i)
    {
        std::memset(&names[i], 0, sizeof(names[i]));
        std::strncpy(names[i].name, mAnnounce.StagedName(i), sizeof(names[i].name) - 1);
        names[i].kind = mAnnounce.StagedKind(i);
    }
    if (mServer.ReplaceSimHosted(names.data(), count))
        DIA_LOG_INFO("DebugServer", "DebugServerHostModule: sim announcement resync complete, %u names", count);
    else
        DIA_LOG_ERROR("DebugServer", "DebugServerHostModule: sim announcement resync could not install %u names", count);
}

void DebugServerHostModule::TickAnnounce(uint64_t nowMs)
{
    if (mAnnounce.Tick(nowMs) == SimAnnounceAction::DiscardStaging)
        DIA_LOG_WARNING("DebugServer", "DebugServerHostModule: sim announcement resync timed out, staging discarded");
}

void DebugServerHostModule::HandleSimReply(const Dia::DebugServer::SimReplyEvent& reply, uint64_t nowMs)
{
    const SimRegistration reg = ParseSimRegistrationReply(reply);
    switch (reg.op)
    {
        case SimRegistrationOp::NotRegistration:
            mServer.CompleteSimReply(reply);
            break;
        case SimRegistrationOp::Register:
        case SimRegistrationOp::Unregister:
        {
            const uint32_t gapsBefore = mAnnounce.AnnounceGaps();
            const SimAnnounceAction action = mAnnounce.OnRegistration(reg, nowMs);
            if (mAnnounce.AnnounceGaps() != gapsBefore)
                DIA_LOG_WARNING("DebugServer", "DebugServerHostModule: sim announcement gap at seq %u (total gaps %u)",
                    reg.seq, mAnnounce.AnnounceGaps());
            if (action == SimAnnounceAction::Apply || action == SimAnnounceAction::ApplyAndRequestResync)
            {
                if (reg.op == SimRegistrationOp::Register)
                    mServer.RegisterSimHosted(Dia::Core::StringCRC(reg.name), reg.kind);
                else
                    mServer.UnregisterSimHosted(Dia::Core::StringCRC(reg.name));
            }
            if (action == SimAnnounceAction::ApplyAndRequestResync)
                RequestResync();
            break;
        }
        case SimRegistrationOp::Digest:
        {
            uint32_t count = 0, hash = 0;
            mServer.GetSimHostedDigest(count, hash);
            const uint32_t before = mAnnounce.DigestMismatches();
            const SimAnnounceAction action = mAnnounce.OnDigest(reg, count, hash, nowMs);
            if (mAnnounce.DigestMismatches() != before)
                DIA_LOG_WARNING("DebugServer",
                    "DebugServerHostModule: sim digest mismatch (local count=%u hash=0x%08x, sim count=%u hash=0x%08x)",
                    count, hash, reg.count, reg.hash);
            if (action == SimAnnounceAction::RequestResync)
                RequestResync();
            break;
        }
        case SimRegistrationOp::ResyncBegin:
            if (mAnnounce.OnResyncBegin(reg, nowMs) == SimAnnounceAction::InstallSet)
                InstallStagedSet();
            break;
        case SimRegistrationOp::Replay:
        {
            const SimAnnounceAction action = mAnnounce.OnReplay(reg, nowMs);
            if (action == SimAnnounceAction::InstallSet)
                InstallStagedSet();
            else if (action == SimAnnounceAction::DiscardStaging)
                DIA_LOG_WARNING("DebugServer", "DebugServerHostModule: sim announcement resync timed out, staging discarded");
            break;
        }
        case SimRegistrationOp::Malformed:
            DIA_LOG_WARNING("DebugServer", "DebugServerHostModule: malformed sim registration message ignored");
            break;
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
    // Deactivate and destroy inspector data sources before stopping the server.
    for (auto& src : mSources)
    {
        if (src) src->Deactivate();
        src.reset();
    }

    mServerService.Deregister();
    mServerPtr = nullptr;
    mServer.SetSimRequestSink(nullptr);
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
