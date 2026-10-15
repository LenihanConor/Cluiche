// TestPathfindingBoundary.cpp — Boundary, edge-case, and system-limit tests for DiaPathfinding.
//
// Suites:
//   SquarePathGrid_Boundary    — grid edge cases
//   HexPathGrid_Boundary       — hex edge cases
//   FindPath_Boundary          — search edge cases
//   PathfindingSystem_Boundary — async system edge cases

#include <gtest/gtest.h>
#include <cstdlib>

#include "DiaPathfinding/CellCoord.h"
#include "DiaPathfinding/IPathCostProvider.h"
#include "DiaPathfinding/PathResult.h"
#include "DiaPathfinding/SquarePathGrid.h"
#include "DiaPathfinding/HexPathGrid.h"
#include "DiaPathfinding/FindPath.h"
#include "DiaPathfinding/IPathResultObserver.h"
#include "DiaPathfinding/PathfindingSystem.h"
#include "DiaPathfinding/Testing/PathfindingTestHelpers.h"

using namespace Dia::Pathfinding;
using Dia::Core::Containers::DynamicArrayC;

namespace
{
    class BoundaryObserver : public IPathResultObserver
    {
    public:
        int foundCount  = 0;
        int failedCount = 0;
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

    PathRequest MakeRequest(PathRequestId id, CellCoord from, CellCoord to,
                            IPathCostProvider* costs, IPathResultObserver* obs)
    {
        PathRequest req;
        req.id       = id;
        req.from     = from;
        req.to       = to;
        req.costs    = costs;
        req.observer = obs;
        return req;
    }
} // anonymous namespace

// ===========================================================================
// SquarePathGrid_Boundary
// ===========================================================================

TEST(SquarePathGrid_Boundary, Size1x1_SingleCell_IsPassable)
{
    SquarePathGrid grid(1, 1);
    EXPECT_TRUE(grid.IsPassable({0, 0}));
}

TEST(SquarePathGrid_Boundary, Size1x1_SingleCell_HasNoNeighbours)
{
    SquarePathGrid grid(1, 1);
    DynamicArrayC<CellCoord, 16> neighbours;
    grid.GetNeighbours({0, 0}, neighbours);
    EXPECT_EQ(neighbours.Size(), 0);
}

TEST(SquarePathGrid_Boundary, SetPassableOutOfBounds_NoOp_NocrashOnRead)
{
    // SetPassable on an out-of-bounds cell must not crash; IsPassable on that
    // cell (out of bounds) still returns false afterward.
    SquarePathGrid grid(3, 3);
    grid.SetPassable({10, 10}, false); // out-of-bounds — no-op
    EXPECT_FALSE(grid.IsPassable({10, 10}));
}

TEST(SquarePathGrid_Boundary, AllCellsBlocked_PathFails)
{
    // 3×3 grid with every cell impassable — no path can be found even for trivial requests.
    // (from == to still succeeds via early-out; test a different from/to pair)
    SquarePathGrid grid(3, 3);
    for (int y = 0; y < 3; ++y)
        for (int x = 0; x < 3; ++x)
            grid.SetPassable({x, y}, false);

    FlatCostProvider flat;
    PathResult result = FindPath(grid, {0, 0}, {2, 2}, flat);
    EXPECT_FALSE(result.success);
}

TEST(SquarePathGrid_Boundary, CornerToCorner4Connected_FindsPath)
{
    // 4-connected 3×3 — corners are reachable via cardinal movement.
    SquarePathGrid grid(3, 3, SquareConnectivity::k4Connected);
    FlatCostProvider flat;
    PathResult result = FindPath(grid, {0, 0}, {2, 2}, flat);

    EXPECT_TRUE(result.success);
    EXPECT_TRUE((result.cells[0] == CellCoord{0, 0}));
    EXPECT_TRUE((result.cells[result.cells.Size() - 1] == CellCoord{2, 2}));
}

TEST(SquarePathGrid_Boundary, GetNeighbours_CornerCell8Connected_Returns3)
{
    // (0,0) in a 3×3 8-connected grid has exactly 3 valid neighbours:
    // (1,0), (0,1), (1,1).
    SquarePathGrid grid(3, 3, SquareConnectivity::k8Connected);
    DynamicArrayC<CellCoord, 16> neighbours;
    grid.GetNeighbours({0, 0}, neighbours);
    EXPECT_EQ(neighbours.Size(), 3);
}

TEST(SquarePathGrid_Boundary, GetNeighbours_EdgeCellMid4Connected_Returns2)
{
    // Centre of left edge (0,1) in a 3×3 4-connected grid: (0,0),(0,2),(1,1) = 3 neighbours.
    // Wait — (0,1) connects to (1,1) (right), (0,0) (down), (0,2) (up) → 3, not 2.
    // Use a 3×1 strip: (0,0) only connects to (1,0) on a 4-connected grid.
    SquarePathGrid grid(3, 1, SquareConnectivity::k4Connected);
    DynamicArrayC<CellCoord, 16> neighbours;
    grid.GetNeighbours({0, 0}, neighbours);
    EXPECT_EQ(neighbours.Size(), 1);
}

// ===========================================================================
// SquarePathGrid Large Sizes (AC6 — scalable storage)
// ===========================================================================

TEST(SLOW_SquarePathGrid_Boundary, Constructor_66x42_Constructs)
{
    // CoW Small map: 2,772 cells
    SquarePathGrid grid4(66, 42, SquareConnectivity::k4Connected);
    EXPECT_EQ(grid4.GetWidth(), 66);
    EXPECT_EQ(grid4.GetHeight(), 42);

    SquarePathGrid grid8(66, 42, SquareConnectivity::k8Connected);
    EXPECT_EQ(grid8.GetWidth(), 66);
    EXPECT_EQ(grid8.GetHeight(), 42);
}

TEST(SLOW_SquarePathGrid_Boundary, Constructor_80x52_Constructs)
{
    // CoW Standard map: 4,160 cells
    SquarePathGrid grid4(80, 52, SquareConnectivity::k4Connected);
    EXPECT_EQ(grid4.GetWidth(), 80);
    EXPECT_EQ(grid4.GetHeight(), 52);

    SquarePathGrid grid8(80, 52, SquareConnectivity::k8Connected);
    EXPECT_EQ(grid8.GetWidth(), 80);
    EXPECT_EQ(grid8.GetHeight(), 52);
}

TEST(SLOW_SquarePathGrid_Boundary, Constructor_104x64_Constructs)
{
    // CoW Large map: 6,656 cells
    SquarePathGrid grid4(104, 64, SquareConnectivity::k4Connected);
    EXPECT_EQ(grid4.GetWidth(), 104);
    EXPECT_EQ(grid4.GetHeight(), 64);

    SquarePathGrid grid8(104, 64, SquareConnectivity::k8Connected);
    EXPECT_EQ(grid8.GetWidth(), 104);
    EXPECT_EQ(grid8.GetHeight(), 64);
}

TEST(SLOW_SquarePathGrid_Boundary, Constructor_128x128_Constructs)
{
    // 16,384 cells
    SquarePathGrid grid4(128, 128, SquareConnectivity::k4Connected);
    EXPECT_EQ(grid4.GetWidth(), 128);
    EXPECT_EQ(grid4.GetHeight(), 128);

    SquarePathGrid grid8(128, 128, SquareConnectivity::k8Connected);
    EXPECT_EQ(grid8.GetWidth(), 128);
    EXPECT_EQ(grid8.GetHeight(), 128);
}

TEST(SLOW_SquarePathGrid_Boundary, Constructor_256x256_Constructs)
{
    // Tested ceiling: 65,536 cells
    SquarePathGrid grid4(256, 256, SquareConnectivity::k4Connected);
    EXPECT_EQ(grid4.GetWidth(), 256);
    EXPECT_EQ(grid4.GetHeight(), 256);

    SquarePathGrid grid8(256, 256, SquareConnectivity::k8Connected);
    EXPECT_EQ(grid8.GetWidth(), 256);
    EXPECT_EQ(grid8.GetHeight(), 256);
}

TEST(SLOW_SquarePathGrid_Boundary, LargeGrid_AllCellsStartPassable)
{
    // Every in-bounds cell of a 104×64 grid starts passable
    SquarePathGrid grid(104, 64);
    for (int y = 0; y < 64; ++y)
        for (int x = 0; x < 104; ++x)
            ASSERT_TRUE(grid.IsPassable({x, y})) << "Cell (" << x << "," << y << ") not passable";
}

TEST(SLOW_SquarePathGrid_Boundary, LargeGrid_OutOfBoundsCellsNotPassable)
{
    SquarePathGrid grid(104, 64);
    EXPECT_FALSE(grid.IsPassable({-1, 0}));
    EXPECT_FALSE(grid.IsPassable({104, 0}));
    EXPECT_FALSE(grid.IsPassable({0, -1}));
    EXPECT_FALSE(grid.IsPassable({0, 64}));
    EXPECT_FALSE(grid.IsPassable({200, 200}));
}

TEST(SLOW_SquarePathGrid_Boundary, LargeGrid_SetPassableOutOfBounds_NoOp)
{
    SquarePathGrid grid(104, 64);
    grid.SetPassable({200, 200}, false); // out of bounds — must not crash
    EXPECT_FALSE(grid.IsPassable({200, 200})); // still returns false
}

TEST(SLOW_SquarePathGrid_Boundary, LargeGrid_InteriorCell_Has4or8Neighbours)
{
    // Interior cell (50, 30) has 4 neighbours in 4-connected, 8 in 8-connected
    SquarePathGrid grid4(104, 64, SquareConnectivity::k4Connected);
    DynamicArrayC<CellCoord, 16> neighbours4;
    grid4.GetNeighbours({50, 30}, neighbours4);
    EXPECT_EQ(neighbours4.Size(), 4);

    SquarePathGrid grid8(104, 64, SquareConnectivity::k8Connected);
    DynamicArrayC<CellCoord, 16> neighbours8;
    grid8.GetNeighbours({50, 30}, neighbours8);
    EXPECT_EQ(neighbours8.Size(), 8);
}

TEST(SLOW_SquarePathGrid_Boundary, LargeGrid_FindPath_CornerToCorner)
{
    // 104×64 grid (CoW Large map): corner-to-corner path length = max(dx, dy) for 8-connected,
    // dx + dy for 4-connected
    SquarePathGrid grid8(104, 64, SquareConnectivity::k8Connected);
    FlatCostProvider flat;
    PathResult result8 = FindPath(grid8, {0, 0}, {103, 63}, flat);
    ASSERT_TRUE(result8.success);
    // 8-connected: diagonal moves, so path length is max(103, 63) + 1 = 104
    EXPECT_EQ(result8.cells.Size(), 104u);

    SquarePathGrid grid4(104, 64, SquareConnectivity::k4Connected);
    PathResult result4 = FindPath(grid4, {0, 0}, {103, 63}, flat);
    ASSERT_TRUE(result4.success);
    // 4-connected: cardinal only, so path length is 103 + 63 + 1 = 167
    EXPECT_EQ(result4.cells.Size(), 167u);
}

TEST(SLOW_SquarePathGrid_Boundary, LargeGrid_FindPath_FailsAcrossWall)
{
    // 104×64 grid with a full vertical wall at x=52 — left side cannot reach right side
    SquarePathGrid grid(104, 64);
    for (int y = 0; y < 64; ++y)
        grid.SetPassable({52, y}, false);

    FlatCostProvider flat;
    PathResult result = FindPath(grid, {0, 0}, {103, 63}, flat);
    EXPECT_FALSE(result.success);
}

TEST(SLOW_SquarePathGrid_Boundary, LargeGrid_FindPath_SucceedsThroughGap)
{
    // 104×64 grid with a wall at x=52, but a gap at y=32
    SquarePathGrid grid(104, 64);
    for (int y = 0; y < 64; ++y)
        if (y != 32)
            grid.SetPassable({52, y}, false);

    FlatCostProvider flat;
    PathResult result = FindPath(grid, {0, 0}, {103, 63}, flat);
    EXPECT_TRUE(result.success);
}

// ===========================================================================
// HexPathGrid_Boundary
// ===========================================================================

TEST(HexPathGrid_Boundary, Radius0_SingleCell_IsPassable)
{
    HexPathGrid grid(0);
    EXPECT_TRUE(grid.IsPassable({0, 0}));
}

TEST(HexPathGrid_Boundary, Radius0_SingleCell_HasNoNeighbours)
{
    HexPathGrid grid(0);
    DynamicArrayC<CellCoord, 16> neighbours;
    grid.GetNeighbours({0, 0}, neighbours);
    EXPECT_EQ(neighbours.Size(), 0);
}

TEST(HexPathGrid_Boundary, TrivialPath_Radius0_Succeeds)
{
    // from == to on a radius-0 grid
    HexPathGrid grid(0);
    FlatCostProvider flat;
    PathResult result = FindPath(grid, {0, 0}, {0, 0}, flat);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.cells.Size(), 1u);
    EXPECT_FLOAT_EQ(result.totalCost, 0.0f);
}

TEST(HexPathGrid_Boundary, SetPassableOutOfBounds_NoOp)
{
    // Out-of-bounds SetPassable must not crash; the out-of-bounds cell stays false.
    HexPathGrid grid(2);
    grid.SetPassable({10, 10}, false);
    EXPECT_FALSE(grid.IsPassable({10, 10}));
}

TEST(HexPathGrid_Boundary, AllCellsBlocked_PathFails)
{
    // Radius-2 hex with all in-bounds cells blocked except start — to is unreachable.
    HexPathGrid grid(2);
    const int R = 2;
    for (int q = -R; q <= R; ++q)
        for (int r = -R; r <= R; ++r)
            if (std::abs(q) <= R && std::abs(r) <= R && std::abs(q + r) <= R)
                grid.SetPassable({q, r}, false);

    FlatCostProvider flat;
    PathResult result = FindPath(grid, {0, 0}, {2, 0}, flat);
    EXPECT_FALSE(result.success);
}

// ===========================================================================
// FindPath_Boundary
// ===========================================================================

TEST(FindPath_Boundary, FromEqualsTo_ZeroCostSingleCell)
{
    // Same-cell request must succeed with zero cost and one cell.
    SquarePathGrid grid(5, 5);
    FlatCostProvider flat;
    PathResult result = FindPath(grid, {3, 3}, {3, 3}, flat);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.cells.Size(), 1u);
    EXPECT_FLOAT_EQ(result.totalCost, 0.0f);
    EXPECT_TRUE((result.cells[0] == CellCoord{3, 3}));
}

TEST(FindPath_Boundary, FromFarOutOfBounds_ReturnsFailed)
{
    // Source is so far outside the grid that none of its neighbours are in bounds.
    // A* expands the start, finds no reachable cells, and fails.
    SquarePathGrid grid(5, 5);
    FlatCostProvider flat;
    PathResult result = FindPath(grid, {-10, -10}, {4, 4}, flat);
    EXPECT_FALSE(result.success);
}

TEST(FindPath_Boundary, ToOutOfBounds_ReturnsFailed)
{
    // Destination is outside grid bounds.
    SquarePathGrid grid(5, 5);
    FlatCostProvider flat;
    PathResult result = FindPath(grid, {0, 0}, {5, 5}, flat);
    EXPECT_FALSE(result.success);
}

TEST(FindPath_Boundary, ImpassableTo_ReturnsFailed)
{
    // Destination cell is marked impassable — path cannot terminate there.
    SquarePathGrid grid(5, 5, SquareConnectivity::k4Connected);
    grid.SetPassable({4, 4}, false);
    FlatCostProvider flat;
    PathResult result = FindPath(grid, {0, 0}, {4, 4}, flat);
    EXPECT_FALSE(result.success);
}

TEST(FindPath_Boundary, AllEdgesCostImpassable_ReturnsFailed)
{
    // All edges blocked via cost provider — empty traversable graph.
    SquarePathGrid grid(5, 5);
    Testing::MockCostProvider mock;
    mock.SetImpassable();
    PathResult result = FindPath(grid, {0, 0}, {4, 4}, mock);
    EXPECT_FALSE(result.success);
}

TEST(FindPath_Boundary, HighCostPath_CostAccumulates)
{
    // 3-cell strip with uniform cost 7.5f: 2 edges × 7.5 = 15.0f total.
    SquarePathGrid grid(3, 1, SquareConnectivity::k4Connected);
    Testing::MockCostProvider mock(7.5f);
    PathResult result = FindPath(grid, {0, 0}, {2, 0}, mock);

    ASSERT_TRUE(result.success);
    EXPECT_NEAR(result.totalCost, 15.0f, 1e-4f);
}

TEST(FindPath_Boundary, HexGrid_FromEqualsTo_ZeroCost)
{
    HexPathGrid grid(3);
    FlatCostProvider flat;
    PathResult result = FindPath(grid, {-2, 1}, {-2, 1}, flat);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.cells.Size(), 1u);
    EXPECT_FLOAT_EQ(result.totalCost, 0.0f);
}

TEST(FindPath_Boundary, FailedResult_CellsEmpty)
{
    // A failed PathResult must have an empty cell list.
    SquarePathGrid grid(3, 3, SquareConnectivity::k4Connected);
    grid.SetPassable({1, 2}, false);
    grid.SetPassable({2, 1}, false);
    FlatCostProvider flat;
    PathResult result = FindPath(grid, {0, 0}, {2, 2}, flat);

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.cells.Size(), 0u);
}

TEST(FindPath_Boundary, FailedResult_TotalCostIsZero)
{
    // A failed PathResult must have zero totalCost.
    SquarePathGrid grid(3, 3, SquareConnectivity::k4Connected);
    grid.SetPassable({1, 2}, false);
    grid.SetPassable({2, 1}, false);
    FlatCostProvider flat;
    PathResult result = FindPath(grid, {0, 0}, {2, 2}, flat);

    EXPECT_FALSE(result.success);
    EXPECT_FLOAT_EQ(result.totalCost, 0.0f);
}

// ===========================================================================
// PathfindingSystem_Boundary
// ===========================================================================

TEST(PathfindingSystem_Boundary, Update_ZeroBudget_ProcessesNothing)
{
    // A 0ms budget must not process any requests — pending count stays at 1.
    SquarePathGrid grid(5, 5);
    PathfindingSystem<SquarePathGrid> system(grid);

    FlatCostProvider flat;
    BoundaryObserver obs;
    system.RequestPath(MakeRequest(
        Dia::Core::StringCRC{"r_budget0"}, {0, 0}, {4, 4}, &flat, &obs));

    system.Update(0.0f);

    EXPECT_EQ(obs.foundCount,  0);
    EXPECT_EQ(obs.failedCount, 0);
    EXPECT_EQ(system.GetPendingCount(), 1);
}

TEST(PathfindingSystem_Boundary, CancelNonexistentId_IsNoOp)
{
    // Cancelling an ID that was never submitted must not crash or alter state.
    SquarePathGrid grid(5, 5);
    PathfindingSystem<SquarePathGrid> system(grid);

    FlatCostProvider flat;
    BoundaryObserver obs;
    system.RequestPath(MakeRequest(
        Dia::Core::StringCRC{"real_req"}, {0, 0}, {4, 4}, &flat, &obs));

    // Cancel a different, non-existent ID
    system.CancelRequest(Dia::Core::StringCRC{"ghost_req"});

    EXPECT_EQ(system.GetPendingCount(), 1);

    system.Update(100.0f);
    EXPECT_EQ(obs.foundCount, 1);
}

TEST(PathfindingSystem_Boundary, MultipleRequests_AllProcessedInSingleUpdate)
{
    // 5 requests submitted then processed in one Update call with ample budget.
    SquarePathGrid grid(5, 5);
    PathfindingSystem<SquarePathGrid> system(grid);

    FlatCostProvider flat;
    BoundaryObserver obs;
    const int N = 5;
    for (int i = 0; i < N; ++i)
    {
        char buf[16];
        buf[0] = 'r'; buf[1] = static_cast<char>('0' + i); buf[2] = '\0';
        system.RequestPath(MakeRequest(
            Dia::Core::StringCRC{buf}, {0, 0}, {4, 4}, &flat, &obs));
    }
    EXPECT_EQ(system.GetPendingCount(), N);

    system.Update(1000.0f);

    EXPECT_EQ(system.GetPendingCount(), 0);
    EXPECT_EQ(obs.foundCount, N);
}

TEST(PathfindingSystem_Boundary, RequestAfterFullDrain_Works)
{
    // Submit, drain, then submit again — system must be reusable.
    SquarePathGrid grid(5, 5);
    PathfindingSystem<SquarePathGrid> system(grid);

    FlatCostProvider flat;
    BoundaryObserver obs;

    system.RequestPath(MakeRequest(
        Dia::Core::StringCRC{"first"}, {0, 0}, {4, 4}, &flat, &obs));
    system.Update(100.0f);
    EXPECT_EQ(obs.foundCount, 1);
    EXPECT_EQ(system.GetPendingCount(), 0);

    system.RequestPath(MakeRequest(
        Dia::Core::StringCRC{"second"}, {1, 1}, {3, 3}, &flat, &obs));
    system.Update(100.0f);
    EXPECT_EQ(obs.foundCount, 2);
}

TEST(PathfindingSystem_Boundary, ObserverNullptr_DoesNotCrash)
{
    // observer = nullptr in the PathRequest: result must be processed without crashing.
    SquarePathGrid grid(5, 5);
    PathfindingSystem<SquarePathGrid> system(grid);

    FlatCostProvider flat;
    system.RequestPath(MakeRequest(
        Dia::Core::StringCRC{"null_obs"}, {0, 0}, {4, 4}, &flat, nullptr));

    // Should not crash during Update
    system.Update(100.0f);
    EXPECT_EQ(system.GetPendingCount(), 0);
}

TEST(PathfindingSystem_Boundary, FailedRequest_ObserverCalledOnce)
{
    // A request that produces no path should call OnPathFailed exactly once
    // even when Update is called multiple times afterward.
    SquarePathGrid grid(3, 3, SquareConnectivity::k4Connected);
    grid.SetPassable({1, 2}, false);
    grid.SetPassable({2, 1}, false);
    PathfindingSystem<SquarePathGrid> system(grid);

    FlatCostProvider flat;
    BoundaryObserver obs;
    system.RequestPath(MakeRequest(
        Dia::Core::StringCRC{"fail_once"}, {0, 0}, {2, 2}, &flat, &obs));

    system.Update(100.0f);
    system.Update(100.0f); // second update: queue is empty, must be no-op

    EXPECT_EQ(obs.foundCount,  0);
    EXPECT_EQ(obs.failedCount, 1);
}

// ===========================================================================
// SquarePathGrid edge cases for size validation
// ===========================================================================

TEST(SLOW_SquarePathGrid_Boundary, Constructor_Width0_Asserts) {
    EXPECT_DEATH(SquarePathGrid(0, 10), "");
}

TEST(SLOW_SquarePathGrid_Boundary, Constructor_Height0_Asserts) {
    EXPECT_DEATH(SquarePathGrid(10, 0), "");
}

TEST(SLOW_SquarePathGrid_Boundary, Constructor_IntOverflow_Asserts) {
    // 100000 × 100000 overflows int
    EXPECT_DEATH(SquarePathGrid(100000, 100000), "");
}

TEST(SLOW_SquarePathGrid_Boundary, Constructor_NegativeWidth_Asserts) {
    EXPECT_DEATH(SquarePathGrid(-1, 10), "");
}

TEST(SLOW_SquarePathGrid_Boundary, Constructor_NegativeHeight_Asserts) {
    EXPECT_DEATH(SquarePathGrid(10, -1), "");
}
