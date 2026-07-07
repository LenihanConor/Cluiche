// TestPathfinding.cpp — GoogleTest coverage for the DiaPathfinding module.
//
// Suites:
//   CellCoord, IPathCostProvider, SquarePathGrid, HexPathGrid,
//   FindPath, PathfindingSystem, TestHelpers

#include <gtest/gtest.h>
#include <cstdlib>  // std::abs (integer)
#include <cmath>    // std::fabsf

#include "DiaPathfinding/CellCoord.h"
#include "DiaPathfinding/CPathGraph.h"
#include "DiaPathfinding/IPathCostProvider.h"
#include "DiaPathfinding/PathResult.h"
#include "DiaPathfinding/SquarePathGrid.h"
#include "DiaPathfinding/HexPathGrid.h"
#include "DiaPathfinding/FindPath.h"
#include "DiaPathfinding/IPathResultObserver.h"
#include "DiaPathfinding/PathfindingSystem.h"
#include "DiaPathfinding/Testing/PathfindingTestHelpers.h"

#include <DiaMaths/Vector/Vector2D.h>

using namespace Dia::Pathfinding;
using Dia::Core::Containers::DynamicArrayC;

// ---------------------------------------------------------------------------
// Local mock observer used by PathfindingSystem tests
// ---------------------------------------------------------------------------
namespace
{
    class MockPathResultObserver : public IPathResultObserver
    {
    public:
        int           foundCount  = 0;
        int           failedCount = 0;
        PathRequestId lastFoundId;

        void OnPathFound(PathRequestId id, const PathResult& /*result*/) override
        {
            ++foundCount;
            lastFoundId = id;
        }

        void OnPathFailed(PathRequestId /*id*/) override
        {
            ++failedCount;
        }
    };

    // A cost provider that returns kImpassableCost for a specific cell
    class BlockCellCostProvider : public IPathCostProvider
    {
    public:
        explicit BlockCellCostProvider(CellCoord blockedCell)
            : mBlocked(blockedCell)
        {}

        float GetCost(CellCoord /*from*/, CellCoord to) const override
        {
            if (to == mBlocked)
                return kImpassableCost;
            return 1.0f;
        }

    private:
        CellCoord mBlocked;
    };

} // anonymous namespace

// ===========================================================================
// CellCoord
// ===========================================================================

TEST(CellCoord, Equality_SameCoordsAreEqual)
{
    const CellCoord a{3, 7};
    const CellCoord b{3, 7};
    EXPECT_TRUE(a == b);
}

TEST(CellCoord, Equality_DifferentCoordsAreNotEqual)
{
    const CellCoord a{1, 2};
    const CellCoord b{1, 3};
    EXPECT_FALSE(a == b);

    const CellCoord c{0, 0};
    const CellCoord d{1, 0};
    EXPECT_FALSE(c == d);
}

// ===========================================================================
// IPathCostProvider
// ===========================================================================

TEST(IPathCostProvider, FlatCostProvider_AlwaysReturnsOne)
{
    FlatCostProvider flat;
    EXPECT_FLOAT_EQ(flat.GetCost({0, 0}, {1, 0}), 1.0f);
    EXPECT_FLOAT_EQ(flat.GetCost({3, 4}, {2, 4}), 1.0f);
    EXPECT_FLOAT_EQ(flat.GetCost({0, 0}, {0, 0}), 1.0f);
}

TEST(IPathCostProvider, kImpassableCost_IsNegative)
{
    EXPECT_LT(IPathCostProvider::kImpassableCost, 0.0f);
}

// ===========================================================================
// SquarePathGrid
// ===========================================================================

TEST(SquarePathGrid, Construction_AllCellsPassableByDefault)
{
    SquarePathGrid grid(4, 4);
    for (int y = 0; y < 4; ++y)
        for (int x = 0; x < 4; ++x)
            EXPECT_TRUE(grid.IsPassable({x, y})) << "Cell (" << x << "," << y << ") should be passable";
}

TEST(SquarePathGrid, SetPassable_MakesCellImpassable)
{
    SquarePathGrid grid(5, 5);
    grid.SetPassable({2, 2}, false);
    EXPECT_FALSE(grid.IsPassable({2, 2}));
}

TEST(SquarePathGrid, SetPassable_MakesCellPassableAgain)
{
    SquarePathGrid grid(5, 5);
    grid.SetPassable({1, 1}, false);
    EXPECT_FALSE(grid.IsPassable({1, 1}));
    grid.SetPassable({1, 1}, true);
    EXPECT_TRUE(grid.IsPassable({1, 1}));
}

TEST(SquarePathGrid, GetWidth_GetHeight_ReturnConstructorArgs)
{
    SquarePathGrid grid(7, 3);
    EXPECT_EQ(grid.GetWidth(),  7);
    EXPECT_EQ(grid.GetHeight(), 3);
}

TEST(SquarePathGrid, GetConnectivity_Returns4ConnectedWhenSet)
{
    SquarePathGrid grid(4, 4, SquareConnectivity::k4Connected);
    EXPECT_EQ(grid.GetConnectivity(), SquareConnectivity::k4Connected);
}

TEST(SquarePathGrid, GetConnectivity_Returns8ConnectedByDefault)
{
    SquarePathGrid grid(4, 4);
    EXPECT_EQ(grid.GetConnectivity(), SquareConnectivity::k8Connected);
}

TEST(SquarePathGrid, IsPassable_OutOfBoundsReturnsFalse)
{
    SquarePathGrid grid(3, 3);
    EXPECT_FALSE(grid.IsPassable({-1,  0}));
    EXPECT_FALSE(grid.IsPassable({ 3,  0}));
    EXPECT_FALSE(grid.IsPassable({ 0, -1}));
    EXPECT_FALSE(grid.IsPassable({ 0,  3}));
    EXPECT_FALSE(grid.IsPassable({10, 10}));
}

TEST(SquarePathGrid, GetNeighbours_4Connected_OpenGrid_ReturnsFour)
{
    // Centre cell (1,1) of a fully-open 3×3 grid — 4-connected
    SquarePathGrid grid(3, 3, SquareConnectivity::k4Connected);
    DynamicArrayC<CellCoord, 16> neighbours;
    grid.GetNeighbours({1, 1}, neighbours);
    EXPECT_EQ(neighbours.Size(), 4);
}

TEST(SquarePathGrid, GetNeighbours_8Connected_OpenGrid_ReturnsEight)
{
    // Centre cell (1,1) of a fully-open 3×3 grid — 8-connected
    SquarePathGrid grid(3, 3, SquareConnectivity::k8Connected);
    DynamicArrayC<CellCoord, 16> neighbours;
    grid.GetNeighbours({1, 1}, neighbours);
    EXPECT_EQ(neighbours.Size(), 8);
}

TEST(SquarePathGrid, GetNeighbours_EdgeCell_Returns2Neighbours)
{
    // Corner cell (0,0) of a 3×3 grid — 4-connected; only (1,0) and (0,1) are valid
    SquarePathGrid grid(3, 3, SquareConnectivity::k4Connected);
    DynamicArrayC<CellCoord, 16> neighbours;
    grid.GetNeighbours({0, 0}, neighbours);
    EXPECT_EQ(neighbours.Size(), 2);
}

TEST(SquarePathGrid, GetNeighbours_BlockedNeighbour_NotIncluded)
{
    // Centre cell (1,1) of a 3×3 grid — 4-connected; block the cell above (1,2)
    SquarePathGrid grid(3, 3, SquareConnectivity::k4Connected);
    grid.SetPassable({1, 2}, false);

    DynamicArrayC<CellCoord, 16> neighbours;
    grid.GetNeighbours({1, 1}, neighbours);

    // Blocked cell must not appear
    for (int i = 0; i < neighbours.Size(); ++i)
    {
        EXPECT_FALSE(neighbours[i] == CellCoord{1, 2})
            << "Blocked cell (1,2) should not be in neighbours";
    }
    // With one neighbour blocked: 4 - 1 = 3 valid neighbours
    EXPECT_EQ(neighbours.Size(), 3);
}

// ===========================================================================
// HexPathGrid
// ===========================================================================

TEST(HexPathGrid, Construction_AllCellsPassableByDefault)
{
    HexPathGrid grid(2);
    // Axial coords: all cells where max(|q|, |r|, |q+r|) <= radius are inside
    // Just spot-check a handful of in-bounds cells
    EXPECT_TRUE(grid.IsPassable({0,  0}));
    EXPECT_TRUE(grid.IsPassable({1,  0}));
    EXPECT_TRUE(grid.IsPassable({0,  1}));
    EXPECT_TRUE(grid.IsPassable({-1, 1}));
    EXPECT_TRUE(grid.IsPassable({2,  0}));
    EXPECT_TRUE(grid.IsPassable({0, -2}));
}

TEST(HexPathGrid, GetRadius_ReturnsConstructorArg)
{
    HexPathGrid grid(5);
    EXPECT_EQ(grid.GetRadius(), 5);
}

TEST(HexPathGrid, SetPassable_MakesCellImpassable)
{
    HexPathGrid grid(3);
    grid.SetPassable({1, 0}, false);
    EXPECT_FALSE(grid.IsPassable({1, 0}));
}

TEST(HexPathGrid, IsPassable_OutOfBoundsReturnsFalse)
{
    // Grid radius 2 — cells with hex cube distance > 2 are out of bounds
    HexPathGrid grid(2);
    EXPECT_FALSE(grid.IsPassable({ 3,  0}));
    EXPECT_FALSE(grid.IsPassable({ 0,  3}));
    EXPECT_FALSE(grid.IsPassable({-3,  0}));
    EXPECT_FALSE(grid.IsPassable({ 5,  5}));
}

TEST(HexPathGrid, GetNeighbours_CentreCell_Returns6Neighbours)
{
    // Radius 2, open grid: (0,0) has exactly 6 neighbours
    HexPathGrid grid(2);
    DynamicArrayC<CellCoord, 16> neighbours;
    grid.GetNeighbours({0, 0}, neighbours);
    EXPECT_EQ(neighbours.Size(), 6);
}

TEST(HexPathGrid, GetNeighbours_EdgeCell_FewerNeighbours)
{
    // A cell at the radius boundary (e.g. (2,0) with radius 2) has fewer than 6 valid neighbours
    HexPathGrid grid(2);
    DynamicArrayC<CellCoord, 16> neighbours;
    grid.GetNeighbours({2, 0}, neighbours);
    EXPECT_LT(neighbours.Size(), 6);
    EXPECT_GT(neighbours.Size(), 0);
}

// ===========================================================================
// FindPath (synchronous)
// ===========================================================================

TEST(FindPath, FindPath_SquareGrid_DirectPath_Succeeds)
{
    SquarePathGrid grid(5, 5);
    FlatCostProvider flat;
    PathResult result = FindPath(grid, {0, 0}, {4, 4}, flat);

    EXPECT_TRUE(result.success);
    EXPECT_GT(result.cells.Size(), 0u);
    EXPECT_TRUE(result.cells[0] == CellCoord{0, 0});
    EXPECT_TRUE(result.cells[result.cells.Size() - 1] == CellCoord{4, 4});
}

TEST(FindPath, FindPath_SquareGrid_PathAvoidsWall)
{
    // 5×5 grid with a horizontal wall at row y=2 except for the rightmost column
    //   . . . . .
    //   . . . . .
    //   X X X X .   <- wall row; gap at (4,2)
    //   . . . . .
    //   . . . . .
    SquarePathGrid grid(5, 5);
    for (int x = 0; x < 4; ++x)
        grid.SetPassable({x, 2}, false);

    FlatCostProvider flat;
    PathResult result = FindPath(grid, {0, 0}, {0, 4}, flat);

    EXPECT_TRUE(result.success);
    // Path must pass through the gap at x=4
    bool passedThroughGap = false;
    for (int i = 0; i < result.cells.Size(); ++i)
    {
        if (result.cells[i].x == 4 && result.cells[i].y == 2)
        {
            passedThroughGap = true;
            break;
        }
    }
    EXPECT_TRUE(passedThroughGap) << "Path should use the only gap in the wall at (4,2)";
}

TEST(FindPath, FindPath_SquareGrid_NoPath_ReturnsFalse)
{
    // 3×3 grid with destination completely surrounded by impassable cells
    SquarePathGrid grid(3, 3);
    // Completely wall off (2,2)
    grid.SetPassable({1, 2}, false);
    grid.SetPassable({2, 1}, false);
    // In 8-connected mode (2,2) can still be reached diagonally — use 4-connected
    SquarePathGrid grid4(3, 3, SquareConnectivity::k4Connected);
    grid4.SetPassable({1, 2}, false);
    grid4.SetPassable({2, 1}, false);

    FlatCostProvider flat;
    PathResult result = FindPath(grid4, {0, 0}, {2, 2}, flat);
    EXPECT_FALSE(result.success);
}

TEST(FindPath, FindPath_SquareGrid_SameStartAndEnd_ReturnsSingleCell)
{
    SquarePathGrid grid(5, 5);
    FlatCostProvider flat;
    PathResult result = FindPath(grid, {2, 2}, {2, 2}, flat);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.cells.Size(), 1u);
    EXPECT_TRUE(result.cells[0] == CellCoord{2, 2});
    EXPECT_FLOAT_EQ(result.totalCost, 0.0f);
}

TEST(FindPath, FindPath_SquareGrid_4Connected_PathIsCardinalOnly)
{
    SquarePathGrid grid(5, 5, SquareConnectivity::k4Connected);
    FlatCostProvider flat;
    PathResult result = FindPath(grid, {0, 0}, {4, 4}, flat);

    ASSERT_TRUE(result.success);
    ASSERT_GE(result.cells.Size(), 2u);

    for (int i = 1; i < result.cells.Size(); ++i)
    {
        const CellCoord& prev = result.cells[i - 1];
        const CellCoord& curr = result.cells[i];
        const int manhattanStep = std::abs(curr.x - prev.x) + std::abs(curr.y - prev.y);
        EXPECT_EQ(manhattanStep, 1)
            << "Step " << i << " from (" << prev.x << "," << prev.y
            << ") to (" << curr.x << "," << curr.y << ") is diagonal — not allowed in 4-connected";
    }
}

TEST(FindPath, FindPath_HexGrid_DirectPath_Succeeds)
{
    HexPathGrid grid(5);
    FlatCostProvider flat;
    PathResult result = FindPath(grid, {-4, 0}, {4, 0}, flat);

    EXPECT_TRUE(result.success);
    EXPECT_GT(result.cells.Size(), 0u);
    EXPECT_TRUE(result.cells[0] == CellCoord{-4, 0});
    EXPECT_TRUE(result.cells[result.cells.Size() - 1] == CellCoord{4, 0});
}

TEST(FindPath, FindPath_CostProvider_ImpassableCostBlocksEdge)
{
    // 5×5 grid; block cell (2,2) via cost provider while keeping grid passable
    SquarePathGrid grid(5, 5, SquareConnectivity::k4Connected);
    BlockCellCostProvider costs{{2, 2}};

    // Route from (0,0) to (4,4): must avoid (2,2)
    PathResult result = FindPath(grid, {0, 0}, {4, 4}, costs);

    EXPECT_TRUE(result.success);
    for (int i = 0; i < result.cells.Size(); ++i)
    {
        EXPECT_FALSE(result.cells[i] == CellCoord{2, 2})
            << "Path should avoid the impassable cell (2,2)";
    }
}

TEST(FindPath, FindPath_PathResult_TotalCost_NonZero)
{
    SquarePathGrid grid(5, 5);
    FlatCostProvider flat;
    PathResult result = FindPath(grid, {0, 0}, {4, 4}, flat);

    ASSERT_TRUE(result.success);
    EXPECT_GT(result.totalCost, 0.0f);
}

TEST(FindPath, FindPath_ToWorldPositions_ScalesCorrectly)
{
    // Build a simple 3-cell straight path: (0,0)->(1,0)->(2,0) in a 1D strip
    SquarePathGrid grid(3, 1, SquareConnectivity::k4Connected);
    FlatCostProvider flat;
    PathResult result = FindPath(grid, {0, 0}, {2, 0}, flat);

    ASSERT_TRUE(result.success);
    ASSERT_GE(result.cells.Size(), 2u);

    const float cellSize = 10.0f;
    DynamicArrayC<Dia::Maths::Vector2D, 256> worldPos;
    result.ToWorldPositions(cellSize, worldPos);

    ASSERT_EQ(worldPos.Size(), result.cells.Size());

    // First cell should map to (0*10, 0*10) = (0, 0)
    EXPECT_FLOAT_EQ(worldPos[0].X(), 0.0f);
    EXPECT_FLOAT_EQ(worldPos[0].Y(), 0.0f);

    // Last cell coordinates scaled by cellSize
    const int lastIdx = result.cells.Size() - 1;
    const CellCoord lastCell = result.cells[lastIdx];
    EXPECT_FLOAT_EQ(worldPos[lastIdx].X(), static_cast<float>(lastCell.x) * cellSize);
    EXPECT_FLOAT_EQ(worldPos[lastIdx].Y(), static_cast<float>(lastCell.y) * cellSize);
}

// ===========================================================================
// PathfindingSystem (async)
// ===========================================================================

TEST(PathfindingSystem, PathfindingSystem_RequestPath_IncrementsPendingCount)
{
    SquarePathGrid grid(5, 5);
    PathfindingSystem<SquarePathGrid> system(grid);

    FlatCostProvider flat;
    MockPathResultObserver observer;

    EXPECT_EQ(system.GetPendingCount(), 0);

    PathRequest req;
    req.id       = Dia::Core::StringCRC{"req1"};
    req.from     = {0, 0};
    req.to       = {4, 4};
    req.costs    = &flat;
    req.observer = &observer;

    system.RequestPath(req);
    EXPECT_EQ(system.GetPendingCount(), 1);

    PathRequest req2;
    req2.id       = Dia::Core::StringCRC{"req2"};
    req2.from     = {0, 0};
    req2.to       = {3, 3};
    req2.costs    = &flat;
    req2.observer = &observer;

    system.RequestPath(req2);
    EXPECT_EQ(system.GetPendingCount(), 2);
}

TEST(PathfindingSystem, PathfindingSystem_Update_ProcessesRequest_CallsOnPathFound)
{
    SquarePathGrid grid(5, 5);
    PathfindingSystem<SquarePathGrid> system(grid);

    FlatCostProvider flat;
    MockPathResultObserver observer;

    PathRequest req;
    req.id       = Dia::Core::StringCRC{"req_found"};
    req.from     = {0, 0};
    req.to       = {4, 4};
    req.costs    = &flat;
    req.observer = &observer;

    system.RequestPath(req);
    system.Update(100.0f);

    EXPECT_EQ(observer.foundCount,  1);
    EXPECT_EQ(observer.failedCount, 0);
}

TEST(PathfindingSystem, PathfindingSystem_Update_FailedPath_CallsOnPathFailed)
{
    // Isolate destination with 4-connected topology so it is unreachable
    SquarePathGrid grid(3, 3, SquareConnectivity::k4Connected);
    grid.SetPassable({1, 2}, false);
    grid.SetPassable({2, 1}, false);

    PathfindingSystem<SquarePathGrid> system(grid);

    FlatCostProvider flat;
    MockPathResultObserver observer;

    PathRequest req;
    req.id       = Dia::Core::StringCRC{"req_fail"};
    req.from     = {0, 0};
    req.to       = {2, 2};
    req.costs    = &flat;
    req.observer = &observer;

    system.RequestPath(req);
    system.Update(100.0f);

    EXPECT_EQ(observer.foundCount,  0);
    EXPECT_EQ(observer.failedCount, 1);
}

TEST(PathfindingSystem, PathfindingSystem_CancelRequest_RemovesFromQueue)
{
    SquarePathGrid grid(5, 5);
    PathfindingSystem<SquarePathGrid> system(grid);

    FlatCostProvider flat;
    MockPathResultObserver observer;

    PathRequest req;
    req.id       = Dia::Core::StringCRC{"req_cancel"};
    req.from     = {0, 0};
    req.to       = {4, 4};
    req.costs    = &flat;
    req.observer = &observer;

    system.RequestPath(req);
    EXPECT_EQ(system.GetPendingCount(), 1);

    system.CancelRequest(Dia::Core::StringCRC{"req_cancel"});
    EXPECT_EQ(system.GetPendingCount(), 0);

    // Update should not invoke any callbacks
    system.Update(100.0f);
    EXPECT_EQ(observer.foundCount,  0);
    EXPECT_EQ(observer.failedCount, 0);
}

TEST(PathfindingSystem, PathfindingSystem_GetPendingCount_DecreasesAfterUpdate)
{
    SquarePathGrid grid(5, 5);
    PathfindingSystem<SquarePathGrid> system(grid);

    FlatCostProvider flat;
    MockPathResultObserver observer;

    PathRequest req1;
    req1.id       = Dia::Core::StringCRC{"r1"};
    req1.from     = {0, 0};
    req1.to       = {4, 4};
    req1.costs    = &flat;
    req1.observer = &observer;

    PathRequest req2;
    req2.id       = Dia::Core::StringCRC{"r2"};
    req2.from     = {1, 1};
    req2.to       = {3, 3};
    req2.costs    = &flat;
    req2.observer = &observer;

    system.RequestPath(req1);
    system.RequestPath(req2);
    EXPECT_EQ(system.GetPendingCount(), 2);

    system.Update(100.0f);
    EXPECT_EQ(system.GetPendingCount(), 0);
}

TEST(PathfindingSystem, PathfindingSystem_Update_ReturnedId_MatchesRequest)
{
    SquarePathGrid grid(5, 5);
    PathfindingSystem<SquarePathGrid> system(grid);

    FlatCostProvider flat;
    MockPathResultObserver observer;

    const PathRequestId expectedId = Dia::Core::StringCRC{"req_id_check"};

    PathRequest req;
    req.id       = expectedId;
    req.from     = {0, 0};
    req.to       = {4, 4};
    req.costs    = &flat;
    req.observer = &observer;

    const PathRequestId returnedId = system.RequestPath(req);
    EXPECT_TRUE(returnedId == expectedId);

    system.Update(100.0f);
    EXPECT_EQ(observer.foundCount, 1);
    EXPECT_TRUE(observer.lastFoundId == expectedId);
}

// ===========================================================================
// Testing utilities
// ===========================================================================

TEST(TestHelpers, TestHelpers_AssertPathFound_PassesOnSuccessfulResult)
{
    SquarePathGrid grid(5, 5);
    FlatCostProvider flat;
    PathResult result = FindPath(grid, {0, 0}, {4, 4}, flat);

    ASSERT_TRUE(result.success);
    // AssertPathFound uses DIA_ASSERT; in a passing case it should not abort
    Testing::AssertPathFound(result);
}

TEST(TestHelpers, TestHelpers_MockCostProvider_SetCost_ChangesReturnedCost)
{
    Testing::MockCostProvider mock;
    EXPECT_FLOAT_EQ(mock.GetCost({0, 0}, {1, 0}), 1.0f);

    mock.SetCost(5.0f);
    EXPECT_FLOAT_EQ(mock.GetCost({0, 0}, {1, 0}), 5.0f);
}

TEST(TestHelpers, TestHelpers_MockCostProvider_SetImpassable_ReturnsImpassableSentinel)
{
    Testing::MockCostProvider mock;
    mock.SetImpassable();
    EXPECT_FLOAT_EQ(mock.GetCost({0, 0}, {1, 0}), IPathCostProvider::kImpassableCost);
}

TEST(TestHelpers, TestHelpers_AssertPathCells_MatchesExpectedCells)
{
    // 1×3 horizontal strip: only possible path is (0,0)->(1,0)->(2,0)
    SquarePathGrid grid(3, 1, SquareConnectivity::k4Connected);
    FlatCostProvider flat;
    PathResult result = FindPath(grid, {0, 0}, {2, 0}, flat);

    ASSERT_TRUE(result.success);

    const CellCoord expectedCells[] = {{0, 0}, {1, 0}, {2, 0}};
    // AssertPathCells uses DIA_ASSERT internally; if the cells match it will not abort
    Testing::AssertPathCells(result, expectedCells, 3u);
}
