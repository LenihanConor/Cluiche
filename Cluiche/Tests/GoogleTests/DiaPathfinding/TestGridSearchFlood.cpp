#include <gtest/gtest.h>

#include <algorithm>
#include <deque>
#include <vector>

#include "DiaPathfinding/AdjacencyGraph.h"
#include "DiaPathfinding/GridSearch.h"
#include "DiaPathfinding/GridSearchScratch.h"
#include "DiaPathfinding/HexNeighbours.h"
#include "DiaPathfinding/Testing/GridSearchTestGrids.h"
#include "GridSearchAllocCounter.h"

using namespace Dia::Pathfinding;
using Dia::Geometry2D::HexTopology;
using Dia::Geometry2D::kNoCell;
using namespace Dia::Pathfinding::Testing;

namespace {
    constexpr uint32_t U = kUnreached;

    // Hex 4x4 (index = y*4+x): directions 0..5 = NE,N,NW,SW,S,SE, so column x is a vertical line
    // (N = index-4, S = index+4) and cells of columns 0 and 3 are never adjacent to each other.
    auto InColumn(uint32_t a, uint32_t b = 99u) { return [a, b](uint32_t c) { return c % 4 == a || c % 4 == b; }; }

    template<class G, class P>
    std::vector<uint32_t> Flood(const G& g, uint32_t start, P in, std::vector<uint8_t>& seen, GridSearchScratch& s, uint32_t& count)
    {
        std::vector<uint32_t> cells(g.CellCount() + 1, 777u);
        count = FloodRegion(g, start, in, seen.data(), cells.data(), s);
        cells.resize(count);
        return cells;
    }

    struct Comp {
        std::vector<uint32_t> label, sizes;
        uint32_t count;
    };

    template<class G, class P>
    Comp Components(const G& g, P in, GridSearchScratch& s)
    {
        Comp r;
        r.label.assign(g.CellCount(), 7u);
        r.sizes.assign(g.CellCount() + 1, 7u);
        r.count = ConnectedComponents(g, in, r.label.data(), r.sizes.data(), s);
        r.sizes.resize(r.count);
        return r;
    }

    // Naive reference: label by scanning cells and flooding with std::deque.
    template<class G, class P>
    std::vector<uint32_t> NaiveLabels(const G& g, P in, uint32_t& count)
    {
        std::vector<uint32_t> label(g.CellCount(), U);
        count = 0;
        for (uint32_t i = 0; i < g.CellCount(); ++i) {
            if (!in(i) || label[i] != U) continue;
            std::deque<uint32_t> q{ i };
            label[i] = count;
            while (!q.empty()) {
                const uint32_t c = q.front(); q.pop_front();
                g.ForEachNeighbour(c, [&](uint32_t n) {
                    if (in(n) && label[n] == U) { label[n] = count; q.push_back(n); }
                });
            }
            ++count;
        }
        return label;
    }
}

TEST(GridSearchFlood, FloodRegionBfsOrderOnHexColumn)
{
    const HexTopology topo = MakeSmallHexGrid();
    const HexNeighbours g(topo);
    GridSearchScratch s;
    std::vector<uint8_t> seen(16, 0);
    uint32_t n = 0;
    const auto cells = Flood(g, 8, InColumn(0), seen, s, n);
    // From 8: N (4), S (12) in direction order, then 4's N neighbour (0).
    EXPECT_EQ(cells, (std::vector<uint32_t>{ 8, 4, 12, 0 }));
    for (uint32_t c = 0; c < 16; ++c) EXPECT_EQ(seen[c], c % 4 == 0 ? 1 : 0) << c;
}

TEST(GridSearchFlood, FloodRegionOnChainFifoOrder)
{
    const ChainGraph g(7);
    GridSearchScratch s;
    std::vector<uint8_t> seen(7, 0);
    uint32_t n = 0;
    const auto cells = Flood(g, 3, [](uint32_t) { return true; }, seen, s, n);
    EXPECT_EQ(cells, (std::vector<uint32_t>{ 3, 2, 4, 1, 5, 0, 6 }));
}

TEST(GridSearchFlood, FloodRegionStopsAtRegionBoundary)
{
    const ChainGraph g(7);
    GridSearchScratch s;
    std::vector<uint8_t> seen(7, 0);
    uint32_t n = 0;
    // Region is cells 1..3 and 5..6; start at 2.
    const auto cells = Flood(g, 2, [](uint32_t c) { return c != 0 && c != 4; }, seen, s, n);
    EXPECT_EQ(cells, (std::vector<uint32_t>{ 2, 1, 3 }));
    EXPECT_EQ(seen, (std::vector<uint8_t>{ 0, 1, 1, 1, 0, 0, 0 }));
}

TEST(GridSearchFlood, FloodRegionStartOutsideRegionReturnsZero)
{
    const ChainGraph g(5);
    GridSearchScratch s;
    std::vector<uint8_t> seen(5, 0);
    uint32_t n = 99;
    Flood(g, 2, [](uint32_t c) { return c != 2; }, seen, s, n);
    EXPECT_EQ(n, 0u);
    EXPECT_EQ(seen, (std::vector<uint8_t>{ 0, 0, 0, 0, 0 }));
}

TEST(GridSearchFlood, FloodRegionStartAlreadySeenReturnsZero)
{
    const ChainGraph g(5);
    GridSearchScratch s;
    std::vector<uint8_t> seen(5, 0);
    seen[2] = 1;
    uint32_t n = 99;
    Flood(g, 2, [](uint32_t) { return true; }, seen, s, n);
    EXPECT_EQ(n, 0u);
    EXPECT_EQ(seen, (std::vector<uint8_t>{ 0, 0, 1, 0, 0 }));
}

TEST(GridSearchFlood, FloodRegionDoesNotReenterSeenCells)
{
    const ChainGraph g(5);
    GridSearchScratch s;
    std::vector<uint8_t> seen(5, 0);
    seen[3] = 1;
    uint32_t n = 0;
    const auto cells = Flood(g, 1, [](uint32_t) { return true; }, seen, s, n);
    EXPECT_EQ(cells, (std::vector<uint32_t>{ 1, 0, 2 }));
    EXPECT_EQ(seen[4], 0);
}

TEST(GridSearchFlood, FloodRegionFollowsDirectedEdgesOnTree)
{
    // 0 -> {1, 2}; 1 -> 3; 4 is a second root.
    const TreeGraph g({ kNoCell, 0, 0, 1, kNoCell });
    GridSearchScratch s;
    std::vector<uint8_t> seen(5, 0);
    uint32_t n = 0;
    const auto cells = Flood(g, 0, [](uint32_t) { return true; }, seen, s, n);
    EXPECT_EQ(cells, (std::vector<uint32_t>{ 0, 1, 2, 3 }));
    EXPECT_EQ(seen[4], 0);
}

TEST(GridSearchFlood, ComponentsOnHexColumns)
{
    const HexTopology topo = MakeSmallHexGrid();
    const HexNeighbours g(topo);
    GridSearchScratch s;
    const Comp r = Components(g, InColumn(0, 3), s);
    EXPECT_EQ(r.count, 2u);
    EXPECT_EQ(r.sizes, (std::vector<uint32_t>{ 4, 4 }));
    for (uint32_t c = 0; c < 16; ++c) {
        const uint32_t expect = c % 4 == 0 ? 0u : (c % 4 == 3 ? 1u : U);
        EXPECT_EQ(r.label[c], expect) << c;
    }
}

TEST(GridSearchFlood, ComponentsLabelsAscendByFirstCellOnChain)
{
    const ChainGraph g(10);
    GridSearchScratch s;
    // Region: 1,2 | 4 | 6,7,8 ; cells 0,3,5,9 are outside.
    const Comp r = Components(g, [](uint32_t c) { return c == 1 || c == 2 || c == 4 || (c >= 6 && c <= 8); }, s);
    EXPECT_EQ(r.count, 3u);
    EXPECT_EQ(r.sizes, (std::vector<uint32_t>{ 2, 1, 3 }));
    EXPECT_EQ(r.label, (std::vector<uint32_t>{ U, 0, 0, U, 1, U, 2, 2, 2, U }));
}

TEST(GridSearchFlood, ComponentsNullSizesAccepted)
{
    const ChainGraph g(4);
    GridSearchScratch s;
    std::vector<uint32_t> label(4, 7u);
    EXPECT_EQ(ConnectedComponents(g, [](uint32_t c) { return c != 2; }, label.data(), nullptr, s), 2u);
    EXPECT_EQ(label, (std::vector<uint32_t>{ 0, 0, U, 1 }));
}

TEST(GridSearchFlood, ComponentsPredicateFalseEverywhere)
{
    const HexTopology topo = MakeSmallHexGrid();
    const HexNeighbours g(topo);
    GridSearchScratch s;
    const Comp r = Components(g, [](uint32_t) { return false; }, s);
    EXPECT_EQ(r.count, 0u);
    for (uint32_t l : r.label) EXPECT_EQ(l, U);

    std::vector<uint8_t> seen(16, 0);
    uint32_t n = 99;
    Flood(g, 5, [](uint32_t) { return false; }, seen, s, n);
    EXPECT_EQ(n, 0u);
}

TEST(GridSearchFlood, ComponentsIsolatedCellsEachOwnComponent)
{
    const ChainGraph g(7);
    GridSearchScratch s;
    // Every other cell: 0,2,4,6 are four isolated singletons.
    const Comp r = Components(g, [](uint32_t c) { return c % 2 == 0; }, s);
    EXPECT_EQ(r.count, 4u);
    EXPECT_EQ(r.sizes, (std::vector<uint32_t>{ 1, 1, 1, 1 }));
    EXPECT_EQ(r.label, (std::vector<uint32_t>{ 0, U, 1, U, 2, U, 3 }));
}

TEST(GridSearchFlood, ComponentsFollowForwardEdgesOnlyOnDirectedGraph)
{
    // Roots 0 and 4; 1,2 children of 0; 3 child of 1. Scanning ascending: cell 0 reaches 1,2,3.
    const TreeGraph g({ kNoCell, 0, 0, 1, kNoCell });
    GridSearchScratch s;
    const Comp r = Components(g, [](uint32_t) { return true; }, s);
    EXPECT_EQ(r.count, 2u);
    EXPECT_EQ(r.sizes, (std::vector<uint32_t>{ 4, 1 }));
    EXPECT_EQ(r.label, (std::vector<uint32_t>{ 0, 0, 0, 0, 1 }));
}

TEST(GridSearchFlood, SingleCellGrid)
{
    const HexTopology topo(1, 1);
    const HexNeighbours g(topo);
    GridSearchScratch s;
    std::vector<uint8_t> seen(1, 0);
    uint32_t n = 0;
    EXPECT_EQ(Flood(g, 0, [](uint32_t) { return true; }, seen, s, n), (std::vector<uint32_t>{ 0 }));
    const Comp r = Components(g, [](uint32_t) { return true; }, s);
    EXPECT_EQ(r.count, 1u);
    EXPECT_EQ(r.sizes, (std::vector<uint32_t>{ 1 }));
    EXPECT_EQ(r.label, (std::vector<uint32_t>{ 0 }));
}

TEST(GridSearchFlood, ScratchReusedSmallerAfterLarger)
{
    GridSearchScratch s;
    const ChainGraph big(200);
    const Comp rb = Components(big, [](uint32_t) { return true; }, s);
    EXPECT_EQ(rb.count, 1u);
    EXPECT_EQ(rb.sizes, (std::vector<uint32_t>{ 200 }));

    const ChainGraph small(5);
    const Comp rs = Components(small, [](uint32_t c) { return c != 1; }, s);
    EXPECT_EQ(rs.count, 2u);
    EXPECT_EQ(rs.sizes, (std::vector<uint32_t>{ 1, 3 }));
    EXPECT_EQ(rs.label, (std::vector<uint32_t>{ 0, U, 1, 1, 1 }));

    std::vector<uint8_t> seen(5, 0);
    uint32_t n = 0;
    EXPECT_EQ(Flood(small, 4, [](uint32_t) { return true; }, seen, s, n), (std::vector<uint32_t>{ 4, 3, 2, 1, 0 }));
}

TEST(GridSearchFlood, SecondWarmCallAllocatesNothing)
{
    const HexTopology topo(32, 32);
    const HexNeighbours g(topo);
    GridSearchScratch s;
    auto in = [](uint32_t c) { return (c / 32) % 4 != 0; };
    std::vector<uint32_t> label(g.CellCount()), sizes(g.CellCount()), cells(g.CellCount());
    std::vector<uint8_t> seen(g.CellCount());

    ConnectedComponents(g, in, label.data(), sizes.data(), s);
    FloodRegion(g, 32 * 1, in, seen.data(), cells.data(), s);

    {
        GridSearchTest::AllocCounter counter;
        ConnectedComponents(g, in, label.data(), sizes.data(), s);
        EXPECT_EQ(counter.Count(), 0u);
    }
    {
        std::fill(seen.begin(), seen.end(), uint8_t(0));
        GridSearchTest::AllocCounter counter;
        FloodRegion(g, 32 * 1, in, seen.data(), cells.data(), s);
        EXPECT_EQ(counter.Count(), 0u);
    }
}

TEST(GridSearchFlood, ComponentsMatchNaiveReferenceOnHexGrid)
{
    const HexTopology topo(23, 17);
    const HexNeighbours g(topo);
    GridSearchScratch s;
    // Deterministic pseudo-random mask, ~60% in region.
    auto in = [](uint32_t c) { return ((c * 2654435761u) >> 13) % 10 < 6; };

    uint32_t naiveCount = 0;
    const auto expect = NaiveLabels(g, in, naiveCount);
    const Comp r = Components(g, in, s);
    EXPECT_EQ(r.count, naiveCount);
    EXPECT_EQ(r.label, expect);

    std::vector<uint32_t> expectSizes(naiveCount, 0u);
    for (uint32_t l : expect) if (l != U) ++expectSizes[l];
    EXPECT_EQ(r.sizes, expectSizes);

    // FloodRegion from the first cell of each component yields exactly that component.
    std::vector<uint8_t> seen(g.CellCount(), 0);
    std::vector<uint32_t> cells(g.CellCount());
    for (uint32_t i = 0; i < g.CellCount(); ++i) {
        if (!in(i) || seen[i]) continue;
        const uint32_t n = FloodRegion(g, i, in, seen.data(), cells.data(), s);
        EXPECT_EQ(n, expectSizes[expect[i]]);
        for (uint32_t k = 0; k < n; ++k) EXPECT_EQ(expect[cells[k]], expect[i]);
    }
}

// Forest: 0 -> {1,2}; 4 -> 5; cells 3 and 6 isolated.
TEST(GridSearchFlood, AdjacencyForestFloodAndComponents)
{
    AdjacencyEdge edges[] = { { 4, 5 }, { 0, 2 }, { 0, 1 } };
    AdjacencyGraph g;
    g.Build(7, edges, 3);
    GridSearchScratch s;
    auto all = [](uint32_t) { return true; };

    std::vector<uint8_t> seen(7, 0);
    uint32_t n = 0;
    EXPECT_EQ(Flood(g, 3, all, seen, s, n), (std::vector<uint32_t>{ 3 }));
    EXPECT_EQ(Flood(g, 4, all, seen, s, n), (std::vector<uint32_t>{ 4, 5 }));
    EXPECT_EQ(Flood(g, 0, all, seen, s, n), (std::vector<uint32_t>{ 0, 1, 2 }));
    EXPECT_EQ(seen, (std::vector<uint8_t>{ 1, 1, 1, 1, 1, 1, 0 }));

    const Comp r = Components(g, all, s);
    EXPECT_EQ(r.count, 4u);
    EXPECT_EQ(r.sizes, (std::vector<uint32_t>{ 3, 1, 2, 1 }));
    EXPECT_EQ(r.label, (std::vector<uint32_t>{ 0, 0, 0, 1, 2, 2, 3 }));
}
