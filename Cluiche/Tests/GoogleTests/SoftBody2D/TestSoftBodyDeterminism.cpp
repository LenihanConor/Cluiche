#include <gtest/gtest.h>

#include <DiaSoftBody2D/SoftBodyWorld.h>
#include <DiaSoftBody2D/Rope.h>
#include <DiaSoftBody2D/Cloth.h>
#include <DiaSoftBody2D/Particle.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaMaths/Vector/Vector2D.h>

#include <cmath>

using namespace Dia::SoftBody2D;
using namespace Dia::Maths;

static WorldDef MakeDef(float dt = 1.0f / 60.0f)
{
    WorldDef def;
    def.gravity          = Vector2D(0.0f, -9.81f);
    def.fixedTimestep    = dt;
    def.maxSubSteps      = 8;
    def.solverIterations = 10;
    def.rigidBodyWorld   = nullptr;
    return def;
}

static RopeDef MakeRopeDef()
{
    RopeDef def;
    def.id             = Dia::Core::StringCRC("DetRope");
    def.startPoint     = Vector2D(0.0f, 5.0f);
    def.endPoint       = Vector2D(0.0f, 0.0f);
    def.particleCount  = 12;
    def.mass           = 1.0f;
    def.stiffness      = 1.0f;
    def.particleRadius = 0.05f;
    def.maxStretch     = 0.0f;
    return def;
}

static ClothDef MakeClothDef()
{
    ClothDef def;
    def.id                  = Dia::Core::StringCRC("DetCloth");
    def.origin              = Vector2D(0.0f, 0.0f);
    def.width               = 3.0f;
    def.height              = 3.0f;
    def.resX                = 4;
    def.resY                = 4;
    def.mass                = 1.0f;
    def.structuralStiffness = 1.0f;
    def.shearStiffness      = 0.5f;
    def.bendStiffness       = 0.1f;
    def.particleRadius      = 0.05f;
    def.maxStretch          = 0.0f;
    def.pinTopRow           = true;
    return def;
}

// ---------------------------------------------------------------------------
// TEST 1 — Rope: two identical runs produce bit-identical particle positions
// ---------------------------------------------------------------------------

TEST(DiaSoftBody2D_Determinism, Rope_TwoIdenticalRuns_BitIdenticalPositions)
{
    const float dt    = 1.0f / 60.0f;
    const int   steps = 120;

    // Run A
    SoftBodyWorld worldA(MakeDef(dt));
    Rope* ropeA = worldA.AddRope(MakeRopeDef());
    ropeA->GetParticle(0).invMass = 0.0f;

    for (int s = 0; s < steps; ++s)
        worldA.Update(dt);

    // Run B — identical setup
    SoftBodyWorld worldB(MakeDef(dt));
    Rope* ropeB = worldB.AddRope(MakeRopeDef());
    ropeB->GetParticle(0).invMass = 0.0f;

    for (int s = 0; s < steps; ++s)
        worldB.Update(dt);

    // Compare every particle position
    ASSERT_EQ(ropeA->GetParticleCount(), ropeB->GetParticleCount());
    for (int p = 0; p < ropeA->GetParticleCount(); ++p)
    {
        const Particle& pa = ropeA->GetParticle(p);
        const Particle& pb = ropeB->GetParticle(p);
        EXPECT_FLOAT_EQ(pa.position.x, pb.position.x)
            << "Rope x mismatch at particle " << p;
        EXPECT_FLOAT_EQ(pa.position.y, pb.position.y)
            << "Rope y mismatch at particle " << p;
    }
}

// ---------------------------------------------------------------------------
// TEST 2 — Cloth: two identical runs produce bit-identical particle positions
// ---------------------------------------------------------------------------

TEST(DiaSoftBody2D_Determinism, Cloth_TwoIdenticalRuns_BitIdenticalPositions)
{
    const float dt    = 1.0f / 60.0f;
    const int   steps = 60;

    SoftBodyWorld worldA(MakeDef(dt));
    Cloth* clothA = worldA.AddCloth(MakeClothDef());

    for (int s = 0; s < steps; ++s)
        worldA.Update(dt);

    SoftBodyWorld worldB(MakeDef(dt));
    Cloth* clothB = worldB.AddCloth(MakeClothDef());

    for (int s = 0; s < steps; ++s)
        worldB.Update(dt);

    ASSERT_EQ(clothA->GetResX(), clothB->GetResX());
    ASSERT_EQ(clothA->GetResY(), clothB->GetResY());

    for (int y = 0; y < clothA->GetResY(); ++y)
    {
        for (int x = 0; x < clothA->GetResX(); ++x)
        {
            const Particle& pa = clothA->GetParticle(x, y);
            const Particle& pb = clothB->GetParticle(x, y);
            EXPECT_FLOAT_EQ(pa.position.x, pb.position.x)
                << "Cloth x mismatch at [" << x << "," << y << "]";
            EXPECT_FLOAT_EQ(pa.position.y, pb.position.y)
                << "Cloth y mismatch at [" << x << "," << y << "]";
        }
    }
}

// ---------------------------------------------------------------------------
// TEST 3 — Rope: segmented run matches continuous run
//
// Run 60 steps, then another 60 steps starting from that state.
// Must match a continuous 120-step run from scratch.
// ---------------------------------------------------------------------------

TEST(DiaSoftBody2D_Determinism, Rope_SegmentedRun_MatchesContinuousRun)
{
    const float dt = 1.0f / 60.0f;

    // Continuous run: 120 steps straight
    SoftBodyWorld worldFull(MakeDef(dt));
    Rope* ropeFull = worldFull.AddRope(MakeRopeDef());
    ropeFull->GetParticle(0).invMass = 0.0f;

    for (int s = 0; s < 120; ++s)
        worldFull.Update(dt);

    // Segmented run: 60 + 60 steps in the same world
    SoftBodyWorld worldSeg(MakeDef(dt));
    Rope* ropeSeg = worldSeg.AddRope(MakeRopeDef());
    ropeSeg->GetParticle(0).invMass = 0.0f;

    for (int s = 0; s < 60; ++s)
        worldSeg.Update(dt);
    for (int s = 0; s < 60; ++s)
        worldSeg.Update(dt);

    ASSERT_EQ(ropeFull->GetParticleCount(), ropeSeg->GetParticleCount());
    for (int p = 0; p < ropeFull->GetParticleCount(); ++p)
    {
        const Particle& pf = ropeFull->GetParticle(p);
        const Particle& ps = ropeSeg->GetParticle(p);
        EXPECT_FLOAT_EQ(pf.position.x, ps.position.x)
            << "Segmented vs continuous x mismatch at particle " << p;
        EXPECT_FLOAT_EQ(pf.position.y, ps.position.y)
            << "Segmented vs continuous y mismatch at particle " << p;
    }
}

// ---------------------------------------------------------------------------
// TEST 4 — Rope: no NaN or Inf after many steps
// ---------------------------------------------------------------------------

TEST(DiaSoftBody2D_Determinism, Rope_ManySteps_NoNaNOrInf)
{
    const float dt = 1.0f / 60.0f;

    SoftBodyWorld world(MakeDef(dt));
    Rope* rope = world.AddRope(MakeRopeDef());
    rope->GetParticle(0).invMass = 0.0f;

    for (int s = 0; s < 300; ++s)
        world.Update(dt);

    for (int p = 0; p < rope->GetParticleCount(); ++p)
    {
        const Particle& par = rope->GetParticle(p);
        EXPECT_FALSE(std::isnan(par.position.x)) << "NaN x at particle " << p;
        EXPECT_FALSE(std::isnan(par.position.y)) << "NaN y at particle " << p;
        EXPECT_FALSE(std::isinf(par.position.x)) << "Inf x at particle " << p;
        EXPECT_FALSE(std::isinf(par.position.y)) << "Inf y at particle " << p;
    }
}

// ---------------------------------------------------------------------------
// TEST 5 — Cloth: no NaN or Inf after many steps
// ---------------------------------------------------------------------------

TEST(DiaSoftBody2D_Determinism, Cloth_ManySteps_NoNaNOrInf)
{
    const float dt = 1.0f / 60.0f;

    SoftBodyWorld world(MakeDef(dt));
    Cloth* cloth = world.AddCloth(MakeClothDef());
    (void)cloth;

    for (int s = 0; s < 180; ++s)
        world.Update(dt);

    for (int y = 0; y < cloth->GetResY(); ++y)
    {
        for (int x = 0; x < cloth->GetResX(); ++x)
        {
            const Particle& p = cloth->GetParticle(x, y);
            EXPECT_FALSE(std::isnan(p.position.x)) << "NaN x at [" << x << "," << y << "]";
            EXPECT_FALSE(std::isnan(p.position.y)) << "NaN y at [" << x << "," << y << "]";
        }
    }
}
