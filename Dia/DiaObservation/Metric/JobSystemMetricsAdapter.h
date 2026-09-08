#pragma once

#include <DiaThreading/IJobMetrics.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include <DiaObservation/Metric/Counter.h>
#include <DiaObservation/Metric/Gauge.h>
#include <DiaCore/CRC/StringCRC.h>

namespace Dia { namespace Observation { namespace Metric {

// JobSystemMetricsAdapter
// ---------------------------------------------------------------------------
// Concrete IJobMetrics that wires JobSystem instrumentation callbacks to
// MetricRegistry gauges and counters. Construct one and pass it to
// JobSystem::Initialize() when metrics are desired.
//
// Lifetime: must outlive the JobSystem it is passed to.
// ---------------------------------------------------------------------------
class JobSystemMetricsAdapter : public Dia::Threading::IJobMetrics
{
public:
    JobSystemMetricsAdapter()
    {
        auto& reg        = MetricRegistry::Instance();
        mQueueDepth    = reg.RegisterGauge  (Dia::Core::StringCRC("dia.jobs.queue_depth"));
        mActiveWorkers = reg.RegisterGauge  (Dia::Core::StringCRC("dia.jobs.active_workers"));
        mSubmitted     = reg.RegisterCounter(Dia::Core::StringCRC("dia.jobs.submitted"));
        mCompleted     = reg.RegisterCounter(Dia::Core::StringCRC("dia.jobs.completed"));
    }

    void OnJobSubmitted()                  override { if (mSubmitted)     mSubmitted->Inc(); }
    void OnJobCompleted()                  override { if (mCompleted)     mCompleted->Inc(); }
    void OnQueueDepthChanged(double depth) override { if (mQueueDepth)    mQueueDepth->Set(depth); }
    void OnActiveWorkersChanged(double n)  override { if (mActiveWorkers) mActiveWorkers->Set(n); }

private:
    Gauge*   mQueueDepth    = nullptr;
    Gauge*   mActiveWorkers = nullptr;
    Counter* mSubmitted     = nullptr;
    Counter* mCompleted     = nullptr;
};

}}} // namespace Dia::Observation::Metric
