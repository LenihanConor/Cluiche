// Suite: FlowFieldGolden
// Known-exact direction vectors derived analytically for small 4-connected grids.

#include <gtest/gtest.h>
#include <DiaFlowField/ComputeFlowField.h>
#include <DiaFlowField/SquareFlowAdapter.h>
#include <DiaFlowField/Testing/FlowFieldTestHelpers.h>
#include <DiaPathfinding/SquarePathGrid.h>
#include <DiaPathfinding/IPathCostProvider.h>
#include <DiaMaths/Vector/Vector2D.h>
#include <cmath>

using namespace Dia::FlowField;
using Dia::Pathfinding::CellCoord;
using Dia::Pathfinding::FlatCostProvider;
using Dia::Maths::Vector2D;
using namespace Dia::FlowField::Testing;

// 4×4 grid, goal at (3,3).
// 4-connected Dijkstra from goal: each cell's parent is the step toward goal.
//
// Row 3 (y=3): directions purely +x (toward goal column)
//   (0,3) → (1,3) → parent=(1,3): dir=+x = (1,0)
//   (1,3) → (2,3) → parent=(2,3): dir=+x = (1,0)
//   (2,3) → (3,3) → parent=(3,3): dir=+x = (1,0)
//   (3,3) = goal: reachable, dir=(0,0)
//
// Column 3 (x=3): directions purely +y (toward goal row)
//   (3,0) → (3,1): dir=+y = (0,1)
//   (3,1) → (3,2): dir=+y = (0,1)
//   (3,2) → (3,3): dir=+y = (0,1)
//
// Corner (0,0): can go right OR up (equal-cost tie); Dijkstra breaks ties
//   non-deterministically, so we only check reachable + dir is unit.

namespace {
    struct Golden4x4 : public ::testing::Test {
        Dia::Pathfinding::SquarePathGrid grid{4, 4, Dia::Pathfinding::SquareConnectivity::k4Connected};
        SquareFlowAdapter adapter{grid};
        FlatCostProvider costs;
        FlowField f = ComputeFlowField(adapter, CellCoord{3, 3}, costs, 4, 4);
    };
}

TEST_F(Golden4x4, GoalCell_ReachableDirectionZero) {
    const FlowCell& fc = f.Sample(CellCoord{3, 3});
    EXPECT_TRUE(fc.reachable);
    EXPECT_FLOAT_EQ(fc.direction.X(), 0.0f);
    EXPECT_FLOAT_EQ(fc.direction.Y(), 0.0f);
}

TEST_F(Golden4x4, BottomRow_PointsRight) {
    // Cells (0,3),(1,3),(2,3) are in the same row as goal; only direction is +x.
    AssertCellDirection(f, CellCoord{0, 3}, Vector2D(1.0f, 0.0f), 1.0f);
    AssertCellDirection(f, CellCoord{1, 3}, Vector2D(1.0f, 0.0f), 1.0f);
    AssertCellDirection(f, CellCoord{2, 3}, Vector2D(1.0f, 0.0f), 1.0f);
}

TEST_F(Golden4x4, RightColumn_PointsUp) {
    // Cells (3,0),(3,1),(3,2) are in the same column as goal; only direction is +y.
    AssertCellDirection(f, CellCoord{3, 0}, Vector2D(0.0f, 1.0f), 1.0f);
    AssertCellDirection(f, CellCoord{3, 1}, Vector2D(0.0f, 1.0f), 1.0f);
    AssertCellDirection(f, CellCoord{3, 2}, Vector2D(0.0f, 1.0f), 1.0f);
}

TEST_F(Golden4x4, AllCells_Reachable) {
    EXPECT_TRUE(f.IsComplete());
}

// Isolated goal: 1×1 grid — goal IS the only cell.
TEST(FlowFieldGolden, SingleCellGrid_GoalOnly) {
    Dia::Pathfinding::SquarePathGrid grid(1, 1);
    SquareFlowAdapter adapter(grid);
    FlatCostProvider costs;
    FlowField f = ComputeFlowField(adapter, CellCoord{0, 0}, costs, 1, 1);
    const FlowCell& fc = f.Sample(CellCoord{0, 0});
    EXPECT_TRUE(fc.reachable);
    EXPECT_FLOAT_EQ(fc.direction.X(), 0.0f);
    EXPECT_FLOAT_EQ(fc.direction.Y(), 0.0f);
}

// Corridor: 1×5 horizontal strip, goal at right end (4,0).
// Every cell must point purely right (+x, 0).
TEST(FlowFieldGolden, HorizontalCorridor_AllPointRight) {
    Dia::Pathfinding::SquarePathGrid grid(5, 1);
    SquareFlowAdapter adapter(grid);
    FlatCostProvider costs;
    FlowField f = ComputeFlowField(adapter, CellCoord{4, 0}, costs, 5, 1);
    AssertCellDirection(f, CellCoord{0, 0}, Vector2D(1.0f, 0.0f), 1.0f);
    AssertCellDirection(f, CellCoord{1, 0}, Vector2D(1.0f, 0.0f), 1.0f);
    AssertCellDirection(f, CellCoord{2, 0}, Vector2D(1.0f, 0.0f), 1.0f);
    AssertCellDirection(f, CellCoord{3, 0}, Vector2D(1.0f, 0.0f), 1.0f);
}

// Vertical corridor: 1×5 strip, goal at bottom (0,4).
// Every cell must point purely up (+y).
TEST(FlowFieldGolden, VerticalCorridor_AllPointUp) {
    Dia::Pathfinding::SquarePathGrid grid(1, 5);
    SquareFlowAdapter adapter(grid);
    FlatCostProvider costs;
    FlowField f = ComputeFlowField(adapter, CellCoord{0, 4}, costs, 1, 5);
    AssertCellDirection(f, CellCoord{0, 0}, Vector2D(0.0f, 1.0f), 1.0f);
    AssertCellDirection(f, CellCoord{0, 1}, Vector2D(0.0f, 1.0f), 1.0f);
    AssertCellDirection(f, CellCoord{0, 2}, Vector2D(0.0f, 1.0f), 1.0f);
    AssertCellDirection(f, CellCoord{0, 3}, Vector2D(0.0f, 1.0f), 1.0f);
}
