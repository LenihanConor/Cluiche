// S-1 Terrain Generator task 4: Stage 1 Landmass part A (potential M, land/island budgets, reference mainland).
#include <gtest/gtest.h>
#include <Shared/WorldGeneration/TerrainGenerator.h>

#include <DiaPathfinding/HexTopology.h>

#include <cmath>
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
                ASSERT_EQ(Count(d.ReferenceMainland()), L);
                ASSERT_NEAR(static_cast<float>(L) / static_cast<float>(n), s.landFraction, 0.01f);
                ASSERT_GE(d.islandBudget, 0);
                ASSERT_LE(d.islandBudget, L);
                ASSERT_EQ(Count(d.Mainland(d.islandBudget)), L - d.islandBudget);
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
    EXPECT_EQ(Count(d.ReferenceMainland()), 36);
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
            const auto cur = r.gen.GetStage1Debug().ReferenceMainland();
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
            ASSERT_EQ(Count(r.gen.GetStage1Debug().ReferenceMainland()), Land(s.landFraction, n));
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
                const auto cur = r.gen.GetStage1Debug().ReferenceMainland();
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
