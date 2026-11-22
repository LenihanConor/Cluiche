// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/f1-debug-host-integration.md (Command set, AC12, AC13),
//       worldgen-panel-rest.plan.md (task 1)
// WorldGenSession command/query gaps for the panel: validate, fuller report evidence, worlds listing, status
// unavailableLayers, import line/column errors.
#include <gtest/gtest.h>
#include <Shared/CoWNodeKinds.h>
#include <DraoiHexWorldTerrain/WorldGenSession.h>
#include <Shared/WorldGeneration/CoWTesting/GuaranteeSpecTestHelpers.h>
#include <DraoiHexWorldTerrain/WorldFiles.h>
#include <DraoiHexWorldTerrain/Testing/SessionDrive.h>
#include <DraoiHexWorldTerrain/Testing/TestPlacers.h>
#include <Shared/WorldGeneration/CoWTesting/CoWTerrainRulesTestHelpers.h>

#include <DiaPathfinding/HexTopology.h>

#include <algorithm>
#include <atomic>
#include <memory>
#include <vector>
#include <filesystem>
#include <fstream>
#include <string>

using namespace CoW::WorldGeneration;
using namespace Draoi::HexWorld;
using namespace CoW::WorldGeneration::Testing;
using namespace Draoi::HexWorld::Testing;
namespace fs = std::filesystem;

namespace
{

Json::Value Obj() { return Json::Value(Json::objectValue); }

class WorldGenSessionValidateTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        static std::atomic<int> counter{ 0 };
        mRoot = fs::temp_directory_path() / ("cow_sessionvalidate_test_" + std::to_string(counter.fetch_add(1)) + "_" +
                                              std::to_string(reinterpret_cast<uintptr_t>(this) & 0xFFFFFF));
        std::error_code ec;
        fs::remove_all(mRoot, ec);
        fs::create_directories(mRoot, ec);
        mOut = (mRoot / "out").string();
        mDeployed = (mRoot / "deployed").string();
    }
    void TearDown() override
    {
        std::error_code ec;
        fs::remove_all(mRoot, ec);
    }

    void Generate(WorldGenSession& s, uint64_t seed, const SiteRegistry* registry = nullptr)
    {
        s.SetWorldDirectories(mDeployed, mOut);
        if (registry) { s.SetSiteRegistry(registry); }
        Json::Value regen = Obj();
        regen["seed"] = static_cast<Json::UInt64>(seed);
        ASSERT_TRUE(s.Regenerate(regen)["success"].asBool());
        ASSERT_TRUE(s.Tick(mRng));
        ASSERT_TRUE(s.RegionsFresh());
    }

    // Two neighbouring Lowland cells of a world generated from `seed` (same seed -> same terrain in later sessions).
    bool FindLowlandPair(uint64_t seed, uint32_t& a, uint32_t& b)
    {
        TestSession probe;
        Generate(probe, seed);
        const WorldStructure& w = probe.World();
        const Dia::Pathfinding::HexTopology topo(w.dimensions.width, w.dimensions.height);
        for (int i = 0; i < w.CellCount(); ++i)
        {
            if (w.Cells()[i].terrain != TerrainType::Lowland) { continue; }
            for (uint32_t dir = 0; dir < 6; ++dir)
            {
                const uint32_t n = topo.NeighbourAt(static_cast<uint32_t>(i), dir);
                if (n != Dia::Pathfinding::kNoCell && w.Cells()[n].terrain == TerrainType::Lowland)
                {
                    a = static_cast<uint32_t>(i);
                    b = n;
                    return true;
                }
            }
        }
        return false;
    }

    static const Json::Value* FindGuarantee(const Json::Value& list, const char* name)
    {
        for (const Json::Value& g : list)
        {
            if (g["name"].asString() == name) { return &g; }
        }
        return nullptr;
    }

    fs::path mRoot;
    std::string mOut;
    std::string mDeployed;
    Dia::Maths::RandomStream mRng{ 1 };
};

} // namespace

// ---- validate -------------------------------------------------------------------------------------------------------------

TEST_F(WorldGenSessionValidateTest, ValidateOnAPassingWorldIsOkAndKeepsTheRevision)
{
    TestSession s;
    Generate(s, 11);
    const uint32_t revision = s.Revision();
    const Json::Value r = s.Validate(Obj());
    ASSERT_TRUE(r["success"].asBool()) << r.toStyledString();
    EXPECT_TRUE(r["ok"].asBool()) << r.toStyledString();
    EXPECT_EQ(r["revision"].asUInt(), revision);
    EXPECT_EQ(s.Revision(), revision);
    EXPECT_GE(r["guarantees"].size(), 3u);   // HearthNexus, DynastySpacing, WorldLayout
    EXPECT_NE(FindGuarantee(r["guarantees"], "CoW.Guarantee.HearthNexus"), nullptr);
    for (const Json::Value& g : r["guarantees"])
    {
        EXPECT_TRUE(g["ok"].asBool());
        EXPECT_TRUE(g.isMember("id"));
        EXPECT_TRUE(g["counts"].isMember("proofPaths"));
    }
    EXPECT_TRUE(s.HasEvidence());
    EXPECT_EQ(s.EvidenceRevision(), revision);
}

TEST_F(WorldGenSessionValidateTest, ValidateOnAViolatingWorldFailsWithoutChangingTheWorld)
{
    TestPlacer hearth("CoW.Site.Hearth", {}, 5);   // a Hearth and no Nexus: HearthNexus cannot hold
    CoWSiteRegistry registry;
    ASSERT_TRUE(registry.Register(&hearth));
    TestSession s;
    Generate(s, 12, &registry);
    const uint32_t revision = s.Revision();
    const SiteSet sitesBefore = s.World().sites;

    const Json::Value r = s.Validate(Obj());
    ASSERT_TRUE(r["success"].asBool()) << r.toStyledString();
    EXPECT_FALSE(r["ok"].asBool());
    const Json::Value* g = FindGuarantee(r["guarantees"], "CoW.Guarantee.HearthNexus");
    ASSERT_NE(g, nullptr);
    EXPECT_FALSE((*g)["ok"].asBool());
    EXPECT_EQ((*g)["counts"]["violatingCells"].asUInt(), 1u);
    EXPECT_EQ(s.Revision(), revision);
    EXPECT_TRUE(s.World().sites == sitesBefore);
    EXPECT_EQ(r["revision"].asUInt(), revision);
}

TEST_F(WorldGenSessionValidateTest, AC13_ValidateAfterImportOfAHandEditedWorldFindsTheViolation)
{
    TestPlacer hearth("CoW.Site.Hearth", {}, 5);
    CoWSiteRegistry registry;
    ASSERT_TRUE(registry.Register(&hearth));
    {
        TestSession a;
        Generate(a, 13, &registry);
        Json::Value e = Obj(); e["name"] = "edited";
        ASSERT_TRUE(a.ExportWorld(e)["success"].asBool());
    }
    TestSession b;
    b.SetWorldDirectories(mDeployed, mOut);
    Json::Value i = Obj(); i["name"] = "edited";
    ASSERT_TRUE(b.ImportWorld(i)["success"].asBool());
    const uint32_t revision = b.Revision();

    const Json::Value r = b.Validate(Obj());
    ASSERT_TRUE(r["success"].asBool()) << r.toStyledString();
    EXPECT_FALSE(r["ok"].asBool());
    const Json::Value* g = FindGuarantee(r["guarantees"], "CoW.Guarantee.HearthNexus");
    ASSERT_NE(g, nullptr);
    EXPECT_EQ((*g)["counts"]["violatingCells"].asUInt(), 1u);
    EXPECT_EQ(b.Revision(), revision);
    EXPECT_TRUE(b.IsLocked());
}

TEST_F(WorldGenSessionValidateTest, ValidateBeforeAnyWorldIsAnErrorAndRefusedDuringARun)
{
    TestSession none;
    EXPECT_FALSE(none.Validate(Obj())["success"].asBool());

    AttemptGatedPlacer gated("Test.Gated", 3);
    CoWSiteRegistry registry;
    ASSERT_TRUE(registry.Register(&gated));
    TestSession s;
    Generate(s, 14);
    s.SetSiteRegistry(&registry);
    ASSERT_TRUE(s.Regenerate(Obj())["success"].asBool());
    s.Tick(mRng);
    ASSERT_TRUE(s.IsGenerating());
    const Json::Value busy = s.Validate(Obj());
    EXPECT_FALSE(busy["success"].asBool());
    EXPECT_EQ(busy["error"].asString(), "busy");
    EXPECT_TRUE(busy["busy"].asBool());
    TickUntilIdle(s, mRng);
}

// ---- report evidence --------------------------------------------------------------------------------------------------------

TEST_F(WorldGenSessionValidateTest, ReportEvidenceCountsEqualTheArraySizesAndNameTheRevision)
{
    uint32_t hearthCell = 0, nexusCell = 0;
    ASSERT_TRUE(FindLowlandPair(15, hearthCell, nexusCell));
    TestPlacer hearth("CoW.Site.Hearth", {}, hearthCell);
    TestPlacer nexus("CoW.Site.Nexus", {}, nexusCell);
    CoWSiteRegistry registry;
    ASSERT_TRUE(registry.Register(&hearth));
    ASSERT_TRUE(registry.Register(&nexus));
    TestSession s;
    Generate(s, 15, &registry);
    ASSERT_TRUE(s.Validate(Obj())["ok"].asBool());

    const Json::Value r = s.Report(Obj());
    ASSERT_TRUE(r["success"].asBool());
    const Json::Value& fin = r["finalValidation"];
    EXPECT_EQ(fin["worldRevision"].asUInt(), s.Revision());
    const Json::Value* g = FindGuarantee(fin["guarantees"], "CoW.Guarantee.HearthNexus");
    ASSERT_NE(g, nullptr);
    EXPECT_TRUE((*g)["ok"].asBool());
    EXPECT_EQ((*g)["counts"]["proofPaths"].asUInt(), 1u);
    EXPECT_EQ((*g)["counts"]["proofPaths"].asUInt(), (*g)["proofPaths"].size());
    EXPECT_EQ((*g)["counts"]["violatingCells"].asUInt(), (*g)["violatingCells"].size());
    EXPECT_EQ((*g)["counts"]["violatingPairs"].asUInt(), (*g)["violatingPairs"].size());
    const Json::Value& path = (*g)["proofPaths"][0u];
    EXPECT_EQ(path["from"].asUInt(), hearthCell);
    EXPECT_EQ(path["to"].asUInt(), nexusCell);
    EXPECT_GE(path["cells"].size(), 2u);
    for (const Json::Value& gi : fin["guarantees"])
    {
        EXPECT_EQ(gi["counts"]["violatingCells"].asUInt(), gi["violatingCells"].size());
        EXPECT_EQ(gi["counts"]["violatingPairs"].asUInt(), gi["violatingPairs"].size());
    }
}

TEST_F(WorldGenSessionValidateTest, ReportWithoutValidateStillCarriesTheGenerationEvidenceAndPagesLargeLists)
{
    TestPlacer hearth("CoW.Site.Hearth", {}, 5);
    CoWSiteRegistry registry;
    ASSERT_TRUE(registry.Register(&hearth));
    TestSession s;
    Generate(s, 16, &registry);
    const Json::Value r = s.Report(Obj());
    const Json::Value& fin = r["finalValidation"];
    EXPECT_EQ(fin["worldRevision"].asUInt(), r["worldRevision"].asUInt());
    ASSERT_GE(fin["guarantees"].size(), 1u);
    for (const Json::Value& gi : fin["guarantees"])
    {
        EXPECT_EQ(gi["counts"]["violatingCells"].asUInt(), gi["violatingCells"].size());
    }

    // Filtering to one guarantee returns just that one.
    Json::Value a = Obj(); a["guarantee"] = "HexWorld.Guarantee.WorldLayout";
    const Json::Value one = s.Report(a);
    ASSERT_EQ(one["finalValidation"]["guarantees"].size(), 1u);
    EXPECT_EQ(one["finalValidation"]["guarantees"][0u]["name"].asString(), "HexWorld.Guarantee.WorldLayout");
}

TEST_F(WorldGenSessionValidateTest, ValidateEvidenceGoesStaleWhenTheRevisionMoves)
{
    TestPlacer hearth("CoW.Site.Hearth", {}, 5);
    CoWSiteRegistry registry;
    ASSERT_TRUE(registry.Register(&hearth));
    TestSession s;
    Generate(s, 17, &registry);
    ASSERT_FALSE(s.Validate(Obj())["ok"].asBool());
    Json::Value regen = Obj(); regen["seed"] = 18;
    ASSERT_TRUE(s.Regenerate(regen)["success"].asBool());
    TickUntilIdle(s, mRng);
    // The report now describes the new generation, not the old Validate result.
    const Json::Value r = s.Report(Obj());
    EXPECT_EQ(r["finalValidation"]["worldRevision"].asUInt(), s.Revision());
    EXPECT_NE(s.EvidenceRevision(), s.Revision());
}

// ---- worlds ----------------------------------------------------------------------------------------------------------------

TEST_F(WorldGenSessionValidateTest, WorldsMergesOutAndDeployedAndOutWinsOnAClash)
{
    ASSERT_TRUE(WriteWorldFile(mOut, "alpha", "{}", false).Ok());
    ASSERT_TRUE(WriteWorldFile(mOut, "shared", "{}", false).Ok());
    ASSERT_TRUE(WriteWorldFile(mDeployed, "shared", "{}", false).Ok());
    ASSERT_TRUE(WriteWorldFile(mDeployed, "zeta", "{}", false).Ok());
    std::error_code ec;
    fs::create_directories(mOut, ec);
    { std::ofstream junk((fs::path(mOut) / "not a world.txt").string()); junk << "x"; }

    TestSession s;
    s.SetWorldDirectories(mDeployed, mOut);
    const Json::Value r = s.Worlds();
    ASSERT_TRUE(r["success"].asBool());
    const Json::Value& list = r["worlds"];
    ASSERT_EQ(list.size(), 3u) << r.toStyledString();
    EXPECT_EQ(list[0u]["name"].asString(), "alpha");
    EXPECT_EQ(list[0u]["source"].asString(), "out");
    EXPECT_EQ(list[1u]["name"].asString(), "shared");
    EXPECT_EQ(list[1u]["source"].asString(), "out");
    EXPECT_EQ(list[2u]["name"].asString(), "zeta");
    EXPECT_EQ(list[2u]["source"].asString(), "deployed");
}

TEST_F(WorldGenSessionValidateTest, WorldsWithNoDirectoriesIsEmpty)
{
    TestSession s;
    const Json::Value r = s.Worlds();
    EXPECT_TRUE(r["success"].asBool());
    EXPECT_EQ(r["worlds"].size(), 0u);
}

// ---- import errors ---------------------------------------------------------------------------------------------------------

TEST_F(WorldGenSessionValidateTest, ImportOfBadJsonReportsLineAndColumn)
{
    ASSERT_TRUE(WriteWorldFile(mOut, "broken", "{\n  \"format\": \"cow.world\",\n  \"version\": ,\n}\n", false).Ok());
    TestSession s;
    s.SetWorldDirectories(mDeployed, mOut);
    Json::Value a = Obj(); a["name"] = "broken";
    const Json::Value r = s.ImportWorld(a);
    ASSERT_FALSE(r["success"].asBool());
    EXPECT_EQ(r["line"].asUInt(), 3u) << r.toStyledString();
    EXPECT_GT(r["column"].asUInt(), 0u);
    EXPECT_EQ(s.Revision(), 0u);
}

TEST_F(WorldGenSessionValidateTest, ImportOfAStructuralErrorHasNoLineAndColumn)
{
    ASSERT_TRUE(WriteWorldFile(mOut, "wrongformat", "{\"format\":\"nope\"}", false).Ok());
    TestSession s;
    s.SetWorldDirectories(mDeployed, mOut);
    Json::Value a = Obj(); a["name"] = "wrongformat";
    const Json::Value r = s.ImportWorld(a);
    ASSERT_FALSE(r["success"].asBool());
    EXPECT_FALSE(r.isMember("line"));
}

// ---- status.unavailableLayers -----------------------------------------------------------------------------------------------

namespace
{
bool Contains(const Json::Value& list, const char* id)
{
    for (const Json::Value& v : list) { if (v.asString() == id) { return true; } }
    return false;
}
}

TEST_F(WorldGenSessionValidateTest, StatusUnavailableLayersTracksWhatTheWorldHasData)
{
    TestSession s;
    // Nothing generated yet: every layer is unavailable.
    const Json::Value before = s.Status();
    ASSERT_TRUE(before["unavailableLayers"].isArray());
    EXPECT_TRUE(Contains(before["unavailableLayers"], "elevation"));

    // Live preview: terrain exists, regions do not.
    ASSERT_TRUE(s.Tick(mRng));
    const Json::Value preview = s.Status();
    EXPECT_TRUE(Contains(preview["unavailableLayers"], "regions"));
    EXPECT_FALSE(Contains(preview["unavailableLayers"], "elevation"));

    // Regenerate: everything has data.
    Generate(s, 21);
    const Json::Value full = s.Status();
    ASSERT_TRUE(full["unavailableLayers"].isArray());
    EXPECT_EQ(full["unavailableLayers"].size(), 0u) << full["unavailableLayers"].toStyledString();

    // Imported world: only what the file can reproduce.
    Json::Value e = Obj(); e["name"] = "imp";
    ASSERT_TRUE(s.ExportWorld(e)["success"].asBool());
    TestSession b;
    b.SetWorldDirectories(mDeployed, mOut);
    Json::Value i = Obj(); i["name"] = "imp";
    ASSERT_TRUE(b.ImportWorld(i)["success"].asBool());
    const Json::Value imported = b.Status();
    EXPECT_TRUE(Contains(imported["unavailableLayers"], "coastDistance"));
    EXPECT_TRUE(Contains(imported["unavailableLayers"], "temperature"));
    EXPECT_FALSE(Contains(imported["unavailableLayers"], "elevation"));
    EXPECT_FALSE(Contains(imported["unavailableLayers"], "regions"));
}

// ---- injected guarantee spec (generic special sites task 3) ----------------------------------------------------------------

namespace
{
std::vector<std::string> NamesOf(const Json::Value& list)
{
    std::vector<std::string> names;
    for (const Json::Value& g : list) { names.push_back(g["name"].asString()); }
    return names;
}
}

TEST_F(WorldGenSessionValidateTest, EmptySpecValidatesWithOnlyRegistryGuaranteesAndLayout)
{
    WorldGenSession s;   // default spec is empty
    Dia::Attribute::ParamError perr;
    ASSERT_TRUE(s.LoadRules(TestSidecarText().c_str(), perr));
    s.SetBiomeRegistry(&Draoi::HexWorld::Testing::ShippedBiomeRegistry());
    EXPECT_TRUE(s.GetGuaranteeSpec().guarantees.empty());
    Generate(s, 11);
    const Json::Value r = s.Validate(Obj());
    ASSERT_TRUE(r["success"].asBool()) << r.toStyledString();
    const std::vector<std::string> names = NamesOf(r["guarantees"]);
    EXPECT_EQ(FindGuarantee(r["guarantees"], "CoW.Guarantee.HearthNexus"), nullptr);
    EXPECT_EQ(FindGuarantee(r["guarantees"], "CoW.Guarantee.DynastySpacing"), nullptr);
    EXPECT_EQ(FindGuarantee(r["guarantees"], "CoW.Guarantee.WorldThrone"), nullptr);
    EXPECT_NE(FindGuarantee(r["guarantees"], "HexWorld.Guarantee.WorldLayout"), nullptr);
}

TEST_F(WorldGenSessionValidateTest, ShippedSpecKeepsFileOrderNamesAndSkipsTheThroneRuleWithoutThrones)
{
    TestSession s;
    Generate(s, 11);
    const Json::Value r = s.Validate(Obj());
    ASSERT_TRUE(r["success"].asBool()) << r.toStyledString();
    const std::vector<std::string> names = NamesOf(r["guarantees"]);
    ASSERT_GE(names.size(), 3u);
    EXPECT_EQ(names[0], "CoW.Guarantee.HearthNexus");
    EXPECT_EQ(names[1], "CoW.Guarantee.DynastySpacing");
    EXPECT_EQ(std::count(names.begin(), names.end(), std::string("CoW.Guarantee.WorldThrone")), 0);   // S-1 places no thrones
    EXPECT_EQ(names.back(), "HexWorld.Guarantee.WorldLayout");
    EXPECT_EQ(s.GuaranteeLabel(Dia::Core::StringCRC("CoW.Guarantee.WorldThrone").Value()), "CoW.Guarantee.WorldThrone");
    EXPECT_EQ(s.GuaranteeLabel(0xDEADBEEFu), "0xDEADBEEF");
}

TEST_F(WorldGenSessionValidateTest, CustomSpecEntriesAppearByTheirIdAndAVariantCoverWithoutSitesIsSkipped)
{
    GuaranteeSpec spec;
    GuaranteeEntry camps;
    camps.id = "Test.Guarantee.Camps"; camps.label = "Camp spacing"; camps.type = GuaranteeRuleType::MinSpacing;
    camps.kind = "Test.Site.Camp"; camps.minimum = 3.0f; camps.noun = "camp"; camps.nounPlural = "camps";
    camps.minimumKey = "minimumSpacing"; camps.distanceKey = "pairDistance";
    GuaranteeEntry totems;
    totems.id = "Test.Guarantee.Totems"; totems.type = GuaranteeRuleType::VariantCover;
    totems.kind = "Test.Site.Totem"; totems.variantCount = 2; totems.toStratumIndex = 1;
    totems.noun = "totem"; totems.nounPlural = "totems"; totems.variantNoun = "kind"; totems.targetNoun = "Shores";
    totems.countKey = "totemsOfType"; totems.targetCountKey = "shoreCells";
    spec.guarantees = { camps, totems };

    TestSession s;
    s.SetGuaranteeSpec(spec);
    Generate(s, 11);
    const Json::Value r = s.Validate(Obj());
    ASSERT_TRUE(r["success"].asBool()) << r.toStyledString();
    const std::vector<std::string> names = NamesOf(r["guarantees"]);
    ASSERT_GE(names.size(), 2u);
    EXPECT_EQ(names[0], "Test.Guarantee.Camps");
    EXPECT_EQ(FindGuarantee(r["guarantees"], "Test.Guarantee.Totems"), nullptr);
    EXPECT_EQ(FindGuarantee(r["guarantees"], "CoW.Guarantee.HearthNexus"), nullptr);
    EXPECT_EQ(s.GuaranteeLabel(Dia::Core::StringCRC("Test.Guarantee.Camps").Value()), "Test.Guarantee.Camps");
}
