// TestGridSearchConcept.cpp - CIndexGraph concept and HexNeighbours adapter.

#include <gtest/gtest.h>
#include <vector>

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
