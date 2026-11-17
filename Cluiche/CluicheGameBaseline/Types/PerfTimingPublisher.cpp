#include "Types/PerfTimingPublisher.h"

#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Metric/Gauge.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include <DiaObservation/Profile/CoarseRing.h>

#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace Cluiche { namespace AppFlow {

using Dia::Core::StringCRC;
using namespace Dia::Observation::Profile;

namespace
{
    // Fields with no Main-reachable path today (no stream, data lives on PU threads or is not populated):
    //  latency          LatencyProbe lives in VisualDebuggerModule on the Sim PU, and is only fed in _DEBUG builds.
    //  trips            CoarseChunk::tripCount is never populated (always 0); the detector lives on each PU.
    //  marker_drops     CoarseChunk::markerDrops is never populated (always 0).
    //  suppressed_trips TripDetector::GetSuppressedCount() is PU-owned, only summarised at exit.
    //  suppressed_logs  no counter reachable from Main.
    const char* const kStaticUnavailable[] = { "latency", "trips", "marker_drops", "suppressed_trips", "suppressed_logs" };

    struct NameTable
    {
        std::vector<std::pair<uint32_t, std::string>> entries;

        const std::string* Find(uint32_t crc) const
        {
            for (const auto& e : entries)
                if (e.first == crc) return &e.second;
            return nullptr;
        }
        void Add(const StringCRC& id)
        {
            if (!Find(id.Value()))
                entries.emplace_back(id.Value(), std::string(id.AsChar()));
        }
    };

    void BuildNames(Dia::ApplicationFlow::IApplicationInspectable* app, NameTable& units, NameTable& modules)
    {
        if (!app) return;
        Dia::Core::Containers::DynamicArrayC<StringCRC, 4> pus;
        app->GetProcessingUnits(pus);
        auto active = std::make_unique<Dia::Core::Containers::DynamicArrayC<Dia::ApplicationFlow::ModuleStateInfo, 64>>();
        for (unsigned int p = 0; p < pus.Size(); ++p)
        {
            units.Add(pus[p]);
            active->RemoveAll();
            app->GetActiveModules(pus[p], *active);
            for (unsigned int m = 0; m < active->Size(); ++m)
                modules.Add((*active)[m].instanceId);
        }
    }

    std::string DisplayName(const NameTable& table, uint32_t crc)
    {
        const std::string* name = table.Find(crc);
        if (name && !name->empty()) return *name;
        char hex[16];
        snprintf(hex, sizeof(hex), "0x%08x", crc);
        return hex;
    }

    //  cap_use, peak_queue_depth, time_at_cap: the job observation does not expose pool occupancy, and
    //  nothing calls JobTimingTable::NotePoolSample, so the pool counters stay 0 (published, but not real).
    const char* const kJobsUnavailable[] = { "cap_use", "peak_queue_depth", "time_at_cap" };

    Json::Value BuildJobs(const JobChunkAssembler& jobs, const NameTable& unitNames, const NameTable& moduleNames)
    {
        Json::Value out(Json::objectValue);
        Json::Value units(Json::arrayValue);
        for (uint32_t i = 0; i < jobs.GetUnitCount(); ++i)
        {
            const uint32_t crc = jobs.GetUnitCrcAt(i);
            const JobChunk* latest = jobs.GetLatest(crc);
            if (!latest) continue;
            Json::Value u(Json::objectValue);
            u["unit"] = DisplayName(unitNames, crc);
            Json::Value owners(Json::arrayValue);
            const uint32_t n = latest->ownerCount < JobChunk::kMaxOwners ? latest->ownerCount : JobChunk::kMaxOwners;
            for (uint32_t o = 0; o < n; ++o)
            {
                const JobOwnerTier1& t = latest->owners[o];
                Json::Value e(Json::objectValue);
                e["name"]        = t.owner == 0 ? std::string("(other)") : DisplayName(moduleNames, t.owner);
                e["crc"]         = t.owner;
                e["count"]       = t.count;
                e["busyMs"]      = static_cast<double>(t.busyNs) * 1e-6;
                e["waitP50Ms"]   = static_cast<double>(t.waitP50Ms);
                e["waitP95Ms"]   = static_cast<double>(t.waitP95Ms);
                e["worstWaitMs"] = static_cast<double>(t.worstWaitMs);
                owners.append(e);
            }
            u["owners"] = owners;
            Json::Value pool(Json::objectValue);
            pool["capUse"]          = latest->pool.capUse;
            pool["timeAtCapMs"]     = static_cast<double>(latest->pool.timeAtCapMs);
            pool["peakQueueDepth"]  = latest->pool.peakQueueDepth;
            pool["inlineFallbacks"] = latest->pool.inlineFallbacks;
            pool["inboxDrops"]      = latest->pool.inboxDrops;
            u["pool"] = pool;
            units.append(u);
        }
        out["units"] = units;
        return out;
    }

    // ms -> integer 0.01 ms steps.
    int Hundredths(float ms) { return static_cast<int>(std::lround(ms * 100.0f)); }

    const StringCRC& TopicCrc() { static const StringCRC c(PerfTimingPublisher::TopicName()); return c; }
}

//---------------------------------------------------------------------------
// DebugServerPerfTimingSink
//---------------------------------------------------------------------------

uint32_t DebugServerPerfTimingSink::SubscriberCount() const
{
    return mServer.GetTopicSubscriberCount(TopicCrc());
}

void DebugServerPerfTimingSink::Publish(const Json::Value& payload)
{
    mServer.PublishTopic(TopicCrc(), payload);
}

bool DebugServerPerfTimingSink::TryGetSelfMs(double& outMs) const
{
    // Registry gauge, atomic read.  Every processing unit writes the SAME gauge, so this is the most
    // recent writer's per-frame telemetry self cost, not a per-unit value.
    const auto* g = Dia::Observation::Metric::MetricRegistry::Instance().FindGauge(StringCRC("perf.self_ms"));
    if (!g) return false;
    outMs = g->Value();
    return true;
}

//---------------------------------------------------------------------------
// PerfTimingPublisher
//---------------------------------------------------------------------------

PerfTimingPublisher::PerfTimingPublisher(const CoarseChunkAssembler& assembler,
                                         Dia::ApplicationFlow::IApplicationInspectable* app,
                                         IPerfTimingSink& sink,
                                         const JobChunkAssembler* jobs)
    : mAssembler(assembler)
    , mApp(app)
    , mSink(sink)
    , mJobs(jobs)
    , mLastPublishSec(0.0)
    , mHasPublished(false)
    , mPublishCount(0)
{
}

void PerfTimingPublisher::Tick(double nowSec)
{
    // Gate first: nothing is built or packed without a subscriber (SD-PERF-7).
    if (mSink.SubscriberCount() == 0)
        return;

    const double elapsed = nowSec - mLastPublishSec;
    if (mHasPublished && elapsed >= 0.0 && elapsed < kPeriodSec - 1e-9)
        return;   // 10 Hz at most (SD-PERF-2)
    mHasPublished   = true;
    mLastPublishSec = nowSec;

    NameTable unitNames, moduleNames;
    BuildNames(mApp, unitNames, moduleNames);

    Json::Value root(Json::objectValue);
    root["scale"]     = 0.01;
    root["windowSec"] = static_cast<double>(kPerfTimingWindowSecs);

    Json::Value units(Json::arrayValue);
    const uint32_t unitCount = mAssembler.GetUnitCount();
    for (uint32_t i = 0; i < unitCount; ++i)
    {
        const uint32_t crc = mAssembler.GetUnitCrcAt(i);
        const CoarseRing* ring = mAssembler.GetRing(crc);
        if (!ring) continue;
        const float hz = mAssembler.GetUnitHz(crc);

        Json::Value u(Json::objectValue);
        u["unit"] = DisplayName(unitNames, crc);
        u["hz"]   = static_cast<double>(hz);

        PerfSeriesStats unitStats;
        ComputePerfRingSeriesStats(*ring, CoarseRing::kUnitSlot, hz, kPerfTimingWindowSecs, unitStats, mScratch);
        u["samples"] = unitStats.samples;
        Json::Value tick(Json::arrayValue);
        tick.append(Hundredths(unitStats.ewmaMs));
        tick.append(Hundredths(unitStats.p95Ms));
        u["tick"] = tick;

        Json::Value names(Json::arrayValue), ewma(Json::arrayValue), p95(Json::arrayValue);
        for (uint32_t slot = 0; slot < ring->GetModuleCount(); ++slot)
        {
            const uint32_t mcrc = ring->GetModuleCrc(slot);
            if (mcrc == 0) continue;
            PerfSeriesStats ms;
            ComputePerfRingSeriesStats(*ring, slot, hz, kPerfTimingWindowSecs, ms, mScratch);
            names.append(DisplayName(moduleNames, mcrc));
            ewma.append(Hundredths(ms.ewmaMs));
            p95.append(Hundredths(ms.p95Ms));
        }
        u["modules"] = names;
        u["ewma"]    = ewma;
        u["p95"]     = p95;
        units.append(u);
    }
    root["units"] = units;

    Json::Value unavailable(Json::arrayValue);
    for (const char* name : kStaticUnavailable)
        unavailable.append(name);

    if (mJobs)
    {
        root["jobs"] = BuildJobs(*mJobs, unitNames, moduleNames);
        // The pool sampler does not exist yet, so these three are published as 0 and named here.
        for (const char* name : kJobsUnavailable)
            unavailable.append(name);
    }

    double selfMs = 0.0;
    if (mSink.TryGetSelfMs(selfMs))
        root["selfMs"] = selfMs;
    else
        unavailable.append("self_ms");
    root["unavailable"] = unavailable;

    mSink.Publish(root);
    ++mPublishCount;
}

}} // namespace Cluiche::AppFlow
