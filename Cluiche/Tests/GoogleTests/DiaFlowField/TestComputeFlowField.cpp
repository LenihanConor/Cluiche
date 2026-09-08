// Suite: ComputeFlowField

#include <gtest/gtest.h>
#include <DiaFlowField/ComputeFlowField.h>
#include <DiaFlowField/SquareFlowAdapter.h>
#include <DiaFlowField/Testing/FlowFieldTestHelpers.h>
#include <DiaPathfinding/SquarePathGrid.h>
#include <DiaPathfinding/IPathCostProvider.h>
#include <DiaMaths/Vector/Vector2D.h>

using namespace Dia::FlowField;
using Dia::Pathfinding::CellCoord;
using Dia::Pathfinding::FlatCostProvider;
using namespace Dia::FlowField::Testing;

TEST(ComputeFlowField, GoalCell_IsReachable) {
    MockFlowFieldGraph graph;
    FlatCostProvider costs;
    FlowField f = ComputeFlowField(graph, CellCoord{4, 4}, costs,
                                   MockFlowFieldGraph::kWidth, MockFlowFieldGraph::kHeight);
    AssertReachable(f, CellCoord{4, 4});
}

TEST(ComputeFlowField, AllCells_ReachableOnOpenGrid) {
    MockFlowFieldGraph graph;
    FlatCostProvider costs;
    FlowField f = ComputeFlowField(graph, CellCoord{4, 4}, costs,
                                   MockFlowFieldGraph::kWidth, MockFlowFieldGraph::kHeight);
    EXPECT_TRUE(f.IsComplete());
}

TEST(ComputeFlowField, BlockedCell_IsUnreachable) {
    MockFlowFieldGraph graph;
    // create an impassable island: block (3,4), (4,4) goal is in bottom half,
    // cut off row 3 cells 1-6 to isolate top-left corner (0,0) from goal at (6,6)
    for (int x = 0; x <= 7; ++x)
        graph.SetPassable(CellCoord{x, 3}, false);  // full horizontal wall
    FlatCostProvider costs;
    FlowField f = ComputeFlowField(graph, CellCoord{6, 6}, costs,
                                   MockFlowFieldGraph::kWidth, MockFlowFieldGraph::kHeight);
    // goal is reachable
    AssertReachable(f, CellCoord{6, 6});
    // cell above the wall is isolated (wall is row 3, goal is row 6, above-wall = row 0-2)
    AssertUnreachable(f, CellCoord{0, 0});
}

TEST(ComputeFlowField, DirectionPoints_TowardGoal) {
    MockFlowFieldGraph graph;
    FlatCostProvider costs;
    // goal at (7,7); cell at (0,7) should point right (+x direction)
    FlowField f = ComputeFlowField(graph, CellCoord{7, 7}, costs,
                                   MockFlowFieldGraph::kWidth, MockFlowFieldGraph::kHeight);
    // 4-connected, so direction from (0,7) toward (7,7) is (+1,0)
    AssertCellDirection(f, CellCoord{0, 7}, Dia::Maths::Vector2D(1.0f, 0.0f), 15.0f);
}

TEST(ComputeFlowField, SmallGrid_GoalAtCorner_AllReachable) {
    // Use SquarePathGrid + SquareFlowAdapter on a 3x3 grid
    Dia::Pathfinding::SquarePathGrid grid(3, 3, Dia::Pathfinding::SquareConnectivity::k4Connected);
    SquareFlowAdapter adapter(grid);
    FlatCostProvider costs;
    FlowField f = ComputeFlowField(adapter, CellCoord{0, 0}, costs, 3, 3);
    EXPECT_TRUE(f.IsComplete());
    AssertReachable(f, CellCoord{2, 2});
    AssertReachable(f, CellCoord{0, 0});
}

TEST(ComputeFlowField, GoalDirectionIsZeroVector) {
    MockFlowFieldGraph graph;
    FlatCostProvider costs;
    CellCoord goal{3, 3};
    FlowField f = ComputeFlowField(graph, goal, costs,
                                   MockFlowFieldGraph::kWidth, MockFlowFieldGraph::kHeight);
    // Goal cell: reachable but direction is zero
    const FlowCell& fc = f.Sample(goal);
    EXPECT_TRUE(fc.reachable);
    EXPECT_FLOAT_EQ(fc.direction.X(), 0.0f);
    EXPECT_FLOAT_EQ(fc.direction.Y(), 0.0f);
}
