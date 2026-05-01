#include <gtest/gtest.h>

#include <DiaSoftBody2D/SoftBodyWorld.h>
#include <DiaRigidBody2D/World/PhysicsWorld.h>
#include <DiaRigidBody2D/Bodies/RigidBody2D.h>
#include <DiaGeometry2D/Transform/Transform.h>
#include <DiaGeometry2D/Shapes/AARect.h>
#include <DiaGeometry2D/Shapes/Circle.h>
#include <DiaGeometry2D/Shapes/Line.h>
#include <DiaMaths/Vector/Vector2D.h>
#include <DiaCore/CRC/StringCRC.h>

#include <cmath>

using namespace Dia::SoftBody2D;
using namespace Dia::Maths;

// ---------------------------------------------------------------------------
// Rope settling under gravity
// ---------------------------------------------------------------------------

TEST(SoftBody2D_Integration, RopeSettling_TotalLengthNearRestLength)
{
    WorldDef def;
    def.gravity = Vector2D(0.0f, -9.81f);
    def.fixedTimestep = 1.0f / 60.0f;
    def.maxSubSteps = 8;
    def.solverIterations = 10;
    def.rigidBodyWorld = nullptr;
    SoftBodyWorld world(def);

    RopeDef rdef;
    rdef.id = Dia::Core::StringCRC("SettleRope");
    rdef.startPoint = Vector2D(0.0f, 0.0f);
    rdef.endPoint = Vector2D(0.0f, -4.0f);
    rdef.particleCount = 10;
    rdef.mass = 1.0f;
    rdef.stiffness = 1.0f;
    rdef.particleRadius = 0.05f;
    rdef.maxStretch = 0.0f;
    Rope* rope = world.AddRope(rdef);
    rope->GetParticle(0).invMass = 0.0f;

    float totalRestLength = 0.0f;
    for (int c = 0; c < rope->GetConstraintCount(); ++c)
        totalRestLength += rope->GetConstraint(c).restLength;

    for (int i = 0; i < 300; ++i)
        world.Update(1.0f / 60.0f);

    float totalCurrentLength = 0.0f;
    for (int c = 0; c < rope->GetConstraintCount(); ++c)
    {
        const DistanceConstraint& con = rope->GetConstraint(c);
        float dist = (rope->GetParticle(con.indexB).position - rope->GetParticle(con.indexA).position).Magnitude();
        totalCurrentLength += dist;
    }

    EXPECT_NEAR(totalCurrentLength, totalRestLength, totalRestLength * 0.05f);
}

// ---------------------------------------------------------------------------
// Pinned cloth drape
// ---------------------------------------------------------------------------

TEST(SoftBody2D_Integration, PinnedClothDrape_BottomBelowTop)
{
    WorldDef def;
    def.gravity = Vector2D(0.0f, -9.81f);
    def.fixedTimestep = 1.0f / 60.0f;
    def.maxSubSteps = 8;
    def.solverIterations = 10;
    def.rigidBodyWorld = nullptr;
    SoftBodyWorld world(def);

    ClothDef cdef;
    cdef.id = Dia::Core::StringCRC("DrapeCloth");
    cdef.origin = Vector2D(0.0f, 0.0f);
    cdef.width = 2.0f;
    cdef.height = 2.0f;
    cdef.resX = 5;
    cdef.resY = 5;
    cdef.mass = 1.0f;
    cdef.structuralStiffness = 1.0f;
    cdef.shearStiffness = 0.5f;
    cdef.bendStiffness = 0.1f;
    cdef.particleRadius = 0.02f;
    cdef.maxStretch = 0.0f;
    cdef.pinTopRow = true;
    Cloth* cloth = world.AddCloth(cdef);

    for (int i = 0; i < 300; ++i)
        world.Update(1.0f / 60.0f);

    float topY = cloth->GetParticle(2, 0).position.y;
    float bottomY = cloth->GetParticle(2, 4).position.y;
    EXPECT_LT(bottomY, topY);

    for (int y = 0; y < cloth->GetResY(); ++y)
    {
        for (int x = 0; x < cloth->GetResX(); ++x)
        {
            EXPECT_FALSE(std::isnan(cloth->GetParticle(x, y).position.x));
            EXPECT_FALSE(std::isnan(cloth->GetParticle(x, y).position.y));
            EXPECT_FALSE(std::isinf(cloth->GetParticle(x, y).position.x));
            EXPECT_FALSE(std::isinf(cloth->GetParticle(x, y).position.y));
        }
    }
}

// ---------------------------------------------------------------------------
// Rope on floor
// ---------------------------------------------------------------------------

TEST(SoftBody2D_Integration, RopeOnFloor_AllParticlesAboveSurface)
{
    Dia::Geometry2D::AARect floor(Vector2D(-10.0f, -5.1f), Vector2D(10.0f, -5.0f));

    WorldDef def;
    def.gravity = Vector2D(0.0f, -9.81f);
    def.fixedTimestep = 1.0f / 60.0f;
    def.maxSubSteps = 8;
    def.solverIterations = 10;
    def.rigidBodyWorld = nullptr;
    SoftBodyWorld world(def);
    world.AddStaticShape(&floor);

    RopeDef rdef;
    rdef.id = Dia::Core::StringCRC("FloorRope");
    rdef.startPoint = Vector2D(0.0f, 0.0f);
    rdef.endPoint = Vector2D(0.0f, -3.0f);
    rdef.particleCount = 5;
    rdef.mass = 1.0f;
    rdef.stiffness = 1.0f;
    rdef.particleRadius = 0.1f;
    rdef.maxStretch = 0.0f;
    Rope* rope = world.AddRope(rdef);
    rope->GetParticle(0).invMass = 0.0f;

    for (int i = 0; i < 300; ++i)
        world.Update(1.0f / 60.0f);

    for (int p = 0; p < rope->GetParticleCount(); ++p)
    {
        float y = rope->GetParticle(p).position.y;
        EXPECT_GE(y, -5.0f - rope->GetParticle(p).radius - 0.5f)
            << "Particle " << p << " at y=" << y << " fell through floor";
    }

    world.RemoveStaticShape(&floor);
}

// ---------------------------------------------------------------------------
// Tearing mid-simulation
// ---------------------------------------------------------------------------

TEST(SoftBody2D_Integration, TearingMidSimulation_BothSidesContinue)
{
    WorldDef def;
    def.gravity = Vector2D(0.0f, -50.0f);
    def.fixedTimestep = 1.0f / 60.0f;
    def.maxSubSteps = 8;
    def.solverIterations = 4;
    def.rigidBodyWorld = nullptr;
    SoftBodyWorld world(def);

    RopeDef rdef;
    rdef.id = Dia::Core::StringCRC("TearMidRope");
    rdef.startPoint = Vector2D(0.0f, 0.0f);
    rdef.endPoint = Vector2D(0.0f, -4.0f);
    rdef.particleCount = 5;
    rdef.mass = 0.5f;
    rdef.stiffness = 0.01f;
    rdef.particleRadius = 0.05f;
    rdef.maxStretch = 0.001f;
    Rope* rope = world.AddRope(rdef);
    rope->GetParticle(0).invMass = 0.0f;

    bool tornDuringRun = false;
    for (int i = 0; i < 300; ++i)
    {
        world.Update(1.0f / 60.0f);
        if (rope->IsTorn() && !tornDuringRun)
        {
            tornDuringRun = true;
        }
    }

    EXPECT_TRUE(tornDuringRun);

    for (int p = 0; p < rope->GetParticleCount(); ++p)
    {
        EXPECT_FALSE(std::isnan(rope->GetParticle(p).position.x));
        EXPECT_FALSE(std::isnan(rope->GetParticle(p).position.y));
    }
}

// ---------------------------------------------------------------------------
// Add/remove/add lifecycle
// ---------------------------------------------------------------------------

TEST(SoftBody2D_Integration, AddRemoveAdd_Lifecycle)
{
    WorldDef def;
    def.gravity = Vector2D(0.0f, -9.81f);
    def.fixedTimestep = 1.0f / 60.0f;
    def.maxSubSteps = 8;
    def.solverIterations = 4;
    def.rigidBodyWorld = nullptr;
    SoftBodyWorld world(def);

    RopeDef rdef;
    rdef.id = Dia::Core::StringCRC("LifecycleRope");
    rdef.startPoint = Vector2D(0.0f, 0.0f);
    rdef.endPoint = Vector2D(2.0f, 0.0f);
    rdef.particleCount = 3;
    rdef.mass = 1.0f;
    rdef.stiffness = 1.0f;
    rdef.particleRadius = 0.1f;
    rdef.maxStretch = 0.0f;
    Rope* rope = world.AddRope(rdef);
    EXPECT_EQ(world.GetBodyCount(), 1);

    world.Update(1.0f / 60.0f);

    world.RemoveBody(rope);
    EXPECT_EQ(world.GetBodyCount(), 0);

    world.Update(1.0f / 60.0f);

    ClothDef cdef;
    cdef.id = Dia::Core::StringCRC("LifecycleCloth");
    cdef.origin = Vector2D(0.0f, 0.0f);
    cdef.width = 1.0f;
    cdef.height = 1.0f;
    cdef.resX = 3;
    cdef.resY = 3;
    cdef.mass = 1.0f;
    cdef.structuralStiffness = 1.0f;
    cdef.shearStiffness = 0.5f;
    cdef.bendStiffness = 0.1f;
    cdef.particleRadius = 0.05f;
    cdef.maxStretch = 0.0f;
    cdef.pinTopRow = false;
    Cloth* cloth = world.AddCloth(cdef);
    EXPECT_EQ(world.GetBodyCount(), 1);
    (void)cloth;

    world.Update(1.0f / 60.0f);
    EXPECT_EQ(world.GetBodyCount(), 1);
}

// ---------------------------------------------------------------------------
// Full two-way coupling
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Cloth tearing through simulation
// ---------------------------------------------------------------------------

TEST(SoftBody2D_Integration, ClothTearing_TornParticlesFallFreely)
{
    WorldDef def;
    def.gravity = Vector2D(0.0f, -50.0f);
    def.fixedTimestep = 1.0f / 60.0f;
    def.maxSubSteps = 8;
    def.solverIterations = 4;
    def.rigidBodyWorld = nullptr;
    SoftBodyWorld world(def);

    ClothDef cdef;
    cdef.id = Dia::Core::StringCRC("TearCloth");
    cdef.origin = Vector2D(0.0f, 0.0f);
    cdef.width = 2.0f;
    cdef.height = 2.0f;
    cdef.resX = 4;
    cdef.resY = 4;
    cdef.mass = 0.2f;
    cdef.structuralStiffness = 0.01f;
    cdef.shearStiffness = 0.01f;
    cdef.bendStiffness = 0.01f;
    cdef.particleRadius = 0.02f;
    cdef.maxStretch = 0.001f;
    cdef.pinTopRow = true;
    Cloth* cloth = world.AddCloth(cdef);

    for (int i = 0; i < 300; ++i)
        world.Update(1.0f / 60.0f);

    EXPECT_TRUE(cloth->IsTorn());

    for (int y = 0; y < cloth->GetResY(); ++y)
    {
        for (int x = 0; x < cloth->GetResX(); ++x)
        {
            EXPECT_FALSE(std::isnan(cloth->GetParticle(x, y).position.x));
            EXPECT_FALSE(std::isnan(cloth->GetParticle(x, y).position.y));
        }
    }
}

// ---------------------------------------------------------------------------
// Cloth on floor
// ---------------------------------------------------------------------------

TEST(SoftBody2D_Integration, ClothOnFloor_ParticlesStayAboveSurface)
{
    Dia::Geometry2D::AARect floor(Vector2D(-10.0f, -5.1f), Vector2D(10.0f, -5.0f));

    WorldDef def;
    def.gravity = Vector2D(0.0f, -9.81f);
    def.fixedTimestep = 1.0f / 60.0f;
    def.maxSubSteps = 8;
    def.solverIterations = 10;
    def.rigidBodyWorld = nullptr;
    SoftBodyWorld world(def);
    world.AddStaticShape(&floor);

    ClothDef cdef;
    cdef.id = Dia::Core::StringCRC("FloorCloth");
    cdef.origin = Vector2D(-1.0f, 0.0f);
    cdef.width = 2.0f;
    cdef.height = 2.0f;
    cdef.resX = 4;
    cdef.resY = 4;
    cdef.mass = 1.0f;
    cdef.structuralStiffness = 1.0f;
    cdef.shearStiffness = 0.5f;
    cdef.bendStiffness = 0.1f;
    cdef.particleRadius = 0.05f;
    cdef.maxStretch = 0.0f;
    cdef.pinTopRow = true;
    Cloth* cloth = world.AddCloth(cdef);

    for (int i = 0; i < 300; ++i)
        world.Update(1.0f / 60.0f);

    for (int y = 0; y < cloth->GetResY(); ++y)
    {
        for (int x = 0; x < cloth->GetResX(); ++x)
        {
            EXPECT_GE(cloth->GetParticle(x, y).position.y, -5.0f - cloth->GetParticle(x, y).radius - 0.5f);
        }
    }

    world.RemoveStaticShape(&floor);
}

// ---------------------------------------------------------------------------
// Rope on circle obstacle
// ---------------------------------------------------------------------------

TEST(SoftBody2D_Integration, RopeOnCircle_DrapesAround)
{
    Dia::Geometry2D::Circle obstacle(1.0f, Vector2D(0.0f, -3.0f));

    WorldDef def;
    def.gravity = Vector2D(0.0f, -9.81f);
    def.fixedTimestep = 1.0f / 60.0f;
    def.maxSubSteps = 8;
    def.solverIterations = 10;
    def.rigidBodyWorld = nullptr;
    SoftBodyWorld world(def);
    world.AddStaticShape(&obstacle);

    RopeDef rdef;
    rdef.id = Dia::Core::StringCRC("CircleRope");
    rdef.startPoint = Vector2D(0.0f, 0.0f);
    rdef.endPoint = Vector2D(0.0f, -2.0f);
    rdef.particleCount = 10;
    rdef.mass = 1.0f;
    rdef.stiffness = 1.0f;
    rdef.particleRadius = 0.05f;
    rdef.maxStretch = 0.0f;
    Rope* rope = world.AddRope(rdef);
    rope->GetParticle(0).invMass = 0.0f;

    for (int i = 0; i < 300; ++i)
        world.Update(1.0f / 60.0f);

    for (int p = 0; p < rope->GetParticleCount(); ++p)
    {
        Vector2D pos = rope->GetParticle(p).position;
        float distFromCenter = (pos - obstacle.GetCenter()).Magnitude();
        EXPECT_GE(distFromCenter, obstacle.GetRadius() + rope->GetParticle(p).radius - 0.5f);
    }

    world.RemoveStaticShape(&obstacle);
}

// ---------------------------------------------------------------------------
// Multiple bodies + geometry simultaneously
// ---------------------------------------------------------------------------

TEST(SoftBody2D_Integration, MultipleBodiesWithGeometry_NoCrashNoNaN)
{
    Dia::Geometry2D::AARect floor(Vector2D(-10.0f, -10.1f), Vector2D(10.0f, -10.0f));
    Dia::Geometry2D::Circle pillar(0.5f, Vector2D(2.0f, -5.0f));
    Dia::Geometry2D::Line wall(Vector2D(5.0f, -15.0f), Vector2D(5.0f, 5.0f));

    WorldDef def;
    def.gravity = Vector2D(0.0f, -9.81f);
    def.fixedTimestep = 1.0f / 60.0f;
    def.maxSubSteps = 8;
    def.solverIterations = 10;
    def.rigidBodyWorld = nullptr;
    SoftBodyWorld world(def);
    world.AddStaticShape(&floor);
    world.AddStaticShape(&pillar);
    world.AddStaticShape(&wall);

    RopeDef rdef;
    rdef.id = Dia::Core::StringCRC("MultiRope1");
    rdef.startPoint = Vector2D(0.0f, 0.0f);
    rdef.endPoint = Vector2D(0.0f, -3.0f);
    rdef.particleCount = 8;
    rdef.mass = 1.0f;
    rdef.stiffness = 1.0f;
    rdef.particleRadius = 0.05f;
    rdef.maxStretch = 0.0f;
    Rope* rope1 = world.AddRope(rdef);
    rope1->GetParticle(0).invMass = 0.0f;

    rdef.id = Dia::Core::StringCRC("MultiRope2");
    rdef.startPoint = Vector2D(3.0f, 2.0f);
    rdef.endPoint = Vector2D(3.0f, -1.0f);
    Rope* rope2 = world.AddRope(rdef);
    rope2->GetParticle(0).invMass = 0.0f;

    ClothDef cdef;
    cdef.id = Dia::Core::StringCRC("MultiCloth");
    cdef.origin = Vector2D(-3.0f, 2.0f);
    cdef.width = 2.0f;
    cdef.height = 2.0f;
    cdef.resX = 4;
    cdef.resY = 4;
    cdef.mass = 1.0f;
    cdef.structuralStiffness = 1.0f;
    cdef.shearStiffness = 0.5f;
    cdef.bendStiffness = 0.1f;
    cdef.particleRadius = 0.03f;
    cdef.maxStretch = 0.0f;
    cdef.pinTopRow = true;
    Cloth* cloth = world.AddCloth(cdef);

    EXPECT_EQ(world.GetBodyCount(), 3);

    for (int i = 0; i < 300; ++i)
        world.Update(1.0f / 60.0f);

    for (int p = 0; p < rope1->GetParticleCount(); ++p)
    {
        EXPECT_FALSE(std::isnan(rope1->GetParticle(p).position.x));
        EXPECT_FALSE(std::isnan(rope1->GetParticle(p).position.y));
    }
    for (int p = 0; p < rope2->GetParticleCount(); ++p)
    {
        EXPECT_FALSE(std::isnan(rope2->GetParticle(p).position.x));
        EXPECT_FALSE(std::isnan(rope2->GetParticle(p).position.y));
    }
    for (int y = 0; y < cloth->GetResY(); ++y)
    {
        for (int x = 0; x < cloth->GetResX(); ++x)
        {
            EXPECT_FALSE(std::isnan(cloth->GetParticle(x, y).position.x));
            EXPECT_FALSE(std::isnan(cloth->GetParticle(x, y).position.y));
        }
    }

    world.RemoveStaticShape(&floor);
    world.RemoveStaticShape(&pillar);
    world.RemoveStaticShape(&wall);
}

// ---------------------------------------------------------------------------
// Long-running numerical stability
// ---------------------------------------------------------------------------

TEST(SoftBody2D_Integration, LongRunning_NoNaNOrInfAfter600Frames)
{
    WorldDef def;
    def.gravity = Vector2D(0.0f, -9.81f);
    def.fixedTimestep = 1.0f / 60.0f;
    def.maxSubSteps = 8;
    def.solverIterations = 10;
    def.rigidBodyWorld = nullptr;
    SoftBodyWorld world(def);

    RopeDef rdef;
    rdef.id = Dia::Core::StringCRC("StableRope");
    rdef.startPoint = Vector2D(0.0f, 0.0f);
    rdef.endPoint = Vector2D(0.0f, -5.0f);
    rdef.particleCount = 20;
    rdef.mass = 2.0f;
    rdef.stiffness = 1.0f;
    rdef.particleRadius = 0.05f;
    rdef.maxStretch = 0.0f;
    Rope* rope = world.AddRope(rdef);
    rope->GetParticle(0).invMass = 0.0f;

    ClothDef cdef;
    cdef.id = Dia::Core::StringCRC("StableCloth");
    cdef.origin = Vector2D(5.0f, 0.0f);
    cdef.width = 3.0f;
    cdef.height = 3.0f;
    cdef.resX = 6;
    cdef.resY = 6;
    cdef.mass = 2.0f;
    cdef.structuralStiffness = 1.0f;
    cdef.shearStiffness = 0.5f;
    cdef.bendStiffness = 0.2f;
    cdef.particleRadius = 0.02f;
    cdef.maxStretch = 0.0f;
    cdef.pinTopRow = true;
    Cloth* cloth = world.AddCloth(cdef);

    for (int i = 0; i < 600; ++i)
        world.Update(1.0f / 60.0f);

    for (int p = 0; p < rope->GetParticleCount(); ++p)
    {
        EXPECT_FALSE(std::isnan(rope->GetParticle(p).position.x));
        EXPECT_FALSE(std::isnan(rope->GetParticle(p).position.y));
        EXPECT_FALSE(std::isinf(rope->GetParticle(p).position.x));
        EXPECT_FALSE(std::isinf(rope->GetParticle(p).position.y));
    }
    for (int y = 0; y < cloth->GetResY(); ++y)
    {
        for (int x = 0; x < cloth->GetResX(); ++x)
        {
            EXPECT_FALSE(std::isnan(cloth->GetParticle(x, y).position.x));
            EXPECT_FALSE(std::isnan(cloth->GetParticle(x, y).position.y));
            EXPECT_FALSE(std::isinf(cloth->GetParticle(x, y).position.x));
            EXPECT_FALSE(std::isinf(cloth->GetParticle(x, y).position.y));
        }
    }
}

// ---------------------------------------------------------------------------
// Pinned corners — cloth sags in middle
// ---------------------------------------------------------------------------

TEST(SoftBody2D_Integration, PinnedCorners_ClothSagsInMiddle)
{
    WorldDef def;
    def.gravity = Vector2D(0.0f, -9.81f);
    def.fixedTimestep = 1.0f / 60.0f;
    def.maxSubSteps = 8;
    def.solverIterations = 10;
    def.rigidBodyWorld = nullptr;
    SoftBodyWorld world(def);

    ClothDef cdef;
    cdef.id = Dia::Core::StringCRC("CornerCloth");
    cdef.origin = Vector2D(0.0f, 0.0f);
    cdef.width = 4.0f;
    cdef.height = 4.0f;
    cdef.resX = 5;
    cdef.resY = 5;
    cdef.mass = 1.0f;
    cdef.structuralStiffness = 1.0f;
    cdef.shearStiffness = 0.5f;
    cdef.bendStiffness = 0.1f;
    cdef.particleRadius = 0.02f;
    cdef.maxStretch = 0.0f;
    cdef.pinTopRow = false;
    Cloth* cloth = world.AddCloth(cdef);

    cloth->PinParticle(0, 0);
    cloth->PinParticle(4, 0);
    cloth->PinParticle(0, 4);
    cloth->PinParticle(4, 4);

    float cornerY = cloth->GetParticle(0, 0).position.y;

    for (int i = 0; i < 300; ++i)
        world.Update(1.0f / 60.0f);

    float middleY = cloth->GetParticle(2, 2).position.y;
    EXPECT_LT(middleY, cornerY);
}

// ---------------------------------------------------------------------------
// Runtime unpin triggers movement
// ---------------------------------------------------------------------------

TEST(SoftBody2D_Integration, RuntimeUnpin_ParticleBeginsFalling)
{
    WorldDef def;
    def.gravity = Vector2D(0.0f, -9.81f);
    def.fixedTimestep = 1.0f / 60.0f;
    def.maxSubSteps = 8;
    def.solverIterations = 10;
    def.rigidBodyWorld = nullptr;
    SoftBodyWorld world(def);

    ClothDef cdef;
    cdef.id = Dia::Core::StringCRC("UnpinCloth");
    cdef.origin = Vector2D(0.0f, 0.0f);
    cdef.width = 2.0f;
    cdef.height = 2.0f;
    cdef.resX = 3;
    cdef.resY = 3;
    cdef.mass = 1.0f;
    cdef.structuralStiffness = 1.0f;
    cdef.shearStiffness = 0.5f;
    cdef.bendStiffness = 0.1f;
    cdef.particleRadius = 0.02f;
    cdef.maxStretch = 0.0f;
    cdef.pinTopRow = true;
    Cloth* cloth = world.AddCloth(cdef);

    for (int i = 0; i < 60; ++i)
        world.Update(1.0f / 60.0f);

    float yBeforeUnpin = cloth->GetParticle(1, 0).position.y;

    cloth->UnpinParticle(1, 0);

    for (int i = 0; i < 60; ++i)
        world.Update(1.0f / 60.0f);

    EXPECT_LT(cloth->GetParticle(1, 0).position.y, yBeforeUnpin);
}

// ---------------------------------------------------------------------------
// Full two-way coupling
// ---------------------------------------------------------------------------

TEST(SoftBody2D_Integration, TwoWayCoupling_AnchorFollowsRigidBody)
{
    Dia::RigidBody2D::WorldDef rbDef;
    rbDef.gravity = Vector2D(0.0f, 0.0f);
    rbDef.fixedTimestep = 1.0f / 60.0f;
    rbDef.maxSubSteps = 1;
    rbDef.broadPhase = nullptr;
    Dia::RigidBody2D::PhysicsWorld rbWorld(rbDef);

    Dia::Geometry2D::Transform anchorTransform;
    anchorTransform.SetWorldPosition(Vector2D(0.0f, 5.0f));
    Dia::Geometry2D::Circle anchorCircle(0.3f, Vector2D(0.0f, 0.0f));

    Dia::RigidBody2D::RigidBodyDef rbBodyDef;
    rbBodyDef.id = Dia::Core::StringCRC("CoupledBody");
    rbBodyDef.type = Dia::RigidBody2D::BodyType::kDynamic;
    rbBodyDef.transform = &anchorTransform;
    rbBodyDef.circleShape = &anchorCircle;
    rbBodyDef.mass = 10.0f;
    rbBodyDef.linearDamping = 0.0f;
    Dia::RigidBody2D::RigidBody2D* dynBody = rbWorld.AddRigidBody(rbBodyDef);

    WorldDef sbDef;
    sbDef.gravity = Vector2D(0.0f, -50.0f);
    sbDef.fixedTimestep = 1.0f / 60.0f;
    sbDef.maxSubSteps = 8;
    sbDef.solverIterations = 10;
    sbDef.rigidBodyWorld = &rbWorld;
    SoftBodyWorld sbWorld(sbDef);

    RopeDef rdef;
    rdef.id = Dia::Core::StringCRC("CoupledRope");
    rdef.startPoint = Vector2D(0.0f, 5.0f);
    rdef.endPoint = Vector2D(0.0f, 0.0f);
    rdef.particleCount = 8;
    rdef.mass = 2.0f;
    rdef.stiffness = 1.0f;
    rdef.particleRadius = 0.1f;
    rdef.maxStretch = 0.0f;
    rdef.startAnchor = dynBody;
    Rope* rope = sbWorld.AddRope(rdef);

    for (int i = 0; i < 30; ++i)
    {
        rbWorld.Update(1.0f / 60.0f);
        sbWorld.Update(1.0f / 60.0f);
    }

    Vector2D anchorPos = dynBody->GetTransform()->GetWorldPosition();
    EXPECT_NEAR(rope->GetParticle(0).position.x, anchorPos.x, 0.1f);
    EXPECT_NEAR(rope->GetParticle(0).position.y, anchorPos.y, 0.1f);
}
