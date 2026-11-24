// S-1 Regions, Nodes and Site Hook task 1: RegionTable, SiteSet, PlaceResult, DeriveSubSeed.
#include <gtest/gtest.h>
#include <DraoiHexWorldTerrain/Testing/TerrainTestHost.h>
#include <DraoiHexWorldTerrain/TerrainTypes.h>
#include <DraoiHexWorld/Regions.h>
#include <DraoiHexWorld/Sites.h>
#include <DraoiHexWorldTerrain/PlaceResult.h>
#include <DraoiHexWorldTerrain/DeriveSubSeed.h>

#include <cstdio>
#include <cstring>
#include <set>
#include <string>
#include <type_traits>
#include <DraoiHexWorld/Testing/StratumTestIndices.h>

using namespace Draoi::HexWorld;
using namespace Draoi::HexWorld::Testing;
using Dia::Core::StringCRC;

static_assert(std::is_trivially_copyable_v<Site>, "Site must be trivially copyable");
static_assert(std::is_trivially_copyable_v<SiteKindInfo>, "SiteKindInfo must be trivially copyable");
static_assert(std::is_trivially_copyable_v<PlaceResult>, "PlaceResult must be trivially copyable");
static_assert(std::is_trivially_copyable_v<Cell>, "Cell must be trivially copyable");
static_assert(std::is_trivially_copyable_v<RegionRecord>, "RegionRecord must be trivially copyable");
static_assert(sizeof(Cell) == 16, "Cell must not grow: region fills the old tail padding byte");

namespace
{
Site MakeSite(StringCRC kind, uint32_t cell, RegionId region = 1)
{
    Site s{};
    s.kindCrc = SiteKindId(kind);
    s.cell = cell;
    s.region = region;
    return s;
}

RegionRecord MakeRegion(RegionId id, std::initializer_list<RegionId> neighbours)
{
    RegionRecord r;
    r.id = id;
    for (RegionId n : neighbours) { r.neighbours.Add(n); }
    return r;
}
}

// ---- Cell / WorldStructure -------------------------------------------------------------------
TEST(RegionTypes, CellRegionDefaultsToNone)
{
    const Cell c;
    EXPECT_EQ(c.region, kNoRegion);
    std::printf("sizeof(Cell) = %zu\n", sizeof(Cell));
}

TEST(RegionTypes, ResizeClearsRegionsAndSites)
{
    WorldStructure w;
    w.Resize({4, 4});
    w.regions.regions.Add(MakeRegion(1, {}));
    w.regions.absorbedCells = 3;
    w.regions.mainlandRegionCount = 1;
    w.regions.limitedBy = kStratumWild;
    w.sites.Add(MakeSite(TestNodeKinds().corridor, 2));
    w.Cells()[0].region = 1;

    w.Resize({4, 4});
    EXPECT_EQ(w.regions.regions.Size(), 0u);
    EXPECT_EQ(w.regions.absorbedCells, 0);
    EXPECT_EQ(w.regions.mainlandRegionCount, 0);
    EXPECT_EQ(w.regions.limitedBy, kNoStratum);
    EXPECT_EQ(w.sites.Count(), 0);
    EXPECT_EQ(w.Cells()[0].region, kNoRegion);
}

// ---- RegionTable -----------------------------------------------------------------------------
TEST(RegionTypes, FindZeroValidAndOutOfRange)
{
    RegionTable t;
    t.regions.Add(MakeRegion(1, {2}));
    t.regions.Add(MakeRegion(2, {1}));
    EXPECT_EQ(t.Find(0), nullptr);
    ASSERT_NE(t.Find(1), nullptr);
    EXPECT_EQ(t.Find(1)->id, 1);
    ASSERT_NE(t.Find(2), nullptr);
    EXPECT_EQ(t.Find(2)->id, 2);
    EXPECT_EQ(t.Find(3), nullptr);
    EXPECT_EQ(t.Find(255), nullptr);
    EXPECT_EQ(RegionTable{}.Find(1), nullptr);
}

TEST(RegionTypes, AreAdjacentSymmetricSelfAndUnknown)
{
    RegionTable t;
    t.regions.Add(MakeRegion(1, {2}));
    t.regions.Add(MakeRegion(2, {1, 3}));
    t.regions.Add(MakeRegion(3, {2}));
    EXPECT_TRUE(t.AreAdjacent(1, 2));
    EXPECT_TRUE(t.AreAdjacent(2, 1));
    EXPECT_TRUE(t.AreAdjacent(2, 3));
    EXPECT_FALSE(t.AreAdjacent(1, 3));
    EXPECT_FALSE(t.AreAdjacent(3, 1));
    EXPECT_FALSE(t.AreAdjacent(1, 1));
    EXPECT_FALSE(t.AreAdjacent(0, 1));
    EXPECT_FALSE(t.AreAdjacent(1, 0));
    EXPECT_FALSE(t.AreAdjacent(1, 9));
    EXPECT_FALSE(t.AreAdjacent(9, 1));
    EXPECT_FALSE(RegionTable{}.AreAdjacent(1, 2));
}

TEST(RegionTypes, TableEqualityIsMemberWise)
{
    RegionTable a, b;
    EXPECT_TRUE(a == b);
    a.regions.Add(MakeRegion(1, {2}));
    EXPECT_FALSE(a == b);
    b.regions.Add(MakeRegion(1, {2}));
    EXPECT_TRUE(a == b);
    b.absorbedCells = 1;
    EXPECT_FALSE(a == b);
    b.absorbedCells = 0;
    b.fragmentCells = 1;
    EXPECT_FALSE(a == b);
    b.fragmentCells = 0;
    b.regions[0].neighbours.Add(5);
    EXPECT_FALSE(a == b);
    b.regions[0].neighbours.RemoveAll();
    b.regions[0].neighbours.Add(2);
    EXPECT_TRUE(a == b);
    b.limitedBy = kStratumFirst;
    EXPECT_FALSE(a == b);
}

TEST(RegionTypes, AddRegionRefusesWhenFull)
{
    RegionTable t;
    for (unsigned i = 0; i < RegionTable::kMaxRegions; ++i)
    {
        EXPECT_TRUE(t.AddRegion(MakeRegion(static_cast<RegionId>(i + 1), {})));
    }
    EXPECT_FALSE(t.AddRegion(MakeRegion(200, {})));
    EXPECT_EQ(t.regions.Size(), RegionTable::kMaxRegions);
}

// ---- SiteSet ---------------------------------------------------------------------------------
TEST(SiteSet, EmptySetBehaviour)
{
    SiteSet s;
    EXPECT_EQ(s.Count(), 0);
    EXPECT_EQ(s.Count(TestNodeKinds().river), 0);
    EXPECT_TRUE(s.All().empty());
    EXPECT_TRUE(s.Of(TestNodeKinds().river).empty());
    EXPECT_FALSE(s.Has(0));
    EXPECT_EQ(s.At(0).Size(), 0u);
    EXPECT_EQ(s.Find(TestNodeKinds().river, 0), nullptr);
}

TEST(SiteSet, SortedByKindThenCellAfterOutOfOrderAdds)
{
    SiteSet s;
    const StringCRC kinds[3] = { TestNodeKinds().corridor, TestNodeKinds().river, TestNodeKinds().hunting };
    for (uint32_t cell : { 9u, 2u, 7u, 4u })
    {
        for (const StringCRC& k : kinds) { EXPECT_TRUE(s.Add(MakeSite(k, cell))); }
    }
    ASSERT_EQ(s.Count(), 12);
    const auto all = s.All();
    for (size_t i = 1; i < all.size(); ++i)
    {
        const bool ordered = all[i - 1].kindCrc < all[i].kindCrc ||
                             (all[i - 1].kindCrc == all[i].kindCrc && all[i - 1].cell < all[i].cell);
        EXPECT_TRUE(ordered) << "index " << i;
    }
    const auto of = s.Of(TestNodeKinds().river);
    ASSERT_EQ(of.size(), 4u);
    EXPECT_EQ(of[0].cell, 2u);
    EXPECT_EQ(of[1].cell, 4u);
    EXPECT_EQ(of[2].cell, 7u);
    EXPECT_EQ(of[3].cell, 9u);
    EXPECT_EQ(s.Count(TestNodeKinds().river), 4);
    EXPECT_EQ(s.Count(TestNodeKinds().resource), 0);
    EXPECT_TRUE(s.Of(TestNodeKinds().resource).empty());
}

TEST(SiteSet, HasAtAndFind)
{
    SiteSet s;
    s.Add(MakeSite(TestNodeKinds().river, 5, 3));
    s.Add(MakeSite(TestNodeKinds().corridor, 5, 3));
    s.Add(MakeSite(TestNodeKinds().hunting, 6, 4));
    EXPECT_TRUE(s.Has(5));
    EXPECT_TRUE(s.Has(6));
    EXPECT_FALSE(s.Has(7));
    EXPECT_EQ(s.At(5).Size(), 2u);
    EXPECT_EQ(s.At(6).Size(), 1u);
    EXPECT_EQ(s.At(7).Size(), 0u);
    ASSERT_NE(s.Find(TestNodeKinds().hunting, 6), nullptr);
    EXPECT_EQ(s.Find(TestNodeKinds().hunting, 6)->region, 4);
    EXPECT_EQ(s.Find(TestNodeKinds().hunting, 5), nullptr);
}

TEST(SiteSet, DuplicateKindAndCellRejected)
{
    SiteSet s;
    EXPECT_TRUE(s.Add(MakeSite(TestNodeKinds().river, 5)));
    EXPECT_FALSE(s.Add(MakeSite(TestNodeKinds().river, 5, 9)));
    EXPECT_EQ(s.Count(), 1);
    EXPECT_EQ(s.Find(TestNodeKinds().river, 5)->region, 1);   // original kept
    EXPECT_TRUE(s.Add(MakeSite(TestNodeKinds().corridor, 5)));  // other kind on same cell is fine
    EXPECT_EQ(s.Count(), 2);
}

TEST(SiteSet, ClearAndEquality)
{
    SiteSet a, b;
    EXPECT_TRUE(a == b);
    a.Add(MakeSite(TestNodeKinds().river, 1));
    EXPECT_FALSE(a == b);
    b.Add(MakeSite(TestNodeKinds().river, 1));
    EXPECT_TRUE(a == b);
    a.Clear();
    EXPECT_EQ(a.Count(), 0);
    EXPECT_FALSE(a == b);
}

// ---- Built-in kinds --------------------------------------------------------------------------
TEST(SiteKinds, BuiltinInfoForInjectedKinds)
{
    const StringCRC kinds[4] = { TestNodeKinds().corridor, TestNodeKinds().river,
                                 TestNodeKinds().hunting, TestNodeKinds().resource };
    const char* names[4] = { "Corridor Node", "River Node", "Hunting Ground", "Resource Deposit" };
    std::set<uint32_t> crcs, colours;
    for (int i = 0; i < 4; ++i)
    {
        const SiteKindInfo info = BuiltinNodeKindInfo(TestNodeKinds(), kinds[i]);
        EXPECT_EQ(info.kindCrc, SiteKindId(kinds[i]));
        EXPECT_STREQ(info.id, kinds[i].AsChar());
        EXPECT_STREQ(info.displayName, names[i]);
        EXPECT_EQ(info.debugColour & 0xFF000000u, 0u);
        crcs.insert(info.kindCrc);
        colours.insert(info.debugColour);
    }
    EXPECT_EQ(crcs.size(), 4u);
    EXPECT_EQ(colours.size(), 4u);
}

TEST(SiteKinds, UnknownInjectedKindInfoIsEmptyAndFlagged)
{
    const SiteKindInfo info = BuiltinNodeKindInfo(TestNodeKinds(), StringCRC("Test.Unknown"));
    EXPECT_EQ(info.kindCrc, SiteKindId(StringCRC("Test.Unknown")));
    EXPECT_EQ(info.displayName[0], '\0');
}


// ---- PlaceResult -----------------------------------------------------------------------------
TEST(PlaceResult, FactoriesAndReason)
{
    const PlaceResult ok = PlaceResult::Ok();
    EXPECT_EQ(ok.code, PlaceResult::Code::Ok);
    EXPECT_TRUE(ok.IsOk());
    EXPECT_STREQ(ok.reason, "");

    const PlaceResult r = PlaceResult::Retryable("no room");
    EXPECT_EQ(r.code, PlaceResult::Code::Retryable);
    EXPECT_FALSE(r.IsOk());
    EXPECT_STREQ(r.reason, "no room");

    const PlaceResult t = PlaceResult::TerrainCause("no coast");
    EXPECT_EQ(t.code, PlaceResult::Code::TerrainCause);
    EXPECT_STREQ(t.reason, "no coast");

    EXPECT_STREQ(PlaceResult::Retryable(nullptr).reason, "");
}

TEST(PlaceResult, LongReasonIsTruncatedAndTerminated)
{
    const std::string longText(500, 'x');
    const PlaceResult r = PlaceResult::Retryable(longText.c_str());
    EXPECT_EQ(std::strlen(r.reason), sizeof(r.reason) - 1);
}

// ---- DeriveSubSeed ---------------------------------------------------------------------------
TEST(DeriveSubSeed, Deterministic)
{
    EXPECT_EQ(DeriveSubSeed(42ull, StringCRC("Test.Regions")), DeriveSubSeed(42ull, StringCRC("Test.Regions")));
    EXPECT_EQ(DeriveSubSeed(42ull, uint64_t(7)), DeriveSubSeed(42ull, uint64_t(7)));
}

TEST(DeriveSubSeed, StepIdsDiffer)
{
    const StringCRC steps[] = { StringCRC("Test.Regions"), StringCRC("Test.Node.Corridor"), StringCRC("Test.Node.River"),
                                StringCRC("Test.Node.Fill") };
    std::set<uint64_t> seen;
    for (const StringCRC& s : steps) { seen.insert(DeriveSubSeed(42ull, s)); }
    EXPECT_EQ(seen.size(), 4u);
}

TEST(DeriveSubSeed, ParentChangeChangesOutput)
{
    EXPECT_NE(DeriveSubSeed(1ull, StringCRC("Test.Regions")), DeriveSubSeed(2ull, StringCRC("Test.Regions")));
    EXPECT_NE(DeriveSubSeed(1ull, uint64_t(3)), DeriveSubSeed(2ull, uint64_t(3)));
}

TEST(DeriveSubSeed, KeyChangeChangesOutput)
{
    EXPECT_NE(DeriveSubSeed(9ull, uint64_t(0)), DeriveSubSeed(9ull, uint64_t(1)));
    EXPECT_NE(DeriveSubSeed(0ull, uint64_t(0)), 0ull);
}

TEST(DeriveSubSeed, ThousandConsecutiveAttemptsAreDistinct)
{
    std::set<uint64_t> seen;
    for (uint64_t attempt = 0; attempt < 1000; ++attempt) { seen.insert(DeriveSubSeed(123456789ull, attempt)); }
    EXPECT_EQ(seen.size(), 1000u);

    // Also spread across the 32-bit halves (folded seeds must not collapse).
    std::set<uint32_t> highs;
    for (uint64_t attempt = 0; attempt < 1000; ++attempt) { highs.insert(static_cast<uint32_t>(DeriveSubSeed(123456789ull, attempt) >> 32)); }
    EXPECT_GT(highs.size(), 990u);
}
