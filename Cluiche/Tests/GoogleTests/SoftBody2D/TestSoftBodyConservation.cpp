#include <gtest/gtest.h>

#include <DiaSoftBody2D/SoftBodyWorld.h>
#include <DiaSoftBody2D/Rope.h>
#include <DiaSoftBody2D/Cloth.h>
#include <DiaSoftBody2D/Particle.h>
#include <DiaMaths/Vector/Vector2D.h>
#include <DiaCore/CRC/StringCRC.h>

#include <cmath>

using namespace Dia::SoftBody2D;
using namespace Dia::Maths;

static WorldDef MakeDampedDef(float dt = 1.0f / 60.0f)
{
    WorldDef def;
    def.gravity          = Vector2D(0.0f, -9.81f);
    def.fixedTimestep    = dt;
    def.maxSubSteps      = 8;
    def.solverIterations = 10;
    def.rigidBodyWorld   = nullptr;
    return def;
}

static WorldDef MakeZeroGravDef(float dt = 1.0f / 60.0f)
{
    WorldDef def = MakeDampedDef(dt);
    def.gravity = Vector2D(0.0f, 0.0f);
    return def;
}

// ---------------------------------------------------------------------------
// Helper: compute total kinetic energy of a rope (using Verlet velocity).
// ---------------------------------------------------------------------------

static float RopeKineticEnergy(const Rope& rope, float dt)
{
    float ke = 0.0f;
    for (int p = 0; p < rope.GetParticleCount(); ++p)
    {
        const Particle& par = rope.GetParticle(p);
        if (par.invMass <= 0.0f) continue;
        float m = 1.0f / par.invMass;
        Vector2D v = DeriveVelocity(par, dt);
        ke += 0.5f * m * (v.x * v.x + v.y * v.y);
    }
    return ke;
}

// ---------------------------------------------------------------------------
// TEST 1 — Pinned cloth under gravity: total energy monotonically decreases.
//
// A pinned cloth with damping must dissipate energy — KE should not increase
// from one measurement to the next once the system is out of the initial
// transient (Verlet can introduce a small initial jitter, so we check over a
// long window, not step-by-step).
// ---------------------------------------------------------------------------

TEST(DiaSoftBody2D_Conservation, PinnedCloth_Energy_MonotonicallyDecreases_OverWindow)
{
    const float dt = 1.0f / 60.0f;

    SoftBodyWorld world(MakeDampedDef(dt));

    ClothDef cdef;
    cdef.id                  = Dia::Core::StringCRC("EnergyCloth");
    cdef.origin              = Vector2D(0.0f, 0.0f);
    cdef.width               = 4.0f;
    cdef.height              = 4.0f;
    cdef.resX                = 5;
    cdef.resY                = 5;
    cdef.mass                = 1.0f;
    cdef.structuralStiffness = 1.0f;
    cdef.shearStiffness      = 0.5f;
    cdef.bendStiffness       = 0.1f;
    cdef.particleRadius      = 0.05f;
    cdef.maxStretch          = 0.0f;
    cdef.pinTopRow           = true;
    Cloth* cloth = world.AddCloth(cdef);

    // Let the simulation settle a bit first
    for (int i = 0; i < 30; ++i)
        world.Update(dt);

    // Measure kinetic energy at increasing intervals — it must decrease
    auto clothKE = [&]() {
        float ke = 0.0f;
        for (int y = 0; y < cloth->GetResY(); ++y)
        {
            for (int x = 0; x < cloth->GetResX(); ++x)
            {
                const Particle& p = cloth->GetParticle(x, y);
                if (p.invMass <= 0.0f) continue;
                float m = 1.0f / p.invMass;
                Vector2D v = DeriveVelocity(p, dt);
                ke += 0.5f * m * (v.x * v.x + v.y * v.y);
            }
        }
        return ke;
    };

    float ke0 = clothKE();

    for (int window = 0; window < 3; ++window)
    {
        for (int i = 0; i < 60; ++i)
            world.Update(dt);

        float ke = clothKE();
        // Allow 1% tolerance or an absolute floor for floating-point noise near zero
        float threshold = ke0 * 1.01f + 1e-10f;
        EXPECT_LE(ke, threshold)
            << "KE increased in window " << window << ": " << ke << " > " << ke0;
        ke0 = ke;
    }
}

// ---------------------------------------------------------------------------
// TEST 2 — Free rope (no gravity): center of mass moves at constant velocity.
//
// With zero gravity and no pinned particles, all external forces are zero.
// The rope's CoM must move at constant velocity (p = const).
// ---------------------------------------------------------------------------

TEST(DiaSoftBody2D_Conservation, FreeRope_CenterOfMass_ConstantVelocity)
{
    const float dt = 1.0f / 60.0f;

    SoftBodyWorld world(MakeZeroGravDef(dt));

    RopeDef rdef;
    rdef.id            = Dia::Core::StringCRC("FreeRopeCoM");
    rdef.startPoint    = Vector2D(-2.0f, 0.0f);
    rdef.endPoint      = Vector2D( 2.0f, 0.0f);
    rdef.particleCount = 8;
    rdef.mass          = 1.0f;
    rdef.stiffness     = 1.0f;
    rdef.particleRadius = 0.05f;
    rdef.maxStretch    = 0.0f;
    Rope* rope = world.AddRope(rdef);

    // Give all particles a uniform initial velocity by shifting prevPosition
    // (Verlet: velocity = (pos - prevPos) / dt)
    const Vector2D kick(1.0f, 0.5f);
    for (int p = 0; p < rope->GetParticleCount(); ++p)
    {
        Particle& par = rope->GetParticle(p);
        par.prevPosition = par.position - kick * dt;
    }

    // Let the rope settle into motion (1 step to initialize Verlet state)
    world.Update(dt);

    // Compute initial CoM velocity
    auto computeCoMVel = [&]() -> Vector2D
    {
        Vector2D comVel(0.0f, 0.0f);
        float totalMass = 0.0f;
        for (int p = 0; p < rope->GetParticleCount(); ++p)
        {
            const Particle& par = rope->GetParticle(p);
            if (par.invMass <= 0.0f) continue;
            float m = 1.0f / par.invMass;
            comVel = comVel + DeriveVelocity(par, dt) * m;
            totalMass += m;
        }
        if (totalMass > 0.0f)
            comVel = comVel * (1.0f / totalMass);
        return comVel;
    };

    Vector2D comVel0 = computeCoMVel();

    for (int s = 0; s < 60; ++s)
    {
        world.Update(dt);
        Vector2D comVel = computeCoMVel();
        // CoM velocity should be conserved within integration tolerance
        EXPECT_NEAR(comVel.x, comVel0.x, 0.05f)
            << "CoM vx changed at step " << s;
        EXPECT_NEAR(comVel.y, comVel0.y, 0.05f)
            << "CoM vy changed at step " << s;
    }
}

// ---------------------------------------------------------------------------
// TEST 3 — Pinned rope: pinned particle stays pinned throughout simulation.
//
// A particle with invMass == 0 must not move regardless of constraint forces.
// ---------------------------------------------------------------------------

TEST(DiaSoftBody2D_Conservation, PinnedRope_PinnedParticle_DoesNotMove)
{
    const float dt = 1.0f / 60.0f;

    SoftBodyWorld world(MakeDampedDef(dt));

    RopeDef rdef;
    rdef.id            = Dia::Core::StringCRC("PinnedRope");
    rdef.startPoint    = Vector2D(0.0f, 5.0f);
    rdef.endPoint      = Vector2D(0.0f, 0.0f);
    rdef.particleCount = 12;
    rdef.mass          = 1.0f;
    rdef.stiffness     = 1.0f;
    rdef.particleRadius = 0.05f;
    rdef.maxStretch    = 0.0f;
    Rope* rope = world.AddRope(rdef);

    // Pin the first particle
    rope->GetParticle(0).invMass = 0.0f;
    Vector2D pinnedPos = rope->GetParticle(0).position;

    for (int s = 0; s < 120; ++s)
        world.Update(dt);

    Vector2D finalPos = rope->GetParticle(0).position;
    EXPECT_NEAR(finalPos.x, pinnedPos.x, 1e-4f) << "Pinned particle moved in x";
    EXPECT_NEAR(finalPos.y, pinnedPos.y, 1e-4f) << "Pinned particle moved in y";
}
