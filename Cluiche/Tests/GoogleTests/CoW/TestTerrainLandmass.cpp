// S-1 Terrain Generator task 4: Stage 1 Landmass part A (potential M, land/island budgets, reference mainland).
#include <gtest/gtest.h>
#include <Shared/WorldGeneration/TerrainGenerator.h>

#include <DiaPathfinding/HexTopology.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

using namespace CoW::WorldGeneration;
using Dia::Maths::RandomStream;

namespace {

const GridDimensions kSmall{ 66, 42 };
constexpr float kPi = 3.14159265358979f;

int Land(float f, int n) { return static_cast<int>(std::floor(f * static_cast<float>(n) + 0.5f)); }

const float* Potential(const TerrainGenerator& g)
{
    return g.GetLayers().GetFloat(g.GetLayers().FindLayer("landPotential"));
}

struct Harness
{
    TerrainGenerator gen;
    RandomStream rng{ 1 };
    Harness() : gen(TerrainRules::Defaults()) {}
    void Go(uint64_t seed, const WorldShapeParams& s, GridDimensions d = kSmall) { gen.Update(seed, s, d, rng); }
};

int Count(const std::vector<uint8_t>& m)
{
    int c = 0;
    for (uint8_t v : m) { c += v; }
    return c;
}

// Steps from the nearest map edge (0 = outer ring), offset-grid approximation.
int EdgeSteps(int x, int y, GridDimensions d)
{
    int e = x;
    if (d.width - 1 - x < e) { e = d.width - 1 - x; }
    if (y < e) { e = y; }
    if (d.height - 1 - y < e) { e = d.height - 1 - y; }
    return e;
}

// Map space p = CellCenter(c,1) / maxCentreY.
struct MapPoint { float x, y; };
MapPoint ToMap(int x, int y, GridDimensions d)
{
    const Dia::Pathfinding::CellCoord maxYCell{ d.width > 1 ? 1 : 0, d.height - 1 };
    const float maxY = Dia::Pathfinding::HexTopology::CellCenter(maxYCell, 1.0f).y;
    const auto c = Dia::Pathfinding::HexTopology::CellCenter(Dia::Pathfinding::CellCoord{ x, y }, 1.0f);
    return { c.x / maxY, c.y / maxY };
}

// Cells of potential M in the sector +-20 deg around phi (about continent 0's centre) and further
// than 0.6*R0 from that centre, with M >= threshold.
int SectorCount(const float* M, const SeedFields& sf, float phi, float threshold, GridDimensions d)
{
    constexpr float R0 = 0.55f;
    int cnt = 0;
    for (int i = 0; i < d.width * d.height; ++i)
    {
        const MapPoint p = ToMap(i % d.width, i / d.width, d);
        const float dx = p.x - sf.continents[0].x, dy = p.y - sf.continents[0].y;
        if (std::sqrt(dx * dx + dy * dy) <= 0.6f * R0) { continue; }
        float da = std::atan2(dy, dx) - phi;
        da = std::atan2(std::sin(da), std::cos(da));
        if (std::fabs(da) <= 20.0f * kPi / 180.0f && M[i] >= threshold) { ++cnt; }
    }
    return cnt;
}

} // namespace

TEST(TerrainLandmass, LandPotentialLayerRegistered)
{
    Harness r;
    r.Go(7, WorldShapeParams{});
    const int idx = r.gen.GetLayers().FindLayer("landPotential");
    ASSERT_GE(idx, 0);
    EXPECT_EQ(TerrainLayers::GetLayerInfo(idx).type, LayerType::Float);
    EXPECT_EQ(TerrainLayers::GetLayerInfo(idx).stage, TerrainStage::Landmass);
    EXPECT_NE(r.gen.GetLayers().GetFloat(idx), nullptr);
}

TEST(TerrainLandmass, PotentialDeterministicFiniteExceptBorder)
{
    const int n = kSmall.width * kSmall.height;
    for (uint64_t seed = 1; seed <= 20; ++seed)
    {
        Harness a, b;
        a.Go(seed, WorldShapeParams{});
        b.Go(seed, WorldShapeParams{});
        const float* pa = Potential(a.gen);
        const float* pb = Potential(b.gen);
        ASSERT_NE(pa, nullptr);
        for (int i = 0; i < n; ++i)
        {
            const int x = i % kSmall.width, y = i / kSmall.width;
            ASSERT_FALSE(std::isnan(pa[i])) << "seed " << seed << " cell " << i;
            ASSERT_EQ(pa[i], pb[i]) << "seed " << seed << " cell " << i;
            if (EdgeSteps(x, y, kSmall) < 2)
            {
                ASSERT_TRUE(std::isinf(pa[i]) && pa[i] < 0.0f) << "seed " << seed << " cell " << i;
            }
            else
            {
                ASSERT_TRUE(std::isfinite(pa[i])) << "seed " << seed << " cell " << i;
            }
        }
    }
}

// AC3
TEST(TerrainLandmass, BorderCellsNeverLand)
{
    for (uint64_t seed = 1; seed <= 20; ++seed)
    {
        for (float lf : { 0.20f, 0.42f, 0.65f })
        {
            Harness r;
            WorldShapeParams s; s.landFraction = lf;
            r.Go(seed, s);
            const auto top = r.gen.GetStage1Debug().ReferenceMainland();
            ASSERT_EQ(static_cast<int>(top.size()), kSmall.width * kSmall.height);
            for (int i = 0; i < kSmall.width * kSmall.height; ++i)
            {
                if (EdgeSteps(i % kSmall.width, i / kSmall.width, kSmall) < 2)
                {
                    ASSERT_EQ(top[i], 0) << "seed " << seed << " lf " << lf << " cell " << i;
                }
            }
        }
    }
}

// AC4 (part A form)
static void CheckLandCountSweep(uint64_t seedCount)
{
    const int n = kSmall.width * kSmall.height;
    for (uint64_t seed = 1; seed <= seedCount; ++seed)
    {
        Harness r;
        for (int step = 0; step <= 9; ++step)
        {
            for (float ci : { 0.0f, 0.7f, 1.0f })
            {
                WorldShapeParams s;
                s.landFraction = 0.20f + 0.05f * static_cast<float>(step);
                s.continentIslands = ci;
                r.Go(seed, s);
                const Stage1Debug& d = r.gen.GetStage1Debug();
                const int L = Land(s.landFraction, n);
                ASSERT_FALSE(d.landClamped);
                ASSERT_EQ(d.landTarget, L);
                ASSERT_EQ(Count(d.TopMask(L)), L);
                ASSERT_NEAR(static_cast<float>(L) / static_cast<float>(n), s.landFraction, 0.01f);
                ASSERT_GE(d.islandBudget, 0);
                ASSERT_LE(d.islandBudget, L);
                ASSERT_EQ(Count(d.TopMask(L - d.islandBudget)), L - d.islandBudget);
            }
        }
    }
}

TEST(TerrainLandmass, LandCountIsLandFractionAndIndependentOfContinentIslands)
{
    CheckLandCountSweep(3);   // 20 seeds run in the SLOW_ suite (Debug cost)
}

TEST(SLOW_TerrainLandmass, LandCountSweep20Seeds)
{
    CheckLandCountSweep(20);
}

TEST(TerrainLandmass, IslandBudgetFollowsShareClamp)
{
    const int n = kSmall.width * kSmall.height;
    Harness r;
    const int L = Land(0.42f, n);
    struct Case { uint8_t islands; float ci; float share; };
    const Case cases[] = { {4, 0.7f, 0.3f}, {4, 0.0f, 0.95f}, {4, 1.0f, 0.02f}, {6, 0.5f, 0.5f}, {0, 0.3f, 0.0f} };
    for (const Case& c : cases)
    {
        WorldShapeParams s; s.islands = c.islands; s.continentIslands = c.ci;
        r.Go(3, s);
        EXPECT_EQ(r.gen.GetStage1Debug().islandBudget, Land(c.share, L)) << "ci " << c.ci << " islands " << int(c.islands);
    }
}

TEST(TerrainLandmass, LandTargetClampedToEligibleCells)
{
    Harness r;
    WorldShapeParams s; s.landFraction = 0.65f;
    r.Go(5, s, GridDimensions{ 10, 10 });
    const Stage1Debug& d = r.gen.GetStage1Debug();
    EXPECT_TRUE(d.landClamped);
    EXPECT_EQ(d.landTargetRequested, 65);
    EXPECT_EQ(d.eligibleCount, 36);
    EXPECT_EQ(d.landTarget, 36);
    EXPECT_EQ(Count(d.TopMask(d.landTarget)), 36);
}

// AC5
static void CheckSubsetSweep(uint64_t seedCount)
{
    for (uint64_t seed = 1; seed <= seedCount; ++seed)
    {
        Harness r;
        std::vector<uint8_t> prev;
        for (int step = 0; step <= 9; ++step)
        {
            WorldShapeParams s;
            s.landFraction = 0.20f + 0.05f * static_cast<float>(step);
            r.Go(seed, s);
            const auto cur = r.gen.GetStage1Debug().TopMask(r.gen.GetStage1Debug().landTarget);
            for (size_t i = 0; i < prev.size(); ++i)
            {
                ASSERT_LE(prev[i], cur[i]) << "seed " << seed << " step " << step << " cell " << i;
            }
            prev = cur;
        }
    }
}

TEST(TerrainLandmass, LowerLandFractionIsSubsetOfHigher)
{
    CheckSubsetSweep(4);
}

TEST(SLOW_TerrainLandmass, SubsetSweep20Seeds)
{
    CheckSubsetSweep(20);
}

// AC8. Total land is held at L, so lobe cells displace others in the top-L set; sector growth is therefore
// measured on the potential M: cells in the lobe's sector with M >= the land threshold of the 0-peninsula map
// (M of its L-th ranked cell). Adding lobe j must raise that count above the (j-1)-lobe map's count.
static void CheckPeninsulaSweep(uint64_t seedCount)
{
    const int n = kSmall.width * kSmall.height;
    for (uint64_t seed = 1; seed <= seedCount; ++seed)
    {
        Harness r;
        WorldShapeParams s; s.peninsulas = 0;
        r.Go(seed, s);
        const SeedFields sf = r.gen.GetSeedFields();
        const Stage1Debug& d0 = r.gen.GetStage1Debug();
        const float threshold = Potential(r.gen)[d0.rank[static_cast<size_t>(d0.landTarget) - 1]];
        std::vector<float> prevM(Potential(r.gen), Potential(r.gen) + n);
        for (int pen = 1; pen <= 3; ++pen)
        {
            s.peninsulas = static_cast<uint8_t>(pen);
            r.Go(seed, s);
            ASSERT_EQ(r.gen.GetSeedFields(), sf);   // directions are stage-0 draws
            ASSERT_EQ(Count(r.gen.GetStage1Debug().TopMask(r.gen.GetStage1Debug().landTarget)), Land(s.landFraction, n));
            const float phi = sf.peninsulaAngles[pen - 1];
            const int before = SectorCount(prevM.data(), sf, phi, threshold, kSmall);
            const int after = SectorCount(Potential(r.gen), sf, phi, threshold, kSmall);
            EXPECT_GT(after, before) << "seed " << seed << " pen " << pen;
            prevM.assign(Potential(r.gen), Potential(r.gen) + n);
        }
    }
}

TEST(TerrainLandmass, PeninsulasKeepDirectionsGrowSectorsKeepLandCount)
{
    CheckPeninsulaSweep(4);
}

TEST(SLOW_TerrainLandmass, PeninsulaSweep20Seeds)
{
    CheckPeninsulaSweep(20);
}

TEST(TerrainLandmass, SecondContinentAddsBlobKeepingFirst)
{
    Harness a, b;
    WorldShapeParams s1; s1.continents = 1;
    WorldShapeParams s2; s2.continents = 2;
    a.Go(11, s1);
    b.Go(11, s2);
    EXPECT_EQ(a.gen.GetSeedFields().continents[0], b.gen.GetSeedFields().continents[0]);
    const float* pa = Potential(a.gen);
    const float* pb = Potential(b.gen);
    int raised = 0;
    for (int i = 0; i < kSmall.width * kSmall.height; ++i)
    {
        if (std::isinf(pa[i])) { continue; }
        EXPECT_GE(pb[i], pa[i]);   // max() over more blobs never lowers M
        if (pb[i] > pa[i]) { ++raised; }
    }
    EXPECT_GT(raised, 0);
}

TEST(TerrainLandmass, CoastlineChangesPotentialNotStageZero)
{
    Harness a, b;
    WorldShapeParams s0; s0.coastline = 0.0f;
    WorldShapeParams s1; s1.coastline = 1.0f;
    a.Go(13, s0);
    b.Go(13, s1);
    EXPECT_EQ(a.gen.GetSeedFields(), b.gen.GetSeedFields());
    const float* pa = Potential(a.gen);
    const float* pb = Potential(b.gen);
    int differing = 0;
    for (int i = 0; i < kSmall.width * kSmall.height; ++i)
    {
        if (!std::isinf(pa[i]) && pa[i] != pb[i]) { ++differing; }
    }
    EXPECT_GT(differing, 100);
}

TEST(TerrainLandmass, DirtyBits)
{
    Harness r;
    r.Go(21, WorldShapeParams{});
    WorldShapeParams s;
    s.rivers = 0.9f;
    r.Go(21, s);
    EXPECT_FALSE(r.gen.GetTimings()[static_cast<int>(TerrainStage::Landmass)].ranLastUpdate);
    s.landFraction = 0.5f;
    r.Go(21, s);
    for (int st = 1; st <= 10; ++st)   // stage 11 (Latitude) reads the grid only
    {
        EXPECT_TRUE(r.gen.GetTimings()[st].ranLastUpdate) << "stage " << st;
    }
    EXPECT_FALSE(r.gen.GetTimings()[0].ranLastUpdate);
}

TEST(TerrainLandmass, TimingReport)
{
    Harness r;
    r.Go(1, WorldShapeParams{});
    RecordProperty("Stage1Ms", std::to_string(r.gen.GetTimings()[1].milliseconds));
    EXPECT_TRUE(r.gen.GetTimings()[1].ranLastUpdate);
}

TEST(SLOW_TerrainLandmass, WideSweepBudgetsSubsetsAndBorder)
{
    const GridDimensions dims[] = { {33, 21}, {66, 42}, {100, 64} };
    for (const GridDimensions& d : dims)
    {
        const int n = d.width * d.height;
        for (uint64_t seed = 100; seed < 160; ++seed)
        {
            Harness r;
            std::vector<uint8_t> prev;
            for (int step = 0; step <= 9; ++step)
            {
                WorldShapeParams s;
                s.landFraction = 0.20f + 0.05f * static_cast<float>(step);
                s.continents = static_cast<uint8_t>(1 + seed % 3);
                s.peninsulas = static_cast<uint8_t>(seed % 4);
                s.coastline = static_cast<float>(seed % 5) * 0.25f;
                r.Go(seed, s, d);
                const auto cur = r.gen.GetStage1Debug().TopMask(r.gen.GetStage1Debug().landTarget);
                ASSERT_EQ(Count(cur), Land(s.landFraction, n));
                for (size_t i = 0; i < prev.size(); ++i) { ASSERT_LE(prev[i], cur[i]); }
                for (int i = 0; i < n; ++i)
                {
                    if (EdgeSteps(i % d.width, i / d.width, d) < 2) { ASSERT_EQ(cur[i], 0); }
                }
                prev = cur;
            }
        }
    }
}

// ===========================================================================
// Task 5: Stage 1 part B (islands, clean-up, landmasses, coast distance)
// ===========================================================================
namespace {

using Dia::Pathfinding::HexTopology;
using Dia::Pathfinding::CellCoord;
using Dia::Pathfinding::kNoCell;

const uint32_t* UIntLayer(const TerrainGenerator& g, const char* name)
{
    return g.GetLayers().GetUInt(g.GetLayers().FindLayer(name));
}
const float* FloatLayer(const TerrainGenerator& g, const char* name)
{
    return g.GetLayers().GetFloat(g.GetLayers().FindLayer(name));
}

bool NextTo(const HexTopology& t, int cell, const std::vector<uint8_t>& mask)
{
    for (uint32_t d = 0; d < Dia::Pathfinding::kHexDirections; ++d)
    {
        const uint32_t nb = t.NeighbourAt(static_cast<uint32_t>(cell), d);
        if (nb != kNoCell && mask[nb]) { return true; }
    }
    return false;
}

int HexDist(const HexTopology& t, int a, int b)
{
    return static_cast<int>(HexTopology::Distance(t.ToCoord(static_cast<uint32_t>(a)), t.ToCoord(static_cast<uint32_t>(b))));
}

// Structural checks of the current generator state against the spec stage-1 guarantees.
void CheckStage1Invariants(const TerrainGenerator& g, const WorldShapeParams& s, GridDimensions d, const std::string& ctx)
{
    const int n = d.width * d.height;
    const HexTopology topo(d.width, d.height);
    const Stage1Debug& dbg = g.GetStage1Debug();
    const Effective& eff = g.GetEffective();
    const uint32_t* landmass = UIntLayer(g, "landmass");
    const uint32_t* coast = UIntLayer(g, "coastDistance");
    const uint32_t* volc = UIntLayer(g, "volcanic");
    const float* ip = FloatLayer(g, "islandPotential");
    ASSERT_NE(landmass, nullptr) << ctx;
    ASSERT_EQ(static_cast<int>(dbg.refMainland.size()), n) << ctx;
    ASSERT_EQ(static_cast<int>(dbg.mainland.size()), n) << ctx;
    ASSERT_EQ(static_cast<int>(dbg.islandMask.size()), n) << ctx;
    ASSERT_EQ(static_cast<int>(dbg.islandGroup.size()), n) << ctx;

    int land = 0, islandCells = 0;
    for (int i = 0; i < n; ++i)
    {
        const bool isLand = dbg.mainland[i] || dbg.islandMask[i];
        ASSERT_EQ(landmass[i] != 0, isLand) << ctx << " cell " << i;
        ASSERT_FALSE(dbg.mainland[i] && dbg.islandMask[i]) << ctx << " cell " << i;
        land += isLand ? 1 : 0;
        if (dbg.islandMask[i])
        {
            ++islandCells;
            ASSERT_GE(dbg.islandGroup[i], 0) << ctx;
            ASSERT_LT(dbg.islandGroup[i], eff.islands) << ctx;
            ASSERT_GT(ip[i], 0.0f) << ctx << " cell " << i;
            ASSERT_FALSE(dbg.refMainland[i]) << ctx << " cell " << i;
            ASSERT_FALSE(NextTo(topo, i, dbg.refMainland)) << ctx << " island next to M_ref " << i;
            ASSERT_FALSE(NextTo(topo, i, dbg.mainland)) << ctx << " island next to mainland " << i;
            ASSERT_GE(EdgeSteps(i % d.width, i / d.width, d), 3) << ctx << " cell " << i;
            for (uint32_t dir = 0; dir < 6; ++dir)
            {
                const uint32_t nb = topo.NeighbourAt(static_cast<uint32_t>(i), dir);
                if (nb != kNoCell && dbg.islandMask[nb])
                {
                    ASSERT_EQ(dbg.islandGroup[nb], dbg.islandGroup[i]) << ctx << " cross-group adjacency at " << i;
                }
            }
        }
        else
        {
            ASSERT_EQ(dbg.islandGroup[i], -1) << ctx;
        }
    }
    ASSERT_EQ(land, dbg.landCells) << ctx;
    ASSERT_EQ(islandCells, dbg.islandCellsPlaced) << ctx;

    // Land total (AC4 full form), unless the target was clamped by the grid.
    if (!dbg.landClamped)
    {
        ASSERT_NEAR(static_cast<float>(land) / static_cast<float>(n), s.landFraction, 0.01f) << ctx;
    }

    // Island budget with overflow.
    ASSERT_LE(eff.islands, static_cast<int>(s.islands)) << ctx;
    ASSERT_EQ(static_cast<int>(eff.islandCentres.size()), eff.islands) << ctx;
    const int expectPlaced = eff.islands == 0 ? 0 : std::min(dbg.islandCapacity, std::max(dbg.islandBudget, 3 * eff.islands));
    ASSERT_EQ(islandCells, expectPlaced) << ctx;
    ASSERT_FLOAT_EQ(eff.islandShare, land > 0 ? static_cast<float>(islandCells) / static_cast<float>(land) : 0.0f) << ctx;
    ASSERT_EQ(eff.continents, static_cast<int>(s.continents)) << ctx;

    // Accepted group centres (step 6).
    for (int a = 0; a < eff.islands; ++a)
    {
        const int c = eff.islandCentres[a];
        ASSERT_TRUE(dbg.islandMask[c]) << ctx << " centre " << a << " must be island land";
        ASSERT_EQ(dbg.islandGroup[c], a) << ctx;
        ASSERT_FALSE(dbg.refMainland[c]) << ctx;
        ASSERT_GE(EdgeSteps(c % d.width, c / d.width, d), 3) << ctx;
        int best = 1 << 20;
        for (int i = 0; i < n; ++i)
        {
            if (dbg.refMainland[i]) { best = std::min(best, HexDist(topo, c, i)); }
        }
        ASSERT_GE(best, 2) << ctx << " centre " << a;
        ASSERT_LE(best, 7) << ctx << " centre " << a;
        for (int b = 0; b < a; ++b)
        {
            ASSERT_GE(HexDist(topo, c, eff.islandCentres[b]), 6) << ctx << " centres " << a << "," << b;
        }
    }

    // Vent: only on group 0's centre.
    int vents = 0;
    for (int i = 0; i < n; ++i)
    {
        if (volc[i])
        {
            ++vents;
            ASSERT_TRUE(dbg.islandMask[i]) << ctx;
            ASSERT_EQ(dbg.islandGroup[i], 0) << ctx;
        }
    }
    if (eff.islands >= 1) { ASSERT_EQ(vents, 1) << ctx; ASSERT_EQ(volc[eff.islandCentres[0]], 1u) << ctx; }
    else { ASSERT_EQ(vents, 0) << ctx; }

    // Landmass labelling: numbered by smallest cell index; adjacent land shares a label.
    uint32_t nextId = 1;
    for (int i = 0; i < n; ++i)
    {
        if (landmass[i] == 0) { continue; }
        ASSERT_LE(landmass[i], nextId) << ctx << " labels must first appear in index order";
        if (landmass[i] == nextId) { ++nextId; }
        for (uint32_t dir = 0; dir < 6; ++dir)
        {
            const uint32_t nb = topo.NeighbourAt(static_cast<uint32_t>(i), dir);
            if (nb != kNoCell && landmass[nb] != 0) { ASSERT_EQ(landmass[nb], landmass[i]) << ctx << " adjacent land cells share a label"; }
        }
    }
    if (dbg.protectedPeak >= 0)
    {
        ASSERT_TRUE(dbg.mainland[dbg.protectedPeak]) << ctx;
        ASSERT_EQ(static_cast<int>(landmass[dbg.protectedPeak]), eff.mainLandmass) << ctx;
        ASSERT_GT(eff.mainLandmass, 0) << ctx;
        const float* M = Potential(g);
        for (int i = 0; i < n; ++i)
        {
            if (dbg.mainland[i]) { ASSERT_LE(M[i], M[dbg.protectedPeak]) << ctx; }
        }
    }

    // Coast distance.
    for (int i = 0; i < n; ++i)
    {
        const bool isLand = landmass[i] != 0;
        bool adjOpposite = false;
        for (uint32_t dir = 0; dir < 6; ++dir)
        {
            const uint32_t nb = topo.NeighbourAt(static_cast<uint32_t>(i), dir);
            if (nb != kNoCell && (landmass[nb] != 0) != isLand) { adjOpposite = true; }
        }
        ASSERT_GE(coast[i], 1u) << ctx << " cell " << i;
        ASSERT_EQ(coast[i] == 1u, adjOpposite) << ctx << " cell " << i;
        for (uint32_t dir = 0; dir < 6; ++dir)
        {
            const uint32_t nb = topo.NeighbourAt(static_cast<uint32_t>(i), dir);
            if (nb != kNoCell && (landmass[nb] != 0) == isLand)
            {
                const uint32_t a = coast[i], b = coast[nb];
                ASSERT_LE(a > b ? a - b : b - a, 1u) << ctx << " coast distance is 1-Lipschitz";
            }
        }
    }
}

} // namespace

TEST(TerrainLandmass, PartBLayersRegistered)
{
    Harness r;
    r.Go(7, WorldShapeParams{});
    struct L { const char* name; LayerType type; };
    for (const L& l : { L{"islandPotential", LayerType::Float}, L{"landmass", LayerType::UInt},
                        L{"coastDistance", LayerType::UInt}, L{"volcanic", LayerType::UInt} })
    {
        const int idx = r.gen.GetLayers().FindLayer(l.name);
        ASSERT_GE(idx, 0) << l.name;
        EXPECT_EQ(TerrainLayers::GetLayerInfo(idx).type, l.type) << l.name;
        EXPECT_EQ(TerrainLayers::GetLayerInfo(idx).stage, TerrainStage::Landmass) << l.name;
    }
    EXPECT_NE(UIntLayer(r.gen, "landmass"), nullptr);
}

TEST(TerrainLandmass, CleanUpRemovesSmallComponentsKeepsLarge)
{
    const GridDimensions d{ 12, 12 };
    std::vector<uint8_t> m(144, 0);
    const HexTopology t(12, 12);
    // 5-cell blob around (3,3) is removed; 7-cell flower around (8,8) is kept.
    auto flower = [&](int cx, int cy, int count)
    {
        const int c = static_cast<int>(t.ToIndex(CellCoord{ cx, cy }));
        m[c] = 1;
        for (uint32_t dir = 0; dir < static_cast<uint32_t>(count - 1); ++dir) { m[t.NeighbourAt(c, dir)] = 1; }
    };
    flower(3, 3, 5);
    flower(8, 8, 7);
    CleanUpLand(m, d, 6, -1);
    int small = 0, big = 0;
    for (int i = 0; i < 144; ++i)
    {
        const CellCoord c = t.ToCoord(i);
        (c.x <= 5 ? small : big) += m[i];
    }
    EXPECT_EQ(small, 0);
    EXPECT_EQ(big, 7);
}

TEST(TerrainLandmass, CleanUpProtectedCellKeepsItsSmallComponent)
{
    const GridDimensions d{ 12, 12 };
    const HexTopology t(12, 12);
    std::vector<uint8_t> m(144, 0);
    const int peak = static_cast<int>(t.ToIndex(CellCoord{ 6, 6 }));
    m[peak] = 1;
    CleanUpLand(m, d, 6, peak);
    EXPECT_EQ(m[peak], 1);
    int total = 0;
    for (uint8_t v : m) { total += v; }
    EXPECT_EQ(total, 1);
    CleanUpLand(m, d, 6, -1);
    EXPECT_EQ(m[peak], 0);
}

TEST(TerrainLandmass, CleanUpFillsEnclosedSeaButNotOpenBay)
{
    const GridDimensions d{ 14, 14 };
    const HexTopology t(14, 14);
    // Enclosed: ring of 6 around (4,4). Open bay: ring of 5 around (9,9), one cell missing.
    std::vector<uint8_t> m(196, 0);
    const int enclosed = static_cast<int>(t.ToIndex(CellCoord{ 4, 4 }));
    for (uint32_t dir = 0; dir < 6; ++dir) { m[t.NeighbourAt(enclosed, dir)] = 1; }
    const int bay = static_cast<int>(t.ToIndex(CellCoord{ 9, 9 }));
    for (uint32_t dir = 0; dir < 5; ++dir) { m[t.NeighbourAt(bay, dir)] = 1; }
    CleanUpLand(m, d, 5, -1);
    EXPECT_EQ(m[enclosed], 1) << "enclosed sea becomes land";
    EXPECT_EQ(m[bay], 0) << "bay open to the ocean stays ocean";
}

TEST(TerrainLandmass, CleanUpRemovalHappensBeforeFill)
{
    // A 5-cell arc is removed entirely; nothing is filled.
    const GridDimensions d{ 14, 14 };
    const HexTopology t(14, 14);
    std::vector<uint8_t> m(196, 0);
    const int c = static_cast<int>(t.ToIndex(CellCoord{ 6, 6 }));
    for (uint32_t dir = 0; dir < 5; ++dir) { m[t.NeighbourAt(c, dir)] = 1; }
    CleanUpLand(m, d, 6, -1);
    for (uint8_t v : m) { EXPECT_EQ(v, 0); }
}

TEST(TerrainLandmass, LabelLandmassesNumbersBySmallestCellIndex)
{
    const GridDimensions d{ 8, 6 };
    const HexTopology t(8, 6);
    std::vector<uint8_t> m(48, 0);
    m[t.ToIndex(CellCoord{ 6, 4 })] = 1;
    const int b = static_cast<int>(t.ToIndex(CellCoord{ 1, 1 }));
    m[b] = 1;
    m[t.NeighbourAt(b, 4)] = 1;
    std::vector<uint32_t> out(48, 99);
    EXPECT_EQ(LabelLandmasses(m, d, out.data()), 2);
    EXPECT_EQ(out[b], 1u);
    EXPECT_EQ(out[t.NeighbourAt(b, 4)], 1u);
    EXPECT_EQ(out[t.ToIndex(CellCoord{ 6, 4 })], 2u);
    int zeros = 0;
    for (uint32_t v : out) { zeros += v == 0 ? 1 : 0; }
    EXPECT_EQ(zeros, 45);
}

TEST(TerrainLandmass, CoastDistanceHandBuiltGrid)
{
    // 12x12 map: land = hex disc of radius 2 around (6,6).
    const GridDimensions d{ 12, 12 };
    const HexTopology t(12, 12);
    const int centre = static_cast<int>(t.ToIndex(CellCoord{ 6, 6 }));
    std::vector<uint8_t> m(144, 0);
    for (int i = 0; i < 144; ++i) { if (HexDist(t, i, centre) <= 2) { m[i] = 1; } }
    std::vector<uint32_t> out(144, 77);
    ComputeCoastDistance(m, d, out.data());
    for (int i = 0; i < 144; ++i)
    {
        const int hd = HexDist(t, i, centre);
        const uint32_t expect = hd <= 2 ? static_cast<uint32_t>(3 - hd) : static_cast<uint32_t>(hd - 2);
        EXPECT_EQ(out[i], expect) << "cell " << i << " hex distance " << hd;
    }
}

TEST(TerrainLandmass, DefaultParamsInvariantsAndDeterminism)
{
    for (uint64_t seed = 1; seed <= 3; ++seed)
    {
        Harness a, b;
        a.Go(seed, WorldShapeParams{});
        b.Go(seed, WorldShapeParams{});
        CheckStage1Invariants(a.gen, WorldShapeParams{}, kSmall, "seed " + std::to_string(seed));
        const int n = kSmall.width * kSmall.height;
        for (const char* name : { "landmass", "coastDistance", "volcanic" })
        {
            const uint32_t* ua = UIntLayer(a.gen, name);
            const uint32_t* ub = UIntLayer(b.gen, name);
            ASSERT_NE(ua, nullptr);
            ASSERT_EQ(std::memcmp(ua, ub, sizeof(uint32_t) * n), 0) << name;
        }
        ASSERT_EQ(std::memcmp(FloatLayer(a.gen, "islandPotential"), FloatLayer(b.gen, "islandPotential"), sizeof(float) * n), 0);
        EXPECT_EQ(a.gen.GetEffective().islandCentres, b.gen.GetEffective().islandCentres);
        EXPECT_GE(a.gen.GetEffective().islands, 1) << "default params should place islands on a 66x42 map, seed " << seed;
    }
}

// AC4 full form + invariants over the whole landFraction x continentIslands sweep.
static void CheckLandTotals(uint64_t seedCount)
{
    for (uint64_t seed = 1; seed <= seedCount; ++seed)
    {
        Harness r;
        for (int step = 0; step <= 9; ++step)
        {
            for (float ci : { 0.0f, 0.7f, 1.0f })
            {
                WorldShapeParams s;
                s.landFraction = 0.20f + 0.05f * static_cast<float>(step);
                s.continentIslands = ci;
                r.Go(seed, s);
                CheckStage1Invariants(r.gen, s, kSmall, "seed " + std::to_string(seed) + " step " + std::to_string(step) + " ci " + std::to_string(ci));
                if (::testing::Test::HasFatalFailure()) { return; }
            }
        }
    }
}

TEST(TerrainLandmass, TotalLandWithinOnePercentAfterCleanUp)
{
    CheckLandTotals(3);
}

TEST(SLOW_TerrainLandmass, TotalLandSweep20Seeds)
{
    CheckLandTotals(20);
}

// AC5 (final mainland after clean-up, fixed ContinentIslands).
static void CheckMainlandSubset(uint64_t seedCount)
{
    for (uint64_t seed = 1; seed <= seedCount; ++seed)
    {
        for (float ci : { 0.0f, 0.7f, 1.0f })
        {
            Harness r;
            std::vector<uint8_t> prev;
            for (int step = 0; step <= 9; ++step)
            {
                WorldShapeParams s;
                s.landFraction = 0.20f + 0.05f * static_cast<float>(step);
                s.continentIslands = ci;
                r.Go(seed, s);
                const auto cur = r.gen.GetStage1Debug().mainland;
                for (size_t i = 0; i < prev.size(); ++i)
                {
                    ASSERT_LE(prev[i], cur[i]) << "seed " << seed << " ci " << ci << " step " << step << " cell " << i;
                }
                prev = cur;
            }
        }
    }
}

TEST(TerrainLandmass, MainlandGrowsMonotonicallyWithLandFraction)
{
    CheckMainlandSubset(3);
}

TEST(SLOW_TerrainLandmass, MainlandSubsetSweep20Seeds)
{
    CheckMainlandSubset(20);
}

// AC5b
static void CheckContinentIslandsSweep(uint64_t seedCount)
{
    const int n = kSmall.width * kSmall.height;
    for (uint64_t seed = 1; seed <= seedCount; ++seed)
    {
        Harness r;
        float prevShare = 2.0f;
        std::vector<uint8_t> prevMainland, prevIslands;
        std::vector<int> prevCentres;
        for (int step = 0; step <= 10; ++step)
        {
            WorldShapeParams s;
            s.islands = 4;
            s.continentIslands = 0.1f * static_cast<float>(step);
            r.Go(seed, s);
            const std::string ctx = "seed " + std::to_string(seed) + " ci " + std::to_string(s.continentIslands);
            CheckStage1Invariants(r.gen, s, kSmall, ctx);
            if (::testing::Test::HasFatalFailure()) { return; }
            const Stage1Debug& d = r.gen.GetStage1Debug();
            const Effective& e = r.gen.GetEffective();
            ASSERT_LE(e.islandShare, prevShare + 1e-6f) << ctx;
            prevShare = e.islandShare;
            if (step == 0)
            {
                // Share 0.95 overflows island capacity: the placed share is the capacity.
                ASSERT_GT(d.islandBudget, d.islandCapacity) << ctx;
                ASSERT_EQ(d.islandCellsPlaced, d.islandCapacity) << ctx;
            }
            if (step == 10) { ASSERT_LE(e.islandShare, 0.05f) << ctx; }
            if (step > 0)
            {
                ASSERT_EQ(e.islandCentres, prevCentres) << ctx;
                for (int i = 0; i < n; ++i)
                {
                    ASSERT_GE(d.mainland[i], prevMainland[i]) << ctx << " mainland superset, cell " << i;
                    ASSERT_LE(d.islandMask[i], prevIslands[i]) << ctx << " island subset, cell " << i;
                }
            }
            prevMainland = d.mainland;
            prevIslands = d.islandMask;
            prevCentres = e.islandCentres;
        }
    }
}

TEST(TerrainLandmass, ContinentIslandsSweepShrinksIslandsKeepsLand)
{
    CheckContinentIslandsSweep(3);
}

TEST(SLOW_TerrainLandmass, ContinentIslandsSweep20Seeds)
{
    CheckContinentIslandsSweep(20);
}

// AC6 (stage-1 part)
static void CheckMainLandmass(uint64_t seedCount, int* largerOtherCount)
{
    for (uint64_t seed = 1; seed <= seedCount; ++seed)
    {
        for (float ci : { 0.0f, 0.7f, 1.0f })
        {
            Harness r;
            for (uint8_t cont = 1; cont <= 3; ++cont)
            {
                WorldShapeParams s;
                s.continents = cont;
                s.continentIslands = ci;
                r.Go(seed, s);
                const std::string ctx = "seed " + std::to_string(seed) + " ci " + std::to_string(ci) + " continents " + std::to_string(cont);
                CheckStage1Invariants(r.gen, s, kSmall, ctx);
                if (::testing::Test::HasFatalFailure()) { return; }
                const Stage1Debug& d = r.gen.GetStage1Debug();
                const uint32_t* lm = UIntLayer(r.gen, "landmass");
                const float* M = Potential(r.gen);
                int peak = -1;
                for (int i = 0; i < kSmall.width * kSmall.height; ++i)
                {
                    if (d.mainland[i] && (peak < 0 || M[i] > M[peak])) { peak = i; }
                }
                ASSERT_GE(peak, 0) << ctx;
                const int mainId = r.gen.GetEffective().mainLandmass;
                ASSERT_EQ(static_cast<int>(lm[peak]), mainId) << ctx;
                std::vector<int> size(256, 0);
                for (int i = 0; i < kSmall.width * kSmall.height; ++i) { if (lm[i] && lm[i] < 256) { ++size[lm[i]]; } }
                for (int id = 1; id < 256; ++id)
                {
                    if (id != mainId && size[id] > size[mainId])
                    {
                        if (largerOtherCount) { ++*largerOtherCount; }
                        break;
                    }
                }
            }
        }
    }
}

TEST(TerrainLandmass, MainLandmassContainsMainlandPeak)
{
    int larger = 0;
    CheckMainLandmass(3, &larger);
    RecordProperty("CasesWhereAnotherLandmassIsLarger", larger);
}

TEST(SLOW_TerrainLandmass, MainLandmassSweep20Seeds)
{
    int larger = 0;
    CheckMainLandmass(20, &larger);
    RecordProperty("CasesWhereAnotherLandmassIsLarger", larger);
    std::printf("[ INFO ] main landmass != largest in %d of %d cases\n", larger, 20 * 3 * 3);
}

// AC7 (stage-1 part)
static void CheckIslandCountSweep(uint64_t seedCount)
{
    for (uint64_t seed = 1; seed <= seedCount; ++seed)
    {
        Harness r;
        std::vector<int> prevCentres;
        for (int islands = 0; islands <= 6; ++islands)
        {
            WorldShapeParams s;
            s.islands = static_cast<uint8_t>(islands);
            r.Go(seed, s);
            const std::string ctx = "seed " + std::to_string(seed) + " islands " + std::to_string(islands);
            CheckStage1Invariants(r.gen, s, kSmall, ctx);
            if (::testing::Test::HasFatalFailure()) { return; }
            const std::vector<int>& c = r.gen.GetEffective().islandCentres;
            ASSERT_LE(c.size(), static_cast<size_t>(islands)) << ctx;
            ASSERT_GE(c.size(), prevCentres.size()) << ctx;
            for (size_t i = 0; i < prevCentres.size(); ++i) { ASSERT_EQ(c[i], prevCentres[i]) << ctx << " prefix " << i; }
            prevCentres = c;
        }
    }
}

TEST(TerrainLandmass, IslandGroupsArePrefixStableAndVentIsOnFirstGroup)
{
    CheckIslandCountSweep(3);
}

TEST(SLOW_TerrainLandmass, IslandCountSweep20Seeds)
{
    CheckIslandCountSweep(20);
}

TEST(TerrainLandmass, IslandsFewerThanRequestedWhenCandidatesRunOut)
{
    // A tiny map has no room for 6 islands that satisfy the gap, spacing and edge rules.
    Harness r;
    WorldShapeParams s; s.islands = 6;
    r.Go(3, s, GridDimensions{ 20, 14 });
    EXPECT_LT(r.gen.GetEffective().islands, 6);
    CheckStage1Invariants(r.gen, s, GridDimensions{ 20, 14 }, "tiny map");
}
