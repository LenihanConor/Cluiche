// S-1 Nodes: built-in river nodes + Hunting/Resource fill (hand-built worlds + generated-world AC8b checks).
#include <gtest/gtest.h>
#include <Shared/World/Testing/BiomeTestRegistry.h>
#include <Shared/WorldGeneration/NodesCorridor.h>
#include <Shared/WorldGeneration/NodesFill.h>
#include <Shared/WorldGeneration/NodesRiver.h>
#include <Shared/WorldGeneration/RegionRepair.h>
#include <Shared/WorldGeneration/TerrainGenerator.h>
#include <Shared/WorldGeneration/WorldShapePreset.h>
#include <Shared/WorldGeneration/Testing/RegionChecks.h>
#include <Shared/WorldGeneration/Testing/RegionTestWorlds.h>
#include <Shared/WorldGeneration/Testing/TerrainSliderDriver.h>
#include <Shared/WorldGeneration/Testing/TerrainRulesTestHelpers.h>

#include <DiaMaths/Core/RandomStream.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <vector>
#include <Shared/World/Testing/ZoneTestIndices.h>

using namespace CoW::WorldGeneration;
using namespace CoW::WorldGeneration::Testing;
using namespace CoW::World::Testing;
using Dia::Maths::RandomStream;

namespace
{
    TerrainRules Rules() { return TestRules(); }

    constexpr CoW::World::BiomeIndex G = 2;   // Biome.Grassland (shipped row, pinned by BiomeRegistryData.ShippedRowIndicesArePinned)

    // Independent odd-q hex distance (cube coordinates).
    int Dist(const WorldStructure& w, int a, int b)
    {
        const int width = w.dimensions.width;
        auto cube = [&](int i, int& q, int& r) { const int x = i % width, y = i / width; q = x; r = y - (x - (x & 1)) / 2; };
        int qa, ra, qb, rb;
        cube(a, qa, ra); cube(b, qb, rb);
        const int dq = qa - qb, dr = ra - rb;
        return (std::abs(dq) + std::abs(dr) + std::abs(dq + dr)) / 2;
    }

    std::vector<int> CellsOf(const WorldStructure& w, const Dia::Core::StringCRC& kind)
    {
        std::vector<int> v;
        for (const Site& s : w.sites.Of(kind)) { v.push_back(static_cast<int>(s.cell)); }
        std::sort(v.begin(), v.end());
        return v;
    }

    void AddCorridor(WorldStructure& w, int x, int y)
    {
        Site s;
        s.kindCrc = SiteKindId(BuiltinNodeKinds::Corridor());
        s.cell = static_cast<uint32_t>(CellIndex(w, x, y));
        s.region = w.Cells()[s.cell].region;
        w.sites.Add(s);
    }

    // ---- river worlds: 12 wide, `h` tall, land rows 0..h-2, ocean row h-1.
    WorldStructure RiverWorld(int h = 40, uint8_t landmass = 1)
    {
        WorldStructure w = MakeBlankWorld(12, h);
        FillLand(w, 0, 0, 12, h - 1, kZoneFirst, G, landmass);
        for (int x = 0; x < 12; ++x) { SetOcean(w, x, h - 1); }
        return w;
    }

    // Column river from (x, y0) down to (x, y1) flowing south; the last cell flows into (x, y1 + 1).
    void Column(WorldStructure& w, int x, int y0, int y1)
    {
        for (int y = y0; y <= y1; ++y)
        {
            SetRiver(w, x, y);
            FlowRiver(w, x, y, x, y + 1);
        }
    }

    void Finish(WorldStructure& w)
    {
        AssignRegion(w, 0, 0, w.dimensions.width, w.dimensions.height, 1);
        BuildRegionRecords(w);
    }

    bool HasRiverAt(const WorldStructure& w, int x, int y)
    {
        return w.sites.Find(BuiltinNodeKinds::River(), static_cast<uint32_t>(CellIndex(w, x, y))) != nullptr;
    }

    std::vector<int> RiverRows(const WorldStructure& w, int x)
    {
        std::vector<int> rows;
        for (const Site& s : w.sites.Of(BuiltinNodeKinds::River()))
        {
            if (static_cast<int>(s.cell) % w.dimensions.width == x) { rows.push_back(static_cast<int>(s.cell) / w.dimensions.width); }
        }
        std::sort(rows.begin(), rows.end());
        return rows;
    }
}

// ================================================================================ river nodes

TEST(NodesRiverFill, RiverMouthToOceanGetsNodeAndEverySevenUpstream)
{
    WorldStructure w = RiverWorld();
    Column(w, 5, 0, 38);
    Finish(w);
    const RiverNodeResult r = PlaceRiverNodes(w, Rules());
    EXPECT_EQ(r.trees, 1);
    EXPECT_EQ(r.mouthNodes, 1);
    EXPECT_EQ(r.upstreamNodes, 5);
    EXPECT_EQ(RiverRows(w, 5), (std::vector<int>{ 3, 10, 17, 24, 31, 38 }));
    const Site* mouth = w.sites.Find(BuiltinNodeKinds::River(), static_cast<uint32_t>(CellIndex(w, 5, 38)));
    ASSERT_NE(mouth, nullptr);
    EXPECT_TRUE(RiverIsMouth(mouth->flags));
    EXPECT_EQ(mouth->variant, 0);
    EXPECT_EQ(mouth->region, 1);
    const Site* up = w.sites.Find(BuiltinNodeKinds::River(), static_cast<uint32_t>(CellIndex(w, 5, 31)));
    ASSERT_NE(up, nullptr);
    EXPECT_FALSE(RiverIsMouth(up->flags));
    EXPECT_EQ(up->variant, 0);
}

TEST(NodesRiverFill, RiverMouthToLakeGetsNode)
{
    WorldStructure w = RiverWorld(20);
    SetLake(w, 5, 12);                 // lake cell inside the land block
    Column(w, 5, 0, 11);               // (5,11) flows into the lake
    Finish(w);
    PlaceRiverNodes(w, Rules());
    EXPECT_EQ(RiverRows(w, 5), (std::vector<int>{ 4, 11 }));
    EXPECT_TRUE(RiverIsMouth(w.sites.Find(BuiltinNodeKinds::River(), static_cast<uint32_t>(CellIndex(w, 5, 11)))->flags));
}

TEST(NodesRiverFill, ShortRiverGetsMouthOnly)
{
    WorldStructure w = RiverWorld(20);
    Column(w, 5, 14, 18);
    Finish(w);
    const RiverNodeResult r = PlaceRiverNodes(w, Rules());
    EXPECT_EQ(r.mouthNodes, 1);
    EXPECT_EQ(r.upstreamNodes, 0);
    EXPECT_EQ(w.sites.Count(BuiltinNodeKinds::River()), 1);
}

TEST(NodesRiverFill, RiverThatNeverReachesWaterGetsNothing)
{
    WorldStructure w = RiverWorld(30);
    Column(w, 5, 0, 12);               // (5,12) flows into (5,13): plain land
    Finish(w);
    EXPECT_EQ(PlaceRiverNodes(w, Rules()).trees, 0);
    // A river whose last cell has no direction.
    WorldStructure w2 = RiverWorld(30);
    Column(w2, 5, 0, 12);
    w2.At(5, 12).downstream = kNoDirection;
    Finish(w2);
    PlaceRiverNodes(w2, Rules());
    EXPECT_EQ(w.sites.Count(), 0);
    EXPECT_EQ(w2.sites.Count(), 0);
}

TEST(NodesRiverFill, LongestPathBeatsLowerIndexTipAndTieGoesToLowestIndex)
{
    // Trunk (5,10)..(5,20) flows south into the ocean at (5,21). Branch A continues north up column 5, branch B
    // leaves (5,10) to the north-west (4,10) and climbs column 4.
    auto build = [](int aTipRow) {
        WorldStructure w = RiverWorld(22);
        Column(w, 5, aTipRow, 20);                       // branch A + trunk
        SetRiver(w, 4, 10); FlowRiver(w, 4, 10, 5, 10);  // branch B joins at (5,10)
        for (int y = 1; y <= 9; ++y) { SetRiver(w, 4, y); FlowRiver(w, 4, y, 4, y + 1); }
        Finish(w);
        return w;
    };
    {   // A tip (5,0): depth 20; B tip (4,1): depth 10 + 1 + 9 = 20 -> tie, lowest cell index = A (cell 5).
        WorldStructure w = build(0);
        PlaceRiverNodes(w, Rules());
        EXPECT_EQ(RiverRows(w, 4).size(), 0u);
        EXPECT_EQ(RiverRows(w, 5), (std::vector<int>{ 6, 13, 20 }));
    }
    {   // A tip (5,2): depth 18 < 20 -> B is the longest path: steps 7 = (5,13), 14 = (5,6)? no: trunk is 11 cells.
        WorldStructure w = build(2);
        PlaceRiverNodes(w, Rules());
        // mouth (5,20) step 0; step 7 = (5,13); step 11 = (5,9)? the path leaves via (5,10)->(4,10): steps 10 (5,10),
        // 11 (4,10), 12 (4,9), 13 (4,8), 14 (4,7).
        EXPECT_EQ(RiverRows(w, 5), (std::vector<int>{ 13, 20 }));
        EXPECT_EQ(RiverRows(w, 4), (std::vector<int>{ 7 }));
    }
}

TEST(NodesRiverFill, IslandRiverGetsMouthOnly)
{
    WorldStructure w = RiverWorld(40, 2);   // landmass 2, mainLandmass stays 1
    Column(w, 5, 0, 38);
    Finish(w);
    const RiverNodeResult r = PlaceRiverNodes(w, Rules());
    EXPECT_EQ(r.trees, 1);
    EXPECT_EQ(RiverRows(w, 5), (std::vector<int>{ 38 }));
}

TEST(NodesRiverFill, TreesOrdinalByMouthCellIndexAndNoCrossTreeSpacingLoss)
{
    WorldStructure w = RiverWorld();
    Column(w, 9, 0, 38);
    Column(w, 5, 0, 38);
    Finish(w);
    const RiverNodeResult r = PlaceRiverNodes(w, Rules());
    EXPECT_EQ(r.trees, 2);
    EXPECT_EQ(w.sites.Find(BuiltinNodeKinds::River(), static_cast<uint32_t>(CellIndex(w, 5, 38)))->variant, 0);
    EXPECT_EQ(w.sites.Find(BuiltinNodeKinds::River(), static_cast<uint32_t>(CellIndex(w, 9, 38)))->variant, 1);
    EXPECT_EQ(RiverRows(w, 5).size(), 6u);
    EXPECT_EQ(RiverRows(w, 9).size(), 6u);
}

TEST(NodesRiverFill, MouthsAreNeverDroppedBySpacing)
{
    WorldStructure w = RiverWorld();
    Column(w, 5, 0, 38);
    Column(w, 6, 30, 38);               // mouth next door (distance 1)
    Finish(w);
    AddCorridor(w, 7, 37);              // corridor node right beside the mouths
    PlaceRiverNodes(w, Rules());
    EXPECT_TRUE(HasRiverAt(w, 5, 38));
    EXPECT_TRUE(HasRiverAt(w, 6, 38));
}

TEST(NodesRiverFill, UpstreamNodeNearCorridorIsDroppedUnlessOnBarrierCrossing)
{
    WorldStructure w = RiverWorld();
    Column(w, 5, 0, 38);
    AddFlag(w, 5, 31, TerrainFlag::RiverCrossesBarrier);   // step 7: barrier crossing
    Finish(w);
    AddCorridor(w, 5, 31);              // the corridor on the same cell
    AddCorridor(w, 6, 24);              // adjacent to the step-14 cell (5,24)
    const RiverNodeResult r = PlaceRiverNodes(w, Rules());
    EXPECT_EQ(RiverRows(w, 5), (std::vector<int>{ 3, 10, 17, 31, 38 }));
    EXPECT_EQ(r.skippedSpacing, 1);
    const uint32_t cell = static_cast<uint32_t>(CellIndex(w, 5, 31));
    EXPECT_EQ(w.sites.At(cell).Size(), 2u);   // river + corridor coexist
    EXPECT_NE(w.sites.Find(BuiltinNodeKinds::Corridor(), cell), nullptr);
    EXPECT_NE(w.sites.Find(BuiltinNodeKinds::River(), cell), nullptr);
}

TEST(NodesRiverFill, UnclaimableUpstreamCellsAreSkipped)
{
    WorldStructure w = RiverWorld();
    Column(w, 5, 0, 38);
    Finish(w);
    AssignRegion(w, 0, 0, 12, 21, kNoRegion);   // rows 0..20 not claimable
    for (int y = 0; y <= 20; ++y) { for (int x = 0; x < 12; ++x) { w.At(x, y).region = kNoRegion; } }
    BuildRegionRecords(w);
    const RiverNodeResult r = PlaceRiverNodes(w, Rules());
    EXPECT_EQ(RiverRows(w, 5), (std::vector<int>{ 24, 31, 38 }));
    EXPECT_EQ(r.skippedUnclaimable, 3);
}

TEST(NodesRiverFill, RiverPlacementKeepsOtherSitesAndIsIdempotent)
{
    WorldStructure w = RiverWorld();
    Column(w, 5, 0, 38);
    Finish(w);
    AddCorridor(w, 1, 1);
    Site other;
    other.kindCrc = Dia::Core::StringCRC("Dragon.Hearth").Value();
    other.cell = 3;
    w.sites.Add(other);
    PlaceRiverNodes(w, Rules());
    const SiteSet once = w.sites;
    EXPECT_EQ(w.sites.Count(), 8);
    PlaceRiverNodes(w, Rules());
    EXPECT_TRUE(once == w.sites);
}

// ================================================================================ quotas

namespace
{
    QuotaResult Quotas(std::vector<uint16_t> counts) { return ComputeNodeQuotas(counts, 10); }
}

TEST(NodesRiverFill, QuotaClampsAndRounding)
{
    // 12 regions keep the sum inside 80..150 whatever each one is: use a fixed mix and check each entry.
    const QuotaResult q = Quotas({ 4, 55, 64, 65, 100, 149, 150, 151, 400, 60, 60, 60 });
    ASSERT_EQ(q.quota.size(), 12u);
    EXPECT_EQ(q.quota[0], 6);    // 0.4 -> 0 -> clamp 6
    EXPECT_EQ(q.quota[1], 6);    // 5.5 -> 6
    EXPECT_EQ(q.quota[2], 6);    // 6.4 -> 6
    EXPECT_EQ(q.quota[3], 7);    // 6.5 -> 7
    EXPECT_EQ(q.quota[4], 10);
    EXPECT_EQ(q.quota[5], 15);   // 14.9 -> 15
    EXPECT_EQ(q.quota[6], 15);
    EXPECT_EQ(q.quota[7], 15);
    EXPECT_EQ(q.quota[8], 15);   // clamp
    EXPECT_EQ(q.total, 6u + 6 + 6 + 7 + 10 + 15 + 15 + 15 + 15 + 6 + 6 + 6);
    EXPECT_FALSE(q.nodeLimited);
}

TEST(NodesRiverFill, QuotaScalesUpTo80)
{
    const QuotaResult q = Quotas(std::vector<uint16_t>(8, 60));   // 8 x 6 = 48 -> scaled to 10 each
    EXPECT_EQ(q.total, 80u);
    for (uint16_t v : q.quota) { EXPECT_EQ(v, 10); }
    EXPECT_FALSE(q.nodeLimited);
}

TEST(NodesRiverFill, QuotaScalesDownTo150NudgingLowestIdsFirst)
{
    const QuotaResult q = Quotas(std::vector<uint16_t>(12, 150));   // 12 x 15 = 180 -> 13 each = 156 -> nudge 6 down
    EXPECT_EQ(q.total, 150u);
    for (size_t i = 0; i < 12; ++i) { EXPECT_EQ(q.quota[i], i < 6 ? 12 : 13) << i; }
    EXPECT_FALSE(q.nodeLimited);
}

TEST(NodesRiverFill, QuotaNudgeUpGoesLargestCellCountThenLowestId)
{
    std::vector<uint16_t> counts(13, 60);   // 13 x 6 = 78 -> scaling leaves 78 -> nudge up twice
    counts[6] = 64;                          // still quota 6, but the largest
    const QuotaResult q = Quotas(counts);
    EXPECT_EQ(q.total, 80u);
    for (size_t i = 0; i < 13; ++i) { EXPECT_EQ(q.quota[i], (i == 6 || i == 0) ? 7 : 6) << i; }
    EXPECT_FALSE(q.nodeLimited);
}

TEST(NodesRiverFill, QuotaNodeLimitedWhenClampsBlockTheBounds)
{
    const QuotaResult low = Quotas(std::vector<uint16_t>(3, 1000));   // 3 x 15 = 45 < 80
    EXPECT_EQ(low.total, 45u);
    EXPECT_TRUE(low.nodeLimited);
    const QuotaResult high = Quotas(std::vector<uint16_t>(30, 60));   // 30 x 6 = 180 > 150
    EXPECT_EQ(high.total, 180u);
    EXPECT_TRUE(high.nodeLimited);
    for (uint16_t v : high.quota) { EXPECT_EQ(v, 6); }
    EXPECT_TRUE(Quotas({}).quota.empty());
}

// ================================================================================ yield tables and weights

TEST(NodesRiverFill, YieldTableMatchesGdd)
{
    struct Row { CoW::World::BiomeIndex b; int food; int mat; };
    const Row rows[] = { { B("Biome.Grassland"), 3, 1 }, { B("Biome.TemperateForest"), 2, 3 }, { B("Biome.Wetland"), 2, 1 },
                         { B("Biome.Desert"), 1, 2 },     { B("Biome.Alpine"), 1, 3 },  { B("Biome.Tundra"), 1, 2 },
                         { B("Biome.Rainforest"), 3, 2 },      { B("Biome.Volcanic"), 1, 3 }, { B("Biome.Ocean"), 0, 0 },
                         { B("Biome.Lake"), 0, 0 } };
    for (const Row& r : rows)
    {
        EXPECT_EQ(BiomeFood(r.b), r.food) << static_cast<int>(r.b);
        EXPECT_EQ(BiomeMaterials(r.b), r.mat) << static_cast<int>(r.b);
        const float share = r.food + r.mat ? static_cast<float>(r.food) / static_cast<float>(r.food + r.mat) : 0.0f;
        EXPECT_FLOAT_EQ(BiomeFoodShare(r.b), share);
    }
}

TEST(NodesRiverFill, KindDrawFollowsShareExactlyAtExtremes)
{
    EXPECT_FALSE(DrawIsHunting(0.0f, 0.0f));
    EXPECT_FALSE(DrawIsHunting(0.5f, 0.0f));
    EXPECT_TRUE(DrawIsHunting(0.0f, 1.0f));
    EXPECT_TRUE(DrawIsHunting(0.9999f, 1.0f));
    EXPECT_TRUE(DrawIsHunting(0.74f, 0.75f));
    EXPECT_FALSE(DrawIsHunting(0.75f, 0.75f));
}

TEST(NodesRiverFill, TerrainFactorsAndExclusions)
{
    for (TerrainType t : { TerrainType::Peak, TerrainType::Volcano, TerrainType::River, TerrainType::Lake, TerrainType::Pass,
                           TerrainType::DeepWater, TerrainType::ShallowWater })
    {
        EXPECT_EQ(TerrainFactor(FillKind::Hunting, t), 0) << TerrainTypeName(t);
        EXPECT_EQ(TerrainFactor(FillKind::Resource, t), 0) << TerrainTypeName(t);
    }
    for (TerrainType t : { TerrainType::Lowland, TerrainType::Hills, TerrainType::Highland })
    {
        EXPECT_GT(TerrainFactor(FillKind::Hunting, t), TerrainFactor(FillKind::Hunting, TerrainType::Mountain)) << TerrainTypeName(t);
    }
    EXPECT_GT(TerrainFactor(FillKind::Hunting, TerrainType::Mountain), 0);
    EXPECT_GT(TerrainFactor(FillKind::Hunting, TerrainType::Cliff), 0);
    for (TerrainType t : { TerrainType::Hills, TerrainType::Highland, TerrainType::Mountain, TerrainType::Cliff })
    {
        EXPECT_GT(TerrainFactor(FillKind::Resource, t), TerrainFactor(FillKind::Resource, TerrainType::Lowland)) << TerrainTypeName(t);
    }
    EXPECT_GT(TerrainFactor(FillKind::Resource, TerrainType::Lowland), 0);

    Cell c;
    c.landmass = 1; c.biome = B("Biome.Grassland"); c.terrain = TerrainType::Lowland;
    EXPECT_EQ(CellWeight(FillKind::Hunting, c), 3 * TerrainFactor(FillKind::Hunting, TerrainType::Lowland));
    EXPECT_EQ(CellWeight(FillKind::Resource, c), 1 * TerrainFactor(FillKind::Resource, TerrainType::Lowland));
    c.biome = B("Biome.Alpine"); c.terrain = TerrainType::Mountain;
    EXPECT_EQ(CellWeight(FillKind::Resource, c), 3 * TerrainFactor(FillKind::Resource, TerrainType::Mountain));
    c.waterSource = WaterSource::River; c.terrain = TerrainType::Hills;
    EXPECT_EQ(CellWeight(FillKind::Hunting, c), 0);
    c.waterSource = WaterSource::Lake; c.terrain = TerrainType::Hills;
    EXPECT_EQ(CellWeight(FillKind::Hunting, c), 0);
    c.waterSource = WaterSource::None; c.biome = B("Biome.Lake");
    EXPECT_EQ(CellWeight(FillKind::Hunting, c), 0);
}

// ================================================================================ fill

namespace
{
    // 3 x 2 regions of 16 x 16 cells: quota 15 each, total 90 (inside 80..150).
    WorldStructure SixRegions(CoW::World::BiomeIndex b = G) { return MakeRegionBlocksWorld(3, 2, 16, 16, b); }

    FillResult Fill(WorldStructure& w, unsigned seed)
    {
        RandomStream rng(seed);
        return PlaceFillNodes(w, Rules(), rng);
    }

    int CountBuiltin(const WorldStructure& w, int region)
    {
        int n = 0;
        for (const Site& s : w.sites.All())
        {
            if ((s.kindCrc == SiteKindId(BuiltinNodeKinds::Corridor()) || s.kindCrc == SiteKindId(BuiltinNodeKinds::River()) ||
                 s.kindCrc == SiteKindId(BuiltinNodeKinds::Hunting()) || s.kindCrc == SiteKindId(BuiltinNodeKinds::Resource())) &&
                w.Cells()[s.cell].region == region)
            {
                ++n;
            }
        }
        return n;
    }

    std::vector<int> FillCells(const WorldStructure& w)
    {
        std::vector<int> v = CellsOf(w, BuiltinNodeKinds::Hunting());
        const std::vector<int> r = CellsOf(w, BuiltinNodeKinds::Resource());
        v.insert(v.end(), r.begin(), r.end());
        std::sort(v.begin(), v.end());
        return v;
    }
}

TEST(NodesRiverFill, FillCountIsQuotaMinusExistingNodes)
{
    WorldStructure w = SixRegions();
    AddCorridor(w, 2, 2); AddCorridor(w, 8, 2); AddCorridor(w, 2, 8); AddCorridor(w, 8, 8);   // region 1: 4 nodes
    const FillResult r = Fill(w, 11);
    ASSERT_EQ(r.quota.size(), 6u);
    EXPECT_EQ(r.quotaTotal, 90u);
    EXPECT_FALSE(r.nodeLimited);
    EXPECT_EQ(r.dropped, 0);
    EXPECT_EQ(r.existing[0], 4);
    EXPECT_EQ(r.placed[0], 11);
    for (size_t i = 1; i < 6; ++i) { EXPECT_EQ(r.existing[i], 0); EXPECT_EQ(r.placed[i], 15); }
    for (int id = 1; id <= 6; ++id) { EXPECT_EQ(CountBuiltin(w, id), 15) << id; }
    EXPECT_EQ(r.placedHunting + r.placedResource, 86);
    EXPECT_EQ(w.sites.Count(BuiltinNodeKinds::Hunting()), r.placedHunting);
    EXPECT_EQ(w.sites.Count(BuiltinNodeKinds::Resource()), r.placedResource);
}

TEST(NodesRiverFill, RegionAlreadyAtQuotaGetsNothingAndSitesCarryRegion)
{
    WorldStructure w = SixRegions();
    Fill(w, 3);
    const SiteSet first = w.sites;
    const FillResult again = Fill(w, 4);
    EXPECT_TRUE(first == w.sites);
    EXPECT_EQ(again.placedHunting + again.placedResource, 0);
    for (const Site& s : w.sites.All())
    {
        EXPECT_EQ(s.region, w.Cells()[s.cell].region);
        EXPECT_NE(s.region, kNoRegion);
    }
}

TEST(NodesRiverFill, SingleRegionIsNodeLimitedByTheQuotaSum)
{
    WorldStructure w = MakeRegionBlocksWorld(1, 1, 20, 20, G);
    const FillResult r = Fill(w, 5);
    EXPECT_TRUE(r.quotaLimited);
    EXPECT_TRUE(r.nodeLimited);
    EXPECT_EQ(r.quotaTotal, 15u);
    EXPECT_EQ(r.placed[0], 15);
    EXPECT_EQ(r.dropped, 0);
}

TEST(NodesRiverFill, KindDrawFollowsFoodShareStatistically)
{
    for (const CoW::World::BiomeIndex b : { B("Biome.Grassland"), B("Biome.Alpine"), B("Biome.Rainforest") })
    {
        int hunting = 0, total = 0;
        for (unsigned seed = 1; seed <= 100; ++seed)
        {
            WorldStructure w = SixRegions(b);
            const FillResult r = Fill(w, seed);
            hunting += r.placedHunting;
            total += r.placedHunting + r.placedResource;
        }
        ASSERT_EQ(total, 9000);
        EXPECT_NEAR(static_cast<double>(hunting) / total, BiomeFoodShare(b), 0.03) << static_cast<int>(b);
    }
}

TEST(NodesRiverFill, NodesRespectTerrainExclusions)
{
    WorldStructure w = SixRegions();
    for (int y = 0; y < w.dimensions.height; ++y)
    {
        for (int x = 0; x < w.dimensions.width; x += 4)
        {
            Cell& c = w.At(x, y);
            switch (y % 5)
            {
            case 0: c.terrain = TerrainType::Peak; break;
            case 1: c.terrain = TerrainType::Volcano; break;
            case 2: c.terrain = TerrainType::River; c.waterSource = WaterSource::River; break;
            case 3: c.terrain = TerrainType::Lake; c.waterSource = WaterSource::Lake; c.biome = B("Biome.Lake"); break;
            default: c.terrain = TerrainType::Pass; c.terrainFlags |= TerrainFlag::Pass; break;
            }
        }
    }
    BuildRegionRecords(w);
    for (unsigned seed = 1; seed <= 20; ++seed)
    {
        WorldStructure copy = w;
        const FillResult r = Fill(copy, seed);
        EXPECT_GT(r.placedHunting + r.placedResource, 80);
        for (const Site& s : copy.sites.All())
        {
            const Cell& c = copy.Cells()[s.cell];
            EXPECT_NE(c.terrain, TerrainType::Peak);
            EXPECT_NE(c.terrain, TerrainType::Volcano);
            EXPECT_NE(c.terrain, TerrainType::River);
            EXPECT_NE(c.terrain, TerrainType::Lake);
            EXPECT_NE(c.terrain, TerrainType::Pass);
            EXPECT_NE(c.waterSource, WaterSource::River);
            EXPECT_NE(c.waterSource, WaterSource::Lake);
        }
    }
}

TEST(NodesRiverFill, OnlyPositiveWeightCellsAreChosenAndEmptySlotsAreDropped)
{
    WorldStructure w = MakeRegionBlocksWorld(1, 1, 16, 16, G);
    for (int i = 0; i < w.CellCount(); ++i) { w.Cells()[i].terrain = TerrainType::Peak; }
    for (const auto& xy : { std::pair<int, int>{ 1, 1 }, { 10, 1 }, { 5, 10 } }) { w.At(xy.first, xy.second).terrain = TerrainType::Lowland; }
    const FillResult r = Fill(w, 9);
    EXPECT_EQ(r.placedHunting + r.placedResource, 3);
    EXPECT_EQ(r.dropped, 12);
    EXPECT_TRUE(r.nodeLimited);
    EXPECT_EQ(FillCells(w), (std::vector<int>{ CellIndex(w, 1, 1), CellIndex(w, 10, 1), CellIndex(w, 5, 10) }));

    WorldStructure none = MakeRegionBlocksWorld(1, 1, 8, 8, G);
    for (int i = 0; i < none.CellCount(); ++i) { none.Cells()[i].terrain = TerrainType::Volcano; }
    const FillResult r0 = Fill(none, 9);
    EXPECT_EQ(r0.placed[0], 0);
    EXPECT_EQ(r0.dropped, 15);
    EXPECT_TRUE(r0.nodeLimited);
    EXPECT_EQ(none.sites.Count(), 0);
}

TEST(NodesRiverFill, TinyRegionDropsSlotsAndIsNodeLimited)
{
    WorldStructure w = MakeRegionBlocksWorld(1, 1, 4, 4, G);   // 16 cells, quota 15 (lone region clamps to 15), spacing fits ~2
    const FillResult r = Fill(w, 2);
    EXPECT_GT(r.placed[0], 0);
    EXPECT_GT(r.dropped, 0);
    EXPECT_EQ(r.placed[0] + r.dropped, 15);
    EXPECT_TRUE(r.nodeLimited);
}

TEST(NodesRiverFill, SpacingMaskCoversAllExistingBuiltinNodes)
{
    WorldStructure w = SixRegions();
    AddCorridor(w, 2, 2); AddCorridor(w, 8, 2); AddCorridor(w, 2, 8); AddCorridor(w, 8, 8);
    Site river;
    river.kindCrc = SiteKindId(BuiltinNodeKinds::River());
    river.cell = static_cast<uint32_t>(CellIndex(w, 13, 13));
    river.region = 1;
    w.sites.Add(river);
    for (unsigned seed = 1; seed <= 10; ++seed)
    {
        WorldStructure copy = w;
        Fill(copy, seed);
        std::vector<int> all;
        for (const Site& s : copy.sites.All()) { all.push_back(static_cast<int>(s.cell)); }
        for (size_t i = 0; i < all.size(); ++i)
        {
            for (size_t j = i + 1; j < all.size(); ++j) { EXPECT_GE(Dist(copy, all[i], all[j]), 3) << all[i] << " " << all[j]; }
        }
    }
}

TEST(NodesRiverFill, DeterministicPerSeed)
{
    WorldStructure a = SixRegions(), b = SixRegions(), c = SixRegions();
    Fill(a, 77); Fill(b, 77); Fill(c, 78);
    EXPECT_TRUE(a.sites == b.sites);
    EXPECT_FALSE(a.sites == c.sites);
}

TEST(NodesRiverFill, UnrelatedSiteKindsDoNotAffectTheFill)
{
    WorldStructure plain = SixRegions();
    WorldStructure withOther = SixRegions();
    for (int i = 0; i < 20; ++i)
    {
        Site s;
        s.kindCrc = Dia::Core::StringCRC("Dragon.Thing").Value();
        s.cell = static_cast<uint32_t>(i * 37 % withOther.CellCount());
        withOther.sites.Add(s);
    }
    Fill(plain, 31);
    Fill(withOther, 31);
    for (const auto& kind : { BuiltinNodeKinds::Hunting(), BuiltinNodeKinds::Resource() })
    {
        EXPECT_EQ(CellsOf(plain, kind), CellsOf(withOther, kind));
    }
}

TEST(NodesRiverFill, DrawOrderIsKindThenWeightedCellPerSlot)
{
    // One 20x20 region (lone region: quota clamps to 15) with 14 corridor nodes on a stride grid: exactly one slot.
    WorldStructure w = MakeRegionBlocksWorld(1, 1, 20, 20, G);
    std::vector<std::pair<int, int>> existing;
    for (int y = 1; y < 20 && existing.size() < 14; y += 5)
    {
        for (int x = 1; x < 20 && existing.size() < 14; x += 4) { existing.push_back({ x, y }); }
    }
    ASSERT_EQ(existing.size(), 14u);
    for (const auto& p : existing) { AddCorridor(w, p.first, p.second); }
    WorldStructure base = w;
    RandomStream replay(123);
    const bool hunting = DrawIsHunting(replay.RandomUnit(), 0.75f);
    std::vector<int> eligible;
    for (int i = 0; i < base.CellCount(); ++i)
    {
        bool ok = true;
        for (const auto& p : existing) { if (Dist(base, i, CellIndex(base, p.first, p.second)) < 3) { ok = false; } }
        if (ok) { eligible.push_back(i); }
    }
    ASSERT_FALSE(eligible.empty());
    const int weight = hunting ? 3 * 3 : 1 * 1;   // Grassland Lowland: Food 3 x factor 3, Materials 1 x factor 1
    const int pick = replay.RandomInt() % (weight * static_cast<int>(eligible.size()));
    const int expectedCell = eligible[static_cast<size_t>(pick / weight)];

    const FillResult r = Fill(w, 123);
    EXPECT_EQ(r.placed[0], 1);
    const auto placedKind = hunting ? BuiltinNodeKinds::Hunting() : BuiltinNodeKinds::Resource();
    EXPECT_EQ(CellsOf(w, placedKind), (std::vector<int>{ expectedCell }));
}

// ================================================================================ generated worlds (AC8b)

namespace
{
    const GridDimensions kSmall{ 66, 42 };

    std::string FindPresetDir()
    {
        std::error_code ec;
        std::filesystem::path dir = std::filesystem::absolute(std::filesystem::path(__FILE__), ec).parent_path();
        while (!dir.empty())
        {
            const std::filesystem::path candidate = dir / "Cluiche/Assets/CoW/Global/WorldGen/Presets";
            if (std::filesystem::is_directory(candidate, ec)) { return candidate.string(); }
            const std::filesystem::path parent = dir.parent_path();
            if (parent == dir) { break; }
            dir = parent;
        }
        return {};
    }

    WorldShapeParams PresetParams(const char* id)
    {
        WorldShapePreset p;
        RuleWarnings warn;
        const std::string dir = FindPresetDir();
        EXPECT_FALSE(dir.empty());
        const PresetResult r = LoadPreset(dir, id, p, warn);
        EXPECT_TRUE(r.Ok()) << r.message;
        return p.params;
    }

    struct Stats
    {
        int worlds = 0, limited = 0, inRangeWorlds = 0, overQuotaRegions = 0, mouthCorridorSpacingPairs = 0;
        long totalSum = 0;
        long droppedSum = 0; int corridorOnPeakVolcano = 0; int quotaLimitedWorlds = 0; int totalMin = 1 << 30, totalMax = 0, regionMin = 1 << 30, regionMax = 0, regionsSeen = 0;
        int minRegions = 1 << 30, maxRegions = 0;
        long hunting[::CoW::World::BiomeRegistry::kMaxBiomes] = {}, resource[::CoW::World::BiomeRegistry::kMaxBiomes] = {};
    };

    bool IndLand(const Cell& c) { return c.landmass != 0 && c.waterSource != WaterSource::Ocean && c.waterSource != WaterSource::Lake; }

    void CheckBuilt(WorldStructure w, uint64_t seed, Stats& st)
    {
        const TerrainRules rules = Rules();
        RandomStream rr(static_cast<unsigned>(seed) ^ 0x5151u);
        BuildRegions(w, rules, rr, ::CoW::World::Testing::ShippedBiomeRegistry());
        RandomStream rc(static_cast<unsigned>(seed) + 1);
        PlaceCorridorNodes(w, rules, rc);
        const RiverNodeResult river = PlaceRiverNodes(w, rules);
        RandomStream rf(static_cast<unsigned>(seed) + 2);
        const FillResult fill = PlaceFillNodes(w, rules, rf);
        ++st.worlds;

        const uint32_t kCor = SiteKindId(BuiltinNodeKinds::Corridor()), kRiv = SiteKindId(BuiltinNodeKinds::River()),
                       kHun = SiteKindId(BuiltinNodeKinds::Hunting()), kRes = SiteKindId(BuiltinNodeKinds::Resource());
        const unsigned nReg = w.regions.regions.Size();
        std::vector<int> cnt(nReg + 1, 0);
        std::vector<const Site*> nodes;
        for (const Site& s : w.sites.All())
        {
            if (s.kindCrc != kCor && s.kindCrc != kRiv && s.kindCrc != kHun && s.kindCrc != kRes) { continue; }
            nodes.push_back(&s);
            ASSERT_LT(s.cell, static_cast<uint32_t>(w.CellCount()));
            const Cell& c = w.Cells()[s.cell];
            const char* kindName = s.kindCrc == kCor ? "Corridor" : s.kindCrc == kRiv ? "River" : s.kindCrc == kHun ? "Hunting" : "Resource";
            const std::string tag = "cell " + std::to_string(s.cell) + " kind " + kindName;
            EXPECT_TRUE(IndLand(c)) << tag << " not plain land";
            if (s.kindCrc == kCor)
            {
                // Known corridor-placer gap (NodesCorridor is out of scope here): counted and reported, not asserted.
                if (c.terrain == TerrainType::Peak || c.terrain == TerrainType::Volcano) { ++st.corridorOnPeakVolcano; }
            }
            else
            {
                EXPECT_NE(c.terrain, TerrainType::Peak) << tag;
                EXPECT_NE(c.terrain, TerrainType::Volcano) << tag;
            }
            EXPECT_EQ(s.region, c.region) << tag;
            if (s.kindCrc == kHun || s.kindCrc == kRes)
            {
                EXPECT_NE(c.terrain, TerrainType::River) << tag;
                EXPECT_NE(c.terrain, TerrainType::Pass) << tag;
                EXPECT_NE(c.waterSource, WaterSource::River) << tag;
            }
            if (c.region >= 1 && c.region <= nReg) { ++cnt[c.region]; }
            else { ADD_FAILURE() << tag << " is in no region"; }
        }

        // Spacing: every pair involving a fill node, and non-exempt corridor/river pairs.
        auto flagsAt = [&](const Site& s) { return w.Cells()[s.cell].terrainFlags; };
        for (size_t i = 0; i < nodes.size(); ++i)
        {
            for (size_t j = i + 1; j < nodes.size(); ++j)
            {
                const Site& a = *nodes[i];
                const Site& b = *nodes[j];
                const bool fillPair = a.kindCrc == kHun || a.kindCrc == kRes || b.kindCrc == kHun || b.kindCrc == kRes;
                const bool corCor = a.kindCrc == kCor && b.kindCrc == kCor;
                const bool rivRiv = a.kindCrc == kRiv && b.kindCrc == kRiv;
                const int d = Dist(w, static_cast<int>(a.cell), static_cast<int>(b.cell));
                if (fillPair) { EXPECT_GE(d, 3) << a.cell << " " << b.cell; continue; }
                if (corCor) { continue; }
                bool exempt = false;
                if (rivRiv)
                {
                    exempt = a.variant == b.variant || RiverIsMouth(a.flags) || RiverIsMouth(b.flags);
                }
                else   // river vs corridor
                {
                    const Site& rv = a.kindCrc == kRiv ? a : b;
                    exempt = RiverIsMouth(rv.flags) || (flagsAt(rv) & TerrainFlag::RiverCrossesBarrier);
                }
                if (d < 3 && exempt) { ++st.mouthCorridorSpacingPairs; }
                if (!exempt) { EXPECT_GE(d, 3) << "river/corridor " << a.cell << " " << b.cell; }
            }
        }

        // Rivers: independent tree scan.
        std::map<int, int> mouthOf;            // river cell -> mouth cell (only chains that reach water)
        std::map<int, int> depthOf;            // steps from the mouth
        for (int i = 0; i < w.CellCount(); ++i)
        {
            const Cell& c = w.Cells()[i];
            if (c.waterSource != WaterSource::River || c.landmass == 0) { continue; }
            int cur = i, steps = 0;
            bool sea = false;
            std::vector<int> chain;
            while (steps < w.CellCount())
            {
                chain.push_back(cur);
                const uint8_t dir = w.Cells()[cur].downstream;
                if (dir >= 6) { break; }
                const int nb = ChkNeighbour(w, cur, dir);
                if (nb < 0) { break; }
                const Cell& n = w.Cells()[nb];
                if (n.waterSource == WaterSource::Ocean || n.waterSource == WaterSource::Lake) { sea = true; break; }
                if (n.waterSource != WaterSource::River) { break; }
                cur = nb;
                ++steps;
            }
            if (!sea) { continue; }
            mouthOf[i] = cur;
            depthOf[i] = static_cast<int>(chain.size()) - 1;
        }
        std::set<int> mouths;
        for (const auto& kv : mouthOf) { mouths.insert(kv.second); }
        std::map<int, int> ordinal;
        int ord = 0;
        for (int m : mouths) { ordinal[m] = ord++; }
        EXPECT_EQ(static_cast<int>(mouths.size()), river.trees);
        std::map<int, int> maxDepth, nodeCount;
        for (const auto& kv : mouthOf) { maxDepth[kv.second] = std::max(maxDepth[kv.second], depthOf[kv.first]); }
        for (const Site& s : w.sites.Of(BuiltinNodeKinds::River()))
        {
            const int cell = static_cast<int>(s.cell);
            ASSERT_TRUE(mouthOf.count(cell)) << "river node on a cell that is not a sea-reaching river cell: " << cell;
            const int m = mouthOf[cell];
            EXPECT_EQ(s.variant, ordinal[m]);
            EXPECT_EQ(RiverIsMouth(s.flags), cell == m);
            EXPECT_EQ(depthOf[cell] % 7, 0) << "river node " << cell << " at depth " << depthOf[cell];
            if (w.Cells()[cell].landmass != w.mainLandmass) { EXPECT_EQ(cell, m) << "island river has an upstream node"; }
            ++nodeCount[m];
        }
        for (int m : mouths)
        {
            EXPECT_TRUE(w.sites.Find(BuiltinNodeKinds::River(), static_cast<uint32_t>(m)) != nullptr) << "mouth " << m << " has no node";
            if (w.Cells()[m].landmass != w.mainLandmass) { EXPECT_EQ(nodeCount[m], 1); }
            else
            {
                EXPECT_LE(nodeCount[m], 1 + maxDepth[m] / 7);
                if (river.skippedSpacing == 0 && river.skippedUnclaimable == 0) { EXPECT_EQ(nodeCount[m], 1 + maxDepth[m] / 7) << "mouth " << m; }
            }
        }

        // Counts.
        int total = 0, lo = 1 << 30, hi = 0;
        for (unsigned r = 1; r <= nReg; ++r)
        {
            total += cnt[r];
            lo = std::min(lo, cnt[r]);
            hi = std::max(hi, cnt[r]);
            ASSERT_EQ(fill.quota.size(), nReg);
            EXPECT_EQ(cnt[r], fill.existing[r - 1] + fill.placed[r - 1]) << "region " << r;
            EXPECT_LE(fill.placed[r - 1], std::max(0, fill.quota[r - 1] - fill.existing[r - 1]));
            if (cnt[r] > 15) { ++st.overQuotaRegions; }
            if (cnt[r] < 4)
            {
                const RegionRecord& rec = w.regions.regions[r - 1];
                std::printf("[NodesRiverFill] small region %u seed %llu: cells=%u biome=%s island=%d quota=%u existing=%u placed=%u count=%d\n", r,
                            static_cast<unsigned long long>(seed), rec.cellCount, ShippedBiomeRegistry().Name(rec.primaryBiome), rec.isIsland ? 1 : 0,
                            fill.quota[r - 1], fill.existing[r - 1], fill.placed[r - 1], cnt[r]);
            }
            st.regionMin = std::min(st.regionMin, cnt[r]);
            st.regionMax = std::max(st.regionMax, cnt[r]);
            ++st.regionsSeen;
        }
        st.totalSum += total;
        st.totalMin = std::min(st.totalMin, total);
        st.totalMax = std::max(st.totalMax, total);
        st.minRegions = std::min<int>(st.minRegions, nReg);
        st.maxRegions = std::max<int>(st.maxRegions, nReg);
        if (fill.nodeLimited) { ++st.limited; }
        st.droppedSum += fill.dropped;
        if (fill.quotaLimited) { ++st.quotaLimitedWorlds; }
        const bool regionsInRange = nReg >= 6 && nReg <= 25;
        if (regionsInRange)
        {
            ++st.inRangeWorlds;
            if (!fill.nodeLimited)
            {
                EXPECT_GE(total, 80);
                EXPECT_LE(total, 150);
                for (unsigned r = 1; r <= nReg; ++r)
                {
                    EXPECT_GE(cnt[r], 6) << "region " << r;
                    EXPECT_EQ(cnt[r], std::max<int>(fill.quota[r - 1], fill.existing[r - 1])) << "region " << r;
                }
            }
        }

        for (const Site& s : w.sites.Of(BuiltinNodeKinds::Hunting()))
        {
            if (s.region >= 1 && s.region <= nReg) { ++st.hunting[static_cast<int>(w.regions.regions[s.region - 1].primaryBiome)]; }
        }
        for (const Site& s : w.sites.Of(BuiltinNodeKinds::Resource()))
        {
            if (s.region >= 1 && s.region <= nReg) { ++st.resource[static_cast<int>(w.regions.regions[s.region - 1].primaryBiome)]; }
        }
    }

    void RunSeeds(const char* label, const WorldShapeParams& shape, uint64_t first, int count, Stats& st)
    {
        TerrainRunner runner;
        for (int k = 0; k < count; ++k)
        {
            const uint64_t seed = first + static_cast<uint64_t>(k);
            SCOPED_TRACE(std::string(label) + " seed " + std::to_string(seed));
            runner.Run(seed, shape, kSmall);
            CheckBuilt(runner.World(), seed, st);
        }
    }

    void Report(const char* label, const Stats& st)
    {
        std::printf("[NodesRiverFill] %-12s worlds=%d regions=%d..%d inRange=%d nodeLimited=%d total/world avg=%.1f min=%d max=%d "
                    "perRegion=%d..%d over15=%d exemptCloseRiverPairs=%d droppedSlots=%ld quotaLimited=%d\n",
                    label, st.worlds, st.minRegions, st.maxRegions, st.inRangeWorlds, st.limited,
                    st.worlds ? static_cast<double>(st.totalSum) / st.worlds : 0.0, st.totalMin, st.totalMax,
                    st.regionMin, st.regionMax, st.overQuotaRegions, st.mouthCorridorSpacingPairs, st.droppedSum, st.quotaLimitedWorlds, st.corridorOnPeakVolcano);
        EXPECT_EQ(st.corridorOnPeakVolcano, 0) << label;
        for (int b = 2; b < static_cast<int>(::CoW::World::Testing::ShippedBiomeRegistry().Count()); ++b)
        {
            if (st.hunting[b] + st.resource[b] == 0) { continue; }
            std::printf("[NodesRiverFill]   %-16s hunting=%ld resource=%ld ratio=%.2f\n", ::CoW::World::Testing::ShippedBiomeRegistry().Name(static_cast<::CoW::World::BiomeIndex>(b)),
                        st.hunting[b], st.resource[b], st.resource[b] ? static_cast<double>(st.hunting[b]) / st.resource[b] : 0.0);
        }
    }
}

TEST(NodesRiverFill, GeneratedWorldsDefaultPresetThreeSeeds)
{
    Stats st;
    for (uint64_t seed : { 123ull, 4ull, 77ull })
    {
        SCOPED_TRACE("seed " + std::to_string(seed));
        TerrainRunner runner;
        runner.Run(seed, WorldShapeParams{}, kSmall);
        CheckBuilt(runner.World(), seed, st);
    }
    Report("default x3", st);
    EXPECT_EQ(st.worlds, 3);
}

TEST(SLOW_NodesRiverFill, TwentySeedsAllPresets)
{
    for (const char* id : { "continental", "archipelago", "highlands" })
    {
        Stats st;
        RunSeeds(id, PresetParams(id), 500, 20, st);
        Report(id, st);
    }
}
