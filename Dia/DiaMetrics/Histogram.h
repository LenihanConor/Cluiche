#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>

namespace Dia
{
    namespace Metric
    {
        static const unsigned int kHistogramMaxBounds = 16;

        class Histogram
        {
        public:
            Histogram(const float* bucketBounds, unsigned int bucketCount);

            void Observe(double value);

            // Read via MetricRegistry::Snapshot() — not exposed directly.
            // Internal read used by registry.
            struct Data
            {
                float    bounds[kHistogramMaxBounds]; // upper bound for each user bucket
                uint64_t counts[kHistogramMaxBounds + 1]; // +Inf bucket last
                unsigned int bucketCount; // number of user bounds (not counting +Inf)
                uint64_t total;
                double   sum;
            };
            void ReadData(Data& out) const;

        private:
            float        mBounds[kHistogramMaxBounds];
            unsigned int mBucketCount;

            mutable std::mutex   mMutex;
            uint64_t             mCounts[kHistogramMaxBounds + 1];
            uint64_t             mTotal;
            double               mSum;
        };

    } // namespace Metric
} // namespace Dia
