#include <gtest/gtest.h>
#include <DiaGeometry2D/Spatial/HexGrid.h>
#include <DiaGeometry2D/Spatial/ISpatialStructure.h>
#include <DiaGeometry2D/Shapes/AARect.h>
#include <DiaGeometry2D/Shapes/Circle.h>
#include <DiaGeometry2D/Shapes/Ray.h>
#include <DiaCore/Containers/Handle.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

using namespace Dia::Geometry2D;
using namespace Dia::Maths;
using HandleType = Dia::Core::Handle<int>;
using Grid       = HexGrid<int>;
using QueryOut   = Dia::Core::Containers::DynamicArrayC<HandleType, kMaxQueryResults>;

static Grid::Def MakeWorldDef()
{
    Grid::Def def;
    def.worldBounds = AARect(Vector2D(0.0f, 0.0f), Vector2D(100.0f, 100.0f));
    def.hexRadius   = 5.0f;
    return def;
}

// HexGrid has a large inline footprint — heap-allocate via fixture.
class Geometry2D_HexGrid : public ::testing::Test
{
protected:
    Grid* grid = nullptr;

    void SetUp() override    { grid = new Grid(MakeWorldDef()); }
    void TearDown() override { delete grid; grid = nullptr; }
};

// ---------------------------------------------------------------------------
// Insert / Resolve
// ---------------------------------------------------------------------------

TEST_F(Geometry2D_HexGrid, Insert_ReturnsValidHandle)
{
    HandleType h = grid->Insert(42, AARect(Vector2D(5.0f, 5.0f), Vector2D(8.0f, 8.0f)));
    EXPECT_TRUE(h.IsValid());
}

TEST_F(Geometry2D_HexGrid, Resolve_ValidHandle_ReturnsObject)
{
    HandleType h = grid->Insert(99, AARect(Vector2D(5.0f, 5.0f), Vector2D(8.0f, 8.0f)));
    const int* obj = grid->Resolve(h);
    ASSERT_NE(obj, nullptr);
    EXPECT_EQ(*obj, 99);
}

TEST_F(Geometry2D_HexGrid, Resolve_InvalidHandle_ReturnsNull)
{
    EXPECT_EQ(grid->Resolve(HandleType::Invalid()), nullptr);
}

// ---------------------------------------------------------------------------
// Remove
// ---------------------------------------------------------------------------

TEST_F(Geometry2D_HexGrid, Remove_ValidHandle_ObjectGoneFromQuery)
{
    HandleType h = grid->Insert(1, AARect(Vector2D(5.0f, 5.0f), Vector2D(8.0f, 8.0f)));
    grid->Remove(h);

    QueryOut out;
    grid->QueryRegion(AARect(Vector2D(0.0f, 0.0f), Vector2D(20.0f, 20.0f)), out);
    EXPECT_EQ(out.Size(), 0u);
}

TEST_F(Geometry2D_HexGrid, Resolve_AfterRemove_StaleHandle_ReturnsNull)
{
    HandleType h = grid->Insert(7, AARect(Vector2D(5.0f, 5.0f), Vector2D(8.0f, 8.0f)));
    grid->Remove(h);
    EXPECT_EQ(grid->Resolve(h), nullptr);
}

// ---------------------------------------------------------------------------
// QueryRegion
// ---------------------------------------------------------------------------

TEST_F(Geometry2D_HexGrid, QueryRegion_CoveringObject_ReturnsHandle)
{
    HandleType h = grid->Insert(1, AARect(Vector2D(5.0f, 5.0f), Vector2D(8.0f, 8.0f)));

    QueryOut out;
    grid->QueryRegion(AARect(Vector2D(0.0f, 0.0f), Vector2D(20.0f, 20.0f)), out);
    EXPECT_GE(out.Size(), 1u);
    EXPECT_EQ(out[0], h);
}

TEST_F(Geometry2D_HexGrid, QueryRegion_NotCovering_ReturnsNothing)
{
    grid->Insert(1, AARect(Vector2D(5.0f, 5.0f), Vector2D(8.0f, 8.0f)));

    QueryOut out;
    grid->QueryRegion(AARect(Vector2D(70.0f, 70.0f), Vector2D(90.0f, 90.0f)), out);
    EXPECT_EQ(out.Size(), 0u);
}

TEST_F(Geometry2D_HexGrid, QueryRegion_ObjectSpanningCells_ReturnedOnce)
{
    // Object that deliberately spans multiple hex cells
    grid->Insert(5, AARect(Vector2D(8.0f, 8.0f), Vector2D(20.0f, 20.0f)));

    QueryOut out;
    grid->QueryRegion(AARect(Vector2D(0.0f, 0.0f), Vector2D(30.0f, 30.0f)), out);
    EXPECT_EQ(out.Size(), 1u);
}

// ---------------------------------------------------------------------------
// Update
// ---------------------------------------------------------------------------

TEST_F(Geometry2D_HexGrid, Update_MovesObjectToNewCells)
{
    HandleType h = grid->Insert(3, AARect(Vector2D(5.0f, 5.0f), Vector2D(8.0f, 8.0f)));
    grid->Update(h, AARect(Vector2D(60.0f, 60.0f), Vector2D(63.0f, 63.0f)));

    QueryOut oldPos;
    grid->QueryRegion(AARect(Vector2D(0.0f, 0.0f), Vector2D(20.0f, 20.0f)), oldPos);
    EXPECT_EQ(oldPos.Size(), 0u);

    QueryOut newPos;
    grid->QueryRegion(AARect(Vector2D(50.0f, 50.0f), Vector2D(80.0f, 80.0f)), newPos);
    EXPECT_GE(newPos.Size(), 1u);
}

TEST_F(Geometry2D_HexGrid, Update_HandleRemainsValid)
{
    HandleType h = grid->Insert(42, AARect(Vector2D(5.0f, 5.0f), Vector2D(8.0f, 8.0f)));
    grid->Update(h, AARect(Vector2D(55.0f, 55.0f), Vector2D(58.0f, 58.0f)));
    const int* obj = grid->Resolve(h);
    ASSERT_NE(obj, nullptr);
    EXPECT_EQ(*obj, 42);
}

// ---------------------------------------------------------------------------
// QueryCircle
// ---------------------------------------------------------------------------

TEST_F(Geometry2D_HexGrid, QueryCircle_ObjectInsideCircle_Returned)
{
    grid->Insert(1, AARect(Vector2D(5.0f, 5.0f), Vector2D(8.0f, 8.0f)));

    QueryOut out;
    grid->QueryCircle(Circle(20.0f, Vector2D(6.5f, 6.5f)), out);
    EXPECT_GE(out.Size(), 1u);
}

TEST_F(Geometry2D_HexGrid, QueryCircle_ObjectOutsideCircle_NotReturned)
{
    grid->Insert(1, AARect(Vector2D(5.0f, 5.0f), Vector2D(8.0f, 8.0f)));

    QueryOut out;
    grid->QueryCircle(Circle(1.0f, Vector2D(80.0f, 80.0f)), out);
    EXPECT_EQ(out.Size(), 0u);
}

// ---------------------------------------------------------------------------
// QueryPoint
// ---------------------------------------------------------------------------

TEST_F(Geometry2D_HexGrid, QueryPoint_InsideBounds_Returned)
{
    grid->Insert(1, AARect(Vector2D(5.0f, 5.0f), Vector2D(8.0f, 8.0f)));

    QueryOut out;
    grid->QueryPoint(Vector2D(6.5f, 6.5f), out);
    EXPECT_GE(out.Size(), 1u);
}

TEST_F(Geometry2D_HexGrid, QueryPoint_OutsideBounds_NotReturned)
{
    grid->Insert(1, AARect(Vector2D(5.0f, 5.0f), Vector2D(8.0f, 8.0f)));

    QueryOut out;
    grid->QueryPoint(Vector2D(80.0f, 80.0f), out);
    EXPECT_EQ(out.Size(), 0u);
}

// ---------------------------------------------------------------------------
// Clear
// ---------------------------------------------------------------------------

TEST_F(Geometry2D_HexGrid, Clear_AllQueriesReturnNothing)
{
    grid->Insert(1, AARect(Vector2D(5.0f, 5.0f), Vector2D(8.0f, 8.0f)));
    grid->Insert(2, AARect(Vector2D(50.0f, 50.0f), Vector2D(55.0f, 55.0f)));
    grid->Clear();

    QueryOut out;
    grid->QueryRegion(AARect(Vector2D(0.0f, 0.0f), Vector2D(100.0f, 100.0f)), out);
    EXPECT_EQ(out.Size(), 0u);
}

TEST_F(Geometry2D_HexGrid, Clear_ThenInsert_Works)
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
// GetObjectCount
// ---------------------------------------------------------------------------

TEST_F(Geometry2D_HexGrid, GetObjectCount_TracksInsertAndRemove)
{
    EXPECT_EQ(grid->GetObjectCount(), 0);
    HandleType h = grid->Insert(1, AARect(Vector2D(5.0f, 5.0f), Vector2D(8.0f, 8.0f)));
    EXPECT_EQ(grid->GetObjectCount(), 1);
    grid->Insert(2, AARect(Vector2D(50.0f, 50.0f), Vector2D(55.0f, 55.0f)));
    EXPECT_EQ(grid->GetObjectCount(), 2);
    grid->Remove(h);
    EXPECT_EQ(grid->GetObjectCount(), 1);
    grid->Clear();
    EXPECT_EQ(grid->GetObjectCount(), 0);
}

// ---------------------------------------------------------------------------
// Handle generation
// ---------------------------------------------------------------------------

TEST_F(Geometry2D_HexGrid, InsertRemoveInsert_HandleGenerationChanges)
{
    HandleType h1 = grid->Insert(1, AARect(Vector2D(5.0f, 5.0f), Vector2D(8.0f, 8.0f)));
    grid->Remove(h1);
    HandleType h2 = grid->Insert(2, AARect(Vector2D(5.0f, 5.0f), Vector2D(8.0f, 8.0f)));

    EXPECT_EQ(grid->Resolve(h1), nullptr);
    const int* obj = grid->Resolve(h2);
    ASSERT_NE(obj, nullptr);
    EXPECT_EQ(*obj, 2);
}

// ---------------------------------------------------------------------------
// MultipleObjects
// ---------------------------------------------------------------------------

TEST_F(Geometry2D_HexGrid, MultipleObjects_AllReturnedByFullWorldQuery)
{
    grid->Insert(1, AARect(Vector2D(5.0f,  5.0f),  Vector2D(8.0f,  8.0f)));
    grid->Insert(2, AARect(Vector2D(50.0f, 50.0f), Vector2D(55.0f, 55.0f)));
    grid->Insert(3, AARect(Vector2D(80.0f, 80.0f), Vector2D(85.0f, 85.0f)));

    QueryOut out;
    grid->QueryRegion(AARect(Vector2D(0.0f, 0.0f), Vector2D(100.0f, 100.0f)), out);
    EXPECT_EQ(out.Size(), 3u);
}

// ---------------------------------------------------------------------------
// Hex-specific: WorldToHex / HexToWorld round-trip
// ---------------------------------------------------------------------------

TEST_F(Geometry2D_HexGrid, WorldToHex_HexToWorld_RoundTrip)
{
    // Convert a world point to a hex and back — the result should be the
    // center of that hex, which is near the original point.
    const Vector2D original(30.0f, 30.0f);
    const HexCoord hex    = grid->WorldToHex(original);
    const Vector2D center = grid->HexToWorld(hex);

    // The center should be within one hex-width of the original
    const float dx = center.x - original.x;
    const float dy = center.y - original.y;
    EXPECT_LT(dx * dx + dy * dy, 100.0f);  // within 10 world units
}

TEST_F(Geometry2D_HexGrid, WorldToHex_SamePointTwice_SameCoord)
{
    const Vector2D pt(45.0f, 45.0f);
    EXPECT_EQ(grid->WorldToHex(pt), grid->WorldToHex(pt));
}

TEST_F(Geometry2D_HexGrid, IsValidHex_InsideGrid_True)
{
    const HexCoord hex = grid->WorldToHex(Vector2D(50.0f, 50.0f));
    EXPECT_TRUE(grid->IsValidHex(hex));
}

TEST_F(Geometry2D_HexGrid, IsValidHex_OutsideGrid_False)
{
    EXPECT_FALSE(grid->IsValidHex(HexCoord{ -999, -999 }));
}

// ---------------------------------------------------------------------------
// Hex-specific: GetNeighbours
// ---------------------------------------------------------------------------

TEST_F(Geometry2D_HexGrid, GetNeighbours_CenterHex_Returns6)
{
    const HexCoord center = grid->WorldToHex(Vector2D(50.0f, 50.0f));
    Dia::Core::Containers::DynamicArrayC<HexCoord, 6> neighbours;
    grid->GetNeighbours(center, neighbours);
    EXPECT_EQ(neighbours.Size(), 6u);
}

TEST_F(Geometry2D_HexGrid, GetNeighbours_EachNeighbourIsDistance1)
{
    const HexCoord center = grid->WorldToHex(Vector2D(50.0f, 50.0f));
    Dia::Core::Containers::DynamicArrayC<HexCoord, 6> neighbours;
    grid->GetNeighbours(center, neighbours);

    for (unsigned int i = 0; i < neighbours.Size(); ++i)
    {
        EXPECT_EQ(grid->HexDistance(center, neighbours[i]), 1);
    }
}

// ---------------------------------------------------------------------------
// Hex-specific: HexDistance
// ---------------------------------------------------------------------------

TEST_F(Geometry2D_HexGrid, HexDistance_SameHex_IsZero)
{
    const HexCoord a = grid->WorldToHex(Vector2D(50.0f, 50.0f));
    EXPECT_EQ(grid->HexDistance(a, a), 0);
}

TEST_F(Geometry2D_HexGrid, HexDistance_Symmetric)
{
    const HexCoord a = grid->WorldToHex(Vector2D(20.0f, 20.0f));
    const HexCoord b = grid->WorldToHex(Vector2D(60.0f, 60.0f));
    EXPECT_EQ(grid->HexDistance(a, b), grid->HexDistance(b, a));
}

// ---------------------------------------------------------------------------
// Hex-specific: GetRing
// ---------------------------------------------------------------------------

TEST_F(Geometry2D_HexGrid, GetRing_Radius0_ReturnsCenter)
{
    const HexCoord center = grid->WorldToHex(Vector2D(50.0f, 50.0f));
    Dia::Core::Containers::DynamicArrayC<HexCoord, 256> ring;
    grid->GetRing(center, 0, ring);
    EXPECT_EQ(ring.Size(), 1u);
    EXPECT_EQ(ring[0], center);
}

TEST_F(Geometry2D_HexGrid, GetRing_Radius1_Returns6)
{
    const HexCoord center = grid->WorldToHex(Vector2D(50.0f, 50.0f));
    Dia::Core::Containers::DynamicArrayC<HexCoord, 256> ring;
    grid->GetRing(center, 1, ring);
    // All 6 ring hexes should be valid and distance 1 from center
    for (unsigned int i = 0; i < ring.Size(); ++i)
    {
        EXPECT_TRUE(grid->IsValidHex(ring[i]));
        EXPECT_EQ(grid->HexDistance(center, ring[i]), 1);
    }
    EXPECT_EQ(ring.Size(), 6u);
}

TEST_F(Geometry2D_HexGrid, GetRing_AllHexesDistanceMatchesRadius)
{
    const HexCoord center = grid->WorldToHex(Vector2D(50.0f, 50.0f));
    constexpr int kRadius = 2;
    Dia::Core::Containers::DynamicArrayC<HexCoord, 256> ring;
    grid->GetRing(center, kRadius, ring);

    for (unsigned int i = 0; i < ring.Size(); ++i)
    {
        EXPECT_EQ(grid->HexDistance(center, ring[i]), kRadius);
    }
}

// ---------------------------------------------------------------------------
// Hex-specific: QueryHex
// ---------------------------------------------------------------------------

TEST_F(Geometry2D_HexGrid, QueryHex_CellContainingObject_ReturnsHandle)
{
    const Vector2D pt(50.0f, 50.0f);
    const HexCoord hex = grid->WorldToHex(pt);
    const Vector2D center = grid->HexToWorld(hex);

    // Insert an object centred on that hex
    const float half = 1.0f;
    HandleType h = grid->Insert(77, AARect(
        Vector2D(center.x - half, center.y - half),
        Vector2D(center.x + half, center.y + half)));

    QueryOut out;
    grid->QueryHex(hex, out);
    EXPECT_GE(out.Size(), 1u);
    EXPECT_EQ(out[0], h);
}

TEST_F(Geometry2D_HexGrid, QueryHex_EmptyCell_ReturnsNothing)
{
    const HexCoord hex = grid->WorldToHex(Vector2D(50.0f, 50.0f));
    QueryOut out;
    grid->QueryHex(hex, out);
    EXPECT_EQ(out.Size(), 0u);
}

TEST_F(Geometry2D_HexGrid, QueryHex_InvalidHex_ReturnsNothing)
{
    QueryOut out;
    grid->QueryHex(HexCoord{ -999, -999 }, out);
    EXPECT_EQ(out.Size(), 0u);
}

// ---------------------------------------------------------------------------
// QueryRay
// ---------------------------------------------------------------------------

TEST_F(Geometry2D_HexGrid, QueryRay_ThroughObject_ReturnsHandle)
{
    // Place an object in the middle of the grid and fire a ray through it
    grid->Insert(1, AARect(Vector2D(48.0f, 48.0f), Vector2D(52.0f, 52.0f)));

    QueryOut out;
    // Ray origin well before the object, direction pointing straight at it
    grid->QueryRay(Ray(Vector2D(10.0f, 50.0f), Vector2D(1.0f, 0.0f)), out);
    EXPECT_GE(out.Size(), 1u);
}

TEST_F(Geometry2D_HexGrid, QueryRay_EmptyGrid_ReturnsNothing)
{
    QueryOut out;
    grid->QueryRay(Ray(Vector2D(10.0f, 50.0f), Vector2D(1.0f, 0.0f)), out);
    EXPECT_EQ(out.Size(), 0u);
}

TEST_F(Geometry2D_HexGrid, QueryRay_RayMissingObject_ReturnsNothing)
{
    // Object is at y=80; ray travels along y=10 — should not intersect
    grid->Insert(1, AARect(Vector2D(48.0f, 78.0f), Vector2D(52.0f, 82.0f)));

    QueryOut out;
    grid->QueryRay(Ray(Vector2D(10.0f, 10.0f), Vector2D(1.0f, 0.0f)), out);
    EXPECT_EQ(out.Size(), 0u);
}

TEST_F(Geometry2D_HexGrid, QueryRay_MultipleObjectsInPath_AllReturned)
{
    // Three objects strung along the x axis at y=50
    grid->Insert(1, AARect(Vector2D(20.0f, 48.0f), Vector2D(24.0f, 52.0f)));
    grid->Insert(2, AARect(Vector2D(45.0f, 48.0f), Vector2D(49.0f, 52.0f)));
    grid->Insert(3, AARect(Vector2D(70.0f, 48.0f), Vector2D(74.0f, 52.0f)));

    QueryOut out;
    grid->QueryRay(Ray(Vector2D(5.0f, 50.0f), Vector2D(1.0f, 0.0f)), out);
    EXPECT_GE(out.Size(), 3u);
}

// ---------------------------------------------------------------------------
// QueryKNearest
// ---------------------------------------------------------------------------

TEST_F(Geometry2D_HexGrid, QueryKNearest_K0_ReturnsNothing)
{
    grid->Insert(1, AARect(Vector2D(48.0f, 48.0f), Vector2D(52.0f, 52.0f)));

    QueryOut out;
    grid->QueryKNearest(Vector2D(50.0f, 50.0f), 0, out);
    EXPECT_EQ(out.Size(), 0u);
}

TEST_F(Geometry2D_HexGrid, QueryKNearest_K1_ReturnsClosest)
{
    // Two objects at very different distances from the query point (50, 50)
    HandleType hNear = grid->Insert(1, AARect(Vector2D(48.0f, 48.0f), Vector2D(52.0f, 52.0f)));
    grid->Insert(2, AARect(Vector2D(88.0f, 88.0f), Vector2D(92.0f, 92.0f)));

    QueryOut out;
    grid->QueryKNearest(Vector2D(50.0f, 50.0f), 1, out);
    ASSERT_EQ(out.Size(), 1u);
    EXPECT_EQ(out[0], hNear);
}

TEST_F(Geometry2D_HexGrid, QueryKNearest_KLargerThanObjectCount_ReturnsAll)
{
    grid->Insert(1, AARect(Vector2D(10.0f, 10.0f), Vector2D(12.0f, 12.0f)));
    grid->Insert(2, AARect(Vector2D(50.0f, 50.0f), Vector2D(52.0f, 52.0f)));
    grid->Insert(3, AARect(Vector2D(80.0f, 80.0f), Vector2D(82.0f, 82.0f)));

    QueryOut out;
    grid->QueryKNearest(Vector2D(50.0f, 50.0f), 10, out);
    EXPECT_EQ(out.Size(), 3u);
}

TEST_F(Geometry2D_HexGrid, QueryKNearest_EmptyGrid_ReturnsNothing)
{
    QueryOut out;
    grid->QueryKNearest(Vector2D(50.0f, 50.0f), 5, out);
    EXPECT_EQ(out.Size(), 0u);
}

// ---------------------------------------------------------------------------
// GetNeighbours — grid boundary
// ---------------------------------------------------------------------------

TEST_F(Geometry2D_HexGrid, GetNeighbours_CornerHex_FewerThan6)
{
    // The hex at the bottom-left corner has neighbours that fall outside the grid
    const HexCoord corner = grid->WorldToHex(Vector2D(0.5f, 0.5f));
    Dia::Core::Containers::DynamicArrayC<HexCoord, 6> neighbours;
    grid->GetNeighbours(corner, neighbours);
    EXPECT_LT(neighbours.Size(), 6u);
}

// ---------------------------------------------------------------------------
// GetRing — count
// ---------------------------------------------------------------------------

TEST_F(Geometry2D_HexGrid, GetRing_Radius2_FullyInBounds_Returns12)
{
    // Center of grid — all ring hexes should be in bounds
    const HexCoord center = grid->WorldToHex(Vector2D(50.0f, 50.0f));
    Dia::Core::Containers::DynamicArrayC<HexCoord, 256> ring;
    grid->GetRing(center, 2, ring);
    EXPECT_EQ(ring.Size(), 12u);
}

// ---------------------------------------------------------------------------
// WorldToHex outside world bounds
// ---------------------------------------------------------------------------

TEST_F(Geometry2D_HexGrid, WorldToHex_OutsideBounds_IsValidHex_ReturnsFalse)
{
    const HexCoord hex = grid->WorldToHex(Vector2D(-50.0f, -50.0f));
    EXPECT_FALSE(grid->IsValidHex(hex));
}

// ---------------------------------------------------------------------------
// ISpatialStructure substitution test
// ---------------------------------------------------------------------------

TEST_F(Geometry2D_HexGrid, UsableViaISpatialStructurePointer)
{
    ISpatialStructure<int>* base = grid;
    HandleType h = base->Insert(55, AARect(Vector2D(10.0f, 10.0f), Vector2D(14.0f, 14.0f)));
    EXPECT_TRUE(h.IsValid());

    const int* obj = base->Resolve(h);
    ASSERT_NE(obj, nullptr);
    EXPECT_EQ(*obj, 55);
}

// ---------------------------------------------------------------------------
// Small-capacity grid
// ---------------------------------------------------------------------------

using SmallHexGrid  = HexGrid<int, 4>;
using SmallQueryOut = Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<int>, kMaxQueryResults>;

class Geometry2D_SmallHexGrid : public ::testing::Test
{
protected:
    SmallHexGrid* grid = nullptr;

    void SetUp() override
    {
        SmallHexGrid::Def def;
        def.worldBounds = AARect(Vector2D(0.0f, 0.0f), Vector2D(100.0f, 100.0f));
        def.hexRadius   = 5.0f;
        grid = new SmallHexGrid(def);
    }

    void TearDown() override { delete grid; grid = nullptr; }
};

TEST_F(Geometry2D_SmallHexGrid, Insert_FillSlotsToMax_AllResolvable)
{
    Dia::Core::Handle<int> handles[4];
    handles[0] = grid->Insert(10, AARect(Vector2D( 5.0f,  5.0f), Vector2D( 8.0f,  8.0f)));
    handles[1] = grid->Insert(20, AARect(Vector2D(30.0f, 30.0f), Vector2D(33.0f, 33.0f)));
    handles[2] = grid->Insert(30, AARect(Vector2D(55.0f, 55.0f), Vector2D(58.0f, 58.0f)));
    handles[3] = grid->Insert(40, AARect(Vector2D(80.0f, 80.0f), Vector2D(83.0f, 83.0f)));

    const int expected[] = { 10, 20, 30, 40 };
    for (int i = 0; i < 4; ++i)
    {
        EXPECT_TRUE(handles[i].IsValid());
        const int* obj = grid->Resolve(handles[i]);
        ASSERT_NE(obj, nullptr);
        EXPECT_EQ(*obj, expected[i]);
    }
}
