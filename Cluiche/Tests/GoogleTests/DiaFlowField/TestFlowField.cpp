// Suites: FlowCell, FlowField

#include <gtest/gtest.h>
#include <DiaFlowField/FlowCell.h>
#include <DiaFlowField/FlowField.h>
#include <DiaFlowField/CFlowFieldGraph.h>
#include <DiaFlowField/SquareFlowAdapter.h>
#include <DiaFlowField/HexFlowAdapter.h>
#include <DiaFlowField/Testing/FlowFieldTestHelpers.h>
#include <DiaMaths/Vector/Vector2D.h>

using namespace Dia::FlowField;
using Dia::Pathfinding::CellCoord;
using Dia::Maths::Vector2D;

// FlowCell tests
TEST(FlowCell, DefaultInit_DirectionIsZero_ReachableIsFalse) {
    FlowCell fc{};
    EXPECT_FLOAT_EQ(fc.direction.X(), 0.0f);
    EXPECT_FLOAT_EQ(fc.direction.Y(), 0.0f);
    EXPECT_FALSE(fc.reachable);
}

// FlowField tests
TEST(FlowField, Construct_CellCount_MatchesWidthTimesHeight) {
    FlowField f(3, 4);
    EXPECT_EQ(f.GetCellCount(), 12);
}

TEST(FlowField, Sample_DefaultCell_IsUnreachable) {
    FlowField f(4, 4);
    const FlowCell& fc = f.Sample(CellCoord{1, 1});
    EXPECT_FALSE(fc.reachable);
}

TEST(FlowField, Sample_OutOfBounds_ReturnsDefaultUnreachable) {
    FlowField f(4, 4);
    const FlowCell& fc = f.Sample(CellCoord{10, 10});
    EXPECT_FALSE(fc.reachable);
}

TEST(FlowField, SampleWorld_MapsToCorrectCell) {
    FlowField f(4, 4);
    // cellSize=1.0f, position (1.5, 2.5) -> col=1, row=2
    Vector2D dir = f.SampleWorld(Vector2D(1.5f, 2.5f), 1.0f);
    EXPECT_FLOAT_EQ(dir.X(), 0.0f);
    EXPECT_FLOAT_EQ(dir.Y(), 0.0f);
}

TEST(FlowField, IsComplete_AllUnreachable_ReturnsFalse) {
    FlowField f(2, 2);
    EXPECT_FALSE(f.IsComplete());
}

TEST(FlowField, IsComplete_AllReachable_ReturnsTrue) {
    FlowField f(2, 2);
    // mark all 4 cells reachable via AccessCell
    for (int y = 0; y < 2; ++y)
        for (int x = 0; x < 2; ++x)
            f.AccessCell(CellCoord{x, y}).reachable = true;
    EXPECT_TRUE(f.IsComplete());
}

// CFlowFieldGraph concept tests
TEST(CFlowFieldGraph, MockGraph_SatisfiesConcept) {
    // static_assert in FlowFieldTestHelpers.h already verifies this.
    // This test documents the behaviour.
    using namespace Dia::FlowField::Testing;
    static_assert(CFlowFieldGraph<MockFlowFieldGraph>);
    SUCCEED();
}
