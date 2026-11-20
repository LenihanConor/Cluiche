// S-1 Nodes: built-in corridor node placer (hand-built barriers + generated-world AC8a sweep).
#include <gtest/gtest.h>
#include <Shared/WorldGeneration/NodesCorridor.h>
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

using namespace CoW::WorldGeneration;
using namespace CoW::WorldGeneration::Testing;
using Dia::Maths::RandomStream;

namespace
{
    TerrainRules Rules() { return TestRules(); }

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

    // 60x16 world: barrier 1 on rows 4/5, barrier 2 on rows 10/11.
    WorldStructure Zoned() { return MakeZonedBarrierWorld(60, 16, 5, 11); }

    CorridorResult Place(WorldStructure& w, unsigned seed)
    {
        RandomStream rng(seed);
        return PlaceCorridorNodes(w, Rules(), rng);
    }

    struct Corridor
    {
        int barrier = 0;
        int ordinal = 0;
        std::vector<int> cells;
        std::vector<Site> sites;
    };

    // Groups the Corridor sites by (barrier, variant).
    std::map<std::pair<int, int>, Corridor> Corridors(const WorldStructure& w)
    {
        std::map<std::pair<int, int>, Corridor> m;
        for (const Site& s : w.sites.Of(BuiltinNodeKinds::Corridor()))
        {
            Corridor& c = m[{ CorridorBarrier(s.flags), s.variant }];
            c.barrier = CorridorBarrier(s.flags);
            c.ordinal = s.variant;
            c.cells.push_back(static_cast<int>(s.cell));
            c.sites.push_back(s);
        }
        return m;
    }

    int CountCorridors(const WorldStructure& w, int barrier)
    {
        int n = 0;
        for (const auto& kv : Corridors(w)) { n += kv.second.barrier == barrier; }
        return n;
    }

    void ExpectDecodesConsistently(const WorldStructure& w)
    {
        for (const auto& kv : Corridors(w))
        {
            const Corridor& c = kv.second;
            for (const Site& s : c.sites)
            {
                EXPECT_EQ(CorridorBarrier(s.flags), static_cast<int>(s.flags & 3));
                EXPECT_EQ(CorridorSource(s.flags), static_cast<int>((s.flags >> 2) & 3));
                EXPECT_EQ(CorridorWidth(s.flags), static_cast<int>(((s.flags >> 4) & 3) + 1));
                EXPECT_EQ(CorridorWidth(s.flags), static_cast<int>(c.cells.size()));
                EXPECT_EQ(CorridorSource(s.flags), CorridorSource(c.sites[0].flags));
            }
        }
    }

    // Passes at x = 5, 15, ... 55 on barrier-1 row 5 (10 apart).
    WorldStructure SixPasses()
    {
        WorldStructure w = Zoned();
        for (int x = 5; x <= 55; x += 10) { MarkPass(w, x, 5); }
        return w;
    }

    // Some node of the later corridor is >= kCorridorSpacing from every node of the earlier one.
    bool LaterCorridorHasFarNode(const WorldStructure& w, const Corridor& earlier, const Corridor& later)
    {
        for (int cb : later.cells)
        {
            bool ok = true;
            for (int ca : earlier.cells) { ok &= Dist(w, ca, cb) >= 6; }
            if (ok) { return true; }
        }
        return false;
    }
}

// ---------------------------------------------------------------- decoders

TEST(NodesCorridor, FlagEncodeDecodeRoundTrips)
{
    for (int b = 1; b <= 2; ++b)
    {
        for (int s = 0; s < 3; ++s)
        {
            for (int wd = 1; wd <= 3; ++wd)
            {
                const uint16_t f = MakeCorridorFlags(b, s, wd);
                EXPECT_EQ(CorridorBarrier(f), b);
                EXPECT_EQ(CorridorSource(f), s);
                EXPECT_EQ(CorridorWidth(f), wd);
            }
        }
    }
    EXPECT_EQ(MakeCorridorFlags(2, 1, 3), static_cast<uint16_t>(2 | (1 << 2) | (2 << 4)));
}

// ---------------------------------------------------------------- hand-built barriers

TEST(NodesCorridor, NoCandidatesMeansBothBarriersLimitedAndNoSites)
{
    WorldStructure w = Zoned();
    const CorridorResult r = Place(w, 1);
    EXPECT_TRUE(r.corridorLimited[0]);
    EXPECT_TRUE(r.corridorLimited[1]);
    EXPECT_EQ(r.accepted[0], 0); EXPECT_EQ(r.accepted[1], 0);
    EXPECT_EQ(r.candidates[0], 0); EXPECT_EQ(r.candidates[1], 0);
    EXPECT_EQ(w.sites.Count(), 0);
}

TEST(NodesCorridor, PassesOnlyAreSingleNodePassCorridors)
{
    for (unsigned seed = 1; seed <= 12; ++seed)
    {
        WorldStructure w = SixPasses();
        const CorridorResult r = Place(w, seed);
        EXPECT_EQ(r.candidates[0], 6);
        EXPECT_EQ(r.candidatesByType[0][0], 6);
        EXPECT_EQ(r.candidatesByType[0][1], 0);
        EXPECT_EQ(r.candidatesByType[0][2], 0);
        EXPECT_GE(r.accepted[0], 2); EXPECT_LE(r.accepted[0], 4);
        EXPECT_FALSE(r.corridorLimited[0]);
        EXPECT_EQ(CountCorridors(w, 1), r.accepted[0]);
        EXPECT_EQ(w.sites.Count(), r.accepted[0]);
        for (const Site& s : w.sites.All())
        {
            EXPECT_TRUE(w.Cells()[s.cell].terrainFlags & TerrainFlag::Pass);
            EXPECT_EQ(CorridorWidth(s.flags), 1);
            EXPECT_EQ(CorridorSource(s.flags), 0);
            EXPECT_EQ(CorridorBarrier(s.flags), 1);
        }
        ExpectDecodesConsistently(w);
    }
}

TEST(NodesCorridor, CountIsCappedAtFourAndReachesEveryValueFromTwo)
{
    std::set<int> seen;
    for (unsigned seed = 1; seed <= 40; ++seed)
    {
        WorldStructure w = SixPasses();
        const CorridorResult r = Place(w, seed);
        EXPECT_LE(r.accepted[0], 4);
        EXPECT_GE(r.accepted[0], 2);
        seen.insert(r.accepted[0]);
    }
    EXPECT_EQ(seen, (std::set<int>{ 2, 3, 4 }));
}

TEST(NodesCorridor, VariantIsBarrierOrdinalZeroBased)
{
    WorldStructure w = SixPasses();
    const CorridorResult r = Place(w, 3);
    std::set<int> variants;
    for (const Site& s : w.sites.All()) { variants.insert(s.variant); }
    std::set<int> want;
    for (int i = 0; i < r.accepted[0]; ++i) { want.insert(i); }
    EXPECT_EQ(variants, want);
}

TEST(NodesCorridor, RiverCutsAreBroadCorridorsOfTwoToThreeNodes)
{
    for (unsigned seed = 1; seed <= 12; ++seed)
    {
        WorldStructure w = Zoned();
        for (int x = 10; x <= 12; ++x) { MarkRiverCut(w, x, 5); }
        for (int x = 40; x <= 42; ++x) { MarkRiverCut(w, x, 5); }
        const CorridorResult r = Place(w, seed);
        EXPECT_EQ(r.candidatesByType[0][1], 6);
        EXPECT_EQ(r.candidatesByType[0][0], 0);
        EXPECT_EQ(r.accepted[0], 2);   // two clusters far apart; one corridor per cluster under the spacing
        EXPECT_FALSE(r.corridorLimited[0]);
        for (const auto& kv : Corridors(w))
        {
            const Corridor& c = kv.second;
            EXPECT_GE(c.cells.size(), 2u);   // a river cluster of 3 always has an adjacent boundary neighbour
            EXPECT_LE(c.cells.size(), 3u);
            for (int cell : c.cells) { EXPECT_TRUE(w.Cells()[cell].terrainFlags & TerrainFlag::RiverCrossesBarrier); }
            for (const Site& s : c.sites) { EXPECT_EQ(CorridorSource(s.flags), 1); }
        }
        ExpectDecodesConsistently(w);
    }
}

TEST(NodesCorridor, CoastalGapNeedsCoastWithinReachAndNoRidgeWithinTwo)
{
    WorldStructure base = Zoned();
    MarkCoast(base, 10, 5);
    MarkCoast(base, 45, 5);
    WorldStructure withRidge = base;
    MarkRidge(withRidge, 10, 4);

    WorldStructure a = base, b = withRidge;
    const CorridorResult ra = Place(a, 5);
    const CorridorResult rb = Place(b, 5);
    EXPECT_GT(ra.candidatesByType[0][2], 0);
    EXPECT_EQ(ra.candidatesByType[0][0], 0);
    EXPECT_EQ(ra.candidatesByType[0][1], 0);
    EXPECT_LT(rb.candidatesByType[0][2], ra.candidatesByType[0][2]);
    EXPECT_GE(ra.accepted[0], 2);
    for (const Site& s : a.sites.All())
    {
        EXPECT_EQ(CorridorSource(s.flags), 2);
        bool nearCoast = false;
        for (int c : { CellIndex(a, 10, 5), CellIndex(a, 45, 5) }) { nearCoast |= Dist(a, c, static_cast<int>(s.cell)) <= 4; }
        EXPECT_TRUE(nearCoast) << "cell " << s.cell;
        EXPECT_GE(CorridorWidth(s.flags), 1); EXPECT_LE(CorridorWidth(s.flags), 3);
    }
    for (const Site& s : b.sites.All()) { EXPECT_GT(Dist(b, CellIndex(b, 10, 4), static_cast<int>(s.cell)), 2); }
    ExpectDecodesConsistently(a);
}

TEST(NodesCorridor, CoastalGapReachIsFourSteps)
{
    WorldStructure w = Zoned();
    MarkCoast(w, 30, 0);   // distance 4 to (30,4), 5 to (30,5)
    const CorridorResult r = Place(w, 1);
    EXPECT_GE(r.candidatesByType[0][2], 1);
    EXPECT_FALSE(w.sites.Has(CellIndex(w, 30, 5)));
    for (const Site& s : w.sites.All()) { EXPECT_LE(Dist(w, CellIndex(w, 30, 0), static_cast<int>(s.cell)), 4); }
}

TEST(NodesCorridor, PassesAndRiversArePreferredOverCoastalGaps)
{
    for (unsigned seed = 1; seed <= 15; ++seed)
    {
        WorldStructure w = Zoned();
        MarkPass(w, 6, 5);
        MarkRiverCut(w, 50, 5);
        MarkCoast(w, 28, 5);   // plenty of coastal-gap candidates in the middle
        const CorridorResult r = Place(w, seed);
        EXPECT_GT(r.candidatesByType[0][2], 0);
        EXPECT_TRUE(w.sites.Has(CellIndex(w, 6, 5)));
        EXPECT_TRUE(w.sites.Has(CellIndex(w, 50, 5)));
        EXPECT_GE(r.accepted[0], 2);
    }
}

TEST(NodesCorridor, FewerThanTwoAcceptedIsLimitedNotFailure)
{
    WorldStructure w = Zoned();
    MarkPass(w, 20, 5);
    const CorridorResult r = Place(w, 4);
    EXPECT_EQ(r.accepted[0], 1);
    EXPECT_TRUE(r.corridorLimited[0]);
    EXPECT_EQ(w.sites.Count(), 1);
}

TEST(NodesCorridor, CloseCandidatesRespectCorridorSpacing)
{
    for (unsigned seed = 1; seed <= 10; ++seed)
    {
        WorldStructure w = Zoned();
        MarkPass(w, 20, 5);
        MarkPass(w, 24, 5);   // distance 4 < kCorridorSpacing 6
        const CorridorResult r = Place(w, seed);
        EXPECT_EQ(r.candidates[0], 2);
        EXPECT_EQ(r.accepted[0], 1);
        EXPECT_TRUE(r.corridorLimited[0]);
    }
}

TEST(NodesCorridor, LakeAndOceanCellsAreNeverCandidates)
{
    WorldStructure w = Zoned();
    MarkPass(w, 10, 5);
    w.At(10, 5).waterSource = WaterSource::Lake;
    MarkPass(w, 30, 5);
    SetOcean(w, 30, 5);
    MarkPass(w, 50, 5);
    const CorridorResult r = Place(w, 1);
    EXPECT_EQ(r.candidates[0], 1);
    EXPECT_FALSE(w.sites.Has(CellIndex(w, 10, 5)));
    EXPECT_FALSE(w.sites.Has(CellIndex(w, 30, 5)));
}

TEST(NodesCorridor, BarrierTwoUsesWildReachesAncientInteriorsBoundary)
{
    WorldStructure w = Zoned();
    MarkPass(w, 10, 5);    // barrier 1
    MarkPass(w, 40, 5);
    MarkPass(w, 12, 11);   // barrier 2
    MarkPass(w, 30, 10);
    MarkPass(w, 50, 11);
    MarkPass(w, 30, 8);    // interior of WR: on neither boundary
    const CorridorResult r = Place(w, 2);
    EXPECT_EQ(r.candidates[0], 2);
    EXPECT_EQ(r.candidates[1], 3);
    EXPECT_FALSE(w.sites.Has(CellIndex(w, 30, 8)));
    for (const Site& s : w.sites.All())
    {
        const int y = static_cast<int>(s.cell) / w.dimensions.width;
        if (CorridorBarrier(s.flags) == 1) { EXPECT_TRUE(y == 4 || y == 5); }
        else { EXPECT_TRUE(y == 10 || y == 11); }
    }
    EXPECT_GE(r.accepted[1], 2);
    EXPECT_EQ(CountCorridors(w, 2), r.accepted[1]);
}

TEST(NodesCorridor, SiteRegionComesFromTheCell)
{
    WorldStructure w = Zoned();
    MarkPass(w, 10, 5);
    w.At(10, 5).region = 3;
    Place(w, 1);
    const Site* s = w.sites.Find(BuiltinNodeKinds::Corridor(), CellIndex(w, 10, 5));
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->region, 3);
}

TEST(NodesCorridor, CandidateCountsAreIndependentOfTheStream)
{
    CorridorResult first;
    for (unsigned seed = 1; seed <= 10; ++seed)
    {
        WorldStructure w = SixPasses();
        MarkCoast(w, 30, 5);
        const CorridorResult r = Place(w, seed);
        if (seed == 1) { first = r; continue; }
        EXPECT_EQ(r.candidates[0], first.candidates[0]);
        for (int t = 0; t < 3; ++t) { EXPECT_EQ(r.candidatesByType[0][t], first.candidatesByType[0][t]); }
    }
}

TEST(NodesCorridor, SameStreamIsDeterministicAndDifferentStreamsDiffer)
{
    WorldStructure a = SixPasses(), b = SixPasses();
    Place(a, 7); Place(b, 7);
    EXPECT_TRUE(a.sites == b.sites);
    bool differ = false;
    for (unsigned seed = 8; seed < 30 && !differ; ++seed)
    {
        WorldStructure c = SixPasses();
        Place(c, seed);
        differ = !(c.sites == a.sites);
    }
    EXPECT_TRUE(differ);
}

TEST(NodesCorridor, DoesNotTouchExistingSites)
{
    WorldStructure w = SixPasses();
    Site hunt; hunt.kindCrc = SiteKindId(BuiltinNodeKinds::Hunting()); hunt.cell = 100; hunt.variant = 9;
    Site pre; pre.kindCrc = SiteKindId(BuiltinNodeKinds::Corridor()); pre.cell = CellIndex(w, 5, 5); pre.variant = 77; pre.flags = 3;
    ASSERT_TRUE(w.sites.Add(hunt));
    ASSERT_TRUE(w.sites.Add(pre));
    const CorridorResult r = Place(w, 3);
    EXPECT_EQ(w.sites.Find(BuiltinNodeKinds::Hunting(), 100)->variant, 9);
    EXPECT_EQ(w.sites.Find(BuiltinNodeKinds::Corridor(), pre.cell)->variant, 77);   // pre-existing record untouched
    EXPECT_EQ(w.sites.Count(), 2 + r.accepted[0] + r.accepted[1]);
}

TEST(NodesCorridor, CorridorsKeepSpacingBetweenAcceptedCells)
{
    for (unsigned seed = 1; seed <= 15; ++seed)
    {
        WorldStructure w = Zoned();
        for (int x = 4; x <= 56; x += 4) { MarkRiverCut(w, x, 5); }   // dense candidates
        Place(w, seed);
        const auto cs = Corridors(w);
        // A corridor's accepted cell is one of its nodes (the others are direct neighbours of it).
        for (const auto& a : cs)
        {
            for (const auto& b : cs)
            {
                if (a.second.barrier != b.second.barrier || a.second.ordinal >= b.second.ordinal) { continue; }
                EXPECT_TRUE(LaterCorridorHasFarNode(w, a.second, b.second));
                for (int cb : b.second.cells) { for (int ca : a.second.cells) { EXPECT_GE(Dist(w, ca, cb), 4); } }
            }
        }
    }
}

// ---------------------------------------------------------------- generated worlds (AC8a)

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

    bool IndLand(const Cell& c)
    {
        return c.landmass != 0 && c.waterSource != WaterSource::Ocean && c.waterSource != WaterSource::Lake
            && c.terrain != TerrainType::Peak && c.terrain != TerrainType::Volcano;
    }

    bool IndOnBoundary(const WorldStructure& w, int idx, int barrier)
    {
        const DepthZone a = barrier == 1 ? DepthZone::FirstShores : DepthZone::WildReaches;
        const DepthZone b = barrier == 1 ? DepthZone::WildReaches : DepthZone::AncientInteriors;
        const DepthZone z = w.Cells()[idx].depthZone;
        if (z != a && z != b) { return false; }
        const DepthZone other = z == a ? b : a;
        for (int d = 0; d < 6; ++d)
        {
            const int nb = ChkNeighbour(w, idx, d);
            if (nb >= 0 && w.Cells()[nb].depthZone == other) { return true; }
        }
        return false;
    }

    // 0 pass, 1 river, 2 coastal gap, -1 not a candidate (independent of production).
    int IndSource(const WorldStructure& w, int idx, int barrier)
    {
        const Cell& c = w.Cells()[idx];
        if (!IndLand(c) || !IndOnBoundary(w, idx, barrier)) { return -1; }
        if (c.terrainFlags & TerrainFlag::Pass) { return 0; }
        if (c.terrainFlags & TerrainFlag::RiverCrossesBarrier) { return 1; }
        bool coastNear = false;
        for (int j = 0; j < w.CellCount(); ++j)
        {
            const int d = Dist(w, idx, j);
            if (d <= 2 && (w.Cells()[j].terrainFlags & (TerrainFlag::Ridge | TerrainFlag::Barrier))) { return -1; }
            if (d <= 4 && (w.Cells()[j].terrainFlags & TerrainFlag::Coast)) { coastNear = true; }
        }
        return coastNear ? 2 : -1;
    }

    struct SweepStats { int worlds = 0; int limited[2] = {}; int count[2][5] = {}; };

    void CheckGenerated(const WorldStructure& world, unsigned streamSeed, SweepStats& st)
    {
        WorldStructure w = world;
        w.sites.Clear();
        RandomStream rng(streamSeed);
        const CorridorResult r = PlaceCorridorNodes(w, Rules(), rng);
        ++st.worlds;

        for (int b = 0; b < 2; ++b)
        {
            int indCount[3] = {};
            for (int i = 0; i < w.CellCount(); ++i)
            {
                const int s = IndSource(w, i, b + 1);
                if (s >= 0) { ++indCount[s]; }
            }
            int total = 0;
            for (int t = 0; t < 3; ++t)
            {
                EXPECT_EQ(r.candidatesByType[b][t], indCount[t]) << "barrier " << b + 1 << " type " << t;
                total += indCount[t];
            }
            EXPECT_EQ(r.candidates[b], total);
            EXPECT_EQ(r.corridorLimited[b], r.accepted[b] < 2);
            EXPECT_LE(r.accepted[b], 4);
            EXPECT_EQ(CountCorridors(w, b + 1), r.accepted[b]);
            if (total == 0) { EXPECT_EQ(r.accepted[b], 0); }
            if (r.accepted[b] < 2) { ++st.limited[b]; }
            ++st.count[b][std::min<int>(r.accepted[b], 4)];
        }

        const auto cs = Corridors(w);
        for (const auto& kv : cs)
        {
            const Corridor& c = kv.second;
            ASSERT_GE(c.cells.size(), 1u);
            ASSERT_LE(c.cells.size(), 3u);
            const int src = CorridorSource(c.sites[0].flags);
            if (src == 0) { EXPECT_EQ(c.cells.size(), 1u); }
            for (int cell : c.cells)
            {
                EXPECT_EQ(IndSource(w, cell, c.barrier), src) << "cell " << cell;
            }
        }
        for (const auto& a : cs)
        {
            for (const auto& b : cs)
            {
                if (a.second.barrier != b.second.barrier || a.second.ordinal >= b.second.ordinal) { continue; }
                EXPECT_TRUE(LaterCorridorHasFarNode(w, a.second, b.second));
            }
        }
        ExpectDecodesConsistently(w);
    }

    void RunGenerated(const char* preset, uint64_t first, int count, SweepStats& st)
    {
        const WorldShapeParams shape = PresetParams(preset);
        TerrainRunner runner;
        for (int k = 0; k < count; ++k)
        {
            const uint64_t seed = first + static_cast<uint64_t>(k);
            SCOPED_TRACE(std::string(preset) + " seed " + std::to_string(seed));
            runner.Run(seed, shape, kSmall);
            CheckGenerated(runner.World(), static_cast<unsigned>(seed), st);
        }
    }

    void Report(const char* preset, const SweepStats& st)
    {
        std::printf("[NodesCorridor] %-12s worlds=%d limited(b1)=%d limited(b2)=%d  b1 counts 0..4={%d,%d,%d,%d,%d}  b2={%d,%d,%d,%d,%d}\n",
                    preset, st.worlds, st.limited[0], st.limited[1],
                    st.count[0][0], st.count[0][1], st.count[0][2], st.count[0][3], st.count[0][4],
                    st.count[1][0], st.count[1][1], st.count[1][2], st.count[1][3], st.count[1][4]);
    }
}

TEST(NodesCorridor, GeneratedWorldsDefaultPresetSatisfyAc8a)
{
    SweepStats st;
    RunGenerated("continental", 1, 3, st);
}

TEST(NodesCorridor, GeneratedWorldWithoutPassesOrRiversStillPlaces)
{
    WorldShapeParams shape = PresetParams("continental");
    shape.passes = 0;
    shape.rivers = 0.0f;
    TerrainRunner runner;
    runner.Run(11, shape, kSmall);
    SweepStats st;
    CheckGenerated(runner.World(), 11, st);
    EXPECT_EQ(st.worlds, 1);
}

TEST(SLOW_NodesCorridor, TwentySeedsAllPresets)
{
    for (const char* id : { "continental", "archipelago", "highlands" })
    {
        SweepStats st;
        RunGenerated(id, 100, 20, st);
        Report(id, st);
    }
}

// AC8b: no node on Peak or Volcano cells. Corridor candidates and widened cells must skip them.
TEST(NodesCorridor, NeverPlacesOnPeakOrVolcanoCells)
{
    for (unsigned seed = 1; seed <= 12; ++seed)
    {
        WorldStructure w = Zoned();
        for (int x = 4; x <= 24; ++x)
        {
            MarkRiverCut(w, x, 4);
            MarkRiverCut(w, x, 5);
            if (x % 2 == 0) { w.At(x, 4).terrain = TerrainType::Volcano; w.At(x, 5).terrain = TerrainType::Peak; }
        }
        MarkPass(w, 40, 5);
        w.At(40, 5).terrain = TerrainType::Volcano;
        Place(w, seed);
        for (const Site& s : w.sites.All())
        {
            const TerrainType t = w.Cells()[s.cell].terrain;
            EXPECT_NE(t, TerrainType::Volcano) << "seed " << seed << " cell " << s.cell;
            EXPECT_NE(t, TerrainType::Peak) << "seed " << seed << " cell " << s.cell;
        }
    }
}

// kNodeSpacing applies between corridor nodes of different barriers too (spec: all node placers share it).
TEST(NodesCorridor, CrossBarrierCorridorNodesRespectNodeSpacing)
{
    // Barriers on rows 4/5 and 6/7 are only 2 rows apart, so unspaced passes would collide.
    for (unsigned seed = 1; seed <= 20; ++seed)
    {
        WorldStructure w = MakeZonedBarrierWorld(40, 12, 5, 7);
        for (int x = 2; x < 38; ++x) { MarkPass(w, x, 5); MarkPass(w, x, 6); }
        Place(w, seed);
        const auto sites = w.sites.All();
        for (size_t i = 0; i < sites.size(); ++i)
        {
            for (size_t j = i + 1; j < sites.size(); ++j)
            {
                if (CorridorBarrier(sites[i].flags) == CorridorBarrier(sites[j].flags)) { continue; }
                const int d = Dist(w, static_cast<int>(sites[i].cell), static_cast<int>(sites[j].cell));
                EXPECT_GE(d, 3) << "seed " << seed << " cells " << sites[i].cell << "," << sites[j].cell;
            }
        }
    }
}
