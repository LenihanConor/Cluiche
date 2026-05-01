#include <gtest/gtest.h>

#include <DiaRigidBody2D/Bodies/PointBody2D.h>
#include <DiaRigidBody2D/Bodies/RigidBody2D.h>
#include <DiaGeometry2D/Transform/Transform.h>
#include <DiaMaths/Vector/Vector2D.h>

using namespace Dia::RigidBody2D;
using namespace Dia::Maths;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static PointBodyDef MakePointBodyDef(BodyType type = BodyType::kDynamic)
{
    PointBodyDef def;
    def.id   = Dia::Core::StringCRC("TestPoint");
    def.type = type;
    def.mass = 2.0f;
    def.restitution   = 0.3f;
    def.friction      = 0.6f;
    def.linearDamping = 0.1f;
    def.allowSleeping = true;
    def.layer         = 0x1;
    def.mask          = 0xFFFFFFFF;
    return def;
}

static RigidBodyDef MakeRigidBodyDef(BodyType type = BodyType::kDynamic)
{
    RigidBodyDef def;
    def.id   = Dia::Core::StringCRC("TestRigid");
    def.type = type;
    def.mass            = 4.0f;
    def.restitution     = 0.5f;
    def.friction        = 0.4f;
    def.linearDamping   = 0.05f;
    def.angularDamping  = 0.02f;
    def.momentOfInertia = 2.0f;
    def.allowSleeping   = true;
    def.layer           = 0x1;
    def.mask            = 0xFFFFFFFF;
    return def;
}

// ---------------------------------------------------------------------------
// PointBody2D
// ---------------------------------------------------------------------------

TEST(RigidBody2D_PointBody2D, DynamicConstruction_PropertiesMatchDef)
{
    PointBodyDef def = MakePointBodyDef();
    PointBody2D body(def);

    EXPECT_EQ(body.GetBodyType(),    BodyType::kDynamic);
    EXPECT_FLOAT_EQ(body.GetInverseMass(), 0.5f);  // 1/2
    EXPECT_FLOAT_EQ(body.GetRestitution(), 0.3f);
    EXPECT_FLOAT_EQ(body.GetFriction(),    0.6f);
    EXPECT_FLOAT_EQ(body.GetLinearDamping(), 0.1f);
    EXPECT_EQ(body.GetLayer(), 0x1u);
    EXPECT_EQ(body.GetMask(),  0xFFFFFFFFu);
    EXPECT_TRUE(body.IsAwake());
}

TEST(RigidBody2D_PointBody2D, StaticConstruction_InverseMassZero)
{
    PointBodyDef def = MakePointBodyDef(BodyType::kStatic);
    PointBody2D body(def);

    EXPECT_FLOAT_EQ(body.GetInverseMass(), 0.0f);
}

TEST(RigidBody2D_PointBody2D, ApplyForce_AccumulatesAndClears)
{
    PointBody2D body(MakePointBodyDef());

    body.ApplyForce(Vector2D(10.0f, 0.0f));
    body.ApplyForce(Vector2D(0.0f, 5.0f));

    EXPECT_FLOAT_EQ(body.GetForceAccum().x, 10.0f);
    EXPECT_FLOAT_EQ(body.GetForceAccum().y,  5.0f);

    body.ClearForces();
    EXPECT_FLOAT_EQ(body.GetForceAccum().x, 0.0f);
    EXPECT_FLOAT_EQ(body.GetForceAccum().y, 0.0f);
}

TEST(RigidBody2D_PointBody2D, ApplyImpulse_Static_VelocityUnchanged)
{
    PointBodyDef def = MakePointBodyDef(BodyType::kStatic);
    PointBody2D body(def);

    body.ApplyImpulse(Vector2D(100.0f, 100.0f));

    EXPECT_FLOAT_EQ(body.GetVelocity().x, 0.0f);
    EXPECT_FLOAT_EQ(body.GetVelocity().y, 0.0f);
}

TEST(RigidBody2D_PointBody2D, ApplyImpulse_Dynamic_ChangesVelocity)
{
    PointBody2D body(MakePointBodyDef());  // mass = 2 → invMass = 0.5

    body.ApplyImpulse(Vector2D(4.0f, 0.0f));

    EXPECT_FLOAT_EQ(body.GetVelocity().x, 2.0f);  // 4 * 0.5
    EXPECT_FLOAT_EQ(body.GetVelocity().y, 0.0f);
}

TEST(RigidBody2D_PointBody2D, ApplyForce_Static_NoEffect)
{
    PointBodyDef def = MakePointBodyDef(BodyType::kStatic);
    PointBody2D body(def);

    body.ApplyForce(Vector2D(100.0f, 100.0f));

    EXPECT_FLOAT_EQ(body.GetForceAccum().x, 0.0f);
    EXPECT_FLOAT_EQ(body.GetForceAccum().y, 0.0f);
}

// ---------------------------------------------------------------------------
// RigidBody2D
// ---------------------------------------------------------------------------

TEST(RigidBody2D_RigidBody2D, DynamicConstruction_PropertiesMatchDef)
{
    RigidBody2D body(MakeRigidBodyDef());

    EXPECT_EQ(body.GetBodyType(),       BodyType::kDynamic);
    EXPECT_FLOAT_EQ(body.GetInverseMass(),    0.25f);    // 1/4
    EXPECT_FLOAT_EQ(body.GetInverseInertia(), 0.5f);     // 1/2
    EXPECT_FLOAT_EQ(body.GetRestitution(),    0.5f);
    EXPECT_FLOAT_EQ(body.GetFriction(),       0.4f);
    EXPECT_FLOAT_EQ(body.GetLinearDamping(),  0.05f);
    EXPECT_FLOAT_EQ(body.GetAngularDamping(), 0.02f);
    EXPECT_FLOAT_EQ(body.GetAngularVelocity(), 0.0f);
    EXPECT_TRUE(body.IsAwake());
}

TEST(RigidBody2D_RigidBody2D, StaticConstruction_InversesZero)
{
    RigidBody2D body(MakeRigidBodyDef(BodyType::kStatic));

    EXPECT_FLOAT_EQ(body.GetInverseMass(),    0.0f);
    EXPECT_FLOAT_EQ(body.GetInverseInertia(), 0.0f);
}

TEST(RigidBody2D_RigidBody2D, ApplyForceAtPoint_OffCenter_ProducesTorque)
{
    Dia::Geometry2D::Transform transform;
    transform.SetWorldPosition(Vector2D(0.0f, 0.0f));

    RigidBodyDef def = MakeRigidBodyDef();
    def.transform = &transform;
    RigidBody2D body(def);

    // Apply force at (1, 0) with force (0, 1) — torque = 1*1 - 0*0 = 1
    body.ApplyForceAtPoint(Vector2D(0.0f, 1.0f), Vector2D(1.0f, 0.0f));

    EXPECT_FLOAT_EQ(body.GetTorqueAccum(), 1.0f);
    EXPECT_FLOAT_EQ(body.GetForceAccum().x, 0.0f);
    EXPECT_FLOAT_EQ(body.GetForceAccum().y, 1.0f);
}

TEST(RigidBody2D_RigidBody2D, ApplyAngularImpulse_Dynamic_ChangesAngularVelocity)
{
    RigidBody2D body(MakeRigidBodyDef());  // invInertia = 0.5

    body.ApplyAngularImpulse(4.0f);

    EXPECT_FLOAT_EQ(body.GetAngularVelocity(), 2.0f);  // 4 * 0.5
}

TEST(RigidBody2D_RigidBody2D, ApplyImpulse_Kinematic_NoEffect)
{
    RigidBody2D body(MakeRigidBodyDef(BodyType::kKinematic));

    body.ApplyImpulse(Vector2D(100.0f, 100.0f));

    EXPECT_FLOAT_EQ(body.GetVelocity().x, 0.0f);
    EXPECT_FLOAT_EQ(body.GetVelocity().y, 0.0f);
}

TEST(RigidBody2D_RigidBody2D, AddRemoveConstraint_CountCorrect)
{
    RigidBody2D body(MakeRigidBodyDef());

    // Use a dummy non-null pointer cast — we only test the list management
    IConstraint* dummy = reinterpret_cast<IConstraint*>(0x1);

    body.AddConstraint(dummy);
    EXPECT_EQ(body.GetConstraintCount(), 1);

    body.RemoveConstraint(dummy);
    EXPECT_EQ(body.GetConstraintCount(), 0);
}

TEST(RigidBody2D_RigidBody2D, RemoveConstraint_NonExistent_NoCrash)
{
    RigidBody2D body(MakeRigidBodyDef());
    IConstraint* dummy = reinterpret_cast<IConstraint*>(0x1);

    // Remove from empty list — should not crash
    body.RemoveConstraint(dummy);
    EXPECT_EQ(body.GetConstraintCount(), 0);
}

// ---------------------------------------------------------------------------
// Body2DBase — shared via pointer
// ---------------------------------------------------------------------------

TEST(RigidBody2D_Body2DBase, PointerToPointBody_ResolvesBaseFields)
{
    PointBody2D body(MakePointBodyDef());
    Body2DBase* base = &body;

    EXPECT_EQ(base->GetBodyType(),    BodyType::kDynamic);
    EXPECT_FLOAT_EQ(base->GetInverseMass(), 0.5f);
    EXPECT_TRUE(base->IsAwake());
    EXPECT_EQ(base->GetLayer(), 0x1u);
    EXPECT_EQ(base->GetMask(),  0xFFFFFFFFu);
}

TEST(RigidBody2D_Body2DBase, AllowSleepingFalse_SleepCallIgnored)
{
    PointBodyDef def = MakePointBodyDef();
    def.allowSleeping = false;
    PointBody2D body(def);

    body.Sleep();

    EXPECT_TRUE(body.IsAwake());
}

TEST(RigidBody2D_Body2DBase, SetMaskZero_GetMaskReturnsZero)
{
    PointBody2D body(MakePointBodyDef());
    body.SetMask(0);
    EXPECT_EQ(body.GetMask(), 0u);
}

TEST(RigidBody2D_Body2DBase, SetLayerAndMask_ImmediatelyReadable)
{
    PointBody2D body(MakePointBodyDef());
    body.SetLayer(0x4);
    body.SetMask(0xF0);
    EXPECT_EQ(body.GetLayer(), 0x4u);
    EXPECT_EQ(body.GetMask(),  0xF0u);
}

TEST(RigidBody2D_Body2DBase, WakeSleep_StateTransitions)
{
    PointBody2D body(MakePointBodyDef());
    EXPECT_TRUE(body.IsAwake());

    body.Sleep();
    EXPECT_FALSE(body.IsAwake());

    body.Wake();
    EXPECT_TRUE(body.IsAwake());
}
