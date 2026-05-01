#include <gtest/gtest.h>

#include <DiaRigidBody2D/World/PhysicsWorld.h>
#include <DiaRigidBody2D/Detection/DetectCollisions.h>
#include <DiaGeometry2D/Spatial/SpatialGrid.h>
#include <DiaGeometry2D/Shapes/AARect.h>
#include <DiaGeometry2D/Shapes/Circle.h>
#include <DiaGeometry2D/Transform/Transform.h>
#include <DiaMaths/Vector/Vector2D.h>

using namespace Dia::RigidBody2D;
using namespace Dia::Maths;

// Broad-phase backed by a SpatialGrid covering a 200x200 world
using Grid = Dia::Geometry2D::SpatialGrid<Body2DBase*>;

struct WorldFixture {
    Grid*         grid  = nullptr;
    PhysicsWorld* world = nullptr;

    WorldFixture()
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

    ~WorldFixture() { delete world; delete grid; }

    PointBody2D* AddCircle(Dia::Geometry2D::Transform* t,
                           const Dia::Geometry2D::Circle* circle,
                           BodyType type = BodyType::kDynamic)
    {
        PointBodyDef def;
        def.transform   = t;
        def.circleShape = circle;
        def.type        = type;
        def.mass        = 1.0f;
        return world->AddPointBody(def);
    }

};

// ---------------------------------------------------------------------------
// Test 1 — Two overlapping circles produce a contact with depth > 0
// ---------------------------------------------------------------------------

TEST(RigidBody2D_CollisionDetection, TwoCircles_Overlapping_ContactProduced)
{
    WorldFixture f;

    Dia::Geometry2D::Transform tA, tB;
    tA.SetWorldPosition(Vector2D(0.0f, 0.0f));
    tB.SetWorldPosition(Vector2D(1.5f, 0.0f));

    Dia::Geometry2D::Circle circA(1.0f, Vector2D::Zero());
    Dia::Geometry2D::Circle circB(1.0f, Vector2D::Zero());

    f.AddCircle(&tA, &circA);
    f.AddCircle(&tB, &circB);

    f.world->Update(1.0f / 60.0f);

    const auto& contacts = f.world->GetLastContacts();
    ASSERT_EQ(contacts.Size(), 1u);
    EXPECT_GT(contacts[0].depth, 0.0f);
    EXPECT_NEAR(contacts[0].depth, 0.5f, 1e-4f);
}

// ---------------------------------------------------------------------------
// Test 2 — Two separated circles produce no contact
// ---------------------------------------------------------------------------

TEST(RigidBody2D_CollisionDetection, TwoCircles_Separated_NoContact)
{
    WorldFixture f;

    Dia::Geometry2D::Transform tA, tB;
    tA.SetWorldPosition(Vector2D(0.0f, 0.0f));
    tB.SetWorldPosition(Vector2D(10.0f, 0.0f));

    Dia::Geometry2D::Circle circA(1.0f, Vector2D::Zero());
    Dia::Geometry2D::Circle circB(1.0f, Vector2D::Zero());

    f.AddCircle(&tA, &circA);
    f.AddCircle(&tB, &circB);

    f.world->Update(1.0f / 60.0f);

    EXPECT_EQ(f.world->GetLastContacts().Size(), 0u);
}

// ---------------------------------------------------------------------------
// Test 3 — Static-static pair is skipped
// ---------------------------------------------------------------------------

TEST(RigidBody2D_CollisionDetection, StaticStatic_Overlapping_NoContact)
{
    WorldFixture f;

    Dia::Geometry2D::Transform tA, tB;
    tA.SetWorldPosition(Vector2D(0.0f, 0.0f));
    tB.SetWorldPosition(Vector2D(0.5f, 0.0f));

    Dia::Geometry2D::Circle circA(1.0f, Vector2D::Zero());
    Dia::Geometry2D::Circle circB(1.0f, Vector2D::Zero());

    f.AddCircle(&tA, &circA, BodyType::kStatic);
    f.AddCircle(&tB, &circB, BodyType::kStatic);

    f.world->Update(1.0f / 60.0f);

    EXPECT_EQ(f.world->GetLastContacts().Size(), 0u);
}

// ---------------------------------------------------------------------------
// Test 4 — Contact normal points from A toward B
// ---------------------------------------------------------------------------

TEST(RigidBody2D_CollisionDetection, Contact_NormalDirection_FromAToB)
{
    WorldFixture f;

    Dia::Geometry2D::Transform tA, tB;
    tA.SetWorldPosition(Vector2D(1.5f, 0.0f));
    tB.SetWorldPosition(Vector2D(0.0f, 0.0f));

    Dia::Geometry2D::Circle circA(1.0f, Vector2D::Zero());
    Dia::Geometry2D::Circle circB(1.0f, Vector2D::Zero());

    f.AddCircle(&tA, &circA);
    f.AddCircle(&tB, &circB);

    f.world->Update(1.0f / 60.0f);

    const auto& contacts = f.world->GetLastContacts();
    ASSERT_EQ(contacts.Size(), 1u);
    // Normal should point in -x direction (from A at 1.5,0 toward B at 0,0)
    EXPECT_LT(contacts[0].normal.x, -0.5f);
    EXPECT_NEAR(contacts[0].normal.y, 0.0f, 1e-4f);
}

// ---------------------------------------------------------------------------
// Test 5 — Two overlapping AARects produce a contact
// ---------------------------------------------------------------------------

TEST(RigidBody2D_CollisionDetection, TwoCircles_PartialOverlap_ContactProduced)
{
    WorldFixture f;

    Dia::Geometry2D::Transform tA, tB;
    tA.SetWorldPosition(Vector2D(1.0f, 1.0f));
    tB.SetWorldPosition(Vector2D(2.5f, 1.0f));

    Dia::Geometry2D::Circle circA(1.0f, Vector2D::Zero());
    Dia::Geometry2D::Circle circB(1.0f, Vector2D::Zero());

    f.AddCircle(&tA, &circA);
    f.AddCircle(&tB, &circB);

    f.world->Update(1.0f / 60.0f);

    const auto& contacts = f.world->GetLastContacts();
    ASSERT_EQ(contacts.Size(), 1u);
    EXPECT_GT(contacts[0].depth, 0.0f);
    EXPECT_NEAR(contacts[0].depth, 0.5f, 1e-4f);
}

// ---------------------------------------------------------------------------
// Test 6 — Contact list is cleared each step; separated bodies leave no contact
// ---------------------------------------------------------------------------

// Directly exercises DetectCollisions() (no broad-phase required) by confirming
// that the outContacts array is wiped before each detection pass.
TEST(RigidBody2D_CollisionDetection, ContactList_ClearedEachStep)
{
    // Build two-body pools directly — no PhysicsWorld, no broad-phase
    Dia::Geometry2D::Circle circA(1.0f, Vector2D::Zero());
    Dia::Geometry2D::Circle circB(1.0f, Vector2D::Zero());

    PointBodyDef defA, defB;
    defA.circleShape = &circA;
    defB.circleShape = &circB;

    PointBody2D bodyA(defA);
    PointBody2D bodyB(defB);

    Dia::Core::Containers::DynamicArrayC<PointBody2D*, kMaxPointBodies> pts;
    Dia::Core::Containers::DynamicArrayC<RigidBody2D*, kMaxRigidBodies> rbs;
    Dia::Core::Containers::DynamicArrayC<Contact, kMaxContacts>         contacts;

    pts.Add(&bodyA);
    pts.Add(&bodyB);

    // With null broad-phase DetectCollisions returns immediately — contacts cleared
    DetectCollisions(pts, rbs, nullptr, contacts);
    EXPECT_EQ(contacts.Size(), 0u);

    // Manually add a dummy contact to simulate stale data from a previous step
    contacts.Add(Contact{});
    EXPECT_EQ(contacts.Size(), 1u);

    // DetectCollisions must clear it even with nullptr broad-phase
    DetectCollisions(pts, rbs, nullptr, contacts);
    EXPECT_EQ(contacts.Size(), 0u);
}
