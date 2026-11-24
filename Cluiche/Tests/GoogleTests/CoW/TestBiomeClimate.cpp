// S-1 Biome Classification task 3: Stage 12 (Temperature) and Stage 13 (Moisture); AC8, AC9 parts.
#include <gtest/gtest.h>
#include <DraoiHexWorldTerrain/BiomeClimate.h>
#include <DraoiHexWorldTerrain/Testing/BiomeTestGrids.h>
#include <Shared/WorldGeneration/CoWTesting/CoWTerrainRulesTestHelpers.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

using namespace CoW::WorldGeneration;
using namespace Draoi::HexWorld;
using namespace CoW::WorldGeneration::Testing;
using namespace Draoi::HexWorld::Testing;
using namespace Draoi::HexWorld::Testing;

namespace
{
    struct Stage13Result
    {
        std::vector<uint32_t> dist;
        std::vector<float>    air, rain, moist;
    };

    Stage13Result RunStage13(const ClimateGrid& g, float moistureOffset = 0.0f)
    {
        const TerrainRules rules = TestRules();
        const size_t       n = g.elevation.size();
        Stage13Result      r;
        r.dist.assign(n, 0u);
        r.air.assign(n, 0.0f);
        r.rain.assign(n, 0.0f);
        r.moist.assign(n, 0.0f);
        Stage13Layers out;
        out.waterDistance = r.dist.data();
        out.airMoisture = r.air.data();
        out.rainOut = r.rain.data();
        out.moisture = r.moist.data();
        ComputeStage13(rules, g.dims, moistureOffset, g.elevation.data(), g.latitude.data(), g.waterSource.data(), out);
        return r;
    }

    template <typename T>
    bool BytesEqual(const std::vector<T>& a, const std::vector<T>& b)
    {
        return a.size() == b.size() && (a.empty() || std::memcmp(a.data(), b.data(), a.size() * sizeof(T)) == 0);
    }

    std::vector<float> RunStage12(const ClimateGrid& g, float offset)
    {
        const TerrainRules rules = TestRules();
        std::vector<float> t(g.elevation.size(), -1.0f);
        ComputeStage12(rules, g.dims, offset, g.elevation.data(), g.latitude.data(), t.data());
        return t;
    }
}

TEST(BiomeClimateStage12, FormulaValues)
{
    ClimateGrid g(3, 1);
    g.latitude = {0.0f, 0.5f, 0.2f};
    g.elevation = {0.0f, 0.1f, 0.3f};
    const auto t = RunStage12(g, 0.0f);
    EXPECT_NEAR(t[0], 1.0f, 1e-6f);
    EXPECT_NEAR(t[1], 1.0f - 0.85f * 0.5f - 0.6f * 0.1f, 1e-6f);
    EXPECT_NEAR(t[2], 1.0f - 0.85f * 0.2f - 0.6f * 0.3f, 1e-6f);
}

TEST(BiomeClimateStage12, ClampsBothEnds)
{
    ClimateGrid g(2, 1);
    g.latitude = {1.0f, 0.0f};
    g.elevation = {1.0f, 0.0f};   // 1 - 0.85 - 0.6 < 0 ; 1 + offset > 1
    EXPECT_FLOAT_EQ(RunStage12(g, 0.0f)[0], 0.0f);
    EXPECT_FLOAT_EQ(RunStage12(g, 0.3f)[1], 1.0f);
    EXPECT_FLOAT_EQ(RunStage12(g, -0.3f)[0], 0.0f);
}

TEST(BiomeClimateStage12, OffsetShiftsExactlyWhenUnclamped)
{
    ClimateGrid g(2, 1);
    g.latitude = {0.4f, 0.6f};
    g.elevation = {0.2f, 0.1f};
    const auto base = RunStage12(g, 0.0f);
    const auto up = RunStage12(g, 0.2f);
    const auto dn = RunStage12(g, -0.2f);
    for (int i = 0; i < 2; ++i)
    {
        EXPECT_NEAR(up[i] - base[i], 0.2f, 1e-6f);
        EXPECT_NEAR(dn[i] - base[i], -0.2f, 1e-6f);
    }
}

TEST(BiomeClimateStage12, OceanGetsLatitudeTermOnly)
{
    ClimateGrid g(1, 1, 0.5f);
    g.SetWater(0, 0, WaterSource::Ocean);
    EXPECT_NEAR(RunStage12(g, 0.0f)[0], 1.0f - 0.85f * 0.5f, 1e-6f);
}

TEST(BiomeClimateStage12, IndependentOfStage13Inputs)
{
    ClimateGrid a(3, 3, 0.3f);
    ClimateGrid b = a;
    b.waterSource[4] = static_cast<uint32_t>(WaterSource::Lake);
    b.waterSource[0] = static_cast<uint32_t>(WaterSource::River);
    EXPECT_TRUE(BytesEqual(RunStage12(a, 0.1f), RunStage12(b, 0.1f)));
}

TEST(BiomeClimateStage13, WaterDistanceTinyGrid)
{
    ClimateGrid g(1, 7);
    g.SetWater(0, 0, WaterSource::Ocean);
    g.SetWater(0, 3, WaterSource::River);
    g.SetWater(0, 6, WaterSource::Lake);
    const auto r = RunStage13(g);
    const uint32_t expected[7] = {0, 1, 2, 2, 2, 1, 0};
    for (int i = 0; i < 7; ++i)
    {
        EXPECT_EQ(r.dist[i], expected[i]) << "y=" << i;
    }
}

TEST(BiomeClimateStage13, NoWaterGivesMaxDistanceAndZeroProximity)
{
    ClimateGrid g(3, 3, 0.5f);
    const auto r = RunStage13(g);
    EXPECT_EQ(r.dist[4], 0xFFFFFFFFu);
    for (float m : r.moist)
    {
        EXPECT_GE(m, 0.0f);
        EXPECT_LE(m, 1.0f);
    }
}

TEST(BiomeClimateStage13, ProximityMonotoneInDistance)
{
    // Trade/westerly irrelevant: use a flat grid at the equator with a lake; moisture falls with distance
    // once air is held constant by an identical (flat) profile. Check via the column of cells.
    ClimateGrid g(1, 12, 0.5f);
    g.SetWater(0, 0, WaterSource::Lake);
    const auto r = RunStage13(g);
    for (int y = 1; y < 12; ++y)
    {
        EXPECT_EQ(r.dist[y], static_cast<uint32_t>(y));
        EXPECT_LE(r.moist[y], r.moist[y - 1] + 1e-6f) << "y=" << y;
    }
}

TEST(BiomeClimateStage13, WesterlyRainShadow)
{
    const ClimateGrid g = MakeRidgeGrid(21, 9, 10, 0.5f, 0.5f, true, false);   // 45 deg
    const auto r = RunStage13(g);
    const int y = 4;
    const float windward = r.moist[g.Index(9, y)];
    const float leeward = r.moist[g.Index(11, y)];
    EXPECT_GT(windward, leeward);
    EXPECT_GT(r.rain[g.Index(10, y)], 0.0f);
    EXPECT_EQ(r.rain[g.Index(5, y)], 0.0f);
    EXPECT_EQ(r.rain[g.Index(15, y)], 0.0f);
}

TEST(BiomeClimateStage13, TradeBandShadowFlipsWest)
{
    const ClimateGrid g = MakeRidgeGrid(21, 9, 10, 0.5f, 0.15f, false, true);  // 13.5 deg
    const auto r = RunStage13(g);
    const int y = 4;
    EXPECT_GT(r.moist[g.Index(11, y)], r.moist[g.Index(9, y)]);   // east is windward
    EXPECT_GT(r.rain[g.Index(10, y)], 0.0f);
    EXPECT_EQ(r.rain[g.Index(15, y)], 0.0f);
}

TEST(BiomeClimateStage13, PolarBandIsEasterly)
{
    const ClimateGrid g = MakeRidgeGrid(21, 9, 10, 0.5f, 0.8f, false, true);   // 72 deg
    const auto r = RunStage13(g);
    EXPECT_GT(r.moist[g.Index(11, 4)], r.moist[g.Index(9, 4)]);
}

TEST(BiomeClimateStage13, MoistureOffsetShiftsExactlyOthersIdentical)
{
    const ClimateGrid g = MakeRidgeGrid(21, 9, 10, 0.5f, 0.5f, true, false);
    const auto base = RunStage13(g, 0.0f);
    for (float d : {0.2f, -0.2f, 0.3f, -0.3f})
    {
        const auto s = RunStage13(g, d);
        EXPECT_TRUE(BytesEqual(base.air, s.air));
        EXPECT_TRUE(BytesEqual(base.rain, s.rain));
        EXPECT_TRUE(BytesEqual(base.dist, s.dist));
        for (size_t i = 0; i < base.moist.size(); ++i)
        {
            const float expected = std::min(1.0f, std::max(0.0f, (base.moist[i] + d)));
            // base.moist is itself clamped, so only check cells unclamped at both ends.
            if (base.moist[i] > 0.0f && base.moist[i] < 1.0f && base.moist[i] + d > 0.0f && base.moist[i] + d < 1.0f)
            {
                EXPECT_NEAR(s.moist[i], expected, 1e-5f) << "i=" << i;
            }
            EXPECT_GE(s.moist[i], 0.0f);
            EXPECT_LE(s.moist[i], 1.0f);
        }
    }
}

TEST(BiomeClimateStage13, ClampsMoistureAtBothEnds)
{
    const ClimateGrid g = MakeRidgeGrid(21, 9, 10, 0.5f, 0.5f, true, false);
    const auto hi = RunStage13(g, 0.3f);
    const auto lo = RunStage13(g, -0.3f);
    EXPECT_FLOAT_EQ(hi.moist[g.Index(0, 4)], 1.0f);   // ocean cell: proximity 1, air 1
    EXPECT_FLOAT_EQ(lo.moist[g.Index(15, 4)], 0.0f);  // dry lee
}

TEST(BiomeClimateStage13, IndependentOfTemperatureOffset)
{
    const ClimateGrid g = MakeRidgeGrid(21, 9, 10, 0.5f, 0.5f, true, false);
    const auto a = RunStage13(g);
    RunStage12(g, 0.3f);
    const auto b = RunStage13(g);
    EXPECT_TRUE(BytesEqual(a.moist, b.moist));
}

TEST(BiomeClimateStage13, Deterministic)
{
    const ClimateGrid g = MakeRidgeGrid(21, 9, 10, 0.5f, 0.6f, true, true);
    const auto a = RunStage13(g, 0.1f);
    const auto b = RunStage13(g, 0.1f);
    EXPECT_TRUE(BytesEqual(a.moist, b.moist));
    EXPECT_TRUE(BytesEqual(a.air, b.air));
    EXPECT_TRUE(BytesEqual(a.rain, b.rain));
    EXPECT_TRUE(BytesEqual(a.dist, b.dist));
}

TEST(BiomeClimateStage13, EmptyAndSingleCellGrids)
{
    {
        ClimateGrid g(0, 0);
        const auto t = RunStage12(g, 0.0f);
        const auto r = RunStage13(g);
        EXPECT_TRUE(t.empty());
        EXPECT_TRUE(r.moist.empty());
    }
    {
        ClimateGrid g(1, 1, 0.5f);
        const auto r = RunStage13(g);
        EXPECT_GE(r.moist[0], 0.0f);
        EXPECT_LE(r.moist[0], 1.0f);
        g.SetWater(0, 0, WaterSource::Ocean);
        const auto o = RunStage13(g);
        EXPECT_EQ(o.dist[0], 0u);
        EXPECT_FLOAT_EQ(o.air[0], 1.0f);
        EXPECT_FLOAT_EQ(o.rain[0], 0.0f);
    }
}

TEST(BiomeClimateStage13, NullOutputsSkipped)
{
    const ClimateGrid g = MakeRidgeGrid(5, 5, 2, 0.5f, 0.5f, true, false);
    const TerrainRules rules = TestRules();
    Stage13Layers out;   // all null
    ComputeStage13(rules, g.dims, 0.0f, g.elevation.data(), g.latitude.data(), g.waterSource.data(), out);
    SUCCEED();
}
