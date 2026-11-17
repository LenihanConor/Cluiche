#pragma once
////////////////////////////////////////////////////////////////////////////////
// Filename: PerfTimingPublisher.h
//
// Perf Inspector (task 3): the "perf.timing" push topic.  Called once per Main tick, publishes at
// most 10 Hz, and builds NOTHING unless at least one client is subscribed (SD-PERF-2, SD-PERF-7).
//
// Payload (all timings are integers in 0.01 ms, matching the coarse ring's fixed point):
//   { "scale":0.01, "windowSec":2,
//     "units":[ { "unit":name, "hz":60, "samples":n, "tick":[ewma,p95],
//                 "modules":[names], "ewma":[..], "p95":[..] } ... ],
//     "selfMs":x,                       // omitted when unavailable
//     "unavailable":[ "latency", ... ] }
// Only values reachable on Main WITHOUT a cross-PU read are published.  Anything else is named in
// "unavailable" and its field is omitted (never invented).  See kStaticUnavailable in the .cpp.
//
// Main thread only.  Does not own the assembler, the application view or the sink.
////////////////////////////////////////////////////////////////////////////////

#include <DiaApplicationFlow/IApplicationInspectable.h>
#include <DiaCore/Json/external/json/json.h>
#include <DiaDebugServer/DebugServer.h>
#include <DiaObservation/Profile/CoarseChunkAssembler.h>
#include <DiaObservation/Profile/PerfTimingStats.h>

#include <cstdint>

namespace Cluiche { namespace AppFlow {

// Seam between the publisher and the DebugServer (a fake in tests).
class IPerfTimingSink
{
public:
    virtual ~IPerfTimingSink() = default;
    // Number of clients subscribed to perf.timing (any thread safe on the real sink).
    virtual uint32_t SubscriberCount() const = 0;
    virtual void Publish(const Json::Value& payload) = 0;
    // Telemetry self cost (ms), false when it is not available.
    virtual bool TryGetSelfMs(double& outMs) const = 0;
};

// Production sink: DebugServer::PublishTopic + MetricRegistry gauge "perf.self_ms".
class DebugServerPerfTimingSink : public IPerfTimingSink
{
public:
    explicit DebugServerPerfTimingSink(Dia::DebugServer::DebugServer& server) : mServer(server) {}
    uint32_t SubscriberCount() const override;
    void Publish(const Json::Value& payload) override;
    bool TryGetSelfMs(double& outMs) const override;
private:
    Dia::DebugServer::DebugServer& mServer;
};

class PerfTimingPublisher
{
public:
    static constexpr double kPeriodSec = 0.1;   // 10 Hz
    static const char*      TopicName() { return "perf.timing"; }

    // app may be null (unit/module names fall back to 0x%08x).
    PerfTimingPublisher(const Dia::Observation::Profile::CoarseChunkAssembler& assembler,
                        Dia::ApplicationFlow::IApplicationInspectable* app,
                        IPerfTimingSink& sink);

    PerfTimingPublisher(const PerfTimingPublisher&) = delete;
    PerfTimingPublisher& operator=(const PerfTimingPublisher&) = delete;

    // nowSec: monotonic seconds from the tick's time source.  Publishes when subscribed and at least
    // kPeriodSec has passed since the last publish (the first subscribed tick publishes at once).
    void Tick(double nowSec);

    uint32_t GetPublishCount() const { return mPublishCount; }

private:
    const Dia::Observation::Profile::CoarseChunkAssembler& mAssembler;
    Dia::ApplicationFlow::IApplicationInspectable*         mApp;
    IPerfTimingSink&                                       mSink;
    double                                                 mLastPublishSec;
    bool                                                   mHasPublished;
    uint32_t                                               mPublishCount;
    Dia::Observation::Profile::PerfTimingScratch           mScratch;
};

}} // namespace Cluiche::AppFlow
