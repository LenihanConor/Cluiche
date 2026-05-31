////////////////////////////////////////////////////////////////////////////////
// Filename: TestSpatialGridPickable.cpp
// Tests: SpatialGridPickable — hit cell, miss outside bounds
// AC4
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>
#include <DiaGeometry2DPicking/Adapters/SpatialGridPickable.h>
#include <DiaGeometry2DPicking/PickHit2D.h>
#include <DiaGeometry2D/Spatial/SpatialGrid.h>
#include <DiaMaths/Vector/Vector2D.h>

using namespace Dia::Geometry2D;
using namespace Dia::Maths;
using namespace Dia::Geometry2DPicking;
using Grid = SpatialGrid<int>;

// 100x100 world, 10-unit cells → 10x10 cells
class SpatialGridPickableFixture : public ::testing::Test
{
protected:
    Grid* grid = nullptr;
    SpatialGridPickable<int, 2048>* pickable = nullptr;

    void SetUp() override
    {
        Grid::Def def;
        def.worldBounds = AARect(Vector2D(0.0f, 0.0f), Vector2D(100.0f, 100.0f));
        def.cellSize    = 10.0f;
        grid = new Grid(def);
        pickable = new SpatialGridPickable<int, 2048>(
            *grid, Dia::Core::StringCRC("spatialgrid"), 3);
    }

    void TearDown() override
    {
        delete pickable;
        delete grid;
    }
};

TEST_F(SpatialGridPickableFixture, TryPick_InsideFirstCell_Hits)
{
    PickHit2D hit;
    EXPECT_TRUE(pickable->TryPick(Vector2D(5.0f, 5.0f), hit));
    EXPECT_EQ(hit.kind, PickHit2D::Kind::kSpatialCell);
    EXPECT_EQ(hit.spatialCell.x, 0);
    EXPECT_EQ(hit.spatialCell.y, 0);
    EXPECT_EQ(hit.priority, 3);
}

TEST_F(SpatialGridPickableFixture, TryPick_InsideLastCell_Hits)
{
    PickHit2D hit;
    EXPECT_TRUE(pickable->TryPick(Vector2D(95.0f, 95.0f), hit));
    EXPECT_EQ(hit.spatialCell.x, 9);
    EXPECT_EQ(hit.spatialCell.y, 9);
}

TEST_F(SpatialGridPickableFixture, TryPick_BeyondBounds_Misses)
{
    PickHit2D hit;
    EXPECT_FALSE(pickable->TryPick(Vector2D(150.0f, 150.0f), hit));
}

TEST_F(SpatialGridPickableFixture, TryPick_NegativePos_Misses)
{
    PickHit2D hit;
    EXPECT_FALSE(pickable->TryPick(Vector2D(-1.0f, -1.0f), hit));
}

TEST_F(SpatialGridPickableFixture, TryPick_CellCoordsMatchCellSize)
{
    // Point at (25.5, 35.5) should be in cell (2, 3)
    PickHit2D hit;
    EXPECT_TRUE(pickable->TryPick(Vector2D(25.5f, 35.5f), hit));
    EXPECT_EQ(hit.spatialCell.x, 2);
    EXPECT_EQ(hit.spatialCell.y, 3);
}

TEST_F(SpatialGridPickableFixture, TryPick_PickableIdFilled)
{
    PickHit2D hit;
    EXPECT_TRUE(pickable->TryPick(Vector2D(5.0f, 5.0f), hit));
    EXPECT_EQ(hit.pickableId, Dia::Core::StringCRC("spatialgrid"));
}

TEST_F(SpatialGridPickableFixture, PickArea_OverlappingRect_ReturnsCorrectCells)
{
    // Rect strictly within cells (0,0) and (1,1) only — use 0..19.9 exclusive
    // (cell size is 10, so cell 2 starts at x=20; stay just under that)
    AARect rect(Vector2D(0.0f, 0.0f), Vector2D(19.9f, 19.9f));
    Dia::Core::Containers::DynamicArrayC<PickHit2D, kMaxAreaHits> hits;
    pickable->PickArea(rect, hits);
    EXPECT_EQ(hits.Size(), 4u); // 2x2 cells: (0,0),(1,0),(0,1),(1,1)
}

TEST_F(SpatialGridPickableFixture, PickArea_NoOverlap_ReturnsNoHits)
{
    AARect rect(Vector2D(200.0f, 200.0f), Vector2D(300.0f, 300.0f));
    Dia::Core::Containers::DynamicArrayC<PickHit2D, kMaxAreaHits> hits;
    pickable->PickArea(rect, hits);
    EXPECT_EQ(hits.Size(), 0u);
}
