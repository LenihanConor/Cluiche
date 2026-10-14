#include <gtest/gtest.h>
#include <DiaPathfinding/HexPathGrid.h>
#include <DiaPathfinding/HexTopology.h>
#include <vector>

using namespace Dia::Pathfinding;
using Dia::Core::Containers::DynamicArrayC;

namespace {
    // Expected neighbours of (x,y) per the odd-r table, direction order E,NE,NW,W,SW,SE.
    std::vector<CellCoord> ExpectedTable(int x, int y)
    {
        const bool odd = (y & 1) != 0;
        static const int evenDx[6] = { +1,  0, -1, -1, -1,  0 };
        static const int oddDx[6]  = { +1, +1,  0, -1,  0, +1 };
        static const int dy[6]     = {  0, -1, -1,  0, +1, +1 };
        std::vector<CellCoord> r;
        for (int d = 0; d < 6; ++d)
        {
            r.push_back(CellCoord{ x + (odd ? oddDx[d] : evenDx[d]), y + dy[d] });
        }
        return r;
    }

    std::vector<CellCoord> Collect(const HexPathGrid& g, CellCoord c)
    {
        DynamicArrayC<CellCoord, 16> out;
        g.GetNeighbours(c, out);
        std::vector<CellCoord> r;
        for (unsigned int i = 0; i < out.Size(); ++i) r.push_back(out[i]);
        return r;
    }
}

TEST(HexPathGridRect, AxialLayoutUnchanged)
{
    HexPathGrid g(5);
    EXPECT_EQ(g.GetLayout(), HexLayout::kAxialHexagon);
    EXPECT_EQ(g.GetRadius(), 5);
}

TEST(HexPathGridRect, ConstructionAndBounds)
{
    const int sizes[][2] = { {66, 42}, {80, 52}, {104, 64}, {128, 128} };
    for (const auto& s : sizes)
    {
        const int w = s[0], h = s[1];
        HexPathGrid g(w, h);
        EXPECT_EQ(g.GetLayout(), HexLayout::kOffsetRectangle);
        EXPECT_EQ(g.GetWidth(), w);
        EXPECT_EQ(g.GetHeight(), h);
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x)
                ASSERT_TRUE(g.IsPassable({ x, y }));
        for (int i = 0; i < w; ++i) { EXPECT_FALSE(g.IsPassable({ i, -1 })); EXPECT_FALSE(g.IsPassable({ i, h })); }
        for (int i = 0; i < h; ++i) { EXPECT_FALSE(g.IsPassable({ -1, i })); EXPECT_FALSE(g.IsPassable({ w, i })); }

        g.SetPassable({ -1, 0 }, false);
        g.SetPassable({ w, 0 }, false);
        g.SetPassable({ 0, -1 }, false);
        g.SetPassable({ 0, h }, false);
        EXPECT_TRUE(g.IsPassable({ 0, 0 }));
        EXPECT_TRUE(g.IsPassable({ w - 1, h - 1 }));
    }
}

TEST(HexPathGridRect, SetPassableInBounds)
{
    HexPathGrid g(6, 5);
    g.SetPassable({ 2, 3 }, false);
    EXPECT_FALSE(g.IsPassable({ 2, 3 }));
    EXPECT_TRUE(g.IsPassable({ 3, 3 }));
    g.SetPassable({ 2, 3 }, true);
    EXPECT_TRUE(g.IsPassable({ 2, 3 }));
}

TEST(HexPathGridRect, NeighbourTableEvenAndOddRows)
{
    for (int h : { 6, 7 })
    {
        HexPathGrid g(7, h);
        for (int y : { 2, 3 })
        {
            auto got = Collect(g, { 3, y });
            auto exp = ExpectedTable(3, y);
            ASSERT_EQ(got.size(), 6u);
            for (int d = 0; d < 6; ++d)
            {
                EXPECT_EQ(got[d].x, exp[d].x) << "h=" << h << " y=" << y << " d=" << d;
                EXPECT_EQ(got[d].y, exp[d].y) << "h=" << h << " y=" << y << " d=" << d;
            }
        }
    }
}

TEST(HexPathGridRect, NeighboursMatchTopologyAllCells)
{
    const int w = 12, h = 9;
    HexPathGrid g(w, h);
    HexTopology topo(w, h);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
        {
            std::vector<CellCoord> exp;
            const uint32_t idx = topo.ToIndex({ x, y });
            for (uint32_t d = 0; d < kHexDirections; ++d)
            {
                const uint32_t n = topo.NeighbourAt(idx, d);
                if (n != kNoCell) exp.push_back(topo.ToCoord(n));
            }
            auto got = Collect(g, { x, y });
            ASSERT_EQ(got.size(), exp.size()) << x << "," << y;
            for (size_t i = 0; i < got.size(); ++i)
            {
                EXPECT_EQ(got[i].x, exp[i].x);
                EXPECT_EQ(got[i].y, exp[i].y);
            }
        }
}

TEST(HexPathGridRect, ImpassableNeighboursOmittedOrderKept)
{
    const int w = 12, h = 9;
    HexPathGrid g(w, h);
    HexTopology topo(w, h);
    const CellCoord blocked[] = { {5, 4}, {6, 3}, {0, 0}, {11, 8}, {4, 5} };
    for (auto b : blocked) g.SetPassable(b, false);

    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
        {
            std::vector<CellCoord> exp;
            const uint32_t idx = topo.ToIndex({ x, y });
            for (uint32_t d = 0; d < kHexDirections; ++d)
            {
                const uint32_t n = topo.NeighbourAt(idx, d);
                if (n == kNoCell) continue;
                CellCoord c = topo.ToCoord(n);
                if (g.IsPassable(c)) exp.push_back(c);
            }
            auto got = Collect(g, { x, y });
            ASSERT_EQ(got.size(), exp.size()) << x << "," << y;
            for (size_t i = 0; i < got.size(); ++i)
            {
                EXPECT_EQ(got[i].x, exp[i].x);
                EXPECT_EQ(got[i].y, exp[i].y);
            }
        }

    // The queried cell's own passability does not change its neighbour list.
    auto withBlocked = Collect(g, { 5, 4 });
    g.SetPassable({ 5, 4 }, true);
    auto withOpen = Collect(g, { 5, 4 });
    ASSERT_EQ(withBlocked.size(), withOpen.size());
    for (size_t i = 0; i < withOpen.size(); ++i)
    {
        EXPECT_EQ(withBlocked[i].x, withOpen[i].x);
        EXPECT_EQ(withBlocked[i].y, withOpen[i].y);
    }
}
