// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/regions-and-sites.md (AC13), f1-debug-host-integration.md
// WorldGenSession regions and sites: computed on Regenerate only, cleared by live preview, regions/sites queries,
// extended cell / params / status queries.
#include <gtest/gtest.h>
#include <Shared/WorldGeneration/WorldGenSession.h>
#include <Shared/WorldGeneration/SiteRegistry.h>
#include <Shared/WorldGeneration/Testing/TestPlacers.h>

#include <DiaCore/Json/external/json/json.h>
#include <DiaMaths/Core/RandomStream.h>

#include <set>
#include <string>

using namespace CoW::WorldGeneration;
using namespace CoW::WorldGeneration::Testing;
using Dia::Core::StringCRC;

namespace
{

Json::Value Obj() { return Json::Value(Json::objectValue); }

bool IsError(const Json::Value& r) { return r.isObject() && r.isMember("success") && !r["success"].asBool() && !r["error"].asString().empty(); }

size_t ReplySize(const Json::Value& v) { return Json::FastWriter().write(v).size(); }

// Fresh session that has run one Regenerate.
struct Gen
{
    WorldGenSession s;
    Dia::Maths::RandomStream rng{ 1 };

    explicit Gen(const SiteRegistry* reg = nullptr)
    {
        if (reg) { s.SetSiteRegistry(reg); }
        s.Regenerate(Obj());
        EXPECT_TRUE(s.Tick(rng));
    }
};

int LandCells(const WorldStructure& w)
{
    int n = 0;
    for (int i = 0; i < w.CellCount(); ++i)
    {
        if (w.Cells()[i].terrain != TerrainType::DeepWater && w.Cells()[i].terrain != TerrainType::ShallowWater &&
            w.Cells()[i].terrain != TerrainType::Lake) { ++n; }
    }
    return n;
}

} // namespace

TEST(WorldGenSessionRegions, NothingGeneratedYetIsNotFresh)
{
    WorldGenSession s;
    EXPECT_FALSE(s.Status()["regionsFresh"].asBool());
    EXPECT_TRUE(s.Status()["layout"].isNull());
    EXPECT_EQ(s.Regions()["regions"].size(), 0u);
    EXPECT_EQ(s.Sites(Json::Value(Json::objectValue))["total"].asInt(), 0);
}

TEST(WorldGenSessionRegions, FirstGenerationWithoutRegenerateLeavesRegionsEmpty)
{
    WorldGenSession s;
    Dia::Maths::RandomStream rng(1);
    ASSERT_TRUE(s.Tick(rng));
    EXPECT_EQ(s.World().regions.regions.Size(), 0u);
    EXPECT_EQ(s.World().sites.Count(), 0);
    EXPECT_FALSE(s.Status()["regionsFresh"].asBool());
}

TEST(WorldGenSessionRegions, RegenerateTickPopulatesRegionsAndSites)
{
    Gen g;
    const WorldStructure& w = g.s.World();
    EXPECT_GT(w.regions.regions.Size(), 0u);
    EXPECT_GT(w.sites.Count(BuiltinNodeKinds::Corridor()), 0);
    EXPECT_GE(w.sites.Count(), 80);

    int land = 0;
    for (int i = 0; i < w.CellCount(); ++i)
    {
        const Cell& c = w.Cells()[i];
        const bool water = c.terrain == TerrainType::DeepWater || c.terrain == TerrainType::ShallowWater || c.terrain == TerrainType::Lake;
        if (!water)
        {
            ++land;
            EXPECT_NE(c.region, kNoRegion) << "land cell " << i;
        }
    }
    EXPECT_GT(land, 0);
    EXPECT_EQ(land, LandCells(w));

    const Json::Value st = g.s.Status();
    EXPECT_TRUE(st["regionsFresh"].asBool());
    ASSERT_TRUE(st["layout"].isObject());
    EXPECT_TRUE(st["layout"]["ok"].asBool()) << st["layout"].toStyledString();
    EXPECT_TRUE(st["layout"]["violations"].isArray());
    EXPECT_EQ(st["layout"]["violations"].size(), 0u);
}

TEST(WorldGenSessionRegions, LivePreviewSetClearsRegionsAndSitesUntilNextRegenerate)
{
    Gen g;
    ASSERT_TRUE(g.s.Status()["regionsFresh"].asBool());

    Json::Value a = Obj(); a["rivers"] = 0.9;
    g.s.Set(a);
    ASSERT_TRUE(g.s.Tick(g.rng));
    const WorldStructure& w = g.s.World();
    EXPECT_EQ(w.regions.regions.Size(), 0u);
    EXPECT_EQ(w.sites.Count(), 0);
    for (int i = 0; i < w.CellCount(); ++i) { ASSERT_EQ(w.Cells()[i].region, kNoRegion); }
    EXPECT_FALSE(g.s.Status()["regionsFresh"].asBool());
    EXPECT_TRUE(g.s.Status()["layout"].isNull());
    EXPECT_EQ(g.s.Regions()["regions"].size(), 0u);
    EXPECT_FALSE(g.s.Regions()["fresh"].asBool());
    EXPECT_EQ(g.s.Sites(Obj())["total"].asInt(), 0);

    g.s.Regenerate(Obj());
    ASSERT_TRUE(g.s.Tick(g.rng));
    EXPECT_GT(g.s.World().regions.regions.Size(), 0u);
    EXPECT_GT(g.s.World().sites.Count(), 0);
    EXPECT_TRUE(g.s.Status()["regionsFresh"].asBool());
}

TEST(WorldGenSessionRegions, SizeChangeAlsoClears)
{
    Gen g;
    Json::Value a = Obj(); a["size"] = "Standard";
    g.s.SetSize(a);
    ASSERT_TRUE(g.s.Tick(g.rng));
    EXPECT_EQ(g.s.World().sites.Count(), 0);
    EXPECT_FALSE(g.s.Status()["regionsFresh"].asBool());
}

TEST(WorldGenSessionRegions, SameSeedGivesIdenticalRegionsAndSites)
{
    Gen a;
    Gen b;
    const WorldStructure& wa = a.s.World();
    const WorldStructure& wb = b.s.World();
    ASSERT_EQ(wa.CellCount(), wb.CellCount());
    for (int i = 0; i < wa.CellCount(); ++i) { ASSERT_EQ(wa.Cells()[i].region, wb.Cells()[i].region) << i; }
    EXPECT_TRUE(wa.regions == wb.regions);
    EXPECT_TRUE(wa.sites == wb.sites);
    EXPECT_EQ(Json::FastWriter().write(a.s.Regions()), Json::FastWriter().write(b.s.Regions()));
}

TEST(WorldGenSessionRegions, DifferentSeedGivesDifferentSites)
{
    Gen a;
    WorldGenSession s2;
    Dia::Maths::RandomStream rng(1);
    Json::Value r = Obj(); r["seed"] = 777;
    s2.Regenerate(r);
    ASSERT_TRUE(s2.Tick(rng));
    EXPECT_FALSE(a.s.World().sites == s2.World().sites);
}

TEST(WorldGenSessionRegions, RegionsQueryReturnsEveryRecord)
{
    Gen g;
    const WorldStructure& w = g.s.World();
    const Json::Value r = g.s.Regions();
    ASSERT_TRUE(r["success"].asBool()) << r.toStyledString();
    EXPECT_TRUE(r["fresh"].asBool());
    EXPECT_EQ(r["limitedBy"].asString(), DepthZoneName(w.regions.limitedBy));
    EXPECT_EQ(r["absorbedCells"].asUInt(), static_cast<unsigned>(w.regions.absorbedCells));
    EXPECT_EQ(r["mainlandRegionCount"].asUInt(), static_cast<unsigned>(w.regions.mainlandRegionCount));
    ASSERT_EQ(r["regions"].size(), w.regions.regions.Size());
    for (unsigned i = 0; i < w.regions.regions.Size(); ++i)
    {
        const RegionRecord& rec = w.regions.regions[i];
        const Json::Value& j = r["regions"][i];
        EXPECT_EQ(j["id"].asUInt(), static_cast<unsigned>(rec.id));
        EXPECT_EQ(j["zone"].asString(), DepthZoneName(rec.zone));
        EXPECT_EQ(j["landmass"].asUInt(), static_cast<unsigned>(rec.landmass));
        EXPECT_EQ(j["isIsland"].asBool(), rec.isIsland);
        EXPECT_TRUE(j["primaryBiome"].isString());
        EXPECT_TRUE(j["purity"].isNumeric());
        EXPECT_EQ(j["cellCount"].asUInt(), static_cast<unsigned>(rec.cellCount));
        EXPECT_EQ(j["centreCell"].asUInt(), rec.centreCell);
        ASSERT_EQ(j["neighbours"].size(), rec.neighbours.Size());
        for (unsigned n = 0; n < rec.neighbours.Size(); ++n) { EXPECT_EQ(j["neighbours"][n].asUInt(), static_cast<unsigned>(rec.neighbours[n])); }
    }
    EXPECT_LT(ReplySize(r), 16u * 1024u);
}

TEST(WorldGenSessionRegions, SitesQueryListsAllWithKinds)
{
    Gen g;
    const WorldStructure& w = g.s.World();
    const Json::Value r = g.s.Sites(Obj());
    ASSERT_TRUE(r["success"].asBool()) << r.toStyledString();
    EXPECT_EQ(r["total"].asInt(), w.sites.Count());
    EXPECT_LT(ReplySize(r), 16u * 1024u);
    ASSERT_GT(r["sites"].size(), 0u);
    const Json::Value& s0 = r["sites"][0u];
    for (const char* k : { "kind", "cell", "x", "y", "region", "variant", "flags" }) { EXPECT_TRUE(s0.isMember(k)) << k; }
    EXPECT_EQ(s0["x"].asInt(), s0["cell"].asInt() % w.dimensions.width);
    EXPECT_EQ(s0["y"].asInt(), s0["cell"].asInt() / w.dimensions.width);
    EXPECT_EQ(s0["region"].asUInt(), static_cast<unsigned>(w.Cells()[s0["cell"].asUInt()].region));

    std::set<std::string> kinds;
    for (const Json::Value& k : r["kinds"])
    {
        kinds.insert(k["kind"].asString());
        for (const char* f : { "displayName", "debugColour", "debugShape", "variantLabel" }) { EXPECT_TRUE(k.isMember(f)) << f; }
    }
    for (const char* id : { "CoW.Node.Corridor", "CoW.Node.River", "CoW.Node.Hunting", "CoW.Node.Resource" }) { EXPECT_TRUE(kinds.count(id)) << id; }
    for (const Json::Value& s : r["sites"]) { EXPECT_TRUE(kinds.count(s["kind"].asString())) << s["kind"].asString(); }
}

TEST(WorldGenSessionRegions, SitesQueryFiltersByKind)
{
    Gen g;
    Json::Value a = Obj(); a["kind"] = "CoW.Node.Corridor";
    const Json::Value r = g.s.Sites(a);
    ASSERT_TRUE(r["success"].asBool()) << r.toStyledString();
    EXPECT_EQ(r["total"].asInt(), g.s.World().sites.Count(BuiltinNodeKinds::Corridor()));
    EXPECT_GT(r["total"].asInt(), 0);
    for (const Json::Value& s : r["sites"]) { EXPECT_EQ(s["kind"].asString(), "CoW.Node.Corridor"); }
}

TEST(WorldGenSessionRegions, SitesQueryPagesAndCapsTheReply)
{
    Gen g;
    const int total = g.s.World().sites.Count();
    ASSERT_GT(total, 100) << "default world must exceed one page";
    const Json::Value p0 = g.s.Sites(Obj());
    EXPECT_EQ(p0["total"].asInt(), total);
    EXPECT_LT(p0["sites"].size(), static_cast<unsigned>(total));
    EXPECT_LT(ReplySize(p0), 16u * 1024u);

    size_t gathered = 0;
    std::set<int> cells;
    unsigned offset = 0;
    for (int page = 0; page < 50 && offset < static_cast<unsigned>(total); ++page)
    {
        Json::Value a = Obj(); a["offset"] = offset;
        const Json::Value r = g.s.Sites(a);
        ASSERT_TRUE(r["success"].asBool());
        ASSERT_GT(r["sites"].size(), 0u);
        EXPECT_LT(ReplySize(r), 16u * 1024u);
        gathered += r["sites"].size();
        offset += r["sites"].size();
    }
    EXPECT_EQ(gathered, static_cast<size_t>(total));

    Json::Value small = Obj(); small["limit"] = 5; small["offset"] = 2;
    EXPECT_EQ(g.s.Sites(small)["sites"].size(), 5u);

    // A huge limit is clamped to the reply cap.
    Json::Value big = Obj(); big["limit"] = 100000;
    const Json::Value rb = g.s.Sites(big);
    EXPECT_LT(ReplySize(rb), 16u * 1024u);
    EXPECT_LT(rb["sites"].size(), static_cast<unsigned>(total));

    // Offset past the end: empty page, not an error.
    Json::Value past = Obj(); past["offset"] = total + 10;
    const Json::Value rp = g.s.Sites(past);
    EXPECT_TRUE(rp["success"].asBool());
    EXPECT_EQ(rp["sites"].size(), 0u);
    EXPECT_EQ(rp["total"].asInt(), total);
}

TEST(WorldGenSessionRegions, SitesQueryRejectsBadArguments)
{
    Gen g;
    Json::Value unk = Obj(); unk["kind"] = "No.Such.Kind";
    EXPECT_TRUE(IsError(g.s.Sites(unk)));
    Json::Value notStr = Obj(); notStr["kind"] = 5;
    EXPECT_TRUE(IsError(g.s.Sites(notStr)));
    Json::Value neg = Obj(); neg["offset"] = -1;
    EXPECT_TRUE(IsError(g.s.Sites(neg)));
    Json::Value lim = Obj(); lim["limit"] = "x";
    EXPECT_TRUE(IsError(g.s.Sites(lim)));
}

TEST(WorldGenSessionRegions, ParamsEffectiveReportsLimits)
{
    WorldGenSession fresh;
    const Json::Value before = fresh.Params()["effective"];
    EXPECT_TRUE(before["corridorLimited"].isArray());
    EXPECT_EQ(before["corridorLimited"].size(), 2u);
    EXPECT_FALSE(before["nodeLimited"].asBool());
    EXPECT_EQ(before["regionLimitedBy"].asString(), "None");

    Gen g;
    const Json::Value eff = g.s.Params()["effective"];
    ASSERT_TRUE(eff["corridorLimited"].isArray());
    ASSERT_EQ(eff["corridorLimited"].size(), 2u);
    EXPECT_TRUE(eff["corridorLimited"][0u].isBool());
    EXPECT_TRUE(eff["corridorLimited"][1u].isBool());
    EXPECT_TRUE(eff["nodeLimited"].isBool());
    EXPECT_EQ(eff["regionLimitedBy"].asString(), DepthZoneName(g.s.World().regions.limitedBy));
}

TEST(WorldGenSessionRegions, CellQueryReportsRegionZoneAndSites)
{
    Gen g;
    const WorldStructure& w = g.s.World();
    const Site& site = w.sites.Of(BuiltinNodeKinds::Corridor()).front();

    Json::Value a = Obj(); a["index"] = site.cell;
    const Json::Value r = g.s.Cell(a);
    ASSERT_TRUE(r["success"].asBool()) << r.toStyledString();
    EXPECT_EQ(r["region"].asUInt(), static_cast<unsigned>(w.Cells()[site.cell].region));
    EXPECT_EQ(r["zone"].asString(), DepthZoneName(w.Cells()[site.cell].depthZone));
    ASSERT_TRUE(r["sites"].isArray());
    ASSERT_GE(r["sites"].size(), 1u);
    bool found = false;
    for (const Json::Value& s : r["sites"])
    {
        if (s["kind"].asString() == "CoW.Node.Corridor") { found = true; EXPECT_EQ(s["variant"].asUInt(), static_cast<unsigned>(site.variant)); }
    }
    EXPECT_TRUE(found);

    // A cell with no site reports an empty array; an ocean cell has region 0.
    for (int i = 0; i < w.CellCount(); ++i)
    {
        if (!w.sites.Has(static_cast<uint32_t>(i)))
        {
            Json::Value b = Obj(); b["index"] = i;
            EXPECT_EQ(g.s.Cell(b)["sites"].size(), 0u);
            break;
        }
    }
    for (int i = 0; i < w.CellCount(); ++i)
    {
        if (w.Cells()[i].terrain == TerrainType::DeepWater)
        {
            Json::Value b = Obj(); b["index"] = i;
            EXPECT_EQ(g.s.Cell(b)["region"].asUInt(), 0u);
            break;
        }
    }
}

TEST(WorldGenSessionRegions, RegisteredPlacerSitesAppearWithItsKindInfo)
{
    SiteRegistry reg;
    TestPlacer placer("Test.Site", {}, 5);
    ASSERT_TRUE(reg.Register(&placer));
    Gen g(&reg);
    EXPECT_EQ(placer.placeCalls, 1);
    EXPECT_EQ(g.s.World().sites.Count(StringCRC("Test.Site")), 1);

    Json::Value a = Obj(); a["kind"] = "Test.Site";
    const Json::Value r = g.s.Sites(a);
    ASSERT_TRUE(r["success"].asBool()) << r.toStyledString();
    EXPECT_EQ(r["total"].asInt(), 1);
    ASSERT_EQ(r["sites"].size(), 1u);
    EXPECT_EQ(r["sites"][0u]["cell"].asInt(), 5);
    EXPECT_EQ(r["sites"][0u]["kind"].asString(), "Test.Site");

    bool listed = false;
    const Json::Value allSites = g.s.Sites(Obj());
    for (const Json::Value& k : allSites["kinds"])
    {
        if (k["kind"].asString() == "Test.Site")
        {
            listed = true;
            EXPECT_EQ(k["displayName"].asString(), "Test.Site");
            EXPECT_EQ(k["debugColour"].asUInt(), 0xFF00FFu);
            EXPECT_EQ(k["debugShape"].asString(), "Dot");
        }
    }
    EXPECT_TRUE(listed);

    Json::Value b = Obj(); b["index"] = 5;
    bool atCell = false;
    const Json::Value cell5 = g.s.Cell(b);
    for (const Json::Value& s : cell5["sites"]) { atCell = atCell || s["kind"].asString() == "Test.Site"; }
    EXPECT_TRUE(atCell);

    // Failing placer: the pass result is reported by status and the layout is not ok.
    SiteRegistry bad;
    TestPlacer failing("Test.Fail", {}, 0, PlaceResult::Code::Retryable);
    ASSERT_TRUE(bad.Register(&failing));
    Gen h(&bad);
    const Json::Value st = h.s.Status();
    EXPECT_FALSE(st["layout"]["ok"].asBool());
    EXPECT_NE(st["layout"]["placeResult"].asString().find("Test.Fail"), std::string::npos) << st.toStyledString();
}

TEST(WorldGenSessionRegions, NullRegistryAndEmptyRegistryAgree)
{
    SiteRegistry empty;
    Gen a;
    Gen b(&empty);
    EXPECT_TRUE(a.s.World().sites == b.s.World().sites);
    Gen c;
    c.s.SetSiteRegistry(nullptr);
    c.s.Regenerate(Obj());
    ASSERT_TRUE(c.s.Tick(c.rng));
    EXPECT_TRUE(a.s.World().sites == c.s.World().sites);
}
