#include <gtest/gtest.h>
#include <DiaPathfinding/FindPath.h>
#include <DiaPathfinding/HexPathGrid.h>
#include <DiaPathfinding/HexTopology.h>
#include <cstdint>
#include <cstdlib>
#include <queue>
#include <map>
#include <vector>

using namespace Dia::Pathfinding;
using Dia::Core::Containers::DynamicArrayC;

namespace {

    struct Lcg {
        uint32_t s;
        explicit Lcg(uint32_t seed) : s(seed) {}
        uint32_t Next() { s = s * 1664525u + 1013904223u; return s >> 8; }
    };

    struct CoordLess {
        bool operator()(CellCoord a, CellCoord b) const { return a.x != b.x ? a.x < b.x : a.y < b.y; }
    };

    // Shortest edge count via BFS over the grid's own GetNeighbours; -1 if unreachable.
    int BfsDistance(const HexPathGrid& g, CellCoord from, CellCoord to)
    {
        std::map<CellCoord, int, CoordLess> dist;
        std::queue<CellCoord> q;
        dist[from] = 0;
        q.push(from);
        DynamicArrayC<CellCoord, 16> nb;
        while (!q.empty())
        {
            CellCoord c = q.front(); q.pop();
            if (c == to) return dist[c];
            g.GetNeighbours(c, nb);
            for (unsigned int i = 0; i < nb.Size(); ++i)
            {
                if (dist.find(nb[i]) == dist.end())
                {
                    dist[nb[i]] = dist[c] + 1;
                    q.push(nb[i]);
                }
            }
        }
        return -1;
    }

    void CheckRandomMaps(const HexPathGrid& proto, const std::vector<CellCoord>& allCells, const char* tag, int width, int height, int radius)
    {
        (void)proto;
        for (uint32_t seed = 1; seed <= 25; ++seed)
        {
            Lcg rng(seed * 7919u);
            HexPathGrid g = (width > 0) ? HexPathGrid(width, height) : HexPathGrid(radius);
            for (const CellCoord& c : allCells)
            {
                if (rng.Next() % 4 == 0) g.SetPassable(c, false);
            }
            for (int trial = 0; trial < 6; ++trial)
            {
                CellCoord a = allCells[rng.Next() % allCells.size()];
                CellCoord b = allCells[rng.Next() % allCells.size()];
                if (!g.IsPassable(a) || !g.IsPassable(b)) continue;
                FlatCostProvider costs;
                PathResult r = FindPath(g, a, b, costs);
                const int bfs = BfsDistance(g, a, b);
                if (bfs < 0)
                {
                    EXPECT_FALSE(r.success) << tag << " seed " << seed;
                }
                else
                {
                    ASSERT_TRUE(r.success) << tag << " seed " << seed;
                    EXPECT_FLOAT_EQ(r.totalCost, static_cast<float>(bfs)) << tag << " seed " << seed;
                }
            }
        }
    }
}

TEST(HexFindPath, RectCornerToCornerMatchesDistance)
{
    HexPathGrid g(20, 20);
    FlatCostProvider costs;
    const CellCoord from{ 0, 0 }, to{ 19, 19 };
    PathResult r = FindPath(g, from, to, costs);
    ASSERT_TRUE(r.success);
    EXPECT_EQ(static_cast<uint32_t>(r.cells.Size() - 1), HexTopology::Distance(from, to));
}

TEST(HexFindPath, RectFullWallBlocks)
{
    HexPathGrid g(20, 20);
    for (int x = 0; x < 20; ++x) g.SetPassable(CellCoord{ x, 10 }, false);
    FlatCostProvider costs;
    PathResult r = FindPath(g, CellCoord{ 3, 0 }, CellCoord{ 15, 19 }, costs);
    EXPECT_FALSE(r.success);
}

TEST(HexFindPath, RectWallWithGapPassesThroughGap)
{
    HexPathGrid g(20, 20);
    for (int x = 0; x < 20; ++x) g.SetPassable(CellCoord{ x, 10 }, false);
    const CellCoord gap{ 12, 10 };
    g.SetPassable(gap, true);
    FlatCostProvider costs;
    PathResult r = FindPath(g, CellCoord{ 3, 0 }, CellCoord{ 15, 19 }, costs);
    ASSERT_TRUE(r.success);
    bool through = false;
    for (unsigned int i = 0; i < r.cells.Size(); ++i) if (r.cells[i] == gap) through = true;
    EXPECT_TRUE(through);
}

TEST(HexFindPath, RectDeterministic)
{
    HexPathGrid g(20, 20);
    for (int x = 0; x < 20; ++x) g.SetPassable(CellCoord{ x, 10 }, false);
    g.SetPassable(CellCoord{ 12, 10 }, true);
    FlatCostProvider costs;
    PathResult a = FindPath(g, CellCoord{ 0, 0 }, CellCoord{ 19, 19 }, costs);
    PathResult b = FindPath(g, CellCoord{ 0, 0 }, CellCoord{ 19, 19 }, costs);
    ASSERT_TRUE(a.success);
    ASSERT_TRUE(b.success);
    ASSERT_EQ(a.cells.Size(), b.cells.Size());
    for (unsigned int i = 0; i < a.cells.Size(); ++i) EXPECT_TRUE(a.cells[i] == b.cells[i]);
}

TEST(HexFindPath, RectOptimalVsBfsOnRandomMaps)
{
    std::vector<CellCoord> cells;
    for (int y = 0; y < 12; ++y) for (int x = 0; x < 15; ++x) cells.push_back(CellCoord{ x, y });
    CheckRandomMaps(HexPathGrid(15, 12), cells, "rect", 15, 12, 0);
}

TEST(HexFindPath, AxialOptimalVsBfsOnRandomMaps)
{
    const int R = 8;
    std::vector<CellCoord> cells;
    for (int q = -R; q <= R; ++q)
        for (int r = -R; r <= R; ++r)
            if (std::abs(q + r) <= R) cells.push_back(CellCoord{ q, r });
    CheckRandomMaps(HexPathGrid(R), cells, "axial", 0, 0, R);
}
