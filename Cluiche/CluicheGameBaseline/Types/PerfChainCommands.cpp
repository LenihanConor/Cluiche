#include "Types/PerfChainCommands.h"

#include <DiaAPI/CommandRegistry/CommandRegistry.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Metric/Counter.h>
#include <DiaObservation/Metric/Gauge.h>
#include <DiaObservation/Metric/MetricRegistry.h>

#include <chrono>
#include <string>

namespace Cluiche { namespace AppFlow {

using Dia::ApplicationFlow::Event;
using Dia::Core::StringCRC;
using namespace Dia::Observation::Trace;

namespace
{
    const StringCRC kChainName("perf.chain");

    // The S-1 card path. Baseline (application layer) owns these names; the builder is generic.
    //   writer (Sim):  IDebugDomain.GetJSONState inside DomainStateSnapshotPublisher.Update
    //   reader (Main): DebugPanelPageModule.PollDomainSnapshot (carries the flow link and parses), then
    //                  DebugPanelPageModule.PushDomainStatesToPanel in the same Main trace.
    const char* const kChainTitle  = "Debug panel card state";
    const char* const kEndZone     = "DebugPanelPageModule.PollDomainSnapshot";
    const char* const kTailZone    = "DebugPanelPageModule.PushDomainStatesToPanel";
    const char* const kEdgeStream  = "DomainStateSnapshot";

    uint64_t SteadyNowNs()
    {
        return static_cast<uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
    }

    Json::Value Unavailable(const char* reason)
    {
        Json::Value v(Json::objectValue);
        v["available"] = false;
        v["reason"]    = reason;
        return v;
    }
}

PerfChainCommands::PerfChainCommands(Dia::ApplicationFlow::EventStreamStore<SpanChunk>* store,
                                     ThreadNameFn threadName, EdgeStatsFn edgeStats)
    : mStore(store)
    , mReader(-1)
    , mThreadName(std::move(threadName))
    , mEdgeStats(std::move(edgeStats))
    , mRegistered(false)
    , mHead(0)
    , mCount(0)
    , mHaveLastWrites(false)
    , mLastWrites(0)
    , mLastSteadyNs(0)
{
    if (mStore)
    {
        mReader = mStore->RegisterReader();
        if (mReader < 0)
            DIA_LOG_WARNING("PerfChain", "No free reader on $perfspans; perf.chain will have no data");
        else
        {
            mScratch = std::make_unique<DrainBuffer>();
            mHistory.resize(kHistoryCapacity);
        }
    }
}

PerfChainCommands::~PerfChainCommands()
{
    Unregister();
}

void PerfChainCommands::Register()
{
    if (mRegistered) return;
    mRegistered = true;

    Dia::API::CommandInfoJson c;
    c.name        = kChainName;
    c.description = "Most recent cross-thread trace chain (S-1 debug panel card path): stages with unit and start/end ms, and the stream edges between them with bytes, rate, data age and write cost. Reply {available:false, reason} when tracing is off or no linked chain was recorded";
    c.category    = StringCRC("perf");
    c.owner       = "CluicheGameBaseline";
    c.params      = nullptr;
    c.paramCount  = 0;
    c.callback    = [this](const Json::Value& args) { return HandleChain(args); };
    Dia::API::RegisterCommandJson(c);
    DIA_LOG_INFO("PerfChain", "Registered perf.chain");
}

void PerfChainCommands::Unregister()
{
    if (!mRegistered) return;
    mRegistered = false;
    Dia::API::UnregisterCommandJson(kChainName);
    DIA_LOG_INFO("PerfChain", "Unregistered perf.chain");
}

void PerfChainCommands::Drain()
{
    if (!mStore || mReader < 0 || !mScratch) return;
    // Bounded: at most 32 passes of 4 chunks per tick; leftovers wait for the next tick.
    for (int pass = 0; pass < 32; ++pass)
    {
        mScratch->RemoveAll();
        mStore->Consume(mReader, *mScratch);
        if (mScratch->Size() == 0) return;
        for (unsigned int i = 0; i < mScratch->Size(); ++i)
        {
            const SpanChunk& chunk = (*mScratch)[i].payload;
            const uint32_t n = chunk.count < SpanChunk::kMaxSpans ? chunk.count : SpanChunk::kMaxSpans;
            for (uint32_t s = 0; s < n; ++s)
            {
                mHistory[mHead] = chunk.spans[s];
                mHead = (mHead + 1) % kHistoryCapacity;
                if (mCount < kHistoryCapacity) ++mCount;
            }
        }
    }
}

bool PerfChainCommands::MetricEdgeStats(const char* stream, ChainEdgeStats& out)
{
    using namespace Dia::Observation::Metric;
    auto& reg = MetricRegistry::Instance();
    const std::string prefix = std::string("stream.") + stream + ".edge.";

    const Counter* writes   = reg.FindCounter(StringCRC((prefix + "writes").c_str()));
    const Counter* bytes    = reg.FindCounter(StringCRC((prefix + "bytes").c_str()));
    const Gauge*   dataAge  = reg.FindGauge  (StringCRC((prefix + "data_age_ns").c_str()));
    const Gauge*   writeCost = reg.FindGauge (StringCRC((prefix + "write_cost_ns").c_str()));

    if (writes && bytes)
    {
        const uint64_t w = writes->Value();
        if (w > 0)
        {
            out.hasBytes      = true;
            out.bytesPerWrite = static_cast<double>(bytes->Value()) / static_cast<double>(w);
        }
    }
    if (writes)
    {
        // Rate = writes delta over the steady time between two perf.chain calls; absent on the first call.
        const uint64_t now = SteadyNowNs();
        const uint64_t w   = writes->Value();
        if (mHaveLastWrites && now > mLastSteadyNs && w >= mLastWrites)
        {
            out.hasRate    = true;
            out.ratePerSec = static_cast<double>(w - mLastWrites) * 1.0e9 / static_cast<double>(now - mLastSteadyNs);
        }
        mHaveLastWrites = true;
        mLastWrites     = w;
        mLastSteadyNs   = now;
    }
    if (dataAge)
    {
        out.hasDataAge = true;
        out.dataAgeMs  = dataAge->Value() / 1.0e6;
    }
    if (writeCost)
    {
        out.hasWriteCost = true;
        out.writeCostMs  = writeCost->Value() / 1.0e6;
    }
    return writes || bytes || dataAge || writeCost;
}

Json::Value PerfChainCommands::HandleChain(const Json::Value& /*args*/)
{
    if (mCount == 0)
        return Unavailable("no spans recorded (is tracing on?)");

    // Linearise the ring, oldest first.
    std::vector<ChainSpan> ordered(mCount);
    const uint32_t start = (mHead + kHistoryCapacity - mCount) % kHistoryCapacity;
    for (uint32_t i = 0; i < mCount; ++i)
        ordered[i] = mHistory[(start + i) % kHistoryCapacity];

    const ChainConfig cfg = { kChainTitle, kEndZone, kTailZone, kEdgeStream };
    const EdgeStatsFn stats = mEdgeStats ? mEdgeStats
        : EdgeStatsFn([this](const char* stream, ChainEdgeStats& s) { return MetricEdgeStats(stream, s); });

    std::string text;
    BuildChainJson(ordered.data(), mCount, cfg, mThreadName, stats, text);

    Json::Value root;
    Json::Reader reader;
    if (!reader.parse(text, root))
        return Unavailable("internal error: chain reply did not parse");
    return root;
}

}} // namespace Cluiche::AppFlow
