// Suite: SquareFlowAdapter, HexFlowAdapter

#include <gtest/gtest.h>
#include <DiaFlowField/SquareFlowAdapter.h>
#include <DiaFlowField/HexFlowAdapter.h>
#include <DiaFlowField/CFlowFieldGraph.h>
#include <DiaFlowField/ComputeFlowField.h>
#include <DiaPathfinding/SquarePathGrid.h>
#include <DiaPathfinding/HexPathGrid.h>
#include <DiaPathfinding/IPathCostProvider.h>

using namespace Dia::FlowField;
using Dia::Pathfinding::CellCoord;
using Dia::Pathfinding::FlatCostProvider;
using Dia::Maths::Vector2D;

TEST(SquareFlowAdapter, SatisfiesCFlowFieldGraph) {
    static_assert(CFlowFieldGraph<SquareFlowAdapter>);
    SUCCEED();
}

TEST(HexFlowAdapter, SatisfiesCFlowFieldGraph) {
    static_assert(CFlowFieldGraph<HexFlowAdapter>);
    SUCCEED();
}

TEST(SquareFlowAdapter, CellToWorldPosition_CellCentered) {
    Dia::Pathfinding::SquarePathGrid grid(5, 5);
    SquareFlowAdapter adapter(grid);
    Vector2D pos = adapter.CellToWorldPosition(CellCoord{2, 3}, 10.0f);
    EXPECT_FLOAT_EQ(pos.X(), 25.0f);  // 2*10 + 5
    EXPECT_FLOAT_EQ(pos.Y(), 35.0f);  // 3*10 + 5
}

TEST(SquareFlowAdapter, WorldToCell_InvertsCorrectly) {
    Dia::Pathfinding::SquarePathGrid grid(5, 5);
    SquareFlowAdapter adapter(grid);
    CellCoord cell = adapter.WorldToCell(Vector2D(25.0f, 35.0f), 10.0f);
    EXPECT_EQ(cell.x, 2);
    EXPECT_EQ(cell.y, 3);
}

TEST(SquareFlowAdapter, ForwardsPassability) {
    Dia::Pathfinding::SquarePathGrid grid(5, 5);
    grid.SetPassable(CellCoord{1, 1}, false);
    SquareFlowAdapter adapter(grid);
    EXPECT_TRUE(adapter.IsPassable(CellCoord{0, 0}));
    EXPECT_FALSE(adapter.IsPassable(CellCoord{1, 1}));
}

TEST(SquareFlowAdapter, ComputeFlowField_AllReachable) {
    Dia::Pathfinding::SquarePathGrid grid(4, 4);
    SquareFlowAdapter adapter(grid);
    FlatCostProvider costs;
    FlowField f = ComputeFlowField(adapter, CellCoord{0, 0}, costs,
                                   adapter.GetWidth(), adapter.GetHeight());
    EXPECT_TRUE(f.IsComplete());
}

TEST(HexFlowAdapter, CellToWorldPosition_OriginIsZero) {
    Dia::Pathfinding::HexPathGrid grid(3);
    HexFlowAdapter adapter(grid);
    Vector2D pos = adapter.CellToWorldPosition(CellCoord{0, 0}, 1.0f);
    EXPECT_FLOAT_EQ(pos.X(), 0.0f);
    EXPECT_FLOAT_EQ(pos.Y(), 0.0f);
}

TEST(HexFlowAdapter, ComputeFlowField_SmallHexGrid_GoalReachable) {
    Dia::Pathfinding::HexPathGrid grid(2);
    HexFlowAdapter adapter(grid);
    FlatCostProvider costs;
    // Hex grid radius 2: 5x5 bounding box = 25 cells max.
    FlowField f = ComputeFlowField(adapter, CellCoord{0, 0}, costs, 5, 5);
    EXPECT_TRUE(f.Sample(CellCoord{0, 0}).reachable);
}
