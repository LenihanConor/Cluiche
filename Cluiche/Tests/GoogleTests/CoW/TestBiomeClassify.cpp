// S-1 Biome Classification task 4: Stage 14 (pins, plateau, climate score, zone and forbidden fix-ups).
// Hand-built grids only (no generator). AC2, AC3, AC4, AC5, AC6, AC7 parts.
#include <gtest/gtest.h>
#include <Shared/WorldGeneration/BiomeClassify.h>
#include <Shared/WorldGeneration/Testing/BiomeChecks.h>
#include <DraoiHexWorld/Testing/BiomeTestRegistry.h>
#include <Shared/WorldGeneration/Testing/BiomeTestGrids.h>
#include <Shared/WorldGeneration/CoWTesting/CoWTerrainRulesTestHelpers.h>

#include <DiaPathfinding/HexTopology.h>

#include <cmath>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <limits>
#include <vector>
#include <DraoiHexWorld/Testing/StratumTestIndices.h>

using namespace CoW::WorldGeneration;
using namespace Draoi::HexWorld;
using namespace CoW::WorldGeneration::Testing;
using namespace Draoi::HexWorld::Testing;

namespace
{
    constexpr float kEps = 1e-5f;

    int Nb(const BiomeGrid& g, int c, int d)
    {
        const Dia::Pathfinding::HexTopology topo(g.dims.width, g.dims.height);
        const uint32_t                      nb = topo.NeighbourAt(static_cast<uint32_t>(c), static_cast<uint32_t>(d));
        return nb == Dia::Pathfinding::kNoCell ? -1 : static_cast<int>(nb);
    }

    bool Adjacent(const BiomeGrid& g, int a, int b)
    {
        for (int d = 0; d < 6; ++d)
        {
            if (Nb(g, a, d) == b)
            {
                return true;
            }
        }
        return false;
    }

    // Interior cell of a 9x9 grid, far from every edge.
    constexpr int kCentreX = 4;
    constexpr int kCentreY = 4;

    template <typename T>
    bool BytesEqual(const std::vector<T>& a, const std::vector<T>& b)
    {
        return a.size() == b.size() && (a.empty() || std::memcmp(a.data(), b.data(), a.size() * sizeof(T)) == 0);
    }

    // Deterministic noise grid used by the invariant tests.
    BiomeGrid MakeNoiseGrid(uint32_t seed, int w, int h)
    {
        BiomeGrid g(w, h);
        uint32_t  s = seed * 2654435761u + 12345u;
        auto      rnd = [&s]() { s = s * 1664525u + 1013904223u; return static_cast<float>((s >> 8) & 0xFFFF) / 65535.0f; };
        const int n = w * h;
        for (int c = 0; c < n; ++c)
        {
            g.SetClimate(c, rnd(), rnd());
            const float z = rnd();
            g.SetZone(c, z < 0.33f ? kStratumFirst : (z < 0.75f ? kStratumWild : kStratumAncient));
            g.elevation[c] = rnd() * 0.3f;
            const float k = rnd();
            if (k < 0.08f)       { g.SetWater(c, WaterSource::Ocean); }
            else if (k < 0.12f)  { g.SetWater(c, WaterSource::Lake); }
            else if (k < 0.18f)  { g.SetWater(c, WaterSource::River); }
            else if (k < 0.24f)  { g.AddFlag(c, TerrainFlag::Volcanic); }
            else if (k < 0.32f)  { g.SetTerrain(c, TerrainType::Mountain); }
            else if (k < 0.45f)  { g.AddFlag(c, TerrainFlag::Coast); }
        }
        return g;
    }
}

// ---------------------------------------------------------------------------
// AC7: climate table
// ---------------------------------------------------------------------------
TEST(BiomeClassify, ClimateScoreFormula)
{
    EXPECT_NEAR(BiomeClimateScore(ShippedBiomeRegistry(), B("Biome.Grassland"), 0.60f, 0.35f), 0.0f, kEps);
    // One wT away in temperature: -1.
    EXPECT_NEAR(BiomeClimateScore(ShippedBiomeRegistry(), B("Biome.Grassland"), 0.85f, 0.35f), -1.0f, kEps);
    // One wM away in moisture: -1. Both: -2.
    EXPECT_NEAR(BiomeClimateScore(ShippedBiomeRegistry(), B("Biome.TemperateForest"), 0.50f, 0.80f), -1.0f, kEps);
    EXPECT_NEAR(BiomeClimateScore(ShippedBiomeRegistry(), B("Biome.Tundra"), 0.27f, 0.60f), -2.0f, kEps);
    EXPECT_NEAR(BiomeClimateScore(ShippedBiomeRegistry(), B("Biome.Desert"), 0.80f, 0.08f), 0.0f, kEps);
    EXPECT_NEAR(BiomeClimateScore(ShippedBiomeRegistry(), B("Biome.Rainforest"), 0.85f, 0.90f), 0.0f, kEps);
}

TEST(BiomeClassify, ClimateScoreOfNonClimateBiomeIsLowest)
{
    const float        lowest = std::numeric_limits<float>::lowest();
    EXPECT_EQ(BiomeClimateScore(ShippedBiomeRegistry(), B("Biome.Ocean"), 0.5f, 0.5f), lowest);
    EXPECT_EQ(BiomeClimateScore(ShippedBiomeRegistry(), B("Biome.Wetland"), 0.5f, 0.5f), lowest);
    EXPECT_EQ(BiomeClimateScore(ShippedBiomeRegistry(), B("Biome.Alpine"), 0.5f, 0.5f), lowest);
    EXPECT_EQ(BiomeClimateScore(ShippedBiomeRegistry(), B("Biome.Volcanic"), 0.5f, 0.5f), lowest);
}

TEST(BiomeClassify, ClimateTableWinners)
{
    // Test cells sit three columns apart so none is adjacent to another; the rest is Grassland-perfect land.
    struct Case { int x; int y; float t; float m; Draoi::HexWorld::BiomeIndex expect; };
    const Case cases[] = {
        { 1, 1,  0.85f, 0.90f, B("Biome.Rainforest") },        // warm + wet
        { 4, 1,  0.80f, 0.08f, B("Biome.Desert") },       // warm + dry
        { 7, 1,  0.10f, 0.30f, B("Biome.Tundra") },      // cold
        { 10, 1, 0.12f, 0.80f, B("Biome.Tundra") },      // cold stays tundra even wet (wM 0.30 is wide)
        { 1, 5,  0.60f, 0.30f, B("Biome.Grassland") },   // mid, low moisture; margin: forest score -2.17 vs -0.25
        { 4, 5,  0.50f, 0.65f, B("Biome.TemperateForest") },   // mid, high moisture; margin: grass -2.40... vs -0.06
        { 7, 5,  0.60f, 0.60f, B("Biome.TemperateForest") },   // M 0.60: forest -0.16 vs grass -1.56
        { 10, 5, 0.55f, 0.40f, B("Biome.Grassland") },   // M 0.40: grass -0.07 vs forest -1.30
    };
    BiomeGrid g(12, 8);
    for (const Case& k : cases) { g.SetClimate(g.Index(k.x, k.y), k.t, k.m); }
    const BiomeResult r = RunBiomes(g);
    for (const Case& k : cases)
    {
        const int c = g.Index(k.x, k.y);
        EXPECT_EQ(r.At(c), k.expect) << "cell " << k.x << "," << k.y;
        EXPECT_EQ(r.pin[c], BiomePin::None);
        EXPECT_EQ(r.fixup[c], BiomeFixup::None);
        EXPECT_NEAR(r.score[c], BiomeClimateScore(ShippedBiomeRegistry(), k.expect, k.t, k.m), kEps);
    }
}

TEST(BiomeClassify, BoundaryTiesGoToLowestRow)
{
    // Give two biomes identical envelopes (on a fixture registry: classification no longer reads the kT0_* rules)
    // so their scores are bit-equal; the lower row index must win.
    const Draoi::HexWorld::BiomeRegistry forestAsGrass =
        LoadShippedVariant({ { "\"climate\": {\"t0\": 0.5, \"m0\": 0.6, \"wt\": 0.25, \"wm\": 0.2}",
                        "\"climate\": {\"t0\": 0.6, \"m0\": 0.35, \"wt\": 0.25, \"wm\": 0.2}" } });
    BiomeGrid g(3, 3);
    g.registry = &forestAsGrass;
    EXPECT_EQ(RunBiomes(g).biome[g.Index(1, 1)], B(forestAsGrass, "Biome.Grassland"));

    // Rainforest set equal to Desert: Desert (row 5) beats Rainforest (row 8).
    const Draoi::HexWorld::BiomeRegistry rainAsDesert =
        LoadShippedVariant({ { "\"climate\": {\"t0\": 0.85, \"m0\": 0.9, \"wt\": 0.2, \"wm\": 0.15}",
                        "\"climate\": {\"t0\": 0.8, \"m0\": 0.08, \"wt\": 0.25, \"wm\": 0.12}" } });
    BiomeGrid g2(3, 3, 0.8f, 0.08f);
    g2.registry = &rainAsDesert;
    EXPECT_EQ(RunBiomes(g2).biome[g2.Index(1, 1)], B(rainAsDesert, "Biome.Desert"));
}

// ---------------------------------------------------------------------------
// AC6: pins and precedence
// ---------------------------------------------------------------------------
TEST(BiomeClassify, WaterCellsArePinnedOceanAndLake)
{
    BiomeGrid g(9, 9);
    const int ocean = g.Index(2, 2);
    const int lake = g.Index(6, 6);
    g.SetWater(ocean, WaterSource::Ocean);
    g.SetWater(lake, WaterSource::Lake);
    const BiomeResult r = RunBiomes(g);
    EXPECT_EQ(r.At(ocean), B("Biome.Ocean"));
    EXPECT_EQ(r.pin[ocean], BiomePin::Water);
    EXPECT_EQ(r.At(lake), B("Biome.Lake"));
    EXPECT_EQ(r.pin[lake], BiomePin::Water);
    EXPECT_EQ(r.fixup[ocean], BiomeFixup::None);
    EXPECT_EQ(r.fixup[lake], BiomeFixup::None);
}

TEST(BiomeClassify, WaterBeatsEveryOtherRule)
{
    BiomeGrid g(9, 9);
    const int c = g.Index(4, 4);
    g.SetWater(c, WaterSource::Ocean);
    g.AddFlag(c, TerrainFlag::Volcanic);
    g.SetTerrain(c, TerrainType::Volcano);
    const BiomeResult r = RunBiomes(g);
    EXPECT_EQ(r.At(c), B("Biome.Ocean"));
    EXPECT_EQ(r.pin[c], BiomePin::Water);
}

TEST(BiomeClassify, VolcanicPinRules)
{
    BiomeGrid g(12, 4);
    const int flag = g.Index(1, 1);
    const int volcano = g.Index(4, 1);
    const int chain = g.Index(7, 1);
    const int plateauBarrier = g.Index(10, 1);
    g.AddFlag(flag, TerrainFlag::Volcanic);
    g.SetTerrain(volcano, TerrainType::Volcano);
    g.AddFlag(chain, TerrainFlag::Barrier);
    g.AddFlag(plateauBarrier, TerrainFlag::Barrier);

    BiomeGrid chainGrid = g;
    chainGrid.barrierForm = BarrierForm::VolcanicChain;
    const BiomeResult rc = RunBiomes(chainGrid);
    EXPECT_EQ(rc.At(flag), B("Biome.Volcanic"));
    EXPECT_EQ(rc.pin[flag], BiomePin::Volcanic);
    EXPECT_EQ(rc.At(volcano), B("Biome.Volcanic"));
    EXPECT_EQ(rc.At(chain), B("Biome.Volcanic"));
    EXPECT_EQ(rc.pin[chain], BiomePin::Volcanic);
    EXPECT_EQ(rc.At(plateauBarrier), B("Biome.Volcanic"));   // chain form: any Barrier-flag cell

    // Plateau form: the Barrier flag alone is not volcanic.
    const BiomeResult rp = RunBiomes(g);
    EXPECT_EQ(rp.At(chain), B("Biome.Grassland"));
    EXPECT_EQ(rp.pin[chain], BiomePin::None);
    EXPECT_EQ(rp.At(flag), B("Biome.Volcanic"));
    EXPECT_EQ(rp.At(volcano), B("Biome.Volcanic"));
}

// ---------------------------------------------------------------------------
// Open pin rule ids: the generator's rule table validates the ids a biomes file declares.
// ---------------------------------------------------------------------------
TEST(BiomeClassify, BuiltInPinRuleTable)
{
    using Dia::Core::StringCRC;
    for (const char* name : { "water_ocean", "water_lake", "vent", "alpine", "wetland" })
    {
        EXPECT_TRUE(IsBuiltInPinRule(StringCRC(name))) << name;
    }
    EXPECT_FALSE(IsBuiltInPinRule(StringCRC("bogus_rule")));
    EXPECT_FALSE(IsBuiltInPinRule(StringCRC("")));
}

TEST(BiomeClassify, UnknownPinRuleFailsLoadNamingTheRow)
{
    std::ifstream     in(ShippedBiomesPath(), std::ios::binary);
    std::stringstream ss;
    ss << in.rdbuf();
    std::string text = ss.str();
    const std::string from = "\"rule\": \"vent\"";
    const size_t      at = text.find(from);
    ASSERT_NE(at, std::string::npos);
    text.replace(at, from.size(), "\"rule\": \"bogus_rule\"");

    Draoi::HexWorld::BiomeRegistry reg;
    Draoi::HexWorld::RegistryError err;
    ASSERT_TRUE(reg.LoadFromText(ss.str().c_str(), ::Draoi::HexWorld::Testing::ShippedStratumTable(), err, &IsBuiltInPinRule));
    const uint32_t count = reg.Count();
    const uint64_t hash = reg.Hash();

    EXPECT_FALSE(reg.LoadFromText(text.c_str(), ::Draoi::HexWorld::Testing::ShippedStratumTable(), err, &IsBuiltInPinRule));
    EXPECT_STREQ(err.field.AsCStr(), "pin");
    EXPECT_STREQ(err.id.AsCStr(), "Biome.Volcanic");
    EXPECT_GT(std::strlen(err.reason.AsCStr()), 0u);
    EXPECT_EQ(reg.Count(), count);   // all-or-nothing: the earlier load is untouched
    EXPECT_EQ(reg.Hash(), hash);

    Draoi::HexWorld::BiomeRegistry fresh;
    EXPECT_FALSE(fresh.LoadFromText(text.c_str(), ::Draoi::HexWorld::Testing::ShippedStratumTable(), err, &IsBuiltInPinRule));
    EXPECT_EQ(fresh.Count(), 0u);
}

TEST(BiomeClassify, NoVentBiomeLoadsAndGenerates)
{
    // The shipped rows with the vent biome's pin removed: no biome declares the `vent` rule, so it is unused.
    const Draoi::HexWorld::BiomeRegistry noVent = LoadShippedVariant({ { ", \"pin\": {\"rule\": \"vent\", \"priority\": 3}", "" } });
    EXPECT_EQ(noVent.Count(), ShippedBiomeRegistry().Count());

    BiomeGrid g(12, 4, 0.5f, 0.5f);
    g.registry = &noVent;
    const int volcano = g.Index(4, 1);
    const int flagged = g.Index(8, 1);
    g.SetTerrain(volcano, TerrainType::Volcano);
    g.AddFlag(flagged, TerrainFlag::Volcanic);
    g.SetWater(g.Index(1, 2), WaterSource::Ocean);
    g.SetTerrain(g.Index(10, 3), TerrainType::Mountain);

    const BiomeResult r = RunBiomes(g);
    EXPECT_EQ(r.pin[volcano], BiomePin::None);
    EXPECT_EQ(r.pin[flagged], BiomePin::None);
    EXPECT_EQ(r.At(g.Index(1, 2)), B(noVent, "Biome.Ocean"));   // the other rules still pin
    EXPECT_EQ(r.pin[g.Index(10, 3)], BiomePin::Alpine);
    for (int c = 0; c < g.dims.width * g.dims.height; ++c)
    {
        EXPECT_NE(r.pin[c], BiomePin::Volcanic);
        EXPECT_NE(r.At(c), Draoi::HexWorld::kNoBiome);
    }
}

TEST(BiomeClassify, AlpinePinRules)
{
    BiomeGrid g(12, 4);
    const TerrainType types[] = { TerrainType::Mountain, TerrainType::Peak, TerrainType::Cliff, TerrainType::Pass };
    int               cells[4];
    for (int i = 0; i < 4; ++i)
    {
        cells[i] = g.Index(1 + 3 * i, 1);
        g.SetTerrain(cells[i], types[i]);
    }
    const int hills = g.Index(1, 3);
    g.SetTerrain(hills, TerrainType::Hills);
    const BiomeResult r = RunBiomes(g);
    for (int i = 0; i < 4; ++i)
    {
        EXPECT_EQ(r.At(cells[i]), B("Biome.Alpine")) << TerrainTypeName(types[i]);
        EXPECT_EQ(r.pin[cells[i]], BiomePin::Alpine);
        EXPECT_EQ(r.fixup[cells[i]], BiomeFixup::None);
    }
    EXPECT_NE(r.At(hills), B("Biome.Alpine"));
}

TEST(BiomeClassify, RulePrecedenceVolcanicAlpineWetland)
{
    BiomeGrid g(12, 4);
    // Mountain + Volcanic flag -> Volcanic beats Alpine.
    const int a = g.Index(1, 1);
    g.SetTerrain(a, TerrainType::Mountain);
    g.AddFlag(a, TerrainFlag::Volcanic);
    // Mountain on a wet low coast (wetland-eligible) -> Alpine beats Wetland.
    const int b = g.Index(4, 1);
    g.SetTerrain(b, TerrainType::Mountain);
    g.AddFlag(b, TerrainFlag::Coast);
    g.moisture[b] = 0.9f;
    g.elevation[b] = 0.05f;
    // Volcanic on a wet low coast -> Volcanic beats Wetland.
    const int c = g.Index(7, 1);
    g.AddFlag(c, TerrainFlag::Volcanic);
    g.AddFlag(c, TerrainFlag::Coast);
    g.moisture[c] = 0.9f;
    g.elevation[c] = 0.05f;
    // Plain wet low coast -> Wetland.
    const int d = g.Index(10, 1);
    g.AddFlag(d, TerrainFlag::Coast);
    g.moisture[d] = 0.9f;
    g.elevation[d] = 0.05f;

    const BiomeResult r = RunBiomes(g);
    EXPECT_EQ(r.At(a), B("Biome.Volcanic"));
    EXPECT_EQ(r.pin[a], BiomePin::Volcanic);
    EXPECT_EQ(r.At(b), B("Biome.Alpine"));
    EXPECT_EQ(r.pin[b], BiomePin::Alpine);
    EXPECT_EQ(r.At(c), B("Biome.Volcanic"));
    EXPECT_EQ(r.pin[c], BiomePin::Volcanic);
    EXPECT_EQ(r.At(d), B("Biome.Wetland"));
    EXPECT_EQ(r.pin[d], BiomePin::Wetland);
}

// ---------------------------------------------------------------------------
// Plateau interior
// ---------------------------------------------------------------------------
TEST(BiomeClassify, PlateauInteriorChoosesRainforestOrDesertByMoisture)
{
    BiomeGrid g(12, 4);
    g.barrierForm = BarrierForm::Plateau;
    g.barrier = 0.6f;
    const int wet = g.Index(1, 1);
    const int dry = g.Index(4, 1);
    const int edge = g.Index(7, 1);
    const int wetBoundary = g.Index(10, 1);
    for (int c : { wet, dry, edge, wetBoundary }) { g.barrierMask[c] = 0.6f; }
    g.barrierMask[edge] = 0.49f;
    g.moisture[wet] = 0.9f;
    g.moisture[dry] = 0.3f;
    g.moisture[wetBoundary] = 0.5f;   // moisture >= kPlateauWet (0.5): Rainforest

    const BiomeResult r = RunBiomes(g);
    EXPECT_EQ(r.At(wet), B("Biome.Rainforest"));
    EXPECT_EQ(r.At(dry), B("Biome.Desert"));
    EXPECT_EQ(r.At(wetBoundary), B("Biome.Rainforest"));
    EXPECT_EQ(r.At(edge), B("Biome.Grassland"));   // mask below kPlateauInner: ordinary climate
    EXPECT_EQ(r.pin[wet], BiomePin::None);           // not pinned
    EXPECT_EQ(r.pin[dry], BiomePin::None);
    EXPECT_EQ(r.fixup[wet], BiomeFixup::None);
}

TEST(BiomeClassify, PlateauInteriorOffWhenBarrierZeroOrChainForm)
{
    BiomeGrid g(5, 5);
    const int c = g.Index(2, 2);
    g.barrierMask[c] = 0.9f;   // climate is Grassland-perfect (M 0.35); plateau rule would give Desert
    g.barrier = 0.0f;
    EXPECT_EQ(RunBiomes(g).At(c), B("Biome.Grassland"));

    g.barrier = 0.6f;
    g.barrierForm = BarrierForm::VolcanicChain;
    EXPECT_EQ(RunBiomes(g).At(c), B("Biome.Grassland"));

    g.barrierForm = BarrierForm::Plateau;
    EXPECT_EQ(RunBiomes(g).At(c), B("Biome.Desert"));   // M 0.35 < kPlateauWet
    g.moisture[c] = 0.9f;
    EXPECT_EQ(RunBiomes(g).At(c), B("Biome.Rainforest"));
}

namespace
{
    void MakePlateauRow(BiomeGrid& g, int y, int x0, int x1, const std::vector<float>& moisture)
    {
        g.barrierForm = BarrierForm::Plateau;
        g.barrier = 0.6f;
        for (int x = x0; x <= x1; ++x)
        {
            g.barrierMask[g.Index(x, y)] = 0.8f;
            g.moisture[g.Index(x, y)] = moisture[static_cast<size_t>(x - x0)];
        }
    }
}

TEST(BiomeClassify, PlateauComponentTakesOneClimateByMeanMoisture)   // amended 2026-11-10
{
    // Moisture straddles kPlateauWet (0.5) inside one connected plateau; the mean decides for every cell.
    BiomeGrid wetG(12, 4);
    MakePlateauRow(wetG, 1, 3, 5, { 0.9f, 0.2f, 0.8f });   // mean 0.633
    const BiomeResult wet = RunBiomes(wetG);
    for (int x = 3; x <= 5; ++x) { EXPECT_EQ(wet.At(wetG.Index(x, 1)), B("Biome.Rainforest")) << x; }

    BiomeGrid dryG(12, 4);
    MakePlateauRow(dryG, 1, 3, 5, { 0.1f, 0.9f, 0.3f });   // mean 0.433
    const BiomeResult dry = RunBiomes(dryG);
    for (int x = 3; x <= 5; ++x) { EXPECT_EQ(dry.At(dryG.Index(x, 1)), B("Biome.Desert")) << x; }
    EXPECT_EQ(dry.debug.unresolvedPairs, 0);
}

TEST(BiomeClassify, SeparatePlateausChooseIndependently)
{
    BiomeGrid g(12, 8);
    MakePlateauRow(g, 1, 2, 3, { 0.9f, 0.9f });
    MakePlateauRow(g, 5, 2, 3, { 0.1f, 0.1f });
    const BiomeResult r = RunBiomes(g);
    EXPECT_EQ(r.At(g.Index(2, 1)), B("Biome.Rainforest"));
    EXPECT_EQ(r.At(g.Index(3, 1)), B("Biome.Rainforest"));
    EXPECT_EQ(r.At(g.Index(2, 5)), B("Biome.Desert"));
    EXPECT_EQ(r.At(g.Index(3, 5)), B("Biome.Desert"));
}

TEST(BiomeClassify, PinnedCellSplitsPlateauComponents)
{
    // An Alpine-pinned cell in the middle splits the row: unsplit mean (0.475) would make everything Desert.
    BiomeGrid g(12, 4);
    MakePlateauRow(g, 1, 2, 6, { 0.9f, 0.9f, 0.5f, 0.05f, 0.05f });
    g.SetTerrain(g.Index(4, 1), TerrainType::Mountain);
    const BiomeResult r = RunBiomes(g);
    EXPECT_EQ(r.At(g.Index(4, 1)), B("Biome.Alpine"));
    EXPECT_EQ(r.At(g.Index(2, 1)), B("Biome.Rainforest"));
    EXPECT_EQ(r.At(g.Index(3, 1)), B("Biome.Rainforest"));
    EXPECT_EQ(r.At(g.Index(5, 1)), B("Biome.Desert"));
    EXPECT_EQ(r.At(g.Index(6, 1)), B("Biome.Desert"));
}

TEST(BiomeClassify, PlateauInteriorMayStillBeFixedUpByZone)
{
    BiomeGrid g(5, 5);
    const int c = g.Index(2, 2);
    g.barrier = 0.6f;
    g.barrierMask[c] = 0.8f;
    g.moisture[c] = 0.9f;
    g.SetZone(c, kStratumFirst);   // Rainforest is not valid in First Shores
    const BiomeResult r = RunBiomes(g);
    EXPECT_NE(r.At(c), B("Biome.Rainforest"));
    EXPECT_EQ(r.fixup[c], BiomeFixup::Zone);
}

// ---------------------------------------------------------------------------
// AC5: wetland eligibility
// ---------------------------------------------------------------------------
namespace
{
    struct MouthCase
    {
        BiomeGrid g{9, 9};
        int       ocean = 0, mouth = 0, step = 0, far = 0;
    };

    MouthCase MakeMouthCase()
    {
        MouthCase m;
        m.ocean = m.g.Index(kCentreX, kCentreY);
        m.g.SetWater(m.ocean, WaterSource::Ocean);
        m.mouth = Nb(m.g, m.ocean, 0);
        m.g.SetWater(m.mouth, WaterSource::River);
        m.g.elevation[m.mouth] = 0.05f;
        for (int d = 0; d < 6 && m.step == 0; ++d)
        {
            const int nb = Nb(m.g, m.mouth, d);
            if (nb >= 0 && nb != m.ocean && !Adjacent(m.g, nb, m.ocean))
            {
                m.step = nb;
            }
        }
        m.g.elevation[m.step] = 0.05f;
        m.far = m.g.Index(0, 0);
        m.g.elevation[m.far] = 0.05f;
        return m;
    }
}

TEST(BiomeClassify, WetlandEligibleAtRiverMouthAndOneStepFromIt)
{
    const TerrainRules rules = TestRules();
    MouthCase          m = MakeMouthCase();
    ASSERT_NE(m.step, 0);
    const Stage14Inputs in = m.g.Inputs();
    EXPECT_TRUE(IsRiverMouth(in, m.mouth));
    EXPECT_FALSE(IsRiverMouth(in, m.step));
    EXPECT_TRUE(IsWetlandEligible(rules, in, m.mouth));
    EXPECT_TRUE(IsWetlandEligible(rules, in, m.step));
    EXPECT_FALSE(IsWetlandEligible(rules, in, m.far));
    EXPECT_FALSE(IsWetlandEligible(rules, in, m.ocean));

    const BiomeResult r = RunBiomes(m.g);
    EXPECT_EQ(r.At(m.mouth), B("Biome.Wetland"));
    EXPECT_EQ(r.pin[m.mouth], BiomePin::Wetland);
    EXPECT_EQ(r.At(m.step), B("Biome.Wetland"));
    EXPECT_NE(r.At(m.far), B("Biome.Wetland"));
    EXPECT_TRUE(CheckWetlandRule(rules, in, r.biome.data()).empty());
}

TEST(BiomeClassify, WetlandElevationCap)
{
    const TerrainRules rules = TestRules();
    MouthCase          m = MakeMouthCase();
    m.g.elevation[m.step] = 0.12f;
    EXPECT_TRUE(IsWetlandEligible(rules, m.g.Inputs(), m.step));
    m.g.elevation[m.step] = 0.13f;
    EXPECT_FALSE(IsWetlandEligible(rules, m.g.Inputs(), m.step));
    EXPECT_NE(RunBiomes(m.g).At(m.step), B("Biome.Wetland"));
}

TEST(BiomeClassify, WetlandNextToLake)
{
    const TerrainRules rules = TestRules();
    BiomeGrid          g(9, 9);
    const int          lake = g.Index(kCentreX, kCentreY);
    g.SetWater(lake, WaterSource::Lake);
    const int shore = Nb(g, lake, 1);
    const int away = g.Index(0, 8);
    g.elevation[shore] = 0.05f;
    g.elevation[away] = 0.05f;
    EXPECT_TRUE(IsWetlandEligible(rules, g.Inputs(), shore));
    EXPECT_FALSE(IsWetlandEligible(rules, g.Inputs(), away));
    EXPECT_FALSE(IsWetlandEligible(rules, g.Inputs(), lake));
    g.elevation[shore] = 0.3f;
    EXPECT_FALSE(IsWetlandEligible(rules, g.Inputs(), shore));
}

TEST(BiomeClassify, WetlandWetCoast)
{
    const TerrainRules rules = TestRules();
    BiomeGrid          g(9, 9);
    const int          c = g.Index(2, 2);
    g.AddFlag(c, TerrainFlag::Coast);
    g.elevation[c] = 0.05f;
    g.moisture[c] = 0.7f;   // boundary: >= kWetlandMoist
    EXPECT_TRUE(IsWetlandEligible(rules, g.Inputs(), c));
    g.moisture[c] = 0.69f;
    EXPECT_FALSE(IsWetlandEligible(rules, g.Inputs(), c));

    // Wet but not on the coast.
    const int inland = g.Index(6, 6);
    g.elevation[inland] = 0.05f;
    g.moisture[inland] = 0.95f;
    EXPECT_FALSE(IsWetlandEligible(rules, g.Inputs(), inland));
}

// ---------------------------------------------------------------------------
// AC2: water cells are exactly Ocean / Lake
// ---------------------------------------------------------------------------
TEST(BiomeClassify, WaterCellsAreExactlyOceanAndLake)
{
    for (uint32_t seed = 1; seed <= 4; ++seed)
    {
        const BiomeGrid   g = MakeNoiseGrid(seed, 16, 16);
        const BiomeResult r = RunBiomes(g);
        for (size_t c = 0; c < r.biome.size(); ++c)
        {
            const bool ocean = g.waterSource[c] == static_cast<uint32_t>(WaterSource::Ocean);
            const bool lake = g.waterSource[c] == static_cast<uint32_t>(WaterSource::Lake);
            EXPECT_EQ(r.At(static_cast<int>(c)) == B("Biome.Ocean"), ocean);
            EXPECT_EQ(r.At(static_cast<int>(c)) == B("Biome.Lake"), lake);
            EXPECT_LT(r.biome[c], ::Draoi::HexWorld::Testing::ShippedBiomeRegistry().Count());
        }
    }
}

// ---------------------------------------------------------------------------
// Step 4: zone fix-up
// ---------------------------------------------------------------------------
TEST(BiomeClassify, PinnedAlpineOnFirstShoresBecomesGrasslandOrForest)
{
    BiomeGrid g(12, 4);
    const int grass = g.Index(1, 1);
    const int forest = g.Index(4, 1);
    for (int c : { grass, forest })
    {
        g.SetTerrain(c, TerrainType::Mountain);
        g.SetZone(c, kStratumFirst);
    }
    g.SetClimate(grass, 0.6f, 0.35f);
    g.SetClimate(forest, 0.5f, 0.62f);
    const BiomeResult r = RunBiomes(g);
    EXPECT_EQ(r.At(grass), B("Biome.Grassland"));
    EXPECT_EQ(r.At(forest), B("Biome.TemperateForest"));
    EXPECT_EQ(r.fixup[grass], BiomeFixup::Zone);
    EXPECT_EQ(r.fixup[forest], BiomeFixup::Zone);
    EXPECT_EQ(r.pin[grass], BiomePin::Alpine);   // the pin that fired is still reported
    EXPECT_EQ(r.debug.zoneChanges, 2);
}

TEST(BiomeClassify, ZoneFixupRepicksBestValidClimateBiome)
{
    BiomeGrid g(12, 8);
    const int desertFirst = g.Index(1, 1);      // desert climate, First Shores: Grass (-2.46) beats Forest (-8.2)
    const int rainFirst = g.Index(4, 1);        // rainforest climate, First Shores: Forest (-4.21) beats Grass (-8.56)
    const int grassAncient = g.Index(7, 1);     // grass climate, Ancient: Desert (-5.7) is the best valid
    const int desertAncient = g.Index(10, 1);   // desert in Ancient is valid: untouched
    g.SetClimate(desertFirst, 0.8f, 0.08f);
    g.SetZone(desertFirst, kStratumFirst);
    g.SetClimate(rainFirst, 0.85f, 0.9f);
    g.SetZone(rainFirst, kStratumFirst);
    g.SetZone(grassAncient, kStratumAncient);
    g.SetClimate(desertAncient, 0.8f, 0.08f);
    g.SetZone(desertAncient, kStratumAncient);

    const BiomeResult r = RunBiomes(g);
    EXPECT_EQ(r.At(desertFirst), B("Biome.Grassland"));
    EXPECT_EQ(r.fixup[desertFirst], BiomeFixup::Zone);
    EXPECT_EQ(r.At(rainFirst), B("Biome.TemperateForest"));
    EXPECT_EQ(r.fixup[rainFirst], BiomeFixup::Zone);
    EXPECT_EQ(r.At(grassAncient), B("Biome.Desert"));
    EXPECT_EQ(r.fixup[grassAncient], BiomeFixup::Zone);
    EXPECT_EQ(r.At(desertAncient), B("Biome.Desert"));
    EXPECT_EQ(r.fixup[desertAncient], BiomeFixup::None);
}

TEST(BiomeClassify, ZoneFixupRanksWetlandBelowClimateBiomes)
{
    // A pinned Alpine cell that is also wetland-eligible, on First Shores, becomes Grassland (not Wetland):
    // WetlandDelta has no climate score, so it is only the last-resort candidate.
    BiomeGrid g(9, 9);
    const int c = g.Index(2, 2);
    g.SetTerrain(c, TerrainType::Mountain);
    g.SetZone(c, kStratumFirst);
    g.AddFlag(c, TerrainFlag::Coast);
    g.elevation[c] = 0.05f;
    g.moisture[c] = 0.9f;
    g.temperature[c] = 0.6f;
    const BiomeResult r = RunBiomes(g);
    EXPECT_TRUE(r.At(c) == B("Biome.Grassland") || r.At(c) == B("Biome.TemperateForest"));
    EXPECT_EQ(r.fixup[c], BiomeFixup::Zone);
}

TEST(BiomeClassify, VentIslandKeepsVolcanicInAnyZone)
{
    BiomeGrid g(12, 4);
    const int flagged = g.Index(1, 1);
    const int vent = g.Index(4, 1);
    const int chainOnly = g.Index(7, 1);   // Barrier flag in chain form but not near a vent: no exemption
    g.AddFlag(flagged, TerrainFlag::Volcanic);
    g.SetTerrain(vent, TerrainType::Volcano);
    g.AddFlag(chainOnly, TerrainFlag::Barrier);
    g.barrierForm = BarrierForm::VolcanicChain;
    for (int c : { flagged, vent, chainOnly }) { g.SetZone(c, kStratumFirst); }

    const BiomeResult r = RunBiomes(g);
    EXPECT_EQ(r.At(flagged), B("Biome.Volcanic"));
    EXPECT_EQ(r.fixup[flagged], BiomeFixup::None);
    EXPECT_EQ(r.At(vent), B("Biome.Volcanic"));
    EXPECT_EQ(r.fixup[vent], BiomeFixup::None);
    EXPECT_NE(r.At(chainOnly), B("Biome.Volcanic"));
    EXPECT_EQ(r.fixup[chainOnly], BiomeFixup::Zone);

    EXPECT_TRUE(FindZoneViolations(g.Inputs(), r.biome.data()).empty());
}

TEST(BiomeClassify, WaterCellsAreSkippedByZoneFixup)
{
    BiomeGrid g(5, 5);
    const int c = g.Index(2, 2);
    g.SetWater(c, WaterSource::Lake);
    const BiomeResult r = RunBiomes(g);
    EXPECT_EQ(r.At(c), B("Biome.Lake"));
    EXPECT_EQ(r.fixup[c], BiomeFixup::None);
}

// ---------------------------------------------------------------------------
// Step 5: forbidden-neighbour fix-up
// ---------------------------------------------------------------------------
namespace
{
    // Rainforest-climate cell (slightly off target so its score is below 0) next to a perfect Desert cell.
    void BuildRainforestDesertPair(BiomeGrid& g, int rain, int desert)
    {
        g.SetClimate(rain, 0.85f, 0.8f);     // score -0.44
        g.SetClimate(desert, 0.8f, 0.08f);   // score 0
    }
}

TEST(BiomeClassify, ForbiddenPairLowerScoreCellMovesToCandidateWithNoForbiddenNeighbour)
{
    for (int lowerFirst = 0; lowerFirst < 2; ++lowerFirst)
    {
        BiomeGrid g(9, 9);
        const int a = g.Index(kCentreX, kCentreY);
        const int b = Nb(g, a, 0);
        const int rain = lowerFirst != 0 ? std::min(a, b) : std::max(a, b);
        const int desert = rain == a ? b : a;
        BuildRainforestDesertPair(g, rain, desert);

        const BiomeResult r = RunBiomes(g);
        // The Rainforest cell has the lower score, so it re-picks regardless of cell index.
        // Forest would score better than Grassland but forms a forbidden pair with the Desert neighbour.
        EXPECT_EQ(r.At(desert), B("Biome.Desert"));
        EXPECT_EQ(r.At(rain), B("Biome.Grassland"));
        EXPECT_EQ(r.fixup[rain], BiomeFixup::Forbidden);
        EXPECT_EQ(r.fixup[desert], BiomeFixup::None);
        EXPECT_EQ(r.debug.unresolvedPairs, 0);
        EXPECT_TRUE(FindForbiddenPairs(g.dims, r.biome.data()).empty());
    }
}

TEST(BiomeClassify, ForbiddenPairEqualPinEqualScoreLowerIndexMoves)
{
    BiomeGrid g(9, 9);
    const int a = g.Index(kCentreX, kCentreY);
    const int b = Nb(g, a, 0);
    const int lo = std::min(a, b);
    const int hi = std::max(a, b);
    g.SetClimate(lo, 0.85f, 0.9f);   // Rainforest, score 0
    g.SetClimate(hi, 0.8f, 0.08f);   // Desert, score 0
    const BiomeResult r = RunBiomes(g);
    EXPECT_EQ(r.At(hi), B("Biome.Desert"));
    EXPECT_NE(r.At(lo), B("Biome.Rainforest"));
    EXPECT_EQ(r.fixup[lo], BiomeFixup::Forbidden);
    EXPECT_EQ(r.fixup[hi], BiomeFixup::None);
}

TEST(BiomeClassify, ForbiddenPairLowerPinCellMoves)
{
    // Wetland (pin 1) next to Volcanic (pin 3): Wetland moves.
    {
        BiomeGrid g(9, 9);
        const int v = g.Index(kCentreX, kCentreY);
        const int w = Nb(g, v, 0);
        g.AddFlag(v, TerrainFlag::Volcanic);
        g.AddFlag(w, TerrainFlag::Coast);
        g.elevation[w] = 0.05f;
        g.moisture[w] = 0.9f;
        const BiomeResult r = RunBiomes(g);
        EXPECT_EQ(r.At(v), B("Biome.Volcanic"));
        EXPECT_NE(r.At(w), B("Biome.Wetland"));
        EXPECT_EQ(r.pin[w], BiomePin::Wetland);
        EXPECT_EQ(r.fixup[w], BiomeFixup::Forbidden);
        EXPECT_EQ(r.fixup[v], BiomeFixup::None);
    }
    // Tundra-climate (pin 0, perfect score) next to Volcanic (pin 3): Tundra moves.
    {
        BiomeGrid g(9, 9);
        const int v = g.Index(kCentreX, kCentreY);
        const int t = Nb(g, v, 3);
        g.AddFlag(v, TerrainFlag::Volcanic);
        g.SetClimate(t, 0.12f, 0.3f);
        const BiomeResult r = RunBiomes(g);
        EXPECT_EQ(r.At(v), B("Biome.Volcanic"));
        EXPECT_NE(r.At(t), B("Biome.Tundra"));
        EXPECT_EQ(r.fixup[t], BiomeFixup::Forbidden);
    }
}

namespace
{
    // X (Rainforest) has a Volcanic neighbour N1 (pin 3) and a Rainforest neighbour N2 on the opposite side.
    // X must move, but Desert and Tundra clash with N2 and Rainforest clashes with N1: only Alpine is free.
    void BuildStuckCell(BiomeGrid& g, int& x, int& n1, int& n2, StratumIndex zone)
    {
        x = g.Index(kCentreX, kCentreY);
        n1 = Nb(g, x, 0);
        n2 = Nb(g, x, 3);
        g.AddFlag(n1, TerrainFlag::Volcanic);
        g.SetClimate(x, 0.85f, 0.9f);
        g.SetClimate(n2, 0.85f, 0.9f);
        g.SetZone(x, zone);
        g.SetZone(n2, zone);
    }
}

TEST(BiomeClassify, StuckCellBecomesAlpineWhenZoneValid)   // amended 2026-11-10
{
    // AncientInteriors: Grassland and Forest are not zone-valid, so only Alpine is left.
    BiomeGrid g(9, 9);
    int       x, n1, n2;
    BuildStuckCell(g, x, n1, n2, kStratumAncient);
    ASSERT_FALSE(Adjacent(g, n1, n2));

    const BiomeResult r = RunBiomes(g);
    EXPECT_EQ(r.At(x), B("Biome.Alpine"));
    EXPECT_EQ(r.pin[x], BiomePin::None);
    EXPECT_EQ(r.fixup[x], BiomeFixup::Forbidden);
    EXPECT_EQ(r.At(n1), B("Biome.Volcanic"));
    EXPECT_EQ(r.At(n2), B("Biome.Rainforest"));
    EXPECT_EQ(r.debug.unresolvedPairs, 0);
    EXPECT_TRUE(FindForbiddenPairs(g.dims, r.biome.data()).empty());
}

TEST(BiomeClassify, WildReachesStuckLookingCellUsesForestNotAlpine)
{
    // In WildReaches Forest is zone-valid and forms no forbidden pair with Volcanic or Rainforest.
    BiomeGrid g(9, 9);
    int       x, n1, n2;
    BuildStuckCell(g, x, n1, n2, kStratumWild);
    const BiomeResult r = RunBiomes(g);
    EXPECT_EQ(r.At(x), B("Biome.TemperateForest"));
    EXPECT_EQ(r.debug.unresolvedPairs, 0);
}

TEST(BiomeClassify, AlpineSafetyNetNeverUsedWhenFreeCandidateExists)
{
    // A plain Rainforest/Desert clash resolves to Grassland, not Alpine.
    BiomeGrid g(9, 9);
    const int a = g.Index(kCentreX, kCentreY);
    const int b = Nb(g, a, 0);
    BuildRainforestDesertPair(g, a, b);
    const BiomeResult r = RunBiomes(g);
    EXPECT_NE(r.At(a), B("Biome.Alpine"));
    EXPECT_NE(r.At(b), B("Biome.Alpine"));
}

TEST(BiomeClassify, UnresolvablePairIsKeptAndCountedWhenAlpineNotZoneValid)
{
    // Alpine is not zone-valid in FirstShores, but no land cell there can be stuck (Grassland forms no forbidden
    // pair), so use kNoStratum: no land biome is zone-valid, so the pair stays and is counted.
    BiomeGrid g(9, 9);
    int       x, n1, n2;
    BuildStuckCell(g, x, n1, n2, kNoStratum);

    const BiomeResult r = RunBiomes(g);
    EXPECT_EQ(r.At(x), B("Biome.Rainforest"));
    EXPECT_EQ(r.At(n1), B("Biome.Volcanic"));
    EXPECT_EQ(r.At(n2), B("Biome.Rainforest"));
    EXPECT_EQ(r.fixup[x], BiomeFixup::None);
    EXPECT_EQ(r.debug.unresolvedPairs, 1);
    EXPECT_EQ(FindForbiddenPairs(g.dims, r.biome.data()).size(), 1u);
}

TEST(BiomeClassify, FirstShoresStuckNeighbourResolvesToGrasslandNotAlpine)
{
    // Rainforest is not valid in FirstShores: the zone fix-up moves it; Alpine is never chosen there.
    BiomeGrid g(9, 9);
    int       x, n1, n2;
    BuildStuckCell(g, x, n1, n2, kStratumFirst);
    const BiomeResult r = RunBiomes(g);
    EXPECT_NE(r.At(x), B("Biome.Alpine"));
    EXPECT_NE(r.At(n2), B("Biome.Alpine"));
    EXPECT_EQ(r.debug.unresolvedPairs, 0);
}

TEST(BiomeClassify, ForbiddenFixupHonoursPassLimit)
{
    // Chain of conflicts resolves from the left: with a single pass allowed the result must still be valid
    // (never a new violation) and unresolvedPairs must match what is left.
    TerrainRules rules = TestRules();
    rules.kMaxFixupPasses = 1;
    const BiomeGrid   g = MakeNoiseGrid(7, 14, 14);
    const BiomeResult r = RunBiomes(g, rules);
    EXPECT_LE(r.debug.passes, 1);
    EXPECT_EQ(static_cast<size_t>(r.debug.unresolvedPairs), FindForbiddenPairs(g.dims, r.biome.data()).size());
}

TEST(BiomeClassify, FixupNeverCreatesNewViolations)
{
    const TerrainRules rules = TestRules();
    for (uint32_t seed = 1; seed <= 12; ++seed)
    {
        const BiomeGrid     g = MakeNoiseGrid(seed, 20, 20);
        const BiomeResult   r = RunBiomes(g, rules);
        const Stage14Inputs in = g.Inputs();

        EXPECT_TRUE(FindZoneViolations(in, r.biome.data()).empty()) << "seed " << seed;
        EXPECT_EQ(static_cast<size_t>(r.debug.unresolvedPairs), FindForbiddenPairs(g.dims, r.biome.data()).size())
            << "seed " << seed;
        EXPECT_LE(r.debug.passes, rules.kMaxFixupPasses);
        EXPECT_TRUE(CheckWetlandRule(rules, in, r.biome.data()).empty()) << "seed " << seed;
        for (size_t c = 0; c < r.fixup.size(); ++c)
        {
            EXPECT_LE(r.fixup[c], BiomeFixup::Forbidden);
        }
    }
}

TEST(BiomeClassify, FixupReducesForbiddenPairsComparedToNoFixup)
{
    // With zero extra passes allowed the classifier still runs one pass (kMaxFixupPasses >= 1); compare against
    // the full default run: more passes never leave more pairs.
    TerrainRules one = TestRules();
    one.kMaxFixupPasses = 1;
    for (uint32_t seed = 1; seed <= 6; ++seed)
    {
        const BiomeGrid g = MakeNoiseGrid(seed, 20, 20);
        EXPECT_LE(RunBiomes(g).debug.unresolvedPairs, RunBiomes(g, one).debug.unresolvedPairs) << "seed " << seed;
    }
}

// ---------------------------------------------------------------------------
// Determinism and degenerate grids
// ---------------------------------------------------------------------------
TEST(BiomeClassify, DeterministicByteIdentical)
{
    const BiomeGrid   g = MakeNoiseGrid(3, 24, 24);
    const BiomeResult a = RunBiomes(g);
    const BiomeResult b = RunBiomes(g);
    EXPECT_TRUE(BytesEqual(a.score, b.score));
    EXPECT_TRUE(BytesEqual(a.pin, b.pin));
    EXPECT_TRUE(BytesEqual(a.biome, b.biome));
    EXPECT_TRUE(BytesEqual(a.fixup, b.fixup));
    EXPECT_EQ(a.debug.unresolvedPairs, b.debug.unresolvedPairs);
}

TEST(BiomeClassify, EmptyAndSingleCellGridsDoNotCrash)
{
    {
        BiomeGrid g(0, 0);
        const BiomeResult r = RunBiomes(g);
        EXPECT_TRUE(r.biome.empty());
        EXPECT_EQ(r.debug.unresolvedPairs, 0);
    }
    {
        BiomeGrid g(3, 0);
        EXPECT_TRUE(RunBiomes(g).biome.empty());
    }
    {
        BiomeGrid g(1, 1);
        const BiomeResult r = RunBiomes(g);
        ASSERT_EQ(r.biome.size(), 1u);
        EXPECT_EQ(r.At(0), B("Biome.Grassland"));
        EXPECT_EQ(r.pin[0], BiomePin::None);
        EXPECT_EQ(r.debug.unresolvedPairs, 0);
    }
    {
        BiomeGrid g(1, 1);
        g.SetWater(0, WaterSource::Ocean);
        EXPECT_EQ(RunBiomes(g).At(0), B("Biome.Ocean"));
    }
}

// ---------------------------------------------------------------------------
// Registry-driven classification (biome-registry plan Task 21): pin order, tie order and plateau rows come from the
// registry, not from code. Fixture registries are the shipped file with a few text edits.
// ---------------------------------------------------------------------------
namespace
{

    // A second climate biome whose envelope equals Grassland's bit for bit.
    const char* kGrassTwinRow =
        "{\"id\": \"Biome.GrassTwin\", \"name\": \"Grass Twin\", \"colour\": \"#a6c04b\", \"description\": \"Tie fixture.\", "
        "\"tags\": [\"land\"], \"generated\": true, \"zones\": [\"FirstShores\", \"WildReaches\"], "
        "\"climate\": {\"t0\": 0.6, \"m0\": 0.35, \"wt\": 0.25, \"wm\": 0.2}},\n    ";
}

TEST(BiomeClassify, PinsPlaceInRegistryPriorityOrder)
{
    // One cell that is both a vent (Volcanic flag) and a Mountain: the higher-priority pin wins.
    BiomeGrid g(5, 5);
    const int c = g.Index(2, 2);
    g.AddFlag(c, TerrainFlag::Volcanic);
    g.SetTerrain(c, TerrainType::Mountain);

    const BiomeResult shipped = RunBiomes(g);   // shipped priorities: vent 3 > alpine 2
    EXPECT_EQ(shipped.biome[c], B("Biome.Volcanic"));
    EXPECT_EQ(shipped.pin[c], BiomePin::Volcanic);

    const Draoi::HexWorld::BiomeRegistry swapped =
        LoadShippedVariant({ { "\"rule\": \"alpine\", \"priority\": 2", "\"rule\": \"alpine\", \"priority\": 3" },
                      { "\"rule\": \"vent\", \"priority\": 3", "\"rule\": \"vent\", \"priority\": 2" } });
    g.registry = &swapped;
    const BiomeResult r = RunBiomes(g);
    EXPECT_EQ(r.biome[c], B(swapped, "Biome.Alpine"));
    EXPECT_EQ(r.pin[c], BiomePin::Alpine);
}

TEST(BiomeClassify, ClimateTiesBreakByLowestRowIndex)
{
    // Grassland-perfect cell: Grassland and the twin score bit-equal; the lower row index wins.
    BiomeGrid g(3, 3);
    const int c = g.Index(1, 1);

    // Twin appended after Grassland: Grassland (row 2) wins.
    const Draoi::HexWorld::BiomeRegistry twinAfter =
        LoadShippedVariant({ { "{\"id\": \"Biome.Wetland\"", (std::string(kGrassTwinRow) + "{\"id\": \"Biome.Wetland\"").c_str() } });
    g.registry = &twinAfter;
    EXPECT_EQ(RunBiomes(g).biome[c], B(twinAfter, "Biome.Grassland"));
    EXPECT_LT(B(twinAfter, "Biome.Grassland"), B(twinAfter, "Biome.GrassTwin"));

    // Twin inserted before Grassland: the twin (now the lower row) wins.
    const Draoi::HexWorld::BiomeRegistry twinBefore =
        LoadShippedVariant({ { "{\"id\": \"Biome.Grassland\"", (std::string(kGrassTwinRow) + "{\"id\": \"Biome.Grassland\"").c_str() } });
    g.registry = &twinBefore;
    EXPECT_LT(B(twinBefore, "Biome.GrassTwin"), B(twinBefore, "Biome.Grassland"));
    EXPECT_EQ(RunBiomes(g).biome[c], B(twinBefore, "Biome.GrassTwin"));
}

TEST(BiomeClassify, PlateauCellsUseRegistryPlateauRows)
{
    BiomeGrid g(12, 4);
    g.barrierForm = BarrierForm::Plateau;
    g.barrier = 0.6f;
    const int wet = g.Index(1, 1);
    const int dry = g.Index(4, 1);
    for (int c : { wet, dry }) { g.barrierMask[c] = 0.6f; }
    g.moisture[wet] = 0.9f;
    g.moisture[dry] = 0.3f;

    const BiomeResult shipped = RunBiomes(g);
    EXPECT_EQ(shipped.biome[wet], B("Biome.Rainforest"));
    EXPECT_EQ(shipped.biome[dry], B("Biome.Desert"));

    // Swap which row is the wet and which the dry plateau biome.
    const Draoi::HexWorld::BiomeRegistry swapped =
        LoadShippedVariant({ { "\"plateau\": \"dry\"", "\"plateau\": \"TMP\"" },
                      { "\"plateau\": \"wet\"", "\"plateau\": \"dry\"" },
                      { "\"plateau\": \"TMP\"", "\"plateau\": \"wet\"" } });
    g.registry = &swapped;
    const BiomeResult r = RunBiomes(g);
    EXPECT_EQ(r.biome[wet], B(swapped, "Biome.Desert"));       // wet plateau row is now Desert
    EXPECT_EQ(r.biome[dry], B(swapped, "Biome.Rainforest"));   // dry plateau row is now Rainforest
}

TEST(BiomeClassify, ClassificationHasNoHardCodedBiomeLists)
{
    // A climate biome appended to the registry competes with no code change: a cell sitting exactly on its envelope
    // takes it (the shipped five-biome list would never have placed it).
    const Draoi::HexWorld::BiomeRegistry& extra = ExtraBiomeRegistry();
    const Draoi::HexWorld::BiomeIndex     savanna = B(extra, "Biome.Savanna");
    const Draoi::HexWorld::ClimateEnvelope* e = extra.Climate(savanna);
    ASSERT_NE(e, nullptr);
    BiomeGrid g(3, 3, e->t0, e->m0);
    g.registry = &extra;
    EXPECT_EQ(RunBiomes(g).biome[g.Index(1, 1)], savanna);
}
