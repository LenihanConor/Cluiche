// Suite: SquareFlowAdapter, HexFlowAdapter

#include <gtest/gtest.h>
#include <DiaFlowField/SquareFlowAdapter.h>
#include <DiaFlowField/HexFlowAdapter.h>
#include <DiaFlowField/CFlowFieldGraph.h>
#include <DiaFlowField/ComputeFlowField.h>
#include <DiaPathfinding/SquarePathGrid.h>
#include <DiaPathfinding/HexPathGrid.h>
#include <DiaPathfinding/HexTopology.h>
#include <DiaPathfinding/IPathCostProvider.h>

using namespace Dia::FlowField;
using Dia::Pathfinding::CellCoord;
using Dia::Pathfinding::FlatCostProvider;
using Dia::Maths::Vector2D;

TEST(SquareFlowAdapter, SatisfiesCFlowFieldGraph) {
    static_assert(CFlowFieldGraph<SquareFlowAdapter>);
    SUCCEED();
}

TEST(HexFlowAdapter, SatisfiesCFlowFieldGraph) {
    static_assert(CFlowFieldGraph<HexFlowAdapter>);
    SUCCEED();
}

TEST(SquareFlowAdapter, CellToWorldPosition_CellCentered) {
    Dia::Pathfinding::SquarePathGrid grid(5, 5);
    SquareFlowAdapter adapter(grid);
    Vector2D pos = adapter.CellToWorldPosition(CellCoord{2, 3}, 10.0f);
    EXPECT_FLOAT_EQ(pos.X(), 25.0f);  // 2*10 + 5
    EXPECT_FLOAT_EQ(pos.Y(), 35.0f);  // 3*10 + 5
}

TEST(SquareFlowAdapter, WorldToCell_InvertsCorrectly) {
    Dia::Pathfinding::SquarePathGrid grid(5, 5);
    SquareFlowAdapter adapter(grid);
    CellCoord cell = adapter.WorldToCell(Vector2D(25.0f, 35.0f), 10.0f);
    EXPECT_EQ(cell.x, 2);
    EXPECT_EQ(cell.y, 3);
}

TEST(SquareFlowAdapter, ForwardsPassability) {
    Dia::Pathfinding::SquarePathGrid grid(5, 5);
    grid.SetPassable(CellCoord{1, 1}, false);
    SquareFlowAdapter adapter(grid);
    EXPECT_TRUE(adapter.IsPassable(CellCoord{0, 0}));
    EXPECT_FALSE(adapter.IsPassable(CellCoord{1, 1}));
}

TEST(SquareFlowAdapter, ComputeFlowField_AllReachable) {
    Dia::Pathfinding::SquarePathGrid grid(4, 4);
    SquareFlowAdapter adapter(grid);
    FlatCostProvider costs;
    FlowField f = ComputeFlowField(adapter, CellCoord{0, 0}, costs,
                                   adapter.GetWidth(), adapter.GetHeight());
    EXPECT_TRUE(f.IsComplete());
}

TEST(HexFlowAdapter, CellToWorldPosition_OriginIsZero) {
    Dia::Pathfinding::HexPathGrid grid(3);
    HexFlowAdapter adapter(grid);
    Vector2D pos = adapter.CellToWorldPosition(CellCoord{0, 0}, 1.0f);
    EXPECT_FLOAT_EQ(pos.X(), 0.0f);
    EXPECT_FLOAT_EQ(pos.Y(), 0.0f);
}

TEST(HexFlowAdapter, ComputeFlowField_SmallHexGrid_GoalReachable) {
    Dia::Pathfinding::HexPathGrid grid(2);
    HexFlowAdapter adapter(grid);
    FlatCostProvider costs;
    // Hex grid radius 2: 5x5 bounding box = 25 cells max.
    FlowField f = ComputeFlowField(adapter, CellCoord{0, 0}, costs, 5, 5);
    EXPECT_TRUE(f.Sample(CellCoord{0, 0}).reachable);
}

// ---- Rectangular (flat-top odd-q) layout ----

namespace {
    // Next cell along the field: direction is the grid-space unit vector toward the parent,
    // so its component signs recover the (dx, dy) step.
    CellCoord StepAlongField(const FlowField& f, CellCoord c) {
        Vector2D d = f.Sample(c).direction;
        auto sgn = [](float v) { return v > 0.01f ? 1 : (v < -0.01f ? -1 : 0); };
        return CellCoord{ c.x + sgn(d.X()), c.y + sgn(d.Y()) };
    }
}

TEST(HexFlowAdapterRect, Construct_DoesNotAssert) {
    Dia::Pathfinding::HexPathGrid grid(20, 15);
    HexFlowAdapter adapter(grid);
    EXPECT_EQ(adapter.GetWidth(), 20);
    EXPECT_EQ(adapter.GetHeight(), 15);
}

TEST(HexFlowAdapterRect, WorldMapping_MatchesHexTopology) {
    Dia::Pathfinding::HexPathGrid grid(20, 15);
    HexFlowAdapter adapter(grid);
    for (float size : { 0.5f, 1.0f, 10.0f, 32.0f }) {
        for (int y = 0; y < 15; ++y) {
            for (int x = 0; x < 20; ++x) {
                CellCoord c{ x, y };
                Vector2D expected = Dia::Pathfinding::HexTopology::CellCenter(c, size);
                Vector2D actual = adapter.CellToWorldPosition(c, size);
                EXPECT_FLOAT_EQ(actual.X(), expected.X());
                EXPECT_FLOAT_EQ(actual.Y(), expected.Y());
                CellCoord back = adapter.WorldToCell(actual, size);
                EXPECT_EQ(back.x, x);
                EXPECT_EQ(back.y, y);
                CellCoord ref = Dia::Pathfinding::HexTopology::CellFromPoint(expected, size);
                EXPECT_EQ(back.x, ref.x);
                EXPECT_EQ(back.y, ref.y);
            }
        }
    }
}

TEST(HexFlowAdapterRect, FlowField_EveryReachableCellGetsCloserToGoal) {
    Dia::Pathfinding::HexPathGrid grid(20, 15);
    HexFlowAdapter adapter(grid);
    FlatCostProvider costs;
    CellCoord goal{ 10, 7 };
    FlowField f = ComputeFlowField(adapter, goal, costs, 20, 15);
    for (int y = 0; y < 15; ++y) {
        for (int x = 0; x < 20; ++x) {
            CellCoord c{ x, y };
            if (c.x == goal.x && c.y == goal.y) continue;
            ASSERT_TRUE(f.Sample(c).reachable) << x << "," << y;
            CellCoord n = StepAlongField(f, c);
            EXPECT_EQ(Dia::Pathfinding::HexTopology::Distance(c, n), 1u) << x << "," << y;
            EXPECT_LT(Dia::Pathfinding::HexTopology::Distance(n, goal),
                      Dia::Pathfinding::HexTopology::Distance(c, goal)) << x << "," << y;
        }
    }
}

TEST(HexFlowAdapterRect, FlowField_RoutesAroundWall) {
    Dia::Pathfinding::HexPathGrid grid(20, 15);
    // Vertical wall at columns 9 and 10 (two thick, so no diagonal leak), gap at row 13.
    for (int y = 0; y < 15; ++y) {
        if (y == 13) continue;
        grid.SetPassable(CellCoord{ 9, y }, false);
        grid.SetPassable(CellCoord{ 10, y }, false);
    }
    HexFlowAdapter adapter(grid);
    FlatCostProvider costs;
    CellCoord goal{ 15, 2 };
    FlowField f = ComputeFlowField(adapter, goal, costs, 20, 15);

    CellCoord start{ 3, 2 };
    ASSERT_TRUE(f.Sample(start).reachable);
    int steps = 0;
    bool passedGap = false;
    CellCoord c = start;
    while (!(c.x == goal.x && c.y == goal.y) && steps < 200) {
        EXPECT_TRUE(grid.IsPassable(c)) << c.x << "," << c.y;
        if ((c.x == 9 || c.x == 10) && c.y == 13) passedGap = true;
        c = StepAlongField(f, c);
        ++steps;
    }
    EXPECT_TRUE(c.x == goal.x && c.y == goal.y);
    EXPECT_TRUE(passedGap);
    EXPECT_GT(steps, (int)Dia::Pathfinding::HexTopology::Distance(start, goal));
}
