// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/reachability-validator.md (AC5, AC8, Navigation)
// NavigationQuery: reachability under injected profiles, proof paths, any-profile semantics.
#include <gtest/gtest.h>
#include <Shared/World/Testing/BiomeTestRegistry.h>
#include <Shared/WorldGeneration/NavigationQuery.h>
#include <Shared/WorldGeneration/Testing/TestProfiles.h>
#include <Shared/WorldGeneration/Testing/ValidationTestWorlds.h>

#include <vector>
#include <Shared/World/Testing/ZoneTestIndices.h>

using namespace CoW::WorldGeneration;
using namespace CoW::WorldGeneration::Testing;
using namespace CoW::World::Testing;
using Dia::Core::StringCRC;

namespace
{

NavigationQuery MakeQuery(const WorldStructure& w, std::initializer_list<const INavigationProfile*> profiles)
{
    std::vector<const INavigationProfile*> v(profiles);
    return NavigationQuery(w, v);
}

bool PathAvoidsTerrain(const WorldStructure& w, const ProofPath& p, std::initializer_list<TerrainType> blocked)
{
    for (uint32_t c : p.cells)
    {
        for (TerrainType t : blocked) { if (w.Cells()[c].terrain == t) { return false; } }
    }
    return true;
}

const std::initializer_list<TerrainType> kWater = { TerrainType::DeepWater, TerrainType::ShallowWater, TerrainType::Lake };

} // namespace

// ---- AC5: navigability is respected, not grid adjacency -------------------------------------------------------------

TEST(NavigationQuery, AC5_GraphConnectedButBlockedTerrainIsUnreachable)
{
    const WorldStructure w = MakeChannelWorld(10, 5, 4, 2);
    BlockWaterProfile p("P");
    const NavigationQuery q = MakeQuery(w, { &p });
    const uint32_t a = Idx(w, 1, 2), b = Idx(w, 8, 2);
    EXPECT_FALSE(q.IsReachable(a, b));
    ProofPath path;
    EXPECT_FALSE(q.FindProofPath(a, b, path));
}

TEST(NavigationQuery, AC5_PassableTerrainIsReachableWithValidProofPath)
{
    const WorldStructure w = MakeAllLandWorld(10, 5);
    BlockWaterProfile p("P");
    const NavigationQuery q = MakeQuery(w, { &p });
    const uint32_t a = Idx(w, 0, 0), b = Idx(w, 9, 4);
    EXPECT_TRUE(q.IsReachable(a, b));
    ProofPath path;
    ASSERT_TRUE(q.FindProofPath(a, b, path));
    EXPECT_EQ(path.from, a);
    EXPECT_EQ(path.to, b);
    EXPECT_EQ(path.profileCrc, StringCRC("P").Value());
    EXPECT_TRUE(IsValidWalk(w, path));
    EXPECT_TRUE(PathAvoidsTerrain(w, path, kWater));
}

TEST(NavigationQuery, ProofPathRoutesAroundBlockedTerrainWhenAGapExists)
{
    WorldStructure w = MakeChannelWorld(10, 5, 4, 2);
    for (int x = 4; x < 6; ++x) { SetLand(w, x, 4, kZoneWild, B("Biome.Grassland")); }   // land bridge on the last row
    BlockWaterProfile p("P");
    const NavigationQuery q = MakeQuery(w, { &p });
    ProofPath path;
    ASSERT_TRUE(q.FindProofPath(Idx(w, 1, 0), Idx(w, 8, 0), path));
    EXPECT_TRUE(IsValidWalk(w, path));
    EXPECT_TRUE(PathAvoidsTerrain(w, path, kWater));
    EXPECT_GT(path.cells.Size(), HexSteps(w, path.from, path.to) + 1);   // had to detour
}

// ---- AC8: any-profile semantics -------------------------------------------------------------------------------------

TEST(NavigationQuery, AC8_AnyProfileSemantics)
{
    const WorldStructure w = MakeChannelWorld(10, 5, 4, 2);
    BlockWaterProfile blocking("P");
    AllowWaterProfile allowing("Q");
    const uint32_t a = Idx(w, 1, 2), b = Idx(w, 8, 2);
    EXPECT_FALSE(MakeQuery(w, { &blocking }).IsReachable(a, b));
    EXPECT_TRUE(MakeQuery(w, { &allowing }).IsReachable(a, b));
    EXPECT_TRUE(MakeQuery(w, { &blocking, &allowing }).IsReachable(a, b));
}

TEST(NavigationQuery, AC8_ProofPathNamesTheFirstProfileThatSucceeds)
{
    const WorldStructure w = MakeChannelWorld(10, 5, 4, 2);
    BlockWaterProfile blocking("P");
    AllowWaterProfile q1("Q1"), q2("Q2");
    const uint32_t a = Idx(w, 1, 2), b = Idx(w, 8, 2);
    ProofPath path;
    ASSERT_TRUE(MakeQuery(w, { &blocking, &q1, &q2 }).FindProofPath(a, b, path));
    EXPECT_EQ(path.profileCrc, StringCRC("Q1").Value());
    ASSERT_TRUE(MakeQuery(w, { &q2, &q1 }).FindProofPath(a, b, path));
    EXPECT_EQ(path.profileCrc, StringCRC("Q2").Value());
}

TEST(NavigationQuery, NoProfilesMeansNothingIsReachable)
{
    const WorldStructure w = MakeAllLandWorld(6, 4);
    const NavigationQuery q = MakeQuery(w, {});
    EXPECT_FALSE(q.IsReachable(Idx(w, 0, 0), Idx(w, 1, 0)));
    EXPECT_FALSE(q.IsReachable(Idx(w, 0, 0), Idx(w, 0, 0)));
}

// ---- Edges ----------------------------------------------------------------------------------------------------------

TEST(NavigationQuery, SameCellIsReachableWithOneCellPath)
{
    const WorldStructure w = MakeAllLandWorld(5, 5);
    BlockWaterProfile p("P");
    const NavigationQuery q = MakeQuery(w, { &p });
    ProofPath path;
    ASSERT_TRUE(q.FindProofPath(Idx(w, 2, 2), Idx(w, 2, 2), path));
    ASSERT_EQ(path.cells.Size(), 1u);
    EXPECT_EQ(path.cells[0], Idx(w, 2, 2));
}

TEST(NavigationQuery, OutOfRangeCellsAreUnreachableNotACrash)
{
    const WorldStructure w = MakeAllLandWorld(5, 5);
    BlockWaterProfile p("P");
    const NavigationQuery q = MakeQuery(w, { &p });
    const uint32_t n = static_cast<uint32_t>(w.CellCount());
    EXPECT_FALSE(q.IsReachable(n, 0));
    EXPECT_FALSE(q.IsReachable(0, n));
    EXPECT_FALSE(q.IsReachable(kNoCellIndex, kNoCellIndex));
}

TEST(NavigationQuery, EmptyWorldIsUnreachableNotACrash)
{
    WorldStructure w;
    w.Resize({ 0, 0 });
    BlockWaterProfile p("P");
    const NavigationQuery q = MakeQuery(w, { &p });
    EXPECT_FALSE(q.IsReachable(0, 0));
}

TEST(NavigationQuery, WindingPathLongerThanDiaPathResultCapacityIsFound)
{
    // Serpentine corridor: land rows separated by water rows, one gap per wall alternating ends. Path > 256 cells.
    const int w = 40, h = 17;
    WorldStructure world = MakeBlankWorld(w, h);
    for (int y = 0; y < h; y += 2) { FillLand(world, 0, y, w, y + 1, kZoneWild, B("Biome.Grassland")); }
    for (int y = 1; y < h; y += 2)
    {
        const int gap = (y / 2) % 2 == 0 ? w - 1 : 0;
        SetLand(world, gap, y, kZoneWild, B("Biome.Grassland"));
    }
    BlockWaterProfile p("P");
    const NavigationQuery q = MakeQuery(world, { &p });
    ProofPath path;
    ASSERT_TRUE(q.FindProofPath(Idx(world, 0, 0), Idx(world, 0, h - 1), path));
    EXPECT_GT(path.cells.Size(), 256u);
    EXPECT_TRUE(IsValidWalk(world, path));
    EXPECT_TRUE(PathAvoidsTerrain(world, path, kWater));
}

TEST(NavigationQuery, DeterministicAcrossRepeatedQueries)
{
    const WorldStructure w = MakeAllLandWorld(12, 9);
    BlockWaterProfile p("P");
    const NavigationQuery q = MakeQuery(w, { &p });
    ProofPath a, b;
    ASSERT_TRUE(q.FindProofPath(Idx(w, 0, 0), Idx(w, 11, 8), a));
    ASSERT_TRUE(q.FindProofPath(Idx(w, 0, 0), Idx(w, 11, 8), b));
    EXPECT_EQ(a, b);
    const NavigationQuery q2 = MakeQuery(w, { &p });
    ASSERT_TRUE(q2.FindProofPath(Idx(w, 0, 0), Idx(w, 11, 8), b));
    EXPECT_EQ(a, b);
}

// ---- FindProofPathToAny ---------------------------------------------------------------------------------------------

TEST(NavigationQuery, ToAnyReachesTheNearestTarget)
{
    const WorldStructure w = MakeAllLandWorld(20, 3);
    BlockWaterProfile p("P");
    const NavigationQuery q = MakeQuery(w, { &p });
    const std::vector<uint32_t> targets = { Idx(w, 18, 1), Idx(w, 5, 1), Idx(w, 12, 1) };
    ProofPath path;
    ASSERT_TRUE(q.FindProofPathToAny(Idx(w, 0, 1), targets, path));
    EXPECT_EQ(path.to, Idx(w, 5, 1));
    EXPECT_TRUE(IsValidWalk(w, path));
}

TEST(NavigationQuery, ToAnyTieGoesToTheLowestCellIndex)
{
    const WorldStructure w = MakeAllLandWorld(9, 9);
    BlockWaterProfile p("P");
    const NavigationQuery q = MakeQuery(w, { &p });
    const uint32_t from = Idx(w, 4, 4);
    // Two cells at the same hex distance from 'from'.
    uint32_t t1 = Idx(w, 4, 2), t2 = Idx(w, 4, 6);
    ASSERT_EQ(HexSteps(w, from, t1), HexSteps(w, from, t2));
    const std::vector<uint32_t> targets = { std::max(t1, t2), std::min(t1, t2) };
    ProofPath path;
    ASSERT_TRUE(q.FindProofPathToAny(from, targets, path));
    EXPECT_EQ(path.to, std::min(t1, t2));
}

TEST(NavigationQuery, ToAnySkipsUnreachableTargetsAndFailsWhenNoneReachable)
{
    const WorldStructure w = MakeChannelWorld(10, 5, 4, 2);
    BlockWaterProfile p("P");
    const NavigationQuery q = MakeQuery(w, { &p });
    ProofPath path;
    const std::vector<uint32_t> mixed = { Idx(w, 8, 2), Idx(w, 3, 4) };
    ASSERT_TRUE(q.FindProofPathToAny(Idx(w, 0, 0), mixed, path));
    EXPECT_EQ(path.to, Idx(w, 3, 4));
    const std::vector<uint32_t> far = { Idx(w, 8, 2), Idx(w, 9, 0) };
    EXPECT_FALSE(q.FindProofPathToAny(Idx(w, 0, 0), far, path));
    EXPECT_FALSE(q.FindProofPathToAny(Idx(w, 0, 0), {}, path));
    const std::vector<uint32_t> bad = { static_cast<uint32_t>(w.CellCount()), kNoCellIndex };
    EXPECT_FALSE(q.FindProofPathToAny(Idx(w, 0, 0), bad, path));
}

TEST(NavigationQuery, ToAnyUsesAnyProfile)
{
    const WorldStructure w = MakeChannelWorld(10, 5, 4, 2);
    BlockWaterProfile blocking("P");
    AllowWaterProfile allowing("Q");
    const std::vector<uint32_t> targets = { Idx(w, 8, 2) };
    ProofPath path;
    EXPECT_FALSE(MakeQuery(w, { &blocking }).FindProofPathToAny(Idx(w, 0, 2), targets, path));
    ASSERT_TRUE(MakeQuery(w, { &blocking, &allowing }).FindProofPathToAny(Idx(w, 0, 2), targets, path));
    EXPECT_EQ(path.profileCrc, StringCRC("Q").Value());
}
