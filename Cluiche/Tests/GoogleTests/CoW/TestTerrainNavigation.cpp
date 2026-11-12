// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/reachability-validator.md (Placeholder Profile)
// TerrainNavigationTable and TerrainTableNavigationProfile: the S-1 default navigation profile.
#include <gtest/gtest.h>
#include <Shared/WorldGeneration/TerrainTableNavigationProfile.h>
#include <Shared/WorldGeneration/NavigationQuery.h>
#include <Shared/WorldGeneration/Testing/ValidationTestWorlds.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

using namespace CoW::WorldGeneration;
using namespace CoW::WorldGeneration::Testing;
using Dia::Core::StringCRC;
using Dia::Pathfinding::IPathCostProvider;

namespace
{

std::filesystem::path FindShippedTable()
{
    std::error_code ec;
    std::filesystem::path dir = std::filesystem::absolute(std::filesystem::path(__FILE__), ec).parent_path();
    while (!dir.empty())
    {
        const std::filesystem::path candidate = dir / "Cluiche/Assets/CoW/Global/WorldGen/terrain_navigation.json";
        if (std::filesystem::exists(candidate, ec)) { return candidate; }
        const std::filesystem::path parent = dir.parent_path();
        if (parent == dir) { break; }
        dir = parent;
    }
    return {};
}

bool HasWarning(const RuleWarnings& w, const char* text)
{
    for (unsigned i = 0; i < w.Size(); ++i) { if (w[i] == StringCRC(text)) { return true; } }
    return false;
}

Json::Value Parse(const char* text)
{
    Json::Value v;
    Json::CharReaderBuilder b;
    std::string errs;
    std::istringstream in(text);
    EXPECT_TRUE(Json::parseFromStream(b, in, &v, &errs)) << errs;
    return v;
}

} // namespace

TEST(TerrainNavigationTable, DefaultsBlockWaterAndImpassableRock)
{
    const TerrainNavigationTable t = TerrainNavigationTable::Defaults();
    for (TerrainType blocked : { TerrainType::DeepWater, TerrainType::ShallowWater, TerrainType::Lake,
                                 TerrainType::Peak, TerrainType::Cliff, TerrainType::Volcano })
    {
        EXPECT_FALSE(t.Get(blocked).passable) << TerrainTypeName(blocked);
    }
    for (TerrainType open : { TerrainType::River, TerrainType::Lowland, TerrainType::Hills, TerrainType::Highland,
                              TerrainType::Mountain, TerrainType::Pass })
    {
        EXPECT_TRUE(t.Get(open).passable) << TerrainTypeName(open);
        EXPECT_GT(t.Get(open).cost, 0.0f);
    }
    EXPECT_GT(t.Get(TerrainType::Mountain).cost, t.Get(TerrainType::Pass).cost);   // passes are the cheap way through
}

TEST(TerrainNavigationTable, JsonRoundTripIsLossless)
{
    TerrainNavigationTable t = TerrainNavigationTable::Defaults();
    t.entries[static_cast<int>(TerrainType::Hills)] = { false, 7.5f };
    RuleWarnings warnings;
    TerrainNavigationTable back;
    ASSERT_TRUE(back.LoadFromJson(t.ToJson(), warnings));
    EXPECT_EQ(warnings.Size(), 0u);
    EXPECT_EQ(back, t);
}

TEST(TerrainNavigationTable, MissingNamesKeepDefaultsAndWarn)
{
    const Json::Value json = Parse(R"({"version":1,"terrain":{"Lowland":{"passable":false,"cost":2.0}}})");
    TerrainNavigationTable t = TerrainNavigationTable::Defaults();
    RuleWarnings warnings;
    ASSERT_TRUE(t.LoadFromJson(json, warnings));
    EXPECT_FALSE(t.Get(TerrainType::Lowland).passable);
    EXPECT_FLOAT_EQ(t.Get(TerrainType::Lowland).cost, 2.0f);
    EXPECT_EQ(t.Get(TerrainType::Mountain), TerrainNavigationTable::Defaults().Get(TerrainType::Mountain));
    EXPECT_TRUE(HasWarning(warnings, "missing:Mountain"));
}

TEST(TerrainNavigationTable, UnknownInvalidAndOutOfRangeEntriesWarn)
{
    const Json::Value json = Parse(R"({"version":1,"terrain":{
        "Bogus":{"passable":true,"cost":1},
        "Hills":{"passable":"yes","cost":1},
        "Highland":{"passable":true,"cost":1000},
        "Mountain":{"passable":true,"cost":0.0},
        "Pass":5}})");
    TerrainNavigationTable t = TerrainNavigationTable::Defaults();
    RuleWarnings warnings;
    ASSERT_TRUE(t.LoadFromJson(json, warnings));
    EXPECT_TRUE(HasWarning(warnings, "unknown:Bogus"));
    EXPECT_TRUE(HasWarning(warnings, "invalid:Hills"));
    EXPECT_TRUE(HasWarning(warnings, "range:Highland"));
    EXPECT_TRUE(HasWarning(warnings, "range:Mountain"));
    EXPECT_TRUE(HasWarning(warnings, "invalid:Pass"));
    EXPECT_FLOAT_EQ(t.Get(TerrainType::Highland).cost, kMaxNavigationCost);
    EXPECT_FLOAT_EQ(t.Get(TerrainType::Mountain).cost, kMinNavigationCost);
    EXPECT_EQ(t.Get(TerrainType::Hills), TerrainNavigationTable::Defaults().Get(TerrainType::Hills));   // invalid entry keeps the default
}

TEST(TerrainNavigationTable, NonObjectRootIsRejectedUnchanged)
{
    TerrainNavigationTable t = TerrainNavigationTable::Defaults();
    RuleWarnings warnings;
    EXPECT_FALSE(t.LoadFromJson(Parse("[1,2]"), warnings));
    EXPECT_TRUE(HasWarning(warnings, "invalid:root"));
    EXPECT_EQ(t, TerrainNavigationTable::Defaults());
    RuleWarnings w2;
    EXPECT_FALSE(t.LoadFromJson(Parse(R"({"version":1,"terrain":3})"), w2));
    EXPECT_TRUE(HasWarning(w2, "invalid:terrain"));
}

TEST(TerrainNavigationTable, ShippedFileMatchesDefaultsAndLoadsClean)
{
    const std::filesystem::path path = FindShippedTable();
    ASSERT_FALSE(path.empty()) << "terrain_navigation.json not found above " << __FILE__;
    std::ifstream in(path, std::ios::binary);
    ASSERT_TRUE(in.good());
    Json::Value root;
    Json::CharReaderBuilder builder;
    std::string errs;
    ASSERT_TRUE(Json::parseFromStream(builder, in, &root, &errs)) << errs;
    TerrainNavigationTable t = TerrainNavigationTable::Defaults();
    RuleWarnings warnings;
    EXPECT_TRUE(t.LoadFromJson(root, warnings));
    EXPECT_EQ(warnings.Size(), 0u);
    EXPECT_EQ(t, TerrainNavigationTable::Defaults());
}

TEST(TerrainTableNavigationProfile, IdAndCostProviderFollowTheTable)
{
    WorldStructure w = MakeAllLandWorld(4, 3);
    SetTerrain(w, 1, 0, TerrainType::Mountain);
    SetTerrain(w, 2, 0, TerrainType::Lake);
    TerrainTableNavigationProfile profile;
    EXPECT_EQ(profile.GetId(), StringCRC("CoW.Nav.TerrainTable"));
    const auto costs = profile.CreateCostProvider(w);
    ASSERT_NE(costs, nullptr);
    EXPECT_FLOAT_EQ(costs->GetCost({ 0, 0 }, { 0, 1 }), 1.0f);                                   // Lowland
    EXPECT_FLOAT_EQ(costs->GetCost({ 0, 0 }, { 1, 0 }), TerrainNavigationTable::Defaults().Get(TerrainType::Mountain).cost);
    EXPECT_LT(costs->GetCost({ 1, 0 }, { 2, 0 }), 0.0f);                                          // Lake blocked
    EXPECT_LT(costs->GetCost({ 0, 0 }, { -1, 0 }), 0.0f);                                         // off the map
    EXPECT_LT(costs->GetCost({ 0, 0 }, { 4, 0 }), 0.0f);
    EXPECT_LT(costs->GetCost({ 0, 0 }, { 0, 3 }), 0.0f);
}

TEST(TerrainTableNavigationProfile, CustomTableChangesReachability)
{
    WorldStructure w = MakeAllLandWorld(6, 1);
    for (int x = 2; x < 4; ++x) { SetTerrain(w, x, 0, TerrainType::Hills); }
    TerrainTableNavigationProfile open;
    TerrainNavigationTable blockedHills = TerrainNavigationTable::Defaults();
    blockedHills.entries[static_cast<int>(TerrainType::Hills)].passable = false;
    TerrainTableNavigationProfile closed(blockedHills);
    const uint32_t a = Idx(w, 0, 0), b = Idx(w, 5, 0);
    std::vector<const INavigationProfile*> po = { &open }, pc = { &closed };
    EXPECT_TRUE(NavigationQuery(w, po).IsReachable(a, b));
    EXPECT_FALSE(NavigationQuery(w, pc).IsReachable(a, b));
}

TEST(TerrainTableNavigationProfile, PassesAreTheCheapRouteThroughMountains)
{
    // A mountain wall one cell thick with a Pass in it: the proof path must use the Pass.
    WorldStructure w = MakeAllLandWorld(7, 5);
    for (int y = 0; y < 5; ++y) { SetTerrain(w, 3, y, TerrainType::Mountain); }
    SetTerrain(w, 3, 4, TerrainType::Pass);
    TerrainTableNavigationProfile profile;
    std::vector<const INavigationProfile*> p = { &profile };
    const NavigationQuery q(w, p);
    ProofPath path;
    ASSERT_TRUE(q.FindProofPath(Idx(w, 0, 3), Idx(w, 6, 3), path));
    bool usedPass = false;
    for (uint32_t c : path.cells) { if (w.Cells()[c].terrain == TerrainType::Pass) { usedPass = true; } }
    EXPECT_TRUE(usedPass);
}
