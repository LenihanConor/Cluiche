#pragma once
////////////////////////////////////////////////////////////////////////////////
// Filename: PerfChainCommands.h
//
// Perf Inspector row 6b: Main-hosted DiaAPI command perf.chain.  Owns a reader on the framework
// "$perfspans" EventStream (closed trace spans handed over by SpanChunkSink from the tracer's drain
// thread) and a bounded Main-side history of them.  Drain() keeps the history current every Main tick
// (cheap when nothing is pending); the chain is only built when the command is called, so with no client
// attached nothing is packed.
//
// The command returns ChainData JSON ({"available":true,"name","stages","edges"}) for the S-1 card
// path (GetJSONState and the publisher on Sim, linked to the Main-side snapshot poll and push), or
// {"available":false,"reason":...} when tracing is off or no linked chain has been recorded.
//
// Registered in ALL builds.  Main thread only.  Does not own the store; it must outlive this object.
////////////////////////////////////////////////////////////////////////////////

#include <DiaCore/Json/external/json/json.h>
#include <DiaObservation/Trace/ChainBuilder.h>
#include <DiaObservation/Trace/SpanChunk.h>
#include <DiaStreams/EventStreamStore.h>

#include <cstdint>
#include <memory>
#include <vector>

namespace Cluiche { namespace AppFlow {

class PerfChainCommands
{
public:
    static constexpr uint32_t kHistoryCapacity = 4096;   // spans kept (ring, oldest dropped)

    // store:      the "$perfspans" store (may be null: Drain is a no-op and the reply says no spans).
    // threadName: maps a span's OS thread id to a processing unit name (may be empty: "thread <id>").
    // edgeStats:  optional override of the edge statistics source; empty = read stream.<id>.edge.* from
    //             the MetricRegistry (the rate is the writes counter delta between two perf.chain calls).
    PerfChainCommands(Dia::ApplicationFlow::EventStreamStore<Dia::Observation::Trace::SpanChunk>* store,
                      Dia::Observation::Trace::ThreadNameFn threadName,
                      Dia::Observation::Trace::EdgeStatsFn edgeStats = Dia::Observation::Trace::EdgeStatsFn());
    ~PerfChainCommands();

    PerfChainCommands(const PerfChainCommands&) = delete;
    PerfChainCommands& operator=(const PerfChainCommands&) = delete;

    // Registers perf.chain.  Idempotent.
    void Register();
    // Removes it again.  Idempotent.
    void Unregister();

    // Reads every pending chunk from the stream into the history.
    void Drain();

    // perf.chain: never fails; no data is {"available":false,"reason":...}.
    Json::Value HandleChain(const Json::Value& args);

    uint32_t SpanCount() const { return mCount; }

private:
    bool MetricEdgeStats(const char* stream, Dia::Observation::Trace::ChainEdgeStats& out);

    Dia::ApplicationFlow::EventStreamStore<Dia::Observation::Trace::SpanChunk>* mStore;
    int                                                                         mReader;
    Dia::Observation::Trace::ThreadNameFn                                       mThreadName;
    Dia::Observation::Trace::EdgeStatsFn                                        mEdgeStats;
    bool                                                                        mRegistered;

    std::vector<Dia::Observation::Trace::ChainSpan>                             mHistory;   // ring, kHistoryCapacity
    uint32_t                                                                    mHead;      // next write slot
    uint32_t                                                                    mCount;

    // Edge rate state: writes counter and steady time at the previous perf.chain call.
    bool                                                                        mHaveLastWrites;
    uint64_t                                                                    mLastWrites;
    uint64_t                                                                    mLastSteadyNs;

    typedef Dia::Core::Containers::DynamicArrayC<Dia::ApplicationFlow::Event<Dia::Observation::Trace::SpanChunk>, 4> DrainBuffer;
    std::unique_ptr<DrainBuffer>                                                mScratch;
};

}} // namespace Cluiche::AppFlow
