// S-1 Biome Classification task 1: biome vocabulary, zone validity, forbidden pairs.
#include <gtest/gtest.h>
#include <Shared/WorldGeneration/BiomeTypes.h>
#include <set>
#include <string>
#include <type_traits>
#include <utility>

using namespace CoW::WorldGeneration;

static_assert(std::is_trivially_copyable_v<Cell>, "Cell is memset/memcpy'd; must stay trivially copyable");
static_assert(sizeof(Cell) <= 24, "Cell grew unexpectedly");
static_assert(kBiomeCount == 10, "ten biomes");

namespace
{
    Biome B(int i) { return static_cast<Biome>(i); }

    bool ExpectedValid(Biome b, DepthZone z)
    {
        switch (b)
        {
        case Biome::Ocean:
        case Biome::Lake:
            return z == DepthZone::None;
        case Biome::GrasslandSteppe:
        case Biome::TemperateForest:
        case Biome::WetlandDelta:
            return z == DepthZone::FirstShores || z == DepthZone::WildReaches;
        default:
            return z == DepthZone::WildReaches || z == DepthZone::AncientInteriors;
        }
    }

    bool ExpectedForbidden(Biome a, Biome b)
    {
        static const std::pair<Biome, Biome> kPairs[] = {
            { Biome::VolcanicAshland, Biome::WetlandDelta },
            { Biome::VolcanicAshland, Biome::Rainforest },
            { Biome::VolcanicAshland, Biome::TundraBoreal },
            { Biome::Rainforest,      Biome::DesertBasin },
            { Biome::Rainforest,      Biome::TundraBoreal },
            { Biome::TundraBoreal,    Biome::WetlandDelta },
            { Biome::DesertBasin,     Biome::TemperateForest },
        };
        for (const auto& p : kPairs)
        {
            if ((p.first == a && p.second == b) || (p.first == b && p.second == a))
            {
                return true;
            }
        }
        return false;
    }
}

TEST(BiomeTypes, IdsAreTenDistinct)
{
    std::set<uint32_t> ids;
    for (int i = 0; i < kBiomeCount; ++i)
    {
        ids.insert(static_cast<uint32_t>(BiomeId(B(i)).Value()));
    }
    EXPECT_EQ(static_cast<int>(ids.size()), 10);
    EXPECT_EQ(BiomeId(Biome::Ocean), Dia::Core::StringCRC("Biome.Ocean"));
    EXPECT_EQ(BiomeId(Biome::Lake), Dia::Core::StringCRC("Biome.Lake"));
    EXPECT_EQ(BiomeId(Biome::GrasslandSteppe), Dia::Core::StringCRC("Biome.Grassland"));
    EXPECT_EQ(BiomeId(Biome::TemperateForest), Dia::Core::StringCRC("Biome.TemperateForest"));
    EXPECT_EQ(BiomeId(Biome::WetlandDelta), Dia::Core::StringCRC("Biome.Wetland"));
    EXPECT_EQ(BiomeId(Biome::DesertBasin), Dia::Core::StringCRC("Biome.Desert"));
    EXPECT_EQ(BiomeId(Biome::AlpineMountain), Dia::Core::StringCRC("Biome.Alpine"));
    EXPECT_EQ(BiomeId(Biome::TundraBoreal), Dia::Core::StringCRC("Biome.Tundra"));
    EXPECT_EQ(BiomeId(Biome::Rainforest), Dia::Core::StringCRC("Biome.Rainforest"));
    EXPECT_EQ(BiomeId(Biome::VolcanicAshland), Dia::Core::StringCRC("Biome.Volcanic"));
}

TEST(BiomeTypes, FromIdRoundTrips)
{
    for (int i = 0; i < kBiomeCount; ++i)
    {
        Biome out = Biome::Count;
        EXPECT_TRUE(BiomeFromId(BiomeId(B(i)), out)) << i;
        EXPECT_EQ(out, B(i));
    }
}

TEST(BiomeTypes, FromIdRejectsUnknownAndLeavesOutUnchanged)
{
    Biome out = Biome::Rainforest;
    EXPECT_FALSE(BiomeFromId(Dia::Core::StringCRC("Biome.Nonsense"), out));
    EXPECT_EQ(out, Biome::Rainforest);
    EXPECT_FALSE(BiomeFromId(TerrainTypeId(TerrainType::Mountain), out));
    EXPECT_EQ(out, Biome::Rainforest);
}

TEST(BiomeTypes, NamesAreNonEmptyAndExact)
{
    std::set<std::string> names;
    for (int i = 0; i < kBiomeCount; ++i)
    {
        const char* n = BiomeName(B(i));
        ASSERT_NE(n, nullptr);
        EXPECT_NE(std::string(n), "");
        EXPECT_NE(std::string(n), "Unknown");
        names.insert(n);
    }
    EXPECT_EQ(static_cast<int>(names.size()), 10);
    EXPECT_STREQ(BiomeName(Biome::Ocean), "Ocean");
    EXPECT_STREQ(BiomeName(Biome::Lake), "Lake");
    EXPECT_STREQ(BiomeName(Biome::GrasslandSteppe), "Grassland / Steppe");
    EXPECT_STREQ(BiomeName(Biome::TemperateForest), "Temperate Forest");
    EXPECT_STREQ(BiomeName(Biome::WetlandDelta), "Wetland / Delta");
    EXPECT_STREQ(BiomeName(Biome::DesertBasin), "Desert Basin");
    EXPECT_STREQ(BiomeName(Biome::AlpineMountain), "Alpine Mountain");
    EXPECT_STREQ(BiomeName(Biome::TundraBoreal), "Tundra / Boreal");
    EXPECT_STREQ(BiomeName(Biome::Rainforest), "Rainforest");
    EXPECT_STREQ(BiomeName(Biome::VolcanicAshland), "Volcanic Ashland");
    EXPECT_STREQ(BiomeName(Biome::Count), "Unknown");
}

TEST(BiomeTypes, ZoneValidityTable)
{
    const DepthZone zones[] = { DepthZone::None, DepthZone::FirstShores, DepthZone::WildReaches, DepthZone::AncientInteriors };
    for (int i = 0; i < kBiomeCount; ++i)
    {
        for (DepthZone z : zones)
        {
            EXPECT_EQ(IsBiomeValidInZone(B(i), z), ExpectedValid(B(i), z))
                << BiomeName(B(i)) << " in " << DepthZoneName(z);
        }
    }
}

TEST(BiomeTypes, ForbiddenPairsMatchExpectedSet)
{
    int forbiddenOrdered = 0;
    for (int i = 0; i < kBiomeCount; ++i)
    {
        for (int j = 0; j < kBiomeCount; ++j)
        {
            const bool f = AreBiomesForbidden(B(i), B(j));
            EXPECT_EQ(f, ExpectedForbidden(B(i), B(j))) << BiomeName(B(i)) << " / " << BiomeName(B(j));
            EXPECT_EQ(f, AreBiomesForbidden(B(j), B(i)));
            forbiddenOrdered += f ? 1 : 0;
        }
        EXPECT_FALSE(AreBiomesForbidden(B(i), B(i)));
    }
    EXPECT_EQ(forbiddenOrdered, 14);
}

TEST(BiomeTypes, AlpineOceanLakeNeverForbidden)
{
    for (int i = 0; i < kBiomeCount; ++i)
    {
        EXPECT_FALSE(AreBiomesForbidden(Biome::AlpineMountain, B(i)));
        EXPECT_FALSE(AreBiomesForbidden(Biome::Ocean, B(i)));
        EXPECT_FALSE(AreBiomesForbidden(Biome::Lake, B(i)));
    }
}

TEST(BiomeTypes, CellDefaultsBiomeToOcean)
{
    Cell c;
    EXPECT_EQ(c.biome, Biome::Ocean);
}
