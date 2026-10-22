#pragma once

#include <DiaCore/CRC/StringCRC.h>
#include <DiaObservation/Metric/Counter.h>
#include <DiaObservation/Metric/Gauge.h>
#include <DiaObservation/Metric/Histogram.h>
#include <DiaObservation/Metric/IMetricSink.h>
#include <DiaObservation/Metric/MetricSnapshot.h>

#include <DiaObservation/Metric/MetricInfo.h>

#include <deque>
#include <mutex>
#include <vector>

namespace Dia
{
    namespace Observation
    {
        namespace Metric
        {
            enum class MetricKind { Counter, Gauge, Histogram };

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

                // ---- Metric info (optional label/description/unit) ----
                // Overloads of the Register* calls above. Info strings must outlive the registration
                // (pointers are stored, not copied). Re-registering a name returns the existing metric.
                // Info rules: first non-empty info wins; the same name registered with no info keeps
                // the stored info; a later info after a no-info registration is stored; a later,
                // different info (compared by string content) is ignored and logs ONE warning
                // (channel "VSC") per name.
                Counter*   RegisterCounter  (const Dia::Core::StringCRC& name, const MetricInfo& info);
                Gauge*     RegisterGauge    (const Dia::Core::StringCRC& name, const MetricInfo& info);
                Histogram* RegisterHistogram(const Dia::Core::StringCRC& name,
                                             const float*  bucketBounds,
                                             unsigned int  bucketCount,
                                             const MetricInfo& info);

                // Null when the metric is unknown or has no info. The pointer stays valid until Reset().
                const MetricInfo* GetInfo(const Dia::Core::StringCRC& name) const;

                // Read-only enumeration (counters, then gauges, then histograms). The callback runs
                // outside the registry lock on a copy of the entries, so it may call back into the registry.
                typedef void (*MetricVisitor)(const Dia::Core::StringCRC& name, MetricKind kind,
                                              const MetricInfo* info, void* user);
                void ForEachMetric(MetricVisitor visitor, void* user) const;

                // Increases by at least one on every registration that creates a NEW metric, or that
                // stores an info for a name that had none, or on Reset(). Unchanged by lookups of an
                // existing metric with the same, no, or a conflicting (ignored) info.
                unsigned int Generation() const;

                // Number of info-conflict warnings emitted so far (for tests / diagnostics).
                unsigned int ConflictWarningCount() const;

            private:
                // Applies info under the lock; returns true when a conflict warning should be logged.
                bool ApplyInfoLocked(const Dia::Core::StringCRC& name, const MetricInfo& info);
                void WarnConflict(const Dia::Core::StringCRC& name);

                struct InfoEntry { Dia::Core::StringCRC name; MetricInfo info; bool warned; };
                std::deque<InfoEntry> mInfos; // deque: stable addresses for GetInfo pointers
                unsigned int          mGeneration = 0;
                unsigned int          mConflictWarnings = 0;

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
