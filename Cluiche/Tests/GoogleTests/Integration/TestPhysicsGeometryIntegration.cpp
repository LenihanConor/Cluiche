#include <gtest/gtest.h>

#include <DiaRigidBody2D/World/PhysicsWorld.h>
#include <DiaRigidBody2D/Bodies/PointBody2D.h>
#include <DiaGeometry2D/Spatial/SpatialGrid.h>
#include <DiaGeometry2D/Shapes/AARect.h>
#include <DiaGeometry2D/Shapes/Circle.h>
#include <DiaGeometry2D/Shapes/Ray.h>
#include <DiaGeometry2D/Transform/Transform.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Containers/Handle.h>
#include <DiaMaths/Vector/Vector2D.h>
#include <DiaCore/CRC/StringCRC.h>

#include <cmath>

using namespace Dia::Geometry2D;
using namespace Dia::Maths;

using Grid       = SpatialGrid<int>;
using GridHandle = Dia::Core::Handle<int>;
using QueryOut   = Dia::Core::Containers::DynamicArrayC<GridHandle, kMaxQueryResults>;

static Dia::RigidBody2D::WorldDef MakeDef(float dt = 1.0f / 60.0f)
{
    Dia::RigidBody2D::WorldDef def;
    def.gravity       = Vector2D(0.0f, 0.0f);
    def.fixedTimestep = dt;
    def.maxSubSteps   = 8;
    def.broadPhase    = nullptr;
    return def;
}

static Grid::Def MakeGridDef()
{
    Grid::Def def;
    def.worldBounds = AARect(Vector2D(-100.0f, -100.0f), Vector2D(100.0f, 100.0f));
    def.cellSize    = 10.0f;
    return def;
}

// SpatialGrid has a ~1 MB inline footprint — heap-allocate via fixture.
class Integration_PhysicsGeometry : public ::testing::Test
{
protected:
    Grid* grid = nullptr;

    void SetUp() override    { grid = new Grid(MakeGridDef()); }
    void TearDown() override { delete grid; grid = nullptr; }
};

// ---------------------------------------------------------------------------
// TEST 1 — SpatialGrid: insert and query by region
// ---------------------------------------------------------------------------

TEST_F(Integration_PhysicsGeometry, SpatialGrid_InsertAndQueryRegion_FindsObject)
{
    AARect bounds(Vector2D(0.0f, 0.0f), Vector2D(2.0f, 2.0f));
    GridHandle h = grid->Insert(42, bounds);
    EXPECT_TRUE(h.IsValid());

    QueryOut results;
    AARect query(Vector2D(-1.0f, -1.0f), Vector2D(3.0f, 3.0f));
    grid->QueryRegion(query, results);

    EXPECT_GE(results.Size(), 1u);
    bool found = false;
    for (unsigned int i = 0; i < results.Size(); ++i)
    {
        const int* obj = grid->Resolve(results.At(i));
        if (obj && *obj == 42) { found = true; break; }
    }
    EXPECT_TRUE(found);
}

// ---------------------------------------------------------------------------
// TEST 2 — SpatialGrid: object outside query region not returned
// ---------------------------------------------------------------------------

TEST_F(Integration_PhysicsGeometry, SpatialGrid_QueryRegion_MissesDistantObject)
{
    AARect bounds(Vector2D(50.0f, 50.0f), Vector2D(52.0f, 52.0f));
    grid->Insert(99, bounds);

    QueryOut results;
    AARect query(Vector2D(0.0f, 0.0f), Vector2D(5.0f, 5.0f));
    grid->QueryRegion(query, results);

    bool found = false;
    for (unsigned int i = 0; i < results.Size(); ++i)
    {
        const int* obj = grid->Resolve(results.At(i));
        if (obj && *obj == 99) { found = true; break; }
    }
    EXPECT_FALSE(found);
}

// ---------------------------------------------------------------------------
// TEST 3 — SpatialGrid: query circle finds nearby objects
// ---------------------------------------------------------------------------

TEST_F(Integration_PhysicsGeometry, SpatialGrid_QueryCircle_FindsObjectInsideRadius)
{
    grid->Insert(10, AARect(Vector2D(1.0f, 1.0f), Vector2D(3.0f, 3.0f)));

    QueryOut results;
    Circle queryCircle(5.0f, Vector2D(0.0f, 0.0f));
    grid->QueryCircle(queryCircle, results);

    bool found = false;
    for (unsigned int i = 0; i < results.Size(); ++i)
    {
        const int* obj = grid->Resolve(results.At(i));
        if (obj && *obj == 10) { found = true; break; }
    }
    EXPECT_TRUE(found);
}

// ---------------------------------------------------------------------------
// TEST 4 — SpatialGrid: remove then query returns nothing
// ---------------------------------------------------------------------------

TEST_F(Integration_PhysicsGeometry, SpatialGrid_RemoveThenQuery_ReturnsNothing)
{
    AARect bounds(Vector2D(0.0f, 0.0f), Vector2D(2.0f, 2.0f));
    GridHandle h = grid->Insert(7, bounds);
    grid->Remove(h);

    QueryOut results;
    AARect query(Vector2D(-1.0f, -1.0f), Vector2D(3.0f, 3.0f));
    grid->QueryRegion(query, results);

    bool found = false;
    for (unsigned int i = 0; i < results.Size(); ++i)
    {
        const int* obj = grid->Resolve(results.At(i));
        if (obj && *obj == 7) { found = true; break; }
    }
    EXPECT_FALSE(found);
}

// ---------------------------------------------------------------------------
// TEST 5 — SpatialGrid: update moves object to new cell
// ---------------------------------------------------------------------------

TEST_F(Integration_PhysicsGeometry, SpatialGrid_Update_MovesObjectToNewLocation)
{
    AARect bounds(Vector2D(0.0f, 0.0f), Vector2D(2.0f, 2.0f));
    GridHandle h = grid->Insert(55, bounds);

    AARect newBounds(Vector2D(40.0f, 40.0f), Vector2D(42.0f, 42.0f));
    grid->Update(h, newBounds);

    // Old location should have no results for 55
    QueryOut oldResults;
    grid->QueryRegion(bounds, oldResults);
    bool foundOld = false;
    for (unsigned int i = 0; i < oldResults.Size(); ++i)
    {
        const int* obj = grid->Resolve(oldResults.At(i));
        if (obj && *obj == 55) { foundOld = true; break; }
    }
    EXPECT_FALSE(foundOld);

    // New location should find it
    QueryOut newResults;
    grid->QueryRegion(newBounds, newResults);
    bool foundNew = false;
    for (unsigned int i = 0; i < newResults.Size(); ++i)
    {
        const int* obj = grid->Resolve(newResults.At(i));
        if (obj && *obj == 55) { foundNew = true; break; }
    }
    EXPECT_TRUE(foundNew);
}

// ---------------------------------------------------------------------------
// TEST 6 — SpatialGrid: query point finds containing object
// ---------------------------------------------------------------------------

TEST_F(Integration_PhysicsGeometry, SpatialGrid_QueryPoint_FindsContainingObject)
{
    grid->Insert(33, AARect(Vector2D(-3.0f, -3.0f), Vector2D(3.0f, 3.0f)));

    QueryOut results;
    grid->QueryPoint(Vector2D(0.5f, 0.5f), results);

    bool found = false;
    for (unsigned int i = 0; i < results.Size(); ++i)
    {
        const int* obj = grid->Resolve(results.At(i));
        if (obj && *obj == 33) { found = true; break; }
    }
    EXPECT_TRUE(found);
}

// ---------------------------------------------------------------------------
// TEST 7 — SpatialGrid: multiple objects, all retrievable
// ---------------------------------------------------------------------------

TEST_F(Integration_PhysicsGeometry, SpatialGrid_MultipleObjects_AllRetrievable)
{
    for (int i = 0; i < 10; ++i)
    {
        float x = static_cast<float>(i) * 5.0f;
        grid->Insert(i, AARect(Vector2D(x, 0.0f), Vector2D(x + 2.0f, 2.0f)));
    }

    QueryOut results;
    grid->QueryRegion(AARect(Vector2D(-1.0f, -1.0f), Vector2D(60.0f, 5.0f)), results);

    EXPECT_GE(results.Size(), 10u);
}

// ---------------------------------------------------------------------------
// TEST 8 — SpatialGrid: clear removes all objects
// ---------------------------------------------------------------------------

TEST_F(Integration_PhysicsGeometry, SpatialGrid_Clear_RemovesAllObjects)
{
    for (int i = 0; i < 5; ++i)
        grid->Insert(i, AARect(Vector2D(0.0f, 0.0f), Vector2D(2.0f, 2.0f)));

    grid->Clear();

    QueryOut results;
    grid->QueryRegion(AARect(Vector2D(-10.0f, -10.0f), Vector2D(10.0f, 10.0f)), results);
    EXPECT_EQ(results.Size(), 0u);
}

// ---------------------------------------------------------------------------
// TEST 9 — Physics + Geometry: body positions can be tracked in a SpatialGrid
// ---------------------------------------------------------------------------

TEST_F(Integration_PhysicsGeometry, PhysicsWorld_BodyPositions_TrackableInSpatialGrid)
{
    Dia::RigidBody2D::PhysicsWorld world(MakeDef());

    Dia::RigidBody2D::PointBodyDef pd0;
    pd0.id            = Dia::Core::StringCRC("GridBody0");
    pd0.type          = Dia::RigidBody2D::BodyType::kDynamic;
    pd0.mass          = 1.0f;
    pd0.allowSleeping = false;
    Dia::RigidBody2D::PointBody2D* b0 = world.AddPointBody(pd0);
    b0->GetTransform()->SetLocalPosition(Vector2D(5.0f, 5.0f));
    b0->SetVelocity(Vector2D(1.0f, 0.0f));

    Dia::RigidBody2D::PointBodyDef pd1;
    pd1.id            = Dia::Core::StringCRC("GridBody1");
    pd1.type          = Dia::RigidBody2D::BodyType::kDynamic;
    pd1.mass          = 1.0f;
    pd1.allowSleeping = false;
    Dia::RigidBody2D::PointBody2D* b1 = world.AddPointBody(pd1);
    b1->GetTransform()->SetLocalPosition(Vector2D(-20.0f, -20.0f));
    b1->SetVelocity(Vector2D(-1.0f, 0.0f));

    auto makeBodyBounds = [](Dia::RigidBody2D::PointBody2D* b) -> AARect {
        Vector2D p = b->GetTransform()->GetLocalPosition();
        return AARect(Vector2D(p.x - 0.5f, p.y - 0.5f), Vector2D(p.x + 0.5f, p.y + 0.5f));
    };

    GridHandle h0 = grid->Insert(0, makeBodyBounds(b0));
    GridHandle h1 = grid->Insert(1, makeBodyBounds(b1));

    world.Update(1.0f / 60.0f);

    grid->Update(h0, makeBodyBounds(b0));
    grid->Update(h1, makeBodyBounds(b1));

    Vector2D b0Pos = b0->GetTransform()->GetLocalPosition();
    QueryOut results;
    grid->QueryCircle(Circle(2.0f, b0Pos), results);

    bool foundB0 = false, foundB1 = false;
    for (unsigned int i = 0; i < results.Size(); ++i)
    {
        const int* obj = grid->Resolve(results.At(i));
        if (obj)
        {
            if (*obj == 0) foundB0 = true;
            if (*obj == 1) foundB1 = true;
        }
    }
    EXPECT_TRUE(foundB0)  << "b0 should be found near its own position";
    EXPECT_FALSE(foundB1) << "b1 is far away, should not be found";
}

// ---------------------------------------------------------------------------
// TEST 10 — SpatialGrid: GetObjectCount matches inserted minus removed
// ---------------------------------------------------------------------------

TEST_F(Integration_PhysicsGeometry, SpatialGrid_ObjectCount_MatchesInsertMinusRemove)
{
    GridHandle h0 = grid->Insert(0, AARect(Vector2D(0.0f, 0.0f), Vector2D(1.0f, 1.0f)));
    GridHandle h1 = grid->Insert(1, AARect(Vector2D(5.0f, 5.0f), Vector2D(6.0f, 6.0f)));
    GridHandle h2 = grid->Insert(2, AARect(Vector2D(10.0f, 10.0f), Vector2D(11.0f, 11.0f)));

    EXPECT_EQ(grid->GetObjectCount(), 3);

    grid->Remove(h1);
    EXPECT_EQ(grid->GetObjectCount(), 2);

    grid->Remove(h0);
    EXPECT_EQ(grid->GetObjectCount(), 1);

    (void)h2;
}
