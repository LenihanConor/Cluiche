#include <DiaMetrics/MetricRegistry.h>

#include <DiaCore/Core/Assert.h>
#include <algorithm>
#include <chrono>

namespace Dia
{
    namespace Metric
    {
        // Linear interpolation percentile within the bucket that contains the target rank.
        // Same convention as Prometheus client_golang.
        static double ComputePercentile(const Histogram::Data& data, double quantile)
        {
            if (data.total == 0)
                return 0.0;

            double targetRank = quantile * static_cast<double>(data.total);
            uint64_t cumulative = 0;

            for (unsigned int i = 0; i < data.bucketCount; ++i)
            {
                cumulative += data.counts[i];
                if (static_cast<double>(cumulative) >= targetRank)
                {
                    // Lower bound of this bucket
                    double lowerBound = (i == 0) ? 0.0 : static_cast<double>(data.bounds[i - 1]);
                    double upperBound = static_cast<double>(data.bounds[i]);

                    uint64_t prevCumulative = cumulative - data.counts[i];
                    double bucketFraction = (data.counts[i] == 0) ? 0.0
                        : (targetRank - static_cast<double>(prevCumulative))
                          / static_cast<double>(data.counts[i]);

                    return lowerBound + bucketFraction * (upperBound - lowerBound);
                }
            }

            // Falls in +Inf bucket — return upper bound of last explicit bucket
            return (data.bucketCount > 0)
                ? static_cast<double>(data.bounds[data.bucketCount - 1])
                : 0.0;
        }

        MetricRegistry& MetricRegistry::Instance()
        {
            static MetricRegistry sInstance;
            return sInstance;
        }

        Counter* MetricRegistry::RegisterCounter(const Dia::Core::StringCRC& name)
        {
            std::lock_guard<std::mutex> lock(mMutex);

            for (const auto& e : mCounters)
            {
                if (e.name == name)
                    return e.primitive;
            }

            // Assert if registered under a different kind
            for (const auto& e : mGauges)
                DIA_ASSERT(!(e.name == name), "Metric name already registered as a different kind");
            for (const auto& e : mHistograms)
                DIA_ASSERT(!(e.name == name), "Metric name already registered as a different kind");

            CounterEntry entry;
            entry.name      = name;
            entry.primitive = new Counter();
            mCounters.push_back(entry);
            return entry.primitive;
        }

        Gauge* MetricRegistry::RegisterGauge(const Dia::Core::StringCRC& name)
        {
            std::lock_guard<std::mutex> lock(mMutex);

            for (const auto& e : mGauges)
            {
                if (e.name == name)
                    return e.primitive;
            }

            for (const auto& e : mCounters)
                DIA_ASSERT(!(e.name == name), "Metric name already registered as a different kind");
            for (const auto& e : mHistograms)
                DIA_ASSERT(!(e.name == name), "Metric name already registered as a different kind");

            GaugeEntry entry;
            entry.name      = name;
            entry.primitive = new Gauge();
            mGauges.push_back(entry);
            return entry.primitive;
        }

        Histogram* MetricRegistry::RegisterHistogram(const Dia::Core::StringCRC& name,
                                                      const float*  bucketBounds,
                                                      unsigned int  bucketCount)
        {
            std::lock_guard<std::mutex> lock(mMutex);

            for (const auto& e : mHistograms)
            {
                if (e.name == name)
                    return e.primitive;
            }

            for (const auto& e : mCounters)
                DIA_ASSERT(!(e.name == name), "Metric name already registered as a different kind");
            for (const auto& e : mGauges)
                DIA_ASSERT(!(e.name == name), "Metric name already registered as a different kind");

            HistogramEntry entry;
            entry.name      = name;
            entry.primitive = new Histogram(bucketBounds, bucketCount);
            mHistograms.push_back(entry);
            return entry.primitive;
        }

        Counter* MetricRegistry::FindCounter(const Dia::Core::StringCRC& name) const
        {
            std::lock_guard<std::mutex> lock(mMutex);
            for (const auto& e : mCounters)
            {
                if (e.name == name)
                    return e.primitive;
            }
            return nullptr;
        }

        Gauge* MetricRegistry::FindGauge(const Dia::Core::StringCRC& name) const
        {
            std::lock_guard<std::mutex> lock(mMutex);
            for (const auto& e : mGauges)
            {
                if (e.name == name)
                    return e.primitive;
            }
            return nullptr;
        }

        Histogram* MetricRegistry::FindHistogram(const Dia::Core::StringCRC& name) const
        {
            std::lock_guard<std::mutex> lock(mMutex);
            for (const auto& e : mHistograms)
            {
                if (e.name == name)
                    return e.primitive;
            }
            return nullptr;
        }

        void MetricRegistry::Snapshot(MetricSnapshot& out) const
        {
            out.entryCount = 0;
            out.timestampSteadyNs = static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now().time_since_epoch()).count());

            std::lock_guard<std::mutex> lock(mMutex);

            for (const auto& e : mCounters)
            {
                if (out.entryCount >= kMaxMetricEntries) break;
                MetricEntry& entry  = out.entries[out.entryCount++];
                entry.name          = e.name;
                entry.kind          = MetricEntry::Kind::kCounter;
                entry.counterValue  = e.primitive->Value();
                entry.gaugeValue    = 0.0;
                entry.histCount     = 0;
                entry.histSum       = 0.0;
                entry.bucketCount   = 0;
                entry.p50 = entry.p95 = entry.p99 = 0.0;
            }

            for (const auto& e : mGauges)
            {
                if (out.entryCount >= kMaxMetricEntries) break;
                MetricEntry& entry  = out.entries[out.entryCount++];
                entry.name          = e.name;
                entry.kind          = MetricEntry::Kind::kGauge;
                entry.gaugeValue    = e.primitive->Value();
                entry.counterValue  = 0;
                entry.histCount     = 0;
                entry.histSum       = 0.0;
                entry.bucketCount   = 0;
                entry.p50 = entry.p95 = entry.p99 = 0.0;
            }

            for (const auto& e : mHistograms)
            {
                if (out.entryCount >= kMaxMetricEntries) break;
                MetricEntry& entry = out.entries[out.entryCount++];
                entry.name         = e.name;
                entry.kind         = MetricEntry::Kind::kHistogram;
                entry.counterValue = 0;
                entry.gaugeValue   = 0.0;

                Histogram::Data data;
                e.primitive->ReadData(data);

                entry.histCount   = data.total;
                entry.histSum     = data.sum;
                entry.bucketCount = data.bucketCount + 1; // +1 for +Inf

                for (unsigned int i = 0; i < data.bucketCount; ++i)
                {
                    entry.buckets[i].le    = data.bounds[i];
                    entry.buckets[i].count = data.counts[i];
                }
                // +Inf bucket
                entry.buckets[data.bucketCount].le    = 0.0f; // sentinel: +Inf
                entry.buckets[data.bucketCount].count = data.counts[data.bucketCount];

                entry.p50 = ComputePercentile(data, 0.50);
                entry.p95 = ComputePercentile(data, 0.95);
                entry.p99 = ComputePercentile(data, 0.99);
            }
        }

        void MetricRegistry::RegisterSink(IMetricSink* sink)
        {
            std::lock_guard<std::mutex> lock(mMutex);
            mSinks.push_back(sink);
        }

        void MetricRegistry::UnregisterSink(IMetricSink* sink)
        {
            std::lock_guard<std::mutex> lock(mMutex);
            mSinks.erase(std::remove(mSinks.begin(), mSinks.end(), sink), mSinks.end());
        }

        void MetricRegistry::NotifySnapshot(const MetricSnapshot& snapshot)
        {
            std::lock_guard<std::mutex> lock(mMutex);
            for (IMetricSink* sink : mSinks)
                sink->OnSnapshot(snapshot);
        }

        void MetricRegistry::NotifyFinal(const MetricSnapshot& snapshot)
        {
            std::lock_guard<std::mutex> lock(mMutex);
            for (IMetricSink* sink : mSinks)
                sink->OnFinal(snapshot);
        }

        void MetricRegistry::Reset()
        {
            std::lock_guard<std::mutex> lock(mMutex);

            for (auto& e : mCounters)   delete e.primitive;
            for (auto& e : mGauges)     delete e.primitive;
            for (auto& e : mHistograms) delete e.primitive;

            mCounters.clear();
            mGauges.clear();
            mHistograms.clear();
            mSinks.clear();
        }

    } // namespace Metric
} // namespace Dia
