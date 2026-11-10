// S-1 Terrain Generator task 12: Stage 10 Classify (slope, terrainType, waterSource, terrainFlags) and
// Stage 11 Latitude. Hand-built grids check every rule row and threshold tie exactly through ComputeStage10;
// generator runs check invariants on real terrain, dirty propagation and AC16. Wide sweeps are SLOW_.
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
using Dia::Pathfinding::CellCoord;
using Dia::Pathfinding::HexTopology;
using Dia::Pathfinding::kNoCell;

namespace {

const GridDimensions kSmall{ 66, 42 };

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

constexpr uint32_t T(TerrainType t) { return static_cast<uint32_t>(t); }
constexpr uint32_t W(WaterSource t) { return static_cast<uint32_t>(t); }

float Below(float v) { return std::nextafter(v, 0.0f); }
float Above(float v) { return std::nextafter(v, 2.0f); }

// ---------------------------------------------------------------------------
// Hand-built grid: land everywhere (elevation 0.1, flat), painted per test.
// ---------------------------------------------------------------------------
struct CGrid
{
    GridDimensions        dims;
    HexTopology           topo;
    int                   n;
    std::vector<uint32_t> land, coast, vent, pass, river, lake, type, water, flags;
    std::vector<float>    ridge, band, elev, slope;
    TerrainRules          rules = TerrainRules::Defaults();

    CGrid(int w, int h) : dims{ w, h }, topo(w, h), n(w * h), land(static_cast<size_t>(n), 1u),
        coast(static_cast<size_t>(n), 5u), vent(static_cast<size_t>(n), 0u), pass(static_cast<size_t>(n), 0u),
        river(static_cast<size_t>(n), 0u), lake(static_cast<size_t>(n), 0u), type(static_cast<size_t>(n), 99u),
        water(static_cast<size_t>(n), 99u), flags(static_cast<size_t>(n), 99u), ridge(static_cast<size_t>(n), 0.0f),
        band(static_cast<size_t>(n), 0.0f), elev(static_cast<size_t>(n), 0.1f), slope(static_cast<size_t>(n), -1.0f) {}

    int I(int x, int y) const { return y * dims.width + x; }
    int Nb(int c, uint32_t d) const { return static_cast<int>(topo.NeighbourAt(static_cast<uint32_t>(c), d)); }

    void Run()
    {
        Stage10Layers out;
        out.slope = slope.data();
        out.terrainType = type.data();
        out.waterSource = water.data();
        out.terrainFlags = flags.data();
        ComputeStage10(rules, dims, land.data(), coast.data(), vent.data(), ridge.data(), pass.data(), band.data(),
                       river.data(), lake.data(), elev.data(), out);
    }

    void SetNeighbours(int c, float e)
    {
        for (uint32_t d = 0; d < 6; ++d)
        {
            const uint32_t nb = topo.NeighbourAt(static_cast<uint32_t>(c), d);
            if (nb != kNoCell) { elev[nb] = e; }
        }
    }
};

struct Case
{
    const char* name;
    TerrainType expect;
    float       e = 0.1f;
    float       nbr = 0.1f;
    bool        ocean = false;
    uint32_t    coastDist = 5;
    bool        isLake = false, isRiver = false, isVent = false, isPass = false;
};

// One isolated case per plot; plots are 4 apart so neighbourhoods never overlap.
void Place(CGrid& g, int k, const Case& c, int& cell)
{
    const int x = 2 + 4 * (k % 9);
    const int y = 2 + 4 * (k / 9);
    cell = g.I(x, y);
    g.SetNeighbours(cell, c.nbr);
    g.elev[cell] = c.e;
    g.land[cell] = c.ocean ? 0u : 1u;
    g.coast[cell] = c.coastDist;
    g.lake[cell] = c.isLake ? 1u : 0u;
    g.river[cell] = c.isRiver ? 1u : 0u;
    g.vent[cell] = c.isVent ? 1u : 0u;
    g.pass[cell] = c.isPass ? 1u : 0u;
}

std::vector<Case> RuleCases()
{
    std::vector<Case> v;
    auto add = [&](Case c) { v.push_back(c); };
    // Rows 1-2 (ocean) with coastDistance ties.
    add({ "ocean cd 3", TerrainType::DeepWater, 0.0f, 0.1f, true, 3 });
    add({ "ocean cd 7", TerrainType::DeepWater, 0.0f, 0.1f, true, 7 });
    add({ "ocean cd 2", TerrainType::ShallowWater, 0.0f, 0.1f, true, 2 });
    add({ "ocean cd 0", TerrainType::ShallowWater, 0.0f, 0.1f, true, 0 });
    { Case c{ "ocean beats pass/vent/river/lake", TerrainType::ShallowWater, 0.0f, 0.1f, true, 1 };
      c.isPass = c.isVent = c.isRiver = c.isLake = true; add(c); }
    // Rows 3-4 and priorities.
    { Case c{ "lake", TerrainType::Lake }; c.isLake = true; add(c); }
    { Case c{ "lake over river", TerrainType::Lake }; c.isLake = c.isRiver = true; add(c); }
    { Case c{ "lake over vent+pass", TerrainType::Lake }; c.isLake = c.isVent = c.isPass = true; add(c); }
    { Case c{ "river", TerrainType::River }; c.isRiver = true; add(c); }
    { Case c{ "vent on river is River", TerrainType::River }; c.isRiver = c.isVent = true; add(c); }
    { Case c{ "river on cliff slope is River", TerrainType::River, 0.5f, 0.1f }; c.isRiver = true; add(c); }
    // Row 5.
    { Case c{ "vent", TerrainType::Volcano }; c.isVent = true; add(c); }
    { Case c{ "vent over cliff", TerrainType::Volcano, 0.5f, 0.1f }; c.isVent = true; add(c); }
    { Case c{ "vent over pass", TerrainType::Volcano }; c.isVent = c.isPass = true; add(c); }
    // Row 6: cliff ties (neighbours at 0 so slope == e exactly).
    add({ "slope == 0.18", TerrainType::Cliff, 0.18f, 0.0f });
    add({ "slope just above 0.18", TerrainType::Cliff, Above(0.18f), 0.0f });
    add({ "slope just below 0.18", TerrainType::Mountain, Below(0.18f), 0.0f });
    { Case c{ "pass on cliff is Cliff", TerrainType::Cliff, 0.5f, 0.1f }; c.isPass = true; add(c); }
    // Row 7.
    { Case c{ "pass", TerrainType::Pass }; c.isPass = true; add(c); }
    { Case c{ "pass on peak is Pass", TerrainType::Pass, 0.9f, 0.9f }; c.isPass = true; add(c); }
    { Case c{ "pass with slope just below cliff", TerrainType::Pass, Below(0.18f), 0.0f }; c.isPass = true; add(c); }
    // Row 8.
    add({ "e == 0.80", TerrainType::Peak, 0.80f, 0.80f });
    add({ "e just above 0.80", TerrainType::Peak, Above(0.80f), Above(0.80f) });
    add({ "e just below 0.80", TerrainType::Mountain, Below(0.80f), Below(0.80f) });
    // Row 9.
    add({ "e == 0.55", TerrainType::Mountain, 0.55f, 0.55f });
    add({ "e just below 0.55", TerrainType::Highland, Below(0.55f), Below(0.55f) });
    add({ "slope == 0.10", TerrainType::Mountain, 0.10f, 0.0f });
    add({ "slope just below 0.10", TerrainType::Hills, Below(0.10f), 0.0f });
    // Row 10.
    add({ "e == 0.35", TerrainType::Highland, 0.35f, 0.35f });
    add({ "e just below 0.35", TerrainType::Hills, Below(0.35f), Below(0.35f) });
    // Row 11.
    add({ "e == 0.15", TerrainType::Hills, 0.15f, 0.15f });
    add({ "e just below 0.15", TerrainType::Lowland, Below(0.15f), Below(0.15f) });
    add({ "slope == 0.05", TerrainType::Hills, 0.05f, 0.0f });
    add({ "slope just below 0.05", TerrainType::Lowland, Below(0.05f), 0.0f });
    // Row 12.
    add({ "lowland", TerrainType::Lowland, 0.1f, 0.1f });
    add({ "flat at coast elevation", TerrainType::Lowland, 0.02f, 0.02f });
    return v;
}

TEST(TerrainClassify, EveryRuleRowAndThresholdTie)
{
    CGrid g(40, 40);
    const std::vector<Case> cases = RuleCases();
    ASSERT_LE(cases.size(), 81u);
    std::vector<int> cells(cases.size());
    for (size_t k = 0; k < cases.size(); ++k) { Place(g, static_cast<int>(k), cases[k], cells[k]); }
    g.Run();
    for (size_t k = 0; k < cases.size(); ++k)
    {
        EXPECT_EQ(T(cases[k].expect), g.type[cells[k]])
            << cases[k].name << ": got " << TerrainTypeName(static_cast<TerrainType>(g.type[cells[k]]))
            << " want " << TerrainTypeName(cases[k].expect);
    }
}

TEST(TerrainClassify, EveryTerrainTypeAppearsInRuleCases)
{
    std::set<TerrainType> seen;
    for (const Case& c : RuleCases()) { seen.insert(c.expect); }
    EXPECT_EQ(static_cast<size_t>(TerrainType::Count), seen.size());
}

TEST(TerrainClassify, WaterSourceOceanLakeRiverNone)
{
    CGrid g(40, 40);
    const std::vector<Case> cases = RuleCases();
    std::vector<int> cells(cases.size());
    for (size_t k = 0; k < cases.size(); ++k) { Place(g, static_cast<int>(k), cases[k], cells[k]); }
    g.Run();
    for (size_t k = 0; k < cases.size(); ++k)
    {
        const Case& c = cases[k];
        const WaterSource want = c.ocean ? WaterSource::Ocean : c.isLake ? WaterSource::Lake
                               : c.isRiver ? WaterSource::River : WaterSource::None;
        EXPECT_EQ(W(want), g.water[cells[k]]) << c.name;
    }
}

TEST(TerrainClassify, SlopeIsMaxOverNonOceanNeighboursOceanCellsZero)
{
    CGrid g(12, 12);
    const int c = g.I(5, 5);
    g.SetNeighbours(c, 0.1f);
    g.elev[c] = 0.3f;
    g.elev[g.Nb(c, 0)] = 0.6f;    // largest land difference: 0.3
    g.elev[g.Nb(c, 1)] = 0.25f;
    const int oc = g.Nb(c, 2);
    g.land[oc] = 0u;
    g.elev[oc] = 0.95f;           // would give 0.65 if ocean neighbours were counted
    g.Run();
    EXPECT_FLOAT_EQ(0.3f, g.slope[c]);
    EXPECT_FLOAT_EQ(0.0f, g.slope[oc]);   // ocean cell itself
    // A lone land cell among ocean has no non-ocean neighbour: slope 0 everywhere.
    CGrid h(9, 9);
    std::fill(h.land.begin(), h.land.end(), 0u);
    h.land[h.I(4, 4)] = 1u;
    h.elev[h.I(4, 4)] = 0.9f;
    h.Run();
    for (int i = 0; i < h.n; ++i) { EXPECT_FLOAT_EQ(0.0f, h.slope[i]) << i; }
}

TEST(TerrainClassify, SlopeCountsLakeNeighbours)
{
    CGrid g(12, 12);
    const int c = g.I(5, 5);
    g.SetNeighbours(c, 0.1f);
    g.elev[c] = 0.4f;
    const int lk = g.Nb(c, 3);
    g.lake[lk] = 1u;
    g.elev[lk] = 0.05f;
    g.Run();
    EXPECT_FLOAT_EQ(0.35f, g.slope[c]);
}

// ---------------------------------------------------------------------------
// Flags
// ---------------------------------------------------------------------------
TEST(TerrainClassify, CoastFlagIsLandNextToOcean)
{
    CGrid g(12, 12);
    const int land = g.I(5, 5);
    const int oc = g.Nb(land, 0);
    const int ocOcean = g.Nb(oc, 0);
    g.land[oc] = 0u;
    g.land[ocOcean] = 0u;
    g.Run();
    EXPECT_NE(0u, g.flags[land] & TerrainFlag::Coast);
    EXPECT_EQ(0u, g.flags[oc] & TerrainFlag::Coast) << "ocean cells never carry Coast";
    EXPECT_EQ(0u, g.flags[ocOcean] & TerrainFlag::Coast);
    EXPECT_EQ(0u, g.flags[g.I(10, 10)] & TerrainFlag::Coast) << "inland";
}

TEST(TerrainClassify, RidgeFlagThresholdAtRidgeCellMin)
{
    CGrid g(12, 12);
    const float m = g.rules.kRidgeCellMin();
    g.ridge[g.I(2, 2)] = m;
    g.ridge[g.I(4, 2)] = Above(m);
    g.ridge[g.I(6, 2)] = Below(m);
    g.ridge[g.I(8, 2)] = 0.0f;
    g.Run();
    EXPECT_NE(0u, g.flags[g.I(2, 2)] & TerrainFlag::Ridge);
    EXPECT_NE(0u, g.flags[g.I(4, 2)] & TerrainFlag::Ridge);
    EXPECT_EQ(0u, g.flags[g.I(6, 2)] & TerrainFlag::Ridge);
    EXPECT_EQ(0u, g.flags[g.I(8, 2)] & TerrainFlag::Ridge);
}

TEST(TerrainClassify, BarrierFlagThresholdAtHalf)
{
    CGrid g(12, 12);
    g.band[g.I(2, 2)] = 0.5f;
    g.band[g.I(4, 2)] = Above(0.5f);
    g.band[g.I(6, 2)] = Below(0.5f);
    g.Run();
    EXPECT_NE(0u, g.flags[g.I(2, 2)] & TerrainFlag::Barrier);
    EXPECT_NE(0u, g.flags[g.I(4, 2)] & TerrainFlag::Barrier);
    EXPECT_EQ(0u, g.flags[g.I(6, 2)] & TerrainFlag::Barrier);
    EXPECT_EQ(0u, g.flags[g.I(8, 2)] & TerrainFlag::Barrier);
}

TEST(TerrainClassify, PassFlagFollowsPassMask)
{
    CGrid g(12, 12);
    g.pass[g.I(3, 3)] = 1u;
    g.Run();
    EXPECT_NE(0u, g.flags[g.I(3, 3)] & TerrainFlag::Pass);
    EXPECT_EQ(0u, g.flags[g.I(4, 3)] & TerrainFlag::Pass);
}

TEST(TerrainClassify, VolcanicFlagWithinVentRadiusOnly)
{
    CGrid g(21, 21);
    const int vent = g.I(10, 10);
    g.vent[vent] = 1u;
    // Independent hex-distance BFS from the vent.
    std::vector<int> dist(static_cast<size_t>(g.n), -1);
    std::vector<int> queue{ vent };
    dist[vent] = 0;
    for (size_t head = 0; head < queue.size(); ++head)
    {
        const int c = queue[head];
        for (uint32_t d = 0; d < 6; ++d)
        {
            const int nb = g.Nb(c, d);
            if (nb >= 0 && dist[nb] < 0) { dist[nb] = dist[c] + 1; queue.push_back(nb); }
        }
    }
    g.Run();
    int at2 = 0, at3 = 0;
    for (int c = 0; c < g.n; ++c)
    {
        const bool flagged = (g.flags[c] & TerrainFlag::Volcanic) != 0;
        EXPECT_EQ(dist[c] <= 2, flagged) << "cell " << c << " dist " << dist[c];
        at2 += dist[c] == 2 ? 1 : 0;
        at3 += dist[c] == 3 ? 1 : 0;
    }
    EXPECT_EQ(12, at2);
    EXPECT_GT(at3, 0);
}

TEST(TerrainClassify, VolcanicFlagSkipsOceanAndHandlesGridEdges)
{
    CGrid g(9, 9);
    g.vent[g.I(0, 0)] = 1u;
    g.land[g.I(1, 0)] = 0u;   // ocean cell next to the vent
    g.vent[g.I(8, 8)] = 1u;
    g.Run();
    EXPECT_NE(0u, g.flags[g.I(0, 0)] & TerrainFlag::Volcanic);
    EXPECT_EQ(0u, g.flags[g.I(1, 0)] & TerrainFlag::Volcanic) << "ocean never flagged";
    EXPECT_NE(0u, g.flags[g.I(8, 8)] & TerrainFlag::Volcanic);
    EXPECT_EQ(0u, g.flags[g.I(4, 4)] & TerrainFlag::Volcanic);
}

TEST(TerrainClassify, RiverCrossesBarrierNeedsRiverAndRidgeOrBarrier)
{
    CGrid g(16, 8);
    const float m = g.rules.kRidgeCellMin();
    auto river = [&](int x) { g.river[g.I(x, 2)] = 1u; };
    river(1); g.ridge[g.I(1, 2)] = m;            // river + ridge
    river(3); g.band[g.I(3, 2)] = 0.5f;          // river + barrier
    river(5); g.ridge[g.I(5, 2)] = Below(m);     // river, ridge just below
    river(7);                                    // river alone
    g.ridge[g.I(9, 2)] = 1.0f;                   // ridge alone
    river(11); g.lake[g.I(11, 2)] = 1u; g.ridge[g.I(11, 2)] = 1.0f;    // lake wins over river
    river(13); g.ridge[g.I(13, 2)] = 1.0f; g.band[g.I(13, 2)] = 1.0f;  // both
    g.Run();
    auto crosses = [&](int x) { return (g.flags[g.I(x, 2)] & TerrainFlag::RiverCrossesBarrier) != 0; };
    EXPECT_TRUE(crosses(1));
    EXPECT_TRUE(crosses(3));
    EXPECT_FALSE(crosses(5));
    EXPECT_FALSE(crosses(7));
    EXPECT_FALSE(crosses(9));
    EXPECT_FALSE(crosses(11));
    EXPECT_TRUE(crosses(13));
}

TEST(TerrainClassify, NullOptionalInputsReadAsZeroAndZeroSizeIsNoOp)
{
    CGrid g(6, 6);
    Stage10Layers out;
    out.slope = g.slope.data();
    out.terrainType = g.type.data();
    out.waterSource = g.water.data();
    out.terrainFlags = g.flags.data();
    ComputeStage10(g.rules, g.dims, g.land.data(), nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                   g.elev.data(), out);
    for (int i = 0; i < g.n; ++i)
    {
        EXPECT_EQ(T(TerrainType::Lowland), g.type[i]);
        EXPECT_EQ(0u, g.flags[i]);
    }
    ComputeStage10(g.rules, GridDimensions{ 0, 0 }, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                   nullptr, nullptr, out);
    SUCCEED();
}

// ---------------------------------------------------------------------------
// Real generator
// ---------------------------------------------------------------------------
int CheckWorld(const TerrainGenerator& g, const std::string& tag)
{
    int bad = 0;
    const int n = kSmall.width * kSmall.height;
    const float* slope = F(g, "slope");
    const uint32_t* type = U(g, "terrainType");
    const uint32_t* water = U(g, "waterSource");
    const uint32_t* flags = U(g, "terrainFlags");
    const uint32_t* land = U(g, "landmass");
    const uint32_t* lake = U(g, "lakeMask");
    const uint32_t* river = U(g, "riverMask");
    if (slope == nullptr || type == nullptr || water == nullptr || flags == nullptr)
    {
        ADD_FAILURE() << tag << " layers unreadable";
        return 1;
    }
    int nonNone = 0;
    for (int c = 0; c < n; ++c)
    {
        const bool ocean = land[c] == 0u;
        const bool isLake = !ocean && lake[c] != 0u;
        const bool isRiver = !ocean && !isLake && river[c] != 0u;
        bool ok = type[c] < static_cast<uint32_t>(TerrainType::Count) && slope[c] >= 0.0f && std::isfinite(slope[c]);
        const bool waterType = type[c] == T(TerrainType::DeepWater) || type[c] == T(TerrainType::ShallowWater);
        ok = ok && (waterType == ocean);
        ok = ok && ((type[c] == T(TerrainType::Lake)) == isLake);
        ok = ok && ((type[c] == T(TerrainType::River)) == isRiver);
        ok = ok && water[c] == (ocean ? W(WaterSource::Ocean) : isLake ? W(WaterSource::Lake)
                                : isRiver ? W(WaterSource::River) : W(WaterSource::None));
        ok = ok && flags[c] < 64u;
        ok = ok && (!ocean || slope[c] == 0.0f);
        ok = ok && ((flags[c] & TerrainFlag::RiverCrossesBarrier) == 0
                    || (isRiver && (flags[c] & (TerrainFlag::Ridge | TerrainFlag::Barrier)) != 0));
        nonNone += water[c] != W(WaterSource::None) ? 1 : 0;
        if (!ok && bad++ < 3) { ADD_FAILURE() << tag << " cell " << c << " type " << type[c] << " water " << water[c]; }
    }
    if (nonNone == 0) { ++bad; ADD_FAILURE() << tag << " no non-None water source"; }
    return bad;
}

TEST(TerrainClassify, RealWorldInvariants)
{
    for (uint64_t seed = 1; seed <= 3; ++seed)
    {
        Gen r;
        r.Go(seed, WorldShapeParams{});
        EXPECT_EQ(0, CheckWorld(r.gen, "seed " + std::to_string(seed)));
    }
}

TEST(TerrainClassify, RealWorldHasWaterAndLowland)
{
    Gen r;
    r.Go(1, WorldShapeParams{});
    ASSERT_NE(nullptr, U(r.gen, "terrainType"));
    const uint32_t* type = U(r.gen, "terrainType");
    std::set<uint32_t> seen(type, type + kSmall.width * kSmall.height);
    EXPECT_TRUE(seen.count(T(TerrainType::DeepWater)) > 0);
    EXPECT_TRUE(seen.count(T(TerrainType::ShallowWater)) > 0);
    EXPECT_TRUE(seen.count(T(TerrainType::Lowland)) > 0);
}

TEST(TerrainClassify, DeterministicByteIdentical)
{
    Gen a, b;
    a.Go(7, WorldShapeParams{});
    b.Go(7, WorldShapeParams{});
    const size_t n = static_cast<size_t>(kSmall.width) * kSmall.height;
    for (const char* name : { "slope", "latitude" })
    {
        ASSERT_NE(nullptr, F(a.gen, name));
        EXPECT_EQ(0, std::memcmp(F(a.gen, name), F(b.gen, name), n * sizeof(float))) << name;
    }
    for (const char* name : { "terrainType", "waterSource", "terrainFlags" })
    {
        ASSERT_NE(nullptr, U(a.gen, name));
        EXPECT_EQ(0, std::memcmp(U(a.gen, name), U(b.gen, name), n * sizeof(uint32_t))) << name;
    }
}

TEST(TerrainClassify, SliderChangeThenRevertMatchesFreshRun)
{
    Gen a, b;
    WorldShapeParams s;
    a.Go(5, s);
    WorldShapeParams t = s;
    t.rivers = 0.9f;
    t.lakes = 8;
    a.Go(5, t);
    a.Go(5, s);
    b.Go(5, s);
    const size_t n = static_cast<size_t>(kSmall.width) * kSmall.height;
    ASSERT_NE(nullptr, U(a.gen, "terrainType"));
    EXPECT_EQ(0, std::memcmp(U(a.gen, "terrainType"), U(b.gen, "terrainType"), n * sizeof(uint32_t)));
    EXPECT_EQ(0, std::memcmp(U(a.gen, "terrainFlags"), U(b.gen, "terrainFlags"), n * sizeof(uint32_t)));
    EXPECT_EQ(0, std::memcmp(F(a.gen, "slope"), F(b.gen, "slope"), n * sizeof(float)));
}

// ---------------------------------------------------------------------------
// Dirty propagation
// ---------------------------------------------------------------------------
std::set<int> Closure(int first)
{
    std::set<int> r{ first };
    for (int s = first + 1; s < kImplementedStageCount; ++s)
    {
        for (int d : r)
        {
            if ((kStageReads[s] & (1u << d)) != 0) { r.insert(s); break; }
        }
    }
    return r;
}

TEST(TerrainClassify, StageReadsDeclarations)
{
    EXPECT_EQ((1u << 1) | (1u << 2) | (1u << 3) | (1u << 6) | (1u << 7) | (1u << 8) | (1u << 9),
              kStageReads[static_cast<int>(TerrainStage::Classify)]);
    EXPECT_EQ(0u, kStageReads[static_cast<int>(TerrainStage::Latitude)]);
}

TEST(TerrainClassify, DirtySetsMatchStageReads)
{
    struct Row { const char* name; void (*apply)(WorldShapeParams&); std::set<int> expect; };
    const Row rows[] =
    {
        { "valleyDepth", [](WorldShapeParams& s) { s.valleyDepth = 0.9f; }, { 9, 10 } },
        { "rivers",      [](WorldShapeParams& s) { s.rivers = 0.9f; },      { 6, 8, 9, 10 } },
        { "lakes",       [](WorldShapeParams& s) { s.lakes = 9; },          { 7, 9, 10 } },
        { "passes",      [](WorldShapeParams& s) { s.passes = 4; },         { 2, 4, 5, 6, 7, 8, 9, 10 } },
        { "barrier",     [](WorldShapeParams& s) { s.barrier = 0.9f; },     { 3, 4, 5, 6, 7, 8, 9, 10 } },
        { "landFraction", [](WorldShapeParams& s) { s.landFraction = 0.5f; }, { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 } },
        { "latitudeNorth", [](WorldShapeParams& s) { s.latitudeNorthDeg = 50.0f; }, { 11 } },
        { "latitudeSouth", [](WorldShapeParams& s) { s.latitudeSouthDeg = 10.0f; }, { 11 } },
    };
    for (const Row& row : rows)
    {
        Gen r;
        WorldShapeParams s;
        r.Go(11, s);
        WorldShapeParams t = s;
        row.apply(t);
        r.Go(11, t);
        EXPECT_EQ(row.expect, Ran(r.gen)) << row.name;
    }
    // The literal sets above also follow from kStageReads.
    EXPECT_EQ((std::set<int>{ 9, 10 }), Closure(9));
    EXPECT_EQ((std::set<int>{ 6, 8, 9, 10 }), Closure(6));
    EXPECT_EQ((std::set<int>{ 7, 9, 10 }), Closure(7));
}

// ---------------------------------------------------------------------------
// Stage 11 Latitude (AC16)
// ---------------------------------------------------------------------------
float CentreY(int x, int y) { return HexTopology::CellCenter(CellCoord{ x, y }, 1.0f).y; }

TEST(TerrainLatitude, DefaultSpanEndsAtNorthAndSouth)
{
    Gen r;
    r.Go(1, WorldShapeParams{});
    const float* lat = F(r.gen, "latitude");
    ASSERT_NE(nullptr, lat);
    EXPECT_NEAR(60.0f / 90.0f, lat[0], 1e-6);
    int lowest = 0;
    float maxY = -1.0f;
    for (int y = 0; y < kSmall.height; ++y)
    {
        for (int x = 0; x < kSmall.width; ++x)
        {
            if (CentreY(x, y) > maxY) { maxY = CentreY(x, y); lowest = y * kSmall.width + x; }
        }
    }
    EXPECT_NEAR(20.0f / 90.0f, lat[lowest], 1e-6);
}

TEST(TerrainLatitude, MonotoneInCentreY)
{
    Gen r;
    r.Go(2, WorldShapeParams{});
    const float* lat = F(r.gen, "latitude");
    ASSERT_NE(nullptr, lat);
    std::vector<std::pair<float, float>> v;   // (cy, latitude)
    for (int y = 0; y < kSmall.height; ++y)
    {
        for (int x = 0; x < kSmall.width; ++x) { v.push_back({ CentreY(x, y), lat[y * kSmall.width + x] }); }
    }
    std::sort(v.begin(), v.end());
    for (size_t i = 1; i < v.size(); ++i)
    {
        EXPECT_LE(v[i].second, v[i - 1].second + 1e-6f) << i;   // 60 -> 20 decreases southward
        if (v[i].first == v[i - 1].first) { EXPECT_FLOAT_EQ(v[i].second, v[i - 1].second); }
    }
}

TEST(TerrainLatitude, EquatorCrossingHasMinimumNearMiddle)
{
    Gen r;
    WorldShapeParams s;
    s.latitudeNorthDeg = 30.0f;
    s.latitudeSouthDeg = -30.0f;
    r.Go(3, s);
    const float* lat = F(r.gen, "latitude");
    ASSERT_NE(nullptr, lat);
    const float maxY = MaxCentreY(kSmall);
    int best = 0;
    float bestDist = 1e9f;
    float minLat = 2.0f;
    for (int y = 0; y < kSmall.height; ++y)
    {
        for (int x = 0; x < kSmall.width; ++x)
        {
            const float d = std::fabs(CentreY(x, y) - maxY * 0.5f);
            if (d < bestDist) { bestDist = d; best = y * kSmall.width + x; }
            minLat = std::min(minLat, lat[y * kSmall.width + x]);
        }
    }
    EXPECT_FLOAT_EQ(minLat, lat[best]);
    EXPECT_LT(minLat, 0.02f);
    EXPECT_NEAR(30.0f / 90.0f, lat[0], 1e-6);
}

TEST(TerrainLatitude, ClampsToOne)
{
    Gen r;
    WorldShapeParams s;
    s.latitudeNorthDeg = 90.0f;
    s.latitudeSouthDeg = 90.0f;
    r.Go(4, s);
    const float* lat = F(r.gen, "latitude");
    ASSERT_NE(nullptr, lat);
    for (int i = 0; i < kSmall.width * kSmall.height; ++i) { EXPECT_FLOAT_EQ(1.0f, lat[i]); }

    std::vector<float> out(static_cast<size_t>(kSmall.width) * kSmall.height, -1.0f);
    ComputeStage11(kSmall, 400.0f, -400.0f, out.data());   // out-of-range inputs still clamp
    for (float v : out) { EXPECT_LE(v, 1.0f); EXPECT_GE(v, 0.0f); }
    EXPECT_FLOAT_EQ(1.0f, out[0]);
}

TEST(TerrainLatitude, SliderChangeRunsStageElevenOnly)
{
    Gen r;
    WorldShapeParams s;
    r.Go(6, s);
    EXPECT_EQ(12u, Ran(r.gen).size());
    WorldShapeParams t = s;
    t.latitudeNorthDeg = 45.0f;
    r.Go(6, t);
    EXPECT_EQ((std::set<int>{ 11 }), Ran(r.gen));
    ASSERT_NE(nullptr, F(r.gen, "latitude"));
    EXPECT_NEAR(45.0f / 90.0f, F(r.gen, "latitude")[0], 1e-6);
    t.latitudeSouthDeg = -5.0f;
    r.Go(6, t);
    EXPECT_EQ((std::set<int>{ 11 }), Ran(r.gen));
    t.temperatureOffset = 0.1f;   // Feature 8 stage: no implemented stage re-runs
    r.Go(6, t);
    EXPECT_TRUE(Ran(r.gen).empty());
}

TEST(TerrainLatitude, DegenerateGridsDoNotDivideByZero)
{
    for (GridDimensions d : { GridDimensions{ 1, 1 }, GridDimensions{ 1, 5 }, GridDimensions{ 7, 1 }, GridDimensions{ 2, 1 } })
    {
        std::vector<float> out(static_cast<size_t>(d.width) * d.height, -1.0f);
        ComputeStage11(d, 60.0f, 20.0f, out.data());
        for (float v : out) { EXPECT_TRUE(std::isfinite(v)); EXPECT_GE(v, 0.0f); EXPECT_LE(v, 1.0f); }
        EXPECT_NEAR(60.0f / 90.0f, out[0], 1e-6) << d.width << "x" << d.height;
    }
    ComputeStage11(GridDimensions{ 0, 0 }, 60.0f, 20.0f, nullptr);
    SUCCEED();
}

TEST(TerrainLatitude, MaxCentreYMatchesStageZeroDefinition)
{
    for (GridDimensions d : { GridDimensions{ 66, 42 }, GridDimensions{ 1, 9 }, GridDimensions{ 2, 2 }, GridDimensions{ 5, 3 } })
    {
        float mx = 0.0f;
        for (int y = 0; y < d.height; ++y) { for (int x = 0; x < d.width; ++x) { mx = std::max(mx, CentreY(x, y)); } }
        EXPECT_FLOAT_EQ(mx, MaxCentreY(d)) << d.width << "x" << d.height;
    }
    EXPECT_FLOAT_EQ(1.0f, MaxCentreY(GridDimensions{ 1, 1 }));
}

TEST(TerrainLatitude, GeneratorWorksOnTinyGrids)
{
    Gen r;
    r.Go(1, WorldShapeParams{}, GridDimensions{ 1, 1 });
    ASSERT_NE(nullptr, F(r.gen, "latitude"));
    EXPECT_NEAR(60.0f / 90.0f, F(r.gen, "latitude")[0], 1e-6);
    EXPECT_LT(U(r.gen, "terrainType")[0], static_cast<uint32_t>(TerrainType::Count));
}

// ---------------------------------------------------------------------------
// Wide sweeps
// ---------------------------------------------------------------------------
TEST(SLOW_TerrainClassify, RealWorldInvariantsOver20Seeds)
{
    for (uint64_t seed = 1; seed <= 20; ++seed)
    {
        Gen r;
        r.Go(seed, WorldShapeParams{});
        EXPECT_EQ(0, CheckWorld(r.gen, "seed " + std::to_string(seed)));
    }
}

// ---------------------------------------------------------------------------
// Timing (informational, Release only)
// ---------------------------------------------------------------------------
TEST(TerrainPerf, StagesTenElevenAndTotalReport)
{
#ifdef NDEBUG
    constexpr int kRuns = 50;
    std::vector<double> t10, t11, total;
    Gen r;
    const WorldShapeParams s;
    for (int run = 0; run < kRuns; ++run)
    {
        r.Go(4000 + static_cast<uint64_t>(run), s);
        t10.push_back(r.gen.GetTimings()[static_cast<int>(TerrainStage::Classify)].milliseconds);
        t11.push_back(r.gen.GetTimings()[static_cast<int>(TerrainStage::Latitude)].milliseconds);
        total.push_back(r.gen.GetLastTotalMs());
    }
    for (auto* v : { &t10, &t11, &total }) { std::sort(v->begin(), v->end()); }
    std::printf("[TerrainPerf] stage 10 Classify median %.3f ms, max %.3f ms; stage 11 Latitude median %.3f ms, max %.3f ms\n",
                t10[t10.size() / 2], t10.back(), t11[t11.size() / 2], t11.back());
    std::printf("[TerrainPerf] stages 0-11 total median %.3f ms, max %.3f ms (66x42, %d fresh seeds)\n",
                total[total.size() / 2], total.back(), kRuns);
    SUCCEED();
#else
    GTEST_SKIP() << "Release-only timing report";
#endif
}

} // namespace
