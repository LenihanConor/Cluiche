#include "Modules/InspectorSources/TimingAggregateSource.h"
#include <DiaObservation/Metric/MetricRegistry.h>
#include <DiaObservation/Metric/Gauge.h>
#include <string>

namespace Cluiche { namespace AppFlow {

TimingAggregateSource::TimingAggregateSource(Dia::ApplicationFlow::IApplicationInspectable* app)
    : mApp(app)
{
}

Dia::Core::StringCRC TimingAggregateSource::GetTopic() const
{
    static const Dia::Core::StringCRC kTopic("app.timings");
    return kTopic;
}

Dia::DebugServer::SourcePolicy TimingAggregateSource::GetPolicy() const
{
    return { Dia::DebugServer::SourceStrategy::kPeriodic, 60.0f, 0, 0.0f };
}

void TimingAggregateSource::EnsurePUsCached()
{
    if (mPUCount > 0 || !mApp) return;
    Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 4> puIds;
    mApp->GetProcessingUnits(puIds);
    for (unsigned int p = 0; p < puIds.Size() && mPUCount < kMaxPUs; ++p)
    {
        mPUIds[mPUCount] = puIds[p];
        mAccumulators[mPUCount] = {};
        ++mPUCount;
    }
}

void TimingAggregateSource::AccumulateSample(float /*deltaTime*/)
{
    EnsurePUsCached();
    auto& reg = Dia::Observation::Metric::MetricRegistry::Instance();
    for (unsigned int p = 0; p < mPUCount; ++p)
    {
        std::string metricName = "pu.";
        metricName += mPUIds[p].AsChar();
        metricName += ".last_tick_ms";

        auto* gauge = reg.FindGauge(Dia::Core::StringCRC(metricName.c_str()));
        if (!gauge) continue;

        double tickMs  = gauge->Value();
        auto&  acc     = mAccumulators[p];
        acc.sumMs += tickMs;
        acc.samples++;
        if (tickMs < acc.minMs) acc.minMs = tickMs;
        if (tickMs > acc.maxMs) acc.maxMs = tickMs;
        if (tickMs > static_cast<double>(kHitchThresholdMs)) acc.hitches++;
    }
}

Json::Value TimingAggregateSource::BuildPayload()
{
    Json::Value timingsArray(Json::arrayValue);
    for (unsigned int p = 0; p < mPUCount; ++p)
    {
        const auto& acc   = mAccumulators[p];
        double      avgMs = (acc.samples > 0) ? acc.sumMs / acc.samples : 0.0;

        Json::Value entry;
        entry["puId"]           = mPUIds[p].AsChar();
        entry["lastTickMs"]     = avgMs;
        entry["avgMs"]          = avgMs;
        entry["minMs"]          = (acc.samples > 0) ? acc.minMs : 0.0;
        entry["maxMs"]          = acc.maxMs;
        entry["hitches"]        = acc.hitches;
        entry["samples"]        = acc.samples;
        entry["targetPeriodMs"] = 16.667;
        timingsArray.append(entry);
    }

    Json::Value payload;
    payload["timings"] = timingsArray;
    return payload;
}

void TimingAggregateSource::Reset()
{
    for (unsigned int p = 0; p < mPUCount; ++p)
        mAccumulators[p] = {};
}

}} // namespace Cluiche::AppFlow
