// TestGridSearchConcept.cpp - CIndexGraph concept and HexNeighbours adapter.

#include <gtest/gtest.h>
#include <algorithm>
#include <vector>

#include "DiaPathfinding/AdjacencyGraph.h"
#include "DiaPathfinding/GridSearch.h"
#include "DiaPathfinding/HexNeighbours.h"
#include "DiaPathfinding/Testing/GridSearchTestGrids.h"

using namespace Dia::Pathfinding;
using namespace Dia::Pathfinding::Testing;

namespace {
    struct NoForEach {
        uint32_t CellCount() const { return 1; }
    };
    struct NoCellCount {
        template<class Fn> void ForEachNeighbour(uint32_t, Fn&&) const {}
    };

    static_assert(CIndexGraph<HexNeighbours>);
    static_assert(CIndexGraph<ChainGraph>);
    static_assert(CIndexGraph<TreeGraph>);
    static_assert(!CIndexGraph<NoForEach>);
    static_assert(!CIndexGraph<NoCellCount>);
    static_assert(!CIndexGraph<int>);

    using V = std::vector<uint32_t>;

    template<class G>
    V Collect(const G& g, uint32_t c) {
        V out;
        g.ForEachNeighbour(c, [&](uint32_t n) { out.push_back(n); });
        return out;
    }
}

// 4x4 grid, index = y*4+x; even columns sit high, odd columns half a cell down.
// Direction order: NE, N, NW, SW, S, SE.
TEST(GridSearchConcept, HexNeighboursCornerSkipsOffGrid) {
    HexTopology t = MakeSmallHexGrid();
    HexNeighbours g(t);
    EXPECT_EQ(g.CellCount(), 16u);
    EXPECT_EQ(Collect(g, 0),  (V{ 4, 1 }));      // (0,0): only S, SE
    EXPECT_EQ(Collect(g, 15), (V{ 11, 14 }));    // (3,3) odd: only N, NW
}

TEST(GridSearchConcept, HexNeighboursEdgeSkipsOffGrid) {
    HexTopology t = MakeSmallHexGrid();
    HexNeighbours g(t);
    EXPECT_EQ(Collect(g, 2), (V{ 1, 6, 3 }));    // (2,0): SW, S, SE
}

TEST(GridSearchConcept, HexNeighboursInteriorVisitsAllSixInOrder) {
    HexTopology t = MakeSmallHexGrid();
    HexNeighbours g(t);
    EXPECT_EQ(Collect(g, 5),  (V{ 6, 1, 4, 8, 9, 10 }));   // (1,1) odd column
    EXPECT_EQ(Collect(g, 10), (V{ 7, 6, 5, 9, 14, 11 }));  // (2,2) even column
}

TEST(GridSearchConcept, HexNeighboursMatchesNeighbourAtOrder) {
    HexTopology t = MakeSmallHexGrid();
    HexNeighbours g(t);
    for (uint32_t c = 0; c < t.CellCount(); ++c) {
        V expected;
        for (uint32_t d = 0; d < 6; ++d) {
            const uint32_t n = t.NeighbourAt(c, d);
            if (n != kNoCell) { expected.push_back(n); }
        }
        EXPECT_EQ(Collect(g, c), expected) << "cell " << c;
    }
}

TEST(GridSearchConcept, ChainGraphNeighbourOrder) {
    ChainGraph g(4);
    EXPECT_EQ(g.CellCount(), 4u);
    EXPECT_EQ(Collect(g, 0), (V{ 1 }));
    EXPECT_EQ(Collect(g, 2), (V{ 1, 3 }));
    EXPECT_EQ(Collect(g, 3), (V{ 2 }));
}

TEST(GridSearchConcept, TreeGraphNeighbourOrder) {
    // Parents: 0 is root; 1 and 2 under 0; 3 and 4 under 1.
    TreeGraph g({ kNoCell, 0, 0, 1, 1 });
    EXPECT_EQ(g.CellCount(), 5u);
    EXPECT_EQ(Collect(g, 0), (V{ 1, 2 }));
    EXPECT_EQ(Collect(g, 1), (V{ 3, 4 }));
    EXPECT_EQ(Collect(g, 4), (V{}));
}

static_assert(CIndexGraph<AdjacencyGraph>);

TEST(GridSearchConcept, AdjacencyEmptyEdgeList) {
    AdjacencyGraph g;
    g.Build(3, nullptr, 0);
    EXPECT_EQ(g.CellCount(), 3u);
    for (uint32_t c = 0; c < 3; ++c) {
        EXPECT_EQ(g.NeighbourCount(c), 0u);
        EXPECT_TRUE(Collect(g, c).empty());
    }
}

TEST(GridSearchConcept, AdjacencyZeroCells) {
    AdjacencyGraph g;
    g.Build(0, nullptr, 0);
    EXPECT_EQ(g.CellCount(), 0u);
}

TEST(GridSearchConcept, AdjacencyNeighbourOrderIndependentOfInputOrder) {
    std::vector<AdjacencyEdge> edges = {{0, 3}, {0, 1}, {2, 5}, {2, 4}, {0, 2}, {5, 0}, {4, 4}};
    AdjacencyGraph ref;
    ref.Build(6, edges.data(), static_cast<uint32_t>(edges.size()));
    EXPECT_EQ(Collect(ref, 0), (V{1, 2, 3}));
    EXPECT_EQ(Collect(ref, 1), (V{}));
    EXPECT_EQ(Collect(ref, 2), (V{4, 5}));
    EXPECT_EQ(Collect(ref, 3), (V{}));
    EXPECT_EQ(Collect(ref, 4), (V{4}));
    EXPECT_EQ(Collect(ref, 5), (V{0}));
    EXPECT_EQ(ref.NeighbourCount(0), 3u);

    for (int round = 0; round < 6; ++round) {
        std::vector<AdjacencyEdge> shuffled = edges;
        std::rotate(shuffled.begin(), shuffled.begin() + round, shuffled.end());
        if (round % 2) { std::reverse(shuffled.begin(), shuffled.end()); }
        AdjacencyGraph g;
        g.Build(6, shuffled.data(), static_cast<uint32_t>(shuffled.size()));
        for (uint32_t c = 0; c < 6; ++c) { EXPECT_EQ(Collect(g, c), Collect(ref, c)) << "round " << round << " cell " << c; }
    }
}

TEST(GridSearchConcept, AdjacencyIsolatedCellsHaveEmptyRanges) {
    AdjacencyEdge edges[] = {{1, 3}};
    AdjacencyGraph g;
    g.Build(5, edges, 1);
    EXPECT_TRUE(Collect(g, 0).empty());
    EXPECT_EQ(Collect(g, 1), (V{3}));
    EXPECT_TRUE(Collect(g, 2).empty());
    EXPECT_TRUE(Collect(g, 3).empty());
    EXPECT_TRUE(Collect(g, 4).empty());
}

TEST(GridSearchConcept, AdjacencyDuplicateEdgesKept) {
    AdjacencyEdge edges[] = {{0, 1}, {0, 1}, {0, 2}, {0, 1}};
    AdjacencyGraph g;
    g.Build(3, edges, 4);
    EXPECT_EQ(Collect(g, 0), (V{1, 1, 1, 2}));
    EXPECT_EQ(g.NeighbourCount(0), 4u);
}

TEST(GridSearchConcept, AdjacencyRebuildDifferentSize) {
    AdjacencyGraph g;
    AdjacencyEdge big[] = {{0, 1}, {1, 2}, {2, 3}, {3, 4}, {4, 5}, {5, 6}, {6, 7}, {7, 0}};
    g.Build(8, big, 8);
    EXPECT_EQ(g.CellCount(), 8u);
    EXPECT_EQ(Collect(g, 7), (V{0}));
    AdjacencyEdge small[] = {{1, 0}, {1, 2}};
    g.Build(3, small, 2);
    EXPECT_EQ(g.CellCount(), 3u);
    EXPECT_EQ(Collect(g, 0), (V{}));
    EXPECT_EQ(Collect(g, 1), (V{0, 2}));
    EXPECT_EQ(Collect(g, 2), (V{}));
    g.Build(10, big, 8);
    EXPECT_EQ(g.CellCount(), 10u);
    EXPECT_EQ(Collect(g, 3), (V{4}));
    EXPECT_EQ(Collect(g, 9), (V{}));
}

TEST(GridSearchConcept, TreeGraphIsDirectedAscendingChildren) {
    const uint32_t N = kNoCell;
    TreeGraph t({N, 0, 0, 1, 1, N, 5});
    EXPECT_EQ(t.CellCount(), 7u);
    EXPECT_EQ(Collect(t, 0), (V{1, 2}));
    EXPECT_EQ(Collect(t, 1), (V{3, 4}));
    EXPECT_EQ(Collect(t, 5), (V{6}));
    EXPECT_EQ(Collect(t, 6), (V{}));
}
