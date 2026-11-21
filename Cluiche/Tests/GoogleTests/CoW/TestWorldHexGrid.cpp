// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/layered-debug-rendering.md (Feature 7)
#include <gtest/gtest.h>
#include <Shared/WorldGeneration/WorldHexGrid.h>

#include <DiaPathfinding/HexTopology.h>

#include <cmath>

using namespace CoW::WorldGeneration;
using Dia::Pathfinding::HexTopology;

namespace
{
const GridDimensions kSizes[] = { { 66, 42 }, { 80, 52 }, { 104, 64 } };
}

TEST(WorldHexGrid, CellFromPointOfCentreRoundTrips)
{
    for (const GridDimensions& d : kSizes)
    {
        WorldHexGrid g(d);
        ASSERT_EQ(g.CellCount(), static_cast<uint32_t>(d.width * d.height));
        for (uint32_t c = 0; c < g.CellCount(); ++c)
        {
            ASSERT_EQ(g.CellFromPoint(g.CellCenter(c)), static_cast<int32_t>(c)) << d.width << "x" << d.height << " cell " << c;
        }
    }
}

TEST(WorldHexGrid, NeighbourTableMatchesHexTopology)
{
    for (const GridDimensions& d : kSizes)
    {
        WorldHexGrid g(d);
        const HexTopology topo(d.width, d.height);
        for (uint32_t c = 0; c < g.CellCount(); ++c)
        {
            for (uint32_t dir = 0; dir < 6; ++dir)
            {
                const uint32_t n = topo.NeighbourAt(c, dir);
                const int32_t expected = n == Dia::Pathfinding::kNoCell ? -1 : static_cast<int32_t>(n);
                ASSERT_EQ(g.Neighbour(c, dir), expected) << "cell " << c << " dir " << dir;
                ASSERT_EQ(g.Desc().neighbours[6u * c + dir], expected);
            }
        }
    }
}

TEST(WorldHexGrid, EdgeCellsHaveMinusOneExactlyOffGrid)
{
    for (const GridDimensions& d : kSizes)
    {
        WorldHexGrid g(d);
        const HexTopology topo(d.width, d.height);
        for (uint32_t c = 0; c < g.CellCount(); ++c)
        {
            const auto coord = topo.ToCoord(c);
            for (uint32_t dir = 0; dir < 6; ++dir)
            {
                const Dia::Maths::Vector2D step = HexTopology::DirectionVector(dir);
                // Neighbour centre = this centre + sqrt(3)*size * direction vector.
                const Dia::Maths::Vector2D p = g.CellCenter(c) + step * (std::sqrt(3.0f) * g.CellSize());
                const auto nc = HexTopology::CellFromPoint(p, g.CellSize());
                const bool onGrid = nc.x >= 0 && nc.y >= 0 && nc.x < d.width && nc.y < d.height;
                ASSERT_EQ(g.Neighbour(c, dir) >= 0, onGrid) << "cell " << c << " (" << coord.x << "," << coord.y << ") dir " << dir;
                if (onGrid)
                    ASSERT_EQ(g.Neighbour(c, dir), nc.y * d.width + nc.x);
            }
        }
    }
}

TEST(WorldHexGrid, BoundsContainEveryHexCorner)
{
    for (const GridDimensions& d : kSizes)
    {
        WorldHexGrid g(d);
        const float size = g.CellSize();
        const float pi = 3.14159265358979f;
        for (uint32_t c = 0; c < g.CellCount(); ++c)
        {
            const Dia::Maths::Vector2D ctr = g.CellCenter(c);
            for (int k = 0; k < 6; ++k)
            {
                const float a = pi / 3.0f * static_cast<float>(k);   // flat-top corners at 0, 60, ... degrees
                const float x = ctr.x + size * std::cos(a);
                const float y = ctr.y + size * std::sin(a);
                const float eps = 1e-3f;
                ASSERT_GE(x, g.BoundsMin().x - eps);
                ASSERT_LE(x, g.BoundsMax().x + eps);
                ASSERT_GE(y, g.BoundsMin().y - eps);
                ASSERT_LE(y, g.BoundsMax().y + eps);
            }
        }
    }
}

TEST(WorldHexGrid, DescDescribesStorage)
{
    WorldHexGrid g(GridDimensions{ 66, 42 });
    EXPECT_EQ(g.Desc().topology, Dia::Core::StringCRC("hex.flat.oddq"));
    EXPECT_EQ(g.Desc().width, 66);
    EXPECT_EQ(g.Desc().height, 42);
    EXPECT_FLOAT_EQ(g.Desc().cellSize, WorldHexGrid::kCellSize);
    EXPECT_EQ(g.Desc().centresXY, g.Centres().data());
    EXPECT_EQ(g.Desc().neighbours, g.Neighbours().data());
}

TEST(WorldHexGrid, PointOutsideGridIsMinusOne)
{
    WorldHexGrid g(GridDimensions{ 66, 42 });
    EXPECT_EQ(g.CellFromPoint(Dia::Maths::Vector2D(-500.0f, -500.0f)), -1);
    EXPECT_EQ(g.CellFromPoint(Dia::Maths::Vector2D(1e6f, 1e6f)), -1);
}

TEST(WorldHexGrid, BuildingTwiceIsIdentical)
{
    for (const GridDimensions& d : kSizes)
    {
        WorldHexGrid a(d);
        WorldHexGrid b(d);
        EXPECT_EQ(a.Centres(), b.Centres());
        EXPECT_EQ(a.Neighbours(), b.Neighbours());
        EXPECT_EQ(a.BoundsMin().x, b.BoundsMin().x);
        EXPECT_EQ(a.BoundsMin().y, b.BoundsMin().y);
        EXPECT_EQ(a.BoundsMax().x, b.BoundsMax().x);
        EXPECT_EQ(a.BoundsMax().y, b.BoundsMax().y);
    }
}
