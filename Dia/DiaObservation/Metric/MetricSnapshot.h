#pragma once

#include <DiaCore/CRC/StringCRC.h>
#include <cstdint>

namespace Dia
{
    namespace Observation
    {
        namespace Metric
        {
            static const unsigned int kMaxMetricEntries = 128;
            static const unsigned int kMaxBucketBounds  = 16;

            struct BucketEntry
            {
                float    le;
                uint64_t count;
            };

            struct MetricEntry
            {
                Dia::Core::StringCRC name;

                enum class Kind : uint8_t
                {
                    kCounter,
                    kGauge,
                    kHistogram
                } kind = Kind::kCounter;

                // Counter
                uint64_t counterValue = 0;

                // Gauge
                double gaugeValue = 0.0;

                // Histogram
                uint64_t     histCount = 0;
                double       histSum = 0.0;
                BucketEntry  buckets[kMaxBucketBounds + 1] = {};
                unsigned int bucketCount = 0;
                double       p50 = 0.0;
                double       p95 = 0.0;
                double       p99 = 0.0;
            };

            struct MetricSnapshot
            {
                uint64_t     timestampSteadyNs;
                uint32_t     intervalMs;
                MetricEntry  entries[kMaxMetricEntries];
                unsigned int entryCount;

                MetricSnapshot() : timestampSteadyNs(0), intervalMs(0), entryCount(0) {}
            };

        } // namespace Metric
    } // namespace Observation
} // namespace Dia
