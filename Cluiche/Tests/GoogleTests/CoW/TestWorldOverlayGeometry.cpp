// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/layered-debug-rendering.md (Feature 7, AC7)
#include <gtest/gtest.h>
#include <Shared/WorldGeneration/WorldOverlayGeometry.h>
#include <Shared/WorldGeneration/WorldGenSession.h>

#include <DiaPathfinding/HexTopology.h>

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
    WorldGenSession s;
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
    WorldGenSession s;
    Generate(s, "Large");
    WorldHexGrid grid(s.World().dimensions);
    const size_t rivers = BuildRiverSegments(s.World(), grid).size();
    const size_t outline = BuildNeighbourOutline(grid, 50 * 104 + 51).neighbourOutlines.size() + 6;
    std::cout << "[ INFO ] Large river segments=" << rivers << " outline segments=" << outline << "\n";
    EXPECT_LT(rivers + outline + 6u, 8192u / 2u);
}
