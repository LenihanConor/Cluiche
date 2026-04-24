#include <gtest/gtest.h>
#include <DiaGeometry2D/Spatial/SpatialGrid.h>
#include <DiaGeometry2D/Spatial/ISpatialStructure.h>
#include <DiaGeometry2D/Shapes/AARect.h>
#include <DiaGeometry2D/Shapes/Circle.h>
#include <DiaGeometry2D/Shapes/Ray.h>
#include <DiaCore/Containers/Handle.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

using namespace Dia::Geometry2D;
using namespace Dia::Maths;
using HandleType = Dia::Core::Handle<int>;
using Grid = SpatialGrid<int>;
using QueryOut = Dia::Core::Containers::DynamicArrayC<HandleType, kMaxQueryResults>;

static Grid::Def MakeWorldDef()
{
    Grid::Def def;
    def.worldBounds = AARect(Vector2D(0.0f, 0.0f), Vector2D(100.0f, 100.0f));
    def.cellSize = 10.0f;
    return def;
}

// SpatialGrid has a large inline memory footprint (~1 MB for the default MaxObjects/kMaxCells).
// Heap-allocate via a test fixture to avoid stack overflow.
class Geometry2D_SpatialGrid : public ::testing::Test
{
protected:
    Grid* grid = nullptr;

    void SetUp() override
    {
        grid = new Grid(MakeWorldDef());
    }

    void TearDown() override
    {
        delete grid;
        grid = nullptr;
    }
};

// ---------------------------------------------------------------------------
// Insert / Resolve
// ---------------------------------------------------------------------------

TEST_F(Geometry2D_SpatialGrid, Insert_ReturnsValidHandle)
{
    HandleType h = grid->Insert(42, AARect(Vector2D(5.0f, 5.0f), Vector2D(8.0f, 8.0f)));
    EXPECT_TRUE(h.IsValid());
}

TEST_F(Geometry2D_SpatialGrid, Resolve_ValidHandle_ReturnsObject)
{
    HandleType h = grid->Insert(99, AARect(Vector2D(5.0f, 5.0f), Vector2D(8.0f, 8.0f)));
    const int* obj = grid->Resolve(h);
    ASSERT_NE(obj, nullptr);
    EXPECT_EQ(*obj, 99);
}

TEST_F(Geometry2D_SpatialGrid, Resolve_InvalidHandle_ReturnsNull)
{
    const int* obj = grid->Resolve(HandleType::Invalid());
    EXPECT_EQ(obj, nullptr);
}

// ---------------------------------------------------------------------------
// Remove
// ---------------------------------------------------------------------------

TEST_F(Geometry2D_SpatialGrid, Remove_ValidHandle_ObjectGoneFromQuery)
{
    HandleType h = grid->Insert(1, AARect(Vector2D(5.0f, 5.0f), Vector2D(8.0f, 8.0f)));
    grid->Remove(h);

    QueryOut out;
    grid->QueryRegion(AARect(Vector2D(0.0f, 0.0f), Vector2D(20.0f, 20.0f)), out);
    EXPECT_EQ(out.Size(), 0u);
}

TEST_F(Geometry2D_SpatialGrid, Resolve_AfterRemove_StaleHandle_ReturnsNull)
{
    HandleType h = grid->Insert(7, AARect(Vector2D(5.0f, 5.0f), Vector2D(8.0f, 8.0f)));
    grid->Remove(h);
    EXPECT_EQ(grid->Resolve(h), nullptr);
}

// ---------------------------------------------------------------------------
// QueryRegion
// ---------------------------------------------------------------------------

TEST_F(Geometry2D_SpatialGrid, QueryRegion_CoveringObject_ReturnsHandle)
{
    HandleType h = grid->Insert(1, AARect(Vector2D(5.0f, 5.0f), Vector2D(8.0f, 8.0f)));

    QueryOut out;
    grid->QueryRegion(AARect(Vector2D(0.0f, 0.0f), Vector2D(20.0f, 20.0f)), out);
    EXPECT_EQ(out.Size(), 1u);
    EXPECT_EQ(out[0], h);
}

TEST_F(Geometry2D_SpatialGrid, QueryRegion_NotCovering_ReturnsNothing)
{
    grid->Insert(1, AARect(Vector2D(5.0f, 5.0f), Vector2D(8.0f, 8.0f)));

    QueryOut out;
    grid->QueryRegion(AARect(Vector2D(50.0f, 50.0f), Vector2D(60.0f, 60.0f)), out);
    EXPECT_EQ(out.Size(), 0u);
}

TEST_F(Geometry2D_SpatialGrid, QueryRegion_ObjectSpanning4Cells_ReturnedOnce)
{
    // Object centred on cell boundary (9,9)→(11,11) spans 4 cells
    grid->Insert(5, AARect(Vector2D(8.0f, 8.0f), Vector2D(12.0f, 12.0f)));

    QueryOut out;
    grid->QueryRegion(AARect(Vector2D(0.0f, 0.0f), Vector2D(20.0f, 20.0f)), out);
    EXPECT_EQ(out.Size(), 1u);  // Must appear only once (dedup)
}

// ---------------------------------------------------------------------------
// Update
// ---------------------------------------------------------------------------

TEST_F(Geometry2D_SpatialGrid, Update_MovesObjectToNewCells)
{
    HandleType h = grid->Insert(3, AARect(Vector2D(5.0f, 5.0f), Vector2D(8.0f, 8.0f)));

    grid->Update(h, AARect(Vector2D(55.0f, 55.0f), Vector2D(58.0f, 58.0f)));

    QueryOut oldPos;
    grid->QueryRegion(AARect(Vector2D(0.0f, 0.0f), Vector2D(20.0f, 20.0f)), oldPos);
    EXPECT_EQ(oldPos.Size(), 0u);

    QueryOut newPos;
    grid->QueryRegion(AARect(Vector2D(50.0f, 50.0f), Vector2D(70.0f, 70.0f)), newPos);
    EXPECT_EQ(newPos.Size(), 1u);
}

// ---------------------------------------------------------------------------
// QueryCircle
// ---------------------------------------------------------------------------

TEST_F(Geometry2D_SpatialGrid, QueryCircle_ObjectInsideCircle_Returned)
{
    grid->Insert(1, AARect(Vector2D(5.0f, 5.0f), Vector2D(8.0f, 8.0f)));

    QueryOut out;
    grid->QueryCircle(Circle(20.0f, Vector2D(6.5f, 6.5f)), out);
    EXPECT_EQ(out.Size(), 1u);
}

TEST_F(Geometry2D_SpatialGrid, QueryCircle_ObjectOutsideCircle_NotReturned)
{
    grid->Insert(1, AARect(Vector2D(5.0f, 5.0f), Vector2D(8.0f, 8.0f)));

    QueryOut out;
    grid->QueryCircle(Circle(1.0f, Vector2D(50.0f, 50.0f)), out);
    EXPECT_EQ(out.Size(), 0u);
}

// ---------------------------------------------------------------------------
// QueryPoint
// ---------------------------------------------------------------------------

TEST_F(Geometry2D_SpatialGrid, QueryPoint_InsideBounds_Returned)
{
    grid->Insert(1, AARect(Vector2D(5.0f, 5.0f), Vector2D(8.0f, 8.0f)));

    QueryOut out;
    grid->QueryPoint(Vector2D(6.5f, 6.5f), out);
    EXPECT_EQ(out.Size(), 1u);
}

TEST_F(Geometry2D_SpatialGrid, QueryPoint_OutsideBounds_NotReturned)
{
    grid->Insert(1, AARect(Vector2D(5.0f, 5.0f), Vector2D(8.0f, 8.0f)));

    QueryOut out;
    grid->QueryPoint(Vector2D(50.0f, 50.0f), out);
    EXPECT_EQ(out.Size(), 0u);
}

// ---------------------------------------------------------------------------
// Clear
// ---------------------------------------------------------------------------

TEST_F(Geometry2D_SpatialGrid, Clear_AllQueriesReturnNothing)
{
    grid->Insert(1, AARect(Vector2D(5.0f, 5.0f), Vector2D(8.0f, 8.0f)));
    grid->Insert(2, AARect(Vector2D(15.0f, 15.0f), Vector2D(18.0f, 18.0f)));
    grid->Clear();

    QueryOut out;
    grid->QueryRegion(AARect(Vector2D(0.0f, 0.0f), Vector2D(100.0f, 100.0f)), out);
    EXPECT_EQ(out.Size(), 0u);
}

// ---------------------------------------------------------------------------
// ISpatialStructure substitution — same results as brute force
// ---------------------------------------------------------------------------

TEST_F(Geometry2D_SpatialGrid, MultipleObjects_AllReturnedByFullWorldQuery)
{
    grid->Insert(1, AARect(Vector2D(5.0f, 5.0f), Vector2D(8.0f, 8.0f)));
    grid->Insert(2, AARect(Vector2D(25.0f, 25.0f), Vector2D(28.0f, 28.0f)));
    grid->Insert(3, AARect(Vector2D(75.0f, 75.0f), Vector2D(78.0f, 78.0f)));

    QueryOut out;
    grid->QueryRegion(AARect(Vector2D(0.0f, 0.0f), Vector2D(100.0f, 100.0f)), out);
    EXPECT_EQ(out.Size(), 3u);
}

// ---------------------------------------------------------------------------
// GetObjectCount — tracks insertions and removals
// ---------------------------------------------------------------------------

TEST_F(Geometry2D_SpatialGrid, GetObjectCount_AfterInsert_IncreasesCount)
{
    EXPECT_EQ(grid->GetObjectCount(), 0);
    grid->Insert(1, AARect(Vector2D(5.0f, 5.0f), Vector2D(8.0f, 8.0f)));
    EXPECT_EQ(grid->GetObjectCount(), 1);
    grid->Insert(2, AARect(Vector2D(15.0f, 15.0f), Vector2D(18.0f, 18.0f)));
    EXPECT_EQ(grid->GetObjectCount(), 2);
}

TEST_F(Geometry2D_SpatialGrid, GetObjectCount_AfterRemove_DecreasesCount)
{
    HandleType h1 = grid->Insert(1, AARect(Vector2D(5.0f, 5.0f), Vector2D(8.0f, 8.0f)));
    grid->Insert(2, AARect(Vector2D(15.0f, 15.0f), Vector2D(18.0f, 18.0f)));
    EXPECT_EQ(grid->GetObjectCount(), 2);
    grid->Remove(h1);
    EXPECT_EQ(grid->GetObjectCount(), 1);
}

TEST_F(Geometry2D_SpatialGrid, GetObjectCount_AfterClear_IsZero)
{
    grid->Insert(1, AARect(Vector2D(5.0f, 5.0f), Vector2D(8.0f, 8.0f)));
    grid->Insert(2, AARect(Vector2D(25.0f, 25.0f), Vector2D(28.0f, 28.0f)));
    grid->Clear();
    EXPECT_EQ(grid->GetObjectCount(), 0);
}

// ---------------------------------------------------------------------------
// Insert / Remove / Insert — generation should advance
// ---------------------------------------------------------------------------

TEST_F(Geometry2D_SpatialGrid, InsertRemoveInsert_HandleGenerationChanges)
{
    HandleType h1 = grid->Insert(1, AARect(Vector2D(5.0f, 5.0f), Vector2D(8.0f, 8.0f)));
    grid->Remove(h1);
    HandleType h2 = grid->Insert(2, AARect(Vector2D(5.0f, 5.0f), Vector2D(8.0f, 8.0f)));

    // The old handle must be stale after the slot was recycled.
    EXPECT_EQ(grid->Resolve(h1), nullptr);

    // The new handle should resolve correctly.
    const int* obj = grid->Resolve(h2);
    ASSERT_NE(obj, nullptr);
    EXPECT_EQ(*obj, 2);
}

// ---------------------------------------------------------------------------
// Update — handle remains valid after moving object
// ---------------------------------------------------------------------------

TEST_F(Geometry2D_SpatialGrid, Update_HandleRemainsValid_AfterUpdate)
{
    HandleType h = grid->Insert(42, AARect(Vector2D(5.0f, 5.0f), Vector2D(8.0f, 8.0f)));
    EXPECT_TRUE(h.IsValid());
    grid->Update(h, AARect(Vector2D(55.0f, 55.0f), Vector2D(58.0f, 58.0f)));

    // The handle should still resolve to the same object.
    const int* obj = grid->Resolve(h);
    ASSERT_NE(obj, nullptr);
    EXPECT_EQ(*obj, 42);
}

// ---------------------------------------------------------------------------
// QueryRegion — multiple objects in same cell
// ---------------------------------------------------------------------------

TEST_F(Geometry2D_SpatialGrid, QueryRegion_MultipleObjectsInSameCell_AllReturned)
{
    // Both objects fit within the same cell (0–10 region).
    grid->Insert(1, AARect(Vector2D(1.0f, 1.0f), Vector2D(3.0f, 3.0f)));
    grid->Insert(2, AARect(Vector2D(4.0f, 4.0f), Vector2D(6.0f, 6.0f)));
    grid->Insert(3, AARect(Vector2D(2.0f, 2.0f), Vector2D(5.0f, 5.0f)));

    QueryOut out;
    grid->QueryRegion(AARect(Vector2D(0.0f, 0.0f), Vector2D(10.0f, 10.0f)), out);
    EXPECT_EQ(out.Size(), 3u);
}

TEST_F(Geometry2D_SpatialGrid, QueryRegion_PartialOverlap_OnlyOverlappingReturned)
{
    // Object A is in the lower-left region, object B is in the upper-right region.
    grid->Insert(1, AARect(Vector2D(5.0f, 5.0f), Vector2D(8.0f, 8.0f)));
    grid->Insert(2, AARect(Vector2D(55.0f, 55.0f), Vector2D(58.0f, 58.0f)));

    // Query only covers the lower-left — should find only object 1.
    QueryOut out;
    grid->QueryRegion(AARect(Vector2D(0.0f, 0.0f), Vector2D(20.0f, 20.0f)), out);
    EXPECT_EQ(out.Size(), 1u);

    const int* obj = grid->Resolve(out[0]);
    ASSERT_NE(obj, nullptr);
    EXPECT_EQ(*obj, 1);
}

// ---------------------------------------------------------------------------
// QueryPoint — boundary / edge
// ---------------------------------------------------------------------------

TEST_F(Geometry2D_SpatialGrid, QueryPoint_AtCellBoundary_ReturnsObjectInBoth)
{
    // Object spanning the cell boundary at x=10. A point query exactly at x=10
    // (the boundary) should still find the object.
    grid->Insert(7, AARect(Vector2D(8.0f, 8.0f), Vector2D(12.0f, 12.0f)));

    QueryOut out;
    grid->QueryPoint(Vector2D(10.0f, 10.0f), out);
    EXPECT_GE(out.Size(), 1u);
}

TEST_F(Geometry2D_SpatialGrid, QueryPoint_NoObjectsInserted_ReturnsEmpty)
{
    QueryOut out;
    grid->QueryPoint(Vector2D(50.0f, 50.0f), out);
    EXPECT_EQ(out.Size(), 0u);
}

// ---------------------------------------------------------------------------
// Clear then insert
// ---------------------------------------------------------------------------

TEST_F(Geometry2D_SpatialGrid, Clear_ThenInsert_Works)
{
    grid->Insert(10, AARect(Vector2D(5.0f, 5.0f), Vector2D(8.0f, 8.0f)));
    grid->Clear();

    HandleType h = grid->Insert(20, AARect(Vector2D(5.0f, 5.0f), Vector2D(8.0f, 8.0f)));
    EXPECT_TRUE(h.IsValid());

    const int* obj = grid->Resolve(h);
    ASSERT_NE(obj, nullptr);
    EXPECT_EQ(*obj, 20);
}

// ---------------------------------------------------------------------------
// QueryCircle — additional cases
// ---------------------------------------------------------------------------

TEST_F(Geometry2D_SpatialGrid, QueryCircle_MultipleObjects_ReturnsAllIntersecting)
{
    grid->Insert(1, AARect(Vector2D(5.0f, 5.0f), Vector2D(8.0f, 8.0f)));
    grid->Insert(2, AARect(Vector2D(12.0f, 12.0f), Vector2D(15.0f, 15.0f)));
    grid->Insert(3, AARect(Vector2D(80.0f, 80.0f), Vector2D(85.0f, 85.0f)));

    // Big circle centred at (10,10) with radius 15 — should cover objects 1 and 2.
    QueryOut out;
    grid->QueryCircle(Circle(15.0f, Vector2D(10.0f, 10.0f)), out);
    EXPECT_GE(out.Size(), 2u);
}

TEST_F(Geometry2D_SpatialGrid, QueryCircle_EmptyGrid_ReturnsEmpty)
{
    QueryOut out;
    grid->QueryCircle(Circle(50.0f, Vector2D(50.0f, 50.0f)), out);
    EXPECT_EQ(out.Size(), 0u);
}

// ---------------------------------------------------------------------------
// Small-capacity grid — insert to capacity (MaxObjects = 4)
// ---------------------------------------------------------------------------

using SmallGrid     = SpatialGrid<int, 4>;
using SmallHandleType = Dia::Core::Handle<int>;
using SmallQueryOut = Dia::Core::Containers::DynamicArrayC<SmallHandleType, kMaxQueryResults>;

static SmallGrid::Def MakeSmallWorldDef()
{
    SmallGrid::Def def;
    def.worldBounds = AARect(Vector2D(0.0f, 0.0f), Vector2D(100.0f, 100.0f));
    def.cellSize = 10.0f;
    return def;
}

class Geometry2D_SmallSpatialGrid : public ::testing::Test
{
protected:
    SmallGrid* grid = nullptr;

    void SetUp() override
    {
        grid = new SmallGrid(MakeSmallWorldDef());
    }

    void TearDown() override
    {
        delete grid;
        grid = nullptr;
    }
};

TEST_F(Geometry2D_SmallSpatialGrid, Insert_FillSlotsToMax_AllResolvable)
{
    SmallHandleType handles[4];
    handles[0] = grid->Insert(10, AARect(Vector2D( 5.0f,  5.0f), Vector2D( 8.0f,  8.0f)));
    handles[1] = grid->Insert(20, AARect(Vector2D(15.0f, 15.0f), Vector2D(18.0f, 18.0f)));
    handles[2] = grid->Insert(30, AARect(Vector2D(25.0f, 25.0f), Vector2D(28.0f, 28.0f)));
    handles[3] = grid->Insert(40, AARect(Vector2D(35.0f, 35.0f), Vector2D(38.0f, 38.0f)));

    int expected[] = { 10, 20, 30, 40 };
    for (int i = 0; i < 4; ++i)
    {
        EXPECT_TRUE(handles[i].IsValid());
        const int* obj = grid->Resolve(handles[i]);
        ASSERT_NE(obj, nullptr);
        EXPECT_EQ(*obj, expected[i]);
    }
}

TEST_F(Geometry2D_SmallSpatialGrid, GetObjectCount_Tracks4Objects)
{
    grid->Insert(1, AARect(Vector2D( 5.0f,  5.0f), Vector2D( 8.0f,  8.0f)));
    grid->Insert(2, AARect(Vector2D(15.0f, 15.0f), Vector2D(18.0f, 18.0f)));
    grid->Insert(3, AARect(Vector2D(25.0f, 25.0f), Vector2D(28.0f, 28.0f)));
    grid->Insert(4, AARect(Vector2D(35.0f, 35.0f), Vector2D(38.0f, 38.0f)));
    EXPECT_EQ(grid->GetObjectCount(), 4);
}
