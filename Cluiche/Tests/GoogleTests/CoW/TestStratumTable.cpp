// Depth zone table: data-driven zone names (count stays 3), sidecar-owned shares, registry resolution,
// world JSON round trip with zone names, and generation under renamed zones.
#include <gtest/gtest.h>

#include <Shared/World/BiomeRegistry.h>
#include <Shared/World/Testing/BiomeTestRegistry.h>
#include <Shared/World/ZoneTable.h>
#include <Shared/WorldGeneration/Testing/TerrainRulesTestHelpers.h>
#include <Shared/WorldGeneration/TerrainGenerator.h>
#include <Shared/WorldGeneration/WorldJson.h>

#include <string>

using namespace CoW::World;
using namespace CoW::WorldGeneration;
using namespace CoW::World::Testing;

namespace {

std::string Zones(const char* names)   // names: comma separated, e.g. "A,B,C"
{
    std::string list;
    std::string cur;
    auto flush = [&] { if (!cur.empty() || !list.empty() || true) { if (!list.empty()) list += ","; list += "{\"name\":\"" + cur + "\"}"; cur.clear(); } };
    for (const char* p = names; ; ++p)
    {
        if (*p == ',' || *p == '\0') { flush(); if (*p == '\0') break; }
        else cur += *p;
    }
    return "{\"format\":\"cow.zones\",\"version\":1,\"zones\":[" + list + "]}";
}

std::string Sidecar() { return Detail::ReadFileText(ShippedZoneSidecarPath()); }

bool Load(ZoneTable& t, const std::string& zonesText, ZoneError& err)
{
    return t.LoadFromText(zonesText.c_str(), Sidecar().c_str(), err);
}

std::string Replace(std::string text, const std::string& from, const std::string& to)
{
    for (size_t at = text.find(from); at != std::string::npos; at = text.find(from, at + to.size()))
        text.replace(at, from.size(), to);
    return text;
}

}

TEST(ZoneTable, ShippedFileLoadsThreeZonesInOrder)
{
    const ZoneTable& z = ShippedZoneTable();
    ASSERT_EQ(z.Count(), 3u);
    EXPECT_STREQ(z.Name(0), "None");
    EXPECT_STREQ(z.Name(1), "FirstShores");
    EXPECT_STREQ(z.Name(2), "WildReaches");
    EXPECT_STREQ(z.Name(3), "AncientInteriors");
    EXPECT_EQ(z.Name(4), nullptr);
    EXPECT_EQ(z.IndexOf("None"), 0);
    EXPECT_EQ(z.IndexOf("FirstShores"), 1);
    EXPECT_EQ(z.IndexOf("AncientInteriors"), 3);
    EXPECT_EQ(z.IndexOf("Nowhere"), -1);
    EXPECT_EQ(z.IndexOf(nullptr), -1);
    EXPECT_EQ(z.Last(), 3);
}

TEST(ZoneTable, SidecarDefaultSharesEqualOldRules)
{
    // The values TerrainRules::kFirstShoresShare / kAncientShare carried before the move.
    EXPECT_EQ(ShippedZoneTable().FirstShare(), 0.45f);
    EXPECT_EQ(ShippedZoneTable().LastShare(), 0.15f);
}

TEST(ZoneTable, RejectsBadDataAndLeavesNothingLoaded)
{
    const char* bad[] = {
        "{ not json",
        "{\"format\":\"cow.zones\",\"version\":1,\"zones\":[]}",                              // empty
        "{\"format\":\"cow.zones\",\"version\":1,\"zones\":[{\"name\":\"A\"},{\"name\":\"A\"}]}",   // duplicate
        "{\"format\":\"cow.zones\",\"version\":1,\"zones\":[{\"name\":\"A\"},{\"name\":\"B\"},{\"name\":\"C\"},{\"name\":\"D\"}]}", // too many
        "{\"format\":\"cow.zones\",\"version\":1,\"zones\":[{\"name\":\"\"}]}",                    // empty name
        "{\"format\":\"cow.zones\",\"version\":1,\"zones\":[{\"name\":\"None\"}]}",                // reserved
        "{\"format\":\"cow.biomes\",\"version\":1,\"zones\":[{\"name\":\"A\"}]}",                  // wrong format
    };
    for (const char* text : bad)
    {
        ZoneTable z;
        ZoneError err;
        EXPECT_FALSE(Load(z, text, err)) << text;
        EXPECT_GT(err.reason.size(), 0u) << text;
        EXPECT_EQ(z.Count(), 0u) << text;
    }
}

TEST(ZoneTable, FailedLoadKeepsPreviousTable)
{
    ZoneTable z;
    ZoneError err;
    ASSERT_TRUE(Load(z, Zones("A,B,C"), err));
    EXPECT_FALSE(Load(z, Zones("A,A"), err));
    ASSERT_EQ(z.Count(), 3u);
    EXPECT_STREQ(z.Name(2), "B");
}

TEST(ZoneTable, BadSidecarFailsLoud)
{
    ZoneTable z;
    ZoneError err;
    EXPECT_FALSE(z.LoadFromText(Zones("A,B").c_str(), "{}", err));
    EXPECT_EQ(z.Count(), 0u);
}

TEST(ZoneTable, RegistryResolvesZoneNamesAgainstTable)
{
    const std::string shipped = Detail::ReadFileText(ShippedBiomesPath());
    BiomeRegistry reg;
    RegistryError err;
    // The same biomes against a table lacking one of the named zones is refused.
    ZoneTable missing;
    ZoneError zerr;
    ASSERT_TRUE(Load(missing, Zones("FirstShores,WildReaches"), zerr));
    EXPECT_FALSE(reg.LoadFromText(shipped.c_str(), missing, err));
    EXPECT_STREQ(err.field.AsCStr(), "zones");
    EXPECT_EQ(reg.Count(), 0u);
    ASSERT_TRUE(reg.LoadFromText(shipped.c_str(), ShippedZoneTable(), err));
    EXPECT_EQ(reg.Zones().Count(), 3u);
}

TEST(ZoneTable, RenamedZonesGiveIdenticalLayoutAndHash)
{
    ZoneTable renamed;
    ZoneError zerr;
    ASSERT_TRUE(Load(renamed, Zones("Shore,Wilds,Core"), zerr));
    std::string biomes = Detail::ReadFileText(ShippedBiomesPath());
    biomes = Replace(biomes, "\"FirstShores\"", "\"Shore\"");
    biomes = Replace(biomes, "\"WildReaches\"", "\"Wilds\"");
    biomes = Replace(biomes, "\"AncientInteriors\"", "\"Core\"");
    BiomeRegistry reg;
    RegistryError err;
    ASSERT_TRUE(reg.LoadFromText(biomes.c_str(), renamed, err)) << err.reason.AsCStr();
    EXPECT_EQ(reg.Hash(), ShippedBiomeRegistry().Hash());   // hash covers zone indices, not names
    EXPECT_STREQ(reg.Zones().Name(3), "Core");

    const GridDimensions dims{ 48, 32 };
    auto layout = [&](const BiomeRegistry& r)
    {
        TerrainGenerator g(TestRules(), &r);
        Dia::Maths::RandomStream rng(1);
        g.Update(42, WorldShapeParams{}, dims, rng);
        const uint32_t* zone = g.GetLayers().GetUInt(g.GetLayers().FindLayer("depthZone"));
        return std::vector<uint32_t>(zone, zone + dims.width * dims.height);
    };
    const std::vector<uint32_t> a = layout(ShippedBiomeRegistry());
    const std::vector<uint32_t> b = layout(reg);
    EXPECT_EQ(a, b);
    bool anyInner = false;
    for (uint32_t v : a) { anyInner = anyInner || v == 3; }
    EXPECT_TRUE(anyInner);
}
