// Suites: SLOW_FlowField_Boundary, FlowFieldCache_Boundary, FlowFieldCache_Metrics

#include <gtest/gtest.h>
#include <DiaFlowField/FlowField.h>
#include <DiaFlowField/FlowCell.h>
#include <DiaFlowField/FlowFieldCache.h>
#include <DiaFlowField/ComputeFlowField.h>
#include <DiaFlowField/SquareFlowAdapter.h>
#include <DiaFlowField/Testing/FlowFieldTestHelpers.h>
#include <DiaPathfinding/SquarePathGrid.h>
#include <DiaPathfinding/IPathCostProvider.h>
#include <DiaMaths/Vector/Vector2D.h>

using namespace Dia::FlowField;
using Dia::Pathfinding::CellCoord;
using Dia::Pathfinding::FlatCostProvider;
using Dia::Maths::Vector2D;
using namespace Dia::FlowField::Testing;

// --- FlowField death tests (DIA_ASSERT preconditions) ---

TEST(SLOW_FlowField_Boundary, Constructor_OverCapacity_Asserts) {
    // 65x64 = 4160 > kMaxFlowFieldCells(4096)
    EXPECT_DEATH(FlowField(65, 64), "");
}

TEST(SLOW_FlowField_Boundary, AccessCell_NegativeX_Asserts) {
    FlowField f(3, 3);
    EXPECT_DEATH(f.AccessCell(CellCoord{-1, 0}), "");
}

TEST(SLOW_FlowField_Boundary, AccessCell_XEqualsWidth_Asserts) {
    FlowField f(3, 3);
    EXPECT_DEATH(f.AccessCell(CellCoord{3, 0}), "");
}

TEST(SLOW_FlowField_Boundary, AccessCell_NegativeY_Asserts) {
    FlowField f(3, 3);
    EXPECT_DEATH(f.AccessCell(CellCoord{0, -1}), "");
}

TEST(SLOW_FlowField_Boundary, AccessCell_YEqualsHeight_Asserts) {
    FlowField f(3, 3);
    EXPECT_DEATH(f.AccessCell(CellCoord{0, 3}), "");
}

// --- FlowField boundary edge cases ---

TEST(SLOW_FlowField_Boundary, OneByOneGrid_GoalIsReachable_DirectionIsZero) {
    Dia::Pathfinding::SquarePathGrid grid(1, 1);
    SquareFlowAdapter adapter(grid);
    FlatCostProvider costs;
    FlowField f = ComputeFlowField(adapter, CellCoord{0, 0}, costs, 1, 1);
    const FlowCell& fc = f.Sample(CellCoord{0, 0});
    EXPECT_TRUE(fc.reachable);
    EXPECT_FLOAT_EQ(fc.direction.X(), 0.0f);
    EXPECT_FLOAT_EQ(fc.direction.Y(), 0.0f);
}

TEST(SLOW_FlowField_Boundary, SampleWorld_LargeCellSize_MapsToCorrectCell) {
    // cellSize=10.0f: position (15, 25) -> col = int(15/10) = 1, row = int(25/10) = 2
    FlowField f(4, 4);
    f.AccessCell(CellCoord{1, 2}).direction = Vector2D(1.0f, 0.0f);
    f.AccessCell(CellCoord{1, 2}).reachable = true;
    Vector2D dir = f.SampleWorld(Vector2D(15.0f, 25.0f), 10.0f);
    EXPECT_FLOAT_EQ(dir.X(), 1.0f);
    EXPECT_FLOAT_EQ(dir.Y(), 0.0f);
}

// --- FlowFieldCache boundary edge cases ---

TEST(FlowFieldCache_Boundary, Invalidate_NonExistentKey_IsNoOp) {
    MockFlowFieldGraph graph;
    FlatCostProvider costs;
    FlowFieldCache<MockFlowFieldGraph> cache(graph, costs,
        MockFlowFieldGraph::kWidth, MockFlowFieldGraph::kHeight);
    cache.Invalidate(FlowFieldKey("nonexistent"));  // should not crash
    EXPECT_EQ(cache.GetCachedCount(), 0);
}

TEST(FlowFieldCache_Boundary, InvalidateRegion_EmptyRegion_NoFieldsDirtied) {
    MockFlowFieldGraph graph;
    FlatCostProvider costs;
    FlowFieldCache<MockFlowFieldGraph> cache(graph, costs,
        MockFlowFieldGraph::kWidth, MockFlowFieldGraph::kHeight);
    cache.GetOrCompute(FlowFieldKey("a"), CellCoord{5, 5});
    cache.InvalidateRegion(CellCoord{0, 0}, CellCoord{1, 1});  // {5,5} is outside
    cache.GetOrCompute(FlowFieldKey("a"), CellCoord{5, 5});  // should be a hit
    EXPECT_EQ(cache.GetCacheHits(), 1);
    EXPECT_EQ(cache.GetRecomputeCount(), 1);  // no extra recompute
}

TEST(FlowFieldCache_Boundary, InvalidateRegion_EncompassesAllGoals_AllDirtied) {
    MockFlowFieldGraph graph;
    FlatCostProvider costs;
    FlowFieldCache<MockFlowFieldGraph> cache(graph, costs,
        MockFlowFieldGraph::kWidth, MockFlowFieldGraph::kHeight);
    cache.GetOrCompute(FlowFieldKey("x"), CellCoord{1, 1});
    cache.GetOrCompute(FlowFieldKey("y"), CellCoord{6, 6});
    cache.InvalidateRegion(CellCoord{0, 0}, CellCoord{7, 7});  // covers both goals
    cache.GetOrCompute(FlowFieldKey("x"), CellCoord{1, 1});
    cache.GetOrCompute(FlowFieldKey("y"), CellCoord{6, 6});
    EXPECT_EQ(cache.GetRecomputeCount(), 4);   // 2 initial + 2 after dirty
    EXPECT_EQ(cache.GetCacheMisses(), 4);
}

// --- FlowFieldCache metrics ---

TEST(FlowFieldCache_Metrics, InitialCounts_AreZero) {
    MockFlowFieldGraph graph;
    FlatCostProvider costs;
    FlowFieldCache<MockFlowFieldGraph> cache(graph, costs,
        MockFlowFieldGraph::kWidth, MockFlowFieldGraph::kHeight);
    EXPECT_EQ(cache.GetCacheHits(), 0);
    EXPECT_EQ(cache.GetCacheMisses(), 0);
    EXPECT_EQ(cache.GetRecomputeCount(), 0);
}

TEST(FlowFieldCache_Metrics, NewEntry_CountsAsMissAndRecompute) {
    MockFlowFieldGraph graph;
    FlatCostProvider costs;
    FlowFieldCache<MockFlowFieldGraph> cache(graph, costs,
        MockFlowFieldGraph::kWidth, MockFlowFieldGraph::kHeight);
    cache.GetOrCompute(FlowFieldKey("a"), CellCoord{0, 0});
    EXPECT_EQ(cache.GetCacheMisses(), 1);
    EXPECT_EQ(cache.GetRecomputeCount(), 1);
    EXPECT_EQ(cache.GetCacheHits(), 0);
}

TEST(FlowFieldCache_Metrics, SecondAccess_SameKey_CountsAsHit) {
    MockFlowFieldGraph graph;
    FlatCostProvider costs;
    FlowFieldCache<MockFlowFieldGraph> cache(graph, costs,
        MockFlowFieldGraph::kWidth, MockFlowFieldGraph::kHeight);
    cache.GetOrCompute(FlowFieldKey("a"), CellCoord{0, 0});
    cache.GetOrCompute(FlowFieldKey("a"), CellCoord{0, 0});
    EXPECT_EQ(cache.GetCacheHits(), 1);
    EXPECT_EQ(cache.GetCacheMisses(), 1);
    EXPECT_EQ(cache.GetRecomputeCount(), 1);
}

TEST(FlowFieldCache_Metrics, AfterInvalidate_RecomputeCountsAsMiss) {
    MockFlowFieldGraph graph;
    FlatCostProvider costs;
    FlowFieldCache<MockFlowFieldGraph> cache(graph, costs,
        MockFlowFieldGraph::kWidth, MockFlowFieldGraph::kHeight);
    cache.GetOrCompute(FlowFieldKey("a"), CellCoord{0, 0});  // miss
    cache.GetOrCompute(FlowFieldKey("a"), CellCoord{0, 0});  // hit
    cache.Invalidate(FlowFieldKey("a"));
    cache.GetOrCompute(FlowFieldKey("a"), CellCoord{0, 0});  // miss + recompute
    EXPECT_EQ(cache.GetCacheHits(), 1);
    EXPECT_EQ(cache.GetCacheMisses(), 2);
    EXPECT_EQ(cache.GetRecomputeCount(), 2);
}
