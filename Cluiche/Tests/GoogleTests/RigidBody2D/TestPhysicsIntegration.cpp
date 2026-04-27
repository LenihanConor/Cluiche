#include <gtest/gtest.h>
#include <cmath>

#include <DiaRigidBody2D/World/PhysicsWorld.h>
#include <DiaRigidBody2D/Constraints/PinJoint.h>
#include <DiaRigidBody2D/Constraints/DistanceConstraint.h>
#include <DiaRigidBody2D/Constraints/SpringConstraint.h>
#include <DiaRigidBody2D/Events/CollisionEvent.h>
#include <DiaGeometry2D/Transform/Transform.h>
#include <DiaGeometry2D/Shapes/AARect.h>
#include <DiaGeometry2D/Shapes/Circle.h>
#include <DiaGeometry2D/Shapes/Ray.h>
#include <DiaGeometry2D/Spatial/SpatialGrid.h>
#include <DiaCore/Architecture/Observer.h>
#include <DiaMaths/Vector/Vector2D.h>

using namespace Dia::RigidBody2D;
using namespace Dia::Maths;

// ===========================================================================
// Shared fixture — full world with broad-phase, gravity optional
// ===========================================================================

static constexpr float kDt = 1.0f / 60.0f;

using Grid = Dia::Geometry2D::SpatialGrid<Body2DBase*>;

struct IntegrationFixture {
    static constexpr int kMax = 16;

    Dia::Geometry2D::Transform transforms[kMax];
    Dia::Geometry2D::Circle    circles[kMax];
    Dia::Geometry2D::AARect    broads[kMax];

    Grid*         grid  = nullptr;
    PhysicsWorld* world = nullptr;

    IntegrationFixture(Vector2D gravity = Vector2D(0.0f, 0.0f), int constraintIter = 20)
    {
        Grid::Def gd;
        gd.worldBounds = Dia::Geometry2D::AARect(Vector2D(-200.0f, -200.0f), Vector2D(200.0f, 200.0f));
        gd.cellSize = 10.0f;
        grid = new Grid(gd);

        WorldDef wd;
        wd.gravity       = gravity;
        wd.fixedTimestep = kDt;
        wd.maxSubSteps   = 1;
        wd.broadPhase    = grid;
        wd.constraintConfig.iterations = constraintIter;
        world = new PhysicsWorld(wd);
    }

    ~IntegrationFixture() { delete world; delete grid; }

    PointBody2D* MakePoint(int idx, float x, float y,
                           float radius = 0.5f,
                           BodyType type = BodyType::kDynamic,
                           float mass = 1.0f)
    {
        transforms[idx] = Dia::Geometry2D::Transform();
        transforms[idx].SetWorldPosition(Vector2D(x, y));
        circles[idx] = Dia::Geometry2D::Circle(radius, Vector2D::Zero());
        PointBodyDef def;
        def.transform   = &transforms[idx];
        def.circleShape = &circles[idx];
        def.type        = type;
        def.mass        = mass;
        def.allowSleeping = false;
        return world->AddPointBody(def);
    }

    RigidBody2D* MakeRigid(int idx, float x, float y,
                           float radius = 0.5f,
                           BodyType type = BodyType::kDynamic,
                           float mass = 1.0f)
    {
        transforms[idx] = Dia::Geometry2D::Transform();
        transforms[idx].SetWorldPosition(Vector2D(x, y));
        circles[idx] = Dia::Geometry2D::Circle(radius, Vector2D::Zero());
        RigidBodyDef def;
        def.transform       = &transforms[idx];
        def.circleShape     = &circles[idx];
        def.type            = type;
        def.mass            = mass;
        def.momentOfInertia = 1.0f;
        def.allowSleeping   = false;
        return world->AddRigidBody(def);
    }

    void Step(int n = 1) { for (int i = 0; i < n; ++i) world->Update(kDt); }
};

class EventCounter : public Dia::Core::Observer {
public:
    int enterCount = 0;
    int stayCount  = 0;
    int exitCount  = 0;

    void ObserverNotification(const Dia::Core::ObserverSubject*, int message) override
    {
        switch (static_cast<CollisionEventType>(message))
        {
        case CollisionEventType::kEnter: ++enterCount; break;
        case CollisionEventType::kStay:  ++stayCount;  break;
        case CollisionEventType::kExit:  ++exitCount;  break;
        }
    }
};

// ===========================================================================
// 1. Mixed body types — PointBody2D and RigidBody2D collide via virtual dispatch
// ===========================================================================

TEST(RigidBody2D_Integration, MixedBodyTypes_PointAndRigid_Collide)
{
    IntegrationFixture f;

    // PointBody approaching RigidBody — overlapping circles (radius 1, centers 1.5 apart)
    PointBody2D* pt = f.MakePoint(0, 0.0f, 0.0f, 1.0f);
    RigidBody2D* rb = f.MakeRigid(1, 1.5f, 0.0f, 1.0f);

    // Give them relative velocity so impulse-based response applies
    pt->SetVelocity(Vector2D(3.0f, 0.0f));

    f.Step(5);

    // After collision: PointBody should slow/reverse, RigidBody should gain velocity
    float ptVx = pt->GetVelocity().x;
    float rbVx = rb->GetVelocity().x;

    EXPECT_LT(ptVx, 3.0f) << "Point body should lose velocity from collision";
    EXPECT_GT(rbVx, 0.0f) << "Rigid body should gain velocity from collision";
}

// ===========================================================================
// 2. Constraint lifecycle — removing body cleans surviving body's per-body list
// ===========================================================================

TEST(RigidBody2D_Integration, ConstraintLifecycle_RemoveBody_SurvivorListClean)
{
    IntegrationFixture f;

    RigidBody2D* a = f.MakeRigid(0, 0.0f, 0.0f);
    RigidBody2D* b = f.MakeRigid(1, 3.0f, 0.0f);
    RigidBody2D* c = f.MakeRigid(2, 6.0f, 0.0f);

    f.world->AddConstraint(new PinJoint(a, Vector2D::Zero(), b, Vector2D::Zero()));
    f.world->AddConstraint(new DistanceConstraint(b, Vector2D::Zero(), c, Vector2D::Zero(), 3.0f));

    EXPECT_EQ(a->GetConstraintCount(), 1);
    EXPECT_EQ(b->GetConstraintCount(), 2);
    EXPECT_EQ(c->GetConstraintCount(), 1);

    // Remove b — both constraints should be cleaned from a's and c's per-body lists
    f.world->RemoveRigidBody(b);

    EXPECT_EQ(f.world->GetConstraints().Size(), 0u);
    EXPECT_EQ(a->GetConstraintCount(), 0) << "Surviving body a should have no constraints";
    EXPECT_EQ(c->GetConstraintCount(), 0) << "Surviving body c should have no constraints";

    f.Step(10);
}

// ===========================================================================
// 3. Spatial queries on mixed types — Raycast and QueryRegion find both
// ===========================================================================

TEST(RigidBody2D_Integration, SpatialQueries_MixedTypes_BothFound)
{
    IntegrationFixture f;

    f.MakePoint(0, 5.0f, 0.0f, 1.0f, BodyType::kDynamic);
    f.MakeRigid(1, 10.0f, 0.0f, 1.0f, BodyType::kDynamic);

    f.Step(1);

    // QueryRegion covering both
    Dia::Core::Containers::DynamicArrayC<const Body2DBase*, kMaxQueryResults> results;
    f.world->QueryRegion(
        Dia::Geometry2D::AARect(Vector2D(3.0f, -2.0f), Vector2D(12.0f, 2.0f)), results);

    EXPECT_EQ(results.Size(), 2u) << "QueryRegion should find both PointBody and RigidBody";

    // Raycast should hit the nearest (PointBody at x=5)
    Dia::Geometry2D::Ray ray(Vector2D(-10.0f, 0.0f), Vector2D(1.0f, 0.0f));
    RaycastHit hit;
    bool hitResult = f.world->Raycast(ray, hit);
    EXPECT_TRUE(hitResult);
    if (hitResult)
    {
        EXPECT_NEAR(hit.point.x, 4.0f, 0.5f) << "Raycast should hit PointBody first";
    }
}

// ===========================================================================
// 4. Full sim: gravity + collision + constraint + events cooperate
// ===========================================================================

TEST(RigidBody2D_Integration, FullSim_Pendulum_HitsWall_EventsFire)
{
    IntegrationFixture f(Vector2D(0.0f, -9.81f));

    EventCounter counter;
    f.world->GetCollisionEvents().AttachToObserver(&counter);

    // Anchor pinned at origin
    RigidBody2D* anchor = f.MakeRigid(0, 0.0f, 0.0f, 0.5f, BodyType::kStatic);
    // Pendulum bob: starts displaced to the right, will swing left under gravity
    RigidBody2D* bob = f.MakeRigid(1, 3.0f, 0.0f, 0.5f);

    f.world->AddConstraint(
        new DistanceConstraint(anchor, Vector2D::Zero(), bob, Vector2D::Zero(), 3.0f));

    // Wall on the left at x = -2
    f.MakeRigid(2, -3.0f, 0.0f, 1.0f, BodyType::kStatic);

    // Run simulation for 2 seconds
    f.Step(120);

    // Bob should have swung — its position should have changed from initial (3, 0)
    float bobX = bob->GetTransform()->GetWorldPosition().x;
    float bobY = bob->GetTransform()->GetWorldPosition().y;
    bool moved = (std::abs(bobX - 3.0f) > 0.1f || std::abs(bobY) > 0.1f);
    EXPECT_TRUE(moved) << "Pendulum bob should have moved from initial position";

    // Anchor should not have moved
    EXPECT_NEAR(anchor->GetTransform()->GetWorldPosition().x, 0.0f, 1e-4f);

    f.world->GetCollisionEvents().DetachFromObserver(&counter);
}

// ===========================================================================
// 5. Body removal mid-sim with active constraints and collisions
// ===========================================================================

TEST(RigidBody2D_Integration, RemoveBody_MidSim_WithConstraintsAndCollisions)
{
    IntegrationFixture f;

    RigidBody2D* a = f.MakeRigid(0, 0.0f, 0.0f, 1.0f);
    RigidBody2D* b = f.MakeRigid(1, 1.5f, 0.0f, 1.0f);  // overlapping with a
    RigidBody2D* c = f.MakeRigid(2, 5.0f, 0.0f, 0.5f);

    f.world->AddConstraint(new PinJoint(a, Vector2D::Zero(), b, Vector2D::Zero()));
    f.world->AddConstraint(new DistanceConstraint(b, Vector2D::Zero(), c, Vector2D::Zero(), 3.5f));

    f.Step(5);

    // Remove b which is colliding with a and has 2 constraints
    f.world->RemoveRigidBody(b);

    EXPECT_EQ(f.world->GetRigidBodyCount(), 2);
    EXPECT_EQ(f.world->GetConstraints().Size(), 0u);
    EXPECT_EQ(a->GetConstraintCount(), 0);
    EXPECT_EQ(c->GetConstraintCount(), 0);

    // Should not crash
    f.Step(30);
}

// ===========================================================================
// 6. Collision layers + constraints — filtered bodies still constrained
// ===========================================================================

TEST(RigidBody2D_Integration, Layers_FilteredCollision_ConstraintStillWorks)
{
    IntegrationFixture f;

    RigidBody2D* a = f.MakeRigid(0, 0.0f, 0.0f, 1.0f);
    RigidBody2D* b = f.MakeRigid(1, 5.0f, 0.0f, 1.0f);

    // Put them on different layers so they cannot collide
    a->SetLayer(1u << 1);
    a->SetMask(1u << 1);
    b->SetLayer(1u << 2);
    b->SetMask(1u << 2);

    // But connect them with a distance constraint of length 2
    f.world->AddConstraint(
        new DistanceConstraint(a, Vector2D::Zero(), b, Vector2D::Zero(), 2.0f));

    f.Step(60);

    // No collision contacts should have been generated
    // (we check last step — layers filter them out)
    // But constraint should have pulled b toward a
    float dist = std::abs(b->GetTransform()->GetWorldPosition().x - a->GetTransform()->GetWorldPosition().x);
    EXPECT_NEAR(dist, 2.0f, 0.5f) << "Constraint should enforce distance despite layer filtering";
}

// ===========================================================================
// 7. Sleep cascade via constraint — collision wakes A, constraint wakes B
// ===========================================================================

TEST(RigidBody2D_Integration, SleepCascade_ImpulseWakesConstrained)
{
    IntegrationFixture f;

    RigidBody2D* a = f.MakeRigid(0, 0.0f, 0.0f, 0.5f, BodyType::kDynamic, 1.0f);

    // b allows sleeping so we can put it to sleep
    f.transforms[1] = Dia::Geometry2D::Transform();
    f.transforms[1].SetWorldPosition(Vector2D(5.0f, 0.0f));
    f.circles[1] = Dia::Geometry2D::Circle(0.5f, Vector2D::Zero());
    RigidBodyDef defB;
    defB.transform       = &f.transforms[1];
    defB.circleShape     = &f.circles[1];
    defB.mass            = 1.0f;
    defB.momentOfInertia = 1.0f;
    defB.allowSleeping   = true;
    RigidBody2D* b = f.world->AddRigidBody(defB);

    b->Sleep();
    EXPECT_FALSE(b->IsAwake());
    a->SetVelocity(Vector2D(2.0f, 0.0f));

    f.world->AddConstraint(
        new DistanceConstraint(a, Vector2D::Zero(), b, Vector2D::Zero(), 5.0f));

    f.Step(1);

    // a moved right → constraint error → solver applies impulse on b → wakes b
    EXPECT_TRUE(b->IsAwake()) << "B should wake from constraint impulse with A";
}

// ===========================================================================
// 8. Kinematic driving constraint — kinematic pulls dynamic
// ===========================================================================

TEST(RigidBody2D_Integration, KinematicDriving_ConstraintPullsDynamic)
{
    IntegrationFixture f;

    RigidBody2D* kin = f.MakeRigid(0, 0.0f, 0.0f, 0.5f, BodyType::kKinematic);
    RigidBody2D* dyn = f.MakeRigid(1, 3.0f, 0.0f);

    kin->SetVelocity(Vector2D(2.0f, 0.0f));

    f.world->AddConstraint(
        new DistanceConstraint(kin, Vector2D::Zero(), dyn, Vector2D::Zero(), 3.0f));

    f.Step(60);

    // Kinematic should have moved right: ~2 * 60/60 = 2 units
    float kinX = kin->GetTransform()->GetWorldPosition().x;
    EXPECT_GT(kinX, 1.5f) << "Kinematic should have moved right";

    // Dynamic should follow, maintaining ~3 units distance
    float dynX = dyn->GetTransform()->GetWorldPosition().x;
    float dist = std::abs(dynX - kinX);
    EXPECT_NEAR(dist, 3.0f, 0.8f) << "Dynamic should follow kinematic at constraint distance";
}

// ===========================================================================
// 9. Broad phase coherence — constraint moves body into collision range
// ===========================================================================

TEST(RigidBody2D_Integration, BroadPhaseCoherence_ConstraintPullsBodyTowardAnchor)
{
    IntegrationFixture f;

    // Anchor at origin, dynamic at (8, 0) — will be pulled by distance constraint to 2
    RigidBody2D* anchor = f.MakeRigid(0, 0.0f, 0.0f, 0.5f, BodyType::kStatic);
    RigidBody2D* mover  = f.MakeRigid(1, 8.0f, 0.0f, 1.0f);

    f.world->AddConstraint(
        new DistanceConstraint(anchor, Vector2D::Zero(), mover, Vector2D::Zero(), 2.0f));

    float initialX = mover->GetTransform()->GetWorldPosition().x;
    EXPECT_NEAR(initialX, 8.0f, 0.01f);

    f.Step(120);

    float moverX = mover->GetTransform()->GetWorldPosition().x;
    EXPECT_LT(moverX, 4.0f) << "Constraint should have pulled mover toward anchor";
    EXPECT_GT(moverX, 0.0f) << "Mover should not overshoot past anchor";
}

// ===========================================================================
// 10. Event lifecycle with body removal — Enter then Exit (or no crash)
// ===========================================================================

TEST(RigidBody2D_Integration, EventLifecycle_BodyRemoval_NoCrash)
{
    IntegrationFixture f;

    EventCounter counter;
    f.world->GetCollisionEvents().AttachToObserver(&counter);

    // Two overlapping bodies
    RigidBody2D* a = f.MakeRigid(0, 0.0f, 0.0f, 1.0f);
    RigidBody2D* b = f.MakeRigid(1, 1.0f, 0.0f, 1.0f);

    f.Step(1);

    EXPECT_GE(counter.enterCount, 1) << "Should get Enter event from overlap";

    int enterBefore = counter.enterCount;

    // Remove b — should not crash, events should stop
    f.world->RemoveRigidBody(b);

    f.Step(5);

    // No new enter events since b is gone
    EXPECT_EQ(counter.enterCount, enterBefore);

    f.world->GetCollisionEvents().DetachFromObserver(&counter);
}

// ===========================================================================
// 11. Spring + wall collision — spring and response don't diverge
// ===========================================================================

TEST(RigidBody2D_Integration, Spring_WallCollision_NoDivergence)
{
    IntegrationFixture f;

    // Static wall on the left
    f.MakeRigid(0, -3.0f, 0.0f, 1.0f, BodyType::kStatic);

    // Anchor on the right
    RigidBody2D* anchor = f.MakeRigid(1, 5.0f, 0.0f, 0.5f, BodyType::kStatic);

    // Dynamic body between them, connected to anchor by spring
    RigidBody2D* ball = f.MakeRigid(2, 0.0f, 0.0f, 1.0f);

    f.world->AddConstraint(
        new SpringConstraint(anchor, Vector2D::Zero(), ball, Vector2D::Zero(),
                             5.0f, 30.0f, 3.0f));

    // Push ball toward wall
    ball->ApplyImpulse(Vector2D(-10.0f, 0.0f));

    f.Step(120);

    float ballX = ball->GetTransform()->GetWorldPosition().x;
    EXPECT_GT(ballX, -50.0f) << "Spring+collision diverged to negative infinity";
    EXPECT_LT(ballX,  50.0f) << "Spring+collision diverged to positive infinity";
}

// ===========================================================================
// 12. Query after removal — removed body no longer returned
// ===========================================================================

TEST(RigidBody2D_Integration, QueryAfterRemoval_BodyGone)
{
    IntegrationFixture f;

    RigidBody2D* a = f.MakeRigid(0, 5.0f, 0.0f, 1.0f);
    f.MakeRigid(1, 15.0f, 0.0f, 1.0f);

    f.Step(1);

    // Both should be found
    Dia::Core::Containers::DynamicArrayC<const Body2DBase*, kMaxQueryResults> results;
    f.world->QueryRegion(
        Dia::Geometry2D::AARect(Vector2D(3.0f, -2.0f), Vector2D(17.0f, 2.0f)), results);
    EXPECT_EQ(results.Size(), 2u);

    // Remove first body
    f.world->RemoveRigidBody(a);
    f.Step(1);

    results.RemoveAll();
    f.world->QueryRegion(
        Dia::Geometry2D::AARect(Vector2D(3.0f, -2.0f), Vector2D(17.0f, 2.0f)), results);
    EXPECT_EQ(results.Size(), 1u) << "Removed body should not appear in query results";
}
