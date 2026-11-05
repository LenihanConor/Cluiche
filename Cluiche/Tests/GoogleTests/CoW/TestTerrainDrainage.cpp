// S-1 Terrain Generator task 8: Stage 5 Drainage (filledElevation, basinDepth, basinRank,
// flowDirection, flowAccumulation). Hand-built grids check the algorithm exactly through
// ComputeStage5; generator runs check the invariants on real terrain. TerrainPerf (Release only)
// reports the stage 5 time at Small.
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

// A hand-built grid: elevation + ocean mask in, the five layers + debug out.
struct Grid
{
    GridDimensions       dims;
    std::vector<float>   elev;
    std::vector<uint8_t> ocean;
    std::vector<float>   filled, depth, acc;
    std::vector<uint32_t> rank, dir;
    Stage5Debug          dbg;

    Grid(int w, int h, float landElev) : dims{ w, h },
        elev(static_cast<size_t>(w * h), landElev), ocean(static_cast<size_t>(w * h), 0) {}

    int  N() const { return dims.width * dims.height; }
    int  I(int x, int y) const { return y * dims.width + x; }
    void Ocean(int i) { ocean[i] = 1; elev[i] = 0.0f; }
    void OceanBorder()
    {
        for (int y = 0; y < dims.height; ++y)
        {
            for (int x = 0; x < dims.width; ++x)
            {
                if (x == 0 || y == 0 || x == dims.width - 1 || y == dims.height - 1) { Ocean(I(x, y)); }
            }
        }
    }
    void Run(const TerrainRules& rules = TerrainRules::Defaults())
    {
        const size_t n = static_cast<size_t>(N());
        filled.assign(n, -1.0f); depth.assign(n, -1.0f); acc.assign(n, -1.0f);
        rank.assign(n, 99u); dir.assign(n, 99u);
        Stage5Layers l;
        l.filledElevation = filled.data();
        l.basinDepth = depth.data();
        l.basinRank = rank.data();
        l.flowDirection = dir.data();
        l.flowAccumulation = acc.data();
        ComputeStage5(rules, dims, elev.data(), ocean.data(), l, dbg);
    }
};

// Every invariant of stage 5 on one result. Returns the number of failures (for ASSERT_EQ(0, ...)).
int CheckInvariants(GridDimensions dims, const float* elev, const uint8_t* ocean, const float* filled,
                    const float* depth, const uint32_t* rank, const uint32_t* dir, const float* acc,
                    const Stage5Debug& dbg, const std::string& ctx)
{
    const TerrainRules rules = TerrainRules::Defaults();
    const int n = dims.width * dims.height;
    const HexTopology topo(dims.width, dims.height);
    int fails = 0;
    auto fail = [&](const std::string& what, int i) { if (fails < 10) { ADD_FAILURE() << ctx << ": " << what << " at cell " << i; } ++fails; };

    if (static_cast<int>(dbg.fillLevel.size()) != n) { fail("fillLevel size", -1); return fails; }

    // Fill, depth, flow direction.
    for (int i = 0; i < n; ++i)
    {
        if (ocean[i] != 0)
        {
            if (filled[i] != elev[i]) { fail("ocean filled != raw", i); }
            if (depth[i] != 0.0f) { fail("ocean depth", i); }
            if (rank[i] != 0u) { fail("ocean rank", i); }
            if (dir[i] != kFlowSink) { fail("ocean not a sink", i); }
            if (acc[i] != 0.0f) { fail("ocean acc", i); }
            continue;
        }
        if (!(filled[i] >= elev[i])) { fail("filled < raw", i); }
        if (!(filled[i] >= dbg.fillLevel[i])) { fail("filled < level", i); }
        const float d = dbg.fillLevel[i] - elev[i];
        const float expectDepth = d >= kBasinDepthFloor ? d : 0.0f;
        if (depth[i] != expectDepth) { fail("depth != level - raw", i); }

        // Largest drop, ties by lowest d, strictly lower.
        uint32_t best = kFlowSink;
        float bestDrop = 0.0f;
        for (uint32_t k = 0; k < 6; ++k)
        {
            const uint32_t nb = topo.NeighbourAt(static_cast<uint32_t>(i), k);
            if (nb == kNoCell) { continue; }
            const float drop = filled[i] - filled[nb];
            if (drop > bestDrop) { bestDrop = drop; best = k; }
        }
        if (best == kFlowSink) { fail("land cell with no lower neighbour", i); continue; }
        if (dir[i] != best) { fail("flow direction is not the largest drop / lowest d", i); continue; }
        const uint32_t down = topo.NeighbourAt(static_cast<uint32_t>(i), dir[i]);
        if (!(filled[down] < filled[i])) { fail("flow not strictly descending", i); }
        // Accumulation: downstream land >= upstream, own contribution included.
        const float own = 1.0f + rules.kHighlandRain() * elev[i];
        if (!(acc[i] >= own * (1.0f - 1e-6f))) { fail("acc below own contribution", i); }
        if (ocean[down] == 0 && !(acc[down] >= acc[i])) { fail("acc falls downstream", i); }
    }

    // Conservation: everything arriving at the sea equals all rain on land.
    double rain = 0.0, mouths = 0.0;
    for (int i = 0; i < n; ++i)
    {
        if (ocean[i] != 0) { continue; }
        rain += 1.0 + static_cast<double>(rules.kHighlandRain()) * elev[i];
        if (dir[i] < 6u && ocean[topo.NeighbourAt(static_cast<uint32_t>(i), dir[i])] != 0) { mouths += acc[i]; }
    }
    if (std::fabs(rain - mouths) > 1e-4 * std::max(1.0, rain)) { fail("conservation: rain " + std::to_string(rain) + " mouths " + std::to_string(mouths), -1); }

    // Basins: components of depth > 0, cells disjoint, adjacent depth cells share a basin.
    std::vector<int> owner(static_cast<size_t>(n), -1);
    for (size_t b = 0; b < dbg.basins.size(); ++b)
    {
        const Basin& basin = dbg.basins[b];
        if (basin.cells.empty()) { fail("empty basin", -1); continue; }
        if (basin.firstCell != basin.cells.front()) { fail("firstCell", basin.firstCell); }
        if (!std::is_sorted(basin.cells.begin(), basin.cells.end())) { fail("cells not ascending", basin.firstCell); }
        if (b > 0 && !(dbg.basins[b - 1].firstCell < basin.firstCell)) { fail("basins not ordered by firstCell", basin.firstCell); }
        float maxDepth = 0.0f, volume = 0.0f;
        for (int c : basin.cells)
        {
            if (owner[c] != -1) { fail("cell in two basins", c); }
            owner[c] = static_cast<int>(b);
            if (ocean[c] != 0) { fail("ocean cell in a basin", c); }
            if (!(depth[c] > 0.0f)) { fail("basin cell without depth", c); }
            if (dbg.fillLevel[c] != basin.spillHeight) { fail("basin cell level != spill height", c); }
            maxDepth = std::max(maxDepth, depth[c]);
            volume += depth[c];
        }
        if (maxDepth != basin.maxDepth) { fail("maxDepth", basin.firstCell); }
        if (volume != basin.volume) { fail("volume", basin.firstCell); }
        const bool eligible = basin.maxDepth >= rules.kMinBasinDepth() && basin.cells.size() >= 2;
        if (eligible != basin.eligible) { fail("eligibility", basin.firstCell); }
        if (basin.spillCell < 0 || basin.spillCell >= n) { fail("spill cell range", basin.firstCell); }
        else if (std::find(basin.cells.begin(), basin.cells.end(), basin.spillCell) != basin.cells.end())
        {
            fail("spill cell inside its basin", basin.spillCell);
        }
        for (int c : basin.cells)
        {
            if (rank[c] != basin.rank) { fail("basinRank layer != basin rank", c); }
        }
    }
    for (int i = 0; i < n; ++i)
    {
        if (depth[i] > 0.0f && owner[i] == -1) { fail("depth cell in no basin", i); }
        if (owner[i] == -1 && rank[i] != 0u) { fail("rank outside basins", i); }
        if (depth[i] <= 0.0f) { continue; }
        for (uint32_t k = 0; k < 6; ++k)
        {
            const uint32_t nb = topo.NeighbourAt(static_cast<uint32_t>(i), k);
            if (nb != kNoCell && depth[nb] > 0.0f && owner[nb] != owner[i]) { fail("adjacent basin cells in different basins", i); }
        }
    }

    // Ranking: eligible basins only, volume desc then smallest index, 1-based.
    int eligibleCount = 0;
    for (const Basin& b : dbg.basins) { eligibleCount += b.eligible ? 1 : 0; }
    if (eligibleCount != dbg.eligibleBasins || static_cast<int>(dbg.ranked.size()) != eligibleCount) { fail("eligible count", -1); }
    for (size_t r = 0; r < dbg.ranked.size(); ++r)
    {
        const Basin& b = dbg.basins[static_cast<size_t>(dbg.ranked[r])];
        if (!b.eligible || b.rank != static_cast<uint32_t>(r + 1)) { fail("rank order entry", b.firstCell); }
        if (r > 0)
        {
            const Basin& p = dbg.basins[static_cast<size_t>(dbg.ranked[r - 1])];
            const bool ordered = p.volume > b.volume || (p.volume == b.volume && p.firstCell < b.firstCell);
            if (!ordered) { fail("rank order", b.firstCell); }
        }
    }
    for (const Basin& b : dbg.basins)
    {
        if (!b.eligible && b.rank != 0u) { fail("ineligible basin ranked", b.firstCell); }
    }
    return fails;
}

int CheckGrid(const Grid& g, const std::string& ctx)
{
    return CheckInvariants(g.dims, g.elev.data(), g.ocean.data(), g.filled.data(), g.depth.data(), g.rank.data(),
                           g.dir.data(), g.acc.data(), g.dbg, ctx);
}

int CheckGenerator(const TerrainGenerator& gen, GridDimensions d, const std::string& ctx)
{
    const int n = d.width * d.height;
    const float* elev = F(gen, "elevationRaw");
    std::vector<uint8_t> ocean(static_cast<size_t>(n), 0);
    const uint32_t* lm = U(gen, "landmass");
    for (int i = 0; i < n; ++i) { ocean[i] = lm[i] == 0u ? 1 : 0; }
    return CheckInvariants(d, elev, ocean.data(), F(gen, "filledElevation"), F(gen, "basinDepth"), U(gen, "basinRank"),
                           U(gen, "flowDirection"), F(gen, "flowAccumulation"), gen.GetStage5Debug(), ctx);
}

// A 1-row grid is a line: odd columns reach west by d = 2, even columns by d = 3.
Grid Line(std::initializer_list<float> elevations)
{
    Grid g(static_cast<int>(elevations.size()), 1, 0.0f);
    int i = 0;
    for (float e : elevations) { g.elev[i++] = e; }
    g.Ocean(0);
    return g;
}

// Deterministic rugged terrain: an LCG height field over an ocean border.
Grid Rugged(int w, int h, uint32_t seed)
{
    Grid g(w, h, 0.0f);
    uint32_t s = seed;
    for (int i = 0; i < g.N(); ++i)
    {
        s = s * 1664525u + 1013904223u;
        g.elev[i] = 0.02f + 0.6f * static_cast<float>(s >> 8) / 16777216.0f;
    }
    g.OceanBorder();
    return g;
}

} // namespace

// ---------------------------------------------------------------------------
// Layers and wiring
// ---------------------------------------------------------------------------
TEST(TerrainDrainage, LayersAreRegisteredOnStageFive)
{
    TerrainLayers layers;
    const struct { const char* name; LayerType type; } expected[] = {
        { "filledElevation", LayerType::Float }, { "basinDepth", LayerType::Float }, { "basinRank", LayerType::UInt },
        { "flowDirection", LayerType::UInt }, { "flowAccumulation", LayerType::Float } };
    for (const auto& e : expected)
    {
        const int idx = layers.FindLayer(e.name);
        ASSERT_GE(idx, 0) << e.name;
        EXPECT_EQ(TerrainLayers::GetLayerInfo(idx).type, e.type) << e.name;
        EXPECT_EQ(TerrainLayers::GetLayerInfo(idx).stage, TerrainStage::Drainage) << e.name;
    }
}

// ---------------------------------------------------------------------------
// Hand-built grids: exact behaviour
// ---------------------------------------------------------------------------
TEST(TerrainDrainage, LineDrainsWestWithDirectionEncodingAndExactAccumulation)
{
    Grid g = Line({ 0.0f, 0.1f, 0.2f, 0.3f, 0.4f });
    g.Run();
    EXPECT_EQ(g.dir[0], kFlowSink);
    EXPECT_EQ(g.dir[1], 2u);   // odd column: west = NW
    EXPECT_EQ(g.dir[2], 3u);   // even column: west = SW
    EXPECT_EQ(g.dir[3], 2u);
    EXPECT_EQ(g.dir[4], 3u);
    for (int i = 0; i < 5; ++i) { EXPECT_EQ(g.filled[i], g.elev[i]) << i; EXPECT_EQ(g.depth[i], 0.0f) << i; }
    const float k = TerrainRules::Defaults().kHighlandRain();
    const float a4 = 1.0f + k * 0.4f;
    const float a3 = (1.0f + k * 0.3f) + a4;
    const float a2 = (1.0f + k * 0.2f) + a3;
    const float a1 = (1.0f + k * 0.1f) + a2;
    EXPECT_FLOAT_EQ(g.acc[4], a4);
    EXPECT_FLOAT_EQ(g.acc[3], a3);
    EXPECT_FLOAT_EQ(g.acc[2], a2);
    EXPECT_FLOAT_EQ(g.acc[1], a1);
    EXPECT_EQ(g.acc[0], 0.0f);
    EXPECT_TRUE(g.dbg.basins.empty());
    EXPECT_EQ(0, CheckGrid(g, "line"));
}

TEST(TerrainDrainage, LineBowlFillsExactlyToItsSpillHeight)
{
    // ocean, rim 0.3, floor 0.1, floor 0.1, 0.4: the two floor cells fill to 0.3.
    Grid g = Line({ 0.0f, 0.3f, 0.1f, 0.1f, 0.4f });
    g.Run();
    EXPECT_EQ(g.filled[1], 0.3f);
    EXPECT_EQ(g.filled[2], 0.3f + kFillEpsilon);
    EXPECT_EQ(g.filled[3], (0.3f + kFillEpsilon) + kFillEpsilon);
    EXPECT_EQ(g.filled[4], 0.4f);
    EXPECT_EQ(g.depth[1], 0.0f);
    EXPECT_EQ(g.depth[2], 0.3f - 0.1f);
    EXPECT_EQ(g.depth[3], 0.3f - 0.1f);
    EXPECT_EQ(g.depth[4], 0.0f);
    ASSERT_EQ(g.dbg.basins.size(), 1u);
    const Basin& b = g.dbg.basins[0];
    EXPECT_EQ(b.cells, (std::vector<int>{ 2, 3 }));
    EXPECT_EQ(b.firstCell, 2);
    EXPECT_EQ(b.spillHeight, 0.3f);
    EXPECT_EQ(b.spillCell, 1);
    EXPECT_EQ(b.maxDepth, 0.3f - 0.1f);
    EXPECT_EQ(b.volume, (0.3f - 0.1f) + (0.3f - 0.1f));
    EXPECT_TRUE(b.eligible);
    EXPECT_EQ(b.rank, 1u);
    EXPECT_EQ(g.dbg.eligibleBasins, 1);
    EXPECT_EQ(g.dbg.ranked, (std::vector<int>{ 0 }));
    EXPECT_EQ(g.rank[2], 1u);
    EXPECT_EQ(g.rank[3], 1u);
    EXPECT_EQ(g.rank[1], 0u);
    // Water crosses the filled basin: 4 -> 3 -> 2 -> 1 -> ocean.
    EXPECT_EQ(g.dir[4], 3u);
    EXPECT_EQ(g.dir[3], 2u);
    EXPECT_EQ(g.dir[2], 3u);
    EXPECT_EQ(g.dir[1], 2u);
    EXPECT_EQ(0, CheckGrid(g, "line bowl"));
}

TEST(TerrainDrainage, HexBowlFillsToOutletWithSpillCellAtThePourPoint)
{
    // 7x7: ocean border, rim ring 0.5 with one outlet at 0.3, 3x3 floor at 0.1.
    Grid g(7, 7, 0.5f);
    g.OceanBorder();
    for (int y = 2; y <= 4; ++y) { for (int x = 2; x <= 4; ++x) { g.elev[g.I(x, y)] = 0.1f; } }
    const int outlet = g.I(1, 3);
    g.elev[outlet] = 0.3f;
    g.Run();
    ASSERT_EQ(g.dbg.basins.size(), 1u);
    const Basin& b = g.dbg.basins[0];
    ASSERT_EQ(b.cells.size(), 9u);
    EXPECT_EQ(b.firstCell, g.I(2, 2));
    EXPECT_EQ(b.spillHeight, 0.3f);
    EXPECT_EQ(b.spillCell, outlet);
    EXPECT_EQ(b.maxDepth, 0.3f - 0.1f);
    float volume = 0.0f;
    for (int i = 0; i < 9; ++i) { volume += 0.3f - 0.1f; }
    EXPECT_EQ(b.volume, volume);
    EXPECT_EQ(b.rank, 1u);
    for (int c : b.cells)
    {
        EXPECT_EQ(g.depth[c], 0.3f - 0.1f) << c;
        EXPECT_GT(g.filled[c], 0.3f) << c;
        EXPECT_LT(g.filled[c], 0.3f + 10.0f * kFillEpsilon) << c;
        EXPECT_EQ(g.rank[c], 1u) << c;
    }
    // Every floor cell's water leaves through the outlet.
    const HexTopology topo(7, 7);
    for (int c : b.cells)
    {
        int cur = c;
        for (int step = 0; step < 49 && g.ocean[cur] == 0 && g.depth[cur] > 0.0f; ++step)
        {
            cur = static_cast<int>(topo.NeighbourAt(static_cast<uint32_t>(cur), g.dir[cur]));
        }
        EXPECT_EQ(cur, outlet) << c;
    }
    EXPECT_EQ(0, CheckGrid(g, "hex bowl"));
}

TEST(TerrainDrainage, LongFlatsAndGentleSlopesMakeNoSpuriousBasin)
{
    // A 60-cell flat strip: the eps ramp reaches 6e-4 (> 1e-4) at its far end, yet depth stays 0.
    Grid flat(60, 3, 0.3f);
    for (int y = 0; y < 3; ++y) { flat.Ocean(flat.I(0, y)); }
    flat.Run();
    for (int i = 0; i < flat.N(); ++i) { EXPECT_EQ(flat.depth[i], 0.0f) << i; EXPECT_EQ(flat.rank[i], 0u) << i; }
    EXPECT_TRUE(flat.dbg.basins.empty());
    EXPECT_EQ(flat.dbg.eligibleBasins, 0);
    EXPECT_GT(flat.filled[flat.I(59, 1)] - flat.elev[flat.I(59, 1)], kBasinDepthFloor);   // the hazard is real
    EXPECT_EQ(0, CheckGrid(flat, "flat"));

    // Rising inland by less than eps per cell (and by more), and a flat at kCoastElev.
    for (float slope : { 3e-6f, 2e-5f })
    {
        Grid g(60, 3, 0.0f);
        for (int y = 0; y < 3; ++y) { for (int x = 1; x < 60; ++x) { g.elev[g.I(x, y)] = 0.02f + slope * static_cast<float>(x); } }
        for (int y = 0; y < 3; ++y) { g.Ocean(g.I(0, y)); }
        g.Run();
        for (int i = 0; i < g.N(); ++i) { EXPECT_EQ(g.depth[i], 0.0f) << "slope " << slope << " cell " << i; }
        EXPECT_TRUE(g.dbg.basins.empty()) << slope;
        EXPECT_EQ(0, CheckGrid(g, "slope"));
    }
}

TEST(TerrainDrainage, FlatPlateauDrainsAndTiesTakeLowestDirection)
{
    // Single land cell surrounded by ocean: every drop is equal, so d = 0.
    Grid island(5, 5, 0.0f);
    for (int i = 0; i < 25; ++i) { island.Ocean(i); }
    const int c = island.I(2, 2);
    island.ocean[c] = 0;
    island.elev[c] = 0.4f;
    island.Run();
    EXPECT_EQ(island.dir[c], 0u);
    EXPECT_FLOAT_EQ(island.acc[c], 1.0f + TerrainRules::Defaults().kHighlandRain() * 0.4f);
    EXPECT_EQ(0, CheckGrid(island, "island"));

    // Flat land with ocean at directions 3 and 5 of the centre: the tie goes to 3.
    Grid g(5, 5, 0.5f);
    const HexTopology topo(5, 5);
    const int centre = g.I(2, 2);
    g.Ocean(static_cast<int>(topo.NeighbourAt(static_cast<uint32_t>(centre), 3)));
    g.Ocean(static_cast<int>(topo.NeighbourAt(static_cast<uint32_t>(centre), 5)));
    g.Run();
    EXPECT_EQ(g.dir[centre], 3u);
    for (int i = 0; i < 25; ++i) { EXPECT_EQ(g.depth[i], 0.0f) << i; }
    EXPECT_EQ(0, CheckGrid(g, "flat plateau"));

    // Symmetric grid, repeated runs: identical directions.
    Grid h(5, 5, 0.5f);
    h.Run();   // no ocean at all: degenerate, every cell is a sink
    for (int i = 0; i < 25; ++i) { EXPECT_EQ(h.dir[i], kFlowSink) << i; EXPECT_EQ(h.filled[i], 0.5f) << i; }
    Grid s1(9, 9, 0.5f), s2(9, 9, 0.5f);
    s1.OceanBorder(); s2.OceanBorder();
    s1.Run(); s2.Run();
    EXPECT_EQ(s1.dir, s2.dir);
    EXPECT_EQ(0, std::memcmp(s1.filled.data(), s2.filled.data(), s1.filled.size() * sizeof(float)));
    EXPECT_EQ(0, CheckGrid(s1, "symmetric"));
}

TEST(TerrainDrainage, EligibilityNeedsDepthAndTwoCells)
{
    // One deep cell: a basin, but not eligible.
    Grid one(7, 7, 0.5f);
    one.OceanBorder();
    one.elev[one.I(3, 3)] = 0.1f;
    one.Run();
    ASSERT_EQ(one.dbg.basins.size(), 1u);
    EXPECT_EQ(one.dbg.basins[0].cells.size(), 1u);
    EXPECT_FALSE(one.dbg.basins[0].eligible);
    EXPECT_EQ(one.dbg.basins[0].rank, 0u);
    EXPECT_EQ(one.depth[one.I(3, 3)], 0.5f - 0.1f);
    EXPECT_EQ(one.rank[one.I(3, 3)], 0u);
    EXPECT_EQ(one.dbg.eligibleBasins, 0);
    EXPECT_EQ(0, CheckGrid(one, "one cell"));

    // Two cells, depth 0.005 < kMinBasinDepth: not eligible; depth 0.05: eligible.
    for (float floor : { 0.495f, 0.45f })
    {
        Grid two(7, 7, 0.5f);
        two.OceanBorder();
        two.elev[two.I(2, 3)] = floor;
        two.elev[two.I(3, 3)] = floor;
        two.Run();
        ASSERT_EQ(two.dbg.basins.size(), 1u) << floor;
        EXPECT_EQ(two.dbg.basins[0].cells.size(), 2u);
        const bool expectEligible = floor < 0.49f;
        EXPECT_EQ(two.dbg.basins[0].eligible, expectEligible) << floor;
        EXPECT_EQ(two.dbg.eligibleBasins, expectEligible ? 1 : 0);
        EXPECT_EQ(two.rank[two.I(2, 3)], expectEligible ? 1u : 0u);
        EXPECT_EQ(0, CheckGrid(two, "two cells"));
    }
}

TEST(TerrainDrainage, RankByVolumeThenSmallestCellIndex)
{
    // Two identical bowls (translated by an even column offset, so the hex shape is identical).
    auto build = [](float floorB)
    {
        Grid g(15, 7, 0.5f);
        g.OceanBorder();
        for (int x : { 3, 4 }) { g.elev[g.I(x, 3)] = 0.1f; }
        for (int x : { 9, 10 }) { g.elev[g.I(x, 3)] = floorB; }
        g.Run();
        return g;
    };
    Grid equal = build(0.1f);
    ASSERT_EQ(equal.dbg.basins.size(), 2u);
    EXPECT_EQ(equal.dbg.basins[0].volume, equal.dbg.basins[1].volume);
    EXPECT_EQ(equal.rank[equal.I(3, 3)], 1u);
    EXPECT_EQ(equal.rank[equal.I(9, 3)], 2u);
    EXPECT_EQ(equal.dbg.ranked, (std::vector<int>{ 0, 1 }));
    EXPECT_EQ(equal.dbg.eligibleBasins, 2);
    EXPECT_EQ(0, CheckGrid(equal, "equal bowls"));

    Grid deeperB = build(0.05f);
    ASSERT_EQ(deeperB.dbg.basins.size(), 2u);
    EXPECT_EQ(deeperB.rank[deeperB.I(9, 3)], 1u);
    EXPECT_EQ(deeperB.rank[deeperB.I(3, 3)], 2u);
    EXPECT_EQ(deeperB.dbg.ranked, (std::vector<int>{ 1, 0 }));
    EXPECT_EQ(0, CheckGrid(deeperB, "deeper B"));
}

TEST(TerrainDrainage, RuggedAndLandlockedGridsHoldEveryInvariant)
{
    for (uint32_t seed : { 1u, 2u, 3u })
    {
        Grid g = Rugged(24, 18, seed);
        // An inland ocean cell (an enclosed sea is still a sink) and a landlocked plateau.
        g.Ocean(g.I(12, 9));
        for (int y = 3; y <= 6; ++y) { for (int x = 3; x <= 6; ++x) { g.elev[g.I(x, y)] = 0.45f; } }
        g.Run();
        EXPECT_GT(g.dbg.basins.size(), 0u) << seed;
        EXPECT_EQ(0, CheckGrid(g, "rugged " + std::to_string(seed)));
    }
}

TEST(TerrainDrainage, EmptyAndNullInputsAreSafe)
{
    Stage5Debug dbg;
    Stage5Layers none;
    ComputeStage5(TerrainRules::Defaults(), GridDimensions{ 0, 0 }, nullptr, nullptr, none, dbg);
    EXPECT_TRUE(dbg.basins.empty());
    EXPECT_EQ(dbg.eligibleBasins, 0);
    Grid g(3, 3, 0.2f);
    Stage5Layers partial;
    std::vector<float> filled(9, 7.0f);
    partial.filledElevation = filled.data();
    ComputeStage5(TerrainRules::Defaults(), g.dims, g.elev.data(), g.ocean.data(), partial, dbg);
    ComputeStage5(TerrainRules::Defaults(), g.dims, nullptr, nullptr, partial, dbg);
    for (GridDimensions d : { GridDimensions{ 1, 1 }, GridDimensions{ 2, 1 }, GridDimensions{ 1, 5 } })
    {
        Grid t(d.width, d.height, 0.3f);
        t.Ocean(0);
        t.Run();
        EXPECT_EQ(0, CheckGrid(t, "tiny"));
    }
}

// ---------------------------------------------------------------------------
// Generator integration
// ---------------------------------------------------------------------------
TEST(TerrainDrainage, GeneratorInvariantsOnThreeSeeds)
{
    for (uint64_t seed : { 1u, 7u, 42u })
    {
        Gen r;
        r.Go(seed, WorldShapeParams{});
        ASSERT_NE(F(r.gen, "filledElevation"), nullptr);
        EXPECT_EQ(0, CheckGenerator(r.gen, kSmall, "seed " + std::to_string(seed)));
        EXPECT_EQ(r.gen.GetEffective().eligibleBasins, r.gen.GetStage5Debug().eligibleBasins);
        EXPECT_LE(r.gen.GetEffective().lakes, r.gen.GetEffective().eligibleBasins);   // stage 7: min(Lakes, eligible)
        int land = 0;
        const float* acc = F(r.gen, "flowAccumulation");
        float maxAcc = 0.0f;
        for (int i = 0; i < kN; ++i) { land += U(r.gen, "landmass")[i] != 0u ? 1 : 0; maxAcc = std::max(maxAcc, acc[i]); }
        EXPECT_GT(land, 0);
        EXPECT_GT(maxAcc, 10.0f) << seed;   // water gathers
    }
}

TEST(TerrainDrainage, GeneratorIsByteIdenticalAcrossRuns)
{
    Gen a, b;
    a.Go(11, WorldShapeParams{});
    b.Go(11, WorldShapeParams{});
    for (const char* name : { "filledElevation", "basinDepth", "flowAccumulation" })
    {
        EXPECT_EQ(0, std::memcmp(F(a.gen, name), F(b.gen, name), kN * sizeof(float))) << name;
    }
    for (const char* name : { "basinRank", "flowDirection" })
    {
        EXPECT_EQ(0, std::memcmp(U(a.gen, name), U(b.gen, name), kN * sizeof(uint32_t))) << name;
    }
    const Stage5Debug& da = a.gen.GetStage5Debug();
    const Stage5Debug& db = b.gen.GetStage5Debug();
    ASSERT_EQ(da.basins.size(), db.basins.size());
    for (size_t i = 0; i < da.basins.size(); ++i)
    {
        EXPECT_EQ(da.basins[i].cells, db.basins[i].cells);
        EXPECT_EQ(da.basins[i].volume, db.basins[i].volume);
        EXPECT_EQ(da.basins[i].spillCell, db.basins[i].spillCell);
    }
    EXPECT_EQ(da.ranked, db.ranked);
}

TEST(TerrainDrainage, DirtyBitsRuggednessMountainousnessAndLatitude)
{
    Gen r;
    WorldShapeParams s;
    r.Go(5, s);
    EXPECT_EQ(Ran(r.gen).count(5), 1u);

    s.ruggedness = 0.9f;
    r.Go(5, s);
    EXPECT_EQ(Ran(r.gen).count(4), 1u);
    EXPECT_EQ(Ran(r.gen).count(5), 1u);

    s.latitudeNorthDeg = 70.0f;
    const std::vector<float> before(F(r.gen, "filledElevation"), F(r.gen, "filledElevation") + kN);
    r.Go(5, s);
    EXPECT_EQ(Ran(r.gen).count(4), 0u);
    EXPECT_EQ(Ran(r.gen).count(5), 0u);
    EXPECT_EQ(0, std::memcmp(before.data(), F(r.gen, "filledElevation"), kN * sizeof(float)));

    s.mountainousness = 0.9f;
    r.Go(5, s);
    EXPECT_EQ(Ran(r.gen).count(2), 1u);
    EXPECT_EQ(Ran(r.gen).count(5), 1u);
    EXPECT_EQ(0, CheckGenerator(r.gen, kSmall, "after slider changes"));
}

TEST(TerrainDrainage, OddGridSizesHoldInvariants)
{
    for (GridDimensions d : { GridDimensions{ 1, 1 }, GridDimensions{ 3, 3 }, GridDimensions{ 8, 8 }, GridDimensions{ 20, 5 } })
    {
        Gen r;
        r.Go(2, WorldShapeParams{}, d);
        EXPECT_EQ(0, CheckGenerator(r.gen, d, std::to_string(d.width) + "x" + std::to_string(d.height)));
    }
}

// ---------------------------------------------------------------------------
// Sweeps
// ---------------------------------------------------------------------------
TEST(SLOW_TerrainDrainage, InvariantsSweep20Seeds)
{
    for (uint64_t seed = 1; seed <= 20; ++seed)
    {
        for (BarrierForm form : { BarrierForm::Plateau, BarrierForm::VolcanicChain })
        {
            for (float rugged : { 0.0f, 1.0f })
            {
                WorldShapeParams s;
                s.barrierForm = form;
                s.ruggedness = rugged;
                Gen r;
                r.Go(seed, s);
                ASSERT_EQ(0, CheckGenerator(r.gen, kSmall, "seed " + std::to_string(seed) + " " + BarrierFormName(form) +
                                                           " rugged " + std::to_string(rugged)));
            }
        }
    }
    for (uint32_t seed = 10; seed < 30; ++seed)
    {
        Grid g = Rugged(40, 30, seed);
        g.Run();
        ASSERT_EQ(0, CheckGrid(g, "rugged sweep " + std::to_string(seed)));
    }
}

// ---------------------------------------------------------------------------
// Timing (informational, Release only)
// ---------------------------------------------------------------------------
TEST(TerrainPerf, StageFiveDrainageReport)
{
#ifdef NDEBUG
    constexpr int kRuns = 50;
    std::vector<double> times;
    Gen r;
    const WorldShapeParams s;
    for (int run = 0; run < kRuns; ++run)
    {
        r.Go(2000 + static_cast<uint64_t>(run), s);
        times.push_back(r.gen.GetTimings()[static_cast<int>(TerrainStage::Drainage)].milliseconds);
    }
    std::sort(times.begin(), times.end());
    std::printf("[TerrainPerf] stage 5 Drainage median %.3f ms, max %.3f ms (66x42, %d runs)%s\n",
                times[times.size() / 2], times.back(), kRuns, times[times.size() / 2] > 1.0 ? "  ** EXCEEDS 1 ms **" : "");
    SUCCEED();
#else
    GTEST_SKIP() << "Release-only timing report";
#endif
}
