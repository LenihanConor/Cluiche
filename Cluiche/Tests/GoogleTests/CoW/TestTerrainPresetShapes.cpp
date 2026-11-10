// S-1 Terrain Generator task 16: AC18 (presets produce worlds), the shared shape checks themselves, and AC17
// performance (Release only). The 3 presets x 50 seeds sweep is SLOW_; a cheap subset runs by default.
#include <gtest/gtest.h>
#include <Shared/WorldGeneration/TerrainGenerator.h>
#include <Shared/WorldGeneration/WorldShapePreset.h>
#include <Shared/WorldGeneration/Testing/TerrainShapeChecks.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <set>
#include <string>
#include <vector>

using namespace CoW::WorldGeneration;
using namespace CoW::WorldGeneration::Testing;

namespace {

const GridDimensions kSmall{ 66, 42 };

std::string FindShippedPresetDir()
{
    std::error_code ec;
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
    WorldShapePreset p;
    RuleWarnings w;
    const std::string dir = FindShippedPresetDir();
    EXPECT_FALSE(dir.empty());
    const PresetResult r = LoadPreset(dir, id, p, w);
    EXPECT_TRUE(r.Ok()) << r.message;
    return p.params;
}

struct World
{
    TerrainRules             rules = TerrainRules::Defaults();
    TerrainGenerator         gen;
    Dia::Maths::RandomStream rng{ 1 };
    WorldStructure           world;
    World() : gen(rules) {}
    void Go(uint64_t seed, const WorldShapeParams& s, GridDimensions d = kSmall)
    {
        gen.Update(seed, s, d, rng);
        gen.WriteTerrain(world);
    }
};

const char* const kPresetIds[3] = { "continental", "archipelago", "highlands" };

// Runs the AC18 checks for one preset on seeds [first, first + count).
void CheckPreset(const char* id, uint64_t first, int count)
{
    const WorldShapeParams shape = LoadParams(id);
    WorldShapeParams noPasses = shape;
    noPasses.passes = 0;
    World w, w0;
    for (int k = 0; k < count; ++k)
    {
        const uint64_t seed = first + static_cast<uint64_t>(k);
        SCOPED_TRACE(std::string(id) + " seed " + std::to_string(seed));
        w.Go(seed, shape);
        const ShapeFailures f = CheckWorldShape(w.gen, w.world, w.rules);
        ASSERT_TRUE(f.Empty()) << f.Text();

        w0.Go(seed, noPasses);
        ShapeFailures f0;
        CheckSingleRidgeGroup(w0.gen, kSmall, f0);
        ASSERT_TRUE(f0.Empty()) << f0.Text();
        const ShapeFailures fw0 = CheckWorldShape(w0.gen, w0.world, w0.rules);
        ASSERT_TRUE(fw0.Empty()) << "passes=0 variant: " << fw0.Text();
    }
}

double Median(std::vector<double> v)
{
    std::sort(v.begin(), v.end());
    return v[v.size() / 2];
}

} // namespace

// ---- The shape checks must be able to fail --------------------------------------------------------
TEST(TerrainShapeChecks, CleanWorldPasses)
{
    World w;
    w.Go(1, WorldShapeParams{});
    const ShapeFailures f = CheckWorldShape(w.gen, w.world, w.rules);
    EXPECT_TRUE(f.Empty()) << f.Text();
}

TEST(TerrainShapeChecks, DetectsBrokenContractBorderAndLand)
{
    auto fresh = []() { auto w = std::make_unique<World>(); w->Go(1, WorldShapeParams{}); return w; };

    {   // land cell on the border
        auto w = fresh();
        Cell& c = w->world.At(0, 5);
        c.landmass = 1; c.elevation = 0.5f; c.depthZone = DepthZone::FirstShores;
        EXPECT_FALSE(CheckWorldShape(w->gen, w->world, w->rules).Empty());
    }
    {   // elevation out of range
        auto w = fresh();
        w->world.At(30, 20).elevation = 1.5f;
        EXPECT_FALSE(CheckWorldShape(w->gen, w->world, w->rules).Empty());
    }
    {   // ocean with a depth zone
        auto w = fresh();
        w->world.At(1, 1).depthZone = DepthZone::WildReaches;
        EXPECT_FALSE(CheckWorldShape(w->gen, w->world, w->rules).Empty());
    }
    {   // land fraction off target
        auto w = fresh();
        w->world.provenance.shape.landFraction = 0.60f;
        EXPECT_FALSE(CheckWorldShape(w->gen, w->world, w->rules).Empty());
    }
    {   // no water source anywhere
        auto w = fresh();
        for (int i = 0; i < w->world.CellCount(); ++i) { w->world.Cells()[i].waterSource = WaterSource::None; }
        EXPECT_FALSE(CheckWorldShape(w->gen, w->world, w->rules).Empty());
    }
}

TEST(TerrainShapeChecks, DetectsBrokenRiversAndZones)
{
    // Find a seed whose default world has a river.
    for (uint64_t seed = 1; seed < 20; ++seed)
    {
        World w;
        w.Go(seed, WorldShapeParams{});
        int river = -1;
        for (int i = 0; i < w.world.CellCount() && river < 0; ++i)
        {
            if (w.world.Cells()[i].waterSource == WaterSource::River) { river = i; }
        }
        if (river < 0) { continue; }
        ASSERT_TRUE(CheckWorldShape(w.gen, w.world, w.rules).Empty());

        {   // river cell rises above its upstream neighbour: lift the river cell's downstream target
            World a;
            a.Go(seed, WorldShapeParams{});
            Cell& c = a.world.Cells()[river];
            const Dia::Pathfinding::HexTopology topo(kSmall.width, kSmall.height);
            Cell& next = a.world.Cells()[topo.NeighbourAt(static_cast<uint32_t>(river), c.downstream)];
            next.elevation = c.elevation + 0.1f;
            EXPECT_FALSE(CheckWorldShape(a.gen, a.world, a.rules).Empty());
        }
        {   // broken path: turn the downstream cell to plain land
            World a;
            a.Go(seed, WorldShapeParams{});
            Cell& c = a.world.Cells()[river];
            const Dia::Pathfinding::HexTopology topo(kSmall.width, kSmall.height);
            Cell& next = a.world.Cells()[topo.NeighbourAt(static_cast<uint32_t>(river), c.downstream)];
            if (next.waterSource == WaterSource::River)
            {
                next.waterSource = WaterSource::None;
                EXPECT_FALSE(CheckWorldShape(a.gen, a.world, a.rules).Empty());
            }
        }
        {   // a zone on the wrong cell
            World a;
            a.Go(seed, WorldShapeParams{});
            for (int i = 0; i < a.world.CellCount(); ++i)
            {
                Cell& c = a.world.Cells()[i];
                if (c.landmass != 0 && c.depthZone == DepthZone::FirstShores)
                {
                    c.depthZone = DepthZone::AncientInteriors;   // a coast / shallow cell cannot be Ancient
                    break;
                }
            }
            EXPECT_FALSE(CheckWorldShape(a.gen, a.world, a.rules).Empty());
        }
        return;
    }
    FAIL() << "no seed with a river in 1..19";
}

TEST(TerrainShapeChecks, ConnectedGroupCounter)
{
    const GridDimensions d{ 10, 10 };
    EXPECT_EQ(CountConnectedGroups([](int) { return false; }, d), 0);
    EXPECT_EQ(CountConnectedGroups([](int) { return true; }, d), 1);
    // Two far-apart single cells.
    EXPECT_EQ(CountConnectedGroups([](int i) { return i == 11 || i == 88; }, d), 2);
    // A full row is one group.
    EXPECT_EQ(CountConnectedGroups([](int i) { return i / 10 == 4; }, d), 1);
}

TEST(TerrainShapeChecks, SingleRidgeGroupCheckFailsOnPassesAndPasses)
{
    // passes = 4 at spineToRing 0 gives several ridge groups on a typical seed.
    bool sawMulti = false;
    for (uint64_t seed = 1; seed <= 10 && !sawMulti; ++seed)
    {
        World w;
        WorldShapeParams s;
        s.spineToRing = 0.0f;
        s.passes = 4;
        w.Go(seed, s);
        ShapeFailures f;
        CheckSingleRidgeGroup(w.gen, kSmall, f);
        sawMulti = !f.Empty();
    }
    EXPECT_TRUE(sawMulti);
}

// ---- AC18 -----------------------------------------------------------------------------------------
TEST(TerrainPresetShapes, EveryPresetProducesValidWorldsCheapSubset)
{
    for (const char* id : kPresetIds)
    {
        CheckPreset(id, 1, 3);
    }
}

TEST(SLOW_TerrainPresetShapes, ContinentalFiftySeeds)  { CheckPreset("continental", 1000, 50); }
TEST(SLOW_TerrainPresetShapes, ArchipelagoFiftySeeds)  { CheckPreset("archipelago", 2000, 50); }
TEST(SLOW_TerrainPresetShapes, HighlandsFiftySeeds)    { CheckPreset("highlands", 3000, 50); }

TEST(TerrainPresetShapes, PresetsDifferInShapeAsNamed)
{
    // Archipelago keeps 2 continents; Highlands is more ring-like than Continental.
    World a, c, h;
    a.Go(5, LoadParams("archipelago"));
    c.Go(5, LoadParams("continental"));
    h.Go(5, LoadParams("highlands"));
    std::set<uint8_t> landmassesA, landmassesC;
    for (int i = 0; i < a.world.CellCount(); ++i)
    {
        if (a.world.Cells()[i].landmass) { landmassesA.insert(a.world.Cells()[i].landmass); }
        if (c.world.Cells()[i].landmass) { landmassesC.insert(c.world.Cells()[i].landmass); }
    }
    EXPECT_GE(landmassesA.size(), 3u);   // 2 continents + islands
    EXPECT_GE(landmassesC.size(), 1u);
    EXPECT_NE(a.world.provenance.shape.continents, c.world.provenance.shape.continents);
    EXPECT_GT(h.world.provenance.shape.spineToRing, c.world.provenance.shape.spineToRing);
}

// ---- AC17 (Release only) --------------------------------------------------------------------------
TEST(TerrainPerf, FullUpdateAndSliderChangesAtSmall)
{
#ifdef NDEBUG
    constexpr int kRuns = 50;
    // Full Invalidate() + Update, median of 50 (distinct seeds).
    for (const char* id : kPresetIds)
    {
        const WorldShapeParams shape = LoadParams(id);
        World w;
        std::vector<double> full;
        for (int i = 0; i < kRuns; ++i)
        {
            w.gen.Invalidate();
            const auto t0 = std::chrono::steady_clock::now();
            w.gen.Update(7000 + static_cast<uint64_t>(i), shape, kSmall, w.rng);
            const auto t1 = std::chrono::steady_clock::now();
            full.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
        }
        const double med = Median(full);
        std::printf("[TerrainPerf] %s full Invalidate+Update median %.3f ms (66x42, %d runs)\n", id, med, kRuns);
        EXPECT_LT(med, 20.0) << id;
    }

    // rivers / lakes / valleyDepth change, median of 50 alternating values.
    struct Slider { const char* name; void (*set)(WorldShapeParams&, bool); };
    const Slider sliders[] =
    {
        { "rivers",      [](WorldShapeParams& p, bool hi) { p.rivers = hi ? 0.8f : 0.3f; } },
        { "lakes",       [](WorldShapeParams& p, bool hi) { p.lakes = hi ? 9 : 3; } },
        { "valleyDepth", [](WorldShapeParams& p, bool hi) { p.valleyDepth = hi ? 0.9f : 0.2f; } },
    };
    for (const Slider& s : sliders)
    {
        World w;
        WorldShapeParams p;
        w.gen.Update(11, p, kSmall, w.rng);
        std::vector<double> ms;
        for (int i = 0; i < kRuns; ++i)
        {
            s.set(p, (i & 1) == 0);
            const auto t0 = std::chrono::steady_clock::now();
            w.gen.Update(11, p, kSmall, w.rng);
            const auto t1 = std::chrono::steady_clock::now();
            ms.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
        }
        const double med = Median(ms);
        std::printf("[TerrainPerf] %s change median %.3f ms (66x42, %d runs)\n", s.name, med, kRuns);
        EXPECT_LT(med, 5.0) << s.name;
    }
#else
    GTEST_SKIP() << "Release-only performance gate (AC17)";
#endif
}

// Timings are correct in every build.
TEST(TerrainTimings, OneEntryPerStageWithCorrectRanFlags)
{
    World w;
    w.Go(3, WorldShapeParams{});
    const StageTiming* t = w.gen.GetTimings();
    float sum = 0.0f;
    for (int s = 0; s < kImplementedStageCount; ++s)
    {
        EXPECT_EQ(static_cast<int>(t[s].stage), s);
        EXPECT_TRUE(t[s].ranLastUpdate) << s;
        EXPECT_GE(t[s].milliseconds, 0.0f);
        sum += t[s].milliseconds;
    }
    EXPECT_GT(w.gen.GetLastTotalMs(), 0.0f);
    EXPECT_NEAR(sum, w.gen.GetLastTotalMs(), std::max(1.0f, w.gen.GetLastTotalMs() * 0.5f));

    // A rivers change re-runs stages 6.. that read rivers (and only those); the rest report not-run.
    WorldShapeParams p;
    p.rivers = 0.9f;
    w.gen.Update(3, p, kSmall, w.rng);
    EXPECT_FALSE(t[static_cast<int>(TerrainStage::SeedFields)].ranLastUpdate);
    EXPECT_FALSE(t[static_cast<int>(TerrainStage::Landmass)].ranLastUpdate);
    EXPECT_FALSE(t[static_cast<int>(TerrainStage::Drainage)].ranLastUpdate);
    EXPECT_TRUE(t[static_cast<int>(TerrainStage::Rivers)].ranLastUpdate);
    EXPECT_FALSE(t[static_cast<int>(TerrainStage::Latitude)].ranLastUpdate);
    EXPECT_EQ(t[static_cast<int>(TerrainStage::Landmass)].milliseconds, 0.0f);

    // An identical Update runs nothing and reports zero total.
    w.gen.Update(3, p, kSmall, w.rng);
    for (int s = 0; s < kImplementedStageCount; ++s) { EXPECT_FALSE(t[s].ranLastUpdate) << s; }
    EXPECT_EQ(w.gen.GetLastTotalMs(), 0.0f);
}
