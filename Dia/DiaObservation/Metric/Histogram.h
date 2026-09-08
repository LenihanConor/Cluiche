#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>

namespace Dia
{
    namespace Observation
    {
        namespace Metric
        {
            static const unsigned int kHistogramMaxBounds = 16;

            class Histogram
            {
            public:
                Histogram(const float* bucketBounds, unsigned int bucketCount);

                void Observe(double value);

                struct Data
                {
                    float    bounds[kHistogramMaxBounds];
                    uint64_t counts[kHistogramMaxBounds + 1];
                    unsigned int bucketCount;
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
    } // namespace Observation
} // namespace Dia
