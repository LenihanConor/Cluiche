#include <gtest/gtest.h>

#include <DiaRigidBody2D/Integration/Integration.h>
#include <DiaRigidBody2D/Integration/MomentOfInertia.h>
#include <DiaRigidBody2D/Bodies/PointBody2D.h>
#include <DiaRigidBody2D/Bodies/RigidBody2D.h>
#include <DiaGeometry2D/Transform/Transform.h>
#include <DiaMaths/Vector/Vector2D.h>
#include <DiaMaths/Core/Angle.h>

using namespace Dia::RigidBody2D;
using namespace Dia::Maths;
using namespace Dia::Core::Containers;

static constexpr float kDt = 1.0f / 60.0f;
static const Vector2D  kGravity(0.0f, -9.81f);
static const Vector2D  kZeroGravity(0.0f, 0.0f);

// Helpers

static PointBody2D* MakePointBody(BodyType type, float mass = 1.0f, float linearDamping = 0.0f)
{
    PointBodyDef def;
    def.type          = type;
    def.mass          = mass;
    def.linearDamping = linearDamping;
    return new PointBody2D(def);
}

static RigidBody2D* MakeRigidBody(BodyType type, float mass = 1.0f,
                                  float linearDamping = 0.0f, float angularDamping = 0.0f,
                                  float moi = 1.0f)
{
    RigidBodyDef def;
    def.type            = type;
    def.mass            = mass;
    def.linearDamping   = linearDamping;
    def.angularDamping  = angularDamping;
    def.momentOfInertia = moi;
    return new RigidBody2D(def);
}

// ---------------------------------------------------------------------------
// PointBody2D integration tests
// ---------------------------------------------------------------------------

TEST(RigidBody2D_Integration, Static_PointBody_NoMovement)
{
    Dia::Geometry2D::Transform t;
    PointBodyDef def;
    def.type      = BodyType::kStatic;
    def.transform = &t;
    PointBody2D body(def);

    DynamicArrayC<PointBody2D*, kMaxPointBodies> pool;
    pool.Add(&body);

    IntegrateLinearForces(pool, kGravity, kDt);
    IntegrateLinearVelocities(pool, kDt);

    EXPECT_FLOAT_EQ(body.GetTransform()->GetLocalPosition().x, 0.0f);
    EXPECT_FLOAT_EQ(body.GetTransform()->GetLocalPosition().y, 0.0f);
    EXPECT_FLOAT_EQ(body.GetVelocity().x, 0.0f);
    EXPECT_FLOAT_EQ(body.GetVelocity().y, 0.0f);
}

TEST(RigidBody2D_Integration, Dynamic_PointBody_FreeFall_OneStep)
{
    Dia::Geometry2D::Transform t;
    PointBodyDef def;
    def.type      = BodyType::kDynamic;
    def.mass      = 1.0f;
    def.transform = &t;
    PointBody2D body(def);

    DynamicArrayC<PointBody2D*, kMaxPointBodies> pool;
    pool.Add(&body);

    IntegrateLinearForces(pool, kGravity, kDt);
    IntegrateLinearVelocities(pool, kDt);

    // After 1 step: vel = gravity * dt; pos = vel * dt (semi-implicit)
    float expectedVelY = kGravity.y * kDt;
    float expectedPosY = expectedVelY * kDt;

    EXPECT_FLOAT_EQ(body.GetVelocity().y, expectedVelY);
    EXPECT_NEAR(body.GetTransform()->GetLocalPosition().y, expectedPosY, 1e-6f);
}

TEST(RigidBody2D_Integration, Dynamic_PointBody_AppliedForce_OneStep)
{
    Dia::Geometry2D::Transform t;
    PointBodyDef def;
    def.type      = BodyType::kDynamic;
    def.mass      = 2.0f;
    def.transform = &t;
    PointBody2D body(def);
    body.ApplyForce(Vector2D(20.0f, 0.0f));  // a = 10 m/s²

    DynamicArrayC<PointBody2D*, kMaxPointBodies> pool;
    pool.Add(&body);

    IntegrateLinearForces(pool, kZeroGravity, kDt);
    IntegrateLinearVelocities(pool, kDt);

    float expectedVelX = 10.0f * kDt;
    EXPECT_NEAR(body.GetVelocity().x, expectedVelX, 1e-6f);
    EXPECT_NEAR(body.GetTransform()->GetLocalPosition().x, expectedVelX * kDt, 1e-6f);
}

TEST(RigidBody2D_Integration, Dynamic_PointBody_LinearDamping)
{
    PointBodyDef def;
    def.type          = BodyType::kDynamic;
    def.mass          = 1.0f;
    def.linearDamping = 0.1f;
    PointBody2D body(def);
    body.SetVelocity(Vector2D(10.0f, 0.0f));

    DynamicArrayC<PointBody2D*, kMaxPointBodies> pool;
    pool.Add(&body);

    IntegrateLinearForces(pool, kZeroGravity, kDt);

    float expected = 10.0f * (1.0f - 0.1f * kDt);
    EXPECT_NEAR(body.GetVelocity().x, expected, 1e-5f);
}

TEST(RigidBody2D_Integration, Kinematic_PointBody_VelocityApplied_GravityIgnored)
{
    Dia::Geometry2D::Transform t;
    PointBodyDef def;
    def.type      = BodyType::kKinematic;
    def.mass      = 1.0f;
    def.transform = &t;
    PointBody2D body(def);
    body.SetVelocity(Vector2D(5.0f, 0.0f));

    DynamicArrayC<PointBody2D*, kMaxPointBodies> pool;
    pool.Add(&body);

    IntegrateLinearForces(pool, kGravity, kDt);
    IntegrateLinearVelocities(pool, kDt);

    // Velocity unchanged by gravity; position moved by velocity
    EXPECT_FLOAT_EQ(body.GetVelocity().x, 5.0f);
    EXPECT_FLOAT_EQ(body.GetVelocity().y, 0.0f);
    EXPECT_NEAR(body.GetTransform()->GetLocalPosition().x, 5.0f * kDt, 1e-6f);
}

// ---------------------------------------------------------------------------
// RigidBody2D integration tests
// ---------------------------------------------------------------------------

TEST(RigidBody2D_Integration, Dynamic_RigidBody_Torque_AngularVelocity)
{
    RigidBodyDef def;
    def.type            = BodyType::kDynamic;
    def.mass            = 1.0f;
    def.momentOfInertia = 2.0f;  // invInertia = 0.5
    RigidBody2D body(def);
    body.ApplyTorque(10.0f);  // angularAccel = 10 * 0.5 = 5

    DynamicArrayC<RigidBody2D*, kMaxRigidBodies> pool;
    pool.Add(&body);

    IntegrateAngularForces(pool, kDt);

    float expected = 5.0f * kDt;
    EXPECT_NEAR(body.GetAngularVelocity(), expected, 1e-6f);
}

TEST(RigidBody2D_Integration, Dynamic_RigidBody_AngularDamping)
{
    RigidBodyDef def;
    def.type            = BodyType::kDynamic;
    def.momentOfInertia = 1.0f;
    def.angularDamping  = 0.2f;
    RigidBody2D body(def);
    body.SetAngularVelocity(10.0f);

    DynamicArrayC<RigidBody2D*, kMaxRigidBodies> pool;
    pool.Add(&body);

    IntegrateAngularForces(pool, kDt);

    float expected = 10.0f * (1.0f - 0.2f * kDt);
    EXPECT_NEAR(body.GetAngularVelocity(), expected, 1e-5f);
}

TEST(RigidBody2D_Integration, Dynamic_RigidBody_RotationIntegration)
{
    Dia::Geometry2D::Transform t;
    RigidBodyDef def;
    def.type            = BodyType::kDynamic;
    def.momentOfInertia = 1.0f;
    def.transform       = &t;
    RigidBody2D body(def);
    body.SetAngularVelocity(1.0f);  // 1 rad/s

    DynamicArrayC<RigidBody2D*, kMaxRigidBodies> pool;
    pool.Add(&body);

    IntegrateAngularVelocities(pool, kDt);

    float expected = 1.0f * kDt;
    EXPECT_NEAR(body.GetTransform()->GetLocalRotation().AsRadians(), expected, 1e-6f);
}

// ---------------------------------------------------------------------------
// Moment of inertia helpers
// ---------------------------------------------------------------------------

TEST(RigidBody2D_MomentOfInertia, Circle)
{
    // I = 0.5 * m * r²
    EXPECT_NEAR(MomentOfInertia::ForCircle(2.0f, 3.0f), 0.5f * 2.0f * 9.0f, 1e-6f);
}

TEST(RigidBody2D_MomentOfInertia, AARect)
{
    // I = m * (w² + h²) / 12
    EXPECT_NEAR(MomentOfInertia::ForAARect(3.0f, 4.0f, 2.0f), 3.0f * (16.0f + 4.0f) / 12.0f, 1e-6f);
}

TEST(RigidBody2D_MomentOfInertia, Triangle)
{
    // I = m * (b² + h²) / 18
    EXPECT_NEAR(MomentOfInertia::ForTriangle(1.0f, 3.0f, 4.0f), (9.0f + 16.0f) / 18.0f, 1e-5f);
}
