#include <gtest/gtest.h>

#include <DiaRigidBody2D/World/PhysicsWorld.h>
#include <DiaGeometry2D/Spatial/SpatialGrid.h>
#include <DiaGeometry2D/Shapes/AARect.h>
#include <DiaGeometry2D/Shapes/Circle.h>
#include <DiaGeometry2D/Shapes/Ray.h>
#include <DiaGeometry2D/Transform/Transform.h>
#include <DiaMaths/Vector/Vector2D.h>

using namespace Dia::RigidBody2D;
using namespace Dia::Maths;

using Grid = Dia::Geometry2D::SpatialGrid<Body2DBase*>;

// ---------------------------------------------------------------------------
// Fixture — 200x200 world, 10-unit cells, no gravity
// ---------------------------------------------------------------------------

struct QueryFixture {
    Grid*         grid  = nullptr;
    PhysicsWorld* world = nullptr;

    QueryFixture()
    {
        Grid::Def gd;
        gd.worldBounds = Dia::Geometry2D::AARect(Vector2D(-100.0f, -100.0f), Vector2D(100.0f, 100.0f));
        gd.cellSize    = 10.0f;
        grid = new Grid(gd);

        WorldDef wd;
        wd.gravity       = Vector2D(0.0f, 0.0f);
        wd.fixedTimestep = 1.0f / 60.0f;
        wd.maxSubSteps   = 1;
        wd.broadPhase    = grid;
        world = new PhysicsWorld(wd);
    }

    ~QueryFixture() { delete world; delete grid; }

    PointBody2D* AddCircle(Dia::Geometry2D::Transform* t,
                           const Dia::Geometry2D::Circle* circle)
    {
        PointBodyDef def;
        def.transform   = t;
        def.circleShape = circle;
        def.mass        = 1.0f;
        def.type        = BodyType::kStatic;
        return world->AddPointBody(def);
    }

};

// ---------------------------------------------------------------------------
// Test 1 — Raycast hits a single body; correct hit, distance, normal
// ---------------------------------------------------------------------------

TEST(RigidBody2D_SpatialQueries, Raycast_HitsSingleBody)
{
    QueryFixture f;

    Dia::Geometry2D::Transform t;
    t.SetWorldPosition(Vector2D(10.0f, 0.0f));
    Dia::Geometry2D::Circle circ(2.0f, Vector2D::Zero());
    PointBody2D* body = f.AddCircle(&t, &circ);

    Dia::Geometry2D::Ray ray(Vector2D(0.0f, 0.0f), Vector2D(1.0f, 0.0f));
    RaycastHit hit;

    bool result = f.world->Raycast(ray, hit);

    EXPECT_TRUE(result);
    ASSERT_NE(hit.body, nullptr);
    EXPECT_EQ(hit.body, static_cast<const Body2DBase*>(body));
    EXPECT_NEAR(hit.distance, 8.0f, 0.1f);  // hit at left edge of circle (10 - 2 = 8)
    // Normal should point roughly in -x (ray enters from the left)
    EXPECT_LT(hit.normal.x, 0.0f);
}

// ---------------------------------------------------------------------------
// Test 2 — Raycast returns nearest of two bodies along ray
// ---------------------------------------------------------------------------

TEST(RigidBody2D_SpatialQueries, Raycast_ReturnsNearest)
{
    QueryFixture f;

    Dia::Geometry2D::Transform tA, tB;
    tA.SetWorldPosition(Vector2D(5.0f, 0.0f));
    tB.SetWorldPosition(Vector2D(20.0f, 0.0f));

    Dia::Geometry2D::Circle circA(1.0f, Vector2D::Zero());
    Dia::Geometry2D::Circle circB(1.0f, Vector2D::Zero());

    PointBody2D* nearBody = f.AddCircle(&tA, &circA);
    f.AddCircle(&tB, &circB);

    Dia::Geometry2D::Ray ray(Vector2D(0.0f, 0.0f), Vector2D(1.0f, 0.0f));
    RaycastHit hit;

    ASSERT_TRUE(f.world->Raycast(ray, hit));
    EXPECT_EQ(hit.body, static_cast<const Body2DBase*>(nearBody));
    EXPECT_LT(hit.distance, 10.0f);
}

// ---------------------------------------------------------------------------
// Test 3 — Raycast misses all bodies; returns false
// ---------------------------------------------------------------------------

TEST(RigidBody2D_SpatialQueries, Raycast_MissesAll_ReturnsFalse)
{
    QueryFixture f;

    Dia::Geometry2D::Transform t;
    t.SetWorldPosition(Vector2D(0.0f, 10.0f));
    Dia::Geometry2D::Circle circ(1.0f, Vector2D::Zero());
    f.AddCircle(&t, &circ);

    Dia::Geometry2D::Ray ray(Vector2D(0.0f, 0.0f), Vector2D(1.0f, 0.0f));  // along x-axis
    RaycastHit hit;

    EXPECT_FALSE(f.world->Raycast(ray, hit));
    EXPECT_EQ(hit.body, nullptr);
}

// ---------------------------------------------------------------------------
// Test 4 — QueryRegion returns only overlapping bodies
// ---------------------------------------------------------------------------

TEST(RigidBody2D_SpatialQueries, QueryRegion_OnlyOverlapping)
{
    QueryFixture f;

    Dia::Geometry2D::Transform tA, tB;
    tA.SetWorldPosition(Vector2D(5.0f, 5.0f));
    tB.SetWorldPosition(Vector2D(50.0f, 50.0f));

    Dia::Geometry2D::Circle circA(1.0f, Vector2D::Zero());
    Dia::Geometry2D::Circle circB(1.0f, Vector2D::Zero());

    PointBody2D* inBody = f.AddCircle(&tA, &circA);
    f.AddCircle(&tB, &circB);

    Dia::Geometry2D::AARect region(Vector2D(0.0f, 0.0f), Vector2D(10.0f, 10.0f));
    Dia::Core::Containers::DynamicArrayC<const Body2DBase*, kMaxQueryResults> results;
    f.world->QueryRegion(region, results);

    ASSERT_EQ(results.Size(), 1u);
    EXPECT_EQ(results[0], static_cast<const Body2DBase*>(inBody));
}

// ---------------------------------------------------------------------------
// Test 5 — QueryCircle returns only bodies within radius
// ---------------------------------------------------------------------------

TEST(RigidBody2D_SpatialQueries, QueryCircle_OnlyWithinRadius)
{
    QueryFixture f;

    Dia::Geometry2D::Transform tA, tB;
    tA.SetWorldPosition(Vector2D(3.0f, 0.0f));
    tB.SetWorldPosition(Vector2D(30.0f, 0.0f));

    Dia::Geometry2D::Circle circA(1.0f, Vector2D::Zero());
    Dia::Geometry2D::Circle circB(1.0f, Vector2D::Zero());

    PointBody2D* inBody = f.AddCircle(&tA, &circA);
    f.AddCircle(&tB, &circB);

    Dia::Geometry2D::Circle query(8.0f, Vector2D(0.0f, 0.0f));
    Dia::Core::Containers::DynamicArrayC<const Body2DBase*, kMaxQueryResults> results;
    f.world->QueryCircle(query, results);

    ASSERT_EQ(results.Size(), 1u);
    EXPECT_EQ(results[0], static_cast<const Body2DBase*>(inBody));
}

// ---------------------------------------------------------------------------
// Test 6 — Empty world returns nothing / false for all queries
// ---------------------------------------------------------------------------

TEST(RigidBody2D_SpatialQueries, EmptyWorld_AllQueriesEmpty)
{
    QueryFixture f;

    Dia::Geometry2D::Ray ray(Vector2D(0.0f, 0.0f), Vector2D(1.0f, 0.0f));
    RaycastHit hit;
    EXPECT_FALSE(f.world->Raycast(ray, hit));

    Dia::Core::Containers::DynamicArrayC<const Body2DBase*, kMaxQueryResults> results;
    Dia::Geometry2D::AARect region(Vector2D(-10.0f, -10.0f), Vector2D(10.0f, 10.0f));
    f.world->QueryRegion(region, results);
    EXPECT_EQ(results.Size(), 0u);

    Dia::Geometry2D::Circle query(10.0f, Vector2D(0.0f, 0.0f));
    f.world->QueryCircle(query, results);
    EXPECT_EQ(results.Size(), 0u);
}

// ---------------------------------------------------------------------------
// Test 7 — Body removed; queries no longer return it
// ---------------------------------------------------------------------------

TEST(RigidBody2D_SpatialQueries, RemovedBody_NotReturned)
{
    QueryFixture f;

    Dia::Geometry2D::Transform t;
    t.SetWorldPosition(Vector2D(5.0f, 0.0f));
    Dia::Geometry2D::Circle circ(2.0f, Vector2D::Zero());
    PointBody2D* body = f.AddCircle(&t, &circ);

    // Confirm it's there first
    Dia::Core::Containers::DynamicArrayC<const Body2DBase*, kMaxQueryResults> results;
    Dia::Geometry2D::AARect region(Vector2D(0.0f, -5.0f), Vector2D(10.0f, 5.0f));
    f.world->QueryRegion(region, results);
    ASSERT_EQ(results.Size(), 1u);

    // Remove it
    f.world->RemovePointBody(body);

    results.RemoveAll();
    f.world->QueryRegion(region, results);
    EXPECT_EQ(results.Size(), 0u);
}
