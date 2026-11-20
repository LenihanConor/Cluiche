// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/layered-debug-rendering.md (Feature 7, AC7)
#include <gtest/gtest.h>
#include <Shared/WorldGeneration/WorldOverlayGeometry.h>
#include <Shared/WorldGeneration/WorldGenSession.h>
#include <Shared/WorldGeneration/Testing/TerrainRulesTestHelpers.h>

#include <DiaPathfinding/HexTopology.h>

#include <cmath>
#include <iostream>

using namespace CoW::WorldGeneration;
using Dia::Pathfinding::HexTopology;

namespace
{
void Generate(WorldGenSession& s, const char* size)
{
    Json::Value sz(Json::objectValue);
    sz["size"] = size;
    s.SetSize(sz);
    Json::Value seed(Json::objectValue);
    seed["seed"] = Json::Value(static_cast<Json::UInt64>(12345));
    Dia::Maths::RandomStream rng(12345);
    s.Regenerate(seed);
    ASSERT_TRUE(s.Tick(rng));
}
bool Same(Dia::Maths::Vector2D a, Dia::Maths::Vector2D b) { return std::abs(a.x - b.x) < 1e-3f && std::abs(a.y - b.y) < 1e-3f; }
}

TEST(WorldOverlayGeometry, RiverSegmentsEqualDownstreamLinesSmall)
{
    TestSession s;
    Generate(s, "Small");
    const WorldStructure& w = s.World();
    WorldHexGrid grid(w.dimensions);
    const HexTopology topo(w.dimensions.width, w.dimensions.height);

    std::vector<LineSegment> expected;
    for (int i = 0; i < w.CellCount(); ++i)
    {
        const Cell& c = w.Cells()[i];
        if (c.waterSource != WaterSource::River || c.downstream > 5) continue;
        const uint32_t n = topo.NeighbourAt(static_cast<uint32_t>(i), c.downstream);
        if (n == Dia::Pathfinding::kNoCell) continue;
        expected.push_back({ grid.CellCenter(static_cast<uint32_t>(i)), grid.CellCenter(n) });
    }
    const std::vector<LineSegment> got = BuildRiverSegments(w, grid);
    ASSERT_GT(expected.size(), 0u);
    ASSERT_EQ(got.size(), expected.size());
    for (size_t k = 0; k < got.size(); ++k)
    {
        EXPECT_TRUE(Same(got[k].a, expected[k].a)) << k;
        EXPECT_TRUE(Same(got[k].b, expected[k].b)) << k;
    }
}

TEST(WorldOverlayGeometry, RiverSegmentsEmptyOnDimensionMismatch)
{
    WorldStructure w;
    w.Resize({ 4, 4 });
    WorldHexGrid grid({ 5, 5 });
    EXPECT_TRUE(BuildRiverSegments(w, grid).empty());
}

TEST(WorldOverlayGeometry, RiverSegmentsSkipNoDirectionAndNonRiver)
{
    WorldStructure w;
    w.Resize({ 4, 4 });
    WorldHexGrid grid({ 4, 4 });
    w.Cells()[5].waterSource = WaterSource::River;   // downstream stays kNoDirection
    w.Cells()[6].downstream  = 0;                    // not a river
    EXPECT_TRUE(BuildRiverSegments(w, grid).empty());
}

TEST(WorldOverlayGeometry, CornerCellYieldsOnlyInBoundsNeighbours)
{
    WorldHexGrid grid({ 66, 42 });
    const NeighbourOutline o = BuildNeighbourOutline(grid, 0);
    int present = 0;
    for (int d = 0; d < 6; ++d)
    {
        EXPECT_EQ(o.hasLabel[d], grid.Neighbour(0, d) >= 0) << d;
        present += o.hasLabel[d] ? 1 : 0;
    }
    EXPECT_GT(present, 0);
    EXPECT_LT(present, 6);
    EXPECT_EQ(o.neighbourOutlines.size(), static_cast<size_t>(present) * 6u);
    EXPECT_EQ(o.selectedOutline.size(), 6u);
}

TEST(WorldOverlayGeometry, EvenAndOddColumnsMatchHexTopology)
{
    WorldHexGrid grid({ 66, 42 });
    const HexTopology topo(66, 42);
    const int cells[] = { 20 * 66 + 10, 20 * 66 + 11 };   // even column, odd column
    for (int cell : cells)
    {
        const NeighbourOutline o = BuildNeighbourOutline(grid, cell);
        EXPECT_EQ(o.cell, cell);
        for (uint32_t d = 0; d < 6; ++d)
        {
            const uint32_t n = topo.NeighbourAt(static_cast<uint32_t>(cell), d);
            ASSERT_NE(n, Dia::Pathfinding::kNoCell);
            EXPECT_TRUE(o.hasLabel[d]);
            EXPECT_EQ(o.neighbourCell[d], static_cast<int>(n));
            EXPECT_TRUE(Same(o.labelAnchor[d], grid.CellCenter(n))) << cell << " dir " << d;
        }
        EXPECT_EQ(o.neighbourOutlines.size(), 36u);
    }
}

TEST(WorldOverlayGeometry, InvalidCellIsEmpty)
{
    WorldHexGrid grid({ 66, 42 });
    EXPECT_EQ(BuildNeighbourOutline(grid, -1).cell, -1);
    EXPECT_TRUE(BuildNeighbourOutline(grid, -1).selectedOutline.empty());
    EXPECT_TRUE(BuildNeighbourOutline(grid, 66 * 42).selectedOutline.empty());
}

TEST(WorldOverlayGeometry, LargeWorldSegmentCountUnderDebugDrawBudget)
{
    TestSession s;
    Generate(s, "Large");
    WorldHexGrid grid(s.World().dimensions);
    const size_t rivers = BuildRiverSegments(s.World(), grid).size();
    const size_t outline = BuildNeighbourOutline(grid, 50 * 104 + 51).neighbourOutlines.size() + 6;
    std::cout << "[ INFO ] Large river segments=" << rivers << " outline segments=" << outline << "\n";
    EXPECT_LT(rivers + outline + 6u, 8192u / 2u);
}

// ---- Regions plan task 10: region edges and site markers ----

namespace
{
float Dist(Dia::Maths::Vector2D a, Dia::Maths::Vector2D b) { return std::hypot(a.x - b.x, a.y - b.y); }
Dia::Maths::Vector2D Mid(const LineSegment& s) { return Dia::Maths::Vector2D((s.a.x + s.b.x) * 0.5f, (s.a.y + s.b.y) * 0.5f); }

// Independent scan: every in-bounds neighbour pair (i < n) whose regions differ (0 = ocean / not fresh counts as a region).
std::vector<std::pair<uint32_t, uint32_t>> RegionEdgePairs(const WorldStructure& w, const WorldHexGrid& grid)
{
    std::vector<std::pair<uint32_t, uint32_t>> out;
    for (uint32_t i = 0; i < grid.CellCount(); ++i)
        for (uint32_t d = 0; d < 6; ++d)
        {
            const int32_t n = grid.Neighbour(i, d);
            if (n < 0 || static_cast<uint32_t>(n) <= i) continue;
            if (w.Cells()[i].region != w.Cells()[n].region) out.push_back({ i, static_cast<uint32_t>(n) });
        }
    return out;
}
}

TEST(WorldOverlayGeometry, RegionEdgesAreTheSharedHexEdgeOfEveryDifferingPair)
{
    WorldStructure w;
    w.Resize({ 6, 4 });
    for (int y = 0; y < 4; ++y)
        for (int x = 0; x < 6; ++x)
            w.Cells()[y * 6 + x].region = static_cast<RegionId>(x < 3 ? 1 : 2);
    w.Cells()[0].region = kNoRegion;       // an ocean cell next to region 1
    w.Cells()[3 * 6 + 5].region = kNoRegion;
    WorldHexGrid grid(w.dimensions);

    const auto pairs = RegionEdgePairs(w, grid);
    const std::vector<LineSegment> got = BuildRegionEdges(w, grid);
    ASSERT_GT(pairs.size(), 0u);
    ASSERT_EQ(got.size(), pairs.size()) << "one segment per differing pair, map border excluded";
    for (const auto& p : pairs)
    {
        const Dia::Maths::Vector2D ca = grid.CellCenter(p.first), cb = grid.CellCenter(p.second);
        const Dia::Maths::Vector2D m((ca.x + cb.x) * 0.5f, (ca.y + cb.y) * 0.5f);
        int matches = 0;
        for (const LineSegment& s : got)
        {
            if (Dist(Mid(s), m) < 1e-2f)
            {
                ++matches;
                EXPECT_NEAR(Dist(s.a, s.b), grid.CellSize(), 1e-2f) << "a hex side";
                EXPECT_NEAR(Dist(s.a, ca), grid.CellSize(), 1e-2f) << "endpoints are corners of both cells";
                EXPECT_NEAR(Dist(s.b, cb), grid.CellSize(), 1e-2f);
            }
        }
        EXPECT_EQ(matches, 1) << p.first << "-" << p.second;
    }
}

TEST(WorldOverlayGeometry, RegionEdgesEmptyWhenNoRegionsOrDimensionMismatch)
{
    WorldStructure w;
    w.Resize({ 5, 5 });
    WorldHexGrid grid({ 5, 5 });
    EXPECT_TRUE(BuildRegionEdges(w, grid).empty()) << "all region 0 (regions not fresh)";
    for (int i = 0; i < w.CellCount(); ++i) w.Cells()[i].region = 3;
    EXPECT_TRUE(BuildRegionEdges(w, grid).empty()) << "one region, map border not drawn";
    w.Cells()[2 * 5 + 2].region = 4;
    EXPECT_EQ(BuildRegionEdges(w, grid).size(), 6u) << "an interior cell of another region";
    WorldHexGrid other({ 6, 5 });
    EXPECT_TRUE(BuildRegionEdges(w, other).empty());
}

TEST(WorldOverlayGeometry, GeneratedRegionEdgesMatchTheIndependentScan)
{
    TestSession s;
    Generate(s, "Small");
    ASSERT_TRUE(s.RegionsFresh());
    WorldHexGrid grid(s.World().dimensions);
    const size_t got = BuildRegionEdges(s.World(), grid).size();
    EXPECT_EQ(got, RegionEdgePairs(s.World(), grid).size());
    EXPECT_GT(got, 0u);
}

TEST(WorldOverlayGeometry, EverySiteShapeStaysInsideItsRadiusAndShapesDiffer)
{
    const Dia::Maths::Vector2D c(100.0f, 50.0f);
    const float r = 8.0f;
    const SiteShape shapes[] = { SiteShape::Dot, SiteShape::Diamond, SiteShape::Square, SiteShape::Triangle, SiteShape::Star, SiteShape::Ring };
    std::vector<std::vector<LineSegment>> all;
    for (SiteShape sh : shapes)
    {
        std::vector<LineSegment> seg;
        AppendSiteMarker(seg, c, r, sh);
        ASSERT_GE(seg.size(), 3u) << static_cast<int>(sh);
        for (const LineSegment& s : seg)
        {
            EXPECT_LE(Dist(s.a, c), r + 1e-3f);
            EXPECT_LE(Dist(s.b, c), r + 1e-3f);
        }
        all.push_back(seg);
    }
    for (size_t i = 0; i < all.size(); ++i)
        for (size_t j = i + 1; j < all.size(); ++j)
        {
            bool same = all[i].size() == all[j].size();
            for (size_t k = 0; same && k < all[i].size(); ++k)
                same = Same(all[i][k].a, all[j][k].a) && Same(all[i][k].b, all[j][k].b);
            EXPECT_FALSE(same) << "shapes " << i << " and " << j << " draw identically";
        }
}

TEST(WorldOverlayGeometry, SiteMarkersOnlyForTheRequestedKindCentredOnTheirCells)
{
    WorldStructure w;
    w.Resize({ 8, 8 });
    const uint32_t kindA = SiteKindId(Dia::Core::StringCRC("Test.A"));
    const uint32_t kindB = SiteKindId(Dia::Core::StringCRC("Test.B"));
    Site s; s.kindCrc = kindA; s.cell = 9;  w.sites.Add(s);
    s.cell = 30;                            w.sites.Add(s);
    s.kindCrc = kindB; s.cell = 9;          w.sites.Add(s);
    WorldHexGrid grid(w.dimensions);

    std::vector<LineSegment> one;
    AppendSiteMarker(one, Dia::Maths::Vector2D(0, 0), 6.0f, SiteShape::Square);
    const std::vector<LineSegment> a = BuildSiteMarkers(w, grid, kindA, SiteShape::Square, 6.0f);
    ASSERT_EQ(a.size(), 2u * one.size());
    for (size_t k = 0; k < a.size(); ++k)
    {
        const uint32_t cell = k < one.size() ? 9u : 30u;
        EXPECT_LE(Dist(a[k].a, grid.CellCenter(cell)), 6.0f + 1e-3f) << k;
    }
    EXPECT_EQ(BuildSiteMarkers(w, grid, kindB, SiteShape::Square, 6.0f).size(), one.size());
    EXPECT_TRUE(BuildSiteMarkers(w, grid, 0x1234u, SiteShape::Square, 6.0f).empty());
    WorldHexGrid other({ 9, 8 });
    EXPECT_TRUE(BuildSiteMarkers(w, other, kindA, SiteShape::Square, 6.0f).empty());
}

TEST(WorldOverlayGeometry, LargeRegionEdgesAndSitesUnderDebugDrawBudget)
{
    TestSession s;
    Generate(s, "Large");
    WorldHexGrid grid(s.World().dimensions);
    size_t lines = BuildRiverSegments(s.World(), grid).size() + BuildRegionEdges(s.World(), grid).size();
    for (const SiteKindInfo& k : ListSiteKinds(s.Registry()))
        lines += BuildSiteMarkers(s.World(), grid, k.kindCrc, k.debugShape, grid.CellSize() * 0.45f).size();
    std::cout << "[ INFO ] Large rivers+region edges+site markers=" << lines << "\n";
    EXPECT_LT(lines, 8192u) << "IDebugDraw line budget";
}
