// S-1 Regions: region partition second half (repair, records, renumbering, write into the world).
#include <gtest/gtest.h>
#include <Shared/World/Testing/BiomeTestRegistry.h>
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
#include <filesystem>
#include <string>
#include <vector>
#include <Shared/World/Testing/StratumTestIndices.h>

using namespace CoW::WorldGeneration;
using namespace CoW::WorldGeneration::Testing;
using namespace CoW::World::Testing;
using Dia::Maths::RandomStream;

namespace
{
    constexpr StratumIndex FS = kStratumFirst;
    constexpr StratumIndex WR = kStratumWild;
    constexpr StratumIndex AI = kStratumAncient;
    constexpr CoW::World::BiomeIndex     G  = 2;   // Biome.Grassland (shipped row, pinned by BiomeRegistryData.ShippedRowIndicesArePinned)
    constexpr CoW::World::BiomeIndex     F  = 3;   // Biome.TemperateForest (shipped row, pinned by BiomeRegistryData.ShippedRowIndicesArePinned)
    constexpr CoW::World::BiomeIndex     L  = 1;   // Biome.Lake (shipped row, pinned by BiomeRegistryData.ShippedRowIndicesArePinned)

    TerrainRules RulesMin(int minCells)
    {
        TerrainRules r = TestRules();
        r.kMinRegionCells = minCells;
        return r;
    }

    struct Hand
    {
        WorldStructure w;
        RegionGrowth   g;
    };

    // rows: one string per grid row. '.' ocean, '1'..'9' provisional region id, 'u' land with region 0.
    // zones[i] = zone of region i+1 (its cells take that zone). uZone = zone of 'u' cells.
    Hand Make(const std::vector<std::string>& rows, const std::vector<StratumIndex>& zones, StratumIndex uZone = FS, CoW::World::BiomeIndex biome = G)
    {
        Hand h;
        const int W = static_cast<int>(rows[0].size());
        const int H = static_cast<int>(rows.size());
        h.w = MakeBlankWorld(W, H);
        h.g.regionOf.assign(static_cast<size_t>(W) * H, kNoRegion);
        for (int y = 0; y < H; ++y)
        {
            for (int x = 0; x < W; ++x)
            {
                const char ch = rows[y][x];
                if (ch == '.') { SetOcean(h.w, x, y); continue; }
                if (ch == 'u') { SetLand(h.w, x, y, uZone, biome); continue; }
                const int id = ch - '0';
                SetLand(h.w, x, y, zones[id - 1], biome);
                h.g.regionOf[static_cast<size_t>(y) * W + x] = static_cast<RegionId>(id);
            }
        }
        for (StratumIndex z : zones)
        {
            RegionSeed s;
            s.biome = biome; s.stratum = z; s.landmass = 1;
            h.g.seeds.push_back(s);
        }
        h.g.mainlandRegionCount = static_cast<uint8_t>(zones.size());
        return h;
    }

    // Registers the given cells as one tiny zone component (as BuildProvisionalRegions does for components under
    // kMinRegionCells).
    void MarkTiny(Hand& h, const std::vector<std::pair<int, int>>& xy)
    {
        RegionComponent comp;
        comp.landmass = 1;
        comp.tiny = true;
        for (const auto& q : xy) { comp.cells.push_back(static_cast<uint32_t>(q.second * h.w.dimensions.width + q.first)); }
        std::sort(comp.cells.begin(), comp.cells.end());
        comp.stratum = h.w.Cells()[comp.cells[0]].stratum;
        h.g.tinyComponents.push_back(static_cast<uint32_t>(h.g.components.size()));
        h.g.components.push_back(comp);
    }

    RegionBuild Finish(Hand& h, int minCells = 1) { return FinishRegions(h.w, RulesMin(minCells), h.g, ShippedBiomeRegistry()); }

    RegionId At(const Hand& h, int x, int y) { return h.w.At(x, y).region; }

    void ExpectChecksPass(const WorldStructure& w, float share = -1.0f)
    {
        const CheckErrors e = CheckAllRegions(w, share);
        EXPECT_TRUE(e.empty()) << ChkJoin(e);
    }
}

// ---------------------------------------------------------------- split pieces

TEST(RegionRepair, SplitKeepsLargestPieceAndGivesRestToLongestBorder)
{
    Hand h = Make({ "11", "32", "32", "11", "11" }, { FS, FS, FS });
    const RegionBuild b = Finish(h);
    EXPECT_EQ(b.splitPiecesMoved, 1);
    // Top piece borders old 3 with two edges and old 2 with one: goes to old 3 (which becomes id 1 by min cell).
    EXPECT_EQ(At(h, 0, 0), 1); EXPECT_EQ(At(h, 1, 0), 1); EXPECT_EQ(At(h, 0, 1), 1); EXPECT_EQ(At(h, 0, 2), 1);
    EXPECT_EQ(At(h, 1, 1), 2); EXPECT_EQ(At(h, 1, 2), 2);
    EXPECT_EQ(At(h, 0, 3), 3); EXPECT_EQ(At(h, 1, 4), 3);
    EXPECT_EQ(h.w.regions.regions.Size(), 3u);
    EXPECT_EQ(h.w.regions.regions[0].cellCount, 4);
    EXPECT_EQ(h.w.regions.regions[2].cellCount, 4);
    EXPECT_EQ(h.w.regions.absorbedCells, 0);
    ExpectChecksPass(h.w);
}

TEST(RegionRepair, SplitPrefersSameZoneOverLongerBorder)
{
    // Same layout; old 1 and old 2 are WildReaches, old 3 FirstShores.
    Hand h = Make({ "11", "32", "32", "11", "11" }, { WR, WR, FS });
    Finish(h);
    // Piece goes to old 2 despite the shorter border; old 2 then owns cell 0 -> id 1.
    EXPECT_EQ(At(h, 0, 0), 1); EXPECT_EQ(At(h, 1, 0), 1); EXPECT_EQ(At(h, 1, 1), 1); EXPECT_EQ(At(h, 1, 2), 1);
    EXPECT_EQ(At(h, 0, 1), 2);
    EXPECT_EQ(At(h, 0, 3), 3);
    EXPECT_EQ(h.w.regions.absorbedCells, 0);
    ExpectChecksPass(h.w);
}

TEST(RegionRepair, SplitTieOnPieceSizeKeepsLowestMinCell)
{
    Hand h = Make({ "1", "1", "2", "1", "1" }, { FS, FS });
    const RegionBuild b = Finish(h);
    EXPECT_EQ(b.splitPiecesMoved, 1);
    EXPECT_EQ(At(h, 0, 0), 1); EXPECT_EQ(At(h, 0, 1), 1);
    EXPECT_EQ(At(h, 0, 2), 2); EXPECT_EQ(At(h, 0, 3), 2); EXPECT_EQ(At(h, 0, 4), 2);
    ExpectChecksPass(h.w);
}

TEST(RegionRepair, RenumberedBySmallestCellIndex)
{
    // Provisional ids are reversed relative to cell order.
    Hand h = Make({ "3", "3", "2", "2", "1", "1" }, { FS, FS, FS });
    Finish(h);
    EXPECT_EQ(At(h, 0, 0), 1); EXPECT_EQ(At(h, 0, 2), 2); EXPECT_EQ(At(h, 0, 4), 3);
    ExpectChecksPass(h.w);
}

// ---------------------------------------------------------------- undersized merges

TEST(RegionRepair, UndersizedMergesIntoSameZoneNeighbourFirst)
{
    Hand h = Make({ "1", "1", "1", "1", "1", "1", "2", "2", "3", "3", "3", "3", "3", "3" }, { FS, WR, WR });
    const RegionBuild b = Finish(h, 4);
    EXPECT_EQ(b.undersizedMerged, 1);
    EXPECT_EQ(h.w.regions.regions.Size(), 2u);
    EXPECT_EQ(At(h, 0, 6), 2);   // joined the same-zone region despite the equal border
    EXPECT_EQ(h.w.regions.absorbedCells, 0);
    ExpectChecksPass(h.w);
}

TEST(RegionRepair, UndersizedMergesAnyZoneWhenNoSameZoneAndCountsAbsorbed)
{
    Hand h = Make({ "1", "1", "1", "1", "1", "1", "2", "2", "3", "3", "3", "3", "3", "3" }, { FS, WR, AI });
    Finish(h, 4);
    // No same-zone neighbour: equal borders, lowest id wins -> region 1; its 2 cells are absorbed.
    EXPECT_EQ(At(h, 0, 6), 1);
    EXPECT_EQ(h.w.regions.regions.Size(), 2u);
    EXPECT_EQ(h.w.regions.absorbedCells, 2);
    ExpectChecksPass(h.w);
}

TEST(RegionRepair, UndersizedMergesAscendingSizeAndCascades)
{
    Hand h = Make({ "1", "1", "1", "1", "1", "1", "2", "2", "3" }, { FS, FS, FS });
    const RegionBuild b = Finish(h, 4);
    EXPECT_EQ(b.undersizedMerged, 2);   // size 1 into size 2 (size 3, still < 4), then into the big one
    EXPECT_EQ(h.w.regions.regions.Size(), 1u);
    EXPECT_EQ(h.w.regions.regions[0].cellCount, 9);
    ExpectChecksPass(h.w);
}

TEST(RegionRepair, UndersizedWithNoNeighbourStays)
{
    Hand h = Make({ "1", "1", "1", "1", "1", ".", "2", "2" }, { FS, FS });
    Finish(h, 4);
    EXPECT_EQ(h.w.regions.regions.Size(), 2u);
    ExpectChecksPass(h.w);
}

// ---------------------------------------------------------------- loose land (tiny components, unzoned)

TEST(RegionRepair, TinyComponentIsAbsorbedAndCounted)
{
    Hand h = Make({ "1", "1", "1", "1", "1", "u", "u" }, { FS }, AI);
    const RegionBuild b = Finish(h);
    EXPECT_EQ(b.looseLandPiecesAbsorbed, 1);
    EXPECT_EQ(b.orphanRegions, 0);
    EXPECT_EQ(At(h, 0, 5), 1); EXPECT_EQ(At(h, 0, 6), 1);
    EXPECT_EQ(h.w.regions.absorbedCells, 2);
    ExpectChecksPass(h.w);
}

TEST(RegionRepair, UnzonedCellIsAbsorbedAndCounted)
{
    Hand h = Make({ "1", "1", "1", "u" }, { FS }, kNoStratum);
    Finish(h);
    EXPECT_EQ(At(h, 0, 3), 1);
    EXPECT_EQ(h.w.regions.absorbedCells, 1);
    EXPECT_EQ(h.w.regions.fragmentCells, 0);
    ExpectChecksPass(h.w);
}

// AC3 amendment: a tiny zone fragment absorbed whole counts as fragmentCells, not absorbedCells.
TEST(RegionRepair, TinyFragmentAbsorbedCountsAsFragmentNotAbsorbed)
{
    Hand h = Make({ "1", "1", "1", "1", "1", "u", "u" }, { FS }, AI);
    MarkTiny(h, { { 0, 5 }, { 0, 6 } });
    Finish(h);
    EXPECT_EQ(At(h, 0, 5), 1); EXPECT_EQ(At(h, 0, 6), 1);
    EXPECT_EQ(h.w.regions.fragmentCells, 2);
    EXPECT_EQ(h.w.regions.absorbedCells, 0);
    ExpectChecksPass(h.w);
}

TEST(RegionRepair, FragmentAndUnzonedCountedSeparately)
{
    Hand h = Make({ "1", "1", "1", "1", "1", "u", "u", "u" }, { FS }, AI);
    h.w.At(0, 7).stratum = kNoStratum;   // unzoned, not part of the tiny fragment
    MarkTiny(h, { { 0, 5 }, { 0, 6 } });
    Finish(h);
    EXPECT_EQ(h.w.regions.fragmentCells, 2);
    EXPECT_EQ(h.w.regions.absorbedCells, 1);
    ExpectChecksPass(h.w);
}

TEST(RegionRepair, TinyFragmentJoiningSameZoneRegionIsNeitherFragmentNorAbsorbed)
{
    Hand h = Make({ "1", "1", "1", "u", "u" }, { AI }, AI);
    MarkTiny(h, { { 0, 3 }, { 0, 4 } });
    Finish(h);
    EXPECT_EQ(h.w.regions.fragmentCells, 0);
    EXPECT_EQ(h.w.regions.absorbedCells, 0);
}

TEST(RegionRepair, TinyFragmentThatBecomesOrphanRegionIsNotAFragment)
{
    Hand h = Make({ "1", "1", "1", "1", "1", ".", "u", "u" }, { FS }, AI);
    MarkTiny(h, { { 0, 6 }, { 0, 7 } });
    Finish(h);
    EXPECT_EQ(h.w.regions.fragmentCells, 0);   // own (orphan) region of its own zone
    EXPECT_EQ(h.w.regions.absorbedCells, 0);
}

// ---------------------------------------------------------------- AC4 amendment: terrain-limited after repair

TEST(RegionRepair, MergesBelowTenSetLimitedByToZoneThatLostRegions)
{
    Hand h = Make({ "1", "1", "1", "1", "2", "3", "3", "3", "3" }, { FS, WR, AI });
    h.g.targets.count[0] = 1; h.g.targets.count[1] = 1; h.g.targets.count[2] = 1;
    const RegionBuild b = Finish(h, 4);
    EXPECT_EQ(b.undersizedMerged, 1);   // the WildReaches region merged away
    EXPECT_EQ(h.w.regions.mainlandRegionCount, 2);
    EXPECT_EQ(h.w.regions.limitedBy, WR);
}

TEST(RegionRepair, GrowthLimitedByIsKeptAfterRepair)
{
    Hand h = Make({ "1", "1", "1", "1", "2", "3", "3", "3", "3" }, { FS, WR, AI });
    h.g.targets.count[0] = 1; h.g.targets.count[1] = 1; h.g.targets.count[2] = 1;
    h.g.targets.limitedBy = AI;
    Finish(h, 4);
    EXPECT_EQ(h.w.regions.limitedBy, AI);
}

TEST(RegionRepair, BelowTenDeficitTieGoesToZoneOrder)
{
    // Targets 2/2/1, final 1/1/1: FirstShores and WildReaches both lost one; zone order picks FirstShores.
    Hand h = Make({ "1", "1", "1", "2", "3", "3", "3", "4", "5", "5", "5" }, { FS, FS, WR, WR, AI });
    h.g.targets.count[0] = 2; h.g.targets.count[1] = 2; h.g.targets.count[2] = 1;
    Finish(h, 3);
    EXPECT_EQ(h.w.regions.mainlandRegionCount, 3);
    EXPECT_EQ(h.w.regions.limitedBy, FS);
}

TEST(RegionRepair, BelowTenWithoutDeficitStillReportsLimited)
{
    // Targets unset (0/0/0): no zone lost a region, yet the mainland is below 10. Never left None.
    Hand h = Make({ "1", "1", "2", "2", "3", "3" }, { FS, WR, AI });
    Finish(h);
    EXPECT_EQ(h.w.regions.mainlandRegionCount, 3);
    EXPECT_EQ(h.w.regions.limitedBy, FS);
}

// Exactly ten mainland regions is in range: not reported terrain-limited (mutation check, plan task 12).
TEST(RegionRepair, ExactlyTenMainlandRegionsIsNotTerrainLimited)
{
    Hand h = Make({ "123456789:" }, { FS, FS, FS, FS, FS, FS, FS, FS, FS, FS });
    Finish(h);
    EXPECT_EQ(h.w.regions.mainlandRegionCount, 10);
    EXPECT_EQ(h.w.regions.limitedBy, kNoStratum);
}

TEST(RegionRepair, NineMainlandRegionsIsTerrainLimited)
{
    Hand h = Make({ "123456789" }, { FS, FS, FS, FS, FS, FS, FS, FS, FS });
    Finish(h);
    EXPECT_EQ(h.w.regions.mainlandRegionCount, 9);
    EXPECT_NE(h.w.regions.limitedBy, kNoStratum);
}

TEST(RegionRepair, LoosePiecePrefersSameZoneRegion)
{
    Hand h = Make({ "1", "1", "1", "u", "u", "2", "2", "2" }, { FS, WR }, WR);
    Finish(h);
    EXPECT_EQ(At(h, 0, 3), 2); EXPECT_EQ(At(h, 0, 4), 2);   // equal borders; WildReaches piece joins the WR region
    EXPECT_EQ(h.w.regions.absorbedCells, 0);
    ExpectChecksPass(h.w);
}

TEST(RegionRepair, LoosePieceBehindAnotherPieceIsAbsorbedInLaterRound)
{
    Hand h = Make({ "1", "u", "u" }, { FS }, AI);
    h.w.At(0, 2).stratum = kNoStratum;   // second piece is unzoned, touches only the first piece
    const RegionBuild b = Finish(h);
    EXPECT_EQ(b.looseLandPiecesAbsorbed, 2);
    EXPECT_EQ(b.orphanRegions, 0);
    EXPECT_EQ(At(h, 0, 2), 1);
    EXPECT_EQ(h.w.regions.absorbedCells, 2);
    ExpectChecksPass(h.w);
}

TEST(RegionRepair, IsolatedPieceBecomesOrphanRegion)
{
    Hand h = Make({ "1", "1", "1", "1", "1", ".", "u", "u" }, { FS }, AI);
    const RegionBuild b = Finish(h);
    EXPECT_EQ(b.orphanRegions, 1);
    EXPECT_EQ(b.unassignedLandCells, 0u);
    ASSERT_EQ(h.w.regions.regions.Size(), 2u);
    EXPECT_EQ(At(h, 0, 6), 2); EXPECT_EQ(At(h, 0, 7), 2);
    EXPECT_EQ(h.w.regions.regions[1].stratum, AI);
    EXPECT_FALSE(h.w.regions.regions[1].isIsland);
    EXPECT_EQ(h.w.regions.absorbedCells, 0);
    ExpectChecksPass(h.w);
}

TEST(RegionRepair, OrphanThatDoesNotFitStaysZeroAndIsReported)
{
    Hand h;
    h.w = MakeBlankWorld(1, 66);
    h.g.regionOf.assign(66, kNoRegion);
    for (int i = 0; i < 64; ++i)
    {
        RegionSeed s; s.stratum = FS; s.landmass = 1; s.biome = G;
        h.g.seeds.push_back(s);
        SetLand(h.w, 0, i, FS, G);
        h.g.regionOf[i] = static_cast<RegionId>(i + 1);
    }
    SetOcean(h.w, 0, 64);
    SetLand(h.w, 0, 65, AI, G);
    const RegionBuild b = Finish(h);
    EXPECT_TRUE(b.overflow);
    EXPECT_EQ(b.orphanRegions, 0);
    EXPECT_EQ(b.unassignedLandCells, 1u);
    EXPECT_EQ(h.w.regions.regions.Size(), 64u);
    EXPECT_EQ(At(h, 0, 65), 0);
}

TEST(RegionRepair, GrowthOverflowIsPropagated)
{
    Hand h = Make({ "1", "1" }, { FS });
    h.g.overflow = true;
    EXPECT_TRUE(Finish(h).overflow);
}

// ---------------------------------------------------------------- islands

TEST(RegionRepair, IslandIsOneRegionWithMajorityZoneAndExemptFromRules)
{
    Hand h = Make({ "1", "1", "1", "1", "1", ".", "2", "2", "2" }, { FS, WR });
    for (int y = 6; y < 9; ++y) { h.w.At(0, y).landmass = 2; }
    h.w.At(0, 6).stratum = FS;   // 1 FirstShores vs 2 WildReaches cells
    h.g.seeds[1].isIsland = true;
    h.g.seeds[1].landmass = 2;
    const RegionBuild b = Finish(h, 4);   // island has 3 < 4 cells and a mixed zone: still untouched
    EXPECT_EQ(b.undersizedMerged, 0);
    ASSERT_EQ(h.w.regions.regions.Size(), 2u);
    const RegionRecord& isl = h.w.regions.regions[1];
    EXPECT_TRUE(isl.isIsland);
    EXPECT_EQ(isl.landmass, 2);
    EXPECT_EQ(isl.stratum, WR);
    EXPECT_EQ(isl.cellCount, 3);
    EXPECT_EQ(h.w.regions.absorbedCells, 0);
    EXPECT_EQ(h.w.regions.mainlandRegionCount, 1);
    ExpectChecksPass(h.w);
}

// ---------------------------------------------------------------- records

namespace
{
    Hand OneRegionStrip(int len)
    {
        return Make(std::vector<std::string>(static_cast<size_t>(len), "1"), { FS });
    }
}

TEST(RegionRepair, PrimaryBiomeIgnoresLakeCellsPurityCountsAllCells)
{
    Hand h = OneRegionStrip(10);
    for (int y = 0; y < 4; ++y) { SetBiome(h.w, 0, y, L); h.w.At(0, y).waterSource = WaterSource::Lake; }
    for (int y = 4; y < 7; ++y) { SetBiome(h.w, 0, y, G); }
    for (int y = 7; y < 10; ++y) { SetBiome(h.w, 0, y, F); }
    Finish(h);
    const RegionRecord& r = h.w.regions.regions[0];
    EXPECT_EQ(r.primaryBiome, G);   // 4 lake cells ignored; G 3 vs F 3 ties to the lowest enum
    EXPECT_FLOAT_EQ(r.purity, 0.3f);
    ExpectChecksPass(h.w);
}

TEST(RegionRepair, PrimaryBiomeMajorityAndTieLowestEnum)
{
    Hand h = OneRegionStrip(10);
    for (int y = 0; y < 6; ++y) { SetBiome(h.w, 0, y, F); }
    for (int y = 6; y < 10; ++y) { SetBiome(h.w, 0, y, G); }
    Finish(h);
    EXPECT_EQ(h.w.regions.regions[0].primaryBiome, F);
    EXPECT_FLOAT_EQ(h.w.regions.regions[0].purity, 0.6f);

    Hand t = OneRegionStrip(10);
    for (int y = 0; y < 5; ++y) { SetBiome(t.w, 0, y, F); }
    for (int y = 5; y < 10; ++y) { SetBiome(t.w, 0, y, G); }
    Finish(t);
    EXPECT_EQ(t.w.regions.regions[0].primaryBiome, G);
    EXPECT_FLOAT_EQ(t.w.regions.regions[0].purity, 0.5f);
}

TEST(RegionRepair, AllLakeRegionReportsLake)
{
    Hand h = OneRegionStrip(3);
    for (int y = 0; y < 3; ++y) { SetBiome(h.w, 0, y, L); h.w.At(0, y).waterSource = WaterSource::Lake; }
    Finish(h);
    EXPECT_EQ(h.w.regions.regions[0].primaryBiome, L);
    EXPECT_FLOAT_EQ(h.w.regions.regions[0].purity, 1.0f);
}

TEST(RegionRepair, CentreIsNearestCellToCentroid)
{
    Hand h = OneRegionStrip(9);
    Finish(h);
    EXPECT_EQ(h.w.regions.regions[0].centreCell, 4u);
}

TEST(RegionRepair, CentreMovesOffLakeToLowestIndexNearest)
{
    Hand h = OneRegionStrip(9);
    SetBiome(h.w, 0, 4, L); h.w.At(0, 4).waterSource = WaterSource::Lake;
    Finish(h);
    EXPECT_EQ(h.w.regions.regions[0].centreCell, 3u);
}

TEST(RegionRepair, CentreOfUShapedRegionIsARegionCell)
{
    // U: columns 0 and 2 plus the bottom link; the centroid sits in the ocean gap.
    std::vector<std::string> rows(6, "1.1");
    rows[4] = "111"; rows[5] = "...";
    Hand h = Make(rows, { FS });
    Finish(h);
    ExpectChecksPass(h.w);
    const uint32_t c = h.w.regions.regions[0].centreCell;
    EXPECT_EQ(h.w.Cells()[c].region, 1);
}

TEST(RegionRepair, NeighboursAreSymmetricAndMatchEdgeScan)
{
    Hand h = Make({ "11", "32", "32", "11", "11" }, { FS, FS, FS });
    Finish(h);
    ExpectChecksPass(h.w);
    EXPECT_TRUE(h.w.regions.AreAdjacent(1, 2));
    EXPECT_TRUE(h.w.regions.AreAdjacent(2, 3));
    EXPECT_FALSE(h.w.regions.AreAdjacent(1, 1));
}

TEST(RegionRepair, DistantRegionsAreNotNeighbours)
{
    Hand h = Make({ "1", "1", ".", "2", "2" }, { FS, FS });
    Finish(h);
    EXPECT_EQ(h.w.regions.regions[0].neighbours.Size(), 0u);
    EXPECT_FALSE(h.w.regions.AreAdjacent(1, 2));
}

TEST(RegionRepair, NeighbourOverflowKeepsLowestSixteenAndFlags)
{
    // Hub = column 0 rows 0..19; each (1,y) is its own region touching the hub.
    const int H = 20;
    Hand h;
    h.w = MakeBlankWorld(2, H);
    h.g.regionOf.assign(static_cast<size_t>(2) * H, kNoRegion);
    RegionSeed hub; hub.stratum = FS; hub.landmass = 1; hub.biome = G;
    h.g.seeds.push_back(hub);
    for (int y = 0; y < H; ++y)
    {
        SetLand(h.w, 0, y, FS, G);
        h.g.regionOf[static_cast<size_t>(y) * 2] = 1;
        SetLand(h.w, 1, y, FS, G);
        h.g.regionOf[static_cast<size_t>(y) * 2 + 1] = static_cast<RegionId>(2 + y);
        h.g.seeds.push_back(hub);
    }
    const RegionBuild b = Finish(h);
    EXPECT_TRUE(b.neighbourOverflow);
    ASSERT_EQ(h.w.regions.regions.Size(), 21u);
    const RegionRecord& r = h.w.regions.regions[0];
    ASSERT_EQ(r.neighbours.Size(), RegionRecord::kMaxNeighbours);
    for (unsigned i = 0; i < r.neighbours.Size(); ++i) { EXPECT_EQ(r.neighbours[i], i + 2); }
    EXPECT_TRUE(h.w.regions.AreAdjacent(1, 21));   // symmetric answer even though 1's list is truncated
    const CheckErrors e = CheckNeighbours(h.w);
    EXPECT_TRUE(e.empty()) << ChkJoin(e);
}

TEST(RegionRepair, ClearsPreviousContentsAndIsIdempotent)
{
    Hand h = Make({ "11", "32", "32", "11", "11" }, { FS, FS, FS });
    h.w.regions.AddRegion(RegionRecord{});
    h.w.regions.absorbedCells = 99;
    h.w.At(0, 0).region = 7;
    Finish(h);
    const RegionTable first = h.w.regions;
    const std::vector<RegionId> firstIds = [&] { std::vector<RegionId> v; for (int i = 0; i < h.w.CellCount(); ++i) { v.push_back(h.w.Cells()[i].region); } return v; }();
    EXPECT_EQ(first.absorbedCells, 0);
    Finish(h);
    EXPECT_TRUE(first == h.w.regions);
    for (int i = 0; i < h.w.CellCount(); ++i) { EXPECT_EQ(h.w.Cells()[i].region, firstIds[i]); }
}

// ---------------------------------------------------------------- generated worlds

namespace
{
    const GridDimensions kSmall{ 66, 42 };

    std::string FindShippedPresetDir()
    {
        std::error_code       ec;
        std::filesystem::path dir = std::filesystem::absolute(std::filesystem::path(__FILE__), ec).parent_path();
        while (!dir.empty())
        {
            const std::filesystem::path candidate = dir / "Cluiche/Assets/CoW/Global/WorldGen/Presets";
            if (std::filesystem::exists(candidate, ec)) { return candidate.string(); }
            if (dir == dir.parent_path()) { break; }
            dir = dir.parent_path();
        }
        return {};
    }

    WorldShapeParams LoadParams(const char* id)
    {
        WorldShapePreset  p;
        RuleWarnings      w;
        const std::string dir = FindShippedPresetDir();
        EXPECT_FALSE(dir.empty());
        const PresetResult r = LoadPreset(dir, id, p, w);
        EXPECT_TRUE(r.Ok()) << r.message;
        return p.params;
    }

    struct Sweep
    {
        int      worlds = 0;
        int      regions = 0;
        int      lowPurityRegions = 0;       // purity < 0.6 (tuning-gated)
        int      mainMin = 1000, mainMax = 0, overAbsorb = 0;
        int      orphanRegions = 0, neighbourOverflows = 0, splitMoved = 0, merged = 0, limited = 0, belowTen = 0;
        uint32_t maxRegionCells = 0;
        float    maxRegionShare = 0.0f;      // of main-landmass land (tuning-gated)
        float    minPurity = 1.0f, purityTotal = 0.0f;
        float    maxAbsorbedShare = 0.0f;
        float    maxFragmentShare = 0.0f;
        int      overMaxSize = 0;            // AC5 upper bound kMaxRegionShare * mean + kMaxRegionSlack (tuning-gated)
        int      zoneBelowMinAfterRepair = 0; // worlds where merges left a zone under its range minimum (total >= 10)
    };

    // limits=false: structural checks only (AC2, AC6, islands, purity bookkeeping). limits=true: also AC3's absorbed
    // share and AC4's region counts.
    void CheckGeneratedWorld(const char* label, uint64_t seed, const WorldShapeParams& shape, Sweep& st, bool limits)
    {
        SCOPED_TRACE(std::string(label) + " seed " + std::to_string(seed));
        TerrainRunner runner;
        runner.Run(seed, shape, kSmall);
        WorldStructure w = runner.World();
        const TerrainRules rules = TestRules();
        RandomStream rng(7);
        const RegionGrowth g = BuildProvisionalRegions(w, rules, rng, ShippedBiomeRegistry());
        const RegionBuild b = FinishRegions(w, rules, g, ShippedBiomeRegistry());

        EXPECT_FALSE(b.overflow);
        EXPECT_EQ(b.unassignedLandCells, 0u);
        const CheckErrors e = CheckAllRegions(w, limits ? rules.kMaxAbsorbedShare : -1.0f);
        EXPECT_TRUE(e.empty()) << ChkJoin(e);

        // AC4: counts.
        int mainland = 0;
        uint32_t mainLand = 0;
        for (unsigned i = 0; i < w.regions.regions.Size(); ++i) { const RegionRecord& r = w.regions.regions[i]; if (!r.isIsland) { ++mainland; mainLand += r.cellCount; } }
        EXPECT_EQ(w.regions.mainlandRegionCount, mainland);
        // AC4 (amended): growth's limitedBy is kept; repair dropping the mainland below 10 also reports terrain-limited.
        if (g.targets.limitedBy != kNoStratum) { EXPECT_EQ(w.regions.limitedBy, g.targets.limitedBy); }
        else if (mainland < 10) { EXPECT_NE(w.regions.limitedBy, kNoStratum); }
        else { EXPECT_EQ(w.regions.limitedBy, kNoStratum); }
        // AC3 (amended): tiny-fragment cells have their own bound.
        if (limits) { EXPECT_LE(static_cast<float>(w.regions.fragmentCells), rules.kMaxFragmentShare * static_cast<float>(ChkLandCount(w)))
            << "fragmentCells " << w.regions.fragmentCells; }
        if (limits) { EXPECT_TRUE((mainland >= 10 && mainland <= 16) || w.regions.limitedBy != kNoStratum)
            << "mainland regions " << mainland << " targets " << int(g.targets.total); }
        // AC4 per zone: First Shores 4-6, Wild Reaches 4-6, Ancient Interiors 2-4 unless terrain-limited.
        int zoneCount[kMaxStrata] = {};
        for (unsigned i = 0; i < w.regions.regions.Size(); ++i)
        {
            const RegionRecord& r = w.regions.regions[i];
            const int z = StratumSlot(r.stratum);
            if (!r.isIsland && z >= 0 && z < kMaxStrata) { ++zoneCount[z]; }
        }
        // Decision (user, 2026-11-19): the AC4 amendment stands, total-only. Terrain-limited is reported only when the
        // mainland total drops below 10; a single zone ending under its minimum (archipelago, e.g. seed 514 slot 2)
        // while the total stays >= 10 is accepted. Zone upper bounds are asserted; zone shortfalls are counted and
        // printed (zoneBelowMinAfterRepair), never failed.
        if (limits && w.regions.limitedBy == kNoStratum)
        {
            bool below = false;
            for (int z = 0; z < kMaxStrata; ++z)
            {
                EXPECT_LE(zoneCount[z], ShippedStratumTable().Depth()->strata[z].regionsMax) << "zone slot " << z;
                below |= zoneCount[z] < ShippedStratumTable().Depth()->strata[z].regionsMin;
            }
            st.zoneBelowMinAfterRepair += below ? 1 : 0;
        }
        // AC5: every mainland region reaches kMinRegionCells; the upper bound is tuning-gated (counted, printed).
        const int minCells = rules.kMinRegionCells;
        const float meanSize = mainland ? static_cast<float>(mainLand) / mainland : 0.0f;
        const float maxSize = rules.kMaxRegionShare * meanSize + static_cast<float>(rules.kMaxRegionSlack);
        for (unsigned i = 0; i < w.regions.regions.Size(); ++i)
        {
            const RegionRecord& r = w.regions.regions[i];
            if (r.isIsland) { continue; }
            if (limits) { EXPECT_GE(static_cast<int>(r.cellCount), minCells) << "region " << int(r.id); }
            st.overMaxSize += static_cast<float>(r.cellCount) > maxSize ? 1 : 0;
        }

        const float land = static_cast<float>(ChkLandCount(w));
        ++st.worlds;
        st.overAbsorb += w.regions.absorbedCells > rules.kMaxAbsorbedShare * static_cast<float>(ChkLandCount(w));
        st.regions += static_cast<int>(w.regions.regions.Size());
        st.mainMin = std::min(st.mainMin, mainland);
        st.mainMax = std::max(st.mainMax, mainland);
        st.orphanRegions += b.orphanRegions; st.neighbourOverflows += b.neighbourOverflow;
        st.splitMoved += b.splitPiecesMoved; st.merged += b.undersizedMerged;
        st.limited += w.regions.limitedBy != kNoStratum;
        st.belowTen += mainland < 10;
        st.maxAbsorbedShare = std::max(st.maxAbsorbedShare, land > 0 ? w.regions.absorbedCells / land : 0.0f);
        st.maxFragmentShare = std::max(st.maxFragmentShare, land > 0 ? w.regions.fragmentCells / land : 0.0f);
        for (unsigned ri = 0; ri < w.regions.regions.Size(); ++ri)
        {
            const RegionRecord& r = w.regions.regions[ri];
            st.maxRegionCells = std::max<uint32_t>(st.maxRegionCells, r.cellCount);
            if (!r.isIsland && mainLand > 0) { st.maxRegionShare = std::max(st.maxRegionShare, static_cast<float>(r.cellCount) / mainLand); }
            st.minPurity = std::min(st.minPurity, r.purity);
            st.purityTotal += r.purity;
            st.lowPurityRegions += r.purity < 0.6f;
        }
    }

    void PrintSweep(const char* label, const Sweep& s)
    {
        std::printf("[RegionRepair sweep] %s: worlds %d regions %d mainland[%d..%d] belowTen %d overAbsorb %d limitedBy %d | maxRegionCells %u "
                    "maxRegionShareOfMain %.3f | purity min %.3f avg %.3f lowPurity(<0.6) %d | absorbedMax %.4f fragmentMax %.4f | orphans %d "
                    "neighbourOverflow %d splitMoved %d merged %d | overMaxSize(AC5, tuning-gated) %d zoneBelowMinAfterRepair %d\n",
                    label, s.worlds, s.regions, s.mainMin, s.mainMax, s.belowTen, s.overAbsorb, s.limited, s.maxRegionCells, s.maxRegionShare,
                    s.minPurity, s.regions ? s.purityTotal / s.regions : 0.0f, s.lowPurityRegions, s.maxAbsorbedShare, s.maxFragmentShare,
                    s.orphanRegions, s.neighbourOverflows, s.splitMoved, s.merged, s.overMaxSize, s.zoneBelowMinAfterRepair);
    }
}

TEST(RegionRepair, GeneratedWorldsDefaultPresetThreeSeeds)
{
    Sweep st;
    for (uint64_t seed : { 123ull, 4ull, 77ull }) { CheckGeneratedWorld("default", seed, WorldShapeParams{}, st, true); }
    PrintSweep("default x3", st);
}

TEST(RegionRepair, BuildRegionsEqualsProvisionalPlusFinish)
{
    TerrainRunner runner;
    runner.Run(123, WorldShapeParams{}, kSmall);
    WorldStructure a = runner.World();
    WorldStructure b = runner.World();
    const TerrainRules rules = TestRules();
    RandomStream ra(7), rb(7);
    BuildRegions(a, rules, ra, ShippedBiomeRegistry());
    const RegionGrowth g = BuildProvisionalRegions(b, rules, rb, ShippedBiomeRegistry());
    FinishRegions(b, rules, g, ShippedBiomeRegistry());
    EXPECT_TRUE(a.regions == b.regions);
    for (int i = 0; i < a.CellCount(); ++i) { ASSERT_EQ(a.Cells()[i].region, b.Cells()[i].region); }
}

TEST(RegionRepair, GeneratedWorldOtherCellFieldsUntouchedAndRebuildIsIdentical)
{
    TerrainRunner runner;
    runner.Run(123, WorldShapeParams{}, kSmall);
    WorldStructure w = runner.World();
    const std::vector<Cell> before(w.Cells(), w.Cells() + w.CellCount());
    const TerrainRules rules = TestRules();
    RandomStream r1(7);
    BuildRegions(w, rules, r1, ShippedBiomeRegistry());
    for (int i = 0; i < w.CellCount(); ++i)
    {
        const Cell& a = before[i];
        const Cell& c = w.Cells()[i];
        ASSERT_TRUE(a.elevation == c.elevation && a.latitude == c.latitude && a.waterSource == c.waterSource &&
                    a.terrain == c.terrain && a.stratum == c.stratum && a.landmass == c.landmass &&
                    a.terrainFlags == c.terrainFlags && a.downstream == c.downstream && a.biome == c.biome)
            << "cell " << i;
    }
    const RegionTable first = w.regions;
    RandomStream r2(7);
    BuildRegions(w, rules, r2, ShippedBiomeRegistry());   // rebuild over existing regions
    EXPECT_TRUE(first == w.regions);
}

namespace
{
    void SweepPresets(bool limits)
    {
        for (const char* id : { "continental", "archipelago", "highlands" })
        {
            Sweep st;
            const WorldShapeParams shape = LoadParams(id);
            for (uint64_t seed = 500; seed < 520; ++seed) { CheckGeneratedWorld(id, seed, shape, st, limits); }
            PrintSweep(id, st);
        }
    }
}

TEST(SLOW_RegionRepair, GeneratedWorldsThreePresetsTwentySeedsStructure)
{
    SweepPresets(false);
}

// AC3 absorbed share and AC4 counts over the sweep.
TEST(SLOW_RegionRepair, GeneratedWorldsThreePresetsTwentySeedsCountsAndAbsorbLimits)
{
    SweepPresets(true);
}

// ---------------------------------------------------------------------------
// Biome registry (plan Task 22, AC 6 region-repair bullet): ocean and lake come from the registry roles, not from
// fixed enum values. The fixture swaps the roles: row 0 (Biome.Ocean) becomes the lake-role row and row 1 (Biome.Lake)
// becomes the ocean-role row, so neither role sits on its legacy row.
// ---------------------------------------------------------------------------
namespace
{
    const CoW::World::BiomeRegistry& RolesSwappedRegistry()
    {
        static const CoW::World::BiomeRegistry reg = LoadShippedVariant({
            { "\"roles\": [\"ocean\"]", "\"roles\": [\"lakeTmp\"]" },
            { "\"roles\": [\"lake\"]",  "\"roles\": [\"ocean\"]" },
            { "\"roles\": [\"lakeTmp\"]", "\"roles\": [\"lake\"]" } });
        return reg;
    }
}

TEST(RegionRepair, RegistryRoleFixtureHasOceanAndLakeOffTheirLegacyRows)
{
    const auto& reg = RolesSwappedRegistry();
    EXPECT_EQ(reg.Role(CoW::World::BiomeRole::Lake), 0);
    EXPECT_EQ(reg.Role(CoW::World::BiomeRole::Ocean), 1);
}

TEST(RegionRepair, MajorityIgnoresTheRegistrysLakeRoleRow)
{
    // 3 cells of row 0 (the lake-role row here) and 1 grassland cell: the lake-role cells never win the majority.
    Hand h = Make({ "1111" }, { FS }, FS, RolesSwappedRegistry().Role(CoW::World::BiomeRole::Lake));
    SetBiome(h.w, 3, 0, B("Biome.Grassland"));
    const RegionBuild b = FinishRegions(h.w, RulesMin(1), h.g, RolesSwappedRegistry());
    ASSERT_EQ(b.regionCount, 1u);
    const RegionRecord& rec = h.w.regions.regions[0];
    EXPECT_EQ(rec.primaryBiome, B("Biome.Grassland"));
    EXPECT_FLOAT_EQ(rec.purity, 0.25f);
}

TEST(RegionRepair, AllLakeRoleRegionReportsTheRegistrysLakeRow)
{
    Hand h = Make({ "111" }, { FS }, FS, RolesSwappedRegistry().Role(CoW::World::BiomeRole::Lake));
    FinishRegions(h.w, RulesMin(1), h.g, RolesSwappedRegistry());
    const RegionRecord& rec = h.w.regions.regions[0];
    EXPECT_EQ(rec.primaryBiome, RolesSwappedRegistry().Role(CoW::World::BiomeRole::Lake));
    EXPECT_FLOAT_EQ(rec.purity, 1.0f);
}

TEST(RegionRepair, RegionWithNoCellsReportsTheRegistrysOceanRow)
{
    WorldStructure w = MakeBlankWorld(2, 2);
    RegionRecord rec;
    rec.id = 1; rec.stratum = FS; rec.landmass = 1;
    w.regions.AddRegion(rec);
    RecomputeRegionRecords(w, RolesSwappedRegistry());
    EXPECT_EQ(w.regions.regions[0].primaryBiome, RolesSwappedRegistry().Role(CoW::World::BiomeRole::Ocean));
    EXPECT_EQ(w.regions.regions[0].cellCount, 0u);
    EXPECT_FLOAT_EQ(w.regions.regions[0].purity, 0.0f);
}

TEST(RegionRepair, ShippedRegistryKeepsOceanAtRowZeroAndLakeAtRowOne)
{
    WorldStructure w = MakeBlankWorld(2, 2);
    RegionRecord rec;
    rec.id = 1; rec.stratum = FS; rec.landmass = 1;
    w.regions.AddRegion(rec);
    RecomputeRegionRecords(w, ShippedBiomeRegistry());
    EXPECT_EQ(w.regions.regions[0].primaryBiome, B("Biome.Ocean"));
}
