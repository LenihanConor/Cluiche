// S-1 Terrain Generator task 7: Stage 3 Barrier (barrierMask, chain vents) and Stage 4 Elevation
// (elevationRaw, oceanDepth), plus the stages 0-4 timing checkpoint (TerrainPerf, Release only).
#include <gtest/gtest.h>
#include <Shared/WorldGeneration/TerrainGenerator.h>

#include <DiaPathfinding/HexTopology.h>

#include <algorithm>
#include <chrono>
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

namespace {

const GridDimensions kSmall{ 66, 42 };
const int kN = 66 * 42;

const float* F(const TerrainGenerator& g, const char* name) { return g.GetLayers().GetFloat(g.GetLayers().FindLayer(name)); }
const uint32_t* U(const TerrainGenerator& g, const char* name) { return g.GetLayers().GetUInt(g.GetLayers().FindLayer(name)); }

std::vector<float> FVec(const TerrainGenerator& g, const char* name, int n = kN)
{
    const float* p = F(g, name);
    return p != nullptr ? std::vector<float>(p, p + n) : std::vector<float>();
}

WorldShapeParams Shape(float barrier, BarrierForm form, float mountainousness = 0.55f, float ruggedness = 0.4f, uint8_t passes = 2)
{
    WorldShapeParams s;
    s.passes = passes;
    s.barrier = barrier;
    s.barrierForm = form;
    s.mountainousness = mountainousness;
    s.ruggedness = ruggedness;
    return s;
}

struct Gen
{
    TerrainGenerator gen;
    RandomStream rng{ 1 };
    Gen() : gen(TerrainRules::Defaults()) {}
    void Go(uint64_t seed, const WorldShapeParams& s, GridDimensions d = kSmall) { gen.Update(seed, s, d, rng); }
};

int HexDist(int a, int b, int w)
{
    return static_cast<int>(HexTopology::Distance(CellCoord{ a % w, a / w }, CellCoord{ b % w, b / w }));
}

std::set<int> Ran(const TerrainGenerator& g)
{
    std::set<int> r;
    for (int s = 0; s < static_cast<int>(TerrainStage::Count); ++s)
    {
        if (g.GetTimings()[s].ranLastUpdate) { r.insert(s); }
    }
    return r;
}

bool NearVent(const uint32_t* volc, int i, int radius, int w, int n)
{
    for (int j = 0; j < n; ++j)
    {
        if (volc[j] != 0u && HexDist(i, j, w) <= radius) { return true; }
    }
    return false;
}

} // namespace

// ---------------------------------------------------------------------------
// Stage 3: Barrier
// ---------------------------------------------------------------------------
TEST(TerrainBarrier, LayersAreRegistered)
{
    TerrainLayers layers;
    const int b = layers.FindLayer("barrierMask");
    const int e = layers.FindLayer("elevationRaw");
    const int o = layers.FindLayer("oceanDepth");
    ASSERT_GE(b, 0);
    ASSERT_GE(e, 0);
    ASSERT_GE(o, 0);
    EXPECT_EQ(TerrainLayers::GetLayerInfo(b).type, LayerType::Float);
    EXPECT_EQ(TerrainLayers::GetLayerInfo(b).stage, TerrainStage::Barrier);
    EXPECT_EQ(TerrainLayers::GetLayerInfo(e).stage, TerrainStage::Elevation);
    EXPECT_EQ(TerrainLayers::GetLayerInfo(o).stage, TerrainStage::Elevation);
}

TEST(TerrainBarrier, BStarIsLowerQuantileOverMainLandmassOnly)
{
    // 8x4 grid: landmass 1 has coast distance x+1 (values 1..8, four each); landmass 2 is deep.
    const GridDimensions d{ 8, 4 };
    std::vector<uint32_t> landmass(32, 1u), coast(32, 0u), volc(32, 0u);
    std::vector<float> mask(32, 0.0f);
    for (int i = 0; i < 32; ++i) { coast[i] = static_cast<uint32_t>(i % 8 + 1); }
    for (int i = 28; i < 32; ++i) { landmass[i] = 2u; coast[i] = 100u; }
    SeedFields seeds{};
    Stage3Debug dbg;
    Stage3Layers layers;
    layers.barrierMask = mask.data();
    layers.volcanic = volc.data();
    WorldShapeParams s = Shape(1.0f, BarrierForm::Plateau);
    ComputeStage3(TerrainRules::Defaults(), s, d, seeds, landmass.data(), coast.data(), 1, layers, dbg);
    EXPECT_EQ(dbg.mainCells, 28);
    // 28 values sorted; floor(0.75 * 27) = 20 -> the 21st value.
    std::vector<uint32_t> main;
    for (int i = 0; i < 28; ++i) { main.push_back(coast[i]); }
    std::sort(main.begin(), main.end());
    EXPECT_FLOAT_EQ(dbg.bStar, static_cast<float>(main[20]));
    for (int i = 28; i < 32; ++i)
    {
        EXPECT_EQ(mask[i], 0.0f) << i;
        EXPECT_EQ(dbg.band[i], 0.0f) << i;
    }
}

TEST(TerrainBarrier, NoMainLandmassOrEmptyGridIsEmptyAndSafe)
{
    const GridDimensions d{ 6, 4 };
    std::vector<uint32_t> landmass(24, 0u), coast(24, 0u), volc(24, 0u);
    std::vector<float> mask(24, 7.0f);
    SeedFields seeds{};
    Stage3Debug dbg;
    Stage3Layers layers;
    layers.barrierMask = mask.data();
    layers.volcanic = volc.data();
    ComputeStage3(TerrainRules::Defaults(), Shape(1.0f, BarrierForm::VolcanicChain), d, seeds,
                  landmass.data(), coast.data(), 0, layers, dbg);
    for (float m : mask) { EXPECT_EQ(m, 0.0f); }
    EXPECT_TRUE(dbg.chainVents.empty());
    EXPECT_EQ(dbg.mainCells, 0);

    // Zero-size grid and null layers must not crash.
    Stage3Layers none;
    ComputeStage3(TerrainRules::Defaults(), Shape(1.0f, BarrierForm::Plateau), GridDimensions{ 0, 0 }, seeds,
                  nullptr, nullptr, 1, none, dbg);
    EXPECT_TRUE(dbg.chainVents.empty());
}

TEST(TerrainBarrier, TinyMainLandmassIsSafe)
{
    // A single-cell main landmass: b* = its coast distance, no crash, mask within [0,1].
    const GridDimensions d{ 5, 5 };
    std::vector<uint32_t> landmass(25, 0u), coast(25, 0u), volc(25, 0u);
    std::vector<float> mask(25, 0.0f);
    landmass[12] = 1u;
    coast[12] = 1u;
    SeedFields seeds{};
    Stage3Debug dbg;
    Stage3Layers layers;
    layers.barrierMask = mask.data();
    layers.volcanic = volc.data();
    for (BarrierForm f : { BarrierForm::Plateau, BarrierForm::VolcanicChain })
    {
        ComputeStage3(TerrainRules::Defaults(), Shape(1.0f, f), d, seeds, landmass.data(), coast.data(), 1, layers, dbg);
        EXPECT_EQ(dbg.mainCells, 1);
        for (float m : mask) { EXPECT_GE(m, 0.0f); EXPECT_LE(m, 1.0f); }
        EXPECT_LE(dbg.chainVents.size(), 1u);
    }
}

TEST(TerrainBarrier, PlateauMaskShape)
{
    Gen r;
    r.Go(7, Shape(0.6f, BarrierForm::Plateau));
    const float* mask = F(r.gen, "barrierMask");
    const uint32_t* lm = U(r.gen, "landmass");
    const uint32_t* coast = U(r.gen, "coastDistance");
    ASSERT_NE(mask, nullptr);
    const int mainId = r.gen.GetEffective().mainLandmass;
    ASSERT_GT(mainId, 0);
    const float bStar = r.gen.GetStage3Debug().bStar;
    float maxMask = 0.0f;
    int deepInterior = 0;
    for (int i = 0; i < kN; ++i)
    {
        ASSERT_GE(mask[i], 0.0f);
        ASSERT_LE(mask[i], 1.0f);
        maxMask = std::max(maxMask, mask[i]);
        if (lm[i] != static_cast<uint32_t>(mainId)) { EXPECT_EQ(mask[i], 0.0f) << i; }
        else if (static_cast<float>(coast[i]) > bStar)
        {
            EXPECT_GE(mask[i], 0.5f) << i;   // high interior table
            ++deepInterior;
        }
        else if (static_cast<float>(coast[i]) < bStar - 1.5f - 1.5f)
        {
            EXPECT_EQ(mask[i], 0.0f) << i;   // outside wobble + half width
        }
    }
    EXPECT_GT(maxMask, 0.0f);
    EXPECT_GT(deepInterior, 0);
}

TEST(TerrainBarrier, ChainMaskIsHalfBandAndNeverAboveHalf)
{
    Gen r;
    r.Go(7, Shape(0.6f, BarrierForm::VolcanicChain));
    const float* mask = F(r.gen, "barrierMask");
    const std::vector<float>& band = r.gen.GetStage3Debug().band;
    ASSERT_EQ(band.size(), static_cast<size_t>(kN));
    float maxMask = 0.0f;
    for (int i = 0; i < kN; ++i)
    {
        EXPECT_FLOAT_EQ(mask[i], 0.5f * band[i]) << i;
        maxMask = std::max(maxMask, mask[i]);
    }
    EXPECT_GT(maxMask, 0.0f);
    EXPECT_LE(maxMask, 0.5f);
}

TEST(TerrainBarrier, BarrierZeroIsEmptyButKeepsIslandVent)
{
    for (BarrierForm f : { BarrierForm::Plateau, BarrierForm::VolcanicChain })
    {
        Gen r;
        r.Go(7, Shape(0.0f, f));
        const float* mask = F(r.gen, "barrierMask");
        const uint32_t* volc = U(r.gen, "volcanic");
        for (int i = 0; i < kN; ++i)
        {
            EXPECT_EQ(mask[i], 0.0f) << i;
            EXPECT_EQ(volc[i] & kVolcanicChainBit, 0u) << i;
        }
        EXPECT_TRUE(r.gen.GetStage3Debug().chainVents.empty());
        const Effective& eff = r.gen.GetEffective();
        ASSERT_GT(eff.islands, 0);
        EXPECT_EQ(volc[eff.islandCentres[0]] & kVolcanicIslandBit, kVolcanicIslandBit);
    }
}

TEST(TerrainBarrier, ChainVentsAreSpacedOnBandAndKeepIslandVent)
{
    Gen r;
    r.Go(7, Shape(0.8f, BarrierForm::VolcanicChain));
    const Stage3Debug& dbg = r.gen.GetStage3Debug();
    const uint32_t* volc = U(r.gen, "volcanic");
    const uint32_t* lm = U(r.gen, "landmass");
    ASSERT_FALSE(dbg.chainVents.empty());
    const int spacing = TerrainRules::Defaults().GetInt(Rule::kVentSpacing);
    for (size_t a = 0; a < dbg.chainVents.size(); ++a)
    {
        const int c = dbg.chainVents[a];
        EXPECT_GT(dbg.band[c], 0.0f) << c;
        EXPECT_EQ(lm[c], static_cast<uint32_t>(r.gen.GetEffective().mainLandmass));
        EXPECT_EQ(volc[c] & kVolcanicChainBit, kVolcanicChainBit);
        for (size_t b = a + 1; b < dbg.chainVents.size(); ++b)
        {
            EXPECT_GE(HexDist(c, dbg.chainVents[b], kSmall.width), spacing) << c << "," << dbg.chainVents[b];
        }
    }
    int chainBits = 0;
    for (int i = 0; i < kN; ++i) { chainBits += (volc[i] & kVolcanicChainBit) ? 1 : 0; }
    EXPECT_EQ(chainBits, static_cast<int>(dbg.chainVents.size()));
    const Effective& eff = r.gen.GetEffective();
    ASSERT_GT(eff.islands, 0);
    EXPECT_EQ(volc[eff.islandCentres[0]] & kVolcanicIslandBit, kVolcanicIslandBit);
}

TEST(TerrainBarrier, PlateauHasNoChainVents)
{
    Gen r;
    r.Go(7, Shape(0.8f, BarrierForm::Plateau));
    const uint32_t* volc = U(r.gen, "volcanic");
    for (int i = 0; i < kN; ++i) { EXPECT_EQ(volc[i] & kVolcanicChainBit, 0u) << i; }
    EXPECT_TRUE(r.gen.GetStage3Debug().chainVents.empty());
}

TEST(TerrainBarrier, SwitchingFormBackClearsChainVentsAndKeepsIslandVent)
{
    Gen r;
    r.Go(7, Shape(0.8f, BarrierForm::Plateau));
    const std::vector<uint32_t> before(U(r.gen, "volcanic"), U(r.gen, "volcanic") + kN);
    r.Go(7, Shape(0.8f, BarrierForm::VolcanicChain));
    r.Go(7, Shape(0.8f, BarrierForm::Plateau));
    EXPECT_EQ(0, std::memcmp(before.data(), U(r.gen, "volcanic"), before.size() * sizeof(uint32_t)));
}

TEST(TerrainBarrier, Deterministic)
{
    Gen a, b;
    a.Go(11, Shape(0.7f, BarrierForm::VolcanicChain));
    b.Go(11, Shape(0.7f, BarrierForm::VolcanicChain));
    for (const char* name : { "barrierMask", "elevationRaw", "oceanDepth" })
    {
        EXPECT_EQ(0, std::memcmp(F(a.gen, name), F(b.gen, name), kN * sizeof(float))) << name;
    }
    EXPECT_EQ(0, std::memcmp(U(a.gen, "volcanic"), U(b.gen, "volcanic"), kN * sizeof(uint32_t)));
    EXPECT_EQ(a.gen.GetStage3Debug().chainVents, b.gen.GetStage3Debug().chainVents);
}

TEST(TerrainBarrier, FormChangeLeavesStagesZeroToTwoIdenticalAndRerunsThreeUp)
{
    Gen r;
    r.Go(5, Shape(0.6f, BarrierForm::Plateau));
    std::vector<std::vector<uint32_t>> u;
    std::vector<std::vector<float>> f;
    const char* ulayers[] = { "landmass", "coastDistance", "passMask" };
    const char* flayers[] = { "landPotential", "islandPotential", "ridgeMask" };
    for (const char* n : ulayers) { u.emplace_back(U(r.gen, n), U(r.gen, n) + kN); }
    for (const char* n : flayers) { f.push_back(FVec(r.gen, n)); }
    const std::vector<uint32_t> volcBefore(U(r.gen, "volcanic"), U(r.gen, "volcanic") + kN);
    const std::vector<float> maskBefore = FVec(r.gen, "barrierMask");

    r.Go(5, Shape(0.6f, BarrierForm::VolcanicChain));
    EXPECT_FALSE(r.gen.GetTimings()[0].ranLastUpdate);
    EXPECT_FALSE(r.gen.GetTimings()[1].ranLastUpdate);
    EXPECT_FALSE(r.gen.GetTimings()[2].ranLastUpdate);
    EXPECT_TRUE(r.gen.GetTimings()[3].ranLastUpdate);
    EXPECT_TRUE(r.gen.GetTimings()[4].ranLastUpdate);
    for (size_t k = 0; k < u.size(); ++k)
    {
        EXPECT_EQ(0, std::memcmp(u[k].data(), U(r.gen, ulayers[k]), kN * sizeof(uint32_t))) << ulayers[k];
    }
    for (size_t k = 0; k < f.size(); ++k)
    {
        EXPECT_EQ(0, std::memcmp(f[k].data(), F(r.gen, flayers[k]), kN * sizeof(float))) << flayers[k];
    }
    // The volcanic layer is stage 1's, but stage 3 owns its chain bit; the island bit never moves.
    for (int i = 0; i < kN; ++i)
    {
        EXPECT_EQ(volcBefore[i] & kVolcanicIslandBit, U(r.gen, "volcanic")[i] & kVolcanicIslandBit) << i;
    }
    EXPECT_NE(0, std::memcmp(maskBefore.data(), F(r.gen, "barrierMask"), kN * sizeof(float)));
}

TEST(TerrainBarrier, DirtyBitsForBarrierRuggednessAndMountainousness)
{
    Gen r;
    WorldShapeParams s = Shape(0.6f, BarrierForm::Plateau);
    r.Go(5, s);

    s.barrier = 0.9f;
    r.Go(5, s);
    EXPECT_EQ(Ran(r.gen).count(1), 0u);
    EXPECT_EQ(Ran(r.gen).count(2), 0u);
    EXPECT_EQ(Ran(r.gen).count(3), 1u);
    EXPECT_EQ(Ran(r.gen).count(4), 1u);

    s.ruggedness = 0.9f;
    r.Go(5, s);
    EXPECT_EQ(Ran(r.gen).count(3), 0u);
    EXPECT_EQ(Ran(r.gen).count(4), 1u);
    EXPECT_EQ(Ran(r.gen).count(2), 0u);

    s.mountainousness = 0.9f;
    r.Go(5, s);
    EXPECT_EQ(Ran(r.gen).count(2), 1u);
    EXPECT_EQ(Ran(r.gen).count(3), 0u);   // stage 3 reads only {0,1}
    EXPECT_EQ(Ran(r.gen).count(4), 1u);
}

// ---------------------------------------------------------------------------
// Stage 4: Elevation
// ---------------------------------------------------------------------------
TEST(TerrainElevation, BoundedOnLandAndZeroInOcean)
{
    const float coastElev = TerrainRules::Defaults().kCoastElev();
    for (BarrierForm form : { BarrierForm::Plateau, BarrierForm::VolcanicChain })
    {
        Gen r;
        r.Go(3, Shape(1.0f, form, 1.0f, 1.0f));
        const float* e = F(r.gen, "elevationRaw");
        const uint32_t* lm = U(r.gen, "landmass");
        ASSERT_NE(e, nullptr);
        int land = 0;
        for (int i = 0; i < kN; ++i)
        {
            if (lm[i] == 0u) { EXPECT_EQ(e[i], 0.0f) << i; }
            else
            {
                ++land;
                EXPECT_GE(e[i], coastElev) << i;
                EXPECT_LE(e[i], 1.0f) << i;
            }
        }
        EXPECT_GT(land, 0);
    }
}

TEST(TerrainElevation, OceanDepthIsShelfRampOnOceanOnly)
{
    Gen r;
    r.Go(3, Shape(0.6f, BarrierForm::Plateau));
    const float* od = F(r.gen, "oceanDepth");
    const uint32_t* lm = U(r.gen, "landmass");
    const uint32_t* coast = U(r.gen, "coastDistance");
    const float shelf = TerrainRules::Defaults().kShelfScale();
    float maxDepth = 0.0f;
    for (int i = 0; i < kN; ++i)
    {
        if (lm[i] != 0u) { EXPECT_EQ(od[i], 0.0f) << i; }
        else
        {
            EXPECT_FLOAT_EQ(od[i], std::min(1.0f, static_cast<float>(coast[i]) / shelf)) << i;
            EXPECT_GT(od[i], 0.0f);
            maxDepth = std::max(maxDepth, od[i]);
        }
    }
    EXPECT_GT(maxDepth, 0.0f);
}

TEST(TerrainElevation, PlainLowlandMatchesBaseFormulaExactly)
{
    // Mountainousness 0, ruggedness 0, barrier 0: land far from vents is coastElev + lowland rise.
    Gen r;
    r.Go(3, Shape(0.0f, BarrierForm::Plateau, 0.0f, 0.0f));
    const TerrainRules rules = TerrainRules::Defaults();
    const float* e = F(r.gen, "elevationRaw");
    const uint32_t* lm = U(r.gen, "landmass");
    const uint32_t* coast = U(r.gen, "coastDistance");
    const uint32_t* volc = U(r.gen, "volcanic");
    int checked = 0;
    for (int i = 0; i < kN; ++i)
    {
        if (lm[i] == 0u || NearVent(volc, i, 3, kSmall.width, kN)) { continue; }
        const float d = static_cast<float>(coast[i]);
        const float expect = rules.kCoastElev() + rules.kLowlandRise() * (1.0f - std::exp(-(d - 1.0f) / rules.kInlandScale()));
        EXPECT_NEAR(e[i], expect, 1e-5f) << i;
        ++checked;
    }
    EXPECT_GT(checked, 50);
}

TEST(TerrainElevation, VentRaisesTerrainAroundIslandAndChainVents)
{
    Gen r;
    r.Go(3, Shape(0.8f, BarrierForm::VolcanicChain, 0.0f, 0.0f));
    const TerrainRules rules = TerrainRules::Defaults();
    const float* e = F(r.gen, "elevationRaw");
    const uint32_t* volc = U(r.gen, "volcanic");
    int vents = 0;
    for (int i = 0; i < kN; ++i)
    {
        if (volc[i] != 0u)
        {
            ++vents;
            EXPECT_GE(e[i], std::min(1.0f, rules.kCoastElev() + rules.kVentHeight())) << i;
        }
    }
    EXPECT_GT(vents, 1);
}

TEST(TerrainElevation, BarrierTermIsHeightTimesMask)
{
    const TerrainRules rules = TerrainRules::Defaults();
    Gen lo, hi;
    lo.Go(3, Shape(0.0f, BarrierForm::Plateau, 0.0f, 0.0f));
    hi.Go(3, Shape(1.0f, BarrierForm::Plateau, 0.0f, 0.0f));
    const float* elo = F(lo.gen, "elevationRaw");
    const float* ehi = F(hi.gen, "elevationRaw");
    const float* mask = F(hi.gen, "barrierMask");
    const uint32_t* lm = U(hi.gen, "landmass");
    const uint32_t* volc = U(hi.gen, "volcanic");
    int raised = 0;
    for (int i = 0; i < kN; ++i)
    {
        if (lm[i] == 0u || NearVent(volc, i, 3, kSmall.width, kN)) { continue; }
        EXPECT_NEAR(ehi[i], std::min(1.0f, elo[i] + rules.kBarrierHeight() * mask[i]), 1e-5f) << i;
        raised += ehi[i] > elo[i] ? 1 : 0;
    }
    EXPECT_GT(raised, 0);
}

TEST(TerrainElevation, MonotoneInMountainousnessAndBarrier)
{
    // Passes = 0: stage 2's ridgeMask is itself only monotone in mountainousness without passes
    // (the pass gap widens with the ridge width), so the elevation rule is checked on that basis.
    for (BarrierForm form : { BarrierForm::Plateau, BarrierForm::VolcanicChain })
    {
        std::vector<float> prevM;
        for (float m : { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f })
        {
            Gen r;
            r.Go(9, Shape(0.6f, form, m, 0.4f, 0));
            const std::vector<float> e = FVec(r.gen, "elevationRaw");
            if (!prevM.empty())
            {
                for (int i = 0; i < kN; ++i) { ASSERT_GE(e[i], prevM[i] - 1e-6f) << "m=" << m << " cell " << i; }
            }
            prevM = e;
        }
        std::vector<float> prevB;
        std::vector<float> prevMask;
        for (float b : { 0.0f, 0.2f, 0.5f, 0.8f, 1.0f })
        {
            Gen r;
            r.Go(9, Shape(b, form));
            const std::vector<float> e = FVec(r.gen, "elevationRaw");
            const std::vector<float> mask = FVec(r.gen, "barrierMask");
            if (!prevB.empty())
            {
                for (int i = 0; i < kN; ++i)
                {
                    ASSERT_GE(e[i], prevB[i] - 1e-6f) << "b=" << b << " cell " << i;
                    if (b > 0.0f && prevMask[i] > 0.0f) { ASSERT_EQ(mask[i], prevMask[i]) << i; }   // band fixed by landmass
                }
            }
            prevB = e;
            prevMask = mask;
        }
    }
}

TEST(TerrainElevation, RuggednessIsBoundedAndTouchesNothingElse)
{
    const float amp = TerrainRules::Defaults().kRuggedAmp();
    Gen a, b;
    a.Go(9, Shape(0.6f, BarrierForm::Plateau, 0.55f, 0.2f));
    b.Go(9, Shape(0.6f, BarrierForm::Plateau, 0.55f, 1.0f));
    for (const char* n : { "landPotential", "ridgeMask", "barrierMask", "oceanDepth" })
    {
        EXPECT_EQ(0, std::memcmp(F(a.gen, n), F(b.gen, n), kN * sizeof(float))) << n;
    }
    for (const char* n : { "landmass", "coastDistance", "passMask", "volcanic" })
    {
        EXPECT_EQ(0, std::memcmp(U(a.gen, n), U(b.gen, n), kN * sizeof(uint32_t))) << n;
    }
    const float* ea = F(a.gen, "elevationRaw");
    const float* eb = F(b.gen, "elevationRaw");
    const uint32_t* coast = U(a.gen, "coastDistance");
    const uint32_t* lm = U(a.gen, "landmass");
    float maxDelta = 0.0f;
    for (int i = 0; i < kN; ++i)
    {
        const float delta = std::fabs(ea[i] - eb[i]);
        EXPECT_LE(delta, amp * 0.8f + 1e-6f) << i;
        maxDelta = std::max(maxDelta, delta);
        if (lm[i] != 0u && coast[i] == 1u) { EXPECT_EQ(ea[i], eb[i]) << "coast cell " << i; }   // fades out over two cells
    }
    EXPECT_GT(maxDelta, 0.0f);
}

TEST(TerrainElevation, TinyAndEmptyInputsAreSafe)
{
    const GridDimensions d{ 5, 5 };
    std::vector<uint32_t> landmass(25, 0u), coast(25, 0u), volc(25, 0u);
    std::vector<float> elev(25, 9.0f), depth(25, 9.0f);
    SeedFields seeds{};
    Stage4Layers layers;
    layers.elevationRaw = elev.data();
    layers.oceanDepth = depth.data();
    ComputeStage4(TerrainRules::Defaults(), WorldShapeParams{}, d, seeds, landmass.data(), coast.data(),
                  nullptr, nullptr, nullptr, layers);
    for (int i = 0; i < 25; ++i) { EXPECT_EQ(elev[i], 0.0f); EXPECT_EQ(depth[i], 0.0f); }

    // One land cell, no ridge/barrier layers, no ocean (coast 0 elsewhere).
    landmass[12] = 1u;
    coast[12] = 1u;
    ComputeStage4(TerrainRules::Defaults(), WorldShapeParams{}, d, seeds, landmass.data(), coast.data(),
                  nullptr, nullptr, nullptr, layers);
    EXPECT_NEAR(elev[12], TerrainRules::Defaults().kCoastElev() + 0.0f, 0.2f);
    EXPECT_GE(elev[12], TerrainRules::Defaults().kCoastElev());

    ComputeStage4(TerrainRules::Defaults(), WorldShapeParams{}, GridDimensions{ 0, 0 }, seeds, nullptr, nullptr,
                  nullptr, nullptr, nullptr, layers);
}

TEST(TerrainElevation, SmallAndOddGridsDoNotCrash)
{
    for (GridDimensions d : { GridDimensions{ 1, 1 }, GridDimensions{ 3, 3 }, GridDimensions{ 8, 8 }, GridDimensions{ 20, 5 } })
    {
        for (BarrierForm form : { BarrierForm::Plateau, BarrierForm::VolcanicChain })
        {
            Gen r;
            r.Go(2, Shape(1.0f, form, 1.0f, 1.0f), d);
            const int n = d.width * d.height;
            const float* e = F(r.gen, "elevationRaw");
            ASSERT_NE(e, nullptr);
            for (int i = 0; i < n; ++i) { EXPECT_TRUE(e[i] >= 0.0f && e[i] <= 1.0f); }
        }
    }
}

// ---------------------------------------------------------------------------
// Sweeps
// ---------------------------------------------------------------------------
TEST(SLOW_TerrainElevation, StabilityRulesSweep20Seeds)
{
    const float amp = TerrainRules::Defaults().kRuggedAmp();
    const int spacing = TerrainRules::Defaults().GetInt(Rule::kVentSpacing);
    for (uint64_t seed = 1; seed <= 20; ++seed)
    {
        for (BarrierForm form : { BarrierForm::Plateau, BarrierForm::VolcanicChain })
        {
            const std::string ctx = "seed " + std::to_string(seed) + " form " + BarrierFormName(form);
            std::vector<float> prevM, prevB;
            for (float m : { 0.0f, 0.3f, 0.6f, 1.0f })
            {
                Gen r;
                r.Go(seed, Shape(0.6f, form, m, 0.4f, 0));
                const std::vector<float> e = FVec(r.gen, "elevationRaw");
                if (!prevM.empty())
                {
                    for (int i = 0; i < kN; ++i) { ASSERT_GE(e[i], prevM[i] - 1e-6f) << ctx << " m=" << m << " cell " << i; }
                }
                prevM = e;
            }
            for (float b : { 0.0f, 0.3f, 0.6f, 1.0f })
            {
                Gen r;
                r.Go(seed, Shape(b, form));
                const std::vector<float> e = FVec(r.gen, "elevationRaw");
                if (!prevB.empty())
                {
                    for (int i = 0; i < kN; ++i) { ASSERT_GE(e[i], prevB[i] - 1e-6f) << ctx << " b=" << b << " cell " << i; }
                }
                prevB = e;
                const Stage3Debug& dbg = r.gen.GetStage3Debug();
                if (b > 0.0f && form == BarrierForm::Plateau) { EXPECT_TRUE(dbg.chainVents.empty()) << ctx; }
                for (size_t a = 0; a < dbg.chainVents.size(); ++a)
                {
                    EXPECT_GT(dbg.band[dbg.chainVents[a]], 0.0f) << ctx;
                    for (size_t c = a + 1; c < dbg.chainVents.size(); ++c)
                    {
                        EXPECT_GE(HexDist(dbg.chainVents[a], dbg.chainVents[c], kSmall.width), spacing) << ctx;
                    }
                }
                const uint32_t* volc = U(r.gen, "volcanic");
                if (r.gen.GetEffective().islands > 0)
                {
                    EXPECT_EQ(volc[r.gen.GetEffective().islandCentres[0]] & kVolcanicIslandBit, kVolcanicIslandBit) << ctx;
                }
            }
            Gen lo, hi;
            lo.Go(seed, Shape(0.6f, form, 0.55f, 0.0f));
            hi.Go(seed, Shape(0.6f, form, 0.55f, 1.0f));
            const float* el = F(lo.gen, "elevationRaw");
            const float* eh = F(hi.gen, "elevationRaw");
            for (int i = 0; i < kN; ++i)
            {
                ASSERT_LE(std::fabs(el[i] - eh[i]), amp + 1e-6f) << ctx << " cell " << i;
                ASSERT_GE(eh[i], 0.0f);
                ASSERT_LE(eh[i], 1.0f);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Timing checkpoint (AC17 early measurement). Informational only: never asserts on time and is
// skipped outside Release builds.
// ---------------------------------------------------------------------------
TEST(TerrainPerf, StagesZeroToFourReport)
{
#ifdef NDEBUG
    constexpr int kRuns = 50;
    constexpr int kStages = 5;
    std::vector<double> perStage[kStages];
    std::vector<double> totals;
    Gen r;
    const WorldShapeParams s;   // defaults
    for (int run = 0; run < kRuns; ++run)
    {
        r.Go(1000 + static_cast<uint64_t>(run), s);   // new seed: every stage re-runs
        double total = 0.0;
        for (int st = 0; st < kStages; ++st)
        {
            const double ms = r.gen.GetTimings()[st].milliseconds;
            perStage[st].push_back(ms);
            total += ms;
        }
        totals.push_back(total);
    }
    auto median = [](std::vector<double> v) { std::sort(v.begin(), v.end()); return v[v.size() / 2]; };
    for (int st = 0; st < kStages; ++st)
    {
        std::printf("[TerrainPerf] stage %d median %.3f ms\n", st, median(perStage[st]));
    }
    const double total = median(totals);
    std::printf("[TerrainPerf] stages 0-4 total median %.3f ms (66x42, %d runs)%s\n", total, kRuns,
                total > 8.0 ? "  ** EXCEEDS 8 ms **" : "");
    SUCCEED();
#else
    GTEST_SKIP() << "Release-only timing report";
#endif
}
