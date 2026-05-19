#pragma once

#include <DiaCore/CRC/StringCRC.h>
#include <DiaObservation/Metric/Counter.h>
#include <DiaObservation/Metric/Gauge.h>
#include <DiaObservation/Metric/Histogram.h>
#include <DiaObservation/Metric/IMetricSink.h>
#include <DiaObservation/Metric/MetricSnapshot.h>

#include <mutex>
#include <vector>

namespace Dia
{
    namespace Observation
    {
        namespace Metric
        {
            class MetricRegistry
            {
            public:
                static MetricRegistry& Instance();

                Counter*   RegisterCounter  (const Dia::Core::StringCRC& name);
                Gauge*     RegisterGauge    (const Dia::Core::StringCRC& name);
                Histogram* RegisterHistogram(const Dia::Core::StringCRC& name,
                                             const float*  bucketBounds,
                                             unsigned int  bucketCount);

                Counter*   FindCounter  (const Dia::Core::StringCRC& name) const;
                Gauge*     FindGauge    (const Dia::Core::StringCRC& name) const;
                Histogram* FindHistogram(const Dia::Core::StringCRC& name) const;

                void Snapshot(MetricSnapshot& out) const;

                void RegisterSink  (IMetricSink* sink);
                void UnregisterSink(IMetricSink* sink);

                void NotifySnapshot(const MetricSnapshot& snapshot);
                void NotifyFinal   (const MetricSnapshot& snapshot);

                void Reset();

            private:
                MetricRegistry()  = default;
                ~MetricRegistry() = default;

                MetricRegistry(const MetricRegistry&)            = delete;
                MetricRegistry& operator=(const MetricRegistry&) = delete;

                struct CounterEntry  { Dia::Core::StringCRC name; Counter*   primitive; };
                struct GaugeEntry    { Dia::Core::StringCRC name; Gauge*     primitive; };
                struct HistogramEntry{ Dia::Core::StringCRC name; Histogram* primitive; };

                mutable std::mutex           mMutex;
                std::vector<CounterEntry>    mCounters;
                std::vector<GaugeEntry>      mGauges;
                std::vector<HistogramEntry>  mHistograms;
                std::vector<IMetricSink*>    mSinks;
            };

        } // namespace Metric
    } // namespace Observation
} // namespace Dia
