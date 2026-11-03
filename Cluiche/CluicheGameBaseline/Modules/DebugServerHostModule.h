#pragma once
////////////////////////////////////////////////////////////////////////////////
// Filename: DebugServerHostModule.h
//
// DiaApplicationFlow v2 Module adapter for Dia::DebugServer::DebugServer.
// Owns a DebugServer instance and drives its Start / Tick / Stop from the
// v2 module lifecycle.  Implements IDebugStateProvider to translate the
// v2 IApplicationInspectable interface into the narrow shape DebugServer
// consumes.
//
// Inspector data sources (module state, stream state, timing, lifecycle
// events) are managed as IInspectorDataSource instances — see
// InspectorSources/ for implementations.
//
// This adapter lives in CluicheGameBaseline so that Dia/DiaDebugServer
// itself has zero dependency on DiaApplicationFlow.
////////////////////////////////////////////////////////////////////////////////

#include <DiaApplicationFlow/MainModule.h>
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaApplicationFlow/Streams/ServiceStreamWriter.h>
#include <DiaCore/SimTime/SimTimeContext.h>
#include <DiaStreams/IStreamStore.h>
#include <DiaStreams/EventStreamReader.h>
#include <DiaStreams/EventStreamWriter.h>
#include <DiaDebugServer/DebugServer.h>
#include <DiaDebugServer/ISimRequestSink.h>
#include <DiaDebugServer/SimHostedTypes.h>
#include <DiaDebugServer/IDebugStateProvider.h>
#include <DiaDebugServer/IInspectorDataSource.h>
#include <DiaCore/CRC/StringCRC.h>
#include <memory>

#include <DiaBlackboard/BlackboardRegistry.h>
#include "Types/DebugServerPushEvent.h"
#include "Types/SimAnnounceTracker.h"

namespace Dia { namespace Observation { namespace Metric {
    class Gauge;
    class Counter;
    class Histogram;
} } }

namespace Cluiche { namespace AppFlow {

class DebugServerHostModule
    : public Dia::ApplicationFlow::MainModule
    , private Dia::DebugServer::IDebugStateProvider
    , private Dia::DebugServer::ISimRequestSink
{
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kMain;
    static constexpr const char* kDescription = "WebSocket debug server for editor connection";

    explicit DebugServerHostModule(const Dia::Core::StringCRC& instanceId);
    ~DebugServerHostModule() override;

    // Exposed so game code can reach the underlying server (subscribers,
    // query registry, etc.) in its own DoStart/DoUpdate after this module
    // has started.  Returns the same pointer for the lifetime of the
    // module — may be null before DoStart.
    Dia::DebugServer::DebugServer* GetServer() { return &mServer; }

    // Exposes the blackboard registry owned by this module so game modules
    // can register/unregister boards without a separate SimPU module.
    Dia::Blackboard::BlackboardRegistry& GetBlackboardRegistry() { return mBlackboardRegistry; }

    // Stream ID for the cross-PU DebugServer service.
    static constexpr const char* kServiceStreamId = "DebugServerService";

protected:
    // v2 Module lifecycle
    void OnConfigure(const char* configJson) override;
    void OnConnectStreams(Dia::ApplicationFlow::Application& app) override;
    Dia::ApplicationFlow::StartResult DoStart() override;
    void DoUpdate(const Dia::SimTime::MainTimeContext& ctx) override;
    Dia::ApplicationFlow::StopResult DoStop() override;

    // IDebugStateProvider — translates v2 IApplicationInspectable into the
    // debug-server-local types so DiaDebugServer doesn't depend on
    // DiaApplicationFlow.
    Dia::Core::StringCRC GetCurrentStage() const override;
    bool IsTransitioning() const override;
    bool IsShuttingDown() const override;
    void GetProcessingUnitIds(
        Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 4>& out) const override;
    void GetModulesInPU(
        const Dia::Core::StringCRC& puId,
        Dia::Core::Containers::DynamicArrayC<Dia::DebugServer::DebugModuleInfo, 64>& out) const override;
    Dia::DebugServer::IStreamTapTarget* FindStream(
        const Dia::Core::StringCRC& id) override;
    Json::Value SerializeStreamPayload(
        const Dia::Core::StringCRC& dataType,
        const void* bytes,
        size_t size) override;

    // ISimRequestSink — posts a sim-hosted request onto the SimQueryRequest EventStream
    // (Main -> Sim). False if the stream is not connected or rejected the event.
    bool PostRequest(const Dia::DebugServer::SimRequestEvent& request) override;

    // Handles one SimQueryReply event (registration side channel + announcement resync, else CompleteSimReply).
    // nowMs is a monotonic millisecond clock.  Exposed to derived test doubles.
    // senderCrc is the stream envelope's sender (0 = unknown, never guarded). The first sender of an announcement
    // owns the sim-hosted name table; announcements from any other sender are ignored with an Error (one host per app).
    void HandleSimReply(const Dia::DebugServer::SimReplyEvent& reply, uint64_t nowMs, unsigned int senderCrc = 0);

    // Registers the five Main-side announce metrics (announce_overflow lives on SimQueryHostModule). Called from DoStart.
    void RegisterAnnounceMetrics();
    // Publishes tracker counter deltas to the registry. Called once per update.
    void PublishAnnounceMetrics();
    uint32_t IgnoredForeignAnnounceCount() const { return mForeignAnnounceIgnored; }

    // Times out a stalled resync staging.  Called once per update.
    void TickAnnounce(uint64_t nowMs);

    const SimAnnounceTracker& GetAnnounceTracker() const { return mAnnounce; }

private:
    void RequestResync();
    void InstallStagedSet();
    static uint64_t SteadyNowMs();

    void QueryMemory();

    // Drains SimQueryReply (Sim -> Main). Reply requestId 0 is a registration message
    // (Types/SimRegistrationMessage.h) -> RegisterSimHosted / UnregisterSimHosted; any other
    // reply -> DebugServer::CompleteSimReply. Runs on Main before mServer.Tick().
    void DrainSimReplies();

    Dia::DebugServer::DebugServer mServer;
    Dia::DebugServer::DebugServer* mServerPtr = nullptr;
    Dia::ApplicationFlow::ServiceStreamWriter<Dia::DebugServer::DebugServer*> mServerService{
        this, Dia::Core::StringCRC(kServiceStreamId)};

    // Rolling FPS over a short accumulation window.
    float  mFpsAccMs   = 0.0f;
    int    mFpsFrames  = 0;
    double mUptimeSecs = 0.0;
    static constexpr float kFpsWindowSec = 0.5f;

    // Metric primitives — owned by MetricRegistry, pointers nulled on DoStop.
    Dia::Observation::Metric::Gauge*     mMetricFps           = nullptr;
    Dia::Observation::Metric::Gauge*     mMetricFrameTimeMs   = nullptr;
    Dia::Observation::Metric::Gauge*     mMetricMemoryBytes   = nullptr;
    Dia::Observation::Metric::Gauge*     mMetricUptimeSecs    = nullptr;
    Dia::Observation::Metric::Gauge*     mMetricConnections   = nullptr;
    Dia::Observation::Metric::Gauge*     mMetricSubscriptions = nullptr;
    Dia::Observation::Metric::Counter*   mMetricMessagesSent  = nullptr;
    Dia::Observation::Metric::Histogram* mMetricTickMs        = nullptr;
    int                                  mPrevMessagesSent    = 0;

    // Announcement resync metrics (counters mirror the SimAnnounceTracker's cumulative counts).
    Dia::Observation::Metric::Counter*   mMetricAnnounceGaps     = nullptr;
    Dia::Observation::Metric::Counter*   mMetricDigestMismatches = nullptr;
    Dia::Observation::Metric::Counter*   mMetricResyncsRequested = nullptr;
    Dia::Observation::Metric::Counter*   mMetricResyncsCompleted = nullptr;
    Dia::Observation::Metric::Counter*   mMetricResyncTimeouts   = nullptr;
    uint32_t mPrevGaps = 0, mPrevMismatches = 0, mPrevRequested = 0, mPrevCompleted = 0, mPrevTimeouts = 0;

    // Second-host guard: the sender of the first announcement owns the table.
    unsigned int mAnnounceOwnerCrc       = 0;
    unsigned int mLoggedForeignCrc       = 0;
    uint32_t     mForeignAnnounceIgnored = 0;

    // Inspector data sources — created in DoStart, activated, ticked, deactivated.
    static constexpr unsigned int kSourceCount = 5;
    std::unique_ptr<Dia::DebugServer::IInspectorDataSource> mSources[kSourceCount];

    // Single reader for all SimPU→MainPU debug push events.
    // EntityInspectorModule and AIInspectorModule both write to this stream;
    // DebugServerHostModule forwards each event without knowing the sender.
    Dia::ApplicationFlow::EventStreamReader<DebugServerPushEvent> mDebugPushReader{
        this, Dia::Core::StringCRC("DebugServerPush")};

    // Sim-hosted queries: Main -> Sim requests, Sim -> Main replies (and name registrations).
    // Counterpart on the SimPU is SimQueryHostModule.
    Dia::ApplicationFlow::EventStreamWriter<Dia::DebugServer::SimRequestEvent> mSimRequests{
        this, Dia::Core::StringCRC("SimQueryRequest")};
    Dia::ApplicationFlow::EventStreamReader<Dia::DebugServer::SimReplyEvent> mSimReplies{
        this, Dia::Core::StringCRC("SimQueryReply")};

    // Announcement resync state machine (Main thread only; no locking).
    SimAnnounceTracker mAnnounce;

    // Blackboard registry owned here (MainPU); game modules register/unregister
    // boards via GetBlackboardRegistry(). mSources[4] polls this directly.
    Dia::Blackboard::BlackboardRegistry mBlackboardRegistry;
};

} } // namespace Cluiche::AppFlow
