// S-1 Terrain Generator task 11: Stage 9 Valleys (valleyCarve, elevation).
// Hand-built grids check carving, floors and the downhill walk exactly through ComputeStage9; generator
// runs check AC12 (valleyDepth monotonicity, locality, downhill rivers) on real terrain. Wide sweeps are SLOW_.
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

// ---------------------------------------------------------------------------
// Hand-built grid: 14x9 land at 0.5 with an ocean column at x = 0. Rivers run west along row 4.
// ---------------------------------------------------------------------------
constexpr float kT = 10.0f;

struct VGrid
{
    GridDimensions        dims{ 14, 9 };
    HexTopology           topo{ 14, 9 };
    int                   n = 14 * 9;
    std::vector<float>    raw, acc, surfaces, carve, elev;
    std::vector<uint32_t> dir, river, lake;
    Stage9Debug           dbg;
    TerrainRules          rules = TerrainRules::Defaults();

    VGrid() : raw(static_cast<size_t>(n), 0.5f), acc(static_cast<size_t>(n), 1.0f), carve(static_cast<size_t>(n), -1.0f),
        elev(static_cast<size_t>(n), -1.0f), dir(static_cast<size_t>(n), 6u), river(static_cast<size_t>(n), 0u),
        lake(static_cast<size_t>(n), 0u)
    {
        for (int y = 0; y < 9; ++y) { raw[I(0, y)] = 0.0f; }
    }

    int I(int x, int y) const { return y * 14 + x; }

    uint32_t DirTo(int from, int to) const
    {
        for (uint32_t d = 0; d < 6; ++d)
        {
            if (topo.NeighbourAt(static_cast<uint32_t>(from), d) == static_cast<uint32_t>(to)) { return d; }
        }
        return 6u;
    }

    // River cells x = xFrom down to xTo along row y, flowing west; acc rises downstream starting at accStart.
    void River(int y, int xFrom, int xTo, float accStart = kT, float accStep = 5.0f)
    {
        float a = accStart;
        for (int x = xFrom; x >= xTo; --x)
        {
            river[I(x, y)] = 1u;
            acc[I(x, y)] = a;
            a += accStep;
            dir[I(x, y)] = DirTo(I(x, y), I(x - 1, y));
        }
    }

    void Lake(int y, int xFrom, int xTo, uint32_t id, float accStart)
    {
        float a = accStart;
        for (int x = xFrom; x >= xTo; --x)
        {
            lake[I(x, y)] = id;
            acc[I(x, y)] = a;
            a += 1.0f;
            dir[I(x, y)] = DirTo(I(x, y), I(x - 1, y));
        }
    }

    void Run(float valleyDepth, bool withRivers = true)
    {
        Stage9Layers out;
        out.valleyCarve = carve.data();
        out.elevation = elev.data();
        ComputeStage9(rules, dims, valleyDepth, kT, raw.data(), dir.data(), acc.data(),
                      withRivers ? river.data() : nullptr, lake.data(), surfaces.data(),
                      static_cast<int>(surfaces.size()), out, dbg);
    }
};

constexpr float kEps = 1e-6f;

// ---------------------------------------------------------------------------
// Carve size
// ---------------------------------------------------------------------------
TEST(TerrainValleys, CarveSizeAtThresholdIsPointFour)
{
    const TerrainRules r = TerrainRules::Defaults();
    EXPECT_NEAR(ValleyCarveSize(r, kT, kT), 0.4f, kEps);
    EXPECT_NEAR(ValleyCarveSize(r, kT * 0.5f, kT), 0.4f, kEps);
}

TEST(TerrainValleys, CarveSizeReachesOneAtEightT)
{
    const TerrainRules r = TerrainRules::Defaults();
    EXPECT_NEAR(ValleyCarveSize(r, 8.0f * kT, kT), 1.0f, 1e-5f);
    EXPECT_NEAR(ValleyCarveSize(r, 100.0f * kT, kT), 1.0f, kEps);
}

TEST(TerrainValleys, CarveSizeIsLogarithmicAtMidpoint)
{
    const TerrainRules r = TerrainRules::Defaults();
    // acc = T * sqrt(8) is half way on the log scale: 0.4 + 0.6 * 0.5.
    EXPECT_NEAR(ValleyCarveSize(r, kT * std::sqrt(8.0f), kT), 0.7f, 1e-4f);
}

TEST(TerrainValleys, CarveSizeIsMonotone)
{
    const TerrainRules r = TerrainRules::Defaults();
    float prev = 0.0f;
    for (float a = kT; a <= 12.0f * kT; a += 3.0f)
    {
        const float s = ValleyCarveSize(r, a, kT);
        EXPECT_GE(s, prev);
        prev = s;
    }
}

// ---------------------------------------------------------------------------
// Carving
// ---------------------------------------------------------------------------
TEST(TerrainValleys, RiverCellCarvedByDepthKCarveMaxSize)
{
    VGrid g;
    g.River(4, 10, 1, kT, 0.0f);   // every river cell at acc == T: size 0.4
    g.Run(1.0f);
    const float expected = 1.0f * 0.12f * 0.4f;
    EXPECT_NEAR(g.carve[g.I(5, 4)], expected, kEps);
    EXPECT_NEAR(g.elev[g.I(5, 4)], 0.5f - expected, kEps);
}

TEST(TerrainValleys, FullSizeRiverCarveAtEightT)
{
    VGrid g;
    g.River(4, 10, 1, 8.0f * kT, 0.0f);
    g.Run(0.5f);
    EXPECT_NEAR(g.carve[g.I(5, 4)], 0.5f * 0.12f * 1.0f, 1e-5f);
}

TEST(TerrainValleys, NeighbourCarveFactorsHalfAndFifth)
{
    VGrid g;
    g.River(4, 10, 1, kT, 0.0f);
    g.Run(1.0f);
    const float c = 0.12f * 0.4f;
    EXPECT_NEAR(g.carve[g.I(5, 3)], 0.5f * c, kEps);   // one step
    EXPECT_NEAR(g.carve[g.I(5, 5)], 0.5f * c, kEps);
    EXPECT_NEAR(g.carve[g.I(5, 2)], 0.2f * c, kEps);   // two steps
    EXPECT_NEAR(g.carve[g.I(5, 6)], 0.2f * c, kEps);
    EXPECT_EQ(g.carve[g.I(5, 1)], 0.0f);               // three steps
    EXPECT_EQ(g.carve[g.I(5, 7)], 0.0f);
    EXPECT_NEAR(g.elev[g.I(5, 3)], 0.5f - 0.5f * c, kEps);
    EXPECT_EQ(g.elev[g.I(5, 1)], 0.5f);
}

TEST(TerrainValleys, NeighbourUsesDeepestAdjacentCarve)
{
    VGrid g;
    g.River(4, 10, 1, kT, 0.0f);
    g.acc[g.I(5, 4)] = 8.0f * kT;   // one deep cell among shallow ones
    g.acc[g.I(4, 4)] = 8.0f * kT;
    g.acc[g.I(6, 4)] = 8.0f * kT;
    g.Run(1.0f);
    // (5,3) touches (5,4) and (4,4)/(6,4) depending on parity; all three are deep, so 0.5 x full carve.
    EXPECT_NEAR(g.carve[g.I(5, 3)], 0.5f * 0.12f, 1e-5f);
    EXPECT_NEAR(g.carve[g.I(5, 2)], 0.2f * 0.12f, 1e-5f);
}

TEST(TerrainValleys, LakeCellsUncarvedAndAtSurface)
{
    VGrid g;
    g.River(4, 10, 1, kT, 0.0f);
    g.surfaces = { 0.31f };
    g.lake[g.I(5, 3)] = 1u;
    g.Run(1.0f);
    EXPECT_EQ(g.carve[g.I(5, 3)], 0.0f);
    EXPECT_EQ(g.elev[g.I(5, 3)], 0.31f);
}

TEST(TerrainValleys, OceanStaysZeroAndUncarved)
{
    VGrid g;
    g.River(4, 10, 1, kT, 0.0f);
    g.Run(1.0f);
    for (int y = 0; y < 9; ++y)
    {
        EXPECT_EQ(g.elev[g.I(0, y)], 0.0f);
        EXPECT_EQ(g.carve[g.I(0, y)], 0.0f);
    }
}

TEST(TerrainValleys, NoRiversMeansNoCarve)
{
    VGrid g;
    g.River(4, 10, 1, kT, 0.0f);
    g.raw[g.I(8, 8)] = 0.001f;       // below the coast clamp
    g.surfaces = { 0.31f };
    g.lake[g.I(3, 7)] = 1u;
    g.Run(1.0f, false);
    EXPECT_EQ(g.dbg.riverCells, 0);
    for (int i = 0; i < g.n; ++i)
    {
        EXPECT_EQ(g.carve[i], 0.0f);
    }
    EXPECT_EQ(g.elev[g.I(5, 4)], 0.5f);
    EXPECT_EQ(g.elev[g.I(8, 8)], g.rules.kCoastElev());
    EXPECT_EQ(g.elev[g.I(3, 7)], 0.31f);
    EXPECT_EQ(g.elev[g.I(0, 4)], 0.0f);
}

TEST(TerrainValleys, ZeroValleyDepthCarvesNothing)
{
    VGrid g;
    g.River(4, 10, 1, kT, 0.0f);
    g.Run(0.0f);
    for (int i = 0; i < g.n; ++i) { EXPECT_EQ(g.carve[i], 0.0f); }
    EXPECT_EQ(g.elev[g.I(5, 4)], 0.5f);
}

TEST(TerrainValleys, LandElevationClampedToCoast)
{
    VGrid g;
    g.River(4, 10, 1, 8.0f * kT, 0.0f);
    g.raw[g.I(5, 4)] = 0.03f;   // carve 0.12 would take it below the coast clamp
    g.Run(1.0f);
    EXPECT_EQ(g.elev[g.I(5, 4)], g.rules.kCoastElev());
}

// ---------------------------------------------------------------------------
// Floors
// ---------------------------------------------------------------------------
TEST(TerrainValleys, FloorRaisesCarvedRiverCellToLakeSurface)
{
    VGrid g;
    g.River(4, 10, 3, kT, 0.0f);          // drains into the lake at x = 2..1
    g.Lake(4, 2, 1, 1u, 100.0f);
    g.surfaces = { 0.49f };               // carve 0.048 would leave 0.452, below the surface
    g.Run(1.0f);
    for (int x = 3; x <= 10; ++x)
    {
        EXPECT_NEAR(g.elev[g.I(x, 4)], 0.49f, kEps) << "x=" << x;
    }
    EXPECT_GT(g.dbg.raisedByFloor, 0);
    EXPECT_EQ(g.elev[g.I(2, 4)], 0.49f);
}

TEST(TerrainValleys, DirectToSeaFloorIsCoastElev)
{
    VGrid g;
    g.River(4, 10, 1, 8.0f * kT, 0.0f);
    g.raw[g.I(1, 4)] = 0.03f;
    g.Run(1.0f);
    EXPECT_GE(g.elev[g.I(1, 4)], g.rules.kCoastElev());
}

// ---------------------------------------------------------------------------
// Downhill walk
// ---------------------------------------------------------------------------
TEST(TerrainValleys, DownhillWalkFixesUphillStep)
{
    VGrid g;
    g.River(4, 10, 1, kT, 5.0f);
    g.raw[g.I(6, 4)] = 0.2f;   // a pit upstream of higher ground
    g.Run(0.0f);
    EXPECT_EQ(g.elev[g.I(6, 4)], 0.2f);
    EXPECT_EQ(g.elev[g.I(5, 4)], 0.2f);
    for (int x = 10; x > 1; --x)
    {
        EXPECT_GE(g.elev[g.I(x, 4)], g.elev[g.I(x - 1, 4)]) << "x=" << x;
    }
    EXPECT_GT(g.dbg.loweredByWalk, 0);
}

TEST(TerrainValleys, GorgeCutThroughDryBasinRim)
{
    VGrid g;
    g.River(4, 10, 1, kT, 5.0f);
    g.raw[g.I(8, 4)] = 0.3f;
    g.raw[g.I(7, 4)] = 0.9f;   // rim
    g.Run(0.0f);
    EXPECT_EQ(g.elev[g.I(7, 4)], 0.3f);
    EXPECT_EQ(g.raw[g.I(7, 3)], 0.5f);
    EXPECT_EQ(g.elev[g.I(7, 3)], 0.5f);   // the rim neighbours stay put
}

TEST(TerrainValleys, LakeOutletNeverAboveSurface)
{
    VGrid g;
    g.River(4, 4, 1, kT, 50.0f);
    g.Lake(4, 6, 5, 1u, 20.0f);
    g.surfaces = { 0.4f };
    g.raw[g.I(4, 4)] = 0.9f;    // the rim cell the river leaves through
    g.Run(0.0f);
    EXPECT_EQ(g.elev[g.I(5, 4)], 0.4f);
    EXPECT_LE(g.elev[g.I(4, 4)], 0.4f);
}

TEST(TerrainValleys, WalkMonotoneInValleyDepthOnHandGrid)
{
    VGrid g;
    g.River(4, 10, 1, kT, 5.0f);
    g.raw[g.I(8, 4)] = 0.3f;
    g.raw[g.I(7, 4)] = 0.9f;
    g.raw[g.I(3, 4)] = 0.6f;
    std::vector<float> prev;
    for (float vd = 0.0f; vd <= 1.0f; vd += 0.25f)
    {
        g.Run(vd);
        if (!prev.empty())
        {
            for (int i = 0; i < g.n; ++i) { EXPECT_LE(g.elev[i], prev[i]) << "cell " << i << " vd " << vd; }
        }
        prev = g.elev;
    }
}

// ---------------------------------------------------------------------------
// Generator integration
// ---------------------------------------------------------------------------
TEST(TerrainValleys, LayersExistWithCorrectSizeAndType)
{
    Gen r;
    r.Go(7, WorldShapeParams{});
    const int ci = r.gen.GetLayers().FindLayer("valleyCarve");
    const int ei = r.gen.GetLayers().FindLayer("elevation");
    ASSERT_GE(ci, 0);
    ASSERT_GE(ei, 0);
    EXPECT_EQ(TerrainLayers::GetLayerInfo(ci).stage, TerrainStage::Valleys);
    EXPECT_EQ(TerrainLayers::GetLayerInfo(ei).stage, TerrainStage::Valleys);
    ASSERT_NE(F(r.gen, "valleyCarve"), nullptr);
    ASSERT_NE(F(r.gen, "elevation"), nullptr);
}

TEST(TerrainValleys, DirtyBitsValleyDepthRerunsNineAndTen)
{
    Gen r;
    WorldShapeParams s;
    r.Go(11, s);
    s.valleyDepth = 0.9f;
    r.Go(11, s);
    EXPECT_EQ(Ran(r.gen), (std::set<int>{ 9, 10 }));
}

TEST(TerrainValleys, DirtyBitsRiversRerunsSixEightNineTen)
{
    Gen r;
    WorldShapeParams s;
    r.Go(11, s);
    s.rivers = 0.8f;
    r.Go(11, s);
    EXPECT_EQ(Ran(r.gen), (std::set<int>{ 6, 8, 9, 10 }));
}

TEST(TerrainValleys, DirtyBitsLakesRerunsSevenNineTen)
{
    Gen r;
    WorldShapeParams s;
    r.Go(11, s);
    s.lakes = 7;
    r.Go(11, s);
    EXPECT_EQ(Ran(r.gen), (std::set<int>{ 7, 9, 10 }));
}

TEST(TerrainValleys, DeterministicByteIdentical)
{
    Gen a, b;
    const WorldShapeParams s;
    a.Go(21, s);
    b.Go(21, s);
    EXPECT_EQ(std::memcmp(F(a.gen, "elevation"), F(b.gen, "elevation"), sizeof(float) * kN), 0);
    EXPECT_EQ(std::memcmp(F(a.gen, "valleyCarve"), F(b.gen, "valleyCarve"), sizeof(float) * kN), 0);
}

TEST(TerrainValleys, IncrementalMatchesFreshRun)
{
    Gen a, b;
    WorldShapeParams s;
    a.Go(5, s);
    s.valleyDepth = 0.9f;
    a.Go(5, s);
    b.Go(5, s);
    EXPECT_EQ(std::memcmp(F(a.gen, "elevation"), F(b.gen, "elevation"), sizeof(float) * kN), 0);
}

// Hex distance (<= 2) to the nearest river cell; 3 means farther.
std::vector<int> RiverDistance(const uint32_t* river)
{
    const HexTopology topo(kSmall.width, kSmall.height);
    std::vector<int> dist(static_cast<size_t>(kN), 3);
    std::vector<int> q;
    for (int i = 0; i < kN; ++i) { if (river[i] != 0u) { dist[i] = 0; q.push_back(i); } }
    for (size_t head = 0; head < q.size(); ++head)
    {
        const int c = q[head];
        if (dist[c] >= 2) { continue; }
        for (uint32_t d = 0; d < 6; ++d)
        {
            const uint32_t nb = topo.NeighbourAt(static_cast<uint32_t>(c), d);
            if (nb != kNoCell && dist[nb] > dist[c] + 1) { dist[nb] = dist[c] + 1; q.push_back(static_cast<int>(nb)); }
        }
    }
    return dist;
}

// Checks AC12 for one seed over valleyDepth 0..1 step 0.25. Returns the number of failures (first few printed).
int CheckValleySweep(uint64_t seed, float rivers, int lakes)
{
    int fails = 0;
    auto fail = [&](const std::string& what)
    {
        if (++fails <= 5) { std::printf("[valleys] seed %llu: %s\n", static_cast<unsigned long long>(seed), what.c_str()); }
    };
    Gen r;
    WorldShapeParams s;
    s.rivers = rivers;
    s.lakes = static_cast<uint8_t>(lakes);
    std::vector<float> base, prev;
    std::vector<uint32_t> riverBase, lakeBase;
    for (int step = 0; step <= 4; ++step)
    {
        s.valleyDepth = 0.25f * static_cast<float>(step);
        r.Go(seed, s);
        const float* elev = F(r.gen, "elevation");
        const float* raw = F(r.gen, "elevationRaw");
        const uint32_t* river = U(r.gen, "riverMask");
        const uint32_t* lake = U(r.gen, "lakeMask");
        const uint32_t* dir = U(r.gen, "flowDirection");
        const std::vector<Lake>& lakesDbg = r.gen.GetStage7Debug().lakes;
        const float coast = TerrainRules::Defaults().kCoastElev();
        const HexTopology topo(kSmall.width, kSmall.height);
        if (step == 0)
        {
            base.assign(elev, elev + kN);
            riverBase.assign(river, river + kN);
            lakeBase.assign(lake, lake + kN);
        }
        else
        {
            if (!std::equal(riverBase.begin(), riverBase.end(), river) || !std::equal(lakeBase.begin(), lakeBase.end(), lake))
            {
                fail("river/lake masks changed with valleyDepth");
            }
            const std::vector<int> dist = RiverDistance(river);
            for (int i = 0; i < kN; ++i)
            {
                if (elev[i] > prev[i]) { fail("elevation rose with valleyDepth at cell " + std::to_string(i) + " step " + std::to_string(step)
                                               + " " + std::to_string(prev[i]) + " -> " + std::to_string(elev[i])); }
                if (dist[i] > 2 && elev[i] != base[i]) { fail("cell beyond 2 steps changed: " + std::to_string(i)); }
            }
        }
        for (int i = 0; i < kN; ++i)
        {
            if (raw[i] <= 0.0f) { if (elev[i] != 0.0f) { fail("ocean not 0"); } continue; }
            if (elev[i] < coast) { fail("land below kCoastElev at " + std::to_string(i)); }
            if (lake[i] != 0u)
            {
                if (lake[i] > lakesDbg.size() || elev[i] != lakesDbg[lake[i] - 1].surfaceHeight) { fail("lake cell not at surface"); }
                continue;
            }
            if (river[i] == 0u) { continue; }
            // Follow the river downstream: never rise, never below the lake it drains into.
            const uint32_t nbDir = dir[i];
            const uint32_t ds = nbDir <= 5u ? topo.NeighbourAt(static_cast<uint32_t>(i), nbDir) : kNoCell;
            if (ds != kNoCell && raw[ds] > 0.0f && elev[ds] > elev[i])
            {
                fail("river rises downstream at cell " + std::to_string(i) + " " + std::to_string(elev[i]) + " -> " + std::to_string(elev[ds]));
            }
            uint32_t cur = static_cast<uint32_t>(i);
            for (int k = 0; k < kN && cur != kNoCell; ++k)
            {
                if (raw[cur] <= 0.0f) { break; }
                if (lake[cur] != 0u)
                {
                    if (elev[i] < lakesDbg[lake[cur] - 1].surfaceHeight) { fail("river below its lake surface at " + std::to_string(i)); }
                    break;
                }
                cur = dir[cur] <= 5u ? topo.NeighbourAt(cur, dir[cur]) : kNoCell;
            }
        }
        prev.assign(elev, elev + kN);
    }
    return fails;
}

TEST(TerrainValleys, ValleyDepthSweepInvariantsOnFourSeeds)
{
    for (uint64_t seed = 100; seed < 104; ++seed)
    {
        EXPECT_EQ(CheckValleySweep(seed, 0.5f, 4), 0) << "seed " << seed;
    }
}

TEST(TerrainValleys, ValleyDepthSweepWithManyRiversAndLakes)
{
    for (uint64_t seed = 200; seed < 203; ++seed)
    {
        EXPECT_EQ(CheckValleySweep(seed, 0.9f, 10), 0) << "seed " << seed;
    }
}

TEST(TerrainValleys, RiversZeroLeavesRawElevationOnLand)
{
    Gen r;
    WorldShapeParams s;
    s.rivers = 0.0f;
    r.Go(33, s);
    const float* elev = F(r.gen, "elevation");
    const float* raw = F(r.gen, "elevationRaw");
    const uint32_t* lake = U(r.gen, "lakeMask");
    const float* carve = F(r.gen, "valleyCarve");
    const float coast = TerrainRules::Defaults().kCoastElev();
    for (int i = 0; i < kN; ++i)
    {
        EXPECT_EQ(carve[i], 0.0f);
        if (raw[i] <= 0.0f) { EXPECT_EQ(elev[i], 0.0f); }
        else if (lake[i] == 0u) { EXPECT_EQ(elev[i], std::max(coast, raw[i])); }
    }
}

TEST(TerrainValleys, CarveLayerPositiveOnlyNearRivers)
{
    Gen r;
    WorldShapeParams s;
    s.valleyDepth = 1.0f;
    r.Go(44, s);
    const float* carve = F(r.gen, "valleyCarve");
    const std::vector<int> dist = RiverDistance(U(r.gen, "riverMask"));
    for (int i = 0; i < kN; ++i)
    {
        EXPECT_GE(carve[i], 0.0f);
        if (dist[i] > 2) { EXPECT_EQ(carve[i], 0.0f); }
    }
}

TEST(SLOW_TerrainValleys, ValleyDepthSweepOver20Seeds)
{
    for (uint64_t seed = 1000; seed < 1020; ++seed)
    {
        EXPECT_EQ(CheckValleySweep(seed, 0.5f, 4), 0) << "seed " << seed;
        EXPECT_EQ(CheckValleySweep(seed, 1.0f, 12), 0) << "seed " << seed;
    }
}

TEST(TerrainPerf, StageNineValleysReport)
{
#ifdef NDEBUG
    constexpr int kRuns = 50;
    std::vector<double> t9;
    Gen r;
    const WorldShapeParams s;
    for (int run = 0; run < kRuns; ++run)
    {
        r.Go(4000 + static_cast<uint64_t>(run), s);
        t9.push_back(r.gen.GetTimings()[static_cast<int>(TerrainStage::Valleys)].milliseconds);
    }
    std::sort(t9.begin(), t9.end());
    std::printf("[TerrainPerf] stage 9 Valleys median %.3f ms, max %.3f ms (66x42, %d runs)\n",
                t9[t9.size() / 2], t9.back(), kRuns);
    SUCCEED();
#else
    GTEST_SKIP() << "Release-only timing report";
#endif
}

} // namespace
