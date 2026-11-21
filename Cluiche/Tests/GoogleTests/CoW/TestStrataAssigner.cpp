// Generic strata task 2: the IStrataAssigner seam (registry, contract wrapper, selection by data) and the
// coast_cost_bands banding generalised to N strata (hand-built grids, 1, 2 and 5 strata, 3-strata parity).
// Spec: docs/specs/features/draoi/generic-strata.md (ACs 1-3, 6).
#include <gtest/gtest.h>

#include <DraoiHexWorld/Testing/BiomeTestRegistry.h>
#include <Shared/WorldGeneration/StrataAssigner.h>
#include <Shared/WorldGeneration/TerrainGenerator.h>
#include <Shared/WorldGeneration/Testing/TerrainRulesTestHelpers.h>

#include <DiaPathfinding/HexTopology.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

using namespace CoW::WorldGeneration;
using namespace CoW::World;
using namespace CoW::World::Testing;
using Dia::Maths::RandomStream;
using Dia::Pathfinding::HexTopology;
using Dia::Pathfinding::kNoCell;

namespace {

// ---------------------------------------------------------------------------
// Test doubles
// ---------------------------------------------------------------------------
// Writes whatever value the test chose at one cell, a fixed band elsewhere.
class FixedAssigner : public IStrataAssigner
{
public:
    int           badCell = -1;
    StratumIndex  badValue = 0;
    void Assign(const StrataAssignContext& ctx, const StratumSetSpec&, std::span<StratumIndex> out) override
    {
        for (size_t i = 0; i < out.size(); ++i) { out[i] = ctx.landmass[i] != 0u ? 1 : 0; }
        if (badCell >= 0) { out[static_cast<size_t>(badCell)] = badValue; }
    }
};

// Three vertical bands by column; ignores costs entirely.
class ColumnBandsAssigner : public IStrataAssigner
{
public:
    int calls = 0;
    void Assign(const StrataAssignContext& ctx, const StratumSetSpec& set, std::span<StratumIndex> out) override
    {
        ++calls;
        const int w = ctx.dims.width;
        for (size_t i = 0; i < out.size(); ++i)
        {
            const int x = static_cast<int>(i) % w;
            out[i] = ctx.landmass[i] == 0u ? 0 : static_cast<StratumIndex>(1 + std::min<int>(x * static_cast<int>(set.count) / w, static_cast<int>(set.count) - 1));
        }
    }
};

// ---------------------------------------------------------------------------
// Hand-built grid (same scheme as TestTerrainStrata): ocean except the painted blocks.
// ---------------------------------------------------------------------------
struct Grid
{
    GridDimensions        dims;
    HexTopology           topo;
    int                   n;
    std::vector<uint32_t> land, coast, river, zone;
    std::vector<float>    ridge, barrier, base, cost;
    Stage8Debug           dbg;
    TerrainRules          rules = TestRules();

    Grid(int w, int h) : dims{ w, h }, topo(w, h), n(w * h), land(static_cast<size_t>(n), 0u), coast(static_cast<size_t>(n), 0u),
        river(static_cast<size_t>(n), 0u), zone(static_cast<size_t>(n), 99u),
        ridge(static_cast<size_t>(n), 0.0f), barrier(static_cast<size_t>(n), 0.0f),
        base(static_cast<size_t>(n), -1.0f), cost(static_cast<size_t>(n), -1.0f) {}

    int I(int x, int y) const { return y * dims.width + x; }
    void Block(int x0, int y0, int x1, int y1, uint32_t id)
    {
        for (int y = y0; y <= y1; ++y) { for (int x = x0; x <= x1; ++x) { land[I(x, y)] = id; } }
    }

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

    void Run(const StratumSetSpec& set, int mainId = 1)
    {
        ComputeCoast();
        ComputeStage8(rules, set, dims, 0.6f, mainId, land.data(), coast.data(), ridge.data(), barrier.data(), river.data(),
                      Stage8Layers{ base.data(), cost.data(), zone.data() }, dbg);
    }
};

// A set of N strata with the given shares; optional rules per stratum (1-based index into the vectors).
StratumSetSpec MakeSet(std::vector<float> shares, std::vector<int> mainOnly = {}, std::vector<int> minDepth = {})
{
    StratumSetSpec set;
    std::snprintf(set.id, sizeof(set.id), "test");
    set.count = static_cast<uint32_t>(shares.size());
    for (size_t i = 0; i < shares.size(); ++i)
    {
        std::snprintf(set.strata[i].name, sizeof(set.strata[i].name), "S%zu", i + 1);
        set.strata[i].share = shares[i];
        set.strata[i].mainLandmassOnly = i < mainOnly.size() && mainOnly[i] != 0;
        set.strata[i].minCoastDepth = i < minDepth.size() ? minDepth[i] : 0;
    }
    return set;
}

// The sorted base costs of the main landmass.
std::vector<float> MainCosts(const Grid& g, uint32_t main = 1)
{
    std::vector<float> s;
    for (int i = 0; i < g.n; ++i) { if (g.land[i] == main) { s.push_back(g.base[i]); } }
    std::sort(s.begin(), s.end());
    return s;
}

int CountAtMost(const Grid& g, uint32_t k)
{
    int c = 0;
    for (int i = 0; i < g.n; ++i) { if (g.land[i] != 0u && g.zone[i] <= k) { ++c; } }
    return c;
}

// Every land cell in 1..N, ocean 0.
void ExpectRange(const Grid& g, uint32_t count)
{
    for (int i = 0; i < g.n; ++i)
    {
        if (g.land[i] == 0u) { EXPECT_EQ(g.zone[i], 0u) << "ocean cell " << i; }
        else { EXPECT_TRUE(g.zone[i] >= 1u && g.zone[i] <= count) << "land cell " << i << " stratum " << g.zone[i]; }
    }
}

// Without per-stratum rules a higher cost is never in a lower stratum.
void ExpectMonotone(const Grid& g)
{
    for (int a = 0; a < g.n; ++a)
    {
        if (g.land[a] == 0u) { continue; }
        for (int b = 0; b < g.n; ++b)
        {
            if (g.land[b] != 0u && g.cost[a] < g.cost[b]) { ASSERT_LE(g.zone[a], g.zone[b]) << a << " vs " << b; }
        }
    }
}

StrataAssignContext MakeContext(const WorldStructure& w, const TerrainRules& rules, GridDimensions dims, const std::vector<uint32_t>& land)
{
    StrataAssignContext ctx{ w, rules };
    ctx.dims = dims;
    ctx.landmass = land.data();
    return ctx;
}

} // namespace

// ---------------------------------------------------------------------------
// Registry
// ---------------------------------------------------------------------------
TEST(StrataAssignerRegistry, BuiltInIsRegisteredByDefault)
{
    StrataAssignerRegistry r;
    EXPECT_TRUE(r.Has(kBuiltinAssignerType));
    EXPECT_NE(r.Find(kBuiltinAssignerType), nullptr);
    EXPECT_EQ(r.Find("nope"), nullptr);
    EXPECT_FALSE(r.Has("nope"));
}

TEST(StrataAssignerRegistry, RegisterFindAndRefusals)
{
    StrataAssignerRegistry r;
    FixedAssigner a, b;
    EXPECT_TRUE(r.Register("fixed", &a));
    EXPECT_EQ(r.Find("fixed"), &a);
    EXPECT_FALSE(r.Register("fixed", &b));              // duplicate name
    EXPECT_EQ(r.Find("fixed"), &a);                     // unchanged
    EXPECT_FALSE(r.Register(kBuiltinAssignerType, &b)); // the built-in name is taken
    EXPECT_FALSE(r.Register("other", nullptr));
    EXPECT_FALSE(r.Register("", &b));
    EXPECT_FALSE(r.Register("a_name_that_is_much_longer_than_thirty_one_characters", &b));
}

TEST(StrataAssignerRegistry, TypeCheckKnowsRegisteredNames)
{
    StrataAssignerRegistry r;
    FixedAssigner a;
    r.Register("fixed", &a);
    const AssignerTypeCheck check = r.TypeCheck();
    EXPECT_TRUE(check("fixed"));
    EXPECT_TRUE(check(kBuiltinAssignerType));
    EXPECT_FALSE(check("unknown"));
}

TEST(StrataAssignerRegistry, TableLoadAcceptsRegisteredTypeAndRefusesUnknownOne)
{
    std::string text = Detail::ReadFileText(ShippedStrataPath());
    const std::string sidecar = Detail::ReadFileText(ShippedStratumSidecarPath());
    const size_t at = text.find("coast_cost_bands");
    ASSERT_NE(at, std::string::npos);
    text.replace(at, std::strlen("coast_cost_bands"), "fixed");

    StratumError err;
    StratumTable plain;
    EXPECT_FALSE(plain.LoadFromText(text.c_str(), sidecar.c_str(), err));   // default: built-in only
    EXPECT_NE((err.field + err.reason).find("fixed"), std::string::npos);

    StrataAssignerRegistry r;
    FixedAssigner a;
    StratumTable table;
    EXPECT_FALSE(table.LoadFromText(text.c_str(), sidecar.c_str(), err, r.TypeCheck()));   // not registered yet
    r.Register("fixed", &a);
    ASSERT_TRUE(table.LoadFromText(text.c_str(), sidecar.c_str(), err, r.TypeCheck())) << err.field << " " << err.reason;
    EXPECT_STREQ(table.Depth()->assignerType, "fixed");
}

// ---------------------------------------------------------------------------
// Contract wrapper (AC 6)
// ---------------------------------------------------------------------------
namespace {
struct ContractFixture
{
    WorldStructure        world;
    TerrainRules          rules = TestRules();
    GridDimensions        dims{ 4, 3 };
    std::vector<uint32_t> land = std::vector<uint32_t>(12, 0u);
    std::vector<StratumIndex> out = std::vector<StratumIndex>(12, 0);
    StratumSetSpec        set = MakeSet({ 0.5f, 0.5f });
    StrataAssignError     err;
    FixedAssigner         assigner;
    ContractFixture() { land[5] = 1u; land[6] = 1u; land[9] = 1u; }
    bool Run()
    {
        StrataAssignContext ctx = MakeContext(world, rules, dims, land);
        return RunStrataAssigner("fixed", assigner, ctx, set, std::span<StratumIndex>(out.data(), out.size()), err);
    }
};
}

TEST(StrataAssignerContract, ValidOutputPasses)
{
    ContractFixture f;
    EXPECT_TRUE(f.Run());
    EXPECT_EQ(f.out[5], 1);
    EXPECT_EQ(f.out[0], 0);
}

TEST(StrataAssignerContract, ValueAboveNIsRejectedNamingAssignerAndCell)
{
    ContractFixture f;
    f.assigner.badCell = 6;
    f.assigner.badValue = 3;   // the set has 2 strata
    EXPECT_FALSE(f.Run());
    EXPECT_EQ(f.err.assigner, "fixed");
    EXPECT_EQ(f.err.cell, 6);
    EXPECT_EQ(f.err.value, 3u);
    EXPECT_NE(f.err.reason.find("fixed"), std::string::npos);
    EXPECT_NE(f.err.reason.find("cell 6"), std::string::npos);
}

TEST(StrataAssignerContract, NonZeroOceanIsRejected)
{
    ContractFixture f;
    f.assigner.badCell = 2;   // ocean
    f.assigner.badValue = 1;
    EXPECT_FALSE(f.Run());
    EXPECT_EQ(f.err.cell, 2);
}

TEST(StrataAssignerContract, ZeroOnLandIsRejected)
{
    ContractFixture f;
    f.assigner.badCell = 9;   // land
    f.assigner.badValue = 0;
    EXPECT_FALSE(f.Run());
    EXPECT_EQ(f.err.cell, 9);
}

TEST(StrataAssignerContract, FirstOffendingCellIsReported)
{
    ContractFixture f;
    f.assigner.badCell = 9;
    f.assigner.badValue = 0;
    f.land[5] = 0u;   // cell 5 is ocean and the assigner writes 0 there (fine); cell 9 is the first and only offender
    EXPECT_FALSE(f.Run());
    EXPECT_EQ(f.err.cell, 9);
}

// ---------------------------------------------------------------------------
// Selection by name through the pipeline (AC 6)
// ---------------------------------------------------------------------------
namespace {
struct CustomPipeline
{
    StrataAssignerRegistry assigners;
    ColumnBandsAssigner    columns;
    BiomeRegistry          biomes;
    StratumTable           table;

    CustomPipeline()
    {
        assigners.Register("column_bands", &columns);
        std::string text = Detail::ReadFileText(ShippedStrataPath());
        text.replace(text.find("coast_cost_bands"), std::strlen("coast_cost_bands"), "column_bands");
        StratumError err;
        if (!table.LoadFromText(text.c_str(), Detail::ReadFileText(ShippedStratumSidecarPath()).c_str(), err, assigners.TypeCheck()))
        {
            ADD_FAILURE() << "strata load: " << err.field << " " << err.reason;
        }
        RegistryError rerr;
        if (!biomes.LoadFromText(Detail::ReadFileText(ShippedBiomesPath()).c_str(), table, rerr))
        {
            ADD_FAILURE() << "biomes load: " << rerr.field.AsCStr() << " " << rerr.reason.AsCStr();
        }
    }
};
}

TEST(StrataAssignerPipeline, DataSelectedCustomAssignerProducesTheStrata)
{
    CustomPipeline p;
    TerrainGenerator gen(TestRules(), &p.biomes);
    gen.SetAssignerRegistry(&p.assigners);
    RandomStream rng(1);
    const GridDimensions dims{ 66, 42 };
    gen.Update(5, WorldShapeParams{}, dims, rng);

    EXPECT_GT(p.columns.calls, 0);
    EXPECT_TRUE(gen.GetStrataError().empty()) << gen.GetStrataError();
    const uint32_t* zone = gen.GetLayers().GetUInt(gen.GetLayers().FindLayer("stratum"));
    const uint32_t* land = gen.GetLayers().GetUInt(gen.GetLayers().FindLayer("landmass"));
    int bands[4] = {};
    for (int i = 0; i < dims.width * dims.height; ++i)
    {
        const int x = i % dims.width;
        const uint32_t expect = land[i] == 0u ? 0u : static_cast<uint32_t>(1 + std::min(x * 3 / dims.width, 2));
        ASSERT_EQ(zone[i], expect) << "cell " << i;
        ++bands[zone[i]];
    }
    EXPECT_GT(bands[1], 0);
    EXPECT_GT(bands[2], 0);
    EXPECT_GT(bands[3], 0);
}

TEST(StrataAssignerPipeline, ViolatingAssignerIsReportedAndBuiltInStillProducesAWorld)
{
    StrataAssignerRegistry assigners;
    FixedAssigner bad;
    bad.badCell = 700;
    bad.badValue = 9;
    assigners.Register("column_bands", &bad);
    std::string text = Detail::ReadFileText(ShippedStrataPath());
    text.replace(text.find("coast_cost_bands"), std::strlen("coast_cost_bands"), "column_bands");
    StratumTable table;
    StratumError err;
    ASSERT_TRUE(table.LoadFromText(text.c_str(), Detail::ReadFileText(ShippedStratumSidecarPath()).c_str(), err, assigners.TypeCheck()));
    BiomeRegistry biomes;
    RegistryError rerr;
    ASSERT_TRUE(biomes.LoadFromText(Detail::ReadFileText(ShippedBiomesPath()).c_str(), table, rerr));

    TerrainGenerator gen(TestRules(), &biomes);
    gen.SetAssignerRegistry(&assigners);
    RandomStream rng(1);
    gen.Update(5, WorldShapeParams{}, GridDimensions{ 66, 42 }, rng);
    EXPECT_NE(gen.GetStrataError().find("column_bands"), std::string::npos) << gen.GetStrataError();
    EXPECT_NE(gen.GetStrataError().find("cell 700"), std::string::npos) << gen.GetStrataError();
    const uint32_t* zone = gen.GetLayers().GetUInt(gen.GetLayers().FindLayer("stratum"));
    const uint32_t* land = gen.GetLayers().GetUInt(gen.GetLayers().FindLayer("landmass"));
    for (int i = 0; i < 66 * 42; ++i)
    {
        if (land[i] != 0u) { EXPECT_TRUE(zone[i] >= 1 && zone[i] <= 3) << i; } else { EXPECT_EQ(zone[i], 0u); }
    }
}

TEST(StrataAssignerPipeline, DefaultGeneratorUsesTheBuiltInAndReportsNoError)
{
    TerrainGenerator gen(TestRules(), &ShippedBiomeRegistry());
    RandomStream rng(1);
    gen.Update(3, WorldShapeParams{}, GridDimensions{ 66, 42 }, rng);
    EXPECT_TRUE(gen.GetStrataError().empty());
    EXPECT_GT(gen.GetStage8Debug().mainCells, 0);
}

// ---------------------------------------------------------------------------
// coast_cost_bands over N strata
// ---------------------------------------------------------------------------
TEST(CoastCostBands, ThreeStrataSpecOverloadMatchesTableOverload)
{
    for (int variant = 0; variant < 2; ++variant)
    {
        Grid a(30, 22), b(30, 22);
        for (Grid* g : { &a, &b })
        {
            g->Block(2, 2, 20, 19, 1u);
            if (variant == 1) { g->Block(24, 2, 28, 8, 2u); g->ridge[g->I(10, 10)] = 0.5f; g->river[g->I(8, 9)] = 1u; }
        }
        a.ComputeCoast();
        b.ComputeCoast();
        const StratumTable& table = ShippedStratumTable();
        std::vector<uint32_t> zoneA(static_cast<size_t>(a.n));   // (kept as uint32 layer values)
        ComputeStage8(a.rules, table, a.dims, 0.6f, 1, a.land.data(), a.coast.data(), a.ridge.data(), a.barrier.data(), a.river.data(),
                      Stage8Layers{ a.base.data(), a.cost.data(), zoneA.data() }, a.dbg);
        b.Run(*table.Depth());
        for (int i = 0; i < a.n; ++i) { EXPECT_EQ(zoneA[i], b.zone[i]) << variant << " cell " << i; }
        EXPECT_EQ(a.dbg.t1, b.dbg.t1);
        EXPECT_EQ(a.dbg.t2, b.dbg.t2);
        EXPECT_EQ(a.dbg.hasAncient, b.dbg.hasAncient);
    }
}

TEST(CoastCostBands, OneStratumHoldsEveryLandCell)
{
    Grid g(24, 18);
    g.Block(2, 2, 21, 15, 1u);
    g.Block(23, 2, 23, 3, 2u);
    g.Run(MakeSet({ 1.0f }, { 1 }, { 4 }));   // a lone stratum ignores its own rules: land is never 0
    ExpectRange(g, 1);
    for (int i = 0; i < g.n; ++i) { if (g.land[i] != 0u) { EXPECT_EQ(g.zone[i], 1u); } }
}

TEST(CoastCostBands, TwoStrataSplitByShareOnTheStrip)
{
    Grid g(10, 5);
    g.Block(1, 1, 8, 3, 1u);   // 18 coast cells at cost 1, 6 cells at cost 2
    g.Run(MakeSet({ 0.5f, 0.5f }));
    ExpectRange(g, 2);
    // m = 24: the last stratum starts at sorted[24 - ceil(12)] = 1, which is not above the baseline 1, so it moves to the next
    // distinct cost, 2: the six cost-2 cells are stratum 2.
    EXPECT_TRUE(g.dbg.hasAncient);
    EXPECT_FLOAT_EQ(g.dbg.t2, 2.0f);
    EXPECT_EQ(CountAtMost(g, 1), 18);
    ExpectMonotone(g);
}

TEST(CoastCostBands, TwoStrataWithNoDistinctCostCollapseToOne)
{
    Grid g(10, 3);
    g.Block(1, 1, 8, 1, 1u);   // a one-row island: every cell is coast, cost 1
    g.Run(MakeSet({ 0.5f, 0.5f }));
    ExpectRange(g, 2);
    EXPECT_FALSE(g.dbg.hasAncient);
    EXPECT_EQ(CountAtMost(g, 1), 8);
}

TEST(CoastCostBands, FiveStrataFollowCumulativeShares)
{
    Grid g(44, 32);
    g.Block(2, 2, 41, 29, 1u);   // flat continent: cost == coast distance, rings 1..14
    g.Run(MakeSet({ 0.2f, 0.2f, 0.2f, 0.2f, 0.2f }));
    ExpectRange(g, 5);
    ExpectMonotone(g);

    const std::vector<float> s = MainCosts(g);
    const int m = static_cast<int>(s.size());
    auto endCut = [&](double cum) { return s[static_cast<size_t>(std::max(0, static_cast<int>(std::floor(cum * m)) - 1))]; };
    const float e1 = endCut(static_cast<double>(0.2f));
    float e2 = endCut(static_cast<double>(0.2f) + static_cast<double>(0.2f));
    float e3 = endCut(static_cast<double>(0.2f) + static_cast<double>(0.2f) + static_cast<double>(0.2f));
    if (e2 <= e1) { e2 = *std::upper_bound(s.begin(), s.end(), e1); }
    if (e3 <= e2) { e3 = *std::upper_bound(s.begin(), s.end(), e2); }
    float last = s[static_cast<size_t>(m - static_cast<int>(std::ceil(static_cast<double>(0.2f) * m)))];
    if (last <= e3) { last = *std::upper_bound(s.begin(), s.end(), e3); }
    for (int i = 0; i < g.n; ++i)
    {
        if (g.land[i] == 0u) { continue; }
        const float c = g.cost[i];
        const uint32_t expect = c <= e1 ? 1u : c <= e2 ? 2u : c <= e3 ? 3u : c >= last ? 5u : 4u;
        EXPECT_EQ(g.zone[i], expect) << "cell " << i << " cost " << c;
    }
    // Every stratum is populated on a deep continent, and the cumulative shares hold at each cut.
    for (uint32_t k = 1; k <= 5; ++k)
    {
        EXPECT_GT(CountAtMost(g, k), CountAtMost(g, k - 1)) << "stratum " << k << " empty";
    }
    EXPECT_GE(CountAtMost(g, 1), static_cast<int>(std::floor(0.2 * m)));
    EXPECT_GE(CountAtMost(g, 2), static_cast<int>(std::floor(0.4 * m)));
    EXPECT_GE(CountAtMost(g, 3), static_cast<int>(std::floor(0.6 * m)));
}

TEST(CoastCostBands, ZeroShareMiddleStratumStaysEmpty)
{
    Grid g(44, 32);
    g.Block(2, 2, 41, 29, 1u);
    g.Run(MakeSet({ 0.5f, 0.0f, 0.25f, 0.25f }));
    ExpectRange(g, 4);
    for (int i = 0; i < g.n; ++i) { if (g.land[i] != 0u) { EXPECT_NE(g.zone[i], 2u) << i; } }
    ExpectMonotone(g);
}

TEST(CoastCostBands, CollapsedThresholdsLeaveInnerStrataEmptyNotUnassigned)
{
    Grid g(10, 3);
    g.Block(1, 1, 8, 1, 1u);   // every cost is 1: no distinct cost above the first cut
    g.Run(MakeSet({ 0.25f, 0.25f, 0.25f, 0.25f }));
    ExpectRange(g, 4);
    for (int i = 0; i < g.n; ++i) { if (g.land[i] != 0u) { EXPECT_EQ(g.zone[i], 1u); } }
    EXPECT_FALSE(g.dbg.hasAncient);
}

TEST(CoastCostBands, PerStratumRulesFallToThePreviousStratum)
{
    Grid g(44, 32);
    g.Block(2, 2, 30, 29, 1u);    // main continent
    g.Block(34, 4, 42, 27, 2u);   // a second deep landmass
    // Stratum 3 is main-landmass-only; stratum 5 is main-only and needs coast distance >= 4.
    g.Run(MakeSet({ 0.2f, 0.2f, 0.2f, 0.2f, 0.2f }, { 0, 0, 1, 0, 1 }, { 0, 0, 0, 0, 4 }));
    ExpectRange(g, 5);
    for (int i = 0; i < g.n; ++i)
    {
        if (g.land[i] == 0u) { continue; }
        if (g.land[i] != 1u) { EXPECT_NE(g.zone[i], 3u) << "non-main stratum 3 at " << i; EXPECT_NE(g.zone[i], 5u) << "non-main stratum 5 at " << i; }
        if (g.zone[i] == 5u) { EXPECT_GE(g.coast[i], 4u) << i; }
    }
    int fives = 0;
    for (int i = 0; i < g.n; ++i) { fives += g.zone[i] == 5u ? 1 : 0; }
    EXPECT_GT(fives, 0);
}

TEST(CoastCostBands, NoMainLandmassKeepsTheCoastRingFirst)
{
    Grid g(24, 18);
    g.Block(2, 2, 21, 15, 1u);
    g.Run(MakeSet({ 0.3f, 0.3f, 0.2f, 0.2f }), 0);   // no main landmass: thresholds fall back to cost 1
    ExpectRange(g, 4);
    EXPECT_FLOAT_EQ(g.dbg.t1, 1.0f);
    EXPECT_EQ(g.dbg.mainCells, 0);
    for (int i = 0; i < g.n; ++i)
    {
        if (g.land[i] != 0u) { EXPECT_EQ(g.zone[i], g.cost[i] <= 1.0f ? 1u : 3u) << i; }   // beyond the cuts: the stratum before the last
    }
}

TEST(CoastCostBands, BuiltInAssignerThroughTheWrapperMeetsTheContract)
{
    Grid g(30, 22);
    g.Block(2, 2, 20, 19, 1u);
    g.ComputeCoast();
    StrataAssignerRegistry registry;
    const StratumTable& table = ShippedStratumTable();
    WorldStructure world;
    StrataAssignContext ctx{ world, g.rules };
    ctx.dims = g.dims;
    ctx.barrier = 0.6f;
    ctx.mainLandmass = 1;
    ctx.landmass = g.land.data();
    ctx.coastDistance = g.coast.data();
    ctx.ridgeMask = g.ridge.data();
    ctx.barrierMask = g.barrier.data();
    ctx.riverMask = g.river.data();
    ctx.zoneCostBase = g.base.data();
    ctx.zoneCost = g.cost.data();
    ctx.debug = &g.dbg;
    ComputeStrataCosts(g.rules, g.dims, 0.6f, g.land.data(), g.coast.data(), g.ridge.data(), g.barrier.data(), g.river.data(), g.base.data(), g.cost.data());
    StrataAssignError err;
    std::vector<StratumIndex> out(static_cast<size_t>(g.n));
    ASSERT_TRUE(RunStrataAssigner(kBuiltinAssignerType, *registry.Find(kBuiltinAssignerType), ctx, *table.Depth(),
                                  std::span<StratumIndex>(out.data(), out.size()), err)) << err.reason;
    EXPECT_GT(g.dbg.mainCells, 0);
}
