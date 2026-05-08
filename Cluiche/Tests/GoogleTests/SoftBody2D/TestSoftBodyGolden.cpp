#include <gtest/gtest.h>

#include <DiaSoftBody2D/SoftBodyWorld.h>
#include <DiaSoftBody2D/Rope.h>
#include <DiaSoftBody2D/Cloth.h>
#include <DiaMaths/Vector/Vector2D.h>
#include <DiaCore/CRC/StringCRC.h>

#include <cmath>

using namespace Dia::SoftBody2D;
using namespace Dia::Maths;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static WorldDef MakeStandardWorldDef()
{
    WorldDef def;
    def.gravity          = Vector2D(0.0f, -9.81f);
    def.fixedTimestep    = 1.0f / 60.0f;
    def.maxSubSteps      = 8;
    def.solverIterations = 10;
    def.rigidBodyWorld   = nullptr;
    return def;
}

static RopeDef MakeRopeDef(const char* id,
                            Vector2D    start = Vector2D(0.0f,  0.0f),
                            Vector2D    end   = Vector2D(0.0f, -3.0f),
                            int         count = 10)
{
    RopeDef rdef;
    rdef.id            = Dia::Core::StringCRC(id);
    rdef.startPoint    = start;
    rdef.endPoint      = end;
    rdef.particleCount = count;
    rdef.mass          = 1.0f;
    rdef.stiffness     = 1.0f;
    rdef.particleRadius = 0.05f;
    rdef.maxStretch    = 0.0f;
    return rdef;
}

static ClothDef MakeClothDef(const char* id,
                              int resX = 6,
                              int resY = 6)
{
    ClothDef cdef;
    cdef.id                 = Dia::Core::StringCRC(id);
    cdef.origin             = Vector2D(0.0f, 0.0f);
    cdef.width              = 3.0f;
    cdef.height             = 3.0f;
    cdef.resX               = resX;
    cdef.resY               = resY;
    cdef.mass               = 1.0f;
    cdef.structuralStiffness = 1.0f;
    cdef.shearStiffness     = 0.5f;
    cdef.bendStiffness      = 0.1f;
    cdef.particleRadius     = 0.05f;
    cdef.maxStretch         = 0.0f;
    cdef.pinTopRow          = true;
    return cdef;
}

// ---------------------------------------------------------------------------
// TEST 1 — Determinism: rope rebuilt identically produces identical positions
// ---------------------------------------------------------------------------

TEST(DiaSoftBody2D_Golden, Determinism_Rope_120Steps_IdenticalParticlePositions)
{
    static constexpr int kSteps     = 120;
    static constexpr int kParticles = 10;
    const float dt = 1.0f / 60.0f;

    Vector2D positionsA[kParticles];
    Vector2D positionsB[kParticles];

    // --- Run A ---
    {
        SoftBodyWorld world(MakeStandardWorldDef());
        Rope* rope = world.AddRope(MakeRopeDef("DeterRopeA"));
        rope->GetParticle(0).invMass = 0.0f;  // Pin top particle

        for (int i = 0; i < kSteps; ++i)
            world.Update(dt);

        for (int p = 0; p < kParticles; ++p)
            positionsA[p] = rope->GetParticle(p).position;
    }

    // --- Run B (identical setup) ---
    {
        SoftBodyWorld world(MakeStandardWorldDef());
        Rope* rope = world.AddRope(MakeRopeDef("DeterRopeA"));
        rope->GetParticle(0).invMass = 0.0f;  // Pin top particle

        for (int i = 0; i < kSteps; ++i)
            world.Update(dt);

        for (int p = 0; p < kParticles; ++p)
            positionsB[p] = rope->GetParticle(p).position;
    }

    for (int p = 0; p < kParticles; ++p)
    {
        EXPECT_FLOAT_EQ(positionsA[p].x, positionsB[p].x)
            << "Particle " << p << " x mismatch between run A and run B";
        EXPECT_FLOAT_EQ(positionsA[p].y, positionsB[p].y)
            << "Particle " << p << " y mismatch between run A and run B";
    }
}

// ---------------------------------------------------------------------------
// TEST 2 — Determinism: cloth rebuilt identically produces identical positions
// ---------------------------------------------------------------------------

TEST(DiaSoftBody2D_Golden, Determinism_Cloth_60Steps_IdenticalParticlePositions)
{
    static constexpr int kSteps = 60;
    static constexpr int kResX  = 4;
    static constexpr int kResY  = 4;
    const float dt = 1.0f / 60.0f;

    Vector2D positionsA[kResX][kResY];
    Vector2D positionsB[kResX][kResY];

    for (int run = 0; run < 2; ++run)
    {
        SoftBodyWorld world(MakeStandardWorldDef());
        Cloth* cloth = world.AddCloth(MakeClothDef("DeterCloth", kResX, kResY));

        for (int i = 0; i < kSteps; ++i)
            world.Update(dt);

        for (int y = 0; y < kResY; ++y)
            for (int x = 0; x < kResX; ++x)
            {
                if (run == 0)
                    positionsA[x][y] = cloth->GetParticle(x, y).position;
                else
                    positionsB[x][y] = cloth->GetParticle(x, y).position;
            }
    }

    for (int y = 0; y < kResY; ++y)
    {
        for (int x = 0; x < kResX; ++x)
        {
            EXPECT_FLOAT_EQ(positionsA[x][y].x, positionsB[x][y].x)
                << "Cloth particle (" << x << "," << y << ") x mismatch";
            EXPECT_FLOAT_EQ(positionsA[x][y].y, positionsB[x][y].y)
                << "Cloth particle (" << x << "," << y << ") y mismatch";
        }
    }
}

// ---------------------------------------------------------------------------
// TEST 3 — Stable simulation: cloth 200 frames, no NaN or Inf
// ---------------------------------------------------------------------------

TEST(DiaSoftBody2D_Golden, StableSimulation_Cloth_200Frames_NoNaNOrInf)
{
    const float dt = 1.0f / 60.0f;

    SoftBodyWorld world(MakeStandardWorldDef());
    Cloth* cloth = world.AddCloth(MakeClothDef("StableCloth", 8, 8));

    for (int i = 0; i < 200; ++i)
        world.Update(dt);

    for (int y = 0; y < cloth->GetResY(); ++y)
    {
        for (int x = 0; x < cloth->GetResX(); ++x)
        {
            const Particle& p = cloth->GetParticle(x, y);
            EXPECT_FALSE(std::isnan(p.position.x))
                << "NaN in cloth particle (" << x << "," << y << ") x";
            EXPECT_FALSE(std::isnan(p.position.y))
                << "NaN in cloth particle (" << x << "," << y << ") y";
            EXPECT_FALSE(std::isinf(p.position.x))
                << "Inf in cloth particle (" << x << "," << y << ") x";
            EXPECT_FALSE(std::isinf(p.position.y))
                << "Inf in cloth particle (" << x << "," << y << ") y";
        }
    }
}

// ---------------------------------------------------------------------------
// TEST 4 — Stable simulation: rope 200 frames, no NaN or Inf
// ---------------------------------------------------------------------------

TEST(DiaSoftBody2D_Golden, StableSimulation_Rope_200Frames_NoNaNOrInf)
{
    const float dt = 1.0f / 60.0f;

    SoftBodyWorld world(MakeStandardWorldDef());
    Rope* rope = world.AddRope(MakeRopeDef("StableRope"));
    rope->GetParticle(0).invMass = 0.0f;  // Pin top

    for (int i = 0; i < 200; ++i)
        world.Update(dt);

    for (int p = 0; p < rope->GetParticleCount(); ++p)
    {
        EXPECT_FALSE(std::isnan(rope->GetParticle(p).position.x))
            << "NaN in rope particle " << p << " x";
        EXPECT_FALSE(std::isnan(rope->GetParticle(p).position.y))
            << "NaN in rope particle " << p << " y";
        EXPECT_FALSE(std::isinf(rope->GetParticle(p).position.x))
            << "Inf in rope particle " << p << " x";
        EXPECT_FALSE(std::isinf(rope->GetParticle(p).position.y))
            << "Inf in rope particle " << p << " y";
    }
}

// ---------------------------------------------------------------------------
// TEST 5 — Gravity affects particles: free-falling rope center of mass drops
//
// A fully free rope (no pinned particles) under gravity must have its center
// of mass move downward after N simulation steps.
// ---------------------------------------------------------------------------

TEST(DiaSoftBody2D_Golden, Gravity_FreeFallingRope_CenterOfMassDrops)
{
    const float dt     = 1.0f / 60.0f;
    const int   kSteps = 60;

    SoftBodyWorld world(MakeStandardWorldDef());

    RopeDef rdef = MakeRopeDef("FreeRope",
                               Vector2D(0.0f,  5.0f),
                               Vector2D(0.0f,  2.0f),
                               8);
    // No pinned particles — all free to fall
    Rope* rope = world.AddRope(rdef);

    // Record initial center of mass Y
    float comYBefore = 0.0f;
    for (int p = 0; p < rope->GetParticleCount(); ++p)
        comYBefore += rope->GetParticle(p).position.y;
    comYBefore /= static_cast<float>(rope->GetParticleCount());

    for (int i = 0; i < kSteps; ++i)
        world.Update(dt);

    float comYAfter = 0.0f;
    for (int p = 0; p < rope->GetParticleCount(); ++p)
        comYAfter += rope->GetParticle(p).position.y;
    comYAfter /= static_cast<float>(rope->GetParticleCount());

    EXPECT_LT(comYAfter, comYBefore)
        << "Center of mass should have moved downward under gravity";
}

// ---------------------------------------------------------------------------
// TEST 6 — Gravity affects particles: free-falling cloth center of mass drops
// ---------------------------------------------------------------------------

TEST(DiaSoftBody2D_Golden, Gravity_FreeFallingCloth_CenterOfMassDrops)
{
    const float dt     = 1.0f / 60.0f;
    const int   kSteps = 60;

    WorldDef wdef = MakeStandardWorldDef();
    SoftBodyWorld world(wdef);

    ClothDef cdef = MakeClothDef("FreeCloth", 4, 4);
    cdef.pinTopRow = false;  // Fully free to fall
    Cloth* cloth  = world.AddCloth(cdef);

    float comYBefore = 0.0f;
    int   total      = cloth->GetParticleCount();
    for (int y = 0; y < cloth->GetResY(); ++y)
        for (int x = 0; x < cloth->GetResX(); ++x)
            comYBefore += cloth->GetParticle(x, y).position.y;
    comYBefore /= static_cast<float>(total);

    for (int i = 0; i < kSteps; ++i)
        world.Update(dt);

    float comYAfter = 0.0f;
    for (int y = 0; y < cloth->GetResY(); ++y)
        for (int x = 0; x < cloth->GetResX(); ++x)
            comYAfter += cloth->GetParticle(x, y).position.y;
    comYAfter /= static_cast<float>(total);

    EXPECT_LT(comYAfter, comYBefore)
        << "Cloth center of mass should have moved downward under gravity";
}
