// TestPathfindingSystemBudgetAdapter.cpp — GoogleTest coverage for
// PathfindingSystemBudgetAdapter (Task 4.5, DiaSimTime migration).
//
// Confirms the adapter forwards UpdateBudgeted() to the wrapped
// PathfindingSystem::Update() and reports the expected priority.

#include <gtest/gtest.h>

#include "DiaPathfinding/PathfindingSystemBudgetAdapter.h"
#include "DiaPathfinding/SquarePathGrid.h"
#include "DiaPathfinding/IPathCostProvider.h"
#include "DiaPathfinding/IPathResultObserver.h"
#include <DiaCore/CRC/StringCRC.h>
#include <DiaSimTime/SimTimePriority.h>
#include <DiaSimTime/SimTimeBudget.h>

using namespace Dia::Pathfinding;

namespace
{
    class RecordingObserver : public IPathResultObserver
    {
    public:
        int foundCount  = 0;
        int failedCount = 0;

        void OnPathFound(PathRequestId /*id*/, const PathResult& /*result*/) override { ++foundCount; }
        void OnPathFailed(PathRequestId /*id*/) override { ++failedCount; }
    };
} // namespace

TEST(DiaPathfinding_BudgetAdapter, GetPriority_ReportsBackground)
{
    SquarePathGrid grid(4, 4);
    PathfindingSystem<SquarePathGrid> system(grid);
    PathfindingSystemBudgetAdapter<SquarePathGrid> adapter(system, Dia::Core::StringCRC("pathfinding-system"));

    EXPECT_EQ(adapter.GetPriority(), Dia::SimTime::SimTimePriority::kBackground);
}

TEST(DiaPathfinding_BudgetAdapter, GetSystemId_ReturnsConstructedId)
{
    SquarePathGrid grid(4, 4);
    PathfindingSystem<SquarePathGrid> system(grid);
    const Dia::Core::StringCRC id("pathfinding-system");
    PathfindingSystemBudgetAdapter<SquarePathGrid> adapter(system, id);

    EXPECT_EQ(adapter.GetSystemId(), id);
}

TEST(DiaPathfinding_BudgetAdapter, UpdateBudgeted_ForwardsToWrappedSystem_DrainsQueue)
{
    SquarePathGrid grid(4, 4);
    PathfindingSystem<SquarePathGrid> system(grid);
    PathfindingSystemBudgetAdapter<SquarePathGrid> adapter(system, Dia::Core::StringCRC("pathfinding-system"));

    FlatCostProvider costs;
    RecordingObserver observer;

    PathRequest request;
    request.id       = Dia::Core::StringCRC("req-1");
    request.from     = CellCoord{0, 0};
    request.to       = CellCoord{3, 3};
    request.costs    = &costs;
    request.observer = &observer;

    system.RequestPath(request);
    EXPECT_EQ(system.GetPendingCount(), 1);

    adapter.UpdateBudgeted(9999.0f);

    EXPECT_EQ(system.GetPendingCount(), 0);
    EXPECT_EQ(observer.foundCount, 1);
}

TEST(DiaPathfinding_BudgetAdapter, RegisteredWithSimTimeBudget_RunIfCapacity_RunsSystem)
{
    SquarePathGrid grid(4, 4);
    PathfindingSystem<SquarePathGrid> system(grid);
    PathfindingSystemBudgetAdapter<SquarePathGrid> adapter(system, Dia::Core::StringCRC("pathfinding-system"));

    FlatCostProvider costs;
    RecordingObserver observer;

    PathRequest request;
    request.id       = Dia::Core::StringCRC("req-1");
    request.from     = CellCoord{0, 0};
    request.to       = CellCoord{3, 3};
    request.costs    = &costs;
    request.observer = &observer;
    system.RequestPath(request);

    Dia::SimTime::SimTimeContext ctx{
        Dia::Core::TimeAbsolute::Zero(), Dia::Core::TimeRelative::Zero(), 0, 1.0f, false};

    Dia::SimTime::SimTimeBudget budget;
    budget.Register(&adapter);
    budget.AllocateTick(ctx);

    EXPECT_TRUE(budget.RunIfCapacity(&adapter));
    EXPECT_EQ(system.GetPendingCount(), 0);
    EXPECT_EQ(observer.foundCount, 1);

    budget.Unregister(&adapter);
}
