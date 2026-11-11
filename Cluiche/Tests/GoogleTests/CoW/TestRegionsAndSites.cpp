// S-1 Regions, Nodes and Site Hook: RegionsAndSites::Place (AC1, AC9, placer failure, AC12) and CheckLayout (AC8).
#include <gtest/gtest.h>
#include <Shared/WorldGeneration/DeriveSubSeed.h>
#include <Shared/WorldGeneration/LayoutCheck.h>
#include <Shared/WorldGeneration/NodesCorridor.h>
#include <Shared/WorldGeneration/RegionsAndSites.h>
#include <Shared/WorldGeneration/TerrainGenerator.h>
#include <Shared/WorldGeneration/WorldShapePreset.h>
#include <Shared/WorldGeneration/Testing/RegionTestWorlds.h>
#include <Shared/WorldGeneration/Testing/TerrainSliderDriver.h>
#include <Shared/WorldGeneration/Testing/TestPlacers.h>

#include <DiaMaths/Core/RandomStream.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

using namespace CoW::WorldGeneration;
using namespace CoW::WorldGeneration::Testing;
using Dia::Core::StringCRC;
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

    WorldStructure MakeTerrain(uint64_t seed, const WorldShapeParams& shape)
    {
        TerrainRunner runner;
        runner.Run(seed, shape, kSmall);
        return runner.World();
    }

    bool SameExceptRegion(const Cell& a, const Cell& c)
    {
        return a.elevation == c.elevation && a.latitude == c.latitude && a.waterSource == c.waterSource &&
               a.terrain == c.terrain && a.depthZone == c.depthZone && a.landmass == c.landmass &&
               a.terrainFlags == c.terrainFlags && a.downstream == c.downstream && a.biome == c.biome;
    }

    bool SameTerrainAndBiomes(const WorldStructure& a, const WorldStructure& b)
    {
        if (a.CellCount() != b.CellCount()) { return false; }
        for (int i = 0; i < a.CellCount(); ++i) { if (!SameExceptRegion(a.Cells()[i], b.Cells()[i])) { return false; } }
        return true;
    }

    bool SameRegionCells(const WorldStructure& a, const WorldStructure& b)
    {
        for (int i = 0; i < a.CellCount(); ++i) { if (a.Cells()[i].region != b.Cells()[i].region) { return false; } }
        return true;
    }

    bool SameSpan(std::span<const Site> a, std::span<const Site> b) { return std::equal(a.begin(), a.end(), b.begin(), b.end()); }

    uint64_t AttemptSeed(uint64_t outer, int attempt) { return DeriveSubSeed(outer, static_cast<uint64_t>(attempt)); }

    PlaceResult PlaceWorld(WorldStructure& w, const SiteRegistry& reg, uint64_t outer, int attempt, RegionsAndSites::PlaceReport* rep = nullptr)
    {
        RandomStream rng(1);
        return RegionsAndSites::Place(w, reg, TerrainRules::Defaults(), AttemptSeed(outer, attempt), attempt, rng, rep);
    }

    // Records what it was given; its site cell depends on its stream.
    class RecPlacer : public ISitePlacer
    {
    public:
        RecPlacer(const char* id, std::initializer_list<const char*> deps, std::vector<std::string>* log,
                  PlaceResult::Code result = PlaceResult::Code::Ok)
            : mInfo(MakeSiteKindInfo(StringCRC(id), id, 0xFF00FF, SiteShape::Dot)), mLog(log), mResult(result)
        {
            for (const char* d : deps) { mDeps.push_back(StringCRC(d)); }
        }
        const SiteKindInfo& Info() const override { return mInfo; }
        std::span<const StringCRC> DependsOn() const override { return { mDeps.data(), mDeps.size() }; }
        PlaceResult Place(const PlacementContext& ctx, RandomStream& rng, SiteSet& out) override
        {
            ++calls;
            if (mLog) { mLog->push_back(mInfo.id); }
            firstDraw = rng.RandomInt();
            ctxAttempt = ctx.attempt;
            regionsAtEntry = ctx.world.regions.regions.Size();
            sitesAtEntry = ctx.world.sites.Count();
            depSites = 0;
            for (const StringCRC& d : mDeps) { depSites += ctx.world.sites.Count(d); }
            if (mResult == PlaceResult::Code::Retryable) { return PlaceResult::Retryable("no room"); }
            if (mResult == PlaceResult::Code::TerrainCause) { return PlaceResult::TerrainCause("no mountains"); }
            Site s;
            s.kindCrc = mInfo.kindCrc;
            s.cell = static_cast<uint32_t>(firstDraw) % static_cast<uint32_t>(ctx.world.CellCount());
            out.Add(s);
            return PlaceResult::Ok();
        }
        int calls = 0, firstDraw = -1, ctxAttempt = -1, depSites = 0, sitesAtEntry = -1;
        unsigned regionsAtEntry = 0;
    private:
        SiteKindInfo              mInfo;
        std::vector<StringCRC>    mDeps;
        std::vector<std::string>* mLog;
        PlaceResult::Code         mResult;
    };

    int ExpectedFirstDraw(uint64_t attemptSeed, const char* stepId)
    {
        RandomStream e(1);
        e.Restore(FoldSeed(DeriveSubSeed(attemptSeed, StringCRC(stepId))), 0);
        return e.RandomInt();
    }

    const uint32_t kBuiltin[4] = { SiteKindId(BuiltinNodeKinds::Corridor()), SiteKindId(BuiltinNodeKinds::River()),
                                   SiteKindId(BuiltinNodeKinds::Hunting()),  SiteKindId(BuiltinNodeKinds::Resource()) };
}

// ---- Place: determinism (AC1) ----------------------------------------------------------------------
TEST(RegionsAndSites, SameSeedAndAttemptIsByteIdentical)
{
    SiteRegistry reg;
    for (uint64_t seed : { 123ull, 4ull, 77ull })
    {
        const WorldStructure base = MakeTerrain(seed, WorldShapeParams{});
        WorldStructure a = base, b = base;
        ASSERT_TRUE(PlaceWorld(a, reg, seed, 0).IsOk());
        ASSERT_TRUE(PlaceWorld(b, reg, seed, 0).IsOk());
        EXPECT_TRUE(SameRegionCells(a, b)) << seed;
        EXPECT_TRUE(a.regions == b.regions) << seed;
        EXPECT_TRUE(a.sites == b.sites) << seed;
        EXPECT_GT(a.regions.regions.Size(), 0u);
        EXPECT_GT(a.sites.Count(), 0);
    }
}

TEST(RegionsAndSites, AttemptsDifferInRegionsButNotTerrainOrBiomes)
{
    SiteRegistry reg;
    int differ = 0, n = 0;
    for (uint64_t seed : { 123ull, 4ull, 77ull })
    {
        const WorldStructure base = MakeTerrain(seed, WorldShapeParams{});
        WorldStructure a0 = base, a1 = base;
        ASSERT_TRUE(PlaceWorld(a0, reg, seed, 0).IsOk());
        ASSERT_TRUE(PlaceWorld(a1, reg, seed, 1).IsOk());
        EXPECT_TRUE(SameTerrainAndBiomes(base, a0)) << seed;
        EXPECT_TRUE(SameTerrainAndBiomes(a0, a1)) << seed;
        ++n;
        differ += !SameRegionCells(a0, a1);
    }
    EXPECT_GE(differ * 10, n * 9) << differ << "/" << n;
}

TEST(RegionsAndSites, OnlyRegionsAndSitesAreWrittenAndPriorOnesAreDiscarded)
{
    SiteRegistry reg;
    WorldStructure w = MakeTerrain(123, WorldShapeParams{});
    const WorldStructure base = w;
    // junk from an earlier attempt
    for (int i = 0; i < w.CellCount(); ++i) { w.Cells()[i].region = 3; }
    Site junk; junk.kindCrc = 12345; junk.cell = 0;
    w.sites.Add(junk);
    ASSERT_TRUE(PlaceWorld(w, reg, 123, 0).IsOk());
    EXPECT_TRUE(SameTerrainAndBiomes(base, w));
    EXPECT_EQ(w.sites.Of(12345u).size(), 0u);
    WorldStructure again = base;
    ASSERT_TRUE(PlaceWorld(again, reg, 123, 0).IsOk());
    EXPECT_TRUE(SameRegionCells(w, again));
    EXPECT_TRUE(w.regions == again.regions);
    EXPECT_TRUE(w.sites == again.sites);
}

TEST(RegionsAndSites, BuiltInNodesAreInRegionsAndLimitsAreReported)
{
    SiteRegistry reg;
    WorldStructure w = MakeTerrain(123, WorldShapeParams{});
    RegionsAndSites::PlaceReport rep;
    ASSERT_TRUE(PlaceWorld(w, reg, 123, 0, &rep).IsOk());
    EXPECT_TRUE(rep.result.IsOk());
    EXPECT_EQ(rep.failedPlacerIndex, -1);
    EXPECT_TRUE(rep.ranPlacers.empty());
    int builtin = 0;
    for (uint32_t k : kBuiltin) { builtin += static_cast<int>(w.sites.Of(k).size()); }
    EXPECT_EQ(builtin, w.sites.Count());
    EXPECT_GT(w.sites.Of(SiteKindId(BuiltinNodeKinds::Corridor())).size(), 0u);
    EXPECT_EQ(rep.limits.regionLimitedBy, w.regions.limitedBy);
    for (const Site& s : w.sites.All()) { EXPECT_EQ(s.region, w.Cells()[s.cell].region); }
}

// ---- Place: registered kinds (AC9) -----------------------------------------------------------------
TEST(RegionsAndSites, RegisteredKindsRunAfterBuiltInsInDependencyOrderWithOwnStreams)
{
    std::vector<std::string> log;
    RecPlacer a("Test.A", {}, &log), b("Test.B", { "Test.A" }, &log), c("Test.C", {}, &log);
    SiteRegistry reg;
    reg.Register(&b); reg.Register(&a); reg.Register(&c);   // B registered before its dependency

    WorldStructure w = MakeTerrain(123, WorldShapeParams{});
    RegionsAndSites::PlaceReport rep;
    ASSERT_TRUE(PlaceWorld(w, reg, 123, 1, &rep).IsOk());

    ASSERT_EQ(log.size(), 3u);
    const auto posA = std::find(log.begin(), log.end(), "Test.A") - log.begin();
    const auto posB = std::find(log.begin(), log.end(), "Test.B") - log.begin();
    EXPECT_LT(posA, posB);
    EXPECT_EQ(rep.ranPlacers.size(), 3u);

    const uint64_t as = AttemptSeed(123, 1);
    EXPECT_EQ(a.firstDraw, ExpectedFirstDraw(as, "Test.A"));
    EXPECT_EQ(b.firstDraw, ExpectedFirstDraw(as, "Test.B"));
    EXPECT_EQ(c.firstDraw, ExpectedFirstDraw(as, "Test.C"));
    EXPECT_EQ(a.ctxAttempt, 1);

    // They see regions and the built-in nodes; B sees A's site.
    EXPECT_GT(a.regionsAtEntry, 0u);
    EXPECT_GT(a.sitesAtEntry, 0);
    EXPECT_EQ(b.depSites, 1);
    EXPECT_EQ(w.sites.Count(StringCRC("Test.A")), 1);
    EXPECT_EQ(w.sites.Count(StringCRC("Test.B")), 1);
}

TEST(RegionsAndSites, RegisteringOrRemovingAKindLeavesOthersIdentical)
{
    const WorldStructure base = MakeTerrain(123, WorldShapeParams{});
    RecPlacer a("Test.A", {}, nullptr), b("Test.B", { "Test.A" }, nullptr), c("Test.C", {}, nullptr);

    SiteRegistry ab; ab.Register(&a); ab.Register(&b);
    SiteRegistry abc; abc.Register(&a); abc.Register(&b); abc.Register(&c);
    SiteRegistry cab; cab.Register(&c); cab.Register(&a); cab.Register(&b);   // C first
    SiteRegistry removed; removed.Register(&a); removed.Register(&b); removed.Register(&c); removed.Unregister(StringCRC("Test.C"));

    WorldStructure w0 = base, w1 = base, w2 = base, w3 = base;
    ASSERT_TRUE(PlaceWorld(w0, ab, 123, 0).IsOk());
    ASSERT_TRUE(PlaceWorld(w1, abc, 123, 0).IsOk());
    ASSERT_TRUE(PlaceWorld(w2, cab, 123, 0).IsOk());
    ASSERT_TRUE(PlaceWorld(w3, removed, 123, 0).IsOk());
    EXPECT_EQ(w1.sites.Count(StringCRC("Test.C")), 1);
    EXPECT_EQ(w0.sites.Count(StringCRC("Test.C")), 0);
    for (const WorldStructure* o : { &w1, &w2, &w3 })
    {
        EXPECT_TRUE(SameRegionCells(w0, *o));
        EXPECT_TRUE(w0.regions == o->regions);
        for (uint32_t k : kBuiltin) { EXPECT_TRUE(SameSpan(w0.sites.Of(k), o->sites.Of(k))) << k; }
        EXPECT_TRUE(SameSpan(w0.sites.Of(StringCRC("Test.A")), o->sites.Of(StringCRC("Test.A"))));
        EXPECT_TRUE(SameSpan(w0.sites.Of(StringCRC("Test.B")), o->sites.Of(StringCRC("Test.B"))));
    }
}

// ---- Place: failures -------------------------------------------------------------------------------
TEST(RegionsAndSites, RetryableEndsTheAttemptAndLaterPlacersDoNotRun)
{
    RecPlacer a("Test.A", {}, nullptr), bad("Test.Bad", { "Test.A" }, nullptr, PlaceResult::Code::Retryable), c("Test.C", { "Test.Bad" }, nullptr);
    SiteRegistry reg; reg.Register(&a); reg.Register(&bad); reg.Register(&c);
    WorldStructure w = MakeTerrain(123, WorldShapeParams{});
    RegionsAndSites::PlaceReport rep;
    const PlaceResult r = PlaceWorld(w, reg, 123, 0, &rep);
    EXPECT_EQ(r.code, PlaceResult::Code::Retryable);
    EXPECT_EQ(rep.result.code, PlaceResult::Code::Retryable);
    EXPECT_EQ(rep.failedPlacerIndex, 1);
    EXPECT_EQ(a.calls, 1);
    EXPECT_EQ(bad.calls, 1);
    EXPECT_EQ(c.calls, 0);
    EXPECT_EQ(rep.ranPlacers.size(), 2u);
    EXPECT_EQ(w.sites.Count(StringCRC("Test.Bad")), 0);
}

TEST(RegionsAndSites, TerrainCauseNamesTheKind)
{
    RecPlacer bad("Test.Mountains", {}, nullptr, PlaceResult::Code::TerrainCause), c("Test.C", {}, nullptr);
    SiteRegistry reg; reg.Register(&bad); reg.Register(&c);
    WorldStructure w = MakeTerrain(123, WorldShapeParams{});
    RegionsAndSites::PlaceReport rep;
    const PlaceResult r = PlaceWorld(w, reg, 123, 0, &rep);
    EXPECT_EQ(r.code, PlaceResult::Code::TerrainCause);
    EXPECT_NE(std::string(r.reason).find("Test.Mountains"), std::string::npos) << r.reason;
    EXPECT_NE(std::string(r.reason).find("no mountains"), std::string::npos) << r.reason;
    EXPECT_EQ(rep.failedPlacerIndex, 0);
    EXPECT_EQ(c.calls, 0);
}

// ---- CheckLayout (AC8 detection) -------------------------------------------------------------------
namespace
{
    constexpr int kBlk = 12;

    void AddNode(WorldStructure& w, uint32_t kind, int x, int y, uint16_t variant = 0, uint16_t flags = 0)
    {
        Site s;
        s.kindCrc = kind;
        s.cell = static_cast<uint32_t>(CellIndex(w, x, y));
        s.region = w.Cells()[s.cell].region;
        s.variant = variant;
        s.flags = flags;
        w.sites.Add(s);
    }

    // 5x2 blocks of 12x12 land (10 regions of 144 cells), 9 Hunting nodes per region on a 4-step grid = 90 nodes.
    WorldStructure ValidWorld()
    {
        WorldStructure w = MakeRegionBlocksWorld(5, 2, kBlk, kBlk, Biome::GrasslandSteppe);
        w.regions.mainlandRegionCount = 10;
        for (int by = 0; by < 2; ++by)
            for (int bx = 0; bx < 5; ++bx)
                for (int ny = 0; ny < 3; ++ny)
                    for (int nx = 0; nx < 3; ++nx)
                        AddNode(w, SiteKindId(BuiltinNodeKinds::Hunting()), bx * kBlk + nx * 4, by * kBlk + ny * 4);
        return w;
    }

    LayoutLimits NoBarriers()
    {
        LayoutLimits l;
        l.corridorLimited[0] = l.corridorLimited[1] = true;   // all one zone: no barrier, no corridors
        return l;
    }

    const LayoutViolation* Find(const LayoutReport& r, LayoutCode c, RegionId region = 0)
    {
        for (const LayoutViolation& v : r.violations) { if (v.code == c && (region == 0 || v.region == region)) { return &v; } }
        return nullptr;
    }

    bool HasCell(const LayoutViolation& v, int cell)
    {
        for (unsigned i = 0; i < v.cellCount; ++i) { if (v.cells[i] == static_cast<uint32_t>(cell)) { return true; } }
        return false;
    }

    std::string Codes(const LayoutReport& r)
    {
        std::string s;
        for (const LayoutViolation& v : r.violations) { s += std::string(LayoutCodeName(v.code)) + " "; }
        return s;
    }
}

TEST(LayoutCheck, ValidHandBuiltWorldPasses)
{
    const WorldStructure w = ValidWorld();
    const LayoutReport r = CheckLayout(w, TerrainRules::Defaults(), NoBarriers());
    EXPECT_TRUE(r.ok) << Codes(r);
    EXPECT_TRUE(r.violations.empty());
}

TEST(LayoutCheck, UncoveredLandCell)
{
    WorldStructure w = ValidWorld();
    const int cell = CellIndex(w, 1, 1);
    w.Cells()[cell].region = 0;
    const LayoutReport r = CheckLayout(w, TerrainRules::Defaults(), NoBarriers());
    EXPECT_FALSE(r.ok);
    const LayoutViolation* v = Find(r, LayoutCode::UncoveredLand);
    ASSERT_NE(v, nullptr) << Codes(r);
    EXPECT_TRUE(HasCell(*v, cell));
    EXPECT_EQ(v->count, 1u);
}

TEST(LayoutCheck, OceanCellWithARegion)
{
    WorldStructure w = ValidWorld();
    const int cell = CellIndex(w, 1, 1);
    SetOcean(w, 1, 1);
    w.Cells()[cell].region = 2;
    const LayoutReport r = CheckLayout(w, TerrainRules::Defaults(), NoBarriers());
    const LayoutViolation* v = Find(r, LayoutCode::OceanHasRegion);
    ASSERT_NE(v, nullptr) << Codes(r);
    EXPECT_TRUE(HasCell(*v, cell));
}

TEST(LayoutCheck, DisconnectedRegion)
{
    WorldStructure w = ValidWorld();
    w.At(13, 1).region = 1;   // a cell of region 2 relabelled as region 1
    const LayoutReport r = CheckLayout(w, TerrainRules::Defaults(), NoBarriers());
    const LayoutViolation* v = Find(r, LayoutCode::RegionDisconnected, 1);
    ASSERT_NE(v, nullptr) << Codes(r);
    EXPECT_TRUE(HasCell(*v, CellIndex(w, 13, 1)));
}

TEST(LayoutCheck, TwoZoneRegion)
{
    WorldStructure w = ValidWorld();
    for (int y = 0; y < kBlk; ++y)
        for (int x = 3 * kBlk; x < 4 * kBlk; ++x) { w.At(x, y).depthZone = DepthZone::WildReaches; }   // region 4
    const LayoutReport r = CheckLayout(w, TerrainRules::Defaults(), NoBarriers());
    const LayoutViolation* v = Find(r, LayoutCode::RegionTwoZones, 4);
    ASSERT_NE(v, nullptr) << Codes(r);
    EXPECT_GT(v->count, 0u);
}

TEST(LayoutCheck, RegionUnderMinimumCells)
{
    WorldStructure w = ValidWorld();
    // region 10 (bx 4, by 1) keeps its right two columns; the rest goes to region 9
    for (int y = kBlk; y < 2 * kBlk; ++y)
        for (int x = 4 * kBlk; x < 4 * kBlk + 10; ++x) { w.At(x, y).region = 9; }
    const LayoutReport r = CheckLayout(w, TerrainRules::Defaults(), NoBarriers());
    const LayoutViolation* v = Find(r, LayoutCode::RegionTooSmall, 10);
    ASSERT_NE(v, nullptr) << Codes(r);
    EXPECT_EQ(v->count, 24u);
    EXPECT_EQ(Find(r, LayoutCode::RegionDisconnected), nullptr) << Codes(r);
}

TEST(LayoutCheck, RegionCountOutOfRangeUnlessTerrainLimited)
{
    WorldStructure w = MakeRegionBlocksWorld(3, 3, kBlk, kBlk, Biome::GrasslandSteppe);   // 9 regions
    LayoutLimits lim = NoBarriers();
    lim.nodeLimited = true;
    const LayoutReport r = CheckLayout(w, TerrainRules::Defaults(), lim);
    const LayoutViolation* v = Find(r, LayoutCode::RegionCountRange);
    ASSERT_NE(v, nullptr) << Codes(r);
    EXPECT_EQ(v->count, 9u);
    lim.regionLimitedBy = DepthZone::AncientInteriors;
    EXPECT_EQ(Find(CheckLayout(w, TerrainRules::Defaults(), lim), LayoutCode::RegionCountRange), nullptr);

    WorldStructure many = MakeRegionBlocksWorld(17, 1, kBlk, kBlk, Biome::GrasslandSteppe);   // 17 regions
    lim.regionLimitedBy = DepthZone::None;
    const LayoutReport r2 = CheckLayout(many, TerrainRules::Defaults(), lim);
    const LayoutViolation* v2 = Find(r2, LayoutCode::RegionCountRange);
    ASSERT_NE(v2, nullptr);
    EXPECT_EQ(v2->count, 17u);
}

TEST(LayoutCheck, AbsorbedCellsOverCap)
{
    WorldStructure w = ValidWorld();   // 1440 land cells, cap 43
    w.regions.absorbedCells = 43;
    EXPECT_EQ(Find(CheckLayout(w, TerrainRules::Defaults(), NoBarriers()), LayoutCode::AbsorbedOverCap), nullptr);
    w.regions.absorbedCells = 44;
    const LayoutReport r = CheckLayout(w, TerrainRules::Defaults(), NoBarriers());
    const LayoutViolation* v = Find(r, LayoutCode::AbsorbedOverCap);
    ASSERT_NE(v, nullptr) << Codes(r);
    EXPECT_EQ(v->count, 44u);
}

TEST(LayoutCheck, NodeOnWater)
{
    WorldStructure w = ValidWorld();
    SetOcean(w, 2, 2);
    AddNode(w, SiteKindId(BuiltinNodeKinds::Resource()), 2, 2);
    SetLake(w, 6, 2);
    w.At(6, 2).region = 1;
    AddNode(w, SiteKindId(BuiltinNodeKinds::Resource()), 6, 2);
    const LayoutReport r = CheckLayout(w, TerrainRules::Defaults(), NoBarriers());
    const LayoutViolation* v = Find(r, LayoutCode::NodeOnWater);
    ASSERT_NE(v, nullptr) << Codes(r);
    EXPECT_TRUE(HasCell(*v, CellIndex(w, 2, 2)));
    EXPECT_TRUE(HasCell(*v, CellIndex(w, 6, 2)));
}

TEST(LayoutCheck, NodesCloserThanSpacing)
{
    WorldStructure w = ValidWorld();
    AddNode(w, SiteKindId(BuiltinNodeKinds::Resource()), 1, 0);   // next to the node at (0,0)
    const LayoutReport r = CheckLayout(w, TerrainRules::Defaults(), NoBarriers());
    const LayoutViolation* v = Find(r, LayoutCode::NodeSpacing);
    ASSERT_NE(v, nullptr) << Codes(r);
    EXPECT_TRUE(HasCell(*v, CellIndex(w, 1, 0)));
    EXPECT_TRUE(HasCell(*v, CellIndex(w, 0, 0)));
}

TEST(LayoutCheck, SameCorridorNodesAreExemptFromSpacingAgainstEachOther)
{
    // Two corridor nodes of one width-2 corridor, adjacent, on an otherwise empty land block.
    WorldStructure w = MakeBlankWorld(20, 20);
    FillLand(w, 0, 0, 20, 20, DepthZone::FirstShores, Biome::GrasslandSteppe);
    AssignRegion(w, 0, 0, 20, 20, 1);
    BuildRegionRecords(w);
    AddNode(w, SiteKindId(BuiltinNodeKinds::Corridor()), 10, 10, 0, MakeCorridorFlags(1, 1, 2));
    AddNode(w, SiteKindId(BuiltinNodeKinds::Corridor()), 11, 10, 0, MakeCorridorFlags(1, 1, 2));
    LayoutLimits lim = NoBarriers();
    lim.nodeLimited = true;
    EXPECT_EQ(Find(CheckLayout(w, TerrainRules::Defaults(), lim), LayoutCode::NodeSpacing), nullptr);
    // A second corridor right next to them is a violation.
    AddNode(w, SiteKindId(BuiltinNodeKinds::Corridor()), 12, 11, 1, MakeCorridorFlags(1, 0, 1));
    const LayoutReport r = CheckLayout(w, TerrainRules::Defaults(), lim);
    const LayoutViolation* v = Find(r, LayoutCode::NodeSpacing);
    ASSERT_NE(v, nullptr);
    EXPECT_TRUE(HasCell(*v, CellIndex(w, 12, 11)));
}

TEST(LayoutCheck, FiveCorridorsOnABarrier)
{
    WorldStructure w = ValidWorld();
    const int xs[5] = { 2, 6, 10, 2, 6 }, ys[5] = { 2, 2, 2, 6, 6 };
    for (int i = 0; i < 5; ++i) { AddNode(w, SiteKindId(BuiltinNodeKinds::Corridor()), xs[i], ys[i], static_cast<uint16_t>(i), MakeCorridorFlags(1, 0, 1)); }
    LayoutLimits lim = NoBarriers();
    lim.corridorLimited[0] = false;
    const LayoutReport r = CheckLayout(w, TerrainRules::Defaults(), lim);
    const LayoutViolation* v = Find(r, LayoutCode::CorridorCount);
    ASSERT_NE(v, nullptr) << Codes(r);
    EXPECT_EQ(v->count, 5u);
}

TEST(LayoutCheck, TooFewCorridorsUnlessLimited)
{
    WorldStructure w = ValidWorld();
    AddNode(w, SiteKindId(BuiltinNodeKinds::Corridor()), 2, 2, 0, MakeCorridorFlags(1, 0, 1));
    LayoutLimits lim = NoBarriers();
    lim.corridorLimited[0] = false;
    EXPECT_NE(Find(CheckLayout(w, TerrainRules::Defaults(), lim), LayoutCode::CorridorCount), nullptr);
    lim.corridorLimited[0] = true;
    EXPECT_EQ(Find(CheckLayout(w, TerrainRules::Defaults(), lim), LayoutCode::CorridorCount), nullptr);
}

TEST(LayoutCheck, RegionWithSixteenNodes)
{
    WorldStructure w = ValidWorld();
    for (int i = 0; i < 7; ++i) { AddNode(w, SiteKindId(BuiltinNodeKinds::Resource()), 1 + i, 1); }   // spacing noise ignored
    const LayoutReport r = CheckLayout(w, TerrainRules::Defaults(), NoBarriers());
    const LayoutViolation* v = Find(r, LayoutCode::RegionNodeCount, 1);
    ASSERT_NE(v, nullptr) << Codes(r);
    EXPECT_EQ(v->count, 16u);
}

TEST(LayoutCheck, NodeTotalsAndRegionQuotasSkippedWhenNodeLimited)
{
    WorldStructure thin = MakeRegionBlocksWorld(5, 2, kBlk, kBlk, Biome::GrasslandSteppe);
    thin.regions.mainlandRegionCount = 10;
    for (int by = 0; by < 2; ++by)
        for (int bx = 0; bx < 5; ++bx)
            for (int nx = 0; nx < 3; ++nx) { AddNode(thin, SiteKindId(BuiltinNodeKinds::Hunting()), bx * kBlk + nx * 4, by * kBlk); }
    LayoutLimits lim = NoBarriers();
    const LayoutReport r = CheckLayout(thin, TerrainRules::Defaults(), lim);
    ASSERT_NE(Find(r, LayoutCode::TotalNodeCount), nullptr) << Codes(r);
    EXPECT_EQ(Find(r, LayoutCode::TotalNodeCount)->count, 30u);
    EXPECT_NE(Find(r, LayoutCode::RegionNodeCount), nullptr);
    lim.nodeLimited = true;
    const LayoutReport r2 = CheckLayout(thin, TerrainRules::Defaults(), lim);
    EXPECT_TRUE(r2.ok) << Codes(r2);
}

TEST(LayoutCheck, ViolationEvidenceIsCappedAtSixteenCells)
{
    WorldStructure w = ValidWorld();
    for (int i = 0; i < 30; ++i) { w.At(i % kBlk, 1 + i / kBlk).region = 0; }
    const LayoutReport r = CheckLayout(w, TerrainRules::Defaults(), NoBarriers());
    const LayoutViolation* v = Find(r, LayoutCode::UncoveredLand);
    ASSERT_NE(v, nullptr);
    EXPECT_EQ(v->count, 30u);
    EXPECT_EQ(v->cellCount, 16u);
}

// ---- Generated worlds ------------------------------------------------------------------------------
TEST(RegionsAndSites, GeneratedDefaultWorldsPassLayoutCheckThreeSeeds)
{
    SiteRegistry reg;
    for (uint64_t seed : { 123ull, 4ull, 77ull })
    {
        WorldStructure w = MakeTerrain(seed, WorldShapeParams{});
        RegionsAndSites::PlaceReport rep;
        ASSERT_TRUE(PlaceWorld(w, reg, seed, 0, &rep).IsOk());
        const LayoutReport r = CheckLayout(w, TerrainRules::Defaults(), rep.limits);
        EXPECT_TRUE(r.ok) << "seed " << seed << ": " << Codes(r);
    }
}

namespace
{
    struct PresetSweep
    {
        int worlds = 0, layoutPass = 0, differ = 0, terrainSame = 0, placeOk = 0;
        std::map<std::string, int> failing;
    };

    PresetSweep SweepPreset(const char* id, uint64_t firstSeed, int count, bool variation)
    {
        PresetSweep s;
        const WorldShapeParams shape = LoadParams(id);
        SiteRegistry reg;
        for (int i = 0; i < count; ++i)
        {
            const uint64_t seed = firstSeed + static_cast<uint64_t>(i);
            const WorldStructure base = MakeTerrain(seed, shape);
            WorldStructure w0 = base;
            RegionsAndSites::PlaceReport rep;
            ++s.worlds;
            if (!PlaceWorld(w0, reg, seed, 0, &rep).IsOk()) { continue; }
            ++s.placeOk;
            const LayoutReport r = CheckLayout(w0, TerrainRules::Defaults(), rep.limits);
            if (r.ok) { ++s.layoutPass; }
            for (const LayoutViolation& v : r.violations) { ++s.failing[LayoutCodeName(v.code)]; }
            if (variation)
            {
                WorldStructure w1 = base;
                PlaceWorld(w1, reg, seed, 1);
                s.differ += !SameRegionCells(w0, w1);
                s.terrainSame += SameTerrainAndBiomes(base, w0) && SameTerrainAndBiomes(base, w1);
            }
        }
        return s;
    }

    void Print(const char* id, const PresetSweep& s)
    {
        std::string f;
        for (const auto& kv : s.failing) { f += kv.first + "x" + std::to_string(kv.second) + " "; }
        std::printf("[RegionsAndSites sweep] %s: worlds %d placeOk %d layoutPass %d | attemptDiffers %d terrainSame %d | failing: %s\n",
                    id, s.worlds, s.placeOk, s.layoutPass, s.differ, s.terrainSame, f.c_str());
    }
}

// AC1 over four parameter sets, 20 seeds each.
TEST(SLOW_RegionsAndSites, AttemptVariationAndTerrainIdentityTwentySeedsPerPreset)
{
    for (const char* id : { "default", "continental", "archipelago", "highlands" })
    {
        const PresetSweep s = SweepPreset(id, 900, 20, true);
        Print(id, s);
        EXPECT_EQ(s.placeOk, s.worlds) << id;
        EXPECT_EQ(s.terrainSame, s.worlds) << id;
        EXPECT_GE(s.differ * 10, s.worlds * 9) << id;
    }
}

// AC8: >= 95% of 100 seeds pass the layout check at attempt 0 (default preset asserted; presets reported).
TEST(SLOW_RegionsAndSites, LayoutPassRateHundredSeedsDefault)
{
    const PresetSweep s = SweepPreset("default", 1000, 100, false);
    Print("default x100", s);
    EXPECT_EQ(s.placeOk, s.worlds);
    EXPECT_GE(s.layoutPass * 100, s.worlds * 95);
}

TEST(SLOW_RegionsAndSites, LayoutPassRateHundredSeedsPerPresetReported)
{
    for (const char* id : { "continental", "archipelago", "highlands" })
    {
        const PresetSweep s = SweepPreset(id, 2000, 100, false);
        Print(id, s);   // reported only: the archipelago partition has a known open issue
        EXPECT_EQ(s.placeOk, s.worlds) << id;
    }
}

// ---- AC12 (Release only) ---------------------------------------------------------------------------
namespace
{
    double Median(std::vector<double> v) { std::sort(v.begin(), v.end()); return v[v.size() / 2]; }

    double TimePlace(const WorldStructure& base, const SiteRegistry& reg, int runs)
    {
        WorldStructure w = base;
        std::vector<double> ms;
        for (int i = 0; i < runs; ++i)
        {
            RandomStream rng(1);
            const auto t0 = std::chrono::steady_clock::now();
            RegionsAndSites::Place(w, reg, TerrainRules::Defaults(), AttemptSeed(7, 0), 0, rng);
            const auto t1 = std::chrono::steady_clock::now();
            ms.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
        }
        return Median(ms);
    }
}

TEST(RegionsAndSitesPerf, PlaceSmallWithAndWithoutThreeKinds)
{
#ifdef NDEBUG
    const WorldStructure base = MakeTerrain(123, WorldShapeParams{});
    SiteRegistry none;
    TestPlacerSet t;
    SiteRegistry three; three.Register(&t.a); three.Register(&t.b); three.Register(&t.c);
    const double m0 = TimePlace(base, none, 50);
    const double m3 = TimePlace(base, three, 50);
    std::printf("[RegionsAndSitesPerf] Place (no kinds) median %.3f ms; with three kinds %.3f ms (+%.3f) (66x42, 50 runs)\n", m0, m3, m3 - m0);
    EXPECT_LT(m0, 15.0);
    EXPECT_LT(m3 - m0, 2.0);
#else
    GTEST_SKIP() << "Release-only performance gate (AC12)";
#endif
}
