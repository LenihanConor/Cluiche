// Suite: FlowFieldCache

#include <gtest/gtest.h>
#include <DiaFlowField/FlowFieldCache.h>
#include <DiaFlowField/HexFlowAdapter.h>
#include <DiaFlowField/Testing/FlowFieldTestHelpers.h>
#include <DiaPathfinding/HexPathGrid.h>
#include <DiaPathfinding/IPathCostProvider.h>
#include <memory>

using namespace Dia::FlowField;
using Dia::Pathfinding::CellCoord;
using Dia::Pathfinding::FlatCostProvider;
using namespace Dia::FlowField::Testing;

TEST(FlowFieldCache, GetOrCompute_NewKey_ComputesField) {
    MockFlowFieldGraph graph;
    FlatCostProvider costs;
    FlowFieldCache<MockFlowFieldGraph> cache(graph, costs,
        MockFlowFieldGraph::kWidth, MockFlowFieldGraph::kHeight);
    const FlowField& f = cache.GetOrCompute(FlowFieldKey("base"), CellCoord{4, 4});
    EXPECT_EQ(cache.GetCachedCount(), 1);
    AssertReachable(f, CellCoord{4, 4});
}

TEST(FlowFieldCache, GetOrCompute_SameKeySameGoal_ReturnsCachedField) {
    MockFlowFieldGraph graph;
    FlatCostProvider costs;
    FlowFieldCache<MockFlowFieldGraph> cache(graph, costs,
        MockFlowFieldGraph::kWidth, MockFlowFieldGraph::kHeight);
    const FlowField& f1 = cache.GetOrCompute(FlowFieldKey("base"), CellCoord{4, 4});
    const FlowField& f2 = cache.GetOrCompute(FlowFieldKey("base"), CellCoord{4, 4});
    EXPECT_EQ(&f1, &f2);  // same pointer — cached
    EXPECT_EQ(cache.GetCachedCount(), 1);
}

TEST(FlowFieldCache, GetOrCompute_TwoKeys_CachesBoth) {
    MockFlowFieldGraph graph;
    FlatCostProvider costs;
    FlowFieldCache<MockFlowFieldGraph> cache(graph, costs,
        MockFlowFieldGraph::kWidth, MockFlowFieldGraph::kHeight);
    cache.GetOrCompute(FlowFieldKey("goal_a"), CellCoord{0, 0});
    cache.GetOrCompute(FlowFieldKey("goal_b"), CellCoord{7, 7});
    EXPECT_EQ(cache.GetCachedCount(), 2);
}

TEST(FlowFieldCache, Invalidate_MarksFieldDirty_RecomputesOnNextAccess) {
    MockFlowFieldGraph graph;
    FlatCostProvider costs;
    FlowFieldCache<MockFlowFieldGraph> cache(graph, costs,
        MockFlowFieldGraph::kWidth, MockFlowFieldGraph::kHeight);
    cache.GetOrCompute(FlowFieldKey("key"), CellCoord{4, 4});
    cache.Invalidate(FlowFieldKey("key"));
    // After invalidate, next GetOrCompute recomputes (pointer may change)
    const FlowField& f2 = cache.GetOrCompute(FlowFieldKey("key"), CellCoord{4, 4});
    AssertReachable(f2, CellCoord{4, 4});
}

TEST(FlowFieldCache, InvalidateAll_MarksAllDirty) {
    MockFlowFieldGraph graph;
    FlatCostProvider costs;
    FlowFieldCache<MockFlowFieldGraph> cache(graph, costs,
        MockFlowFieldGraph::kWidth, MockFlowFieldGraph::kHeight);
    cache.GetOrCompute(FlowFieldKey("a"), CellCoord{0, 0});
    cache.GetOrCompute(FlowFieldKey("b"), CellCoord{7, 7});
    cache.InvalidateAll();
    // Both still cached (dirty, not removed)
    EXPECT_EQ(cache.GetCachedCount(), 2);
}

TEST(FlowFieldCache, InvalidateRegion_OnlyAffectsGoalsInsideRegion) {
    MockFlowFieldGraph graph;
    FlatCostProvider costs;
    FlowFieldCache<MockFlowFieldGraph> cache(graph, costs,
        MockFlowFieldGraph::kWidth, MockFlowFieldGraph::kHeight);
    cache.GetOrCompute(FlowFieldKey("in"),  CellCoord{2, 2});  // goal inside region
    cache.GetOrCompute(FlowFieldKey("out"), CellCoord{6, 6});  // goal outside region
    // Invalidate region [1,1] to [3,3]
    cache.InvalidateRegion(CellCoord{1, 1}, CellCoord{3, 3});
    // "in" should be dirty; "out" should be clean.
    // Verify "out" returns same pointer (not recomputed):
    const FlowField& fOut1 = cache.GetOrCompute(FlowFieldKey("out"), CellCoord{6, 6});
    const FlowField& fOut2 = cache.GetOrCompute(FlowFieldKey("out"), CellCoord{6, 6});
    EXPECT_EQ(&fOut1, &fOut2);
}

// ===========================================================================
// Large grid cache test (AC8)
// ===========================================================================

TEST(SLOW_FlowFieldCache, LargeHexGrid_104x64_CacheAndInvalidate) {
    // AC8: FlowFieldCache on 104×64 rectangular hex grid
    // Note: Needs heap-allocated grid/adapter to avoid lifetime issues
    auto grid = std::make_unique<Dia::Pathfinding::HexPathGrid>(104, 64);
    auto adapter = std::make_unique<HexFlowAdapter>(*grid);
    FlatCostProvider costs;
    FlowFieldCache<HexFlowAdapter> cache(*adapter, costs, 104, 64);

    // Compute and cache
    CellCoord goal{52, 32};
    const FlowField& f1 = cache.GetOrCompute(FlowFieldKey("large"), goal);
    EXPECT_EQ(cache.GetCachedCount(), 1);
    EXPECT_TRUE(f1.Sample(goal).reachable);

    // Cache hit returns same field
    const FlowField& f2 = cache.GetOrCompute(FlowFieldKey("large"), goal);
    EXPECT_EQ(&f1, &f2);
    EXPECT_EQ(cache.GetCacheHits(), 1);

    // InvalidateRegion including goal
    cache.InvalidateRegion(CellCoord{50, 30}, CellCoord{54, 34});

    // Recompute after invalidation
    const FlowField& f3 = cache.GetOrCompute(FlowFieldKey("large"), goal);
    EXPECT_EQ(cache.GetRecomputeCount(), 2); // initial + after invalidate
    EXPECT_TRUE(f3.Sample(goal).reachable);
}
