// A4 Weather Feature 1 (weather-data.plan.md Task 2): weather data types.
#include <gtest/gtest.h>

#include <DraoiHexWorldWeather/WeatherTypes.h>

namespace
{
    using namespace Draoi::HexWorld;
    using namespace Draoi::HexWorld::Weather;

    template <class T>
    void FillAllocated(Dia::Core::Containers::DynamicArray<T>& a, T v)
    {
        for (uint32_t i = 0; i < a.Size(); ++i) a[i] = v;
    }

    WeatherTerrain MakeTerrain(uint32_t w, uint32_t h)
    {
        WeatherTerrain t;
        t.width = w;
        t.height = h;
        const uint32_t n = w * h;
        t.elevation.Reserve(n + 2);   // DynamicArray does not grow on Add; the "one long" tests add one more
        t.latitude.Reserve(n + 2);
        t.waterSource.Reserve(n + 2);
        t.biome.Reserve(n + 2);
        for (uint32_t i = 0; i < n; ++i)
        {
            t.elevation.Add(0.5f);
            t.latitude.Add(0.25f);
            t.waterSource.Add(WaterSource::None);
            t.biome.Add(BiomeIndex(0));
        }
        return t;
    }
}

TEST(WeatherTypes, SeasonOrderAndCount)
{
    EXPECT_EQ(kSeasonCount, 4u);
    EXPECT_EQ(static_cast<uint8_t>(Season::Spring), 0);
    EXPECT_EQ(static_cast<uint8_t>(Season::Winter), 3);
}

TEST(WeatherTypes, FieldsResizeSizesAllTenArraysZeroFilled)
{
    WeatherFields f;
    f.Resize(24);
    const Dia::Core::Containers::DynamicArray<float>* all[] = {
        &f.moisture, &f.temperature, &f.pressure, &f.windDirection, &f.windSpeed,
        &f.precipitation, &f.cloudCover, &f.waterToGround, &f.snowPack, &f.pressureAnomaly };
    ASSERT_EQ(sizeof(all) / sizeof(all[0]), 10u);
    for (const auto* a : all)
    {
        ASSERT_EQ(a->Size(), 24u);
        for (uint32_t i = 0; i < 24; ++i) EXPECT_EQ((*a)[i], 0.0f);
    }
    EXPECT_EQ(f.CellCount(), 24u);
}

TEST(WeatherTypes, FieldsSmallerSecondResizeLeavesNoStaleValues)
{
    WeatherFields f;
    f.Resize(24);
    FillAllocated(f.moisture, 9.0f);
    FillAllocated(f.pressureAnomaly, -0.4f);
    f.Resize(6);
    EXPECT_EQ(f.moisture.Size(), 6u);
    EXPECT_EQ(f.pressureAnomaly.Size(), 6u);
    for (uint32_t i = 0; i < 6; ++i)
    {
        EXPECT_EQ(f.moisture[i], 0.0f);
        EXPECT_EQ(f.pressureAnomaly[i], 0.0f);
    }
    f.Resize(24);   // and growing again is zero too
    for (uint32_t i = 0; i < 24; ++i) EXPECT_EQ(f.moisture[i], 0.0f);
}

TEST(WeatherTypes, FieldsResizeZeroEmptiesEverything)
{
    WeatherFields f;
    f.Resize(5);
    f.Resize(0);
    EXPECT_EQ(f.CellCount(), 0u);
    EXPECT_EQ(f.snowPack.Size(), 0u);
}

TEST(WeatherTypes, HandoffResizeSizesBothArraysZeroFilled)
{
    WeatherHandoff h;
    h.Resize(10);
    FillAllocated(h.glacierInput, 1.5f);
    FillAllocated(h.influenceWater, 1.5f);
    h.Resize(4);
    ASSERT_EQ(h.glacierInput.Size(), 4u);
    ASSERT_EQ(h.influenceWater.Size(), 4u);
    for (uint32_t i = 0; i < 4; ++i)
    {
        EXPECT_EQ(h.glacierInput[i], 0.0f);
        EXPECT_EQ(h.influenceWater[i], 0.0f);
    }
}

TEST(WeatherTypes, HydrologyInputsResizeAndDefaults)
{
    HydrologyInputs h;
    EXPECT_FALSE(h.present);
    EXPECT_EQ(h.glacierCheckWeek, 26u);
    EXPECT_FLOAT_EQ(h.glacierSnowFloor, 0.2f);
    h.Resize(7);
    ASSERT_EQ(h.surfaceWetness.Size(), 7u);
    ASSERT_EQ(h.openWater.Size(), 7u);
    ASSERT_EQ(h.glacierEligible.Size(), 7u);
    for (uint32_t i = 0; i < 7; ++i)
    {
        EXPECT_EQ(h.surfaceWetness[i], 0.0f);
        EXPECT_EQ(h.glacierEligible[i], 0);
    }
}

TEST(WeatherTypes, TerrainConsistentWhenAllArraysMatch)
{
    EXPECT_TRUE(MakeTerrain(6, 4).IsConsistent());
}

TEST(WeatherTypes, TerrainInconsistentWhenAnyArrayIsOneShort)
{
    { WeatherTerrain t = MakeTerrain(6, 4); t.elevation.Remove();   EXPECT_FALSE(t.IsConsistent()); }
    { WeatherTerrain t = MakeTerrain(6, 4); t.latitude.Remove();    EXPECT_FALSE(t.IsConsistent()); }
    { WeatherTerrain t = MakeTerrain(6, 4); t.waterSource.Remove(); EXPECT_FALSE(t.IsConsistent()); }
    { WeatherTerrain t = MakeTerrain(6, 4); t.biome.Remove();       EXPECT_FALSE(t.IsConsistent()); }
}

TEST(WeatherTypes, TerrainInconsistentWhenAnyArrayIsOneLong)
{
    { WeatherTerrain t = MakeTerrain(6, 4); t.elevation.Add(0.0f);        EXPECT_FALSE(t.IsConsistent()); }
    { WeatherTerrain t = MakeTerrain(6, 4); t.biome.Add(BiomeIndex(1));   EXPECT_FALSE(t.IsConsistent()); }
}

TEST(WeatherTypes, TerrainInconsistentWhenDimensionsDisagree)
{
    WeatherTerrain t = MakeTerrain(6, 4);
    t.width = 5;
    EXPECT_FALSE(t.IsConsistent());
}

TEST(WeatherTypes, TerrainWithZeroCellsIsInconsistent)
{
    WeatherTerrain t;   // width = height = 0, all arrays empty
    EXPECT_FALSE(t.IsConsistent());
    t.width = 4;        // arrays still empty, 4 * 0 == 0
    EXPECT_FALSE(t.IsConsistent());
}

TEST(WeatherTypes, TerrainCopyIsIndependent)
{
    WeatherTerrain a = MakeTerrain(3, 2);
    WeatherTerrain b = a;
    b.elevation[0] = 0.9f;
    EXPECT_FLOAT_EQ(a.elevation[0], 0.5f);
    EXPECT_TRUE(b.IsConsistent());
}
