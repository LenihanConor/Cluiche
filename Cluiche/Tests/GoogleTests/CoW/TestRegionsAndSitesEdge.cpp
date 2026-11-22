// S-1 Regions, Nodes and Site Hook: exhaustive-pass edge cases (plan task 12).
// Generated worlds at the slider extremes (land fraction 0.20, no passes/ridges, one landmass, many islands, tiny
// grids), the regions layer paging edges, and the AC7 border-coherence measurement (tuning-gated, SLOW_).
// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/regions-and-sites.md (AC2, AC4, AC7, AC13)
#include <gtest/gtest.h>
#include <Shared/CoWStepSeedNames.h>
#include <Shared/CoWNodeKinds.h>
#include <DraoiHexWorldTerrain/DeriveSubSeed.h>
#include <DraoiHexWorldTerrain/LayoutCheck.h>
#include <DraoiHexWorldTerrain/RegionsAndSites.h>
#include <DraoiHexWorldTerrain/SiteRegistry.h>
#include <DraoiHexWorldTerrain/WorldGenSession.h>
#include <DraoiHexWorldTerrain/WorldShapePreset.h>
#include <DraoiHexWorldTerrain/Testing/RegionChecks.h>
#include <Shared/WorldGeneration/Testing/TerrainSliderDriver.h>
#include <Shared/WorldGeneration/CoWTesting/CoWTerrainRulesTestHelpers.h>

#include <DiaCore/Json/external/json/json.h>
#include <DiaMaths/Core/RandomStream.h>

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

using namespace CoW::WorldGeneration;
using namespace Draoi::HexWorld;
using namespace CoW::WorldGeneration::Testing;
using namespace Draoi::HexWorld::Testing;
using Dia::Maths::RandomStream;

namespace
{
    const GridDimensions kSmall{ 66, 42 };

    std::string FindShippedPresetDir()
    {
        std::error_code       ec;
        std::filesystem::path dir = std::filesystem::absolute(std::filesystem::path(__FILE__), ec).parent_path();
        while (!dir.empty())
        {
            const std::filesystem::path candidate = dir / "Cluiche/Assets/CoW/Global/WorldGen/Presets";
            if (std::filesystem::exists(candidate, ec)) { return candidate.string(); }
            if (dir == dir.parent_path()) { break; }
            dir = dir.parent_path();
        }
        return {};
    }

    WorldShapeParams LoadParams(const char* id)
    {
        if (std::string(id) == "default") { return WorldShapeParams{}; }
        WorldShapePreset  p;
        RuleWarnings      w;
        const std::string dir = FindShippedPresetDir();
        EXPECT_FALSE(dir.empty());
        const PresetResult r = LoadPreset(dir, id, p, w);
        EXPECT_TRUE(r.Ok()) << r.message;
        return p.params;
    }

    struct Placed
    {
        WorldStructure                world;
        PlaceResult                   result;
        RegionsAndSites::PlaceReport  report;
        LayoutReport                  layout;
    };

    Placed GenerateAndPlace(uint64_t seed, const WorldShapeParams& shape, GridDimensions dims = kSmall)
    {
        TerrainRunner runner;
        runner.Run(seed, shape, dims);
        Placed p;
        p.world = runner.World();
        CoWSiteRegistry reg;
        RandomStream rng(1);
        const TerrainRules rules = TestRules();
        p.result = RegionsAndSites::Place(p.world, reg, rules, ::Draoi::HexWorld::Testing::ShippedBiomeRegistry(), DeriveSubSeed(seed, uint64_t{ 0 }), 0, rng, CoWStepSeedNames(), &p.report);
        p.layout = CheckLayout(p.world, rules, CoWNodeKindIds(), p.report.limits);
        return p;
    }

    std::string Violations(const LayoutReport& r)
    {
        std::string s;
        for (const LayoutViolation& v : r.violations) { s += std::string(LayoutCodeName(v.code)) + " x" + std::to_string(v.count) + " "; }
        return s;
    }

    bool HasViolation(const LayoutReport& r, LayoutCode code)
    {
        return std::any_of(r.violations.begin(), r.violations.end(), [&](const LayoutViolation& v) { return v.code == code; });
    }

    int CountLandmasses(const WorldStructure& w)
    {
        uint8_t maxId = 0;
        for (int i = 0; i < w.CellCount(); ++i) { maxId = std::max(maxId, w.Cells()[i].landmass); }
        std::vector<uint8_t> seen(maxId + 1u, 0);
        int n = 0;
        for (int i = 0; i < w.CellCount(); ++i)
        {
            const Cell& c = w.Cells()[i];
            if (ChkIsLand(c) && !seen[c.landmass]) { seen[c.landmass] = 1; ++n; }
        }
        return n;
    }

    int IslandRegions(const WorldStructure& w)
    {
        int n = 0;
        for (unsigned i = 0; i < w.regions.regions.Size(); ++i) { n += w.regions.regions[i].isIsland ? 1 : 0; }
        return n;
    }

    // Structure that must hold on every world, however extreme: coverage, connectivity, ids, records, neighbours.
    void ExpectValidPartition(const Placed& p)
    {
        const CheckErrors e = CheckAllRegions(p.world, -1.0f);
        EXPECT_TRUE(e.empty()) << ChkJoin(e);
        EXPECT_FALSE(p.report.regionBuild.overflow);
        EXPECT_EQ(p.report.regionBuild.unassignedLandCells, 0u);
        EXPECT_FALSE(HasViolation(p.layout, LayoutCode::UncoveredLand)) << Violations(p.layout);
        EXPECT_FALSE(HasViolation(p.layout, LayoutCode::OceanHasRegion)) << Violations(p.layout);
        EXPECT_FALSE(HasViolation(p.layout, LayoutCode::NodeOnWater)) << Violations(p.layout);
    }
}

// AC4: at landFraction 0.20 the world is terrain-limited if capacity requires it, never failed.
TEST(RegionsAndSitesEdge, LandFractionTwentyIsTerrainLimitedNeverFailed)
{
    for (const char* id : { "default", "continental", "archipelago", "highlands" })
    {
        WorldShapeParams shape = LoadParams(id);
        shape.landFraction = 0.20f;
        for (uint64_t seed : { 1ull, 2ull, 3ull })
        {
            SCOPED_TRACE(std::string(id) + " seed " + std::to_string(seed));
            const Placed p = GenerateAndPlace(seed, shape);
            ASSERT_TRUE(p.result.IsOk()) << p.result.reason;
            ExpectValidPartition(p);
            const int mainland = p.world.regions.mainlandRegionCount;
            EXPECT_TRUE((mainland >= 10 && mainland <= 16) || p.world.regions.limitedBy != kNoStratum)
                << "mainland " << mainland;
            EXPECT_FALSE(HasViolation(p.layout, LayoutCode::RegionCountRange)) << Violations(p.layout);
        }
    }
}

// No passes, no mountains, no barrier: corridors may be limited, generation still succeeds with a valid partition.
TEST(RegionsAndSitesEdge, NoPassesNoRidgesStillPlaces)
{
    WorldShapeParams shape;
    shape.passes = 0;
    shape.mountainousness = 0.0f;
    shape.barrier = 0.0f;
    shape.rivers = 0.0f;
    for (uint64_t seed : { 1ull, 2ull, 3ull })
    {
        SCOPED_TRACE("seed " + std::to_string(seed));
        const Placed p = GenerateAndPlace(seed, shape);
        ASSERT_TRUE(p.result.IsOk()) << p.result.reason;
        ExpectValidPartition(p);
        EXPECT_GT(p.world.regions.regions.Size(), 0u);
        EXPECT_FALSE(HasViolation(p.layout, LayoutCode::CorridorCount)) << Violations(p.layout);
        int pass = 0;
        for (int i = 0; i < p.world.CellCount(); ++i) { pass += (p.world.Cells()[i].terrainFlags & TerrainFlag::Pass) ? 1 : 0; }
        EXPECT_EQ(pass, 0);
    }
}

// One landmass: every region is a mainland region.
TEST(RegionsAndSitesEdge, SingleLandmassHasNoIslandRegions)
{
    WorldShapeParams shape;
    shape.continents = 1;
    shape.islands = 0;
    shape.continentIslands = 0.0f;
    shape.peninsulas = 0;
    for (uint64_t seed : { 1ull, 2ull, 3ull })
    {
        SCOPED_TRACE("seed " + std::to_string(seed));
        const Placed p = GenerateAndPlace(seed, shape);
        ASSERT_TRUE(p.result.IsOk()) << p.result.reason;
        ExpectValidPartition(p);
        const int landmasses = CountLandmasses(p.world);
        EXPECT_EQ(IslandRegions(p.world), landmasses - 1);   // 0 when the generator really made one landmass
        EXPECT_EQ(static_cast<int>(p.world.regions.mainlandRegionCount), static_cast<int>(p.world.regions.regions.Size()) - (landmasses - 1));
    }
}

// Many islands and continents: each non-main landmass is exactly one region, and the table never overflows.
TEST(RegionsAndSitesEdge, ManyIslandsAreOneRegionEach)
{
    WorldShapeParams shape;
    shape.continents = 3;
    shape.islands = 6;
    shape.continentIslands = 1.0f;
    shape.landFraction = 0.30f;
    for (uint64_t seed : { 1ull, 2ull, 3ull, 4ull })
    {
        SCOPED_TRACE("seed " + std::to_string(seed));
        const Placed p = GenerateAndPlace(seed, shape);
        ASSERT_TRUE(p.result.IsOk()) << p.result.reason;
        ExpectValidPartition(p);
        EXPECT_EQ(IslandRegions(p.world), CountLandmasses(p.world) - 1);
        EXPECT_LE(p.world.regions.regions.Size(), RegionTable::kMaxRegions);
    }
}

// Tiny grids: no crash, coverage holds, small land is reported terrain-limited rather than failing.
TEST(RegionsAndSitesEdge, TinyGridsPlaceWithoutFailing)
{
    for (GridDimensions dims : { GridDimensions{ 1, 1 }, GridDimensions{ 4, 3 }, GridDimensions{ 12, 8 }, GridDimensions{ 20, 14 } })
    {
        SCOPED_TRACE(std::to_string(dims.width) + "x" + std::to_string(dims.height));
        const Placed p = GenerateAndPlace(7, WorldShapeParams{}, dims);
        ASSERT_TRUE(p.result.IsOk()) << p.result.reason;
        ExpectValidPartition(p);
        if (p.world.regions.mainlandRegionCount < 10 && p.world.mainLandmass != 0)
        {
            EXPECT_NE(p.world.regions.limitedBy, kNoStratum) << "mainland " << int(p.world.regions.mainlandRegionCount);
        }
        EXPECT_FALSE(HasViolation(p.layout, LayoutCode::RegionCountRange)) << Violations(p.layout);
    }
}

// AC13: regions layer paging edges match the generator layers' contract.
TEST(RegionsAndSitesEdge, RegionsLayerPagingEdges)
{
    TestSession s;
    Json::Value a(Json::objectValue);
    a["layer"] = "regions";
    EXPECT_FALSE(s.Layers(a)["success"].asBool());   // nothing generated yet

    RandomStream rng(1);
    s.Regenerate(Json::Value(Json::objectValue));
    ASSERT_TRUE(s.Tick(rng));
    const int total = s.World().CellCount();

    Json::Value end = a; end["offset"] = total;
    const Json::Value re = s.Layers(end);
    ASSERT_TRUE(re["success"].asBool());
    EXPECT_EQ(re["count"].asInt(), 0);
    EXPECT_TRUE(re["nextOffset"].isNull());

    Json::Value zero = a; zero["offset"] = 10; zero["count"] = 0;
    const Json::Value rz = s.Layers(zero);
    ASSERT_TRUE(rz["success"].asBool());
    EXPECT_EQ(rz["count"].asInt(), 0);
    EXPECT_EQ(rz["nextOffset"].asInt(), 10);

    Json::Value past = a; past["offset"] = total + 1;
    EXPECT_FALSE(s.Layers(past)["success"].asBool());

    Json::Value last = a; last["offset"] = total - 1; last["count"] = 500;
    const Json::Value rl = s.Layers(last);
    ASSERT_TRUE(rl["success"].asBool());
    ASSERT_EQ(rl["values"].size(), 1u);
    EXPECT_EQ(rl["values"][0].asUInt(), s.World().Cells()[total - 1].region);
    EXPECT_TRUE(rl["nextOffset"].isNull());
}

namespace
{
    // Share of mainland region-border edges that run along a Ridge cell or a biome change.
    float BorderCoherence(const WorldStructure& w, const std::vector<RegionId>& reg)
    {
        uint32_t edges = 0, coherent = 0;
        for (int i = 0; i < w.CellCount(); ++i)
        {
            const Cell& a = w.Cells()[i];
            if (reg[i] == kNoRegion || a.landmass != w.mainLandmass) { continue; }
            for (int d = 0; d < 6; ++d)
            {
                const int j = ChkNeighbour(w, i, d);
                if (j < i || reg[j] == kNoRegion || reg[j] == reg[i] || w.Cells()[j].landmass != w.mainLandmass) { continue; }
                const Cell& b = w.Cells()[j];
                ++edges;
                const bool ridge = ((a.terrainFlags | b.terrainFlags) & TerrainFlag::Ridge) != 0;
                coherent += (ridge || a.biome != b.biome) ? 1u : 0u;
            }
        }
        return edges ? static_cast<float>(coherent) / static_cast<float>(edges) : 0.0f;
    }

    // Random same-size partition of the main landmass: `count` random seeds, multi-source BFS in random order.
    std::vector<RegionId> RandomPartition(const WorldStructure& w, int count, RandomStream& rng)
    {
        std::vector<RegionId> reg(static_cast<size_t>(w.CellCount()), kNoRegion);
        std::vector<int> land;
        for (int i = 0; i < w.CellCount(); ++i) { if (ChkIsLand(w.Cells()[i]) && w.Cells()[i].landmass == w.mainLandmass) { land.push_back(i); } }
        if (land.empty() || count <= 0) { return reg; }
        std::vector<int> frontier;
        for (int r = 1; r <= count && !land.empty(); ++r)
        {
            const int pick = static_cast<int>(static_cast<uint32_t>(rng.RandomInt()) % land.size());
            if (reg[land[pick]] != kNoRegion) { continue; }
            reg[land[pick]] = static_cast<RegionId>(r);
            frontier.push_back(land[pick]);
        }
        while (!frontier.empty())
        {
            const size_t k = static_cast<uint32_t>(rng.RandomInt()) % frontier.size();
            const int c = frontier[k];
            bool grew = false;
            for (int d = 0; d < 6; ++d)
            {
                const int nb = ChkNeighbour(w, c, d);
                if (nb < 0 || reg[nb] != kNoRegion || !ChkIsLand(w.Cells()[nb]) || w.Cells()[nb].landmass != w.mainLandmass) { continue; }
                reg[nb] = reg[c];
                frontier.push_back(nb);
                grew = true;
                break;
            }
            if (!grew) { frontier[k] = frontier.back(); frontier.pop_back(); }
        }
        return reg;
    }
}

// AC7 (tuning-gated): mean purity >= 0.6 on >= 90% of seeds, and borders follow ridges/biome changes more than a
// random same-size partition. Measured and printed only: both are tuning-gated and were left untuned at the AC15
// visual gate (2026-11-12); a failure here means the cost constants need tuning, not a code defect.
TEST(SLOW_RegionsAndSitesEdge, BorderCoherenceAndPurityTwentySeedsPerPreset)
{
    for (const char* id : { "default", "continental", "archipelago", "highlands" })
    {
        const WorldShapeParams shape = LoadParams(id);
        int purityPass = 0, coherentWins = 0, worlds = 0;
        float coherenceSum = 0.0f, randomSum = 0.0f, puritySum = 0.0f;
        for (uint64_t seed = 600; seed < 620; ++seed)
        {
            const Placed p = GenerateAndPlace(seed, shape);
            if (!p.result.IsOk()) { ADD_FAILURE() << id << " seed " << seed << ": " << p.result.reason; continue; }
            const WorldStructure& w = p.world;
            std::vector<RegionId> reg(static_cast<size_t>(w.CellCount()));
            for (int i = 0; i < w.CellCount(); ++i) { reg[i] = w.Cells()[i].region; }

            float purity = 0.0f; int mainland = 0;
            for (unsigned r = 0; r < w.regions.regions.Size(); ++r)
            {
                if (!w.regions.regions[r].isIsland) { purity += w.regions.regions[r].purity; ++mainland; }
            }
            purity = mainland ? purity / mainland : 0.0f;

            const float coherence = BorderCoherence(w, reg);
            RandomStream rr(seed * 31 + 5);
            float random = 0.0f;
            for (int k = 0; k < 5; ++k) { random += BorderCoherence(w, RandomPartition(w, mainland, rr)); }
            random /= 5.0f;

            ++worlds;
            purityPass += purity >= 0.6f ? 1 : 0;
            coherentWins += coherence > random ? 1 : 0;
            coherenceSum += coherence; randomSum += random; puritySum += purity;
        }
        std::printf("[AC7] %s: worlds %d purity avg %.3f (>=0.6 on %d) | border coherence avg %.3f vs random %.3f (wins %d/%d)\n",
                    id, worlds, worlds ? puritySum / worlds : 0.0f, purityPass, worlds ? coherenceSum / worlds : 0.0f,
                    worlds ? randomSum / worlds : 0.0f, coherentWins, worlds);
        // Tuning-gated (plan: report, do not tune): measured 2026-11-12, default/continental 10/20 wins (0.350 vs 0.342),
        // archipelago 18/20, highlands 19/20. Asserted only as "measured on every world".
        EXPECT_EQ(worlds, 20) << id;
    }
}
