#include <gtest/gtest.h>

#include <DiaSoftBody2D/SoftBodyWorld.h>
#include <DiaMaths/Vector/Vector2D.h>
#include <DiaCore/CRC/StringCRC.h>

#include <cmath>

using namespace Dia::SoftBody2D;
using namespace Dia::Maths;

// ---------------------------------------------------------------------------
// Determinism — identical setup must produce identical results
// ---------------------------------------------------------------------------

TEST(SoftBody2D_Regression, RopeDeterminism_TwoRunsIdentical)
{
    auto runSimulation = [](Vector2D* outPositions, int count)
    {
        WorldDef def;
        def.gravity = Vector2D(0.0f, -9.81f);
        def.fixedTimestep = 1.0f / 60.0f;
        def.maxSubSteps = 8;
        def.solverIterations = 10;
        def.rigidBodyWorld = nullptr;
        SoftBodyWorld world(def);

        RopeDef rdef;
        rdef.id = Dia::Core::StringCRC("DetRope");
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

        for (int p = 0; p < count; ++p)
            outPositions[p] = rope->GetParticle(p).position;
    };

    Vector2D positionsA[10];
    Vector2D positionsB[10];
    runSimulation(positionsA, 10);
    runSimulation(positionsB, 10);

    for (int i = 0; i < 10; ++i)
    {
        EXPECT_EQ(positionsA[i].x, positionsB[i].x) << "Particle " << i << " x differs";
        EXPECT_EQ(positionsA[i].y, positionsB[i].y) << "Particle " << i << " y differs";
    }
}

TEST(SoftBody2D_Regression, ClothDeterminism_TwoRunsIdentical)
{
    auto runSimulation = [](Vector2D* outPositions)
    {
        WorldDef def;
        def.gravity = Vector2D(0.0f, -9.81f);
        def.fixedTimestep = 1.0f / 60.0f;
        def.maxSubSteps = 4;
        def.solverIterations = 4;
        def.rigidBodyWorld = nullptr;
        SoftBodyWorld world(def);

        ClothDef cdef;
        cdef.id = Dia::Core::StringCRC("DetCloth");
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
        cdef.pinTopRow = true;
        Cloth* cloth = world.AddCloth(cdef);

        for (int i = 0; i < 60; ++i)
            world.Update(1.0f / 60.0f);

        int idx = 0;
        for (int y = 0; y < 4; ++y)
            for (int x = 0; x < 4; ++x)
                outPositions[idx++] = cloth->GetParticle(x, y).position;
    };

    Vector2D positionsA[16];
    Vector2D positionsB[16];
    runSimulation(positionsA);
    runSimulation(positionsB);

    for (int i = 0; i < 16; ++i)
    {
        EXPECT_EQ(positionsA[i].x, positionsB[i].x) << "Particle " << i << " x differs";
        EXPECT_EQ(positionsA[i].y, positionsB[i].y) << "Particle " << i << " y differs";
    }
}

// ---------------------------------------------------------------------------
// Symmetry — symmetric setup must produce symmetric results
// ---------------------------------------------------------------------------

TEST(SoftBody2D_Regression, SymmetricCloth_MirroredPositions)
{
    WorldDef def;
    def.gravity = Vector2D(0.0f, -9.81f);
    def.fixedTimestep = 1.0f / 60.0f;
    def.maxSubSteps = 8;
    def.solverIterations = 10;
    def.rigidBodyWorld = nullptr;
    SoftBodyWorld world(def);

    ClothDef cdef;
    cdef.id = Dia::Core::StringCRC("SymCloth");
    cdef.origin = Vector2D(0.0f, 0.0f);
    cdef.width = 4.0f;
    cdef.height = 2.0f;
    cdef.resX = 5;
    cdef.resY = 3;
    cdef.mass = 1.0f;
    cdef.structuralStiffness = 1.0f;
    cdef.shearStiffness = 0.5f;
    cdef.bendStiffness = 0.1f;
    cdef.particleRadius = 0.02f;
    cdef.maxStretch = 0.0f;
    cdef.pinTopRow = true;
    Cloth* cloth = world.AddCloth(cdef);

    for (int i = 0; i < 120; ++i)
        world.Update(1.0f / 60.0f);

    for (int y = 0; y < 3; ++y)
    {
        float centerX = cloth->GetParticle(2, y).position.x;
        float leftY = cloth->GetParticle(0, y).position.y;
        float rightY = cloth->GetParticle(4, y).position.y;
        float leftDx = std::abs(cloth->GetParticle(0, y).position.x - centerX);
        float rightDx = std::abs(cloth->GetParticle(4, y).position.x - centerX);

        EXPECT_NEAR(leftY, rightY, 1e-4f) << "Row " << y << " not y-symmetric";
        EXPECT_NEAR(leftDx, rightDx, 1e-4f) << "Row " << y << " not x-symmetric";
    }
}

TEST(SoftBody2D_Regression, SymmetricRope_HorizontalEndsAtSameY)
{
    WorldDef def;
    def.gravity = Vector2D(0.0f, -9.81f);
    def.fixedTimestep = 1.0f / 60.0f;
    def.maxSubSteps = 8;
    def.solverIterations = 10;
    def.rigidBodyWorld = nullptr;
    SoftBodyWorld world(def);

    RopeDef rdef;
    rdef.id = Dia::Core::StringCRC("SymRope");
    rdef.startPoint = Vector2D(-2.0f, 0.0f);
    rdef.endPoint = Vector2D(2.0f, 0.0f);
    rdef.particleCount = 11;
    rdef.mass = 1.0f;
    rdef.stiffness = 1.0f;
    rdef.particleRadius = 0.05f;
    rdef.maxStretch = 0.0f;
    Rope* rope = world.AddRope(rdef);
    rope->GetParticle(0).invMass = 0.0f;
    rope->GetParticle(10).invMass = 0.0f;

    for (int i = 0; i < 120; ++i)
        world.Update(1.0f / 60.0f);

    float midX = (rope->GetParticle(0).position.x + rope->GetParticle(10).position.x) / 2.0f;
    float midParticleX = rope->GetParticle(5).position.x;
    EXPECT_NEAR(midParticleX, midX, 0.01f);

    for (int p = 0; p < 5; ++p)
    {
        float leftY = rope->GetParticle(p).position.y;
        float rightY = rope->GetParticle(10 - p).position.y;
        EXPECT_NEAR(leftY, rightY, 0.005f) << "Particle pair " << p << "/" << (10-p) << " not symmetric";
    }
}

// ---------------------------------------------------------------------------
// Constraint satisfaction — rest lengths approximately maintained
// ---------------------------------------------------------------------------

TEST(SoftBody2D_Regression, RopeRestLengths_ApproximatelyMaintained)
{
    WorldDef def;
    def.gravity = Vector2D(0.0f, -9.81f);
    def.fixedTimestep = 1.0f / 60.0f;
    def.maxSubSteps = 8;
    def.solverIterations = 10;
    def.rigidBodyWorld = nullptr;
    SoftBodyWorld world(def);

    RopeDef rdef;
    rdef.id = Dia::Core::StringCRC("RestRope");
    rdef.startPoint = Vector2D(0.0f, 0.0f);
    rdef.endPoint = Vector2D(0.0f, -3.0f);
    rdef.particleCount = 10;
    rdef.mass = 1.0f;
    rdef.stiffness = 1.0f;
    rdef.particleRadius = 0.05f;
    rdef.maxStretch = 0.0f;
    Rope* rope = world.AddRope(rdef);
    rope->GetParticle(0).invMass = 0.0f;

    float expectedRestLength = 3.0f / 9.0f;

    for (int i = 0; i < 300; ++i)
        world.Update(1.0f / 60.0f);

    for (int p = 0; p < 9; ++p)
    {
        Vector2D a = rope->GetParticle(p).position;
        Vector2D b = rope->GetParticle(p + 1).position;
        float dx = b.x - a.x;
        float dy = b.y - a.y;
        float dist = std::sqrt(dx * dx + dy * dy);
        float stretchRatio = dist / expectedRestLength;
        EXPECT_NEAR(stretchRatio, 1.0f, 0.05f)
            << "Segment " << p << "-" << (p+1) << " stretch=" << stretchRatio;
    }
}

// ---------------------------------------------------------------------------
// Energy — system reaches steady state (velocity diminishes)
// ---------------------------------------------------------------------------

TEST(SoftBody2D_Regression, RopeSettles_VelocityDecreases)
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
    rdef.endPoint = Vector2D(3.0f, 0.0f);
    rdef.particleCount = 10;
    rdef.mass = 1.0f;
    rdef.stiffness = 1.0f;
    rdef.particleRadius = 0.05f;
    rdef.maxStretch = 0.0f;
    Rope* rope = world.AddRope(rdef);
    rope->GetParticle(0).invMass = 0.0f;

    for (int i = 0; i < 60; ++i)
        world.Update(1.0f / 60.0f);

    float earlyKE = 0.0f;
    for (int p = 0; p < rope->GetParticleCount(); ++p)
    {
        if (rope->GetParticle(p).invMass <= 0.0f) continue;
        Vector2D vel = DeriveVelocity(rope->GetParticle(p), def.fixedTimestep);
        earlyKE += (vel.x * vel.x + vel.y * vel.y);
    }

    for (int i = 0; i < 540; ++i)
        world.Update(1.0f / 60.0f);

    float lateKE = 0.0f;
    for (int p = 0; p < rope->GetParticleCount(); ++p)
    {
        if (rope->GetParticle(p).invMass <= 0.0f) continue;
        Vector2D vel = DeriveVelocity(rope->GetParticle(p), def.fixedTimestep);
        lateKE += (vel.x * vel.x + vel.y * vel.y);
    }

    EXPECT_LT(lateKE, earlyKE) << "Late KE should be less than early KE after settling";
}

// ---------------------------------------------------------------------------
// Gravity direction — particles fall in correct direction
// ---------------------------------------------------------------------------

TEST(SoftBody2D_Regression, GravityDirection_ParticlesFallDown)
{
    WorldDef def;
    def.gravity = Vector2D(0.0f, -9.81f);
    def.fixedTimestep = 1.0f / 60.0f;
    def.maxSubSteps = 8;
    def.solverIterations = 10;
    def.rigidBodyWorld = nullptr;
    SoftBodyWorld world(def);

    RopeDef rdef;
    rdef.id = Dia::Core::StringCRC("GravRope");
    rdef.startPoint = Vector2D(0.0f, 0.0f);
    rdef.endPoint = Vector2D(2.0f, 0.0f);
    rdef.particleCount = 5;
    rdef.mass = 1.0f;
    rdef.stiffness = 1.0f;
    rdef.particleRadius = 0.05f;
    rdef.maxStretch = 0.0f;
    Rope* rope = world.AddRope(rdef);

    float initialY = rope->GetParticle(2).position.y;

    for (int i = 0; i < 30; ++i)
        world.Update(1.0f / 60.0f);

    EXPECT_LT(rope->GetParticle(2).position.y, initialY);
}

TEST(SoftBody2D_Regression, SidewaysGravity_ParticlesMoveRight)
{
    WorldDef def;
    def.gravity = Vector2D(9.81f, 0.0f);
    def.fixedTimestep = 1.0f / 60.0f;
    def.maxSubSteps = 8;
    def.solverIterations = 10;
    def.rigidBodyWorld = nullptr;
    SoftBodyWorld world(def);

    RopeDef rdef;
    rdef.id = Dia::Core::StringCRC("SideRope");
    rdef.startPoint = Vector2D(0.0f, 0.0f);
    rdef.endPoint = Vector2D(0.0f, -2.0f);
    rdef.particleCount = 5;
    rdef.mass = 1.0f;
    rdef.stiffness = 1.0f;
    rdef.particleRadius = 0.05f;
    rdef.maxStretch = 0.0f;
    Rope* rope = world.AddRope(rdef);

    float initialX = rope->GetParticle(2).position.x;

    for (int i = 0; i < 30; ++i)
        world.Update(1.0f / 60.0f);

    EXPECT_GT(rope->GetParticle(2).position.x, initialX);
}

// ---------------------------------------------------------------------------
// Total rope length — should not grow unboundedly
// ---------------------------------------------------------------------------

TEST(SoftBody2D_Regression, RopeTotalLength_BoundedOverTime)
{
    WorldDef def;
    def.gravity = Vector2D(0.0f, -9.81f);
    def.fixedTimestep = 1.0f / 60.0f;
    def.maxSubSteps = 8;
    def.solverIterations = 10;
    def.rigidBodyWorld = nullptr;
    SoftBodyWorld world(def);

    RopeDef rdef;
    rdef.id = Dia::Core::StringCRC("LenRope");
    rdef.startPoint = Vector2D(0.0f, 0.0f);
    rdef.endPoint = Vector2D(0.0f, -5.0f);
    rdef.particleCount = 20;
    rdef.mass = 2.0f;
    rdef.stiffness = 1.0f;
    rdef.particleRadius = 0.05f;
    rdef.maxStretch = 0.0f;
    Rope* rope = world.AddRope(rdef);
    rope->GetParticle(0).invMass = 0.0f;

    float initialRestLength = 5.0f;

    for (int i = 0; i < 600; ++i)
        world.Update(1.0f / 60.0f);

    float totalLength = 0.0f;
    for (int p = 0; p < 19; ++p)
    {
        Vector2D a = rope->GetParticle(p).position;
        Vector2D b = rope->GetParticle(p + 1).position;
        float dx = b.x - a.x;
        float dy = b.y - a.y;
        totalLength += std::sqrt(dx * dx + dy * dy);
    }

    EXPECT_LT(totalLength, initialRestLength * 1.15f)
        << "Total rope length " << totalLength << " exceeds 115% of rest length";
}

// ---------------------------------------------------------------------------
// Cloth pinned row — pinned particles stay at original positions
// ---------------------------------------------------------------------------

TEST(SoftBody2D_Regression, ClothPinnedRow_MaintainsPosition)
{
    WorldDef def;
    def.gravity = Vector2D(0.0f, -9.81f);
    def.fixedTimestep = 1.0f / 60.0f;
    def.maxSubSteps = 8;
    def.solverIterations = 10;
    def.rigidBodyWorld = nullptr;
    SoftBodyWorld world(def);

    ClothDef cdef;
    cdef.id = Dia::Core::StringCRC("PinRowCloth");
    cdef.origin = Vector2D(0.0f, 0.0f);
    cdef.width = 3.0f;
    cdef.height = 3.0f;
    cdef.resX = 4;
    cdef.resY = 4;
    cdef.mass = 1.0f;
    cdef.structuralStiffness = 1.0f;
    cdef.shearStiffness = 0.5f;
    cdef.bendStiffness = 0.1f;
    cdef.particleRadius = 0.02f;
    cdef.maxStretch = 0.0f;
    cdef.pinTopRow = true;
    Cloth* cloth = world.AddCloth(cdef);

    Vector2D pinnedPositions[4];
    for (int x = 0; x < 4; ++x)
        pinnedPositions[x] = cloth->GetParticle(x, 0).position;

    for (int i = 0; i < 300; ++i)
        world.Update(1.0f / 60.0f);

    for (int x = 0; x < 4; ++x)
    {
        EXPECT_NEAR(cloth->GetParticle(x, 0).position.x, pinnedPositions[x].x, 1e-5f);
        EXPECT_NEAR(cloth->GetParticle(x, 0).position.y, pinnedPositions[x].y, 1e-5f);
    }
}

// ---------------------------------------------------------------------------
// Zero gravity — free-floating rope should maintain initial length
// ---------------------------------------------------------------------------

TEST(SoftBody2D_Regression, ZeroGravity_RopeMaintainsShape)
{
    WorldDef def;
    def.gravity = Vector2D(0.0f, 0.0f);
    def.fixedTimestep = 1.0f / 60.0f;
    def.maxSubSteps = 8;
    def.solverIterations = 10;
    def.rigidBodyWorld = nullptr;
    SoftBodyWorld world(def);

    RopeDef rdef;
    rdef.id = Dia::Core::StringCRC("ZeroGRope");
    rdef.startPoint = Vector2D(0.0f, 0.0f);
    rdef.endPoint = Vector2D(3.0f, 0.0f);
    rdef.particleCount = 10;
    rdef.mass = 1.0f;
    rdef.stiffness = 1.0f;
    rdef.particleRadius = 0.05f;
    rdef.maxStretch = 0.0f;
    Rope* rope = world.AddRope(rdef);

    Vector2D initialPositions[10];
    for (int p = 0; p < 10; ++p)
        initialPositions[p] = rope->GetParticle(p).position;

    for (int i = 0; i < 120; ++i)
        world.Update(1.0f / 60.0f);

    for (int p = 0; p < 10; ++p)
    {
        EXPECT_NEAR(rope->GetParticle(p).position.x, initialPositions[p].x, 1e-3f);
        EXPECT_NEAR(rope->GetParticle(p).position.y, initialPositions[p].y, 1e-3f);
    }
}
