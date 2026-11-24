// A4 Weather Feature 1 (weather-data.plan.md Tasks 3 and 4): SeasonalTargetTemperature and WeatherSimulation::Initialise.
// Tests build WeatherParameters by hand; they never load the sidecar.
#include <gtest/gtest.h>

#include <DraoiHexWorld/BiomeRegistry.h>
#include <DraoiHexWorld/Testing/BiomeTestRegistry.h>
#include <DraoiHexWorldWeather/WeatherSimulation.h>

#include <cstring>

namespace
{
    using namespace Draoi::HexWorld;
    using namespace Draoi::HexWorld::Weather;
    namespace T = Draoi::HexWorld::Testing;

    constexpr float kCooling   = 0.3f;
    constexpr float kWaterMois = 0.8f;
    constexpr float kLandMois  = 0.3f;

    // Distinct, in-range baseline per (season, biome): 0.05 per biome step, 0.1 per season step.
    float TableValue(uint32_t season, uint32_t biome) { return 0.05f * float(biome + 1) + 0.1f * float(season); }

    WeatherParameters MakeParams()
    {
        WeatherParameters p;
        p.latitudeCoolingFactor = kCooling;
        p.initialMoistureWater  = kWaterMois;
        p.initialMoistureLand   = kLandMois;
        for (uint32_t s = 0; s < kSeasonCount; ++s)
            for (uint32_t b = 0; b < BiomeRegistry::kMaxBiomes; ++b) p.BaselineRow(Season(s))[b] = TableValue(s, b);
        return p;
    }

    // 6 x 4 hand-built terrain: biomes cycle through the registry, latitude rises with the row, a few water sources.
    WeatherTerrain MakeTerrain(uint32_t w = 6, uint32_t h = 4, uint32_t biomeCount = 13)
    {
        WeatherTerrain t;
        t.width = w;
        t.height = h;
        const uint32_t n = w * h;
        t.elevation.Reserve(n); t.latitude.Reserve(n); t.waterSource.Reserve(n); t.biome.Reserve(n);
        for (uint32_t i = 0; i < n; ++i)
        {
            t.elevation.Add(0.3f);
            t.latitude.Add(float(i / w) / float(h > 1 ? h - 1 : 1));
            t.waterSource.Add(i == 0 || i == 1 ? WaterSource::Ocean : (i == 5 ? WaterSource::Lake : WaterSource::None));
            t.biome.Add(BiomeIndex(i % biomeCount));
        }
        return t;
    }

    float Clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

    bool SameFields(const WeatherFields& a, const WeatherFields& b)
    {
        auto same = [](const Dia::Core::Containers::DynamicArray<float>& x, const Dia::Core::Containers::DynamicArray<float>& y)
        {
            return x.Size() == y.Size() && (x.Size() == 0 || std::memcmp(&x[0u], &y[0u], x.Size() * sizeof(float)) == 0);
        };
        return same(a.moisture, b.moisture) && same(a.temperature, b.temperature) && same(a.pressure, b.pressure)
            && same(a.windDirection, b.windDirection) && same(a.windSpeed, b.windSpeed)
            && same(a.precipitation, b.precipitation) && same(a.cloudCover, b.cloudCover)
            && same(a.waterToGround, b.waterToGround) && same(a.snowPack, b.snowPack)
            && same(a.pressureAnomaly, b.pressureAnomaly);
    }

    const BiomeRegistry& Reg() { return T::ShippedBiomeRegistry(); }
}

// ---- SeasonalTargetTemperature (Task 3) -------------------------------------------------------------------------

TEST(WeatherInitialise, TargetTemperatureLatitudeZeroGivesBaseline)
{
    EXPECT_FLOAT_EQ(SeasonalTargetTemperature(0.6f, 0.0f, 0.3f), 0.6f);
}

TEST(WeatherInitialise, TargetTemperatureLatitudeOneCoolsByTheFactor)
{
    EXPECT_FLOAT_EQ(SeasonalTargetTemperature(0.6f, 1.0f, 0.3f), 0.6f * 0.7f);
    EXPECT_FLOAT_EQ(SeasonalTargetTemperature(0.5f, 1.0f, 0.5f), 0.25f);
}

TEST(WeatherInitialise, TargetTemperatureClampsToUnitRange)
{
    EXPECT_FLOAT_EQ(SeasonalTargetTemperature(1.5f, 0.0f, 0.3f), 1.0f);
    EXPECT_FLOAT_EQ(SeasonalTargetTemperature(-0.2f, 0.0f, 0.3f), 0.0f);
}

// ---- Rejection paths (Task 3) -----------------------------------------------------------------------------------

TEST(WeatherInitialise, NoWaterSourceIsRejectedAndLeavesSimUninitialised)
{
    WeatherTerrain t = MakeTerrain();
    for (uint32_t i = 0; i < t.waterSource.Size(); ++i) t.waterSource[i] = WaterSource::None;
    WeatherSimulation sim;
    EXPECT_FALSE(sim.Initialise(t, MakeParams(), Reg()));
    EXPECT_FALSE(sim.IsInitialised());
}

TEST(WeatherInitialise, InconsistentTerrainIsRejected)
{
    WeatherTerrain t = MakeTerrain();
    t.biome.Remove();
    WeatherSimulation sim;
    EXPECT_FALSE(sim.Initialise(t, MakeParams(), Reg()));
    EXPECT_FALSE(sim.IsInitialised());
}

TEST(WeatherInitialise, ZeroCellTerrainIsRejected)
{
    WeatherSimulation sim;
    EXPECT_FALSE(sim.Initialise(WeatherTerrain(), MakeParams(), Reg()));
    EXPECT_FALSE(sim.IsInitialised());
}

TEST(WeatherInitialise, BaselineInputOfWrongLengthIsRejected)
{
    SeasonalBaselineInput in;
    in.temperature.Reserve(23);
    for (uint32_t i = 0; i < 23; ++i) in.temperature.Add(0.5f);   // 6 x 4 = 24 cells
    WeatherSimulation sim;
    EXPECT_FALSE(sim.Initialise(MakeTerrain(), MakeParams(), Reg(), &in));
    EXPECT_FALSE(sim.IsInitialised());
}

TEST(WeatherInitialise, FailedInitialiseKeepsThePreviousWorld)
{
    WeatherSimulation sim;
    ASSERT_TRUE(sim.Initialise(MakeTerrain(), MakeParams(), Reg()));

    WeatherSimulation reference;
    ASSERT_TRUE(reference.Initialise(MakeTerrain(), MakeParams(), Reg()));

    WeatherTerrain bad = MakeTerrain(3, 3);
    for (uint32_t i = 0; i < bad.waterSource.Size(); ++i) bad.waterSource[i] = WaterSource::None;
    EXPECT_FALSE(sim.Initialise(bad, MakeParams(), Reg()));

    EXPECT_TRUE(sim.IsInitialised());
    EXPECT_EQ(sim.Terrain().width, 6u);
    EXPECT_EQ(sim.Terrain().height, 4u);
    EXPECT_TRUE(SameFields(sim.Fields(), reference.Fields()));
}

// ---- Seeding (Task 4) -------------------------------------------------------------------------------------------

TEST(WeatherInitialise, SeedsSpringBaselineTimesLatitudeModifier)
{
    const WeatherTerrain t = MakeTerrain();
    WeatherSimulation sim;
    ASSERT_TRUE(sim.Initialise(t, MakeParams(), Reg()));
    ASSERT_TRUE(sim.IsInitialised());
    const WeatherFields& f = sim.Fields();
    ASSERT_EQ(f.CellCount(), 24u);
    for (uint32_t i = 0; i < 24; ++i)
    {
        const float base = TableValue(0, t.biome[i]);   // Season::Spring is 0
        EXPECT_FLOAT_EQ(f.temperature[i], Clamp01(base * (1.0f - kCooling * t.latitude[i]))) << "cell " << i;
    }
}

TEST(WeatherInitialise, SeedsMoistureByWaterSourceAndPressureHalf)
{
    const WeatherTerrain t = MakeTerrain();
    WeatherSimulation sim;
    ASSERT_TRUE(sim.Initialise(t, MakeParams(), Reg()));
    const WeatherFields& f = sim.Fields();
    ASSERT_EQ(f.moisture.Size(), 24u);
    ASSERT_EQ(f.pressure.Size(), 24u);
    for (uint32_t i = 0; i < 24; ++i)
    {
        const bool water = (i == 0 || i == 1 || i == 5);
        EXPECT_FLOAT_EQ(f.moisture[i], water ? kWaterMois : kLandMois) << "cell " << i;
        EXPECT_FLOAT_EQ(f.pressure[i], 0.5f);
    }
}

TEST(WeatherInitialise, EveryOtherFieldStartsAtZero)
{
    WeatherSimulation sim;
    ASSERT_TRUE(sim.Initialise(MakeTerrain(), MakeParams(), Reg()));
    const WeatherFields& f = sim.Fields();
    ASSERT_EQ(f.pressureAnomaly.Size(), 24u);
    ASSERT_EQ(f.windDirection.Size(), 24u);
    for (uint32_t i = 0; i < 24; ++i)
    {
        EXPECT_EQ(f.windDirection[i], 0.0f);
        EXPECT_EQ(f.windSpeed[i], 0.0f);
        EXPECT_EQ(f.precipitation[i], 0.0f);
        EXPECT_EQ(f.cloudCover[i], 0.0f);
        EXPECT_EQ(f.waterToGround[i], 0.0f);
        EXPECT_EQ(f.snowPack[i], 0.0f);
        EXPECT_EQ(f.pressureAnomaly[i], 0.0f);
    }
}

TEST(WeatherInitialise, BaselineInputWinsOverTheTable)
{
    SeasonalBaselineInput in;
    in.temperature.Reserve(24);
    for (uint32_t i = 0; i < 24; ++i) in.temperature.Add(0.9f);
    const WeatherTerrain t = MakeTerrain();
    WeatherSimulation sim;
    ASSERT_TRUE(sim.Initialise(t, MakeParams(), Reg(), &in));
    for (uint32_t i = 0; i < 24; ++i)
        EXPECT_FLOAT_EQ(sim.Fields().temperature[i], Clamp01(0.9f * (1.0f - kCooling * t.latitude[i])));
}

TEST(WeatherInitialise, UnknownBiomeTakesTheFallbackBiomeValue)
{
    const BiomeRegistry& reg = Reg();
    const BiomeIndex fallback = reg.Role(BiomeRole::Fallback);
    ASSERT_NE(fallback, kNoBiome);
    ASSERT_LT(reg.Count(), 250u);

    WeatherTerrain t = MakeTerrain();
    t.biome[2] = BiomeIndex(reg.Count());   // one past the last biome
    t.biome[3] = BiomeIndex(250);           // far out of range, not kNoBiome
    WeatherSimulation sim;
    ASSERT_TRUE(sim.Initialise(t, MakeParams(), reg));
    const float expected2 = Clamp01(TableValue(0, fallback) * (1.0f - kCooling * t.latitude[2]));
    const float expected3 = Clamp01(TableValue(0, fallback) * (1.0f - kCooling * t.latitude[3]));
    EXPECT_FLOAT_EQ(sim.Fields().temperature[2], expected2);
    EXPECT_FLOAT_EQ(sim.Fields().temperature[3], expected3);
}

TEST(WeatherInitialise, NoBiomeSeedsHalfBaselineNeverReadsOutOfBounds)
{
    WeatherTerrain t = MakeTerrain();
    t.biome[4] = kNoBiome;
    WeatherSimulation sim;
    ASSERT_TRUE(sim.Initialise(t, MakeParams(), Reg()));
    EXPECT_FLOAT_EQ(sim.Fields().temperature[4], Clamp01(0.5f * (1.0f - kCooling * t.latitude[4])));
}

TEST(WeatherInitialise, ExtraBiomeInTheRegistryIsServedWithNoCodeChange)
{
    const BiomeRegistry& extra = T::ExtraBiomeRegistry();
    ASSERT_GT(extra.Count(), Reg().Count());
    WeatherTerrain t = MakeTerrain(6, 4, extra.Count());
    const BiomeIndex last = BiomeIndex(extra.Count() - 1);
    t.biome[7] = last;
    WeatherSimulation sim;
    ASSERT_TRUE(sim.Initialise(t, MakeParams(), extra));
    EXPECT_FLOAT_EQ(sim.Fields().temperature[7], Clamp01(TableValue(0, last) * (1.0f - kCooling * t.latitude[7])));
}

TEST(WeatherInitialise, ReinitialiseOnADifferentSizeEqualsAFreshSimulation)
{
    WeatherSimulation reused;
    ASSERT_TRUE(reused.Initialise(MakeTerrain(6, 4), MakeParams(), Reg()));
    ASSERT_TRUE(reused.Initialise(MakeTerrain(3, 2), MakeParams(), Reg()));

    WeatherSimulation fresh;
    ASSERT_TRUE(fresh.Initialise(MakeTerrain(3, 2), MakeParams(), Reg()));

    EXPECT_TRUE(SameFields(reused.Fields(), fresh.Fields()));
    EXPECT_EQ(reused.Terrain().width, fresh.Terrain().width);
    EXPECT_EQ(reused.Terrain().height, fresh.Terrain().height);
    EXPECT_EQ(reused.Fields().CellCount(), 6u);
}

TEST(WeatherInitialise, SameTerrainTwiceIsBitIdentical)
{
    WeatherSimulation a, b;
    ASSERT_TRUE(a.Initialise(MakeTerrain(), MakeParams(), Reg()));
    ASSERT_TRUE(b.Initialise(MakeTerrain(), MakeParams(), Reg()));
    EXPECT_TRUE(SameFields(a.Fields(), b.Fields()));
}

TEST(WeatherInitialise, CallersTerrainIsNotRetained)
{
    WeatherTerrain t = MakeTerrain();
    WeatherSimulation sim;
    ASSERT_TRUE(sim.Initialise(t, MakeParams(), Reg()));
    ASSERT_EQ(sim.Terrain().latitude.Size(), 24u);
    const float before = sim.Terrain().latitude[5];
    t.latitude[5] = 0.99f;
    t.waterSource[5] = WaterSource::None;
    EXPECT_FLOAT_EQ(sim.Terrain().latitude[5], before);
    EXPECT_EQ(sim.Terrain().waterSource[5], WaterSource::Lake);
}

TEST(WeatherInitialise, OneByOneWorldWithAWaterSourceInitialises)
{
    WeatherTerrain t = MakeTerrain(1, 1);
    t.waterSource[0] = WaterSource::Ocean;
    WeatherSimulation sim;
    ASSERT_TRUE(sim.Initialise(t, MakeParams(), Reg()));
    EXPECT_FLOAT_EQ(sim.Fields().moisture[0], kWaterMois);
}

TEST(WeatherInitialise, EveryCellAWaterSourceInitialises)
{
    WeatherTerrain t = MakeTerrain();
    for (uint32_t i = 0; i < t.waterSource.Size(); ++i) t.waterSource[i] = WaterSource::Ocean;
    WeatherSimulation sim;
    ASSERT_TRUE(sim.Initialise(t, MakeParams(), Reg()));
    for (uint32_t i = 0; i < 24; ++i) EXPECT_FLOAT_EQ(sim.Fields().moisture[i], kWaterMois);
}

TEST(WeatherInitialise, WorldWithOnlyWaterSourceAndBiomeGivesValidState)
{
    // AC 16: there is no input for S-1 climate; elevation and latitude are the only other terrain inputs.
    WeatherTerrain t = MakeTerrain();
    WeatherSimulation sim;
    ASSERT_TRUE(sim.Initialise(t, MakeParams(), Reg()));
    for (uint32_t i = 0; i < 24; ++i)
    {
        EXPECT_GE(sim.Fields().temperature[i], 0.0f);
        EXPECT_LE(sim.Fields().temperature[i], 1.0f);
    }
}

// ---- Gap-audit edges (Task 9) -----------------------------------------------------------------------------------

TEST(WeatherInitialise, BaselineRowSelectsEachSeasonsOwnColumn)
{
    WeatherParameters p = MakeParams();
    EXPECT_EQ(p.BaselineRow(Season::Spring), p.baselineSpring);
    EXPECT_EQ(p.BaselineRow(Season::Summer), p.baselineSummer);
    EXPECT_EQ(p.BaselineRow(Season::Autumn), p.baselineAutumn);
    EXPECT_EQ(p.BaselineRow(Season::Winter), p.baselineWinter);
    const WeatherParameters& cp = p;
    EXPECT_EQ(cp.BaselineRow(Season::Autumn), p.baselineAutumn);
}

TEST(WeatherInitialise, LargeMapInitialisesAndIsDeterministic)
{
    // S-1 Large is 104 x 64 = 6,656 cells.
    WeatherTerrain t = MakeTerrain(104, 64);
    WeatherSimulation a, b;
    ASSERT_TRUE(a.Initialise(t, MakeParams(), Reg()));
    ASSERT_TRUE(b.Initialise(t, MakeParams(), Reg()));
    EXPECT_EQ(a.Fields().CellCount(), 6656u);
    EXPECT_TRUE(SameFields(a.Fields(), b.Fields()));
    EXPECT_FLOAT_EQ(a.Fields().moisture[0], kWaterMois);
    EXPECT_FLOAT_EQ(a.Fields().moisture[6655], kLandMois);
}

TEST(WeatherInitialise, GrowingThenShrinkingThenGrowingAgainStaysEqualToFresh)
{
    WeatherSimulation reused;
    ASSERT_TRUE(reused.Initialise(MakeTerrain(3, 2), MakeParams(), Reg()));
    ASSERT_TRUE(reused.Initialise(MakeTerrain(104, 64), MakeParams(), Reg()));
    ASSERT_TRUE(reused.Initialise(MakeTerrain(6, 4), MakeParams(), Reg()));
    WeatherSimulation fresh;
    ASSERT_TRUE(fresh.Initialise(MakeTerrain(6, 4), MakeParams(), Reg()));
    EXPECT_TRUE(SameFields(reused.Fields(), fresh.Fields()));
    EXPECT_EQ(reused.Terrain().biome.Size(), 24u);
}

TEST(WeatherInitialise, BaselineInputOfWrongLengthLeavesThePreviousWorldAlone)
{
    WeatherSimulation sim;
    ASSERT_TRUE(sim.Initialise(MakeTerrain(), MakeParams(), Reg()));
    WeatherSimulation reference;
    ASSERT_TRUE(reference.Initialise(MakeTerrain(), MakeParams(), Reg()));
    SeasonalBaselineInput in;
    in.temperature.Reserve(5);
    for (uint32_t i = 0; i < 5; ++i) in.temperature.Add(0.5f);
    EXPECT_FALSE(sim.Initialise(MakeTerrain(), MakeParams(), Reg(), &in));
    EXPECT_TRUE(SameFields(sim.Fields(), reference.Fields()));
}

TEST(WeatherInitialise, BaselineInputIgnoresTheBiomeArray)
{
    SeasonalBaselineInput in;
    in.temperature.Reserve(24);
    for (uint32_t i = 0; i < 24; ++i) in.temperature.Add(0.7f);
    WeatherTerrain t = MakeTerrain();
    t.biome[0] = kNoBiome;
    t.biome[1] = BiomeIndex(250);
    WeatherSimulation sim;
    ASSERT_TRUE(sim.Initialise(t, MakeParams(), Reg(), &in));
    EXPECT_FLOAT_EQ(sim.Fields().temperature[0], Clamp01(0.7f * (1.0f - kCooling * t.latitude[0])));
    EXPECT_FLOAT_EQ(sim.Fields().temperature[1], Clamp01(0.7f * (1.0f - kCooling * t.latitude[1])));
}
