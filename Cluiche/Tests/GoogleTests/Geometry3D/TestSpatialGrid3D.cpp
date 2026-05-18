#include <gtest/gtest.h>
#include <DiaGeometry3D/Spatial/SpatialGrid3D.h>
#include <DiaGeometry3D/Shapes/Frustum.h>
#include <memory>

using namespace Dia::Geometry3D;
using namespace Dia::Maths;
using Dia::Core::Handle;
using Dia::Core::Containers::DynamicArrayC;

// SpatialGrid3D is ~1MB (4096 cells × DynamicArrayC<uint32_t,64>); must be heap-allocated in tests.

using Grid = SpatialGrid3D<int>;

static Grid::Def MakeTestDef()
{
    Grid::Def d;
    d.worldBounds = AABB(Vector3D(0,0,0), Vector3D(100,100,100));
    d.cellSize    = 10.0f;
    return d;
}

// ============================================================================
// Basic construction
// ============================================================================

TEST(SpatialGrid3DBasicTest, Construction_CellCountCorrect)
{
    auto g = std::make_unique<Grid>(MakeTestDef());
    EXPECT_EQ(g->GetCellCountX(), 10);
    EXPECT_EQ(g->GetCellCountY(), 10);
    EXPECT_EQ(g->GetCellCountZ(), 10);
    EXPECT_EQ(g->GetCellCount(), 1000);
    EXPECT_EQ(g->GetObjectCount(), 0);
    EXPECT_NEAR(g->GetCellSize(), 10.0f, 1e-5f);
}

TEST(SpatialGrid3DBasicTest, WorldBounds_Correct)
{
    auto g = std::make_unique<Grid>(MakeTestDef());
    EXPECT_EQ(g->GetWorldBounds().GetMin().X(), 0.0f);
    EXPECT_EQ(g->GetWorldBounds().GetMax().X(), 100.0f);
}

// ============================================================================
// Insert
// ============================================================================

TEST(SpatialGrid3DInsertTest, Insert_ReturnsValidHandle)
{
    auto g = std::make_unique<Grid>(MakeTestDef());
    Handle<int> h = g->Insert(42, AABB(Vector3D(5,5,5), Vector3D(15,15,15)));
    EXPECT_TRUE(h.IsValid());
    EXPECT_EQ(g->GetObjectCount(), 1);
}

TEST(SpatialGrid3DInsertTest, Resolve_ReturnsInsertedObject)
{
    auto g = std::make_unique<Grid>(MakeTestDef());
    Handle<int> h = g->Insert(99, AABB(Vector3D(5,5,5), Vector3D(15,15,15)));
    const int* p = g->Resolve(h);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(*p, 99);
}

TEST(SpatialGrid3DInsertTest, SubsequentInserts_DifferentSlots)
{
    auto g = std::make_unique<Grid>(MakeTestDef());
    AABB b(Vector3D(5,5,5), Vector3D(15,15,15));
    Handle<int> h1 = g->Insert(1, b);
    Handle<int> h2 = g->Insert(2, b);
    EXPECT_TRUE(h1.IsValid());
    EXPECT_TRUE(h2.IsValid());
    EXPECT_NE(h1.GetIndex(), h2.GetIndex());
    EXPECT_EQ(g->GetObjectCount(), 2);
}

// ============================================================================
// Stale handle
// ============================================================================

TEST(SpatialGrid3DStaleHandleTest, StaleHandle_ResolveReturnsNull)
{
    auto g = std::make_unique<Grid>(MakeTestDef());
    AABB b(Vector3D(5,5,5), Vector3D(15,15,15));
    Handle<int> h1 = g->Insert(10, b);
    g->Remove(h1);
    Handle<int> h2 = g->Insert(20, b);
    EXPECT_EQ(g->Resolve(h1), nullptr);
    ASSERT_NE(g->Resolve(h2), nullptr);
    EXPECT_EQ(*g->Resolve(h2), 20);
}

// ============================================================================
// Multi-cell deduplication
// ============================================================================

TEST(SpatialGrid3DMultiCellTest, ObjectSpanningMultipleCells_AppearOnce)
{
    auto g = std::make_unique<Grid>(MakeTestDef());
    g->Insert(1, AABB(Vector3D(0,0,0), Vector3D(25,25,25)));

    DynamicArrayC<Handle<int>, kMaxQueryResults> out;
    g->QueryRegion(AABB(Vector3D(0,0,0), Vector3D(100,100,100)), out);
    EXPECT_EQ(out.Size(), 1u);
}

// ============================================================================
// Update
// ============================================================================

TEST(SpatialGrid3DUpdateTest, UpdateBounds_QueriesReflectNewPosition)
{
    auto g = std::make_unique<Grid>(MakeTestDef());
    Handle<int> h = g->Insert(7, AABB(Vector3D(5,5,5), Vector3D(9,9,9)));
    g->Update(h, AABB(Vector3D(55,55,55), Vector3D(65,65,65)));

    DynamicArrayC<Handle<int>, kMaxQueryResults> outOld, outNew;
    g->QueryRegion(AABB(Vector3D(0,0,0), Vector3D(20,20,20)), outOld);
    g->QueryRegion(AABB(Vector3D(50,50,50), Vector3D(70,70,70)), outNew);

    EXPECT_EQ(outOld.Size(), 0u);
    EXPECT_EQ(outNew.Size(), 1u);
}

// ============================================================================
// QueryRegion
// ============================================================================

TEST(SpatialGrid3DQueryRegionTest, EmptyGrid_ReturnsNothing)
{
    auto g = std::make_unique<Grid>(MakeTestDef());
    DynamicArrayC<Handle<int>, kMaxQueryResults> out;
    g->QueryRegion(AABB(Vector3D(0,0,0), Vector3D(100,100,100)), out);
    EXPECT_EQ(out.Size(), 0u);
}

TEST(SpatialGrid3DQueryRegionTest, QueryContainingAll_ReturnsAll)
{
    auto g = std::make_unique<Grid>(MakeTestDef());
    g->Insert(1, AABB(Vector3D(5,5,5), Vector3D(9,9,9)));
    g->Insert(2, AABB(Vector3D(55,55,55), Vector3D(59,59,59)));
    g->Insert(3, AABB(Vector3D(85,85,85), Vector3D(89,89,89)));

    DynamicArrayC<Handle<int>, kMaxQueryResults> out;
    g->QueryRegion(AABB(Vector3D(0,0,0), Vector3D(100,100,100)), out);
    EXPECT_EQ(out.Size(), 3u);
}

TEST(SpatialGrid3DQueryRegionTest, QueryContainingSome_ReturnsSome)
{
    auto g = std::make_unique<Grid>(MakeTestDef());
    g->Insert(1, AABB(Vector3D(5,5,5), Vector3D(9,9,9)));
    g->Insert(2, AABB(Vector3D(55,55,55), Vector3D(59,59,59)));

    DynamicArrayC<Handle<int>, kMaxQueryResults> out;
    g->QueryRegion(AABB(Vector3D(0,0,0), Vector3D(20,20,20)), out);
    EXPECT_EQ(out.Size(), 1u);
}

// ============================================================================
// QuerySphere
// ============================================================================

TEST(SpatialGrid3DQuerySphereTest, SphereOverlappingAABB_ReturnsHit)
{
    auto g = std::make_unique<Grid>(MakeTestDef());
    g->Insert(1, AABB(Vector3D(5,5,5), Vector3D(15,15,15)));

    DynamicArrayC<Handle<int>, kMaxQueryResults> out;
    g->QuerySphere(Sphere(Vector3D(10,10,10), 5.0f), out);
    EXPECT_EQ(out.Size(), 1u);
}

TEST(SpatialGrid3DQuerySphereTest, SphereNotOverlapping_ReturnsNothing)
{
    auto g = std::make_unique<Grid>(MakeTestDef());
    g->Insert(1, AABB(Vector3D(80,80,80), Vector3D(90,90,90)));

    DynamicArrayC<Handle<int>, kMaxQueryResults> out;
    g->QuerySphere(Sphere(Vector3D(10,10,10), 5.0f), out);
    EXPECT_EQ(out.Size(), 0u);
}

// ============================================================================
// QueryPoint
// ============================================================================

TEST(SpatialGrid3DQueryPointTest, PointInsideAABB_ReturnsHit)
{
    auto g = std::make_unique<Grid>(MakeTestDef());
    g->Insert(1, AABB(Vector3D(5,5,5), Vector3D(15,15,15)));

    DynamicArrayC<Handle<int>, kMaxQueryResults> out;
    g->QueryPoint(Vector3D(10,10,10), out);
    EXPECT_EQ(out.Size(), 1u);
}

TEST(SpatialGrid3DQueryPointTest, PointOutsideAllAABBs_ReturnsNothing)
{
    auto g = std::make_unique<Grid>(MakeTestDef());
    g->Insert(1, AABB(Vector3D(5,5,5), Vector3D(9,9,9)));

    DynamicArrayC<Handle<int>, kMaxQueryResults> out;
    g->QueryPoint(Vector3D(50,50,50), out);
    EXPECT_EQ(out.Size(), 0u);
}

TEST(SpatialGrid3DQueryPointTest, PointOnAABBFace_ReturnsHit)
{
    auto g = std::make_unique<Grid>(MakeTestDef());
    g->Insert(1, AABB(Vector3D(5,5,5), Vector3D(15,15,15)));

    DynamicArrayC<Handle<int>, kMaxQueryResults> out;
    g->QueryPoint(Vector3D(15,10,10), out);
    EXPECT_EQ(out.Size(), 1u);
}

// ============================================================================
// QueryRay
// ============================================================================

TEST(SpatialGrid3DQueryRayTest, RayHittingObject_ReturnsHit)
{
    auto g = std::make_unique<Grid>(MakeTestDef());
    g->Insert(1, AABB(Vector3D(45,45,45), Vector3D(55,55,55)));

    DynamicArrayC<Handle<int>, kMaxQueryResults> out;
    g->QueryRay(Ray(Vector3D(50, 0, 50), Vector3D(0, 1, 0)), out);
    EXPECT_EQ(out.Size(), 1u);
}

TEST(SpatialGrid3DQueryRayTest, RayMissingAll_ReturnsNothing)
{
    auto g = std::make_unique<Grid>(MakeTestDef());
    g->Insert(1, AABB(Vector3D(45,45,45), Vector3D(55,55,55)));

    DynamicArrayC<Handle<int>, kMaxQueryResults> out;
    g->QueryRay(Ray(Vector3D(5, 0, 5), Vector3D(0, 1, 0)), out);
    EXPECT_EQ(out.Size(), 0u);
}

TEST(SpatialGrid3DQueryRayTest, RayStartingInsideAABB_ReturnsHit)
{
    auto g = std::make_unique<Grid>(MakeTestDef());
    g->Insert(1, AABB(Vector3D(45,45,45), Vector3D(55,55,55)));

    DynamicArrayC<Handle<int>, kMaxQueryResults> out;
    g->QueryRay(Ray(Vector3D(50, 50, 50), Vector3D(0, 1, 0)), out);
    EXPECT_EQ(out.Size(), 1u);
}

// ============================================================================
// QueryFrustum
// ============================================================================

TEST(SpatialGrid3DQueryFrustumTest, DefaultFrustum_NoObjectsHit)
{
    auto g = std::make_unique<Grid>(MakeTestDef());
    g->Insert(1, AABB(Vector3D(5,5,5), Vector3D(15,15,15)));
    g->Insert(2, AABB(Vector3D(55,55,55), Vector3D(65,65,65)));

    // Default Frustum is a unit cube centered at origin — objects at [5..65] don't overlap
    Frustum f;
    DynamicArrayC<Handle<int>, kMaxQueryResults> out;
    g->QueryFrustum(f, out);
    EXPECT_EQ(out.Size(), 0u);
}

TEST(SpatialGrid3DQueryFrustumTest, WorldSpanningFrustum_ReturnsAllObjects)
{
    auto g = std::make_unique<Grid>(MakeTestDef());
    g->Insert(1, AABB(Vector3D(5,5,5), Vector3D(9,9,9)));
    g->Insert(2, AABB(Vector3D(50,50,50), Vector3D(54,54,54)));

    // Frustum that spans [0..100]³ with inward normals
    Plane near  (Vector3D( 0, 0, 1),   0.0f);   // z >= 0
    Plane far2  (Vector3D( 0, 0,-1), -100.0f);  // z <= 100  (d = -100)
    Plane left2 (Vector3D( 1, 0, 0),   0.0f);   // x >= 0
    Plane right2(Vector3D(-1, 0, 0), -100.0f);  // x <= 100
    Plane top2  (Vector3D( 0,-1, 0), -100.0f);  // y <= 100
    Plane bot   (Vector3D( 0, 1, 0),   0.0f);   // y >= 0
    Frustum f(near, far2, left2, right2, top2, bot);

    DynamicArrayC<Handle<int>, kMaxQueryResults> out;
    g->QueryFrustum(f, out);
    EXPECT_EQ(out.Size(), 2u);
}

// ============================================================================
// QueryKNearest
// ============================================================================

TEST(SpatialGrid3DKNearestTest, K1_ReturnsNearest)
{
    auto g = std::make_unique<Grid>(MakeTestDef());
    Handle<int> h1 = g->Insert(1, AABB(Vector3D(5,5,5), Vector3D(9,9,9)));
    Handle<int> h2 = g->Insert(2, AABB(Vector3D(50,50,50), Vector3D(54,54,54)));
    Handle<int> h3 = g->Insert(3, AABB(Vector3D(90,90,90), Vector3D(95,95,95)));
    (void)h2; (void)h3;

    DynamicArrayC<Handle<int>, kMaxQueryResults> out;
    g->QueryKNearest(Vector3D(7,7,7), 1, out);
    EXPECT_EQ(out.Size(), 1u);
    EXPECT_EQ(out[0].GetIndex(), h1.GetIndex());
}

TEST(SpatialGrid3DKNearestTest, KMoreThanObjects_ReturnsAll)
{
    auto g = std::make_unique<Grid>(MakeTestDef());
    g->Insert(1, AABB(Vector3D(5,5,5), Vector3D(9,9,9)));
    g->Insert(2, AABB(Vector3D(50,50,50), Vector3D(54,54,54)));

    DynamicArrayC<Handle<int>, kMaxQueryResults> out;
    g->QueryKNearest(Vector3D(30,30,30), 10, out);
    EXPECT_EQ(out.Size(), 2u);
}

TEST(SpatialGrid3DKNearestTest, K3_ResultsOrderedByDistance)
{
    auto g = std::make_unique<Grid>(MakeTestDef());
    g->Insert(1, AABB(Vector3D(5,5,5), Vector3D(9,9,9)));       // center (7,7,7)
    g->Insert(2, AABB(Vector3D(25,25,25), Vector3D(29,29,29)));  // center (27,27,27)
    g->Insert(3, AABB(Vector3D(55,55,55), Vector3D(59,59,59)));  // center (57,57,57)

    DynamicArrayC<Handle<int>, kMaxQueryResults> out;
    g->QueryKNearest(Vector3D(0,0,0), 3, out);
    EXPECT_EQ(out.Size(), 3u);
    // Nearest should be slot with object 1 at (7,7,7)
    ASSERT_NE(g->Resolve(out[0]), nullptr);
    EXPECT_EQ(*g->Resolve(out[0]), 1);
}

// ============================================================================
// Clear
// ============================================================================

TEST(SpatialGrid3DClearTest, Clear_ZerosObjectCount)
{
    auto g = std::make_unique<Grid>(MakeTestDef());
    g->Insert(1, AABB(Vector3D(5,5,5), Vector3D(9,9,9)));
    g->Insert(2, AABB(Vector3D(55,55,55), Vector3D(59,59,59)));
    g->Clear();
    EXPECT_EQ(g->GetObjectCount(), 0);
}

TEST(SpatialGrid3DClearTest, Clear_StaleHandles_ReturnsNull)
{
    auto g = std::make_unique<Grid>(MakeTestDef());
    Handle<int> h = g->Insert(1, AABB(Vector3D(5,5,5), Vector3D(9,9,9)));
    g->Clear();
    EXPECT_EQ(g->Resolve(h), nullptr);
}

TEST(SpatialGrid3DClearTest, Clear_QueryReturnsEmpty)
{
    auto g = std::make_unique<Grid>(MakeTestDef());
    g->Insert(1, AABB(Vector3D(5,5,5), Vector3D(9,9,9)));
    g->Clear();

    DynamicArrayC<Handle<int>, kMaxQueryResults> out;
    g->QueryRegion(AABB(Vector3D(0,0,0), Vector3D(100,100,100)), out);
    EXPECT_EQ(out.Size(), 0u);
}

// ============================================================================
// DenseGrid
// ============================================================================

TEST(SpatialGrid3DDenseGridTest, LargeMaxCells_Constructs)
{
    auto g = std::make_unique<SpatialGrid3D<int, 2048, 32768>>(SpatialGrid3D<int, 2048, 32768>::Def{
        AABB(Vector3D(0,0,0), Vector3D(32,32,32)),
        1.0f
    });
    EXPECT_EQ(g->GetCellCountX(), 32);
    EXPECT_EQ(g->GetCellCountY(), 32);
    EXPECT_EQ(g->GetCellCountZ(), 32);
    EXPECT_EQ(g->GetCellCount(), 32768);
}
