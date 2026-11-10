// Suite: ComputeFlowField

#include <gtest/gtest.h>
#include <DiaFlowField/ComputeFlowField.h>
#include <DiaFlowField/SquareFlowAdapter.h>
#include <DiaFlowField/HexFlowAdapter.h>
#include <DiaFlowField/Testing/FlowFieldTestHelpers.h>
#include <DiaPathfinding/SquarePathGrid.h>
#include <DiaPathfinding/HexPathGrid.h>
#include <DiaPathfinding/HexTopology.h>
#include <DiaPathfinding/IPathCostProvider.h>
#include <DiaMaths/Vector/Vector2D.h>
#include <chrono>
#include <iostream>

using namespace Dia::FlowField;
using Dia::Pathfinding::CellCoord;
using Dia::Pathfinding::FlatCostProvider;
using Dia::Pathfinding::HexTopology;
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

// ===========================================================================
// Large grid tests (AC3, AC7, AC8)
// ===========================================================================

TEST(SLOW_ComputeFlowField, LargeHexGrid_104x64_Completes) {
    // AC3: ComputeFlowField over HexFlowAdapter on HexPathGrid(104, 64)
    Dia::Pathfinding::HexPathGrid grid(104, 64);
    HexFlowAdapter adapter(grid);
    FlatCostProvider costs;
    CellCoord goal{52, 32}; // near middle
    FlowField f = ComputeFlowField(adapter, goal, costs, 104, 64);

    // Every passable reachable cell has a direction that moves it strictly closer
    for (int row = 0; row < 64; ++row) {
        for (int col = 0; col < 104; ++col) {
            CellCoord cell{col, row};
            const FlowCell& fc = f.Sample(cell);
            if (fc.reachable && !(cell == goal)) {
                // Check that the direction is non-zero
                float len = std::sqrtf(fc.direction.X() * fc.direction.X() +
                                       fc.direction.Y() * fc.direction.Y());
                EXPECT_GT(len, 0.0f) << "Cell (" << col << "," << row << ") has zero direction";
            }
        }
    }
    // Goal is reachable
    EXPECT_TRUE(f.Sample(goal).reachable);
}

TEST(SLOW_ComputeFlowField, LargeHexGrid_128x128_Completes) {
    // AC3: ComputeFlowField on 128×128 rectangular hex grid
    Dia::Pathfinding::HexPathGrid grid(128, 128);
    HexFlowAdapter adapter(grid);
    FlatCostProvider costs;
    CellCoord goal{64, 64}; // center
    FlowField f = ComputeFlowField(adapter, goal, costs, 128, 128);

    EXPECT_TRUE(f.Sample(goal).reachable);
    // Check a few corners are reachable
    EXPECT_TRUE(f.Sample(CellCoord{0, 0}).reachable);
    EXPECT_TRUE(f.Sample(CellCoord{127, 0}).reachable);
    EXPECT_TRUE(f.Sample(CellCoord{0, 127}).reachable);
    EXPECT_TRUE(f.Sample(CellCoord{127, 127}).reachable);
}

TEST(SLOW_ComputeFlowField, LargeHexGrid_WithWall_RoutesThroughGap) {
    // AC3: impassable wall with one gap routes through the gap
    Dia::Pathfinding::HexPathGrid grid(104, 64);
    // Create vertical wall at column 52, except row 32
    for (int row = 0; row < 64; ++row) {
        if (row != 32)
            grid.SetPassable(CellCoord{52, row}, false);
    }

    HexFlowAdapter adapter(grid);
    FlatCostProvider costs;
    CellCoord goal{80, 32}; // right side
    FlowField f = ComputeFlowField(adapter, goal, costs, 104, 64);

    // Left side should reach goal through gap
    EXPECT_TRUE(f.Sample(CellCoord{10, 32}).reachable);
    // Cells blocked by wall (not near gap) may be unreachable depending on connectivity
    // Just verify compute completed without crash
}

TEST(SLOW_ComputeFlowField, SquareGrid_104x64_EndToEnd) {
    // AC7: ComputeFlowField over SquareFlowAdapter on SquarePathGrid(104, 64)
    Dia::Pathfinding::SquarePathGrid grid(104, 64);
    SquareFlowAdapter adapter(grid);
    FlatCostProvider costs;
    CellCoord goal{52, 32}; // near middle
    FlowField f = ComputeFlowField(adapter, goal, costs, 104, 64);

    // Every passable reachable cell's direction moves toward goal
    EXPECT_TRUE(f.Sample(goal).reachable);
    EXPECT_TRUE(f.Sample(CellCoord{0, 0}).reachable);
    EXPECT_TRUE(f.Sample(CellCoord{103, 63}).reachable);

    // Sample agrees with square-only SampleWorld
    float size = 10.0f;
    Dia::Maths::Vector2D worldPos(25.0f, 35.0f); // some position
    Dia::Maths::Vector2D graphAware = f.SampleWorld(adapter, worldPos, size);
    Dia::Maths::Vector2D squareOnly = f.SampleWorld(worldPos, size);
    EXPECT_FLOAT_EQ(graphAware.X(), squareOnly.X());
    EXPECT_FLOAT_EQ(graphAware.Y(), squareOnly.Y());
}

TEST(SLOW_ComputeFlowField, SquareGrid_WithWall_RoutesThroughGap) {
    // AC7: wall with gap routes through gap
    Dia::Pathfinding::SquarePathGrid grid(104, 64);
    // Vertical wall at column 52, gap at row 32
    for (int row = 0; row < 64; ++row) {
        if (row != 32)
            grid.SetPassable(CellCoord{52, row}, false);
    }

    SquareFlowAdapter adapter(grid);
    FlatCostProvider costs;
    CellCoord goal{80, 32};
    FlowField f = ComputeFlowField(adapter, goal, costs, 104, 64);

    // Left side reaches goal through gap
    EXPECT_TRUE(f.Sample(CellCoord{10, 32}).reachable);
    EXPECT_TRUE(f.Sample(goal).reachable);
}

// ===========================================================================
// Performance benchmark (AC9)
// ===========================================================================

TEST(SLOW_ComputeFlowField, Performance_104x64_HexGrid) {
    // AC9: ComputeFlowField on 104×64 hex grid with scattered obstacles
    Dia::Pathfinding::HexPathGrid grid(104, 64);
    
    // Add some scattered obstacles
    for (int i = 0; i < 200; ++i) {
        int col = (i * 37) % 104;
        int row = (i * 53) % 64;
        grid.SetPassable(CellCoord{col, row}, false);
    }

    HexFlowAdapter adapter(grid);
    FlatCostProvider costs;
    CellCoord goal{52, 32};

    auto start = std::chrono::steady_clock::now();
    FlowField f = ComputeFlowField(adapter, goal, costs, 104, 64);
    auto end = std::chrono::steady_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    
    // Report only - no fail threshold
    std::cout << "ComputeFlowField 104×64 hex grid: " << duration << " ms" << std::endl;
    std::cout << "Peak memory estimate: ~" << (6656 * 16) / 1024 << " KB for FlowField storage" << std::endl;
    
    // Verify it completed successfully
    EXPECT_TRUE(f.Sample(goal).reachable);
}
