#include <gtest/gtest.h>

#include <DiaSoftBody2D/SoftBodyWorld.h>
#include <DiaGeometry2D/Shapes/AARect.h>
#include <DiaGeometry2D/Shapes/Circle.h>
#include <DiaGeometry2D/Shapes/Line.h>
#include <DiaMaths/Vector/Vector2D.h>
#include <DiaCore/CRC/StringCRC.h>

#include <cmath>

using namespace Dia::SoftBody2D;
using namespace Dia::Maths;

// ---------------------------------------------------------------------------
// Max-size rope (200 particles)
// ---------------------------------------------------------------------------

TEST(SoftBody2D_Stress, MaxRope200Particles_ConstructsAndSimulates)
{
    WorldDef def;
    def.gravity = Vector2D(0.0f, -9.81f);
    def.fixedTimestep = 1.0f / 60.0f;
    def.maxSubSteps = 8;
    def.solverIterations = 10;
    def.rigidBodyWorld = nullptr;
    SoftBodyWorld world(def);

    RopeDef rdef;
    rdef.id = Dia::Core::StringCRC("MaxRope");
    rdef.startPoint = Vector2D(0.0f, 0.0f);
    rdef.endPoint = Vector2D(0.0f, -20.0f);
    rdef.particleCount = 200;
    rdef.mass = 5.0f;
    rdef.stiffness = 1.0f;
    rdef.particleRadius = 0.02f;
    rdef.maxStretch = 0.0f;
    Rope* rope = world.AddRope(rdef);
    rope->GetParticle(0).invMass = 0.0f;

    EXPECT_EQ(rope->GetParticleCount(), 200);
    EXPECT_EQ(rope->GetConstraintCount(), 199);

    for (int i = 0; i < 120; ++i)
        world.Update(1.0f / 60.0f);

    for (int p = 0; p < rope->GetParticleCount(); ++p)
    {
        EXPECT_FALSE(std::isnan(rope->GetParticle(p).position.x));
        EXPECT_FALSE(std::isnan(rope->GetParticle(p).position.y));
        EXPECT_FALSE(std::isinf(rope->GetParticle(p).position.x));
        EXPECT_FALSE(std::isinf(rope->GetParticle(p).position.y));
    }
}

// ---------------------------------------------------------------------------
// Large cloth (32x32 = 1024 particles)
// ---------------------------------------------------------------------------

TEST(SoftBody2D_Stress, LargeCloth32x32_ConstructsAndSimulates)
{
    WorldDef def;
    def.gravity = Vector2D(0.0f, -9.81f);
    def.fixedTimestep = 1.0f / 60.0f;
    def.maxSubSteps = 4;
    def.solverIterations = 4;
    def.rigidBodyWorld = nullptr;
    SoftBodyWorld world(def);

    ClothDef cdef;
    cdef.id = Dia::Core::StringCRC("LargeCloth");
    cdef.origin = Vector2D(0.0f, 0.0f);
    cdef.width = 10.0f;
    cdef.height = 10.0f;
    cdef.resX = 32;
    cdef.resY = 32;
    cdef.mass = 5.0f;
    cdef.structuralStiffness = 1.0f;
    cdef.shearStiffness = 0.5f;
    cdef.bendStiffness = 0.1f;
    cdef.particleRadius = 0.01f;
    cdef.maxStretch = 0.0f;
    cdef.pinTopRow = true;
    Cloth* cloth = world.AddCloth(cdef);

    EXPECT_EQ(cloth->GetParticleCount(), 1024);
    EXPECT_GT(cloth->GetConstraintCount(), 0);

    for (int i = 0; i < 60; ++i)
        world.Update(1.0f / 60.0f);

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
// Many bodies in one world
// ---------------------------------------------------------------------------

TEST(SoftBody2D_Stress, ManyBodies_15InOneWorld)
{
    WorldDef def;
    def.gravity = Vector2D(0.0f, -9.81f);
    def.fixedTimestep = 1.0f / 60.0f;
    def.maxSubSteps = 4;
    def.solverIterations = 4;
    def.rigidBodyWorld = nullptr;
    SoftBodyWorld world(def);

    for (int i = 0; i < 10; ++i)
    {
        RopeDef rdef;
        char name[32];
        snprintf(name, sizeof(name), "StressRope%d", i);
        rdef.id = Dia::Core::StringCRC(name);
        rdef.startPoint = Vector2D(static_cast<float>(i) * 2.0f, 0.0f);
        rdef.endPoint = Vector2D(static_cast<float>(i) * 2.0f, -3.0f);
        rdef.particleCount = 5;
        rdef.mass = 1.0f;
        rdef.stiffness = 1.0f;
        rdef.particleRadius = 0.05f;
        rdef.maxStretch = 0.0f;
        Rope* rope = world.AddRope(rdef);
        rope->GetParticle(0).invMass = 0.0f;
    }

    for (int i = 0; i < 5; ++i)
    {
        ClothDef cdef;
        char name[32];
        snprintf(name, sizeof(name), "StressCloth%d", i);
        cdef.id = Dia::Core::StringCRC(name);
        cdef.origin = Vector2D(static_cast<float>(i) * 3.0f, 5.0f);
        cdef.width = 1.0f;
        cdef.height = 1.0f;
        cdef.resX = 3;
        cdef.resY = 3;
        cdef.mass = 0.5f;
        cdef.structuralStiffness = 1.0f;
        cdef.shearStiffness = 0.5f;
        cdef.bendStiffness = 0.1f;
        cdef.particleRadius = 0.02f;
        cdef.maxStretch = 0.0f;
        cdef.pinTopRow = true;
        world.AddCloth(cdef);
    }

    EXPECT_EQ(world.GetBodyCount(), 15);

    for (int i = 0; i < 120; ++i)
        world.Update(1.0f / 60.0f);

    EXPECT_EQ(world.GetBodyCount(), 15);
}

// ---------------------------------------------------------------------------
// Extreme gravity
// ---------------------------------------------------------------------------

TEST(SoftBody2D_Stress, ExtremeGravity_NoNaNOrInf)
{
    WorldDef def;
    def.gravity = Vector2D(0.0f, -10000.0f);
    def.fixedTimestep = 1.0f / 60.0f;
    def.maxSubSteps = 1;
    def.solverIterations = 4;
    def.rigidBodyWorld = nullptr;
    SoftBodyWorld world(def);

    RopeDef rdef;
    rdef.id = Dia::Core::StringCRC("ExtremeRope");
    rdef.startPoint = Vector2D(0.0f, 0.0f);
    rdef.endPoint = Vector2D(0.0f, -2.0f);
    rdef.particleCount = 5;
    rdef.mass = 1.0f;
    rdef.stiffness = 1.0f;
    rdef.particleRadius = 0.05f;
    rdef.maxStretch = 0.0f;
    Rope* rope = world.AddRope(rdef);
    rope->GetParticle(0).invMass = 0.0f;

    for (int i = 0; i < 60; ++i)
        world.Update(1.0f / 60.0f);

    for (int p = 0; p < rope->GetParticleCount(); ++p)
    {
        EXPECT_FALSE(std::isnan(rope->GetParticle(p).position.x));
        EXPECT_FALSE(std::isnan(rope->GetParticle(p).position.y));
        EXPECT_FALSE(std::isinf(rope->GetParticle(p).position.x));
        EXPECT_FALSE(std::isinf(rope->GetParticle(p).position.y));
    }
}

// ---------------------------------------------------------------------------
// Very small timestep
// ---------------------------------------------------------------------------

TEST(SoftBody2D_Stress, VerySmallTimestep_StableSimulation)
{
    WorldDef def;
    def.gravity = Vector2D(0.0f, -9.81f);
    def.fixedTimestep = 1.0f / 240.0f;
    def.maxSubSteps = 16;
    def.solverIterations = 4;
    def.rigidBodyWorld = nullptr;
    SoftBodyWorld world(def);

    RopeDef rdef;
    rdef.id = Dia::Core::StringCRC("SmallDtRope");
    rdef.startPoint = Vector2D(0.0f, 0.0f);
    rdef.endPoint = Vector2D(0.0f, -2.0f);
    rdef.particleCount = 10;
    rdef.mass = 1.0f;
    rdef.stiffness = 1.0f;
    rdef.particleRadius = 0.05f;
    rdef.maxStretch = 0.0f;
    Rope* rope = world.AddRope(rdef);
    rope->GetParticle(0).invMass = 0.0f;

    for (int i = 0; i < 240; ++i)
        world.Update(1.0f / 240.0f);

    for (int p = 0; p < rope->GetParticleCount(); ++p)
    {
        EXPECT_FALSE(std::isnan(rope->GetParticle(p).position.x));
        EXPECT_FALSE(std::isnan(rope->GetParticle(p).position.y));
    }
}

// ---------------------------------------------------------------------------
// All particles pinned — no movement
// ---------------------------------------------------------------------------

TEST(SoftBody2D_Stress, AllPinned_NoMovement)
{
    WorldDef def;
    def.gravity = Vector2D(0.0f, -9.81f);
    def.fixedTimestep = 1.0f / 60.0f;
    def.maxSubSteps = 8;
    def.solverIterations = 10;
    def.rigidBodyWorld = nullptr;
    SoftBodyWorld world(def);

    ClothDef cdef;
    cdef.id = Dia::Core::StringCRC("PinnedCloth");
    cdef.origin = Vector2D(0.0f, 0.0f);
    cdef.width = 2.0f;
    cdef.height = 2.0f;
    cdef.resX = 4;
    cdef.resY = 4;
    cdef.mass = 1.0f;
    cdef.structuralStiffness = 1.0f;
    cdef.shearStiffness = 0.5f;
    cdef.bendStiffness = 0.1f;
    cdef.particleRadius = 0.02f;
    cdef.maxStretch = 0.0f;
    cdef.pinTopRow = false;
    Cloth* cloth = world.AddCloth(cdef);

    for (int y = 0; y < cloth->GetResY(); ++y)
        for (int x = 0; x < cloth->GetResX(); ++x)
            cloth->PinParticle(x, y);

    Vector2D cornerBefore = cloth->GetParticle(0, 0).position;
    Vector2D centerBefore = cloth->GetParticle(2, 2).position;

    for (int i = 0; i < 120; ++i)
        world.Update(1.0f / 60.0f);

    EXPECT_NEAR(cloth->GetParticle(0, 0).position.x, cornerBefore.x, 1e-5f);
    EXPECT_NEAR(cloth->GetParticle(0, 0).position.y, cornerBefore.y, 1e-5f);
    EXPECT_NEAR(cloth->GetParticle(2, 2).position.x, centerBefore.x, 1e-5f);
    EXPECT_NEAR(cloth->GetParticle(2, 2).position.y, centerBefore.y, 1e-5f);
}

// ---------------------------------------------------------------------------
// Minimal 2x2 cloth
// ---------------------------------------------------------------------------

TEST(SoftBody2D_Stress, MinimalCloth2x2_ValidConstruction)
{
    WorldDef def;
    def.gravity = Vector2D(0.0f, -9.81f);
    def.fixedTimestep = 1.0f / 60.0f;
    def.maxSubSteps = 8;
    def.solverIterations = 10;
    def.rigidBodyWorld = nullptr;
    SoftBodyWorld world(def);

    ClothDef cdef;
    cdef.id = Dia::Core::StringCRC("TinyCloth");
    cdef.origin = Vector2D(0.0f, 0.0f);
    cdef.width = 1.0f;
    cdef.height = 1.0f;
    cdef.resX = 2;
    cdef.resY = 2;
    cdef.mass = 1.0f;
    cdef.structuralStiffness = 1.0f;
    cdef.shearStiffness = 0.5f;
    cdef.bendStiffness = 0.1f;
    cdef.particleRadius = 0.02f;
    cdef.maxStretch = 0.0f;
    cdef.pinTopRow = true;
    Cloth* cloth = world.AddCloth(cdef);

    EXPECT_EQ(cloth->GetParticleCount(), 4);
    EXPECT_GT(cloth->GetConstraintCount(), 0);

    for (int i = 0; i < 60; ++i)
        world.Update(1.0f / 60.0f);

    EXPECT_LT(cloth->GetParticle(0, 1).position.y, cloth->GetParticle(0, 0).position.y);
}

// ---------------------------------------------------------------------------
// Zero stiffness — maximally floppy
// ---------------------------------------------------------------------------

TEST(SoftBody2D_Stress, ZeroStiffness_NoNaNOrCrash)
{
    WorldDef def;
    def.gravity = Vector2D(0.0f, -9.81f);
    def.fixedTimestep = 1.0f / 60.0f;
    def.maxSubSteps = 8;
    def.solverIterations = 10;
    def.rigidBodyWorld = nullptr;
    SoftBodyWorld world(def);

    RopeDef rdef;
    rdef.id = Dia::Core::StringCRC("FloppyRope");
    rdef.startPoint = Vector2D(0.0f, 0.0f);
    rdef.endPoint = Vector2D(0.0f, -3.0f);
    rdef.particleCount = 10;
    rdef.mass = 1.0f;
    rdef.stiffness = 0.0f;
    rdef.particleRadius = 0.05f;
    rdef.maxStretch = 0.0f;
    Rope* rope = world.AddRope(rdef);
    rope->GetParticle(0).invMass = 0.0f;

    for (int i = 0; i < 120; ++i)
        world.Update(1.0f / 60.0f);

    for (int p = 0; p < rope->GetParticleCount(); ++p)
    {
        EXPECT_FALSE(std::isnan(rope->GetParticle(p).position.x));
        EXPECT_FALSE(std::isnan(rope->GetParticle(p).position.y));
    }
}

// ---------------------------------------------------------------------------
// Many static shapes registered
// ---------------------------------------------------------------------------

TEST(SoftBody2D_Stress, ManyStaticShapes_NoCrash)
{
    static const int kShapeCount = 7;
    Dia::Geometry2D::AARect rects[kShapeCount];
    Dia::Geometry2D::Circle circles[kShapeCount];

    for (int i = 0; i < kShapeCount; ++i)
    {
        float y = -2.0f - static_cast<float>(i) * 3.0f;
        rects[i] = Dia::Geometry2D::AARect(
            Vector2D(-10.0f, y - 0.1f), Vector2D(10.0f, y));
        circles[i] = Dia::Geometry2D::Circle(
            0.5f, Vector2D(static_cast<float>(i) * 2.0f, y + 1.0f));
    }

    WorldDef def;
    def.gravity = Vector2D(0.0f, -9.81f);
    def.fixedTimestep = 1.0f / 60.0f;
    def.maxSubSteps = 4;
    def.solverIterations = 4;
    def.rigidBodyWorld = nullptr;
    SoftBodyWorld world(def);

    for (int i = 0; i < kShapeCount; ++i)
    {
        world.AddStaticShape(&rects[i]);
        world.AddStaticShape(&circles[i]);
    }

    RopeDef rdef;
    rdef.id = Dia::Core::StringCRC("ShapeStressRope");
    rdef.startPoint = Vector2D(0.0f, 0.0f);
    rdef.endPoint = Vector2D(0.0f, -3.0f);
    rdef.particleCount = 10;
    rdef.mass = 1.0f;
    rdef.stiffness = 1.0f;
    rdef.particleRadius = 0.05f;
    rdef.maxStretch = 0.0f;
    Rope* rope = world.AddRope(rdef);
    rope->GetParticle(0).invMass = 0.0f;

    for (int i = 0; i < 120; ++i)
        world.Update(1.0f / 60.0f);

    for (int p = 0; p < rope->GetParticleCount(); ++p)
    {
        EXPECT_FALSE(std::isnan(rope->GetParticle(p).position.x));
        EXPECT_FALSE(std::isnan(rope->GetParticle(p).position.y));
    }

    for (int i = 0; i < kShapeCount; ++i)
    {
        world.RemoveStaticShape(&rects[i]);
        world.RemoveStaticShape(&circles[i]);
    }
}

// ---------------------------------------------------------------------------
// Rapid add/remove cycling
// ---------------------------------------------------------------------------

TEST(SoftBody2D_Stress, RapidAddRemoveCycle_NoLeakOrCrash)
{
    WorldDef def;
    def.gravity = Vector2D(0.0f, -9.81f);
    def.fixedTimestep = 1.0f / 60.0f;
    def.maxSubSteps = 4;
    def.solverIterations = 4;
    def.rigidBodyWorld = nullptr;
    SoftBodyWorld world(def);

    for (int cycle = 0; cycle < 10; ++cycle)
    {
        RopeDef rdef;
        char name[32];
        snprintf(name, sizeof(name), "CycleRope%d", cycle);
        rdef.id = Dia::Core::StringCRC(name);
        rdef.startPoint = Vector2D(0.0f, 0.0f);
        rdef.endPoint = Vector2D(2.0f, 0.0f);
        rdef.particleCount = 5;
        rdef.mass = 1.0f;
        rdef.stiffness = 1.0f;
        rdef.particleRadius = 0.05f;
        rdef.maxStretch = 0.0f;
        Rope* rope = world.AddRope(rdef);

        world.Update(1.0f / 60.0f);

        world.RemoveBody(rope);
    }

    EXPECT_EQ(world.GetBodyCount(), 0);
}
