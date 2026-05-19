#include <DiaMetrics/Histogram.h>

#include <algorithm>
#include <cstring>

namespace Dia
{
    namespace Metric
    {
        Histogram::Histogram(const float* bucketBounds, unsigned int bucketCount)
            : mBucketCount(0)
            , mTotal(0)
            , mSum(0.0)
        {
            unsigned int count = bucketCount < kHistogramMaxBounds ? bucketCount : kHistogramMaxBounds;
            mBucketCount = count;
            std::memset(mBounds, 0, sizeof(mBounds));
            std::memset(mCounts, 0, sizeof(mCounts));
            for (unsigned int i = 0; i < count; ++i)
                mBounds[i] = bucketBounds[i];
        }

        void Histogram::Observe(double value)
        {
            std::lock_guard<std::mutex> lock(mMutex);
            mTotal++;
            mSum += value;

            bool placed = false;
            for (unsigned int i = 0; i < mBucketCount; ++i)
            {
                if (value <= static_cast<double>(mBounds[i]))
                {
                    mCounts[i]++;
                    placed = true;
                    break;
                }
            }
            if (!placed)
                mCounts[mBucketCount]++; // +Inf bucket
        }

        void Histogram::ReadData(Data& out) const
        {
            std::lock_guard<std::mutex> lock(mMutex);
            out.bucketCount = mBucketCount;
            out.total       = mTotal;
            out.sum         = mSum;
            for (unsigned int i = 0; i < mBucketCount; ++i)
                out.bounds[i] = mBounds[i];
            for (unsigned int i = 0; i <= mBucketCount; ++i)
                out.counts[i] = mCounts[i];
        }

    } // namespace Metric
} // namespace Dia
