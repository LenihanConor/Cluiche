// S-1 Terrain Generator task 9: Stage 6 Rivers (riverMask) and Stage 7 Lakes (lakeMask).
// Hand-built grids check the keep rules exactly through ComputeStage6/ComputeStage7; generator runs
// check AC10 (rivers) and AC11 (lakes) on real terrain. Wide sweeps live in SLOW_ suites.
#include <gtest/gtest.h>
#include <Shared/WorldGeneration/TerrainGenerator.h>

#include <DiaPathfinding/HexTopology.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>

using namespace CoW::WorldGeneration;
using Dia::Maths::RandomStream;
using Dia::Pathfinding::HexTopology;
using Dia::Pathfinding::kNoCell;

namespace {

const GridDimensions kSmall{ 66, 42 };
const int kN = 66 * 42;

const float* F(const TerrainGenerator& g, const char* name) { return g.GetLayers().GetFloat(g.GetLayers().FindLayer(name)); }
const uint32_t* U(const TerrainGenerator& g, const char* name) { return g.GetLayers().GetUInt(g.GetLayers().FindLayer(name)); }

struct Gen
{
    TerrainGenerator gen;
    RandomStream rng{ 1 };
    Gen() : gen(TerrainRules::Defaults()) {}
    void Go(uint64_t seed, const WorldShapeParams& s, GridDimensions d = kSmall) { gen.Update(seed, s, d, rng); }
};

std::set<int> Ran(const TerrainGenerator& g)
{
    std::set<int> r;
    for (int s = 0; s < static_cast<int>(TerrainStage::Count); ++s)
    {
        if (g.GetTimings()[s].ranLastUpdate) { r.insert(s); }
    }
    return r;
}

std::vector<uint8_t> OceanOf(const TerrainGenerator& g, int n)
{
    std::vector<uint8_t> o(static_cast<size_t>(n), 0);
    const float* e = F(g, "elevationRaw");
    for (int i = 0; i < n; ++i) { o[i] = e[i] <= 0.0f ? 1 : 0; }
    return o;
}

// ---------------------------------------------------------------------------
// Hand-built stage 6 grid: explicit flow paths, filled elevation and accumulation.
// Row 0 is ocean; chains run up a column toward larger y.
// ---------------------------------------------------------------------------
struct RGrid
{
    GridDimensions        dims;
    HexTopology           topo;
    std::vector<float>    filled, acc;
    std::vector<uint32_t> dir, mask;
    std::vector<uint8_t>  ocean;
    Stage6Debug           dbg;

    RGrid(int w, int h) : dims{ w, h }, topo(w, h), filled(static_cast<size_t>(w * h), 0.5f),
        acc(static_cast<size_t>(w * h), 0.0f), dir(static_cast<size_t>(w * h), kFlowSink),
        mask(static_cast<size_t>(w * h), 7u), ocean(static_cast<size_t>(w * h), 0)
    {
        for (int x = 0; x < w; ++x) { ocean[I(x, 0)] = 1; filled[I(x, 0)] = 0.0f; }
    }

    int I(int x, int y) const { return y * dims.width + x; }
    uint32_t DirTo(int a, int b) const
    {
        for (uint32_t d = 0; d < 6; ++d)
        {
            if (topo.NeighbourAt(static_cast<uint32_t>(a), d) == static_cast<uint32_t>(b)) { return d; }
        }
        ADD_FAILURE() << "cells " << a << " and " << b << " are not adjacent";
        return kFlowSink;
    }
    // path: source first, last cell drains into `mouth`. Elevation falls 0.01 per step from sourceElev;
    // accumulation rises by 1 per step downstream starting at accSource.
    void Path(const std::vector<int>& path, int mouth, float sourceElev, float accSource)
    {
        for (size_t k = 0; k < path.size(); ++k)
        {
            const int next = (k + 1 < path.size()) ? path[k + 1] : mouth;
            dir[path[k]] = DirTo(path[k], next);
            filled[path[k]] = sourceElev - 0.01f * static_cast<float>(k);
            acc[path[k]] = accSource + static_cast<float>(k);
        }
    }
    // Straight column going up from (x,1): returns cells source (top) first.
    std::vector<int> Column(int x, int length) const
    {
        std::vector<int> p;
        for (int y = length; y >= 1; --y) { p.push_back(I(x, y)); }
        return p;
    }
    void Run(float rivers, const TerrainRules& rules = TerrainRules::Defaults())
    {
        ComputeStage6(rules, dims, rivers, filled.data(), dir.data(), acc.data(), ocean.data(), mask.data(), dbg);
    }
    int Count() const { return static_cast<int>(std::count(mask.begin(), mask.end(), 1u)); }
};

// ---------------------------------------------------------------------------
// Real-generator river checks (AC10)
// ---------------------------------------------------------------------------
int CheckRivers(const TerrainGenerator& g, GridDimensions dims, const std::string& ctx)
{
    const int n = dims.width * dims.height;
    const HexTopology topo(dims.width, dims.height);
    const TerrainRules rules = TerrainRules::Defaults();
    const uint32_t* river = U(g, "riverMask");
    const uint32_t* lake = U(g, "lakeMask");
    const uint32_t* dir = U(g, "flowDirection");
    const float* filled = F(g, "filledElevation");
    const std::vector<uint8_t> ocean = OceanOf(g, n);
    int fails = 0;
    auto fail = [&](const std::string& what, int i) { if (fails < 10) { ADD_FAILURE() << ctx << ": " << what << " at cell " << i; } ++fails; };

    auto down = [&](int c) -> int
    {
        if (dir[c] >= 6u) { return -1; }
        const uint32_t nb = topo.NeighbourAt(static_cast<uint32_t>(c), dir[c]);
        return nb == kNoCell ? -1 : static_cast<int>(nb);
    };

    std::vector<int> inflow(static_cast<size_t>(n), 0), mouth(static_cast<size_t>(n), -1);
    for (int i = 0; i < n; ++i)
    {
        if (river[i] > 1u) { fail("riverMask not 0/1", i); }
        if (river[i] == 0u) { continue; }
        if (ocean[i] != 0) { fail("river on ocean", i); continue; }
        // Reaches the sea through river and lake cells only.
        int c = i;
        bool reached = false;
        for (int steps = 0; steps <= n; ++steps)
        {
            if (ocean[c] != 0) { reached = true; mouth[i] = c; break; }
            if (river[c] == 0u && lake[c] == 0u) { fail("river path leaves river/lake cells", i); break; }
            c = down(c);
            if (c < 0) { break; }
        }
        if (!reached && mouth[i] < 0) { fail("river never reaches the sea", i); }
        const int d = down(i);
        if (d >= 0 && river[d] != 0u) { ++inflow[d]; }
    }

    // Independent tree check: group river cells by mouth, test source elevation and longest run.
    std::vector<int> order;
    for (int i = 0; i < n; ++i) { if (river[i] != 0u) { order.push_back(i); } }
    std::sort(order.begin(), order.end(), [&](int a, int b) { return filled[a] != filled[b] ? filled[a] > filled[b] : a < b; });
    std::vector<int> run(static_cast<size_t>(n), 1);
    std::map<int, std::pair<float, int>> trees;   // mouth -> (highest source, longest run)
    for (int c : order)
    {
        if (mouth[c] < 0) { continue; }
        auto it = trees.emplace(mouth[c], std::make_pair(-1.0f, 0)).first;
        if (inflow[c] == 0) { it->second.first = std::max(it->second.first, filled[c]); }
        it->second.second = std::max(it->second.second, run[c]);
        const int d = down(c);
        if (d >= 0 && river[d] != 0u) { run[d] = std::max(run[d], run[c] + 1); }
    }
    for (const auto& kv : trees)
    {
        if (!(kv.second.first >= rules.kSourceMinElev())) { fail("kept tree highest source below kSourceMinElev", kv.first); }
        if (kv.second.second < static_cast<int>(rules.kMinRiverLength())) { fail("kept tree shorter than kMinRiverLength", kv.first); }
    }
    return fails;
}

int CountRivers(const TerrainGenerator& g)
{
    const uint32_t* r = U(g, "riverMask");
    int c = 0;
    for (int i = 0; i < kN; ++i) { c += r[i] != 0u ? 1 : 0; }
    return c;
}

// ---------------------------------------------------------------------------
// Lake checks (AC11)
// ---------------------------------------------------------------------------
int CheckLakes(const TerrainGenerator& g, GridDimensions dims, const std::string& ctx)
{
    const int n = dims.width * dims.height;
    const uint32_t* lake = U(g, "lakeMask");
    const uint32_t* rank = U(g, "basinRank");
    const std::vector<uint8_t> ocean = OceanOf(g, n);
    const Stage5Debug& d5 = g.GetStage5Debug();
    const Stage7Debug& d7 = g.GetStage7Debug();
    int fails = 0;
    auto fail = [&](const std::string& what, int i) { if (fails < 10) { ADD_FAILURE() << ctx << ": " << what << " at " << i; } ++fails; };

    const int expect = std::min(static_cast<int>(g.GetEffective().lakes), d5.eligibleBasins);
    if (static_cast<int>(d7.lakes.size()) != expect) { fail("lake count != effective.lakes", static_cast<int>(d7.lakes.size())); }
    if (g.GetEffective().lakes > d5.eligibleBasins) { fail("effective.lakes above eligible", g.GetEffective().lakes); }

    std::vector<int> expectedCell(static_cast<size_t>(n), 0);
    for (size_t k = 0; k < d7.lakes.size(); ++k)
    {
        const Lake& l = d7.lakes[k];
        if (l.basinIndex < 0 || l.basinIndex >= static_cast<int>(d5.basins.size())) { fail("basin index", static_cast<int>(k)); continue; }
        if (d5.ranked[k] != l.basinIndex) { fail("lake k is not the k-th ranked basin", static_cast<int>(k)); }
        const Basin& b = d5.basins[l.basinIndex];
        if (!b.eligible) { fail("lake on an ineligible basin", b.firstCell); }
        if (l.cells != b.cells) { fail("lake cells != basin cells", b.firstCell); }
        if (l.surfaceHeight != b.spillHeight) { fail("surface != spill height", b.firstCell); }
        if (l.spillCell != b.spillCell) { fail("spill cell", b.firstCell); }
        for (int c : l.cells)
        {
            expectedCell[c] = static_cast<int>(k + 1);
            if (rank[c] != static_cast<uint32_t>(k + 1)) { fail("lake id != basin rank", c); }
        }
        if (l.outflowPath.empty() || l.outflowPath.front() != l.spillCell) { fail("outflow does not start at spill cell", b.firstCell); }
        else if (ocean[l.outflowPath.back()] == 0) { fail("outflow path does not reach the ocean", b.firstCell); }
    }
    for (int i = 0; i < n; ++i)
    {
        if (static_cast<int>(lake[i]) != expectedCell[i]) { fail("lakeMask != lake cells", i); }
    }
    return fails;
}

} // namespace

// ---------------------------------------------------------------------------
// Layers and wiring
// ---------------------------------------------------------------------------
TEST(TerrainRivers, LayersAreRegistered)
{
    TerrainLayers layers;
    const int r = layers.FindLayer("riverMask");
    const int l = layers.FindLayer("lakeMask");
    ASSERT_GE(r, 0);
    ASSERT_GE(l, 0);
    EXPECT_EQ(TerrainLayers::GetLayerInfo(r).type, LayerType::UInt);
    EXPECT_EQ(TerrainLayers::GetLayerInfo(r).stage, TerrainStage::Rivers);
    EXPECT_EQ(TerrainLayers::GetLayerInfo(l).type, LayerType::UInt);
    EXPECT_EQ(TerrainLayers::GetLayerInfo(l).stage, TerrainStage::Lakes);
}

// ---------------------------------------------------------------------------
// Hand-built grids: stage 6 keep rules
// ---------------------------------------------------------------------------
TEST(TerrainRivers, RiversZeroGivesEmptyMaskEvenWithHugeAccumulation)
{
    RGrid g(9, 12);
    g.Path(g.Column(3, 8), g.I(3, 0), 0.9f, 1000.0f);
    g.Run(0.0f);
    EXPECT_EQ(g.Count(), 0);
    EXPECT_EQ(g.dbg.candidates, 0);
}

TEST(TerrainRivers, ShortTreeIsDroppedAndFourCellTreeIsKept)
{
    RGrid g(9, 12);
    g.Path(g.Column(3, 3), g.I(3, 0), 0.9f, 100.0f);
    g.Run(0.5f);
    EXPECT_EQ(g.Count(), 0) << "3-cell run is below kMinRiverLength";
    ASSERT_EQ(g.dbg.trees.size(), 1u);
    EXPECT_EQ(g.dbg.trees[0].longestRun, 3);
    EXPECT_FALSE(g.dbg.trees[0].kept);

    RGrid k(9, 12);
    k.Path(k.Column(3, 4), k.I(3, 0), 0.9f, 100.0f);
    k.Run(0.5f);
    EXPECT_EQ(k.Count(), 4);
    for (int y = 1; y <= 4; ++y) { EXPECT_EQ(k.mask[k.I(3, y)], 1u) << y; }
    ASSERT_EQ(k.dbg.trees.size(), 1u);
    EXPECT_TRUE(k.dbg.trees[0].kept);
    EXPECT_EQ(k.dbg.trees[0].mouth, k.I(3, 0));
    EXPECT_EQ(k.dbg.trees[0].longestRun, 4);
}

TEST(TerrainRivers, LowSourceTreeIsDroppedAndSourceAtMinimumIsKept)
{
    RGrid low(9, 12);
    low.Path(low.Column(3, 8), low.I(3, 0), 0.29f, 100.0f);
    low.Run(0.5f);
    EXPECT_EQ(low.Count(), 0);

    RGrid edge(9, 12);
    edge.Path(edge.Column(3, 8), edge.I(3, 0), TerrainRules::Defaults().kSourceMinElev(), 100.0f);
    edge.Run(0.5f);
    EXPECT_EQ(edge.Count(), 8) << "source == kSourceMinElev is kept (>=)";

    RGrid tall(9, 12);
    tall.Path(tall.Column(3, 8), tall.I(3, 0), 0.8f, 100.0f);
    tall.Run(0.5f);
    EXPECT_EQ(tall.Count(), 8);
}

TEST(TerrainRivers, AccumulationEqualToThresholdIsACandidate)
{
    // Rivers == 1 gives T = kFlowMin = 12 exactly.
    RGrid g(9, 12);
    g.Path(g.Column(3, 4), g.I(3, 0), 0.9f, 12.0f);
    g.Run(1.0f);
    EXPECT_FLOAT_EQ(g.dbg.threshold, 12.0f);
    EXPECT_EQ(g.Count(), 4) << "acc == T is a candidate";

    RGrid under(9, 12);
    under.Path(under.Column(3, 4), under.I(3, 0), 0.9f, 11.99f);
    under.Run(1.0f);
    EXPECT_EQ(under.Count(), 0) << "source cell under T leaves a 3-cell run";
}

TEST(TerrainRivers, ThresholdFollowsTheFormula)
{
    const TerrainRules rules = TerrainRules::Defaults();
    RGrid g(9, 12);
    g.Path(g.Column(3, 6), g.I(3, 0), 0.9f, 500.0f);
    for (float r : { 0.1f, 0.25f, 0.5f, 0.75f, 1.0f })
    {
        g.Run(r);
        const double expect = 400.0 * std::pow(12.0 / 400.0, static_cast<double>(r));
        EXPECT_NEAR(g.dbg.threshold, expect, 1e-3) << r;
        (void)rules;
    }
}

TEST(TerrainRivers, CandidatesGroupByMouthCellAndTreeIsKeptWhole)
{
    // Two branches reach the same ocean cell M: A is long with a low source, B is short with a high
    // source. The tree takes its highest source from B and its longest run from A, so all of it is kept.
    RGrid g(9, 12);
    const int m = g.I(5, 0);   // odd column: three land neighbours in row 1
    std::vector<int> landNbs;
    for (uint32_t d = 0; d < 6; ++d)
    {
        const uint32_t nb = g.topo.NeighbourAt(static_cast<uint32_t>(m), d);
        if (nb != kNoCell && g.ocean[nb] == 0) { landNbs.push_back(static_cast<int>(nb)); }
    }
    ASSERT_GE(landNbs.size(), 2u);
    const int a1 = landNbs[0];
    const int b1 = landNbs[1];
    std::vector<int> pathA, pathB;
    for (int y = 6; y >= 1; --y) { pathA.push_back(g.I(a1 % 9, y)); }
    ASSERT_EQ(pathA.back(), a1);
    pathB = { g.I(b1 % 9, 2), b1 };
    g.Path(pathA, m, 0.2f, 100.0f);
    g.Path(pathB, m, 0.9f, 100.0f);
    g.Run(0.5f);
    ASSERT_EQ(g.dbg.trees.size(), 1u) << "both branches share one mouth";
    EXPECT_EQ(g.dbg.trees[0].cellCount, 8);
    EXPECT_FLOAT_EQ(g.dbg.trees[0].highestSource, 0.9f);
    EXPECT_EQ(g.dbg.trees[0].longestRun, 6);
    EXPECT_TRUE(g.dbg.trees[0].kept);
    EXPECT_EQ(g.Count(), 8);
}

TEST(TerrainRivers, TreesAtDifferentMouthsAreEvaluatedSeparately)
{
    RGrid g(11, 12);
    g.Path(g.Column(2, 8), g.I(2, 0), 0.1f, 100.0f);    // low source: dropped
    g.Path(g.Column(6, 8), g.I(6, 0), 0.8f, 100.0f);    // kept
    g.Path(g.Column(9, 2), g.I(9, 0), 0.8f, 100.0f);    // too short: dropped
    g.Run(0.5f);
    ASSERT_EQ(g.dbg.trees.size(), 3u);
    EXPECT_EQ(g.Count(), 8);
    for (int y = 1; y <= 8; ++y)
    {
        EXPECT_EQ(g.mask[g.I(2, y)], 0u);
        EXPECT_EQ(g.mask[g.I(6, y)], 1u);
    }
    EXPECT_EQ(g.mask[g.I(9, 1)], 0u);
    EXPECT_EQ(g.mask[g.I(9, 2)], 0u);
}

TEST(TerrainRivers, LandThatNoOceanReachesIsIgnored)
{
    RGrid g(9, 12);
    g.Path(g.Column(3, 8), g.I(3, 0), 0.9f, 100.0f);
    g.dir[g.I(3, 1)] = kFlowSink;   // last cell is a land sink: nothing reaches the sea
    g.Run(0.5f);
    EXPECT_EQ(g.Count(), 0);
    EXPECT_TRUE(g.dbg.trees.empty());
}

TEST(TerrainRivers, NullInputsWriteAnEmptyMask)
{
    RGrid g(9, 12);
    Stage6Debug d;
    ComputeStage6(TerrainRules::Defaults(), g.dims, 0.5f, nullptr, g.dir.data(), g.acc.data(), g.ocean.data(), g.mask.data(), d);
    EXPECT_EQ(g.Count(), 0);
}

// ---------------------------------------------------------------------------
// Hand-built stage 7
// ---------------------------------------------------------------------------
TEST(TerrainRivers, StageSevenFillsRankedBasinsAndIgnoresIneligible)
{
    const GridDimensions dims{ 6, 4 };
    const int n = 24;
    std::vector<uint8_t> ocean(n, 0);
    std::vector<uint32_t> dir(n, kFlowSink), lake(n, 9u);
    for (int x = 0; x < 6; ++x) { ocean[x] = 1; }
    HexTopology topo(6, 4);
    // Everything drains straight down column by column toward row 0.
    for (int i = 6; i < n; ++i)
    {
        for (uint32_t d = 0; d < 6; ++d)
        {
            const uint32_t nb = topo.NeighbourAt(static_cast<uint32_t>(i), d);
            if (nb != kNoCell && static_cast<int>(nb) == i - 6) { dir[i] = d; }
        }
    }
    Stage5Debug d5;
    for (int b = 0; b < 3; ++b)
    {
        Basin basin;
        basin.cells = { 12 + b * 2, 13 + b * 2 };
        basin.firstCell = basin.cells.front();
        basin.spillHeight = 0.4f + 0.1f * static_cast<float>(b);
        basin.spillCell = 6 + b;
        basin.eligible = b != 1;
        d5.basins.push_back(basin);
    }
    d5.ranked = { 2, 0 };
    d5.eligibleBasins = 2;

    Stage7Debug d7;
    EXPECT_EQ(ComputeStage7(dims, 1, d5, dir.data(), ocean.data(), lake.data(), d7), 1);
    ASSERT_EQ(d7.lakes.size(), 1u);
    EXPECT_EQ(d7.lakes[0].basinIndex, 2);
    EXPECT_FLOAT_EQ(d7.lakes[0].surfaceHeight, 0.6f);
    EXPECT_EQ(lake[16], 1u);
    EXPECT_EQ(lake[17], 1u);
    EXPECT_EQ(lake[12], 0u);
    EXPECT_EQ(lake[0], 0u);
    ASSERT_FALSE(d7.lakes[0].outflowPath.empty());
    EXPECT_EQ(d7.lakes[0].outflowPath.front(), 8);
    EXPECT_EQ(d7.lakes[0].outflowPath.back(), 2);

    EXPECT_EQ(ComputeStage7(dims, 12, d5, dir.data(), ocean.data(), lake.data(), d7), 2);
    EXPECT_EQ(lake[16], 1u);
    EXPECT_EQ(lake[12], 2u);
    EXPECT_EQ(lake[14], 0u) << "ineligible basin stays dry";

    EXPECT_EQ(ComputeStage7(dims, 0, d5, dir.data(), ocean.data(), lake.data(), d7), 0);
    EXPECT_TRUE(d7.lakes.empty());
    EXPECT_EQ(std::count(lake.begin(), lake.end(), 0u), n);
}

// ---------------------------------------------------------------------------
// Generator integration: rivers (AC10)
// ---------------------------------------------------------------------------
TEST(TerrainRivers, GeneratorInvariantsOnFourSeeds)
{
    for (uint64_t seed : { 1u, 7u, 42u, 100u })
    {
        Gen r;
        r.Go(seed, WorldShapeParams{});
        ASSERT_NE(U(r.gen, "riverMask"), nullptr);
        ASSERT_NE(U(r.gen, "lakeMask"), nullptr);
        EXPECT_EQ(0, CheckRivers(r.gen, kSmall, "seed " + std::to_string(seed)));
        EXPECT_EQ(0, CheckLakes(r.gen, kSmall, "seed " + std::to_string(seed)));
    }
}

TEST(TerrainRivers, RiversZeroHasNoRiverCells)
{
    Gen r;
    WorldShapeParams s;
    s.rivers = 0.0f;
    r.Go(7, s);
    EXPECT_EQ(CountRivers(r.gen), 0);
}

TEST(TerrainRivers, RiverSetsNestAsRiversRises)
{
    for (uint64_t seed : { 1u, 42u })
    {
        Gen r;
        WorldShapeParams s;
        std::vector<uint32_t> prev;
        for (int step = 0; step <= 10; ++step)
        {
            s.rivers = static_cast<float>(step) * 0.1f;
            r.Go(seed, s);
            const std::vector<uint32_t> cur(U(r.gen, "riverMask"), U(r.gen, "riverMask") + kN);
            if (!prev.empty())
            {
                for (int i = 0; i < kN; ++i)
                {
                    ASSERT_FALSE(prev[i] != 0u && cur[i] == 0u) << "seed " << seed << " rivers " << s.rivers << " drops cell " << i;
                }
            }
            EXPECT_EQ(0, CheckRivers(r.gen, kSmall, "seed " + std::to_string(seed) + " rivers " + std::to_string(s.rivers)));
            prev = cur;
        }
    }
}

TEST(TerrainRivers, DefaultParamsProduceRiversOnTheFourSeeds)
{
    for (uint64_t seed : { 1u, 7u, 42u, 100u })
    {
        Gen r;
        r.Go(seed, WorldShapeParams{});
        EXPECT_GT(CountRivers(r.gen), 0) << "seed " << seed;
    }
}

// ---------------------------------------------------------------------------
// Generator integration: lakes (AC11)
// ---------------------------------------------------------------------------
TEST(TerrainRivers, LakeCountIsMinOfSliderAndEligibleAndPrefixesAreStable)
{
    for (uint64_t seed : { 1u, 42u })
    {
        Gen r;
        WorldShapeParams s;
        int prevCount = -1;
        std::vector<std::vector<int>> prevCells;
        for (int lakes = 0; lakes <= 12; ++lakes)
        {
            s.lakes = static_cast<uint8_t>(lakes);
            r.Go(seed, s);
            const std::string ctx = "seed " + std::to_string(seed) + " lakes " + std::to_string(lakes);
            EXPECT_EQ(0, CheckLakes(r.gen, kSmall, ctx));
            const int expect = std::min(lakes, r.gen.GetEffective().eligibleBasins);
            EXPECT_EQ(r.gen.GetEffective().lakes, expect) << ctx;
            EXPECT_GE(r.gen.GetEffective().lakes, prevCount) << ctx;
            const auto& ls = r.gen.GetStage7Debug().lakes;
            ASSERT_EQ(static_cast<int>(ls.size()), expect) << ctx;
            for (size_t k = 0; k < prevCells.size(); ++k)
            {
                EXPECT_EQ(ls[k].cells, prevCells[k]) << ctx << " lake " << k;
            }
            prevCells.clear();
            for (const Lake& l : ls) { prevCells.push_back(l.cells); }
            prevCount = r.gen.GetEffective().lakes;
        }
    }
}

TEST(TerrainRivers, ZeroLakesGivesEmptyLakeMask)
{
    Gen r;
    WorldShapeParams s;
    s.lakes = 0;
    r.Go(7, s);
    const uint32_t* lake = U(r.gen, "lakeMask");
    EXPECT_EQ(std::count(lake, lake + kN, 0u), kN);
    EXPECT_EQ(r.gen.GetEffective().lakes, 0);
}

// ---------------------------------------------------------------------------
// Determinism and dirty bits
// ---------------------------------------------------------------------------
TEST(TerrainRivers, RunsAreByteIdentical)
{
    Gen a, b;
    const WorldShapeParams s;
    a.Go(11, s);
    b.Go(11, s);
    EXPECT_EQ(0, std::memcmp(U(a.gen, "riverMask"), U(b.gen, "riverMask"), kN * sizeof(uint32_t)));
    EXPECT_EQ(0, std::memcmp(U(a.gen, "lakeMask"), U(b.gen, "lakeMask"), kN * sizeof(uint32_t)));
    const auto& la = a.gen.GetStage7Debug().lakes;
    const auto& lb = b.gen.GetStage7Debug().lakes;
    ASSERT_EQ(la.size(), lb.size());
    for (size_t i = 0; i < la.size(); ++i)
    {
        EXPECT_EQ(la[i].cells, lb[i].cells);
        EXPECT_EQ(la[i].outflowPath, lb[i].outflowPath);
    }
}

TEST(TerrainRivers, DirtyBitsRiversLakesAndRuggedness)
{
    Gen r;
    WorldShapeParams s;
    r.Go(5, s);
    EXPECT_EQ(Ran(r.gen).count(6), 1u);
    EXPECT_EQ(Ran(r.gen).count(7), 1u);

    s.rivers = 0.8f;
    r.Go(5, s);
    EXPECT_EQ(Ran(r.gen), (std::set<int>{ 6, 8, 9, 10 }));

    s.lakes = 9;
    r.Go(5, s);
    EXPECT_EQ(Ran(r.gen), (std::set<int>{ 7, 9, 10 }));

    s.ruggedness = 0.9f;
    r.Go(5, s);
    EXPECT_EQ(Ran(r.gen).count(5), 1u);
    EXPECT_EQ(Ran(r.gen).count(6), 1u);
    EXPECT_EQ(Ran(r.gen).count(7), 1u);
    EXPECT_EQ(0, CheckRivers(r.gen, kSmall, "after rugged"));
    EXPECT_EQ(0, CheckLakes(r.gen, kSmall, "after rugged"));
}

// ---------------------------------------------------------------------------
// Sweeps
// ---------------------------------------------------------------------------
TEST(SLOW_TerrainRivers, RiverExistenceRateAndInvariantsOver20Seeds)
{
    int withRiver = 0;
    for (uint64_t seed = 1; seed <= 20; ++seed)
    {
        Gen r;
        r.Go(seed, WorldShapeParams{});
        ASSERT_EQ(0, CheckRivers(r.gen, kSmall, "seed " + std::to_string(seed)));
        ASSERT_EQ(0, CheckLakes(r.gen, kSmall, "seed " + std::to_string(seed)));
        withRiver += CountRivers(r.gen) > 0 ? 1 : 0;
    }
    std::printf("[TerrainRivers] river existence at default params: %d / 20 seeds\n", withRiver);
    EXPECT_GE(withRiver, 19) << ">= 95% of seeds must have at least one river";
}

TEST(SLOW_TerrainRivers, RiverNestingAndLakePrefixSweep20Seeds)
{
    int nestingFailures = 0;
    for (uint64_t seed = 1; seed <= 20; ++seed)
    {
        Gen r;
        WorldShapeParams s;
        std::vector<uint32_t> prev;
        for (int step = 0; step <= 10; ++step)
        {
            s.rivers = static_cast<float>(step) * 0.1f;
            r.Go(seed, s);
            const std::vector<uint32_t> cur(U(r.gen, "riverMask"), U(r.gen, "riverMask") + kN);
            for (int i = 0; i < kN && !prev.empty(); ++i)
            {
                if (prev[i] != 0u && cur[i] == 0u) { ++nestingFailures; }
            }
            EXPECT_EQ(0, CheckRivers(r.gen, kSmall, "seed " + std::to_string(seed) + " rivers " + std::to_string(s.rivers)));
            prev = cur;
        }
        s.rivers = 0.5f;
        int prevCount = -1;
        std::vector<std::vector<int>> prevCells;
        for (int lakes = 0; lakes <= 12; ++lakes)
        {
            s.lakes = static_cast<uint8_t>(lakes);
            r.Go(seed, s);
            EXPECT_EQ(0, CheckLakes(r.gen, kSmall, "seed " + std::to_string(seed) + " lakes " + std::to_string(lakes)));
            const auto& ls = r.gen.GetStage7Debug().lakes;
            EXPECT_GE(static_cast<int>(ls.size()), prevCount);
            for (size_t k = 0; k < prevCells.size() && k < ls.size(); ++k) { EXPECT_EQ(ls[k].cells, prevCells[k]); }
            prevCells.clear();
            for (const Lake& l : ls) { prevCells.push_back(l.cells); }
            prevCount = static_cast<int>(ls.size());
        }
    }
    EXPECT_EQ(nestingFailures, 0);
}

// ---------------------------------------------------------------------------
// Timing (informational, Release only)
// ---------------------------------------------------------------------------
TEST(TerrainPerf, StageSixAndSevenRiversLakesReport)
{
#ifdef NDEBUG
    constexpr int kRuns = 50;
    std::vector<double> t6, t7;
    Gen r;
    const WorldShapeParams s;
    for (int run = 0; run < kRuns; ++run)
    {
        r.Go(3000 + static_cast<uint64_t>(run), s);
        t6.push_back(r.gen.GetTimings()[static_cast<int>(TerrainStage::Rivers)].milliseconds);
        t7.push_back(r.gen.GetTimings()[static_cast<int>(TerrainStage::Lakes)].milliseconds);
    }
    std::sort(t6.begin(), t6.end());
    std::sort(t7.begin(), t7.end());
    std::printf("[TerrainPerf] stage 6 Rivers median %.3f ms, max %.3f ms; stage 7 Lakes median %.3f ms, max %.3f ms (66x42, %d runs)\n",
                t6[t6.size() / 2], t6.back(), t7[t7.size() / 2], t7.back(), kRuns);
    SUCCEED();
#else
    GTEST_SKIP() << "Release-only timing report";
#endif
}
