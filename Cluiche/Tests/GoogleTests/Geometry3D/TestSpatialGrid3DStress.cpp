#include <gtest/gtest.h>
#include <DiaGeometry3D/Spatial/SpatialGrid3D.h>
#include <DiaGeometry3D/Shapes/Frustum.h>
#include <memory>

using namespace Dia::Geometry3D;
using namespace Dia::Maths;
using Dia::Core::Handle;
using Dia::Core::Containers::DynamicArrayC;

// SpatialGrid3D stress and edge-case tests.
// All grid instances are heap-allocated via std::make_unique to avoid stack overflow.

// ============================================================================
// Small grid: 4 objects, 8 cells (2x2x2 at 10-unit cells)
// ============================================================================

using SmallGrid = SpatialGrid3D<int, 4, 8>;
static SmallGrid::Def MakeSmallDef()
{
    SmallGrid::Def d;
    d.worldBounds = AABB(Vector3D(0,0,0), Vector3D(20,20,20));
    d.cellSize    = 10.0f;
    return d;
}

// Default grid: 2048 objects, 1000 cells (10x10x10 at 10-unit cells in a 100³ world)
using Grid = SpatialGrid3D<int>;
static Grid::Def MakeTestDef()
{
    Grid::Def d;
    d.worldBounds = AABB(Vector3D(0,0,0), Vector3D(100,100,100));
    d.cellSize    = 10.0f;
    return d;
}

// Single-cell grid: 2048 objects, 1 cell (giant cell spanning 10³ world)
using SingleCellGrid = SpatialGrid3D<int, 2048, 1>;
static SingleCellGrid::Def MakeSingleCellDef()
{
    SingleCellGrid::Def d;
    d.worldBounds = AABB(Vector3D(0,0,0), Vector3D(10,10,10));
    d.cellSize    = 100.0f;  // one cell that spans the whole world
    return d;
}

// ============================================================================
// Capacity: fill to MaxObjects and cycle
// ============================================================================

TEST(SpatialGrid3DStressTest, FillToCapacity_AllHandlesValid)
{
    auto g = std::make_unique<SmallGrid>(MakeSmallDef());
    AABB b(Vector3D(5,5,5), Vector3D(9,9,9));

    Handle<int> handles[4];
    for (int i = 0; i < 4; ++i)
    {
        handles[i] = g->Insert(i, b);
        EXPECT_TRUE(handles[i].IsValid()) << "slot " << i;
    }
    EXPECT_EQ(g->GetObjectCount(), 4);

    for (int i = 0; i < 4; ++i)
    {
        const int* p = g->Resolve(handles[i]);
        ASSERT_NE(p, nullptr) << "slot " << i;
        EXPECT_EQ(*p, i);
    }
}

TEST(SpatialGrid3DStressTest, FillDrainRefill_SlotReuse)
{
    auto g = std::make_unique<SmallGrid>(MakeSmallDef());
    AABB b(Vector3D(5,5,5), Vector3D(9,9,9));

    Handle<int> first[4];
    for (int i = 0; i < 4; ++i)
        first[i] = g->Insert(i * 10, b);

    for (int i = 0; i < 4; ++i)
        g->Remove(first[i]);

    EXPECT_EQ(g->GetObjectCount(), 0);

    Handle<int> second[4];
    for (int i = 0; i < 4; ++i)
        second[i] = g->Insert(i * 100, b);

    for (int i = 0; i < 4; ++i)
    {
        EXPECT_EQ(g->Resolve(first[i]), nullptr)  << "old handle " << i << " should be stale";
        const int* p = g->Resolve(second[i]);
        ASSERT_NE(p, nullptr) << "new handle " << i;
        EXPECT_EQ(*p, i * 100);
    }
}

TEST(SpatialGrid3DStressTest, ManyInsertRemoveCycles_StaleHandlesNeverResolve)
{
    auto g = std::make_unique<SmallGrid>(MakeSmallDef());
    AABB b(Vector3D(5,5,5), Vector3D(9,9,9));

    Handle<int> prev = Handle<int>::Invalid();

    for (int cycle = 0; cycle < 30; ++cycle)
    {
        Handle<int> h = g->Insert(cycle, b);
        EXPECT_TRUE(h.IsValid()) << "cycle " << cycle;

        if (prev.IsValid())
            EXPECT_EQ(g->Resolve(prev), nullptr) << "cycle " << cycle << " old handle should be stale";

        g->Remove(h);
        EXPECT_EQ(g->Resolve(h), nullptr) << "cycle " << cycle << " removed handle should be stale";
        prev = h;
    }
}

// ============================================================================
// kMaxQueryResults cap: output array never overflows
// ============================================================================

TEST(SpatialGrid3DStressTest, QueryRegion_ExceedsLimit_ResultsCapped)
{
    // MaxObjects=2048 > kMaxQueryResults=1024 — inserting 2048 objects across many cells
    // then querying the whole world must cap at kMaxQueryResults.
    // Each object is 1-unit in a unique area; world is 100³ with cellSize=10 → 1000 cells,
    // kMaxObjectsPerCell=64, so cells never fill individually.
    auto g = std::make_unique<Grid>(MakeTestDef());

    const int kInsert = 2048;
    for (int i = 0; i < kInsert; ++i)
    {
        // Distribute across a 40x8x8 = 2560 positions grid within the world
        float x = static_cast<float>((i % 40)       * 2) + 1.0f;
        float y = static_cast<float>(((i / 40) % 8) * 10) + 1.0f;
        float z = static_cast<float>((i / 320)      * 10) + 1.0f;
        g->Insert(i, AABB(Vector3D(x, y, z), Vector3D(x+0.5f, y+0.5f, z+0.5f)));
    }

    DynamicArrayC<Handle<int>, kMaxQueryResults> out;
    g->QueryRegion(AABB(Vector3D(0,0,0), Vector3D(100,100,100)), out);
    EXPECT_EQ(out.Size(), kMaxQueryResults);
}

TEST(SpatialGrid3DStressTest, QuerySphere_ExceedsLimit_ResultsCapped)
{
    auto g = std::make_unique<Grid>(MakeTestDef());

    const int kInsert = 2048;
    for (int i = 0; i < kInsert; ++i)
    {
        float x = static_cast<float>((i % 40)       * 2) + 1.0f;
        float y = static_cast<float>(((i / 40) % 8) * 10) + 1.0f;
        float z = static_cast<float>((i / 320)      * 10) + 1.0f;
        g->Insert(i, AABB(Vector3D(x, y, z), Vector3D(x+0.5f, y+0.5f, z+0.5f)));
    }

    DynamicArrayC<Handle<int>, kMaxQueryResults> out;
    g->QuerySphere(Sphere(Vector3D(50,50,50), 200.0f), out);
    EXPECT_EQ(out.Size(), kMaxQueryResults);
}

// ============================================================================
// Boundary objects: at world corners and spanning the whole world
// ============================================================================

TEST(SpatialGrid3DStressTest, BoundaryObject_AtWorldMinCorner)
{
    auto g = std::make_unique<Grid>(MakeTestDef());
    Handle<int> h = g->Insert(42, AABB(Vector3D(0,0,0), Vector3D(5,5,5)));
    EXPECT_TRUE(h.IsValid());

    DynamicArrayC<Handle<int>, kMaxQueryResults> out;
    g->QueryRegion(AABB(Vector3D(0,0,0), Vector3D(10,10,10)), out);
    EXPECT_EQ(out.Size(), 1u);
}

TEST(SpatialGrid3DStressTest, BoundaryObject_AtWorldMaxCorner)
{
    auto g = std::make_unique<Grid>(MakeTestDef());
    Handle<int> h = g->Insert(99, AABB(Vector3D(95,95,95), Vector3D(100,100,100)));
    EXPECT_TRUE(h.IsValid());

    DynamicArrayC<Handle<int>, kMaxQueryResults> out;
    g->QueryRegion(AABB(Vector3D(90,90,90), Vector3D(100,100,100)), out);
    EXPECT_EQ(out.Size(), 1u);
}

TEST(SpatialGrid3DStressTest, BoundaryObject_SpanningEntireWorld)
{
    auto g = std::make_unique<Grid>(MakeTestDef());
    g->Insert(1, AABB(Vector3D(0,0,0), Vector3D(100,100,100)));

    DynamicArrayC<Handle<int>, kMaxQueryResults> out;
    g->QueryRegion(AABB(Vector3D(50,50,50), Vector3D(60,60,60)), out);
    EXPECT_EQ(out.Size(), 1u);
}

// ============================================================================
// Out-of-bounds objects: bounds outside world are clamped to border cells
// ============================================================================

TEST(SpatialGrid3DStressTest, OutOfBoundsObject_StillInserts)
{
    auto g = std::make_unique<Grid>(MakeTestDef());
    // Entirely outside world (world is 0..100)
    Handle<int> h = g->Insert(77, AABB(Vector3D(-50,-50,-50), Vector3D(-10,-10,-10)));
    EXPECT_TRUE(h.IsValid());
    EXPECT_EQ(g->GetObjectCount(), 1);

    const int* p = g->Resolve(h);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(*p, 77);
}

TEST(SpatialGrid3DStressTest, OutOfBoundsObject_NotReturnedByQueryInside)
{
    auto g = std::make_unique<Grid>(MakeTestDef());
    g->Insert(77, AABB(Vector3D(-50,-50,-50), Vector3D(-10,-10,-10)));

    // Object bounds don't overlap any point inside the world — query should find nothing
    DynamicArrayC<Handle<int>, kMaxQueryResults> out;
    g->QueryRegion(AABB(Vector3D(0,0,0), Vector3D(100,100,100)), out);
    EXPECT_EQ(out.Size(), 0u);
}

// ============================================================================
// QueryRay: axis-parallel rays
// ============================================================================

TEST(SpatialGrid3DStressTest, QueryRay_ParallelToX_HitsObject)
{
    auto g = std::make_unique<Grid>(MakeTestDef());
    g->Insert(1, AABB(Vector3D(45,45,45), Vector3D(55,55,55)));

    DynamicArrayC<Handle<int>, kMaxQueryResults> out;
    g->QueryRay(Ray(Vector3D(0, 50, 50), Vector3D(1, 0, 0)), out);
    EXPECT_EQ(out.Size(), 1u);
}

TEST(SpatialGrid3DStressTest, QueryRay_ParallelToZ_HitsObject)
{
    auto g = std::make_unique<Grid>(MakeTestDef());
    g->Insert(1, AABB(Vector3D(45,45,45), Vector3D(55,55,55)));

    DynamicArrayC<Handle<int>, kMaxQueryResults> out;
    g->QueryRay(Ray(Vector3D(50, 50, 0), Vector3D(0, 0, 1)), out);
    EXPECT_EQ(out.Size(), 1u);
}

TEST(SpatialGrid3DStressTest, QueryRay_AxisAligned_MissesObject)
{
    auto g = std::make_unique<Grid>(MakeTestDef());
    g->Insert(1, AABB(Vector3D(45,45,45), Vector3D(55,55,55)));

    DynamicArrayC<Handle<int>, kMaxQueryResults> out;
    g->QueryRay(Ray(Vector3D(5, 0, 5), Vector3D(0, 1, 0)), out);  // Y axis, offset away
    EXPECT_EQ(out.Size(), 0u);
}

// ============================================================================
// QueryPoint: outside world, on boundary
// ============================================================================

TEST(SpatialGrid3DStressTest, QueryPoint_OutsideWorld_ReturnsEmpty)
{
    auto g = std::make_unique<Grid>(MakeTestDef());
    g->Insert(1, AABB(Vector3D(5,5,5), Vector3D(9,9,9)));

    DynamicArrayC<Handle<int>, kMaxQueryResults> out;
    g->QueryPoint(Vector3D(-10,-10,-10), out);
    EXPECT_EQ(out.Size(), 0u);
}

TEST(SpatialGrid3DStressTest, QueryPoint_OnObjectBoundaryEdge_ReturnsHit)
{
    auto g = std::make_unique<Grid>(MakeTestDef());
    g->Insert(1, AABB(Vector3D(0,0,0), Vector3D(10,10,10)));

    DynamicArrayC<Handle<int>, kMaxQueryResults> out;
    g->QueryPoint(Vector3D(0,0,0), out);  // exact min corner
    EXPECT_EQ(out.Size(), 1u);
}

// ============================================================================
// QueryKNearest: edge cases
// ============================================================================

TEST(SpatialGrid3DStressTest, QueryKNearest_ZeroK_ReturnsEmpty)
{
    auto g = std::make_unique<Grid>(MakeTestDef());
    g->Insert(1, AABB(Vector3D(5,5,5), Vector3D(9,9,9)));

    DynamicArrayC<Handle<int>, kMaxQueryResults> out;
    g->QueryKNearest(Vector3D(7,7,7), 0, out);
    EXPECT_EQ(out.Size(), 0u);
}

TEST(SpatialGrid3DStressTest, QueryKNearest_EmptyGrid_ReturnsEmpty)
{
    auto g = std::make_unique<Grid>(MakeTestDef());

    DynamicArrayC<Handle<int>, kMaxQueryResults> out;
    g->QueryKNearest(Vector3D(50,50,50), 5, out);
    EXPECT_EQ(out.Size(), 0u);
}

TEST(SpatialGrid3DStressTest, QueryKNearest_PointOutsideWorld_StillFindsObjects)
{
    auto g = std::make_unique<Grid>(MakeTestDef());
    g->Insert(1, AABB(Vector3D(5,5,5), Vector3D(9,9,9)));
    g->Insert(2, AABB(Vector3D(90,90,90), Vector3D(95,95,95)));

    // Origin clamped to grid — nearest should still be found
    DynamicArrayC<Handle<int>, kMaxQueryResults> out;
    g->QueryKNearest(Vector3D(-100,-100,-100), 1, out);
    EXPECT_EQ(out.Size(), 1u);
}

TEST(SpatialGrid3DStressTest, QueryKNearest_NegativeK_ReturnsEmpty)
{
    auto g = std::make_unique<Grid>(MakeTestDef());
    g->Insert(1, AABB(Vector3D(5,5,5), Vector3D(9,9,9)));

    DynamicArrayC<Handle<int>, kMaxQueryResults> out;
    g->QueryKNearest(Vector3D(7,7,7), -1, out);
    EXPECT_EQ(out.Size(), 0u);
}

// ============================================================================
// Update: multiple moves, old regions clear
// ============================================================================

TEST(SpatialGrid3DStressTest, MultipleUpdates_ObjectMovesCorrectly)
{
    auto g = std::make_unique<Grid>(MakeTestDef());
    Handle<int> h = g->Insert(42, AABB(Vector3D(5,5,5), Vector3D(9,9,9)));

    g->Update(h, AABB(Vector3D(25,25,25), Vector3D(29,29,29)));
    g->Update(h, AABB(Vector3D(55,55,55), Vector3D(59,59,59)));
    g->Update(h, AABB(Vector3D(85,85,85), Vector3D(89,89,89)));

    DynamicArrayC<Handle<int>, kMaxQueryResults> inNew;
    g->QueryRegion(AABB(Vector3D(80,80,80), Vector3D(95,95,95)), inNew);
    EXPECT_EQ(inNew.Size(), 1u);

    DynamicArrayC<Handle<int>, kMaxQueryResults> inOld;
    g->QueryRegion(AABB(Vector3D(0,0,0), Vector3D(70,70,70)), inOld);
    EXPECT_EQ(inOld.Size(), 0u);
}

// ============================================================================
// Clear: fill, clear, refill
// ============================================================================

TEST(SpatialGrid3DStressTest, ClearAndRefill_WorksCorrectly)
{
    auto g = std::make_unique<Grid>(MakeTestDef());

    for (int i = 0; i < 50; ++i)
    {
        float fi = static_cast<float>(i);
        g->Insert(i, AABB(Vector3D(fi, fi, fi), Vector3D(fi+4, fi+4, fi+4)));
    }

    g->Clear();
    EXPECT_EQ(g->GetObjectCount(), 0);

    Handle<int> handles[10];
    for (int i = 0; i < 10; ++i)
    {
        float fi = static_cast<float>(i * 5);
        handles[i] = g->Insert(i * 1000, AABB(Vector3D(fi, fi, fi), Vector3D(fi+4, fi+4, fi+4)));
        EXPECT_TRUE(handles[i].IsValid()) << "slot " << i;
    }

    EXPECT_EQ(g->GetObjectCount(), 10);
    for (int i = 0; i < 10; ++i)
    {
        const int* p = g->Resolve(handles[i]);
        ASSERT_NE(p, nullptr) << "slot " << i;
        EXPECT_EQ(*p, i * 1000);
    }
}
