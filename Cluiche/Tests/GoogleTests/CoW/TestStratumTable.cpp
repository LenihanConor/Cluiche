// Stratum table: data-driven sets and strata (the generator supports three until Strata tasks 2-3), sidecar-owned
// shares, registry resolution, world JSON round trip with stratum names, and generation under renamed strata.
// Spec: docs/specs/features/draoi/generic-strata.md (task 1, AC 5).
#include <gtest/gtest.h>

#include <Shared/World/BiomeRegistry.h>
#include <Shared/World/Testing/BiomeTestRegistry.h>
#include <Shared/World/StratumTable.h>
#include <Shared/WorldGeneration/Testing/TerrainRulesTestHelpers.h>
#include <Shared/WorldGeneration/TerrainGenerator.h>
#include <Shared/WorldGeneration/WorldJson.h>

#include <string>
#include <vector>

using namespace CoW::World;
using namespace CoW::WorldGeneration;
using namespace CoW::World::Testing;

namespace {

const std::vector<std::string> kThree = { "A", "B", "C" };
const std::vector<float>       kShares = { 0.45f, 0.40f, 0.15f };

std::string Strata(const std::vector<std::string>& names) { return TestStrataText(names); }

bool LoadWith(StratumTable& t, const std::string& strataText, const std::vector<std::string>& names, const std::vector<float>& shares,
              StratumError& err)
{
    return t.LoadFromText(strataText.c_str(), TestStrataSidecar(names, shares).c_str(), err);
}

std::string Replace(std::string text, const std::string& from, const std::string& to)
{
    for (size_t at = text.find(from); at != std::string::npos; at = text.find(from, at + to.size()))
        text.replace(at, from.size(), to);
    return text;
}

// A document with a custom sets array body.
std::string Doc(const std::string& setsBody) { return "{\"format\":\"cow.strata\",\"version\":1,\"sets\":" + setsBody + "}"; }
std::string DepthSet(const std::string& strataArray, const std::string& extra = "")
{
    return "{\"id\":\"depth\"," + extra + "\"assigner\":{\"type\":\"coast_cost_bands\"},\"strata\":" + strataArray + "}";
}
const char* kRegions = ",\"regions_min\":4,\"regions_max\":6";
std::string St(const char* name, const std::string& extra = kRegions)
{
    return std::string("{\"name\":\"") + name + "\"" + extra + "}";
}
std::string List(std::initializer_list<std::string> items)
{
    std::string s = "[";
    for (const std::string& i : items) { s += (s.size() > 1 ? "," : "") + i; }
    return s + "]";
}

}

TEST(StratumTable, ShippedFileLoadsThreeStrataInOrder)
{
    const StratumTable& z = ShippedStratumTable();
    ASSERT_EQ(z.Count(), 3u);
    ASSERT_EQ(z.SetCount(), 1u);
    ASSERT_NE(z.Depth(), nullptr);
    EXPECT_STREQ(z.Depth()->id, "depth");
    EXPECT_TRUE(z.Depth()->ordered);
    EXPECT_STREQ(z.Depth()->assignerType, kBuiltinAssignerType);
    EXPECT_STREQ(z.Depth()->metric, "coast_cost");
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
    EXPECT_EQ(z.FindSet("depth"), z.Depth());
    EXPECT_EQ(z.FindSet("other"), nullptr);
}

TEST(StratumTable, EmptyTableHasNoStrata)
{
    StratumTable z;
    EXPECT_EQ(z.Count(), 0u);
    EXPECT_EQ(z.Depth(), nullptr);
    EXPECT_EQ(z.Last(), kNoStratum);
    EXPECT_STREQ(z.Name(0), "None");
    EXPECT_EQ(z.Name(1), nullptr);
    EXPECT_EQ(z.IndexOf("None"), 0);
    EXPECT_EQ(z.FirstShare(), 0.0f);
}

TEST(StratumTable, ShippedFileCarriesTodaysRegionCountsAndInnermostRules)
{
    const StratumSetSpec& d = *ShippedStratumTable().Depth();
    EXPECT_EQ(d.strata[0].regionsMin, 4); EXPECT_EQ(d.strata[0].regionsMax, 6);
    EXPECT_EQ(d.strata[1].regionsMin, 4); EXPECT_EQ(d.strata[1].regionsMax, 6);
    EXPECT_EQ(d.strata[2].regionsMin, 2); EXPECT_EQ(d.strata[2].regionsMax, 4);
    EXPECT_FALSE(d.strata[0].mainLandmassOnly);
    EXPECT_FALSE(d.strata[1].mainLandmassOnly);
    EXPECT_TRUE(d.strata[2].mainLandmassOnly);
    EXPECT_EQ(d.strata[2].minCoastDepth, 3);
}

TEST(StratumTable, SidecarDefaultSharesEqualOldRules)
{
    // The values kFirstZoneShare / kLastZoneShare carried before the move, and the remainder in the middle.
    EXPECT_EQ(ShippedStratumTable().FirstShare(), 0.45f);
    EXPECT_EQ(ShippedStratumTable().LastShare(), 0.15f);
    EXPECT_EQ(ShippedStratumTable().Depth()->strata[1].share, 0.40f);
}

TEST(StratumTable, SidecarSharesAreTheEffectiveShares)
{
    StratumTable z;
    StratumError err;
    ASSERT_TRUE(LoadWith(z, Strata(kThree), kThree, { 0.5f, 0.3f, 0.2f }, err)) << err.reason;
    EXPECT_EQ(z.FirstShare(), 0.5f);
    EXPECT_EQ(z.LastShare(), 0.2f);
}

TEST(StratumTable, RejectsBadDataNamingTheFaultAndLeavesNothingLoaded)
{
    struct Case { const char* label; std::string text; const char* mustMention; };
    const std::string three = List({ St("A"), St("B"), St("C", ",\"regions_min\":2,\"regions_max\":4") });
    const Case bad[] = {
        { "not json", "{ not json", "" },
        { "wrong format", "{\"format\":\"cow.biomes\",\"version\":1,\"sets\":[]}", "format" },
        { "old zones file", "{\"format\":\"cow.zones\",\"version\":1,\"zones\":[{\"name\":\"A\"}]}", "format" },
        { "wrong version", "{\"format\":\"cow.strata\",\"version\":9,\"sets\":[]}", "version" },
        { "no sets", Doc("[]"), "set" },
        { "sets not an array", Doc("{}"), "sets" },
        { "duplicate set id", Doc(List({ DepthSet(three), DepthSet(three) })), "duplicate set" },
        { "no strata", Doc(List({ DepthSet("[]") })), "depth" },
        { "more than kMaxStrata", Doc(List({ DepthSet(List({ St("A"), St("B"), St("C"), St("D"), St("E"), St("F"), St("G"), St("H"), St("I") })) })), "depth" },
        { "depth set not three strata (generator limit)", Doc(List({ DepthSet(List({ St("A"), St("B") })) })), "generator supports 3 strata" },
        { "regions_min > regions_max", Doc(List({ DepthSet(List({ St("A"), St("B"), St("C", ",\"regions_min\":5,\"regions_max\":4") })) })), "C" },
        { "missing regions", Doc(List({ DepthSet(List({ St("A"), St("B"), St("C", "") })) })), "C" },
        { "empty name", Doc(List({ DepthSet(List({ St("A"), St("B"), St("") })) })), "depth" },
        { "reserved name None", Doc(List({ DepthSet(List({ St("A"), St("B"), St("None") })) })), "None" },
        { "duplicate stratum name", Doc(List({ DepthSet(List({ St("A"), St("B"), St("A") })) })), "duplicate" },
        { "unknown assigner type", Doc("[{\"id\":\"depth\",\"assigner\":{\"type\":\"nope\"},\"strata\":" + three + "}]"), "nope" },
        { "missing assigner", Doc("[{\"id\":\"depth\",\"strata\":" + three + "}]"), "assigner" },
        { "bad ordered type", Doc(List({ DepthSet(three, "\"ordered\":\"yes\",") })), "ordered" },
        { "bad share type", Doc(List({ DepthSet(List({ St("A", ",\"share\":\"x\",\"regions_min\":4,\"regions_max\":6"), St("B"), St("C") })) })), "A" },
        { "bad main_landmass_only type", Doc(List({ DepthSet(List({ St("A", ",\"main_landmass_only\":3,\"regions_min\":4,\"regions_max\":6"), St("B"), St("C") })) })), "A" },
        { "negative min_coast_depth", Doc(List({ DepthSet(List({ St("A", ",\"min_coast_depth\":-1,\"regions_min\":4,\"regions_max\":6"), St("B"), St("C") })) })), "A" },
        { "no depth set", Doc("[{\"id\":\"other\",\"assigner\":{\"type\":\"coast_cost_bands\"},\"strata\":" + three + "}]"), "depth" },
    };
    for (const Case& c : bad)
    {
        StratumTable z;
        StratumError err;
        EXPECT_FALSE(LoadWith(z, c.text, kThree, kShares, err)) << c.label;
        EXPECT_GT(err.reason.size(), 0u) << c.label;
        const std::string both = err.field + " " + err.reason;
        EXPECT_NE(both.find(c.mustMention), std::string::npos) << c.label << ": " << both;
        EXPECT_EQ(z.Count(), 0u) << c.label;
        EXPECT_EQ(z.SetCount(), 0u) << c.label;
    }
}

TEST(StratumTable, SharesNotCompletingOneAreRefusedNamingTheSet)
{
    StratumTable z;
    StratumError err;
    EXPECT_FALSE(LoadWith(z, Strata(kThree), kThree, { 0.5f, 0.5f, 0.5f }, err));
    EXPECT_NE((err.field + err.reason).find("depth"), std::string::npos);
    EXPECT_FALSE(LoadWith(z, Strata(kThree), kThree, { 0.2f, 0.2f, 0.2f }, err));
    EXPECT_EQ(z.Count(), 0u);
}

TEST(StratumTable, SidecarMissingAStratumKeyIsRefusedNamingIt)
{
    StratumTable z;
    StratumError err;
    // The sidecar knows A and B only; the file declares C too.
    EXPECT_FALSE(z.LoadFromText(Strata(kThree).c_str(), TestStrataSidecar({ "A", "B" }, { 0.5f, 0.5f }).c_str(), err));
    EXPECT_NE((err.field + err.reason).find("depth.C.share"), std::string::npos) << err.field << " " << err.reason;
}

TEST(StratumTable, ASecondSetLoadsAndIsFindableByIdWhileDepthStaysTheGeneratorSet)
{
    const std::string other = "{\"id\":\"homeland\",\"ordered\":false,\"assigner\":{\"type\":\"coast_cost_bands\"},\"strata\":" + List({ St("X"), St("Y") }) + "}";
    std::string side = TestStrataSidecar(kThree, kShares);
    const std::string extra =
        ",{\"key\":\"homeland.X.share\",\"kind\":\"parameter\",\"label\":\"x\",\"description\":\"d\",\"type\":\"float\",\"min\":0.0,\"max\":1.0,\"default\":0.5},"
        "{\"key\":\"homeland.Y.share\",\"kind\":\"parameter\",\"label\":\"y\",\"description\":\"d\",\"type\":\"float\",\"min\":0.0,\"max\":1.0,\"default\":0.5}";
    side.insert(side.rfind(']'), extra);
    StratumTable z;
    StratumError err;
    ASSERT_TRUE(z.LoadFromText(Doc(List({ DepthSet(List({ St("A"), St("B"), St("C", std::string(kRegions) + ",\"min_coast_depth\":3") })), other })).c_str(),
                               side.c_str(), err)) << err.field << " " << err.reason;
    EXPECT_EQ(z.SetCount(), 2u);
    ASSERT_NE(z.FindSet("homeland"), nullptr);
    EXPECT_FALSE(z.FindSet("homeland")->ordered);
    EXPECT_EQ(z.FindSet("homeland")->count, 2u);
    EXPECT_EQ(z.Count(), 3u);   // Count() is the depth set
    EXPECT_STREQ(z.Name(1), "A");
}

TEST(StratumTable, FailedLoadKeepsPreviousTable)
{
    StratumTable z;
    StratumError err;
    ASSERT_TRUE(LoadWith(z, Strata(kThree), kThree, kShares, err));
    EXPECT_FALSE(LoadWith(z, Strata({ "A", "A", "C" }), kThree, kShares, err));
    ASSERT_EQ(z.Count(), 3u);
    EXPECT_STREQ(z.Name(2), "B");
    EXPECT_EQ(z.LastShare(), 0.15f);
}

TEST(StratumTable, BadSidecarFailsLoud)
{
    StratumTable z;
    StratumError err;
    EXPECT_FALSE(z.LoadFromText(Strata(kThree).c_str(), "{}", err));
    EXPECT_EQ(z.Count(), 0u);
}

TEST(StratumTable, RegistryResolvesStratumNamesAgainstTable)
{
    const std::string shipped = Detail::ReadFileText(ShippedBiomesPath());
    BiomeRegistry reg;
    RegistryError err;
    // The same biomes against a table lacking one of the named strata is refused.
    const std::vector<std::string> names = { "FirstShores", "WildReaches", "Other" };
    StratumTable missing;
    StratumError zerr;
    ASSERT_TRUE(LoadWith(missing, Strata(names), names, kShares, zerr));
    EXPECT_FALSE(reg.LoadFromText(shipped.c_str(), missing, err));
    EXPECT_STREQ(err.field.AsCStr(), "zones");
    EXPECT_EQ(reg.Count(), 0u);
    ASSERT_TRUE(reg.LoadFromText(shipped.c_str(), ShippedStratumTable(), err));
    EXPECT_EQ(reg.Strata().Count(), 3u);
}

TEST(StratumTable, RenamedStrataGiveIdenticalLayoutAndHash)
{
    const std::vector<std::string> names = { "Shore", "Wilds", "Core" };
    StratumTable renamed;
    StratumError zerr;
    ASSERT_TRUE(LoadWith(renamed, Strata(names), names, kShares, zerr)) << zerr.reason;
    std::string biomes = Detail::ReadFileText(ShippedBiomesPath());
    biomes = Replace(biomes, "\"FirstShores\"", "\"Shore\"");
    biomes = Replace(biomes, "\"WildReaches\"", "\"Wilds\"");
    biomes = Replace(biomes, "\"AncientInteriors\"", "\"Core\"");
    BiomeRegistry reg;
    RegistryError err;
    ASSERT_TRUE(reg.LoadFromText(biomes.c_str(), renamed, err)) << err.reason.AsCStr();
    EXPECT_EQ(reg.Hash(), ShippedBiomeRegistry().Hash());   // hash covers stratum indices, not names
    EXPECT_STREQ(reg.Strata().Name(3), "Core");

    const GridDimensions dims{ 48, 32 };
    auto layout = [&](const BiomeRegistry& r)
    {
        TerrainGenerator g(TestRules(), &r);
        Dia::Maths::RandomStream rng(1);
        g.Update(42, WorldShapeParams{}, dims, rng);
        const uint32_t* stratum = g.GetLayers().GetUInt(g.GetLayers().FindLayer("stratum"));
        return std::vector<uint32_t>(stratum, stratum + dims.width * dims.height);
    };
    const std::vector<uint32_t> a = layout(ShippedBiomeRegistry());
    const std::vector<uint32_t> b = layout(reg);
    EXPECT_EQ(a, b);
    bool anyInner = false;
    for (uint32_t v : a) { anyInner = anyInner || v == 3; }
    EXPECT_TRUE(anyInner);
}

TEST(StratumTable, MinCoastDepthIsOwnedByTheSidecar)
{
    StratumTable z;
    StratumError err;
    ASSERT_TRUE(LoadWith(z, Strata(kThree), kThree, kShares, err));
    EXPECT_EQ(z.Depth()->strata[2].minCoastDepth, 3);
    EXPECT_TRUE(z.Depth()->strata[2].declaresMinCoastDepth);
    EXPECT_FALSE(z.Depth()->strata[0].declaresMinCoastDepth);

    // A sidecar default of 5 overrides the authored 3.
    std::string side = TestStrataSidecar(kThree, kShares);
    const size_t at = side.find("\"default\":3.0");
    ASSERT_NE(at, std::string::npos);
    side.replace(at, std::string("\"default\":3.0").size(), "\"default\":5.0");
    StratumTable five;
    ASSERT_TRUE(five.LoadFromText(Strata(kThree).c_str(), side.c_str(), err)) << err.field << " " << err.reason;
    EXPECT_EQ(five.Depth()->strata[2].minCoastDepth, 5);
}

TEST(StratumTable, DeclaredMinCoastDepthNeedsItsSidecarKey)
{
    StratumTable z;
    StratumError err;
    std::string side = TestStrataSidecar(kThree, kShares);
    side.erase(side.rfind(",{\"key\":\"depth.C.min_coast_depth\""));
    side += "]}";
    EXPECT_FALSE(z.LoadFromText(Strata(kThree).c_str(), side.c_str(), err));
    EXPECT_NE((err.field + err.reason).find("depth.C.min_coast_depth"), std::string::npos) << err.field << " " << err.reason;
}
