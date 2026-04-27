#include <gtest/gtest.h>

#include <DiaRigidBody2D/Integration/UpdateSleepTimers.h>
#include <DiaRigidBody2D/Integration/Integration.h>
#include <DiaRigidBody2D/Detection/DetectCollisions.h>
#include <DiaRigidBody2D/Detection/Contact.h>
#include <DiaRigidBody2D/Bodies/PointBody2D.h>
#include <DiaRigidBody2D/Bodies/RigidBody2D.h>
#include <DiaRigidBody2D/World/PhysicsWorld.h>
#include <DiaGeometry2D/Spatial/SpatialGrid.h>
#include <DiaGeometry2D/Shapes/AARect.h>
#include <DiaGeometry2D/Shapes/Circle.h>
#include <DiaGeometry2D/Transform/Transform.h>
#include <DiaMaths/Vector/Vector2D.h>

using namespace Dia::RigidBody2D;
using namespace Dia::Maths;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static PointBody2D* MakePointBody(float mass = 1.0f, bool allowSleeping = true,
                                  BodyType type = BodyType::kDynamic)
{
    PointBodyDef d;
    d.mass          = mass;
    d.allowSleeping = allowSleeping;
    d.type          = type;
    return new PointBody2D(d);
}

static RigidBody2D* MakeRigidBody(float mass = 1.0f, float moi = 1.0f,
                                  bool allowSleeping = true,
                                  BodyType type = BodyType::kDynamic)
{
    RigidBodyDef d;
    d.mass            = mass;
    d.momentOfInertia = moi;
    d.allowSleeping   = allowSleeping;
    d.type            = type;
    return new RigidBody2D(d);
}

static const float kLinearThreshold  = 0.01f;
static const float kAngularThreshold = 0.01f;
static const float kSleepTime        = 0.5f;
static const float kDt               = 1.0f / 60.0f;

// ---------------------------------------------------------------------------
// Test 1 — Body at rest accumulates timer and becomes kSleeping
// ---------------------------------------------------------------------------

TEST(RigidBody2D_BodySleeping, RestBody_SleepsAfterThreshold)
{
    Dia::Core::Containers::DynamicArrayC<PointBody2D*, kMaxPointBodies> bodies;
    PointBody2D* body = MakePointBody();
    body->SetVelocity(Vector2D::Zero());
    bodies.Add(body);

    const int steps = static_cast<int>(kSleepTime / kDt) + 5;
    for (int i = 0; i < steps; ++i)
        UpdateSleepTimers(bodies, kDt, kLinearThreshold, kSleepTime);

    EXPECT_EQ(body->GetSleepState(), SleepState::kSleeping);
    EXPECT_FALSE(body->IsAwake());

    delete body;
}

// ---------------------------------------------------------------------------
// Test 2 — Sleeping body position does not change each step via integration
// ---------------------------------------------------------------------------

TEST(RigidBody2D_BodySleeping, SleepingBody_PositionUnchanged)
{
    Dia::Geometry2D::Transform t;
    t.SetLocalPosition(Vector2D(5.0f, 5.0f));

    PointBodyDef d;
    d.mass          = 1.0f;
    d.allowSleeping = true;
    d.transform     = &t;
    PointBody2D body(d);
    body.SetVelocity(Vector2D(1.0f, 0.0f));
    body.SetSleepState(SleepState::kSleeping);  // force sleep

    Dia::Core::Containers::DynamicArrayC<PointBody2D*, kMaxPointBodies> bodies;
    bodies.Add(&body);

    // IntegrateLinearVelocities skips sleeping bodies — position unchanged
    IntegrateLinearVelocities(bodies, kDt);

    EXPECT_NEAR(body.GetTransform()->GetLocalPosition().x, 5.0f, 1e-5f);
    EXPECT_NEAR(body.GetTransform()->GetLocalPosition().y, 5.0f, 1e-5f);
}

// ---------------------------------------------------------------------------
// Test 3 — ApplyImpulse wakes sleeping PointBody2D
// ---------------------------------------------------------------------------

TEST(RigidBody2D_BodySleeping, ApplyImpulse_WakesSleepingBody)
{
    PointBody2D* body = MakePointBody();
    body->SetSleepState(SleepState::kSleeping);
    EXPECT_FALSE(body->IsAwake());

    body->ApplyImpulse(Vector2D(1.0f, 0.0f));

    EXPECT_TRUE(body->IsAwake());
    delete body;
}

// ---------------------------------------------------------------------------
// Test 4 — ApplyForce wakes sleeping body
// ---------------------------------------------------------------------------

TEST(RigidBody2D_BodySleeping, ApplyForce_WakesSleepingBody)
{
    PointBody2D* body = MakePointBody();
    body->SetSleepState(SleepState::kSleeping);
    EXPECT_FALSE(body->IsAwake());

    body->ApplyForce(Vector2D(0.0f, 1.0f));

    EXPECT_TRUE(body->IsAwake());
    delete body;
}

// ---------------------------------------------------------------------------
// Test 5 — Wake() explicit immediately wakes body and resets sleep timer
// ---------------------------------------------------------------------------

TEST(RigidBody2D_BodySleeping, ExplicitWake_ResetsTimer)
{
    PointBody2D* body = MakePointBody();
    body->SetSleepState(SleepState::kSleeping);
    body->SetSleepTimer(1.0f);

    body->Wake();

    EXPECT_TRUE(body->IsAwake());
    EXPECT_NEAR(body->GetSleepTimer(), 0.0f, 1e-5f);
    delete body;
}

// ---------------------------------------------------------------------------
// Test 6 — Body oscillates near threshold — timer resets, does not sleep
// ---------------------------------------------------------------------------

TEST(RigidBody2D_BodySleeping, OscillatingBody_DoesNotSleepPrematurely)
{
    Dia::Core::Containers::DynamicArrayC<PointBody2D*, kMaxPointBodies> bodies;
    PointBody2D* body = MakePointBody();
    bodies.Add(body);

    // Run 20 steps below threshold, then 1 step above — timer should reset
    for (int i = 0; i < 20; ++i)
    {
        body->SetVelocity(Vector2D::Zero());
        UpdateSleepTimers(bodies, kDt, kLinearThreshold, kSleepTime);
    }

    // Kick it — above threshold
    body->SetVelocity(Vector2D(1.0f, 0.0f));
    UpdateSleepTimers(bodies, kDt, kLinearThreshold, kSleepTime);

    EXPECT_TRUE(body->IsAwake());
    EXPECT_NEAR(body->GetSleepTimer(), 0.0f, 1e-5f);

    delete body;
}

// ---------------------------------------------------------------------------
// Test 7 — allowSleeping = false — body never sleeps
// ---------------------------------------------------------------------------

TEST(RigidBody2D_BodySleeping, AllowSleepingFalse_NeverSleeps)
{
    Dia::Core::Containers::DynamicArrayC<PointBody2D*, kMaxPointBodies> bodies;
    PointBody2D* body = MakePointBody(1.0f, false);
    body->SetVelocity(Vector2D::Zero());
    bodies.Add(body);

    const int steps = static_cast<int>(kSleepTime / kDt) + 5;
    for (int i = 0; i < steps; ++i)
        UpdateSleepTimers(bodies, kDt, kLinearThreshold, kSleepTime);

    EXPECT_TRUE(body->IsAwake());
    delete body;
}

// ---------------------------------------------------------------------------
// Test 8 — Static body never enters sleep state
// ---------------------------------------------------------------------------

TEST(RigidBody2D_BodySleeping, StaticBody_NeverSleeps)
{
    Dia::Core::Containers::DynamicArrayC<PointBody2D*, kMaxPointBodies> bodies;
    PointBody2D* body = MakePointBody(0.0f, true, BodyType::kStatic);
    body->SetVelocity(Vector2D::Zero());
    bodies.Add(body);

    const int steps = static_cast<int>(kSleepTime / kDt) + 5;
    for (int i = 0; i < steps; ++i)
        UpdateSleepTimers(bodies, kDt, kLinearThreshold, kSleepTime);

    EXPECT_TRUE(body->IsAwake());
    delete body;
}

// ---------------------------------------------------------------------------
// Test 9 — Kinematic body never enters sleep state
// ---------------------------------------------------------------------------

TEST(RigidBody2D_BodySleeping, KinematicBody_NeverSleeps)
{
    Dia::Core::Containers::DynamicArrayC<PointBody2D*, kMaxPointBodies> bodies;
    PointBody2D* body = MakePointBody(1.0f, true, BodyType::kKinematic);
    body->SetVelocity(Vector2D::Zero());
    bodies.Add(body);

    const int steps = static_cast<int>(kSleepTime / kDt) + 5;
    for (int i = 0; i < steps; ++i)
        UpdateSleepTimers(bodies, kDt, kLinearThreshold, kSleepTime);

    EXPECT_TRUE(body->IsAwake());
    delete body;
}

// ---------------------------------------------------------------------------
// Test 10 — Active body overlapping sleeping body wakes the sleeping body
// (tests DetectCollisions directly to avoid requiring a full Transform/world step)
// ---------------------------------------------------------------------------

TEST(RigidBody2D_BodySleeping, BroadPhaseOverlap_WakesSleepingBody)
{
    using Grid = Dia::Geometry2D::SpatialGrid<Body2DBase*>;
    Grid::Def gd;
    gd.worldBounds = Dia::Geometry2D::AARect(Vector2D(-100.0f, -100.0f), Vector2D(100.0f, 100.0f));
    gd.cellSize    = 10.0f;
    Grid* grid = new Grid(gd);

    Dia::Geometry2D::Transform tA, tB;
    tA.SetWorldPosition(Vector2D(0.0f, 0.0f));
    tB.SetWorldPosition(Vector2D(0.5f, 0.0f));

    Dia::Geometry2D::Circle circA(1.0f, Vector2D::Zero());
    Dia::Geometry2D::Circle circB(1.0f, Vector2D::Zero());

    PointBodyDef defA;
    defA.mass        = 0.0f;
    defA.type        = BodyType::kStatic;
    defA.transform   = &tA;
    defA.circleShape = &circA;
    PointBody2D bodyA(defA);
    bodyA.SetUniqueId(1);
    bodyA.SetSleepState(SleepState::kSleeping);
    EXPECT_FALSE(bodyA.IsAwake());

    PointBodyDef defB;
    defB.mass        = 1.0f;
    defB.transform   = &tB;
    defB.circleShape = &circB;
    PointBody2D bodyB(defB);
    bodyB.SetUniqueId(2);

    Dia::Geometry2D::AARect aabbA(Vector2D(-1.0f, -1.0f), Vector2D(1.0f, 1.0f));
    Dia::Geometry2D::AARect aabbB(Vector2D(-0.5f, -1.0f), Vector2D(1.5f, 1.0f));
    auto handleA = grid->Insert(static_cast<Body2DBase*>(&bodyA), aabbA);
    auto handleB = grid->Insert(static_cast<Body2DBase*>(&bodyB), aabbB);

    // Run DetectCollisions — sleeping A should be woken when overlapping awake B
    Dia::Core::Containers::DynamicArrayC<PointBody2D*, kMaxPointBodies> pts;
    Dia::Core::Containers::DynamicArrayC<RigidBody2D*, kMaxRigidBodies> rbs;
    Dia::Core::Containers::DynamicArrayC<Contact, kMaxContacts>         contacts;
    pts.Add(&bodyA);
    pts.Add(&bodyB);

    DetectCollisions(pts, rbs, grid, contacts);

    EXPECT_TRUE(bodyA.IsAwake());

    delete grid;
    (void)handleA; (void)handleB;
}
