// Suites: FlowFieldStress
// Large-grid, many-cache-entry, and rapid-invalidation stress tests.
// Verify no NaN/Inf in any direction component, correct cell counts.

#include <gtest/gtest.h>
#include <DiaFlowField/ComputeFlowField.h>
#include <DiaFlowField/FlowFieldCache.h>
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

// kMaxFlowFieldCells = 4096; 64×63 = 4032 ≤ 4096.
TEST(FlowFieldStress, LargeGrid_NoNaNOrInf) {
    const int W = 64, H = 63;
    Dia::Pathfinding::SquarePathGrid grid(W, H);
    SquareFlowAdapter adapter(grid);
    FlatCostProvider costs;
    FlowField f = ComputeFlowField(adapter, CellCoord{W/2, H/2}, costs, W, H);
    EXPECT_EQ(f.GetCellCount(), W * H);
    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            const FlowCell& fc = f.Sample(CellCoord{x, y});
            if (!fc.reachable) continue;
            EXPECT_FALSE(std::isnan(fc.direction.X())) << "NaN at (" << x << "," << y << ")";
            EXPECT_FALSE(std::isnan(fc.direction.Y())) << "NaN at (" << x << "," << y << ")";
            EXPECT_FALSE(std::isinf(fc.direction.X())) << "Inf at (" << x << "," << y << ")";
            EXPECT_FALSE(std::isinf(fc.direction.Y())) << "Inf at (" << x << "," << y << ")";
        }
    }
}

// 20 distinct cache entries; all are reachable.
TEST(FlowFieldStress, Cache_TwentyEntries_AllReachable) {
    MockFlowFieldGraph graph;
    FlatCostProvider costs;
    FlowFieldCache<MockFlowFieldGraph> cache(graph, costs,
        MockFlowFieldGraph::kWidth, MockFlowFieldGraph::kHeight);

    // Scatter 20 goals across the 8×8 grid (wrap by modulo).
    for (int i = 0; i < 20; ++i) {
        int x = i % MockFlowFieldGraph::kWidth;
        int y = (i * 3) % MockFlowFieldGraph::kHeight;
        char buf[16];
        snprintf(buf, sizeof(buf), "g%d", i);
        const FlowField& f = cache.GetOrCompute(FlowFieldKey(buf), CellCoord{x, y});
        EXPECT_TRUE(f.Sample(CellCoord{x, y}).reachable) << "Goal unreachable for entry " << i;
    }
    EXPECT_EQ(cache.GetCachedCount(), 20);
}

// 50 invalidate-then-recompute cycles; no crashes or NaN.
TEST(FlowFieldStress, Cache_RepeatedInvalidationCycles) {
    MockFlowFieldGraph graph;
    FlatCostProvider costs;
    FlowFieldCache<MockFlowFieldGraph> cache(graph, costs,
        MockFlowFieldGraph::kWidth, MockFlowFieldGraph::kHeight);

    FlowFieldKey key("cycle");
    CellCoord goal{3, 3};

    cache.GetOrCompute(key, goal);
    for (int i = 0; i < 50; ++i) {
        cache.Invalidate(key);
        const FlowField& f = cache.GetOrCompute(key, goal);
        EXPECT_TRUE(f.Sample(goal).reachable) << "Goal unreachable on cycle " << i;
    }
    // 1 initial + 50 recomputes after invalidate
    EXPECT_EQ(cache.GetRecomputeCount(), 51);
}

// InvalidateAll then recompute for all 20 entries; recompute count should be 40.
TEST(FlowFieldStress, Cache_InvalidateAll_ThenRecomputeAll) {
    MockFlowFieldGraph graph;
    FlatCostProvider costs;
    FlowFieldCache<MockFlowFieldGraph> cache(graph, costs,
        MockFlowFieldGraph::kWidth, MockFlowFieldGraph::kHeight);

    for (int i = 0; i < 20; ++i) {
        char buf[16];
        snprintf(buf, sizeof(buf), "e%d", i);
        cache.GetOrCompute(FlowFieldKey(buf), CellCoord{i % 8, (i * 2) % 8});
    }
    cache.InvalidateAll();
    for (int i = 0; i < 20; ++i) {
        char buf[16];
        snprintf(buf, sizeof(buf), "e%d", i);
        cache.GetOrCompute(FlowFieldKey(buf), CellCoord{i % 8, (i * 2) % 8});
    }
    EXPECT_EQ(cache.GetRecomputeCount(), 40);  // 20 initial + 20 after InvalidateAll
    EXPECT_EQ(cache.GetCacheHits(), 0);
}
