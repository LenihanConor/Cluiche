// Suites: SLOW_FlowField_Boundary, FlowFieldCache_Boundary, FlowFieldCache_Metrics

#include <gtest/gtest.h>
#include <DiaFlowField/FlowField.h>
#include <DiaFlowField/FlowCell.h>
#include <DiaFlowField/FlowFieldCache.h>
#include <DiaFlowField/ComputeFlowField.h>
#include <DiaFlowField/SquareFlowAdapter.h>
#include <DiaFlowField/HexFlowAdapter.h>
#include <DiaFlowField/Testing/FlowFieldTestHelpers.h>
#include <DiaPathfinding/SquarePathGrid.h>
#include <DiaPathfinding/HexPathGrid.h>
#include <DiaPathfinding/HexTopology.h>
#include <DiaPathfinding/IPathCostProvider.h>
#include <DiaMaths/Vector/Vector2D.h>

using namespace Dia::FlowField;
using Dia::Pathfinding::CellCoord;
using Dia::Pathfinding::FlatCostProvider;
using Dia::Pathfinding::HexTopology;
using Dia::Maths::Vector2D;
using namespace Dia::FlowField::Testing;

// --- FlowField construction (AC1) ---

TEST(SLOW_FlowField_Boundary, Constructor_104x64_Constructs) {
    // 104×64 = 6656 cells (CoW Large map)
    FlowField f(104, 64);
    EXPECT_EQ(f.GetCellCount(), 6656);
    EXPECT_EQ(f.GetWidth(), 104);
    EXPECT_EQ(f.GetHeight(), 64);
}

TEST(SLOW_FlowField_Boundary, Constructor_128x128_Constructs) {
    // 128×128 = 16384 cells (tested ceiling)
    FlowField f(128, 128);
    EXPECT_EQ(f.GetCellCount(), 16384);
    EXPECT_EQ(f.GetWidth(), 128);
    EXPECT_EQ(f.GetHeight(), 128);
}

TEST(SLOW_FlowField_Boundary, Sample_FirstAndLastCell_Work) {
    FlowField f(104, 64);
    // First cell
    f.AccessCell(CellCoord{0, 0}).direction = Vector2D(1.0f, 0.0f);
    f.AccessCell(CellCoord{0, 0}).reachable = true;
    const FlowCell& fc0 = f.Sample(CellCoord{0, 0});
    EXPECT_TRUE(fc0.reachable);
    EXPECT_FLOAT_EQ(fc0.direction.X(), 1.0f);

    // Last cell (103, 63)
    f.AccessCell(CellCoord{103, 63}).direction = Vector2D(0.0f, 1.0f);
    f.AccessCell(CellCoord{103, 63}).reachable = true;
    const FlowCell& fc_last = f.Sample(CellCoord{103, 63});
    EXPECT_TRUE(fc_last.reachable);
    EXPECT_FLOAT_EQ(fc_last.direction.Y(), 1.0f);
}

TEST(SLOW_FlowField_Boundary, Sample_OutOfBounds_ReturnsDefaultUnreachable) {
    FlowField f(104, 64);
    const FlowCell& fc = f.Sample(CellCoord{200, 200});
    EXPECT_FALSE(fc.reachable);
    EXPECT_FLOAT_EQ(fc.direction.X(), 0.0f);
    EXPECT_FLOAT_EQ(fc.direction.Y(), 0.0f);
}

// --- FlowField death tests (DIA_ASSERT preconditions) ---

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

// --- Graph-aware sampling (AC4, AC5) ---

TEST(SLOW_FlowField_Boundary, GraphAwareSample_HexGrid_RoundTrips) {
    // AC4: For a field on HexPathGrid(20, 15), Sample(adapter, CellCenter(c), size)
    // returns the same cell as Sample(c) for every cell
    Dia::Pathfinding::HexPathGrid grid(20, 15);
    HexFlowAdapter adapter(grid);
    FlatCostProvider costs;
    FlowField f = ComputeFlowField(adapter, CellCoord{10, 7}, costs, 20, 15);

    const float size = 10.0f;
    for (int row = 0; row < 15; ++row) {
        for (int col = 0; col < 20; ++col) {
            CellCoord cell{col, row};
            Vector2D worldPos = HexTopology::CellCenter(cell, size);
            const FlowCell& sampled = f.Sample(adapter, worldPos, size);
            const FlowCell& direct = f.Sample(cell);
            EXPECT_EQ(sampled.reachable, direct.reachable)
                << "Cell (" << col << "," << row << ") reachability mismatch";
            EXPECT_FLOAT_EQ(sampled.direction.X(), direct.direction.X())
                << "Cell (" << col << "," << row << ") direction.X mismatch";
            EXPECT_FLOAT_EQ(sampled.direction.Y(), direct.direction.Y())
                << "Cell (" << col << "," << row << ") direction.Y mismatch";
        }
    }
}

TEST(SLOW_FlowField_Boundary, GraphAwareSample_HexGrid_DisagreesWithSquareOnly) {
    // AC4: The square-only SampleWorld(worldPos, cellSize) on a hex field
    // is shown to disagree for at least one cell
    Dia::Pathfinding::HexPathGrid grid(20, 15);
    HexFlowAdapter adapter(grid);
    FlatCostProvider costs;
    FlowField f = ComputeFlowField(adapter, CellCoord{10, 7}, costs, 20, 15);

    const float size = 10.0f;
    bool foundDisagreement = false;
    for (int row = 0; row < 15 && !foundDisagreement; ++row) {
        for (int col = 0; col < 20 && !foundDisagreement; ++col) {
            CellCoord cell{col, row};
            Vector2D worldPos = HexTopology::CellCenter(cell, size);

            // Graph-aware sample
            const FlowCell& graphAware = f.Sample(adapter, worldPos, size);
            // Square-only sample (wrong for hex)
            Vector2D squareOnly = f.SampleWorld(worldPos, size);

            if (graphAware.direction.X() != squareOnly.X() ||
                graphAware.direction.Y() != squareOnly.Y()) {
                foundDisagreement = true;
            }
        }
    }
    EXPECT_TRUE(foundDisagreement) << "Square-only SampleWorld should disagree with hex on at least one cell";
}

TEST(SLOW_FlowField_Boundary, GraphAwareSample_OutOfBounds_ReturnsDefault) {
    // AC4: Out-of-bounds world positions return the default unreachable cell
    Dia::Pathfinding::HexPathGrid grid(20, 15);
    HexFlowAdapter adapter(grid);
    FlatCostProvider costs;
    FlowField f = ComputeFlowField(adapter, CellCoord{10, 7}, costs, 20, 15);

    const float size = 10.0f;
    Vector2D farOutside(10000.0f, 10000.0f);
    const FlowCell& fc = f.Sample(adapter, farOutside, size);
    EXPECT_FALSE(fc.reachable);
    EXPECT_FLOAT_EQ(fc.direction.X(), 0.0f);
    EXPECT_FLOAT_EQ(fc.direction.Y(), 0.0f);
}

TEST(SLOW_FlowField_Boundary, GraphAwareSample_SquareGrid_AgreesWithSquareOnly) {
    // AC5: Sample(adapter, ...) agrees with the square-only SampleWorld on a square grid
    Dia::Pathfinding::SquarePathGrid grid(20, 15);
    SquareFlowAdapter adapter(grid);
    FlatCostProvider costs;
    FlowField f = ComputeFlowField(adapter, CellCoord{10, 7}, costs, 20, 15);

    const float size = 10.0f;
    for (int row = 0; row < 15; ++row) {
        for (int col = 0; col < 20; ++col) {
            Vector2D worldPos((col + 0.5f) * size, (row + 0.5f) * size);

            // Graph-aware sample
            Vector2D graphAware = f.SampleWorld(adapter, worldPos, size);
            // Square-only sample
            Vector2D squareOnly = f.SampleWorld(worldPos, size);

            EXPECT_FLOAT_EQ(graphAware.X(), squareOnly.X())
                << "Position (" << worldPos.X() << "," << worldPos.Y() << ") direction.X mismatch";
            EXPECT_FLOAT_EQ(graphAware.Y(), squareOnly.Y())
                << "Position (" << worldPos.X() << "," << worldPos.Y() << ") direction.Y mismatch";
        }
    }
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
