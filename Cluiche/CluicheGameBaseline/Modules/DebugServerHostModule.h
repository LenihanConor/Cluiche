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
// This adapter lives in CluicheGameBaseline so that Dia/DiaDebugServer
// itself has zero dependency on DiaApplicationFlow.
////////////////////////////////////////////////////////////////////////////////

#include <DiaApplicationFlow/Module.h>
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaApplicationFlow/Streams/ServiceStreamWriter.h>
#include <DiaStreams/IStreamStore.h>
#include <DiaStreams/EventStreamReader.h>
#include <DiaDebugServer/DebugServer.h>
#include <DiaDebugServer/IDebugStateProvider.h>
#include <DiaCore/CRC/StringCRC.h>

#include "Types/EntityInspectEvent.h"

namespace Dia { namespace Observation { namespace Metric {
    class Gauge;
    class Counter;
    class Histogram;
} } }

namespace Cluiche { namespace AppFlow {

class DebugServerHostModule
    : public Dia::ApplicationFlow::Module
    , private Dia::DebugServer::IDebugStateProvider
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

    // Stream ID for the cross-PU DebugServer service.
    static constexpr const char* kServiceStreamId = "DebugServerService";

protected:
    // v2 Module lifecycle
    void OnConfigure(const char* configJson) override;
    void OnConnectStreams(Dia::ApplicationFlow::Application& app) override;
    Dia::ApplicationFlow::StartResult DoStart() override;
    void DoUpdate(float deltaTime) override;
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

private:
    void QueryMemory();

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

    // Handle for the $lifecycle tap used to forward stage transitions to the
    // debug server as push notifications.  Attached in DoStart, detached in DoStop.
    unsigned int                         mLifecycleTapId      = 0;

    // EventStream reader for entity inspect events produced by SimPU.
    // Consumed here (MainPU) so NotifySubscribers is called from the host thread.
    Dia::ApplicationFlow::EventStreamReader<EntityInspectEvent> mEntityInspectReader{
        this, Dia::Core::StringCRC("EntityInspectPush")};
};

} } // namespace Cluiche::AppFlow
