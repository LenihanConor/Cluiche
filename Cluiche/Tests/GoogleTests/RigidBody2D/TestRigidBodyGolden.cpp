#include <gtest/gtest.h>

#include <DiaRigidBody2D/World/PhysicsWorld.h>
#include <DiaRigidBody2D/Bodies/PointBody2D.h>
#include <DiaRigidBody2D/Bodies/RigidBody2D.h>
#include <DiaGeometry2D/Transform/Transform.h>
#include <DiaMaths/Vector/Vector2D.h>
#include <DiaCore/CRC/StringCRC.h>

#include <cmath>

using namespace Dia::RigidBody2D;
using namespace Dia::Maths;

// ---------------------------------------------------------------------------
// TEST 1 — Determinism: two identical worlds produce identical positions
// ---------------------------------------------------------------------------

TEST(RigidBody2D_Golden, Determinism_TwoBodies_60Steps_IdenticalPositions)
{
    static constexpr int kSteps = 60;
    const float dt = 1.0f / 60.0f;

    Vector2D posA0, posA1, posB0, posB1;

    auto runSim = [&](Vector2D& out0, Vector2D& out1) {
        WorldDef def;
        def.gravity       = Vector2D(0.0f, -9.81f);
        def.fixedTimestep = dt;
        def.maxSubSteps   = 8;
        def.broadPhase    = nullptr;
        PhysicsWorld world(def);

        PointBodyDef dA;
        dA.id            = Dia::Core::StringCRC("Body0");
        dA.type          = BodyType::kDynamic;
        dA.mass          = 1.0f;
        dA.linearDamping = 0.0f;
        dA.allowSleeping = false;
        PointBody2D* b0 = world.AddPointBody(dA);
        b0->GetTransform()->SetLocalPosition(Vector2D(0.0f, 0.0f));

        PointBodyDef dB;
        dB.id            = Dia::Core::StringCRC("Body1");
        dB.type          = BodyType::kDynamic;
        dB.mass          = 2.0f;
        dB.linearDamping = 0.0f;
        dB.allowSleeping = false;
        PointBody2D* b1 = world.AddPointBody(dB);
        b1->GetTransform()->SetLocalPosition(Vector2D(5.0f, 10.0f));

        for (int i = 0; i < kSteps; ++i)
            world.Update(dt);

        out0 = b0->GetTransform()->GetLocalPosition();
        out1 = b1->GetTransform()->GetLocalPosition();
    };

    runSim(posA0, posA1);
    runSim(posB0, posB1);

    EXPECT_FLOAT_EQ(posA0.x, posB0.x);
    EXPECT_FLOAT_EQ(posA0.y, posB0.y);
    EXPECT_FLOAT_EQ(posA1.x, posB1.x);
    EXPECT_FLOAT_EQ(posA1.y, posB1.y);
    // Both bodies must have actually moved
    EXPECT_LT(posA0.y, 0.0f);
    EXPECT_LT(posA1.y, 10.0f);
}

// ---------------------------------------------------------------------------
// TEST 2 — Determinism: two RigidBody2D bodies, 60 steps, identical positions
// ---------------------------------------------------------------------------

TEST(RigidBody2D_Golden, Determinism_TwoRigidBodies_60Steps_IdenticalPositions)
{
    static constexpr int kSteps = 60;
    const float dt = 1.0f / 60.0f;

    Vector2D posA, posB;

    for (int run = 0; run < 2; ++run)
    {
        Dia::Geometry2D::Transform t0, t1;
        t0.SetLocalPosition(Vector2D(-2.0f, 8.0f));
        t1.SetLocalPosition(Vector2D( 2.0f, 8.0f));

        WorldDef def;
        def.gravity       = Vector2D(0.0f, -9.81f);
        def.fixedTimestep = dt;
        def.maxSubSteps   = 8;
        def.broadPhase    = nullptr;
        PhysicsWorld world(def);

        RigidBodyDef rd0;
        rd0.id             = Dia::Core::StringCRC("RB0");
        rd0.type           = BodyType::kDynamic;
        rd0.mass           = 1.0f;
        rd0.allowSleeping  = false;
        rd0.momentOfInertia = 1.0f;
        rd0.transform      = &t0;
        RigidBody2D* rb0 = world.AddRigidBody(rd0);

        RigidBodyDef rd1;
        rd1.id             = Dia::Core::StringCRC("RB1");
        rd1.type           = BodyType::kDynamic;
        rd1.mass           = 3.0f;
        rd1.allowSleeping  = false;
        rd1.momentOfInertia = 2.0f;
        rd1.transform      = &t1;
        RigidBody2D* rb1 = world.AddRigidBody(rd1);

        for (int i = 0; i < kSteps; ++i)
            world.Update(dt);

        (void)rb1;

        if (run == 0)
            posA = rb0->GetTransform()->GetLocalPosition();
        else
            posB = rb0->GetTransform()->GetLocalPosition();
    }

    EXPECT_FLOAT_EQ(posA.x, posB.x);
    EXPECT_FLOAT_EQ(posA.y, posB.y);
    EXPECT_LT(posA.y, 8.0f);
}

// ---------------------------------------------------------------------------
// TEST 3 — Free-fall golden value: compare final Y against analytical formula
//
// Semi-implicit Euler: v += g*dt each step, then pos += v*dt.
// Analytical closed form for N steps of semi-implicit Euler:
//   vel_n = g * dt * n
//   pos_n = sum_{k=1}^{n} vel_k * dt = g * dt^2 * (1 + 2 + ... + n) = g * dt^2 * n*(n+1)/2
// ---------------------------------------------------------------------------

TEST(RigidBody2D_Golden, FreeFall_30Steps_MatchesAnalytical)
{
    const float dt      = 1.0f / 60.0f;
    const float g       = -9.81f;
    const float y0      = 100.0f;
    const int   kSteps  = 30;

    Dia::Geometry2D::Transform t;
    t.SetLocalPosition(Vector2D(0.0f, y0));

    WorldDef def;
    def.gravity       = Vector2D(0.0f, g);
    def.fixedTimestep = dt;
    def.maxSubSteps   = 8;
    def.broadPhase    = nullptr;
    PhysicsWorld world(def);

    PointBodyDef pd;
    pd.id           = Dia::Core::StringCRC("Faller");
    pd.type         = BodyType::kDynamic;
    pd.mass         = 1.0f;
    pd.linearDamping = 0.0f;
    pd.allowSleeping = false;
    pd.transform    = &t;
    PointBody2D* body = world.AddPointBody(pd);

    for (int i = 0; i < kSteps; ++i)
        world.Update(dt);

    // Semi-implicit Euler closed form (starting from rest at y0)
    float n    = static_cast<float>(kSteps);
    float disp = g * dt * dt * (n * (n + 1.0f) / 2.0f);
    float expectedY = y0 + disp;

    EXPECT_NEAR(body->GetTransform()->GetLocalPosition().y, expectedY, 0.01f);
}

// ---------------------------------------------------------------------------
// TEST 4 — Free-fall golden value: body falls below start after N steps
// ---------------------------------------------------------------------------

TEST(RigidBody2D_Golden, FreeFall_BodyFallsBelowStart)
{
    const float dt = 1.0f / 60.0f;

    Dia::Geometry2D::Transform t;
    t.SetLocalPosition(Vector2D(0.0f, 50.0f));

    WorldDef def;
    def.gravity       = Vector2D(0.0f, -9.81f);
    def.fixedTimestep = dt;
    def.maxSubSteps   = 8;
    def.broadPhase    = nullptr;
    PhysicsWorld world(def);

    PointBodyDef pd;
    pd.id           = Dia::Core::StringCRC("Drop");
    pd.type         = BodyType::kDynamic;
    pd.mass         = 1.0f;
    pd.linearDamping = 0.0f;
    pd.allowSleeping = false;
    pd.transform    = &t;
    PointBody2D* drop = world.AddPointBody(pd);

    for (int i = 0; i < 30; ++i)
        world.Update(dt);

    EXPECT_LT(drop->GetTransform()->GetLocalPosition().y, 50.0f);
    EXPECT_FLOAT_EQ(drop->GetTransform()->GetLocalPosition().x, 0.0f);
}

// ---------------------------------------------------------------------------
// TEST 5 — Static body does not move under gravity
// ---------------------------------------------------------------------------

TEST(RigidBody2D_Golden, StaticBody_NoMovementUnderGravity)
{
    const float dt = 1.0f / 60.0f;

    Dia::Geometry2D::Transform t;
    t.SetLocalPosition(Vector2D(3.0f, 7.0f));

    WorldDef def;
    def.gravity       = Vector2D(0.0f, -9.81f);
    def.fixedTimestep = dt;
    def.maxSubSteps   = 8;
    def.broadPhase    = nullptr;
    PhysicsWorld world(def);

    PointBodyDef pd;
    pd.id           = Dia::Core::StringCRC("Static");
    pd.type         = BodyType::kStatic;
    pd.mass         = 1.0f;
    pd.allowSleeping = false;
    pd.transform    = &t;
    world.AddPointBody(pd);

    for (int i = 0; i < 120; ++i)
        world.Update(dt);

    EXPECT_FLOAT_EQ(t.GetLocalPosition().x, 3.0f);
    EXPECT_FLOAT_EQ(t.GetLocalPosition().y, 7.0f);
}

// ---------------------------------------------------------------------------
// TEST 6 — Zero gravity: body with initial velocity moves in straight line
// ---------------------------------------------------------------------------

TEST(RigidBody2D_Golden, ZeroGravity_ConstantVelocity_GoldenPosition)
{
    const float dt       = 1.0f / 60.0f;
    const int   kSteps   = 60;
    const float vx       = 3.0f;

    Dia::Geometry2D::Transform t;
    t.SetLocalPosition(Vector2D(0.0f, 0.0f));

    WorldDef def;
    def.gravity       = Vector2D(0.0f, 0.0f);
    def.fixedTimestep = dt;
    def.maxSubSteps   = 8;
    def.broadPhase    = nullptr;
    PhysicsWorld world(def);

    PointBodyDef pd;
    pd.id           = Dia::Core::StringCRC("Coaster");
    pd.type         = BodyType::kDynamic;
    pd.mass         = 1.0f;
    pd.linearDamping = 0.0f;
    pd.allowSleeping = false;
    pd.transform    = &t;
    PointBody2D* body = world.AddPointBody(pd);
    body->SetVelocity(Vector2D(vx, 0.0f));

    for (int i = 0; i < kSteps; ++i)
        world.Update(dt);

    float expectedX = vx * dt * static_cast<float>(kSteps);
    EXPECT_NEAR(body->GetTransform()->GetLocalPosition().x, expectedX, 0.001f);
    EXPECT_NEAR(body->GetTransform()->GetLocalPosition().y, 0.0f, 1e-5f);
}

// ---------------------------------------------------------------------------
// TEST 7 — Stable simulation: two bodies, no NaN or Inf after 120 frames
// ---------------------------------------------------------------------------

TEST(RigidBody2D_Golden, StableSimulation_TwoBodies_NoNaNOrInf)
{
    const float dt = 1.0f / 60.0f;

    Dia::Geometry2D::Transform t0, t1;
    t0.SetLocalPosition(Vector2D(0.0f, 0.0f));
    t1.SetLocalPosition(Vector2D(10.0f, 5.0f));

    WorldDef def;
    def.gravity       = Vector2D(0.0f, -9.81f);
    def.fixedTimestep = dt;
    def.maxSubSteps   = 8;
    def.broadPhase    = nullptr;
    PhysicsWorld world(def);

    PointBodyDef pd0;
    pd0.id           = Dia::Core::StringCRC("S0");
    pd0.type         = BodyType::kDynamic;
    pd0.mass         = 1.0f;
    pd0.allowSleeping = false;
    PointBody2D* b0 = world.AddPointBody(pd0);
    b0->GetTransform()->SetLocalPosition(Vector2D(0.0f, 0.0f));

    PointBodyDef pd1;
    pd1.id           = Dia::Core::StringCRC("S1");
    pd1.type         = BodyType::kDynamic;
    pd1.mass         = 5.0f;
    pd1.allowSleeping = false;
    PointBody2D* b1 = world.AddPointBody(pd1);
    b1->GetTransform()->SetLocalPosition(Vector2D(10.0f, 5.0f));

    for (int i = 0; i < 120; ++i)
        world.Update(dt);

    EXPECT_FALSE(std::isnan(b0->GetTransform()->GetLocalPosition().x));
    EXPECT_FALSE(std::isnan(b0->GetTransform()->GetLocalPosition().y));
    EXPECT_FALSE(std::isinf(b0->GetTransform()->GetLocalPosition().x));
    EXPECT_FALSE(std::isinf(b0->GetTransform()->GetLocalPosition().y));

    EXPECT_FALSE(std::isnan(b1->GetTransform()->GetLocalPosition().x));
    EXPECT_FALSE(std::isnan(b1->GetTransform()->GetLocalPosition().y));
    EXPECT_FALSE(std::isinf(b1->GetTransform()->GetLocalPosition().x));
    EXPECT_FALSE(std::isinf(b1->GetTransform()->GetLocalPosition().y));
}

// ---------------------------------------------------------------------------
// TEST 8 — Stable simulation: RigidBody2D with rotation, 120 frames
// ---------------------------------------------------------------------------

TEST(RigidBody2D_Golden, StableSimulation_RigidBody_RotationNoNaNOrInf)
{
    const float dt = 1.0f / 60.0f;

    Dia::Geometry2D::Transform t;
    t.SetLocalPosition(Vector2D(0.0f, 5.0f));

    WorldDef def;
    def.gravity       = Vector2D(0.0f, -9.81f);
    def.fixedTimestep = dt;
    def.maxSubSteps   = 8;
    def.broadPhase    = nullptr;
    PhysicsWorld world(def);

    RigidBodyDef rd;
    rd.id             = Dia::Core::StringCRC("Spinner");
    rd.type           = BodyType::kDynamic;
    rd.mass           = 1.0f;
    rd.allowSleeping  = false;
    rd.momentOfInertia = 0.5f;
    rd.transform      = &t;
    RigidBody2D* rb = world.AddRigidBody(rd);
    rb->SetAngularVelocity(2.0f);  // 2 rad/s initial spin

    for (int i = 0; i < 120; ++i)
        world.Update(dt);

    EXPECT_FALSE(std::isnan(rb->GetTransform()->GetLocalPosition().x));
    EXPECT_FALSE(std::isnan(rb->GetTransform()->GetLocalPosition().y));
    EXPECT_FALSE(std::isinf(rb->GetTransform()->GetLocalPosition().x));
    EXPECT_FALSE(std::isinf(rb->GetTransform()->GetLocalPosition().y));
    EXPECT_FALSE(std::isnan(rb->GetTransform()->GetLocalRotation().AsRadians()));
    EXPECT_FALSE(std::isinf(rb->GetTransform()->GetLocalRotation().AsRadians()));
    EXPECT_FALSE(std::isnan(rb->GetAngularVelocity()));
    EXPECT_FALSE(std::isinf(rb->GetAngularVelocity()));
}

// ---------------------------------------------------------------------------
// TEST 9 — Free-fall velocity: after N steps velocity matches g*dt*N
// ---------------------------------------------------------------------------

TEST(RigidBody2D_Golden, FreeFall_VelocityMatchesGravityAccumulation)
{
    const float dt     = 1.0f / 60.0f;
    const float g      = -9.81f;
    const int   kSteps = 20;

    Dia::Geometry2D::Transform t;
    t.SetLocalPosition(Vector2D(0.0f, 500.0f));

    WorldDef def;
    def.gravity       = Vector2D(0.0f, g);
    def.fixedTimestep = dt;
    def.maxSubSteps   = 8;
    def.broadPhase    = nullptr;
    PhysicsWorld world(def);

    PointBodyDef pd;
    pd.id           = Dia::Core::StringCRC("VelCheck");
    pd.type         = BodyType::kDynamic;
    pd.mass         = 1.0f;
    pd.linearDamping = 0.0f;
    pd.allowSleeping = false;
    pd.transform    = &t;
    PointBody2D* body = world.AddPointBody(pd);

    for (int i = 0; i < kSteps; ++i)
        world.Update(dt);

    float expectedVelY = g * dt * static_cast<float>(kSteps);
    EXPECT_NEAR(body->GetVelocity().y, expectedVelY, 0.001f);
    EXPECT_FLOAT_EQ(body->GetVelocity().x, 0.0f);
}

// ---------------------------------------------------------------------------
// TEST 10 — Two bodies fall at same rate (mass independence)
// ---------------------------------------------------------------------------

TEST(RigidBody2D_Golden, FreeFall_MassIndependence_SameDisplacement)
{
    const float dt     = 1.0f / 60.0f;
    const int   kSteps = 60;

    Dia::Geometry2D::Transform tLight, tHeavy;
    tLight.SetLocalPosition(Vector2D(0.0f, 20.0f));
    tHeavy.SetLocalPosition(Vector2D(5.0f, 20.0f));

    WorldDef def;
    def.gravity       = Vector2D(0.0f, -9.81f);
    def.fixedTimestep = dt;
    def.maxSubSteps   = 8;
    def.broadPhase    = nullptr;
    PhysicsWorld world(def);

    PointBodyDef light;
    light.id           = Dia::Core::StringCRC("Light");
    light.type         = BodyType::kDynamic;
    light.mass         = 0.1f;
    light.linearDamping = 0.0f;
    light.allowSleeping = false;
    PointBody2D* bLight = world.AddPointBody(light);
    bLight->GetTransform()->SetLocalPosition(Vector2D(0.0f, 20.0f));

    PointBodyDef heavy;
    heavy.id           = Dia::Core::StringCRC("Heavy");
    heavy.type         = BodyType::kDynamic;
    heavy.mass         = 100.0f;
    heavy.linearDamping = 0.0f;
    heavy.allowSleeping = false;
    PointBody2D* bHeavy = world.AddPointBody(heavy);
    bHeavy->GetTransform()->SetLocalPosition(Vector2D(5.0f, 20.0f));

    for (int i = 0; i < kSteps; ++i)
        world.Update(dt);

    float displLight = bLight->GetTransform()->GetLocalPosition().y - 20.0f;
    float displHeavy = bHeavy->GetTransform()->GetLocalPosition().y - 20.0f;

    EXPECT_NEAR(displLight, displHeavy, 0.001f);
}
