// S-1 Biome Registry: the shipped Global/World/biomes.json (plan tasks 7 and 8).
// Expectations are literals copied from the former legacy code (the deleted Biome enum, BiomeClassify.cpp, terrain_rules.schema.json),
// not read back from the registry.
#include <gtest/gtest.h>
#include <Shared/World/BiomeRegistry.h>
#include <Shared/WorldGeneration/TerrainTypes.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <type_traits>

using namespace CoW::World;
using CoW::WorldGeneration::DepthZone;
using Dia::Core::StringCRC;

namespace
{
    std::string FindShippedFile(const char* relative)
    {
        std::error_code ec;
        std::filesystem::path dir = std::filesystem::absolute(std::filesystem::path(__FILE__), ec).parent_path();
        while (!dir.empty())
        {
            const std::filesystem::path candidate = dir / "Cluiche/Assets/CoW" / relative;
            if (std::filesystem::is_regular_file(candidate, ec)) { return candidate.string(); }
            const std::filesystem::path parent = dir.parent_path();
            if (parent == dir) { break; }
            dir = parent;
        }
        return {};
    }

    const BiomeRegistry& Shipped()
    {
        static BiomeRegistry reg;
        static bool loaded = false;
        if (!loaded)
        {
            loaded = true;
            const std::string path = FindShippedFile("Global/World/biomes.json");
            std::ifstream in(path, std::ios::binary);
            std::stringstream ss;
            ss << in.rdbuf();
            RegistryError err;
            const bool ok = !path.empty() && in.good() && reg.LoadFromText(ss.str().c_str(), err);
            EXPECT_TRUE(ok) << "path=" << path << " field=" << err.field.AsCStr() << " id=" << err.id.AsCStr()
                            << " row=" << err.row << " reason=" << err.reason.AsCStr();
        }
        return reg;
    }

    enum Row { Ocean, Lake, Grass, Forest, Wetland, Desert, Alpine, Tundra, Rain, Volcanic, Scrub, Burned, Desertified, kRows };

    struct Expect
    {
        const char* id;
        const char* name;
        uint32_t    colour;
        bool        generated;
        const char* tags;     // comma separated
        bool        zones[4]; // None, FirstShores, WildReaches, AncientInteriors
    };

    const Expect kExpect[kRows] = {
        { "Biome.Ocean",            "Ocean",              0x1f4f8f, true,  "water,aquatic",            { true,  false, false, false } },
        { "Biome.Lake",             "Lake",               0x4aa3df, true,  "water,aquatic",            { true,  false, false, false } },
        { "Biome.Grassland",        "Grassland / Steppe", 0xa6c04a, true,  "land,open,fuel",           { false, true,  true,  false } },
        { "Biome.TemperateForest",  "Temperate Forest",   0x2f7d3a, true,  "land,forest,fuel",         { false, true,  true,  false } },
        { "Biome.Wetland",          "Wetland / Delta",    0x4fb39a, true,  "land,wet,aquatic",         { false, true,  true,  false } },
        { "Biome.Desert",           "Desert Basin",       0xd9b45a, true,  "land,open,hot",            { false, false, true,  true  } },
        { "Biome.Alpine",           "Alpine Mountain",    0xc4c9d1, true,  "land,mountain,cold",       { false, false, true,  true  } },
        { "Biome.Tundra",           "Tundra / Boreal",    0x8aa39a, true,  "land,open,cold",           { false, false, true,  true  } },
        { "Biome.Rainforest",       "Rainforest",         0x0f5a2a, true,  "land,forest,wet,hot",      { false, false, true,  true  } },
        { "Biome.Volcanic",         "Volcanic Ashland",   0x6b3a36, true,  "land,volcanic,hot",        { false, false, true,  true  } },
        { "Biome.Scrub",            "Scrub",              0x8f8a3c, false, "land,open,fuel,succession",{ false, false, false, false } },
        { "Biome.BurnedAshland",    "Burned Ashland",     0x2b2523, false, "land,succession",          { false, false, false, false } },
        { "Biome.DesertifiedPlain", "Desertified Plain",  0xc98a4b, false, "land,open,hot,succession", { false, false, false, false } },
    };

    const DepthZone kZones[4] = { DepthZone::None, DepthZone::FirstShores, DepthZone::WildReaches, DepthZone::AncientInteriors };

    uint64_t MaskOf(const BiomeRegistry& r, const char* csv)
    {
        uint64_t mask = 0;
        std::string s(csv);
        size_t start = 0;
        while (start <= s.size())
        {
            size_t end = s.find(',', start);
            if (end == std::string::npos) { end = s.size(); }
            const std::string name = s.substr(start, end - start);
            const TagId t = r.FindTag(StringCRC(name.c_str()));
            EXPECT_NE(t, kNoTag) << "tag " << name;
            if (t != kNoTag) { mask |= (1ull << t); }
            start = end + 1;
        }
        return mask;
    }
}

TEST(BiomeRegistryData, ShippedFileLoadsWithThirteenRows)
{
    EXPECT_EQ(Shipped().Count(), 13u);
}

TEST(BiomeRegistryData, IdsNamesColoursGeneratedInEnumOrder)
{
    const BiomeRegistry& r = Shipped();
    ASSERT_EQ(r.Count(), 13u);
    for (int i = 0; i < kRows; ++i)
    {
        const BiomeIndex b = static_cast<BiomeIndex>(i);
        EXPECT_STREQ(r.IdText(b), kExpect[i].id) << i;
        EXPECT_EQ(r.Id(b), StringCRC(kExpect[i].id)) << i;
        EXPECT_EQ(r.Find(StringCRC(kExpect[i].id)), b) << i;
        EXPECT_STREQ(r.Name(b), kExpect[i].name) << i;
        EXPECT_EQ(r.Colour(b), kExpect[i].colour) << i;
        EXPECT_EQ(r.IsGenerated(b), kExpect[i].generated) << i;
    }
}

TEST(BiomeRegistryData, TagMasksMatchConfirmedAssignment)
{
    const BiomeRegistry& r = Shipped();
    ASSERT_EQ(r.Count(), 13u);
    for (int i = 0; i < kRows; ++i)
    {
        EXPECT_EQ(r.TagMask(static_cast<BiomeIndex>(i)), MaskOf(r, kExpect[i].tags)) << kExpect[i].id;
    }
}

TEST(BiomeRegistryData, ZoneValidityAllZonesAllBiomes)
{
    const BiomeRegistry& r = Shipped();
    ASSERT_EQ(r.Count(), 13u);
    for (int i = 0; i < kRows; ++i)
    {
        for (int z = 0; z < 4; ++z)
        {
            EXPECT_EQ(r.IsValidInZone(static_cast<BiomeIndex>(i), kZones[z]), kExpect[i].zones[z]) << kExpect[i].id << " zone " << z;
        }
    }
}

TEST(BiomeRegistryData, ForbiddenPairsAreTheSevenLegacyPairs)
{
    const BiomeRegistry& r = Shipped();
    ASSERT_EQ(r.Count(), 13u);
    const int pairs[7][2] = { { Volcanic, Wetland }, { Volcanic, Rain }, { Volcanic, Tundra }, { Rain, Desert },
                              { Rain, Tundra }, { Tundra, Wetland }, { Desert, Forest } };
    bool expected[kRows][kRows] = {};
    for (const auto& p : pairs)
    {
        EXPECT_TRUE(r.AreForbidden(static_cast<BiomeIndex>(p[0]), static_cast<BiomeIndex>(p[1])));
        EXPECT_TRUE(r.AreForbidden(static_cast<BiomeIndex>(p[1]), static_cast<BiomeIndex>(p[0])));
        expected[p[0]][p[1]] = expected[p[1]][p[0]] = true;
    }
    // Every other combination (including self pairs) is not forbidden.
    for (int a = 0; a < kRows; ++a)
    {
        for (int b = 0; b < kRows; ++b)
        {
            EXPECT_EQ(r.AreForbidden(static_cast<BiomeIndex>(a), static_cast<BiomeIndex>(b)), expected[a][b]) << a << "," << b;
        }
    }
    EXPECT_FALSE(r.AreForbidden(Grass, Forest));
    EXPECT_FALSE(r.AreForbidden(Alpine, Volcanic));
}

TEST(BiomeRegistryData, ClimateEnvelopesAndOrder)
{
    const BiomeRegistry& r = Shipped();
    ASSERT_EQ(r.Count(), 13u);
    struct E { int row; float t0, m0, wt, wm; };
    const E env[5] = {
        { Grass,  0.60f, 0.35f, 0.25f, 0.20f },
        { Forest, 0.50f, 0.60f, 0.25f, 0.20f },
        { Desert, 0.80f, 0.08f, 0.25f, 0.12f },
        { Tundra, 0.12f, 0.30f, 0.15f, 0.30f },
        { Rain,   0.85f, 0.90f, 0.20f, 0.15f },
    };
    for (const E& e : env)
    {
        const ClimateEnvelope* c = r.Climate(static_cast<BiomeIndex>(e.row));
        ASSERT_NE(c, nullptr) << e.row;
        EXPECT_FLOAT_EQ(c->t0, e.t0) << e.row;
        EXPECT_FLOAT_EQ(c->m0, e.m0) << e.row;
        EXPECT_FLOAT_EQ(c->wt, e.wt) << e.row;
        EXPECT_FLOAT_EQ(c->wm, e.wm) << e.row;
    }
    for (int i = 0; i < kRows; ++i)
    {
        bool has = false;
        for (const E& e : env) { has = has || e.row == i; }
        if (!has) { EXPECT_EQ(r.Climate(static_cast<BiomeIndex>(i)), nullptr) << i; }
    }
    uint32_t n = 0;
    const BiomeIndex* list = r.ClimateBiomes(n);
    ASSERT_EQ(n, 5u);
    EXPECT_EQ(list[0], Grass);
    EXPECT_EQ(list[1], Forest);
    EXPECT_EQ(list[2], Desert);
    EXPECT_EQ(list[3], Tundra);
    EXPECT_EQ(list[4], Rain);
}

TEST(BiomeRegistryData, PinsByPriorityPreserveLegacyPrecedence)
{
    // Legacy order: water (ocean, lake: mutually exclusive), volcanic, alpine, wetland.
    const BiomeRegistry& r = Shipped();
    ASSERT_EQ(r.Count(), 13u);
    uint32_t n = 0;
    const BiomeIndex* pins = r.PinsByPriority(n);
    ASSERT_EQ(n, 5u);
    EXPECT_EQ(pins[0], Ocean);
    EXPECT_EQ(pins[1], Lake);
    EXPECT_EQ(pins[2], Volcanic);
    EXPECT_EQ(pins[3], Alpine);
    EXPECT_EQ(pins[4], Wetland);
    EXPECT_EQ(r.Pin(Ocean), PinRule::WaterOcean);
    EXPECT_EQ(r.Pin(Lake), PinRule::WaterLake);
    EXPECT_EQ(r.Pin(Volcanic), PinRule::Vent);
    EXPECT_EQ(r.Pin(Alpine), PinRule::Alpine);
    EXPECT_EQ(r.Pin(Wetland), PinRule::Wetland);
}

TEST(BiomeRegistryData, PlateauAndRoles)
{
    const BiomeRegistry& r = Shipped();
    ASSERT_EQ(r.Count(), 13u);
    EXPECT_EQ(r.Plateau(true), Rain);
    EXPECT_EQ(r.Plateau(false), Desert);
    EXPECT_EQ(r.Role(BiomeRole::Ocean), Ocean);
    EXPECT_EQ(r.Role(BiomeRole::Lake), Lake);
    EXPECT_EQ(r.Role(BiomeRole::Fallback), Grass);
}

static_assert(std::is_trivially_copyable_v<CoW::WorldGeneration::Cell>, "Cell is memset/memcpy'd; must stay trivially copyable");
static_assert(sizeof(CoW::WorldGeneration::Cell) <= 24, "Cell grew unexpectedly");

// Tripwire: the committed S-1 layer goldens hash biomeIndex values, so the first ten must never renumber.
TEST(BiomeRegistryData, ShippedRowIndicesArePinned)
{
    const BiomeRegistry& r = Shipped();
    ASSERT_EQ(r.Count(), 13u);
    for (int i = 0; i < kRows; ++i)
    {
        EXPECT_EQ(r.Find(StringCRC(kExpect[i].id)), static_cast<BiomeIndex>(i)) << kExpect[i].id;
    }
    uint32_t generated = 0;
    for (BiomeIndex i = 0; i < r.Count(); ++i) { generated += r.IsGenerated(i) ? 1u : 0u; }
    EXPECT_EQ(generated, 10u);
}

TEST(BiomeRegistryData, FindRejectsUnknownIds)
{
    const BiomeRegistry& r = Shipped();
    EXPECT_EQ(r.Find(StringCRC("Biome.Nonsense")), kNoBiome);
    EXPECT_EQ(r.Find(StringCRC("Terrain.Mountain")), kNoBiome);
}

TEST(BiomeRegistryData, AlpineOceanLakeNeverForbidden)
{
    const BiomeRegistry& r = Shipped();
    for (BiomeIndex i = 0; i < r.Count(); ++i)
    {
        EXPECT_FALSE(r.AreForbidden(Alpine, i));
        EXPECT_FALSE(r.AreForbidden(Ocean, i));
        EXPECT_FALSE(r.AreForbidden(Lake, i));
    }
}

TEST(BiomeRegistryData, CellDefaultsBiomeToOceanRow)
{
    CoW::WorldGeneration::Cell c;
    EXPECT_EQ(c.biome, static_cast<BiomeIndex>(Ocean));
}

// ---- Task 9: BiomeTestRegistry.h ----
#include <Shared/World/Testing/BiomeTestRegistry.h>

TEST(BiomeTestRegistry, LookupByIdReturnsLegacyOrdinals)
{
    using namespace CoW::World::Testing;
    EXPECT_EQ(B("Biome.Grassland"), 2);
    EXPECT_EQ(B("Biome.Volcanic"), 9);
    EXPECT_EQ(B(ShippedBiomeRegistry(), "Biome.Rainforest"), 8);
}

TEST(BiomeTestRegistry, UnknownIdAborts)
{
    using namespace CoW::World::Testing;
    EXPECT_DEATH(B("Biome.DoesNotExist"), "");
}

TEST(BiomeTestRegistry, ExtraRegistryHasThirteenShippedRowsPlusOneClimateBiome)
{
    using namespace CoW::World::Testing;
    const BiomeRegistry& e = ExtraBiomeRegistry();
    const BiomeRegistry& s = ShippedBiomeRegistry();
    ASSERT_EQ(e.Count(), 14u);
    for (BiomeIndex i = 0; i < 13; ++i)
    {
        EXPECT_EQ(e.Id(i), s.Id(i)) << int(i);
        EXPECT_STREQ(e.Name(i), s.Name(i)) << int(i);
    }
    EXPECT_STREQ(e.IdText(13), "Biome.Savanna");
    ASSERT_NE(e.Climate(13), nullptr);
    uint32_t n = 0;
    const BiomeIndex* list = e.ClimateBiomes(n);
    ASSERT_EQ(n, 6u);
    EXPECT_EQ(list[5], 13);
}
