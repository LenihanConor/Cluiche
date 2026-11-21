#pragma once
////////////////////////////////////////////////////////////////////////////////
// Filename: PerfTripPublisher.h
//
// Perf Inspector (task 3b): the "perf.trip" ordered topic.  Drain() is called once per Main tick,
// reads every pending TripChunk from the framework "$perftrips" EventStream and, ONLY when at least
// one client is subscribed, publishes one ordered event per trip.  With no subscriber the chunks are
// consumed and discarded (nothing is built).
//
// Payload: one JSON object with the keys of Dia::Observation::Profile::SerializeTripRecord (the
// trips.jsonl schema: unit, build_config, first_frame, last_frame, budget_ms, peak_ms,
// suppressed_since_last_trip, top_modules, sections, worst_frame, recent_unit_ms).
//
// Main thread only.  Does not own the store or the sink.  The reader slot is registered once in the
// constructor (slots cannot be released), so keep the object across debug server stop/start and
// swap the sink with SetSink().
////////////////////////////////////////////////////////////////////////////////

#include <DiaCore/Json/external/json/json.h>
#include <DiaDebugServer/DebugServer.h>
#include <DiaObservation/Profile/TripChunk.h>
#include <DiaStreams/EventStreamStore.h>

#include <cstdint>
#include <memory>

namespace Cluiche { namespace AppFlow {

// Seam between the publisher and the DebugServer (a fake in tests).
class IPerfTripSink
{
public:
    virtual ~IPerfTripSink() = default;
    virtual uint32_t SubscriberCount() const = 0;
    virtual void PublishOrdered(const Json::Value& payload) = 0;
};

// Production sink: DebugServer::PublishTopicOrdered("perf.trip").
class DebugServerPerfTripSink : public IPerfTripSink
{
public:
    explicit DebugServerPerfTripSink(Dia::DebugServer::DebugServer& server) : mServer(server) {}
    uint32_t SubscriberCount() const override;
    void PublishOrdered(const Json::Value& payload) override;
private:
    Dia::DebugServer::DebugServer& mServer;
};

class PerfTripPublisher
{
public:
    static const char* TopicName() { return "perf.trip"; }

    // store: the "$perftrips" store (may be null: Drain is then a no-op).  A reader is registered on it.
    explicit PerfTripPublisher(Dia::ApplicationFlow::EventStreamStore<Dia::Observation::Profile::TripChunk>* store);

    PerfTripPublisher(const PerfTripPublisher&) = delete;
    PerfTripPublisher& operator=(const PerfTripPublisher&) = delete;

    // null = drain and discard.
    void SetSink(IPerfTripSink* sink) { mSink = sink; }
    bool HasReader() const { return mReader >= 0; }

    void Drain();

    uint32_t GetPublishedCount() const { return mPublished; }

private:
    typedef Dia::Core::Containers::DynamicArrayC<Dia::ApplicationFlow::Event<Dia::Observation::Profile::TripChunk>, 4> DrainBuffer;

    Dia::ApplicationFlow::EventStreamStore<Dia::Observation::Profile::TripChunk>* mStore;
    int                         mReader;
    IPerfTripSink*              mSink;
    uint32_t                    mPublished;
    std::unique_ptr<DrainBuffer> mScratch;   // 4 x ~4 KB events: heap, never on the stack
};

}} // namespace Cluiche::AppFlow
