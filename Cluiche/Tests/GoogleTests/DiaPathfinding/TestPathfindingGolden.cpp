// TestPathfindingGolden.cpp — Golden-value and optimality tests for DiaPathfinding.
//
// Suites:
//   FindPath_Golden  — exact cost / length / cell-sequence verification
//   FindPath_Optimal — A* must return minimum-cost paths

#include <gtest/gtest.h>
#include <cstdlib>

#include "DiaPathfinding/CellCoord.h"
#include "DiaPathfinding/IPathCostProvider.h"
#include "DiaPathfinding/PathResult.h"
#include "DiaPathfinding/SquarePathGrid.h"
#include "DiaPathfinding/HexPathGrid.h"
#include "DiaPathfinding/FindPath.h"

#include <DiaMaths/Vector/Vector2D.h>

using namespace Dia::Pathfinding;
using Dia::Core::Containers::DynamicArrayC;

namespace
{
    // Charges 100.0f for a specific destination cell; 1.0f for all others.
    class HighCostCellProvider : public IPathCostProvider
    {
    public:
        explicit HighCostCellProvider(CellCoord expensiveCell)
            : mExpensiveCell(expensiveCell) {}

        float GetCost(CellCoord /*from*/, CellCoord to) const override
        {
            return (to == mExpensiveCell) ? 100.0f : 1.0f;
        }

    private:
        CellCoord mExpensiveCell;
    };
} // anonymous namespace

// ===========================================================================
// FindPath_Golden — exact numeric values
// ===========================================================================

TEST(FindPath_Golden, StraightLine4Connected_ExactCost)
{
    // 5×1 strip: only path is (0,0)→(1,0)→…→(4,0) — 4 moves × 1.0f = 4.0f
    SquarePathGrid grid(5, 1, SquareConnectivity::k4Connected);
    FlatCostProvider flat;
    PathResult result = FindPath(grid, {0, 0}, {4, 0}, flat);

    ASSERT_TRUE(result.success);
    EXPECT_FLOAT_EQ(result.totalCost, 4.0f);
}

TEST(FindPath_Golden, StraightLine4Connected_PathLength)
{
    // Same strip: 4 moves + start = 5 cells
    SquarePathGrid grid(5, 1, SquareConnectivity::k4Connected);
    FlatCostProvider flat;
    PathResult result = FindPath(grid, {0, 0}, {4, 0}, flat);

    ASSERT_TRUE(result.success);
    EXPECT_EQ(result.cells.Size(), 5u);
}

TEST(FindPath_Golden, StraightLine4Connected_CellSequence)
{
    // 3×1 forced strip: exact cells must be (0,0),(1,0),(2,0)
    SquarePathGrid grid(3, 1, SquareConnectivity::k4Connected);
    FlatCostProvider flat;
    PathResult result = FindPath(grid, {0, 0}, {2, 0}, flat);

    ASSERT_TRUE(result.success);
    ASSERT_EQ(result.cells.Size(), 3u);
    EXPECT_TRUE((result.cells[0] == CellCoord{0, 0}));
    EXPECT_TRUE((result.cells[1] == CellCoord{1, 0}));
    EXPECT_TRUE((result.cells[2] == CellCoord{2, 0}));
}

TEST(FindPath_Golden, Diagonal8Connected_ExactCost)
{
    // 4×4 open grid, 8-connected. Optimal (0,0)→(3,3) uses 3 diagonal steps.
    // FlatCostProvider: each edge = 1.0f → total cost = 3.0f (Chebyshev).
    SquarePathGrid grid(4, 4, SquareConnectivity::k8Connected);
    FlatCostProvider flat;
    PathResult result = FindPath(grid, {0, 0}, {3, 3}, flat);

    ASSERT_TRUE(result.success);
    EXPECT_FLOAT_EQ(result.totalCost, 3.0f);
}

TEST(FindPath_Golden, Diagonal8Connected_PathLength)
{
    // 3 diagonal moves + start = 4 cells
    SquarePathGrid grid(4, 4, SquareConnectivity::k8Connected);
    FlatCostProvider flat;
    PathResult result = FindPath(grid, {0, 0}, {3, 3}, flat);

    ASSERT_TRUE(result.success);
    EXPECT_EQ(result.cells.Size(), 4u);
}

TEST(FindPath_Golden, HexGrid_StraightLine_ExactCost)
{
    // Radius-5 hex. Axial path (-3,0)→(3,0): 6 steps × 1.0f = 6.0f
    HexPathGrid grid(5);
    FlatCostProvider flat;
    PathResult result = FindPath(grid, {-3, 0}, {3, 0}, flat);

    ASSERT_TRUE(result.success);
    EXPECT_FLOAT_EQ(result.totalCost, 6.0f);
}

TEST(FindPath_Golden, HexGrid_StraightLine_PathLength)
{
    // 6 steps + start = 7 cells
    HexPathGrid grid(5);
    FlatCostProvider flat;
    PathResult result = FindPath(grid, {-3, 0}, {3, 0}, flat);

    ASSERT_TRUE(result.success);
    EXPECT_EQ(result.cells.Size(), 7u);
}

TEST(FindPath_Golden, ToWorldPositions_FailedResult_ProducesEmptyOutput)
{
    // A default (failed) PathResult must produce an empty world-position list.
    PathResult failed;
    DynamicArrayC<Dia::Maths::Vector2D, 256> worldPos;
    failed.ToWorldPositions(10.0f, worldPos);

    EXPECT_EQ(worldPos.Size(), 0u);
}

TEST(FindPath_Golden, ToWorldPositions_CellSizeScalesCorrectly)
{
    // Forced 3-cell straight path (0,0)→(1,0)→(2,0). cellSize=5:
    // expected world positions: (0,0), (5,0), (10,0)
    SquarePathGrid grid(3, 1, SquareConnectivity::k4Connected);
    FlatCostProvider flat;
    PathResult result = FindPath(grid, {0, 0}, {2, 0}, flat);
    ASSERT_TRUE(result.success);
    ASSERT_EQ(result.cells.Size(), 3u);

    const float cellSize = 5.0f;
    DynamicArrayC<Dia::Maths::Vector2D, 256> worldPos;
    result.ToWorldPositions(cellSize, worldPos);

    ASSERT_EQ(worldPos.Size(), 3u);
    EXPECT_FLOAT_EQ(worldPos[0].X(),  0.0f);
    EXPECT_FLOAT_EQ(worldPos[0].Y(),  0.0f);
    EXPECT_FLOAT_EQ(worldPos[1].X(),  5.0f);
    EXPECT_FLOAT_EQ(worldPos[1].Y(),  0.0f);
    EXPECT_FLOAT_EQ(worldPos[2].X(), 10.0f);
    EXPECT_FLOAT_EQ(worldPos[2].Y(),  0.0f);
}

TEST(FindPath_Golden, PathFirstCellIsFrom_LastCellIsTo)
{
    // Any successful non-trivial path must begin at 'from' and end at 'to'.
    SquarePathGrid grid(6, 6);
    FlatCostProvider flat;
    PathResult result = FindPath(grid, {1, 2}, {4, 5}, flat);

    ASSERT_TRUE(result.success);
    ASSERT_GE(result.cells.Size(), 2u);
    EXPECT_TRUE((result.cells[0] == CellCoord{1, 2}));
    EXPECT_TRUE((result.cells[result.cells.Size() - 1] == CellCoord{4, 5}));
}

// ===========================================================================
// FindPath_Optimal — A* must return minimum-cost paths
// ===========================================================================

TEST(FindPath_Optimal, TwoRoutes_ChoosesCheaperRoute)
{
    // 3×3 4-connected. S=(0,1), E=(2,1).
    //
    // Route A (direct):   (0,1)→(1,1)→(2,1)            — entering (1,1) costs 100 → total 101
    // Route B (detour):   (0,1)→(0,0)→(1,0)→(2,0)→(2,1) — 4 × 1.0f = 4
    //
    // A* must choose route B; totalCost == 4.0f; (1,1) absent from path.
    SquarePathGrid grid(3, 3, SquareConnectivity::k4Connected);
    HighCostCellProvider costs{{1, 1}};

    PathResult result = FindPath(grid, {0, 1}, {2, 1}, costs);

    ASSERT_TRUE(result.success);
    EXPECT_FLOAT_EQ(result.totalCost, 4.0f);

    for (int i = 0; i < result.cells.Size(); ++i)
    {
        EXPECT_FALSE((result.cells[i] == CellCoord{1, 1}))
            << "Path should avoid expensive cell (1,1)";
    }
}

TEST(FindPath_Optimal, DiagonalStepCheaperThanCardinalDetour)
{
    // 3×3 8-connected from (0,0) to (2,2).
    // Diagonal path: 2 moves × 1.0f = 2.0f, 3 cells.
    // Cardinal detour: 4 moves × 1.0f = 4.0f.
    // A* with Chebyshev heuristic must find the diagonal route.
    SquarePathGrid grid(3, 3, SquareConnectivity::k8Connected);
    FlatCostProvider flat;
    PathResult result = FindPath(grid, {0, 0}, {2, 2}, flat);

    ASSERT_TRUE(result.success);
    EXPECT_FLOAT_EQ(result.totalCost, 2.0f);
    EXPECT_EQ(result.cells.Size(), 3u);
}
