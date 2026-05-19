#pragma once

#include <DiaCore/CRC/StringCRC.h>
#include <cstdint>

namespace Dia
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
            } kind;

            // Counter
            uint64_t counterValue;

            // Gauge
            double gaugeValue;

            // Histogram
            uint64_t     histCount;
            double       histSum;
            BucketEntry  buckets[kMaxBucketBounds + 1]; // 16 user bounds + implicit +Inf
            unsigned int bucketCount;
            double       p50;
            double       p95;
            double       p99;
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
} // namespace Dia
