// A4 weather: wind snap, prevailing direction, and test executors (ca-pipeline Task 2; AC 21, 22, 24).
// Tie window is inclusive: |angle - tie| <= 1e-4 rad. Edge tests use 1e-4 -/+ 1e-6 because a float
// cannot represent "exactly 1e-4 off" the tie.
#include <gtest/gtest.h>

#include <DraoiHexWorldWeather/WindDirection.h>
#include <DraoiHexWorldWeather/Testing/WeatherTestExecutors.h>

#include <math.h>
#include <vector>
#include <utility>

using namespace Draoi::HexWorld::Weather;

namespace
{
    constexpr float kPiF = 3.14159265358979f;
    constexpr float kDeg = kPiF / 180.0f;

    using Ranges = std::vector<std::pair<uint32_t, uint32_t>>;
    using FixedExec = Draoi::HexWorld::Weather::Testing::FixedRangeExecutor;
    using ReverseExec = Draoi::HexWorld::Weather::Testing::ReverseRangeExecutor;

    Ranges Collect(Dia::ParallelExecutor::IParallelExecutor& ex, uint32_t count, uint32_t minRange)
    {
        Ranges r;
        ex.ParallelFor(count, minRange, [&r](uint32_t b, uint32_t e) { r.emplace_back(b, e); });
        return r;
    }

    void ExpectCover(const Ranges& r, uint32_t count, bool ascending)
    {
        std::vector<int> hit(count, 0);
        for (const auto& p : r) { for (uint32_t i = p.first; i < p.second; ++i) { ++hit[i]; } }
        for (uint32_t i = 0; i < count; ++i) { EXPECT_EQ(hit[i], 1) << "cell " << i; }
        for (size_t i = 1; i < r.size(); ++i)
        {
            if (ascending) { EXPECT_EQ(r[i].first, r[i - 1].second); }
            else { EXPECT_EQ(r[i].second, r[i - 1].first); }
        }
    }
}

TEST(WeatherExecutors, FixedRangeCoversOnceAscending)
{
    const uint32_t N = 100;
    for (uint32_t n : { 1u, 3u, 17u, N })
    {
        FixedExec ex(n);
        const Ranges r = Collect(ex, N, 256);
        EXPECT_EQ(r.size(), n);
        EXPECT_EQ(ex.GetLastRangeCount(), n);
        ExpectCover(r, N, true);
    }
}

TEST(WeatherExecutors, FixedRangeNEqualsCountIsOneCellPerRange)
{
    FixedExec ex(7);
    const Ranges r = Collect(ex, 7, 1);
    ASSERT_EQ(r.size(), 7u);
    for (uint32_t i = 0; i < 7; ++i) { EXPECT_EQ(r[i], std::make_pair(i, i + 1)); }
}

TEST(WeatherExecutors, FixedRangeClampsToCount)
{
    FixedExec ex(50);
    const Ranges r = Collect(ex, 5, 1);
    EXPECT_EQ(r.size(), 5u);
    EXPECT_EQ(ex.GetLastRangeCount(), 5u);
    ExpectCover(r, 5, true);
}

TEST(WeatherExecutors, EmptyCountRunsNothing)
{
    FixedExec f(3);
    ReverseExec r(3);
    EXPECT_TRUE(Collect(f, 0, 1).empty());
    EXPECT_TRUE(Collect(r, 0, 1).empty());
    EXPECT_EQ(f.GetLastRangeCount(), 0u);
    EXPECT_EQ(r.GetLastRangeCount(), 0u);
}

TEST(WeatherExecutors, ReverseRangeCoversOnceDescending)
{
    const uint32_t N = 100;
    for (uint32_t n : { 1u, 3u, 17u, N })
    {
        ReverseExec ex(n);
        const Ranges r = Collect(ex, N, 256);
        EXPECT_EQ(r.size(), n);
        EXPECT_EQ(ex.GetLastRangeCount(), n);
        ExpectCover(r, N, false);
    }
}

TEST(WeatherExecutors, ReverseRangeDefaultUsesMinRange)
{
    ReverseExec ex;
    const Ranges r = Collect(ex, 1000, 256);
    EXPECT_EQ(r.size(), 4u);
    EXPECT_EQ(ex.GetLastRangeCount(), 4u);
    ExpectCover(r, 1000, false);
    EXPECT_EQ(Collect(ex, 10, 256).size(), 1u);
}

TEST(WeatherWindSnap, NonTieAnglesMapToNearestDirection)
{
    for (uint32_t d = 0; d < 6; ++d)
    {
        const float a = (30.0f + 60.0f * d) * kDeg;
        EXPECT_EQ(SnapWindDirection(a, 0), d);
        EXPECT_EQ(SnapWindDirection(a, 1), d);
        EXPECT_EQ(SnapWindDirection(a + 20.0f * kDeg, 0), d);
        EXPECT_EQ(SnapWindDirection(a - 20.0f * kDeg, 1), d);
    }
}

TEST(WeatherWindSnap, EastTieByColumnParity)
{
    for (float a : { 0.0f, 2.0f * kPiF, 5e-5f, -5e-5f })
    {
        EXPECT_EQ(SnapWindDirection(a, 0), 5u);
        EXPECT_EQ(SnapWindDirection(a, 2), 5u);
        EXPECT_EQ(SnapWindDirection(a, 1), 0u);
        EXPECT_EQ(SnapWindDirection(a, 3), 0u);
    }
}

TEST(WeatherWindSnap, WestTieByColumnParity)
{
    for (float a : { kPiF, kPiF + 5e-5f, kPiF - 5e-5f })
    {
        EXPECT_EQ(SnapWindDirection(a, 0), 3u);
        EXPECT_EQ(SnapWindDirection(a, 1), 2u);
    }
}

TEST(WeatherWindSnap, TieWindowEdgeIsInclusive)
{
    const float inside = 1e-4f - 1e-6f;
    const float outside = 1e-4f + 1e-6f;
    EXPECT_EQ(SnapWindDirection(inside, 0), 5u);
    EXPECT_EQ(SnapWindDirection(inside, 1), 0u);
    EXPECT_EQ(SnapWindDirection(outside, 0), 0u);   // just above 0: NE nearer
    EXPECT_EQ(SnapWindDirection(outside, 1), 0u);
    EXPECT_EQ(SnapWindDirection(-outside, 0), 5u);  // just below 0: SE nearer
    EXPECT_EQ(SnapWindDirection(-outside, 1), 5u);
    EXPECT_EQ(SnapWindDirection(kPiF + inside, 0), 3u);
    EXPECT_EQ(SnapWindDirection(kPiF + inside, 1), 2u);
    EXPECT_EQ(SnapWindDirection(kPiF + outside, 0), 3u);
    EXPECT_EQ(SnapWindDirection(kPiF + outside, 1), 3u);
    EXPECT_EQ(SnapWindDirection(kPiF - outside, 0), 2u);
    EXPECT_EQ(SnapWindDirection(kPiF - outside, 1), 2u);
}

TEST(WeatherWindSnap, OneThousandthEitherSideSnapsToNearerNotParity)
{
    const float e = 1e-3f;
    for (uint32_t col = 0; col < 2; ++col)
    {
        EXPECT_EQ(SnapWindDirection(e, col), 0u);
        EXPECT_EQ(SnapWindDirection(-e, col), 5u);
        EXPECT_EQ(SnapWindDirection(2.0f * kPiF - e, col), 5u);
        EXPECT_EQ(SnapWindDirection(kPiF + e, col), 3u);
        EXPECT_EQ(SnapWindDirection(kPiF - e, col), 2u);
    }
}

TEST(WeatherWindSnap, OtherTiesUseLowerIndex)
{
    // Half-steps at 60, 120, 240, 300 degrees sit between (0,1), (1,2), (3,4), (4,5).
    const uint32_t expected[] = { 0, 1, 3, 4 };
    const float deg[] = { 60.0f, 120.0f, 240.0f, 300.0f };
    for (int i = 0; i < 4; ++i)
    {
        EXPECT_EQ(SnapWindDirection(deg[i] * kDeg, 0), expected[i]);
        EXPECT_EQ(SnapWindDirection(deg[i] * kDeg, 1), expected[i]);
    }
}

TEST(WeatherWindSnap, OutOfRangeAnglesNormalise)
{
    const float twoPi = 2.0f * kPiF;
    for (uint32_t d = 0; d < 6; ++d)
    {
        const float a = (30.0f + 60.0f * d) * kDeg;
        EXPECT_EQ(SnapWindDirection(a + twoPi, 0), d);
        EXPECT_EQ(SnapWindDirection(a - twoPi, 0), d);
        EXPECT_EQ(SnapWindDirection(a + 3.0f * twoPi, 1), d);
        EXPECT_EQ(SnapWindDirection(a - 4.0f * twoPi, 1), d);
    }
    EXPECT_EQ(SnapWindDirection(-kPiF, 0), 3u);  // -pi is a west tie
    EXPECT_EQ(SnapWindDirection(-kPiF, 1), 2u);
    EXPECT_EQ(SnapWindDirection(-twoPi, 1), 0u);
}

TEST(WeatherWindSnap, DirectionVectorsMatchAngles)
{
    for (int d = 0; d < 6; ++d)
    {
        const float a = (30.0f + 60.0f * d) * kDeg;
        EXPECT_NEAR(kDirectionVectors[d].x, cosf(a), 1e-6f);
        EXPECT_NEAR(kDirectionVectors[d].y, -sinf(a), 1e-6f);
    }
}

TEST(WeatherPrevailingDirection, BandEdgesAndMidpoints)
{
    const WesterlyBand band;
    EXPECT_FLOAT_EQ(PrevailingDirection(0.0f, band), 0.0f);
    EXPECT_FLOAT_EQ(PrevailingDirection(0.2499f, band), 0.0f);
    EXPECT_FLOAT_EQ(PrevailingDirection(0.25f, band), 0.0f);
    EXPECT_NEAR(PrevailingDirection(0.30f, band), kPiF * 0.5f, 1e-5f);
    EXPECT_NEAR(PrevailingDirection(0.35f, band), kPiF, 1e-5f);
    EXPECT_FLOAT_EQ(PrevailingDirection(0.45f, band), kPiF);
    EXPECT_NEAR(PrevailingDirection(0.5499f, band), kPiF, 1e-5f);
    EXPECT_NEAR(PrevailingDirection(0.55f, band), kPiF, 1e-5f);
    EXPECT_NEAR(PrevailingDirection(0.60f, band), 1.5f * kPiF, 1e-5f);
    EXPECT_NEAR(PrevailingDirection(0.6499f, band), 2.0f * kPiF, 4e-3f);
    EXPECT_FLOAT_EQ(PrevailingDirection(0.65f, band), 0.0f);
    EXPECT_FLOAT_EQ(PrevailingDirection(1.0f, band), 0.0f);
}

TEST(WeatherPrevailingDirection, ContinuousAcrossTransitions)
{
    const WesterlyBand band;
    const float eps = 1e-4f;
    EXPECT_NEAR(PrevailingDirection(band.tradeEnd - eps, band), PrevailingDirection(band.tradeEnd + eps, band), 1e-2f);
    EXPECT_NEAR(PrevailingDirection(band.westerlyStart - eps, band), PrevailingDirection(band.westerlyStart + eps, band), 1e-2f);
    EXPECT_NEAR(PrevailingDirection(band.westerlyEnd - eps, band), PrevailingDirection(band.westerlyEnd + eps, band), 1e-2f);
    // Polar edge wraps 2pi -> 0 (same direction).
    EXPECT_NEAR(PrevailingDirection(band.polarStart - eps, band), 2.0f * kPiF, 1e-2f);
}

TEST(WeatherPrevailingDirection, CustomBandShiftsEdges)
{
    WesterlyBand band;
    band.tradeEnd = 0.35f; band.westerlyStart = 0.45f; band.westerlyEnd = 0.65f; band.polarStart = 0.75f;
    EXPECT_FLOAT_EQ(PrevailingDirection(0.30f, band), 0.0f);
    EXPECT_NEAR(PrevailingDirection(0.40f, band), kPiF * 0.5f, 1e-5f);
    EXPECT_FLOAT_EQ(PrevailingDirection(0.55f, band), kPiF);
}
