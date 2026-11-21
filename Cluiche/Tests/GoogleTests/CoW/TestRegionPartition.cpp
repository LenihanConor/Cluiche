// S-1 Regions: region partition first half (targets, components, seeds, growth).
#include <gtest/gtest.h>
#include <Shared/World/Testing/BiomeTestRegistry.h>
#include <Shared/WorldGeneration/RegionPartition.h>
#include <Shared/WorldGeneration/TerrainGenerator.h>
#include <Shared/WorldGeneration/Testing/RegionTestWorlds.h>
#include <Shared/WorldGeneration/Testing/TerrainSliderDriver.h>
#include <Shared/WorldGeneration/Testing/TerrainRulesTestHelpers.h>

#include <DiaMaths/Core/RandomStream.h>

#include <algorithm>
#include <set>

using namespace CoW::WorldGeneration;
using namespace CoW::WorldGeneration::Testing;
using namespace CoW::World::Testing;
using Dia::Maths::RandomStream;

namespace
{
    constexpr DepthZone FS = DepthZone::FirstShores;
    constexpr DepthZone WR = DepthZone::WildReaches;
    constexpr DepthZone AI = DepthZone::AncientInteriors;
    constexpr CoW::World::BiomeIndex     G  = 2;   // Biome.Grassland (shipped row, pinned by BiomeRegistryData.ShippedRowIndicesArePinned)
    constexpr CoW::World::BiomeIndex     F  = 3;   // Biome.TemperateForest (shipped row, pinned by BiomeRegistryData.ShippedRowIndicesArePinned)

    TerrainRules Rules() { return TestRules(); }

    // First seed draw for a component of `size` cells, as PlaceSeeds does it.
    uint32_t FirstDraw(unsigned seed, size_t size) { RandomStream r(seed); return static_cast<uint32_t>(r.RandomInt()) % size; }

    unsigned SeedWithFirstDraw(size_t size, uint32_t want)
    {
        for (unsigned s = 1; s < 5000; ++s) { if (FirstDraw(s, size) == want) { return s; } }
        ADD_FAILURE() << "no seed found";
        return 1;
    }

    // Vertical one-cell-wide strip at x = 0: cell i is at distance |i - j| from cell j.
    WorldStructure Strip(int len, DepthZone z = FS, CoW::World::BiomeIndex b = G)
    {
        WorldStructure w = MakeBlankWorld(3, len);
        FillLand(w, 0, 0, 1, len, z, b);
        return w;
    }

    RegionGrowth GrowWith(const WorldStructure& w, const std::vector<uint32_t>& seeds)
    {
        RegionGrowth g;
        g.components = FindZoneComponents(w, Rules());
        std::vector<std::vector<uint32_t>> s(g.components.size());
        s[0] = seeds;
        GrowRegions(w, Rules(), s, g);
        return g;
    }

    int Count(const RegionGrowth& g, RegionId r) { int n = 0; for (RegionId v : g.regionOf) { n += v == r; } return n; }
}

// ---------------------------------------------------------------- targets

TEST(RegionPartition, RebalanceAddsToMostUnusedCapacityTiesByZoneOrder)
{
    uint8_t count[3] = { 2, 4, 2 };
    const uint16_t cap[3] = { 2, 6, 4 };
    DepthZone lim = DepthZone::FirstShores;
    RebalanceRegionCounts(count, cap, lim);
    EXPECT_EQ(count[0], 2); EXPECT_EQ(count[1], 5); EXPECT_EQ(count[2], 3);
    EXPECT_EQ(lim, DepthZone::None);
}

TEST(RegionPartition, RebalanceRemovesFromLeastSpareCapacityTiesByZoneOrder)
{
    uint8_t count[3] = { 6, 6, 6 };
    const uint16_t cap[3] = { 6, 6, 6 };
    DepthZone lim;
    RebalanceRegionCounts(count, cap, lim);
    EXPECT_EQ(count[0], 5); EXPECT_EQ(count[1], 5); EXPECT_EQ(count[2], 6);
}

// Mutation-check additions (plan task 12): exact tie and floor behaviour of the rebalance loops.
TEST(RegionPartition, RebalanceAddTieGoesToEarlierZoneWhenOnlyOneAddIsNeeded)
{
    uint8_t count[3] = { 2, 4, 3 };            // total 9: exactly one add
    const uint16_t cap[3] = { 2, 6, 5 };       // zone 1 and zone 2 both have 2 unused
    DepthZone lim;
    RebalanceRegionCounts(count, cap, lim);
    EXPECT_EQ(count[0], 2); EXPECT_EQ(count[1], 5); EXPECT_EQ(count[2], 3);
    EXPECT_EQ(lim, DepthZone::None);
}

TEST(RegionPartition, RebalanceRemoveNeverTakesAZoneAtItsRangeMinimum)
{
    uint8_t count[3] = { 4, 7, 6 };            // total 17; zone 0 sits at its minimum with no spare capacity
    const uint16_t cap[3] = { 4, 7, 6 };
    DepthZone lim;
    RebalanceRegionCounts(count, cap, lim);
    EXPECT_EQ(count[0], 4); EXPECT_EQ(count[1], 6); EXPECT_EQ(count[2], 6);
}

TEST(RegionPartition, TargetsReachBothEndsOfEveryZoneRange)
{
    // Capacity 8 / 8 / 6 (300 cells each at 36 per region) never limits, so the desired count is the final count.
    WorldStructure w = MakeBlankWorld(60, 30);
    FillLand(w, 0, 0, 20, 15, FS, G);
    FillLand(w, 20, 0, 40, 15, WR, G);
    FillLand(w, 40, 0, 60, 15, AI, G);
    int lo[3] = { 99, 99, 99 }, hi[3] = { 0, 0, 0 };
    for (unsigned seed = 1; seed <= 300; ++seed)
    {
        RandomStream rng(seed);
        const RegionTargets t = ComputeRegionTargets(w, Rules(), rng);
        for (int z = 0; z < 3; ++z) { lo[z] = std::min<int>(lo[z], t.count[z]); hi[z] = std::max<int>(hi[z], t.count[z]); }
    }
    for (int z = 0; z < 3; ++z)
    {
        EXPECT_EQ(lo[z], kZoneCountMin[z]) << "zone " << z;
        EXPECT_EQ(hi[z], kZoneCountMax[z]) << "zone " << z;
    }
}

TEST(RegionPartition, RebalanceTerrainLimitedNamesSmallestCapacityToMinimum)
{
    uint8_t count[3] = { 3, 2, 2 };
    const uint16_t cap[3] = { 3, 2, 2 };
    DepthZone lim;
    RebalanceRegionCounts(count, cap, lim);
    EXPECT_EQ(count[0] + count[1] + count[2], 7);
    EXPECT_EQ(lim, DepthZone::WildReaches);   // 3/4, 2/4, 2/2

    uint8_t c2[3] = { 2, 2, 1 };
    const uint16_t cap2[3] = { 2, 2, 1 };
    RebalanceRegionCounts(c2, cap2, lim);
    EXPECT_EQ(lim, DepthZone::FirstShores);   // equal ratios: zone order
}

TEST(RegionPartition, TargetsAreCapacityLimited)
{
    // FS 80 cells -> capacity 2 (36 each); WR 216 -> 6; AI 144 -> 4.
    WorldStructure w = MakeBlankWorld(40, 20);
    FillLand(w, 0, 0, 8, 10, FS, G);
    FillLand(w, 8, 0, 18, 10, WR, G);
    FillLand(w, 8, 10, 20, 20, WR, G);
    FillLand(w, 20, 0, 32, 12, AI, G);
    for (unsigned seed = 1; seed <= 30; ++seed)
    {
        RandomStream rng(seed);
        const RegionTargets t = ComputeRegionTargets(w, Rules(), rng);
        EXPECT_EQ(t.capacity[0], 2);
        EXPECT_LE(t.count[0], 2);
        for (int z = 0; z < 3; ++z) { EXPECT_LE(t.count[z], t.capacity[z]); EXPECT_LE(t.count[z], kZoneCountMax[z]); }
        EXPECT_GE(t.total, 10) << "seed " << seed;   // capacity 12 allows it, rebalance must reach 10
        EXPECT_LE(t.total, 16);
        EXPECT_EQ(t.limitedBy, DepthZone::None);
    }
}

TEST(RegionPartition, TargetsTerrainLimitedIsReportedNotFailure)
{
    WorldStructure w = MakeBlankWorld(30, 12);
    FillLand(w, 0, 0, 12, 12, FS, G);   // 144 -> 4
    FillLand(w, 12, 0, 18, 12, WR, G);  // 72 -> 2
    RandomStream rng(3);
    const RegionTargets t = ComputeRegionTargets(w, Rules(), rng);
    EXPECT_EQ(t.total, 6);
    EXPECT_NE(t.limitedBy, DepthZone::None);
    EXPECT_EQ(t.capacity[2], 0);
}

TEST(RegionPartition, TargetsUseOneDrawPerZoneInOrder)
{
    WorldStructure w = MakeBlankWorld(40, 40);
    FillLand(w, 0, 0, 40, 40, FS, G);
    RandomStream rng(9);
    (void)ComputeRegionTargets(w, Rules(), rng);
    RandomStream ref(9);
    for (int i = 0; i < 3; ++i) { (void)ref.RandomInt(); }
    EXPECT_EQ(rng.GetDrawCount(), ref.GetDrawCount());
}

TEST(RegionPartition, TargetsDeterministic)
{
    WorldStructure w = MakeBlankWorld(40, 40);
    FillLand(w, 0, 0, 40, 14, FS, G); FillLand(w, 0, 14, 40, 28, WR, G); FillLand(w, 0, 28, 40, 40, AI, G);
    RandomStream a(5), b(5);
    const RegionTargets ta = ComputeRegionTargets(w, Rules(), a), tb = ComputeRegionTargets(w, Rules(), b);
    for (int z = 0; z < 3; ++z) { EXPECT_EQ(ta.count[z], tb.count[z]); }
}

// ---------------------------------------------------------------- components

TEST(RegionPartition, ComponentsSplitAtZoneBoundary)
{
    WorldStructure w = MakeBlankWorld(12, 6);
    FillLand(w, 0, 0, 6, 6, FS, G);
    FillLand(w, 6, 0, 12, 6, WR, G);
    const auto comps = FindZoneComponents(w, Rules());
    ASSERT_EQ(comps.size(), 2u);
    EXPECT_EQ(comps[0].zone, FS); EXPECT_EQ(comps[1].zone, WR);
    EXPECT_EQ(comps[0].cells.size(), 36u);
    EXPECT_TRUE(std::is_sorted(comps[0].cells.begin(), comps[0].cells.end()));
    EXPECT_FALSE(comps[0].tiny);
}

TEST(RegionPartition, ComponentsSplitDisconnectedPiecesAndFlagTinyOnes)
{
    WorldStructure w = MakeBlankWorld(20, 8);
    FillLand(w, 0, 0, 7, 7, WR, G);     // 49
    FillLand(w, 9, 0, 11, 2, WR, G);    // 4: tiny, ocean gap between
    const auto comps = FindZoneComponents(w, Rules());
    ASSERT_EQ(comps.size(), 2u);
    EXPECT_FALSE(comps[0].tiny);
    EXPECT_TRUE(comps[1].tiny);
    EXPECT_EQ(comps[1].cells.size(), 4u);

    RandomStream rng(1);
    const RegionGrowth g = BuildProvisionalRegions(w, Rules(), rng, ShippedBiomeRegistry());
    ASSERT_EQ(g.tinyComponents.size(), 1u);
    EXPECT_EQ(g.tinyComponents[0], 1u);
    for (uint32_t c : g.components[1].cells) { EXPECT_EQ(g.regionOf[c], kNoRegion); }
    EXPECT_EQ(g.targets.capacity[0], 0);
}

TEST(RegionPartition, ComponentsIgnoreOtherLandmassesAndOcean)
{
    WorldStructure w = MakeBlankWorld(20, 8);
    FillLand(w, 0, 0, 6, 6, FS, G, 1);
    FillLand(w, 10, 0, 16, 6, FS, G, 2);
    EXPECT_EQ(FindZoneComponents(w, Rules()).size(), 1u);
}

TEST(RegionPartition, LakesAndRiversAreOrdinaryCells)
{
    WorldStructure w = MakeBlankWorld(8, 8);
    FillLand(w, 0, 0, 8, 8, FS, G);
    w.At(3, 3).waterSource = WaterSource::Lake;
    w.At(4, 4).waterSource = WaterSource::River;
    const auto comps = FindZoneComponents(w, Rules());
    ASSERT_EQ(comps.size(), 1u);
    EXPECT_EQ(comps[0].cells.size(), 64u);
}

// ---------------------------------------------------------------- shares and seeds

TEST(RegionPartition, SharesLargestRemainderAndMinOne)
{
    WorldStructure w = MakeBlankWorld(40, 12);
    FillLand(w, 0, 0, 10, 10, FS, G);   // 100
    FillLand(w, 12, 0, 20, 10, FS, G);  // 80
    FillLand(w, 22, 0, 26, 10, FS, G);  // 40
    const auto comps = FindZoneComponents(w, Rules());
    ASSERT_EQ(comps.size(), 3u);
    RegionTargets t; t.count[0] = 5;
    const auto shares = AllocateSeedShares(comps, t);
    // quotas 2.27, 1.82, 0.91 -> 2,1,1 (floor, min 1) = 4, remainder 1 to the largest fraction (comp 2: .91)
    EXPECT_EQ(shares[0], 2); EXPECT_EQ(shares[1], 1); EXPECT_EQ(shares[2], 2);
}

TEST(RegionPartition, SharesAreCappedByWhatEachComponentCanHold)
{
    WorldStructure w = MakeBlankWorld(40, 12);
    FillLand(w, 0, 0, 10, 10, FS, G);   // 100 -> at most 2 regions of 36
    FillLand(w, 12, 0, 20, 10, FS, G);  // 80  -> at most 2
    FillLand(w, 22, 0, 26, 10, FS, G);  // 40  -> at most 1
    const auto comps = FindZoneComponents(w, Rules());
    RegionTargets t; t.count[0] = 5;
    const auto shares = AllocateSeedShares(comps, t, 36);
    EXPECT_EQ(shares[0], 2); EXPECT_EQ(shares[1], 2); EXPECT_EQ(shares[2], 1);   // surplus from the 40-cell component moves on
    t.count[0] = 6;
    const auto full = AllocateSeedShares(comps, t, 36);
    EXPECT_EQ(full[0] + full[1] + full[2], 5);   // 2 + 2 + 1: nowhere left to put the sixth
}

TEST(RegionPartition, SeedsFarthestPointWithLowestIndexTie)
{
    const WorldStructure w = Strip(21);
    const unsigned seed = SeedWithFirstDraw(21, 10);   // middle: both ends at distance 10
    RandomStream rng(seed);
    const auto comps = FindZoneComponents(w, Rules());
    const auto seeds = PlaceSeeds(w, Rules(), comps[0], 2, rng, ShippedBiomeRegistry());
    ASSERT_EQ(seeds.size(), 2u);
    EXPECT_EQ(seeds[0], comps[0].cells[10]);
    EXPECT_EQ(seeds[1], comps[0].cells[0]);
}

TEST(RegionPartition, SeedsPickFarthestEnd)
{
    const WorldStructure w = Strip(20);
    RandomStream rng(SeedWithFirstDraw(20, 4));
    const auto comps = FindZoneComponents(w, Rules());
    const auto seeds = PlaceSeeds(w, Rules(), comps[0], 3, rng, ShippedBiomeRegistry());
    ASSERT_EQ(seeds.size(), 3u);
    EXPECT_EQ(seeds[1], comps[0].cells[19]);
    EXPECT_EQ(seeds[2], comps[0].cells[11]);   // midpoint of 4..19 (lowest index of the tie 11/12)
}

TEST(RegionPartition, SeedsBiomeBonusOutweighsShorterDistance)
{
    WorldStructure w = Strip(20);
    SetBiome(w, 0, 0, F);   // cell 0: distance 9 + bonus 3 beats cell 19: distance 10
    RandomStream rng(SeedWithFirstDraw(20, 9));
    const auto comps = FindZoneComponents(w, Rules());
    const auto seeds = PlaceSeeds(w, Rules(), comps[0], 2, rng, ShippedBiomeRegistry());
    ASSERT_EQ(seeds.size(), 2u);
    EXPECT_EQ(seeds[1], comps[0].cells[0]);
}

TEST(RegionPartition, SeedsDeterministicAndStreamDependent)
{
    WorldStructure w = MakeBlankWorld(40, 40);
    FillLand(w, 0, 0, 40, 40, FS, G);
    const auto comps = FindZoneComponents(w, Rules());
    RandomStream a(11), b(11);
    EXPECT_EQ(PlaceSeeds(w, Rules(), comps[0], 4, a, ShippedBiomeRegistry()), PlaceSeeds(w, Rules(), comps[0], 4, b, ShippedBiomeRegistry()));
    std::set<std::vector<uint32_t>> variants;
    for (unsigned s = 1; s <= 8; ++s) { RandomStream r(s); variants.insert(PlaceSeeds(w, Rules(), comps[0], 4, r, ShippedBiomeRegistry())); }
    EXPECT_GT(variants.size(), 1u);
}

// ---------------------------------------------------------------- growth

TEST(RegionPartition, GrowthStaysInsideZoneAndClaimsEverything)
{
    WorldStructure w = MakeBlankWorld(30, 12);
    FillLand(w, 0, 0, 10, 10, FS, G);
    FillLand(w, 10, 0, 20, 10, WR, F);
    FillLand(w, 22, 0, 27, 5, WR, G);    // separate WR piece, 25 cells: tiny
    for (unsigned seed = 1; seed <= 10; ++seed)
    {
        RandomStream rng(seed);
        const RegionGrowth g = BuildProvisionalRegions(w, Rules(), rng, ShippedBiomeRegistry());
        for (int c = 0; c < w.CellCount(); ++c)
        {
            const Cell& cell = w.Cells()[c];
            if (cell.landmass == 0) { EXPECT_EQ(g.regionOf[c], kNoRegion); continue; }
            if (c % 30 >= 22) { EXPECT_EQ(g.regionOf[c], kNoRegion); continue; }   // tiny piece
            ASSERT_NE(g.regionOf[c], kNoRegion) << "cell " << c << " seed " << seed;
            EXPECT_EQ(g.seeds[g.regionOf[c] - 1].zone, cell.depthZone);
            EXPECT_EQ(g.seeds[g.regionOf[c] - 1].landmass, cell.landmass);
        }
    }
}

TEST(RegionPartition, CapIsRespectedBeforeLeftoverPhase)
{
    // 40-cell strip, seeds at 0,1,2: cap = floor(1.7 * 40 / 3) = 22. Region 3 expands right alone.
    const WorldStructure w = Strip(40);
    const RegionGrowth g = GrowWith(w, { 0, 3, 6 });   // strip cell i is index 3*i (width 3)
    ASSERT_EQ(g.seeds.size(), 3u);
    for (size_t r = 0; r < 3; ++r) { EXPECT_LE(g.capClaims[r], 22u); }
    EXPECT_EQ(g.capClaims[2], 22u);
    EXPECT_EQ(g.capClaims[0], 1u);
    EXPECT_EQ(g.capClaims[1], 1u);
    EXPECT_EQ(g.leftoverCells, 16u);
    EXPECT_EQ(Count(g, 3), 38);
    EXPECT_EQ(Count(g, 1) + Count(g, 2) + Count(g, 3), 40);
}

TEST(RegionPartition, FloorPhaseLetsASatisfiedRegionYieldSoNeighbourReachesMinimum)
{
    // 10x6 room (60 cells) with a 30-cell corridor leaving its right edge; seed 1 in the room, seed 2 in the corridor.
    // Plain capped Dijkstra gives the room (and the corridor mouth) to region 1 and leaves region 2 under kMinRegionCells.
    WorldStructure w = MakeBlankWorld(40, 6);
    FillLand(w, 0, 0, 10, 6, FS, G);
    FillLand(w, 10, 2, 40, 3, FS, G);
    const int minCells = Rules().kMinRegionCells;
    const RegionGrowth g = GrowWith(w, { 2u * 40u + 5u, 2u * 40u + 14u });
    ASSERT_EQ(g.seeds.size(), 2u);
    EXPECT_GE(Count(g, 1), minCells);
    EXPECT_GE(Count(g, 2), minCells);
    EXPECT_EQ(Count(g, 1) + Count(g, 2), 90);
}

TEST(RegionPartition, RidgeShiftsBoundaryOntoNeighbour)
{
    // 41-cell strip, seeds at 10 and 30. Plain: cell 20 ties and goes to region 1.
    WorldStructure plain = Strip(41);
    const RegionGrowth a = GrowWith(plain, { 30, 90 });
    EXPECT_EQ(a.regionOf[60], 1);
    WorldStructure ridged = Strip(41);
    AddFlag(ridged, 0, 19, TerrainFlag::Ridge);
    const RegionGrowth b = GrowWith(ridged, { 30, 90 });
    EXPECT_EQ(b.regionOf[60], 2);   // entering ridge cell 19 costs region 1 an extra 1.0, so cell 20 goes to region 2
    EXPECT_EQ(b.regionOf[57], 1);
}

TEST(RegionPartition, BiomeMismatchShiftsBoundary)
{
    WorldStructure w = Strip(41);
    for (int y = 20; y < 41; ++y) { SetBiome(w, 0, y, F); }
    const RegionGrowth g = GrowWith(w, { 30, 90 });   // seed 1 grass at cell 10, seed 2 forest at cell 30
    EXPECT_EQ(g.regionOf[57], 1);   // cell 19
    EXPECT_EQ(g.regionOf[60], 2);   // cell 20: mismatch for region 1 breaks the tie
}

TEST(RegionPartition, IslandsAreOneRegionEach)
{
    WorldStructure w = MakeBlankWorld(40, 12);
    FillLand(w, 0, 0, 14, 10, FS, G, 1);
    FillLand(w, 16, 0, 17, 2, WR, F, 2);      // 2 cells
    FillLand(w, 20, 0, 36, 10, AI, G, 3);     // 160 cells, mixed zones below
    FillLand(w, 20, 0, 24, 10, WR, G, 3);     // 40 WR cells: AI stays majority (120)
    RandomStream rng(2);
    const RegionGrowth g = BuildProvisionalRegions(w, Rules(), rng, ShippedBiomeRegistry());
    std::set<RegionId> lm2, lm3;
    for (int c = 0; c < w.CellCount(); ++c)
    {
        if (w.Cells()[c].landmass == 2) { lm2.insert(g.regionOf[c]); }
        if (w.Cells()[c].landmass == 3) { lm3.insert(g.regionOf[c]); }
    }
    ASSERT_EQ(lm2.size(), 1u); ASSERT_EQ(lm3.size(), 1u);
    EXPECT_NE(*lm2.begin(), kNoRegion);
    const RegionSeed& s3 = g.seeds[*lm3.begin() - 1];
    EXPECT_TRUE(s3.isIsland);
    EXPECT_EQ(s3.zone, AI);
    EXPECT_EQ(g.seeds[*lm2.begin() - 1].zone, WR);
    EXPECT_FALSE(g.seeds[0].isIsland);
    EXPECT_EQ(static_cast<size_t>(g.mainlandRegionCount), g.seeds.size() - 2);
}

TEST(RegionPartition, OverflowFlagInsteadOfCrash)
{
    WorldStructure w = MakeBlankWorld(140, 3);
    w.mainLandmass = 1;
    for (int i = 0; i < 70; ++i) { SetLand(w, 2 * i, 1, FS, G, static_cast<uint8_t>(i + 2)); }
    RandomStream rng(1);
    const RegionGrowth g = BuildProvisionalRegions(w, Rules(), rng, ShippedBiomeRegistry());
    EXPECT_TRUE(g.overflow);
    EXPECT_EQ(g.seeds.size(), static_cast<size_t>(RegionTable::kMaxRegions));
}

TEST(RegionPartition, GrowthDeterministicAndSeedDependent)
{
    WorldStructure w = MakeBlankWorld(40, 40);
    FillLand(w, 0, 0, 40, 40, FS, G);
    RandomStream a(4), b(4), c(5);
    const RegionGrowth ga = BuildProvisionalRegions(w, Rules(), a, ShippedBiomeRegistry()), gb = BuildProvisionalRegions(w, Rules(), b, ShippedBiomeRegistry());
    EXPECT_EQ(ga.regionOf, gb.regionOf);
    bool anyDifferent = false;
    for (unsigned s = 5; s < 12 && !anyDifferent; ++s)
    {
        RandomStream r(s);
        anyDifferent = BuildProvisionalRegions(w, Rules(), r, ShippedBiomeRegistry()).regionOf != ga.regionOf;
    }
    EXPECT_TRUE(anyDifferent);
}

TEST(RegionPartition, SmokeGeneratedSmallWorld)
{
    TerrainRunner runner;
    runner.Run(123, WorldShapeParams{}, GridDimensions{ 66, 42 });
    const WorldStructure& w = runner.World();
    RandomStream rng(7);
    const RegionGrowth g = BuildProvisionalRegions(w, TestRules(), rng, ShippedBiomeRegistry());
    EXPECT_FALSE(g.overflow);
    EXPECT_TRUE((g.targets.total >= 10 && g.targets.total <= 16) || g.targets.limitedBy != DepthZone::None)
        << "total " << int(g.targets.total);
    std::set<uint32_t> tiny;
    for (uint32_t ci : g.tinyComponents) { tiny.insert(ci); }
    std::set<uint32_t> tinyCells;
    for (uint32_t ci : tiny) { for (uint32_t c : g.components[ci].cells) { tinyCells.insert(c); } }
    std::set<uint32_t> unzoned(g.unzonedCells.begin(), g.unzonedCells.end());
    for (int c = 0; c < w.CellCount(); ++c)
    {
        if (!IsRegionLand(w.Cells()[c])) { EXPECT_EQ(g.regionOf[c], kNoRegion); continue; }
        if (tinyCells.count(c) || unzoned.count(c)) { continue; }
        EXPECT_NE(g.regionOf[c], kNoRegion) << "cell " << c;
    }
}
