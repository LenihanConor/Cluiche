#include <gtest/gtest.h>

#include <DiaSoftBody2D/SoftBodyWorld.h>
#include <DiaRigidBody2D/World/PhysicsWorld.h>
#include <DiaRigidBody2D/Bodies/RigidBody2D.h>
#include <DiaGeometry2D/Transform/Transform.h>
#include <DiaGeometry2D/Shapes/Circle.h>
#include <DiaMaths/Vector/Vector2D.h>
#include <DiaCore/CRC/StringCRC.h>

using namespace Dia::SoftBody2D;
using namespace Dia::Maths;

static Dia::RigidBody2D::WorldDef MakeRBWorldDef()
{
    Dia::RigidBody2D::WorldDef def;
    def.gravity = Vector2D(0.0f, 0.0f);
    def.fixedTimestep = 1.0f / 60.0f;
    def.maxSubSteps = 1;
    def.broadPhase = nullptr;
    return def;
}

static WorldDef MakeSBWorldDef(Dia::RigidBody2D::PhysicsWorld* rbWorld = nullptr)
{
    WorldDef def;
    def.gravity = Vector2D(0.0f, -9.81f);
    def.fixedTimestep = 1.0f / 60.0f;
    def.maxSubSteps = 8;
    def.solverIterations = 10;
    def.rigidBodyWorld = rbWorld;
    return def;
}

TEST(SoftBody2D_RigidBodyCoupling, NullRigidBodyWorld_NoOpNoCrash)
{
    WorldDef def = MakeSBWorldDef(nullptr);
    SoftBodyWorld world(def);

    RopeDef rdef;
    rdef.id = Dia::Core::StringCRC("CouplingRope");
    rdef.startPoint = Vector2D(0.0f, 0.0f);
    rdef.endPoint = Vector2D(4.0f, 0.0f);
    rdef.particleCount = 5;
    rdef.mass = 1.0f;
    rdef.stiffness = 1.0f;
    rdef.particleRadius = 0.1f;
    rdef.maxStretch = 0.0f;
    world.AddRope(rdef);

    for (int i = 0; i < 60; ++i)
        world.Update(1.0f / 60.0f);

    EXPECT_EQ(world.GetBodyCount(), 1);
}

TEST(SoftBody2D_RigidBodyCoupling, NullRigidBodyWorld_ClothSimulatesNormally)
{
    WorldDef def = MakeSBWorldDef(nullptr);
    SoftBodyWorld world(def);

    ClothDef cdef;
    cdef.id = Dia::Core::StringCRC("CouplingCloth");
    cdef.origin = Vector2D(0.0f, 0.0f);
    cdef.width = 2.0f;
    cdef.height = 2.0f;
    cdef.resX = 3;
    cdef.resY = 3;
    cdef.mass = 1.0f;
    cdef.structuralStiffness = 1.0f;
    cdef.shearStiffness = 0.5f;
    cdef.bendStiffness = 0.1f;
    cdef.particleRadius = 0.05f;
    cdef.maxStretch = 0.0f;
    cdef.pinTopRow = true;
    Cloth* cloth = world.AddCloth(cdef);

    float initialY = cloth->GetParticle(1, 2).position.y;
    for (int i = 0; i < 60; ++i)
        world.Update(1.0f / 60.0f);

    EXPECT_LT(cloth->GetParticle(1, 2).position.y, initialY);
}

TEST(SoftBody2D_RigidBodyCoupling, AnchorTracksRigidBodyPosition)
{
    Dia::RigidBody2D::WorldDef rbDef = MakeRBWorldDef();
    Dia::RigidBody2D::PhysicsWorld rbWorld(rbDef);

    Dia::Geometry2D::Transform anchorTransform;
    anchorTransform.SetWorldPosition(Vector2D(0.0f, 5.0f));
    Dia::Geometry2D::Circle anchorCircle(0.5f, Vector2D(0.0f, 0.0f));

    Dia::RigidBody2D::RigidBodyDef rbBodyDef;
    rbBodyDef.id = Dia::Core::StringCRC("Anchor");
    rbBodyDef.type = Dia::RigidBody2D::BodyType::kStatic;
    rbBodyDef.transform = &anchorTransform;
    rbBodyDef.circleShape = &anchorCircle;
    rbBodyDef.mass = 1.0f;
    Dia::RigidBody2D::RigidBody2D* anchorBody = rbWorld.AddRigidBody(rbBodyDef);

    WorldDef sbDef = MakeSBWorldDef(&rbWorld);
    sbDef.gravity = Vector2D(0.0f, 0.0f);
    SoftBodyWorld sbWorld(sbDef);

    RopeDef rdef;
    rdef.id = Dia::Core::StringCRC("AnchorRope");
    rdef.startPoint = Vector2D(0.0f, 5.0f);
    rdef.endPoint = Vector2D(0.0f, 0.0f);
    rdef.particleCount = 5;
    rdef.mass = 1.0f;
    rdef.stiffness = 1.0f;
    rdef.particleRadius = 0.1f;
    rdef.maxStretch = 0.0f;
    rdef.startAnchor = anchorBody;
    Rope* rope = sbWorld.AddRope(rdef);

    sbWorld.Update(1.0f / 60.0f);

    Vector2D anchorPos = anchorBody->GetTransform()->GetWorldPosition();
    EXPECT_NEAR(rope->GetParticle(0).position.x, anchorPos.x, 0.01f);
    EXPECT_NEAR(rope->GetParticle(0).position.y, anchorPos.y, 0.01f);
}

TEST(SoftBody2D_RigidBodyCoupling, AnchorPinnedEndpoint_NoBackImpulseOnPinned)
{
    Dia::RigidBody2D::WorldDef rbDef = MakeRBWorldDef();
    Dia::RigidBody2D::PhysicsWorld rbWorld(rbDef);

    Dia::Geometry2D::Transform anchorTransform;
    anchorTransform.SetWorldPosition(Vector2D(0.0f, 5.0f));
    Dia::Geometry2D::Circle anchorCircle(0.5f, Vector2D(0.0f, 0.0f));

    Dia::RigidBody2D::RigidBodyDef rbBodyDef;
    rbBodyDef.id = Dia::Core::StringCRC("DynAnchor");
    rbBodyDef.type = Dia::RigidBody2D::BodyType::kDynamic;
    rbBodyDef.transform = &anchorTransform;
    rbBodyDef.circleShape = &anchorCircle;
    rbBodyDef.mass = 1.0f;
    Dia::RigidBody2D::RigidBody2D* anchorBody = rbWorld.AddRigidBody(rbBodyDef);

    WorldDef sbDef = MakeSBWorldDef(&rbWorld);
    sbDef.gravity = Vector2D(0.0f, -50.0f);
    SoftBodyWorld sbWorld(sbDef);

    RopeDef rdef;
    rdef.id = Dia::Core::StringCRC("TensionRope");
    rdef.startPoint = Vector2D(0.0f, 5.0f);
    rdef.endPoint = Vector2D(0.0f, 0.0f);
    rdef.particleCount = 5;
    rdef.mass = 1.0f;
    rdef.stiffness = 1.0f;
    rdef.particleRadius = 0.1f;
    rdef.maxStretch = 0.0f;
    rdef.startAnchor = anchorBody;
    Rope* rope = sbWorld.AddRope(rdef);

    EXPECT_FLOAT_EQ(rope->GetParticle(0).invMass, 0.0f);

    for (int i = 0; i < 10; ++i)
        sbWorld.Update(1.0f / 60.0f);

    Vector2D vel = anchorBody->GetVelocity();
    EXPECT_NEAR(vel.x, 0.0f, 0.01f);
    EXPECT_NEAR(vel.y, 0.0f, 0.01f);
}

TEST(SoftBody2D_RigidBodyCoupling, PinnedEndpoint_NoBackImpulse)
{
    Dia::RigidBody2D::WorldDef rbDef = MakeRBWorldDef();
    Dia::RigidBody2D::PhysicsWorld rbWorld(rbDef);

    Dia::Geometry2D::Transform anchorTransform;
    anchorTransform.SetWorldPosition(Vector2D(0.0f, 5.0f));
    Dia::Geometry2D::Circle anchorCircle(0.5f, Vector2D(0.0f, 0.0f));

    Dia::RigidBody2D::RigidBodyDef rbBodyDef;
    rbBodyDef.id = Dia::Core::StringCRC("PinnedAnchor");
    rbBodyDef.type = Dia::RigidBody2D::BodyType::kDynamic;
    rbBodyDef.transform = &anchorTransform;
    rbBodyDef.circleShape = &anchorCircle;
    rbBodyDef.mass = 1.0f;
    Dia::RigidBody2D::RigidBody2D* anchorBody = rbWorld.AddRigidBody(rbBodyDef);

    WorldDef sbDef = MakeSBWorldDef(&rbWorld);
    sbDef.gravity = Vector2D(0.0f, -50.0f);
    SoftBodyWorld sbWorld(sbDef);

    RopeDef rdef;
    rdef.id = Dia::Core::StringCRC("PinnedRope");
    rdef.startPoint = Vector2D(0.0f, 5.0f);
    rdef.endPoint = Vector2D(0.0f, 0.0f);
    rdef.particleCount = 3;
    rdef.mass = 1.0f;
    rdef.stiffness = 1.0f;
    rdef.particleRadius = 0.1f;
    rdef.maxStretch = 0.0f;
    rdef.startAnchor = anchorBody;
    Rope* rope = sbWorld.AddRope(rdef);

    // Particle[0] is already pinned by anchor constructor (invMass=0)
    // Back-impulse should be skipped
    EXPECT_FLOAT_EQ(rope->GetParticle(0).invMass, 0.0f);

    sbWorld.Update(1.0f / 60.0f);

    // With invMass=0 on the anchor endpoint, the back-impulse is skipped
    // The anchor body should have zero velocity (no impulse applied)
    Vector2D vel = anchorBody->GetVelocity();
    EXPECT_NEAR(vel.x, 0.0f, 0.01f);
    EXPECT_NEAR(vel.y, 0.0f, 0.01f);
}

TEST(SoftBody2D_RigidBodyCoupling, QueryCircleReturnsNoCandidates_NoCrash)
{
    Dia::RigidBody2D::WorldDef rbDef = MakeRBWorldDef();
    Dia::RigidBody2D::PhysicsWorld rbWorld(rbDef);

    WorldDef sbDef = MakeSBWorldDef(&rbWorld);
    SoftBodyWorld sbWorld(sbDef);

    RopeDef rdef;
    rdef.id = Dia::Core::StringCRC("LonelyRope");
    rdef.startPoint = Vector2D(0.0f, 0.0f);
    rdef.endPoint = Vector2D(2.0f, 0.0f);
    rdef.particleCount = 3;
    rdef.mass = 1.0f;
    rdef.stiffness = 1.0f;
    rdef.particleRadius = 0.1f;
    rdef.maxStretch = 0.0f;
    sbWorld.AddRope(rdef);

    for (int i = 0; i < 10; ++i)
        sbWorld.Update(1.0f / 60.0f);

    SUCCEED();
}
