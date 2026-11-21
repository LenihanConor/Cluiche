// S-1 Biome Classification task 1: biome vocabulary, zone validity, forbidden pairs.
#include <gtest/gtest.h>
#include <Shared/WorldGeneration/BiomeTypes.h>
#include <Shared/World/Testing/BiomeTestRegistry.h>
#include <set>
#include <string>
#include <type_traits>
#include <utility>

using namespace CoW::WorldGeneration;
using namespace CoW::World::Testing;

static_assert(std::is_trivially_copyable_v<Cell>, "Cell is memset/memcpy'd; must stay trivially copyable");
static_assert(sizeof(Cell) <= 24, "Cell grew unexpectedly");
static_assert(kBiomeCount == 13, "ten generated biomes plus three succession biomes");
static_assert(kGeneratedBiomeCount == 10, "the generator emits only the first ten");

namespace
{
    Biome E(int i) { return static_cast<Biome>(i); }
    const CoW::World::BiomeRegistry& Reg() { return ShippedBiomeRegistry(); }

    bool ExpectedValid(Biome b, DepthZone z)
    {
        if (!IsGeneratedBiome(b))
        {
            return false;   // succession biomes are classification predicates: never valid in a zone
        }
        const CoW::World::BiomeIndex i = Idx(b);
        if (i == B("Biome.Ocean") || i == B("Biome.Lake"))
        {
            return z == DepthZone::None;
        }
        if (i == B("Biome.Grassland") || i == B("Biome.TemperateForest") || i == B("Biome.Wetland"))
        {
            return z == DepthZone::FirstShores || z == DepthZone::WildReaches;
        }
        return z == DepthZone::WildReaches || z == DepthZone::AncientInteriors;
    }

    bool ExpectedForbidden(Biome a, Biome b)
    {
        static const std::pair<CoW::World::BiomeIndex, CoW::World::BiomeIndex> kPairs[] = {
            { B("Biome.Volcanic"),   B("Biome.Wetland") },
            { B("Biome.Volcanic"),   B("Biome.Rainforest") },
            { B("Biome.Volcanic"),   B("Biome.Tundra") },
            { B("Biome.Rainforest"), B("Biome.Desert") },
            { B("Biome.Rainforest"), B("Biome.Tundra") },
            { B("Biome.Tundra"),     B("Biome.Wetland") },
            { B("Biome.Desert"),     B("Biome.TemperateForest") },
        };
        for (const auto& p : kPairs)
        {
            if ((p.first == Idx(a) && p.second == Idx(b)) || (p.first == Idx(b) && p.second == Idx(a)))
            {
                return true;
            }
        }
        return false;
    }
}

TEST(BiomeTypes, IdsAreThirteenDistinct)
{
    std::set<uint32_t> ids;
    for (int i = 0; i < kBiomeCount; ++i)
    {
        ids.insert(static_cast<uint32_t>(BiomeId(E(i)).Value()));
    }
    EXPECT_EQ(static_cast<int>(ids.size()), 13);
    EXPECT_EQ(BiomeId(LegacyEnumB("Biome.Ocean")), Dia::Core::StringCRC("Biome.Ocean"));
    EXPECT_EQ(Reg().Find(Dia::Core::StringCRC("Biome.Ocean")), B("Biome.Ocean"));
    EXPECT_STREQ(Reg().IdText(B("Biome.Ocean")), "Biome.Ocean");
    EXPECT_EQ(BiomeId(LegacyEnumB("Biome.Lake")), Dia::Core::StringCRC("Biome.Lake"));
    EXPECT_EQ(Reg().Find(Dia::Core::StringCRC("Biome.Lake")), B("Biome.Lake"));
    EXPECT_STREQ(Reg().IdText(B("Biome.Lake")), "Biome.Lake");
    EXPECT_EQ(BiomeId(LegacyEnumB("Biome.Grassland")), Dia::Core::StringCRC("Biome.Grassland"));
    EXPECT_EQ(Reg().Find(Dia::Core::StringCRC("Biome.Grassland")), B("Biome.Grassland"));
    EXPECT_STREQ(Reg().IdText(B("Biome.Grassland")), "Biome.Grassland");
    EXPECT_EQ(BiomeId(LegacyEnumB("Biome.TemperateForest")), Dia::Core::StringCRC("Biome.TemperateForest"));
    EXPECT_EQ(Reg().Find(Dia::Core::StringCRC("Biome.TemperateForest")), B("Biome.TemperateForest"));
    EXPECT_STREQ(Reg().IdText(B("Biome.TemperateForest")), "Biome.TemperateForest");
    EXPECT_EQ(BiomeId(LegacyEnumB("Biome.Wetland")), Dia::Core::StringCRC("Biome.Wetland"));
    EXPECT_EQ(Reg().Find(Dia::Core::StringCRC("Biome.Wetland")), B("Biome.Wetland"));
    EXPECT_STREQ(Reg().IdText(B("Biome.Wetland")), "Biome.Wetland");
    EXPECT_EQ(BiomeId(LegacyEnumB("Biome.Desert")), Dia::Core::StringCRC("Biome.Desert"));
    EXPECT_EQ(Reg().Find(Dia::Core::StringCRC("Biome.Desert")), B("Biome.Desert"));
    EXPECT_STREQ(Reg().IdText(B("Biome.Desert")), "Biome.Desert");
    EXPECT_EQ(BiomeId(LegacyEnumB("Biome.Alpine")), Dia::Core::StringCRC("Biome.Alpine"));
    EXPECT_EQ(Reg().Find(Dia::Core::StringCRC("Biome.Alpine")), B("Biome.Alpine"));
    EXPECT_STREQ(Reg().IdText(B("Biome.Alpine")), "Biome.Alpine");
    EXPECT_EQ(BiomeId(LegacyEnumB("Biome.Tundra")), Dia::Core::StringCRC("Biome.Tundra"));
    EXPECT_EQ(Reg().Find(Dia::Core::StringCRC("Biome.Tundra")), B("Biome.Tundra"));
    EXPECT_STREQ(Reg().IdText(B("Biome.Tundra")), "Biome.Tundra");
    EXPECT_EQ(BiomeId(LegacyEnumB("Biome.Rainforest")), Dia::Core::StringCRC("Biome.Rainforest"));
    EXPECT_EQ(Reg().Find(Dia::Core::StringCRC("Biome.Rainforest")), B("Biome.Rainforest"));
    EXPECT_STREQ(Reg().IdText(B("Biome.Rainforest")), "Biome.Rainforest");
    EXPECT_EQ(BiomeId(LegacyEnumB("Biome.Volcanic")), Dia::Core::StringCRC("Biome.Volcanic"));
    EXPECT_EQ(Reg().Find(Dia::Core::StringCRC("Biome.Volcanic")), B("Biome.Volcanic"));
    EXPECT_STREQ(Reg().IdText(B("Biome.Volcanic")), "Biome.Volcanic");
    EXPECT_EQ(BiomeId(LegacyEnumB("Biome.Scrub")), Dia::Core::StringCRC("Biome.Scrub"));
    EXPECT_EQ(Reg().Find(Dia::Core::StringCRC("Biome.Scrub")), B("Biome.Scrub"));
    EXPECT_STREQ(Reg().IdText(B("Biome.Scrub")), "Biome.Scrub");
    EXPECT_EQ(BiomeId(LegacyEnumB("Biome.BurnedAshland")), Dia::Core::StringCRC("Biome.BurnedAshland"));
    EXPECT_EQ(Reg().Find(Dia::Core::StringCRC("Biome.BurnedAshland")), B("Biome.BurnedAshland"));
    EXPECT_STREQ(Reg().IdText(B("Biome.BurnedAshland")), "Biome.BurnedAshland");
    EXPECT_EQ(BiomeId(LegacyEnumB("Biome.DesertifiedPlain")), Dia::Core::StringCRC("Biome.DesertifiedPlain"));
    EXPECT_EQ(Reg().Find(Dia::Core::StringCRC("Biome.DesertifiedPlain")), B("Biome.DesertifiedPlain"));
    EXPECT_STREQ(Reg().IdText(B("Biome.DesertifiedPlain")), "Biome.DesertifiedPlain");
}

// Tripwire: the committed S-1 layer goldens hash biomeIndex values, so the first ten must never renumber.
TEST(BiomeTypes, GeneratedBiomeNumericValuesArePinned)
{
    EXPECT_EQ(static_cast<int>(B("Biome.Ocean")), 0);
    EXPECT_EQ(static_cast<int>(B("Biome.Lake")), 1);
    EXPECT_EQ(static_cast<int>(B("Biome.Grassland")), 2);
    EXPECT_EQ(static_cast<int>(B("Biome.TemperateForest")), 3);
    EXPECT_EQ(static_cast<int>(B("Biome.Wetland")), 4);
    EXPECT_EQ(static_cast<int>(B("Biome.Desert")), 5);
    EXPECT_EQ(static_cast<int>(B("Biome.Alpine")), 6);
    EXPECT_EQ(static_cast<int>(B("Biome.Tundra")), 7);
    EXPECT_EQ(static_cast<int>(B("Biome.Rainforest")), 8);
    EXPECT_EQ(static_cast<int>(B("Biome.Volcanic")), 9);
    EXPECT_EQ(static_cast<int>(B("Biome.Scrub")), 10);
    EXPECT_EQ(static_cast<int>(B("Biome.BurnedAshland")), 11);
    EXPECT_EQ(static_cast<int>(B("Biome.DesertifiedPlain")), 12);
    EXPECT_EQ(static_cast<int>(Biome::Count), 13);
    EXPECT_EQ(Reg().Count(), 13u);
}

TEST(BiomeTypes, IsGeneratedBiomeIsTrueForExactlyTheFirstTen)
{
    for (int i = 0; i < kBiomeCount; ++i)
    {
        EXPECT_EQ(IsGeneratedBiome(E(i)), i < 10) << i;
    }
    EXPECT_FALSE(IsGeneratedBiome(Biome::Count));
    EXPECT_FALSE(IsGeneratedBiome(E(200)));
}

TEST(BiomeTypes, FromIdRoundTrips)
{
    for (int i = 0; i < kBiomeCount; ++i)
    {
        Biome out = Biome::Count;
        EXPECT_TRUE(BiomeFromId(BiomeId(E(i)), out)) << i;
        EXPECT_EQ(out, E(i));
        EXPECT_EQ(Reg().Find(BiomeId(E(i))), Idx(E(i))) << i;
    }
}

TEST(BiomeTypes, FromIdRejectsUnknownAndLeavesOutUnchanged)
{
    Biome out = LegacyEnumB("Biome.Rainforest");
    EXPECT_FALSE(BiomeFromId(Dia::Core::StringCRC("Biome.Nonsense"), out));
    EXPECT_EQ(Idx(out), B("Biome.Rainforest"));
    EXPECT_FALSE(BiomeFromId(TerrainTypeId(TerrainType::Mountain), out));
    EXPECT_EQ(Idx(out), B("Biome.Rainforest"));
    EXPECT_EQ(Reg().Find(Dia::Core::StringCRC("Biome.Nonsense")), CoW::World::kNoBiome);
    EXPECT_EQ(Reg().Find(TerrainTypeId(TerrainType::Mountain)), CoW::World::kNoBiome);
}

TEST(BiomeTypes, NamesAreNonEmptyAndExact)
{
    std::set<std::string> names;
    for (int i = 0; i < kBiomeCount; ++i)
    {
        const char* n = BiomeName(E(i));
        ASSERT_NE(n, nullptr);
        EXPECT_NE(std::string(n), "");
        EXPECT_NE(std::string(n), "Unknown");
        names.insert(n);
    }
    EXPECT_EQ(static_cast<int>(names.size()), 13);
    EXPECT_STREQ(BiomeName(LegacyEnumB("Biome.Ocean")), "Ocean");
    EXPECT_STREQ(Reg().Name(B("Biome.Ocean")), "Ocean");
    EXPECT_STREQ(BiomeName(LegacyEnumB("Biome.Lake")), "Lake");
    EXPECT_STREQ(Reg().Name(B("Biome.Lake")), "Lake");
    EXPECT_STREQ(BiomeName(LegacyEnumB("Biome.Grassland")), "Grassland / Steppe");
    EXPECT_STREQ(Reg().Name(B("Biome.Grassland")), "Grassland / Steppe");
    EXPECT_STREQ(BiomeName(LegacyEnumB("Biome.TemperateForest")), "Temperate Forest");
    EXPECT_STREQ(Reg().Name(B("Biome.TemperateForest")), "Temperate Forest");
    EXPECT_STREQ(BiomeName(LegacyEnumB("Biome.Wetland")), "Wetland / Delta");
    EXPECT_STREQ(Reg().Name(B("Biome.Wetland")), "Wetland / Delta");
    EXPECT_STREQ(BiomeName(LegacyEnumB("Biome.Desert")), "Desert Basin");
    EXPECT_STREQ(Reg().Name(B("Biome.Desert")), "Desert Basin");
    EXPECT_STREQ(BiomeName(LegacyEnumB("Biome.Alpine")), "Alpine Mountain");
    EXPECT_STREQ(Reg().Name(B("Biome.Alpine")), "Alpine Mountain");
    EXPECT_STREQ(BiomeName(LegacyEnumB("Biome.Tundra")), "Tundra / Boreal");
    EXPECT_STREQ(Reg().Name(B("Biome.Tundra")), "Tundra / Boreal");
    EXPECT_STREQ(BiomeName(LegacyEnumB("Biome.Rainforest")), "Rainforest");
    EXPECT_STREQ(Reg().Name(B("Biome.Rainforest")), "Rainforest");
    EXPECT_STREQ(BiomeName(LegacyEnumB("Biome.Volcanic")), "Volcanic Ashland");
    EXPECT_STREQ(Reg().Name(B("Biome.Volcanic")), "Volcanic Ashland");
    EXPECT_STREQ(BiomeName(LegacyEnumB("Biome.Scrub")), "Scrub");
    EXPECT_STREQ(Reg().Name(B("Biome.Scrub")), "Scrub");
    EXPECT_STREQ(BiomeName(LegacyEnumB("Biome.BurnedAshland")), "Burned Ashland");
    EXPECT_STREQ(Reg().Name(B("Biome.BurnedAshland")), "Burned Ashland");
    EXPECT_STREQ(BiomeName(LegacyEnumB("Biome.DesertifiedPlain")), "Desertified Plain");
    EXPECT_STREQ(Reg().Name(B("Biome.DesertifiedPlain")), "Desertified Plain");
    EXPECT_STREQ(BiomeName(Biome::Count), "Unknown");
}

TEST(BiomeTypes, ZoneValidityTable)
{
    const DepthZone zones[] = { DepthZone::None, DepthZone::FirstShores, DepthZone::WildReaches, DepthZone::AncientInteriors };
    for (int i = 0; i < kBiomeCount; ++i)
    {
        for (DepthZone z : zones)
        {
            EXPECT_EQ(IsBiomeValidInZone(E(i), z), ExpectedValid(E(i), z))
                << BiomeName(E(i)) << " in " << DepthZoneName(z);
            EXPECT_EQ(Reg().IsValidInZone(Idx(E(i)), z), ExpectedValid(E(i), z))
                << Reg().Name(Idx(E(i))) << " in " << DepthZoneName(z);
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
            const bool f = AreBiomesForbidden(E(i), E(j));
            EXPECT_EQ(f, ExpectedForbidden(E(i), E(j))) << BiomeName(E(i)) << " / " << BiomeName(E(j));
            EXPECT_EQ(f, AreBiomesForbidden(E(j), E(i)));
            EXPECT_EQ(Reg().AreForbidden(Idx(E(i)), Idx(E(j))), ExpectedForbidden(E(i), E(j)));
            forbiddenOrdered += f ? 1 : 0;
        }
        EXPECT_FALSE(AreBiomesForbidden(E(i), E(i)));
    }
    EXPECT_EQ(forbiddenOrdered, 14);
}

TEST(BiomeTypes, AlpineOceanLakeNeverForbidden)
{
    for (int i = 0; i < kBiomeCount; ++i)
    {
        EXPECT_FALSE(AreBiomesForbidden(LegacyEnumB("Biome.Alpine"), E(i)));
        EXPECT_FALSE(Reg().AreForbidden(B("Biome.Alpine"), Idx(E(i))));
        EXPECT_FALSE(AreBiomesForbidden(LegacyEnumB("Biome.Ocean"), E(i)));
        EXPECT_FALSE(Reg().AreForbidden(B("Biome.Ocean"), Idx(E(i))));
        EXPECT_FALSE(AreBiomesForbidden(LegacyEnumB("Biome.Lake"), E(i)));
        EXPECT_FALSE(Reg().AreForbidden(B("Biome.Lake"), Idx(E(i))));
    }
}

TEST(BiomeTypes, CellDefaultsBiomeToOcean)
{
    Cell c;
    EXPECT_EQ(Idx(c.biome), B("Biome.Ocean"));
}
