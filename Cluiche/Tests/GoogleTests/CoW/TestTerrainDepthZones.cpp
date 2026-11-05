// S-1 Terrain Generator task 10: Stage 8 DepthZones (zoneCostBase, zoneCost, depthZone).
// Hand-built grids check costs, thresholds and zones exactly through ComputeStage8; generator runs
// check AC13 (zone ordering) and AC13b (river monotonicity) on real terrain. Wide sweeps are SLOW_.
#include <gtest/gtest.h>
#include <Shared/WorldGeneration/TerrainGenerator.h>

#include <DiaPathfinding/HexTopology.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
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

constexpr uint32_t kNone = static_cast<uint32_t>(DepthZone::None);
constexpr uint32_t kFirst = static_cast<uint32_t>(DepthZone::FirstShores);
constexpr uint32_t kWild = static_cast<uint32_t>(DepthZone::WildReaches);
constexpr uint32_t kAncient = static_cast<uint32_t>(DepthZone::AncientInteriors);

// ---------------------------------------------------------------------------
// Hand-built grid: ocean everywhere except the blocks the test paints.
// ---------------------------------------------------------------------------
struct DGrid
{
    GridDimensions        dims;
    HexTopology           topo;
    int                   n;
    std::vector<uint32_t> land, coast, river, zone;
    std::vector<float>    ridge, barrier, base, cost;
    Stage8Debug           dbg;
    TerrainRules          rules = TerrainRules::Defaults();

    DGrid(int w, int h) : dims{ w, h }, topo(w, h), n(w * h), land(static_cast<size_t>(n), 0u),
        coast(static_cast<size_t>(n), 0u), river(static_cast<size_t>(n), 0u), zone(static_cast<size_t>(n), 99u),
        ridge(static_cast<size_t>(n), 0.0f), barrier(static_cast<size_t>(n), 0.0f),
        base(static_cast<size_t>(n), -1.0f), cost(static_cast<size_t>(n), -1.0f) {}

    int I(int x, int y) const { return y * dims.width + x; }

    void Block(int x0, int y0, int x1, int y1, uint32_t id)
    {
        for (int y = y0; y <= y1; ++y) { for (int x = x0; x <= x1; ++x) { land[I(x, y)] = id; } }
    }

    // BFS distance from the nearest ocean cell (land cells get >= 1).
    void ComputeCoast()
    {
        std::vector<int> dist(static_cast<size_t>(n), -1);
        std::vector<int> q;
        for (int i = 0; i < n; ++i) { if (land[i] == 0u) { dist[i] = 0; q.push_back(i); } }
        for (size_t head = 0; head < q.size(); ++head)
        {
            const int c = q[head];
            for (uint32_t d = 0; d < 6; ++d)
            {
                const uint32_t nb = topo.NeighbourAt(static_cast<uint32_t>(c), d);
                if (nb != kNoCell && dist[nb] < 0) { dist[nb] = dist[c] + 1; q.push_back(static_cast<int>(nb)); }
            }
        }
        for (int i = 0; i < n; ++i) { coast[i] = dist[i] > 0 ? static_cast<uint32_t>(dist[i]) : 0u; }
    }

    void Run(float barrierSlider = 0.6f, int mainId = 1)
    {
        ComputeCoast();
        Stage8Layers out;
        out.zoneCostBase = base.data();
        out.zoneCost = cost.data();
        out.depthZone = zone.data();
        ComputeStage8(rules, dims, barrierSlider, mainId, land.data(), coast.data(), ridge.data(), barrier.data(),
                      river.data(), out, dbg);
    }
};

// Land with ocean everywhere else: the 3-row strip (rows 1..3, columns 1..8 of a 10x5 grid).
// Top/bottom rows and the two end cells are coast (cost 1, 18 cells); the six middle cells cost 2.
DGrid Strip3()
{
    DGrid g(10, 5);
    g.Block(1, 1, 8, 3, 1u);
    return g;
}

// ---------------------------------------------------------------------------
// Real-terrain checks
// ---------------------------------------------------------------------------
double EntryCost(const TerrainRules& r, float barrierSlider, const float* ridge, const float* barrier, const uint32_t* river, int c)
{
    const double terms = static_cast<double>(r.kRidgeCost()) * ridge[c] +
                         static_cast<double>(r.kBarrierCost()) * barrierSlider * barrier[c];
    return river[c] != 0u ? 1.0 + static_cast<double>(r.kRiverCorridor()) * terms : 1.0 + terms;
}

int CountDistinctAbove(const TerrainGenerator& g, int main, float t1)
{
    std::set<float> s;
    const uint32_t* lm = U(g, "landmass");
    const float* base = F(g, "zoneCostBase");
    for (int i = 0; i < kN; ++i) { if (lm[i] == static_cast<uint32_t>(main) && base[i] > t1) { s.insert(base[i]); } }
    return static_cast<int>(s.size());
}

// AC13 structural checks. Returns failure count; fills stats.
struct ZoneStats { int mainCells = 0; int ancient = 0; bool hasAncient = false; bool expectAncient = false; };

int CheckZones(const TerrainGenerator& g, const std::string& tag, ZoneStats* stats = nullptr)
{
    int fails = 0;
    auto fail = [&](const std::string& m) { ++fails; ADD_FAILURE() << tag << ": " << m; };
    const uint32_t* lm = U(g, "landmass");
    const uint32_t* coast = U(g, "coastDistance");
    const uint32_t* zone = U(g, "depthZone");
    const float* base = F(g, "zoneCostBase");
    const float* cost = F(g, "zoneCost");
    const Stage8Debug& d = g.GetStage8Debug();
    const int main = g.GetEffective().mainLandmass;

    std::set<uint32_t> ids;
    for (int i = 0; i < kN; ++i) { if (lm[i] != 0u) { ids.insert(lm[i]); } }
    for (const uint32_t id : ids)
    {
        float maxFirst = -1e30f, minWild = 1e30f, minAncient = 1e30f;
        for (int i = 0; i < kN; ++i)
        {
            if (lm[i] != id) { continue; }
            if (zone[i] == kFirst) { maxFirst = std::max(maxFirst, cost[i]); }
            else if (zone[i] == kWild) { minWild = std::min(minWild, cost[i]); }
            else if (zone[i] == kAncient) { minAncient = std::min(minAncient, cost[i]); }
            else { fail("land cell with zone " + std::to_string(zone[i])); }
        }
        // Exactly what holds: First <= t1 < Wild, Ancient >= t2 > t1. Wild may overlap Ancient's range
        // because shallow high-cost cells are demoted to Wild Reaches by the depth test.
        if (maxFirst > d.t1) { fail("First Shores cost above t1"); }
        if (minWild <= d.t1) { fail("Wild Reaches cost at or below t1"); }
        if (minAncient < 1e29f)
        {
            if (id != static_cast<uint32_t>(main)) { fail("Ancient Interiors off the main landmass"); }
            if (minAncient < d.t2 || !(d.t1 < d.t2)) { fail("Ancient below t2 or t1 >= t2"); }
        }
    }
    int ancient = 0;
    for (int i = 0; i < kN; ++i)
    {
        if (lm[i] == 0u)
        {
            if (zone[i] != kNone || cost[i] != 0.0f || base[i] != 0.0f) { fail("ocean cell not None/0"); }
            continue;
        }
        if (cost[i] > base[i]) { fail("zoneCost above zoneCostBase"); }
        if (cost[i] < 1.0f) { fail("land cost below 1"); }
        if (coast[i] == 1u && zone[i] != kFirst) { fail("coast cell not First Shores"); }
        if (zone[i] == kAncient)
        {
            ++ancient;
            if (coast[i] < 3u) { fail("Ancient cell with coastDistance < 3"); }
        }
    }
    if (d.hasAncient && !(d.t1 < d.t2)) { fail("hasAncient but t1 >= t2"); }
    if (stats != nullptr)
    {
        stats->mainCells = d.mainCells;
        stats->ancient = ancient;
        stats->hasAncient = d.hasAncient;
        stats->expectAncient = d.mainCells >= 40 && CountDistinctAbove(g, main, d.t1) > 0;
    }
    return fails;
}

// True when some optimal predecessor chain of cell c (at the given state) passes through a non-coast river cell.
bool ChainHitsRiver(const TerrainGenerator& g, const TerrainRules& r, float barrierSlider, int c)
{
    const HexTopology topo(kSmall.width, kSmall.height);
    const float* cost = F(g, "zoneCost");
    const uint32_t* river = U(g, "riverMask");
    const uint32_t* coast = U(g, "coastDistance");
    const uint32_t* lm = U(g, "landmass");
    std::vector<uint8_t> seen(static_cast<size_t>(kN), 0);
    std::vector<int> stack{ c };
    seen[c] = 1;
    while (!stack.empty())
    {
        const int x = stack.back();
        stack.pop_back();
        if (coast[x] == 1u) { continue; }   // source: its own entry cost never applies
        if (river[x] != 0u) { return true; }
        const double entry = EntryCost(r, barrierSlider, F(g, "ridgeMask"), F(g, "barrierMask"), river, x);
        for (uint32_t d = 0; d < 6; ++d)
        {
            const uint32_t nb = topo.NeighbourAt(static_cast<uint32_t>(x), d);
            if (nb == kNoCell || lm[nb] == 0u || seen[nb]) { continue; }
            if (std::fabs(static_cast<double>(cost[nb]) + entry - static_cast<double>(cost[x])) < 1e-3)
            {
                seen[nb] = 1;
                stack.push_back(static_cast<int>(nb));
            }
        }
    }
    return false;
}

int CheckRiverSweep(uint64_t seed)
{
    int fails = 0;
    Gen r;
    WorldShapeParams s;
    const TerrainRules rules = TerrainRules::Defaults();
    std::vector<float> prevCost, firstBase;
    std::vector<uint32_t> prevZone;
    float t1 = 0.0f, t2 = 0.0f;
    bool hasAnc = false;
    for (int step = 0; step <= 10; ++step)
    {
        s.rivers = static_cast<float>(step) * 0.1f;
        r.Go(seed, s);
        const std::string tag = "seed " + std::to_string(seed) + " rivers " + std::to_string(s.rivers);
        fails += CheckZones(r.gen, tag);
        const std::vector<float> cost(F(r.gen, "zoneCost"), F(r.gen, "zoneCost") + kN);
        const std::vector<float> base(F(r.gen, "zoneCostBase"), F(r.gen, "zoneCostBase") + kN);
        const std::vector<uint32_t> zone(U(r.gen, "depthZone"), U(r.gen, "depthZone") + kN);
        const Stage8Debug& d = r.gen.GetStage8Debug();
        if (step == 0)
        {
            firstBase = base;
            t1 = d.t1;
            t2 = d.t2;
            hasAnc = d.hasAncient;
        }
        else
        {
            if (base != firstBase || d.t1 != t1 || d.t2 != t2 || d.hasAncient != hasAnc)
            {
                ++fails;
                ADD_FAILURE() << tag << ": base or thresholds moved with Rivers";
            }
            for (int i = 0; i < kN; ++i)
            {
                if (cost[i] > prevCost[i]) { ++fails; ADD_FAILURE() << tag << ": zoneCost rose at " << i; }
                if (zone[i] > prevZone[i]) { ++fails; ADD_FAILURE() << tag << ": zone moved outward at " << i; }
                if (zone[i] != prevZone[i])
                {
                    if (!(cost[i] < prevCost[i])) { ++fails; ADD_FAILURE() << tag << ": zone changed without cost drop at " << i; }
                    else if (!ChainHitsRiver(r.gen, rules, s.barrier, i)) { ++fails; ADD_FAILURE() << tag << ": changed cell " << i << " has no river on its shortest path"; }
                }
            }
        }
        prevCost = cost;
        prevZone = zone;
    }
    return fails;
}

} // namespace

// ---------------------------------------------------------------------------
// Hand-built grids
// ---------------------------------------------------------------------------
TEST(TerrainDepthZones, StripCostsThresholdsAndDepthDemotion)
{
    DGrid g = Strip3();
    g.Run();
    int ones = 0, twos = 0;
    for (int i = 0; i < g.n; ++i)
    {
        if (g.land[i] == 0u) { continue; }
        if (g.base[i] == 1.0f) { ++ones; } else if (g.base[i] == 2.0f) { ++twos; }
    }
    EXPECT_EQ(ones, 18);
    EXPECT_EQ(twos, 6);
    // n = 24: t1 = value at index floor(0.45*24)-1 = 9 -> 1; t2 = value at index 24-ceil(3.6)=20 -> 2.
    EXPECT_FLOAT_EQ(g.dbg.t1, 1.0f);
    EXPECT_FLOAT_EQ(g.dbg.t2, 2.0f);
    EXPECT_TRUE(g.dbg.hasAncient);
    EXPECT_EQ(g.dbg.mainCells, 24);
    // The six cost-2 cells have coastDistance 2 (< kAncientMinDepth 3): demoted to Wild Reaches.
    int first = 0, wild = 0, ancient = 0;
    for (int i = 0; i < g.n; ++i)
    {
        if (g.land[i] == 0u) { continue; }
        first += g.zone[i] == kFirst ? 1 : 0;
        wild += g.zone[i] == kWild ? 1 : 0;
        ancient += g.zone[i] == kAncient ? 1 : 0;
    }
    EXPECT_EQ(first, 18);
    EXPECT_EQ(wild, 6);
    EXPECT_EQ(ancient, 0);
}

TEST(TerrainDepthZones, NoDistinctCostAboveT1MeansNoAncient)
{
    DGrid g(10, 3);
    g.Block(1, 1, 8, 1, 1u);   // one row: every land cell is coast, cost 1
    // Ridge and barrier on the coast cells must not change source cost 1.
    for (int x = 1; x <= 8; ++x) { g.ridge[g.I(x, 1)] = 1.0f; g.barrier[g.I(x, 1)] = 1.0f; }
    g.Run();
    for (int x = 1; x <= 8; ++x)
    {
        EXPECT_FLOAT_EQ(g.base[g.I(x, 1)], 1.0f);
        EXPECT_FLOAT_EQ(g.cost[g.I(x, 1)], 1.0f);
        EXPECT_EQ(g.zone[g.I(x, 1)], kFirst);
    }
    EXPECT_FALSE(g.dbg.hasAncient);
    EXPECT_FLOAT_EQ(g.dbg.t1, 1.0f);
    EXPECT_FLOAT_EQ(g.dbg.t2, g.dbg.t1);
}

TEST(TerrainDepthZones, RidgeAndBarrierAccumulateAndRiverIsCorridor)
{
    DGrid g = Strip3();
    const int c = g.I(4, 2);   // a middle cell: its neighbours in rows 1 and 3 are coast (cost 1)
    g.ridge[c] = 0.5f;
    g.barrier[c] = 1.0f;
    g.Run(0.5f);
    // entry = 1 + 4*0.5 + 4*0.5*1 = 5, reached from a coast cell at cost 1.
    EXPECT_FLOAT_EQ(g.base[c], 6.0f);
    EXPECT_FLOAT_EQ(g.cost[c], 6.0f);
    for (int x = 2; x <= 7; ++x) { if (x != 4) { EXPECT_FLOAT_EQ(g.base[g.I(x, 2)], 2.0f); } }

    g.river[c] = 1u;
    g.Run(0.5f);
    // river entry = 1 + 0.25 * (2 + 2) = 2 -> cost 3; base unchanged.
    EXPECT_FLOAT_EQ(g.base[c], 6.0f);
    EXPECT_FLOAT_EQ(g.cost[c], 3.0f);

    g.river[c] = 0u;
    g.Run(0.0f);   // Barrier = 0 removes the barrier term: entry = 1 + 2 = 3 -> cost 4
    EXPECT_FLOAT_EQ(g.base[c], 4.0f);
}

TEST(TerrainDepthZones, MatchesBellmanFordOracleOnRandomFields)
{
    DGrid g(24, 16);
    g.Block(1, 1, 22, 14, 1u);
    uint32_t rng = 12345u;
    auto next = [&]() { rng = rng * 1664525u + 1013904223u; return static_cast<float>((rng >> 8) & 0xFFFF) / 65535.0f; };
    for (int i = 0; i < g.n; ++i)
    {
        g.ridge[i] = next() < 0.3f ? next() : 0.0f;
        g.barrier[i] = next() < 0.3f ? next() : 0.0f;
        g.river[i] = next() < 0.15f ? 1u : 0u;
    }
    g.Run(0.7f);
    for (int pass = 0; pass < 2; ++pass)
    {
        std::vector<double> dist(static_cast<size_t>(g.n), 1e30);
        for (int i = 0; i < g.n; ++i) { if (g.land[i] != 0u && g.coast[i] == 1u) { dist[i] = 1.0; } }
        const std::vector<uint32_t> none(static_cast<size_t>(g.n), 0u);
        const uint32_t* riv = pass == 0 ? none.data() : g.river.data();
        for (bool changed = true; changed;)
        {
            changed = false;
            for (int c = 0; c < g.n; ++c)
            {
                if (g.land[c] == 0u || dist[c] >= 1e29) { continue; }
                for (uint32_t d = 0; d < 6; ++d)
                {
                    const uint32_t nb = g.topo.NeighbourAt(static_cast<uint32_t>(c), d);
                    if (nb == kNoCell || g.land[nb] == 0u) { continue; }
                    const double nd = dist[c] + EntryCost(g.rules, 0.7f, g.ridge.data(), g.barrier.data(), riv, static_cast<int>(nb));
                    if (nd < dist[nb]) { dist[nb] = nd; changed = true; }
                }
            }
        }
        const std::vector<float>& got = pass == 0 ? g.base : g.cost;
        for (int i = 0; i < g.n; ++i)
        {
            if (g.land[i] != 0u) { EXPECT_NEAR(got[i], dist[i], 1e-3) << "pass " << pass << " cell " << i; }
        }
    }
    for (int i = 0; i < g.n; ++i) { EXPECT_LE(g.cost[i], g.base[i]); }
}

TEST(TerrainDepthZones, OtherLandmassesGetNoAncientAndUseMainThresholds)
{
    DGrid g(30, 12);
    g.Block(1, 1, 8, 3, 1u);     // main: the 3-row strip, t1 = 1, t2 = 2
    g.Block(12, 1, 28, 10, 2u);  // a deep second continent (interior costs up to 5)
    g.Run();
    EXPECT_FLOAT_EQ(g.dbg.t1, 1.0f);
    EXPECT_FLOAT_EQ(g.dbg.t2, 2.0f);
    EXPECT_EQ(g.dbg.mainCells, 24);
    int deepWild = 0;
    for (int i = 0; i < g.n; ++i)
    {
        if (g.land[i] != 2u) { continue; }
        EXPECT_FLOAT_EQ(g.base[i], static_cast<float>(g.coast[i]));   // flat landmass: cost == coastDistance
        EXPECT_NE(g.zone[i], kAncient);
        EXPECT_EQ(g.zone[i], g.base[i] <= 1.0f ? kFirst : kWild);
        deepWild += (g.coast[i] >= 3u && g.zone[i] == kWild) ? 1 : 0;
    }
    EXPECT_GT(deepWild, 0);
}

TEST(TerrainDepthZones, FlatContinentThresholdsAndAncientDepth)
{
    DGrid g(24, 18);
    g.Block(2, 2, 21, 15, 1u);
    g.Run();
    std::vector<float> sorted;
    for (int i = 0; i < g.n; ++i)
    {
        if (g.land[i] == 0u) { EXPECT_EQ(g.zone[i], kNone); EXPECT_EQ(g.base[i], 0.0f); EXPECT_EQ(g.cost[i], 0.0f); continue; }
        EXPECT_FLOAT_EQ(g.base[i], static_cast<float>(g.coast[i]));
        EXPECT_FLOAT_EQ(g.cost[i], g.base[i]);
        sorted.push_back(g.base[i]);
    }
    std::sort(sorted.begin(), sorted.end());
    const int n = static_cast<int>(sorted.size());
    EXPECT_FLOAT_EQ(g.dbg.t1, sorted[static_cast<size_t>(std::max(0, static_cast<int>(std::floor(0.45 * n)) - 1))]);
    ASSERT_TRUE(g.dbg.hasAncient);
    EXPECT_GT(g.dbg.t2, g.dbg.t1);
    int ancient = 0;
    for (int i = 0; i < g.n; ++i)
    {
        if (g.land[i] == 0u) { continue; }
        const bool expectFirst = g.base[i] <= g.dbg.t1;
        const bool expectAncient = !expectFirst && g.base[i] >= g.dbg.t2 && g.coast[i] >= 3u;
        EXPECT_EQ(g.zone[i], expectFirst ? kFirst : (expectAncient ? kAncient : kWild));
        ancient += g.zone[i] == kAncient ? 1 : 0;
    }
    EXPECT_GT(ancient, 0);
}

TEST(TerrainDepthZones, TiesAtT1AllShareTheZone)
{
    DGrid g(24, 18);
    g.Block(2, 2, 21, 15, 1u);
    g.Run();
    // Costs tie on flat rings: every cell whose cost equals t1 is First Shores, none above.
    for (int i = 0; i < g.n; ++i)
    {
        if (g.land[i] != 0u && g.base[i] == g.dbg.t1) { EXPECT_EQ(g.zone[i], kFirst); }
        if (g.land[i] != 0u && g.base[i] > g.dbg.t1) { EXPECT_NE(g.zone[i], kFirst); }
    }
}

TEST(TerrainDepthZones, RiverLowersZoneCostAndZonesOnlyMoveInland)
{
    DGrid g(24, 18);
    g.Block(2, 2, 21, 15, 1u);
    g.ComputeCoast();
    for (int i = 0; i < g.n; ++i) { if (g.land[i] != 0u && g.coast[i] >= 2u) { g.barrier[i] = 1.0f; } }
    g.Run(1.0f);
    const std::vector<uint32_t> zoneNoRiver = g.zone;
    const std::vector<float> baseNoRiver = g.base;
    const Stage8Debug dNo = g.dbg;
    // A river line down column 11 across the barrier.
    for (int y = 3; y <= 14; ++y) { g.river[g.I(11, y)] = 1u; }
    g.Run(1.0f);
    EXPECT_EQ(g.base, baseNoRiver);
    EXPECT_EQ(g.dbg.t1, dNo.t1);
    EXPECT_EQ(g.dbg.t2, dNo.t2);
    int lowered = 0;
    for (int i = 0; i < g.n; ++i)
    {
        EXPECT_LE(g.cost[i], g.base[i]);
        EXPECT_LE(g.zone[i], zoneNoRiver[i]);
        lowered += g.cost[i] < g.base[i] ? 1 : 0;
    }
    EXPECT_GT(lowered, 0);
}

TEST(TerrainDepthZones, HandBuiltRunsAreByteIdentical)
{
    DGrid a = Strip3();
    DGrid b = Strip3();
    a.ridge[a.I(4, 2)] = 0.3f;
    b.ridge[b.I(4, 2)] = 0.3f;
    a.Run();
    b.Run();
    EXPECT_EQ(0, std::memcmp(a.base.data(), b.base.data(), a.base.size() * sizeof(float)));
    EXPECT_EQ(0, std::memcmp(a.cost.data(), b.cost.data(), a.cost.size() * sizeof(float)));
    EXPECT_EQ(0, std::memcmp(a.zone.data(), b.zone.data(), a.zone.size() * sizeof(uint32_t)));
}

// ---------------------------------------------------------------------------
// Generator integration
// ---------------------------------------------------------------------------
TEST(TerrainDepthZones, LayersAreRegisteredAndReadable)
{
    Gen r;
    r.Go(3, WorldShapeParams{});
    const TerrainLayers& l = r.gen.GetLayers();
    ASSERT_GE(l.FindLayer("zoneCostBase"), 0);
    ASSERT_GE(l.FindLayer("zoneCost"), 0);
    ASSERT_GE(l.FindLayer("depthZone"), 0);
    EXPECT_EQ(TerrainLayers::GetLayerInfo(l.FindLayer("zoneCostBase")).stage, TerrainStage::DepthZones);
    EXPECT_NE(F(r.gen, "zoneCostBase"), nullptr);
    EXPECT_NE(F(r.gen, "zoneCost"), nullptr);
    EXPECT_NE(U(r.gen, "depthZone"), nullptr);
    EXPECT_EQ(F(r.gen, "depthZone"), nullptr);   // UInt layer
}

TEST(TerrainDepthZones, Ac13ZoneOrderingOnRealTerrain)
{
    for (uint64_t seed = 1; seed <= 3; ++seed)
    {
        Gen r;
        r.Go(seed, WorldShapeParams{});
        EXPECT_EQ(0, CheckZones(r.gen, "seed " + std::to_string(seed)));
        const uint32_t* lake = U(r.gen, "lakeMask");
        const uint32_t* zone = U(r.gen, "depthZone");
        const uint32_t* lm = U(r.gen, "landmass");
        for (int i = 0; i < kN; ++i)
        {
            if (lake[i] != 0u) { EXPECT_NE(zone[i], kNone); EXPECT_NE(lm[i], 0u); }   // a lake takes its position's zone
        }
    }
}

TEST(TerrainDepthZones, Ac13bRiverMonotonicityOnRealTerrain)
{
    for (uint64_t seed = 1; seed <= 3; ++seed) { EXPECT_EQ(0, CheckRiverSweep(seed)); }
}

TEST(TerrainDepthZones, GeneratorRunsAreByteIdentical)
{
    Gen a, b;
    a.Go(11, WorldShapeParams{});
    b.Go(11, WorldShapeParams{});
    EXPECT_EQ(0, std::memcmp(F(a.gen, "zoneCostBase"), F(b.gen, "zoneCostBase"), kN * sizeof(float)));
    EXPECT_EQ(0, std::memcmp(F(a.gen, "zoneCost"), F(b.gen, "zoneCost"), kN * sizeof(float)));
    EXPECT_EQ(0, std::memcmp(U(a.gen, "depthZone"), U(b.gen, "depthZone"), kN * sizeof(uint32_t)));
}

TEST(TerrainDepthZones, DirtyBits)
{
    Gen r;
    WorldShapeParams s;
    r.Go(5, s);
    EXPECT_EQ(Ran(r.gen).count(8), 1u);

    s.rivers = 0.8f;
    r.Go(5, s);
    EXPECT_EQ(Ran(r.gen), (std::set<int>{ 6, 8, 9, 10 }));

    s.lakes = 9;
    r.Go(5, s);
    EXPECT_EQ(Ran(r.gen).count(8), 0u);

    s.latitudeNorthDeg = 40.0f;
    r.Go(5, s);
    EXPECT_EQ(Ran(r.gen).count(8), 0u);

    s.mountainousness = 0.85f;
    r.Go(5, s);
    EXPECT_EQ(Ran(r.gen).count(8), 1u);   // via ridgeMask (stage 2)
    EXPECT_EQ(Ran(r.gen).count(3), 0u);

    // Barrier's first stage is 3; stage 8 reads 3, so downstream propagation reruns it and the costs follow.
    const std::vector<float> before(F(r.gen, "zoneCostBase"), F(r.gen, "zoneCostBase") + kN);
    s.barrier = 1.0f;
    r.Go(5, s);
    EXPECT_EQ(Ran(r.gen).count(3), 1u);
    EXPECT_EQ(Ran(r.gen).count(8), 1u);
    EXPECT_NE(before, std::vector<float>(F(r.gen, "zoneCostBase"), F(r.gen, "zoneCostBase") + kN));
    EXPECT_EQ(0, CheckZones(r.gen, "after barrier"));
}

// ---------------------------------------------------------------------------
// Sweeps
// ---------------------------------------------------------------------------
TEST(SLOW_TerrainDepthZones, ZoneInvariantsAndAncientRateOver20Seeds)
{
    int hasAncient = 0, expectButNone = 0, expected = 0;
    for (uint64_t seed = 1; seed <= 20; ++seed)
    {
        Gen r;
        r.Go(seed, WorldShapeParams{});
        ZoneStats st;
        EXPECT_EQ(0, CheckZones(r.gen, "seed " + std::to_string(seed), &st));
        hasAncient += st.hasAncient ? 1 : 0;
        expected += st.expectAncient ? 1 : 0;
        expectButNone += (st.expectAncient && st.ancient == 0) ? 1 : 0;
    }
    std::printf("[TerrainDepthZones] hasAncient %d / 20; Ancient-expected %d / 20; expected but zero Ancient cells %d\n",
                hasAncient, expected, expectButNone);
    SUCCEED();
}

TEST(SLOW_TerrainDepthZones, RiverSweepMonotonicityOver20Seeds)
{
    for (uint64_t seed = 1; seed <= 20; ++seed) { EXPECT_EQ(0, CheckRiverSweep(seed)); }
}

// ---------------------------------------------------------------------------
// Timing (informational, Release only)
// ---------------------------------------------------------------------------
TEST(TerrainPerf, StageEightDepthZonesReport)
{
#ifdef NDEBUG
    constexpr int kRuns = 50;
    std::vector<double> t8;
    Gen r;
    const WorldShapeParams s;
    for (int run = 0; run < kRuns; ++run)
    {
        r.Go(3000 + static_cast<uint64_t>(run), s);
        t8.push_back(r.gen.GetTimings()[static_cast<int>(TerrainStage::DepthZones)].milliseconds);
    }
    std::sort(t8.begin(), t8.end());
    std::printf("[TerrainPerf] stage 8 DepthZones median %.3f ms, max %.3f ms (66x42, %d runs)\n",
                t8[t8.size() / 2], t8.back(), kRuns);
    SUCCEED();
#else
    GTEST_SKIP() << "Release-only timing report";
#endif
}
