////////////////////////////////////////////////////////////////////////////////
// Filename: TestHexGridPickable.cpp
// Tests: HexGridPickable — hit centre, hit edge, miss outside, miss origin (−1,−1)
// AC3
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>
#include <DiaGeometry2DPicking/Adapters/HexGridPickable.h>
#include <DiaGeometry2DPicking/PickHit2D.h>
#include <DiaGeometry2D/Spatial/HexGrid.h>
#include <DiaMaths/Vector/Vector2D.h>

using namespace Dia::Geometry2D;
using namespace Dia::Maths;
using namespace Dia::Geometry2DPicking;
using Grid = HexGrid<int>;

// 5x5 grid, radius 10, origin at (0,0)
class HexGridPickableFixture : public ::testing::Test
{
protected:
    Grid* grid = nullptr;
    HexGridPickable<int, 2048>* pickable = nullptr;

    void SetUp() override
    {
        Grid::Def def;
        def.origin    = Vector2D(0.0f, 0.0f);
        def.colCount  = 5;
        def.rowCount  = 5;
        def.hexRadius = 10.0f;
        grid = new Grid(def);
        pickable = new HexGridPickable<int, 2048>(
            *grid, Dia::Core::StringCRC("hexgrid"), 5);
    }

    void TearDown() override
    {
        delete pickable;
        delete grid;
    }
};

TEST_F(HexGridPickableFixture, GetPickableId_MatchesConstructorId)
{
    EXPECT_EQ(pickable->GetPickableId(), Dia::Core::StringCRC("hexgrid"));
}

TEST_F(HexGridPickableFixture, GetPriority_MatchesConstructorPriority)
{
    EXPECT_EQ(pickable->GetPriority(), 5);
}

TEST_F(HexGridPickableFixture, TryPick_CellCentre_ReturnsExactCoord)
{
    // WorldToHex(HexToWorld({q,r})) must round-trip exactly for all cells.
    // Clicking the drawn centre must select that specific cell, not a neighbour.
    for (int r = 0; r < 5; ++r)
    {
        for (int q = 0; q < 5; ++q)
        {
            const Vector2D centre = grid->HexToWorld(HexCoord{q, r});
            PickHit2D hit;
            EXPECT_TRUE(pickable->TryPick(centre, hit));
            EXPECT_EQ(hit.hexCell.q, q) << "round-trip failed for q=" << q << " r=" << r;
            EXPECT_EQ(hit.hexCell.r, r) << "round-trip failed for q=" << q << " r=" << r;
        }
    }
}

TEST_F(HexGridPickableFixture, TryPick_FarOutside_Misses)
{
    // Way outside any hex cell
    PickHit2D hit;
    EXPECT_FALSE(pickable->TryPick(Vector2D(9999.0f, 9999.0f), hit));
}

TEST_F(HexGridPickableFixture, TryPick_NegativeOrigin_Misses)
{
    // Clearly negative world pos outside any grid cell
    PickHit2D hit;
    EXPECT_FALSE(pickable->TryPick(Vector2D(-500.0f, -500.0f), hit));
}

TEST_F(HexGridPickableFixture, TryPick_PickableId_FilledInHit)
{
    const Vector2D centre = grid->HexToWorld(HexCoord{1, 1});
    PickHit2D hit;
    EXPECT_TRUE(pickable->TryPick(centre, hit));
    EXPECT_EQ(hit.pickableId, Dia::Core::StringCRC("hexgrid"));
}

TEST_F(HexGridPickableFixture, PickArea_AllCellsInBigRect_ReturnsAllCells)
{
    // Rect that covers the entire grid
    const Vector2D centre44 = grid->HexToWorld(HexCoord{4, 4});
    Dia::Geometry2D::AARect bigRect(
        Vector2D(-5.0f, -5.0f),
        Vector2D(centre44.x + 20.0f, centre44.y + 20.0f));

    Dia::Core::Containers::DynamicArrayC<PickHit2D, kMaxAreaHits> hits;
    pickable->PickArea(bigRect, hits);

    EXPECT_EQ(hits.Size(), 25u); // 5x5 = 25 cells
}

TEST_F(HexGridPickableFixture, PickArea_TinyRect_ReturnsOnlyContainedCells)
{
    const Vector2D centre00 = grid->HexToWorld(HexCoord{0, 0});
    // Rect right around only cell (0,0)
    Dia::Geometry2D::AARect tinyRect(
        Vector2D(centre00.x - 1.0f, centre00.y - 1.0f),
        Vector2D(centre00.x + 1.0f, centre00.y + 1.0f));

    Dia::Core::Containers::DynamicArrayC<PickHit2D, kMaxAreaHits> hits;
    pickable->PickArea(tinyRect, hits);

    EXPECT_GE(hits.Size(), 1u);
    EXPECT_EQ(hits[0].hexCell.q, 0);
    EXPECT_EQ(hits[0].hexCell.r, 0);
}
