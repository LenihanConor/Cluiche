// A4 weather test helper: executors for unit tests (Feature 2 passes).
// Ships with the library (no gtest dependency).
// Spec: docs/specs/applications/cow/systems/a4-weather/ca-pipeline.md
#pragma once

#include <DiaParallelExecutor/IParallelExecutor.h>

#include <stdint.h>

namespace Draoi {
namespace HexWorld {
namespace Weather {
namespace Testing {

namespace Detail
{
    // Range i of n over [0, count): contiguous, near-equal.
    inline void RangeBounds(uint32_t count, uint32_t n, uint32_t i, uint32_t& begin, uint32_t& end)
    {
        begin = static_cast<uint32_t>(static_cast<uint64_t>(count) * i / n);
        end = static_cast<uint32_t>(static_cast<uint64_t>(count) * (i + 1) / n);
    }

    inline uint32_t ClampRangeCount(uint32_t count, uint32_t n)
    {
        if (count == 0) { return 0; }
        if (n == 0) { n = 1; }
        return n > count ? count : n;
    }
}

// Splits [0, count) into exactly n contiguous near-equal ranges (clamped to count), ascending order.
// Ignores minRange. n == count gives one cell per range.
class FixedRangeExecutor : public Dia::ParallelExecutor::IParallelExecutor
{
public:
    explicit FixedRangeExecutor(uint32_t n) : mN(n) {}

    void ParallelFor(uint32_t count, uint32_t, const Dia::ParallelExecutor::RangeFn& fn) override
    {
        const uint32_t n = Detail::ClampRangeCount(count, mN);
        mLastRangeCount = n;
        for (uint32_t i = 0; i < n; ++i)
        {
            uint32_t b, e;
            Detail::RangeBounds(count, n, i, b, e);
            fn(b, e);
        }
    }

    uint32_t GetLastRangeCount() const override { return mLastRangeCount; }

private:
    uint32_t mN;
    uint32_t mLastRangeCount = 0;
};

// Same split as FixedRangeExecutor but runs the ranges in descending order. With n == 0 the range count is
// ceil(count / minRange), at least 1 (0 when count is 0).
class ReverseRangeExecutor : public Dia::ParallelExecutor::IParallelExecutor
{
public:
    explicit ReverseRangeExecutor(uint32_t n = 0) : mN(n) {}

    void ParallelFor(uint32_t count, uint32_t minRange, const Dia::ParallelExecutor::RangeFn& fn) override
    {
        uint32_t want = mN;
        if (want == 0)
        {
            want = minRange == 0 ? 1u : (count + minRange - 1) / minRange;
        }
        const uint32_t n = Detail::ClampRangeCount(count, want);
        mLastRangeCount = n;
        for (uint32_t i = n; i > 0; --i)
        {
            uint32_t b, e;
            Detail::RangeBounds(count, n, i - 1, b, e);
            fn(b, e);
        }
    }

    uint32_t GetLastRangeCount() const override { return mLastRangeCount; }

private:
    uint32_t mN;
    uint32_t mLastRangeCount = 0;
};

} // namespace Testing
} // namespace Weather
} // namespace HexWorld
} // namespace Draoi
