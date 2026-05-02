#include <gtest/gtest.h>

#include <DiaGeometry2D/Spatial/Quadtree.h>
#include <DiaGeometry2D/Spatial/ISpatialStructure.h>
#include <DiaGeometry2D/Shapes/AARect.h>
#include <DiaGeometry2D/Shapes/Circle.h>
#include <DiaGeometry2D/Shapes/Ray.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Containers/Handle.h>
#include <DiaMaths/Vector/Vector2D.h>

using namespace Dia::Geometry2D;
using namespace Dia::Maths;

using QTree    = Quadtree<int>;
using Handle   = Dia::Core::Handle<int>;
using QueryOut = Dia::Core::Containers::DynamicArrayC<Handle, kMaxQueryResults>;

// Quadtree has a large inline footprint — heap-allocate via fixture
class Geometry2D_Quadtree : public ::testing::Test
{
protected:
    QTree* qt = nullptr;

    void SetUp() override
    {
        QTree::Def def;
        def.worldBounds    = AARect(Vector2D(-100.0f, -100.0f), Vector2D(100.0f, 100.0f));
        def.splitThreshold = 4;
        def.maxDepth       = 8;
        qt = new QTree(def);
    }

    void TearDown() override { delete qt; qt = nullptr; }
};

// ---------------------------------------------------------------------------
// Insert / Resolve
// ---------------------------------------------------------------------------

TEST_F(Geometry2D_Quadtree, Insert_ReturnsValidHandle)
{
    Handle h = qt->Insert(42, AARect(Vector2D(0.0f, 0.0f), Vector2D(2.0f, 2.0f)));
    EXPECT_TRUE(h.IsValid());
}

TEST_F(Geometry2D_Quadtree, Insert_ThenResolve_ReturnsObject)
{
    Handle h = qt->Insert(99, AARect(Vector2D(1.0f, 1.0f), Vector2D(3.0f, 3.0f)));
    const int* obj = qt->Resolve(h);
    ASSERT_NE(obj, nullptr);
    EXPECT_EQ(*obj, 99);
}

TEST_F(Geometry2D_Quadtree, DefaultConstruction_ZeroObjects)
{
    EXPECT_EQ(qt->GetObjectCount(), 0);
}

TEST_F(Geometry2D_Quadtree, Insert_IncrementsCount)
{
    qt->Insert(1, AARect(Vector2D(0.0f, 0.0f), Vector2D(1.0f, 1.0f)));
    qt->Insert(2, AARect(Vector2D(5.0f, 5.0f), Vector2D(6.0f, 6.0f)));
    EXPECT_EQ(qt->GetObjectCount(), 2);
}

// ---------------------------------------------------------------------------
// Remove
// ---------------------------------------------------------------------------

TEST_F(Geometry2D_Quadtree, Remove_DecrementsCount)
{
    Handle h = qt->Insert(7, AARect(Vector2D(0.0f, 0.0f), Vector2D(2.0f, 2.0f)));
    EXPECT_EQ(qt->GetObjectCount(), 1);
    qt->Remove(h);
    EXPECT_EQ(qt->GetObjectCount(), 0);
}

TEST_F(Geometry2D_Quadtree, Remove_ThenQuery_ReturnsNothing)
{
    Handle h = qt->Insert(7, AARect(Vector2D(0.0f, 0.0f), Vector2D(2.0f, 2.0f)));
    qt->Remove(h);

    QueryOut results;
    qt->QueryRegion(AARect(Vector2D(-1.0f, -1.0f), Vector2D(3.0f, 3.0f)), results);

    bool found = false;
    for (unsigned int i = 0; i < results.Size(); ++i)
    {
        const int* obj = qt->Resolve(results.At(i));
        if (obj && *obj == 7) { found = true; break; }
    }
    EXPECT_FALSE(found);
}

// ---------------------------------------------------------------------------
// Update
// ---------------------------------------------------------------------------

TEST_F(Geometry2D_Quadtree, Update_MovesObjectToNewLocation)
{
    AARect oldBounds(Vector2D(0.0f, 0.0f), Vector2D(2.0f, 2.0f));
    Handle h = qt->Insert(55, oldBounds);

    AARect newBounds(Vector2D(40.0f, 40.0f), Vector2D(42.0f, 42.0f));
    qt->Update(h, newBounds);

    // Old location should no longer find 55
    QueryOut oldResults;
    qt->QueryRegion(oldBounds, oldResults);
    bool foundOld = false;
    for (unsigned int i = 0; i < oldResults.Size(); ++i)
    {
        const int* obj = qt->Resolve(oldResults.At(i));
        if (obj && *obj == 55) { foundOld = true; break; }
    }
    EXPECT_FALSE(foundOld);

    // New location should find it
    QueryOut newResults;
    qt->QueryRegion(newBounds, newResults);
    bool foundNew = false;
    for (unsigned int i = 0; i < newResults.Size(); ++i)
    {
        const int* obj = qt->Resolve(newResults.At(i));
        if (obj && *obj == 55) { foundNew = true; break; }
    }
    EXPECT_TRUE(foundNew);
}

// ---------------------------------------------------------------------------
// Clear
// ---------------------------------------------------------------------------

TEST_F(Geometry2D_Quadtree, Clear_RemovesAllObjects)
{
    for (int i = 0; i < 5; ++i)
        qt->Insert(i, AARect(Vector2D(0.0f, 0.0f), Vector2D(2.0f, 2.0f)));

    qt->Clear();

    EXPECT_EQ(qt->GetObjectCount(), 0);

    QueryOut results;
    qt->QueryRegion(AARect(Vector2D(-10.0f, -10.0f), Vector2D(10.0f, 10.0f)), results);
    EXPECT_EQ(results.Size(), 0u);
}

// ---------------------------------------------------------------------------
// Rebuild
// ---------------------------------------------------------------------------

TEST_F(Geometry2D_Quadtree, Rebuild_PreservesObjectCount)
{
    for (int i = 0; i < 8; ++i)
    {
        float x = static_cast<float>(i) * 5.0f;
        qt->Insert(i, AARect(Vector2D(x, 0.0f), Vector2D(x + 2.0f, 2.0f)));
    }
    int countBefore = qt->GetObjectCount();
    qt->Rebuild();
    EXPECT_EQ(qt->GetObjectCount(), countBefore);
}

// ---------------------------------------------------------------------------
// QueryRegion
// ---------------------------------------------------------------------------

TEST_F(Geometry2D_Quadtree, QueryRegion_FindsInsertedObject)
{
    qt->Insert(42, AARect(Vector2D(1.0f, 1.0f), Vector2D(3.0f, 3.0f)));

    QueryOut results;
    qt->QueryRegion(AARect(Vector2D(0.0f, 0.0f), Vector2D(5.0f, 5.0f)), results);

    bool found = false;
    for (unsigned int i = 0; i < results.Size(); ++i)
    {
        const int* obj = qt->Resolve(results.At(i));
        if (obj && *obj == 42) { found = true; break; }
    }
    EXPECT_TRUE(found);
}

TEST_F(Geometry2D_Quadtree, QueryRegion_MissesDistantObject)
{
    qt->Insert(99, AARect(Vector2D(50.0f, 50.0f), Vector2D(52.0f, 52.0f)));

    QueryOut results;
    qt->QueryRegion(AARect(Vector2D(0.0f, 0.0f), Vector2D(5.0f, 5.0f)), results);

    bool found = false;
    for (unsigned int i = 0; i < results.Size(); ++i)
    {
        const int* obj = qt->Resolve(results.At(i));
        if (obj && *obj == 99) { found = true; break; }
    }
    EXPECT_FALSE(found);
}

// ---------------------------------------------------------------------------
// QueryCircle
// ---------------------------------------------------------------------------

TEST_F(Geometry2D_Quadtree, QueryCircle_FindsObjectInsideRadius)
{
    qt->Insert(10, AARect(Vector2D(1.0f, 1.0f), Vector2D(3.0f, 3.0f)));

    QueryOut results;
    qt->QueryCircle(Circle(5.0f, Vector2D(0.0f, 0.0f)), results);

    bool found = false;
    for (unsigned int i = 0; i < results.Size(); ++i)
    {
        const int* obj = qt->Resolve(results.At(i));
        if (obj && *obj == 10) { found = true; break; }
    }
    EXPECT_TRUE(found);
}

// ---------------------------------------------------------------------------
// QueryPoint
// ---------------------------------------------------------------------------

TEST_F(Geometry2D_Quadtree, QueryPoint_FindsContainingObject)
{
    qt->Insert(33, AARect(Vector2D(-3.0f, -3.0f), Vector2D(3.0f, 3.0f)));

    QueryOut results;
    qt->QueryPoint(Vector2D(0.5f, 0.5f), results);

    bool found = false;
    for (unsigned int i = 0; i < results.Size(); ++i)
    {
        const int* obj = qt->Resolve(results.At(i));
        if (obj && *obj == 33) { found = true; break; }
    }
    EXPECT_TRUE(found);
}

// ---------------------------------------------------------------------------
// Stress: split threshold exceeded
// ---------------------------------------------------------------------------

TEST_F(Geometry2D_Quadtree, ManyInserts_ExceedSplitThreshold_AllRetrievable)
{
    // Insert more than splitThreshold (4) objects in the same region to force subdivision
    const int N = 20;
    for (int i = 0; i < N; ++i)
    {
        float offset = static_cast<float>(i) * 0.5f;
        qt->Insert(i, AARect(Vector2D(offset, offset), Vector2D(offset + 1.0f, offset + 1.0f)));
    }

    EXPECT_EQ(qt->GetObjectCount(), N);

    QueryOut results;
    qt->QueryRegion(AARect(Vector2D(-1.0f, -1.0f), Vector2D(15.0f, 15.0f)), results);
    EXPECT_GE(results.Size(), static_cast<unsigned int>(N));
}

// ---------------------------------------------------------------------------
// Depth increases with more objects
// ---------------------------------------------------------------------------

TEST_F(Geometry2D_Quadtree, GetDepth_IncreasesWithObjects)
{
    // No objects — depth may be 0 or 1
    int depthEmpty = qt->GetDepth();

    for (int i = 0; i < 32; ++i)
    {
        float x = static_cast<float>(i) * 2.0f;
        qt->Insert(i, AARect(Vector2D(x - 60.0f, x - 60.0f), Vector2D(x - 59.0f, x - 59.0f)));
    }

    int depthFull = qt->GetDepth();
    EXPECT_GE(depthFull, depthEmpty);
}
