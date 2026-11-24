// S-1 Biome Classification task 5: stages 12-14 wired into TerrainGenerator.
// AC1 (live == fresh), AC2 (biome validity), AC3 (zone rule), AC4 (no forbidden pairs), AC9 (slider isolation).
#include <gtest/gtest.h>
#include <DraoiHexWorld/Testing/BiomeTestRegistry.h>
#include <DraoiHexWorldTerrain/TerrainGenerator.h>
#include <DraoiHexWorldTerrain/WorldShapePreset.h>
#include <DraoiHexWorldTerrain/Testing/BiomeChecks.h>
#include <DraoiHexWorldTerrain/Testing/TerrainSliderDriver.h>
#include <Shared/WorldGeneration/CoWTesting/CoWTerrainRulesTestHelpers.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

using namespace CoW::WorldGeneration;
using namespace Draoi::HexWorld;
using namespace CoW::WorldGeneration::Testing;
using namespace Draoi::HexWorld::Testing;
using namespace Draoi::HexWorld::Testing;

namespace
{
    const GridDimensions kSmall{ 66, 42 };

    constexpr int kTemperature = static_cast<int>(TerrainStage::Temperature);
    constexpr int kMoisture    = static_cast<int>(TerrainStage::Moisture);
    constexpr int kBiomes      = static_cast<int>(TerrainStage::Biomes);

    std::string FindShippedPresetDir()
    {
        std::error_code       ec;
        std::filesystem::path dir = std::filesystem::absolute(std::filesystem::path(__FILE__), ec).parent_path();
        while (!dir.empty())
        {
            const std::filesystem::path candidate = dir / "Cluiche/Assets/CoW/Global/WorldGen/Presets";
            if (std::filesystem::is_directory(candidate, ec)) { return candidate.string(); }
            const std::filesystem::path parent = dir.parent_path();
            if (parent == dir) { break; }
            dir = parent;
        }
        return {};
    }

    WorldShapeParams LoadParams(const char* id)
    {
        WorldShapePreset  p;
        RuleWarnings      w;
        const std::string dir = FindShippedPresetDir();
        EXPECT_FALSE(dir.empty());
        const PresetResult r = LoadPreset(dir, id, p, w);
        EXPECT_TRUE(r.Ok()) << r.message;
        return p.params;
    }

    std::set<int> RanStages(const TerrainGenerator& g)
    {
        std::set<int> r;
        for (int s = 0; s < static_cast<int>(TerrainStage::Count); ++s)
        {
            if (g.GetTimings()[s].ranLastUpdate) { r.insert(s); }
        }
        return r;
    }

    int LayerIdx(const TerrainGenerator& g, const char* name) { return g.GetLayers().FindLayer(name); }

    std::string Join(const std::vector<std::string>& v)
    {
        std::string s;
        for (const std::string& x : v) { s += x + " "; }
        return s;
    }

    std::vector<float> CopyFloat(const TerrainGenerator& g, const char* name)
    {
        const float* p = g.GetLayers().GetFloat(LayerIdx(g, name));
        return p ? std::vector<float>(p, p + kSmall.width * kSmall.height) : std::vector<float>();
    }

    Stage14Inputs InputsOf(const TerrainGenerator& g, const WorldShapeParams& shape, GridDimensions dims)
    {
        const TerrainLayers& L = g.GetLayers();
        Stage14Inputs        in;
        in.dims         = dims;
        in.temperature  = L.GetFloat(L.FindLayer("temperature"));
        in.moisture     = L.GetFloat(L.FindLayer("moisture"));
        in.elevation    = L.GetFloat(L.FindLayer("elevation"));
        in.terrainType  = L.GetUInt(L.FindLayer("terrainType"));
        in.stratum    = L.GetUInt(L.FindLayer("stratum"));
        in.terrainFlags = L.GetUInt(L.FindLayer("terrainFlags"));
        in.barrierMask  = L.GetFloat(L.FindLayer("barrierMask"));
        in.waterSource  = L.GetUInt(L.FindLayer("waterSource"));
        in.barrierForm  = shape.barrierForm;
        in.barrier      = shape.barrier;
        in.biomes       = &::Draoi::HexWorld::Testing::ShippedBiomeRegistry();
        return in;
    }

    const float kOffsets[] = { -0.3f, -0.1f, 0.0f, 0.15f, 0.3f };

    // Terrain script with the two climate sliders interleaved (every second step touches one of them).
    void ApplyStep(WorldShapeParams& p, const SliderChange& c, ScriptRng& rng, int step)
    {
        SetSliderValue(p, c.slider, c.value);
        if (step % 4 == 1) { p.temperatureOffset = kOffsets[rng.Next(5)]; }
        if (step % 4 == 3) { p.moistureOffset = kOffsets[rng.Next(5)]; }
        p.Clamp();
    }

    void CheckLiveEqualsFresh(uint64_t seed, int changes)
    {
        TerrainRunner    live, fresh;
        WorldShapeParams p;
        live.Run(seed, p, kSmall);
        ScriptRng                       rng(seed * 31 + 7);
        const std::vector<SliderChange> script = BuildSliderScript(seed + 1000, changes);
        for (int i = 0; i < changes; ++i)
        {
            ApplyStep(p, script[i], rng, i);
            const TerrainSnapshot          a = live.Run(seed, p, kSmall);
            const TerrainSnapshot          b = fresh.RunFresh(seed, p, kSmall);
            const std::vector<std::string> d = DiffSnapshots(a, b);
            ASSERT_TRUE(d.empty()) << "seed " << seed << " step " << i << ": " << Join(d);
            ASSERT_EQ(live.Gen().GetEffective().unresolvedPairs, fresh.Gen().GetEffective().unresolvedPairs);
        }
    }

    // AC2/AC3/AC4 over one generated world.
    void CheckWorldValid(const char* label, uint64_t seed, const WorldShapeParams& shape, int& totalUnresolved)
    {
        TerrainRunner r;
        r.Run(seed, shape, kSmall);
        const TerrainGenerator& g     = r.Gen();
        const TerrainLayers&    L     = g.GetLayers();
        const int               n     = kSmall.width * kSmall.height;
        const uint32_t*         biome = L.GetUInt(L.FindLayer("biomeIndex"));
        const uint32_t*         water = L.GetUInt(L.FindLayer("waterSource"));
        ASSERT_NE(biome, nullptr);
        ASSERT_NE(water, nullptr);
        ASSERT_EQ(r.World().CellCount(), n);

        for (int c = 0; c < n; ++c)
        {
            const uint32_t b = biome[c];
            ASSERT_LT(b, ::Draoi::HexWorld::Testing::ShippedBiomeRegistry().Count()) << label << " seed " << seed << " cell " << c;
            ASSERT_EQ(static_cast<uint32_t>(r.World().Cells()[c].biome), b);
            const bool oceanWater = water[c] == static_cast<uint32_t>(WaterSource::Ocean);
            const bool lakeWater  = water[c] == static_cast<uint32_t>(WaterSource::Lake);
            ASSERT_EQ(b == static_cast<uint32_t>(B("Biome.Ocean")), oceanWater) << label << " seed " << seed << " cell " << c;
            ASSERT_EQ(b == static_cast<uint32_t>(B("Biome.Lake")), lakeWater) << label << " seed " << seed << " cell " << c;
        }

        // Lakes sit in whatever depth zone their cell has, but IsValidInStratum(Lake) demands None, so water
        // cells are exempt here (spec AC3 reads "every cell"; see task report).
        const Stage14Inputs in = InputsOf(g, shape, kSmall);
        std::vector<int>    zoneBad;
        for (int c : FindZoneViolations(in, biome))
        {
            if (water[c] == static_cast<uint32_t>(WaterSource::None) || water[c] == static_cast<uint32_t>(WaterSource::River))
            {
                zoneBad.push_back(c);
            }
        }
        EXPECT_TRUE(zoneBad.empty()) << label << " seed " << seed << ": " << zoneBad.size() << " zone violations";

        const std::vector<std::pair<int, int>> pairs = FindForbiddenPairs(kSmall, biome);
        std::map<std::string, int> byType;
        for (const auto& pr : pairs)
        {
            std::string a = ::Draoi::HexWorld::Testing::ShippedBiomeRegistry().Name(static_cast<::Draoi::HexWorld::BiomeIndex>(biome[pr.first]));
            std::string b = ::Draoi::HexWorld::Testing::ShippedBiomeRegistry().Name(static_cast<::Draoi::HexWorld::BiomeIndex>(biome[pr.second]));
            if (b < a) { std::swap(a, b); }
            ++byType[a + " / " + b];
        }
        for (const auto& kv : byType)
        {
            ADD_FAILURE() << label << " seed " << seed << " forbidden pair " << kv.first << " x" << kv.second;
        }
        EXPECT_EQ(g.GetEffective().unresolvedPairs, static_cast<int>(pairs.size())) << label << " seed " << seed;
        totalUnresolved += g.GetEffective().unresolvedPairs;
    }

    void CheckAllShapes(uint64_t firstSeed, int count)
    {
        std::vector<std::pair<std::string, WorldShapeParams>> shapes;
        shapes.emplace_back("default", WorldShapeParams{});
        for (const char* id : { "continental", "archipelago", "highlands" })
        {
            shapes.emplace_back(id, LoadParams(id));
        }
        int unresolved = 0;
        for (const auto& s : shapes)
        {
            for (int i = 0; i < count; ++i)
            {
                CheckWorldValid(s.first.c_str(), firstSeed + static_cast<uint64_t>(i), s.second, unresolved);
            }
        }
        EXPECT_EQ(unresolved, 0);
    }

    // Runs base, applies `change`, and checks which stages re-ran and that stages 0-11 are untouched.
    template <typename F>
    void RunIsolation(F change, const std::set<int>& expectedRan, TerrainRunner& r, TerrainSnapshot& base, TerrainSnapshot& after,
                      int lastUntouchedStage = 11)
    {
        WorldShapeParams p;
        base = r.Run(11, p, kSmall);
        change(p);
        after = r.Run(11, p, kSmall);
        EXPECT_EQ(RanStages(r.Gen()), expectedRan);
        for (int i = 0; i < TerrainLayers::GetLayerCount(); ++i)
        {
            if (static_cast<int>(TerrainLayers::GetLayerInfo(i).stage) <= lastUntouchedStage)
            {
                EXPECT_EQ(base.layerHash[i], after.layerHash[i]) << TerrainLayers::GetLayerInfo(i).name;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Plumbing
// ---------------------------------------------------------------------------
TEST(BiomeGenerator, NewStagesRunOnFirstUpdateAndLayersExist)
{
    TerrainRunner r;
    r.Run(5, WorldShapeParams{}, kSmall);
    const StageTiming* t = r.Gen().GetTimings();
    EXPECT_TRUE(t[kTemperature].ranLastUpdate);
    EXPECT_TRUE(t[kMoisture].ranLastUpdate);
    EXPECT_TRUE(t[kBiomes].ranLastUpdate);
    for (const char* name : { "temperature", "airMoisture", "rainOut", "moisture", "biomeScore" })
    {
        EXPECT_NE(r.Gen().GetLayers().GetFloat(LayerIdx(r.Gen(), name)), nullptr) << name;
    }
    for (const char* name : { "waterDistance", "biomePin", "biomeIndex", "fixupChanges" })
    {
        EXPECT_NE(r.Gen().GetLayers().GetUInt(LayerIdx(r.Gen(), name)), nullptr) << name;
    }
    EXPECT_EQ(TerrainLayers::GetLayerInfo(LayerIdx(r.Gen(), "temperature")).stage, TerrainStage::Temperature);
    EXPECT_EQ(TerrainLayers::GetLayerInfo(LayerIdx(r.Gen(), "moisture")).stage, TerrainStage::Moisture);
    EXPECT_EQ(TerrainLayers::GetLayerInfo(LayerIdx(r.Gen(), "biomeIndex")).stage, TerrainStage::Biomes);
}

TEST(BiomeGenerator, FirstStageAffectedMatchesSlidersTable)
{
    EXPECT_EQ(FirstStageAffected(ShapeParam::TemperatureOffset), TerrainStage::Temperature);
    EXPECT_EQ(FirstStageAffected(ShapeParam::MoistureOffset), TerrainStage::Moisture);
    EXPECT_EQ(kTemperature, 12);
    EXPECT_EQ(kMoisture, 13);
    EXPECT_EQ(kBiomes, 14);
    EXPECT_EQ(kImplementedStageCount, 15);
}

// ---------------------------------------------------------------------------
// AC9: slider isolation
// ---------------------------------------------------------------------------
TEST(BiomeGenerator, TemperatureOffsetRerunsOnlyStages12And14)
{
    TerrainRunner  r;
    TerrainSnapshot base, after;
    std::vector<float> baseTemp;
    {
        WorldShapeParams p;
        r.Run(11, p, kSmall);
        baseTemp = CopyFloat(r.Gen(), "temperature");
    }
    RunIsolation([](WorldShapeParams& p) { p.temperatureOffset = 0.2f; }, { 12, 14 }, r, base, after);

    const std::vector<float> shifted = CopyFloat(r.Gen(), "temperature");
    ASSERT_EQ(shifted.size(), baseTemp.size());
    int checked = 0;
    for (size_t i = 0; i < shifted.size(); ++i)
    {
        if (baseTemp[i] > 0.0f && baseTemp[i] < 1.0f)
        {
            EXPECT_NEAR(shifted[i], std::clamp(baseTemp[i] + 0.2f, 0.0f, 1.0f), 1e-5f) << i;
            ++checked;
        }
    }
    EXPECT_GT(checked, 100);
    // Moisture layers and the terrain are byte-identical.
    for (const char* name : { "moisture", "airMoisture", "rainOut", "waterDistance" })
    {
        const int idx = LayerIdx(r.Gen(), name);
        EXPECT_EQ(base.layerHash[idx], after.layerHash[idx]) << name;
    }
    const int temp = LayerIdx(r.Gen(), "temperature");
    EXPECT_NE(base.layerHash[temp], after.layerHash[temp]);
}

TEST(BiomeGenerator, MoistureOffsetRerunsOnlyStages13And14)
{
    TerrainRunner      r;
    TerrainSnapshot    base, after;
    std::vector<float> baseMoist;
    {
        WorldShapeParams p;
        r.Run(11, p, kSmall);
        baseMoist = CopyFloat(r.Gen(), "moisture");
    }
    RunIsolation([](WorldShapeParams& p) { p.moistureOffset = -0.2f; }, { 13, 14 }, r, base, after);

    const std::vector<float> shifted = CopyFloat(r.Gen(), "moisture");
    ASSERT_EQ(shifted.size(), baseMoist.size());
    int checked = 0;
    for (size_t i = 0; i < shifted.size(); ++i)
    {
        if (baseMoist[i] > 0.0f && baseMoist[i] < 1.0f)
        {
            EXPECT_NEAR(shifted[i], std::clamp(baseMoist[i] - 0.2f, 0.0f, 1.0f), 1e-5f) << i;
            ++checked;
        }
    }
    EXPECT_GT(checked, 100);
    const int temp = LayerIdx(r.Gen(), "temperature");
    EXPECT_EQ(base.layerHash[temp], after.layerHash[temp]);
    const int moist = LayerIdx(r.Gen(), "moisture");
    EXPECT_NE(base.layerHash[moist], after.layerHash[moist]);
}

TEST(BiomeGenerator, LatitudeChangeRerunsAllThreeClimateStages)
{
    TerrainRunner   r;
    TerrainSnapshot base, after;
    RunIsolation([](WorldShapeParams& p) { p.latitudeNorthDeg = 20.0f; }, { 11, 12, 13, 14 }, r, base, after, 10);
}

TEST(BiomeGenerator, RiversChangeReachesClimateStages)
{
    TerrainRunner    r;
    WorldShapeParams p;
    r.Run(11, p, kSmall);
    p.rivers = 0.9f;
    r.Run(11, p, kSmall);
    const std::set<int> ran = RanStages(r.Gen());
    EXPECT_TRUE(ran.count(kTemperature));   // via Valleys (elevation)
    EXPECT_TRUE(ran.count(kMoisture));
    EXPECT_TRUE(ran.count(kBiomes));
    EXPECT_FALSE(ran.count(0));
    EXPECT_FALSE(ran.count(1));
}

TEST(BiomeGenerator, BarrierChangeRerunsBiomes)
{
    TerrainRunner    r;
    WorldShapeParams p;
    r.Run(11, p, kSmall);
    p.barrier = 0.8f;
    r.Run(11, p, kSmall);
    EXPECT_TRUE(RanStages(r.Gen()).count(kBiomes));
}

// ---------------------------------------------------------------------------
// AC1: live preview equals fresh generation
// ---------------------------------------------------------------------------
TEST(BiomeGenerator, TwoFreshRunsAreByteIdentical)
{
    for (uint64_t seed : { 1ull, 42ull, 99ull })
    {
        TerrainRunner a, b;
        const TerrainSnapshot sa = a.Run(seed, WorldShapeParams{}, kSmall);
        const TerrainSnapshot sb = b.Run(seed, WorldShapeParams{}, kSmall);
        EXPECT_TRUE(DiffSnapshots(sa, sb).empty()) << Join(DiffSnapshots(sa, sb));
        const TerrainSnapshot sc = a.RunFresh(seed, WorldShapeParams{}, kSmall);
        EXPECT_TRUE(DiffSnapshots(sa, sc).empty()) << Join(DiffSnapshots(sa, sc));
    }
}

TEST(BiomeGenerator, LiveSliderSequenceEqualsFreshGeneration)
{
    for (uint64_t seed : { 3ull, 17ull, 123ull })
    {
        CheckLiveEqualsFresh(seed, 40);
    }
}

TEST(SLOW_BiomeGenerator, LiveSliderSequenceEqualsFreshGenerationLong)
{
    for (uint64_t seed : { 3ull, 17ull, 123ull, 777ull, 4242ull })
    {
        CheckLiveEqualsFresh(seed, 200);
    }
}

// ---------------------------------------------------------------------------
// AC2 / AC3 / AC4
// ---------------------------------------------------------------------------
TEST(BiomeGenerator, WorldsAreValidCheapSubset)
{
    CheckAllShapes(1, 3);
}

TEST(SLOW_BiomeGenerator, WorldsAreValidTwentySeeds)
{
    CheckAllShapes(500, 20);
}
