// Suites: FlowFieldInvariant, FlowFieldDeterminism
// Property tests over random inputs (fixed seed for reproducibility).

#include <gtest/gtest.h>
#include <DiaFlowField/ComputeFlowField.h>
#include <DiaFlowField/Testing/FlowFieldTestHelpers.h>
#include <DiaPathfinding/IPathCostProvider.h>
#include <DiaMaths/Vector/Vector2D.h>
#include <cmath>
#include <random>

using namespace Dia::FlowField;
using Dia::Pathfinding::CellCoord;
using Dia::Pathfinding::FlatCostProvider;
using Dia::Maths::Vector2D;
using namespace Dia::FlowField::Testing;

// Every reachable non-goal cell must have a direction vector of magnitude ≈ 1.
// Tested over 100 different goal positions on the 8×8 mock grid.
TEST(FlowFieldInvariant, ReachableNonGoalCells_DirectionIsUnitVector) {
    MockFlowFieldGraph graph;
    FlatCostProvider costs;
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> dist(0, MockFlowFieldGraph::kWidth - 1);

    for (int trial = 0; trial < 100; ++trial) {
        CellCoord goal{ dist(rng), dist(rng) };
        FlowField f = ComputeFlowField(graph, goal, costs,
                                       MockFlowFieldGraph::kWidth, MockFlowFieldGraph::kHeight);
        for (int y = 0; y < MockFlowFieldGraph::kHeight; ++y) {
            for (int x = 0; x < MockFlowFieldGraph::kWidth; ++x) {
                CellCoord cell{x, y};
                if (cell.x == goal.x && cell.y == goal.y) continue;
                const FlowCell& fc = f.Sample(cell);
                if (!fc.reachable) continue;
                float mag = std::sqrtf(fc.direction.X() * fc.direction.X()
                                     + fc.direction.Y() * fc.direction.Y());
                EXPECT_NEAR(mag, 1.0f, 1e-4f)
                    << "Trial " << trial << " cell (" << x << "," << y
                    << ") goal (" << goal.x << "," << goal.y << ") magnitude=" << mag;
            }
        }
    }
}

// Goal cell itself must always have direction {0,0} and be reachable.
TEST(FlowFieldInvariant, GoalCell_AlwaysReachableZeroDirection) {
    MockFlowFieldGraph graph;
    FlatCostProvider costs;
    std::mt19937 rng(99);
    std::uniform_int_distribution<int> dist(0, MockFlowFieldGraph::kWidth - 1);

    for (int trial = 0; trial < 100; ++trial) {
        CellCoord goal{ dist(rng), dist(rng) };
        FlowField f = ComputeFlowField(graph, goal, costs,
                                       MockFlowFieldGraph::kWidth, MockFlowFieldGraph::kHeight);
        const FlowCell& fc = f.Sample(goal);
        EXPECT_TRUE(fc.reachable) << "Goal cell not reachable on trial " << trial;
        EXPECT_FLOAT_EQ(fc.direction.X(), 0.0f);
        EXPECT_FLOAT_EQ(fc.direction.Y(), 0.0f);
    }
}

// Blocking a cell that was previously reachable must not increase reachable count.
TEST(FlowFieldInvariant, BlockingCell_DoesNotIncreaseReachableCount) {
    FlatCostProvider costs;
    MockFlowFieldGraph open;
    FlowField fOpen = ComputeFlowField(open, CellCoord{4, 4}, costs,
                                       MockFlowFieldGraph::kWidth, MockFlowFieldGraph::kHeight);

    MockFlowFieldGraph blocked;
    blocked.SetPassable(CellCoord{2, 2}, false);
    FlowField fBlocked = ComputeFlowField(blocked, CellCoord{4, 4}, costs,
                                          MockFlowFieldGraph::kWidth, MockFlowFieldGraph::kHeight);

    int openCount = 0, blockedCount = 0;
    for (int y = 0; y < MockFlowFieldGraph::kHeight; ++y)
        for (int x = 0; x < MockFlowFieldGraph::kWidth; ++x) {
            if (fOpen.Sample(CellCoord{x, y}).reachable) ++openCount;
            if (fBlocked.Sample(CellCoord{x, y}).reachable) ++blockedCount;
        }
    EXPECT_LE(blockedCount, openCount);
}

// --- Determinism tests ---

// Two identical ComputeFlowField calls must produce bit-identical FlowField output.
TEST(FlowFieldDeterminism, IdenticalRuns_ProduceBitIdenticalOutput) {
    MockFlowFieldGraph graph;
    FlatCostProvider costs;
    CellCoord goal{3, 5};

    FlowField f1 = ComputeFlowField(graph, goal, costs,
                                    MockFlowFieldGraph::kWidth, MockFlowFieldGraph::kHeight);
    FlowField f2 = ComputeFlowField(graph, goal, costs,
                                    MockFlowFieldGraph::kWidth, MockFlowFieldGraph::kHeight);

    for (int y = 0; y < MockFlowFieldGraph::kHeight; ++y) {
        for (int x = 0; x < MockFlowFieldGraph::kWidth; ++x) {
            CellCoord cell{x, y};
            const FlowCell& c1 = f1.Sample(cell);
            const FlowCell& c2 = f2.Sample(cell);
            EXPECT_EQ(c1.reachable, c2.reachable)
                << "Reachability mismatch at (" << x << "," << y << ")";
            if (c1.reachable && c2.reachable) {
                EXPECT_FLOAT_EQ(c1.direction.X(), c2.direction.X())
                    << "Direction.X mismatch at (" << x << "," << y << ")";
                EXPECT_FLOAT_EQ(c1.direction.Y(), c2.direction.Y())
                    << "Direction.Y mismatch at (" << x << "," << y << ")";
            }
        }
    }
}

// Different goals must produce different fields (not a trivial no-op).
TEST(FlowFieldDeterminism, DifferentGoals_ProduceDifferentFields) {
    MockFlowFieldGraph graph;
    FlatCostProvider costs;
    FlowField f1 = ComputeFlowField(graph, CellCoord{0, 0}, costs,
                                    MockFlowFieldGraph::kWidth, MockFlowFieldGraph::kHeight);
    FlowField f2 = ComputeFlowField(graph, CellCoord{7, 7}, costs,
                                    MockFlowFieldGraph::kWidth, MockFlowFieldGraph::kHeight);
    // Cell (3,3) should point in different directions for the two goals.
    const FlowCell& c1 = f1.Sample(CellCoord{3, 3});
    const FlowCell& c2 = f2.Sample(CellCoord{3, 3});
    // At least one component must differ.
    bool differ = (c1.direction.X() != c2.direction.X())
               || (c1.direction.Y() != c2.direction.Y());
    EXPECT_TRUE(differ);
}
