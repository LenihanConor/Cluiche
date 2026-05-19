#pragma once

#include <DiaCore/CRC/StringCRC.h>
#include <DiaMetrics/Counter.h>
#include <DiaMetrics/Gauge.h>
#include <DiaMetrics/Histogram.h>
#include <DiaMetrics/IMetricSink.h>
#include <DiaMetrics/MetricSnapshot.h>

#include <mutex>
#include <vector>

namespace Dia
{
    namespace Metric
    {
        class MetricRegistry
        {
        public:
            static MetricRegistry& Instance();

            // Registration — safe to call before any session starts.
            // Returns existing primitive if already registered with the same kind.
            // Asserts in Debug if name is registered with a different kind.
            Counter*   RegisterCounter  (const Dia::Core::StringCRC& name);
            Gauge*     RegisterGauge    (const Dia::Core::StringCRC& name);
            Histogram* RegisterHistogram(const Dia::Core::StringCRC& name,
                                         const float*  bucketBounds,
                                         unsigned int  bucketCount);

            // Lookup — returns nullptr if not registered.
            Counter*   FindCounter  (const Dia::Core::StringCRC& name) const;
            Gauge*     FindGauge    (const Dia::Core::StringCRC& name) const;
            Histogram* FindHistogram(const Dia::Core::StringCRC& name) const;

            // Snapshot — fills out with current values of all registered primitives.
            void Snapshot(MetricSnapshot& out) const;

            // Sink registration.
            void RegisterSink  (IMetricSink* sink);
            void UnregisterSink(IMetricSink* sink);

            // Broadcast to all registered sinks.
            void NotifySnapshot(const MetricSnapshot& snapshot);
            void NotifyFinal   (const MetricSnapshot& snapshot);

            // For testing — resets all registered primitives and sinks.
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
} // namespace Dia
