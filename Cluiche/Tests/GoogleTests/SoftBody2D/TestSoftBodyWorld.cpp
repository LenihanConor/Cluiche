#include <gtest/gtest.h>

#include <DiaSoftBody2D/SoftBodyWorld.h>
#include <DiaMaths/Vector/Vector2D.h>
#include <DiaCore/CRC/StringCRC.h>

#include <cmath>

using namespace Dia::SoftBody2D;
using namespace Dia::Maths;

static WorldDef MakeDefaultWorldDef()
{
    WorldDef def;
    def.gravity = Vector2D(0.0f, -9.81f);
    def.fixedTimestep = 1.0f / 60.0f;
    def.maxSubSteps = 8;
    def.solverIterations = 10;
    def.rigidBodyWorld = nullptr;
    return def;
}

static RopeDef MakeSimpleRopeDef(int particleCount = 5)
{
    RopeDef def;
    def.id = Dia::Core::StringCRC("WorldRope");
    def.startPoint = Vector2D(0.0f, 0.0f);
    def.endPoint = Vector2D(4.0f, 0.0f);
    def.particleCount = particleCount;
    def.mass = 1.0f;
    def.stiffness = 1.0f;
    def.particleRadius = 0.1f;
    def.maxStretch = 0.0f;
    return def;
}

static ClothDef MakeSimpleClothDef(int res = 3)
{
    ClothDef def;
    def.id = Dia::Core::StringCRC("WorldCloth");
    def.origin = Vector2D(0.0f, 0.0f);
    def.width = 2.0f;
    def.height = 2.0f;
    def.resX = res;
    def.resY = res;
    def.mass = 1.0f;
    def.structuralStiffness = 1.0f;
    def.shearStiffness = 0.5f;
    def.bendStiffness = 0.1f;
    def.particleRadius = 0.05f;
    def.maxStretch = 0.0f;
    def.pinTopRow = false;
    return def;
}

TEST(SoftBody2D_World, Construction_ZeroBodies)
{
    WorldDef def = MakeDefaultWorldDef();
    SoftBodyWorld world(def);
    EXPECT_EQ(world.GetBodyCount(), 0);
}

TEST(SoftBody2D_World, AddRope_BodyCountIncrements)
{
    WorldDef def = MakeDefaultWorldDef();
    SoftBodyWorld world(def);

    Rope* rope = world.AddRope(MakeSimpleRopeDef());
    EXPECT_NE(rope, nullptr);
    EXPECT_EQ(world.GetBodyCount(), 1);
}

TEST(SoftBody2D_World, AddCloth_BodyCountIncrements)
{
    WorldDef def = MakeDefaultWorldDef();
    SoftBodyWorld world(def);

    Cloth* cloth = world.AddCloth(MakeSimpleClothDef());
    EXPECT_NE(cloth, nullptr);
    EXPECT_EQ(world.GetBodyCount(), 1);
}

TEST(SoftBody2D_World, RemoveBody_BodyCountDecrements)
{
    WorldDef def = MakeDefaultWorldDef();
    SoftBodyWorld world(def);

    Rope* rope = world.AddRope(MakeSimpleRopeDef());
    EXPECT_EQ(world.GetBodyCount(), 1);
    world.RemoveBody(rope);
    EXPECT_EQ(world.GetBodyCount(), 0);
}

TEST(SoftBody2D_World, SetGravity_UpdatesGravity)
{
    WorldDef def = MakeDefaultWorldDef();
    SoftBodyWorld world(def);

    world.SetGravity(Vector2D(0.0f, -20.0f));
    EXPECT_FLOAT_EQ(world.GetGravity().y, -20.0f);
}

TEST(SoftBody2D_World, Update_RopeParticlesFallUnderGravity)
{
    WorldDef def = MakeDefaultWorldDef();
    SoftBodyWorld world(def);

    Rope* rope = world.AddRope(MakeSimpleRopeDef());
    float initialY = rope->GetParticle(2).position.y;

    world.Update(1.0f / 60.0f);

    EXPECT_LT(rope->GetParticle(2).position.y, initialY);
}

TEST(SoftBody2D_World, Update_ClothParticlesFallUnderGravity)
{
    WorldDef def = MakeDefaultWorldDef();
    SoftBodyWorld world(def);

    Cloth* cloth = world.AddCloth(MakeSimpleClothDef());
    float initialY = cloth->GetParticle(1, 1).position.y;

    world.Update(1.0f / 60.0f);

    EXPECT_LT(cloth->GetParticle(1, 1).position.y, initialY);
}

TEST(SoftBody2D_World, Update_ZeroGravity_ParticlesStayStill)
{
    WorldDef def = MakeDefaultWorldDef();
    def.gravity = Vector2D(0.0f, 0.0f);
    SoftBodyWorld world(def);

    Rope* rope = world.AddRope(MakeSimpleRopeDef());
    Vector2D initialPos = rope->GetParticle(2).position;

    world.Update(1.0f / 60.0f);

    EXPECT_NEAR(rope->GetParticle(2).position.x, initialPos.x, 1e-4f);
    EXPECT_NEAR(rope->GetParticle(2).position.y, initialPos.y, 1e-4f);
}

TEST(SoftBody2D_World, Update_AccumulatorHandlesLargeDelta)
{
    WorldDef def = MakeDefaultWorldDef();
    def.maxSubSteps = 2;
    SoftBodyWorld world(def);

    world.AddRope(MakeSimpleRopeDef());
    world.Update(10.0f);
    EXPECT_EQ(world.GetBodyCount(), 1);
}

TEST(SoftBody2D_World, Update_EmptyWorld_NoCrash)
{
    WorldDef def = MakeDefaultWorldDef();
    SoftBodyWorld world(def);
    world.Update(1.0f / 60.0f);
}

TEST(SoftBody2D_World, GetBodies_ReturnsAllAdded)
{
    WorldDef def = MakeDefaultWorldDef();
    SoftBodyWorld world(def);

    world.AddRope(MakeSimpleRopeDef());
    world.AddCloth(MakeSimpleClothDef());

    EXPECT_EQ(world.GetBodies().Size(), 2u);
}

TEST(SoftBody2D_World, NullRigidBodyWorld_NoCrash)
{
    WorldDef def = MakeDefaultWorldDef();
    def.rigidBodyWorld = nullptr;
    SoftBodyWorld world(def);

    world.AddRope(MakeSimpleRopeDef());
    for (int i = 0; i < 60; ++i)
        world.Update(1.0f / 60.0f);

    EXPECT_EQ(world.GetBodyCount(), 1);
}

TEST(SoftBody2D_World, ConstraintProjection_PullsStretchedParticlesBack)
{
    WorldDef def = MakeDefaultWorldDef();
    def.gravity = Vector2D(0.0f, 0.0f);
    def.solverIterations = 20;
    def.maxSubSteps = 1;
    SoftBodyWorld world(def);

    RopeDef rdef = MakeSimpleRopeDef(3);
    rdef.stiffness = 1.0f;
    Rope* rope = world.AddRope(rdef);

    rope->GetParticle(0).invMass = 0.0f;
    rope->GetParticle(2).position = Vector2D(10.0f, 0.0f);

    float stretchedDist = (rope->GetParticle(2).position - rope->GetParticle(1).position).Magnitude();

    world.Update(def.fixedTimestep);

    float afterDist = (rope->GetParticle(2).position - rope->GetParticle(1).position).Magnitude();
    EXPECT_LT(afterDist, stretchedDist);
}

TEST(SoftBody2D_World, MoreIterations_BetterConvergence)
{
    auto runWithIterations = [](int iterations) -> float
    {
        WorldDef def = MakeDefaultWorldDef();
        def.gravity = Vector2D(0.0f, 0.0f);
        def.solverIterations = iterations;
        def.maxSubSteps = 1;
        SoftBodyWorld world(def);

        RopeDef rdef;
        rdef.id = Dia::Core::StringCRC("ConvergeRope");
        rdef.startPoint = Vector2D(0.0f, 0.0f);
        rdef.endPoint = Vector2D(2.0f, 0.0f);
        rdef.particleCount = 5;
        rdef.mass = 1.0f;
        rdef.stiffness = 1.0f;
        rdef.particleRadius = 0.05f;
        rdef.maxStretch = 0.0f;
        Rope* rope = world.AddRope(rdef);
        rope->GetParticle(0).invMass = 0.0f;
        rope->GetParticle(4).position = Vector2D(10.0f, 0.0f);

        world.Update(def.fixedTimestep);

        float totalError = 0.0f;
        for (int c = 0; c < rope->GetConstraintCount(); ++c)
        {
            const DistanceConstraint& con = rope->GetConstraint(c);
            float dist = (rope->GetParticle(con.indexB).position - rope->GetParticle(con.indexA).position).Magnitude();
            totalError += std::abs(dist - con.restLength);
        }
        return totalError;
    };

    float error1 = runWithIterations(1);
    float error20 = runWithIterations(20);
    EXPECT_LT(error20, error1);
}

TEST(SoftBody2D_World, AccumulatorZeroSteps_DeltaLessThanTimestep)
{
    WorldDef def = MakeDefaultWorldDef();
    SoftBodyWorld world(def);

    Rope* rope = world.AddRope(MakeSimpleRopeDef());
    Vector2D posBeforeStep = rope->GetParticle(2).position;

    world.Update(def.fixedTimestep * 0.5f);

    EXPECT_FLOAT_EQ(rope->GetParticle(2).position.x, posBeforeStep.x);
    EXPECT_FLOAT_EQ(rope->GetParticle(2).position.y, posBeforeStep.y);
}

TEST(SoftBody2D_World, ClothConstraintProjection_PullsStretchedParticlesBack)
{
    WorldDef def = MakeDefaultWorldDef();
    def.gravity = Vector2D(0.0f, 0.0f);
    def.solverIterations = 20;
    def.maxSubSteps = 1;
    SoftBodyWorld world(def);

    ClothDef cdef = MakeSimpleClothDef(3);
    cdef.structuralStiffness = 1.0f;
    Cloth* cloth = world.AddCloth(cdef);

    cloth->PinParticle(0, 0);
    cloth->GetParticle(2, 2).position = Vector2D(20.0f, -20.0f);

    float stretchedDist = (cloth->GetParticle(2, 2).position - cloth->GetParticle(1, 1).position).Magnitude();

    world.Update(def.fixedTimestep);

    float afterDist = (cloth->GetParticle(2, 2).position - cloth->GetParticle(1, 1).position).Magnitude();
    EXPECT_LT(afterDist, stretchedDist);
}

TEST(SoftBody2D_World, ClothTearing_ThroughWorldUpdate)
{
    WorldDef def = MakeDefaultWorldDef();
    def.gravity = Vector2D(0.0f, -100.0f);
    def.solverIterations = 4;
    SoftBodyWorld world(def);

    ClothDef cdef = MakeSimpleClothDef(3);
    cdef.maxStretch = 0.001f;
    cdef.mass = 0.1f;
    cdef.structuralStiffness = 0.01f;
    cdef.shearStiffness = 0.01f;
    cdef.bendStiffness = 0.01f;
    cdef.pinTopRow = true;
    Cloth* cloth = world.AddCloth(cdef);

    for (int i = 0; i < 300; ++i)
        world.Update(1.0f / 60.0f);

    EXPECT_TRUE(cloth->IsTorn());
}

TEST(SoftBody2D_World, PinnedParticles_DoNotMoveUnderGravity)
{
    WorldDef def = MakeDefaultWorldDef();
    SoftBodyWorld world(def);

    RopeDef rdef = MakeSimpleRopeDef(3);
    Rope* rope = world.AddRope(rdef);
    rope->GetParticle(0).invMass = 0.0f;
    Vector2D pinnedPos = rope->GetParticle(0).position;

    for (int i = 0; i < 60; ++i)
        world.Update(1.0f / 60.0f);

    EXPECT_NEAR(rope->GetParticle(0).position.x, pinnedPos.x, 1e-5f);
    EXPECT_NEAR(rope->GetParticle(0).position.y, pinnedPos.y, 1e-5f);
}

TEST(SoftBody2D_World, MixedRopeAndCloth_BothSimulate)
{
    WorldDef def = MakeDefaultWorldDef();
    SoftBodyWorld world(def);

    Rope* rope = world.AddRope(MakeSimpleRopeDef());
    ClothDef cdef = MakeSimpleClothDef();
    cdef.origin = Vector2D(5.0f, 0.0f);
    Cloth* cloth = world.AddCloth(cdef);

    float ropeY = rope->GetParticle(2).position.y;
    float clothY = cloth->GetParticle(1, 1).position.y;

    world.Update(1.0f / 60.0f);

    EXPECT_LT(rope->GetParticle(2).position.y, ropeY);
    EXPECT_LT(cloth->GetParticle(1, 1).position.y, clothY);
}
