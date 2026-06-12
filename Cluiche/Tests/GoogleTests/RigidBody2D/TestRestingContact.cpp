#include <gtest/gtest.h>
#include <cmath>

#include <DiaRigidBody2D/World/PhysicsWorld.h>
#include <DiaRigidBody2D/Response/ResolveCollisions.h>
#include <DiaRigidBody2D/Detection/Contact.h>
#include <DiaRigidBody2D/Bodies/PointBody2D.h>
#include <DiaGeometry2D/Transform/Transform.h>
#include <DiaGeometry2D/Shapes/Circle.h>
#include <DiaGeometry2D/Shapes/AARect.h>
#include <DiaGeometry2D/Spatial/SpatialGrid.h>
#include <DiaMaths/Vector/Vector2D.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

// ===========================================================================
// Resting-contact settling.
//
// These tests pin the mathematical invariant that lets a body come to rest
// (and therefore sleep) on a static surface. The CluicheTest RigidBody2D stage
// validates the same behaviour end-to-end ("all bodies settled"), but an e2e
// stage can only report "still moving" after burning its frame budget — it
// gives no signal about *why*. The math below isolates the exact condition.
//
// Invariant: a body resting on a surface gains an apparent approach velocity of
// |gravity . n| * dt every step from force integration alone. If restitution is
// applied to that approach, the body is re-launched at e * |gravity . n| * dt
// each step and never settles. ResolveCollisions must therefore suppress
// restitution once the approach velocity is within
//     restitutionVelocitySlop + |gravity . n| * dt
// of zero. The stage uses gravity = -120 and dt = 1/60, so the gravity-induced
// approach is 2.0 — four times the default slop of 0.5 — which is why, before
// the fix, the stage could never reach rest.
// ===========================================================================

using namespace Dia::RigidBody2D;
using namespace Dia::Maths;

static constexpr float kDt = 1.0f / 60.0f;

// ---------------------------------------------------------------------------
// 1. Isolated invariant — restitution is suppressed for a gravity-only approach
//
// A single resting contact whose only approach velocity is the one gravity
// injected this step (contactVel = -|gravity| * dt) must resolve to ~zero
// normal velocity, NOT bounce. This is the smallest possible reproduction of
// the stage bug, with no integration or detection in the loop.
// ---------------------------------------------------------------------------

TEST(RigidBody2D_RestingContact, RestitutionSuppressed_ForGravityOnlyApproach)
{
    const Vector2D gravity(0.0f, -120.0f);

    Dia::Geometry2D::Transform tBall, tGround;
    tBall.SetLocalPosition(Vector2D(0.0f, 1.0f));
    tGround.SetLocalPosition(Vector2D(0.0f, -1.0f));

    PointBodyDef defBall, defGround;
    defBall.transform = &tBall; defBall.mass = 1.0f;
    defBall.restitution = 0.3f; defBall.friction = 0.0f;
    defGround.transform = &tGround; defGround.type = BodyType::kStatic;
    defGround.mass = 0.0f; defGround.restitution = 0.3f; defGround.friction = 0.0f;
    PointBody2D ball(defBall), ground(defGround);

    // Velocity gravity would have produced this step: v = g * dt downward.
    ball.SetVelocity(Vector2D(0.0f, gravity.y * kDt));  // -2.0

    // Engine convention (DetectCollisions): normal points from bodyA toward
    // bodyB. Ball is A (above), ground is B (below), so the normal is -y.
    Contact c;
    c.bodyA = &ball; c.bodyB = &ground;
    c.normal = Vector2D(0.0f, -1.0f);
    c.depth = 0.02f;
    c.point = Vector2D(0.0f, 0.0f);

    Dia::Core::Containers::DynamicArrayC<Contact, kMaxContacts> contacts;
    contacts.Add(c);

    ResponseConfig cfg;  // defaults: restitutionVelocitySlop = 0.5
    ResolveCollisions(contacts, cfg, kDt, gravity);

    // With restitution suppressed (e treated as 0 for this resting approach),
    // the impulse removes the approach velocity and leaves ~zero, rather than
    // rebounding at e * 2.0 = 0.6.
    EXPECT_NEAR(ball.GetVelocity().y, 0.0f, 1e-3f)
        << "Resting body re-launched by restitution — it will never settle";
}

// ---------------------------------------------------------------------------
// 2. Genuine impacts still bounce — the fix must not kill real restitution
//
// A body approaching far faster than the gravity-induced slop must still
// rebound at -e * v. Guards against "fixing" settling by globally disabling
// restitution.
// ---------------------------------------------------------------------------

TEST(RigidBody2D_RestingContact, GenuineImpact_StillBounces)
{
    const Vector2D gravity(0.0f, -120.0f);

    Dia::Geometry2D::Transform tBall, tGround;
    tBall.SetLocalPosition(Vector2D(0.0f, 1.0f));
    tGround.SetLocalPosition(Vector2D(0.0f, -1.0f));

    PointBodyDef defBall, defGround;
    defBall.transform = &tBall; defBall.mass = 1.0f;
    defBall.restitution = 0.5f; defBall.friction = 0.0f;
    defGround.transform = &tGround; defGround.type = BodyType::kStatic;
    defGround.mass = 0.0f; defGround.restitution = 0.5f; defGround.friction = 0.0f;
    PointBody2D ball(defBall), ground(defGround);

    // Fast impact: 40 units/s downward, far above slop + |g|*dt = 0.5 + 2.0.
    ball.SetVelocity(Vector2D(0.0f, -40.0f));

    // Normal points from bodyA (ball, above) toward bodyB (ground, below) = -y.
    Contact c;
    c.bodyA = &ball; c.bodyB = &ground;
    c.normal = Vector2D(0.0f, -1.0f);
    c.depth = 0.05f;
    c.point = Vector2D(0.0f, 0.0f);

    Dia::Core::Containers::DynamicArrayC<Contact, kMaxContacts> contacts;
    contacts.Add(c);

    ResponseConfig cfg;
    cfg.baumgarteFactor = 0.0f;
    ResolveCollisions(contacts, cfg, kDt, gravity);

    // new vel = -e * old vel = -0.5 * -40 = +20
    EXPECT_NEAR(ball.GetVelocity().y, 20.0f, 1e-3f)
        << "Real impact restitution must be preserved";
}

// ---------------------------------------------------------------------------
// 3. The invariant boundary — settling requires slop > |gravity . n| * dt
//
// Parametric proof of the structural condition. For each (gravity, slop) pair
// we drive one resting contact for many steps and check whether the normal
// velocity converges toward zero. With the gravity-aware threshold this holds
// for every case; the test documents the exact relationship the stage relies on.
// ---------------------------------------------------------------------------

namespace {
// Returns the steady-state |normal velocity| of a body resting on static ground
// after N resolve+integrate steps under the given gravity.
float SettleResidualSpeed(float gravityY, const ResponseConfig& cfg, int steps)
{
    const Vector2D gravity(0.0f, gravityY);

    Dia::Geometry2D::Transform tBall, tGround;
    tBall.SetLocalPosition(Vector2D(0.0f, 1.0f));
    tGround.SetLocalPosition(Vector2D(0.0f, -1.0f));

    PointBodyDef defBall, defGround;
    defBall.transform = &tBall; defBall.mass = 1.0f;
    defBall.restitution = 0.3f; defBall.friction = 0.0f;
    defGround.transform = &tGround; defGround.type = BodyType::kStatic;
    defGround.mass = 0.0f; defGround.restitution = 0.3f; defGround.friction = 0.0f;
    PointBody2D ball(defBall), ground(defGround);

    float v = 0.0f;
    for (int i = 0; i < steps; ++i)
    {
        // Mirror StepOnce(): integrate gravity into velocity, then resolve a
        // persistent resting contact, then read the post-solve velocity.
        v += gravity.y * kDt;
        ball.SetVelocity(Vector2D(0.0f, v));

        Contact c;
        c.bodyA = &ball; c.bodyB = &ground;
        c.normal = Vector2D(0.0f, -1.0f);  // bodyA (ball) toward bodyB (ground)
        c.depth = 0.02f;  // held just above baumgarteSlop so contact persists
        c.point = Vector2D(0.0f, 0.0f);

        Dia::Core::Containers::DynamicArrayC<Contact, kMaxContacts> contacts;
        contacts.Add(c);
        ResolveCollisions(contacts, cfg, kDt, gravity);

        v = ball.GetVelocity().y;
    }
    return std::abs(v);
}
}  // namespace

TEST(RigidBody2D_RestingContact, ResidualSpeedStaysBelowSleepThreshold)
{
    ResponseConfig cfg;  // default slop 0.5
    const float kSleepThreshold = 0.01f;

    // Stage configuration: gravity -120, default slop. The gravity-aware
    // threshold suppresses restitution, so the residual normal speed each step
    // is the gravity impulse the *next* resolve cancels — it must stay below the
    // sleep threshold so UpdateSleepTimers can accumulate.
    float residual = SettleResidualSpeed(-120.0f, cfg, 600);
    EXPECT_LT(residual, kSleepThreshold)
        << "Resting body's residual speed exceeds sleep threshold (gravity=-120)";

    // Lighter gravity must also settle.
    EXPECT_LT(SettleResidualSpeed(-9.81f, cfg, 600), kSleepThreshold);
}

// ---------------------------------------------------------------------------
// 4. Full world — a dropped circle settles and sleeps (the stage's question)
//
// This is the RigidBody2D test stage reduced to one body and run through the
// real PhysicsWorld. It asserts the body reaches kSleeping within a frame
// budget — the deterministic unit-test form of "all bodies settled".
// ---------------------------------------------------------------------------

TEST(RigidBody2D_RestingContact, DroppedCircle_SettlesAndSleeps)
{
    using Grid = Dia::Geometry2D::SpatialGrid<Body2DBase*>;
    Grid::Def gd;
    gd.worldBounds = Dia::Geometry2D::AARect(Vector2D(-500.0f, -500.0f),
                                             Vector2D( 500.0f,  500.0f));
    gd.cellSize = 20.0f;
    Grid grid(gd);

    WorldDef wd;
    wd.gravity       = Vector2D(0.0f, -120.0f);
    wd.fixedTimestep = kDt;
    wd.maxSubSteps   = 1;
    wd.broadPhase    = &grid;
    PhysicsWorld world(wd);

    // Static ground (large circle), top surface near y = 0.
    Dia::Geometry2D::Transform tGround;
    tGround.SetLocalPosition(Vector2D(0.0f, -200.0f));
    Dia::Geometry2D::Circle groundShape(200.0f, Vector2D::Zero());
    RigidBodyDef groundDef;
    groundDef.transform   = &tGround;
    groundDef.circleShape = &groundShape;
    groundDef.type        = BodyType::kStatic;
    groundDef.mass        = 0.0f;
    groundDef.restitution = 0.3f;
    groundDef.friction    = 0.5f;
    world.AddRigidBody(groundDef);

    // Dynamic circle dropped from above the surface.
    Dia::Geometry2D::Transform tBall;
    tBall.SetLocalPosition(Vector2D(0.0f, 60.0f));
    Dia::Geometry2D::Circle ballShape(30.0f, Vector2D::Zero());
    RigidBodyDef ballDef;
    ballDef.transform     = &tBall;
    ballDef.circleShape   = &ballShape;
    ballDef.type          = BodyType::kDynamic;
    ballDef.mass          = 1.0f;
    ballDef.restitution   = 0.3f;
    ballDef.friction      = 0.5f;
    ballDef.linearDamping = 0.01f;
    ballDef.allowSleeping = true;
    RigidBody2D* ball = world.AddRigidBody(ballDef);

    int sleptAtFrame = -1;
    for (int frame = 0; frame < 900; ++frame)
    {
        world.Update(kDt);
        if (!ball->IsAwake())
        {
            sleptAtFrame = frame;
            break;
        }
    }

    ASSERT_GE(sleptAtFrame, 0)
        << "Dropped circle never reached rest within 900 frames (it bounces forever)";
    EXPECT_FALSE(ball->IsAwake());
}
