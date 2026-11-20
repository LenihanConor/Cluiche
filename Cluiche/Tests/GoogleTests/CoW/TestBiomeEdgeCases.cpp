// S-1 Biome Classification task 9: exhaustive pass. Edge cases for stages 12-14 (tiny grids, no water,
// all ocean, extreme sliders), full-pipeline validity at Standard / Large, AC5/AC6 on generated worlds,
// WriteTerrain / Invalidate behaviour, and WorldGenSession stagesRan coverage of the biome stages.
#include <gtest/gtest.h>
#include <Shared/WorldGeneration/BiomeClassify.h>
#include <Shared/WorldGeneration/BiomeClimate.h>
#include <Shared/WorldGeneration/BiomeTypes.h>
#include <Shared/WorldGeneration/TerrainGenerator.h>
#include <Shared/WorldGeneration/WorldGenSession.h>
#include <Shared/WorldGeneration/Testing/BiomeChecks.h>
#include <Shared/WorldGeneration/Testing/BiomeTestGrids.h>
#include <Shared/WorldGeneration/Testing/TerrainSliderDriver.h>
#include <Shared/WorldGeneration/Testing/TerrainRulesTestHelpers.h>

#include <cmath>
#include <cstring>
#include <string>
#include <vector>

using namespace CoW::WorldGeneration;
using namespace CoW::WorldGeneration::Testing;

namespace
{
    struct ClimateOut
    {
        std::vector<uint32_t> dist;
        std::vector<float>    air, rain, moist, temp;
    };

    ClimateOut RunClimate(const ClimateGrid& g, float tOff, float mOff)
    {
        const TerrainRules rules = TestRules();
        const size_t       n = static_cast<size_t>(g.dims.width) * g.dims.height;
        ClimateOut         o;
        o.dist.assign(n, 7u);
        o.air.assign(n, -5.0f);
        o.rain.assign(n, -5.0f);
        o.moist.assign(n, -5.0f);
        o.temp.assign(n, -5.0f);
        Stage13Layers out;
        out.waterDistance = o.dist.data();
        out.airMoisture = o.air.data();
        out.rainOut = o.rain.data();
        out.moisture = o.moist.data();
        ComputeStage12(rules, g.dims, tOff, g.elevation.data(), g.latitude.data(), o.temp.data());
        ComputeStage13(rules, g.dims, mOff, g.elevation.data(), g.latitude.data(), g.waterSource.data(), out);
        return o;
    }

    void ExpectFinite01(const ClimateOut& o, const char* label)
    {
        for (size_t i = 0; i < o.moist.size(); ++i)
        {
            EXPECT_TRUE(std::isfinite(o.air[i]) && o.air[i] >= 0.0f && o.air[i] <= 1.0f) << label << " air " << i << " " << o.air[i];
            EXPECT_TRUE(std::isfinite(o.rain[i]) && o.rain[i] >= 0.0f && o.rain[i] <= 1.0f) << label << " rain " << i << " " << o.rain[i];
            EXPECT_TRUE(std::isfinite(o.moist[i]) && o.moist[i] >= 0.0f && o.moist[i] <= 1.0f) << label << " moist " << i << " " << o.moist[i];
            EXPECT_TRUE(std::isfinite(o.temp[i]) && o.temp[i] >= 0.0f && o.temp[i] <= 1.0f) << label << " temp " << i << " " << o.temp[i];
        }
    }

    // ---- generator helpers --------------------------------------------------------------------------------
    Stage14Inputs InputsOf(const TerrainGenerator& g, const WorldShapeParams& shape, GridDimensions dims)
    {
        const TerrainLayers& L = g.GetLayers();
        Stage14Inputs        in;
        in.dims         = dims;
        in.temperature  = L.GetFloat(L.FindLayer("temperature"));
        in.moisture     = L.GetFloat(L.FindLayer("moisture"));
        in.elevation    = L.GetFloat(L.FindLayer("elevation"));
        in.terrainType  = L.GetUInt(L.FindLayer("terrainType"));
        in.depthZone    = L.GetUInt(L.FindLayer("depthZone"));
        in.terrainFlags = L.GetUInt(L.FindLayer("terrainFlags"));
        in.barrierMask  = L.GetFloat(L.FindLayer("barrierMask"));
        in.waterSource  = L.GetUInt(L.FindLayer("waterSource"));
        in.barrierForm  = shape.barrierForm;
        in.barrier      = shape.barrier;
        return in;
    }

    // Every float layer finite; every biome-stage layer in range. Returns count of bad cells.
    int CountBadLayerValues(const TerrainGenerator& g, GridDimensions dims, std::string& firstBad)
    {
        const int n = dims.width * dims.height;
        int       bad = 0;
        for (int i = 0; i < TerrainLayers::GetLayerCount(); ++i)
        {
            // Earlier-stage layers (e.g. landPotential) deliberately hold -inf for masked cells.
            if (static_cast<int>(TerrainLayers::GetLayerInfo(i).stage) < static_cast<int>(TerrainStage::Valleys)) { continue; }
            if (const float* f = g.GetLayers().GetFloat(i))
            {
                for (int c = 0; c < n; ++c)
                {
                    if (!std::isfinite(f[c]))
                    {
                        if (bad++ == 0) { firstBad = std::string(TerrainLayers::GetLayerInfo(i).name) + "@" + std::to_string(c); }
                    }
                }
            }
        }
        const char* climate[] = { "temperature", "airMoisture", "rainOut", "moisture" };
        for (const char* name : climate)
        {
            const float* f = g.GetLayers().GetFloat(g.GetLayers().FindLayer(name));
            for (int c = 0; f != nullptr && c < n; ++c)
            {
                if (f[c] < 0.0f || f[c] > 1.0f)
                {
                    if (bad++ == 0) { firstBad = std::string(name) + " out of [0,1] @" + std::to_string(c); }
                }
            }
        }
        return bad;
    }

    // Validity of a generated world: layers, ids, water, zone, forbidden pairs.
    void CheckGeneratedWorld(const char* label, const TerrainRunner& r, const WorldShapeParams& shape, GridDimensions dims,
                             bool requireNoUnresolved)
    {
        const TerrainGenerator& g = r.Gen();
        const TerrainLayers&    L = g.GetLayers();
        const int               n = dims.width * dims.height;
        const uint32_t*         biome = L.GetUInt(L.FindLayer("biomeIndex"));
        const uint32_t*         water = L.GetUInt(L.FindLayer("waterSource"));
        ASSERT_NE(biome, nullptr);
        ASSERT_NE(water, nullptr);
        ASSERT_EQ(r.World().CellCount(), n) << label;

        std::string firstBad;
        EXPECT_EQ(CountBadLayerValues(g, dims, firstBad), 0) << label << " " << firstBad;

        for (int c = 0; c < n; ++c)
        {
            ASSERT_LT(biome[c], static_cast<uint32_t>(kBiomeCount)) << label << " cell " << c;
            ASSERT_EQ(static_cast<uint32_t>(r.World().Cells()[c].biome), biome[c]) << label << " cell " << c;
            ASSERT_EQ(biome[c] == static_cast<uint32_t>(Biome::Ocean), water[c] == static_cast<uint32_t>(WaterSource::Ocean)) << label;
            ASSERT_EQ(biome[c] == static_cast<uint32_t>(Biome::Lake), water[c] == static_cast<uint32_t>(WaterSource::Lake)) << label;
        }

        const Stage14Inputs in = InputsOf(g, shape, dims);
        for (int c : FindZoneViolations(in, biome))
        {
            const bool isWater = water[c] == static_cast<uint32_t>(WaterSource::Ocean) || water[c] == static_cast<uint32_t>(WaterSource::Lake);
            EXPECT_TRUE(isWater) << label << " zone violation at land cell " << c;
        }
        const auto pairs = FindForbiddenPairs(dims, biome);
        EXPECT_EQ(g.GetEffective().unresolvedPairs, static_cast<int>(pairs.size())) << label;
        if (requireNoUnresolved)
        {
            EXPECT_TRUE(pairs.empty()) << label << " forbidden pairs " << pairs.size();
        }
        EXPECT_TRUE(CheckWetlandRule(TestRules(), in, biome).empty()) << label << " wetland rule";
    }

    // AC6 on a generated world: only cells with fixupChanges == 0 are held to the pin rules.
    void CheckSpecialRules(const char* label, const TerrainGenerator& g, const WorldShapeParams& shape, GridDimensions dims)
    {
        const TerrainLayers& L = g.GetLayers();
        const int            n = dims.width * dims.height;
        const uint32_t*      biome = L.GetUInt(L.FindLayer("biomeIndex"));
        const uint32_t*      fix = L.GetUInt(L.FindLayer("fixupChanges"));
        const uint32_t*      pin = L.GetUInt(L.FindLayer("biomePin"));
        const uint32_t*      type = L.GetUInt(L.FindLayer("terrainType"));
        const uint32_t*      flags = L.GetUInt(L.FindLayer("terrainFlags"));
        const uint32_t*      water = L.GetUInt(L.FindLayer("waterSource"));
        const float*         mask = L.GetFloat(L.FindLayer("barrierMask"));
        const TerrainRules   rules = TestRules();

        for (int c = 0; c < n; ++c)
        {
            if (water[c] == static_cast<uint32_t>(WaterSource::Ocean) || water[c] == static_cast<uint32_t>(WaterSource::Lake) || fix[c] != 0)
            {
                continue;
            }
            const bool volcanicRule = (flags[c] & TerrainFlag::Volcanic) != 0 || type[c] == static_cast<uint32_t>(TerrainType::Volcano) ||
                                      (shape.barrierForm == BarrierForm::VolcanicChain && (flags[c] & TerrainFlag::Barrier) != 0);
            const bool alpineTerrain = type[c] == static_cast<uint32_t>(TerrainType::Mountain) || type[c] == static_cast<uint32_t>(TerrainType::Peak) ||
                                       type[c] == static_cast<uint32_t>(TerrainType::Cliff) || type[c] == static_cast<uint32_t>(TerrainType::Pass);
            if (volcanicRule)
            {
                EXPECT_EQ(biome[c], static_cast<uint32_t>(Biome::VolcanicAshland)) << label << " cell " << c;
                EXPECT_EQ(pin[c], BiomePin::Volcanic) << label;
            }
            else if (alpineTerrain)
            {
                EXPECT_EQ(biome[c], static_cast<uint32_t>(Biome::AlpineMountain)) << label << " cell " << c;
                EXPECT_EQ(pin[c], BiomePin::Alpine) << label;
            }
        }

        // Plateau interiors: each connected group of unpinned interior cells (fixup 0) is one climate.
        if (shape.barrierForm == BarrierForm::Plateau && shape.barrier > 0.0f && mask != nullptr)
        {
            const Dia::Pathfinding::HexTopology topo(dims.width, dims.height);
            std::vector<int>                    comp(static_cast<size_t>(n), -1);
            int                                 comps = 0;
            for (int s = 0; s < n; ++s)
            {
                const bool inner = mask[s] >= rules.kPlateauInner && pin[s] == BiomePin::None;
                if (!inner || comp[s] >= 0) { continue; }
                std::vector<int> stack{ s };
                comp[s] = comps;
                uint32_t         first = 0xFFFFFFFFu;
                while (!stack.empty())
                {
                    const int c = stack.back();
                    stack.pop_back();
                    if (fix[c] == 0)
                    {
                        EXPECT_TRUE(biome[c] == static_cast<uint32_t>(Biome::Rainforest) || biome[c] == static_cast<uint32_t>(Biome::DesertBasin))
                            << label << " interior cell " << c << " is " << biome[c];
                        if (first == 0xFFFFFFFFu) { first = biome[c]; }
                        EXPECT_EQ(biome[c], first) << label << " plateau component " << comps << " mixed at " << c;
                    }
                    for (uint32_t d = 0; d < 6u; ++d)
                    {
                        const uint32_t nb = topo.NeighbourAt(static_cast<uint32_t>(c), d);
                        if (nb != Dia::Pathfinding::kNoCell && comp[nb] < 0 && mask[nb] >= rules.kPlateauInner && pin[nb] == BiomePin::None)
                        {
                            comp[nb] = comps;
                            stack.push_back(static_cast<int>(nb));
                        }
                    }
                }
                ++comps;
            }
        }
    }

    WorldShapeParams WithLat(float north, float south, float tOff, float mOff)
    {
        WorldShapeParams p;
        p.latitudeNorthDeg = north;
        p.latitudeSouthDeg = south;
        p.temperatureOffset = tOff;
        p.moistureOffset = mOff;
        p.Clamp();
        return p;
    }
} // namespace

// ---------------------------------------------------------------------------
// Stages 12-13 on degenerate grids
// ---------------------------------------------------------------------------
TEST(BiomeEdge, ClimateTinyGridsAreFinite)
{
    for (int w = 1; w <= 2; ++w)
    {
        for (int h = 1; h <= 2; ++h)
        {
            for (float lat : { 0.0f, 0.34f, 0.5f, 0.67f, 1.0f })
            {
                ClimateGrid g(w, h, lat);
                const ClimateOut dry = RunClimate(g, 0.0f, 0.0f);
                ExpectFinite01(dry, "tiny-dry");
                for (uint32_t d : dry.dist) { EXPECT_EQ(d, 0xFFFFFFFFu); }
                ClimateGrid wet(w, h, lat);
                wet.SetWater(0, 0, WaterSource::Ocean);
                ExpectFinite01(RunClimate(wet, 0.0f, 0.0f), "tiny-ocean");
            }
        }
    }
}

TEST(BiomeEdge, AllLandNoWaterMoistureIsFiniteAndFromAirOnly)
{
    for (float lat : { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f })
    {
        ClimateGrid      g(12, 9, lat);
        const ClimateOut o = RunClimate(g, 0.0f, 0.0f);
        ExpectFinite01(o, "all-land");
        for (size_t i = 0; i < o.dist.size(); ++i)
        {
            EXPECT_EQ(o.dist[i], 0xFFFFFFFFu);
            // proximity is 0 and no air is ever carried in: air 0, rain 0, moisture 0.
            EXPECT_EQ(o.air[i], 0.0f);
            EXPECT_EQ(o.rain[i], 0.0f);
            EXPECT_EQ(o.moist[i], 0.0f);
        }
    }
}

TEST(BiomeEdge, AllOceanWorldHasUnitAirAndZeroDistance)
{
    ClimateGrid g(10, 8, 0.4f);
    for (int y = 0; y < 8; ++y) { for (int x = 0; x < 10; ++x) { g.SetWater(x, y, WaterSource::Ocean); } }
    const ClimateOut o = RunClimate(g, 0.0f, 0.0f);
    ExpectFinite01(o, "all-ocean");
    for (size_t i = 0; i < o.dist.size(); ++i)
    {
        EXPECT_EQ(o.dist[i], 0u);
        EXPECT_EQ(o.air[i], 1.0f);
        EXPECT_EQ(o.rain[i], 0.0f);
        EXPECT_EQ(o.moist[i], 1.0f);   // 0.4 * 1 + 0.6 * 1 = 1
    }
}

TEST(BiomeEdge, ExtremeOffsetsAndLatitudesStayFiniteAndClamped)
{
    for (float lat : { 0.0f, 1.0f, 0.5f })
    {
        for (float tOff : { -0.3f, 0.3f })
        {
            for (float mOff : { -0.3f, 0.3f })
            {
                ClimateGrid g = MakeRidgeGrid(14, 10, 7, 0.9f, lat, true, true);
                const ClimateOut o = RunClimate(g, tOff, mOff);
                ExpectFinite01(o, "extreme");
            }
        }
    }
}

TEST(BiomeEdge, OceanAndRiverOnlyGridsHaveRiverPenaltyDistance)
{
    ClimateGrid g(5, 1, 0.1f);
    g.SetWater(2, 0, WaterSource::River);
    const ClimateOut o = RunClimate(g, 0.0f, 0.0f);
    EXPECT_EQ(o.dist[2], 2u);   // kRiverPenalty
    EXPECT_GE(o.dist[1], 3u - 1u);
    ExpectFinite01(o, "river-only");
}

// ---------------------------------------------------------------------------
// Stage 14 on degenerate grids
// ---------------------------------------------------------------------------
TEST(BiomeEdge, Stage14TwoByTwoGrids)
{
    {
        BiomeGrid         g(2, 2);   // all grassland-perfect land
        const BiomeResult r = RunBiomes(g);
        for (int c = 0; c < 4; ++c) { EXPECT_EQ(r.At(c), Biome::GrasslandSteppe); }
        EXPECT_EQ(r.debug.unresolvedPairs, 0);
    }
    {
        BiomeGrid g(2, 2);
        g.SetClimate(0, 0.85f, 0.9f);   // Rainforest
        g.SetClimate(1, 0.80f, 0.08f);  // Desert (neighbour of 0: forbidden pair)
        g.SetClimate(2, 0.12f, 0.30f);  // Tundra
        g.SetClimate(3, 0.50f, 0.60f);  // Forest
        const BiomeResult r = RunBiomes(g);
        EXPECT_EQ(r.debug.unresolvedPairs, 0);
        EXPECT_TRUE(FindForbiddenPairs(g.dims, r.biome.data()).empty());
        EXPECT_TRUE(FindZoneViolations(g.Inputs(), r.biome.data()).empty());
    }
    {
        BiomeGrid g(2, 2);
        g.SetWater(0, WaterSource::Ocean);
        g.SetWater(1, WaterSource::Lake);
        const BiomeResult r = RunBiomes(g);
        EXPECT_EQ(r.At(0), Biome::Ocean);
        EXPECT_EQ(r.At(1), Biome::Lake);
        EXPECT_EQ(r.pin[0], BiomePin::Water);
    }
}

TEST(BiomeEdge, Stage14AllOceanGrid)
{
    BiomeGrid g(8, 6);
    for (int c = 0; c < 48; ++c) { g.SetWater(c, WaterSource::Ocean); }
    const BiomeResult r = RunBiomes(g);
    for (int c = 0; c < 48; ++c)
    {
        EXPECT_EQ(r.At(c), Biome::Ocean);
        EXPECT_EQ(r.fixup[c], BiomeFixup::None);
    }
    EXPECT_EQ(r.debug.unresolvedPairs, 0);
}

TEST(BiomeEdge, Stage14AllLandNoWaterAtMoistureZero)
{
    // Moisture 0 everywhere (the no-water outcome of stage 13): every land cell scores finite and gets a land biome.
    BiomeGrid g(9, 7, 0.6f, 0.0f);
    const BiomeResult r = RunBiomes(g);
    for (size_t c = 0; c < r.biome.size(); ++c)
    {
        EXPECT_LT(r.biome[c], static_cast<uint32_t>(kBiomeCount));
        EXPECT_NE(r.At(static_cast<int>(c)), Biome::Ocean);
        EXPECT_NE(r.At(static_cast<int>(c)), Biome::Lake);
        EXPECT_TRUE(std::isfinite(r.score[c]));
    }
}

// ---------------------------------------------------------------------------
// Full pipeline
// ---------------------------------------------------------------------------
TEST(BiomeEdge, TinyGeneratorGridsDoNotCrashAndStayValid)
{
    for (GridDimensions d : { GridDimensions{ 1, 1 }, GridDimensions{ 2, 2 }, GridDimensions{ 3, 2 } })
    {
        TerrainRunner r;
        r.Run(5, WorldShapeParams{}, d);
        std::string firstBad;
        EXPECT_EQ(CountBadLayerValues(r.Gen(), d, firstBad), 0) << d.width << "x" << d.height << " " << firstBad;
        for (int c = 0; c < d.width * d.height; ++c)
        {
            EXPECT_LT(static_cast<int>(r.World().Cells()[c].biome), kBiomeCount);
        }
    }
}

TEST(BiomeEdge, SliderCornersAtSmallAreFiniteAndValid)
{
    const GridDimensions dims{ 66, 42 };
    const float          lats[][2] = { { 90, 90 }, { -90, -90 }, { 90, -90 }, { -90, 90 }, { 0, 0 }, { 60, 20 }, { 45, 45 } };
    for (const auto& lat : lats)
    {
        for (float tOff : { -0.3f, 0.3f })
        {
            for (float mOff : { -0.3f, 0.3f })
            {
                const WorldShapeParams p = WithLat(lat[0], lat[1], tOff, mOff);
                TerrainRunner          r;
                r.Run(3, p, dims);
                const std::string label = "lat " + std::to_string(lat[0]) + "/" + std::to_string(lat[1]) + " t" + std::to_string(tOff) +
                                          " m" + std::to_string(mOff);
                CheckGeneratedWorld(label.c_str(), r, p, dims, true);
            }
        }
    }
}

TEST(BiomeEdge, StandardWorldIsValid)
{
    const GridDimensions dims{ 80, 52 };
    for (uint64_t seed : { 1ull, 2ull })
    {
        TerrainRunner r;
        r.Run(seed, WorldShapeParams{}, dims);
        CheckGeneratedWorld("standard", r, WorldShapeParams{}, dims, true);
        CheckSpecialRules("standard", r.Gen(), WorldShapeParams{}, dims);
    }
}

TEST(SLOW_BiomeEdge, LargeWorldIsValid)
{
    const GridDimensions dims{ 104, 64 };
    for (uint64_t seed : { 1ull, 2ull, 3ull })
    {
        TerrainRunner r;
        r.Run(seed, WorldShapeParams{}, dims);
        CheckGeneratedWorld("large", r, WorldShapeParams{}, dims, true);
        CheckSpecialRules("large", r.Gen(), WorldShapeParams{}, dims);
    }
}

TEST(BiomeEdge, WetlandRuleAndSpecialRulesHoldOnGeneratedWorlds)
{
    const GridDimensions dims{ 66, 42 };
    int                  wetlandCells = 0;
    for (uint64_t seed = 1; seed <= 6; ++seed)
    {
        for (float rivers : { 0.3f, 1.0f })
        {
            WorldShapeParams p;
            p.rivers = rivers;
            p.lakes = 10;
            p.barrier = (seed % 2 == 0) ? 0.7f : 0.0f;
            p.barrierForm = (seed % 3 == 0) ? BarrierForm::VolcanicChain : BarrierForm::Plateau;
            p.Clamp();
            TerrainRunner r;
            r.Run(seed, p, dims);
            const TerrainGenerator& g = r.Gen();
            const uint32_t*         biome = g.GetLayers().GetUInt(g.GetLayers().FindLayer("biomeIndex"));
            const Stage14Inputs     in = InputsOf(g, p, dims);
            EXPECT_TRUE(CheckWetlandRule(TestRules(), in, biome).empty()) << "seed " << seed;
            for (int c = 0; c < dims.width * dims.height; ++c) { wetlandCells += biome[c] == static_cast<uint32_t>(Biome::WetlandDelta) ? 1 : 0; }
            CheckSpecialRules("special", g, p, dims);
        }
    }
    EXPECT_GT(wetlandCells, 0) << "no wetland produced: AC5 test would be vacuous";
}

TEST(BiomeEdge, WetlandCellsAreLowAndMoistOrNextToWater)
{
    // Directly re-derives AC5's elevation clause from the layers (independent of IsWetlandEligible).
    const GridDimensions dims{ 66, 42 };
    const TerrainRules   rules = TestRules();
    for (uint64_t seed = 1; seed <= 4; ++seed)
    {
        TerrainRunner r;
        r.Run(seed, WorldShapeParams{}, dims);
        const TerrainLayers& L = r.Gen().GetLayers();
        const uint32_t*      biome = L.GetUInt(L.FindLayer("biomeIndex"));
        const float*         elev = L.GetFloat(L.FindLayer("elevation"));
        for (int c = 0; c < dims.width * dims.height; ++c)
        {
            if (biome[c] == static_cast<uint32_t>(Biome::WetlandDelta))
            {
                EXPECT_LE(elev[c], rules.kWetlandMaxElev) << seed << " cell " << c;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// WriteTerrain / Invalidate
// ---------------------------------------------------------------------------
TEST(BiomeEdge, BiomeIndexLayerEqualsCellBiomeAfterWriteTerrain)
{
    const GridDimensions dims{ 66, 42 };
    TerrainRunner        r;
    r.Run(9, WorldShapeParams{}, dims);
    const TerrainLayers& L = r.Gen().GetLayers();
    const uint32_t*      biome = L.GetUInt(L.FindLayer("biomeIndex"));
    for (int c = 0; c < dims.width * dims.height; ++c)
    {
        EXPECT_EQ(static_cast<uint32_t>(r.World().Cells()[c].biome), biome[c]);
    }
}

TEST(BiomeEdge, WriteTerrainBeforeUpdateLeavesBiomeUntouched)
{
    TerrainRules     rules = TestRules();
    TerrainGenerator g(rules);
    WorldStructure   w;
    w.Resize(GridDimensions{ 4, 3 });
    for (int c = 0; c < w.CellCount(); ++c) { w.Cells()[c].biome = Biome::Rainforest; }
    g.WriteTerrain(w);
    ASSERT_EQ(w.CellCount(), 12);
    for (int c = 0; c < w.CellCount(); ++c) { EXPECT_EQ(w.Cells()[c].biome, Biome::Rainforest); }

    // Same after an Update then Invalidate.
    Dia::Maths::RandomStream rng(1);
    g.Update(2, WorldShapeParams{}, GridDimensions{ 20, 14 }, rng);
    g.Invalidate();
    WorldStructure w2;
    w2.Resize(GridDimensions{ 4, 3 });
    for (int c = 0; c < w2.CellCount(); ++c) { w2.Cells()[c].biome = Biome::DesertBasin; }
    g.WriteTerrain(w2);
    for (int c = 0; c < w2.CellCount(); ++c) { EXPECT_EQ(w2.Cells()[c].biome, Biome::DesertBasin); }
}

TEST(BiomeEdge, InvalidateThenUpdateTwiceIsByteIdentical)
{
    const GridDimensions dims{ 66, 42 };
    TerrainRunner        r;
    const TerrainSnapshot first = r.RunFresh(21, WorldShapeParams{}, dims);
    const TerrainSnapshot second = r.RunFresh(21, WorldShapeParams{}, dims);
    EXPECT_TRUE(DiffSnapshots(first, second).empty());
    const uint32_t* a = r.Gen().GetLayers().GetUInt(r.Gen().GetLayers().FindLayer("biomeIndex"));
    std::vector<uint32_t> before(a, a + dims.width * dims.height);
    r.RunFresh(21, WorldShapeParams{}, dims);
    const uint32_t* b = r.Gen().GetLayers().GetUInt(r.Gen().GetLayers().FindLayer("biomeIndex"));
    EXPECT_EQ(0, std::memcmp(before.data(), b, before.size() * sizeof(uint32_t)));
}

// ---------------------------------------------------------------------------
// WorldGenSession stagesRan includes the biome stages (investigation of "no biome stages in stagesRan")
// ---------------------------------------------------------------------------
namespace
{
    bool HasStage(const Json::Value& arr, const char* s)
    {
        for (const Json::Value& v : arr) { if (v.isString() && v.asString() == s) { return true; } }
        return false;
    }
}

TEST(BiomeEdge, SessionStagesRanListsBiomeStages)
{
    TestSession          s;
    Dia::Maths::RandomStream rng(1);
    ASSERT_TRUE(s.Tick(rng));
    Json::Value ran = s.Status()["stagesRan"];
    EXPECT_TRUE(HasStage(ran, "Temperature"));
    EXPECT_TRUE(HasStage(ran, "Moisture"));
    EXPECT_TRUE(HasStage(ran, "Biomes"));
    EXPECT_EQ(ran.size(), static_cast<Json::ArrayIndex>(kImplementedStageCount));

    Json::Value a(Json::objectValue);
    a["temperatureOffset"] = 0.1;
    s.Set(a);
    ASSERT_TRUE(s.Tick(rng));
    ran = s.Status()["stagesRan"];
    EXPECT_TRUE(HasStage(ran, "Temperature"));
    EXPECT_FALSE(HasStage(ran, "Moisture"));
    EXPECT_TRUE(HasStage(ran, "Biomes"));
    EXPECT_EQ(ran.size(), 2u);

    Json::Value b(Json::objectValue);
    b["moistureOffset"] = 0.1;
    s.Set(b);
    ASSERT_TRUE(s.Tick(rng));
    ran = s.Status()["stagesRan"];
    EXPECT_FALSE(HasStage(ran, "Temperature"));
    EXPECT_TRUE(HasStage(ran, "Moisture"));
    EXPECT_TRUE(HasStage(ran, "Biomes"));
}

// ---------------------------------------------------------------------------
// Mutation survivors: band blend weights at the 30 / 60 degree boundaries, and fix-up tie-break
// ---------------------------------------------------------------------------
namespace
{
    // airMoisture at one cell of a grid whose only water is an ocean column on the west edge. The easterly sweep
    // carries no air to that cell, so airMoisture = westerlyWeight * westerlyAir. Returns the cell's air.
    float WesterlyOnlyAir(float latitudeDeg)
    {
        ClimateGrid g(8, 6, latitudeDeg / 90.0f);
        for (int y = 0; y < 6; ++y) { g.SetWater(0, y, WaterSource::Ocean); }
        return RunClimate(g, 0.0f, 0.0f).air[g.Index(2, 3)];
    }
}

TEST(BiomeEdge, BandBlendWeightsAtBoundaries)
{
    const float full = WesterlyOnlyAir(45.0f);   // weight 1
    ASSERT_GT(full, 0.5f);
    struct Case { float deg; float weight; };
    const Case cases[] = { { 20.0f, 0.0f }, { 24.0f, 0.0f }, { 25.0f, 0.0f }, { 27.5f, 0.25f }, { 30.0f, 0.5f },
                           { 32.5f, 0.75f }, { 35.0f, 1.0f }, { 40.0f, 1.0f }, { 55.0f, 1.0f }, { 57.5f, 0.75f },
                           { 60.0f, 0.5f }, { 62.5f, 0.25f }, { 65.0f, 0.0f }, { 70.0f, 0.0f }, { 90.0f, 0.0f } };
    for (const Case& c : cases)
    {
        EXPECT_NEAR(WesterlyOnlyAir(c.deg) / full, c.weight, 2e-3f) << c.deg << " deg";
    }
}

TEST(BiomeEdge, ZoneFixupTieGoesToLowestEnumCandidate)
{
    // Pinned Alpine on First Shores re-picks among Grassland / Forest; identical targets tie bit-exactly and the
    // lower enum value (Grassland) must win.
    TerrainRules rules = TestRules();
    rules.kT0_Forest = rules.kT0_Grass;
    rules.kM0_Forest = rules.kM0_Grass;
    rules.kWT_Forest = rules.kWT_Grass;
    rules.kWM_Forest = rules.kWM_Grass;
    BiomeGrid g(5, 5);
    const int c = g.Index(2, 2);
    g.SetTerrain(c, TerrainType::Mountain);
    g.SetZone(c, DepthZone::FirstShores);
    const BiomeResult r = RunBiomes(g, rules);
    EXPECT_EQ(r.At(c), Biome::GrasslandSteppe);
    EXPECT_EQ(r.fixup[c], BiomeFixup::Zone);
}

// Hand-computed stage 13 values on a one-row chain (westerly band, lat 45 deg: weight exactly 1).
// Ocean at x=0 with elevation 0.1 (so no orographic rise at x=1). Defaults: decay 0.04, mix 0.4, scale 8, river 2.
TEST(BiomeEdge, Stage13ChainMatchesHandComputedValues)
{
    ClimateGrid g(6, 1, 0.5f);
    g.SetWater(0, 0, WaterSource::Ocean);
    g.elevation[0] = 0.1f;
    const ClimateOut o = RunClimate(g, 0.0f, 0.0f);
    for (int k = 0; k < 6; ++k)
    {
        EXPECT_EQ(o.dist[k], static_cast<uint32_t>(k));
        const float air = (k == 0) ? 1.0f : 1.0f - 0.04f * static_cast<float>(k);
        EXPECT_NEAR(o.air[k], air, 1e-5f) << k;
        EXPECT_NEAR(o.rain[k], 0.0f, 1e-6f) << k;
        const float expected = 0.4f * std::exp(-static_cast<float>(k) / 8.0f) + 0.6f * air;
        EXPECT_NEAR(o.moist[k], std::min(expected, 1.0f), 1e-5f) << k;
    }
}

TEST(BiomeEdge, Stage13OrographicRainOutValuesAndClamp)
{
    // Rise of 0.1 at x=3: rainOut = min(carried 0.92, 4.0 * 0.1) = 0.4; air = 0.92 - 0.04 - 0.4 = 0.48.
    {
        ClimateGrid g(6, 1, 0.5f);
        g.SetWater(0, 0, WaterSource::Ocean);
        g.elevation[0] = 0.1f;
        g.elevation[3] = 0.2f;
        const ClimateOut o = RunClimate(g, 0.0f, 0.0f);
        EXPECT_NEAR(o.rain[3], 0.4f, 1e-5f);
        EXPECT_NEAR(o.air[3], 0.48f, 1e-5f);
        EXPECT_NEAR(o.moist[3], 0.4f * std::exp(-3.0f / 8.0f) + 0.6f * 0.48f + 0.4f, 1e-5f);
    }
    // Rise of 0.5: the min clamps to what the air carries (0.92); air drops to 0.
    {
        ClimateGrid g(6, 1, 0.5f);
        g.SetWater(0, 0, WaterSource::Ocean);
        g.elevation[0] = 0.1f;
        g.elevation[3] = 0.6f;
        const ClimateOut o = RunClimate(g, 0.0f, 0.0f);
        EXPECT_NEAR(o.rain[3], 0.92f, 1e-5f);
        EXPECT_NEAR(o.air[3], 0.0f, 1e-5f);
    }
    // A descent (negative rise) produces no rain.
    {
        ClimateGrid g(6, 1, 0.5f);
        g.SetWater(0, 0, WaterSource::Ocean);
        g.elevation[0] = 0.1f;
        g.elevation[1] = 0.5f;
        const ClimateOut o = RunClimate(g, 0.0f, 0.0f);
        EXPECT_NEAR(o.rain[2], 0.0f, 1e-6f);
    }
}
