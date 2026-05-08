#include <gtest/gtest.h>

#include <DiaGeometry2D/Spatial/BVH.h>
#include <DiaGeometry2D/Spatial/ISpatialStructure.h>
#include <DiaGeometry2D/Shapes/AARect.h>
#include <DiaGeometry2D/Shapes/Circle.h>
#include <DiaGeometry2D/Shapes/Ray.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Containers/Handle.h>
#include <DiaMaths/Vector/Vector2D.h>

using namespace Dia::Geometry2D;
using namespace Dia::Maths;

using BVHi       = BVH<int>;
using Handle     = Dia::Core::Handle<int>;
using QueryOut   = Dia::Core::Containers::DynamicArrayC<Handle, kMaxQueryResults>;
using BuildEntry = BVHi::BuildEntry;
using BuildList  = Dia::Core::Containers::DynamicArrayC<BuildEntry, 2048>;

// BVH has a large inline footprint — heap-allocate via fixture
class Geometry2D_BVH : public ::testing::Test
{
protected:
    BVHi* bvh = nullptr;

    void SetUp() override
    {
        BVHi::Def def;
        def.maxLeafObjects = 4;
        bvh = new BVHi(def);
    }

    void TearDown() override { delete bvh; bvh = nullptr; }

    static BuildEntry Entry(int val, float minX, float minY, float maxX, float maxY)
    {
        BuildEntry e;
        e.object = val;
        e.bounds = AARect(Vector2D(minX, minY), Vector2D(maxX, maxY));
        return e;
    }
};

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

TEST_F(Geometry2D_BVH, DefaultConstruction_NotBuilt)
{
    EXPECT_FALSE(bvh->IsBuilt());
    EXPECT_EQ(bvh->GetObjectCount(), 0);
}

// ---------------------------------------------------------------------------
// Build
// ---------------------------------------------------------------------------

TEST_F(Geometry2D_BVH, Build_SingleObject_IsBuilt)
{
    BuildList entries;
    entries.Add(Entry(1, 0.0f, 0.0f, 2.0f, 2.0f));
    bvh->Build(entries);

    EXPECT_TRUE(bvh->IsBuilt());
    EXPECT_EQ(bvh->GetObjectCount(), 1);
}

TEST_F(Geometry2D_BVH, Build_TenObjects_IsBuiltCorrectCount)
{
    BuildList entries;
    for (int i = 0; i < 10; ++i)
    {
        float x = static_cast<float>(i) * 5.0f;
        entries.Add(Entry(i, x, 0.0f, x + 2.0f, 2.0f));
    }
    bvh->Build(entries);

    EXPECT_TRUE(bvh->IsBuilt());
    EXPECT_EQ(bvh->GetObjectCount(), 10);
}

TEST_F(Geometry2D_BVH, Rebuild_UpdatesContent)
{
    BuildList first;
    first.Add(Entry(1, 0.0f, 0.0f, 2.0f, 2.0f));
    bvh->Build(first);
    EXPECT_EQ(bvh->GetObjectCount(), 1);

    BuildList second;
    for (int i = 0; i < 5; ++i)
        second.Add(Entry(i, static_cast<float>(i) * 3.0f, 0.0f, static_cast<float>(i) * 3.0f + 1.0f, 1.0f));
    bvh->Rebuild(second);
    EXPECT_EQ(bvh->GetObjectCount(), 5);
}

// ---------------------------------------------------------------------------
// QueryRegion
// ---------------------------------------------------------------------------

TEST_F(Geometry2D_BVH, QueryRegion_FindsObjectInsideRegion)
{
    BuildList entries;
    entries.Add(Entry(42, 1.0f, 1.0f, 3.0f, 3.0f));
    bvh->Build(entries);

    QueryOut results;
    bvh->QueryRegion(AARect(Vector2D(0.0f, 0.0f), Vector2D(5.0f, 5.0f)), results);

    EXPECT_GE(results.Size(), 1u);
    bool found = false;
    for (unsigned int i = 0; i < results.Size(); ++i)
    {
        const int* obj = bvh->Resolve(results.At(i));
        if (obj && *obj == 42) { found = true; break; }
    }
    EXPECT_TRUE(found);
}

TEST_F(Geometry2D_BVH, QueryRegion_MissesDistantObject)
{
    BuildList entries;
    entries.Add(Entry(99, 50.0f, 50.0f, 52.0f, 52.0f));
    bvh->Build(entries);

    QueryOut results;
    bvh->QueryRegion(AARect(Vector2D(0.0f, 0.0f), Vector2D(5.0f, 5.0f)), results);

    bool found = false;
    for (unsigned int i = 0; i < results.Size(); ++i)
    {
        const int* obj = bvh->Resolve(results.At(i));
        if (obj && *obj == 99) { found = true; break; }
    }
    EXPECT_FALSE(found);
}

TEST_F(Geometry2D_BVH, QueryRegion_MultipleObjects_FindsAll)
{
    BuildList entries;
    for (int i = 0; i < 5; ++i)
    {
        float x = static_cast<float>(i) * 3.0f;
        entries.Add(Entry(i, x, 0.0f, x + 1.0f, 1.0f));
    }
    bvh->Build(entries);

    QueryOut results;
    bvh->QueryRegion(AARect(Vector2D(-1.0f, -1.0f), Vector2D(20.0f, 5.0f)), results);
    EXPECT_GE(results.Size(), 5u);
}

// ---------------------------------------------------------------------------
// QueryCircle
// ---------------------------------------------------------------------------

TEST_F(Geometry2D_BVH, QueryCircle_FindsNearbyObject)
{
    BuildList entries;
    entries.Add(Entry(7, 1.0f, 1.0f, 3.0f, 3.0f));
    bvh->Build(entries);

    QueryOut results;
    bvh->QueryCircle(Circle(5.0f, Vector2D(0.0f, 0.0f)), results);

    bool found = false;
    for (unsigned int i = 0; i < results.Size(); ++i)
    {
        const int* obj = bvh->Resolve(results.At(i));
        if (obj && *obj == 7) { found = true; break; }
    }
    EXPECT_TRUE(found);
}

// ---------------------------------------------------------------------------
// QueryPoint
// ---------------------------------------------------------------------------

TEST_F(Geometry2D_BVH, QueryPoint_FindsContainingObject)
{
    BuildList entries;
    entries.Add(Entry(33, -3.0f, -3.0f, 3.0f, 3.0f));
    bvh->Build(entries);

    QueryOut results;
    bvh->QueryPoint(Vector2D(0.5f, 0.5f), results);

    bool found = false;
    for (unsigned int i = 0; i < results.Size(); ++i)
    {
        const int* obj = bvh->Resolve(results.At(i));
        if (obj && *obj == 33) { found = true; break; }
    }
    EXPECT_TRUE(found);
}

// ---------------------------------------------------------------------------
// Resolve
// ---------------------------------------------------------------------------

TEST_F(Geometry2D_BVH, Resolve_ValidHandle_ReturnsCorrectObject)
{
    BuildList entries;
    entries.Add(Entry(55, 0.0f, 0.0f, 2.0f, 2.0f));
    bvh->Build(entries);

    QueryOut results;
    bvh->QueryRegion(AARect(Vector2D(-1.0f, -1.0f), Vector2D(5.0f, 5.0f)), results);

    ASSERT_GE(results.Size(), 1u);
    const int* obj = bvh->Resolve(results.At(0));
    ASSERT_NE(obj, nullptr);
    EXPECT_EQ(*obj, 55);
}

// ---------------------------------------------------------------------------
// Stats
// ---------------------------------------------------------------------------

TEST_F(Geometry2D_BVH, GetNodeCount_NonZero_AfterBuild)
{
    BuildList entries;
    for (int i = 0; i < 8; ++i)
    {
        float x = static_cast<float>(i) * 2.0f;
        entries.Add(Entry(i, x, 0.0f, x + 1.0f, 1.0f));
    }
    bvh->Build(entries);
    EXPECT_GT(bvh->GetNodeCount(), 0);
}

TEST_F(Geometry2D_BVH, GetDepth_NonZero_WithMultipleObjects)
{
    BuildList entries;
    for (int i = 0; i < 16; ++i)
    {
        float x = static_cast<float>(i) * 3.0f;
        entries.Add(Entry(i, x, 0.0f, x + 1.0f, 1.0f));
    }
    bvh->Build(entries);
    EXPECT_GT(bvh->GetDepth(), 0);
}
