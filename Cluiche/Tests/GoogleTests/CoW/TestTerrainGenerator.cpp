// S-1 Terrain Generator task 3: scheduling, dirty propagation, layer registry, stage 0 SeedFields.
#include <gtest/gtest.h>
#include <Shared/WorldGeneration/TerrainGenerator.h>

#include <DiaPathfinding/HexTopology.h>

#include <cmath>
#include <set>

using namespace CoW::WorldGeneration;
using Dia::Maths::RandomStream;

namespace {

const GridDimensions kDims{ 48, 32 };

std::set<int> RanStages(const TerrainGenerator& g)
{
    std::set<int> r;
    for (int s = 0; s < static_cast<int>(TerrainStage::Count); ++s)
    {
        if (g.GetTimings()[s].ranLastUpdate)
        {
            r.insert(s);
        }
    }
    return r;
}

std::set<int> ExpectedDirty(ShapeParam p)
{
    std::set<int> d;
    d.insert(static_cast<int>(FirstStageAffected(p)));
    for (int s = 1; s < kImplementedStageCount; ++s)
    {
        for (int u : d)
        {
            if (kStageReads[s] & (1u << u))
            {
                d.insert(s);
                break;
            }
        }
    }
    for (auto it = d.begin(); it != d.end();)
    {
        it = (*it >= kImplementedStageCount) ? d.erase(it) : std::next(it);
    }
    return d;
}

// Returns a copy of the defaults with the given slider changed.
WorldShapeParams Tweak(ShapeParam p)
{
    WorldShapeParams s;
    switch (p)
    {
    case ShapeParam::LandFraction:       s.landFraction = 0.5f; break;
    case ShapeParam::Continents:         s.continents = 2; break;
    case ShapeParam::Islands:            s.islands = 5; break;
    case ShapeParam::ContinentIslands:   s.continentIslands = 0.3f; break;
    case ShapeParam::Peninsulas:         s.peninsulas = 3; break;
    case ShapeParam::Coastline:          s.coastline = 0.9f; break;
    case ShapeParam::Mountainousness:    s.mountainousness = 0.9f; break;
    case ShapeParam::SpineToRing:        s.spineToRing = 0.8f; break;
    case ShapeParam::Orientation:        s.orientationDeg = 100.0f; break;
    case ShapeParam::Passes:             s.passes = 3; break;
    case ShapeParam::Barrier:            s.barrier = 0.1f; break;
    case ShapeParam::BarrierForm:        s.barrierForm = BarrierForm::VolcanicChain; break;
    case ShapeParam::Ruggedness:         s.ruggedness = 0.9f; break;
    case ShapeParam::Rivers:             s.rivers = 0.9f; break;
    case ShapeParam::ValleyDepth:        s.valleyDepth = 0.9f; break;
    case ShapeParam::Lakes:              s.lakes = 9; break;
    case ShapeParam::LatitudeNorth:      s.latitudeNorthDeg = 10.0f; break;
    case ShapeParam::LatitudeSouth:      s.latitudeSouthDeg = -10.0f; break;
    case ShapeParam::TemperatureOffset:  s.temperatureOffset = 0.2f; break;
    case ShapeParam::MoistureOffset:     s.moistureOffset = 0.2f; break;
    case ShapeParam::Count:              break;
    }
    return s;
}

} // namespace

TEST(TerrainGenerator, SliderDirtiesFirstStageAndDownstreamPerSpecTable)
{
    for (int p = 0; p < static_cast<int>(ShapeParam::Count); ++p)
    {
        const ShapeParam param = static_cast<ShapeParam>(p);
        TerrainGenerator g(TerrainRules::Defaults());
        RandomStream rng(1);
        g.Update(42, WorldShapeParams{}, kDims, rng);
        g.Update(42, Tweak(param), kDims, rng);
        EXPECT_EQ(RanStages(g), ExpectedDirty(param)) << ShapeParamName(param);
        EXPECT_FALSE(g.GetTimings()[static_cast<int>(TerrainStage::SeedFields)].ranLastUpdate) << ShapeParamName(param);
    }
}

TEST(TerrainGenerator, HardCodedDownstreamExpectations)
{
    auto run = [](ShapeParam p)
    {
        TerrainGenerator g(TerrainRules::Defaults());
        RandomStream rng(1);
        g.Update(42, WorldShapeParams{}, kDims, rng);
        g.Update(42, Tweak(p), kDims, rng);
        return RanStages(g);
    };
    EXPECT_EQ(run(ShapeParam::Rivers), (std::set<int>{ 6, 8, 9, 10 }));
    EXPECT_EQ(run(ShapeParam::Lakes), (std::set<int>{ 7, 9, 10 }));
    EXPECT_EQ(run(ShapeParam::Ruggedness), (std::set<int>{ 4, 5, 6, 7, 8, 9, 10 }));
    EXPECT_EQ(run(ShapeParam::Barrier), (std::set<int>{ 3, 4, 5, 6, 7, 8, 9, 10 }));
    EXPECT_EQ(run(ShapeParam::LandFraction), (std::set<int>{ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }));
    EXPECT_EQ(run(ShapeParam::ValleyDepth), (std::set<int>{ 9, 10 }));
    EXPECT_EQ(run(ShapeParam::LatitudeNorth), (std::set<int>{ 11 }));
    EXPECT_EQ(run(ShapeParam::TemperatureOffset), (std::set<int>{}));   // stage 12: Feature 8
}

TEST(TerrainGenerator, StageReadsMatchSpecTable)
{
    auto mask = [](std::initializer_list<int> l) { uint32_t m = 0; for (int i : l) { m |= 1u << i; } return m; };
    EXPECT_EQ(kStageReads[0], 0u);
    EXPECT_EQ(kStageReads[1], mask({ 0 }));
    EXPECT_EQ(kStageReads[2], mask({ 0, 1 }));
    EXPECT_EQ(kStageReads[3], mask({ 0, 1 }));
    EXPECT_EQ(kStageReads[4], mask({ 0, 1, 2, 3 }));
    EXPECT_EQ(kStageReads[5], mask({ 4 }));
    EXPECT_EQ(kStageReads[6], mask({ 4, 5 }));
    EXPECT_EQ(kStageReads[7], mask({ 5 }));
    EXPECT_EQ(kStageReads[8], mask({ 1, 2, 3, 6 }));
    EXPECT_EQ(kStageReads[9], mask({ 4, 5, 6, 7 }));
    EXPECT_EQ(kStageReads[10], mask({ 1, 2, 3, 6, 7, 8, 9 }));
    EXPECT_EQ(kStageReads[11], 0u);
}

TEST(TerrainGenerator, FirstUpdateRunsEveryImplementedStage)
{
    TerrainGenerator g(TerrainRules::Defaults());
    RandomStream rng(1);
    g.Update(7, WorldShapeParams{}, kDims, rng);
    std::set<int> all;
    for (int s = 0; s < kImplementedStageCount; ++s) { all.insert(s); }
    EXPECT_EQ(RanStages(g), all);
    EXPECT_GE(g.GetLastTotalMs(), 0.0f);
}

TEST(TerrainGenerator, NoChangeRunsNoStage)
{
    TerrainGenerator g(TerrainRules::Defaults());
    RandomStream rng(1);
    g.Update(7, WorldShapeParams{}, kDims, rng);
    g.Update(7, WorldShapeParams{}, kDims, rng);
    EXPECT_TRUE(RanStages(g).empty());
    EXPECT_EQ(g.GetLastTotalMs(), 0.0f);
}

TEST(TerrainGenerator, SeedDimensionAndInvalidateRunEverything)
{
    std::set<int> all;
    for (int s = 0; s < kImplementedStageCount; ++s) { all.insert(s); }

    TerrainGenerator g(TerrainRules::Defaults());
    RandomStream rng(1);
    g.Update(7, WorldShapeParams{}, kDims, rng);

    g.Update(8, WorldShapeParams{}, kDims, rng);
    EXPECT_EQ(RanStages(g), all);

    g.Update(8, WorldShapeParams{}, GridDimensions{ 40, 32 }, rng);
    EXPECT_EQ(RanStages(g), all);

    g.Update(8, WorldShapeParams{}, GridDimensions{ 40, 30 }, rng);
    EXPECT_EQ(RanStages(g), all);

    g.Update(8, WorldShapeParams{}, GridDimensions{ 40, 30 }, rng);
    EXPECT_TRUE(RanStages(g).empty());

    g.Invalidate();
    g.Update(8, WorldShapeParams{}, GridDimensions{ 40, 30 }, rng);
    EXPECT_EQ(RanStages(g), all);
}

TEST(TerrainGenerator, Stage12To14NeverRun)
{
    TerrainGenerator g(TerrainRules::Defaults());
    RandomStream rng(1);
    g.Update(7, WorldShapeParams{}, kDims, rng);
    for (int s = kImplementedStageCount; s < static_cast<int>(TerrainStage::Count); ++s)
    {
        EXPECT_FALSE(g.GetTimings()[s].ranLastUpdate);
    }
}

TEST(TerrainGenerator, SeedFieldsDeterministicForSameSeed)
{
    TerrainGenerator a(TerrainRules::Defaults());
    TerrainGenerator b(TerrainRules::Defaults());
    RandomStream ra(1), rb(999);   // initial stream state must not matter
    a.Update(1234, WorldShapeParams{}, kDims, ra);
    b.Update(1234, WorldShapeParams{}, kDims, rb);
    EXPECT_TRUE(a.GetSeedFields() == b.GetSeedFields());
}

TEST(TerrainGenerator, SeedFieldsUnchangedByDownstreamSliderChange)
{
    TerrainGenerator g(TerrainRules::Defaults());
    RandomStream rng(1);
    g.Update(55, WorldShapeParams{}, kDims, rng);
    const SeedFields before = g.GetSeedFields();
    for (int p = 0; p < static_cast<int>(ShapeParam::Count); ++p)
    {
        g.Update(55, Tweak(static_cast<ShapeParam>(p)), kDims, rng);
        EXPECT_TRUE(before == g.GetSeedFields()) << ShapeParamName(static_cast<ShapeParam>(p));
    }
}

TEST(TerrainGenerator, DifferentSeedGivesDifferentSeedFields)
{
    TerrainGenerator a(TerrainRules::Defaults());
    TerrainGenerator b(TerrainRules::Defaults());
    RandomStream ra(1), rb(1);
    a.Update(1, WorldShapeParams{}, kDims, ra);
    b.Update(2, WorldShapeParams{}, kDims, rb);
    EXPECT_FALSE(a.GetSeedFields() == b.GetSeedFields());
    EXPECT_NE(a.GetSeedFields().noiseSeeds[0], b.GetSeedFields().noiseSeeds[0]);
}

TEST(TerrainGenerator, DrawCountIndependentOfSliders)
{
    TerrainGenerator base(TerrainRules::Defaults());
    RandomStream rbase(1);
    base.Update(31, WorldShapeParams{}, kDims, rbase);
    const uint64_t expected = rbase.GetDrawCount();
    EXPECT_GT(expected, 0u);
    EXPECT_EQ(base.GetSeedFields().drawCount, expected);

    for (int p = 0; p < static_cast<int>(ShapeParam::Count); ++p)
    {
        TerrainGenerator g(TerrainRules::Defaults());
        RandomStream rng(1);
        g.Update(31, Tweak(static_cast<ShapeParam>(p)), kDims, rng);
        EXPECT_EQ(rng.GetDrawCount(), expected) << ShapeParamName(static_cast<ShapeParam>(p));
    }
}

TEST(TerrainGenerator, RestoreReseedsStreamIdenticallyToFreshStream)
{
    RandomStream used(5);
    for (int i = 0; i < 100; ++i) { used.RandomUnit(); }
    used.Restore(777u, 0);
    EXPECT_EQ(used.GetDrawCount(), 0u);

    RandomStream fresh(777u);
    for (int i = 0; i < 50; ++i)
    {
        EXPECT_EQ(used.RandomInt(), fresh.RandomInt());
        EXPECT_EQ(used.RandomUnit(), fresh.RandomUnit());
    }
    EXPECT_EQ(used.GetDrawCount(), fresh.GetDrawCount());
}

TEST(TerrainGenerator, StreamAfterStage0EqualsFreshStreamWithFoldedSeed)
{
    TerrainGenerator g(TerrainRules::Defaults());
    RandomStream rng(3);
    g.Update(2024, WorldShapeParams{}, kDims, rng);
    EXPECT_EQ(g.GetSeedFields().foldedSeed, FoldSeed(2024));
    EXPECT_EQ(rng.GetSeed(), FoldSeed(2024));
    EXPECT_EQ(rng.GetDrawCount(), g.GetSeedFields().drawCount);
}

TEST(TerrainGenerator, CandidateConstraintsHoldAcrossSeeds)
{
    int exhausted = 0;
    for (uint64_t seed = 1; seed <= 300; ++seed)
    {
        TerrainGenerator g(TerrainRules::Defaults());
        RandomStream rng(1);
        g.Update(seed, WorldShapeParams{}, GridDimensions{ 96, 48 }, rng);
        const SeedFields& f = g.GetSeedFields();
        const float A = f.mapWidth;
        EXPECT_GT(A, 1.0f);

        const ContinentCandidate& c0 = f.continents[0];
        EXPECT_LE(std::hypot(c0.x - A * 0.5f, c0.y - 0.5f), 0.12f + 1e-5f);

        for (int k = 0; k < kContinentCandidates; ++k)
        {
            const ContinentCandidate& c = f.continents[k];
            EXPECT_GE(c.axisRatio, 0.65f);
            EXPECT_LE(c.axisRatio, 1.0f);
            if (k == 0) { continue; }
            if (c.exhausted) { ++exhausted; continue; }
            EXPECT_GE(c.x, 0.2f - 1e-5f);
            EXPECT_LE(c.x, A - 0.2f + 1e-5f);
            EXPECT_GE(c.y, 0.2f - 1e-5f);
            EXPECT_LE(c.y, 0.8f + 1e-5f);
            for (int j = 0; j < k; ++j)
            {
                EXPECT_GE(std::hypot(c.x - f.continents[j].x, c.y - f.continents[j].y), 0.45f - 1e-5f);
            }
        }

        for (int i = 0; i < kPassCandidates; ++i)
        {
            EXPECT_GE(f.passT[i], 0.12f);
            EXPECT_LE(f.passT[i], 0.88f + 1e-6f);
        }
        EXPECT_GE(f.spineOffset, -0.15f);
        EXPECT_LE(f.spineOffset, 0.15f + 1e-6f);

        for (int i = 0; i < kIslandCandidates; ++i)
        {
            const IslandCandidate& isl = f.islands[i];
            EXPECT_GE(isl.blobCount, 1);
            EXPECT_LE(isl.blobCount, 4);
            for (int b = 0; b < kMaxIslandBlobs; ++b)
            {
                EXPECT_LE(std::hypot(isl.blobDx[b], isl.blobDy[b]), 2.5f + 1e-4f);
                EXPECT_GE(isl.blobRadius[b], 1.5f);
                EXPECT_LE(isl.blobRadius[b], 3.5f + 1e-6f);
            }
        }
    }
    // Wide map (~2.6 x 1): exhaustion (documented furthest-try fallback) should be rare.
    EXPECT_LT(exhausted, 30);
}

TEST(TerrainGenerator, PeninsulaAnglesAtLeast60DegreesApart)
{
    const float sixty = 3.14159265f / 3.0f;
    for (uint64_t seed = 1; seed <= 300; ++seed)
    {
        TerrainGenerator g(TerrainRules::Defaults());
        RandomStream rng(1);
        g.Update(seed, WorldShapeParams{}, kDims, rng);
        const SeedFields& f = g.GetSeedFields();
        for (int i = 0; i < kPeninsulaCandidates; ++i)
        {
            for (int j = i + 1; j < kPeninsulaCandidates; ++j)
            {
                float d = std::fabs(f.peninsulaAngles[i] - f.peninsulaAngles[j]);
                d = std::fmod(d, 6.28318531f);
                d = std::min(d, 6.28318531f - d);
                EXPECT_GE(d, sixty - 1e-3f) << "seed " << seed << " pair " << i << "," << j;
            }
        }
    }
}

TEST(TerrainGenerator, MapWidthIsAspectOfGrid)
{
    TerrainGenerator g(TerrainRules::Defaults());
    RandomStream rng(1);
    g.Update(1, WorldShapeParams{}, GridDimensions{ 64, 64 }, rng);
    const float maxY = Dia::Pathfinding::HexTopology::CellCenter({ 1, 63 }, 1.0f).y;
    const float maxX = Dia::Pathfinding::HexTopology::CellCenter({ 63, 63 }, 1.0f).x;
    EXPECT_NEAR(g.GetSeedFields().mapWidth, maxX / maxY, 1e-5f);
}

TEST(TerrainGenerator, TinyGridsDoNotCrash)
{
    TerrainGenerator g(TerrainRules::Defaults());
    RandomStream rng(1);
    g.Update(1, WorldShapeParams{}, GridDimensions{ 1, 1 }, rng);
    g.Update(1, WorldShapeParams{}, GridDimensions{ 0, 0 }, rng);
    EXPECT_GT(g.GetSeedFields().mapWidth, 0.0f);
}

TEST(TerrainGenerator, LayerRegistryLookups)
{
    TerrainGenerator g(TerrainRules::Defaults());
    RandomStream rng(1);
    g.Update(1, WorldShapeParams{}, kDims, rng);
    const TerrainLayers& layers = g.GetLayers();
    EXPECT_EQ(layers.FindLayer("doesNotExist"), -1);
    EXPECT_EQ(layers.FindLayer(nullptr), -1);
    EXPECT_EQ(layers.GetFloat(-1), nullptr);
    EXPECT_EQ(layers.GetUInt(-1), nullptr);
    EXPECT_EQ(layers.GetFloat(TerrainLayers::GetLayerCount()), nullptr);
    EXPECT_EQ(layers.GetUInt(TerrainLayers::GetLayerCount()), nullptr);
    for (int i = 0; i < TerrainLayers::GetLayerCount(); ++i)
    {
        const LayerInfo& info = TerrainLayers::GetLayerInfo(i);
        EXPECT_EQ(layers.FindLayer(info.name), i);
        // Wrong-type accessor returns nullptr.
        if (info.type == LayerType::Float) { EXPECT_EQ(layers.GetUInt(i), nullptr); }
        else { EXPECT_EQ(layers.GetFloat(i), nullptr); }
    }
}
