// S-1 Terrain Generator task 13: WriteTerrain (Cell output and provenance). Checks the Cell array against
// the layers field by field, the AC2 / AC3 / AC10 / AC21 data contracts, determinism and the live-preview path.
// Wide sweeps are SLOW_; TerrainPerf reports the Release time of WriteTerrain at Small.
#include <gtest/gtest.h>
#include <Shared/WorldGeneration/TerrainGenerator.h>

#include <DiaPathfinding/HexTopology.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <map>

using namespace CoW::WorldGeneration;
using Dia::Maths::RandomStream;
using Dia::Pathfinding::HexTopology;
using Dia::Pathfinding::kNoCell;

namespace {

const GridDimensions kSmall{ 66, 42 };

const float* F(const TerrainGenerator& g, const char* name) { return g.GetLayers().GetFloat(g.GetLayers().FindLayer(name)); }
const uint32_t* U(const TerrainGenerator& g, const char* name) { return g.GetLayers().GetUInt(g.GetLayers().FindLayer(name)); }

struct Gen
{
    TerrainRules rules = TerrainRules::Defaults();
    TerrainGenerator gen;
    RandomStream rng{ 1 };
    WorldStructure world;
    Gen() : gen(rules) {}
    void Go(uint64_t seed, const WorldShapeParams& s, GridDimensions d = kSmall)
    {
        gen.Update(seed, s, d, rng);
        gen.WriteTerrain(world);
    }
};

void CheckContract(Gen& g, uint64_t seed)
{
    SCOPED_TRACE(seed);
    const WorldStructure& w = g.world;
    const int W = w.dimensions.width, H = w.dimensions.height;
    ASSERT_GT(W, 0);
    ASSERT_GT(H, 0);
    ASSERT_EQ(w.CellCount(), W * H);
    HexTopology topo(W, H);
    const float coast = g.rules.kCoastElev();
    const int border = g.rules.GetInt(Rule::kBorderWidth);
    const uint32_t* lakeMask = U(g.gen, "lakeMask");
    std::map<uint32_t, float> lakeElev;
    bool anyWater = false, mainSeen = false;
    for (int y = 0; y < H; ++y)
    {
        for (int x = 0; x < W; ++x)
        {
            const int i = y * W + x;
            const Cell& c = w.At(x, y);
            ASSERT_GE(c.elevation, 0.0f); ASSERT_LE(c.elevation, 1.0f);
            ASSERT_GE(c.latitude, 0.0f);  ASSERT_LE(c.latitude, 1.0f);
            ASSERT_LE(static_cast<uint8_t>(c.waterSource), static_cast<uint8_t>(WaterSource::River));
            ASSERT_LT(static_cast<uint8_t>(c.terrain), static_cast<uint8_t>(TerrainType::Count));
            ASSERT_LE(static_cast<uint8_t>(c.depthZone), static_cast<uint8_t>(DepthZone::AncientInteriors));
            anyWater = anyWater || c.waterSource != WaterSource::None;
            mainSeen = mainSeen || c.landmass == w.mainLandmass;
            const bool ocean = c.landmass == 0;
            ASSERT_EQ(c.depthZone == DepthZone::None, ocean);
            if (ocean) { ASSERT_EQ(c.elevation, 0.0f); }
            else { ASSERT_GE(c.elevation, coast); }
            if (lakeMask[i] != 0)
            {
                auto it = lakeElev.find(lakeMask[i]);
                if (it == lakeElev.end()) { lakeElev[lakeMask[i]] = c.elevation; }
                else { ASSERT_EQ(it->second, c.elevation); }
            }
            if (std::min(std::min(x, W - 1 - x), std::min(y, H - 1 - y)) < border)
            {
                ASSERT_EQ(c.landmass, 0); ASSERT_EQ(c.elevation, 0.0f);
                ASSERT_EQ(c.waterSource, WaterSource::Ocean);
            }
            for (uint32_t d = 0; d < 6; ++d)
            {
                const uint32_t n = topo.NeighbourAt(static_cast<uint32_t>(i), d);
                if (n == kNoCell) { continue; }
                ASSERT_EQ(topo.NeighbourAt(n, (d + 3) % 6), static_cast<uint32_t>(i));
            }
            if (c.waterSource == WaterSource::River)
            {
                ASSERT_LE(c.downstream, 5);
                ASSERT_EQ(static_cast<uint32_t>(c.downstream), U(g.gen, "flowDirection")[i]);
                const uint32_t n = topo.NeighbourAt(static_cast<uint32_t>(i), c.downstream);
                ASSERT_NE(n, kNoCell);
                const WaterSource ws = w.Cells()[n].waterSource;
                ASSERT_TRUE(ws == WaterSource::River || ws == WaterSource::Lake || ws == WaterSource::Ocean);
            }
            else { ASSERT_EQ(c.downstream, kNoDirection); }
        }
    }
    ASSERT_TRUE(anyWater);
    ASSERT_TRUE(mainSeen);
    ASSERT_GT(w.mainLandmass, 0);
}

void CheckFields(const Gen& g)
{
    const int n = g.world.CellCount();
    ASSERT_EQ(n, std::max(g.world.dimensions.width, 0) * std::max(g.world.dimensions.height, 0));
    const uint32_t* flow = U(g.gen, "flowDirection");
    for (int i = 0; i < n; ++i)
    {
        const Cell& c = g.world.Cells()[i];
        ASSERT_EQ(c.elevation, F(g.gen, "elevation")[i]);
        ASSERT_EQ(c.latitude, F(g.gen, "latitude")[i]);
        ASSERT_EQ(static_cast<uint32_t>(c.waterSource), U(g.gen, "waterSource")[i]);
        ASSERT_EQ(static_cast<uint32_t>(c.terrain), U(g.gen, "terrainType")[i]);
        ASSERT_EQ(static_cast<uint32_t>(c.depthZone), U(g.gen, "depthZone")[i]);
        ASSERT_EQ(static_cast<uint32_t>(c.landmass), std::min<uint32_t>(U(g.gen, "landmass")[i], 255u));
        ASSERT_EQ(static_cast<uint32_t>(c.terrainFlags), U(g.gen, "terrainFlags")[i]);
        ASSERT_EQ(c.downstream, c.waterSource == WaterSource::River ? static_cast<uint8_t>(flow[i]) : kNoDirection);
    }
}

} // namespace

TEST(TerrainOutput, ContractAndFields)
{
    for (uint64_t seed : { 1ull, 7ull, 42ull, 2024ull })
    {
        Gen g;
        g.Go(seed, WorldShapeParams{});
        CheckContract(g, seed);
        CheckFields(g);
    }
}

TEST(TerrainOutput, Provenance)
{
    Gen g;
    WorldShapeParams s; s.rivers = 0.8f; s.lakes = 6;
    g.Go(99, s);
    const WorldProvenance& p = g.world.provenance;
    EXPECT_EQ(p.seed, 99u);
    EXPECT_TRUE(p.shape == s);
    EXPECT_EQ(p.presetId[0], '\0');
    EXPECT_EQ(p.generatorVersion, kTerrainGeneratorVersion);
    EXPECT_EQ(p.rulesVersion, g.rules.rulesVersion);
    EXPECT_EQ(g.world.mainLandmass, g.gen.GetEffective().mainLandmass);
    EXPECT_EQ(g.world.dimensions.width, kSmall.width);
    EXPECT_EQ(g.world.dimensions.height, kSmall.height);
}

TEST(TerrainOutput, RulesVersionPropagates)
{
    TerrainRules r = TerrainRules::Defaults();
    r.rulesVersion = 77;
    TerrainGenerator gen(r);
    RandomStream rng{ 1 };
    WorldStructure w;
    gen.Update(5, WorldShapeParams{}, kSmall, rng);
    gen.WriteTerrain(w);
    EXPECT_EQ(w.provenance.rulesVersion, 77u);
}

TEST(TerrainOutput, ClampedShapeIsStoredAsGiven)
{
    // Update stores the shape it was given; callers Clamp() first. Provenance mirrors the Update argument.
    Gen g;
    WorldShapeParams s; s.Clamp();
    g.Go(3, s);
    EXPECT_TRUE(g.world.provenance.shape == s);
}

TEST(TerrainOutput, DeterministicBytes)
{
    Gen g;
    g.Go(11, WorldShapeParams{});
    WorldStructure dirty;
    dirty.Resize(kSmall);
    std::memset(dirty.Cells(), 0xAB, sizeof(Cell) * static_cast<size_t>(dirty.CellCount()));
    g.gen.WriteTerrain(dirty);
    ASSERT_EQ(dirty.CellCount(), g.world.CellCount());
    EXPECT_EQ(0, std::memcmp(dirty.Cells(), g.world.Cells(), sizeof(Cell) * static_cast<size_t>(g.world.CellCount())));
    g.gen.WriteTerrain(g.world);
    EXPECT_EQ(0, std::memcmp(dirty.Cells(), g.world.Cells(), sizeof(Cell) * static_cast<size_t>(g.world.CellCount())));
}

TEST(TerrainOutput, LivePreviewReflectsSliderChange)
{
    Gen g;
    WorldShapeParams s;
    g.Go(21, s);
    auto count = [&](WaterSource w) { int n = 0; for (int i = 0; i < g.world.CellCount(); ++i) { n += g.world.Cells()[i].waterSource == w; } return n; };
    const int before = count(WaterSource::River);
    s.rivers = 1.0f;
    g.Go(21, s);
    CheckFields(g);
    int mask = 0;
    for (int i = 0; i < g.world.CellCount(); ++i) { mask += U(g.gen, "riverMask")[i] != 0; }
    EXPECT_GT(count(WaterSource::River), 0);
    EXPECT_NE(count(WaterSource::River), before);
    EXPECT_TRUE(g.world.provenance.shape == s);
    EXPECT_GT(mask, 0);
}

TEST(TerrainOutput, NeverUpdatedLeavesWorldUntouched)
{
    TerrainGenerator gen(TerrainRules::Defaults());
    WorldStructure w;
    w.Resize({ 3, 3 });
    w.mainLandmass = 9;
    w.provenance.seed = 5;
    gen.WriteTerrain(w);
    EXPECT_EQ(w.CellCount(), 9);
    EXPECT_EQ(w.mainLandmass, 9);
    EXPECT_EQ(w.provenance.seed, 5u);
    WorldStructure empty;
    gen.WriteTerrain(empty);
    EXPECT_EQ(empty.CellCount(), 0);
}

TEST(TerrainOutput, TinyGridsDoNotCrash)
{
    for (GridDimensions d : { GridDimensions{ 1, 1 }, GridDimensions{ 3, 3 }, GridDimensions{ 0, 0 } })
    {
        Gen g;
        g.Go(4, WorldShapeParams{}, d);
        EXPECT_EQ(g.world.CellCount(), d.width * d.height);
        EXPECT_EQ(g.world.provenance.seed, 4u);
        if (d.width > 0) { CheckFields(g); }
    }
}

TEST(SLOW_TerrainOutput, ContractAcrossSeeds)
{
    for (uint64_t seed = 100; seed < 120; ++seed)
    {
        Gen g;
        g.Go(seed, WorldShapeParams{});
        CheckContract(g, seed);
        CheckFields(g);
    }
}

TEST(TerrainPerf, WriteTerrainSmall)
{
    Gen g;
    g.Go(1, WorldShapeParams{});
    const auto t0 = std::chrono::steady_clock::now();
#ifdef NDEBUG
    constexpr int kReps = 200;
#else
    constexpr int kReps = 3;
#endif
    for (int i = 0; i < kReps; ++i) { g.gen.WriteTerrain(g.world); }
    const auto t1 = std::chrono::steady_clock::now();
    const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count() / kReps;
    std::printf("[TerrainPerf] WriteTerrain Small: %.4f ms\n", ms);
    SUCCEED();
}
