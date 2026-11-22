// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/reachability-validator.md (Placeholder Profile)
// TerrainNavigationTable and TerrainTableNavigationProfile: the S-1 default navigation profile.
#include <gtest/gtest.h>
#include <Shared/WorldGeneration/TerrainTableNavigationProfile.h>
#include <Shared/WorldGeneration/NavigationQuery.h>
#include <Shared/WorldGeneration/CoWTesting/CoWTerrainRulesTestHelpers.h>
#include <Shared/WorldGeneration/Testing/ValidationTestWorlds.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

using namespace CoW::WorldGeneration;
using namespace CoW::WorldGeneration::Testing;
using Dia::Core::StringCRC;
using Dia::Pathfinding::IPathCostProvider;
using CoW::WorldGeneration::TestRules;

TEST(TerrainNavigationTable, NavigationTableFromRules)
{
    // Literals equal the values of the deleted terrain_navigation.json (now the sidecar terrain.passable / terrain.cost maps).
    const TerrainNavigationTable t = TerrainNavigationTable::FromRules(TestRules());
    const bool  passable[] = { false, false, false, true, true, true, true, true, false, false, true, false };
    const float cost[]     = { 1.0f, 1.0f, 1.0f, 1.5f, 1.0f, 1.5f, 2.0f, 4.0f, 1.0f, 1.0f, 1.0f, 1.0f };
    static_assert(sizeof(passable) == kNavigationTerrainCount * sizeof(bool), "one row per TerrainType");
    for (int i = 0; i < kNavigationTerrainCount; ++i)
    {
        const TerrainType type = static_cast<TerrainType>(i);
        EXPECT_EQ(t.Get(type).passable, passable[i]) << TerrainTypeName(type);
        EXPECT_FLOAT_EQ(t.Get(type).cost, cost[i]) << TerrainTypeName(type);
    }
}

TEST(TerrainNavigationTable, OldNavigationFileIsGoneAndUncatalogued)
{
    std::error_code ec;
    std::filesystem::path dir = std::filesystem::absolute(std::filesystem::path(__FILE__), ec).parent_path();
    while (!dir.empty())
    {
        const std::filesystem::path assets = dir / "Cluiche/Assets/CoW";
        if (std::filesystem::exists(assets / "assets.catalogue.json", ec))
        {
            EXPECT_FALSE(std::filesystem::exists(assets / "Global/WorldGen/terrain_navigation.json", ec));
            std::ifstream in(assets / "assets.catalogue.json");
            std::stringstream ss;
            ss << in.rdbuf();
            EXPECT_EQ(ss.str().find("terrain_navigation"), std::string::npos);
            return;
        }
        const std::filesystem::path parent = dir.parent_path();
        if (parent == dir) { break; }
        dir = parent;
    }
    ADD_FAILURE() << "assets.catalogue.json not found above " << __FILE__;
}

TEST(TerrainTableNavigationProfile, IdAndCostProviderFollowTheTable)
{
    WorldStructure w = MakeAllLandWorld(4, 3);
    SetTerrain(w, 1, 0, TerrainType::Mountain);
    SetTerrain(w, 2, 0, TerrainType::Lake);
    TerrainTableNavigationProfile profile(TestRules());
    EXPECT_EQ(profile.GetId(), StringCRC("HexWorld.Nav.TerrainTable"));
    const auto costs = profile.CreateCostProvider(w);
    ASSERT_NE(costs, nullptr);
    EXPECT_FLOAT_EQ(costs->GetCost({ 0, 0 }, { 0, 1 }), 1.0f);                                   // Lowland
    EXPECT_FLOAT_EQ(costs->GetCost({ 0, 0 }, { 1, 0 }), TerrainNavigationTable::FromRules(TestRules()).Get(TerrainType::Mountain).cost);
    EXPECT_LT(costs->GetCost({ 1, 0 }, { 2, 0 }), 0.0f);                                          // Lake blocked
    EXPECT_LT(costs->GetCost({ 0, 0 }, { -1, 0 }), 0.0f);                                         // off the map
    EXPECT_LT(costs->GetCost({ 0, 0 }, { 4, 0 }), 0.0f);
    EXPECT_LT(costs->GetCost({ 0, 0 }, { 0, 3 }), 0.0f);
}

TEST(TerrainTableNavigationProfile, CustomTableChangesReachability)
{
    WorldStructure w = MakeAllLandWorld(6, 1);
    for (int x = 2; x < 4; ++x) { SetTerrain(w, x, 0, TerrainType::Hills); }
    TerrainTableNavigationProfile open(TestRules());
    TerrainNavigationTable blockedHills = TerrainNavigationTable::FromRules(TestRules());
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
    TerrainTableNavigationProfile profile(TestRules());
    std::vector<const INavigationProfile*> p = { &profile };
    const NavigationQuery q(w, p);
    ProofPath path;
    ASSERT_TRUE(q.FindProofPath(Idx(w, 0, 3), Idx(w, 6, 3), path));
    bool usedPass = false;
    for (uint32_t c : path.cells) { if (w.Cells()[c].terrain == TerrainType::Pass) { usedPass = true; } }
    EXPECT_TRUE(usedPass);
}
